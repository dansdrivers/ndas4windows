/*++

  NDAS Service Parameter Definitions

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

  Remarks:

  This header contains the structures for manipulating service parameters

--*/

#ifndef _NDAS_SERVICE_PARAM_H_
#define _NDAS_SERVICE_PARAM_H_

#pragma once

/* All structures in this header are unaligned. */
#include <pshpack1.h>

/* Constants */
#define NDASSVC_PARAM_SUSPEND_PROCESS 0x0010
/* This parameter uses DwordValue */
#define NDASSVC_SUSPEND_DENY		0x0001
#define NDASSVC_SUSPEND_ALLOW		0x0002
// #define NDASSVC_SUSPEND_EJECT_ALL	0x0002
// #define NDASSVC_SUSPEND_UNPLUG_ALL	0x0003

#define NDASSVC_PARAM_USE_CONTENT_ENCRYPTION 0x0020
/* This parameter uses BoolValue */

/* NDAS Service Parameters */
#define NDASSVC_PARAM_BYTE_VALUE_MAX   56

typedef struct _NDAS_SERVICE_PARAM
{
	DWORD ParamCode;
	union {
		BOOL BoolValue;
		DWORD DwordValue;
		struct {
			DWORD cbByteValue;
			BYTE ByteValue[NDASSVC_PARAM_BYTE_VALUE_MAX];
		} ArrayValue;
	};
} NDAS_SERVICE_PARAM, *PNDAS_SERVICE_PARAM;

/* End of packing */
#include <poppack.h>

#endif /* _NDAS_SERVICE_PARAM_H_ */
