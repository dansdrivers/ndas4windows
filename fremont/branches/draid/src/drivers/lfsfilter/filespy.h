/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    filespy.h

Abstract:

    Header file which contains the structures, type definitions,
    and constants that are shared between the kernel mode driver, 
    filespy.sys, and the user mode executable, filespy.exe.

Environment:

    Kernel mode

--*/
#ifndef __FILESPY_H__
#define __FILESPY_H__

typedef struct _FILESPY_DEVICE_EXTENSION *PFILESPY_DEVICE_EXTENSION;
#define DEVICE_NAMES_SZ  100

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable

#include "LfsFilterPublic.h"
#include <lpxtdi.h>

#include "LfsDbg.h"

#include "FastIoDispatch.h"
#include "NdftProtocolHeader.h"
#include "Lfs.h"

#include "LfsFiltLib.h"

//
//  Enable these warnings in the code.
//

#pragma warning(error:4100)   // Unreferenced formal parameter
#pragma warning(error:4101)   // Unreferenced local variable



#define MAX_BUFFERS     100

//
//  Attach modes for the filespy kernel driver
//

#define FILESPY_ATTACH_ON_DEMAND    1   
    //  Filespy will only attach to a volume when a user asks to start logging 
    //  that volume.
                                        
#define FILESPY_ATTACH_ALL_VOLUMES  2   
    //  VERSION NOTE:
    //  
    //  On Windows 2000, Filespy will attach to all volumes in the system that
    //  it sees mount but not turn on logging until requested to through the
    //  filespy user application.  Therefore, if filespy is set to mount on 
    //  demand, it will miss the mounting of the local volumes at boot time.  
    //  If filespy is set to load at boot time, it will see all the local 
    //  volumes be mounted and attach.  This can be beneficial if you want
    //  filespy to attach low in the device stack.
    //
    //  On Windows XP and later, Filespy will attach to all volumes in the
    //  system when it is loaded and all volumes that mount after Filespy is
    //  loaded.  Again, logging on these volumes will not be turned on until 
    //  the user asks it to be.
    //
                                        
//
//  Record types field definitions
//

typedef enum _RECORD_TYPE_FLAGS {

    RECORD_TYPE_STATIC                  = 0x80000000,
    RECORD_TYPE_NORMAL                  = 0x00000000,

    RECORD_TYPE_IRP                     = 0x00000001,
    RECORD_TYPE_FASTIO                  = 0x00000002,
#if WINVER >= 0x0501    
    RECORD_TYPE_FS_FILTER_OP            = 0x00000003,
#endif    

    RECORD_TYPE_OUT_OF_MEMORY           = 0x10000000,
    RECORD_TYPE_EXCEED_MEMORY_ALLOWANCE = 0x20000000

} RECORD_TYPE_FLAGS;

//
//  Macro to return the lower portion of RecordType
//

#define GET_RECORD_TYPE(pLogRecord) ((pLogRecord)->RecordType & 0x0000FFFF)

//
//  Structure defining the information recorded for an IRP operation
//

typedef struct _RECORD_IRP {

    LARGE_INTEGER OriginatingTime; //  The time the IRP originated
    LARGE_INTEGER CompletionTime;  //  The time the IRP was completed

    UCHAR IrpMajor;                //  From _IO_STACK_LOCATION
    UCHAR IrpMinor;                //  From _IO_STACK_LOCATION
    ULONG IrpFlags;                //  From _IRP (no cache, paging i/o, sync. 
                                   //  api, assoc. irp, buffered i/o, etc.)                   
    FILE_ID FileObject;            //  From _IO_STACK_LOCATION (This is the 
                                   //     PFILE_OBJECT, but this isn't 
                                   //     available in user-mode)
    DEVICE_ID DeviceObject;        //  From _IO_STACK_LOCATION (This is the 
                                   //     PDEVICE_OBJECT, but this isn't 
                                   //     available in user-mode)
    NTSTATUS ReturnStatus;         //  From _IRP->IoStatus.Status
    ULONG_PTR ReturnInformation;   //  From _IRP->IoStatus.Information
    FILE_ID ProcessId;
    FILE_ID ThreadId;
    
    //
    //  These fields are only filled in the appropriate
    //  Verbose mode.
    //
    
    PVOID Argument1;               //  
    PVOID Argument2;               //  Current IrpStackLocation
    PVOID Argument3;               //  Parameters
    PVOID Argument4;               //  
    ACCESS_MASK DesiredAccess;     //  Only used for CREATE irps

} RECORD_IRP, *PRECORD_IRP;

