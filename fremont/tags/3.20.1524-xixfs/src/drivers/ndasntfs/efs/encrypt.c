Data[0]))->KeyLength
                            = DES_KEYSIZE;

                    ((PEFS_KEY)&(((PFSCTL_INPUT)currentEfsStream)->EfsFsData[0]))->Algorithm
                            = CALG_DES;

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH + sizeof ( EFS_KEY ),
                        &(EfsData.SessionKey),
                        DES_KEYSIZE
                        );

                    RtlCopyMemory(
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH
                            + DES_KEYSIZE + sizeof ( EFS_KEY ) ,
                        ((PUCHAR) currentEfsStream) + EFS_FSCTL_HEADER_LENGTH,
                        DES_KEYSIZE + sizeof ( EFS_KEY )
                        );

                    //
                    // Encrypt our Input data
                    //
                    EfsEncryptKeyFsData(
                        currentEfsStream,
                        inputDataLength,
                        sizeof(ULONG),
                        EFS_FSCTL_HEADER_LENGTH + DES_KEYSIZE + sizeof ( EFS_KEY ),
                        DES_KEYSIZE + sizeof ( EFS_KEY )
                        );

                default:
                    break;
            }

            //
            // Free the memory from the EFS server
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

            if (ProcessAttached){
                KeUnstackDetachProcess(&ApcState);
                ObDereferenceObject(EfsData.LsaProcess);
                ProcessAttached = FALSE;
            }

            if ( NT_SUCCESS(status) ){


                //
                // Prepare a FSCTL IRP
                //
                KeInitializeEvent( &event, SynchronizationEvent, FALSE);

                if ( EfsContext->Status & TURN_ON_ENCRYPTION_BIT ) {
                    FsCode = FSCTL_SET_ENCRYPTION;
                    *(ULONG *) currentEfsStream = EFS_ENCRYPT_STREAM;
                } else {
                    FsCode = FSCTL_ENCRYPTION_FSCTL_IO;
                }

                fsCtlIrp = IoBuildDeviceIoControlRequest( FsCode,
                                                     DeviceObject,
                                                     currentEfsStream,
                                                     inputDataLength,
                                                     NULL,
                                                     0,
                                                     FALSE,
                                                     &event,
                                                     &ioStatus
                                                     );
                if ( fsCtlIrp ) {

                    fsCtlIrpSp = IoGetNextIrpStackLocation( fsCtlIrp );
                    fsCtlIrpSp->MajorFunction = IRP_MJ_FILE_SYSTEM_CONTROL;
                    fsCtlIrpSp->MinorFunction = IRP_MN_USER_FS_REQUEST;
                    fsCtlIrpSp->FileObject = irpSp->FileObject;

                    status = IoCallDriver( DeviceObject, fsCtlIrp);
                    if (status == STATUS_PENDING) {

                        status = KeWaitForSingleObject( &event,
                                               Executive,
                                               KernelMode,
                                               FALSE,
                                               (PLARGE_INTEGER) NULL );
                        status = ioStatus.Status;
                    }

                    if ( !NT_SUCCESS(status) ){
                        //
                        // Write EFS and set Key Blob failed. Failed the create
                        //

 

    Input buffer can only be touched once. This the requirement by the Ntfs & CC.

--*/
{
    ULONGLONG chainBlock;
    ULONGLONG tmpData;
    PUCHAR   KeyTable;

    PAGED_CODE();

    ASSERT (Length % DESX_BLOCKLEN == 0);

    chainBlock = *(ULONGLONG *)IV;
    KeyTable = &(KeyBlob->Key[0]);
    while (Length > 0){

        //
        //  Block chaining
        //
        tmpData = *(ULONGLONG *)InBuffer;
        tmpData ^= chainBlock;

        //
        //  Call LIB to encrypt the DESX_BLOCKLEN bytes
        //  We are using DECRYPT/ENCRYPT for real ENCRYPT/DECRYPT. This is for the backward
        //  compatiblity. The old definitions were reversed.
        //

        desx( OutBuffer, (PUCHAR) &tmpData,  KeyTable, DECRYPT );
        chainBlock = *(ULONGLONG *)OutBuffer;
        Length -= DESX_BLOCKLEN;
        InBuffer += DESX_BLOCKLEN;
        OutBuffer += DESX_BLOCKLEN;
    }
}

VOID
EFSDesXDec(
    IN OUT PUCHAR   Buffer,
    IN PUCHAR   IV,
    IN PKEY_BLOB   KeyBlob,
    IN LONG     Length
    )
/*++

Routine Description:

    This routine implements DESX CBC decryption. The DESX is implemented by
    LIBRARY function desx().

Arguments:

    Buffer - Pointer to the data buffer (decryption in place)
    IV - Initial chaining vector (DESX_BLOCKLEN bytes)
    KeyBlob - Set during the create or FSCTL
    Length - Length of the data in the buffer ( Length % DESX_BLOCKLEN = 0)

--*/
{
    ULONGLONG chainBlock;
    PUCHAR  pBuffer;
    PUCHAR   KeyTable;

    PAGED_CODE();

    ASSERT (Length % DESX_BLOCKLEN == 0);

    pBuffer = Buffer + Length - DESX_BLOCKLEN;
    KeyTable = &(KeyBlob->Key[0]);

    while (pBuffer > Buffer){

        //
        //  Call LIB to decrypt the DESX_BLOCKLEN bytes
        //  We are using DECRYPT/ENCRYPT for real ENCRYPT/DECRYPT. This is for the backward
        //  compatiblity. The old definitions were reversed.
        //

        desx( pBuffer, pBuffer, KeyTable, ENCRYPT );

        //
        //  Undo the block chaining
        //

        chainBlock = *(ULONGLONG *)( pBuffer - DESX_BLOCKLEN );
        *(ULONGLONG *)pBuffer ^= chainBlock;

        pBuffer -= DESX_BLOCKLEN;
    }

    //
    // Now decrypt the first block
    //
    desx( pBuffer, pBuffer, KeyTable, ENCRYPT );

    chainBlock = *(ULONGLONG *)IV;
    *(ULONGLONG *)pBuffer ^= chainBlock;
}

VOID
EFSDes3Enc(
    IN PUCHAR   InBuffer,
    OUT PUCHAR  OutBuffer,
    IN PUCHAR   IV,
    IN PKEY_BLOB   KeyBlob,
    IN LONG     Length
    )
/*++

Routine Description:

    This routine implements DES3 CBC encryption. The DES3 is implemented by 
    LIBRARY function tripledes().

Arguments:

    InBuffer - Pointer to the data buffer (encryption in place)
    IV - Initial chaining vector (DES_BLOCKLEN bytes)
    KeyBlob - Set during the create or FSCTL
    Length - Length of the data in the buffer ( Length % DES_BLOCKLEN = 0)

Note:

    Input buffer can only be touched once. This the requirement by the Ntfs & CC.

--*/
{
    ULONGLONG chainBlock = *(ULONGLONG *)IV;
    ULONGLONG tmpData;
    PUCHAR   KeyTable;
   
    ASSERT (Length % DES_BLOCKLEN == 0);

    EfsData.FipsFunctionTable.FipsBlockCBC(        
        FIPS_CBC_3DES, 
        OutBuffer, 
        InBuffer,
        Length,
        &(KeyBlob->Key[0]), 
        ENCRYPT, 
        (PUCHAR) &chainBlock 
        );
}

VOID
EFSDes3Dec(
    IN OUT PUCHAR   Buffer,
    IN PUCHAR   IV,
    IN PKEY_BLOB   KeyBlob,
    IN LONG     Length
    )
/*++

Routine Description:

    This routine implements DES3 CBC decryption. The DES3 is implemented by 
    LIBRARY function tripledes().

Arguments:

    Buffer - Pointer to the data buffer (decryption in place)
    IV - Initial chaining vector (DES_BLOCKLEN bytes)
    KeyBlob - Set during the create or FSCTL
    Length - Length of the data in the buffer ( Length % DES_BLOCKLEN = 0)

--*/
{
    ULONGLONG ChainIV = *(ULONGLONG *)IV;
   
    ASSERT (Length % DESX_BLOCKLEN == 0);


    EfsData.FipsFunctionTable.FipsBlockCBC( 
        FIPS_CBC_3DES, 
        Buffer, 
        Buffer, 
        Length, 
        &(KeyBlob->Key[0]), 
        DECRYPT, 
        (PUCHAR) &ChainIV 
        );

}
