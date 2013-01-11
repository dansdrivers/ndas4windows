/*++

This module contains the WMI support code 

--*/
#include "port.h"
#include <initguid.h>
#include "ndasport.wmi.h"

#ifdef RUN_WPP
#include "wmi.tmh"
#endif

#ifndef IRP_MN_REGINFO_EX
#define IRP_MN_REGINFO_EX 0x0b
#endif

#define NDASPORT_FDO_MOFRESOURCENAME L"NdasPortWMI"

WMIGUIDREGINFO NdasPortWmiGuidList[] =
{
    &NdasPortWmiInformation_GUID, 1, 0, // driver information
	&NdasPortWmiEvent_GUID, 1, 0,
#if defined(RUN_WPP) && defined(WPP_TRACE_W2K_COMPATABILITY)
	(LPCGUID)&WPP_TRACE_CONTROL_NULL_GUID, 1, 
		(WMIREG_FLAG_TRACE_CONTROL_GUID | WMIREG_FLAG_TRACED_GUID),
#endif
};

enum {
	NDASPORT_FDO_WMI_INFORMATION_INDEX,
	NDASPORT_FDO_WMI_EVENT_INDEX
};

#define NUMBER_OF_WMI_GUIDS countof(NdasPortWmiGuidList)

NTSTATUS
NdasPortFdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName,
    OUT PUNICODE_STRING *RegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT *Pdo);

NTSTATUS
NdasPortFdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG InstanceCount,
    IN OUT PULONG InstanceLengthArray,
    IN ULONG OutBufferSize,
    OUT PUCHAR Buffer);

NTSTATUS
NdasPortFdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer);

NTSTATUS
NdasPortFdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,NdasPortFdoDispatchSystemControl)
#pragma alloc_text(PAGE,NdasPortFdoWmiRegister)
#pragma alloc_text(PAGE,NdasPortFdoWmiDeregister)
#pragma alloc_text(PAGE,NdasPortFdoQueryWmiRegInfo)
#pragma alloc_text(PAGE,NdasPortFdoQueryWmiDataBlock)
#pragma alloc_text(PAGE,NdasPortFdoSetWmiDataBlock)
#pragma alloc_text(PAGE,NdasPortFdoSetWmiDataItem)
#endif /* ALLOC_PRAGMA */

/*++

Routine Description

Registers with WMI as a data provider for this
instance of the device

--*/
NTSTATUS
NdasPortFdoWmiRegister(
    __in PNDASPORT_FDO_EXTENSION FdoExtension)
{
    NTSTATUS status;

    PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, __FUNCTION__ "\n");

    FdoExtension->WmiLibInfo.GuidCount = NUMBER_OF_WMI_GUIDS;
    FdoExtension->WmiLibInfo.GuidList = NdasPortWmiGuidList;
    FdoExtension->WmiLibInfo.QueryWmiRegInfo = NdasPortFdoQueryWmiRegInfo;
    FdoExtension->WmiLibInfo.QueryWmiDataBlock = NdasPortFdoQueryWmiDataBlock;
    FdoExtension->WmiLibInfo.SetWmiDataBlock = NdasPortFdoSetWmiDataBlock;
    FdoExtension->WmiLibInfo.SetWmiDataItem = NdasPortFdoSetWmiDataItem;
    FdoExtension->WmiLibInfo.ExecuteWmiMethod = NULL;
    FdoExtension->WmiLibInfo.WmiFunctionControl = NULL;

    //
    // Register with WMI
    //
    
    status = IoWMIRegistrationControl(
		FdoExtension->DeviceObject,
        WMIREG_ACTION_REGISTER);

    //
    // Initialize the Std device data structure
    //
                    
    return status;
    
}

/*++
Routine Description

     Inform WMI to remove this DeviceObject from its 
     list of providers. This function also 
     decrements the reference count of the deviceobject.

--*/
NTSTATUS
NdasPortFdoWmiDeregister(
	__in PNDASPORT_FDO_EXTENSION FdoExtension)
{
	PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, __FUNCTION__ "\n");

    return IoWMIRegistrationControl(
		FdoExtension->DeviceObject,
		WMIREG_ACTION_DEREGISTER);
}

