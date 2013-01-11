#pragma once

#ifdef RUN_WPP

#include <wpp/ndassvc.wpp.h>
#include <wpp/lpxtrans.wpp.h>

#define WPP_CONTROL_GUIDS \
	NDASSVC_WPP_CONTROL_GUIDS \
	LPXTRANS_WPP_CONTROL_GUIDS

#define WPP_LEVEL_FLAG_LOGGER(_Level,_Flags) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_LEVEL_FLAG_ENABLED(_Level,_Flags) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#define WPP_FLAG_LEVEL_LOGGER(_Flags,_Level) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_FLAG_LEVEL_ENABLED(_Flags,_Level) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#else

#define NDASSVC_GENERAL             0x00000001
#define NDASSVC_INIT                0x00000002
#define NDASSVC_PNP                 0x00000004
#define NDASSVC_HIXSERVER           0x00000008
#define NDASSVC_HIXCLIENT           0x00000010
#define NDASSVC_CMDSERVER           0x00000020
#define NDASSVC_CMDPROCESSOR        0x00000040
#define NDASSVC_NDASDEVICEREGISTRAR 0x00000080
#define NDASSVC_NDASLOGDEVMANAGER   0x00000100
#define NDASSVC_NDASDEVICE          0x00000200
#define NDASSVC_NDASUNITDEVICE      0x00000400
#define NDASSVC_NDASLOGDEVICE       0x00000800
#define NDASSVC_SYSTEMUTIL          0x00001000
#define NDASSVC_AUTOREG             0x00002000
#define NDASSVC_HEARTBEAT           0x00004000
#define NDASSVC_HIXUTIL             0x00008000
#define NDASSVC_EVENTPUB            0x00010000
#define NDASSVC_NDASCOMM            0x00020000
#define NDASSVC_EVENTMON            0x00040000
#define NDASSVC_LPXCOMM             0x00080000
#define NDASSVC_IX                  0x00100000

#endif

#include <xtl/xtltrace.h>

#ifndef COMVERIFY
#define COMVERIFY(x) XTLVERIFY(SUCCEEDED(x))
#endif

