/*++

Copyright (c) 1989-1999  Microsoft Corporation

Module Name:

    namelookup.c

Abstract:
    Header file which contains the definitions that may be
    shared with the file spy kernel debugger extensions


Environment:

    Kernel mode

--*/

//
//  Fixes Win2K compatibility regarding lookaside lists.
//

#include "LfsProc.h"

#ifndef _WIN2K_COMPAT_SLIST_USAGE
#define _WIN2K_COMPAT_SLIST_USAGE
#endif

#if !__NDAS_FS__
#include <strsafe.h>
#endif
#include "ntifs.h"
#include "ntdddisk.h"
#include "namelookup.h"

#define NL_POOL_TAG 'tPlN'

#if WINVER == 0x0500

#define RtlInitEmptyUnicodeString(_ucStr,_buf,_bufSize) \
    ((_ucStr)->Buffer = (_buf), \
     (_ucStr)->Length = 0, \
     (_ucStr)->MaximumLength = (USHORT)(_bufSize))


#define ExFreePoolWithTag( a, b ) ExFreePool( (a) )

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif

//
//  The context used to send the task of getting the DOS device name off to
//  a worker thread.
//

typedef struct _NL_DOS_NAME_COMPLETION_CONTEXT {

    WORK_QUEUE_ITEM WorkItem;

    //
    //  The DeviceObject whose name is being retrieved.  We need this in addition
    //  to the other fields so we can make sure it doesn't disappear while
    //  we're retrieving the name.
    //

    PDEVICE_OBJECT DeviceObject;

    //
    //  The name library device extension header of the device object to get
    //  the DOS name of.
    //

    PNL_DEVICE_EXTENSION_HEADER NLExtHeader;

} NL_DOS_NAME_COMPLETION_CONTEXT, *PNL_DOS_NAME_COMPLETION_CONTEXT;


//
//  Function prototypes
//


NTSTATUS
NLPQueryFileSystemForFileName (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT NextDeviceObject,
    __inout PNAME_CONTROL FileName
    );

NTSTATUS
NLPQueryCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PKEVENT SynchronizingEvent
    );

VOID
NLPGetDosDeviceNameWorker (
    __in PNL_DOS_NAME_COMPLETION_CONTEXT Context
    );

#pragma alloc_text(PAGE, NLPQueryFileSystemForFileName)
#pragma alloc_text(PAGE, NLInitNameControl)
#pragma alloc_text(PAGE, NLCleanupNameControl)
#pragma alloc_text(PAGE, NLGetAndAllocateObjectName)
#pragma alloc_text(PAGE, NLGetObjectName)
#pragma alloc_text(PAGE, NLGetDosDeviceName)
#pragma alloc_text(PAGE, NLPGetDosDeviceNameWorker)
#pragma alloc_text(PAGE, NLAllocateAndCopyUnicodeString)
#pragma alloc_text(PAGE, NLInitNameControl)
#pragma alloc_text(PAGE, NLCleanupNameControl)
#pragma alloc_text(PAGE, NLReallocNameControl)
#pragma alloc_text(PAGE, NLCheckAndGrowNameControl)

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
    )
