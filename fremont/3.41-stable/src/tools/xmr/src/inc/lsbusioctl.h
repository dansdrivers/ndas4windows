/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Author:

     
Environment:

    user and kernel
Notes:


Revision History:


--*/

#ifndef __LANSCSI_BUS_IOCTL_H
#define __LANSCSI_BUS_IOCTL_H

#include "SocketLpx.h"

//
// Define an Interface Guid for bus enumerator class.
// This GUID is used to register (IoRegisterDeviceInterface) 
// an instance of an interface so that enumerator application 
// can send an ioctl to the bus driver.
//

DEFINE_GUID(GUID_LANSCSI_BUS_ENUMERATOR_INTERFACE_CLASS, 
		0xefe0bfc1, 0x63bd, 0x4fba, 0x87, 0x85, 0x3b, 0xf3, 0xbc, 0x65, 0xa5, 0xee);
// {EFE0BFC1-63BD-4fba-8785-3BF3BC65A5EE}

// Added by jgahn.
DEFINE_GUID(GUID_LANSCSI_BUS_DEVICE_INTERFACE_CLASS, 
		0x42b9da47, 0x6a20, 0x48ae, 0x8b, 0x7d, 0x4b, 0x3f, 0xc2, 0xb, 0xfd, 0x5e);
// {42B9DA47-6A20-48ae-8B7D-4B3FC20BFD5E}

//
// GUID definition are required to be outside of header inclusion pragma to avoid
// error during precompiled headers.
//

//////////////////////////////////////////////////////////////////////////
//
//	Defines
//
//#define BUS_HARDWARE_IDS	L"LanscsiBus\\LanscsiMiniport\0"
#define BUS_HARDWARE_IDS	L"LanscsiBus\\NetDisk_V0\0"
#define BUS_HARDWARE_IDS_LENGTH sizeof (BUS_HARDWARE_IDS)

#define FILE_DEVICE_BUSENUM         FILE_DEVICE_BUS_EXTENDER

#define BUSENUM_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

//#define IOCTL_BUSENUM_PLUGIN_HARDWARE				BUSENUM_IOCTL	(0x0)		// Obsolete
#define IOCTL_BUSENUM_UNPLUG_HARDWARE               BUSENUM_IOCTL	(0x1)
#define IOCTL_BUSENUM_EJECT_HARDWARE                BUSENUM_IOCTL	(0x2)
//#define IOCTL_TOASTER_DONT_DISPLAY_IN_UI_DEVICE	BUSENUM_IOCTL	(0x3)		// Obsolete

//
//	Max values
//
#define	LSBUS_MAX_DEVICES		0x10000
#define	MAX_NR_LOGICAL_TARGETS	0x1				/* If possible, you can change this values							*/
#define MAX_NR_LOGICAL_LU		0x1				/* But you should be more thinking, more coding and more debugging	*/
#define INITIATOR_ID			(MAX_NR_LOGICAL_TARGETS+1)
#define	MAX_NR_TC_PER_TARGET	16
#define	MAX_NR_LU_OBJECT		32

#define	LSBUSACTION_START	0x00000001

#define IOCTL_BUSENUM_QUERY_NODE_ALIVE				BUSENUM_IOCTL	(0x4)
//#define IOCTL_BUSENUM_GET_LANSCSI_ADAPTER			BUSENUM_IOCTL	(0x10)		// Obsolete
//#define IOCTL_BUSENUM_RECONNECT					BUSENUM_IOCTL	(0x11)		// Obsolete
#define IOCTL_LANSCSI_ADD_TARGET					BUSENUM_IOCTL	(0x20)
#define IOCTL_LANSCSI_REMOVE_TARGET					BUSENUM_IOCTL	(0x21)
//#define IOCTL_LANSCSI_COPY_TARGET					BUSENUM_IOCTL	(0x22)		// Obsolete
//#define IOCTL_LANSCSI_ADD_TARGET_EX				BUSENUM_IOCTL	(0x23)		// Obsolete
//#define IOCTL_LANSCSI_QUERY_SLOTNO				BUSENUM_IOCTL	(0x30)		// Obsolete
#define IOCTL_LANSCSI_UPGRADETOWRITE				BUSENUM_IOCTL	(0x31)
#define IOCTL_BUSENUM_QUERY_INFORMATION				BUSENUM_IOCTL	(0x32)
#define IOCTL_LANSCSI_QUERY_LSMPINFORMATION			BUSENUM_IOCTL	(0x33)
//#define IOCTL_BUSENUM_SET_DEREFOBJ				BUSENUM_IOCTL	(0x34)		// Obsolete
#define IOCTL_BUSENUM_PLUGIN_HARDWARE_EX			BUSENUM_IOCTL	(0x41)
//#define IOCTL_BUSENUM_QUERY_ALARM_STATUS			BUSENUM_IOCTL	(0x42)		// Obsolete
#define IOCTL_LANSCSI_SETPDOINFO					BUSENUM_IOCTL	(0x43)
#define IOCTL_LANSCSI_REGISTER_NDASDEVICE			BUSENUM_IOCTL	(0x44)
#define IOCTL_LANSCSI_UNREGISTER_NDASDEVICE			BUSENUM_IOCTL	(0x45)
#define IOCTL_LANSCSI_GETVERSION					BUSENUM_IOCTL	(0x201)

