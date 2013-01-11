#ifndef __XIMETA_EVENT_QUEUE_H__
#define __XIMETA_EVENT_QUEUE_H__

//
//	LFS scsi address
//

typedef struct _LFS_SCSI_ADDRESS {
	ULONG Length;
	UCHAR PortNumber;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;
} LFS_SCSI_ADDRESS, *PLFS_SCSI_ADDRESS;

typedef PVOID	LFS_EVTQUEUE_HANDLE, *PLFS_EVTQUEUE_HANDLE;

//
//  LFS_EVTQUEUE_HANDLE value is created by kernel and passed to user. 
//	To make driver support both 64 bit and 32 bit application, 32 bit thunk type is added
//
typedef PVOID	LFS_EVTQUEUE_HANDLE, *PLFS_EVTQUEUE_HANDLE;
typedef VOID*POINTER_32 LFS_EVTQUEUE_HANDLE_32;
typedef LFS_EVTQUEUE_HANDLE_32* PLFS_EVTQUEUE_HANDLE_32;

//
//	XEvent class
//

#define XEVTCLS_PRIMARY_UNKNOWN						0
#define XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED	1
#define XEVTCLS_PRIMARY_DISCONNECTED_ABNORMALLY		2
#define XEVTCLS_LFSFILT_SHUTDOWN					3

//
//	Variable length parameter
//

//	XEVTCLS_PRIMARY_VOLUME_INVALID_OR_LOCKED
typedef struct _XEVENT_VPARAM_VOLUMELOCKED {

	//
	//	NDAS device's slot number
	//

	UINT32				SlotNumber;


	//
	//	NDAS device's unit number
	//

	UINT32				UnitNumber;


	//
	//	NDAS device's SCSI address
	// Unused for now.
	//

	LFS_SCSI_ADDRESS	ScsiAddress;

} XEVENT_VPARAM_VOLUMELOCKED,*PXEVENT_VPARAM_VOLUMELOCKED;


//	XEVTCLS_PRIMARY_DISCONNECTED_ABNORMALLY
typedef struct _XEVENT_VPARAM_DISCONNECT_ABNORMAL {

	//	File name list in multi-string format
	WCHAR	SuspectedFileNameList[1];

} XEVENT_VPARAM_DISCONNECT_ABNORMAL,*PXEVENT_VPARAM_DISCONNECT_ABNORMAL;

//
//	XEvent item
//

typedef struct _XEVENT_ITEM {
	UINT32			EventLength;
	UINT32			EventClass;
	UINT32			DiskVolumeNumber;
	UINT32			Reserved;

	UINT32			VParamLength;
	union {
		XEVENT_VPARAM_VOLUMELOCKED			VolumeLocked;
		XEVENT_VPARAM_DISCONNECT_ABNORMAL	AbnormalDiscon;
	};
} XEVENT_ITEM, *PXEVENT_ITEM;


#endif