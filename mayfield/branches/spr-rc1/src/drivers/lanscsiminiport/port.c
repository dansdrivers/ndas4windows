
    // waiting for the CompletionDpc to be run.
    //

    struct _SRB_DATA *CompletedRequests;
    ULONG ErrorLogRetryCount;
    ULONG SequenceNumber;
    PVOID MapRegisterBase;
    ULONG NumberOfMapRegisters;

    //
    // The offset between the  data buffer for this request and the data 
    // buffer described by the MDL in the irp.
    //

    ULONG_PTR DataOffset;

    PVOID RequestSenseSave;

    //
    // These data values will be restored to the SRB when it is retried within
    // the port driver.
    // 

    ULONG OriginalDataTransferLength;

    //
    // SRB Data flags.
    //

    ULONG Flags;

    //
    // Pointer to the adapter this block was allocated from.  This is used 
    // when freeing srbdata blocks from the lookaside list back to pool.
    //

    PADAPTER_EXTENSION Adapter;

    //
    // The queue tag which was initially allocated for this srb_data block.
    // This tag will be used for any tagged srb's which are associated with 
    // this block.
    //

    ULONG QueueTag;

    //
    // Internal status value - only returned if srb->SrbStatus is set to 
    // SRBP_STATUS_INTERNAL_ERROR.
    //

    NTSTATUS InternalStatus;

    //
    // The tick count when this request was last touched.
    //

    ULONG TickCount;

    //
    // The MDL of the remapped buffer (per IoMapTransfer or GET_SCATTER_GATHER)
    //

    PMDL RemappedMdl;

    //
    // The original data buffer pointer for this request - this will be 
    // restored when the request is completed.
    //

    PVOID OriginalDataBuffer;

    //
    // Pointer to the scatter gather list for this request
    //

    PSRB_SCATTER_GATHER ScatterGatherList;

    //
    // The "small" scatter gather list for this request.  Small
    // by the constant SP_SMALL_PHYSICAL_BREAK_VALUE - small lists contain
    // this many entries or less.
    //

    SRB_SCATTER_GATHER SmallScatterGatherList[SP_SMALL_PHYSICAL_BREAK_VALUE];
};

typedef struct _LOGICAL_UNIT_BIN {
    KSPIN_LOCK Lock;
    PLOGICAL_UNIT_EXTENSION List;
} LOGICAL_UNIT_BIN, *PLOGICAL_UNIT_BIN;

//
// WMI request item, queued on a miniport request.
//

typedef struct _WMI_MINIPORT_REQUEST_ITEM {
   //
   // WnodeEventItem MUST be the first field in WMI_MINIPORT_REQUEST_ITEM, in
   // order to accommodate a copy optimization in ScsiPortCompletionDpc().
   //
   UCHAR  WnodeEventItem[WMI_MINIPORT_EVENT_ITEM_MAX_SIZE];
   UCHAR  TypeOfRequest;                                  // [Event/Reregister]
   UCHAR  PathId;                                         // [0xFF for adapter]
   UCHAR  TargetId;
   UCHAR  Lun;
   struct _WMI_MINIPORT_REQUEST_ITEM * NextRequest;
} WMI_MINIPORT_REQUEST_ITEM, *PWMI_MINIPORT_REQUEST_ITEM;

//
// WMI parameters.
//

typedef struct _WMI_PARAMETERS {
   ULONG_PTR ProviderId; // ProviderId parameter from IRP
   PVOID DataPath;      // DataPath parameter from IRP
   ULONG BufferSize;    // BufferSize parameter from IRP
   PVOID Buffer;        // Buffer parameter from IRP
} WMI_PARAMETERS, *PWMI_PARAMETERS;

//
// SpInsertFreeWmiMiniPortItem context structure.
//

typedef struct _WMI_INSERT_CONTEXT {
   PDEVICE_OBJECT             DeviceObject;                     // [FDO or PDO]
   PWMI_MINIPORT_REQUEST_ITEM ItemsToInsert;
} WMI_INSERT_CONTEXT, *PWMI_INSERT_CONTEXT;

//
// SpRemoveFreeWmiMiniPortItem context structure.
//

typedef struct _WMI_REMOVE_CONTEXT {
   PDEVICE_OBJECT             DeviceObject;                     // [FDO or PDO]
   USHORT                     NumberToRemove;
} WMI_REMOVE_CONTEXT, *PWMI_REMOVE_CONTEXT;

//
// Define data storage for access at interrupt Irql.
//

