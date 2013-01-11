#include "port.h"

#ifdef RUN_WPP
#include "network.tmh"
#endif

VOID
NdasPortBindingChangeHandler(
	__in TDI_PNP_OPCODE PnPOpcode,
	__in PUNICODE_STRING TransportName,
	__in PWSTR BindingList)
{
	PNDASPORT_DRIVER_EXTENSION driverExtension;

	PAGED_CODE();

	driverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	DebugPrint((1, 
		"NdasPortBindingChangeHandler: PnPOpcode=%s(%x), TransportName=%wZ,\n", 
		DbgTdiPnpOpCodeString(PnPOpcode),
		PnPOpcode, 
		TransportName));

	NdasPortTrace(NDASPORT_GENERAL, TRACE_LEVEL_INFORMATION,
		"NdasPortBindingChangeHandler: PnpOpCode=%s(%x), TransportName=%wZ,\n",
		DbgTdiPnpOpCodeString(PnPOpcode),
		PnPOpcode,
		TransportName);

	switch (PnPOpcode) 
	{
	case TDI_PNP_OP_ADD:
	case TDI_PNP_OP_DEL:
	case TDI_PNP_OP_PROVIDERREADY:
		break;

	case TDI_PNP_OP_NETREADY:
		{
			BOOLEAN previouslyReady;

			previouslyReady = driverExtension->IsNetworkReady;
			driverExtension->IsNetworkReady = TRUE;

			//
			// State change from network "not ready" to
			// network "ready" state. Will initialize
			// device detection
			//
			if (previouslyReady == FALSE) 
			{
				KIRQL oldIrql;
				PLIST_ENTRY entry;
				DebugPrint((3, "Network is now ready!\n"));

				//
				// Request device enumeration request from PnP.
				//

				KeAcquireSpinLock(&driverExtension->FdoListSpinLock, &oldIrql);
				for (entry = driverExtension->FdoList.Flink;
					entry != &driverExtension->FdoList;
					entry = entry->Flink) 
				{
					PNDASPORT_FDO_EXTENSION fdoExtension;
					fdoExtension = CONTAINING_RECORD(entry, NDASPORT_FDO_EXTENSION, Link);
					IoInvalidateDeviceRelations(
						fdoExtension->LowerPdo,
						BusRelations);
				}
				KeReleaseSpinLock(&driverExtension->FdoListSpinLock, oldIrql);
			}

			break;
		}
	default: 
		{
			break;
		}
	}

	return;
}

BOOLEAN
NdasPortIsLpxAddress(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName)
{
	static CONST WCHAR LPX_DEVICE_NAME_PREFIX[] = L"\\device\\lpx" L"_";
	UNICODE_STRING lpxDeviceNamePrefixString;
	BOOLEAN isPrefix;

	RtlInitUnicodeString(&lpxDeviceNamePrefixString, LPX_DEVICE_NAME_PREFIX);

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
		&lpxDeviceNamePrefixString, 
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
			"%02X:%02X:%02X-%02X:%02X:%02X\n",
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

	DebugPrint((0, "\tLocal LPX Address Added: %02X:%02X:%02X:%02X:%02X:%02X\n",
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
	
	entry = DriverExtension->LpxLocalAddressList.Flink;
	while (entry != &DriverExtension->LpxLocalAddressList)
	{
		PTDI_ADDRESS_LPX_LIST_ENTRY lpxAddressEntry;

		lpxAddressEntry = CONTAINING_RECORD(
			entry, 
			TDI_ADDRESS_LPX_LIST_ENTRY, 
			ListEntry);

		//
		// Forward the link first
		//
		entry = entry->Flink;
		
		//
		// And remove the actual entry
		//
		RemoveEntryList(&lpxAddressEntry->ListEntry);
		
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

VOID
NdasPortAddNetAddressHandler(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{
	PNDASPORT_DRIVER_EXTENSION driverExtension;
	PTDI_ADDRESS_LPX localAddress;

	DebugPrint((1, "NdasPortAddNetAddressHandler: %wZ Type=%04X Length=%04X %02X:%02X:%02X:%02X:%02X:%02X\n",
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
		"NdasPortAddNetAddressHandler: %wZ Type=%04X Length=%04X %02X:%02X:%02X:%02X:%02X:%02X\n",
		DeviceName,
		Address->AddressType,
		Address->AddressLength,
		Address->AddressLength > 0 ? Address->Address[0] : 0x00,
		Address->AddressLength > 1 ? Address->Address[1] : 0x00,
		Address->AddressLength > 2 ? Address->Address[2] : 0x00,
		Address->AddressLength > 3 ? Address->Address[3] : 0x00,
		Address->AddressLength > 4 ? Address->Address[4] : 0x00,
		Address->AddressLength > 5 ? Address->Address[5] : 0x00);


	if (!NdasPortIsLpxAddress(Address, DeviceName))
	{
		return;
	}

	driverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	localAddress = (PTDI_ADDRESS_LPX) &Address->Address[0];
	NdasPortAddToLpxLocalAddressList(driverExtension, localAddress);

	InterlockedIncrement(&driverExtension->LpxLocalAddressListUpdateCounter);

	//
	// LPX Address is found!
	//
}

VOID
NdasPortDelNetAddressHandler(
	__in PTA_ADDRESS Address,
	__in PUNICODE_STRING DeviceName,
	__in PTDI_PNP_CONTEXT Context)
{
	PNDASPORT_DRIVER_EXTENSION driverExtension;
	PTDI_ADDRESS_LPX localAddress;

	DebugPrint((1, "NdasPortDelNetAddressHandler: %wZ Type=%04X Length=%04X %02X:%02X:%02X:%02X:%02X:%02X\n",
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
		"NdasPortDelNetAddressHandler: %wZ Type=%04X Length=%04X %02X:%02X:%02X:%02X:%02X:%02X\n",
		DeviceName,
		Address->AddressType,
		Address->AddressLength,
		Address->AddressLength > 0 ? Address->Address[0] : 0x00,
		Address->AddressLength > 1 ? Address->Address[1] : 0x00,
		Address->AddressLength > 2 ? Address->Address[2] : 0x00,
		Address->AddressLength > 3 ? Address->Address[3] : 0x00,
		Address->AddressLength > 4 ? Address->Address[4] : 0x00,
		Address->AddressLength > 5 ? Address->Address[5] : 0x00);

	if (!NdasPortIsLpxAddress(Address, DeviceName))
	{
		return;
	}

	driverExtension = (PNDASPORT_DRIVER_EXTENSION) IoGetDriverObjectExtension(
		NdasPortDriverObject,
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

	localAddress = (PTDI_ADDRESS_LPX) &Address->Address[0];
	NdasPortRemoveFromLpxLocalAddressList(driverExtension, localAddress);

	InterlockedIncrement(&driverExtension->LpxLocalAddressListUpdateCounter);
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
	clientInterfaceInfo.PnPPowerHandler = NULL;

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

