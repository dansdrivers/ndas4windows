#include "port.h"

#ifdef RUN_WPP
#include "network.tmh"
#endif

VOID
NdasPortFdoBindChangeHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in TDI_PNP_OPCODE PnpOpCode,
	__in PUNICODE_STRING TransportName,
	__in PWSTR BindingList);

VOID
NdasPortFdoAddNetAddressHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context);

VOID
NdasPortFdoDelNetAddressHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context);

NTSTATUS
NdasPortTdiPowerHandler(
	__in PUNICODE_STRING DeviceName,
	__in PNET_PNP_EVENT PowerEvent,
	__in PTDI_PNP_CONTEXT Context1,
	__in PTDI_PNP_CONTEXT Context2);

VOID
NdasPortFdoBindChangeHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in TDI_PNP_OPCODE PnpOpCode,
	__in PUNICODE_STRING TransportName,
	__in PWSTR BindingList)
{
	switch (PnpOpCode) 
	{
	case TDI_PNP_OP_ADD:
	case TDI_PNP_OP_NETREADY:
	case TDI_PNP_OP_PROVIDERREADY:
		NdasPortFdoInitializeLogicalUnits(FdoExtension);
		break;
	case TDI_PNP_OP_DEL:
	default: 
		break;
	}
}

VOID
NdasPortFdoAddNetAddressHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{
	NdasPortFdoInitializeLogicalUnits(FdoExtension);
}