typedef struct _INTERRUPT_DATA {

    //
    // SCSI port interrupt flags
    //

    ULONG InterruptFlags;

    //
    // List head for singlely linked list of complete IRPs.
    //

    PSRB_DATA CompletedRequests;

    //
    // Adapter object transfer parameters.
    //

    ADAPTER_TRANSFER MapTransferParameters a special case.
            //

            if (srb->Function == SRB_FUNCTION_ABORT_COMMAND) {

                ASSERT(FALSE);
                logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                                      srb->PathId,
                                                      srb->TargetId,
                                                      srb->Lun,
                                                      FALSE,
                                                      FALSE);

                logicalUnit->CompletedAbort =
                    deviceExtension->InterruptData.CompletedAbort;

                deviceExtension->InterruptData.CompletedAbort = logicalUnit;

            } else {

                //
                // Validate the srb data.
                //

                srbData = srb->OriginalRequest;

#if DBG
                ASSERT_SRB_DATA(srbData);

                ASSERT(srbData->CurrentSrb == srb);

                ASSERT(srbData->CurrentSrb != NULL &&
                       srbData->CompletedRequests == NULL);

                if ((srb->SrbStatus == SRB_STATUS_SUCCESS) &&
                    ((srb->Cdb[0] == SCSIOP_READ) ||
                     (srb->Cdb[0] == SCSIOP_WRITE))) {
                    ASSERT(srb->DataTransferLength);
                }
#endif

                if(((srb->SrbStatus == SRB_STATUS_SUCCESS) ||
                    (srb->SrbStatus == SRB_STATUS_DATA_OVERRUN)) &&
                   (TEST_FLAG(srb->SrbFlags, SRB_FLAGS_UNSPECIFIED_DIRECTION))) {
                    ASSERT(srbData->OriginalDataTransferLength >=
                           srb->DataTransferLength);
                }

                srbData->CompletedRequests =
                    deviceExtension->InterruptData.CompletedRequests;
                deviceExtension->InterruptData.CompletedRequests = srbData;

                //
                // Cache away the last logical unit we touched in the miniport.
                // This is cleared when we come out of the miniport
                // synchronization but provides a shortcut for finding the
                // logical unit before going into the hash table.
                //

                deviceExtension->CachedLogicalUnit = srbData->LogicalUnit;
            }

            break;

        case ResetDetected:

            //
            // Notifiy the port driver that a reset has been reported.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_RESET_REPORTED | PD_RESET_HOLD;
            break;

        case NextLuRequest:

            //
            // The miniport driver is ready for the next request and
            // can accept a request for this logical unit.
            //

            pathId = va_arg(ap, UCHAR);
            targetId = va_arg(ap, UCHAR);
            lun = va_arg(ap, UCHAR);

            //
            // A next request is impiled by this notification so set the
            // ready for next reqeust flag.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_READY_FOR_NEXT_REQUEST;

            logicalUnit = deviceExtension->CachedLogicalUnit;

            if((logicalUnit == NULL) ||
               (logicalUnit->TargetId != targetId) ||
               (logicalUnit->PathId != pathId) ||
               (logicalUnit->Lun != lun)) {

                logicalUnit = GetLogicalUnitExtension(deviceExtension,
                                                      pathId,
                                                      targetId,
                                                      lun,
                                                      FALSE,
                                                      FALSE);
            }

            if (logicalUnit != NULL && logicalUnit->ReadyLogicalUnit != NULL) {

                //
                // Since our ReadyLogicalUnit link field is not NULL we must
                // have already been linked onto a ReadyLogicalUnit list.
                // There is nothing to do.
                //

                break;
            }

            //
            // Don't process this as request for the next logical unit, if
            // there is a untagged request for active for this logical unit.
            // The logical unit will be started when untagged request completes.
            //

            if (logicalUnit->CurrentUntaggedRequest == NULL) {

                //
                // Add the logical unit to the chain of logical units that
                // another request maybe processed for.
                //

                logicalUnit->ReadyLogicalUnit =
                    deviceExtension->InterruptData.ReadyLogicalUnit;
                deviceExtension->InterruptData.ReadyLogicalUnit = logicalUnit;
            }

            break;

        case CallDisableInterrupts:

            ASSERT(deviceExtension->InterruptData.InterruptFlags &
                   PD_DISABLE_INTERRUPTS);

            //
            // The miniport wants us to call the specified routine
            // with interrupts disabled.  This is done after the current
            // HwRequestInterrutp routine completes. Indicate the call is
            // needed and save the routine to be called.
            //

            deviceExtension->Flags |= PD_DISABLE_CALL_REQUEST;

            deviceExtension->HwRequestInterrupt = va_arg(ap, PHW_INTERRUPT);

            break;

        case CallEnableInterrupts:

            //
            // The miniport wants us to call the specified routine
            // with interrupts enabled this is done from the DPC.
            // Disable calls to the interrupt routine, indicate the call is
            // needed and save the routine to be called.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_DISABLE_INTERRUPTS | PD_ENABLE_CALL_REQUEST;

            deviceExtension->HwRequestInterrupt = va_arg(ap, PHW_INTERRUPT);

            break;

        case RequestTimerCall:

            //
            // The driver wants to set the miniport timer.
            // Save the timer parameters.
            //

            deviceExtension->InterruptData.InterruptFlags |=
                PD_TIMER_CALL_REQUEST;
            deviceExtension->InterruptData.HwTimerRequest =
                va_arg(ap, PHW_INTERRUPT);
            deviceExtension->InterruptData.MiniportTimerValue =
                va_arg(ap, ULONG);
            break;

        case WMIEvent: {

            //
            // The miniport wishes to post a WMI event for the adapter
            // or a specified SCSI target.
            //

            PWMI_MINIPORT_REQUEST_ITEM lastMiniPortRequest;
            PWMI_MINIPORT_REQUEST_ITEM wmiMiniPortRequest;
            PWNODE_EVENT_ITEM          wnodeEventItem;
            PWNODE_EVENT_ITEM          wnodeEventItemCopy;

            wnodeEventItem     = va_arg(ap, PWNODE_EVENT_ITEM);
            pathId             = va_arg(ap, UCHAR);

            if (pathId != 0xFF) {
                targetId = va_arg(ap, UCHAR);
                lun      = va_arg(ap, UCHAR);
            }

            //
            // Validate the event first.  Then attempt to obtain a free
            // WMI_MINIPORT_REQUEST_ITEM structure so that we may store
            // this request and process it at DPC level later.  If none
            // are obtained or the event is bad, we ignore the request.
            //

            if ((wnodeEventItem == NULL) ||
                (wnodeEventItem->WnodeHeader.BufferSize >
                 WMI_MINIPORT_EVENT_ITEM_MAX_SIZE)) {

                va_end(ap);    //  size, no free WMI_MINIPORT_REQUEST_ITEMs left]
                return;
            }

            //
            // Remove the WMI_MINIPORT_REQUEST_ITEM from the free list.
            //
            wmiMiniPortRequest = SpWmiPopFreeRequestItem(deviceExtension);

            //
            // Log an error if a free request item could not be dequeued
            // (log only once in the lifetime of this adapter).
            //
            if (wmiMiniPortRequest == NULL) {

                if (!deviceExtension->WmiFreeMiniPortRequestsExhausted) {
                    deviceExtension->WmiFreeMiniPortRequestsExhausted = TRUE;
                    ScsiPortLogError(HwDeviceExtension,
                                         NULL,
                                         pathId,
                                         targetId,
                                         lun,
                                         SP_LOST_WMI_MINIPORT_REQUEST,
                                         0);
                }

                va_end(ap);
                return;
            }

            //
            // Save information pertaining to this WMI request for later
            // processing.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_WMI_REQUEST;

            wmiMiniPortRequest->TypeOfRequest = (UCHAR)WMIEvent;
            wmiMiniPortRequest->PathId        = pathId;
            wmiMiniPortRequest->TargetId      = targetId;
            wmiMiniPortRequest->Lun           = lun;

            RtlCopyMemory(wmiMiniPortRequest->WnodeEventItem,
                          wnodeEventItem,
                          wnodeEventItem->WnodeHeader.BufferSize);

            //
            // Queue the new WMI_MINIPORT_REQUEST_ITEM to the end of list in the
            // interrupt data structure.
            //
            wmiMiniPortRequest->NextRequest = NULL;

            lastMiniPortRequest =
                deviceExtension->InterruptData.WmiMiniPortRequests;

            if (lastMiniPortRequest) {

                while (lastMiniPortRequest->NextRequest) {
                    lastMiniPortRequest = lastMiniPortRequest->NextRequest;
                }
                lastMiniPortRequest->NextRequest = wmiMiniPortRequest;

            } else {
                deviceExtension->InterruptData.WmiMiniPortRequests =
                    wmiMiniPortRequest;
            }

            break;
        }

        case WMIReregister: {
            //
            // The miniport wishes to re-register the GUIDs for the adapter or
            // a specified SCSI target.
            //

            PWMI_MINIPORT_REQUEST_ITEM lastMiniPortRequest;
            PWMI_MINIPORT_REQUEST_ITEM wmiMiniPortRequest;

            pathId             = va_arg(ap, UCHAR);

            if (pathId != 0xFF) {
                targetId = va_arg(ap, UCHAR);
                lun      = va_arg(ap, UCHAR);
            }

            //
            // Attempt to obtain a free WMI_MINIPORT_REQUEST_ITEM structure
            // so that we may store this request and process it at DPC
            // level later. If none are obtained or the event is bad, we
            // ignore the request.
            //
            // Remove a WMI_MINPORT_REQUEST_ITEM from the free list.
            //
            wmiMiniPortRequest = SpWmiPopFreeRequestItem(deviceExtension);

            if (wmiMiniPortRequest == NULL) {

                //
                // Log an error if a free request item could not be dequeued
                // (log only once in the lifetime of this adapter).
                //
                if (!deviceExtension->WmiFreeMiniPortRequestsExhausted) {

                    deviceExtension->WmiFreeMiniPortRequestsExhausted = TRUE;

                    ScsiPortLogError(HwDeviceExtension,
                                     NULL,
                                     pathId,
                                     targetId,
                                     lun,
                                     SP_LOST_WMI_MINIPORT_REQUEST,
                                     0);
                 }

                va_end(ap);
                return;
            }

            //
            // Save information pertaining to this WMI request for later
            // processing.
            //

            deviceExtension->InterruptData.InterruptFlags |= PD_WMI_REQUEST;
            wmiMiniPortRequest->TypeOfRequest = (UCHAR)WMIReregister;
            wmiMiniPortRequest->PathId        = pathId;
            wmiMiniPortRequest->TargetId      = targetId;
            wmiMiniPortRequest->Lun           = lun;

            //
            // Queue the new WMI_MINIPORT_REQUEST_ITEM to the end of list in the
            // interrupt data structure.
            //
            wmiMiniPortRequest->NextRequest = NULL;

            lastMiniPortRequest =
                deviceExtension->InterruptData.WmiMiniPortRequests;

            if (lastMiniPortRequest) {

                while (lastMiniPortRequest->NextRequest) {
                    lastMiniPortRequest = lastMiniPortRequest->NextRequest;
                }
                lastMiniPortRequest->NextRequest = wmiMiniPortRequest;

            } else {
                deviceExtension->InterruptData.WmiMiniPortRequests =
                    wmiMiniPortRequest;
            }

            break;
        }

        case BusChangeDetected: {

            SET_FLAG(deviceExtension->InterruptData.InterruptFlags,
                     PD_BUS_CHANGE_DETECTED);
            break;
        }

        default: {
             ASSERT(0);
             break;
        }
    }

    va_end(ap);

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

} // end ScsiPortNotification()


