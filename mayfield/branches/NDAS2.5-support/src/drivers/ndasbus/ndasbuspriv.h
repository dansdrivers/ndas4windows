	//	Close handles
	//

	if(NdasDeviceReg)
		ZwClose(NdasDeviceReg);
	if(DeviceReg)
		ZwClose(DeviceReg);

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
	KeLeaveCriticalRegion();

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Worker to enumerate NDAS devices at late time.
//

#define NDBUSWRK_MAX_ADDRESSLEN	128

typedef struct _NDBUS_ENUMWORKER {

	PIO_WORKITEM		IoWorkItem;
	PFDO_DEVICE_DATA	FdoData;
	BOOLEAN				AddedAddressVaild;
	UCHAR				AddedTaAddress[NDBUSWRK_MAX_ADDRESSLEN];
	ULONG				PlugInTimeMask;

} NDBUS_ENUMWORKER, *PNDBUS_ENUMWORKER;

static
VOID
EnumWorker(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID Context
){
	PNDBUS_ENUMWORKER	ctx = (PNDBUS_ENUMWORKER)Context;
	PFDO_DEVICE_DATA	fdoData = ctx->FdoData;
	ULONG				timeMask = ctx->PlugInTimeMask;

	UNREFERENCED_PARAMETER(DeviceObject);


	//
	//	IO_WORKITEM is rare resource, give it back to the system now.
	//

	IoFreeWorkItem(ctx->IoWorkItem);


	//
	//	Start enumerating
	//

	if(ctx->AddedAddressVaild) {
		TA_LSTRANS_ADDRESS			bindAddr;
		PTA_ADDRESS					addedAddress = (PTA_ADDRESS)ctx->AddedTaAddress;

		bindAddr.TAAddressCount = 1;
		bindAddr.Address[0].AddressLength = addedAddress->AddressLength;
		bindAddr.Address[0].AddressType = addedAddress->AddressType;
		RtlCopyMemory(&bindAddr.Address[0].Address, addedAddress->Address, addedAddress->AddressLength);

		//
		//	We have to wait for the added address is accessible.
		//	Especially, we can not open LPX device early time on MP systems.
		//

		LstransWaitForAddress(&bindAddr, 10);

		LSBus_PlugInDeviceFromRegistry(fdoData, addedAddress, timeMask);
	}
	else
		LSBus_PlugInDeviceFromRegistry(fdoData,  NULL, timeMask);

	ExFreePool(Context);

}


//
//	Queue a workitem to plug in NDAS device by reading registry.
//

NTSTATUS
LSBUS_QueueWorker_PlugInByRegistry(
		PFDO_DEVICE_DATA	FdoData,
		PTA_ADDRESS			AddedAddress,
		ULONG				PlugInTimeMask
	) {
	NTSTATUS			status;
	PNDBUS_ENUMWORKER	workItemCtx;
	ULONG				addrLen;

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("entered.\n"));

	//
	//	Parameter check
	//
	if(!FdoData) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FdoData NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}


	KeEnterCriticalRegion();
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	if(FdoData->PersistentPdo == FALSE) {
		ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
		KeLeaveCriticalRegion();

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PersistentPDO has been off.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);
	KeLeaveCriticalRegion();

	workItemCtx = (PNDBUS_ENUMWORKER)ExAllocatePoolWithTag(NonPagedPool, sizeof(NDBUS_ENUMWORKER), NDBUSREG_POOLTAG_WORKITEM);
	if(!w