#define IOCTL_VDVD_GET_VDVD_HD_INFO			BUSENUM_IOCTL (0x50)
#define IOCTL_VDVD_GET_VDVD_DISC_INFO		BUSENUM_IOCTL (0x51)
#define IOCTL_VDVD_SET_VDVD_BURN			BUSENUM_IOCTL (0x52)
#define IOCTL_VDVD_CHANGE_VDVD_DISC			BUSENUM_IOCTL (0x53)
#define IOCTL_DVD_GET_STATUS				BUSENUM_IOCTL (0x60)
#define IOCTL_DVD_SET_REFRESH				BUSENUM_IOCTL (0x61)
// added by ILGU HONG 2004_12_06
#define IOCTL_DVD_RESET_DVD_REFRESH			BUSENUM_IOCTL (0x62)
#define IOCTL_DVD_SET_WRITE_WAIT_START		BUSENUM_IOCTL (0x63)
// added by ILGU HONG 2004_12_06 End
//////////////////////////////////////////////////////////////////////////
//
//	Mirror V2 information
//  Used in _LURELATION_NODE_DESC
//  Negative sector means index from last sector(ex: -1 indicates last sector, -2 for 1 before last sector...)
//
typedef struct _INFO_RAID_1 {
	UINT64					SectorInfo; // sector where DIB_V2 exists
	UINT32					OffsetFlagInfo; // dirty flag's byte offset in DIB_V2
	UINT64					SectorBitmapStart; // sector where bitmap starts
	UINT32					SectorsPerBit; // default 128
	UINT64					SectorLastWrittenInfo; // sector where to write last written sectors
} INFO_RAID_1, *PINFO_RAID_1;

typedef struct _LSBUS_UNITDISK {
	LPX_ADDRESS	Address;
	LPX_ADDRESS	NICAddr;		// 36 bytes
	UCHAR			IsWANConnection;
	UCHAR		ucUnitNumber;
	UCHAR		Reserved1[2];	// 40 bytes
	ULONG		ulUnitBlocks;

	//
	// Added by Park,Junmo to support Version 1.1 
	//

	IN	UCHAR	ucHWType;
	IN	UCHAR	ucHWVersion;
	IN UCHAR	Reserved2[2];	// 48 bytes

	//
	// Added by Park,Junmo End
	//
	IN	unsigned _int32	iUserID;
	IN	ULONG		ulPhysicalBlocks;
	IN	unsigned _int64	iPassword;	// 64 bytes


	// RAID 1 mirror information (will be used for RAID 5 or later)
	IN union {
		INFO_RAID_1 RAID_1;
	};


} LSBUS_UNITDISK, *PLSBUS_UNITDISK;


typedef struct _LANSCSI_ADD_TARGET_DATA
{
	// Size of Parameter.
    IN	ULONG			ulSize;                          

	// LanscsiBus Slot Number.
	IN	ULONG			ulSlotNo;					// 8 bytes

	IN	ULONG			ulTargetBlocks;
	IN	ACCESS_MASK		DesiredAccess;	// 16 bytes

	IN	HANDLE			hEvent;
	
	IN	ULONG			ulNumberOfUnitDiskList; // 24 bytes
	// Disk Info.
#define DISK_TYPE_NORMAL		1					// V1 : normal || V2 nDiskCount >= 1 || no info
#define	DISK_TYPE_MIRROR		2					// V1 mirror
#define DISK_TYPE_AGGREGATION	3					// V1 aggr
#define DISK_TYPE_DVD			4					// PIdentify config(12:8) == 0x05
#define DISK_TYPE_VDVD			5					// V1 VDVD || V2 iMediaType | NDT_VDVD
#define DISK_TYPE_MO			6					// PIdentify config(12:8) == 0x07
#define DISK_TYPE_BIND_RAID1	0xc0				// V2 nDiskCount >= 2
	IN	UCHAR			ucTargetType;
	IN	UCHAR			Reserved[7];	// 32 bytes

	IN	LSBUS_UNITDISK	UnitDiskList[1];

} LANSCSI_ADD_TARGET_DATA, *PLANSCSI_ADD_TARGET_DATA;


