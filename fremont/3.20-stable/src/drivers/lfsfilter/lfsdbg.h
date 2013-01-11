#ifndef __LFS_DBG_H__
#define __LFS_DBG_H__

#if DBG
#define MEMORY_CHECK_SIZE	0 //	((UCHAR)128) //	should be < 255
#else
#define MEMORY_CHECK_SIZE	0
#endif

struct _ObjectCounts 
{
	LONG	LfsDeviceExtCount;

	LONG	PrimarySessionCount;
	LONG	PrimarySessionThreadCount;
	LONG	OpenFileCount;
	
	LONG	SecondaryCount;
	LONG	SecondaryThreadCount;
	LONG	SecondaryRequestCount;
	LONG	FcbCount;
	LONG	FileExtCount;
	
	LONG	EnabledNetdiskCount;
	LONG	NetdiskPartitionCount;
	LONG	NetdiskManagerRequestCount;
	
	LONG	StoppedVolumeCount;
	LONG	RedirectIrpCount;

};

#if DBG
extern struct _ObjectCounts LfsObjectCounts;
#endif

extern PCHAR	IrpMajors[];
extern char* AttributeTypeCode[];

#endif