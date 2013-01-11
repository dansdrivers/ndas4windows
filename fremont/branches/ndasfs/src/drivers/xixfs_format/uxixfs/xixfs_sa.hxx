#ifndef XIXFSSA
#define XIXFSSA

#include "drive.hxx"
#include "supera.hxx"
#include "hmem.hxx"
//
//      Forward references
//


DECLARE_CLASS(	XIXFS_SA		);


class XIXFS_SA: public SUPERAREA {

        public:

		UXIXFS_EXPORT
        DECLARE_CONSTRUCTOR( XIXFS_SA );

		UXIXFS_EXPORT
        ~XIXFS_SA(
            );

        NONVIRTUAL
        UXIXFS_EXPORT
        BOOLEAN
        Initialize(
            IN OUT  PLOG_IO_DP_DRIVE    Drive,
            IN OUT  PMESSAGE            Message
            );

 
        NONVIRTUAL
        PVOID
        GetBuf(
            );

        NONVIRTUAL
        BOOLEAN
        Create(
            IN      PCNUMBER_SET    BadSectors,
            IN OUT  PMESSAGE        Message,
            IN      PCWSTRING       Label                DEFAULT NULL,
            IN      ULONG           Flags                DEFAULT FORMAT_BACKWARD_COMPATIBLE,
            IN      ULONG           ClusterSize          DEFAULT 0,
            IN      ULONG           VirtualSize          DEFAULT 0
            ) ;

        NONVIRTUAL
        BOOLEAN
        VerifyAndFix(
            IN      FIX_LEVEL   FixLevel,
            IN OUT  PMESSAGE    Message,
            IN      ULONG       Flags           DEFAULT FALSE,
            IN      ULONG       LogFileSize     DEFAULT 0,
            IN      USHORT      Algorithm       DEFAULT 0,
            OUT     PULONG      ExitStatus      DEFAULT NULL,
            IN      PCWSTRING   DriveLetter     DEFAULT NULL
            ) ;

        NONVIRTUAL
        BOOLEAN
        RecoverFile(
            IN      PCWSTRING   FullPathFileName,
            IN OUT  PMESSAGE    Message
            ) ;

        NONVIRTUAL
        PARTITION_SYSTEM_ID
        QuerySystemId(
            ) CONST ;

        NONVIRTUAL
        VOID
        PrintFormatReport (
            IN OUT PMESSAGE Message,
            IN     PFILE_FS_SIZE_INFORMATION    FsSizeInfo,
            IN     PFILE_FS_VOLUME_INFORMATION  FsVolInfo
            ) ;


		NONVIRTUAL
		VOID
		XixFsSetBit(unsigned __int64 bitIndex, volatile void * Map);


		NONVIRTUAL
		VOID
		XixFsClearBit(unsigned __int64 bitIndex, volatile void * Map);


		NONVIRTUAL
		unsigned int
		XixFsTestBit(unsigned __int64 bitIndex, volatile void *Map);


    private:

		NONVIRTUAL
		LARGE_INTEGER
		GetXixFsTime();



		NONVIRTUAL
		unsigned __int64
		SectorAddressOfLot(
				IN	unsigned __int64	LotIndex
		);

		NONVIRTUAL
		unsigned __int64
		GetAddrOfFileDataSec(
				unsigned __int32	Flag,
				unsigned __int64	LotIndex		
		);

        NONVIRTUAL
        VOID
        Construct (
                );

        NONVIRTUAL
        VOID
        Destroy(
            );

		HMEM		_hmem;
		ULONG		_LotSize;
		ULONG		_SectorPerLot;
		ULONG		_SectorSize;
		__int64		_TotalDiskSize;
		__int64		_TotalValidDiskSize;
		__int64		_LotCount;
		LARGE_INTEGER	_time;
		

};


INLINE
VOID
XIXFS_SA::XixFsSetBit(unsigned __int64 bitIndex, volatile void * Map)
{
	((unsigned char *) Map)[bitIndex >> 3] |= (1U << (bitIndex & 7));
}

INLINE
VOID
XIXFS_SA::XixFsClearBit(unsigned __int64 bitIndex, volatile void * Map)
{
	((unsigned char *) Map)[bitIndex >> 3] &= ~(1U << (bitIndex & 7));
}

INLINE
unsigned int
XIXFS_SA::XixFsTestBit(unsigned __int64 bitIndex, volatile void *Map)
{
	 return ((unsigned char *) Map)[bitIndex >> 3] & (1U << (bitIndex & 7));
}




INLINE
LARGE_INTEGER
XIXFS_SA::GetXixFsTime()
{
	LARGE_INTEGER Time = {0,0};
	FILETIME	FileTime;
	
	GetSystemTimeAsFileTime(&FileTime);

	Time.LowPart = FileTime.dwLowDateTime;
	Time.HighPart = FileTime.dwHighDateTime;

	return Time;
}



#endif //#ifndef XIXFSSA