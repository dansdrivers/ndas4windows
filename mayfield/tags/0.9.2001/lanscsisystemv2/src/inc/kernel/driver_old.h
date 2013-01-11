/*++
Copyright (c) 1990-2000    Microsoft Corporation All Rights Reserved

Module Name:

    driver.h

Abstract:

    This module contains the common declarations for the 
    bus, function and filter drivers.

Author:

     Eliyas Yakub   Sep 16, 1998
     
Environment:

    kernel mode only

Notes:


Revision History:


--*/

#ifdef __LANSCSI_BUS__
#include "lanscsibus.h"
#else
#include "public.h"
#endif

//
// Define an Interface Guid to access the proprietary toaster interface.
// This guid is used to identify a specific interface in IRP_MN_QUERY_INTERFACE
// handler.
//

#ifdef __LANSCSI_BUS__
DEFINE_GUID(GUID_LANSCSI_INTERFACE_STANDARD, 
		0xf3f32f99, 0x8383, 0x41c4, 0x84, 0x94, 0x1b, 0x18, 0x9c, 0xad, 0xf5, 0x82);
// {F3F32F99-8383-41c4-8494-1B189CADF582}
#else
DEFINE_GUID(GUID_TOASTER_INTERFACE_STANDARD, 
        0xe0b27630, 0x5434, 0x11d3, 0xb8, 0x90, 0x0, 0xc0, 0x4f, 0xad, 0x51, 0x71);
// {E0B27630-5434-11d3-B890-00C04FAD5171}
#endif

//
// Define a Guid for toaster bus type. This is returned in response to
// IRP_MN_QUERY_BUS_INTERFACE on PDO.
//

#ifdef __LANSCSI_BUS__
DEFINE_GUID(GUID_LANSCSI_BUS_TYPE, 
		0xd1f671c0, 0xd291, 0x41d6, 0x9a, 0x4c, 0x7b, 0xd5, 0x37, 0x0, 0x42, 0xc3);
// {D1F671C0-D291-41d6-9A4C-7BD5370042C3}
#else
DEFINE_GUID(GUID_TOASTER_BUS_TYPE, 
        0xaf5b4200, 0x6d32, 0x11d3, 0xb8, 0x9c, 0x0, 0xc0, 0x4f, 0xad, 0x51, 0x71);
// {AF5B4200-6D32-11d3-B89C-00C04FAD5171}
#endif

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

#ifndef __DRIVER_H
#define __DRIVER_H

//
// Define Interface reference/dereference routines for
//  Interfaces exported by IRP_MN_QUERY_INTERFACE
//

typedef VOID (*PINTERFACE_REFERENCE)(PVOID Context);
typedef VOID (*PINTERFACE_DEREFERENCE)(PVOID Context);

typedef
BOOLEAN
(*PTOASTER_GET_CRISPINESS_LEVEL)(
                           IN   PVOID Context,
                           OUT  PUCHAR Level
                               );

typedef
BOOLEAN
(*PTOASTER_SET_CRISPINESS_LEVEL)(
                           IN   PVOID Context,
                           OUT  UCHAR Level
                               );

typedef
BOOLEAN
(*PTOASTER_IS_CHILD_PROTECTED)(
                             IN PVOID Context
                             );

//
// Interface for getting and setting power level etc.,
//
typedef struct _TOASTER_INTERFACE_STANDARD {
   USHORT                           Size;
   USHORT                           Version;
   PINTERFACE_REFERENCE             InterfaceReference;
   PINTERFACE_DEREFERENCE           InterfaceDereference;
   PVOID                            Context;
   PTOASTER_GET_CRISPINESS_LEVEL    GetCrispinessLevel;
   PTOASTER_SET_CRISPINESS_LEVEL    SetCrispinessLevel;
   PTOASTER_IS_CHILD_PROTECTED      IsSafetyLockEnabled; //):
} TOASTER_INTERFACE_STANDARD, *PTOASTER_INTERFACE_STANDARD;


#endif