NTSTATUS
NdasPortFdoDispatchSystemControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
	NTSTATUS status;
	PNDASPORT_FDO_EXTENSION FdoExtension;
    SYSCTL_IRP_DISPOSITION disposition;
	PIO_STACK_LOCATION irpStack;
	ULONG isRemoved;

    FdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

	irpStack = IoGetCurrentIrpStackLocation(Irp);

	NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_WARNING, 
		"FDO %s\n", DbgWmiMinorFunctionStringA(irpStack->MinorFunction));

	isRemoved = NpAcquireRemoveLock(FdoExtension->CommonExtension, Irp);
	if (isRemoved)
	{
		return NpReleaseRemoveLockAndCompleteIrp(
			FdoExtension->CommonExtension,
			Irp,
			STATUS_NO_SUCH_DEVICE);
	}

	if ((PDEVICE_OBJECT) irpStack->Parameters.WMI.ProviderId != DeviceObject)
	{
		return NpReleaseRemoveLockAndForwardIrp(
			FdoExtension->CommonExtension,
			Irp);
	}

#ifdef RUN_WPP
#ifdef WPP_TRACE_W2K_COMPATABILITY

    //
    // One of the mythical things that people tell you that
    //  you need to do to get WPP tracing to work on Windows
    //  2000 is to call the WPP_SYSTEMCONTROL macro in your
    //  DriverEntry entry point. The problem with this is that
    //  what is does is actually trash your IRP_MJ_SYSTEM_CONTROL
    //  entry point, meaning that you can no longer expose your
    //  driver through WMI. What we will do to get around this
    //  is to incorporate a call to the function that the WPP
    //  IRP_MJ_SYSTEM_CONTROL handler calls in order to handle
    //  WPP data. By doing this, we can satisfy WPP while
    //  also supporting our own WMI data.
    //
    //  You can find the code for the WPP SYSTEM_CONTROL
    //  handler (WPPSystemControlDispatch) in the km-init.tpl
    //  file located in the DDK's bin\wppconfig directory.
    //
    if (DeviceObject == (PDEVICE_OBJECT)irpStack->Parameters.WMI.ProviderId) 
	{
        //
        // If this is a REG_INFO request and we have not registered
        //  with WMI yet, we must zero the buffer. The reason for this
        //  is that the WPP code will just blindly use it without
        //  verifying any of the members of the structure. We learned
        //  this trick from the WPPSystemControl handler code in
        //  km-init.tpl
        //
        if (irpStack->MinorFunction == IRP_MN_REGINFO && !FdoExtension->IsWmiRegistered) 
		{
            RtlZeroMemory(
				irpStack->Parameters.WMI.Buffer,
                irpStack->Parameters.WMI.BufferSize);
        }

        //
        // Yet another nice thing about trying to get tracing and WMI
        //  support working is that we can NOT allow an IRP_MN_REGINFO_EX
        //  request to get to the WMI library, we must do the registering
        //  of WMI through an IRP_MN_REGINFO request.
        //
        // Why? Because the W2K tracing code has no idea what an
        //  IRP_MN_REGINFO_EX is, so it will refuse to setup the
        //  parts necessary for WPP tracing support. So, if
        //  we see an IRP_MN_REGINFO_EX request come in and we're
        //  built for Windows 2000, that means that we need to
        //  reject it to force the code to send us an IRP_MN_REGINFO
        //  request instead.
        //
        // IRP_MN_REGINFO_EX isn't defined for Windows 2000, so we use
        //  its constant value of 0xb here.
        //
        if (IRP_MN_REGINFO_EX == irpStack->MinorFunction) 
		{
            //
            // Fail the request
            //
			return NpReleaseRemoveLockAndCompleteIrpEx(
				FdoExtension->CommonExtension,
				Irp,
				STATUS_INVALID_DEVICE_REQUEST,
				0,
				IO_NO_INCREMENT);
        }

    }