/*++

Routine Description:

    This routine retrieves the full pathname of the FileObject.  Note that
    the buffers containing pathname components may be stored in paged pool,
    therefore if we are at DISPATCH_LEVEL we cannot look up the name.

    The file is looked up one of the following ways based on the LookupFlags:
    1.  FlagOn( FileObject->Flags, FO_VOLUME_OPEN ) or
        (FileObject->FileName.Length == 0).  This is a volume open, so just use
        DeviceName from the NLExtHeader for the FileName, if it exists.
    2.  NAMELOOKUPFL_IN_CREATE and NAMELOOKUPFL_OPEN_BY_ID are set.
        This is an open by file id, so format the file id into the FileName
        string if there is enough room.
    3.  NAMELOOKUPFL_IN_CREATE set and FileObject->RelatedFileObject != NULL.
        This is a relative open, therefore the fullpath file name must
        be built up from the name of the FileObject->RelatedFileObject
        and FileObject->FileName.
    4.  NAMELOOKUPFL_IN_CREATE and FileObject->RelatedFileObject == NULL.
        This is an absolute open, therefore the fullpath file name is
        found in FileObject->FileName.
    5.  No LookupFlags set.
        This is a lookup sometime after CREATE.  FileObject->FileName is
        no longer guaranteed to be valid, so we call
        NLPQueryFileSystemForFileName which rolls a IRP_MJ_QUERY_INFORMATION to
        get the file name.

    NONPAGED: This routine is called by FILESPY in the paging file path so it
              must be non-paged.

Arguments:

    FileObject - Pointer to the FileObject to the get name of.

    FileNameControl - A caller-allocated name control that will be filled with
        the filename.  The string will be NULL terminated.

    NLExtHeader - The portion of a device extension needed to build the file
        name.

    LookupFlags - Flags that determine how the name is generated.

    CacheName - TRUE if the returned name should be saved in the cache, FALSE
        if the returned name should NOT be saved in the cache.

Return Value:

    STATUS_INSUFFICIENT_RESOURCES if there was not enough memory to retrieve
    the full name.  If this is the case, FileNameControl->Name.Length will
    be set to 0.  Do not expect any specific value for CacheName.

    STATUS_BUFFER_OVERFLOW if the status buffer to hold the file ID or object ID
        was not large enough.  If this is the case, FileNameControl->Name.Length
        will be set to 0.  Do not expect any specific value for CacheName.

    STATUS_SUCCESS if
        1) The full name was retrieved successfully, OR
        2) At DPC level, full name could not be retrieved, OR
        3) The name was only to be looked up in the cache, OR
        4) Nested operation, full name could not be retrieved, OR

--*/
{
    NTSTATUS status = STATUS_SUCCESS;
    NTSTATUS returnValue = STATUS_SUCCESS;
    ULONG i;
    BOOLEAN cacheName = TRUE;
    LONG count;

    //
    //  Copy over the name the user gave for this device.  These names
    //  should be meaningful to the user.  Note that we do not do this for
    //  NETWORK file system because internally they already show the
    //  connection name.  If this is a direct device open of the network
    //  file system device, we will copy over the device name to be
    //  returned to the user.
    //

    if (FILE_DEVICE_NETWORK_FILE_SYSTEM != NLExtHeader->ThisDeviceObject->DeviceType &&
        FlagOn( LookupFlags, NLFL_USE_DOS_DEVICE_NAME ) &&
        NLExtHeader->DosName.Length != 0) {

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            NLExtHeader->DosName.Length );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlCopyUnicodeString( &FileNameControl->Name, &NLExtHeader->DosName );

    } else if (FlagOn( FileObject->Flags, FO_DIRECT_DEVICE_OPEN )) {

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            NLExtHeader->DeviceName.Length );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlCopyUnicodeString( &FileNameControl->Name,
                              &NLExtHeader->DeviceName );

        //
        //  We are now done since there will be no more to the name in this
        //  case, so return TRUE.
        //

        *CacheName = TRUE;
        return STATUS_SUCCESS;

    } else {

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            NLExtHeader->DeviceName.Length );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlCopyUnicodeString( &FileNameControl->Name,
                              &NLExtHeader->DeviceName );
    }

    //
    //  See if we can request the name
    //

    if (FlagOn( LookupFlags, NLFL_ONLY_CHECK_CACHE )) {
#       define NotInCacheMsg L"[-=Not In Cache=-]"

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            sizeof( NotInCacheMsg ) );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeToString( &FileNameControl->Name,
                                  NotInCacheMsg );

        *CacheName = TRUE;
        return STATUS_UNSUCCESSFUL;
    }

    //
    //  Can not get the name at DPC level
    //

    if (KeGetCurrentIrql() > APC_LEVEL) {

#       define AtDPCMsg L"[-=At DPC Level=-]"

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            sizeof( AtDPCMsg ) );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeToString( &FileNameControl->Name,
                                  AtDPCMsg );

        *CacheName = FALSE;
        return STATUS_UNSUCCESSFUL;
    }

    //
    //  If there is a ToplevelIrp then this is a nested operation and
    //  there might be other locks held.  Can not get name without the
    //  potential of deadlocking.
    //

    if (IoGetTopLevelIrp() != NULL) {

#       define NestedMsg L"[-=Nested Operation=-]"

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            sizeof( NestedMsg ) );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeToString( &FileNameControl->Name,
                                  NestedMsg );
        *CacheName = FALSE;
        return STATUS_UNSUCCESSFUL;
    }

    //
    //  CASE 1:  This FileObject refers to a Volume open.  Either the
    //           flag is set or no filename is specified.
    //

    if (FlagOn( FileObject->Flags, FO_VOLUME_OPEN ) ||
        (FlagOn( LookupFlags, NLFL_IN_CREATE ) &&
         (FileObject->FileName.Length == 0) &&
         (FileObject->RelatedFileObject == NULL))) {

        //
        //  We've already copied the VolumeName so just return.
        //
    }

    //
    //  CASE 2:  We are opening the file by ID.
    //

    else if (FlagOn( LookupFlags, NLFL_IN_CREATE ) &&
             FlagOn( LookupFlags, NLFL_OPEN_BY_ID )) {

#       define OBJECT_ID_KEY_LENGTH 16
#       define ID_BUFFER_SIZE       40

        //
        //  Static buffer big enough for a file Id or object Id.
        //

        WCHAR idName[ID_BUFFER_SIZE];

        if (FileObject->FileName.Length == sizeof(LONGLONG)) {

            //
            //  Opening by FILE ID, generate a name.
            //
            //  To get the actual file name, you could use the file id to open
            //  the file and then get the file name.
            //

#if __NDAS_FS__
            count = _snwprintf( idName,
                                ID_BUFFER_SIZE,
                                L"<%016I64x>",
                                *((PLONGLONG)FileObject->FileName.Buffer) );
#else
            count = StringCbPrintf( idName,
                                    sizeof(idName),
                                    L"<%016I64x>",
                                    *((PLONGLONG)FileObject->FileName.Buffer) );
#endif

            //
            //  If the buffer wasn't big enough for the entire string, fail.
            //

            if (count < 0) {

                FileNameControl->Name.Length = 0;
                return STATUS_BUFFER_OVERFLOW;
            }

        } else if ((FileObject->FileName.Length == OBJECT_ID_KEY_LENGTH) ||
                   (FileObject->FileName.Length == OBJECT_ID_KEY_LENGTH +
                                                                sizeof(WCHAR)))
        {

            PUCHAR idBuffer;

            //
            //  Opening by Object ID, generate a name
            //
            //  To get the actual file name, you could use the file id to open
            //  the file and then get the file name.
            //

            idBuffer = (PUCHAR)&FileObject->FileName.Buffer[0];

            if (FileObject->FileName.Length != OBJECT_ID_KEY_LENGTH) {

                //
                //  Skip win32 backslash at start of buffer
                //

                idBuffer = (PUCHAR)&FileObject->FileName.Buffer[1];
            }

#if __NDAS_FS__
            count = _snwprintf( idName,
                                ID_BUFFER_SIZE,
                                L"<%08x-%04hx-%04hx-%04hx-%04hx%08x>",
                                *(PULONG)&idBuffer[0],
                                *(PUSHORT)&idBuffer[0+4],
                                *(PUSHORT)&idBuffer[0+4+2],
                                *(PUSHORT)&idBuffer[0+4+2+2],
                                *(PUSHORT)&idBuffer[0+4+2+2+2],
                                *(PULONG)&idBuffer[0+4+2+2+2+2] );
#else
            count = StringCbPrintf( idName,
                                    sizeof(idName),
                                    L"<%08x-%04hx-%04hx-%04hx-%04hx%08x>",
                                    *(PULONG)&idBuffer[0],
                                    *(PUSHORT)&idBuffer[0+4],
                                    *(PUSHORT)&idBuffer[0+4+2],
                                    *(PUSHORT)&idBuffer[0+4+2+2],
                                    *(PUSHORT)&idBuffer[0+4+2+2+2],
                                    *(PULONG)&idBuffer[0+4+2+2+2+2] );
#endif

            //
            //  If the buffer wasn't big enough for the entire string, fail.
            //

            if (count < 0) {

                FileNameControl->Name.Length = 0;
                return STATUS_BUFFER_OVERFLOW;
            }

        } else {

            //
            //  Unknown ID format.
            //

#if __NDAS_FS__
            count = _snwprintf( idName,
                                ID_BUFFER_SIZE,
                                L"[-=Unknown ID (Len=%u)=-]\n",
                                FileObject->FileName.Length );
#else
            count = StringCbPrintf( idName,
                                    sizeof(idName),
                                    L"[-=Unknown ID (Len=%u)=-]\n",
                                    FileObject->FileName.Length );
#endif
        }

        //
        //  Append the idName to FileNameControl.
        //

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            ID_BUFFER_SIZE );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeToString( &FileNameControl->Name,
                                  idName );

        //
        //  Don't cache the ID name
        //

        cacheName = FALSE;

        //
        //  Continue on to the end of the routine and return STATUS_UNSUCCESSFUL
        //  since we are not able to return a "usable" file name (caller cannot
        //  use the file name we return in other calls because it is not
        //  "valid").
        //

        returnValue = STATUS_UNSUCCESSFUL;

    }

    //
    //  CASE 3: We are opening a file that has a RelatedFileObject.
    //

    else if (FlagOn( LookupFlags, NLFL_IN_CREATE ) &&
             (NULL != FileObject->RelatedFileObject)) {

        //
        //  Must be a relative open.  We cannot use ObQueryNameString to get
        //  the name of the RelatedFileObject because it may result in
        //  deadlock.
        //

        PNAME_CONTROL relativeName;
        //ULONG returnLength;

        status = NLAllocateNameControl( &relativeName, LookasideList );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        status = NLPQueryFileSystemForFileName( FileObject->RelatedFileObject,
                                                NLExtHeader->AttachedToDeviceObject,
                                                relativeName );

        if (NT_SUCCESS( status ))
        {

            //
            //  We were able to get the relative FileoBject's name.
            //  Build up the file name in the following format:
            //      [volumeName]\[relativeFileObjectName]\[FileObjectName]
            //  The VolumeName is already in FileNameControl if we've got one.
            //

            status = NLCheckAndGrowNameControl( FileNameControl,
                                                FileNameControl->Name.Length +
                                                relativeName->Name.Length );

            if (!NT_SUCCESS( status )) {

                goto NoResources;
            }

            RtlAppendUnicodeStringToString( &FileNameControl->Name,
                                            &relativeName->Name );

        } else {

            //
            //  The query for the relative FileObject name was
            //  unsuccessful. Build up the file name in the following format:
            //      [volumeName]\...\[FileObjectName]
            //  The volumeName is already in FileNameControl if we've got one.

#           define RFOPlaceholder L"...\\"

            status = NLCheckAndGrowNameControl( FileNameControl,
                                                FileNameControl->Name.Length +
                                                sizeof( RFOPlaceholder ) );

            if (!NT_SUCCESS( status )) {

                goto NoResources;
            }

            RtlAppendUnicodeToString( &FileNameControl->Name,
                                      RFOPlaceholder );

            cacheName = FALSE;

        }

        //
        //  If there is not a slash and the end of the related file object
        //  string and there is not a slash at the front of the file object
        //  string, then add one.
        //

        if (((FileNameControl->Name.Length < sizeof(WCHAR) ||
             (FileNameControl->Name.Buffer[(FileNameControl->Name.Length/sizeof(WCHAR))-1] != L'\\')))
             && ((FileObject->FileName.Length < sizeof(WCHAR)) ||
             (FileObject->FileName.Buffer[0] != L'\\')))
        {

            status = NLCheckAndGrowNameControl( FileNameControl,
                                                FileNameControl->Name.Length +
                                                sizeof(WCHAR) );

            if (!NT_SUCCESS( status )) {

                goto NoResources;
            }

            RtlAppendUnicodeToString( &FileNameControl->Name, L"\\" );
        }

        NLFreeNameControl( relativeName, LookasideList );

        //
        //  At this time, copy over the FileObject->FileName to FileNameControl.
        //

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            FileObject->FileName.Length );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeStringToString( &FileNameControl->Name,
                                        &FileObject->FileName );

        //
        //  Continue on to the end of the routine and return
        //  STATUS_UNSUCCESSFUL since we are not able to return a "usable"
        //  file name (caller cannot use the file name we return in other
        //  calls because it is not "valid").
        //

        returnValue = STATUS_UNSUCCESSFUL;

    }

    //
    //  CASE 4: We have a open on a file with an absolute path.
    //

    else if (FlagOn( LookupFlags, NLFL_IN_CREATE ) &&
             (FileObject->RelatedFileObject == NULL) ) {

        //
        //  We have an absolute path, so try to copy that into FileNameControl.
        //

        status = NLCheckAndGrowNameControl( FileNameControl,
                                            FileNameControl->Name.Length +
                                            FileObject->FileName.Length );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        RtlAppendUnicodeStringToString( &FileNameControl->Name,
                                        &FileObject->FileName );
    }

    //
    //  CASE 5: We are retrieving the file name sometime after the
    //  CREATE operation.
    //

    else if (!FlagOn( LookupFlags, NLFL_IN_CREATE )) {

        PNAME_CONTROL nameInfo;
        //ULONG returnLength;

        status = NLAllocateNameControl( &nameInfo, LookasideList );

        if (!NT_SUCCESS( status )) {

            goto NoResources;
        }

        status = NLPQueryFileSystemForFileName( FileObject,
                                                NLExtHeader->AttachedToDeviceObject,
                                                nameInfo );

        if (NT_SUCCESS( status )) {

            status = NLCheckAndGrowNameControl( FileNameControl,
                                                FileNameControl->Name.Length +
                                                nameInfo->Name.Length );

            if (!NT_SUCCESS( status )) {

                goto NoResources;
            }

            RtlAppendUnicodeStringToString( &FileNameControl->Name,
                                            &nameInfo->Name );

        } else {

            //
            //  Got an error trying to get the file name from the base file
            //  system, so fail.
            //

#if __NDAS_FS__
            count = _snwprintf( nameInfo->Name.Buffer,
                                nameInfo->BufferSize,
                                L"[-=Error 0x%x Getting Name=-]",
                                status );
#else
            count = StringCbPrintf( nameInfo->Name.Buffer,
                                    nameInfo->BufferSize,
                                    L"[-=Error 0x%x Getting Name=-]",
                                    status );
#endif

            nameInfo->Name.Length = (USHORT)max( 0, count );

            status = NLCheckAndGrowNameControl( FileNameControl,
                                                FileNameControl->Name.Length +
                                                nameInfo->Name.Length );

            if (!NT_SUCCESS( status )) {

                goto NoResources;
            }

            RtlAppendUnicodeStringToString( &FileNameControl->Name,
                                            &nameInfo->Name );

            cacheName = FALSE;
            return STATUS_UNSUCCESSFUL;
        }

        NLFreeNameControl( nameInfo, LookasideList );
    }

    //
    //  When we get here we have a valid name.
    //  Sometimes when we query a name it has a trailing slash, other times
    //  it doesn't.  To make sure the contexts are correct we are going to
    //  remove a trailing slash if there is not a ":" just before it.
    //

    if ((FileNameControl->Name.Length >= (2*sizeof(WCHAR))) &&
        (FileNameControl->Name.Buffer[(FileNameControl->Name.Length/sizeof(WCHAR))-1] == L'\\') &&
        (FileNameControl->Name.Buffer[(FileNameControl->Name.Length/sizeof(WCHAR))-2] != L':'))
    {

        FileNameControl->Name.Length -= sizeof(WCHAR);
    }

    //
    //  See if we are actually opening the target directory.  If so then
    //  remove the trailing name and slash.  Note that we won't remove
    //  the initial slash (just after the colon).
    //

    if (FlagOn( LookupFlags, NLFL_OPEN_TARGET_DIR ) &&
        (FileNameControl->Name.Length > 0))
    {
        i = (FileNameControl->Name.Length / sizeof(WCHAR)) - 1;

        //
        //  See if the path ends in a backslash, if so skip over it
        //  (since the file system did).
        //

        if ((i > 0) &&
            (FileNameControl->Name.Buffer[i] == L'\\') &&
            (FileNameControl->Name.Buffer[i-1] != L':')) {

            i--;
        }

        //
        //  Scan backwards over the last component
        //

        for ( ; i > 0; i-- ) {

            if (FileNameControl->Name.Buffer[i] == L'\\') {

                if ((i > 0) && (FileNameControl->Name.Buffer[i-1] == L':')) {

                    i++;
                }

                FileNameControl->Name.Length = (USHORT)(i * sizeof(WCHAR));
                break;
            }
        }
    }

    *CacheName = cacheName;

    return returnValue;

