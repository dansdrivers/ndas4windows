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
#include "lurdesc.h"


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
#define LSMINIPORT_HARDWARE_IDS_A "NDAS\\SCSIAdapter_R01\0"

#define WIDEN_STRING2(x) L ## x
#define WIDEN_STRING(x) WIDEN_STRING2(x)

#define LSMINIPORT_HARDWARE_IDS_W WIDEN_STRING(LSMINIPORT_HARDWARE_IDS_A)

#define LSMINIPORT_HARDWARE_IDS_A_SIZE sizeof(LSMINIPORT_HARDWARE_IDS_A)
#define LSMINIPORT_HARDWARE_IDS_W_SIZE sizeof(LSMINIPORT_HARDWARE_IDS_W)


#define FILE_DEVICE_BUSENUM         FILE_DEVICE_BUS_EXTENDER

#define BUSENUM_IOCTL(_index_) \
    CTL_CODE (FILE_DEVICE_BUSENUM, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

//#define IOCTL_BUSENUM_PLUGIN_HARDWARE				BUSENUM_IOCTL	(0x0)		// Obsolete
#define IOCTL_BUSENUM_UNPLUG_HARDWARE               BUSENUM_IOCTL	(0x1)
#define IOCTL_BUSENUM_EJECT_HARDWARE                BUSENUM_IOCTL	(0x2)
//#define IOCTL_TOASTER_DONT_DISPLAY_IN_UI_DEVICE	BUSENUM_IOCTL	(0x3)		// Obsolete
#define IOCTL_BUSENUM_QUERY_NODE_ALIVE				BUSENUM_IOCTL	(0x4)
//#define IOCTL_BUSENUM_GET_LANSCSI_ADAPTER			BUSENUM_IOCTL	(0x10)		// Obsolete
//#define IOCTL_BUSENUM_RECONNECT					BUSENUM_IOCTL	(0x11)		// Obsolete
#define IOCTL_LANSCSI_ADD_TARGET					BUSENUM_IOCTL	(0x20)
#define IOCTL_LANSCSI_REMOVE_TARGET					BUSENUM_IOCTL	(0x21)
//#define IOCTL_LANSCSI_COPY_TARGET					BUSENUM_IOCTL	(0x22)		// Obsolete
//#define IOCTL_LANSCSI_ADD_TARGET_EX				BUSENUM_IOCTL	(0x23)		// Obsolete
//#define IOCTL_LANSCSI_RECOVER_TARGET				BUSENUM_IOCTL	(0x24)		// Obsolete
//#define IOCTL_LANSCSI_QUERY_SLOTNO				BUSENUM_IOCTL	(0x30)		// Obsolete
#define IOCTL_LANSCSI_UPGRADETOWRITE				BUSENUM_IOCTL	(0x31)
#define IOCTL_BUSENUM_QUERY_INFORMATION				BUSENUM_IOCTL	(0x32)
#define IOCTL_NDASBUS_QUERY_NDASSCSIINFO			BUSENUM_IOCTL	(0x33)
//#define IOCTL_BUSENUM_SET_DEREFOBJ				BUSENUM_IOCTL	(0x34)		// Obsolete
//#define IOCTL_BUSENUM_PLUGIN_HARDWARE_EX			BUSENUM_IOCTL	(0x41)		// Obsolete
//#define IOCTL_BUSENUM_QUERY_ALARM_STATUS			BUSENUM_IOCTL	(0x42)		// Obsolete
#define IOCTL_LANSCSI_SETPDOINFO					BUSENUM_IOCTL	(0x43)
#define IOCTL_LANSCSI_REGISTER_DEVICE				BUSENUM_IOCTL	(0x44)
#define IOCTL_LANSCSI_UNREGISTER_DEVICE				BUSENUM_IOCTL	(0x45)
#define IOCTL_LANSCSI_QUERY_REGISTER				BUSENUM_IOCTL	(0x46)
#define	IOCTL_LANSCSI_REDIRECT_NDASSCSI				BUSENUM_IOCTL	(0x47)
#define IOCTL_BUSENUM_PLUGIN_HARDWARE_EX2			BUSENUM_IOCTL	(0x48)
#define IOCTL_LANSCSI_REGISTER_TARGET				BUSENUM_IOCTL	(0x49)
#define IOCTL_LANSCSI_UNREGISTER_TARGET				BUSENUM_IOCTL	(0x4a)
#define IOCTL_LANSCSI_STARTSTOP_REGISTRARENUM		BUSENUM_IOCTL	(0x4b)
#define IOCTL_LANSCSI_GETVERSION					BUSENUM_IOCTL	(0x201)

#define IOCTL_VDVD_GET_VDVD_HD_INFO			BUSENUM_IOCTL (0x50)
#define IOCTL_VDVD_GET_VDVD_DISC_INFO		BUSENUM_IOCTL (0x51)
#define IOCTL_VDVD_SET_VDVD_BURN			BUSENUM_IOCTL (0x52)
#define IOCTL_VDVD_CHANGE_VDVD_DISC			BUSENUM_IOCTL (0x53)
#define IOCTL_DVD_GET_STATUS				BUSENUM_IOCTL (0x60)


//
//	Max values
//
#define	LSBUS_MAX_DEVICES		0x10000
#define	MAX_NR_LOGICAL_TARGETS	0x1				/* If possible, you can change this values							*/
#define MAX_NR_LOGICAL_LU		0x1				/* But you should be more thinking, more coding and more debugging	*/
#define INITIATOR_ID			(MAX_NR_LOGICAL_TARGETS+1)
#define	MAX_NR_TC_PER_TARGET	16
#define	MAX_NR_LU_OBJECT		32
#define	LSREGISTER_MAX_HARDWAREIDS_LENGTH		256

#define	LSBUSACTION_START	0x00000001

//////////////////////////////////////////////////////////////////////////
//
//	Structures
//

//
//	UnitDisk
//

typedef struct _LSBUS_UNITDISK {
	LPX_ADDRESS		Address;
	LPX_ADDRESS		NICAddr;		// 36 bytes
	UCHAR			IsWANConnection;
	UCHAR			ucUnitNumber;
	UCHAR			Reserved1[2];	// 40 bytes

	UINT64			ulUnitBlocks;
	UINT64			ulPhysicalBlocks;

	UCHAR			ucHWType;
	UCHAR			ucHWVersion;
	UINT16			ucHWRevision;		// 48 bytes

	unsigned _int32	iUserID;
	unsigned _int64	iPassword;			// 64 bytes

	//
	//	Override default settings.
	//	Reference to lurdesc.h
	//
	ULONG			LurnOptions;

	//
	//	Reconnection parameters
	//
	//	ReconnTrial: Number of reconnection trial
	//	ReconnInterval:	Reconnection interval in milliseconds.
	//
	//	NOTE: Valid with LURNOPTION_SET_RECONNECTION option.
	//
	ULONG			ReconnTrial;
	ULONG			ReconnInterval;


	//
	//	Restrict UDMA mode.
	//	Set 0xff to disable UDMA
	//

	UCHAR			UDMARestrict;
	UCHAR			Reserved[3];

	//
	//	Max data send/receive length
	//

	ULONG			UnitMaxDataSendLength;
	ULONG			UnitMaxDataRecvLength;

} LSBUS_UNITDISK, *PLSBUS_UNITDISK;

//
// Device types to be set in ucTargetType field.
//

#define NDASSCSI_TYPE_DISK_NORMAL		0x01	// V1 : normal || V2 nDiskCount >= 1 || no info
#define NDASSCSI_TYPE_DISK_MIRROR		0x02	// V1 mirror
#define NDASSCSI_TYPE_DISK_AGGREGATION	0x03	// V1 aggr
#define NDASSCSI_TYPE_DISK_RAID0		0x06	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID1		0x04	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4		0x05	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_DISK_RAID1R2		0x0A	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4R2		0x0B	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_DISK_RAID1R3		0x0C	// V2 nDiskCount >= 2
#define NDASSCSI_TYPE_DISK_RAID4R3		0x0D	// V2 nDiskCount >= 3
#define NDASSCSI_TYPE_AOD				0x08	// Append only disk
#define NDASSCSI_TYPE_VDVD				0x64	// V1 VDVD || V2 iMediaType | NDT_VDVD
#define NDASSCSI_TYPE_DVD				0x65	// PIdentify config(12:8) == 0x05
#define NDASSCSI_TYPE_MO				0x66	// PIdentify config(12:8) == 0x07

#define NDASBUS_SLOT_ALL				(-1)

typedef struct _LANSCSI_ADD_TARGET_DATA
{
	// Size of Parameter.
    ULONG			ulSize;

	// LanscsiBus Slot Number.
	ULONG			ulSlotNo;				// 8 bytes

	UINT64			ulTargetBlocks;
	NDAS_DEV_ACCESSMODE	DeviceMode;			// 16 bytes

	ULONG			ulNumberOfUnitDiskList; // 24 bytes

	UCHAR			ucTargetType;
	UCHAR			ucTargetId;
	UCHAR			Reserved[4];			// 30 bytes

	//
	//	Content encryption key.
	//	Key length in bytes = CntEcrKeyLength.
	//	If CntEcrMethod is zero, there is no content encryption.
	//	If key length is zero, there is no content encryption.
	//
	UCHAR			CntEcrMethod;
	UCHAR			CntEcrKeyLength;
	UCHAR			CntEcrKey[NDAS_CONTENTENCRYPT_KEY_LENGTH];

	//
	//	Offset to block access control list from the beginning of the structure
	//	Length of the block access control list is included
	//	in the size field of structure.
	//	Block access control list structure defined in lurdesc.h 
	//
	UINT32				BACLOffset;

	//
	//	LU relation options.
	//	Reference to lurdesc.h
	//
	UINT32				LurOptions;

	//
	// RAID information
	//
	//	Reference to lurdesc.h
	//
	INFO_RAID			RAID_Info;

	//
	//	UnitDisk list
	//
	LSBUS_UNITDISK	UnitDiskList[1];

} LANSCSI_ADD_TARGET_DATA, *PLANSCSI_ADD_TARGET_DATA;

//
//	Verify size field of AddTargetData structure
//	return zero if verification fails.
//

__inline
int
VerifySizeOfAddTargetData(
	IN PLANSCSI_ADD_TARGET_DATA		AddTargetData,
	OUT PULONG						Length
){
	ULONG	lengthAddTargetData;

	lengthAddTargetData = 0;

	if(AddTargetData == NULL)
		return 0;
	lengthAddTargetData = FIELD_OFFSET(LANSCSI_ADD_TARGET_DATA, UnitDiskList)
		+ sizeof(LSBUS_UNITDISK) * AddTargetData->ulNumberOfUnitDiskList;
	if(AddTargetData->BACLOffset) {
		PNDAS_BLOCK_ACL	ndasBACL;
		ULONG			baclLen;

		ndasBACL = (PNDAS_BLOCK_ACL)((PUCHAR)AddTargetData + AddTargetData->BACLOffset);
		baclLen= FIELD_OFFSET(NDAS_BLOCK_ACL, BlockACEs)
			+ sizeof(NDAS_BLOCK_ACE) * ndasBACL->BlockACECnt;
		 lengthAddTargetData += baclLen;

		if(ndasBACL->Length != baclLen)
			return 0;
	}

	if(AddTargetData->ulSize != lengthAddTargetData)
		return 0;

	if(Length) {
		*Length = lengthAddTargetData;
	}

	return 1;
}




typedef struct _LANSCSI_REMOVE_TARGET_DATA
{
    ULONG			ulSlotNo;
	ULONG			ulTargetId;
	UCHAR			Reserved[4];
	LSBUS_UNITDISK	MasterUnitDisk;

} LANSCSI_REMOVE_TARGET_DATA, *PLANSCSI_REMOVE_TARGET_DATA;

typedef struct _LANSCSI_UNREGISTER_NDASDEV
{
    ULONG			SlotNo;
	UCHAR			Reserved[4];

} LANSCSI_UNREGISTER_NDASDEV, *PLANSCSI_UNREGISTER_NDASDEV;

typedef struct _LANSCSI_UNREGISTER_TARGET
{
    ULONG			SlotNo;
	ULONG			TargetId;
	UCHAR			Reserved[4];

} LANSCSI_UNREGISTER_TARGET, *PLANSCSI_UNREGISTER_TARGET;

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

	IN ULONG	SlotNo;
	IN ULONG	Size;
	OUT	ULONG	Status;

} BUSENUM_DVD_STATUS, *PBUSENUM_DVD_STATUS;