#endif // W2K
#endif //RUN_WPP

    //
    // Call WMILIB to process the request.
    //

    status = WmiSystemControl(
		&FdoExtension->WmiLibInfo,
        DeviceObject,
        Irp,
        &disposition);

    //
    // Check the disposition of the request, so that we can determine
    // what to do.
    //

    switch(disposition) {

        case IrpProcessed:
        {
            //
            // This IRP has been processed and may be completed or pending.
			//
			
			//
			// DispatchWmiXXXX routine is responsible to hold the remove lock.
			// WmiRoutine or our DispatchWmiXXX may later complete the IRP.
			// We just release our remove lock here.
			//
			NpReleaseRemoveLock(
				FdoExtension->CommonExtension, 
				Irp);
			return status;
        }
       
        case IrpNotCompleted:
        {
			NTSTATUS wppStatus = STATUS_SUCCESS;
#ifdef RUN_WPP
#ifdef WPP_TRACE_W2K_COMPATABILITY
			ULONG bytesReturned;

            //
            // If the WMILIB didn't complete the IRP, then let's
            //  see if WPP will.
            //
			KdPrint(("NDASPORT.SYS: WPP_TRACE_CONTROL (MN=%X)\n", irpStack->MinorFunction));
            wppStatus = WPP_TRACE_CONTROL(
				irpStack->MinorFunction,
                irpStack->Parameters.WMI.Buffer,
                irpStack->Parameters.WMI.BufferSize,
                bytesReturned);

            if (!NT_SUCCESS(status) &&
                NT_SUCCESS(wppStatus)) 
			{
                //
                // WMILIB failed the IRP, but WPP completed it
                //  with success. Therefore, we'll change the IRP's
                //  status and bytes returned count to what WPP
                //  would like them to be.
                //
                Irp->IoStatus.Status = wppStatus;
                Irp->IoStatus.Information = bytesReturned;
                status = wppStatus;
            }

#endif // W2K
#endif //RUN_WPP
 
            //
            // This IRP has not been completed, but has been fully processed.
            // we will complete it now.
            //
			return NpReleaseRemoveLockAndCompleteIrp(
				FdoExtension->CommonExtension,
				Irp,
				wppStatus);
        }
       
		case IrpForward:
			return NpReleaseRemoveLockAndForwardIrp(
				FdoExtension->CommonExtension,
				Irp);
        case IrpNotWmi:
		default:
			status = Irp->IoStatus.Status;
			return NpReleaseRemoveLockAndCompleteIrp(
				FdoExtension->CommonExtension,
				Irp,
				status);
#if 0
        {
            //
            // This IRP is either not a WMI IRP or is a WMI IRP targeted
            // at a device lower in the stack.

			return NpReleaseRemoveLockAndForwardIrp(
				FdoExtension->CommonExtension,
				Irp);
        }
                                   
        default:
        {
             //
            // We really should never get here, but if we do just forward.... 
            //
            ASSERTMSG("Unkown SYSCTL_IRP_DISPOSITION from WmiSystemControl",
                      FALSE);
			return NpReleaseRemoveLockAndForwardIrp(
				FdoExtension->CommonExtension,
				Irp);
       }       
#endif
    }
}

//
// WMI System Call back functions
//


/*++

Routine Description:

    This routine is a callback into the driver to set for the contents of
    a data block. When the driver has finished filling the data block it
    must call WmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    InstanceIndex is the index that denotes which instance of the data block
        is being queried.
            
    DataItemId has the id of the data item being set

    BufferSize has the size of the data item passed

    Buffer has the new values for the data item


Return Value:

    status

--*/
NTSTATUS
NdasPortFdoSetWmiDataItem(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG DataItemId,
    IN ULONG BufferSize,
    IN PUCHAR Buffer)
{
	PNDASPORT_FDO_EXTENSION FdoExtension;
    NTSTATUS status;
    ULONG requiredSize = 0;

    PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, __FUNCTION__ "\n");

	FdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

    switch (GuidIndex) 
	{
    case NDASPORT_FDO_WMI_INFORMATION_INDEX:
 
        status = STATUS_WMI_READ_ONLY;
        break;

	case NDASPORT_FDO_WMI_EVENT_INDEX:
		status = STATUS_WMI_READ_ONLY;
		break;

	default:
        status = STATUS_WMI_GUID_NOT_FOUND;
    }

    status = WmiCompleteRequest(
		DeviceObject,
        Irp,
        status,
        requiredSize,
        IO_NO_INCREMENT);

    return status;
}