VOID
ScsiPortFlushDma(
    IN PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine checks to see if the perivious IoMapTransfer has been done
    started.  If it has not, then the PD_MAP_TRANSER flag is cleared, and the
    routine returns; otherwise, this routine schedules a DPC which will call
    IoFlushAdapter buffers.

Arguments:

    HwDeviceExtension - Supplies a the hardware device extension for the
        host bus adapter which will be doing the data transfer.


Return Value:

    None.

--*/

{

    PADAPTER_EXTENSION deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);

    if(Sp64BitPhysicalAddresses) {
        KeBugCheckEx(PORT_DRIVER_INTERNAL, 
                     0,
                     STATUS_NOT_SUPPORTED,
                     (ULONG_PTR) HwDeviceExtension,
                     (ULONG_PTR) deviceExtension->DeviceObject->DriverObject);
    }

    if (deviceExtension->InterruptData.InterruptFlags & PD_MAP_TRANSFER) {

        //
        // The transfer has not been started so just clear the map transfer
        // flag and return.
        //

        deviceExtension->InterruptData.InterruptFlags &= ~PD_MAP_TRANSFER;
        return;
    }

    deviceExtension->InterruptData.InterruptFlags |= PD_FLUSH_ADAPTER_BUFFERS;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

    return;

}

