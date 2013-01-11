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

/* 8 byte packing of structures */
#include <pshpack8.h>

/* Constants */

/*

Advise a service how to process suspend/hibernation query from the system.
Suspend allow or deny is effective only if there are any mounted
NDAS device in the system. If no device is mounted, suspend query is 
always granted.

*/

/*@@NDASSVC_PARAM_CODE */
/* <TITLE NDASSVC_PARAM_CODE> */

/* <COMBINE NDASSVC_PARAM_CODE> */
#define NDASSVC_PARAM_SUSPEND_PROCESS 0x0010

/* This parameter uses DwordValue */
/* <COMBINE NDASSVC_PARAM_CODE> */
#define NDASSVC_SUSPEND_DENY		0x0001
/* <COMBINE NDASSVC_PARAM_CODE> */
#define NDASSVC_SUSPEND_ALLOW		0x0002
/* #define NDASSVC_SUSPEND_EJECT_ALL	0x0002 */
/* #define NDASSVC_SUSPEND_UNPLUG_ALL	0x0003 */

/* This parameter uses BoolValue */
/* Obsolete */
/* #define NDASSVC_PARAM_USE_CONTENT_ENCRYPTION 0x0020 */

/* 

Advise a service to reset the system key and re-read from the registry
This is sent after the system key change with 'ndasenc'
   
No parameters are used.

*/
/* <COMBINE NDASSVC_PARAM_CODE> */
#define NDASSVC_PARAM_RESET_SYSKEY 0x0021

/* NDAS Service Parameters */
/* <COMBINE NDAS_SERVICE_PARAM> */
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

C_ASSERT(64 == sizeof(NDAS_SERVICE_PARAM));

/* End of packing */
#include <poppack.h>

#endif /* _NDAS_SERVICE_PARAM_H_ */
