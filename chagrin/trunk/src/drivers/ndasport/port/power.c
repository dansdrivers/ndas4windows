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
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
	PNDASPORT_COMMON_EXTENSION CommonExtension;
	ULONG isRemoved;

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

	if (CommonExtension->IsPdo)
	{
		NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
			"PDO %s(%x) IRP=%p %s(%x) %s(%x)\n",
			DbgPowerMinorFunctionString(irpStack->MinorFunction), 
			irpStack->MinorFunction,
			Irp,
			DbgSystemPowerString(CommonExtension->SystemPowerState),
			CommonExtension->SystemPowerState,
			DbgDevicePowerString(CommonExtension->DevicePowerState),
			CommonExtension->DevicePowerState);

		status = NdasPortPdoDispatchPower(
			(PNDASPORT_PDO_EXTENSION) DeviceObject->DeviceExtension,
			Irp);
	}
	else
	{
		NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
			"FDO %s(%x) IRP=%p %s(%x) %s(%x)\n",
			DbgPowerMinorFunctionString(irpStack->MinorFunction), 
			irpStack->MinorFunction,
			Irp,
			DbgSystemPowerString(CommonExtension->SystemPowerState),
			CommonExtension->SystemPowerState,
			DbgDevicePowerString(CommonExtension->DevicePowerState),
			CommonExtension->DevicePowerState);

		status = NdasPortFdoDispatchPower(
			(PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension,
			Irp);

	}

	NpReleaseRemoveLock(CommonExtension, Irp);
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
    PIO_STACK_LOCATION  stack;

    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

    //
    // If the device is not stated yet, just pass it down.
    //
    
    if (FdoExtension->CommonExtension->DevicePnPState == NotStarted) 
	{
        PoStartNextPowerIrp(Irp);
        IoSkipCurrentIrpStackLocation(Irp);
        status = PoCallDriver(FdoExtension->CommonExtension->LowerDeviceObject, Irp);
        return status;

    }

    if(stack->MinorFunction == IRP_MN_SET_POWER) 
	{
		if (SystemPowerState == powerType)
		{
			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tRequest to set System state to %s\n",
				DbgSystemPowerString(powerState.SystemState));
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
    status =  PoCallDriver(FdoExtension->CommonExtension->LowerDeviceObject, Irp);
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
    NTSTATUS            status;
    PIO_STACK_LOCATION  stack;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    stack = IoGetCurrentIrpStackLocation (Irp);
    powerType = stack->Parameters.Power.Type;
    powerState = stack->Parameters.Power.State;

    switch (stack->MinorFunction) 
	{
    case IRP_MN_SET_POWER:

		switch (powerType) 
		{
        case DevicePowerState:
            
			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tSetting Device power state to %s\n", 
				DbgDevicePowerString(powerState.DeviceState));

			PoSetPowerState(
				PdoExtension->DeviceObject, 
				powerType, 
				powerState);

            PdoExtension->CommonExtension->
				DevicePowerState = powerState.DeviceState;
            status = STATUS_SUCCESS;
            break;

        case SystemPowerState:

			NdasPortTrace(NDASPORT_POWER, TRACE_LEVEL_INFORMATION,
				"\tSetting System power state to %s\n", 
				DbgSystemPowerString(powerState.SystemState));

            PdoExtension->CommonExtension->
				SystemPowerState = powerState.SystemState;
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
    case IRP_MN_POWER_SEQUENCE:
    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (status != STATUS_NOT_SUPPORTED) 
	{
        Irp->IoStatus.Status = status;
    }

    PoStartNextPowerIrp(Irp);
    status = Irp->IoStatus.Status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);

    return status;
}



//NTSTATUS
//NdasPortDispatchPower(
//    __in PDEVICE_OBJECT DeviceObject,
//    __inout PIRP Irp)
//{
//    PNDASPORT_COMMON_EXTENSION CommonExtension = 
//		(PNDASPORT_COMMON_EXTENSION) DeviceObject->DeviceExtension;
//
//	PIO_STACK_LOCATION irpStack;
//
//	irpStack = IoGetCurrentIrpStackLocation(Irp);
//
//    DebugPrint((1, "NdasPortDispatchPower: DeviceObject %x, Irp %x, Minor %x\n",
//                DeviceObject, Irp, irpStack->MinorFunction));
//
//	if (!CommonExtension->IsPdo) 
//	{
//		//
//		// FDO requests are not handled;
//		//
//		PoStartNextPowerIrp(Irp);
//		IoSkipCurrentIrpStackLocation(Irp);
//		return PoCallDriver(CommonExtension->LowerDeviceObject, Irp);
//	}
//	else
//	{
//		//
//		// PDO should complete the request
//		//
//		PoStartNextPowerIrp(Irp);
//		Irp->IoStatus.Status = STATUS_SUCCESS;
//		IoCompleteRequest(Irp, IO_NO_INCREMENT);
//		return STATUS_SUCCESS;
//	}
//
//}
