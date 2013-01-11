/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

   efsrtlsp.c

Abstract:

   This module will provide EFS RTL support routines.

Author:

    Robert Gu (robertg) 20-Dec-1996
Environment:

   Kernel Mode Only

Revision History:

--*/

#include "efsrtl.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, EfsReadEfsData)
#pragma alloc_text(PAGE, EfsVerifyGeneralFsData)
#pragma alloc_text(PAGE, EfsVerifyKeyFsData)
#pragma alloc_text(PAGE, EfsDeleteEfsData)
#pragma alloc_text(PAGE, EfsSetEncrypt)
#pragma alloc_text(PAGE, EfsEncryptStream)
#pragma alloc_text(PAGE, EfsEncryptFile)
#pragma alloc_text(PAGE, EfsDecryptStream)
#pragma alloc_text(PAGE, EfsDecryptFile)
#pragma alloc_text(PAGE, EfsEncryptDir)
#pragma alloc_text(PAGE, EfsModifyEfsState)
#pragma alloc_text(PAGE, GetEfsStreamOffset)
#pragma alloc_text(PAGE, SetEfsData)
#pragma alloc_text(PAGE, EfsFindInCache)
#pragma alloc_text(PAGE, EfsRefreshCache)
#pragma alloc_text(PAGE, SkipCheckStream)
#endif


NTSTATUS
EfsReadEfsData(
       IN OBJECT_HANDLE FileHdl,
       IN PIRP_CONTEXT IrpContext,
       OUT PVOID   *EfsStreamData,
       OUT PULONG   PEfsStreamLength,
       OUT PULONG Information
       )