typedef struct _LANSCSI_REMOVE_TARGET_DATA
{
    ULONG			ulSlotNo;
	LSBUS_UNITDISK	MasterUnitDisk;

} LANSCSI_REMOVE_TARGET_DATA, *PLANSCSI_REMOVE_TARGET_DATA;

/*
typedef struct _LANSCSI_COPY_TARGET_DATA
{
	LPX_ADDRESS	SourceAddress;
	UCHAR		SourceLanscsiId;

	UCHAR		UnitCount;
	ULONGLONG	UnitSizes[MAX_NR_UNIT];

	LPX_ADDRESS	DestinationAddress;
	UCHAR		DestinationLanscsiId;
                                                                        
} LANSCSI_COPY_TARGET_DATA, *PLANSCSI_COPY_TARGET_DATA;
*/

typedef struct _BUSENUM_NODE_ALIVE_IN
{
    ULONG   SlotNo;
    
} BUSENUM_NODE_ALIVE_IN, *PBUSENUM_NODE_ALIVE_IN;

typedef struct _BUSENUM_NODE_ALIVE_OUT
{
    ULONG   SlotNo;
	BOOLEAN	bAlive;
	BOOLEAN	bHasError;
    
} BUSENUM_NODE_ALIVE_OUT, *PBUSENUM_NODE_ALIVE_OUT;

#define DVD_NON_OPERATION		0
#define DVD_OPERATION			1
typedef struct _BUSENUM_DVD_STATUS
{
    ULONG   SlotNo;
	ULONG	Size;
	OUT	ULONG Status;
    
} BUSENUM_DVD_STATUS, *PBUSENUM_DVD_STATUS;

//#endif

//
//  Data structure used in PlugIn and UnPlug ioctls
//

typedef struct _BUSENUM_PLUGIN_HARDWARE
{
    //
    // sizeof (struct _BUSENUM_HARDWARE)
    //
    IN ULONG Size;                          
    
    //
    // Unique serial number of the device to be enumerated.
    // Enumeration will be failed if another device on the 
    // bus has the same serail number.
    //

    IN ULONG SlotNo;

    //
	// Added by jgahn.
	//
	IN	ULONG	MaxRequestBlocks;

	IN	PHANDLE	phEvent;

    //
    // An array of (zero terminated wide character strings). The array itself
    //  also null terminated (ie, MULTI_SZ)
    //
    
    //IN  WCHAR   HardwareIDs[]; 
	IN  WCHAR   HardwareIDs[1];		// Avoid Waring....
                                                                        
} BUSENUM_PLUGIN_HARDWARE, *PBUSENUM_PLUGIN_HARDWARE;


//
//  Data structure used in PlugIn EX ioctls
//

typedef struct _BUSENUM_PLUGIN_HARDWARE_EX
{
    //
    // sizeof (struct _BUSENUM_HARDWARE)
    //
    IN ULONG Size;                          
    
    //
    // Unique serial number of the device to be enumerated.
    // Enumeration will be failed if another device on the 
    // bus has the same serail number.
    //

    IN ULONG SlotNo;

    //
	// Added by jgahn.
	//
	IN	ULONG	MaxRequestBlocks;

	IN	PHANDLE	phEvent;
	IN  PHANDLE phAlarmEvent;
    //
    // An array of (zero terminated wide character strings). The array itself
    //  also null terminated (ie, MULTI_SZ)
    //
	IN  WCHAR   HardwareIDs[1];

} BUSENUM_PLUGIN_HARDWARE_EX, *PBUSENUM_PLUGIN_HARDWARE_EX;