VOID
ScsiPortIoMapTransfer(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb,
    IN PVOID LogicalAddress,
    IN ULONG Length
    )
/*++

Routine Description:

    Saves the parameters for the call to IoMapTransfer and schedules the DPC
    if necessary.

Arguments:

    HwDeviceExtension - Supplies a the hardware device extension for the
        host bus adapter which will be doing the data transfer.

    Srb - Supplies the particular request that data transfer is for.

    LogicalAddress - Supplies the logical address where the transfer should
        begin.

    Length - Supplies the maximum length in bytes of the transfer.

Return Value:

   None.

--*/

{
    PADAPTER_EXTENSION deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);
    PSRB_DATA srbData = Srb->OriginalRequest;

    ASSERT_SRB_DATA(srbData);

    //
    // If this is a 64-bit system then this call is illegal.  Bugcheck.
    //

    if(Sp64BitPhysicalAddresses) {
        KeBugCheckEx(PORT_DRIVER_INTERNAL, 
                     1,
                     STATUS_NOT_SUPPORTED,
                     (ULONG_PTR) HwDeviceExtension,
                     (ULONG_PTR) deviceExtension->DeviceObject->DriverObject);
    }

    //
    // Make sure this host bus adapter has an Dma adapter object.
    //

    if (deviceExtension->DmaAdapterObject == NULL) {

        //
        // No DMA adapter, no work.
        //

        return;
    }

    ASSERT((Srb->SrbFlags & SRB_FLAGS_UNSPECIFIED_DIRECTION) != SRB_FLAGS_UNSPECIFIED_DIRECTION);

    deviceExtension->InterruptData.MapTransferParameters.SrbData = srbData;

    deviceExtension->InterruptData.MapTransferParameters.LogicalAddress = LogicalAddress;
    deviceExtension->InterruptData.MapTransferParameters.Length = Length;
    deviceExtension->InterruptData.MapTransferParameters.SrbFlags = Srb->SrbFlags;

    deviceExtension->InterruptData.InterruptFlags |= PD_MAP_TRANSFER;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

} // end ScsiPortIoMapTransfer()


