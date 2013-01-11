#ifndef __XIXFS_COM_PROTOCOL_H__
#define __XIXFS_COM_PROTOCOL_H__

#include "XixFsType.h"
#include "SocketLpx.h"

// Little Endian

#include <pshpack8.h>
#pragma warning(error: 4820)

typedef struct _XIFS_COMM_HEADER {
		//Changed by ILGU HONG
		//	chesung suggest
		/*
		uint8		Protocol[4];			//4
		uint8		SrcMac[6];			//10
		uint8		DstMac[6];			//16
		uint32		XifsMajorVersion;	//20
		uint32		XifsMinorVersion;	//24
		uint32		Type;				//28
		uint32		MessageSize; //Total Message Size (include Header size)	32
		*/
	
		uint8		Protocol[4];		//4
		uint32		_alignment_1_;		//8
		uint8		SrcMac[32];			//40
		uint8		DstMac[32];			//72
		uint32		XifsMajorVersion;	//76
		uint32		XifsMinorVersion;	//80
		uint32		Type;				//84
		uint32		MessageSize; //Total Message Size (include Header size)	88
}XIFS_COMM_HEADER, *PXIFS_COMM_HEADER;

C_ASSERT(sizeof(XIFS_COMM_HEADER) == 88);


typedef struct _XIFS_LOCK_REQUEST{
		// Changed by ILGU HONG
			// chesung suggest
		/*
		uint8		DiskId[6];		//6
		uint32	 	PartionId;		//10	
		uint64	 	LotNumber;		//18
		uint32		PacketNumber;	//22
		uint8		LotOwnerID[16];	//38
		uint8		LotOwnerMac[6];	//44
		*/
		uint8		DiskId[16];		//16
		uint32	 	PartionId;		//20	
		uint32		_alignment_1_;	//24
		uint64	 	LotNumber;		//32
		uint32		PacketNumber;	//36
		uint32		_alignment_2_;	//40
		uint8		LotOwnerID[16];	//56
		uint8		LotOwnerMac[32];//88
		uint8		Reserved[40];	//128
}XIFS_LOCK_REQUEST, *PXIFS_LOCK_REQUEST;

C_ASSERT(sizeof(XIFS_LOCK_REQUEST) == 128);

typedef struct _XIFS_LOCK_REPLY{
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];		//6
		uint32	 	PartionId;		//10	
		uint64	 	LotNumber;		//18
		uint32		PacketNumber;	//22
		uint32		LotState;		//26
		uint8		Reserved[18];	//44
		*/
		uint8		DiskId[16];		//16
		uint32	 	PartionId;		//20
		uint32		_alignment_1_;	//24
		uint64	 	LotNumber;		//32
		uint32		PacketNumber;	//36
		uint32		LotState;		//40
		uint8		Reserved[88];	//128
}XIFS_LOCK_REPLY, *PXIFS_LOCK_REPLY;

C_ASSERT(sizeof(XIFS_LOCK_REPLY) == 128);

typedef struct _XIFS_FILE_CHANGE_BROADCAST {
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;			
		uint64	 	LotNumber;			//18
		uint32		SubCommand;			//22
		uint64		PrevParentLotNumber;
		uint64		NewParentLotNumber;
		uint8		Reserved[6];		//44
		*/
		uint8		DiskId[16];				//16
		uint32	 	PartionId;				//20
		uint32		_alignment_1_;			//24
		uint64	 	LotNumber;				//32
		uint32		SubCommand;				//36
		uint32		_alignment_2_;			//40
		uint64		PrevParentLotNumber;	//48
		uint64		NewParentLotNumber;		//56 		
		uint8		Reserved[72];			//128
}XIFS_FILE_CHANGE_BROADCAST, *PXIFS_FILE_CHANGE_BROADCAST;

C_ASSERT(sizeof(XIFS_FILE_CHANGE_BROADCAST) == 128);


typedef struct _XIFS_LOCK_BROADCAST{
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;
		uint64	 	LotNumber;		//18
		uint8		Reserved[26];	//44
		*/
		uint8		DiskId[16];		//16
		uint32	 	PartionId;		//20
		uint32		_alignment_1_;	//24
		uint64	 	LotNumber;		//32
		uint8		Reserved[96];	//128
}XIFS_LOCK_BROADCAST, *PXIFS_LOCK_BROADCAST;

