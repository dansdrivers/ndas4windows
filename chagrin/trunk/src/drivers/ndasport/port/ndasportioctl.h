#ifndef _NDASPORT_IOCTL_H_
#define _NDASPORT_IOCTL_H_
#pragma once

#include <pshpack8.h>

#define IOCTL_NDASPORT_BASE FILE_DEVICE_NETWORK

#define NDASPORT_ENUMERATOR L"{56552ED5-F57A-4A9D-B13E-251A838E5D7B}"

#define NDASPORT_IOCTL_ANY(_index_) \
	CTL_CODE(IOCTL_NDASPORT_BASE, _index_, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define NDASPORT_IOCTL_RW(_index_) \
	CTL_CODE(IOCTL_NDASPORT_BASE, _index_, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

#define NDASPORT_IOCTL_READ(_index_) \
	CTL_CODE(IOCTL_NDASPORT_BASE, _index_, METHOD_BUFFERED, FILE_READ_ACCESS)

//
// IOCTL's for Port Device Object
//

// Input : NDAS_LOGICALUNIT_DESCRIPTOR
// Output: None
//
#define IOCTL_NDASPORT_PLUGIN_LOGICALUNIT            NDASPORT_IOCTL_RW(0x800)
//
// Input : NDAS_LOGICALUNIT_ADDRESS
// Output: None
//
#define IOCTL_NDASPORT_EJECT_LOGICALUNIT             NDASPORT_IOCTL_RW(0x801)
//
// Input : NDAS_LOGICALUNIT_ADDRESS
// Output: None
//
#define IOCTL_NDASPORT_UNPLUG_LOGICALUNIT            NDASPORT_IOCTL_RW(0x802)
//
// Input : NDAS_LOGICALUNIT_ADDRESS
// Output: None
//
#define IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE NDASPORT_IOCTL_ANY(0x803)
//
// Input : NDAS_LOGICALUNIT_ADDRESS
// Output: NDAS_LOGICALUNIT_DESCRIPTOR
// OutputBufferLength: at least sizeof(NDAS_LOGICALUNIT_ADDRESS)
//
#define IOCTL_NDASPORT_GET_LOGICALUNIT_DESCRIPTOR    NDASPORT_IOCTL_READ(0x804)
//
// Input : None
// Output: ULONG (Port Number, cast to UCHAR in LOGICAL_UNIT_ADDRESS)
// OutputBufferLength: sizeof(ULONG)
//
#define IOCTL_NDASPORT_GET_PORT_NUMBER               NDASPORT_IOCTL_READ(0x805)

//
// IOCTL's for LogicalUnit Device Object
//

// Input : None
// Output: Nonoe if OutputBufferLength is less than sizeof(GUID), but success
//         Otherwise NDASPORT_LOGICALUNIT_IDENTITY_GUID is filled
//         in the output buffer
// OutputBufferLength: recommended to provide at least sizeof(GUID)
//
#define IOCTL_NDASPORT_LOGICALUNIT_EXIST          NDASPORT_IOCTL_ANY(0x8F0)
//
// Input : None
// Output: NDAS_LOGICALUNIT_ADDRESS
// OutputBufferLength: sizeof(NDAS_LOGICALUNIT_ADDRESS)
//
#define IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS    NDASPORT_IOCTL_READ(0x8F1)
//
// Input : None 
// Output: NDAS_LOGICALUNIT_DESCRIPTOR
// OutputBufferLength: at least sizeof(NDAS_LOGICALUNIT_ADDRESS)
//
//  First, set the output buffer size as sizeof(NDAS_LOGICALUNIT_DESCRIPTOR)
//  Then, reissue the IOCTL with the buffer size as specified in Descriptor->Size
//  to obtain the actual full descriptor
//
#define IOCTL_NDASPORT_LOGICALUNIT_GET_DESCRIPTOR NDASPORT_IOCTL_READ(0x8F3)
//
// Input:  SRB_IO_CONTROL + additional data
// Output: SRB_IO_CONTROL + additional data
//
#define IOCTL_NDASPORT_LOGICALUNIT_IOCTL_READ  NDASPORT_IOCTL_READ(0x8F4)
#define IOCTL_NDASPORT_LOGICALUNIT_IOCTL_WRITE NDASPORT_IOCTL_READ(0x8F5)
#define IOCTL_NDASPORT_LOGICALUNIT_IOCTL_ANY   NDASPORT_IOCTL_READ(0x8F6)

#pragma warning(disable: 4201) // nameless union

typedef union _NDAS_LOGICALUNIT_ADDRESS {
	struct {
		UCHAR PortNumber;
		UCHAR PathId;
		UCHAR TargetId;
		UCHAR Lun;
	};
	ULONG Address;
} NDAS_LOGICALUNIT_ADDRESS, *PNDAS_LOGICALUNIT_ADDRESS;

#pragma warning(default: 4201)

C_ASSERT(sizeof(NDAS_LOGICALUNIT_ADDRESS) == 4);

typedef enum _NDAS_LOGICALUNIT_TYPE {
	NdasUnspecifiedDevice = 0x00000000,
	NdasDiskDevice,
	NdasAtaDevice,
	NdasAtapiDevice,
	NdasAggregatedDisk,
	NdasMirroredDisk, /* obsolete, do not use */
	NdasRAID0,
	NdasRAID1,
	NdasRAID4,
	NdasRAID5,
	NdasVirtualDVD,
	NdasExternalType = 0x80000000,
	MakeNdasLogicalUnitTypeAsULong = 0xFFFFFFFF
} NDAS_LOGICALUNIT_TYPE, *PNDAS_LOGICALUNIT_TYPE;

C_ASSERT(sizeof(NDAS_LOGICALUNIT_TYPE) == 4);

typedef enum _NDAS_LOGICALUNIT_FLAGS {
	NDAS_LOGICALUNIT_FLAG_PERSISTENT_UNIT  = 0x00000001
} NDAS_LOGICALUNIT_FLAGS;

typedef struct _NDAS_LOGICALUNIT_DESCRIPTOR {
	ULONG Version; /* sizeof(NDASPORT_LOGICALUNIT_DESCRIPTOR) */
	ULONG Size;    /* sizeof entire logical unit descriptor */
	NDAS_LOGICALUNIT_ADDRESS Address;
	NDAS_LOGICALUNIT_TYPE Type;	
	ULONG Flags;
	ULONG Reserved;
	GUID ExternalTypeGuid; /* ndasport_{XXXX} */
} NDAS_LOGICALUNIT_DESCRIPTOR, *PNDAS_LOGICALUNIT_DESCRIPTOR;

C_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == 24 + 16);

typedef struct _NDASPORT_LOGICALUNIT_EJECT {
	ULONG Size;
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;
	ULONG Flags;
	ULONG Reserved;
} NDASPORT_LOGICALUNIT_EJECT, *PNDASPORT_LOGICALUNIT_EJECT;

C_ASSERT(sizeof(NDASPORT_LOGICALUNIT_EJECT) == 16);

typedef struct _NDASPORT_LOGICALUNIT_UNPLUG {
	ULONG Size;
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;
	ULONG Flags;
	ULONG Reserved;
} NDASPORT_LOGICALUNIT_UNPLUG, *PNDASPORT_LOGICALUNIT_UNPLUG;

C_ASSERT(sizeof(NDASPORT_LOGICALUNIT_UNPLUG) == 16);

typedef enum _NDASPORT_PNP_NOTIFICATION_TYPE {
	NdasPortLogicalUnitIsReady,
	NdasPortLogicalUnitIsRemoved,
	_MakeNdasPnpNotificationTypeAsULong = 0xFFFFFFFF
} NDASPORT_PNP_NOTIFICATION_TYPE;

C_ASSERT(sizeof(NDASPORT_PNP_NOTIFICATION_TYPE) == 4);

typedef struct _NDASPORT_PNP_NOTIFICATION {
	NDASPORT_PNP_NOTIFICATION_TYPE Type;
	NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress;
} NDASPORT_PNP_NOTIFICATION, *PNDASPORT_PNP_NOTIFICATION;

C_ASSERT(sizeof(NDASPORT_PNP_NOTIFICATION) == 8);

#include <poppack.h>

#endif /* _NDASPORT_IOCTL_H_ */