VOID
ScsiPortLogError(
    IN PVOID HwDeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb OPTIONAL,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN ULONG ErrorCode,
    IN ULONG UniqueId
    )

/*++

Routine Description:

    This routine saves the error log information, and queues a DPC if necessary.

Arguments:

    HwDeviceExtension - Supplies the HBA miniport driver's adapter data storage.

    Srb - Supplies an optional pointer to srb if there is one.

    TargetId, Lun and PathId - specify device address on a SCSI bus.

    ErrorCode - Supplies an error code indicating the type of error.

    UniqueId - Supplies a unique identifier for the error.

Return Value:

    None.

--*/

{
    PADAPTER_EXTENSION deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);
    PDEVICE_OBJECT DeviceObject = deviceExtension->CommonExtension.DeviceObject;
    PSRB_DATA srbData;
    PERROR_LOG_ENTRY errorLogEntry;

    //
    // If the error log entry is already full, then dump the error.
    //

    if (deviceExtension->InterruptData.InterruptFlags & PD_LOG_ERROR) {

#if SCSIDBG_ENABLED
        DebugPrint((1,"ScsiPortLogError: Dumping scsi error log packet.\n"));
        DebugPrint((1,
            "PathId = %2x, TargetId = %2x, Lun = %2x, ErrorCode = %x, UniqueId = %x.",
            PathId,
            TargetId,
            Lun,
            ErrorCode,
            UniqueId
            ));
#endif
        return;
    }

    //
    // Save the error log data in the log entry.
    //

    errorLogEntry = &deviceExtension->InterruptData.LogEntry;

    errorLogEntry->ErrorCode = ErrorCode;
    errorLogEntry->TargetId = TargetId;
    errorLogEntry->Lun = Lun;
    errorLogEntry->PathId = PathId;
    errorLogEntry->UniqueId = UniqueId;

    //
    // Get the sequence number from the SRB data.
    //

    if (Srb != NULL) {

        srbData = Srb->OriginalRequest;

        ASSERT_SRB_DATA(srbData);

        errorLogEntry->SequenceNumber = srbData->SequenceNumber;
        errorLogEntry->ErrorLogRetryCount = srbData->ErrorLogRetryCount++;
    } else {
        errorLogEntry->SequenceNumber = 0;
        errorLogEntry->ErrorLogRetryCount = 0;
    }

    //
    // Indicate that the error log entry is in use.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_LOG_ERROR;

    //
    // Request a DPC be queued after the interrupt completes.
    //

    deviceExtension->InterruptData.InterruptFlags |= PD_NOTIFICATION_REQUIRED;

    return;

} // end ScsiPortLogError()


VOID
ScsiPortCompleteRequest(
    IN PVOID HwDeviceExtension,
    IN UCHAR PathId,
    IN UCHAR TargetId,
    IN UCHAR Lun,
    IN UCHAR SrbStatus
    )

/*++

Routine Description:

    Complete all active requests for the specified logical unit.

Arguments:

    DeviceExtenson - Supplies the HBA miniport driver's adapter data storage.

    TargetId, Lun and PathId - specify device address on a SCSI bus.

    SrbStatus - Status to be returned in each completed SRB.

Return Value:

    None.

--*/

{
    PADAPTER_EXTENSION deviceExtension = GET_FDO_EXTENSION(HwDeviceExtension);
    ULONG binNumber;

    for (binNumber = 0; binNumber < NUMBER_LOGICAL_UNIT_BINS; binNumber++) {

        PLOGICAL_UNIT_BIN bin = &deviceExtension->LogicalUnitList[binNumber];
        PLOGICAL_UNIT_EXTENSION logicalUnit;
        ULONG limit = 0;

        logicalUnit = bin->List;

        DebugPrint((2, "ScsiPortCompleteRequest: Completing requests in "
                       "bin %d [%#p]\n",
                    binNumber, bin));

        for(logicalUnit = bin->List;
            logicalUnit != NULL;
            logicalUnit = logicalUnit->NextLogicalUnit) {

            PLIST_ENTRY entry;

            ASSERT(limit++ < 1000);

            //
            // See if this logical unit matches the pattern.  Check for -1
            // first since this seems to be the most popular way to complete
            // requests.
            //

            if (((PathId == SP_UNTAGGED) || (PathId == logicalUnit->PathId)) &&
                ((TargetId == SP_UNTAGGED) ||
                 (TargetId == logicalUnit->TargetId)) &&
                ((Lun == SP_UNTAGGED) || (Lun == logicalUnit->Lun))) {

                //
                // Complete any pending abort reqeusts.
                //

                if (logicalUnit->AbortSrb != NULL) {
                    logicalUnit->AbortSrb->SrbStatus = SrbStatus;

                    ScsiPortNotification(
                        RequestComplete,
                        HwDeviceExtension,
                        logicalUnit->AbortSrb
                        );
                }

                if(logicalUnit->CurrentUntaggedRequest != NULL) {

                    SpCompleteSrb(deviceExtension,
                                  logicalUnit->CurrentUntaggedRequest,
                                  SrbStatus);
                }

                //
                // Complete each of the requests in the queue.
                //

                entry = logicalUnit->RequestList.Flink;
                while (entry != &logicalUnit->RequestList) {
                    PSRB_DATA srbData;

                    ASSERT(limit++ < 1000);
                    srbData = CONTAINING_RECORD(entry, SRB_DATA, RequestList);
                    SpCompleteSrb(deviceExtension,  srbData, SrbStatus);
                    entry = srbData->RequestList.Flink;
                }

            }
        }
    }

    return;

} // end ScsiPortCompleteRequest()


