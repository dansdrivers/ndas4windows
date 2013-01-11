/* This file contains the routines for power support */

#include "port.h"

#ifdef RUN_WPP
#include "power.tmh"
#endif

NTSTATUS
NdasPortFdoDispatchPower(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
    __in PIRP Irp);

NTSTATUS
NdasPortPdoDispatchPower(
    __in PNDASPORT_PDO_EXTENSION PdoExtension,
    __in PIRP Irp);

VOID
NdasPortPdoDevicePowerIrpCompletion(
	__in PDEVICE_OBJECT DeviceObject,
	__in UCHAR MinorFunction,
	__in POWER_STATE PowerState,
	__in PVOID Context, /* SystemPowerIrp */
	__in PIO_STATUS_BLOCK IoStatus);

/*++
    Handles power Irps sent to both FDO and child PDOs.
    Note: Currently we do not implement full power handling
          for the FDO.
    
Arguments:

    DeviceObject - Pointer to the device object.
    Irp          - Pointer to the irp.

Return Value:

    NT status is returned.

--*/
NTSTATUS
NdasPortDispatchPower(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp)
{
	NTSTATUS status;
    PIO_STACK_LOCATION  irpStack;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	ULONG isRemoved;
	POWER_STATE powerState;
	POWER_STATE_TYPE powerType;

    status = STATUS_SUCCESS;
    irpStack = IoGetCurrentIrpStackLocation (Irp);
    ASSERT (IRP_MJ_POWER == irpStack->MajorFunction);

	CommonExtension = (PNDASPORT_COMMON_EXTENSION) DeviceObject->DeviceExtension;

	isRemoved = NpAcquireRemoveLock(CommonExtension, Irp);
	if (isRemoved)
	{
		PoStartNextPowerIrp(Irp);
		Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		return status;
	}

    //
    // If the device has been removed, the driver should 
    // not pass the IRP down to the next lower driver.
    //
    
    if (Deleted == CommonExtension->DevicePnPState) 
	{
        PoStartNextPowerIrp (Irp);
        Irp->IoStatus.Status = status = STATUS_NO_SUCH_DEVICE ;
        IoCompleteRequest (Irp, IO_NO_INCREMENT);
		NpReleaseRemoveLock(CommonExtension, Irp);
        return status;
    }

	powerState = irpStack->Parameters.Power.State;
	powerType = irpStack->Parameters.Power.Type;

	if (powerType == SystemPowerState)
	{
		NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
			"%s %s(%x) IRP=%p System=%s(%x)->%s(%x), Device=%s(%x)\n",
			CommonExtension->IsPdo ? "PDO" : "FDO",
			DbgPowerMinorFunctionString(irpStack->MinorFunction), 
			irpStack->MinorFunction,
			Irp,
			DbgSystemPowerString(CommonExtension->SystemPowerState),
			CommonExtension->SystemPowerState,
			DbgSystemPowerString(powerState.SystemState),
			powerState.SystemState,
			DbgDevicePowerString(CommonExtension->DevicePowerState),
			CommonExtension->DevicePowerState);
	}
	else /* powerType == DevicePowerState */
	{
		NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
			"%s %s(%x) IRP=%p System=%s(%x) Device=%s(%x)->%s(%x)\n",
			CommonExtension->IsPdo ? "PDO" : "FDO",
			DbgPowerMinorFunctionString(irpStack->MinorFunction), 
			irpStack->MinorFunction,
			Irp,
			DbgSystemPowerString(CommonExtension->SystemPowerState),
			CommonExtension->SystemPowerState,
			DbgDevicePowerString(CommonExtension->DevicePowerState),
			CommonExtension->DevicePowerState,
			DbgDevicePowerString(powerState.DeviceState),
			powerState.DeviceState);
	}

	if (CommonExtension->IsPdo)
	{
		status = NdasPortPdoDispatchPower(
			(PNDASPORT_PDO_EXTENSION) DeviceObject->DeviceExtension,
			Irp);
	}
	else
	{
		status = NdasPortFdoDispatchPower(
			(PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension,
			Irp);
	}

	if (STATUS_PENDING != status)
	{
		NpReleaseRemoveLock(CommonExtension, Irp);
	}

    return status;
}


