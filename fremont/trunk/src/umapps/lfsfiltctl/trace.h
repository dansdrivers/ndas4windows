#pragma once

#ifdef RUN_WPP

#include <wpp/wppmac.h>
#include <wpp/lfsfiltctl.wpp.h>

#define WPP_CONTROL_GUIDS \
	LFSFILTCTL_WPP_CONTROL_GUIDS

#else

#define xtlTraceGeneric 0x00000001

#endif

#include <xtl/xtltrace.h>
#ifndef COMVERIFY
#define COMVERIFY(x) XTLVERIFY(SUCCEEDED(x))
#endif