/*++

Routine Description:

    This routine is a callback into the driver to set the contents of
    a data block. When the driver has finished filling the data block it
    must call WmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    InstanceIndex is the index that denotes which instance of the data block
        is being queried.
            
    BufferSize has the size of the data block passed

    Buffer has the new values for the data block


Return Value:

    status

--*/
NTSTATUS
NdasPortFdoSetWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer)
{
	PNDASPORT_FDO_EXTENSION FdoExtension;
    NTSTATUS status;
    ULONG requiredSize = 0;

    PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, 
		__FUNCTION__ ": GuidIndex=%u, InstanceIndex=%u, BufferSize=%u, Buffer=%p\n",
		GuidIndex, InstanceIndex, BufferSize, Buffer);

	FdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

    switch (GuidIndex)
	{
    case NDASPORT_FDO_WMI_INFORMATION_INDEX:
		status = STATUS_SUCCESS;
		break;
	case NDASPORT_FDO_WMI_EVENT_INDEX:
		status = STATUS_SUCCESS;
		break;

	default:
        status = STATUS_WMI_GUID_NOT_FOUND;
    }

    status = WmiCompleteRequest(
		DeviceObject,
        Irp,
        status,
        requiredSize,
        IO_NO_INCREMENT);

    return(status);
}

/*++

Routine Description:

    This routine is a callback into the driver to query for the contents of
    a data block. When the driver has finished filling the data block it
    must call WmiCompleteRequest to complete the irp. The driver can
    return STATUS_PENDING if the irp cannot be completed immediately.

Arguments:

    DeviceObject is the device whose data block is being queried

    Irp is the Irp that makes this request

    GuidIndex is the index into the list of guids provided when the
        device registered

    InstanceIndex is the index that denotes which instance of the data block
        is being queried.
            
    InstanceCount is the number of instnaces expected to be returned for
        the data block.
            
    InstanceLengthArray is a pointer to an array of ULONG that returns the 
        lengths of each instance of the data block. If this is NULL then
        there was not enough space in the output buffer to fulfill the request
        so the irp should be completed with the buffer needed.        
            
    BufferAvail on has the maximum size available to write the data
        block.

    Buffer on return is filled with the returned data block


Return Value:

    status

--*/
NTSTATUS
NdasPortFdoQueryWmiDataBlock(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG InstanceCount,
    IN OUT PULONG InstanceLengthArray,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer)
{
	NTSTATUS status;
	PNDASPORT_FDO_EXTENSION FdoExtension;
    ULONG requiredSize = 0;

    PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, 
		__FUNCTION__ ": GuidIndex=%u, InstanceIndex=%u, InstanceCount=%u, "
		"BufferAvail=%u, Buffer=%p\n",
		GuidIndex, InstanceIndex, InstanceCount, BufferAvail, Buffer);

	//
    // Only ever registers 1 instance per guid
    //
    
    ASSERT((InstanceIndex == 0) && (InstanceCount == 1));
  
	FdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

    switch (GuidIndex) 
	{
	case NDASPORT_FDO_WMI_INFORMATION_INDEX:
		{
			PNdasPortWmiInformation ndasportWmiInfo;
			requiredSize = NdasPortWmiInformation_SIZE;
			if ( BufferAvail < requiredSize) 
			{
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}
			ndasportWmiInfo = (PNdasPortWmiInformation) Buffer;

			ExAcquireFastMutex (&FdoExtension->Mutex);
			ndasportWmiInfo->LogicalUnitCount = FdoExtension->NumberOfPDOs;
			ExReleaseFastMutex (&FdoExtension->Mutex); 

			*InstanceLengthArray = requiredSize;
			status = STATUS_SUCCESS;
		}
        break;
	case NDASPORT_FDO_WMI_EVENT_INDEX:
		*InstanceLengthArray = 0;
		status = STATUS_SUCCESS;
		break;
    default:
        status = STATUS_WMI_GUID_NOT_FOUND;
		break;
    }

    status = WmiCompleteRequest(
		DeviceObject,
        Irp,
        status,
        requiredSize,
        IO_NO_INCREMENT);

    return status;
}