/*++
    Handles power Irps sent to the FDO.
    This driver is power policy owner for the bus itself 
    (not the devices on the bus).Power handling for the bus FDO 
    should be implemented similar to the function driver (toaster.sys)
    power code. We will just print some debug outputs and 
    forward this Irp to the next level. 
    
Arguments:

    Data -  Pointer to the FDO device extension.
    Irp  -  Pointer to the irp.

Return Value:

    NT status is returned.

--*/
NTSTATUS
NdasPortFdoDispatchPower(
	__in PNDASPORT_FDO_EXTENSION FdoExtension,
    __in PIRP Irp)
{
    NTSTATUS            status;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;
    PIO_STACK_LOCATION  irpStack;
	PNDASPORT_COMMON_EXTENSION commonExtension;
	PIO_COMPLETION_ROUTINE powerCompletionRoutine;

    irpStack = IoGetCurrentIrpStackLocation (Irp);
    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;
	commonExtension = FdoExtension->CommonExtension;
	powerCompletionRoutine = NULL;

    //
    // If the device is not stated yet, just pass it down.
    //
    
    if (commonExtension->DevicePnPState == NotStarted) 
	{
        PoStartNextPowerIrp(Irp);
        IoSkipCurrentIrpStackLocation(Irp);
        status = PoCallDriver(commonExtension->LowerDeviceObject, Irp);
        return status;

    }

    if (irpStack->MinorFunction == IRP_MN_SET_POWER) 
	{
		if (SystemPowerState == powerType)
		{
			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tRequest to set System state to %s\n",
				DbgSystemPowerString(powerState.SystemState));
#if 0

			if (powerState.SystemState >= PowerSystemShutdown)
			{
				//
				// Shutdown
				//
				commonExtension->SystemPowerState == powerState.SystemState;
			}
			else if ((PowerSystemWorking == commonExtension->SystemPowerState) &&
				(PowerSystemWorking != powerState.SystemState))
			{
				//
				// Powering down.
				//
				// If DevicePowerState != D3 
				//   Send a Device Power Irp, when that completes successfully, 
				//   continue with System Power Irp,
				// Else
				//   Set the completion and pass it down    
				// 

				if (PowerDeviceD3 != commonExtension->DevicePowerState)
				{
					//
					// Powering down the device power IRP first
					//

					IoMarkIrpPending(Irp);

					powerState.DeviceState = PowerDeviceD3;

					status = PoRequestPowerIrp(
						commonExtension->Pdo)
				}
			}
#endif

		}
		else
		{
			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tRequest to set Device state to %s\n",
				DbgDevicePowerString(powerState.DeviceState));

		}
    }

    PoStartNextPowerIrp (Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    status = PoCallDriver(FdoExtension->CommonExtension->LowerDeviceObject, Irp);
    return status;
}

NTSTATUS
NdasPortPdoSendPowerSrb(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in DEVICE_POWER_STATE PowerState,
	__in POWER_ACTION PowerAction)
{
	SCSI_POWER_REQUEST_BLOCK srb;
	PIRP srbIrp;
	KEVENT event;
	IO_STATUS_BLOCK ioStatus;
	NTSTATUS status;
	PIO_STACK_LOCATION nextIrpStack;


	KeInitializeEvent(&event, NotificationEvent, FALSE);

	//srbIrp = IoAllocateIrp(
	//	PdoExtension->DeviceObject->StackSize + 1,
	//	FALSE);

	srbIrp = IoBuildDeviceIoControlRequest(
		IRP_MN_SCSI_CLASS,
		PdoExtension->DeviceObject,
		&srb,
		sizeof(SCSI_POWER_REQUEST_BLOCK),
		NULL,
		0,
		TRUE,
		&event,
		&ioStatus);

	if (NULL == srbIrp)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		return status;
	}

	//
	// Set up IRP stack for the next stack.
	//
	nextIrpStack = IoGetNextIrpStackLocation(srbIrp);
	nextIrpStack->Parameters.Scsi.Srb = (PSCSI_REQUEST_BLOCK)&srb;

	//
	// Set up the power srb.
	//

	RtlZeroMemory(&srb, sizeof(SCSI_POWER_REQUEST_BLOCK));
	srb.Length = sizeof(SCSI_POWER_REQUEST_BLOCK);
	srb.Function = SRB_FUNCTION_POWER;
	srb.SrbStatus = SRB_STATUS_PENDING;
	//
	// only adapter request uses SRB_POWER_FLAGS_ADAPTER_REQUEST
	// this is an LU request
	//
	srb.SrbPowerFlags = 0; 
	srb.PathId = PdoExtension->LogicalUnitAddress.PathId;
	srb.TargetId = PdoExtension->LogicalUnitAddress.TargetId;
	srb.Lun = PdoExtension->LogicalUnitAddress.Lun;
	srb.DevicePowerState = (STOR_DEVICE_POWER_STATE) PowerState;
	srb.SrbFlags = 
		SRB_FLAGS_NO_QUEUE_FREEZE | 
		SRB_FLAGS_BYPASS_LOCKED_QUEUE | 
		SRB_FLAGS_NO_DATA_TRANSFER;
	srb.DataTransferLength;
	srb.TimeOutValue = 0;
	srb.SenseInfoBuffer;
	srb.NextSrb;
	srb.OriginalRequest = srbIrp;
	srb.SrbExtension;
	srb.PowerAction = (STOR_POWER_ACTION) PowerAction;

	status = IoCallDriver(
		PdoExtension->DeviceObject,
		srbIrp);

	if (STATUS_PENDING == status)
	{
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		status = ioStatus.Status;
	}

	return status;
}