VOID
ScsiPortMoveMemory(
    IN PVOID WriteBuffer,
    IN PVOID ReadBuffer,
    IN ULONG Length
    )

/*++

Routine Description:

    Copy from one buffer into another.

Arguments:

    ReadBuffer - source
    WriteBuffer - destination
    Length - number of bytes to copy

Return Value:

    None.

--*/

{

    //
    // See if the length, source and desitination are word aligned.
    //

    if (Length & LONG_ALIGN || (ULONG_PTR) WriteBuffer & LONG_ALIGN ||
        (ULONG_PTR) ReadBuffer & LONG_ALIGN) {

        PCHAR destination = WriteBuffer;
        PCHAR source = ReadBuffer;

        for (; Length > 0; Length--) {
            *destination++ = *source++;
        }
    } else {

        PLONG destination = WriteBuffer;
        PLONG source = ReadBuffer;

        Length /= sizeof(LONG);
        for (; Length > 0; Length--) {
            *destination++ = *source++;
        }
    }

} // end ScsiPortMoveMemory()


#if SCSIDBG_ENABLED

VOID
ScsiDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for scsi miniports.

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= ScsiDebug) {

        _vsnprintf(ScsiBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);

        ASSERTMSG("ScsiDebugPrint overwrote sentinal byte",
                  ((ScsiBufferSentinal == NULL) || (*ScsiBufferSentinal == 0xff)));

        if(ScsiBufferSentinal) {
            *ScsiBufferSentinal = 0xff;
        }

        DbgPrint(ScsiBuffer);
    }

    va_end(ap);

} // end ScsiDebugPrint()


VOID
SpDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )

/*++

Routine Description:

    Debug print for scsiport internal use.

Arguments:

    Debug print level between 0 and 3, with 3 being the most verbose.

Return Value:

    None

--*/

{
    va_list ap;

    va_start(ap, DebugMessage);

    if (DebugPrintLevel <= SpDebug) {

        _vsnprintf(SpBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);

        ASSERTMSG("SpDebugPrint overwrote sentinal byte",
                  ((SpBufferSentinal == NULL) || (*SpBufferSentinal == 0xff)));

        if(ScsiBufferSentinal) {
            *SpBufferSentinal = 0xff;
        }

        DbgPrint(SpBuffer);
    }

    va_end(ap);

} // end ScsiDebugPrint()

#else

//
// ScsiDebugPrint stub
//

VOID
ScsiDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )
{
}

VOID
SpDebugPrint(
    ULONG DebugPrintLevel,
    PCCHAR DebugMessage,
    ...
    )
{
}

#endif

//
// The below I/O access routines are forwarded to the HAL or NTOSKRNL on
// Alpha and Intel platforms.
//
#if !defined(_ALPHA_) && !defined(_X86_)

UCHAR
ScsiPortReadPortUchar(
    IN PUCHAR Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_UCHAR(Port));

}

USHORT
ScsiPortReadPortUshort(
    IN PUSHORT Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_USHORT(Port));

}

ULONG
ScsiPortReadPortUlong(
    IN PULONG Port
    )

/*++

Routine Description:

    Read from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.

Return Value:

    Returns the value read from the specified port address.

--*/

{

    return(READ_PORT_ULONG(Port));

}

VOID
ScsiPortReadPortBufferUchar(
    IN PUCHAR Port,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Read a buffer of unsigned bytes from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_UCHAR(Port, Buffer, Count);

}

VOID
ScsiPortReadPortBufferUshort(
    IN PUSHORT Port,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned shorts from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_USHORT(Port, Buffer, Count);

}

VOID
ScsiPortReadPortBufferUlong(
    IN PULONG Port,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned longs from the specified port address.

Arguments:

    Port - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_PORT_BUFFER_ULONG(Port, Buffer, Count);

}