//#endif

//
//  Data structure used in PlugIn and UnPlug ioctls
//


//
//  Structure used in PlugIn EX2 ioctls
//	NOTE: Whenever updating the structure, update 32 version and thunking code as well.
//

#define	PLUGINFLAG_NOT_REGISTER	0x00000001

typedef struct _BUSENUM_PLUGIN_HARDWARE_EX2
{
	//
	// sizeof (struct _BUSENUM_HARDWARE_EX2)
	//
	IN ULONG Size;                          

	//
	// Unique serial number of the device to be enumerated.
	// Enumeration will be failed if another device on the 
	// bus has the same serial number.
	//
	IN ULONG SlotNo;

	//
	//	Specify Windows IO request byte length for the SCSI adapter.
	//	If zero, the SCSI adapter uses the default value.
	//
	IN	ULONG	MaxOsDataTransferLength;

	//
	//	Disconnection event
	//
	IN	HANDLE	DisconEvent;

	//
	//	Alarm event
	//
	IN  HANDLE AlarmEvent;

	//
	//	Flags
	//
	IN	ULONG	Flags;

	//
	//	Reserved
	//
	IN	UCHAR	Reserved[8];

	//
	// An array of (zero terminated wide character strings). The array itself
	//  also null terminated (ie, MULTI_SZ)
	//
	IN	ULONG	HardwareIDLen;
	IN  WCHAR   HardwareIDs[1];

} BUSENUM_PLUGIN_HARDWARE_EX2, *PBUSENUM_PLUGIN_HARDWARE_EX2;