//
//  Structure defining the information recorded for a Fast IO operation
//

typedef struct _RECORD_FASTIO {

    LARGE_INTEGER StartTime;     //  Time Fast I/O request begins processing
    LARGE_INTEGER CompletionTime;//  Time Fast I/O request completes processing
    LARGE_INTEGER FileOffset;    //  Offset into the file for the I/O
    
    FILE_ID FileObject;          //  Parameter to FASTIO call
    DEVICE_ID DeviceObject;      //  Parameter to FASTIO call

    FILE_ID ProcessId;
    FILE_ID ThreadId;

    FASTIO_TYPE Type;            //  Type of FASTIO operation
    ULONG Length;                //  The length of data for the I/O operation

    NTSTATUS ReturnStatus;       //  From IO_STATUS_BLOCK

    BOOLEAN Wait;                //  Parameter to most FASTIO calls, signifies 
                                 //  if this operation can wait

} RECORD_FASTIO, *PRECORD_FASTIO;

#if WINVER >= 0x0501

//
//  Structure defining the information recorded for FsFilter operations
//

typedef struct _RECORD_FS_FILTER_OPERATION {

    LARGE_INTEGER OriginatingTime;
    LARGE_INTEGER CompletionTime;

    FILE_ID FileObject;
    DEVICE_ID DeviceObject;

    FILE_ID ProcessId;
    FILE_ID ThreadId;
    
    NTSTATUS ReturnStatus;

    UCHAR FsFilterOperation;

} RECORD_FS_FILTER_OPERATION, *PRECORD_FS_FILTER_OPERATION;

#endif

//
//  The three types of records that are possible.
//

typedef union _RECORD_IO {

    RECORD_IRP RecordIrp;
    RECORD_FASTIO RecordFastIo;
#if WINVER >= 0x0501   
    RECORD_FS_FILTER_OPERATION RecordFsFilterOp;
#endif

} RECORD_IO, *PRECORD_IO;


//
//  Log record structure defines the additional information needed for
//  managing the processing of the each IO FileSpy monitors.
//

typedef struct _LOG_RECORD {

    ULONG Length;           //  Length of record including header 
    ULONG SequenceNumber;
    RECORD_TYPE_FLAGS RecordType;
    RECORD_IO Record;
    WCHAR Name[0];          //  The name starts here

} LOG_RECORD, *PLOG_RECORD;


#define SIZE_OF_LOG_RECORD  (sizeof( LOG_RECORD )) 


//
//  This is the in-memory structure used to track log records.
//

typedef enum _RECORD_LIST_FLAGS {

    //
    //  If set, we want to sync this operation back to the dispatch routine
    //

    RLFL_SYNC_TO_DISPATCH       = 0x00000001,

    //
    //  During some operations (like rename) we need to know if the file is
    //  a file or directory.
    //

    RLFL_IS_DIRECTORY           = 0x00000002

} RECORD_LIST_FLAGS;

typedef struct _RECORD_LIST {

    LIST_ENTRY List;
    PVOID NewContext;
    PVOID WaitEvent;
    RECORD_LIST_FLAGS Flags;
    LOG_RECORD LogRecord;

} RECORD_LIST, *PRECORD_LIST;

#define SIZE_OF_RECORD_LIST (sizeof( RECORD_LIST ))

//
//  The statistics that are kept on the file name hash table
//  to monitor its efficiency.
//

typedef struct _FILESPY_STATISTICS {

    ULONG   TotalContextSearches;
    ULONG   TotalContextFound;
    ULONG   TotalContextCreated;
    ULONG   TotalContextTemporary;
    ULONG   TotalContextDuplicateFrees;
    ULONG   TotalContextCtxCallbackFrees;
    ULONG   TotalContextNonDeferredFrees;
    ULONG   TotalContextDeferredFrees;
    ULONG   TotalContextDeleteAlls;
    ULONG   TotalContextsNotSupported;
    ULONG   TotalContextsNotFoundInStreamList;

} FILESPY_STATISTICS, *PFILESPY_STATISTICS;

//
//  Maximum name length definitions
//

#ifndef MAX_PATH
#define MAX_PATH        384
#endif

#define MAX_NAME_SPACE  (MAX_PATH*sizeof(WCHAR))

//
//  Size of the actual records with the name built in.
//

#define RECORD_SIZE     (SIZE_OF_RECORD_LIST + MAX_NAME_SPACE)

#endif /* __FILESPY_H__ */

