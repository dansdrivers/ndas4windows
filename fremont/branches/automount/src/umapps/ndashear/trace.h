#pragma once

#ifdef RUN_WPP

#include <wpp/ndashear.wpp.h>

#define WPP_CONTROL_GUIDS \
	NDASSVC_WPP_CONTROL_GUIDS

#define WPP_LEVEL_FLAG_LOGGER(_Level,_Flags) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_LEVEL_FLAG_ENABLED(_Level,_Flags) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#define WPP_FLAG_LEVEL_LOGGER(_Flags,_Level) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_FLAG_LEVEL_ENABLED(_Flags,_Level) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#else

#define NDASHEAR_GENERAL 0x00000001

#endif

#include <xtl/xtltrace.h>

#ifndef COMVERIFY
#define COMVERIFY(x) XTLVERIFY(SUCCEEDED(x))
#endif