/*++
    Handles power Irps sent to the PDOs.
    Typically a bus driver, that is not a power
    policy owner for the device, does nothing
    more than starting the next power IRP and
    completing this one.
    
Arguments:

    PdoData - Pointer to the PDO device extension.
    Irp     - Pointer to the irp.

Return Value:

    NT status is returned.

--*/
NTSTATUS
NdasPortPdoDispatchPower(
    PNDASPORT_PDO_EXTENSION PdoExtension,
    PIRP Irp)
{
    NTSTATUS status;
    PIO_STACK_LOCATION irpStack;
    POWER_STATE powerState;
    POWER_STATE_TYPE powerType;
	POWER_ACTION powerAction;
	KIRQL oldIrql;
	PNDASPORT_COMMON_EXTENSION commonExtension;
	BOOLEAN sendDevicePowerIrp;

    irpStack = IoGetCurrentIrpStackLocation (Irp);
    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;
	powerAction = irpStack->Parameters.Power.ShutdownType;
	commonExtension = PdoExtension->CommonExtension;

    switch (irpStack->MinorFunction) 
	{
    case IRP_MN_SET_POWER:
		switch (powerType) 
		{
		case SystemPowerState:

			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_WARNING,
				"\tSetting System power state to %s\n", 
				DbgSystemPowerString(powerState.SystemState));

			sendDevicePowerIrp = FALSE;
			status = STATUS_SUCCESS;

			KeAcquireSpinLock(&commonExtension->PowerStateSpinLock, &oldIrql);

			if (powerState.SystemState >= PowerSystemShutdown)
			{
				//
				// Nothing to do for shutdown
				// SRB_FUNCTION_SHUTDOWN is the responsibility of classpnp.sys
				//
				commonExtension->SystemPowerState = powerState.SystemState;
			}
			else if (PowerSystemWorking == commonExtension->SystemPowerState &&
				PowerSystemWorking != powerState.SystemState)
			{
				commonExtension->SystemPowerState = powerState.SystemState;

				if (PowerDeviceD3 != commonExtension->DevicePowerState)
				{
					//
					// Powering down the device
					//
					powerState.DeviceState = PowerDeviceD3;
					sendDevicePowerIrp = TRUE;
				}
			}
			else if (PowerSystemWorking == powerState.SystemState)
			{
				commonExtension->SystemPowerState = powerState.SystemState;

				if (PowerDeviceD0 != commonExtension->DevicePowerState)
				{
					//
					// Powering up the device
					//
					powerState.DeviceState = PowerDeviceD0;
					sendDevicePowerIrp = TRUE;
				}
			}

			KeReleaseSpinLock(&commonExtension->PowerStateSpinLock, oldIrql);

			if (sendDevicePowerIrp)
			{
				NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
					"Sending DevicePowerIrp, pdo=%p, State=%d\n",
					PdoExtension->DeviceObject, powerState);

				IoMarkIrpPending(Irp);

				status = PoRequestPowerIrp(
					PdoExtension->DeviceObject,
					IRP_MN_SET_POWER,
					powerState,
					NdasPortPdoDevicePowerIrpCompletion,
					Irp,
					NULL);

				if (NT_SUCCESS(status))
				{
					return STATUS_PENDING;
				}

				irpStack->Control &= ~SL_PENDING_RETURNED;

				NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_ERROR,
					"PoRequestPowerIrp failed, pdo=%p, status=0x%x\n",
					PdoExtension->DeviceObject, status);

			}

			break;

        case DevicePowerState:
            
			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tSetting Device power state to %s\n", 
				DbgDevicePowerString(powerState.DeviceState));

			KeAcquireSpinLock(&commonExtension->PowerStateSpinLock, &oldIrql);
            commonExtension->DevicePowerState = powerState.DeviceState;
			KeReleaseSpinLock(&commonExtension->PowerStateSpinLock, oldIrql);

			//
			// We do not expect DO_POWER_INRUSH in this PDO
			//

			ASSERT(PASSIVE_LEVEL == KeGetCurrentIrql());

			status = NdasPortPdoSendPowerSrb(
				PdoExtension, 
				powerState.DeviceState,
				powerAction);

			//
			// Power SRB failure is not to be reported to the Power IRP itself.
			//

			if (!NT_SUCCESS(status))
			{
				NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_WARNING,
					"SCSI_POWER_REQUEST_BLOCK failed, status=0x%X\n",
					status);
			}

            status = STATUS_SUCCESS;
            break;

        default:
            status = STATUS_NOT_SUPPORTED;
            break;
        }
        break;

    case IRP_MN_QUERY_POWER:

		status = STATUS_SUCCESS;
        break;

    case IRP_MN_WAIT_WAKE:
        //
        // We cannot support wait-wake because we are root-enumerated 
        // driver, and our parent, the PnP manager, doesn't support wait-wake. 
        // If you are a bus enumerated device, and if  your parent bus supports
        // wait-wake,  you should send a wait/wake IRP (PoRequestPowerIrp)
        // in response to this request. 
        // If you want to test the wait/wake logic implemented in the function
        // driver (toaster.sys), you could do the following simulation: 
        // a) Mark this IRP pending.
        // b) Set a cancel routine.
        // c) Save this IRP in the device extension
        // d) Return STATUS_PENDING.
        // Later on if you suspend and resume your system, your BUS_FDO_POWER
        // will be called to power the bus. In response to IRP_MN_SET_POWER, if the
        // powerstate is PowerSystemWorking, complete this Wake IRP. 
        // If the function driver, decides to cancel the wake IRP, your cancel routine
        // will be called. There you just complete the IRP with STATUS_CANCELLED.
        //
		status = STATUS_NOT_SUPPORTED;
		break;

    case IRP_MN_POWER_SEQUENCE:

		status = STATUS_NOT_SUPPORTED;
		break;

    default:

		status = Irp->IoStatus.Status;
        break;
    }

    Irp->IoStatus.Status = status;
    PoStartNextPowerIrp(Irp);
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return status;
}

