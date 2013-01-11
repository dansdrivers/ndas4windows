/*++

Copyright (c) 2004  Microsoft Corporation

Module Name:

    namelookup.h

Abstract:

    Header file containing the name lookup device extension and name lookup
    function prototypes.


Environment:

    Kernel mode

--*/

#ifndef __NAMELOOKUP_H__
#define __NAMELOOKUP_H__

#include "namelookupdef.h"

//
//  Give pointer alignment characteristics for the different processor types
//

#if defined(_X86_)
#   define DECLSPEC_PTRALIGN __declspec(align(4))
#elif defined(_AMD64_)
#   define DECLSPEC_PTRALIGN __declspec(align(8))
#elif defined(_IA64_)
#   define DECLSPEC_PTRALIGN __declspec(align(8))
#else
#   error "No target architecture defined"
#endif

//
//  These structures are used to retrieve the name of objects.  To prevent
//  allocating memory every time we get a name, and to prevent having large
//  string buffers on the stack, this structure contains a small
//  buffer (which should handle 90+% of all names).  If we do overflow this
//  buffer we will allocate a buffer big enough for the name.
//

typedef struct _NAME_CONTROL {

    //
    //  UNICODE_STRING whos buffer is either SmallBuffer or AllocatedBuffer
    //  if a larger buffer was needed.
    //

    UNICODE_STRING Name;

    //
    //  AllocatedBuffer is used when we need a buffer larger than SmallBuffer.
    //

    PUCHAR AllocatedBuffer;

    //
    //  The size of whatever buffer is currently being used (SmallBuffer or
    //  AllocatedBuffer) in bytes.
    //

    ULONG BufferSize;

    //
    //  This is the buffer that we start out with.  The thinking is that this
    //  should be large enough for most names.
    //

    DECLSPEC_PTRALIGN UCHAR SmallBuffer[254];

} NAME_CONTROL, *PNAME_CONTROL;


//
//  NL_EXTENSION is the part of a device extension that is needed
//  by the name lookup routines.  All the non-namelookup data contained
//  here should be needed by any filter.  Simply use this as part of
//  any filter's device extension.
//

typedef struct _NL_DEVICE_EXTENSION_HEADER {

    //
    //  Device Object this device extension is attached to
    //

    PDEVICE_OBJECT ThisDeviceObject;

    //
    //  Device object this filter is directly attached to
    //

    PDEVICE_OBJECT AttachedToDeviceObject;

    //
    //  When attached to Volume Device Objects, the physical device object
    //  that represents that volume.  NULL when attached to Control Device
    //  objects.
    //

    PDEVICE_OBJECT StorageStackDeviceObject;

    //
    //  DOS representation of the device name.
    //

    UNICODE_STRING DosName;

    //
    //  Name for this device.  If attached to a Volume Device Object it is the
    //  name of the physical disk drive.  If attached to a Control Device
    //  Object it is the name of the Control Device Object.  This is in the
    //  "\Device\...\" format.
    //

    UNICODE_STRING DeviceName;

} NL_DEVICE_EXTENSION_HEADER, *PNL_DEVICE_EXTENSION_HEADER;



/////////////////////////////////////////////////////////////////////////////
//
//  Name lookup functions.
//
/////////////////////////////////////////////////////////////////////////////

NTSTATUS
NLGetFullPathName (
    __in PFILE_OBJECT FileObject,
    __inout PNAME_CONTROL FileNameControl,
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader,
    __in NAME_LOOKUP_FLAGS LookupFlags,
    __in PPAGED_LOOKASIDE_LIST LookasideList,
    __out PBOOLEAN CacheName
    );

PNAME_CONTROL
NLGetAndAllocateObjectName (
    __in PVOID Object,
    __in PPAGED_LOOKASIDE_LIST LookasideList
    );

NTSTATUS
NLGetObjectName (
    __in PVOID Object,
    __inout PNAME_CONTROL ObjectNameCtrl
    );

VOID
NLGetDosDeviceName (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader
    );

/////////////////////////////////////////////////////////////////////////////
//
//  General support routines
//
/////////////////////////////////////////////////////////////////////////////

NTSTATUS
NLAllocateAndCopyUnicodeString (
    __inout PUNICODE_STRING DestName,
    __in PUNICODE_STRING SrcName,
    __in ULONG PoolTag
    );

/////////////////////////////////////////////////////////////////////////////
//
//  Name lookup device extension header functions.
//
/////////////////////////////////////////////////////////////////////////////

VOID
NLInitDeviceExtensionHeader (
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader,
    __in PDEVICE_OBJECT ThisDeviceObject,
    __in_opt PDEVICE_OBJECT StorageStackDeviceObject
    );

VOID
NLCleanupDeviceExtensionHeader(
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader
    );

/////////////////////////////////////////////////////////////////////////////
//
//  Routines to support generic name control structures that allow us
//  to get names of arbitrary size.
//
/////////////////////////////////////////////////////////////////////////////

NTSTATUS
NLAllocateNameControl (
    __out PNAME_CONTROL *NameControl,
    __in PPAGED_LOOKASIDE_LIST LookasideList
    );

VOID
NLFreeNameControl (
    __in PNAME_CONTROL NameControl,
    __in PPAGED_LOOKASIDE_LIST LookasideList
    );

NTSTATUS
NLCheckAndGrowNameControl (
    __inout PNAME_CONTROL NameCtrl,
    __in USHORT NewSize
    );

VOID
NLInitNameControl (
    __inout PNAME_CONTROL NameCtrl
    );

VOID
NLCleanupNameControl (
    __inout PNAME_CONTROL NameCtrl
    );

NTSTATUS
NLReallocNameControl (
    __inout PNAME_CONTROL NameCtrl,
    __in ULONG NewSize,
    __out_opt PWCHAR *RetOriginalBuffer
    );

#endif

