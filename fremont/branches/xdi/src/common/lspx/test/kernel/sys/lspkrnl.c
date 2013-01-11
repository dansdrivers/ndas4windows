/*++

Module Name:

    lspkrnl.c

Abstract:

	Demonstrates the use of new Cancel-Safe queue
	APIs to perform queuing of IRPs without worrying about
	any synchronization issues between cancel lock in the I/O
	manager and the driver's queue lock.

	This driver is written for an hypothetical data acquisition
	device that requires polling at a regular interval.
	The device has some settling period between two reads. 
	Upon user request the driver reads data and records the time. 
	When the next read request comes in, it checks the interval 
	to see if it's reading the device too soon. If so, it pends
	the IRP and sleeps for while and tries again.

	Upon arrival, IRPs are queued in a cancel-safe queue and a 
	semaphore is signaled. A polling thread indefinitely waits on the
	semaphore to process queued IRPs sequentially.

	This sample is adapted from the original cancel 
	sample (KB Q188276) available in MSDN.

Author:

Environment:

    Kernel mode

Revision History:

    Changed the entire sample after WinXP RC1 because a nasty race condition 
    was discovered by Troy Shaw. The sample has to be rewritten because the  
    mechanism used to serialize the IRP didn't fit well with the IoCsq interface. 
    (July 13, 2001) - 

    Updated to use IoCreateDeviceSecure function - May 3, 2002

--*/

#include "lspkrnl.h"
#include "lsptransfer.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, LspDispatchCreate)
#pragma alloc_text( PAGE, LspDispatchClose)
#pragma alloc_text( PAGE, LspDispatchRead)
#pragma alloc_text( PAGE, LspUnload)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    DriverObject - pointer to the driver object

    registryPath - pointer to a unicode string representing the path,
                   to driver-specific key in the registry.

Return Value:

    STATUS_SUCCESS if successful,
    STATUS_UNSUCCESSFUL otherwise

