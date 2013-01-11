#ifndef LANSCSI_LURN_IDE_H
#define LANSCSI_LURN_IDE_H

extern LURN_INTERFACE LurnIdeDiskInterface ;
extern LURN_INTERFACE LurnIdeCDDVDInterface ;
extern LURN_INTERFACE LurnIdeVCDDVDInterface ;

#define LUDATA_FLAG_PRESENT	0x00000001
//#define LUDATA_FLAG_LBA		0x00000002
//#define LUDATA_FLAG_LBA48	0x00000004
//#define LUDATA_FLAG_DMA		0x00000008
//#define LUDATA_FLAG_UDMA	0x00000010
//#define LUDATA_FLAG_PIO		0x00000020

typedef	struct _LU_DATA {

	UCHAR			LanscsiTargetID;
	UCHAR			LanscsiLU;
	CHAR			UnitNumber;
	CHAR			Reserved;
	CHAR			NRRWHost;
	CHAR			NRROHost;
	UINT64			TargetData;

	// IDE Info.
	UINT32			LudataFlags;
	UINT32			PduDescFlags;
	UINT64			SectorCount;

	CHAR			Serial[20];
	CHAR			FW_Rev[8];
	CHAR			Model[40];

} LU_DATA, *PLU_DATA;

typedef struct _LURNEXT_IDE_DEVICE {
	//
	//	Lurn back-pointer
	//
	PLURELATION_NODE	Lurn ;

	//
	//	thread
	//
	HANDLE				ThreadHandle;
	PVOID				ThreadObject;
	KEVENT				ThreadReadyEvent;		// Set when Thread begin.
	//
	//	CCB request
	//	shared between this worker thread and requestor's thread.
	//
	LONG				PendingCcbCount;
	LIST_ENTRY			ThreadCcbQueue;	// protected by LurnSpinLock
	KEVENT				ThreadCcbArrivalEvent;

	//
	//	Lanscsi session
	//
	LANSCSI_SESSION		LanScsiSession;
//	LANSCSI_SESSION		NewLanScsiSession;
	LSTRANS_TYPE		LstransType;

	LU_DATA				LuData ;

} LURNEXT_IDE_DEVICE, *PLURNEXT_IDE_DEVICE;

#define LURNIDE_INITILIZE_PDUDESC(PLURNIDE, PDUDESC_POINTER, OPCODE, COMMAND, DESTADDR, DATABUFFER_LENGTH, DATABUFFER_POINTER)		\
			INITILIZE_PDUDESC(PDUDESC_POINTER, (PLURNIDE)->LuData.LanscsiTargetID, (PLURNIDE)->LuData.LanscsiLU, OPCODE, COMMAND,	\
					(PLURNIDE)->LuData.PduDescFlags,																				\
				 DESTADDR, DATABUFFER_LENGTH, DATABUFFER_POINTER)


extern LURN_INTERFACE LurnIdeDiskInterface ;
extern LURN_INTERFACE LurnIdeODDInterface ;
extern LURN_INTERFACE LurnIdeVODDInterface ;

#endif // LANSCSI_LURN_IDE_H