/*++

Routine Description:

    This routine is a callback into the driver to retrieve the list of
    guids or data blocks that the driver wants to register with WMI. This
    routine may not pend or block. Driver should NOT call
    WmiCompleteRequest.

Arguments:

    DeviceObject is the device whose data block is being queried

    *RegFlags returns with a set of flags that describe the guids being
        registered for this device. If the device wants to enable and disable
        collection callbacks before receiving queries for the registered
        guids then it should return the WMIREG_FLAG_EXPENSIVE flag. Also the
        returned flags may specify WMIREG_FLAG_INSTANCE_PDO in which case
        the instance name is determined from the PDO associated with the
        device object. Note that the PDO must have an associated devnode. If
        WMIREG_FLAG_INSTANCE_PDO is not set then Name must return a unique
        name for the device.

    InstanceName returns with the instance name for the guids if
        WMIREG_FLAG_INSTANCE_PDO is not set in the returned *RegFlags. The
        caller will call ExFreePool with the buffer returned.

    *RegistryPath returns with the registry path of the driver

    *MofResourceName returns with the name of the MOF resource attached to
        the binary file. If the driver does not have a mof resource attached
        then this can be returned as NULL.

    *Pdo returns with the device object for the PDO associated with this
        device if the WMIREG_FLAG_INSTANCE_PDO flag is retured in 
        *RegFlags.

Return Value:

    status

--*/
NTSTATUS
NdasPortFdoQueryWmiRegInfo(
    IN PDEVICE_OBJECT DeviceObject,
    OUT ULONG *RegFlags,
    OUT PUNICODE_STRING InstanceName,
    OUT PUNICODE_STRING *RegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT *Pdo)
{
	PNDASPORT_FDO_EXTENSION FdoExtension;
	PNDASPORT_DRIVER_EXTENSION driverExtension;

    PAGED_CODE();

	NdasPortTrace(NDASPORT_FDO_GENERAL, TRACE_LEVEL_WARNING, __FUNCTION__ "\n");

	driverExtension = IoGetDriverObjectExtension(
		DeviceObject->DriverObject, 
		NdasPortDriverExtensionTag);

	ASSERT(NULL != driverExtension);

    FdoExtension = (PNDASPORT_FDO_EXTENSION) DeviceObject->DeviceExtension;

    *RegFlags = WMIREG_FLAG_INSTANCE_PDO;
    *RegistryPath = &driverExtension->RegistryPath;
    *Pdo = FdoExtension->LowerPdo;
    RtlInitUnicodeString(MofResourceName, NDASPORT_FDO_MOFRESOURCENAME);

    return STATUS_SUCCESS;
}


NTSTATUS
NdasPortPdoDispatchSystemControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION IrpStack;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	ULONG isRemoved;
	SYSCTL_IRP_DISPOSITION disposition;

	PAGED_CODE();

	PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	IrpStack = IoGetCurrentIrpStackLocation (Irp);

	//
	// The PDO, just complete the request with the current status
	//
	NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_WARNING, 
		"PDO %s\n", DbgWmiMinorFunctionStringA(IrpStack->MinorFunction));

	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	if (isRemoved)
	{
		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension,
			Irp,
			STATUS_NO_SUCH_DEVICE);
	}

	if ((PDEVICE_OBJECT) IrpStack->Parameters.WMI.ProviderId != DeviceObject)
	{
		return NpReleaseRemoveLockAndForwardIrp(
			PdoExtension->CommonExtension,
			Irp);
	}

	status = WmiSystemControl(
		&PdoExtension->WmiLibInfo,
		DeviceObject,
		Irp,
		&disposition);

	//
	// Check the disposition of the request, so that we can determine
	// what to do.
	//

	switch(disposition) 
	{
	case IrpProcessed:
		//
		// This IRP has been processed and may be completed or pending.
		//

		//
		// DispatchWmiXXXX routine is responsible to hold the remove lock.
		// WmiRoutine or our DispatchWmiXXX may later complete the IRP.
		// We just release our remove lock here.
		//
		NpReleaseRemoveLock(
			PdoExtension->CommonExtension, 
			Irp);
		return status;
	case IrpNotCompleted:
		status = STATUS_SUCCESS;
		//
		// This IRP has not been completed, but has been fully processed.
		// we will complete it now.
		//
		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension,
			Irp,
			status);
	case IrpForward:
		//
		// This IRP is either not a WMI IRP or is a WMI IRP targeted
		// at a device lower in the stack.
		return NpReleaseRemoveLockAndForwardIrp(
			PdoExtension->CommonExtension,
			Irp);
	case IrpNotWmi:
	default:
		//
		// We really should never get here, but if we do just forward.... 
		//
		//ASSERTMSG("Unknown SYSCTL_IRP_DISPOSITION from WmiSystemControl",
		//	FALSE);
		status = Irp->IoStatus.Status;
		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension,
			Irp,
			status);
	}
}