UCHAR
ScsiPortReadRegisterUchar(
    IN PUCHAR Register
    )

/*++

Routine Description:

    Read from the specificed register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_UCHAR(Register));

}

USHORT
ScsiPortReadRegisterUshort(
    IN PUSHORT Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_USHORT(Register));

}

ULONG
ScsiPortReadRegisterUlong(
    IN PULONG Register
    )

/*++

Routine Description:

    Read from the specified register address.

Arguments:

    Register - Supplies a pointer to the register address.

Return Value:

    Returns the value read from the specified register address.

--*/

{

    return(READ_REGISTER_ULONG(Register));

}

VOID
ScsiPortReadRegisterBufferUchar(
    IN PUCHAR Register,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Read a buffer of unsigned bytes from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_UCHAR(Register, Buffer, Count);

}

VOID
ScsiPortReadRegisterBufferUshort(
    IN PUSHORT Register,
    IN PUSHORT Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned shorts from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_USHORT(Register, Buffer, Count);

}

VOID
ScsiPortReadRegisterBufferUlong(
    IN PULONG Register,
    IN PULONG Buffer,
    IN ULONG Count
    )

/*++

Routine Description:

    Read a buffer of unsigned longs from the specified register address.

Arguments:

    Register - Supplies a pointer to the port address.
    Buffer - Supplies a pointer to the data buffer area.
    Count - The count of items to move.

Return Value:

    None

--*/

{

    READ_REGISTER_BUFFER_ULONG(Register, Buffer, Count);

}

VOID
ScsiPortWritePortUchar(
    IN PUCHAR Port,
    IN UCHAR Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_UCHAR(Port, Value);

}

VOID
ScsiPortWritePortUshort(
    IN PUSHORT Port,
    IN USHORT Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_USHORT(Port, Value);

}

VOID
ScsiPortWritePortUlong(
    IN PULONG Port,
    IN ULONG Value
    )

/*++

Routine Description:

    Write to the specificed port address.

Arguments:

    Port - Supplies a pointer to the port address.

    Value - Supplies the value to be written.

Return Value:

    None

--*/

{

    WRITE_PORT_ULONG(Port, Value);


}

VOID
ScsiPortWritePortBufferUchar(
    IN PUCHAR Port,
    IN PUCHAR Buffer,
    IN ULONG  Count
    )