--*/
{
    NTSTATUS            status = STATUS_SUCCESS;
	NTSTATUS cleanupStatus;
    UNICODE_STRING      unicodeDeviceName;   
    UNICODE_STRING      unicodeDosDeviceName;  
    PDEVICE_OBJECT      deviceObject;
    PDEVICE_EXTENSION   deviceExtension;
    HANDLE              threadHandle;
    UNICODE_STRING sddlString;
    ULONG devExtensionSize; 
    UNREFERENCED_PARAMETER (RegistryPath);

    LSP_KDPRINT(("DriverEntry Enter \n"));
    
   
    (void) RtlInitUnicodeString(&unicodeDeviceName, LSP_DEVICE_NAME_U);

    (void) RtlInitUnicodeString( &sddlString, L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");

	//
    // We will create a secure deviceobject so that only processes running
    // in admin and local system account can access the device. Refer
    // "Security Descriptor String Format" section in the platform
    // SDK documentation to understand the format of the sddl string.
    // We need to do because this is a legacy driver and there is no INF
    // involved in installing the driver. For PNP drivers, security descriptor
    // is typically specified for the FDO in the INF file.
    //

	/* includes LSP Session Buffer Size */
	devExtensionSize = sizeof(DEVICE_EXTENSION) + LSP_SESSION_BUFFER_SIZE - 1;

	status = IoCreateDevice(
		DriverObject,
		devExtensionSize,
		&unicodeDeviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&deviceObject);

    //status = IoCreateDeviceSecure(
    //            DriverObject,
    //            devExtensionSize,
    //            &unicodeDeviceName,
    //            FILE_DEVICE_UNKNOWN,
    //            0,
    //            (BOOLEAN) FALSE,
				//&SDDL_DEVOBJ_SYS_ALL_ADM_RWX_WORLD_R_RES_R, /* &sddlString, */
    //            (LPCGUID)&GUID_DEVCLASS_LSP, 
    //            &deviceObject);

    if (!NT_SUCCESS(status))
    {
        return status;
    }
    
    DbgPrint("DeviceObject %p\n", deviceObject);
    
    //
    // Allocate and initialize a Unicode String containing the Win32 name
    // for our device.
    //

    (void)RtlInitUnicodeString( &unicodeDosDeviceName, LSP_DOS_DEVICE_NAME_U );

	IoDeleteSymbolicLink(&unicodeDosDeviceName);

    status = IoCreateSymbolicLink(
                (PUNICODE_STRING) &unicodeDosDeviceName,
                (PUNICODE_STRING) &unicodeDeviceName
                );

    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceExtension = deviceObject->DeviceExtension;

	RtlZeroMemory(deviceExtension, sizeof(deviceExtension));
	KeInitializeSpinLock(&deviceExtension->LspLock);

#if 1

	{
		// 2.0
		TDI_ADDRESS_LPX deviceAddress = {0x1027, 0x00, 0x0d, 0x0b, 0x5d, 0x80, 0x03};
		// PALE
		TDI_ADDRESS_LPX localAddress =  {0x0000, 0x00, 0x03, 0xff, 0x5d, 0xac, 0xb8};

		RtlCopyMemory(
			&deviceExtension->DeviceAddress,
			&deviceAddress,
			sizeof(TDI_ADDRESS_LPX));

		RtlCopyMemory(
			&deviceExtension->LocalAddress,
			&localAddress,
			sizeof(TDI_ADDRESS_LPX));
	}

#endif

	deviceExtension->CloseWorkItem = IoAllocateWorkItem(deviceObject);
	if (NULL == deviceExtension->CloseWorkItem)
	{
		IoDeleteDevice(deviceObject);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

    DriverObject->MajorFunction[IRP_MJ_CREATE]= LspDispatchCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = LspDispatchClose;
    DriverObject->MajorFunction[IRP_MJ_READ] = LspDispatchRead;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = LspDispatchCleanup;

	DriverObject->DriverStartIo = LspStartIo;
    DriverObject->DriverUnload = LspUnload;

    // deviceObject->Flags |= DO_BUFFERED_IO;
	deviceObject->Flags |= DO_DIRECT_IO;

    //
    // This is used to serialize access to the queue.
    //

    KeInitializeSpinLock(&deviceExtension->QueueLock);

    KeInitializeSemaphore(&deviceExtension->IrpQueueSemaphore, 0, MAXLONG );

    //
    // Initialize the pending Irp device queue
    //

    InitializeListHead( &deviceExtension->PendingIrpQueue );

	KeInitializeEvent(
		&deviceExtension->LspCompletionEvent,
		NotificationEvent,
		FALSE);

    //
    // Initialize the cancel safe queue
    // 
    IoCsqInitialize(
		&deviceExtension->CancelSafeQueue,
        LspInsertIrp,
        LspRemoveIrp,
        LspPeekNextIrp,
        LspAcquireLock,
        LspReleaseLock,
        LspCompleteCanceledIrp );
    //
    // 10 is multiplied because system time is specified in 100ns units
    //

    deviceExtension->PollingInterval.QuadPart = Int32x32To64(
                                LSP_RETRY_INTERVAL, -10);
    //
    // Note down system time
    //

    KeQuerySystemTime (&deviceExtension->LastPollTime);

    //
    // Start the polling thread.
    //
    
    deviceExtension->ThreadShouldStop = FALSE;

    status = PsCreateSystemThread(&threadHandle,
                                (ACCESS_MASK)0,
                                NULL,
                                (HANDLE) 0,
                                NULL,
                                LspPollingThread,
                                deviceObject );

    if( !NT_SUCCESS( status ))
    {
        IoDeleteSymbolicLink( &unicodeDosDeviceName );
        IoDeleteDevice( deviceObject );
        return status;
    }

    //
    // Convert the Thread object handle into a pointer to the Thread object
    // itself. Then close the handle.
    //
    
    ObReferenceObjectByHandle(
		threadHandle,
        THREAD_ALL_ACCESS,
        NULL,
        KernelMode,
        &deviceExtension->ThreadObject,
        NULL );

    ZwClose(threadHandle);

	//
	// Finish initialization
	//
	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    LSP_KDPRINT(("DriverEntry Exit = %x\n", status));

    return status;
}

NTSTATUS
LspDispatchCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

   Process the Create and close IRPs sent to this device.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

      NT Status code

--*/
{
    PIO_STACK_LOCATION   irpStack;
    NTSTATUS             status = STATUS_SUCCESS;
	PDEVICE_EXTENSION deviceExtension;
	KIRQL oldIrql;

    PAGED_CODE ();

    LSP_KDPRINT(("LspDispatchCreate Enter\n"));

    //
    // Get a pointer to the current location in the Irp.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);
	ASSERT(IRP_MJ_CREATE == irpStack->MajorFunction);

	// 
    // The dispatch routine for IRP_MJ_CREATE is called when a 
    // file object associated with the device is created. 
    // This is typically because of a call to CreateFile() in 
    // a user-mode program or because a another driver is 
    // layering itself over a this driver. A driver is 
    // required to supply a dispatch routine for IRP_MJ_CREATE.
    //

	Irp->IoStatus.Information = 0;
	IoMarkIrpPending(Irp);
	IoStartPacket(DeviceObject, Irp, NULL, NULL);
	return STATUS_PENDING;

	//deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
	//
	//status = LspInitializeConnection(DeviceObject);
	//if (STATUS_PENDING == status)
	//{
	//	IoMarkIrpPending(Irp);
	//	status = STATUS_PENDING;
	//}
	//else
	//{
	//	Irp->IoStatus.Information = 0;
	//	Irp->IoStatus.Status = status;
	//	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	//}

 //   LSP_KDPRINT((" LspCreate Exit = %x\n", status));

    return status;
}

NTSTATUS
LspDispatchClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

   Process the close IRPs sent to this device.

Arguments:

   DeviceObject - pointer to a device object.

   Irp - pointer to an I/O Request Packet.

Return Value:

      NT Status code

--*/
{
    PIO_STACK_LOCATION   irpStack;
    NTSTATUS             status = STATUS_SUCCESS, cleanupStatus;

    PAGED_CODE ();

    LSP_KDPRINT(("LspDispatchClose Enter\n"));

    //
    // Get a pointer to the current location in the Irp.
    //

    irpStack = IoGetCurrentIrpStackLocation(Irp);

	ASSERT(IRP_MJ_CLOSE == irpStack->MajorFunction);

	LSP_KDPRINT(("IRP_MJ_CLOSE\n"));

	//
    // The IRP_MJ_CLOSE dispatch routine is called when a file object
    // opened on the driver is being removed from the system; that is,
    // all file object handles have been closed and the reference count
    // of the file object is down to 0. 
    //

	Irp->IoStatus.Information = 0;
	IoMarkIrpPending(Irp);
	IoStartPacket(DeviceObject, Irp, NULL, NULL);
	return STATUS_PENDING;


	////
 //   // Save Status for return and complete Irp
 //   //
 //   
 //   Irp->IoStatus.Status = status;
 //   IoCompleteRequest(Irp, IO_NO_INCREMENT);

 //   LSP_KDPRINT((" LspCreateClose Exit = %x\n", status));

    return status;
}

VOID
LspCreateWorkItem(
	__in PDEVICE_OBJECT DeviceObject, 
	__in PVOID Context)
{
	NTSTATUS status;
	PIRP irp;
	KIRQL oldIrql;

	irp = (PIRP) Context;

	status = LspInitializeConnection(DeviceObject);
	LSP_KDPRINT(("LspInitializeConnection returned status %x\n", status));

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);	

	//
	// IoStartNextPacket should be called at DISPATCH_LEVEL
	//
	KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
	IoStartNextPacket(DeviceObject, TRUE);
	KeLowerIrql(oldIrql);
	
}

VOID
LspCloseWorkItem(
	__in PDEVICE_OBJECT DeviceObject, 
	__in PVOID Context)
{
	NTSTATUS status;
	PIRP irp;
	KIRQL oldIrql;

	irp = (PIRP) Context;

	status = LspCleanupConnection(DeviceObject);
	LSP_KDPRINT(("LspClenaupConnection returned status %x\n", status));

	irp->IoStatus.Information = 0;
	irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(irp, IO_NO_INCREMENT);	

	//
	// IoStartNextPacket should be called at DISPATCH_LEVEL
	//
	KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
	IoStartNextPacket(DeviceObject, TRUE);
	KeLowerIrql(oldIrql);
}

VOID
LspStartIo(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	NTSTATUS status;
	lsp_status_t lspStatus;
	PIO_STACK_LOCATION irpStack;
	PDEVICE_EXTENSION deviceExtension;

	LSP_KDPRINT(("LspStartIo: Irp=%p\n", Irp));

	irpStack = IoGetCurrentIrpStackLocation(Irp);
	deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

	switch (irpStack->MajorFunction)
	{
	case IRP_MJ_CREATE:
		{
			deviceExtension->LspTransferCount = 0;
			deviceExtension->CurrentIrp = NULL;

			IoQueueWorkItem(
				deviceExtension->CloseWorkItem,
				LspCreateWorkItem,
				DelayedWorkQueue,
				Irp);
		}
		break;
	case IRP_MJ_CLOSE:
		{
			deviceExtension->LspTransferCount = 0;
			deviceExtension->CurrentIrp = NULL;

			IoQueueWorkItem(
				deviceExtension->CloseWorkItem, 
				LspCloseWorkItem, 
				DelayedWorkQueue, 
				Irp);
		}
		break;
	case IRP_MJ_READ:
		{
			LARGE_INTEGER location;
			PVOID dataBuffer;
			ULONG dataLength;
			KIRQL oldIrql;

			static ULONG random = 0;

			location.QuadPart = random++;

			dataLength = irpStack->Parameters.Read.Length;
			dataBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

			if (NULL == dataBuffer)
			{
				Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return;
			}

			if (0 != (dataLength % 512))
			{
				Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
				Irp->IoStatus.Information = 0;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				return;
			}

			// KeAcquireSpinLock(&deviceExtension->LspLock, &oldIrql);

			deviceExtension->LspTransferCount = 0;
			deviceExtension->CurrentIrp = Irp;

			deviceExtension->LspStatus = lsp_ide_read(
				deviceExtension->LspHandle,
				(lsp_large_integer_t*)&location,
				(lsp_int16_t)(dataLength >> 9),
				dataBuffer,
				dataLength);

			Irp->IoStatus.Information = dataLength;
			// KeReleaseSpinLock(&deviceExtension->LspLock, oldIrql);

			status = LspProcessNext(DeviceObject, &deviceExtension->LspStatus);
			if (STATUS_PENDING != status)
			{
				deviceExtension->CurrentIrp = NULL;
				Irp->IoStatus.Status = status;
				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				IoStartNextPacket(DeviceObject, TRUE);
				return;
			}
		}
		break;
	default:
		ASSERT(FALSE);
	}
}

NTSTATUS
LspDispatchRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
 )
 /*++
     Routine Description:
  
           Read disptach routine
           
     Arguments:
  
         DeviceObject - pointer to a device object.
                 Irp             - pointer to current Irp
  
     Return Value:
  
         NT status code.
  
--*/
{
    NTSTATUS            status;
    PDEVICE_EXTENSION   deviceExtension;
    PIO_STACK_LOCATION  irpStack;
    LARGE_INTEGER       currentTime;

    PAGED_CODE();

	LSP_KDPRINT(("LspRead Enter: Irp=%p\n", Irp));

    //
    // Get a pointer to the device extension.
    //

    deviceExtension = DeviceObject->DeviceExtension;
   
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    
    //
    // First make sure there is enough room.
    //

	//
	// read should be aligned to a sector size (512 bytes)
	//
	if (0 != (irpStack->Parameters.Read.Length % 512))
	{
		Irp->IoStatus.Status = status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}

    if (irpStack->Parameters.Read.Length < sizeof(INPUT_DATA))
    {
        Irp->IoStatus.Status = status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
        return status;
    }

	IoMarkIrpPending(Irp);
	IoStartPacket(DeviceObject, Irp, NULL, NULL);
	return STATUS_PENDING;

	////
 //   // FOR TESTING:
 //   // Initialize the data to mod 2 of some random number.
 //   // With this value you can control the number of times the
 //   // Irp will be queued before completion. Check 
 //   // LspPollDevice routine to know how this works.
 //   //

 //   KeQuerySystemTime(&currentTime);

 //   // *(ULONG *)Irp->AssociatedIrp.SystemBuffer =((currentTime.LowPart/13)%2);
	//// Irp->DriverContext[4] = ((currentTime.LowPart/13)%2);

 //   //
 //   // Queue the IRP and return STATUS_PENDING after signaling the
 //   // polling thread.
	////
 //   // **Note: IoCsqInsertIrp marks the IRP pending.
 //   //
 //   IoCsqInsertIrp(&deviceExtension->CancelSafeQueue, Irp, NULL);

 //   //
 //   // A semaphore remains signaled as long as its count is greater than 
 //   // zero, and non-signaled when the count is zero. Following function 
 //   // increments the semaphore count by 1.
 //   //

 //   KeReleaseSemaphore(
	//	&deviceExtension->IrpQueueSemaphore,
 //       0,// No priority boost
 //       1,// Increment semaphore by 1
 //       FALSE );// No WaitForXxx after this call

 //   return STATUS_PENDING;
}