typedef struct _BUSENUM_UNPLUG_HARDWARE
{
    //
    // sizeof (struct _REMOVE_HARDWARE)
    //

    IN ULONG Size;                                    

    //
    // Serial number of the device to be plugged out    
    //

    ULONG   SlotNo;
    
    ULONG Reserved[2];    

} BUSENUM_UNPLUG_HARDWARE, *PBUSENUM_UNPLUG_HARDWARE;

typedef struct _BUSENUM_EJECT_HARDWARE
{
    //
    // sizeof (struct _EJECT_HARDWARE)
    //

    IN ULONG Size;                                    

    //
    // Serial number of the device to be ejected
    //

    ULONG   SlotNo;
    
    ULONG Reserved[2];    

} BUSENUM_EJECT_HARDWARE, *PBUSENUM_EJECT_HARDWARE;

//
//	added by hootch 11172003
//
typedef struct _BUSENUM_UPGRADE_TO_WRITE
{
    //
    // sizeof (struct _BUSENUM_UPGRADE_TO_WRITE)
    //
	ULONG	Size;
    ULONG   SlotNo;

} BUSENUM_UPGRADE_TO_WRITE, *PBUSENUM_UPGRADE_TO_WRITE;


typedef struct _BUSENUM_GET_VERSION {

	USHORT	VersionMajor;
	USHORT	VersionMinor;
	USHORT	VersionBuild;
	USHORT	VersionPrivate;
	UCHAR	Reserved[16];

} BUSENUM_GET_VERSION, *PBUSENUM_GET_VERSION;

//////////////////////////////////////////////////////////////////////////
//
//	Query information on LanscsiBus
//
#define ND_ACCESS_ISRW(ACCESS) ((ACCESS & (GENERIC_READ|GENERIC_WRITE) ) == (GENERIC_READ|GENERIC_WRITE))

typedef enum {

	INFORMATION_NUMOFPDOS,
	INFORMATION_PDO,
	INFORMATION_PDOENUM,
	INFORMATION_PDOEVENT,
	INFORMATION_DRVVER

} BUSENUM_INFORMATION_CLASS;

typedef struct _BUSENUM_INFORMATION_PDO {

	ULONG	AdapterStatus;
	ULONG	DesiredAccess;
	ULONG	GrantedAccess;

} BUSENUM_INFORMATION_PDO, *PBUSENUM_INFORMATION_PDO;

typedef struct _BUSENUM_INFORMATION_PDOENUM {
	UINT32	MaxBlocksPerRequest;
	UINT32	Reserved;
#ifdef	_NTDDK_
	PKEVENT	DisconEventToService;
	PKEVENT	AlarmEventToService;
#else
	PVOID	DisconEventToService;
	PVOID	AlarmEventToService;	// 16 bytes
#endif

	LANSCSI_ADD_TARGET_DATA	AddTargetData;

} BUSENUM_INFORMATION_PDOENUM, *PBUSENUM_INFORMATION_PDOENUM;

//
//	If this flags is set, LanscsiBus returns user mode handle.
//	Otherwise, returns kernel mode object pointer.
//
#define LSBUS_QUERYFLAG_USERHANDLE		0x00000001
typedef struct _BUSENUM_INFORMATION_PDOEVENTS {

	UINT32	Flags;
	PVOID	DisconEvent;
	PVOID	AlarmEvent;
	UCHAR	Reserved[16];

} BUSENUM_INFORMATION_PDOEVENTS, *PBUSENUM_INFORMATION_PDOEVENTS;

typedef struct _BUSENUM_INFORMATION
{

    IN ULONG Size;
	BUSENUM_INFORMATION_CLASS	InfoClass;	// 8 bytes
	union {
		ULONG								NumberOfPDOs;
		BUSENUM_INFORMATION_PDO				PdoInfo;
		BUSENUM_INFORMATION_PDOENUM			PdoEnumInfo;
		BUSENUM_INFORMATION_PDOEVENTS		PdoEvents;
	};

} BUSENUM_INFORMATION, *PBUSENUM_INFORMATION;


typedef struct _BUSENUM_QUERY_INFORMATION
{
	ULONG						Size;
	BUSENUM_INFORMATION_CLASS	InfoClass;
    ULONG						SlotNo;
	UINT32						Flags;

} BUSENUM_QUERY_INFORMATION, *PBUSENUM_QUERY_INFORMATION;

