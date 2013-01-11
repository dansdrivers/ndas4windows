#include "lspkrnl.h"
#include "lsptransfer.h"
#include <xtdi.h>
#include <xtdilpx.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

NTSTATUS
LspDataInitialize(
	__in PDEVICE_OBJECT DeviceObject,
	__in PDEVICE_OBJECT ConnectionDeviceObject, 
	__inout PLSP_TRANSFER_DATA LspTransferData)
{
	RtlZeroMemory(LspTransferData, sizeof(LSP_TRANSFER_DATA));

	LspTransferData->Irp = IoAllocateIrp(
		ConnectionDeviceObject->StackSize + 1,
		FALSE);
	if (NULL == LspTransferData->Irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	LspTransferData->Overlapped.CompletionRoutine = LspTransferCompletion;
	LspTransferData->Overlapped.UserContext = DeviceObject;
	return STATUS_SUCCESS;
}

VOID
LspDataCleanup(
	__in PLSP_TRANSFER_DATA LspTransferData)
{
	if (LspTransferData->Irp)
	{
		IoFreeIrp(LspTransferData->Irp);
		LspTransferData->Irp = NULL;
	}

}

VOID
LspDataReset(
	__in PDEVICE_OBJECT DeviceObject,
	__in PLSP_TRANSFER_DATA LspTransferData)
{
	RtlZeroMemory(&LspTransferData->Overlapped, sizeof(XTDI_OVERLAPPED));
	LspTransferData->Overlapped.CompletionRoutine = LspTransferCompletion;
	LspTransferData->Overlapped.UserContext = DeviceObject;
	LspTransferData->Flags = 0;
	ASSERT(NULL != LspTransferData->Irp);
	// IoFreeIrp(LspTransferData->Irp);
	// LspTransferData->Irp = IoAllocateIrp(DeviceObject->StackSize + 1, FALSE);
	IoReuseIrp(LspTransferData->Irp, STATUS_SUCCESS);
}

PLSP_TRANSFER_DATA
LspDataSelect(
	__in PDEVICE_OBJECT DeviceObject)
{
	PDEVICE_EXTENSION deviceExtension;
	PLSP_TRANSFER_DATA lspTransferData;
	ULONG i;
	deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	lspTransferData = deviceExtension->LspTransferData;
	
	for (i = 0; i < countof(deviceExtension->LspTransferData); ++i)
	{
		if (0 == lspTransferData[i].Flags)
		{
			PLSP_TRANSFER_DATA selected = &lspTransferData[i];
			selected->Flags = LSPDATA_IN_USE;
			return selected;
		}
	}
	ASSERT(FALSE);
	return NULL;
}

VOID
LspDataResetAll(
	__in PDEVICE_OBJECT DeviceObject)
{
	PDEVICE_EXTENSION deviceExtension;
	ULONG i;

	deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	for (i = 0; i < countof(deviceExtension->LspTransferData); ++i)
	{
		LspDataReset(DeviceObject, &deviceExtension->LspTransferData[i]);
	}
}

VOID
LspTransferCompletion(
	__in PIRP TransferIrp,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	PDEVICE_OBJECT deviceObject;
	PDEVICE_EXTENSION deviceExtension;

	LONG outstandingLspTransfer;
	KIRQL oldIrql;
	IO_STATUS_BLOCK txIoStatus;

	UNREFERENCED_PARAMETER(TransferIrp);

	deviceObject = (PDEVICE_OBJECT) Overlapped->UserContext;
	deviceExtension = (PDEVICE_EXTENSION) deviceObject->DeviceExtension;

	outstandingLspTransfer = InterlockedDecrement(&deviceExtension->LspTransferCount);
	ASSERT(outstandingLspTransfer >= 0);

	LSP_KDPRINT(("LspTransferCompletion: TransferIrp=%p, Overlapped=%p, TxCount=%d\n", 
		TransferIrp, Overlapped, outstandingLspTransfer));

	if (0 == outstandingLspTransfer)
	{
		txIoStatus = Overlapped->IoStatus;
		deviceExtension->LspStatus = lsp_process_next(deviceExtension->LspHandle);

		LspDataResetAll(deviceObject);

		status = LspProcessNext(deviceObject, &deviceExtension->LspStatus);

		if (STATUS_PENDING == status)
		{
			return;
		}

		if (NULL != deviceExtension->CurrentIrp)
		{
			//
			// Asynchronous Transfer in progress
			//
			PIRP irp = deviceExtension->CurrentIrp;
			
			LSP_KDPRINT(("LspTransferCompletion: Completing Irp=%p.\n", irp));
			
			deviceExtension->CurrentIrp = NULL;
			irp->IoStatus.Status = 
				NT_SUCCESS(txIoStatus.Status) ? status : txIoStatus.Status;
			if (!NT_SUCCESS(irp->IoStatus.Status))
			{
				irp->IoStatus.Information = 0;
			}

			IoCompleteRequest(
				irp, 
				IO_NO_INCREMENT);
			//
			// IoStartNextPacket should be running at DISPATCH_LEVEL
			//
			ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
			oldIrql = KeRaiseIrqlToDpcLevel();
			IoStartNextPacket(deviceObject, TRUE);
			KeLowerIrql(oldIrql);
			return;
		}
		else
		{
			//
			// Synchronous Transfer in progress
			//
			LSP_KDPRINT(("LspTransferCompletion: Synchronous transfer event=%p.\n", &deviceExtension->LspCompletionEvent));
			KeSetEvent(
				&deviceExtension->LspCompletionEvent, 
				IO_NO_INCREMENT, 
				FALSE);
		}
	}
}

NTSTATUS
LspProcessNext(
	__in PDEVICE_OBJECT DeviceObject,
	__inout lsp_status_t* LspStatus)
{
	NTSTATUS status, miscStatus;
	PDEVICE_EXTENSION deviceExtension;
	PVOID dataBuffer;
	UINT32 dataBufferLength;
	ULONG bytesTransferred;

	lsp_handle_t lspHandle;

	PDEVICE_OBJECT connectionDeviceObject;
	PFILE_OBJECT connectionFileObject;

	PLSP_TRANSFER_DATA lspTransferData;
	KIRQL oldIrql;

	LONG transferCount;

	// LSP_KDPRINT(("LspProcessNext: DeviceObject=%p\n", DeviceObject));

	deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

	lspHandle = deviceExtension->LspHandle;

	connectionDeviceObject = deviceExtension->ConnectionDeviceObject;
	connectionFileObject = deviceExtension->ConnectionFileObject;

	//
	// Initial Count is 1, when this counter reaches 0
	// complete the CurrentIrp
	//

	ASSERT(0 == deviceExtension->LspTransferCount);
	transferCount = InterlockedIncrement(&deviceExtension->LspTransferCount);
	ASSERT(1 == transferCount);
	
	while (TRUE)
	{
		LSP_KDPRINT(("P%X\n", *LspStatus));
		switch (*LspStatus)
		{
		case LSP_REQUIRES_SEND:
			{
				PLSP_TRANSFER_DATA lspTransferData;
				lspTransferData = LspDataSelect(DeviceObject);

				dataBuffer = lsp_get_buffer_to_send(
					lspHandle, 
					&dataBufferLength);

				LSP_KDPRINT(("S%d\n", dataBufferLength));

				transferCount = InterlockedIncrement(&deviceExtension->LspTransferCount);
				ASSERT(transferCount > 1);

				status = xTdiSendEx(
					lspTransferData->Irp,
					connectionDeviceObject,
					connectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&lspTransferData->Overlapped);

				if (!NT_SUCCESS(status))
				{
					LSP_KDPRINT(("xTdiSend failed with status %x\n", status));
					ASSERT(FALSE);
					return status;
				}

			}
			break;
		case LSP_REQUIRES_RECEIVE:
			{
				PLSP_TRANSFER_DATA lspTransferData;
				lspTransferData = LspDataSelect(DeviceObject);

				dataBuffer = lsp_get_buffer_to_receive(
					lspHandle, 
					&dataBufferLength);

				LSP_KDPRINT(("R%d\n", dataBufferLength));

				transferCount = InterlockedIncrement(&deviceExtension->LspTransferCount);
				ASSERT(transferCount > 1);

				status = xTdiReceiveEx(
					lspTransferData->Irp,
					connectionDeviceObject,
					connectionFileObject,
					dataBuffer,
					dataBufferLength,
					&bytesTransferred,
					0,
					&lspTransferData->Overlapped);

				if (!NT_SUCCESS(status))
				{
					LSP_KDPRINT(("xTdiSend failed with status %x\n", status));
					ASSERT(FALSE);
					return status;
				}

			}
			break;
		case LSP_REQUIRES_SYNCHRONIZE:
			{
				transferCount = InterlockedDecrement(&deviceExtension->LspTransferCount);

				ASSERT(transferCount >= 0);
				if (transferCount > 0)
				{
					LSP_KDPRINT(("LspProcessNext: DeviceObject=%p, TxCount=%d, Status=%x\n", 
						DeviceObject, transferCount , STATUS_PENDING));

					//
					// Completion routines are not called yet
					//
					return STATUS_PENDING;
				}

				LSP_KDPRINT(("LspProcessNext: DeviceObject=%p, TxCount=%d, Status=%x\n", 
					DeviceObject, transferCount , STATUS_SUCCESS));

				//
				// One or two completion routines are already called.
				// Do the next process
				//
				LspDataResetAll(DeviceObject);

				//
				// Reset the transfer count to 1
				//
				ASSERT(0 == deviceExtension->LspTransferCount);
				transferCount = InterlockedIncrement(&deviceExtension->LspTransferCount);
				ASSERT(1 == transferCount);
			}
			break;
		default:
			{
				transferCount = InterlockedDecrement(&deviceExtension->LspTransferCount);
				ASSERT(0 == transferCount);
				LSP_KDPRINT(("LSP returned %x\n", *LspStatus));
				status = (*LspStatus == LSP_STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
				// LSP_KDPRINT(("LspProcessNext completed: DeviceObject=%p, Status=%x\n", DeviceObject, status));
				return status;
			}
		}
		*LspStatus = lsp_process_next(lspHandle);
	}	
}

NTSTATUS
LspProcessSynchronous(
	__in PDEVICE_OBJECT DeviceObject,
	__inout lsp_status_t* LspStatus)
{
	NTSTATUS status;
	PDEVICE_EXTENSION deviceExtension;

	deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

	ASSERT(NULL == deviceExtension->CurrentIrp);

	deviceExtension->CurrentIrp = NULL;

	KeClearEvent(&deviceExtension->LspCompletionEvent);

	status = LspProcessNext(DeviceObject, LspStatus);

	if (STATUS_PENDING == status)
	{
		status = KeWaitForSingleObject(
			&deviceExtension->LspCompletionEvent,
			Executive,
			KernelMode,
			FALSE,
			NULL);
		ASSERT(STATUS_SUCCESS == status);
	}

	status = (*LspStatus == LSP_STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
	return status;
}

NTSTATUS
LspInitializeConnection(
	__in PDEVICE_OBJECT DeviceObject)
{
	NTSTATUS status, cleanupStatus;
	PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	lsp_status_t lspStatus;
	ULONG i;

	status = xLpxTdiCreateAddressObject(
		&deviceExtension->LocalAddress,
		&deviceExtension->AddressHandle,
		&deviceExtension->AddressFileObject,
		&deviceExtension->AddressDeviceObject);

	if (!NT_SUCCESS(status))
	{
		LSP_KDPRINT(("xLpxTdiCreateAddressObject failed with status %x\n", status));
		// We don't need to cleanup function on initial failure
		// LspCleanupConnection(DeviceObject);
		return status;
	}

	deviceExtension->StatusFlags |= LSPDSTF_ADDRESS_CREATED;

	status = xLpxTdiCreateConnectionObject(
		NULL,
		&deviceExtension->ConnectionHandle,
		&deviceExtension->ConnectionFileObject,
		&deviceExtension->ConnectionDeviceObject);

	if (!NT_SUCCESS(status))
	{
		LSP_KDPRINT(("xLpxTdiCreateConnectionObject failed with status %x\n", status));
		LspCleanupConnection(DeviceObject);
		return status;
	}

	deviceExtension->StatusFlags |= LSPDSTF_CONNECTION_CREATED;

	status = xTdiAssociateAddress(
		deviceExtension->AddressHandle,
		deviceExtension->ConnectionDeviceObject,
		deviceExtension->ConnectionFileObject);

	if (!NT_SUCCESS(status))
	{
		LSP_KDPRINT(("xTdiAssociateAddress failed with status %x\n", status));
		LspCleanupConnection(DeviceObject);
		return status;
	}

	deviceExtension->StatusFlags |= LSPDSTF_ADDRESS_ASSOCIATED;

	status = xLpxTdiConnect(
		deviceExtension->ConnectionDeviceObject,
		deviceExtension->ConnectionFileObject,
		&deviceExtension->DeviceAddress,
		NULL);

	if (!NT_SUCCESS(status))
	{
		LSP_KDPRINT(("xLpxTdiConnect failed with status %x\n", status));
		LspCleanupConnection(DeviceObject);
		return status;
	}

	deviceExtension->StatusFlags |= LSPDSTF_CONNECTED;

	deviceExtension->LspHandle = lsp_initialize_session(
		deviceExtension->LspSessionBuffer, 
		LSP_SESSION_BUFFER_SIZE);
	RtlZeroMemory(
		&deviceExtension->LspLoginInfo, 
		sizeof(lsp_login_info_t));
	deviceExtension->LspLoginInfo.login_type = LSP_LOGIN_TYPE_NORMAL;
	RtlCopyMemory(
		deviceExtension->LspLoginInfo.password,
		LSP_LOGIN_PASSWORD_DEFAULT,
		sizeof(LSP_LOGIN_PASSWORD_DEFAULT));
	deviceExtension->LspLoginInfo.unit_no = 0;

	for (i = 0; i < countof(deviceExtension->LspTransferData); ++i)
	{
		status = LspDataInitialize(
			DeviceObject,
			deviceExtension->ConnectionDeviceObject,
			&deviceExtension->LspTransferData[i]);

		if (!NT_SUCCESS(status))
		{
			LSP_KDPRINT(("xLpxTdiConnect failed with status %x\n", status));
			LspCleanupConnection(DeviceObject);
			return status;
		}
	}

	deviceExtension->LspStatus = lsp_login(
		deviceExtension->LspHandle, 
		&deviceExtension->LspLoginInfo);
	status = LspProcessSynchronous(DeviceObject, &deviceExtension->LspStatus);
	if (!NT_SUCCESS(status))
	{
		LSP_KDPRINT(("lsp_login failed with status %x\n", status));
		LspCleanupConnection(DeviceObject);
		return status;
	}

	deviceExtension->StatusFlags |= LSPDSTF_LOGGED_IN;

	//
	// This routine is a synchronous routine.
	//
	// status = STATUS_SUCCESS;

	LSP_KDPRINT(("LspInitializeConnection %x\n", status));

	return status;
}

NTSTATUS
LspCleanupConnection(
	__in PDEVICE_OBJECT DeviceObject)
{
	NTSTATUS cleanupStatus;
	PDEVICE_EXTENSION deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	lsp_status_t lspStatus;

	if (deviceExtension->StatusFlags & LSPDSTF_LOGGED_IN)
	{
		ASSERT(NULL == deviceExtension->CurrentIrp);
		deviceExtension->CurrentIrp = NULL;
		lspStatus = lsp_logout(deviceExtension->LspHandle);
		cleanupStatus = LspProcessSynchronous(DeviceObject, &lspStatus);
		deviceExtension->StatusFlags &= ~LSPDSTF_LOGGED_IN;
	}

	LspDataCleanup(&deviceExtension->LspTransferData[0]);
	LspDataCleanup(&deviceExtension->LspTransferData[1]);

	if (deviceExtension->StatusFlags & LSPDSTF_CONNECTED)
	{
		cleanupStatus = xTdiDisconnect(
			deviceExtension->ConnectionDeviceObject,
			deviceExtension->ConnectionFileObject,
			TDI_DISCONNECT_RELEASE,
			NULL, 
			NULL, 
			NULL);
		deviceExtension->StatusFlags &= ~LSPDSTF_CONNECTED;
	}

	if (deviceExtension->StatusFlags & LSPDSTF_ADDRESS_ASSOCIATED)
	{
		cleanupStatus = xTdiDisassociateAddress(
			deviceExtension->ConnectionDeviceObject,
			deviceExtension->ConnectionFileObject);
		deviceExtension->StatusFlags &= ~LSPDSTF_ADDRESS_ASSOCIATED;
	}

	if (deviceExtension->StatusFlags & LSPDSTF_CONNECTION_CREATED)
	{
		cleanupStatus = xTdiCloseConnectionObject(
			deviceExtension->ConnectionHandle,
			deviceExtension->ConnectionFileObject);
		deviceExtension->StatusFlags &= ~LSPDSTF_CONNECTION_CREATED;
	}

	if (deviceExtension->StatusFlags & LSPDSTF_ADDRESS_CREATED)
	{
		cleanupStatus = xTdiCloseAddressObject(
			deviceExtension->AddressHandle,
			deviceExtension->AddressFileObject);
		deviceExtension->StatusFlags &= ~LSPDSTF_ADDRESS_CREATED;
	}

	LSP_KDPRINT(("LspCleanupConnection %x\n", STATUS_SUCCESS));

	return STATUS_SUCCESS;
}