VOID
NdasPortPdoDevicePowerIrpCompletion(
	__in PDEVICE_OBJECT DeviceObject,
	__in UCHAR MinorFunction,
	__in POWER_STATE PowerState,
	__in PVOID Context, /* SystemPowerIrp */
	__in PIO_STATUS_BLOCK IoStatus)
{
	PIRP systemPowerIrp = (PIRP) Context;
	PNDASPORT_PDO_EXTENSION pdoExtension = NdasPortPdoGetExtension(DeviceObject);

	if (systemPowerIrp)
	{
		PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(systemPowerIrp);
		SYSTEM_POWER_STATE systemPowerState = irpStack->Parameters.Power.State.SystemState;

		NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
			"DevicePowerIrpCompletion, for a systemPowerIrp=%p, pdo=%p, systemPowerState=%d, devicePowerState=%d, status=0x%x\n",
			systemPowerIrp, 
			DeviceObject,
			systemPowerState,
			PowerState.DeviceState,
			IoStatus->Status);

		systemPowerIrp->IoStatus.Status = STATUS_SUCCESS;
		PoStartNextPowerIrp(systemPowerIrp);
		NpReleaseRemoveLock(pdoExtension->CommonExtension, systemPowerIrp);
		IoCompleteRequest(systemPowerIrp, IO_NO_INCREMENT);
	}
}