//////////////////////////////////////////////////////////////////////////
//
//	Set PDO information.
//	Used by Lanscsiminiport.
//
typedef struct _BUSENUM_SETPDOINFO
{

    IN ULONG				Size;
    ULONG					SlotNo;
	ULONG					AdapterStatus;
	UINT32					DesiredAccess;
	UINT32					GrantedAccess;

} BUSENUM_SETPDOINFO, *PBUSENUM_SETPDOINFO;

//////////////////////////////////////////////////////////////////////////
//
//	NDFS Version Information
//

//
//	Increase the major version number when it does not provide different version compatibility.
//
//	3.02.1009 Beta		0x0001
//	3.02.1011 Release	0x0002
//	3.03.1013 Release	0x0002
//
#define NDFS_COMPAT_VERSION	((USHORT)0x0002)
//
//	Increase the minor version number whenever NDFS protocol changes.
//
#define NDFS_VERSION		((USHORT)0x0001)


/*
	Virtual DVD disk format list
*/

#define DEFAULT_DISC_SIZE	(10 * (1<<21))		//10G
#define DEFAULT_HDINFO_SIZE	(1<<11)				// 1M == (header) + disc_info *19
#define DISC_MAGIC			0x19191919
#define DISC_VERSION		0x00000002

typedef struct _DISC_HEADER {
	unsigned _int32		MAGIC;
	unsigned _int32		VERSION;
	unsigned _int32		DISCS;
	unsigned _int32		ENABLE_DISCS;			// # used vitual dvd
	unsigned _int32		HISTORY;				// # reviced action
} DISC_HEADER, *PDISC_HEADER;


typedef struct _DISC_INFO {
	unsigned _int64		LOCATION;
	unsigned _int64		START_SEC;
	unsigned _int32		NR_SEC;
	unsigned _int32		ENABLED;
	char				INFO[1];
}DISC_INFO, *PDISC_INFO;


typedef struct _JUKE_DISC {
	unsigned _int64		LOCATION;
	unsigned _int64		START_SEC;
	unsigned _int32		NR_SEC;
	unsigned _int32		ENABLED;
	char				NAME[50];
}JUKE_DISC, *PJUKE_DISC;

/*
  Define IOCTL_INTERFACE
*/
typedef struct _VDVD_DISC_HD{
	ULONG   SlotNo;
	ULONG	DISCS;
	ULONG	ENABLE_DISCS;
}VDVD_DISC_HD, *PVDVD_DISC_HD;


#define COM_GET_VDVD_COUNT			1
#define COM_GET_VDVD_HD_INFO		2
#define COM_GET_VDVD_HD_INFO_ALL	3

typedef struct _GET_VDVD_HD_INFO {
	ULONG			Size;          
	ULONG			COM;
	ULONG			COUNT;
	ULONG			SlotNo;
	VDVD_DISC_HD	DISC[1];
} GET_VDVD_HD_INFO, *PGET_VDVD_HD_INFO;

#define COM_GET_VDVD_DISC_INFO		1
#define COM_VDVD_DISC_INFO_ALL		2

typedef struct _GET_VDVD_DISC_INFO{
	ULONG			Size; 
	ULONG			COM;
	ULONG			COUNT;
	ULONG			SlotNo;
	ULONG			DiscNo;
	DISC_INFO		DISC[1];
} GET_VDVD_DISC_INFO, *PGET_VDVD_DISC_INFO;

#define COM_SET_DISC_BURN			1
#define COM_SET_DISC_BURN_END		2
typedef struct _SET_VDVD_DISC_BURN{
	ULONG			Size; 
	ULONG			COM;
	ULONG			SlotNo;
	JUKE_DISC		DISC;
}SET_DVD_DISC_BURN, *PSET_DVD_DISC_BURN;

#define COM_GET_CUR_DISC			1
#define COM_SET_CUR_DISC			2
typedef struct _CHANGE_VDVD_DISC{
	ULONG			Size; 
	ULONG			COM;
	ULONG			SlotNo;
	ULONG			DiscNo;
}CHANGE_VDVD_DISC, *PCHANGE_VDVD_DISC;

//////////////////////////////////////////////////////////////////////////
//
//	Basic types
//
#if defined(_X86_)
typedef short int			INT16, *PINT16;
typedef unsigned short int	UINT16, *PUINT16;
typedef unsigned char		BYTE, *PBYTE;
#else
#error "INT16 and UINT16 is only defined for X86"

#endif

#endif

