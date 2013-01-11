#include "LfsProc.h"

#if DBG


PCHAR	IrpMajors[IRP_MJ_MAXIMUM_FUNCTION + 1] = {

	"IRP_MJ_CREATE",
	"IRP_MJ_CREATE_NAMED_PIPE",
	"IRP_MJ_CLOSE",
	"IRP_MJ_READ",
	"IRP_MJ_WRITE",
	"IRP_MJ_QUERY_INFORMATION",
	"IRP_MJ_SET_INFORMATION",
	"IRP_MJ_QUERY_EA",
	"IRP_MJ_SET_EA",
	"IRP_MJ_FLUSH_BUFFERS",
	"IRP_MJ_QUERY_VOLUME_INFORMATION",
	"IRP_MJ_SET_VOLUME_INFORMATION",
	"IRP_MJ_DIRECTORY_CONTROL",
	"IRP_MJ_FILE_SYSTEM_CONTROL",
	"IRP_MJ_DEVICE_CONTROL",
	"IRP_MJ_INTERNAL_DEVICE_CONTROL",
	"IRP_MJ_SHUTDOWN",
	"IRP_MJ_LOCK_CONTROL",
	"IRP_MJ_CLEANUP",
	"IRP_MJ_CREATE_MAILSLOT",
	"IRP_MJ_QUERY_SECURITY",
	"IRP_MJ_SET_SECURITY",
	"IRP_MJ_POWER",
	"IRP_MJ_SYSTEM_CONTROL",
	"IRP_MJ_DEVICE_CHANGE",
	"IRP_MJ_QUERY_QUOTA",
	"IRP_MJ_SET_QUOTA",
	"IRP_MJ_PNP"
};


PCHAR AttributeTypeCode[] = {

    { "$UNUSED                " },   //  (0X0)
    { "$STANDARD_INFORMATION  " },   //  (0x10)
    { "$ATTRIBUTE_LIST        " },   //  (0x20)
    { "$FILE_NAME             " },   //  (0x30)
    { "$OBJECT_ID             " },   //  (0x40)
    { "$SECURITY_DESCRIPTOR   " },   //  (0x50)
    { "$VOLUME_NAME           " },   //  (0x60)
    { "$VOLUME_INFORMATION    " },   //  (0x70)
    { "$DATA                  " },   //  (0x80)
    { "$INDEX_ROOT            " },   //  (0x90)
    { "$INDEX_ALLOCATION      " },   //  (0xA0)
    { "$BITMAP                " },   //  (0xB0)
    { "$REPARSE_POINT         " },   //  (0xC0)
    { "$EA_INFORMATION        " },   //  (0xD0)
    { "$EA                    " },   //  (0xE0)
    { "   INVALID TYPE CODE   " },   //  (0xF0)
    { "$LOGGED_UTILITY_STREAM " }    //  (0x100)
};


VOID
PrintIrp (
	ULONG					DebugLevel,
	PCHAR					Where,
	PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	PIRP					Irp
	)
{
	PIO_STACK_LOCATION  irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT		fileObject = irpSp->FileObject;
	UCHAR				minorFunction;
    CHAR				irpMajorString[OPERATION_NAME_BUFFER_SIZE];
    CHAR				irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName ( irpSp->MajorFunction,
				 irpSp->MinorFunction,
				 irpSp->Parameters.FileSystemControl.FsControlCode,
				 irpMajorString,
				 OPERATION_NAME_BUFFER_SIZE,
				 irpMinorString,
				 OPERATION_NAME_BUFFER_SIZE );
	
	if (irpSp->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && irpSp->MinorFunction == IRP_MN_USER_FS_REQUEST) {

		minorFunction = (UCHAR)((irpSp->Parameters.FileSystemControl.FsControlCode & 0x00003FFC) >> 2);
	
	} else if (irpSp->MajorFunction == IRP_MJ_DEVICE_CONTROL && irpSp->MinorFunction == 0) {

		minorFunction = (UCHAR)((irpSp->Parameters.DeviceIoControl.IoControlCode & 0x00003FFC) >> 2);
	
	} else {

		minorFunction = irpSp->MinorFunction;
	}

	SPY_LOG_PRINT( DebugLevel,
				   ("%s %p %d %s Irp:%p %s %s (%u:%u) %08x %02x\n"
					"file: %p %p %08x %wZ %d\n",
				    Where,
				    LfsDeviceExt, 
					KeGetCurrentIrql(), 
				    (Irp->RequestorMode == KernelMode) ? "KernelMode" : "UserMode",
				    Irp, irpMajorString, irpMinorString, 
					irpSp->MajorFunction, minorFunction,
				    Irp->Flags, irpSp->Flags,
					fileObject, 
					fileObject ? fileObject->RelatedFileObject : NULL,
					fileObject ? fileObject->Flags : 0,
					fileObject ? &fileObject->FileName : NULL,
					fileObject ? fileObject->FileName.Length : 0) );
	return;
}

#if __NDAS_FS_MINI__

VOID
PrintData (
	IN ULONG					DebugLevel,
	IN PCHAR					Where,
	IN PLFS_DEVICE_EXTENSION	LfsDeviceExt,
	IN PFLT_CALLBACK_DATA		Data
	)
{
	//PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	PFLT_IO_PARAMETER_BLOCK iopb = Data->Iopb;
	PFILE_OBJECT			fileObject = iopb->TargetFileObject;
	UCHAR					minorFunction;
    CHAR					irpMajorString[OPERATION_NAME_BUFFER_SIZE];
    CHAR					irpMinorString[OPERATION_NAME_BUFFER_SIZE];

	GetIrpName ( iopb->MajorFunction,
				 iopb->MinorFunction,
				 iopb->Parameters.FileSystemControl.Common.FsControlCode,
				 irpMajorString,
				 OPERATION_NAME_BUFFER_SIZE,
				 irpMinorString,
				 OPERATION_NAME_BUFFER_SIZE );
	
	if (iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL && iopb->MinorFunction == IRP_MN_USER_FS_REQUEST) {

		minorFunction = (UCHAR)((iopb->Parameters.FileSystemControl.Common.FsControlCode & 0x00003FFC) >> 2);
	
	} else if (iopb->MajorFunction == IRP_MJ_DEVICE_CONTROL && iopb->MinorFunction == 0) {

		minorFunction = (UCHAR)((iopb->Parameters.DeviceIoControl.Common.IoControlCode & 0x00003FFC) >> 2);
	
	} else {

		minorFunction = iopb->MinorFunction;
	}

	SPY_LOG_PRINT( DebugLevel,
				   ("%s %p %d %s Irp:%p %s %s (%u:%u) %08x %02x\n"
					"file: %p %p %08x %wZ %d\n",
				    Where,
				    LfsDeviceExt, 
					KeGetCurrentIrql(), 
				    (Data->RequestorMode == KernelMode) ? "KernelMode" : "UserMode",
				    Data, irpMajorString, irpMinorString, 
					iopb->MajorFunction, minorFunction,
				    iopb->IrpFlags, iopb->OperationFlags,
					fileObject, 
					fileObject ? fileObject->RelatedFileObject : NULL,
					fileObject ? fileObject->Flags : 0,
					fileObject ? &fileObject->FileName : NULL,
					fileObject ? fileObject->FileName.Length : 0) );
	return;
}

#endif

#endif