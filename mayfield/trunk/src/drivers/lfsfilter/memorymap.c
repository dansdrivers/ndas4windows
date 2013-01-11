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
    if (Irp->MdlAddress == NULL) 
	{
        return Irp->UserBuffer;
    
    } else 
	{
        PVOID address = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority );

        if (address == NULL) 
		{
            ExRaiseStatus( STATUS_INSUFFICIENT_RESOURCES );
        }

        return address;
    }
}


PVOID
MapInputBuffer(
	PIRP Irp
 )
{
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	PVOID				inputBuffer;
	ULONG				inputBufferLength;
	BOOLEAN				probe = FALSE;
	

	switch(irpSp->MajorFunction)
	{
	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL:
	{
		struct DeviceIoControl	*deviceIoControl = (struct DeviceIoControl *)&(irpSp->Parameters.DeviceIoControl) ;
		UINT32					bufferMethod = deviceIoControl->IoControlCode & 0x3;

	
		if(deviceIoControl->InputBufferLength == 0)
			return NULL;
	
		if(bufferMethod == METHOD_BUFFERED)
		{
			inputBuffer = Irp->AssociatedIrp.SystemBuffer;
		}
		else if(bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT)
		{
			inputBuffer = MmGetSystemAddressForMdlSafe( Irp->MdlAddress, NormalPagePriority ) ;
		}

		else if(bufferMethod == METHOD_NEITHER)
		{
			inputBuffer = deviceIoControl->Type3InputBuffer;
			inputBufferLength = deviceIoControl->InputBufferLength;			
		}
		else
		{
			ASSERT(LFS_UNEXPECTED);
			inputBuffer = NULL;
		}

		break;
	}

    case IRP_MJ_SET_INFORMATION:
	case IRP_MJ_WRITE:
	case IRP_MJ_SET_VOLUME_INFORMATION:
	case IRP_MJ_SET_EA:
	case IRP_MJ_SET_QUOTA:
	{
		if(irpSp->MajorFunction == IRP_MJ_SET_INFORMATION)
			inputBufferLength = irpSp->Parameters.SetFile.Length;
		if(irpSp->MajorFunction == IRP_MJ_WRITE)
			inputBufferLength = irpSp->Parameters.Write.Length;
		if(irpSp->MajorFunction == IRP_MJ_SET_VOLUME_INFORMATION)
			inputBufferLength = irpSp->Parameters.SetVolume.Length ;
		if(irpSp->MajorFunction == IRP_MJ_SET_EA)
			inputBufferLength = irpSp->Parameters.SetEa.Length ;

		if(inputBufferLength == 0)
			return NULL;

		if((Irp->Flags & IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) 
		{
			ASSERT(!(Irp->Flags & IRP_ASSOCIATED_IRP));
	
			//
			//	IRPs with no MdlAddress can arrive from Redirector
			//
			ASSERT(Irp->MdlAddress == NULL);
	
			inputBuffer = Irp->AssociatedIrp.SystemBuffer;

			break;
		}

		if(Irp->MdlAddress)
		{
			inputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			ASSERT(inputBuffer);
			
			break;
		}
		
		inputBuffer = MapUserBuffer(Irp);

		//
		//	To support filemon.
		// filemon build IRP that uses system buffer without IRP_ASSOCIATED_IRP.
		//
		if(NULL == inputBuffer && Irp->AssociatedIrp.SystemBuffer) {
			inputBuffer = Irp->AssociatedIrp.SystemBuffer ;
			break ;
		}

		
		probe		= TRUE;

		break;
	}
			
	default:
		
		ASSERT(LFS_BUG);
		inputBuffer = NULL;

		break;
	}

	
	if(probe && inputBufferLength)
	{
		try 
		{
			if (Irp->RequestorMode != KernelMode) 
			{
				ProbeForRead( 
					inputBuffer,
					inputBufferLength,
				    sizeof(UCHAR) 
					);
			}

		} except( Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH ) 
		{
			ULONG	status;

			inputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus(
					FsRtlIsNtstatusExpected(status) ?
					status : STATUS_INVALID_USER_BUFFER 
					);
		}
/*		try 
		{
			if (Irp->RequestorMode != KernelMode) 
			{
	            ProbeForRead(
					inputBuffer, 
					inputBufferLength, 
					sizeof(UCHAR) 
					);
			}

		} except( Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH ) 
		{
			ULONG	status;

			inputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus(
					FsRtlIsNtstatusExpected(status) ?
					status : STATUS_INVALID_USER_BUFFER 
					);
		} */
	}
	
	return inputBuffer;
}


PVOID
MapOutputBuffer(
	PIRP Irp
 )
{
	PIO_STACK_LOCATION	irpSp = IoGetCurrentIrpStackLocation(Irp);
	PVOID				outputBuffer;
	ULONG				outputBufferLength;
	BOOLEAN				probe = FALSE;


	switch(irpSp->MajorFunction)
	{
	case IRP_MJ_INTERNAL_DEVICE_CONTROL: 
	case IRP_MJ_DEVICE_CONTROL: 
	case IRP_MJ_FILE_SYSTEM_CONTROL:
	{
		struct DeviceIoControl	*deviceIoControl = (struct DeviceIoControl *)&(irpSp->Parameters.DeviceIoControl) ;
		UINT32					bufferMethod = deviceIoControl->IoControlCode & 0x3;;

	
		if(deviceIoControl->OutputBufferLength == 0)
			return NULL;
	
		if(bufferMethod == METHOD_BUFFERED)
		{
			outputBuffer = Irp->AssociatedIrp.SystemBuffer;
		}
		else if(bufferMethod == METHOD_OUT_DIRECT || bufferMethod == METHOD_IN_DIRECT)
		{
			outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority) ;
		}
		else if(bufferMethod == METHOD_NEITHER)
		{
			outputBuffer		= MapUserBuffer(Irp);
			outputBufferLength	= deviceIoControl->OutputBufferLength;
			probe				= TRUE;
		} else
		{
			ASSERT(LFS_UNEXPECTED);
			outputBuffer = NULL;
		}

		break;
	}

	case IRP_MJ_DIRECTORY_CONTROL:
	case IRP_MJ_QUERY_INFORMATION:
	case IRP_MJ_QUERY_VOLUME_INFORMATION: 
	case IRP_MJ_READ:
	case IRP_MJ_QUERY_SECURITY: //0x14
	case IRP_MJ_QUERY_EA:
	case IRP_MJ_QUERY_QUOTA:
	{
		if(irpSp->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)
			outputBufferLength = irpSp->Parameters.QueryDirectory.Length;
		else if(irpSp->MajorFunction == IRP_MJ_QUERY_INFORMATION)
			outputBufferLength = irpSp->Parameters.QueryFile.Length;
		else if(irpSp->MajorFunction == IRP_MJ_QUERY_VOLUME_INFORMATION)
			outputBufferLength = irpSp->Parameters.QueryVolume.Length;
		else if(irpSp->MajorFunction == IRP_MJ_READ)
			outputBufferLength = irpSp->Parameters.Read.Length;
		else if(irpSp->MajorFunction == IRP_MJ_QUERY_SECURITY)
			outputBufferLength = irpSp->Parameters.QuerySecurity.Length;
		else if(irpSp->MajorFunction == IRP_MJ_QUERY_EA)
			outputBufferLength = irpSp->Parameters.QueryEa.Length;
		else if(irpSp->MajorFunction == IRP_MJ_QUERY_QUOTA)
			outputBufferLength = irpSp->Parameters.QueryQuota.Length;
		else
			outputBufferLength = 0;

		if(outputBufferLength == 0)
			return NULL;

		if((Irp->Flags & IRP_BUFFERED_IO) && Irp->AssociatedIrp.SystemBuffer) 
		{
			ASSERT(!(Irp->Flags & IRP_ASSOCIATED_IRP));
			//
			//	IRPs with no MdlAddress can arrive from Redirector
			//
			ASSERT(Irp->MdlAddress == NULL);

			outputBuffer = Irp->AssociatedIrp.SystemBuffer;

			break;
		}

		if(Irp->MdlAddress)
		{
			outputBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
			ASSERT(outputBuffer);
			
			break;
		}

		outputBuffer = MapUserBuffer(Irp);

		//
		//	To support filemon.
		// filemon build IRP that uses system buffer without IRP_ASSOCIATED_IRP.
		//
		if(NULL == outputBuffer && Irp->AssociatedIrp.SystemBuffer) {
			outputBuffer = Irp->AssociatedIrp.SystemBuffer ;
			break ;
		}
		
		probe		= TRUE;

		break;
	}
			
	default:
		
		ASSERT(LFS_BUG);
		outputBuffer = NULL;

		break;
	}

	if(probe && outputBufferLength)
	{
		try 
		{
			if (Irp->RequestorMode != KernelMode) 
			{
	            ProbeForWrite(
					outputBuffer, 
					outputBufferLength, 
					sizeof(UCHAR) 
					);
			}

		} except( Irp->RequestorMode != KernelMode ? EXCEPTION_EXECUTE_HANDLER: EXCEPTION_CONTINUE_SEARCH ) 
		{
			ULONG	status;

			outputBuffer = NULL;
			status = GetExceptionCode();

			FatRaiseStatus(
					FsRtlIsNtstatusExpected(status) ?
					status : STATUS_INVALID_USER_BUFFER 
					);
		}
	}


	return outputBuffer;
}

		