NoResources:

    *CacheName = FALSE;
    FileNameControl->Name.Length = 0;
    return status;
}



NTSTATUS
NLPQueryFileSystemForFileName (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT NextDeviceObject,
    __inout PNAME_CONTROL FileName
    )
/*++

Routine Description:

    This routine rolls an IRP to query the name of the
    FileObject parameter from the base file system.

    Note:  ObQueryNameString CANNOT be used here because it
      would cause recursive lookup of the file name for FileObject.

Arguments:

    FileObject - the file object for which we want the name.
    NextDeviceObject - the device object for the next driver in the
        stack.  This is where we want to start our request
        for the name of FileObject.
    FileName - Receives the name.  This must be memory that safe to write
        to from kernel space.

Return Value:

    Returns the status of the operation.

--*/
{
    PIRP irp;
    PIO_STACK_LOCATION irpSp;
    KEVENT event;
    IO_STATUS_BLOCK ioStatus;
    NTSTATUS status;
    PFILE_NAME_INFORMATION nameInfo = NULL;
    ULONG nameInfoLength;

    PAGED_CODE();

    try {

        //
        //  We'll start with a small buffer and grow it if needed.
        //

        nameInfoLength = 256;
        nameInfo = ExAllocatePoolWithTag( PagedPool,
                                          nameInfoLength,
                                          NL_POOL_TAG );

        if (nameInfo == NULL) {

            status = STATUS_INSUFFICIENT_RESOURCES;
            leave;
        }

        //
        //  Try getting the name.  If the IRP fails, then our buffer is too small.
        //  We'll grow it and try again (hence the loop).  We can get the needed
        //  buffer size from the FileNameLength field of FILE_NAME_INFORMATION.
        //

        while (TRUE) {

            irp = IoAllocateIrp( NextDeviceObject->StackSize, FALSE );

            if (irp == NULL) {

                status = STATUS_INSUFFICIENT_RESOURCES;
                leave;
            }

            //
            //  Set our current thread as the thread for this
            //  IRP so that the IO Manager always knows which
            //  thread to return to if it needs to get back into
            //  the context of the thread that originated this
            //  IRP.
            //

            irp->Tail.Overlay.Thread = PsGetCurrentThread();

            //
            //  Set that this IRP originated from the kernel so that
            //  the IOManager knows that the buffers do not
            //  need to be probed.
            //

            irp->RequestorMode = KernelMode;

            //
            //  Initialize the UserIosb and UserEvent in the
            //

            ioStatus.Status = STATUS_SUCCESS;
            ioStatus.Information = 0;

            irp->UserIosb = &ioStatus;
            irp->UserEvent = NULL;        //already zeroed

            //
            //  Set the IRP_SYNCHRONOUS_API to denote that this
            //  is a synchronous IO request.
            //

            irp->Flags = IRP_SYNCHRONOUS_API;

            irpSp = IoGetNextIrpStackLocation( irp );

            irpSp->MajorFunction = IRP_MJ_QUERY_INFORMATION;
            irpSp->FileObject = FileObject;

            //
            //  Setup the parameters for IRP_MJ_QUERY_INFORMATION.
            //  The buffer we want to be filled in should be placed in
            //  the system buffer.
            //

            irp->AssociatedIrp.SystemBuffer = nameInfo;

            irpSp->Parameters.QueryFile.Length = nameInfoLength;
            irpSp->Parameters.QueryFile.FileInformationClass = FileNameInformation;

            //
            //  Set up the completion routine so that we know when our
            //  request for the file name is completed.  At that time,
            //  we can free the IRP.
            //

            KeInitializeEvent( &event, NotificationEvent, FALSE );

            IoSetCompletionRoutine( irp,
                                    NLPQueryCompletion,
                                    &event,
                                    TRUE,
                                    TRUE,
                                    TRUE );

            status = IoCallDriver( NextDeviceObject, irp );

            if (STATUS_PENDING == status) {

                (VOID) KeWaitForSingleObject( &event,
                                              Executive,
                                              KernelMode,
                                              FALSE,
                                              NULL );
            }

            //
            //  Verify that our completion routine has been called.
            //

            ASSERT(KeReadStateEvent(&event) ||
                   !NT_SUCCESS(ioStatus.Status));

            //
            //  If the IRP succeeded, go ahead and copy the name and return.
            //

            if (NT_SUCCESS( ioStatus.Status )) {

                //
                //  We retrieved a valid name, set into buffer
                //  Make sure the buffer is big enough to hold the given name
                //

                status = NLCheckAndGrowNameControl( FileName,
                                                    (USHORT)nameInfo->FileNameLength );

                if (NT_SUCCESS( status )) {

                    //
                    //  Copy the name from nameInfo buffer into name control
                    //

                    RtlCopyMemory( FileName->Name.Buffer,
                                   nameInfo->FileName,
                                   nameInfo->FileNameLength );
                    FileName->Name.Length = (USHORT)nameInfo->FileNameLength;
                }

                leave;

            } else if (ioStatus.Status == STATUS_BUFFER_OVERFLOW) {

                //
                //  Buffer was too small, so grow it and try again.  We need space
                //  in the buffer for the name and the FileNameLength ULONG.
                //

                nameInfoLength = (USHORT)nameInfo->FileNameLength +
                                         sizeof( ULONG );

                ExFreePoolWithTag( nameInfo, NL_POOL_TAG );

                nameInfo = ExAllocatePoolWithTag( PagedPool,
                                                  nameInfoLength,
                                                  NL_POOL_TAG );

                if (nameInfo == NULL) {

                    status = STATUS_INSUFFICIENT_RESOURCES;
                    leave;
                }

            } else {

                //
                //  The query name failed, we are done
                //

                status = ioStatus.Status;
                leave;
            }
        }

    } finally {

        if (nameInfo != NULL) {

            ExFreePoolWithTag( nameInfo, NL_POOL_TAG );
        }
    }

    return status;
}


