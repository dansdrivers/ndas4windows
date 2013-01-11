#ifndef __NDAS_BUS_PROC_H__
#define __NDAS_BUS_PROC_H__

#define __NDAS_SCSI_BUS__	1

#include <ntddk.h>
#include <initguid.h>

#include <stdarg.h>
#include <stdio.h>

#include <wdmguid.h>
#include <wmistr.h>
#include <wmilib.h>
#include <devreg.h>

#include <scsi.h>
#include <ntddscsi.h>
#include <ntintsafe.h>

#include "ndascommonheader.h"

extern BOOLEAN NdasTestBug;

#define NDAS_ASSERT( exp )				\
	((NdasTestBug && (exp) == FALSE) ?	\
	NdasDbgBreakPoint() :				\
	FALSE)

#if DBG
#undef INLINE
#define INLINE
#else
#undef INLINE
#define INLINE __inline
#endif

#include "lsprotospec.h"
#include "lsprotoidespec.h"

#include "ndas/ndasdib.h"

#include "ndasscsi.h"
#include "ndasklib.h"

#include "ndasbusioctl.h"

#include "lpxtdiv2.h"

#include "hdreg.h"
#include "public\ndas\ndasiomsg.h"

#include "driver.h"

#include "ndasbus.h"
#include "busenum.h"
#include "ndasbuspriv.h"

#include "binparams.h"


// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions

#include "ndasbus.ver"
#include <ndasverp.h>



#endif