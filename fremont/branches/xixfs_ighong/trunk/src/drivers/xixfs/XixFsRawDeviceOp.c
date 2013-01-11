#include "XixFsType.h"
#include "XixFsErrorInfo.h"
#include "XixFsDebug.h"
#include "XixFsAddrTrans.h"
#include "XixFsDrv.h"
#include "XixFsDiskForm.h"
#include "XixFsRawDiskAccessApi.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, XixFsRawDevIoCtrl)
#pragma alloc_text(PAGE, XixFsRawWriteBlockDevice)
#pragma alloc_text(PAGE, XixFsRawReadBlockDevice)
#endif


/*
 *	Function must be done within waitable thread context
 */


NTSTATUS
XixFsRawDevIoCtrl (
    IN	PDEVICE_OBJECT Device,
    IN	uint32 IoControlCode,
    IN	uint8 * InputBuffer OPTIONAL,
    IN	uint32 InputBufferLength,
    OUT uint8 * OutputBuffer OPTIONAL,
    IN	uint32 OutputBufferLength,
    IN	BOOLEAN InternalDeviceIoControl,
    OUT PIO_STATUS_BLOCK Iosb OPTIONAL
    )
{
    NTSTATUS Status;
    PIRP Irp;
    KEVENT Event;
    IO_STATUS_BLOCK LocalIosb;
    PIO_STATUS_BLOCK IosbToUse = &LocalIosb;

    PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsRawDevIoCtrl \n"));
    //
    //  Check if the user gave us an Iosb.
    //

    if (ARGUMENT_PRESENT( Iosb )) {

        IosbToUse = Iosb;
    }

    IosbToUse->Status = 0;
    IosbToUse->Information = 0;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    Irp = IoBuildDeviceIoControlRequest( IoControlCode,
                                         Device,
                                         InputBuffer,
                                         InputBufferLength,
                                         OutputBuffer,
                                         OutputBufferLength,
                                         InternalDeviceIoControl,
                                         &Event,
                                         IosbToUse );

    if (Irp == NULL) {

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver( Device, Irp );

    //
    //  We check for device not ready by first checking Status
    //  and then if status pending was returned, the Iosb status
    //  value.
    //

    if (Status == STATUS_PENDING) {

        (VOID) KeWaitForSingleObject( &Event,
                                      Executive,
                                      KernelMode,
                                      FALSE,
                                      (PLARGE_INTEGER)NULL );

        Status = IosbToUse->Status;
    }

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsRawDevIoCtrl \n"));
    return Status;
}


NTSTATUS
XixFsRawWriteBlockDevice (
	IN PDEVICE_OBJECT	DeviceObject,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	IN uint8			*Buffer
	)
{
	KEVENT			event;
	PIRP			irp;
	IO_STATUS_BLOCK io_status;
	NTSTATUS		status;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsRawWriteBlockDevice \n"));

	ASSERT(DeviceObject != NULL);
	ASSERT(Offset != NULL);
	ASSERT(Buffer != NULL);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("WriteRaw Off Offset (0x%I64x)  Length(%ld) Buffer(0x%x) DeviceObject(0x%x).\n", 
		Offset->QuadPart, Length, Buffer, DeviceObject));	


	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_WRITE,
		DeviceObject,
		Buffer,
		Length,
		Offset,
		&event,
		&io_status
		);

	if (!irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

    //SetFlag( IoGetNextIrpStackLocation( irp )->Flags, SL_WRITE_THROUGH  );


	status = IoCallDriver(DeviceObject, irp);

	if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = io_status.Status;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsRawWriteBlockDevice \n"));

	return status;
}



NTSTATUS
XixFsRawReadBlockDevice (
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	PLARGE_INTEGER	Offset,
	IN	uint32			Length,
	OUT uint8			*Buffer
	)
{
	KEVENT			event;
	PIRP			irp;
	IO_STATUS_BLOCK io_status;
	NTSTATUS		status;

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsRawReadBlockDevice \n"));

	ASSERT(DeviceObject != NULL);
	ASSERT(Offset != NULL);
	ASSERT(Buffer != NULL);

	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("ReadRaw Off Offset (0x%I64x)  Length(%ld) Buffer(0x%x) DeviceObject(0x%x).\n", 
		Offset->QuadPart,  Length, Buffer, DeviceObject));	

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_READ,
		DeviceObject,
		Buffer,
		Length,
		Offset,
		&event,
		&io_status
		);

	if (!irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	status = IoCallDriver(DeviceObject, irp);

	if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = io_status.Status;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsRawReadBlockDevice \n"));
	return status;
}