NTSTATUS
NLPQueryCompletion (
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PKEVENT SynchronizingEvent
    )
/*++

Routine Description:

    This routine does the cleanup necessary once the query request completed
    by the file system.

Arguments:

    DeviceObject - This will be NULL since we originated this
        IRP.

    Irp - The IO request structure containing the information
        about the current state of our file name query.

    SynchronizingEvent - The event to signal to notify the
        originator of this request that the operation is
        complete.

Return Value:

    Returns STATUS_MORE_PROCESSING_REQUIRED so that IO Manager
    will not try to free the IRP again.

--*/
{

    UNREFERENCED_PARAMETER( DeviceObject );

    //
    //  Make sure that the IRP status is copied over to the users
    //  IO_STATUS_BLOCK so that the originator of this IRP will know
    //  the final status of this operation.
    //

    ASSERT( NULL != Irp->UserIosb );
    *Irp->UserIosb = Irp->IoStatus;

    //
    //  Signal SynchronizingEvent so that the originator of this
    //  IRP know that the operation is completed.
    //

    KeSetEvent( SynchronizingEvent, IO_NO_INCREMENT, FALSE );

    //
    //  We are now done, so clean up the IRP that we allocated.
    //

    IoFreeIrp( Irp );

    //
    //  If we return STATUS_SUCCESS here, the IO Manager will
    //  perform the cleanup work that it thinks needs to be done
    //  for this IO operation.  This cleanup work includes:
    //  * Copying data from the system buffer to the users buffer
    //    if this was a buffered IO operation.
    //  * Freeing any MDLs that are in the IRP.
    //  * Copying the Irp->IoStatus to Irp->UserIosb so that the
    //    originator of this IRP can see the final status of the
    //    operation.
    //  * If this was an asynchronous request or this was a
    //    synchronous request that got pending somewhere along the
    //    way, the IO Manager will signal the Irp->UserEvent, if one
    //    exists, otherwise it will signal the FileObject->Event.
    //    (This can have REALLY bad implications if the IRP originator
    //     did not an Irp->UserEvent and the IRP originator is not
    //     waiting on the FileObject->Event.  It would not be that
    //     farfetched to believe that someone else in the system is
    //     waiting on FileObject->Event and who knows who will be
    //     awoken as a result of the IO Manager signaling this event.
    //
    //  Since some of these operations require the originating thread's
    //  context (e.g., the IO Manager need the UserBuffer address to
    //  be valid when copy is done), the IO Manager queues this work
    //  to an APC on the IRPs originating thread.
    //
    //  We can do this cleanup work more efficiently than the IO Manager
    //  since we are handling a very specific case.  Therefore, it is better
    //  for us to perform the cleanup work here then free the IRP than passing
    //  control back to the IO Manager to do this work.
    //
    //  By returning STATUS_MORE_PROCESS_REQUIRED, we tell the IO Manager
    //  to stop processing this IRP until it is told to restart processing
    //  with a call to IoCompleteRequest.  Since the IO Manager has
    //  already performed all the work we want it to do on this
    //  IRP, we do the cleanup work, return STATUS_MORE_PROCESSING_REQUIRED,
    //  and ask the IO Manager to resume processing by calling
    //  IoCompleteRequest.
    //

    return STATUS_MORE_PROCESSING_REQUIRED;
}


