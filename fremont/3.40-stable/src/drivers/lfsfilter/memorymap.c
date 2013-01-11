#include "LfsProc.h"

#if DBG

BOOLEAN FatTestRaisedStatus = TRUE;

#endif

#if DBG
#define DebugBreakOnStatus(S) {                                                      \
    if (FatTestRaisedStatus) {                                                       \
        if ((S) == STATUS_DISK_CORRUPT_ERROR || (S) == STATUS_FILE_CORRUPT_ERROR) {  \
            DbgPrint( "FAT: Breaking on interesting raised status (%08x)\n", (S) );  \
            DbgPrint( "FAT: Set FatTestRaisedStatus @ %p to 0 to disable\n",		 \
                      &FatTestRaisedStatus );                                        \
            DbgBreakPoint();                                                         \
        }                                                                            \
    }                                                                                \
}
#else
#define DebugBreakOnStatus(S)
#endif

#define FatRaiseStatus(STATUS) {             \
    DebugBreakOnStatus( (STATUS) )           \
    ExRaiseStatus( (STATUS) );               \
}


PVOID
MapUserBuffer (
    IN PIRP Irp
    )
{
    if (Irp->MdlAddress == NULL) {

		NDAS_ASSERT(Irp->UserBuffer);
        return Irp->UserBuffer;
    
    } else {

        PVOID address = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

        NDAS_ASSERT(address);
        return address;
    }
}

PVOID
MapInputBuffer (
	PIRP Irp
 	)
{
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	PVOID				inputBuffer;
	ULONG				inputBufferLength;
	BOOLEAN				probe = FALSE;
	

	switch (irpSp->MajorFunction) {

	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		struct DeviceIoControl	*deviceIoControl = (struct DeviceIoControl *)&(irpSp->Parameters.DeviceIoControl) ;
		UINT32					bufferMethod = deviceIoControl->IoControlCode & 0x3;

		if (deviceIoControl->InputBufferLength == 0) {

			return NULL;
		}

		if (bufferMethod == METHOD_BUFFERED) {

			inputBuffer = Irp->AssociatedIrp.SystemBuffer;
		
		} else if (bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT) {

			inputBuffer = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );
		
		} else if(bufferMethod == METHOD_NEITHER) {

			inputBuffer = deviceIoControl->Type3InputBuffer;
			inputBufferLength = deviceIoControl->InputBufferLength;			
		
		} else {

			NDAS_ASSERT(FALSE);
			inputBuffer = NULL;
		}

		break;
	}

	default:
		
		NDAS_ASSERT(FALSE);

		inputBuffer = NULL;

		break;
	}

	
	if (probe && inputBufferLength) {

		try {

			if (Irp->RequestorMode != KernelMode) {

				ProbeForRead( inputBuffer, inputBufferLength, sizeof(UCHAR) );
			}

		} except (Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {


			ULONG	status;

			inputBuffer = NULL;
			status = GetExceptionCode();

			NDAS_ASSERT(FALSE);

			FatRaiseStatus( FsRtlIsNtstatusExpected(status) ? status : STATUS_INVALID_USER_BUFFER );
		}
	}

	return inputBuffer;
}


PVOID
MapOutputBuffer (
	PIRP Irp
 )
{
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	PVOID				outputBuffer;
	ULONG				outputBufferLength;
	BOOLEAN				probe = FALSE;


	outputBufferLength = 0;

	switch(irpSp->MajorFunction) {

	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		struct DeviceIoControl	*deviceIoControl = (struct DeviceIoControl *)&(irpSp->Parameters.DeviceIoControl) ;
		UINT32					bufferMethod = deviceIoControl->IoControlCode & 0x3;;
	
		if (deviceIoControl->OutputBufferLength == 0) {

			return NULL;
		}

		if (bufferMethod == METHOD_BUFFERED) {

			outputBuffer = Irp->AssociatedIrp.SystemBuffer;
		
		} else if (bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT) {

			outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
		
		} else if (bufferMethod == METHOD_NEITHER) {

			outputBuffer		= MapUserBuffer(Irp);
			outputBufferLength	= deviceIoControl->OutputBufferLength;
			probe				= TRUE;
		
		} else {

			NDAS_ASSERT(FALSE);
			outputBuffer = NULL;
		}

		break;
	}
			
	default:
		
		NDAS_ASSERT(FALSE);
		outputBuffer = NULL;

		break;
	}

	if (probe && outputBufferLength) {

		try {

			if (Irp->RequestorMode != KernelMode) {

	            ProbeForWrite( outputBuffer, outputBufferLength, sizeof(UCHAR) );
			}

		} except(Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

			ULONG	status;

			outputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus( FsRtlIsNtstatusExpected(status) ? status : STATUS_INVALID_USER_BUFFER );
		}
	}

	return outputBuffer;
}

#if __NDAS_FS_MINI__
		