//
//  Structure used in PlugIn EX2 for 32 bit applications
//

typedef struct _BUSENUM_PLUGIN_HARDWARE_EX2_32
{
	//
	// sizeof (struct _BUSENUM_HARDWARE_EX2)
	//
	IN ULONG Size;                          

	//
	// Unique serial number of the device to be enumerated.
	// Enumeration will be failed if another device on the 
	// bus has the same serial number.
	//
	IN ULONG SlotNo;

	//
	//	Specify Windows IO request byte length for the SCSI adapter.
	//	If zero, the SCSI adapter uses the default value.
	//
	IN	ULONG	MaxOsDataTransferLength;

	//
	//	Disconnection event
	//
	IN	VOID*POINTER_32	DisconEvent;

	//
	//	Alarm event
	//
	IN  VOID*POINTER_32 AlarmEvent;

	//
	//	Flags
	//
	IN	ULONG	Flags;

	//
	//	Reserved
	//
	IN	UCHAR	Reserved[8];

	//
	// An array of (zero terminated wide character strings). The array itself
	//  also null terminated (ie, MULTI_SZ)
	//
	IN	ULONG	HardwareIDLen;
	IN  WCHAR   HardwareIDs[1];

} BUSENUM_PLUGIN_HARDWARE_EX2_32, *PBUSENUM_PLUGIN_HARDWARE_EX2_32;

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