PNAME_CONTROL
NLGetAndAllocateObjectName (
    __in PVOID Object,
    __in PPAGED_LOOKASIDE_LIST LookasideList
    )
/*++

Routine description:

    This routine will allocate the name control and retrieve the name of
    the device object or driver object.  If we can not get either a NULL
    entry is returned.

Arguments:

    DeviceObject - The object to get the name for

    ObjectName - The lookaside list to allocate from

Return Value:

    Returns: NAME_CONTROL if we got the name.  The caller must free the
             name control when they are done.
             Else NULL if no name could be retrieved.

--*/
{
    NTSTATUS status;
    PNAME_CONTROL objName;

    PAGED_CODE();

    //
    //  Allocate name control
    //

    status = NLAllocateNameControl( &objName, LookasideList );

    if (NT_SUCCESS( status )) {

        //
        //  Retrieve the name.  Note that we don't test for an error because
        //  we want to use whatever name is returned.
        //

        status = NLGetObjectName( Object, objName );

    } else {

        //
        //  On a failure NLAllocateNameControl returns the buffer NULL'd,
        //  verify this
        //

        ASSERT(objName == NULL);
    }

    return objName;
}


NTSTATUS
NLGetObjectName (
    __in PVOID Object,
    __inout PNAME_CONTROL ObjectName
    )