VOID
LspPollingThread(
    IN PVOID Context
    )
/*++

Routine Description:

    This is the main thread that removes IRP from the queue
    and peforms I/O on it.

Arguments:

    Context     -- pointer to the device object

--*/
{
    PDEVICE_OBJECT DeviceObject = Context;  
    PDEVICE_EXTENSION DevExtension =  DeviceObject->DeviceExtension;
    PIRP Irp;
    NTSTATUS    Status;
    
    KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY );

    //
    // Now enter the main IRP-processing loop
    //
    while( TRUE )
    {
        //
        // Wait indefinitely for an IRP to appear in the work queue or for
        // the Unload routine to stop the thread. Every successful return 
        // from the wait decrements the semaphore count by 1.
        //
        KeWaitForSingleObject(
			&DevExtension->IrpQueueSemaphore,
            Executive,
            KernelMode,
            FALSE,
            NULL );

        //
        // See if thread was awakened because driver is unloading itself...
        //
        
        if( DevExtension->ThreadShouldStop ) 
		{
            PsTerminateSystemThread( STATUS_SUCCESS );
        }

        //
        // Remove a pending IRP from the queue.
        //
        Irp = IoCsqRemoveNextIrp(&DevExtension->CancelSafeQueue, NULL);

        if(!Irp) 
		{
            LSP_KDPRINT(("Oops, a queued irp got cancelled\n"));
            continue; // go back to waiting
        }
        
        while(TRUE) 
		{ 
            //
            // Perform I/O
            //
            Status = LspPollDevice(DeviceObject, Irp);
            if(Status == STATUS_PENDING) 
			{
                // 
                // Device is not ready, so sleep for a while and try again.
                //
                KeDelayExecutionThread(
					KernelMode, 
					FALSE,
                    &DevExtension->PollingInterval);
                
            }
			else 
			{
                //
                // I/O is successful, so complete the Irp.
                //
                Irp->IoStatus.Status = Status;
                IoCompleteRequest (Irp, IO_NO_INCREMENT);
                break; 
            }

        }
        //
        // Go back to the top of the loop to see if there's another request waiting.
        //
    } // end of while-loop
}

