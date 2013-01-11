 BUSY status is returned.
//

#define BUSY_RETRY_COUNT 20

//
// Number of times to retry an INQUIRY request.
//

#define INQUIRY_RETRY_COUNT 2

//
// Number of irp stack locations to allocate for an INQUIRY command.
//

#define INQUIRY_STACK_LOCATIONS 1

//
// Bitmask used for aligning values.
//

#define LONG_ALIGN (sizeof(LONG) - 1)

//
// Size of the ADAPTER_EXTENSION
//

#define ADAPTER_EXTENSION_SIZE sizeof(ADAPTER_EXTENSION)

//
// Assorted macros.
//

#define NEED_REQUEST_SENSE(Srb) (Srb->ScsiStatus == SCSISTAT_CHECK_CONDITION \
        && !(Srb->SrbStatus & SRB_STATUS_AUTOSENSE_VALID) &&                 \
        Srb->SenseInfoBuffer && Srb->SenseInfoBufferLength )

#define GET_FDO_EXTENSION(HwExt) ((CONTAINING_RECORD(HwExt, HW_DEVICE_EXTENSION, HwDeviceExtension))->FdoExtension)

#define ADDRESS_TO_HASH(PathId, TargetId, Lun) (((TargetId) + (Lun)) % NUMBER_LOGICAL_UNIT_BINS)

#define IS_CLEANUP_REQUEST(irpStack)                                                                    \
        (((irpStack)->MajorFunction == IRP_MJ_CLOSE) ||                                                 \
         ((irpStack)->MajorFunction == IRP_MJ_CLEANUP) ||                                               \
         ((irpStack)->MajorFunction == IRP_MJ_SHUTDOWN) ||                                              \
         (((irpStack)->MajorFunction == IRP_MJ_SCSI) &&                                                 \
          (((irpStack)->Parameters.Scsi.Srb->Function == SRB_FUNCTION_RELEASE_DEVICE) ||                \
           ((irpStack)->Parameters.Scsi.Srb->Function == SRB_FUNCTION_FLUSH_QUEUE) ||                   \
           (TEST_FLAG((irpStack)->Parameters.Scsi.Srb->SrbFlags, SRB_FLAGS_BYPASS_FROZEN_QUEUE |        \
                                                                 SRB_FLAGS_BYPASS_LOCKED_QUEUE)))))
             

#define IS_MAPPED_SRB(Srb)                                  \
        ((srb->Function == SRB_FUNCTION_IO_CONTROL) ||      \
         ((srb->Function == SRB_FUNCTION_EXECUTE_SCSI) &&   \
          ((srb->Cdb[0] == SCSIOP_INQUIRY) ||               \
           (srb->Cdb[0] == SCSIOP_REQUEST_SENSE))))

//
// SpIsQueuePausedForSrb(lu, srb) -
//  determines if the queue has been paused for this particular type of
//  srb.  This can be used with SpSrbIsBypassRequest to determine whether the
//  srb needs to be handled specially.
//

#define SpIsQueuePausedForSrb(luFlags, srbFlags)                                                            \
    ((BOOLEAN) ((((luFlags) & LU_QUEUE_FROZEN) && !(srbFlags & SRB_FLAGS_BYPASS_FROZEN_QUEUE)) ||           \
                (((luFlags) & LU_QUEUE_PAUSED) && !(srbFlags & SRB_FLAGS_BYPASS_LOCKED_QUEUE))))

#define SpIsQueuePaused(lu) ((lu)->LuFlags & (LU_QUEUE_FROZEN           |   \
                                              LU_QUEUE_LOCKED))

#define SpSrbRequiresPower(srb)                                             \
    ((BOOLEAN) ((srb->Function == SRB_FUNCTION_EXECUTE_SCSI) ||             \
                 (srb->Function == SRB_FUNCTION_IO_CONTROL) ||              \
                 (srb->Function == SRB_FUNCTION_SHUTDOWN) ||                \
                 (srb->Function == SRB_FUNCTION_FLUSH) ||                   \
                 (srb->Function == SRB_FUNCTION_ABORT_COMMAND) ||           \
                 (srb->Function == SRB_FUNCTION_RESET_BUS) ||               \
                 (srb->Function == SRB_FUNCTION_RESET_DEVICE) ||            \
                 (srb->Function == SRB_FUNCTION_TERMINATE_IO) ||            \
                 (srb->Function == SRB_FUNCTION_REMOVE_DEVICE) ||           \
                 (srb->Function == SRB_FUNCTION_WMI)))

//
// Forward declarations of data structures
//

typedef struct _SRB_DATA SRB_DATA, *PSRB_DATA;

typedef struct _REMOVE_TRACKING_BLOCK
               REMOVE_TRACKING_BLOCK,
               *PREMOVE_TRACKING_BLOCK;

typedef struct _LOGICAL_UNIT_EXTENSION LOGICAL_UNIT_EXTENSION, *PLOGICAL_UNIT_EXTENSION;
typedef struct _ADAPTER_EXTENSION ADAPTER_EXTENSION, *PADAPTER_EXTENSION;

typedef struct _SP_INIT_CHAIN_ENTRY SP_INIT_CHAIN_ENTRY, *PSP_INIT_CHAIN_ENTRY;

typedef struct _HW_DEVICE_EXTENSION HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Type Definitions
//

//
// Structure used for tracking remove lock allocations in checked builds
//

struct _REMOVE_TRACKING_BLOCK {
    PREMOVE_TRACKING_BLOCK NextBlock;
    PVOID Tag;
    LARGE_INTEGER TimeLocked;
    PCSTR File;
    ULONG Line;
};

#if DBG
#define SpAcquireRemoveLock(devobj, tag) \
    SpAcquireRemoveLockEx(devobj, tag, __file__, __LINE__)
#endif

typedef struct _RESET_COMPLETION_CONTEXT {
    PIRP           OriginalIrp;
    PDEVICE_OBJECT SafeLogicalUnit;
    PDEVICE_OBJECT AdapterDeviceObject;

    SCSI_REQUEST_BLOCK Srb;
} RESET_COMPLETION_CONTEXT, *PRESET_COMPLETION_CONTEXT;

//
// Define a pointer to the synchonize execution routine.
//

typedef
BOOLEAN
(*PSYNCHRONIZE_ROUTINE) (
    IN PKINTERRUPT Interrupt,
   