/*++

Routine description:

    This routine reads the name from Object and copies it into the
    given name control structure.  The memory for ObjectName's buffer should
    be allocated by the caller.  We can use ObQueryNameString here because
    Object is a device object or a driver object.

Arguments:

    Object - Supplies the object being queried

    ObjectName - The name control structure to store Object's name.  Will
        grow if needed to hold the object name.

Return Value:

    Returns STATUS_SUCCESS if the name could be retrieved and successfully
        copied into ObjectName.

     Returns STATUS_INSUFFICIENT_RESOURCES if memory cannot be allocated to
        store the name.

--*/
{
    NTSTATUS status;
    POBJECT_NAME_INFORMATION nameInfo;
    ULONG lengthReturned = 0;

    PAGED_CODE();

    ASSERT( ARGUMENT_PRESENT( ObjectName ) );

    //
    //  We just use the buffer in the name control to hold the
    //  OBJECT_NAME_INFORMATION structure plus name buffer.  That
    //  way we don't have to do a separate allocation.
    //

    nameInfo = (POBJECT_NAME_INFORMATION) ObjectName->Name.Buffer;

    //
    //  Query the object manager to get the name
    //

    status = ObQueryNameString( Object,
                                nameInfo,
                                ObjectName->Name.MaximumLength,
                                &lengthReturned );

    //
    //  If we get back a status saying the buffer is too small,
    //  we can try to reallocate a node with the number of bytes needed
    //

    if (status == STATUS_INFO_LENGTH_MISMATCH) {

        status = NLReallocNameControl( ObjectName,
                                       lengthReturned,
                                       NULL );

        if (!NT_SUCCESS( status )) {

            //
            //  If we cannot successfully reallocate a node, we are probably
            //  out of memory. In this case we will just set the string to NULL
            //  and return the error
            //

            ObjectName->Name.Length = 0;
            return status;
        }

        nameInfo = (POBJECT_NAME_INFORMATION) ObjectName->Name.Buffer;

        status = ObQueryNameString( Object,
                                    nameInfo,
                                    ObjectName->Name.MaximumLength,
                                    &lengthReturned );

    }

    //
    //  At this point we should only be getting back errors other than
    //  buffer not large enough (that should have been handled above)
    //

    if (!NT_SUCCESS( status )) {

#if __NDAS_FS__
        int count = _snwprintf( ObjectName->Name.Buffer,
                                           ObjectName->Name.MaximumLength,
                                           L"[-=Error 0x%x Getting Name=-]",
                                           status );
#else
        int count = StringCbPrintf( ObjectName->Name.Buffer,
                                    ObjectName->Name.MaximumLength,
                                    L"[-=Error 0x%x Getting Name=-]",
                                    status );
#endif
        //
        //  We should never really get a negative count back because
        //  Name Control structures allocate a buffer of size 254 or greater
        //  Our error status should never cause us to have string > 254...
        //

        ObjectName->Name.Length = (USHORT)max( 0, count );

        return status;
    }

    //
    //  We got the name, so now just need to shuffle everything into place.
    //

    ObjectName->Name.Length = nameInfo->Name.Length;

    RtlMoveMemory( ObjectName->Name.Buffer,
                   nameInfo->Name.Buffer,
                   ObjectName->Name.Length );

    return status;
}


VOID
NLGetDosDeviceName (
    __in PDEVICE_OBJECT DeviceObject,
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader
    )
/*++

Routine Description:

    This routine gets the DOS device name of DeviceObject.  It uses a worker
    thread due to the threat of deadlock (see inline comments).

    NOTE: This routine will allocate (on success) the DosName buffer in
          NLExtHeader.  Call NLCleanupDeviceExtensionHeader when you tear down
          your device extension to make sure this is freed.

Arguments:

    DeviceObject - The object to get the DOS device name of.
    NLExtHeader - The name lookup extension header for DeviceObject.

Return Value:

    None.

--*/

{
    PNL_DOS_NAME_COMPLETION_CONTEXT completionContext;

    PAGED_CODE();

    //
    //  Mount manager could be on the call stack below us holding
    //  a lock.  NLPGetDosDeviceNameWorker will eventually query the mount
    //  manager which will cause a deadlock in this scenario.
    //  So, we need to do this work in a worker thread.
    //

    completionContext = ExAllocatePoolWithTag( NonPagedPool,
                                               sizeof( NL_DOS_NAME_COMPLETION_CONTEXT ),
                                               NL_POOL_TAG );

    if (completionContext == NULL) {

        //
        //  If we cannot allocate our completion context, we will not
        //  get the DOS name.
        //

        NLExtHeader->DosName.Length = 0;

    } else {

        //
        //  Initialize a work item.  CompletionContext keeps track
        //  of the work queue item and the data that we need
        //  to pass to NLPGetDosDeviceNameWorker in the worker thread.
        //

        ExInitializeWorkItem( &completionContext->WorkItem,
                              NLPGetDosDeviceNameWorker,
                              completionContext );

        //
        //  Don't let the DeviceObject get deleted while we get the DOS
        //  device name asynchronously.
        //

        ObReferenceObject( DeviceObject );

        //
        //  Setup the context.
        //

        completionContext->DeviceObject = DeviceObject;
        completionContext->NLExtHeader = NLExtHeader;

        //
        //  Queue the work item so that it will be run in a
        //  worker thread at some point.
        //

        ExQueueWorkItem( &completionContext->WorkItem ,
                         DelayedWorkQueue );
    }
}


