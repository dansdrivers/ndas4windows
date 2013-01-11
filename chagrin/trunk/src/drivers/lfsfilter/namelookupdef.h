/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    namelookupdef.h

Abstract:

    Header file containing the name lookup definitions needed by both user
    and kernel mode.  No kernel-specific data types are used here.


Environment:

    User and kernel.

--*/
#ifndef __NAMELOOKUPDEF_H__
#define __NAMELOOKUPDEF_H__

//
//  This is set to the number of characters we want to allow the 
//  device extension to store for the name used to identify
//  a device object.
//

#define DEVICE_NAME_SZ  256

//
//  This is set to the number of characters we want to allow the 
//  device extension to use for storing user-provided names.
//  This is only used for devices whos type is FILE_DEVICE_NETWORK_FILE_SYSTEM
//  since we cannot get a DOS device name for them.
//

#define USER_NAMES_SZ   256

//
//    These are flags passed to the name lookup routine to identify different
//    ways the name of a file can be obtained
//

typedef enum _NAME_LOOKUP_FLAGS {

    //
    //  If set, only check in the name cache for the file name.
    //

    NLFL_ONLY_CHECK_CACHE           = 0x00000001,

    //
    //  If set, don't lookup the name
    //

    NLFL_NO_LOOKUP                  = 0x00000002,

    //
    //  if set, we are in the CREATE operation and the full path filename may
    //  need to be built up from the related FileObject.
    //

    NLFL_IN_CREATE                  = 0x00000004,
                
    //
    //  if set and we are looking up the name in the file object, the file 
    //  object does not actually contain a name but it contains a file/object 
    //  ID.
    //

    NLFL_OPEN_BY_ID                 = 0x00000008,

    //
    //  If set, the target directory is being opened
    //

    NLFL_OPEN_TARGET_DIR            = 0x00000010,

    //
    //  If set, use the DOS device name (DosName) instead of the NT device name.
    //

    NLFL_USE_DOS_DEVICE_NAME        = 0x00000020

} NAME_LOOKUP_FLAGS;

#endif

