#pragma once

FORCEINLINE
ULONG
NpAcquireRemoveLock(
    __in PNDASPORT_COMMON_EXTENSION CommonExtension,
    __in_opt PVOID Tag)
{
	LONG result;

	UNREFERENCED_PARAMETER(Tag);

	result = InterlockedIncrement(&CommonExtension->OutstandingIO);

	ASSERT(result > 0);
	//
	// Need to clear StopEvent (when OutstandingIO bumps from 1 to 2) 
	//
	if (result == 2) 
	{
		//
		// We need to clear the event
		//
		KeClearEvent(&CommonExtension->StopEvent);
	}
	return (CommonExtension->DevicePnPState == Deleted);
}

FORCEINLINE
ULONG
NpdoAcquireRemoveLock(
    __in PNDASPORT_PDO_EXTENSION PdoExtension,
    __in_opt PVOID Tag)
{
	return NpAcquireRemoveLock(PdoExtension->CommonExtension, Tag);
}

FORCEINLINE
VOID
NpReleaseRemoveLock(
    __in PNDASPORT_COMMON_EXTENSION CommonExtension,
    __in_opt PVOID Tag)
{
	LONG result;

	UNREFERENCED_PARAMETER(Tag);

	result = InterlockedDecrement(&CommonExtension->OutstandingIO);
	ASSERT(result >= 0);

	if (result == 1) 
	{
		//
		// Set the stop event. Note that when this happens
		// (i.e. a transition from 2 to 1), the type of requests we 
		// want to be processed are already held instead of being 
		// passed away, so that we can't "miss" a request that
		// will appear between the decrement and the moment when
		// the value is actually used.
		//
		KeSetEvent(
			&CommonExtension->StopEvent, 
			IO_NO_INCREMENT, 
			FALSE);
	}
	else if (result == 0) 
	{
		//
		// The count is 1-biased, so it can be zero only if an 
		// extra decrement is done when a remove Irp is received 
		//
		ASSERT(CommonExtension->DevicePnPState == Deleted);

		//
		// Set the remove event, so the device object can be deleted
		//
		KeSetEvent(
			&CommonExtension->RemoveEvent, 
			IO_NO_INCREMENT, 
			FALSE);
	}
}

FORCEINLINE
VOID
NpdoReleaseRemoveLock(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in_opt PVOID Tag)
{
	NpReleaseRemoveLock(PdoExtension->CommonExtension, Tag);
}

FORCEINLINE
NTSTATUS
NpReleaseRemoveLockAndForwardIrp(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension,
	__in PIRP Irp)
{
	//
	// Be sure to get the pointer of the lower device object fist
	// CommonExtension may not be available after ReleaseRemoveLock!
	//
	PDEVICE_OBJECT lowerDeviceObject = CommonExtension->LowerDeviceObject;

	NpReleaseRemoveLock(CommonExtension, Irp);
	// When forwarding we should not change status
	// Irp->IoStatus.Status = Status;
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(lowerDeviceObject, Irp);
}

FORCEINLINE
NTSTATUS
NpReleaseRemoveLockAndCompleteIrpEx(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension,
	__in PIRP Irp,
	__in NTSTATUS Status,
	__in ULONG_PTR Information,
	__in CCHAR PriorityBoost)
{
	NpReleaseRemoveLock(CommonExtension, Irp);
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;
	IoCompleteRequest(Irp, PriorityBoost);
	return Status;
}

FORCEINLINE
NTSTATUS
NpReleaseRemoveLockAndCompleteIrp(
	__in PNDASPORT_COMMON_EXTENSION CommonExtension,
	__in PIRP Irp,
	__in NTSTATUS Status)
{
	NpReleaseRemoveLock(CommonExtension, Irp);
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}
