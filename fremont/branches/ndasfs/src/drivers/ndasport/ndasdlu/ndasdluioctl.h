#ifndef _NDASDLU_IOCTL_H_
#define _NDASDLU_IOCTL_H_
#pragma once

#include <ndas/ndasportioctl.h>

// Suppress error C4201: nonstandard extension used : nameless struct/union
#pragma warning( push )
#pragma warning( disable : 4201 )

#include "ndasscsiioctl.h"

#pragma warning( pop )

#include <pshpack8.h>

//
// NDAS DLU descriptor
//

typedef struct _NDAS_DLU_DESCRIPTOR {
	NDAS_LOGICALUNIT_DESCRIPTOR Header;
	LURELATION_DESC				LurDesc;
} NDAS_DLU_DESCRIPTOR, *PNDAS_DLU_DESCRIPTOR;


#include <poppack.h>

//
// NDAS DLU driver's service name
// Must be the same as in inf file.
//

#define	NDAS_DLU_SVCNAME						L"ndasdlu"


typedef struct _NDAS_DLU_EVENT {
	ULONG DluInternalStatus;
	ULONG LogicalUnitAddress;
} NDAS_DLU_EVENT, *PNDAS_DLU_EVENT;


#endif /* _FILEDISK_IOCTL_H_ */