#ifdef NDASPORT_IMP_SCSI_WMI

typedef struct _NDASPORT_WMI_PARAMETERS {
	ULONG_PTR ProviderId; // ProviderId parameter from IRP
	PVOID DataPath;      // DataPath parameter from IRP
	ULONG BufferSize;    // BufferSize parameter from IRP
	PVOID Buffer;        // Buffer parameter from IRP
} NDASPORT_WMI_PARAMETERS, *PNDASPORT_WMI_PARAMETERS;

NTSTATUS
NdasPortPdoDispatchWmiRegister(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__inout PNDASPORT_WMI_PARAMETERS WmiParameters);

NTSTATUS
NdasPortPdoSendWmiSrb(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in UCHAR WmiMinorFunction,
	__inout PNDASPORT_WMI_PARAMETERS WmiParameters);

NTSTATUS
NdasPortPdoDispatchSystemControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	NTSTATUS status;
	PIO_STACK_LOCATION IrpStack;
	PNDASPORT_PDO_EXTENSION PdoExtension;
	NDASPORT_WMI_PARAMETERS wmiParams;
	ULONG isRemoved;
	SYSCTL_IRP_DISPOSITION disposition;

	PAGED_CODE();

	PdoExtension = NdasPortPdoGetExtension(DeviceObject);
	IrpStack = IoGetCurrentIrpStackLocation (Irp);

	//
	// The PDO, just complete the request with the current status
	//
	NdasPortTrace(NDASPORT_PDO_GENERAL, TRACE_LEVEL_INFORMATION, 
		"PDO %s\n", DbgWmiMinorFunctionStringA(IrpStack->MinorFunction));

	isRemoved = NpAcquireRemoveLock(PdoExtension->CommonExtension, Irp);
	if (isRemoved)
	{
		return NpReleaseRemoveLockAndCompleteIrp(
			PdoExtension->CommonExtension,
			Irp,
			STATUS_NO_SUCH_DEVICE);
	}

	if ((PDEVICE_OBJECT) IrpStack->Parameters.WMI.ProviderId != DeviceObject)
	{
		return NpReleaseRemoveLockAndForwardIrp(
			PdoExtension->CommonExtension,
			Irp);
	}

	wmiParams.ProviderId = IrpStack->Parameters.WMI.ProviderId;
	wmiParams.DataPath = IrpStack->Parameters.WMI.DataPath;
	wmiParams.BufferSize = IrpStack->Parameters.WMI.BufferSize;
	wmiParams.Buffer = IrpStack->Parameters.WMI.Buffer;

	switch (IrpStack->MinorFunction)
	{
	case IRP_MN_QUERY_ALL_DATA:
	case IRP_MN_QUERY_SINGLE_INSTANCE:
	case IRP_MN_CHANGE_SINGLE_INSTANCE:
	case IRP_MN_CHANGE_SINGLE_ITEM:
	case IRP_MN_ENABLE_EVENTS:
	case IRP_MN_DISABLE_EVENTS:
	case IRP_MN_ENABLE_COLLECTION:
	case IRP_MN_DISABLE_COLLECTION:
	case IRP_MN_EXECUTE_METHOD:
		status = STATUS_NOT_SUPPORTED;
		break;
	case IRP_MN_REGINFO:
		status = NdasPortPdoDispatchWmiRegister(
			PdoExtension, &wmiParams);
		break;
	default:
		//
		// Leave the IRP as it is.
		//
		status = Irp->IoStatus.Status;
		break;
	}

	//
	// As this is a pdo, we should not forward it down,
	// but to complete it as it is.
	//

	return NpReleaseRemoveLockAndCompleteIrp(
		PdoExtension->CommonExtension,
		Irp,
		status);
}

NTSTATUS
NdasPortPdoDispatchWmiRegister(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__inout PNDASPORT_WMI_PARAMETERS WmiParameters)
{
	switch ((ULONG) WmiParameters->DataPath)
	{
	case WMIREGISTER:
	case WMIUPDATE:
		return NdasPortPdoSendWmiSrb(
			PdoExtension, 
			IRP_MN_REGINFO, 
			WmiParameters);
	default:
		return STATUS_INVALID_PARAMETER;
	}
}

#define NDASPORT_WMI_TAG 'WadN'

