#ifndef __XIXFS_GLOBAL_DATA_H__
#define __XIXFS_GLOBAL_DATA_H__

#include "XixFsComProto.h"

extern BOOLEAN	XiDataDisable;
extern NPAGED_LOOKASIDE_LIST	XifsFcbLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsCcbLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsIrpContextLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsCloseFcbCtxList;
extern NPAGED_LOOKASIDE_LIST	XifsAddressLookasideList;
extern NPAGED_LOOKASIDE_LIST	XifsLcbLookasideList;
extern UNICODE_STRING XifsdUnicodeDirectoryNames[];




/********************************************************************

	Bitmap Operation

*********************************************************************/
static  __inline uint32 test_bit(uint64 nr, volatile void * addr)
{
    return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}	

static __inline VOID set_bit(uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] |= (1U << (nr & 7));
}

static __inline VOID clear_bit(uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] &= ~(1U << (nr & 7));
}

static  __inline VOID change_bit(uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] ^= (1U << (nr & 7));
}

static  __inline uint32 test_and_set_bit(uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval | mask;
	return oldval & mask;
}

static __inline uint32 test_and_clear_bit(uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval & ~mask;
	return oldval & mask;
}

static __inline int test_and_change_bit(uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval ^ mask;
	return oldval & mask;
}

static __inline uint32 
IsSetBit(
	uint64 nr, 
	volatile void * addr)
{
	return test_bit(nr, addr);
}


static __inline VOID 
setBitToMap(uint64 bitIndex, volatile void * Map){
	set_bit(bitIndex, Map);
}

static __inline VOID  
clearBitToMap(uint64 bitIndex, volatile void * Map){
	clear_bit(bitIndex, Map);
}




static
__inline
LARGE_INTEGER CurrentTime(
	VOID
	)
{
	LARGE_INTEGER Time;
	ULONG		Tick;
	
	KeQueryTickCount(&Time);
	Tick = KeQueryTimeIncrement();
	Time.QuadPart = Time.QuadPart * Tick;

	return Time;
}


static
_inline
LARGE_INTEGER XixGetSystemTime(VOID)
{
	LARGE_INTEGER Time;
	
	KeQuerySystemTime(&Time);
	return Time;
}


typedef struct _XIFS_COMM_CTX {
	uint8	HostMac[32];
	/*Server Context */
	HANDLE 	hServHandle;
	KEVENT	ServShutdownEvent;
	KEVENT	ServDatagramRecvEvent;
	KEVENT	ServNetworkEvent;

	/*Client Context*/
	HANDLE	hCliHandle;
	KEVENT 	CliShutdownEvent;
	KEVENT 	CliNetworkEvent;
	KEVENT	CliSendDataEvent;

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
	FAST_MUTEX					DataMutex;
	PVOID						DataLockThread;

	PDRIVER_OBJECT				XifsDriverObject;
	PDEVICE_OBJECT				XifsControlDeviceObject;
	
	FAST_MUTEX					XifsVDOListMutex;
	LIST_ENTRY					XifsVDOList;
	
	
	FAST_MUTEX					XifsAuxLotLockListMutex;
	LIST_ENTRY					XifsAuxLotLockList;
	
	CACHE_MANAGER_CALLBACKS		XifsCacheMgrCallBacks;
	CACHE_MANAGER_CALLBACKS		XifsCacheMgrVolumeCallBacks;
	uint32						XifsFlags;
	//changed by ILGU HONG
	//	chesung suggest
	//uint8						HostMac[6];
	uint8						HostMac[32];
	uint8						HostId[16];
	
	BOOLEAN						IsXifsComInit;
	XIFS_COMM_CTX				XifsComCtx;
	NETEVTCTX					XifsNetEventCtx;
	/*
		Memory resource managing is needed for CCB FCB IPCONTEXT ....	
	*/

	WORK_QUEUE_ITEM		WorkQueueItem;
	LIST_ENTRY			DelayedCloseList;
	uint32				DelayedCloseCount;


}XIFS_DATA, *PXIFS_DATA;



extern XIFS_DATA	XiGlobalData;


#endif //#ifndef __XIXFS_GLOBAL_DATA_H__
