
ULONG
LspIdeMapError(
        IN PVOID HwDeviceExtension,
        IN PSCSI_REQUEST_BLOCK Srb
        )

/*++

Routine Description:

    This routine maps ATAPI and IDE errors to specific SRB statuses.

Arguments:

    HwDeviceExtension - HBA miniport driver's adapter data storage
    Srb - IO request packet

Return Value:

    SRB status

--*/

{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PIDE_REGISTERS_1     baseIoAddress1  = &deviceExtension->BaseIoAddress1;
    PIDE_REGISTERS_2     baseIoAddress2  = &deviceExtension->BaseIoAddress2;
    ULONG i;
    UCHAR errorByte;
    UCHAR srbStatus;
    UCHAR scsiStatus;
    SENSE_DATA  tempSenseBuffer;
    PSENSE_DATA  senseBuffer = (PSENSE_DATA)&tempSenseBuffer;

    //
    // Read the error register.
    //

    //errorByte = IdePortInPortByte(baseIoAddress1->Error);
    GetErrorByte(baseIoAddress1, errorByte);

    DebugPrint((DBG_IDE_DEVICE_ERROR,
                "MapError: cdb %x and Error register is %x\n",
                Srb->Cdb[0],
                errorByte));

    if (deviceExtension->DeviceFlags[Srb->TargetId] & DFLAGS_ATAPI_DEVICE) {

        switch (errorByte >> 4) {
        case SCSI_SENSE_NO_SENSE:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: No sense information\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_RECOVERED_ERROR:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Recovered error\n"));
            scsiStatus = 0;
            srbStatus = SRB_STATUS_SUCCESS;
            break;

        case SCSI_SENSE_NOT_READY:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Device not ready\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_MEDIUM_ERROR:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Media error\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_HARDWARE_ERROR:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Hardware error\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_ILLEGAL_REQUEST:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Illegal request\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_UNIT_ATTENTION:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Unit attention\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_DATA_PROTECT:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Data protect\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_BLANK_CHECK:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Blank check\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        case SCSI_SENSE_ABORTED_COMMAND:
            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "Atapi: Command Aborted\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;

        default:

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "ATAPI: Invalid sense information\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
            break;
        }

    } else {

        scsiStatus = 0;
        //
        // Save errorByte,to be used by SCSIOP_REQUEST_SENSE.
        //

        deviceExtension->ReturningMediaStatus = errorByte;

        RtlZeroMemory(senseBuffer, sizeof(SENSE_DATA));

        if (errorByte & IDE_ERROR_MEDIA_CHANGE_REQ) {
            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Media change\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;

            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_UNIT_ATTENTION;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_OPERATOR_REQUEST;
                senseBuffer->AdditionalSenseCodeQualifier = SCSI_SENSEQ_MEDIUM_REMOVAL;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

        } else if (errorByte & IDE_ERROR_COMMAND_ABORTED) {

            DebugPrint((DBG_IDE_DEVICE_ERROR, "IDE: Command abort\n"));

            scsiStatus = SCSISTAT_CHECK_CONDITION;
            if ((errorByte & IDE_ERROR_CRC_ERROR) &&
                (deviceExtension->DeviceFlags[Srb->TargetId] & DFLAGS_USE_UDMA) &&
                SRB_USES_DMA(Srb)) {

                DebugPrint((1, "Srb 0x%x had a CRC error using UDMA\n", Srb));

                srbStatus = SRB_STATUS_PARITY_ERROR;

                if (Srb->SenseInfoBuffer) {

                    senseBuffer->ErrorCode = 0x70;
                    senseBuffer->Valid     = 1;
                    senseBuffer->AdditionalSenseLength = 0xb;
                    senseBuffer->SenseKey =  SCSI_SENSE_HARDWARE_ERROR;
                    senseBuffer->AdditionalSenseCode = 0x8;
                    senseBuffer->AdditionalSenseCodeQualifier = 0x3;

                    srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
                }

            } else {

                srbStatus = SRB_STATUS_ABORTED;

                if (Srb->SenseInfoBuffer) {

                    senseBuffer->ErrorCode = 0x70;
                    senseBuffer->Valid     = 1;
                    senseBuffer->AdditionalSenseLength = 0xb;
                    senseBuffer->SenseKey =  SCSI_SENSE_ABORTED_COMMAND;
                    senseBuffer->AdditionalSenseCode = 0;
                    senseBuffer->AdditionalSenseCodeQualifier = 0;

                    srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
                }
            }

            deviceExtension->ErrorCount++;

        } else if (errorByte & IDE_ERROR_END_OF_MEDIA) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: End of media\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;

            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_NOT_READY;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_NO_MEDIA_IN_DEVICE;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }
            if (!(deviceExtension->DeviceFlags[Srb->TargetId] & DFLAGS_MEDIA_STATUS_ENABLED)) {
                deviceExtension->ErrorCount++;
            }

        } else if (errorByte & IDE_ERROR_ILLEGAL_LENGTH) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Illegal length\n"));
            srbStatus = SRB_STATUS_INVALID_REQUEST;

        } else if (errorByte & IDE_ERROR_BAD_BLOCK) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Bad block\n"));
            srbStatus = SRB_STATUS_ERROR;
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_HARDWARE_ERROR;
                senseBuffer->AdditionalSenseCode = 0;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

        } else if (errorByte & IDE_ERROR_ID_NOT_FOUND) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Id not found\n"));
            srbStatus = SRB_STATUS_ERROR;
            scsiStatus = SCSISTAT_CHECK_CONDITION;

            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_ILLEGAL_REQUEST;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_ILLEGAL_BLOCK;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

            deviceExtension->ErrorCount++;

        } else if (errorByte & IDE_ERROR_MEDIA_CHANGE) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Media change\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;

            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey =  SCSI_SENSE_UNIT_ATTENTION;
                senseBuffer->AdditionalSenseCode = SCSI_ADSENSE_MEDIUM_CHANGED;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }

        } else if (errorByte & IDE_ERROR_DATA_ERROR) {

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IDE: Data error\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;

            if (!(deviceExtension->DeviceFlags[Srb->TargetId] & DFLAGS_MEDIA_STATUS_ENABLED)) {
                deviceExtension->ErrorCount++;
            }

            //
            // Build sense buffer
            //

            if (Srb->SenseInfoBuffer) {

                senseBuffer->ErrorCode = 0x70;
                senseBuffer->Valid     = 1;
                senseBuffer->AdditionalSenseLength = 0xb;
                senseBuffer->SenseKey = (deviceExtension->
                                         DeviceFlags[Srb->TargetId] & DFLAGS_REMOVABLE_DRIVE)? SCSI_SENSE_DATA_PROTECT : SCSI_SENSE_MEDIUM_ERROR;
                senseBuffer->AdditionalSenseCode = 0;
                senseBuffer->AdditionalSenseCodeQualifier = 0;

                srbStatus |= SRB_STATUS_AUTOSENSE_VALID;
            }
        } else { // no sense info

            DebugPrint((DBG_IDE_DEVICE_ERROR,
                        "IdePort: No sense information\n"));
            scsiStatus = SCSISTAT_CHECK_CONDITION;
            srbStatus = SRB_STATUS_ERROR;
        }

        if (senseBuffer->Valid == 1) {

            ULONG length = sizeof(SENSE_DATA);

            if (Srb->SenseInfoBufferLength < length ) {

               length = Srb->SenseInfoBufferLength;
            }

            ASSERT(length);
            ASSERT(Srb->SenseInfoBuffer);

            RtlCopyMemory(Srb->SenseInfoBuffer, (PVOID) senseBuffer, length);
        }

        if ((deviceExtension->ErrorCount >= MAX_ERRORS) &&
            (!(deviceExtension->DeviceFlags[Srb->TargetId] & DFLAGS_USE_DMA))) {

            deviceExtension->MaximumBlockXfer[Srb->TargetId] = 0;

            DebugPrint((DBG_ALWAYS,
                        "MapError: Disabling 32-bit PIO and Multi-sector IOs\n"));

            if (deviceExtension->ErrorCount == MAX_ERRORS) {

                //
                // Log the error.
                //

                IdePortLogError( HwDeviceExtension,
                                 Srb,
                                 Srb->PathId,
                                 Srb->TargetId,
                                 Srb->Lun,
                                 SP_BAD_FW_WARNING,
                                 4);
            }

            //
            // Reprogram to not use Multi-sector.
            //

            for (i = 0; i < deviceExtension->MaxIdeDevice; i++) {
                UCHAR statusByte;

                if (deviceExtension->DeviceFlags[i] & DFLAGS_DEVICE_PRESENT &&
                    !(deviceExtension->DeviceFlags[i] & DFLAGS_ATAPI_DEVICE)) {

                    //
                    // Select the device.
                    //
                    SelectIdeDevice(baseIoAddress1, i, 0);

                    //
                    // Setup sector count to reflect the # of blocks.
                    //

                    IdePortOutPortByte(baseIoAddress1->BlockCount,
                                       0);

                    //
                    // Issue the command.
                    //

                    IdePortOutPortByte(baseIoAddress1->Command,
                                       IDE_COMMAND_SET_MULTIPLE);

                    //
                    // Wait for busy to drop.
                    //

                    WaitOnBaseBusy(baseIoAddress1,statusByte);

                    //
                    // Check for errors. Reset the value to 0 (disable MultiBlock) if the
                    // command was aborted.
                    //

                    if (statusByte & IDE_STATUS_ERROR) {

                        //
                        // Read the error register.
                        //

                        errorByte = IdePortInPortByte(baseIoAddress1->Error);

                        DebugPrint((DBG_ALWAYS,
                                    "AtapiHwInitialize: Error setting multiple mode. Status %x, error byte %x\n",
                                    statusByte,
                                    errorByte));
                        //
                        // Adjust the devExt. value, if necessary.
                        //

                        deviceExtension->MaximumBlockXfer[i] = 0;

                    }
                    deviceExtension->DeviceParameters[i].IdePioReadCommand      = IDE_COMMAND_READ;
                    deviceExtension->DeviceParameters[i].IdePioWriteCommand     = IDE_COMMAND_WRITE;

#ifdef ENABLE_48BIT_LBA
                    if (deviceExtension->DeviceFlags[i] & DFLAGS_48BIT_LBA) {
                        deviceExtension->DeviceParameters[i].IdePioReadCommandExt = IDE_COMMAND_READ_EXT;
                        deviceExtension->DeviceParameters[i].IdePioWriteCommandExt = IDE_COMMAND_WRITE_EXT;
                    }
#endif
                    deviceExtension->DeviceParameters[i].MaxBytePerPioInterrupt = 512;
                    deviceExtension->MaximumBlockXfer[i] = 0;
                }
            }
        }
    }


    //
    // Set SCSI status to indicate a check condition.
    //

    Srb->ScsiStatus = scsiStatus;

    return srbStatus;

} // end MapError()