typedef enum {

	INFORMATION_NUMOFPDOS,
	INFORMATION_PDO,
	INFORMATION_PDOENUM,
	INFORMATION_PDOEVENT,
	INFORMATION_ISREGISTERED,
	INFORMATION_PDOSLOTLIST,
	INFORMATION_PDOFILEHANDLE

} BUSENUM_INFORMATION_CLASS;

typedef struct _BUSENUM_INFORMATION_PDO {

	ULONG				AdapterStatus;
	NDAS_DEV_ACCESSMODE	DeviceMode;
	NDAS_FEATURES		SupportedFeatures;
	NDAS_FEATURES		EnabledFeatures;

} BUSENUM_INFORMATION_PDO, *PBUSENUM_INFORMATION_PDO;


//
//	PDO enumeration information
//

#define PDOENUM_FLAG_LURDESC			0x00000001
#define PDOENUM_FLAG_DRV_NOT_INSTALLED	0x00000002
typedef struct _BUSENUM_INFORMATION_PDOENUM {
	UINT32	MaxRequestLength;	// if zero byte, the adapter use the default value.
	UINT32	Flags;
#ifdef	_NTDDK_
	PKEVENT	DisconEventToService;
	PKEVENT	AlarmEventToService;
#else
	PVOID	DisconEventToService;
	PVOID	AlarmEventToService;
#endif
	BYTE	AddDevInfo[1];

} BUSENUM_INFORMATION_PDOENUM, *PBUSENUM_INFORMATION_PDOENUM;


