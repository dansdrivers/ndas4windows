#include <winsock2.h>
#include <crtdbg.h>
#include <lpxwsock.h>
#include <ndas/ndascomm.h>
#include <ndas/ndasid.h>
#include <ndas/ndashixnotify.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>

BOOL
NdasCommNotifypGetNdasDeviceId(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__out NDAS_DEVICE_ID* DeviceId)
{
	BOOL success;

	switch (Class)
	{
	case NDAS_DIC_NDAS_IDA:
		success = NdasIdStringToDeviceExA(
			static_cast<LPCSTR>(Identifier), DeviceId, NULL, NULL);
		break;
	case NDAS_DIC_NDAS_IDW:
		success = NdasIdStringToDeviceExW(
			static_cast<LPCWSTR>(Identifier), DeviceId, NULL, NULL);
		break;
	case NDAS_DIC_DEVICE_ID:
		if (IsBadReadPtr(Identifier, sizeof(NDAS_DEVICE_ID)))
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
		CopyMemory(DeviceId, Identifier, sizeof(NDAS_DEVICE_ID));
		success = TRUE;
		break;
	default:
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	return success;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyDeviceChange(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__in_opt LPCGUID HostIdentifier)
{
	NDAS_DEVICE_ID deviceId;

	BOOL success = NdasCommNotifypGetNdasDeviceId(Class, Identifier, &deviceId);

	if (!success)
	{
		return FALSE;
	}

	success = NdasHixNotifyDeviceChange(&deviceId);

	return success;
}

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyUnitDeviceChange(
	__in NDAS_DEVICE_ID_CLASS Class,
	__in CONST VOID* Identifier,
	__in DWORD UnitNumber,
	__in_opt LPCGUID HostIdentifier)
{
	NDAS_UNITDEVICE_ID unitDeviceId;
	unitDeviceId.UnitNo = UnitNumber;

	BOOL success = NdasCommNotifypGetNdasDeviceId(Class, Identifier, &unitDeviceId.DeviceId);
	if (!success)
	{
		return FALSE;
	}

	success = NdasHixNotifyUnitDeviceChange(&unitDeviceId);

	return success;
}
