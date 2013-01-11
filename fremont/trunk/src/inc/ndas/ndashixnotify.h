#pragma once
#include "ndastypeex.h"
#include "ndashix.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL
WINAPI
NdasHixNotifyUnitDeviceChange(CONST NDAS_UNITDEVICE_ID* pUnitDeviceId);

BOOL
WINAPI
NdasHixNotifyDeviceChange(CONST NDAS_DEVICE_ID* pDeviceId);

#ifdef __cplusplus
}
#endif