VOID
NLPGetDosDeviceNameWorker (
    __in PNL_DOS_NAME_COMPLETION_CONTEXT Context
    )
/*++

Routine Description:

    This routine uses RtlVolumeDeviceToDosName to get the DOS device name of a
    device object.  Note that RtlVolumeDeviceToDosName is obsolete, but we use
    it so the code is backwards compatible with Win2K.  If the DOS name cannot
    be retrieved, we set the Length parameter of DosName to 0

Arguments:

    NLExtHeader - The name lookup extension header corresponding with the device
        object to get the name of.

Return Value:

    None

--*/

{
    NTSTATUS status;
    PNL_DEVICE_EXTENSION_HEADER nlExtHeader;

    PAGED_CODE();

    ASSERT( Context != NULL );

    nlExtHeader = Context->NLExtHeader;



    //
    //  Get DOS device name if we have a storage stack device
    //  object to ask for it. The reason we might not have a storage
    //  stack device is if this is a Remote File System.
    //

    if (nlExtHeader->StorageStackDeviceObject == NULL) {

        ObDereferenceObject( Context->DeviceObject );
        ExFreePoolWithTag( Context, NL_POOL_TAG );

        //
        //  Set the DosName to the empty string
        //

        nlExtHeader->DosName.Length = 0;

        return;
    }

    ASSERT( nlExtHeader->StorageStackDeviceObject != NULL );

    //
    //  Get the DOS device name
    //

    status = RtlVolumeDeviceToDosName( nlExtHeader->StorageStackDeviceObject,
                                       &nlExtHeader->DosName);

    if (!NT_SUCCESS( status )) {

        //
        //  We couldn't get the DOS device name. Set the string to empty
        //

        nlExtHeader->DosName.Length = 0;

    }

    ObDereferenceObject( Context->DeviceObject );
    ExFreePoolWithTag( Context, NL_POOL_TAG );

    return;

}


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
    )
/*++

Routine Description:

    This routine initializes the fields of a NL_DEVICE_EXTENSION_HEADER.  All
    pointer fields are set to NULL, and unicode strings are initialized as
    empty.  Use NLCleanupDeviceExtensionHeader to clean up a
    NL_DEVICE_EXTENSION_HEADER.

    NLExtHeader must *not* be NULL.

Arguments:

    NLExtHeader - The name lookup extension header to initialize.

    ThisDeviceObject - Device Object that this device extension is attached to

Return Value:

    Pointer to the allocated and initialized name control.
    If allocation fails, this function returns NULL.

--*/
{
    ASSERT(NLExtHeader != NULL);

    NLExtHeader->AttachedToDeviceObject = NULL;
    NLExtHeader->ThisDeviceObject = ThisDeviceObject;
    NLExtHeader->StorageStackDeviceObject = StorageStackDeviceObject;

    RtlInitEmptyUnicodeString( &NLExtHeader->DeviceName, NULL, 0 );
    RtlInitEmptyUnicodeString( &NLExtHeader->DosName, NULL, 0 );
}

VOID
NLCleanupDeviceExtensionHeader (
    __in PNL_DEVICE_EXTENSION_HEADER NLExtHeader
    )
/*++

Routine Description:

    This routine frees any memory associated with the given name lookup
    extension header.  This may include the DosName unicode string and buffer,
    and the DeviceName.

Arguments:

    NLExtHeader - The name lookup extension header to clean up.

Return Value:

    None

--*/
{
    if (NLExtHeader->DosName.Buffer != NULL) {

        ExFreePool( NLExtHeader->DosName.Buffer );
    }

    if (NLExtHeader->DeviceName.Buffer != NULL) {

        ExFreePool( NLExtHeader->DeviceName.Buffer );
    }
}


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
    )
/*++

Routine Description:

    This routine will allocate a buffer big enough to hold the unicode string
    in SrcName, copy the name, and properly setup DestName

Arguments:

    DestName - the unicode string we are copying too

    SrcName - the unicode string we are copying from

Return Value:

    STATUS_SUCCESS - if it worked
    STATUS_INSUFFICIENT_RESOURCE - if we could not allocate pool

--*/
{
    USHORT bufSize;
    PVOID buf;

    PAGED_CODE();

    ASSERT(DestName->Buffer == NULL);

    bufSize = SrcName->Length;

    if (bufSize > 0) {

        buf = ExAllocatePoolWithTag( NonPagedPool,
                                     bufSize,
                                     PoolTag );

        if (buf == NULL) {

            RtlInitEmptyUnicodeString( DestName, NULL, bufSize );
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        RtlInitEmptyUnicodeString( DestName, buf, bufSize );
        RtlCopyUnicodeString( DestName, SrcName );

    } else {

        RtlInitEmptyUnicodeString( DestName, NULL, bufSize );
    }

    return STATUS_SUCCESS;
}



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
    )
/*++

Routine Description:

    This routine allocates a name control from LookasideList, initializes it,
    and returns a pointer to it.  If allocation fails, NULL is returned.
    Use NLFreeNameControl to free a name control received from this routine.

    NONPAGED: FILESPY needs to call this on the paging file path.

Arguments:

    LookasideList - The lookaside list to allocate the name control from.

Return Value:

    Pointer to the allocated and initialized name control.
    If allocation fails, this function returns NULL.

--*/
{
    PNAME_CONTROL nameCtrl = NULL;

    nameCtrl = ExAllocateFromPagedLookasideList( LookasideList );

    if (nameCtrl == NULL) {

        *NameControl = NULL;
        return STATUS_INSUFFICIENT_RESOURCES;

    }

    NLInitNameControl( nameCtrl );

    *NameControl = nameCtrl;

    return STATUS_SUCCESS;
}