/*++

Routine Description:

    This is an internal support routine. The purpose is to reduce the code size.
    It is used to read $EFS data and set the context block.

Arguments:

    FileHdl  -- An object handle to access the attached $EFS

    IrpContext -- Used in NtOfsCreateAttributeEx().

    EfsStreamData -- Point to $EFS data read.

    Information -- Return the processing information

Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{
    NTSTATUS ntStatus;
    ATTRIBUTE_HANDLE  attribute = NULL;
    LONGLONG attriOffset;
    ULONG   efsLength;
    PVOID   efsMapBuffer = NULL;
    MAP_HANDLE efsMapHandle;

    PAGED_CODE();

    if (EfsStreamData) {
        *EfsStreamData = NULL;
    }

    try {

        ntStatus = NtOfsCreateAttributeEx(
                             IrpContext,
                             FileHdl,
                             EfsData.EfsName,
                             $LOGGED_UTILITY_STREAM,
                             OPEN_EXISTING,
                             TRUE,
                             &attribute
                             );

        if (NT_SUCCESS(ntStatus)){

                LONGLONG  attrLength;

                NtOfsInitializeMapHandle(&efsMapHandle);

                //
                // Prepare to map and read the $EFS data
                //

                attrLength = NtOfsQueryLength ( attribute );

                if (attrLength <= sizeof ( EFS_DATA_STREAM_HEADER ) ){

                    //
                    // Not our $EFS
                    //

                    NtOfsCloseAttribute(IrpContext, attribute);
                    *Information = EFS_FORMAT_ERROR;
                    ntStatus = STATUS_SUCCESS;

                    leave;

                }

                if ( attrLength > EFS_MAX_LENGTH) {

                    //
                    // EFS stream too long ( > 256K )
                    // We might support that in the future
                    // In that case, we need multiple map window
                    //

                    NtOfsCloseAttribute(IrpContext, attribute);
                    *Information = EFS_FORMAT_ERROR;
                    ntStatus = STATUS_SUCCESS;

                    leave;
                }

                attriOffset = 0;
                *PEfsStreamLength = efsLength = (ULONG) attrLength;

                NtOfsMapAttribute(
                        IrpContext,
                        attribute,
                        attriOffset,
                        efsLength,
                        &efsMapBuffer,
                        &efsMapHandle
                        );

                //
                // Double check the EFS
                //

                if ( efsLength != *(ULONG *)efsMapBuffer){

                    //
                    // Not our $EFS
                    //

                    NtOfsReleaseMap(IrpContext, &efsMapHandle);
                    NtOfsCloseAttribute(IrpContext, attribute);
                    *Information = EFS_FORMAT_ERROR;
                    ntStatus = STATUS_SUCCESS;

                    leave;
                }

                //
                // Allocate memory for $EFS
                //

                if ( EfsStreamData ){

                    //
                    // $EFS must be read
                    //

                    *EfsStreamData = ExAllocatePoolWithTag(
                                        PagedPool,
                                        efsLength,
                                        'msfE'
                                        );

                    if ( NULL == *EfsStreamData ){

                        NtOfsReleaseMap(IrpContext, &efsMapHandle);
                        NtOfsCloseAttribute(IrpContext, attribute);
                        *Information = OUT_OF_MEMORY;
                        ntStatus =  STATUS_INSUFFICIENT_RESOURCES;

                        leave;

                    }

                    RtlCopyMemory(*EfsStreamData, efsMapBuffer, efsLength);

                }

                NtOfsReleaseMap(IrpContext, &efsMapHandle);
                NtOfsCloseAttribute(IrpContext, attribute);

                *Information = EFS_READ_SUCCESSFUL;
                ntStatus = STATUS_SUCCESS;

        } else {

            //
            // Open failed. Not encrypted by EFS.
            //

            *Information = OPEN_EFS_FAIL;
            ntStatus = STATUS_SUCCESS;

        }
    } finally {

        if (AbnormalTermination()) {

            //
            //  Get the exception status
            //
    
            *Information = NTOFS_EXCEPTION;
    
            if (*EfsStreamData) {
                ExFreePool(*EfsStreamData);
                *EfsStreamData = NULL;
            }
            if (efsMapBuffer) {
                NtOfsReleaseMap(IrpContext, &efsMapHandle);
            }
            if (attribute) {
                NtOfsCloseAttribute(IrpContext, attribute);
            }
        }


    }

    return ntStatus;

}

BOOLEAN
EfsVerifyGeneralFsData(
    IN PUCHAR DataOffset,
    IN ULONG InputDataLength
    )
/*++

Routine Description:

    This is an internal support routine. The purpose is to verify the general
    FSCTL input data to see if it is sent by EFS component or not.

    General EFS data format is like the following,

    SessionKey, Handle, Handle, [SessionKey, Handle, Handle]sk

Arguments:

    DataOffset  -- Point to a buffer holding the FSCTL general data part.

    InputDataLength -- The length of the FSCTL input puffer

Return Value:

    TRUE if verified.

--*/
{

    ULONG bytesSame;
    ULONG minLength;

    PAGED_CODE();

    minLength = 4 * DES_BLOCKLEN + 3 * sizeof(ULONG);
    if (InputDataLength < minLength){
        return FALSE;
    }

    //
    // Decrypt the encrypted data part.
    //

    des( DataOffset + 2 * DES_BLOCKLEN,
         DataOffset + 2 * DES_BLOCKLEN,
         &(EfsData.SessionDesTable[0]),
         DECRYPT
       );

    des( DataOffset + 3 * DES_BLOCKLEN,
         DataOffset + 3 * DES_BLOCKLEN,
         &(EfsData.SessionDesTable[0]),
         DECRYPT
       );

    bytesSame = (ULONG)RtlCompareMemory(
                     DataOffset,
                     DataOffset + 2 * DES_BLOCKLEN,
                     2 * DES_BLOCKLEN
                    );

    if (( 2 * DES_BLOCKLEN ) != bytesSame ){

            //
            // Input data format error
            //

            return FALSE;

    }

    bytesSame = (ULONG)RtlCompareMemory(
                     DataOffset,
                     &(EfsData.SessionKey[0]),
                     DES_KEYSIZE
                    );

    if ( DES_KEYSIZE != bytesSame ){

        //
        // Input data is not set by EFS component.
        // The session key does not match.
        //

        return FALSE;

    }

    return TRUE;

}

BOOLEAN
EfsVerifyKeyFsData(
    IN PUCHAR DataOffset,
    IN ULONG InputDataLength
    )
/*++

Routine Description:

    This is an internal support routine. The purpose is to verify the
    FSCTL input data with FEK encrypted to see if it is sent by EFS
    component or not.

    Key EFS data format is like the following,

    FEK, [FEK]sk, [$EFS]

Arguments:

    DataOffset  -- Point to a buffer holding the FSCTL general data part.

    InputDataLength -- The length of the FSCTL input puffer

Return Value:

    TRUE if verified.

--*/
{

    ULONG bytesSame;
    LONG encLength;
    PUCHAR encBuffer;

    PAGED_CODE();

    encLength = EFS_KEY_SIZE( ((PEFS_KEY)DataOffset) );

    if  ( (InputDataLength < (2 * encLength + 3 * sizeof(ULONG))) ||
          (0 != ( encLength % DES_BLOCKLEN )) ||
          ( encLength <= 0 )){
        return FALSE;
    }

    //
    // Decrypt the encrypted data part.
    //

    encBuffer = DataOffset + encLength;

    while ( encLength > 0 ){

        des( encBuffer,
             encBuffer,
             &(EfsData.SessionDesTable[0]),
             DECRYPT 
           );

        encBuffer += DES_BLOCKLEN;
        encLength -= DES_BLOCKLEN;

    }

    //
    //  Compare the two parts.
    //

    encLength = EFS_KEY_SIZE( ((PEFS_KEY)DataOffset) );
    bytesSame = (ULONG)RtlCompareMemory(
                     DataOffset,
                     DataOffset + encLength,
                     encLength
                    );

    if ( ((ULONG) encLength) != bytesSame ){

            //
            // Input data format error
            //

            return FALSE;

    }

    return TRUE;

}