VOID
NdasPortFdoDelNetAddressHandler(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{

}

BOOLEAN
NdasPortIsLpxAddress(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName)
{
	DECLARE_CONST_UNICODE_STRING(LPX_DEVICE_NAME_PREFIX, L"\\Device\\Lpx_");

	BOOLEAN isPrefix;

	//
	// As LPX strings are all TDI_ADDRESS_TYPE_UNSPEC, there is no way
	// to tell if the address is LPX other than using prefix compare.
	// LPX device names are like "\device\lpx_{GUID}".
	// 

	if (Address->AddressType != TDI_ADDRESS_TYPE_LPX)
	{
		return FALSE;
	}

	isPrefix = RtlPrefixUnicodeString(
		&LPX_DEVICE_NAME_PREFIX, 
		DeviceName,
		TRUE);

	if (!isPrefix)
	{
		return FALSE;
	}

	if (Address->AddressLength != sizeof(TDI_ADDRESS_LPX))
	{
		ASSERTMSG("TDI_ADDRESS_LPX size is different?", FALSE);
		return FALSE;
	}

	return TRUE;
}

VOID
NdasPortAddToLpxLocalAddressList(
	PNDASPORT_DRIVER_EXTENSION DriverExtension,
	PTDI_ADDRESS_LPX TdiLpxAddress)
{
	PTDI_ADDRESS_LPX_LIST_ENTRY lpxAddressEntry;

	lpxAddressEntry = (PTDI_ADDRESS_LPX_LIST_ENTRY) ExAllocatePoolWithTag(
		NonPagedPool, 
		sizeof(TDI_ADDRESS_LPX_LIST_ENTRY), 
		NDASPORT_TAG_TDI);

	if (NULL == lpxAddressEntry)
	{
		DebugPrint((0, "NdasPortAddToLpxLocalAddressList: Allocation Failure: "
			"%02X%02X%02X:%02X%02X%02X\n",
			TdiLpxAddress->Node[0], TdiLpxAddress->Node[1],
			TdiLpxAddress->Node[2], TdiLpxAddress->Node[3],
			TdiLpxAddress->Node[4], TdiLpxAddress->Node[5]));
		ASSERT(FALSE);
		return;
	}

	RtlCopyMemory(
		&lpxAddressEntry->TdiAddress,
		TdiLpxAddress,
		sizeof(TDI_ADDRESS_LPX));

	ExInterlockedInsertHeadList(
		&DriverExtension->LpxLocalAddressList,
		&lpxAddressEntry->ListEntry,
		&DriverExtension->LpxLocalAddressListSpinLock);

	DebugPrint((0, "\tLocal LPX Address Added: %02X%02X%02X:%02X%02X%02X\n",
		TdiLpxAddress->Node[0], TdiLpxAddress->Node[1],
		TdiLpxAddress->Node[2], TdiLpxAddress->Node[3],
		TdiLpxAddress->Node[4], TdiLpxAddress->Node[5]));
}

VOID
NdasPortClearLpxLocalAddressList(
	PNDASPORT_DRIVER_EXTENSION DriverExtension)
{
	PLIST_ENTRY entry;
	KIRQL oldIrql;

	KeAcquireSpinLock(&DriverExtension->LpxLocalAddressListSpinLock, &oldIrql);

	for (entry = RemoveHeadList(&DriverExtension->LpxLocalAddressList);
		entry != &DriverExtension->LpxLocalAddressList;
		entry = RemoveHeadList(&DriverExtension->LpxLocalAddressList))
	{
		PTDI_ADDRESS_LPX_LIST_ENTRY lpxAddressEntry;

		lpxAddressEntry = CONTAINING_RECORD(
			entry, 
			TDI_ADDRESS_LPX_LIST_ENTRY, 
			ListEntry);

		//
		// Now we can free the allocated list
		//
		ExFreePoolWithTag(
			lpxAddressEntry,
			NDASPORT_TAG_TDI);
	}

	KeReleaseSpinLock(&DriverExtension->LpxLocalAddressListSpinLock, oldIrql);
}

BOOLEAN
NdasPortRemoveFromLpxLocalAddressList(
	PNDASPORT_DRIVER_EXTENSION DriverExtension,
	PTDI_ADDRESS_LPX TdiLpxAddress)
{
	PLIST_ENTRY entry;
	KIRQL oldIrql;

	KeAcquireSpinLock(&DriverExtension->LpxLocalAddressListSpinLock, &oldIrql);
	
	for (entry = DriverExtension->LpxLocalAddressList.Flink;
		entry != &DriverExtension->LpxLocalAddressList;
		entry = entry->Flink)
	{
		PTDI_ADDRESS_LPX_LIST_ENTRY lpxAddressEntry;
		BOOLEAN equal;
	
		lpxAddressEntry = CONTAINING_RECORD(
			entry, 
			TDI_ADDRESS_LPX_LIST_ENTRY, 
			ListEntry);

		equal = RtlEqualMemory(
			lpxAddressEntry->TdiAddress.Node,
			TdiLpxAddress->Node,
			sizeof(TdiLpxAddress->Node));

		if (equal)
		{
			RemoveEntryList(entry);

			KeReleaseSpinLock(&DriverExtension->LpxLocalAddressListSpinLock, oldIrql);

			ExFreePoolWithTag(
				lpxAddressEntry,
				NDASPORT_TAG_TDI);

			InterlockedIncrement(&DriverExtension->LpxLocalAddressListUpdateCounter);

			//
			// removed
			//
			DebugPrint((0, "\tLocal LPX Address Removed: %02X:%02X:%02X:%02X:%02X:%02X\n",
				TdiLpxAddress->Node[0], TdiLpxAddress->Node[1],
				TdiLpxAddress->Node[2], TdiLpxAddress->Node[3],
				TdiLpxAddress->Node[4], TdiLpxAddress->Node[5]));

			return TRUE;
		}
	}

	KeReleaseSpinLock(&DriverExtension->LpxLocalAddressListSpinLock, oldIrql);

	//
	// not removed
	//

	return FALSE;
}


NTSTATUS
NdasPortGetLpxLocalAddressList(
	__inout PTDI_ADDRESS_LPX TdiLpxAddressBuffer,
	__inout PULONG TdiLpxAddressCount,
	__out PULONG TotalAddressCount,
	__out PULONG UpdateCounter)
{
	PNDASPORT_DRIVER_EXTENSION driverExtension;
	PLIST_ENTRY entry;
	KIRQL oldIrql;
	ULONG bufferCount, i;

	driverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	//
	// TODO: Clarify UpdateCounter coherency and usage
	//
	*UpdateCounter = driverExtension->LpxLocalAddressListUpdateCounter;

	if (NULL == TdiLpxAddressBuffer || 
		NULL == TdiLpxAddressCount ||
		0 == *TdiLpxAddressCount)
	{
		return STATUS_SUCCESS;
	}

	bufferCount = *TdiLpxAddressCount;
	*TdiLpxAddressCount = 0;

	KeAcquireSpinLock(&driverExtension->LpxLocalAddressListSpinLock, &oldIrql);

	for (entry = driverExtension->LpxLocalAddressList.Flink, i = 0;
		entry != &driverExtension->LpxLocalAddressList;
		entry = entry->Flink, ++i)
	{
		if (i < bufferCount)
		{
			PTDI_ADDRESS_LPX_LIST_ENTRY lpxAddressEntry;

			lpxAddressEntry = CONTAINING_RECORD(
				entry, 
				TDI_ADDRESS_LPX_LIST_ENTRY, 
				ListEntry);

			RtlCopyMemory(
				&TdiLpxAddressBuffer[i],
				&lpxAddressEntry->TdiAddress,
				sizeof(TDI_ADDRESS_LPX));

			++(*TdiLpxAddressCount);
		}
	}

	*TotalAddressCount = i;

	KeReleaseSpinLock(&driverExtension->LpxLocalAddressListSpinLock, oldIrql);

	return (bufferCount < i) ? STATUS_MORE_ENTRIES : STATUS_SUCCESS;
}

PNDASPORT_FDO_EXTENSION*
NdasPortpCreateFdoExtensionArray(
	__in PNDASPORT_DRIVER_EXTENSION DriverExtension,
	__out PULONG ArraySize)
{
	PNDASPORT_FDO_EXTENSION* fdoExtensionArray;
	KIRQL oldIrql;
	ULONG count, i;
	ULONG removed;
	PLIST_ENTRY entry;

	*ArraySize = 0;

	KeAcquireSpinLock(&DriverExtension->FdoListSpinLock, &oldIrql);

	count = 0;

	for (entry = DriverExtension->FdoList.Flink;
		entry != &DriverExtension->FdoList;
		entry = entry->Flink) 
	{
		++count;
	}

	if (0 == count)
	{
		KeReleaseSpinLock(&DriverExtension->FdoListSpinLock, oldIrql);
		return NULL;
	}

	//
	// As a SpinLock is acquired, we can only use NonPagedPool
	// 

	fdoExtensionArray = (PNDASPORT_FDO_EXTENSION*) ExAllocatePoolWithTag(
		NonPagedPool, 
		sizeof(PNDASPORT_FDO_EXTENSION) * count, 
		NDASPORT_TAG_TDI_PNP);

	if (NULL == fdoExtensionArray)
	{
		KeReleaseSpinLock(&DriverExtension->FdoListSpinLock, oldIrql);
		return NULL;
	}

	i = 0;

	for (entry = DriverExtension->FdoList.Flink;
		entry != &DriverExtension->FdoList;
		entry = entry->Flink) 
	{
		PNDASPORT_FDO_EXTENSION FdoExtension;

		FdoExtension = CONTAINING_RECORD(
			entry, 
			NDASPORT_FDO_EXTENSION, 
			Link);

		removed = NpAcquireRemoveLock(
			FdoExtension->CommonExtension, 
			NdasPortpCreateFdoExtensionArray);

		if (removed) 
		{
			continue;
		}

		fdoExtensionArray[i++] = FdoExtension;
	}

	KeReleaseSpinLock(&DriverExtension->FdoListSpinLock, oldIrql);

	*ArraySize = i;
	return fdoExtensionArray;
}

VOID
NdasPortpDeleteFdoExtensionArray(
	__in PNDASPORT_FDO_EXTENSION* FdoExtensionArray,
	__in ULONG ArraySize)
{
	ULONG i;

	for (i = 0; i < ArraySize; ++i)
	{
		NpReleaseRemoveLock(
			FdoExtensionArray[i]->CommonExtension,
			NdasPortpCreateFdoExtensionArray);
	}

	ExFreePoolWithTag(
		FdoExtensionArray,
		NDASPORT_TAG_TDI_PNP);
}

VOID
NdasPortBindingChangeHandler(
	__in TDI_PNP_OPCODE PnpOpCode,
	__in PUNICODE_STRING TransportName,
	__in PWSTR BindingList)
{
	PNDASPORT_DRIVER_EXTENSION DriverExtension;
	PNDASPORT_FDO_EXTENSION* fdoExtensionArray;
	ULONG i, count;

	PAGED_CODE();

	DriverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != DriverExtension);

	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_WARNING,
		"NdasPortBindingChangeHandler: PnpOpCode=%s(%x), TransportName=%wZ,\n",
		DbgTdiPnpOpCodeString(PnpOpCode),
		PnpOpCode,
		TransportName);

	fdoExtensionArray = NdasPortpCreateFdoExtensionArray(
		DriverExtension, &count);

	if (NULL != fdoExtensionArray)
	{
		for (i = 0; i < count; ++i)
		{
			NdasPortFdoBindChangeHandler(
				fdoExtensionArray[i],
				PnpOpCode,
				TransportName,
				BindingList);
		}
		NdasPortpDeleteFdoExtensionArray(fdoExtensionArray, count);
	}

	return;
}