VOID
NLFreeNameControl (
    __in PNAME_CONTROL NameControl,
    __in PPAGED_LOOKASIDE_LIST LookasideList
    )
/*++

Routine Description:

    This routine frees a name control received from NLAllocateNameControl.  It
    cleans up the name control, including any larger buffer allocations, and
    then frees the name control back to LookasideList.  If NameControl is NULL,
    this function does nothing.

Arguments:

    NameControl - The name control release.
    LookasideList - The lookaside list that this name control was initially
        allocated from.

Return Value:

    None.

--*/
{
    if (NameControl != NULL) {

        NLCleanupNameControl( NameControl );
        ExFreeToPagedLookasideList( LookasideList, NameControl );
    }
}

NTSTATUS
NLCheckAndGrowNameControl (
    __inout PNAME_CONTROL NameCtrl,
    __in USHORT NewSize
    )
/*++

Routine Description:

    This routine will check the name control's current buffer capacity.  If
    it is less than NewSize, a new larger name buffer will be allocated and put
    into the name control structure.  If there is already an allocated buffer it
    will be freed.  It will also copy any name information from the old buffer
    into the new buffer.

Arguments:

    NameCtrl - The name control we need a bigger buffer for
    NewSize - Size of the new buffer

Return Value:

    Returns STATUS_INSUFFICIENT_RESOURCES if a larger buffer cannot be
    allocated.

--*/
{
    PAGED_CODE();

    if (NewSize > (NameCtrl->BufferSize - sizeof(WCHAR))) {

        return NLReallocNameControl( NameCtrl,
                                     (NewSize + sizeof(WCHAR)),
                                     NULL );

    }

    return STATUS_SUCCESS;
}

VOID
NLInitNameControl (
    __inout PNAME_CONTROL NameCtrl
    )
/*++

Routine Description:

    This will initialize the name control structure.  Use NLCleanupNameControl
    when you are done with it.

    Use NLAllocateNameControl and NLFreeNameControl if you need to
    allocate & initialize / cleanup & deallocate a name control in one step.

Arguments:

    NameCtrl - The name control to initialize.

Return Value:

    None

--*/
{
    PAGED_CODE();

    NameCtrl->AllocatedBuffer = NULL;
    NameCtrl->BufferSize = sizeof( NameCtrl->SmallBuffer );
    RtlInitEmptyUnicodeString( &NameCtrl->Name,
                               (PWCHAR)NameCtrl->SmallBuffer,
                               (USHORT)NameCtrl->BufferSize );
}

VOID
NLCleanupNameControl (
    __inout PNAME_CONTROL NameCtrl
    )
/*++

Routine Description:

    This will cleanup the name control structure, freeing any buffers
    that were allocated for the NameCtrl.

    Use NLAllocateNameControl and NLFreeNameControl if you need to
    allocate & initialize / cleanup & deallocate a name control in one step.

Arguments:

    NameCtrl - The NAME_CONTROL structure to cleanup.

Return Value:

    None

--*/
{
    PAGED_CODE();

    if (NULL != NameCtrl->AllocatedBuffer) {

        ExFreePoolWithTag( NameCtrl->AllocatedBuffer, NL_POOL_TAG );
        NameCtrl->AllocatedBuffer = NULL;
    }
}

NTSTATUS
NLReallocNameControl (
    __inout PNAME_CONTROL NameCtrl,
    __in ULONG NewSize,
    __out_opt PWCHAR *RetOriginalBuffer
    )
/*++

Routine Description:

    This routine will allocate a new larger name buffer and put it into the
    NameControl structure.  If there is already an allocated buffer it will
    be freed.  It will also copy any name information from the old buffer
    into the new buffer.

Arguments:

    NameCtrl - the name control we need a bigger buffer for
    NewSize - size of the new buffer
    RetOrignalBuffer - if defined, receives the buffer that we were
        going to free.  if NULL was returned no buffer needed to be freed.
      WARNING:  if this parameter is defined and a non-null value is returned
        then the caller MUST free this memory else the memory will be lost.

Return Value:

    Returns STATUS_INSUFFICIENT_RESOURCES if a larger buffer cannot be
    allocated.

--*/
{
    PUCHAR newBuffer;

    PAGED_CODE();

    ASSERT( NewSize > NameCtrl->BufferSize);

    //
    //  Flag no buffer to return yet
    //

    if (RetOriginalBuffer) {

        *RetOriginalBuffer = NULL;
    }

    //
    //  Allocate the new buffer
    //

    newBuffer = ExAllocatePoolWithTag( PagedPool, NewSize, NL_POOL_TAG );

    if (NULL == newBuffer) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    //  Copy data from old buffer if there is any, including any stream
    //  name component.
    //

    if (NameCtrl->Name.Length > 0) {

        ASSERT( NewSize > (USHORT)NameCtrl->Name.Length);
        RtlCopyMemory( newBuffer,
                       NameCtrl->Name.Buffer,
                       NameCtrl->Name.Length );
    }

    //
    //  If we had an old buffer free it if the caller doesn't want
    //  it passed back to him.  This is done because there are
    //  cases where the caller has a pointer into the old buffer so
    //  it can't be freed yet.  The caller must free this memory.
    //

    if (NULL != NameCtrl->AllocatedBuffer) {

        if (RetOriginalBuffer) {

            *RetOriginalBuffer = (PWCHAR)NameCtrl->AllocatedBuffer;

        } else {

            ExFreePoolWithTag( NameCtrl->AllocatedBuffer, NL_POOL_TAG );
        }
    }

    //
    //  Set the new buffer into the name control
    //

    NameCtrl->AllocatedBuffer = newBuffer;
    NameCtrl->BufferSize = NewSize;

    NameCtrl->Name.Buffer = (PWCHAR)newBuffer;
    NameCtrl->Name.MaximumLength = (USHORT)min( 0xfff0, NewSize );

    return STATUS_SUCCESS;
}