NTSTATUS
LspPollDevice(
    PDEVICE_OBJECT DeviceObject,
    PIRP    Irp
    )

/*++

Routine Description:

   Polls for data

Arguments:

    DeviceObject     -- pointer to the device object
    Irp             -- pointer to the requesing Irp


Return Value:

    STATUS_SUCCESS   -- if the poll succeeded,
    STATUS_TIMEOUT   -- if the poll failed (timeout),
                        or the checksum was incorrect
    STATUS_PENDING   -- if polled too soon

--*/
{
	NTSTATUS status;
	INT64 TimeBetweenPolls = 1; // 1000 * 1000 * 10;
    PINPUT_DATA         pInput;
    PDEVICE_EXTENSION   deviceExtension;
    PIO_STACK_LOCATION  irpStack;
    LARGE_INTEGER       currentTime;
	ULONG i;
	PUCHAR dataBuffer;
	ULONG dataLength;
	ULARGE_INTEGER location;
	lsp_status_t lspStatus;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    deviceExtension = DeviceObject->DeviceExtension;

    // pInput  = (PINPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
	KeQuerySystemTime(&currentTime);
	if (currentTime.QuadPart < 
		(TimeBetweenPolls + deviceExtension->LastPollTime.QuadPart))
    {
        return  STATUS_PENDING;
    }

    // 
    // Note down the current time as the last polled time
    // 
    KeQuerySystemTime(&deviceExtension->LastPollTime);

	dataLength = MmGetMdlByteCount(Irp->MdlAddress);
	dataBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
	if (NULL == dataBuffer)
	{
		Irp->IoStatus.Information = 0;
		return STATUS_NO_MEMORY;
	}

	if (0 != (dataLength % 512))
	{
		Irp->IoStatus.Information = 0;
		return STATUS_INVALID_PARAMETER;
	}

	//lsp_ide_identify(
	//	deviceExtension->LspHandle,
	//	0,
	//	0,
	//	0,
	//	)
	location.QuadPart = 0;
	location.LowPart = 0;
	
	//{
	//	LARGE_INTEGER delay;
	//	delay.QuadPart = 1000 * 10;
	//	KeDelayExecutionThread(
	//		KernelMode,
	//		FALSE,
	//		&delay);
	//}

	lspStatus = lsp_ide_read(
		deviceExtension->LspHandle,
		(lsp_large_integer_t*)&location,
		(lsp_uint16_t)(dataLength >> 9),
		dataBuffer,
		dataLength);

	// status = LspContinueLspProcess(DeviceObject, &lspStatus);

	status = STATUS_SUCCESS;

	if (NT_SUCCESS(status))
	{
		Irp->IoStatus.Information = dataLength;
	}

	return status;

//#ifdef REAL
//
//    RtlZeroMemory( pInput, sizeof(INPUT_DATA) );
//
//    //
//    // If currenttime is less than the lasttime polled plus
//    // minimum time required for the device to settle
//    // then don't poll  and return STATUS_PENDING
//    //
//    
//    KeQuerySystemTime(&currentTime);
//    if (currentTime->QuadPart < (TimeBetweenPolls +
//                deviceExtension->LastPollTime.QuadPart))
//    {
//        return  STATUS_PENDING;
//    }
//
//    //
//    // Read/Write to the port here.
//    // Fill the INPUT structure
//    // 
//       
//    // 
//    // Note down the current time as the last polled time
//    // 
//    
//    KeQuerySystemTime(&deviceExtension->LastPollTime);
//    
//    
//    return STATUS_SUCCESS;
//#else 
//
//    //
//    // With this conditional statement
//    // you can control the number of times the
//    // i/o should be retried before completing.
//    //
//    
//    if(pInput->Data-- <= 0) 
//    {                    
//        Irp->IoStatus.Information = sizeof(INPUT_DATA);
//        return STATUS_SUCCESS;
//    }
//    return STATUS_PENDING;
//    
// #endif

}

