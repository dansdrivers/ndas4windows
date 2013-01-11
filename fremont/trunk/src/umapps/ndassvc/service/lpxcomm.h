eturn 0;
}

const NDAS_LOGICALDEVICE_GROUP& 
CNdasNullUnitDevice::
GetLDGroup() const
{
	XTLASSERT(FALSE);
	return NDAS_LOGICALDEVICE_GROUP_NONE;
}

bool 
CNdasNullUnitDevice::
RegisterToLogicalDeviceManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullRegister\n");
	return true;
}

bool 
CNdasNullUnitDevice::
UnregisterFromLogicalDeviceManager()
{
	XTLTRACE2(NDASSVC_NDASUNITDEVICE, TRACE_LEVEL_INFORMATION, "NullUnregister\n");
	return true;
}

ACCESS_MASK 
CNdasNullUnitDevice::
GetAllowingAccess()
{
	XTLASSERT(FALSE && "Null unit device do not have this property");
	return 0;
}

UINT64 
CNdasNullUnitDevice::
GetUserBlockCount()
{
	XTLASSERT(FALSE && "Null unit device do not have this property");
	return 0;
}

UINT64 
CNdasNullUnitDevice::
GetPhysicalBlockCount()
{
	XTLASSERT(FALSE && "Null unit device do not have this property");
	return 0;
}

BOOL 
CNdasNullUnitDevice::
CheckNDFSCompatibility()
{
	return FALSE;
}

CNdasLogicalDevicePtr 
CNdasNullUnitDevice::
GetLogicalDevice()
{
	return CNdasLogicalDeviceNullPtr;
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Function Implementations
//
//////////////////////////////////////////////////////////////////////////

namespace
{

NDAS_UNITDEVICE_ID 
pCreateUnitDeviceId(
	const CNdasDevicePtr& pDevice,
	DWORD unitNo)
{
	NDAS_UNITDEVICE_ID deviceID = {pDevice->GetDeviceId(), unitNo};
	return deviceID;
}

NDAS_LOGICALDEVICE_TYPE
pUnitDeviceLogicalDeviceType(
	NDAS_UNITDEVICE_TYPE udType,
	NDAS_UNITDEVICE_SUBTYPE udSubtype)
{
	switch (udType) 
	{
	case NDAS_UNITDEVICE_TYPE_DISK:
		switch (udSubtype.DiskDeviceType) 
		{
		case NDAS_UNITDEVICE_DISK_TYPE_SINGLE:
			return NDAS_LOGICALDEVICE_TYPE_DISK_SINGLE;
		case NDAS_UNITDEVICE_DISK_TYPE_CONFLICT:
			return NDAS_LOGICALDEVICE_TYPE_DISK_CONFLICT_DIB;
		case NDAS_UNITDEVICE_DISK_TYPE_VIRTUAL_DVD:
			return NDAS_LOGICALDEVICE_TYPE_VIRTUAL_DVD;
		case NDAS_UNITDEVICE_DISK_TYPE_AGGREGATED:
			return NDAS_LOGICALDEVICE_TYPE_DISK_AGGREGATED;
		case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_MASTER:
		case NDAS_UNITDEVICE_DISK_TYPE_MIRROR_SL