//
//	If this flags is set, LanscsiBus returns user mode handle.
//	Otherwise, returns kernel mode object pointer.
//
//	NOTE: Whenever updating the structure, update 32 version and thunking code as well.
//

#define LSBUS_QUERYFLAG_USERHANDLE		0x00000001
typedef struct _BUSENUM_INFORMATION_PDOEVENTS {

	UINT32	SlotNo;
	UINT32	Flags;
	PVOID	DisconEvent;
	PVOID	AlarmEvent;
	UCHAR	Reserved[8];

} BUSENUM_INFORMATION_PDOEVENTS, *PBUSENUM_INFORMATION_PDOEVENTS;

//	for 32bit-pointer application
typedef struct _BUSENUM_INFORMATION_PDOEVENTS_32 {

	UINT32			SlotNo;
	UINT32			Flags;
	VOID*POINTER_32	DisconEvent;
	VOID*POINTER_32	AlarmEvent;
	UCHAR			Reserved[8];

} BUSENUM_INFORMATION_PDOEVENTS_32, *PBUSENUM_INFORMATION_PDOEVENTS_32;

//
//	a file handle of PDO
//	NOTE: Whenever updating the structure, update 32 version and thunking code as well.
//
typedef struct _BUSENUM_INFORMATION_PDOFILEHANDLE {

	UINT32	SlotNo;
	UINT32	Flags;
	PVOID	PdoFileHandle;
	UCHAR	Reserved[8];

} BUSENUM_INFORMATION_PDOFILEHANDLE, *PBUSENUM_INFORMATION_PDOFILEHANDLE;

//
//	a file handle of PDO for 32bit-pointer application
//
typedef struct _BUSENUM_INFORMATION_PDOFILEHANDLE_32 {

	UINT32			SlotNo;
	UINT32			Flags;
	VOID*POINTER_32	PdoFileHandle;
	UCHAR			Reserved[8];

} BUSENUM_INFORMATION_PDOFILEHANDLE_32, *PBUSENUM_INFORMATION_PDOFILEHANDLE_32;

typedef struct _BUSENUM_INFORMATION_PDOSLOTLIST {

	UINT32	SlotNoCnt;
	UINT32	SlotNo[1];

} BUSENUM_INFORMATION_PDOSLOTLIST, *PBUSENUM_INFORMATION_PDOSLOTLIST;

typedef struct _BUSENUM_INFORMATION
{

    IN ULONG Size;
	BUSENUM_INFORMATION_CLASS	InfoClass;	// 8 bytes
	union {
		ULONG									NumberOfPDOs;
		BUSENUM_INFORMATION_PDO					PdoInfo;
		BUSENUM_INFORMATION_PDOENUM				PdoEnumInfo;
		BUSENUM_INFORMATION_PDOEVENTS			PdoEvents;
		BUSENUM_INFORMATION_PDOEVENTS_32		PdoEvents32;
		BUSENUM_INFORMATION_PDOSLOTLIST			PdoSlotList;
		ULONG									IsRegistered;
		BUSENUM_INFORMATION_PDOFILEHANDLE		PdoFile;
		BUSENUM_INFORMATION_PDOFILEHANDLE_32	PdoFile32;
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
//	Redirect a IOCTL to a NDASSCSI device object.
//

typedef struct _BUSENUM_REDIRECT_NDASSCSI {

	IN UINT32	Size;
	IN UINT32	SlotNo;
	IN UINT32	IoctlCode;
	IN UINT32	IoctlDataSize;
	IN UCHAR	IoctlData[1];

} BUSENUM_REDIRECT_NDASSCSI, *PBUSENUM_REDIRECT_NDASSCSI;


//////////////////////////////////////////////////////////////////////////
//
//	Set PDO information.
//	Used by NDASSCSI.
//

typedef struct _BUSENUM_SETPDOINFO
{

    IN ULONG	Size;
    IN ULONG	SlotNo;
	IN ULONG				AdapterStatus;
	IN NDAS_DEV_ACCESSMODE	DeviceMode;
	IN NDAS_FEATURES		SupportedFeatures;
	IN NDAS_FEATURES		EnabledFeatures;

} BUSENUM_SETPDOINFO, *PBUSENUM_SETPDOINFO;


//////////////////////////////////////////////////////////////////////////
//
//	Virtual DVD disk format list
//
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


#endif
