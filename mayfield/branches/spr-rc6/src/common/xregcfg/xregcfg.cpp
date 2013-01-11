	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


VOID
LfsFastIoDetachDevice (
    IN PDEVICE_OBJECT				SourceDevice,
    IN PDEVICE_OBJECT				TargetDevice
	)
{
	PFILESPY_DEVICE_EXTENSION	devExt = SourceDevice->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;

	ASSERT(lfsDeviceExt);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("LfsFastIoDetachDevice entered\n")) ;

	UNREFERENCED_PARAMETER(SourceDevice);
	UNREFERENCED_PARAMETER(TargetDevice);
	
	return;
}


BOOLEAN
LfsFastIoQueryNetworkOpenInfo (
    IN PFILE_OBJECT						FileObject,
    IN BOOLEAN							Wait,
    OUT PFILE_NETWORK_OPEN_INFORMATION	Buffer,
    OUT PIO_STATUS_BLOCK				IoStatus,
    IN PDEVICE_OBJECT					DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(Wait);
	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(IoStatus);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoQueryNetworkOpenInfo entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoMdlRead (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN ULONG				Length,
    IN ULONG				LockKey,
    OUT PMDL				*MdlChain,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(LockKey);
	UNREFERENCED_PARAMETER(MdlChain);
	UNREFERENCED_PARAMETER(IoStatus);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoMdlRead entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoMdlReadComplete (
    IN PFILE_OBJECT		FileObject,
    IN PMDL				MdlChain,
    IN PDEVICE_OBJECT	DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(MdlChain);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoMdlReadComplete entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoPrepareMdlWrite (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN ULONG				Length,
    IN ULONG				LockKey,
    OUT PMDL				*MdlChain,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(LockKey);
	UNREFERENCED_PARAMETER(MdlChain);
	UNREFERENCED_PARAMETER(IoStatus);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoPrepareMdlWrite entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoMdlWriteComplete (
    IN PFILE_OBJECT		FileObject,
    IN PLARGE_INTEGER	FileOffset,
    IN PMDL				MdlChain,
    IN PDEVICE_OBJECT	DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(MdlChain);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoMdlWriteComplete entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoReadCompressed (
    IN PFILE_OBJECT						FileObject,
    IN PLARGE_INTEGER					FileOffset,
    IN ULONG							Length,
    IN ULONG							LockKey,
    OUT PVOID							Buffer,
    OUT PMDL							*MdlChain,
    OUT PIO_STATUS_BLOCK				IoStatus,
    OUT struct _COMPRESSED_DATA_INFO	*CompressedDataInfo,
    IN ULONG							CompressedDataInfoLength,
    IN PDEVICE_OBJECT					DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(LockKey);
	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(MdlChain);
	UNREFERENCED_PARAMETER(IoStatus);
	UNREFERENCED_PARAMETER(CompressedDataInfo);
	UNREFERENCED_PARAMETER(CompressedDataInfoLength);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoReadCompressed entered\n")) ;
	
#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoWriteCompressed (
    IN PFILE_OBJECT						FileObject,
    IN PLARGE_INTEGER					FileOffset,
    IN ULONG							Length,
    IN ULONG							LockKey,
    IN PVOID							Buffer,
    OUT PMDL							*MdlChain,
    OUT PIO_STATUS_BLOCK				IoStatus,
    IN struct _COMPRESSED_DATA_INFO		*CompressedDataInfo,
    IN ULONG							CompressedDataInfoLength,
    IN PDEVICE_OBJECT					DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(LockKey);
	UNREFERENCED_PARAMETER(Buffer);
	UNREFERENCED_PARAMETER(MdlChain);
	UNREFERENCED_PARAMETER(IoStatus);
	UNREFERENCED_PARAMETER(CompressedDataInfo);
	UNREFERENCED_PARAMETER(CompressedDataInfoLength);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoWriteCompressed entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoMdlReadCompleteCompressed (
    IN PFILE_OBJECT		FileObject,
    IN PMDL				MdlChain,
    IN PDEVICE_OBJECT	DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(MdlChain);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoMdlReadCompleteCompressed entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoMdlWriteCompleteCompressed (
    IN PFILE_OBJECT		FileObject,
    IN PLARGE_INTEGER	FileOffset,
    IN PMDL				MdlChain,
    IN PDEVICE_OBJECT	DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(MdlChain);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoMdlWriteCompleteCompressed entered\n")) ;

#ifdef __TEST_MODE__
	lfsPastIoResult.LfsResult	 = LFS_RESULT;
	lfsPastIoResult.PastIoResult = FAST_IO_RESULT;
#else
	if(lfsDeviceExt->HookFastIo == TRUE)
	{
		lfsPastIoResult.LfsResult    = TRUE;
		lfsPastIoResult.PastIoResult = FALSE;
	} else
		lfsPastIoResult.LfsResult    = FALSE;
#endif
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoQueryOpen (
    IN PIRP								Irp,
    OUT PFILE_NETWORK_OPEN_INFORMATION	NetworkInformation,
    IN PDEVIC