NTSTATUS
LspDispatchCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
/*++

Routine Description:
    This dispatch routine is called when the last handle (in
    the whole system) to a file object is closed. In other words, the open
    handle count for the file object goes to 0. A driver that holds pending
    IRPs internally must implement a routine for IRP_MJ_CLEANUP. When the
    routine is called, the driver should cancel all the pending IRPs that
    belong to the file object identified by the IRP_MJ_CLEANUP call. In other
    words, it should cancel all the IRPs that have the same file-object pointer
    as the one supplied in the current I/O stack location of the IRP for the
    IRP_MJ_CLEANUP call. Of course, IRPs belonging to other file objects should
    not be canceled. Also, if an outstanding IRP is completed immediately, the
    driver does not have to cancel it.

Arguments:

    DeviceObject     -- pointer to the device object
    Irp             -- pointer to the requesing Irp

Return Value:

    STATUS_SUCCESS   -- if the poll succeeded,
--*/
{

    PDEVICE_EXTENSION     deviceExtension;
    LIST_ENTRY             tempQueue;   
    PLIST_ENTRY            thisEntry;
    PIRP                   pendingIrp;
    PIO_STACK_LOCATION    pendingIrpStack, irpStack;

    LSP_KDPRINT(("LspCleanupIrp enter\n"));

    deviceExtension = DeviceObject->DeviceExtension;

    irpStack = IoGetCurrentIrpStackLocation(Irp);

    while(pendingIrp = IoCsqRemoveNextIrp(&deviceExtension->CancelSafeQueue,
                                irpStack->FileObject))
    {

        //
        // Cancel the IRP
        //
        
        pendingIrp->IoStatus.Information = 0;
        pendingIrp->IoStatus.Status = STATUS_CANCELLED;
        LSP_KDPRINT(("Cleanup cancelled irp\n"));
        IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
        
    }

    //
    // Finally complete the cleanup IRP
    //
    
    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    LSP_KDPRINT(("LspCleanupIrp exit\n"));

    return STATUS_SUCCESS;

}

