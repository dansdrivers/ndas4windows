
* Restrictions on using SCSI_ADDRESS
  
  SCSI_ADDRESS structure has four fields.
    (PortNumber, PathId, TargetId, Lun)
    
  For the sake of supporting ATA compatible features, we only should make
  use of PortNumber and TargetId, leaving PathId and Lun as 0 at all times.
  
  Compatibility problems arose when SMART_SEND_DRIVE_COMMAND ioctl is
  issues from the user application and it is translated to
  IOCTL_SCSI_MINIPORT from disk.c (in disk class driver).
  
  cmdInParameters->bDriveNumber = diskData->ScsiAddress.TargetId;

  As seen above, bDriveNumber field in SENDCMDINPARAMS, other than
  the TargetId field of SCSI_ADDRESS is truncated and when the IOCTL
  is sent to the port fdo (instead of pdo), fdo cannot tell which pdo
  (or lu) should handle this ioctl.
  
  This leaves us nothing but TargetId for SCSI_ADDRESS.
  
* Handling IOCTL
                                     PDO          FDO              Remarks
	IOCTL_ATA_PASS_THROUGH         : ?            Redirect to PDO
	IOCTL_ATA_PASS_THROUGH_DIRECT  : ?            Redirect to PDO
	IOCTL_IDE_PASS_THROUGH         : ?            Redirect to PDO
	IOCTL_SCSI_FREE_DUMP_POINTERS  : N/S          N/S
	IOCTL_SCSI_GET_ADDRESS         : Handled      N/S
	IOCTL_SCSI_GET_DUMP_POINTERS   : N/S          N/S
	IOCTL_SCSI_GET_INQUIRY_DATA    : N/S          N/S              (obsolete)
	IOCTL_SCSI_GET_CAPABILITIES    : Fail         Handled
	IOCTL_SCSI_MINIPORT            : Redirect     Redirect to PDO
	IOCTL_SCSI_PASS_THROUGH        : Redirect     Redirect to PDO
	IOCTL_SCSI_PASS_THROUGH_DIRECT : Redirect     Redirect to PDO
	IOCTL_SCSI_RESCAN_BUS          : N/S          N/S              (obsolete)
 
* Priority Hint Support

	Port drive must support IOCTL_STORAGE_CHECK_PRIORITY_HINT_SUPPORT to
	facilitate the Priority Hint feature in Windows Vista
	
* Power-down handling

  Primarily class driver handles the power in a function ClasspPowerHandler (power.c)
  using several SRBs sending down to the PDO.

  1. PowerDownDeviceInitial2 - Lock the queue

     Srb.SrbFlags |= SRB_FLAGS_BYPASS_LOCKED_QUEUE | SRB_FLAGS_NO_QUEUE_FREEZE
     Srb.Function = SRB_FUNCTION_LOCK_QUEUE

  // Lock was not successful - throw down the power IRP
  // by itself and don't try to spin down the drive or unlock
  // the queue.

  2. PowerDownDeviceLocked2

     (FILE_DEVICE_DISK (disk.sys) gets this SYNCH_CACHE SRB. For others, bypass)

  	 Srb.Function = SRB_FUNCTION_EXECUTE_SCSI 

  	 Srb.SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER |
 	 			  	SRB_FLAGS_DISABLE_AUTOSENSE |
                 	SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
                 	SRB_FLAGS_NO_QUEUE_FREEZE |
                 	SRB_FLAGS_BYPASS_LOCKED_QUEUE;

  	 Srb.SYNCHRONIZE_CACHE10.OperationCode = SCSIOP_SYNCHRONIZE_CACHE

  3. PowerDownDeviceFlushed2

  	 Srb.Function = SRB_FUNCTION_EXECUTE_SCSI 

  	 Srb.SrbFlags = SRB_FLAGS_NO_DATA_TRANSFER |
  			   	  	SRB_FLAGS_DISABLE_AUTOSENSE |
 				 	SRB_FLAGS_DISABLE_SYNCH_TRANSFER |
 				 	SRB_FLAGS_NO_QUEUE_FREEZE |
 				 	SRB_FLAGS_BYPASS_LOCKED_QUEUE;

  	 cdb->START_STOP.OperationCode = SCSIOP_START_STOP_UNIT;
  	 cdb->START_STOP.Start = 0;
  	 cdb->START_STOP.Immediate = 1;

  4. PowerDownDeviceOff2

  	 Srb.Function = SRB_FUNCTION_UNLOCK_QUEUE
  	 Srb.SrbFlags = SRB_FLAGS_BYPASS_LOCKED_QUEUE

  5. PowerDownDeviceUnlocked2

  	 complete the power request

* Power-up Handling

-----------------------------------------------------------------------------------
ClasspPowerUpCompletion()

Routine Description:

    This routine is used for intermediate completion of a power up request.
    PowerUp requires four requests to be sent to the lower driver in sequence.

        * The queue is "power locked" to ensure that the class driver power-up
          work can be done before request processing resumes.

        * The power irp is sent down the stack for any filter drivers and the
          port driver to return power and resume command processing for the
          device.  Since the queue is locked, no queued irps will be sent
          immediately.

        * A start unit command is issued to the device with appropriate flags
          to override the "power locked" queue.

        * The queue is "power unlocked" to start processing requests again.

    This routine uses the function in the srb which just completed to determine
    which state it is in.
-----------------------------------------------------------------------------------


  1. PowerUpDeviceInitial

     Srb.SrbFlags |= SRB_FLAGS_BYPASS_LOCKED_QUEUE | SRB_FLAGS_NO_QUEUE_FREEZE
     Srb.Function = SRB_FUNCTION_LOCK_QUEUE

  // Lock was not successful - throw down the power IRP
  // by itself and don't try to spin down the drive or unlock
  // the queue.


2. BTHENUM\{24df01a9-3e4f-4c9f-9f66-5aa8ab14f8f4}
3. BTHENUM\{fbab37d9-43e4-4760-80fb-64463bb5e5a8}