C_ASSERT(sizeof(XIFS_LOCK_BROADCAST) == 128);

typedef struct _XIFS_RANGE_FLUSH_BROADCAST{
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;			
		uint64	 	LotNumber;		//18
		uint64		StartOffset;		//26
		uint32		DataSize;		//30
		uint8		Reserved[14];	//44
		*/
		uint8		DiskId[16];		//16	
		uint32	 	PartionId;		//20
		uint32		_alignment_1_;	//24
		uint64	 	LotNumber;		//32
		uint64		StartOffset;	//40
		uint32		DataSize;		//44
		uint32		_alignment_2_;	//48
		uint8		Reserved[80];	//128
}XIFS_RANGE_FLUSH_BROADCAST, *PXIFS_RANGE_FLUSH_BROADCAST;

C_ASSERT(sizeof(XIFS_RANGE_FLUSH_BROADCAST) == 128);

typedef struct _XIFS_FILE_LENGTH_CHANGE_BROADCAST {
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;			
		uint64	 	LotNumber;			//18
		uint64		FileLength;			//26
		uint64		AllocationLength;	//34
		uint64		WriteStartOffset;	//42
		uint8		Reserved[2];		//44
		*/
		uint8		DiskId[16];			//16	
		uint32	 	PartionId;			//20
		uint32		_alignment_1_;		//24
		uint64	 	LotNumber;			//32
		uint64		FileLength;			//40
		uint64		AllocationLength;	//48
		uint64		WriteStartOffset;	//56
		uint8		Reserved[72];		//128
}XIFS_FILE_LENGTH_CHANGE_BROADCAST, *PXIFS_FILE_LENGTH_CHANGE_BROADCAST;

C_ASSERT(sizeof(XIFS_FILE_LENGTH_CHANGE_BROADCAST) == 128);

typedef struct _XIFS_DIR_ENTRY_CHANGE_BROADCAST {
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;			
		uint64	 	LotNumber;			//18
		uint32		ChildSlotNumber;	//22
		uint32		SubCommand;			//26
		uint8		Reserved[18];		//44
		*/
		uint8		DiskId[16];			//16
		uint32	 	PartionId;			//20
		uint32		_alignment_1_;		//24
		uint64	 	LotNumber;			//32
		uint32		ChildSlotNumber;	//36
		uint32		SubCommand;			//40
		uint8		Reserved[88];		//128
}XIFS_DIR_ENTRY_CHANGE_BROADCAST, *PXIFS_DIR_ENTRY_CHANGE_BROADCAST;


C_ASSERT(sizeof(XIFS_DIR_ENTRY_CHANGE_BROADCAST) == 128);


typedef struct _XIFS_FILE_RANGE_LOCK_BROADCAST{
		// Changed by ILGU HONG
		// chesung suggest
		/*
		uint8		DiskId[6];
		uint32	 	PartionId;			
		uint64	 	LotNumber;			//18
		uint64		StartOffset;		//26
		uint32		DataSize;			//30
		uint32		SubCommand;			//34
		uint8		Reserved[10];		//44
		*/
		uint8		DiskId[16];			//16
		uint32	 	PartionId;			//20
		uint32		_alignment_1_;		//24
		uint64	 	LotNumber;			//32
		uint64		StartOffset;		//40
		uint32		DataSize;			//44
		uint32		SubCommand;			//48
		uint8		Reserved[80];		//128
}XIFS_FILE_RANGE_LOCK_BROADCAST, *PXIFS_FILE_RANGE_LOCK_BROADCAST;

C_ASSERT(sizeof(XIFS_FILE_RANGE_LOCK_BROADCAST) == 128);


#define	TIMERUNIT_SEC	(10 * 1000 * 1000)

///////////////////////////////////////////////////////////////////////////
//
//
//
//			XIFS DEFINDED VALUE
//
//
//
//




#define XIFS_MEM_TAG_PACKET		'tpix'
#define XIFS_MEM_TAG_SOCKET		'tsix'