NTSTATUS
NdasPortpSignalCompletion(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp,
    __in PVOID Context)
{
	PKEVENT event = (PKEVENT) Context;
    KeSetEvent(event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
NdasPortPdoSendWmiSrb(
	__in PNDASPORT_PDO_EXTENSION PdoExtension,
	__in UCHAR WmiMinorCode,
	__inout PNDASPORT_WMI_PARAMETERS WmiParameters)
{
	NTSTATUS status;
	SCSI_WMI_REQUEST_BLOCK srb;
	PVOID buffer;
	ULONG bufferSize;
	PIRP irp;
	PIO_STACK_LOCATION irpStack;
	PMDL mdl;
	KEVENT event;

	RtlZeroMemory(&srb, sizeof(SCSI_WMI_REQUEST_BLOCK));
	srb.PathId = PdoExtension->LogicalUnitAddress.PathId;
	srb.TargetId = PdoExtension->LogicalUnitAddress.TargetId;
	srb.Lun = PdoExtension->LogicalUnitAddress.Lun;

	if (WmiMinorCode == IRP_MN_EXECUTE_METHOD)
	{
		PWNODE_HEADER wnode = (PWNODE_HEADER) WmiParameters->Buffer;
		bufferSize = max(WmiParameters->BufferSize, wnode->BufferSize);
	} else {
		bufferSize = WmiParameters->BufferSize;
	}

	buffer = ExAllocatePoolWithTag(
		NonPagedPool, 
		bufferSize,
		NDASPORT_WMI_TAG);

	if (NULL == buffer)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlCopyMemory(
		buffer, 
		WmiParameters->Buffer, 
		bufferSize);

	srb.DataBuffer = buffer;
	srb.DataTransferLength = WmiParameters->BufferSize;
	srb.Function = SRB_FUNCTION_WMI;
	srb.Length = sizeof(SCSI_REQUEST_BLOCK);
	srb.WMISubFunction = WmiMinorCode;
	srb.DataPath = WmiParameters->DataPath;
	srb.SrbFlags = SRB_FLAGS_DATA_IN | SRB_FLAGS_NO_QUEUE_FREEZE;
	srb.TimeOutValue = 10;

	irp = IoAllocateIrp(
		PdoExtension->DeviceObject->StackSize, 
		FALSE);

	if (NULL == irp)
	{
		ExFreePoolWithTag(buffer, NDASPORT_WMI_TAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	mdl = IoAllocateMdl(
		buffer, 
		WmiParameters->BufferSize,
		FALSE,
		FALSE,
		irp);

	if (NULL == mdl)
	{
		IoFreeIrp(irp);
		ExFreePoolWithTag(buffer, NDASPORT_WMI_TAG);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	MmBuildMdlForNonPagedPool(mdl);

	srb.OriginalRequest = irp;

	irpStack = IoGetNextIrpStackLocation(irp);
	irpStack->MajorFunction = IRP_MJ_SCSI;
	// irpStack->MinorFunction = IRP_MN_SCSI_CLASS;
	irpStack->Parameters.Scsi.Srb = (PSCSI_REQUEST_BLOCK) &srb;
	
	KeInitializeEvent(
		&event, 
		NotificationEvent, 
		FALSE);

	IoSetCompletionRoutine(
		irp,
		NdasPortpSignalCompletion,
		&event,
		TRUE,
		TRUE,
		TRUE);

	//
	// Flush the data buffer for output.  This will insure that the data is
	// written back to memory.  Since the data-in flag is the the port driver
	// will flush the data again for input which will ensure the data is not
	// in the cache.
	//
	KeFlushIoBuffers(irp->MdlAddress, FALSE, TRUE);

	//
	// Call SRB dispatch routine to handle this request.
	//
	IoCallDriver(PdoExtension->DeviceObject, irp);

	KeWaitForSingleObject(
		&event,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	status = irp->IoStatus.Status;

	//
	// Relay the return buffer's size to the caller on success.
	//
	if (NT_SUCCESS(status)) 
	{
		WmiParameters->BufferSize = srb.DataTransferLength;
	}

	//
	// Copy back the correct number of bytes into the caller provided buffer.
	//
	RtlCopyMemory(
		WmiParameters->Buffer,
		buffer,
		WmiParameters->BufferSize);

	IoFreeMdl(mdl);
	IoFreeIrp(irp);

	return status;
}

#endif /* NDASPORT_IMP_SCSI_WMI */
