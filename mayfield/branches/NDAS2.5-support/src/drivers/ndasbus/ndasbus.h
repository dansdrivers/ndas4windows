oData,
		HANDLE				NdasDeviceReg,
		PTA_ADDRESS			AddedAddress,
		ULONG				PlugInTimeMask
	) {

	NTSTATUS					status;
	PKEY_BASIC_INFORMATION		keyInfo;
	ULONG						outLength;
	LONG						idxKey;
	OBJECT_ATTRIBUTES			objectAttributes;
	UNICODE_STRING				objectName;
	HANDLE						ndasDevInst;
	PPDO_DEVICE_DATA			pdoData;
	PBUSENUM_PLUGIN_HARDWARE_EX2	plugIn;
	PLANSCSI_ADD_TARGET_DATA		addTargetData;
	TA_LSTRANS_ADDRESS			bindAddr;

	UNREFERENCED_PARAMETER(PlugInTimeMask);

	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, NDBUSREG_POOLTAG_KEYINFO);
	if(!keyInfo) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = STATUS_SUCCESS;
	for(idxKey = 0 ; idxKey < MAX_DEVICES_IN_NDAS_REGISTRY; idxKey ++) {

		//
		//	Enumerate subkeys under the NDAS device root
		//

		status = ZwEnumerateKey(
						NdasDeviceReg,
						idxKey,
						KeyBasicInformation