/*++

Routine Description:

    Write a buffer of unsigned bytes from the specified port address.

// stdafx.cpp : source file that includes just the standard includes
//	SnGen2.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"



                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                Microsoft Visual Studio Solution File, Format Version 8.00
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "_ndas_inc", "..\inc\_ndas_inc.vcproj", "{2B926D88-5292-4897-B0CD-DD149FF042D8}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndassvc", "ndassvc\service\ndassvc.vcproj", "{C6D8A7C9-6511-4AA9-BF25-196B24837744}"
	ProjectSection(ProjectDependencies) = postProject
		{14A56611-0DF9-4B1F-BCF8-BA0617D33DF8} = {14A56611-0DF9-4B1F-BCF8-BA0617D33DF8}
		{D992EE21-01E6-42C4-9FB3-5B2B87214E57} = {D992EE21-01E6-42C4-9FB3-5B2B87214E57}
		{F16012BE-0D33-4D27-A16F-AB931D4259D0} = {F16012BE-0D33-4D27-A16F-AB931D4259D0}
		{E5D495C4-C03A-4E00-BD7B-58C9AFEFB990} = {E5D495C4-C03A-4E00-BD7B-58C9AFEFB990}
		{FDDA59D9-1C02-4BC6-BDC0-53E1CBF689A6} = {FDDA59D9-1C02-4BC6-BDC0-53E1CBF689A6}
		{0226EDF1-A8DB-4DDB-971E-CD7F5E7C529F} = {0226EDF1-A8DB-4DDB-971E-CD7F5E7C529F}
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndassvc_support", "ndassvc\support\ndassup.vcproj", "{FDDA59D9-1C02-4BC6-BDC0-53E1CBF689A6}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndassvc_lpxtrans", "ndassvc\lpxtrans\lpxtrans.vcproj", "{E5D495C4-C03A-4E00-BD7B-58C9AFEFB990}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndassvc_inc", "ndassvc\inc\_ndassvc_inc.vcproj", "{3E9F44CA-55B1-42D9-BE64-8AB66DC4271F}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasmsg", "ndasmsg\ndasmsg.vcproj", "{E4DFEA97-69A2-4A82-B725-B2EC808FA981}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasuser", "ndasuser\ndasuser.vcproj", "{40723613-D937-4460-BF18-06C0E12542C3}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "lanscsiop", "lanscsiop\lanscsiop.vcproj", "{0226EDF1-A8DB-4DDB-971E-CD7F5E7C529F}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "lfsfiltctl", "lfsfiltctl\lfsfiltctl.vcproj", "{F16012BE-0D33-4D27-A16F-AB931D4259D0}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "lsbusctl", "lsbusctl\lsbusctl.vcproj", "{D992EE21-01E6-42C4-9FB3-5B2B87214E57}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "rofiltctl", "rofiltctl\rofiltctl.vcproj", "{14A56611-0DF9-4B1F-BCF8-BA0617D33DF8}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasmgmt", "ndasmgmt\program\ndasmgmt.vcproj", "{F42DAA8A-2A43-4F8A-818D-CFDB1AEB7C48}"
	ProjectSection(ProjectDependencies) = postProject
		{40723613-D937-4460-BF18-06C0E12542C3} = {40723613-D937-4460-BF18-06C0E12542C3}
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasmgmt_resource", "ndasmgmt\resource\ndasmgmtres.vcproj", "{6D2AEFEE-3694-4291-AA9B-84DA01A45EC7}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasmgmt_inc", "ndasmgmt\inc\_ndasmgmt_inc.vcproj", "{188F01A2-6302-44D1-A401-2A1B50E6A2EC}"
	ProjectSection(ProjectDependencies) = postProject
	EndProjectSection
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndascmd", "ndascmd\ndascmd.vcproj", "{D7DAFC7F-3711-4FD8-A2BE-FF7E560CB89D}"
	ProjectSection(ProjectDependencies) = postProject
		{40723613-D937-4460-BF18-06C0E12542C3} = {40723613-D937-4460-BF18-06C0E12542C3}
	EndProjectSection
EndProject
Project("{¨&úJ    5   í  water256.bmp.svn-base          `rð    ¨&úJ    5   í  toolbar.bmp.svn-base           `rð    ¨&úJ    5   í  taskbar.ico.svn-base           `rð    ¨&úJ    5   í  sdmedia.ico.svn-base           `rð    ¨&úJ    5   í  proptree.bmp.svn-base          `rð    ¨&úJ    5   í  ndasmgmt.ico.svn-base          `rð    ¨&úJ    5   í  dvddrive.ico.svn-base          `rð    ¨&úJ    5   í  diskdrive.ico.svn-base         `rð    ¨&úJ    5   í  cfmedia.ico.svn-base           `rð    ¨&úJ    5   í  cdmedia.ico.svn-base           `rð    ¨&úJ    5   í  cddrive.ico.svn-base           `rð    ¨&úJ    5   í  banner256.bmp.svn-base         `rð    ¨&úJ    5   í  alert16.ico.svn-base           `rð    ¨&úJ    5   í  aboutheader.jpg.svn-base            
   ð    ¨&úJ    Ê	  í  entries     
   ð    ¨&úJ    ™   í  dir-prop-base       
   ð    ¨&úJ    n	  í  all-wcprops         	    Ü    ¨&úJ    
  í  water256.bmp        	    Ü    ¨&úJ    6  í  toolbar.bmp         	    Ü    ¨&úJ    ~b  í  taskbar.ico         	    Ü    ¨&úJ    ÖW  í  sdmedia.ico         	    Ü    ¨&úJ    x	  í  proptree.bmp        	    Ü    ¨&úJ    ~b  í  ndasmgmt.ico        	    Ü    ¨&úJ    G  í  dvddrive.ico        	    Ü    ¨&úJ    NK  í  diskdrive.ico       	    Ü    ¨&úJ    ÖW  í  cfmedia.ico         	    Ü    ¨&úJ    \  í  cdmedia.ico         	    Ü    ¨&úJ    ~^  í  cddrive.ico         	    Ü    ¨&úJ    ,  í  banner256.bmp       	    Ü    ¨&úJ    6  í  alert16.ico         	    Ü    ¨&úJ    –q  í  aboutheader.jpg        0.    ¨&úJ      í  sources.svn-base               0.    ¨&úJ    ü   í  makefile.svn-base           
    ‹ð    ¨&úJ      í  entries     
    ‹ð    ¨&úJ    ¼  í  all-wcprops         	    ð    ¨&úJ      í  sources     	    ð    ¨&úJ    ü   í  makefile               Àõï    ¨&úJ      í  sources.svn-base               Àõï    ¨&úJ    ü   í  makefile.svn-base           
   Äï    ¨&úJ      í  entries     
   Äï    ¨&úJ    ¼  í  all-wcprops         	   0ð    ¨&úJ      í  sources     	   0ð    ¨&úJ    ü   í  makefile               Pmð    ¨&úJ      í  sources.svn-base               Pmð    ¨&úJ    ü   í  makefile.svn-base           
    ðï    ¨&úJ      í  entries     
    ðï    ¨&úJ    ¼  í  all-wcprops         	   Ðð    ¨&úJ      í  sources     	   Ðð    ¨&úJ    ü   í  makefile               Kï    ¨&úJ      í  sources.svn-base               Kï    ¨&úJ    ü   í  makefile.svn-base           
   Ð    