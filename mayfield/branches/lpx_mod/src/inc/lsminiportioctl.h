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
//	define IOCTL code rule.
//
#ifndef CTL_CODE

#define CTL_CODE( DeviceType, Function, Method, Access ) \
	(((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#define METHOD_BUFFERED                 0
#define METHOD_IN_DIRECT                1
#define METHOD_OUT_DIRECT               2
#define METHOD_NEITHER                  3
#define FILE_ANY_ACCESS                 0
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe

#endif

//
//	define LanscsiMiniport ioctl rule.
//
#define LSMP_DEVICE_TYPE	0x89e9
#define LSMP_CTL_CODE(x) \
	CTL_CODE(LSMP_DEVICE_TYPE, x, METHOD_BUFFERED, FILE_ANY_ACCESS)

//
//	signature of LanScsiMiniport IO control.
//
#define LANSCSIMINIPORT_IOCTL_SIGNATURE				"XIMETA_1"

#define LANSCSIMINIPORT_IOCTL_GET_SLOT_NO			LSMP_CTL_CODE(0x001)
//#define LANSCSIMINIPORT_IOCTL_READ_ONLY			LSMP_CTL_CODE(0x002)		// To be obsolete
//#define LANSCSIMINIPORT_IOCTL_QUERYINFO			LSMP_CTL_CODE(0x003)		// To be obsolete
#define LANSCSIMINIPORT_IOCTL_QUERYINFO_EX			LSMP_CTL_CODE(0x004)
#define LANSCSIMINIPORT_IOCTL_ADD_TARGET			LSMP_CTL_CODE(0x005)
#define LANSCSIMINIPORT_IOCTL_REMOVE_TARGET			LSMP_CTL_CODE(0x006)
#define LANSCSIMINIPORT_IOCTL_ADD_DEVICE			LSMP_CTL_CODE(0x007)
#define LANSCSIMINIPORT_IOCTL_REMOVE_DEVICE			LSMP_CTL_CODE(0x008)
#define LANSCSIMINIPORT_IOCTL_UPGRADETOWRITE		LSMP_CTL_CODE(0x009)
#define LANSCSIMINIPORT_IOCTL_NOOP					LSMP_CTL_CODE(0x00A)
#define LANSCSIMINIPORT_IOCTL_GET_VERSION			LSMP_CTL_CODE(0x201)
// Added by ILGU HONG 2004_07_05
#define LANSCSIMINIPORT_IOCTL_GET_DVD_STATUS		LSMP_CTL_CODE(0x100)
// Added by ILGU HONG 2004_07_05 end
#define ETHERNET_ADDR_LENGTH						6



//////////////////////////////////////////////////////////////////////////
//
//	Query Ioctl
//
typedef enum _LSMP_INFORMATION_CLASS {

	LsmpAdapterInformation = 1,			// 1
	LsmpPrimaryUnitDiskInformation,		// 2
	LsmpAdapterLurInformation,			// 3
	LsmpDriverVersion					// 4

} LSMP_INFORMATION_CLASS, *PLSMP_INFORMATION_CLASS;

typedef struct _LSMPIoctl_QueryInfo {

	UINT32					Length;
	LSMP_INFORMATION_CLASS	InfoClass;
    UINT32					SlotNo;				// Optional
	UINT32					QueryDataLength;
	UCHAR					QueryData[1];

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
#define ADAPTERINFO_STATUSFLAG_RECONNECT_PENDING	0x00000100
#define ADAPTERINFO_STATUSFLAG_POWERSAVING_PENDING	0x00000200
#define ADAPTERINFO_STATUSFLAG_BUSRESET_PENDING		0x00000400
#define ADAPTERINFO_STATUSFLAG_MEMBER_FAULT			0x00000800

#define ADAPTERINFO_ISSTATUS(ADAPTERSTATUS, STATUS)	\
		(((ADAPTERSTATUS) & ADAPTERINFO_STATUS_MASK) == (STATUS))

#define ADAPTERINFO_ISSTATUSFLAG(ADAPTERSTATUS, STATUSFLAG)		\
		(((ADAPTERSTATUS) & (STATUSFLAG)) != 0)

#define LURN_IS_RUNNING(LURNSTATUS)				(((LURNSTATUS) == LURN_STATUS_RUNNING) || ((LURNSTATUS) == LURN_STATUS_STALL))

#define LURN_STATUS_INIT							0x00000000
#define LURN_STATUS_RUNNING							0x00000001
#define LURN_STATUS_STALL							0x00000002
#define LURN_STATUS_STOP_PENDING					0x00000003
#define LURN_STATUS_STOP							0x00000004

typedef struct _AdapterInformation {

	UINT32					Length;
	UINT32					SlotNo;
	UCHAR					InitiatorId;
    UCHAR					NumberOfBuses;
    UCHAR					MaximumNumberOfTargets;
    UCHAR					MaximumNumberOfLogicalUnits;
	UINT32					MaxBlocksPerRequest;
	UINT32					Status;

}	LSMP_ADAPTER, *PLSMP_ADAPTER;

typedef struct _LurInformation {

	UINT32					Length;
	UINT32					DevType;
	UINT32					TargetId;
	UINT32					Lun;
	UINT32					DesiredAccess;
	UINT32					GrantedAccess;
	UINT32					LurnCnt;
	UCHAR					Reserved[16];

}	LSMP_LUR, *PLSMP_LUR;

//
//	single UnitDisk
//
#define STATUSFLAG_UPGRADEPENDING	0x00000001
#define STATUSFLAG_UPGRADEDONE		0x00000002

typedef struct _UnitDiskInformation {

	UINT32					Length;
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

typedef struct _LurnFullInformation {

	UINT32					Length;
	UINT32					LurnId;
	UINT32					LurnType;
	TA_LSTRANS_ADDRESS		NetDiskAddress;
	TA_LSTRANS_ADDRESS		BindingAddress;
	UCHAR					UnitDiskId;
	UCHAR					Reserved;
	UCHAR					UserID[4];
	UCHAR					Password[8];
	ACCESS_MASK				AccessRight;
	UINT64					UnitBlocks;
	UINT32					StatusFlags;

	UCHAR					Reserved2[16];

}	LSMP_LURN_FULL, *PLSMP_LURN_FULL;

//
//	Adapter Information
//
typedef struct _LSMPIoctl_AdapterInfo {

	UINT32					Length;
	LSMP_ADAPTER			Adapter;

}	LSMPIOCTL_ADAPTERINFO, *PLSMPIOCTL_ADAPTERINFO;

//
//	Primary UnitDisk information
//
typedef struct _LSMPIoctl_PrimUnitDiskInfo {

	UINT32					Length;
	
	// added by ktkim at 03/06/2004
	LARGE_INTEGER			EnabledTime;
	
	LSMP_UNITDISK			UnitDisk;

}	LSMPIOCTL_PRIMUNITDISKINFO, *PLSMPIOCTL_PRIMUNITDISKINFO;

//
//	Adapter full Information
//
typedef struct _LSMPIoctl_AdapterLurInfo {

	UINT32					Length;
	LSMP_ADAPTER			Adapter;
	LSMP_LUR				Lur;
	LARGE_INTEGER			EnabledTime;
	UCHAR					Reserved[16];
	UINT32					UnitDiskCnt;
	LSMP_LURN_FULL		UnitDisks[1];

}	LSMPIOCTL_ADAPTERLURINFO, *PLSMPIOCTL_ADAPTERLURINFO;

typedef struct _LSMPIoctl_DriverVersionInfo {

	USHORT						VersionMajor;
	USHORT						VersionMinor;
	USHORT						VersionBuild;
	USHORT						VersionPrivate;
	UCHAR						Reserved[16];

}	LSMPIOCTL_DRVVER, *PLSMPIOCTL_DRVVER;

typedef struct _LSMPIoctl_NOOP {

	BYTE						PathId;
	BYTE						TargetId;
	BYTE						Lun;

}	LSMPIOCTL_NOOP, *PLSMPIOCTL_NOOP;

#endif
