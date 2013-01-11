#ifndef __XIXFS_EXPORT_API_H__
#define __XIXFS_EXPORT_API_H__


NTSTATUS
Xixfs_GenerateUuid(OUT void * uuid);

/*
 *	Dispatch routine
 */
NTSTATUS
XixFsDispatch (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP Irp
);


/*
 *	Fast Io routine
 */




BOOLEAN 
XixFsdFastIoCheckIfPossible(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	IN BOOLEAN					CheckForReadOperation,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);


BOOLEAN 
XixFsdFastIoRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN BOOLEAN					Wait,
	IN ULONG					LockKey,
	OUT PVOID					Buffer,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
XixFsdFastIoWrite(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER				FileOffset,
	IN ULONG						Length,
	IN BOOLEAN						Wait,
	IN ULONG						LockKey,
	OUT PVOID						Buffer,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
XixFsdFastIoQueryBasicInfo(
	IN PFILE_OBJECT					FileObject,
	IN BOOLEAN						Wait,
	OUT PFILE_BASIC_INFORMATION		Buffer,
	OUT PIO_STATUS_BLOCK 			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
XixFsdFastIoQueryStdInfo(
	IN PFILE_OBJECT						FileObject,
	IN BOOLEAN							Wait,
	OUT PFILE_STANDARD_INFORMATION 	Buffer,
	OUT PIO_STATUS_BLOCK 				IoStatus,
	IN PDEVICE_OBJECT					DeviceObject
);

BOOLEAN 
XixFsdFastIoLock(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN PLARGE_INTEGER			Length,
	PEPROCESS					ProcessId,
	ULONG						Key,
	BOOLEAN						FailImmediately,
	BOOLEAN						ExclusiveLock,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
XixFsdFastIoUnlockSingle(
	IN PFILE_OBJECT			FileObject,
	IN PLARGE_INTEGER		FileOffset,
	IN PLARGE_INTEGER		Length,
	PEPROCESS				ProcessId,
	ULONG					Key,
	OUT PIO_STATUS_BLOCK	IoStatus,
	IN PDEVICE_OBJECT		DeviceObject
);

BOOLEAN 
XixFsdFastIoUnlockAll(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
XixFsdFastIoUnlockAllByKey(
	IN PFILE_OBJECT				FileObject,
	PEPROCESS						ProcessId,
	ULONG								Key,
	OUT PIO_STATUS_BLOCK			IoStatus,
	IN PDEVICE_OBJECT				DeviceObject
);

void 
XixFsdFastIoAcqCreateSec(
	IN PFILE_OBJECT			FileObject
);

void 
XixFsdFastIoRelCreateSec(
	IN PFILE_OBJECT			FileObject
);

BOOLEAN 
XixFsdFastIoQueryNetInfo(
	IN PFILE_OBJECT								FileObject,
	IN BOOLEAN									Wait,
	OUT PFILE_NETWORK_OPEN_INFORMATION 			Buffer,
	OUT PIO_STATUS_BLOCK 						IoStatus,
	IN PDEVICE_OBJECT							DeviceObject
);

NTSTATUS 
XixFsdFastIoAcqModWrite(
	IN PFILE_OBJECT					FileObject,
	IN PLARGE_INTEGER					EndingOffset,
	OUT PERESOURCE						*ResourceToRelease,
	IN PDEVICE_OBJECT					DeviceObject
);

NTSTATUS 
XixFsdFastIoRelModWrite(
	IN PFILE_OBJECT				FileObject,
	IN PERESOURCE					ResourceToRelease,
	IN PDEVICE_OBJECT				DeviceObject
);

NTSTATUS 
XixFsdFastIoAcqCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
);

NTSTATUS 
XixFsdFastIoRelCcFlush(
	IN PFILE_OBJECT			FileObject,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
XixFsdFastIoMdlRead(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
XixFsdFastIoMdlReadComplete(
	IN PFILE_OBJECT				FileObject,
	OUT PMDL							MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN 
XixFsdFastIoPrepareMdlWrite(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER			FileOffset,
	IN ULONG					Length,
	IN ULONG					LockKey,
	OUT PMDL					*MdlChain,
	OUT PIO_STATUS_BLOCK		IoStatus,
	IN PDEVICE_OBJECT			DeviceObject
);

BOOLEAN 
XixFsdFastIoMdlWriteComplete(
	IN PFILE_OBJECT				FileObject,
	IN PLARGE_INTEGER				FileOffset,
	OUT PMDL							MdlChain,
	IN PDEVICE_OBJECT				DeviceObject
);

BOOLEAN
XixFsdAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
XixFsdRelLazyWrite(
	IN PVOID Context
);

BOOLEAN
XixFsdAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
XixFsdRelReadAhead(
	IN PVOID Context
);


BOOLEAN
XixFsdNopAcqLazyWrite(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
XixFsdNopRelLazyWrite(
	IN PVOID Context
);

BOOLEAN
XixFsdNopAcqReadAhead(
	IN PVOID	Context,
	IN BOOLEAN	Wait
);

VOID
XixFsdNopRelReadAhead(
	IN PVOID Context
);
#endif //#ifndef __XIXFS_EXPORT_API_H__