VOID
NdasPortAddNetAddressHandler(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{
	PNDASPORT_DRIVER_EXTENSION DriverExtension;

	PNDASPORT_FDO_EXTENSION* fdoExtensionArray;
	ULONG i, count;

	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_WARNING,
		"NdasPortAddNetAddressHandler: %wZ, Type=%04X, "
		"Length=%04X, %02X:%02X:%02X:%02X:%02X:%02X\n",
		DeviceName,
		Address->AddressType,
		Address->AddressLength,
		Address->AddressLength > 0 ? Address->Address[0] : 0x00,
		Address->AddressLength > 1 ? Address->Address[1] : 0x00,
		Address->AddressLength > 2 ? Address->Address[2] : 0x00,
		Address->AddressLength > 3 ? Address->Address[3] : 0x00,
		Address->AddressLength > 4 ? Address->Address[4] : 0x00,
		Address->AddressLength > 5 ? Address->Address[5] : 0x00);

	DriverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != DriverExtension);

	if (NdasPortIsLpxAddress(Address, DeviceName))
	{
		PTDI_ADDRESS_LPX localLpxAddress;

		localLpxAddress = (PTDI_ADDRESS_LPX) &Address->Address[0];
		NdasPortAddToLpxLocalAddressList(DriverExtension, localLpxAddress);

		InterlockedIncrement(&DriverExtension->LpxLocalAddressListUpdateCounter);
	}

	fdoExtensionArray = NdasPortpCreateFdoExtensionArray(
		DriverExtension, &count);

	if (NULL != fdoExtensionArray)
	{
		for (i = 0; i < count; ++i)
		{
			NdasPortFdoAddNetAddressHandler(
				fdoExtensionArray[i],
				Address,
				DeviceName,
				Context);
		}
		NdasPortpDeleteFdoExtensionArray(fdoExtensionArray, count);
	}
}

