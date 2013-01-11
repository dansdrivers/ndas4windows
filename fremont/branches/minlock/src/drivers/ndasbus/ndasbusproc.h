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

#include "driver.h"

#include "ndasbusioctl.h"
#include "ndasbus.h"
#include "busenum.h"
#include "ndasbuspriv.h"

#include "lpxtdiV2.h"

#include "lsklib.h"

#include "ndasscsiioctl.h"
#include "ndas/ndasdib.h"
#include "lsutils.h"
#include "lurdesc.h"
#include "binparams.h"
#include "lsccb.h"
#include "lslur.h"

#include "hdreg.h"

#include "public\ndas\ndasiomsg.h"

#include "lstransport.h"

// When you include module.ver file, you should include <ndasverp.h> also
// in case module.ver does not include any definitions

#include "ndasbus.ver"
#include <ndasverp.h>



#endif