PVOID
MinispyMapInputBuffer (
	IN  PFLT_IO_PARAMETER_BLOCK	Iopb,
	IN  KPROCESSOR_MODE			RequestorMode
	)
{
	PVOID	inputBuffer;
	ULONG	inputBufferLength;
	BOOLEAN	probe = FALSE;
	

	switch(Iopb->MajorFunction) {

	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		UINT32	bufferMethod = Iopb->Parameters.DeviceIoControl.Common.IoControlCode & 0x3;

	
		if (Iopb->Parameters.DeviceIoControl.Common.InputBufferLength == 0)
			return NULL;
	
		if (bufferMethod == METHOD_BUFFERED) {

			inputBuffer = Iopb->Parameters.DeviceIoControl.Buffered.SystemBuffer;
			inputBufferLength = Iopb->Parameters.DeviceIoControl.Buffered.InputBufferLength;
		
		} else if (bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT) {

			inputBuffer = Iopb->Parameters.DeviceIoControl.Direct.InputSystemBuffer;
			inputBufferLength = Iopb->Parameters.DeviceIoControl.Direct.InputBufferLength;
		
		} else if (bufferMethod == METHOD_NEITHER) {

			inputBuffer = Iopb->Parameters.DeviceIoControl.Neither.InputBuffer;
			inputBufferLength = Iopb->Parameters.DeviceIoControl.Neither.InputBufferLength;
		
		} else {

			ASSERT( LFS_UNEXPECTED );
			inputBuffer = NULL;
		}

		break;
	}
			
	default:
		
		ASSERT( LFS_BUG );
		inputBuffer = NULL;

		break;
	}

	
	if (probe && inputBufferLength) {

		try {

			if (RequestorMode != KernelMode) {

				ProbeForRead( inputBuffer, inputBufferLength, sizeof(UCHAR) );
			}

		} except (RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {

			ULONG	status;

			inputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus( FsRtlIsNtstatusExpected(status) ? status : STATUS_INVALID_USER_BUFFER );
		}
	}
	
	return inputBuffer;
}


PVOID
MinispyMapOutputBuffer (
	IN  PFLT_IO_PARAMETER_BLOCK	Iopb,
	IN  KPROCESSOR_MODE			RequestorMode
	)
{
	PVOID				outputBuffer;
	ULONG				outputBufferLength;
	BOOLEAN				probe = FALSE;


	switch (Iopb->MajorFunction) {

	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL: {

		UINT32	bufferMethod = Iopb->Parameters.DeviceIoControl.Common.IoControlCode & 0x3;
	
		if (Iopb->Parameters.DeviceIoControl.Common.OutputBufferLength == 0)
			return NULL;
	
		if (bufferMethod == METHOD_BUFFERED) {

			outputBuffer = Iopb->Parameters.DeviceIoControl.Buffered.SystemBuffer;
			outputBufferLength = Iopb->Parameters.DeviceIoControl.Buffered.OutputBufferLength;

		} else if (bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT) {

			if (Iopb->Parameters.DeviceIoControl.Neither.OutputBuffer) {

				outputBuffer = Iopb->Parameters.DeviceIoControl.Direct.OutputBuffer;

			} else if (Iopb->Parameters.DeviceIoControl.Direct.OutputMdlAddress) {

				outputBuffer = MmGetSystemAddressForMdlSafe(Iopb->Parameters.DeviceIoControl.Direct.OutputMdlAddress, NormalPagePriority);
			}

			outputBufferLength = Iopb->Parameters.DeviceIoControl.Direct.OutputBufferLength;

		} else if (bufferMethod == METHOD_NEITHER) {

			if (Iopb->Parameters.DeviceIoControl.Neither.OutputBuffer) {

				outputBuffer = Iopb->Parameters.DeviceIoControl.Neither.OutputBuffer;
			
			} else if (Iopb->Parameters.DeviceIoControl.Direct.OutputMdlAddress) {

				outputBuffer = MmGetSystemAddressForMdlSafe(Iopb->Parameters.DeviceIoControl.Neither.OutputMdlAddress, NormalPagePriority);
			}

			outputBufferLength = Iopb->Parameters.DeviceIoControl.Neither.OutputBufferLength;

		} else {

			ASSERT( LFS_UNEXPECTED );
			outputBuffer = NULL;
		}

		break;
	}

	default:
		
		ASSERT( LFS_BUG );
		outputBuffer = NULL;

		break;
	}

	if (probe && outputBufferLength) {

		try {

			if (RequestorMode != KernelMode) {

	            ProbeForWrite( outputBuffer, outputBufferLength, sizeof(UCHAR) );
			}

		} except (RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH) {
			
			ULONG	status;

			outputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus( FsRtlIsNtstatusExpected(status) ? status : STATUS_INVALID_USER_BUFFER );
		}
	}

	return outputBuffer;
}

#endif