VOID
NdasPortDelNetAddressHandler(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{
	PNDASPORT_DRIVER_EXTENSION DriverExtension;

	PNDASPORT_FDO_EXTENSION* fdoExtensionArray;
	ULONG i, count;

	DebugPrint((1, "NdasPortDelNetAddressHandler: %wZ Type=%04X Length=%04X %02X%02X%02X:%02X%02X%02X\n",
		DeviceName,
		Address->AddressType,
		Address->AddressLength,
		Address->AddressLength > 0 ? Address->Address[0] : 0x00,
		Address->AddressLength > 1 ? Address->Address[1] : 0x00,
		Address->AddressLength > 2 ? Address->Address[2] : 0x00,
		Address->AddressLength > 3 ? Address->Address[3] : 0x00,
		Address->AddressLength > 4 ? Address->Address[4] : 0x00,
		Address->AddressLength > 5 ? Address->Address[5] : 0x00));

	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_INFORMATION,
		"NdasPortDelNetAddressHandler: %wZ Type=%04X Length=%04X %02X%02X%02X:%02X%02X%02X\n",
		DeviceName,
		Address->AddressType,
		Address->AddressLength,
		Address->AddressLength > 0 ? Address->Address[0] : 0x00,
		Address->AddressLength > 1 ? Address->Address[1] : 0x00,
		Address->AddressLength > 2 ? Address->Address[2] : 0x00,
		Address->AddressLength > 3 ? Address->Address[3] : 0x00,
		Address->AddressLength > 4 ? Address->Address[4] : 0x00,
		Address->AddressLength > 5 ? Address->Address[5] : 0x00);

	DriverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != DriverExtension);

	if (NdasPortIsLpxAddress(Address, DeviceName))
	{
		PTDI_ADDRESS_LPX localLpxAddress;

		localLpxAddress = (PTDI_ADDRESS_LPX) &Address->Address[0];
		NdasPortRemoveFromLpxLocalAddressList(DriverExtension, localLpxAddress);

		InterlockedIncrement(&DriverExtension->LpxLocalAddressListUpdateCounter);
	}

	fdoExtensionArray = NdasPortpCreateFdoExtensionArray(
		DriverExtension, &count);

	if (NULL != fdoExtensionArray)
	{
		for (i = 0; i < count; ++i)
		{
			NdasPortFdoDelNetAddressHandler(
				fdoExtensionArray[i],
				Address,
				DeviceName,
				Context);
		}
		NdasPortpDeleteFdoExtensionArray(fdoExtensionArray, count);
	}
}

