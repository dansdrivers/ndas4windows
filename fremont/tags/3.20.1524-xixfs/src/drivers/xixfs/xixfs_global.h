#ifndef __XIXFS_GLOBAL_H__
#define __XIXFS_GLOBAL_H__

#include "xixfs_event.h"
extern BOOLEAN	XiDataDisable;
extern NPAGED_LOOKASIDE_LIST	XifsFcbLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsCcbLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsIrpContextLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsCloseFcbCtxList;
extern NPAGED_LOOKASIDE_LIST	XifsAddressLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsLcbLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsCoreBuffHeadList;
extern UNICODE_STRING XifsdUnicodeDirectoryNames[];






typedef struct _XIFS_COMM_CTX {
	uint8	HostMac[32];
	/*Server Context */
	HANDLE 	hServHandle;
	KEVENT	ServShutdownEvent;
	KEVENT	ServDatagramRecvEvent;
	KEVENT	ServNetworkEvent;
	KEVENT	ServStopEvent;

	/*Client Context*/
	HANDLE	hCliHandle;
	KEVENT 	CliShutdownEvent;
	KEVENT 	CliNetworkEvent;
	KEVENT	CliSendDataEvent;
	KEVENT	CliStopEvent;
	/* Shared Resource */
	FAST_MUTEX		SendPktListMutex;
	LIST_ENTRY		SendPktList;
	FAST_MUTEX		RecvPktListMutex;
	LIST_ENTRY		RecvPktList;
	
	ULONG			PacketNumber;
}XIFS_COMM_CTX, *PXIFS_COMM_CTX;




#define XIFS_DATA_FLAG_RESOURCE_INITIALIZED		(0x00000001)
#define XIFS_DATA_FLAG_MEMORY_INITIALIZED		(0x00000002)


typedef struct _XIFS_DATA{
	XIFS_NODE_ID				;

	ERESOURCE					DataResource;

	PDRIVER_OBJECT				XifsDriverObject;
	PDEVICE_OBJECT				XifsControlDeviceObject;
	
	LIST_ENTRY					XifsVDOList;
	
	
	FAST_MUTEX					XifsAuxLotLockListMutex;
	
	CACHE_MANAGER_CALLBACKS		XifsCacheMgrCallBacks;
	CACHE_MANAGER_CALLBACKS		XifsCacheMgrVolumeCallBacks;
	uint32						XifsFlags;
	//changed by ILGU HONG
	//	chesung suggest
	uint8						HostMac[32];
	uint8						HostId[16];
	
	
	XIFS_COMM_CTX				XifsComCtx;
	NETEVTCTX					XifsNetEventCtx;
	/*
		Memory resource managing is needed for CCB FCB IPCONTEXT ....	
	*/
	uint32					IsXifsComInit;
	uint32					IsRegistered;
	LONG					QueuedWork;
	KEVENT					QueuedWorkcleareEvent;
}XIFS_DATA, *PXIFS_DATA;



extern XIFS_DATA	XiGlobalData;


#endif //#ifndef __XIXFS_GLOBAL_H__