#define XIFS_DATAGRAM_PROTOCOL	{ 'X', 'I', 'F', 'S' }

#define	XIFS_TYPE_UNKOWN					0x00000000	
#define	XIFS_TYPE_LOCK_REQUEST				0x00000001	//	request packet
#define	XIFS_TYPE_LOCK_REPLY				0x00000002	//	reply packet
#define	XIFS_TYPE_LOCK_BROADCAST			0x00000004	//	broadcasting packet
#define XIFS_TYPE_FLUSH_BROADCAST			0x00000008	//	Flush range
#define	XIFS_TYPE_DIR_CHANGE				0x00000010	// 	Dir Entry change
#define			XIFS_SUBTYPE_DIR_ADD			0x00000001	//		Add
#define			XIFS_SUBTYPE_DIR_DEL			0x00000002	//		Del
#define			XIFS_SUBTYPE_DIR_MOD			0x00000003	//		Mod
#define	XIFS_TYPE_FILE_LENGTH_CHANGE		0x00000020	//	File Length change
#define	XIFS_TYPE_FILE_CHANGE				0x00000040	//	File Change
#define			XIFS_SUBTYPE_FILE_DEL			0x00000001	//		File del
#define			XIFS_SUBTYPE_FILE_MOD			0x00000002	//		File Mod
#define			XIFS_SUBTYPE_FILE_RENAME		0x00000003	//		File Rename
#define			XIFS_SUBTYPE_FILE_LINK			0x00000004	//		File Link
#define	XIFS_TYPE_FILE_RANGE_LOCK			0x00000080
#define			XIFS_SUBTYPE_FILE_RLOCK_ACQ	0x00000001
#define			XIFS_SUBTYPE_FILE_RLOCK_REL	0x00000002
#define 	XIFS_TYPE_MASK					0x000000FF


#define  XIFS_PROTO_MAJOR_VERSION			0x0
#define  XIFS_PROTO_MINOR_VERSION			0x1

#define	DEFAULT_XIFS_SVRPORT				((USHORT)0x1000)
#define	DEFAULT_XIFS_CLIWAIT				( 1 * TIMERUNIT_SEC)	// 1 seconds.
#define DEFAULT_REQUEST_MAX_TIMEOUT			(5 * TIMERUNIT_SEC)
#define MAX_XI_DATAGRAM_DATA_SIZE			(1024 - sizeof(XIFS_COMM_HEADER))
///////////////////////////////////////////////////////////////////////////
//
// define the packet format using datagramtype LPX
//
//
//	head part of a LFS datagram packet
//



typedef union _XIFSDG_RAWPK_DATA {
		XIFS_LOCK_REQUEST						LockReq;
		XIFS_LOCK_REPLY							LockReply;
		XIFS_LOCK_BROADCAST					LockBroadcast;
		XIFS_RANGE_FLUSH_BROADCAST			FlushReq;
		XIFS_DIR_ENTRY_CHANGE_BROADCAST		DirChangeReq;
		XIFS_FILE_LENGTH_CHANGE_BROADCAST		FileLenChangeReq;
		XIFS_FILE_CHANGE_BROADCAST				FileChangeReq;
		XIFS_FILE_RANGE_LOCK_BROADCAST		FileRangeLockReq;
}XIFSDG_RAWPK_DATA, *PXIFSDG_RAWPK_DATA;

C_ASSERT(sizeof(XIFSDG_RAWPK_DATA) == 128);


#pragma warning(default: 4820)
#include <poppack.h>

typedef struct _XIFSDG_PKT{
	//
	//	raw packet
	//	Do not insert any code between RawHeadDG and RawDataDG
	//	We assume that RawHead and RawData are continuos memory area.
	//
	//	do not insert field befoure ...
	XIFS_COMM_HEADER			RawHeadDG ;
	XIFSDG_RAWPK_DATA			RawDataDG ;	
	//  packed data
	

	LONG				RefCnt ;				// reference count
	ULONG				Flags ;			
	PXIFS_LOCK_CONTROL	pLockContext;	
	PIO_WORKITEM		WorkQueueItem;
	LARGE_INTEGER		TimeOut;
	LIST_ENTRY			PktListEntry ;

	LPX_ADDRESS			SourceAddr ;
	ULONG				PacketSize ;
	ULONG				DataSize ;			// Data part size in packet.
	UINT32				Reserved ;
	//--> 40 bytes

}XIFSDG_PKT, *PXIFSDG_PKT;