NTSTATUS
EfsDeleteEfsData(
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext
        )
/*++

Routine Description:

    This is an internal support routine. It deletes $EFS.

Arguments:

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{

    ATTRIBUTE_HANDLE  attribute = NULL;
    NTSTATUS ntStatus;

    PAGED_CODE();

    //
    // Delete the $EFS stream
    //

    try {
        ntStatus = NtOfsCreateAttributeEx(
                             IrpContext,
                             FileHdl,
                             EfsData.EfsName,
                             $LOGGED_UTILITY_STREAM,
                             OPEN_EXISTING,
                             TRUE,
                             &attribute
                             );

        if (NT_SUCCESS(ntStatus)){

            NtOfsDeleteAttribute( IrpContext, FileHdl, attribute );

        }
    } finally {

        if (attribute) {

            //
            // According to BrianAn, we shouldn't get exception below.
            //

            NtOfsCloseAttribute(IrpContext, attribute);
        }
    }

    return ntStatus;
}



NTSTATUS
EfsSetEncrypt(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN ULONG EncryptionFlag,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext,
        IN OUT PVOID *Context,
        IN OUT PULONG PContextLength
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT.

Arguments:

    InputData -- Input data buffer of FSCTL.

    InputDataLength -- The length of input data.

    EncryptionFlag -- Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context -- Blob(key) for READ or WRITE later.

    PContextLength -- Length og the key Blob

Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{

    PAGED_CODE();

    switch ( ((PFSCTL_INPUT)InputData)->CipherSubCode ){

        case EFS_ENCRYPT_STREAM:

            return EfsEncryptStream(
                            InputData,
                            InputDataLength,
                            EncryptionFlag,
                            FileHdl,
                            IrpContext,
                            Context,
                            PContextLength
                            );

        case EFS_ENCRYPT_FILE:

             return EfsEncryptFile(
                            InputData,
                            InputDataLength,
                            EncryptionFlag,
                            FileHdl,
                            IrpContext,
                            Context
                            );

        case EFS_DECRYPT_STREAM:

            return EfsDecryptStream(
                    InputData,
                    InputDataLength,
                    EncryptionFlag,
                    FileHdl,
                    IrpContext,
                    Context,
                    PContextLength
                    );

        case EFS_DECRYPT_FILE:
        case EFS_DECRYPT_DIRFILE:

            return EfsDecryptFile(
                    InputData,
                    InputDataLength,
                    FileHdl,
                    IrpContext
                    );

        case EFS_ENCRYPT_DIRSTR:

             return EfsEncryptDir(
                            InputData,
                            InputDataLength,
                            EncryptionFlag,
                            FileHdl,
                            IrpContext
                            );

            break;

        case EFS_DECRYPT_DIRSTR:

            //
            // EFS ignore this case.\
            //
            break;

        default:
            break;

    }
    return STATUS_SUCCESS;
}

NTSTATUS
EfsEncryptStream(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN ULONG EncryptionFlag,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext,
        IN OUT PVOID *Context,
        IN OUT PULONG PContextLength
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT for encrypting a stream. It verifies the caller
    and set the key Blob for the stream.

Arguments:

    InputData -- Input data buffer of FSCTL.

    InputDataLength -- The length of input data.

    EncryptionFlag - Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context -- Blob(key) for READ or WRITE later.

    PContextLength -- Length of the key Blob


Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{

    ULONG efsLength;
    ULONG information;
    PVOID efsStreamData = NULL;
    PVOID efsKeyBlob = NULL;
    PEFS_KEY    efsKey = NULL;
    NTSTATUS ntStatus;
    ULONG bytesSame;

    PAGED_CODE();

    if ( EncryptionFlag & STREAM_ENCRYPTED ) {

        //
        // Stream already encrypted
        //

        return STATUS_SUCCESS;
    }

    if ( *Context ){

        //
        // The key Blob is already set without the bit set first.
        // Not set by EFS
        //

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // [FsData] = FEK, [FEK]sk, $EFS
    //

    if ( !EfsVerifyKeyFsData(
            &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
            InputDataLength) ){

        //
        // Input data format error
        //

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Try to read an existing $EFS
    //

    ntStatus = EfsReadEfsData(
                        FileHdl,
                        IrpContext,
                        &efsStreamData,
                        &efsLength,
                        &information
                        );

    if ( EFS_READ_SUCCESSFUL == information ){

        BOOLEAN continueProcess = TRUE;
        ULONG efsOffset;

        efsOffset = GetEfsStreamOffset( InputData );

        if ( 0 == (EncryptionFlag & FILE_ENCRYPTED) ){
            //
            // File is not encrypted, but $EFS exist. Invalid status.
            // May caused by a crash during the SET_ENCRYPT file call.
            //

            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
            continueProcess = FALSE;

        } else if ( efsLength != ( InputDataLength - efsOffset )) {
            //
            // $EFS stream length does not match
            //

            ntStatus = STATUS_INVALID_PARAMETER;
            continueProcess = FALSE;

        }

        if ( !continueProcess ) {

            ExFreePool( efsStreamData );
            return ntStatus;

        }

        //
        // Got the $EFS. Now double check the match of the $EFS stream.
        // EFS use the same $EFS for all the stream within a file.
        // Skip comparing the length and status fields.
        //

        bytesSame = (ULONG)RtlCompareMemory(
                        (PUCHAR)efsStreamData + 2 * sizeof(ULONG),
                        InputData + efsOffset + 2 * sizeof(ULONG),
                        efsLength - 2 * sizeof(ULONG)
                        );

        ExFreePool( efsStreamData );

        if ( bytesSame != efsLength - 2 * sizeof(ULONG) ){

            //
            // The EFS are not the same length
            //

            return STATUS_INVALID_PARAMETER;

        }

        efsKey = (PEFS_KEY)&(((PFSCTL_INPUT)InputData)->EfsFsData[0]);
        efsKeyBlob = GetKeyBlobBuffer(efsKey->Algorithm);
        if ( NULL == efsKeyBlob ){
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        if (!SetKeyTable( efsKeyBlob, efsKey )){

            ExFreeToNPagedLookasideList(((PKEY_BLOB)efsKeyBlob)->MemSource, efsKeyBlob);

            //
            // We might be able to return a better error code if needed.
            // This is not in the CreateFile() path.
            //

            return STATUS_ACCESS_DENIED;
        }

        *Context = efsKeyBlob;
        *PContextLength = ((PKEY_BLOB)efsKeyBlob)->KeyLength;
        return STATUS_SUCCESS;

    }

    //
    // Try to encrypt a stream but the $EFS is not there.
    // EFS server will always call encrypt on a file first.
    //

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
EfsEncryptFile(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN ULONG EncryptionFlag,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext,
        IN OUT PVOID *Context
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT for encrypting a file. It does not deal with
    the stream, it only writes the initial $EFS and put the file in
    a transition status so that no one else can open the file.

Arguments:

    InputData -- Input data buffer of FSCTL.

    InputDataLength -- The length of input data.

    EncryptionFlag - Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context - BLOB(key) for READ or WRITE later.


Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{

    ULONG efsLength;
    ULONG information;
    ULONG efsOffset;
    PVOID efsStreamData = NULL;
    PVOID efsKeyBlob = NULL;
    NTSTATUS ntStatus;
    ATTRIBUTE_HANDLE  attribute = NULL;

    PAGED_CODE();

    if ( EncryptionFlag & FILE_ENCRYPTED ){

        //
        // File encrypted.
        //

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // [FsData] = FEK, [FEK]sk, $EFS
    //

    if ( !EfsVerifyKeyFsData(
            &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
            InputDataLength) ){

        //
        // Input data format error
        //

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Allocate memory for $EFS
    // Create the $EFS, if there is one, overwrite it.
    //

    efsOffset = GetEfsStreamOffset( InputData );
    efsLength = InputDataLength - efsOffset;

    try {

        ntStatus = NtOfsCreateAttributeEx(
                         IrpContext,
                         FileHdl,
                         EfsData.EfsName,
                         $LOGGED_UTILITY_STREAM,
                         CREATE_NEW,
                         TRUE,
                         &attribute
                         );

#if DBG
    if ( (EFSTRACEALL | EFSTRACELIGHT ) & EFSDebug ){

        DbgPrint( "\n EFSFILTER: Create Attr. Status %x\n", ntStatus );

    }
#endif

        if (NT_SUCCESS(ntStatus)){

            LONGLONG    attriOffset = 0;
            LONGLONG    attriLength = (LONGLONG) efsLength;

            NtOfsSetLength(
                    IrpContext,
                    attribute,
                    attriLength
                    );

            //
            // Write the $EFS with transition status
            //

            *(PULONG)(InputData + efsOffset + sizeof(ULONG)) =
                    EFS_STREAM_TRANSITION;

            NtOfsPutData(
                    IrpContext,
                    attribute,
                    attriOffset,
                    efsLength,
                    InputData + efsOffset
                    );


            NtOfsFlushAttribute (IrpContext, attribute, FALSE);

        }
    } finally {

        if (attribute) {

            NtOfsCloseAttribute(IrpContext, attribute);

        }
    }

    return ntStatus;
}

NTSTATUS
EfsDecryptStream(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN ULONG EncryptionFlag,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext,
        IN OUT PVOID *Context,
        IN OUT PULONG PContextLength
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT for decrypting a stream. It sets the key Blob to NULL.

Arguments:

    InputData -- Input data buffer of FSCTL.

    EncryptionFlag - Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context -- Blob(key) for READ or WRITE later.

    PContextLength -- Length of the key Blob.

Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/

{
    ULONG efsLength;
    ULONG information;
    NTSTATUS ntStatus;

    PAGED_CODE();

    if ( 0 == (EncryptionFlag & STREAM_ENCRYPTED) ) {

        //
        // Stream already decrypted
        //

        return STATUS_SUCCESS;
    }

    if ( 0 == (EncryptionFlag & FILE_ENCRYPTED)){

        //
        // File decrypted but the stream is still encrypted.
        //

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // [FsData] = SessionKey, Handle, Handle, [SessionKey, Handle, Handle]sk
    // Verify the FsData format.
    //

    if (!EfsVerifyGeneralFsData(
                &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
                InputDataLength)){

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Try to read an existing $EFS
    //

    ntStatus = EfsReadEfsData(
                        FileHdl,
                        IrpContext,
                        NULL,
                        &efsLength,
                        &information
                        );

    if ( EFS_READ_SUCCESSFUL == information ){

        //
        // Everything is OK. We do not check user ID here,
        // we suppose that has been checked during the Open path.
        // Clear the key Blob. The caller should flushed this
        // stream before the FSCTL is issued.
        //

        if ( *Context ){
            CheckValidKeyBlock(*Context,"Please contact RobertG if you see this line, efsrtlsp.c.\n");
            FreeMemoryBlock(Context);
            *PContextLength = 0;
        }

        return STATUS_SUCCESS;

    } else if ( ( OPEN_EFS_FAIL == information ) ||
                ( EFS_FORMAT_ERROR == information ) ) {

        //
        // EFS does not exist or not encrypted by the EFS ?
        //

        ntStatus =  STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // Other error while opening $EFS
    //

    return ntStatus;
}

NTSTATUS
EfsDecryptFile(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT for decrypting a file. It deletes the $EFS. NTFS
    will clear the bit if STATUS_SUCCESS returned.

Arguments:

    InputData -- Input data buffer of FSCTL.

    EncryptionFlag - Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context - BLOB(key) for READ or WRITE later.


Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/

{
    ULONG efsLength;
    ULONG information;
    NTSTATUS ntStatus;

    PAGED_CODE();

    //
    // It is possible to have following situations,
    // File bit set but no $EFS. Crash inside this call last time.
    // File bit not set, $EFS exist. Crash inside EFS_ENCRYPT_FILE.
    //

    //
    // [FsData] = SessionKey, Handle, Handle, [SessionKey, Handle, Handle]sk
    // Verify the FsData format.
    //

    if (!EfsVerifyGeneralFsData(
            &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
            InputDataLength)){

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Try to read an existing $EFS
    //

    ntStatus = EfsReadEfsData(
                        FileHdl,
                        IrpContext,
                        NULL,
                        &efsLength,
                        &information
                        );

    if ( EFS_READ_SUCCESSFUL == information ){

        //
        // Everything is OK.
        //

        return ( EfsDeleteEfsData( FileHdl, IrpContext ) );

    } else if ( OPEN_EFS_FAIL == information ){

        //
        // Bit set, no $EFS. OK, NTFS will clear the bit.
        //

        return STATUS_SUCCESS;

    }

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
EfsEncryptDir(
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN ULONG EncryptionFlag,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext
        )
/*++

Routine Description:

    This is an internal support routine. It process the call of
    FSCTL_SET_ENCRYPT for encrypting a directory. It writes initial $EFS.

Arguments:

    InputData -- Input data buffer of FSCTL.

    InputDataLength -- The length of input data.

    EncryptionFlag - Indicating if this stream is encrypted or not.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

    Context - BLOB(key) for READ or WRITE later.


Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{

    ULONG efsLength;
    ULONG information;
    ULONG efsStreamOffset;
    PVOID efsStreamData = NULL;
    PVOID efsKeyBlob = NULL;
    NTSTATUS ntStatus;
    ATTRIBUTE_HANDLE  attribute = NULL;

    PAGED_CODE();

    if ( EncryptionFlag & STREAM_ENCRYPTED ){

        //
        // Dir string already encrypted.
        //

        return STATUS_INVALID_DEVICE_REQUEST;

    }

    //
    // [FsData] = SessionKey, Handle, Handle, [SessionKey, Handle, Handle]sk
    // Verify the FsData format.
    //

    if (!EfsVerifyGeneralFsData(
            &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
            InputDataLength)){

        return STATUS_INVALID_PARAMETER;

    }

    //
    // Allocate memory for $EFS
    // Create the $EFS, if there is one, overwrite it.
    //

    efsStreamOffset = FIELD_OFFSET( FSCTL_INPUT, EfsFsData[0] )
                      + FIELD_OFFSET( GENERAL_FS_DATA, EfsData[0]);

    efsLength = InputDataLength - efsStreamOffset;

    try {

        ntStatus = NtOfsCreateAttributeEx(
                         IrpContext,
                         FileHdl,
                         EfsData.EfsName,
                         $LOGGED_UTILITY_STREAM,
                         CREATE_NEW,
                         TRUE,
                         &attribute
                         );

        if (NT_SUCCESS(ntStatus)){

            LONGLONG    attriOffset = 0;
            LONGLONG    attriLength = (LONGLONG) efsLength;

            NtOfsSetLength(
                    IrpContext,
                    attribute,
                    attriLength
                    );

            //
            // Write the $EFS
            //

            NtOfsPutData(
                    IrpContext,
                    attribute,
                    attriOffset,
                    efsLength,
                    InputData + efsStreamOffset
                    );


            NtOfsFlushAttribute (IrpContext, attribute, FALSE);

        }
    } finally {

        if (attribute) {
            NtOfsCloseAttribute(IrpContext, attribute);
        }
    }

    return ntStatus;
}

NTSTATUS
EfsModifyEfsState(
        IN ULONG FunctionCode,
        IN PUCHAR InputData,
        IN ULONG InputDataLength,
        IN OBJECT_HANDLE FileHdl,
        IN PIRP_CONTEXT IrpContext
        )
/*++

Routine Description:

    This is an internal support routine. It modifies the state field of $EFS.

Arguments:

    FunctionCode -- EFS private code for FSCTL

    InputData -- Input data buffer of FSCTL.

    FileHdl  -- An object handle to access the attached $EFS.

    IrpContext -- Used in NtOfsCreateAttributeEx().

Return Value:

    Result of the operation.
    The value will be used to return to NTFS.

--*/
{
    NTSTATUS ntStatus;
    ATTRIBUTE_HANDLE  attribute = NULL;

    PAGED_CODE();

    //
    // [FsData] = SessionKey, Handle, Handle, [SessionKey, Handle, Handle]sk
    // Verify the FsData format.
    //

    if (!EfsVerifyGeneralFsData(
            &(((PFSCTL_INPUT)InputData)->EfsFsData[0]),
            InputDataLength)){

        return STATUS_INVALID_PARAMETER;

    }

    try {

        ntStatus = NtOfsCreateAttributeEx(
                         IrpContext,
                         FileHdl,
                         EfsData.EfsName,
                         $LOGGED_UTILITY_STREAM,
                         OPEN_EXISTING,
                         TRUE,
                         &attribute
                         );

        if (NT_SUCCESS(ntStatus)){

            ULONG   efsStatus = EFS_STREAM_NORMAL;

            if ( EFS_DECRYPT_BEGIN == FunctionCode ){

                 efsStatus = EFS_STREAM_TRANSITION;

            }

            //
            // Modify the status
            //

            NtOfsPutData(
                    IrpContext,
                    attribute,
                    (LONGLONG) &((( EFS_STREAM * ) 0)->Status),
                    sizeof( efsStatus ),
                    &efsStatus
                    );

            NtOfsFlushAttribute (IrpContext, attribute, FALSE);

        }
    } finally {

        if (attribute) {
            NtOfsCloseAttribute(IrpContext, attribute);
        }
    }

    return ntStatus;
}

