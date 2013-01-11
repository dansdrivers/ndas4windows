#ifndef __DISKINFO_H__
#define __DISKINFO_H__
#pragma once

//
//	disk information format
//
#include <pshpack1.h>

#define	DISK_INFORMATION_SIGNATURE	0xFE037A4E

#define DISK_INFORMATION_USAGE_TYPE_HOME	0x00
#define DISK_INFORMATION_USAGE_TYPE_OFFICE	0x10

typedef struct _DISK_INFORMATION_BLOCK {

	unsigned _int32	Signature;
	
	unsigned _int8	MajorVersion;
	unsigned _int8	MinorVersion;
	unsigned _int8	reserved1[2];

	unsigned _int32	Sequence;

	unsigned _int8	EtherAddress[6];
	unsigned _int8	UnitNumber;
	unsigned _int8	reserved2;

	unsigned _int8	DiskType;
	unsigned _int8	PeerAddress[6];
	unsigned _int8	PeerUnitNumber;
	unsigned _int8	reserved3;

	unsigned _int8	UsageType;
	unsigned _int8	reserved4[3];

	unsigned char reserved5[512 - 20];

	unsigned _int32	Checksum;

} DISK_INFORMATION_BLOCK, *PDISK_INFORMATION_BLOCK;

#include <poppack.h>

#endif /* __DISKINFO_H__ */