//
//	Datagram
//
typedef struct _LFSDG_NICSocket {

	PFILE_OBJECT	AddressFile ;
    HANDLE			AddressFileHandle ;
	LPX_ADDRESS		NICAddr ;

} LFSDG_NICSocket, *PLFSDG_NICSocket ;

typedef struct _LFSDG_Socket {
	USHORT	SocketCnt ;
	USHORT	Port ;
	LFSDG_NICSocket Sockets[MAX_SOCKETLPX_INTERFACE] ;
} LFSDG_Socket, *PLFSDG_Socket ;

NTSTATUS
LfsOpenDGSocket(
		OUT PLFSDG_Socket	Socket,
		IN USHORT		PortNo
	) ;

VOID
LfsCloseDGSocket(
		IN PLFSDG_Socket	Socket
	) ;


NTSTATUS
LfsSendDatagram(
	IN PLFSDG_Socket		socket,
	IN PXIFSDG_PKT			pkt,
	PLPX_ADDRESS		LpxRemoteAddress

);

BOOLEAN
LfsAllocDGPkt(
		PXIFSDG_PKT	*ppkt,
		uint8 		*SrcMac,
		uint8		*DstMac,
		uint32		Type
);

VOID
LfsReferenceDGPkt(
		PXIFSDG_PKT pkt
	) ;

VOID
LfsDereferenceDGPkt(
		PXIFSDG_PKT pkt
	) ;

BOOLEAN
LfsFreeDGPkt(
		PXIFSDG_PKT pkt
	) ;

NTSTATUS
LfsRegisterDatagramRecvHandler(
		IN	PLFSDG_Socket	ServerDatagramSocket,
		IN	PVOID			EventHandler,
		IN	PVOID			EventContext
   ) ;

BOOLEAN
LfsIsFromLocal(
		PLPX_ADDRESS	NICAddr
	) ;

///////////////////////////////////////////////////////////////////
//
//	LFS exported variables
//


//
//	TODO: remove in the future
//
#define ETHER_ADDR_LENGTH 6


//////////////////////////////////////////////////////////////////////////
//
//	networking event detector.
//
#define NETEVT_MAX_CALLBACKS	4
#define NETEVT_FREQ		( 1 * TIMERUNIT_SEC)	// 1 seconds.

typedef VOID (*NETEVT_CALLBACK)(
				PSOCKETLPX_ADDRESS_LIST	Original,
				PSOCKETLPX_ADDRESS_LIST	Updated,
				PSOCKETLPX_ADDRESS_LIST	Disabled,
				PSOCKETLPX_ADDRESS_LIST	Enabled,
				PVOID					Context
		) ;

typedef struct _NETEVTCTX {
	KEVENT	ShutdownEvent ;
	HANDLE	HThread ;
	SOCKETLPX_ADDRESS_LIST	AddressList ;
	SOCKETLPX_ADDRESS_LIST	EnabledAddressList ;
	SOCKETLPX_ADDRESS_LIST	DisabledAddressList ;
	LONG					CallbackCnt ;
	NETEVT_CALLBACK			Callbacks[NETEVT_MAX_CALLBACKS] ;
	PVOID					CallbackContext[NETEVT_MAX_CALLBACKS] ;
} NETEVTCTX, *PNETEVTCTX ;

BOOLEAN
NetEvtInit(PNETEVTCTX	NetEvtCtx) ;

BOOLEAN
NetEvtTerminate(PNETEVTCTX	NetEvtCtx) ;

#if DBG

VOID
DbgPrintLpxAddrList(
		ULONG					DbgMask,
		PSOCKETLPX_ADDRESS_LIST	AddrList
) ;

#endif



#endif // __XIXFS_COM_PROTOCOL_H__