NTSTATUS
NdasPortTdiPowerHandler(
	__in PUNICODE_STRING DeviceName,
	__in PNET_PNP_EVENT PowerEvent,
	__in PTDI_PNP_CONTEXT Context1,
	__in PTDI_PNP_CONTEXT Context2)
{
	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_WARNING,
		"TdiPnpPowerChange: DeviceName=%wZ, Event=%x\n", 
		DeviceName,
		PowerEvent->NetEvent);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasPortRegisterForNetworkNotification(
	__out HANDLE* BindingHandle)
{
	NTSTATUS status;
	UNICODE_STRING clientName;
	TDI_CLIENT_INTERFACE_INFO clientInterfaceInfo;

	RtlInitUnicodeString(&clientName, NDASPORT_SERVICE_NAME);

	RtlZeroMemory(&clientInterfaceInfo, sizeof(TDI_CLIENT_INTERFACE_INFO));

	clientInterfaceInfo.MajorTdiVersion = TDI_CURRENT_MAJOR_VERSION;
	clientInterfaceInfo.MinorTdiVersion = TDI_CURRENT_MINOR_VERSION;

	clientInterfaceInfo.Unused = 0;
	clientInterfaceInfo.ClientName = &clientName;

	clientInterfaceInfo.BindingHandler = NdasPortBindingChangeHandler;
	clientInterfaceInfo.AddAddressHandlerV2 = NdasPortAddNetAddressHandler;
	clientInterfaceInfo.DelAddressHandlerV2 = NdasPortDelNetAddressHandler;
	clientInterfaceInfo.PnPPowerHandler = NdasPortTdiPowerHandler;

	status = TdiRegisterPnPHandlers(
		&clientInterfaceInfo,
		sizeof(TDI_CLIENT_INTERFACE_INFO),
		BindingHandle);

	if (!NT_SUCCESS(status))
	{
		NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_FATAL,
			"TdiRegisterPnPHandlers failed, Status=%X\n", status);
	}

	return status;
}