ULONG
GetEfsStreamOffset(
        IN PUCHAR InputData
        )
/*++

Ro) currentEfsStream,
                              currentEfsStreamLength,
                              &bufferBase,
                              &bufferLength
                              );

               if ( OldClientToken ) {
                   PsImpersonateClient(
                       PsGetCurrentThread(),
                       OldClientToken,
                       OldCopyOnOpen,
                       OldEffectiveOnly,
                       OldImpersonationLevel
                       );
                   PsDereferenceImpersonationToken(OldClientToken);
                } else {
                    PsRevertToSelf( );
                }
                break;

            case NEW_DIR_EFS_REQUIRED:
                //
                // Call service to get new $EFS
                //

                if (EfsContext->Flags & SYSTEM_IS_READONLY) {
                    ASSERT(FALSE);
                    status = STATUS_MEDIA_WRITE_PROTECTED;
                    if ( OldClientToken ) {
                        PsDereferenceImpersonationToken(OldClientToken);
                    }
                    break;
                }

                PsImpersonateClient(
                    PsGetCurrentThread(),
                    accessToken,
                    TRUE,
                    TRUE,
                    ImpersonationLevel
                    );

                status = GenerateDirEfs(
                              (PEFS_DATA_STREAM_HEADER) currentEfsStream,
                              currentEfsStreamLength,
                              &efsStream,
                              &bufferBase,
                              &bufferLength
                              );

                if ( OldClientToken ) {
                    PsImpersonateClient(
                        PsGetCurrentThread(),
                        OldClientToken,
                        OldCopyOnOpen,
                        OldEffectiveOnly,
                        OldImpersonationLevel
                        );
                    PsDereferenceImpersonationToken(OldClientToken);
                } else {
                    PsRevertToSelf( );
                }
                break;

            case TURN_ON_BIT_ONLY:
                //
                // Fall through intended
                //

            default:

                if ( OldClientToken ) {
                    PsDereferenceImpersonationToken(OldClientToken);
                }

                break;
        }


        if ( ProcessNeedAttach ){

            KeStackAttachProcess (
                EfsData.LsaProcess,
                &ApcState
                );
            ProcessAttached = TRUE;

        }

        if (fek && (fek->Algorithm == CALG_3DES) && !EfsData.FipsFunctionTable.Fips3Des3Key ) {

            //
            // User requested 3des but fips is not available, quit.
            //


            if (bufferBase){
    
                SIZE_T bufferSize;
    
                bufferSize = bufferLength;
                ZwFreeVirtualMemory(
                    CrntProcess,
                    &bufferBase,
                    &bufferSize,
                    MEM_RELEASE
                    );
    
            }
            status = STATUS_ACCESS_DENIED;
        }

        if ( NT_SUCCESS(status) ){

            KEVENT event;
            IO_STATUS_BLOCK ioStatus;
            PIRP fsCtlIrp;
            PIO_STACK_LOCATION fsCtlIrpSp;
            ULONG inputDataLength;
            ULONG actionType;
            ULONG usingCurrentEfs;
            ULONG FsCode;
            PULONG pUlong;

            //
            // We got our FEK, $EFS. Set it with a FSCTL
            // Prepare the input data buffer first
            //

            switch ( EfsContext->Status & ACTION_REQUIRED ){
                case VERIFY_USER_REQUIRED:

                    EfsId =  ExAllocatePoolWithTag(
                                PagedPool,
                                sizeof (GUID),
                                'msfE'
                                );

                    if ( EfsId ){
                        RtlCopyMemory(
                            EfsId,
                            &(((PEFS_DATA_STREAM_HEADER) currentEfsStream)->EfsId),
                            sizeof( GUID ) );
                    }

                    //
                    // Free memory first
                    //
                    ZwFreeVirtualMemory(
                        CrntProcess,
                        &currentEfsStream,
                        &regionSize,
                        MEM_RELEASE
                        );

                    //
                    // Prepare input data buffer
                    //

                    inputDataLength = EFS_FSCTL_HEADER_LENGTH + 2 * EFS_KEY_SIZE( fek );

                    actionType = SET_EFS_KEYBLOB;

                    if ( efsStream && !(EfsContext->Flags & SYSTEM_IS_READONLY)){
                        //
                        // $EFS updated
                        //

                        inputDataLength += *(ULONG *)efsStream;
                        actionType |= WRITE_EFS_ATTRIBUTE;
                    }

                    currentEfsStream = ExAllocatePoolWithTag(
                                PagedPool,
                                inputDataLength,
                                'msfE'
                                );

                    //
                    // Deal with out of memory here
                    //
                    if ( NULL == currentEfsStream ){

                        //
                        // Out of memory
                        //

                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;

                    }

                    pUlong = (ULONG *) currentEfsStream;
                    *pUlong = ((PFSCTL_INPUT)currentEfsStream)->CipherSubCode
                                = actionType;

                    ((PFSCTL_INPUT)currentEfsStream)->EfsFsCode = EFS_SET_ATTRIBUTE;

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH,
                        fek,
                        EFS_KEY_SIZE( fek )
                        );

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH
                            + EFS_KEY_SIZE( fek ),
                        fek,
                        EFS_KEY_SIZE( fek )
                        );

                    if ( efsStream && !(EfsContext->Flags & SYSTEM_IS_READONLY)){

                        RtlCopyMemory(
                            ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH
                                + 2 * EFS_KEY_SIZE( fek ),
                            efsStream,
                            *(ULONG *)efsStream
                            );

                    }

                    //
                    // Encrypt our Input data
                    //
                    EfsEncryptKeyFsData(
                        currentEfsStream,
                        inputDataLength,
                        sizeof(ULONG),
                        EFS_FSCTL_HEADER_LENGTH + EFS_KEY_SIZE( fek ),
                        EFS_KEY_SIZE( fek )
                        );

                    break;
                case NEW_FILE_EFS_REQUIRED:

                    EfsId =  ExAllocatePoolWithTag(
                                PagedPool,
                                sizeof (GUID),
                                'msfE'
                                );

                    if ( EfsId ){
                        RtlCopyMemory(
                            EfsId,
                            &(efsStream->EfsId),
                            sizeof( GUID ) );
                    }

                    //
                    // Free memory first
                    //

                    if ( currentEfsStream ){
                        ZwFreeVirtualMemory(
                            CrntProcess,
                            &currentEfsStream,
                            &regionSize,
                            MEM_RELEASE
                            );
                    }

                    //
                    // Prepare input data buffer
                    //

                    inputDataLength = EFS_FSCTL_HEADER_LENGTH
                                      + 2 * EFS_KEY_SIZE( fek )
                                      + *(ULONG *)efsStream;

                    currentEfsStream = ExAllocatePoolWithTag(
                                PagedPool,
                                inputDataLength,
                                'msfE'
                                );

                    //
                    // Deal with out of memory here
                    //
                    if ( NULL == currentEfsStream ){

                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;

                    }

                    pUlong = (ULONG *) currentEfsStream;
                    *pUlong = ((PFSCTL_INPUT)currentEfsStream)->CipherSubCode
                                            = WRITE_EFS_ATTRIBUTE | SET_EFS_KEYBLOB;

                    ((PFSCTL_INPUT)currentEfsStream)->EfsFsCode = EFS_SET_ATTRIBUTE;

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH,
                        fek,
                        EFS_KEY_SIZE( fek )
                        );

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH
                            + EFS_KEY_SIZE( fek ),
                        fek,
                        EFS_KEY_SIZE( fek )
                        );

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH
                            + 2 *  EFS_KEY_SIZE( fek ) ,
                        efsStream,
                        *(ULONG *)efsStream
                        );

                    //
                    // Encrypt our Input data
                    //
                    EfsEncryptKeyFsData(
                        currentEfsStream,
                        inputDataLength,
                        sizeof(ULONG),
                        EFS_FSCTL_HEADER_LENGTH + EFS_KEY_SIZE( fek ),
                        EFS_KEY_SIZE( fek )
                        );

                    break;
                case NEW_DIR_EFS_REQUIRED:
                    //
                    // Prepare input data buffer
                    //

                    inputDataLength = EFS_FSCTL_HEADER_LENGTH
                                      + 2 * ( sizeof( EFS_KEY ) + DES_KEYSIZE );

                    if ( NULL == efsStream ){

                        //
                        // New directory will inherit the parent $EFS
                        //

                        usingCurrentEfs = TRUE;
                        inputDataLength += currentEfsStreamLength;
                        efsStream = currentEfsStream;

                    } else {

                        //
                        // New $EFS generated. Not in ver 1.0
                        //

                        usingCurrentEfs = FALSE;
                        inputDataLength += *(ULONG *)efsStream;

                        //
                        // Free memory first
                        //

                        if (currentEfsStream){
                            ZwFreeVirtualMemory(
                                CrntProcess,
                                &currentEfsStream,
                                &regionSize,
                                MEM_RELEASE
                                );
                        }

                    }

                    currentEfsStream = ExAllocatePoolWithTag(
                                PagedPool,
                                inputDataLength,
                                'msfE'
                                );

                    //
                    // Deal with out of memory here
                    //
                    if ( NULL == currentEfsStream ){

                        status = STATUS_INSUFFICIENT_RESOURCES;
                        break;

                    }

                    pUlong = (ULONG *) currentEfsStream;
                    *pUlong = ((PFSCTL_INPUT)currentEfsStream)->CipherSubCode
                                = WRITE_EFS_ATTRIBUTE;

                    ((PFSCTL_INPUT)currentEfsStream)->EfsFsCode = EFS_SET_ATTRIBUTE;

                    //
                    // Make up an false FEK with session key
                    //

                    ((PEFS_KEY)&(((PFSCTL_INPUT)currentEfsStream)->EfsFsData[0])