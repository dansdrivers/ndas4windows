#define	__PRIMARY__
#define __SECONDARY__

#include "LfsProc.h"

PFAST_IO_DISPATCH	LfsFastIoDispatch = NULL;


extern PFAST_IO_DISPATCH	LfsFastIoDispatch;

#ifdef __TEST_MODE__
#define LFS_RESULT		TRUE
#define FAST_IO_RESULT	FALSE
#endif

BOOLEAN
LfsFastIoCheckIfPossible (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN ULONG				Length,
    IN BOOLEAN				Wait,
    IN ULONG				LockKey,
    IN BOOLEAN				CheckForReadOperation,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	LFS_FILE_IO					lfsFileIo;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	lfsFileIo.FileIoType = LFS_FILE_IO_FAST_IO_CHECK_IF_POSSIBLE;
	
	lfsFileIo.FastIoCheckIfPossible.FileObject	= FileObject;
	lfsFileIo.FastIoCheckIfPossible.FileOffset	= FileOffset;
	lfsFileIo.FastIoCheckIfPossible.Length		= Length;
	lfsFileIo.FastIoCheckIfPossible.Wait		= Wait;
	lfsFileIo.FastIoCheckIfPossible.LockKey		= LockKey;
	lfsFileIo.FastIoCheckIfPossible.CheckForReadOperation = CheckForReadOperation;
	lfsFileIo.FastIoCheckIfPossible.IoStatus	= IoStatus;


	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoCheckIfPossible entered\n")) ;

	if(lfsDeviceExt->FilteringMode == LFS_SECONDARY)
	{
		ASSERT(lfsDeviceExt->Secondary);
		
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= Secondary_RedirectFileIo(lfsDeviceExt->Secondary, &lfsFileIo);
	}
	else if(lfsDeviceExt->FilteringMode == LFS_READ_ONLY)
	{
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= FALSE;
	}
	else
		lfsPastIoResult.LfsResult    = FALSE;
	
	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoRead (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN ULONG				Length,
    IN BOOLEAN				Wait,
    IN ULONG				LockKey,
    OUT PVOID				Buffer,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;
	LFS_FILE_IO					lfsFileIo;


	lfsFileIo.FileIoType = LFS_FILE_IO_READ;
	
	lfsFileIo.Read.FileObject	= FileObject;
	lfsFileIo.Read.FileOffset	= FileOffset;
	lfsFileIo.Read.Length		= Length;
	lfsFileIo.Read.Wait			= Wait;
	lfsFileIo.Read.LockKey		= LockKey;
	lfsFileIo.Read.Buffer		= Buffer;
	lfsFileIo.Read.IoStatus		= IoStatus;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	if(lfsDeviceExt->FilteringMode == LFS_SECONDARY)
	{
		ASSERT(lfsDeviceExt->Secondary);

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFastIoRead entered\n")) ;
		
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= Secondary_RedirectFileIo(lfsDeviceExt->Secondary, &lfsFileIo);
	}
	else if(lfsDeviceExt->FilteringMode == LFS_READ_ONLY)
	{
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= FALSE;
	}
	else
		lfsPastIoResult.LfsResult    = FALSE;

	return lfsPastIoResult.Result;
}


BOOLEAN        
LfsFastIoWrite (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN ULONG				Length,
    IN BOOLEAN				Wait,
    IN ULONG				LockKey,
    IN PVOID				Buffer,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;
	LFS_FILE_IO					lfsFileIo;


	lfsFileIo.FileIoType = LFS_FILE_IO_WRITE;
	
	lfsFileIo.Write.FileObject	= FileObject;
	lfsFileIo.Write.FileOffset	= FileOffset;
	lfsFileIo.Write.Length		= Length;
	lfsFileIo.Write.Wait		= Wait;
	lfsFileIo.Write.LockKey		= LockKey;
	lfsFileIo.Write.Buffer		= Buffer;
	lfsFileIo.Write.IoStatus	= IoStatus;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	if(lfsDeviceExt->FilteringMode == LFS_SECONDARY)
	{
		ASSERT(lfsDeviceExt->Secondary);
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFastIoWrite entered\n")) ;

		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= Secondary_RedirectFileIo(lfsDeviceExt->Secondary, &lfsFileIo);
	}
	else if(lfsDeviceExt->FilteringMode == LFS_READ_ONLY)
	{
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= FALSE;
	}
	else
		lfsPastIoResult.LfsResult    = FALSE;

	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoQueryBasicInfo (
    IN PFILE_OBJECT				FileObject,
    IN BOOLEAN					Wait,
    OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK		IoStatus,
    IN PDEVICE_OBJECT			DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;
	LFS_FILE_IO					lfsFileIo;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	lfsFileIo.FileIoType = LFS_FILE_IO_QUERY_BASIC_INFO;
	
	lfsFileIo.QueryBasicInfo.FileObject	= FileObject;
	lfsFileIo.QueryBasicInfo.Wait		= Wait;
	lfsFileIo.QueryBasicInfo.Buffer		= Buffer;
	lfsFileIo.QueryBasicInfo.IoStatus	= IoStatus;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	if(lfsDeviceExt->FilteringMode == LFS_SECONDARY)
	{
		ASSERT(lfsDeviceExt->Secondary);
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFastIoQueryBasicInfo entered\n")) ;

		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= Secondary_RedirectFileIo(lfsDeviceExt->Secondary, &lfsFileIo);
	}
	else if(lfsDeviceExt->FilteringMode == LFS_READ_ONLY)
	{
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= FALSE;
	}
	else
		lfsPastIoResult.LfsResult    = FALSE;

	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoQueryStandardInfo (
    IN PFILE_OBJECT					FileObject,
    IN BOOLEAN						Wait,
    OUT PFILE_STANDARD_INFORMATION	Buffer,
    OUT PIO_STATUS_BLOCK			IoStatus,
    IN PDEVICE_OBJECT				DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;
	LFS_FILE_IO					lfsFileIo;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

	lfsFileIo.FileIoType = LFS_FILE_IO_QUERY_STANDARD_INFO;
	
	lfsFileIo.QueryBasicInfo.FileObject	= FileObject;
	lfsFileIo.QueryBasicInfo.Wait		= Wait;
	lfsFileIo.QueryBasicInfo.Buffer		= Buffer;
	lfsFileIo.QueryBasicInfo.IoStatus	= IoStatus;


	if(lfsDeviceExt->FilteringMode == LFS_SECONDARY)
	{
		ASSERT(lfsDeviceExt->Secondary);
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsFastIoQueryStandardInfo entered\n")) ;

		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= Secondary_RedirectFileIo(lfsDeviceExt->Secondary, &lfsFileIo);
	}
	else if(lfsDeviceExt->FilteringMode == LFS_READ_ONLY)
	{
		lfsPastIoResult.LfsResult		= TRUE;
		lfsPastIoResult.PastIoResult	= FALSE;
	}
	else
		lfsPastIoResult.LfsResult    = FALSE;

	return lfsPastIoResult.Result;
}


BOOLEAN
LfsFastIoLock (
    IN PFILE_OBJECT					FileObject,
    IN PLARGE_INTEGER				FileOffset,
    IN PLARGE_INTEGER				Length,
    PEPROCESS						ProcessId,		
    ULONG							Key,
    BOOLEAN							FailImmediately,
    BOOLEAN							ExclusiveLock,
    OUT PIO_STATUS_BLOCK			IoStatus,
    IN PDEVICE_OBJECT				DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(FileOffset);
	UNREFERENCED_PARAMETER(Length);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(FailImmediately);
	UNREFERENCED_PARAMETER(ExclusiveLock);
	UNREFERENCED_PARAMETER(IoStatus);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoLock entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif
	
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
LfsFastIoUnlockSingle (
    IN PFILE_OBJECT			FileObject,
    IN PLARGE_INTEGER		FileOffset,
    IN PLARGE_INTEGER		Length,
    PEPROCESS				ProcessId,
    ULONG					Key,
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
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(IoStatus);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoUnlockSingle entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif
	
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
LfsFastIoUnlockAll (
    IN PFILE_OBJECT			FileObject,
    PEPROCESS				ProcessId,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(IoStatus);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoUnlockAll entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif
	
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
LfsFastIoUnlockAllByKey (
    IN PFILE_OBJECT			FileObject,
    PVOID					ProcessId,
    ULONG					Key,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(IoStatus);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoUnlockAllByKey entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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
LfsFastIoDeviceControl (
    IN PFILE_OBJECT			FileObject,
    IN BOOLEAN				Wait,
    IN PVOID				InputBuffer	 OPTIONAL,
    IN ULONG				InputBufferLength,
    OUT PVOID				OutputBuffer OPTIONAL,
    IN ULONG				OutputBufferLength,
    IN ULONG				IoControlCode,
    OUT PIO_STATUS_BLOCK	IoStatus,
    IN PDEVICE_OBJECT		DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(FileObject);
	UNREFERENCED_PARAMETER(Wait);
	UNREFERENCED_PARAMETER(InputBuffer);
	UNREFERENCED_PARAMETER(InputBufferLength);
	UNREFERENCED_PARAMETER(OutputBuffer);
	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(IoControlCode);
	UNREFERENCED_PARAMETER(IoStatus);
	
	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoDeviceControl entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif


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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif


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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif


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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif


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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif


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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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
#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif

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
    IN PDEVICE_OBJECT					DeviceObject
    )
{
	PFILESPY_DEVICE_EXTENSION	devExt = DeviceObject->DeviceExtension;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt = &devExt->LfsDeviceExt;
	LFS_FAST_IO_RESULT			lfsPastIoResult;

	UNREFERENCED_PARAMETER(Irp);
	UNREFERENCED_PARAMETER(NetworkInformation);

	SPY_LOG_PRINT( LFS_DEBUG_LFS_NOISE, ("LfsFastIoQueryOpen entered\n")) ;

#ifdef __NDFS__
	if (lfsDeviceExt->FileSystemType == LFS_FILE_SYSTEM_NDFAT)
		return (lfsPastIoResult.LfsResult = FALSE);
#endif
	
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
CreateFastIoDispatch(
	VOID
	)
{
    PFAST_IO_DISPATCH	fastIoDispatch;

	
	fastIoDispatch = ExAllocatePoolWithTag( 
						NonPagedPool, 
                        sizeof(FAST_IO_DISPATCH),
						LFS_ALLOC_TAG
						);
	
	if (fastIoDispatch == NULL)
	{
		ASSERT(LFS_INSUFFICIENT_RESOURCES);
		return FALSE;
	}

    RtlZeroMemory(
		fastIoDispatch, 
		sizeof(FAST_IO_DISPATCH)
		);

    fastIoDispatch->SizeOfFastIoDispatch	= sizeof(FAST_IO_DISPATCH);

    fastIoDispatch->FastIoCheckIfPossible		= LfsFastIoCheckIfPossible;
    fastIoDispatch->FastIoRead					= LfsFastIoRead;
    fastIoDispatch->FastIoWrite					= LfsFastIoWrite;
    fastIoDispatch->FastIoQueryBasicInfo		= LfsFastIoQueryBasicInfo;
    fastIoDispatch->FastIoQueryStandardInfo		= LfsFastIoQueryStandardInfo;
    fastIoDispatch->FastIoLock					= LfsFastIoLock;
    fastIoDispatch->FastIoUnlockSingle			= LfsFastIoUnlockSingle;
    fastIoDispatch->FastIoUnlockAll				= LfsFastIoUnlockAll;
    fastIoDispatch->FastIoUnlockAllByKey		= LfsFastIoUnlockAllByKey;
    fastIoDispatch->FastIoDeviceControl			= LfsFastIoDeviceControl;
    fastIoDispatch->FastIoDetachDevice			= LfsFastIoDetachDevice;
    fastIoDispatch->FastIoQueryNetworkOpenInfo	= LfsFastIoQueryNetworkOpenInfo;
    fastIoDispatch->MdlRead						= LfsFastIoMdlRead;
    fastIoDispatch->MdlReadComplete				= LfsFastIoMdlReadComplete;
    fastIoDispatch->PrepareMdlWrite				= LfsFastIoPrepareMdlWrite;
    fastIoDispatch->MdlWriteComplete			= LfsFastIoMdlWriteComplete;
    fastIoDispatch->FastIoReadCompressed		= LfsFastIoReadCompressed;
    fastIoDispatch->FastIoWriteCompressed		= LfsFastIoWriteCompressed;
    fastIoDispatch->MdlReadCompleteCompressed	= LfsFastIoMdlReadCompleteCompressed;
    fastIoDispatch->MdlWriteCompleteCompressed	= LfsFastIoMdlWriteCompleteCompressed;
    fastIoDispatch->FastIoQueryOpen				= LfsFastIoQueryOpen;

	LfsFastIoDispatch = fastIoDispatch;

	return TRUE;
}


VOID
CloseFastIoDispatch(
	VOID
	)
{
	if(LfsFastIoDispatch)
		ExFreePoolWithTag(
			LfsFastIoDispatch,
			LFS_ALLOC_TAG
			);

	LfsFastIoDispatch = NULL;
	
	return;
}


NTSTATUS
LfsPreAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
	    PLFS_FCB fcb = (PLFS_FCB)fileObject->FsContext;
		
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE,
			("LfsPreAcquireForSectionSynchronization Called\n"));

		if(fcb->Header.PagingIoResource != NULL)
		{
			ASSERT(LFS_REQUIRED);
			return STATUS_NOT_IMPLEMENTED;
		}	
	}
	
    *CompletionContext = NULL ;
	return STATUS_SUCCESS ;
}


VOID
LfsPostAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);

	return;
}


NTSTATUS
LfsPreReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);

    *CompletionContext = NULL ;
	return STATUS_SUCCESS ;
}


VOID
LfsPostReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);

	return;
}


