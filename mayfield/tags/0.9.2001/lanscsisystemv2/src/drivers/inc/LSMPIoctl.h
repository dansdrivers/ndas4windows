#ifndef LSMP_IOCTL_H
#define LSMP_IOCTL_H

/********************************************************************

	created:	2003/12/15
	created:	15:12:2003   19:51
	filename: 	LANSCSISYSTEMV2\SRC\INC\DRIVER\LSMPIoctl.h
	file path:	LANSCSISYSTEMV2\SRC\INC\DRIVER
	author:		XiMeta
	
	purpose:	IO control code and structure of LanScsiMiniport

*********************************************************************/

#include "lanscsi.h"
#include "SocketLpx.h"

//
//	signature of LanScsiMiniport IO control.
//
#define LANSCSIMINIPORT_IOCTL_SIGNATURE				"XIMETA_1"

#define LANSCSIMINIPORT_IOCTL_GET_SLOT_NO			0x00000001
//#define LANSCSIMINIPORT_IOCTL_READ_ONLY			0x00000002		// To be obsolete
//#define LANSCSIMINIPORT_IOCTL_QUERYINFO			0x00000003		// To be obsolete
#define LANSCSIMINIPORT_IOCTL_QUERYINFO_EX			0x00000004
#define LANSCSIMINIPORT_IOCTL_ADD_TARGET			0x00000005
#define LANSCSIMINIPORT_IOCTL_REMOVE_TARGET			0x00000006
#define LANSCSIMINIPORT_IOCTL_ADD_DEVICE			0x00000007
#define LANSCSIMINIPORT_IOCTL_REMOVE_DEVICE			0x00000008
#define LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE		0x00000009
#define LANSCSIMINIPORT_IOCTL_NOOP					0x0000000A
// Added by ILGU HONG 2004_07_05
#define LANSCSIMINIPORT_IOCTL_GET_DVD_STATUS		0x00000100
// Added by ILGU HONG 2004_07_05 end
#define ETHERNET_ADDR_LENGTH						6

typedef enum _LSMP_INFORMATION_CLASS {

	LsmpAdapterInformation = 1,			// 1
	LsmpPrimaryUnitDiskInformation,		// 2

} LSMP_INFORMATION_CLASS, *PLSMP_INFORMATION_CLASS;

typedef struct _LSMPIoctl_QueryInfo {

	UINT32					Length ;
	LSMP_INFORMATION_CLASS	InfoClass ;
    UINT32					SlotNo;				// Optional
	UINT32					QueryDataLength ;
	UCHAR					QueryData[1] ;

}	LSMPIOCTL_QUERYINFO, * PLSMPIOCTL_QUERYINFO;

//
//	single Adapter
//
//	Keep the same values as in LanscsiMiniport.h
//
#define ADAPTERINFO_STATUS_MASK						0x000000ff
#define	ADAPTERINFO_STATUS_INIT						0x00000000
#define	ADAPTERINFO_STATUS_RUNNING					0x00000001
#define ADAPTERINFO_STATUS_STOPPING					0x00000002
#define ADAPTERINFO_STATUS_IN_ERROR					0x00000003
#define ADAPTERINFO_STATUS_STOPPED					0x00000004

#define ADAPTERINFO_STATUSFLAG_MASK					0xffffff00
#define ADAPTERINFO_STATUSFLAG_RECONNECTPENDING		0x00000100
#define ADAPTERINFO_STATUSFLAG_POWERSAVING_PENDING	0x00000200
#define ADAPTERINFO_STATUSFLAG_BUSRESET_PENDING		0x00000400
#define ADAPTERINFO_STATUSFLAG_MEMBER_FAULT			0x00000800

#define ADAPTERINFO_ISSTATUS(ADAPTERSTATUS, STATUS)	\
		(((ADAPTERSTATUS) & ADAPTERINFO_STATUS_MASK) == (STATUS))

#define ADAPTERINFO_ISSTATUSFLAG(ADAPTERSTATUS, STATUSFLAG)		\
		(((ADAPTERSTATUS) & (STATUSFLAG)) != 0)


typedef struct _AdapterInformation {

	UINT32					Length;
	UINT32					SlotNo;
	UCHAR					InitiatorId;
    UCHAR					NumberOfBuses;
    UCHAR					MaximumNumberOfTargets;
    UCHAR					MaximumNumberOfLogicalUnits;
	UINT32					MaxBlocksPerRequest;
	UINT32					Status;

}	LSMP_ADAPTER, *PLSMP_ADAPTER ;


//
//	single UnitDisk
//
#define STATUSFLAG_UPGRADEPENDING	0x00000001
#define STATUSFLAG_UPGRADEDONE		0x00000002

typedef struct _UnitDiskInformation {

	UINT32					Length ;
	TA_LSTRANS_ADDRESS		NetDiskAddress;
	TA_LSTRANS_ADDRESS		BindingAddress;
	UINT32					SlotNo;
	UCHAR					UnitDiskId;
	UCHAR					Reserved;
	UCHAR					UserID[4];
	UCHAR					Password[8];
	ACCESS_MASK				DesiredAccess;
	UINT32					GrantedAccess;
	UINT32					UnitBlocks;
	UINT32					StatusFlags;

}	LSMP_UNITDISK, *PLSMP_UNITDISK;


//
//	Adapter Information
//
typedef struct _LSMPIoctl_AdapterInfo {

	UINT32					Length ;
	LSMP_ADAPTER			Adapter ;

}	LSMPIOCTL_ADAPTERINFO, *PLSMPIOCTL_ADAPTERINFO ;


//
//	Primary UnitDisk information
//
typedef struct _LSMPIoctl_PrimUnitDiskInfo {

	UINT32					Length ;
	
	// added by ktkim at 03/06/2004
	LARGE_INTEGER			EnabledTime;
	
	LSMP_UNITDISK			UnitDisk ;

}	LSMPIOCTL_PRIMUNITDISKINFO, *PLSMPIOCTL_PRIMUNITDISKINFO ;


typedef struct _LSMPIoctl_NOOP {

	BYTE						PathId;
	BYTE						TargetId;
	BYTE						Lun;

}	LSMPIOCTL_NOOP, *PLSMPIOCTL_NOOP ;

#endif