NTSTATUS
XixFsRawAlignSafeWriteBlockDevice (
	IN PDEVICE_OBJECT	DeviceObject,
	IN uint32			SectorSize,
	IN PLARGE_INTEGER	Offset,
	IN uint32			Length,
	IN uint8			*Buffer
	)
{
	KEVENT			event;
	PIRP			irp;
	IO_STATUS_BLOCK io_status;
	NTSTATUS		status;
	
	LARGE_INTEGER	NewOffset = {0,0};
	int32			StartMagin = 0;
	uint8			*NewBuffer = NULL;
	uint32			NewLength = 0;
	BOOLEAN			bNewAligned = FALSE;
	



	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsRawWriteBlockDevice \n"));

	ASSERT(DeviceObject != NULL);
	ASSERT(Offset != NULL);
	ASSERT(Buffer != NULL);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("WriteRaw Off Offset (0x%I64x)  Length(%ld) Buffer(0x%x) DeviceObject(0x%x).\n", 
		Offset->QuadPart, Length, Buffer, DeviceObject));	


	NewOffset.QuadPart = SECTOR_ALIGNED_ADDR(SectorSize, Offset->QuadPart);
	NewLength = (uint32) SECTOR_ALIGNED_SIZE(SectorSize, Length);
	
	if((NewOffset.QuadPart != Offset->QuadPart) || 
		(NewLength != Length) )
	{
		bNewAligned = TRUE;
		ASSERT(NewOffset.QuadPart <= Offset->QuadPart);
		StartMagin = (uint32)(Offset->QuadPart - NewOffset.QuadPart);
		
		NewBuffer = ExAllocatePoolWithTag(NonPagedPool, NewLength, TAG_BUFFER);
		if(!NewBuffer){
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(NewBuffer, NewLength);

	}else{
		NewBuffer = Buffer;
	}


	if(bNewAligned){
		DbgPrint("New Aligned Operation Write Offset(%I64d) Length(%ld) NewOffset(%I64d) NewLength(%ld)\n",
			Offset->QuadPart, Length, NewOffset.QuadPart, NewLength);

		KeInitializeEvent(&event, NotificationEvent, FALSE);
	
		irp = IoBuildSynchronousFsdRequest(
			IRP_MJ_READ,
			DeviceObject,
			NewBuffer,
			NewLength,
			&NewOffset,
			&event,
			&io_status
			);

		if (!irp)
		{
			ExFreePool(NewBuffer);	
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		status = IoCallDriver(DeviceObject, irp);

		if (status == STATUS_PENDING)
		{
			KeWaitForSingleObject(
				&event,
				Executive,
				KernelMode,
				FALSE,
				NULL
				);
			status = io_status.Status;
		}

		if(!NT_SUCCESS(status)){
			ExFreePool(NewBuffer);	
			return status;
		}


		RtlCopyMemory(Add2Ptr(NewBuffer, StartMagin), Buffer, Length);
		KeClearEvent(&event);
	}else{
		KeInitializeEvent(&event, NotificationEvent, FALSE);
	}

		


	irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_WRITE,
		DeviceObject,
		NewBuffer,
		NewLength,
		&NewOffset,
		&event,
		&io_status
		);

	if (!irp)
	{
		if(bNewAligned){
			ExFreePool(NewBuffer);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}

    //SetFlag( IoGetNextIrpStackLocation( irp )->Flags, SL_WRITE_THROUGH );


	status = IoCallDriver(DeviceObject, irp);

	if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = io_status.Status;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsRawWriteBlockDevice \n"));

	if(bNewAligned){
		ExFreePool(NewBuffer);
	}

	return status;
}



NTSTATUS
XixFsRawAlignSafeReadBlockDevice (
	IN	PDEVICE_OBJECT	DeviceObject,
	IN	uint32			SectorSize,
	IN	PLARGE_INTEGER	Offset,
	IN	uint32			Length,
	OUT uint8			*Buffer
	)
{
	KEVENT			event;
	PIRP			irp;
	IO_STATUS_BLOCK io_status;
	NTSTATUS		status;

	LARGE_INTEGER	NewOffset = {0,0};
	int32			StartMagin = 0;
	uint8			*NewBuffer = NULL;
	uint32			NewLength = 0;
	BOOLEAN			bNewAligned = FALSE;


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsRawReadBlockDevice \n"));

	ASSERT(DeviceObject != NULL);
	ASSERT(Offset != NULL);
	ASSERT(Buffer != NULL);

	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("ReadRaw Off Offset (0x%I64x)  Length(%ld) Buffer(0x%x) DeviceObject(0x%x).\n", 
		Offset->QuadPart,  Length, Buffer, DeviceObject));	


	NewOffset.QuadPart = SECTOR_ALIGNED_ADDR(SectorSize, Offset->QuadPart);
	NewLength = (uint32) SECTOR_ALIGNED_SIZE(SectorSize, Length);

	if((NewOffset.QuadPart != Offset->QuadPart) || 
		(NewLength != Length) )
	{
		
		DbgPrint("New Aligned Operation Read Offset(%I64d) Length(%ld) NewOffset(%I64d) NewLength(%ld)\n",
			Offset->QuadPart, Length, NewOffset.QuadPart, NewLength);

		bNewAligned = TRUE;
		ASSERT(NewOffset.QuadPart <= Offset->QuadPart);
		StartMagin = (uint32)(Offset->QuadPart - NewOffset.QuadPart);
		
		NewBuffer = ExAllocatePoolWithTag(NonPagedPool, NewLength, TAG_BUFFER);
		if(!NewBuffer){
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		RtlZeroMemory(NewBuffer, NewLength);

	}else{
		NewBuffer = Buffer;
	}




	KeInitializeEvent(&event, NotificationEvent, FALSE);

	irp = IoBuildSynchronousFsdRequest(
		IRP_MJ_READ,
		DeviceObject,
		Buffer,
		Length,
		Offset,
		&event,
		&io_status
		);

	if (!irp)
	{
		if(bNewAligned){
			ExFreePool(NewBuffer);
		}

		return STATUS_INSUFFICIENT_RESOURCES;
	}


	status = IoCallDriver(DeviceObject, irp);

	if (status == STATUS_PENDING)
	{
		KeWaitForSingleObject(
			&event,
			Executive,
			KernelMode,
			FALSE,
			NULL
			);
		status = io_status.Status;
	}


	if(bNewAligned){
		RtlCopyMemory(Buffer, Add2Ptr(NewBuffer, StartMagin), Length);
		ExFreePool(NewBuffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsRawReadBlockDevice \n"));
	return status;
}