VOID
LspUnload(
    IN PDRIVER_OBJECT DriverObject
    )
/*++

Routine Description:

    Free all the allocated resources, etc.

Arguments:

    DriverObject - pointer to a driver object.

Return Value:

    VOID
--*/
{
	NTSTATUS status;
    PDEVICE_OBJECT       deviceObject = DriverObject->DeviceObject;
    UNICODE_STRING      uniWin32NameString;
    PDEVICE_EXTENSION    deviceExtension = deviceObject->DeviceExtension;

    PAGED_CODE();

    LSP_KDPRINT(("LspUnload Enter\n"));

    //
    // Set the Stop flag
    //
    deviceExtension->ThreadShouldStop = TRUE;

    //
    // Make sure the thread wakes up 
    //
    KeReleaseSemaphore(&deviceExtension->IrpQueueSemaphore,
                        0,  // No priority boost
                        1,  // Increment semaphore by 1
                        TRUE );// WaitForXxx after this call

    //
    // Wait for the thread to terminate
    //
    KeWaitForSingleObject(deviceExtension->ThreadObject,
                        Executive,
                        KernelMode,
                        FALSE,
                        NULL );

    ObDereferenceObject(deviceExtension->ThreadObject);

	IoFreeWorkItem(deviceExtension->CloseWorkItem);

    //
    // Create counted string version of our Win32 device name.
    //

    RtlInitUnicodeString( &uniWin32NameString, LSP_DOS_DEVICE_NAME_U );

    IoDeleteSymbolicLink( &uniWin32NameString );

    ASSERT(!deviceObject->AttachedDevice);
    
    IoDeleteDevice( deviceObject );
 
    LSP_KDPRINT(("LspUnload Exit\n"));
    return;
}