NTSTATUS
LfsPreAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
	    PLFS_FCB fcb = (PLFS_FCB)fileObject->FsContext;
		
		if(fcb->Header.PagingIoResource != NULL)
		{
			SPY_LOG_PRINT(LFS_DEBUG_LFS_INFO,
				("LfsPreAcquireForCcFlush Called\n"));

			ASSERT(LFS_REQUIRED);
			return STATUS_NOT_IMPLEMENTED;
		}	
	}
	
    *CompletionContext = NULL ;
	return STATUS_SUCCESS ;
}


VOID
LfsPostAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);

	return;
}


NTSTATUS
LfsPreReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);

    *CompletionContext = NULL ;
	return STATUS_SUCCESS ;
}


VOID
LfsPostReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);

	return;
}


NTSTATUS
LfsPreAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE,
			("LfsPreAcquireForModifiedPageWriter Called\n"));
	
		//return STATUS_NOT_IMPLEMENTED ;
	}

    *CompletionContext = NULL ;
	return STATUS_SUCCESS ;
}


VOID
LfsPostAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;


	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);
	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE,
			("LfsPostAcquireForModifiedPageWriter Called\n"));
	
		return;
	}

	return;
}


NTSTATUS
LfsPreReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE,
			("LfsPreAcquireForModifiedPageWriter Called\n"));
	
		//return STATUS_NOT_IMPLEMENTED ;
	}

    *CompletionContext = NULL;

	return STATUS_SUCCESS ;
}


VOID
LfsPostReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
	UNREFERENCED_PARAMETER(OperationStatus);
	UNREFERENCED_PARAMETER(CompletionContext);

    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT(IS_FILESPY_DEVICE_OBJECT(deviceObject));

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT(fileSpyExt);
	ASSERT(lfsDeviceExt);

	//
	//	we do this only for Secondary volume.
	//

	if(lfsDeviceExt 
		&& lfsDeviceExt->FilteringMode  == LFS_SECONDARY 
		&& lfsDeviceExt->Secondary 
		&& Secondary_LookUpFileExtension(lfsDeviceExt->Secondary, fileObject))
	{
		SPY_LOG_PRINT(LFS_DEBUG_LFS_TRACE,
			("LfsPostAcquireForModifiedPageWriter Called\n"));
	
		return;
	}

	return;
}
