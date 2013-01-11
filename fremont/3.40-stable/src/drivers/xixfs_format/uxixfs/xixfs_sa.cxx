#include <pch.cxx>

#define _NTAPI_ULIB_
#define _UFAT_MEMBER_


extern "C" {
    #include "xixRawDiskData.h"	
	#include "md5.h"
}

#include <Rpc.h>
#include "ulib.hxx"
#include "uxixfs.hxx"
#include "error.hxx"
#include "xixfs_sa.hxx"





DEFINE_CONSTRUCTOR( XIXFS_SA, SUPERAREA );

XIXFS_SA::~XIXFS_SA()
{
    Destroy();
}

VOID
XIXFS_SA::Construct ()
{
	(void)(this);
}


VOID
XIXFS_SA::Destroy()
{

}


BOOLEAN
XIXFS_SA::Initialize(
    IN OUT  PLOG_IO_DP_DRIVE    Drive,
    IN OUT  PMESSAGE            Message
    )
{
	BIG_INT		SecCount;
	ULONG		SecSize = 0;
	

	SecCount = Drive->QuerySectors();
	SecSize = Drive->QuerySectorSize();
	DebugPrint( "XIXFS_SA::Initialize 1!");

	DbgPrint("SecCount %I64d SecSize %ld\n", SecCount, SecSize);


	if(SecCount.GetLargeInteger().QuadPart < XIFS_MINIMUM_DISK_SECTOR_COUNT){
		DebugPrint( "XIXFS_SA::Initialize Fail. Small Disk Size!");
		return FALSE;
	}

	DebugPrint( "XIXFS_SA::Initialize 2!");
	if(SecSize == 0){
		DebugPrint( "XIXFS_SA::Initialize Fail. SectorSize == 0!");
		return FALSE;
	}

	DebugPrint( "XIXFS_SA::Initialize 3!");
	if(SecSize % 512){
		DebugPrint( "XIXFS_SA::Initialize Fail. Invalid SectorSize!");
		return FALSE;
	}


	_TotalDiskSize = SecCount.GetLargeInteger().QuadPart * SecSize;
	_TotalValidDiskSize = _TotalDiskSize - XIFS_OFFSET_VOLUME_LOT_LOC;
	_SectorSize = SecSize;
	_LotSize = XIFS_LOT_SIZE;
	_SectorPerLot = XIFS_LOT_SIZE / SecSize;
	_LotCount = _TotalValidDiskSize/XIFS_LOT_SIZE; 
	_time.QuadPart = GetXixFsTime().QuadPart; 

	DebugPrint( "XIXFS_SA::Initialize 4!");
	return SUPERAREA::Initialize(&_hmem, Drive, _SectorPerLot, Message);
}


PVOID
XIXFS_SA::GetBuf(
    )
{
	return SECRUN::GetBuf();
}


BOOLEAN
XIXFS_SA::Create(
    IN      PCNUMBER_SET    BadSectors,
    IN OUT  PMESSAGE        Message,
    IN      PCWSTRING       Label,
    IN      ULONG           Flags,
    IN      ULONG           ClusterSize,
    IN      ULONG           VirtualSize
    )
{

	PXIDISK_VOLUME_LOT		pVolumeHeader = NULL;
	PXIDISK_HOST_REG_LOT	pRegHostHeader = NULL;
	PXIDISK_MAP_LOT			pMapHeader = NULL;
	PXIDISK_DIR_HEADER		pRootDirHeader = NULL;
	PPACKED_BOOT_SECTOR		pBootSector = NULL;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	PXIDISK_BITMAP_DATA		pBitmapData = NULL;
	unsigned _int64			VolInfoLotIndex = 0;
	unsigned _int64			RegHostIndex = 2;
	unsigned _int64			CheckOutLotMapIndex = 4;
	unsigned _int64			UsedLotMapIndex = 6;
	unsigned _int64			UnusedLotMapIndex = 8;
	unsigned _int64			LogLotIndex = 10;
	unsigned _int64			RootDirectoryIndex = 20;
	unsigned __int64		PhyAddress = 0;

	unsigned int			NumBytesForLotMap = 0;
	unsigned char			*pData = NULL;
	unsigned int			i = 0;	
	UUID					t_uuid;
	uint32					BitmapSize = 0;

	
	DebugPrint( "XIXFS_SA::Create1!");

	_time.QuadPart = GetXixFsTime().QuadPart; 

	/*
	 *	Clean reserved area
	 */

	
	for(i = 0; i<50; i++){
		PhyAddress = i*_SectorPerLot*_drive->QuerySectorSize();
		SECRUN::Relocate(PhyAddress);
		memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
		SECRUN::Write();
	}

	
	// Clear 1/2 disk size 
	PhyAddress =  (_drive->QuerySectors().GetLargeInteger().QuadPart/ 2) * _drive->QuerySectorSize();
	PhyAddress = SECTOR_ALIGNED_COUNT(_SectorSize, PhyAddress);
	SECRUN::Relocate(PhyAddress);
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Write();


	// Clear last disk size
	PhyAddress =  (_drive->QuerySectors().GetLargeInteger().QuadPart - _SectorPerLot) * _drive->QuerySectorSize();
	PhyAddress = SECTOR_ALIGNED_COUNT(_SectorSize, PhyAddress);
	SECRUN::Relocate(PhyAddress);
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Write();


	SECRUN::Relocate(0);
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	pBootSector = (PPACKED_BOOT_SECTOR)GetBuf();
	memset(GetBuf(),0,sizeof(PACKED_BOOT_SECTOR));
	pBootSector->Oem[0] = 'X';
	pBootSector->Oem[1] = 'I';
	pBootSector->Oem[2] = 'F';
	pBootSector->Oem[3] = 'S';
	pBootSector->Oem[4] = 0;
	pBootSector->Oem[5] = 0;
	pBootSector->Oem[6] = 0;
	pBootSector->Oem[7] = 0;
	pBootSector->VolumeSignature = XIFS_VOLUME_SIGNATURE;
	pBootSector->NumLots = _LotCount;
	pBootSector->LotSize = _LotSize;
	pBootSector->LotSignature = (unsigned int)_time.LowPart;
	pBootSector->XifsVesion = XIFS_CURRENT_VERSION;
	pBootSector->FirstVolumeIndex = 0;
	pBootSector->SecondVolumeIndex = 127;
	md5digest_metadata((uint8 *)pBootSector, ( sizeof(PACKED_BOOT_SECTOR) - MD5DIGEST_SIZE), pBootSector->Digest);
	SECRUN::Write();

	SECRUN::Relocate(127);
	SECRUN::Write();

	
	/*
	 *	Make Volume info
	 */

	SECRUN::Relocate(SectorAddressOfLot(VolInfoLotIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	
	pVolumeHeader = (PXIDISK_VOLUME_LOT)GetBuf();
	memset(pVolumeHeader, 0, XIDISK_VOLUME_LOT_SIZE);
	
	pVolumeHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pVolumeHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pVolumeHeader->LotHeader.Lock.LockHostSignature;
	pVolumeHeader->LotHeader.Lock.LockHostMacAddress;
	pVolumeHeader->LotHeader.LotInfo.BeginningLotIndex = 0;
	pVolumeHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pVolumeHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pVolumeHeader->LotHeader.LotInfo.LotIndex = 0;
	pVolumeHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pVolumeHeader->LotHeader.LotInfo.LotTotalDataSize = 0;
	pVolumeHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pVolumeHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_VOLUME;
	pVolumeHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;
	
	md5digest_metadata(
		(uint8 *)&(pVolumeHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pVolumeHeader->LotHeader.Digest
		);




	memset(pVolumeHeader->VolInfo.VolumeId, 0, 16);
	

	

	/*
	 *	Generate UUID
	 */

	if(RPC_S_UUID_NO_ADDRESS == UuidCreate(&t_uuid) ){
		DbgPrint( "XIXFS_SA::Create  Can't make uuid \n\n" );
		return FALSE;				
	}

	memcpy(&pVolumeHeader->VolInfo.VolumeId, (unsigned char *)&t_uuid, 16);
	
	pVolumeHeader->VolInfo.HostRegLotMapIndex = RegHostIndex;
	pVolumeHeader->VolInfo.LotSize = _LotSize;
	pVolumeHeader->VolInfo.NumLots = _LotCount ;
	pVolumeHeader->VolInfo.RootDirectoryLotIndex = RootDirectoryIndex;
	pVolumeHeader->VolInfo.XifsVesion = XIFS_CURRENT_VERSION;
	pVolumeHeader->VolInfo.VolCreationTime = (unsigned __int64)_time.QuadPart;
	pVolumeHeader->VolInfo.VolLabelLength = 0;
	pVolumeHeader->VolInfo.VolSerialNumber ;
	pVolumeHeader->VolInfo.VolumeSignature = XIFS_VOLUME_SIGNATURE;
	pVolumeHeader->VolInfo.LotSignature = (unsigned int)_time.LowPart;
	DbgPrint( "LotSignature 0x%x", pVolumeHeader->VolInfo.LotSignature);
	DbgPrint( "XifsVersion 0x%x", pVolumeHeader->VolInfo.XifsVesion);

	md5digest_metadata(
		(uint8 *)&(pVolumeHeader->VolInfo), 
		( sizeof(XIDISK_VOLUME_INFO) - MD5DIGEST_SIZE), 
		pVolumeHeader->VolInfo.Digest
		);

	SECRUN::Write();
	

	SECRUN::Relocate(SectorAddressOfLot(127));
	pVolumeHeader->LotHeader.LotInfo.BeginningLotIndex = 128;
	pVolumeHeader->LotHeader.LotInfo.LotIndex = 128;

	md5digest_metadata(
		(uint8 *)&(pVolumeHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pVolumeHeader->LotHeader.Digest
		);

	SECRUN::Write();

	
	/*
	 *	Make Register info
	 */
	SECRUN::Relocate(SectorAddressOfLot(RegHostIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	
	pRegHostHeader = (PXIDISK_HOST_REG_LOT)GetBuf();
	memset(pRegHostHeader, 0, XIDISK_HOST_REG_LOT_SIZE);
	
	pRegHostHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pRegHostHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pRegHostHeader->LotHeader.Lock.LockHostSignature;
	pRegHostHeader->LotHeader.Lock.LockHostMacAddress;
	pRegHostHeader->LotHeader.LotInfo.BeginningLotIndex = RegHostIndex;
	pRegHostHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pRegHostHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pRegHostHeader->LotHeader.LotInfo.LotIndex = RegHostIndex;
	pRegHostHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pRegHostHeader->LotHeader.LotInfo.LotTotalDataSize = 0;
	pRegHostHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pRegHostHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_HOSTREG;
	pRegHostHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;

	md5digest_metadata(
		(uint8 *)&(pRegHostHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pRegHostHeader->LotHeader.Digest
		);

	pRegHostHeader->HostInfo.LogFileIndex = LogLotIndex;
	pRegHostHeader->HostInfo.NumHost = 0;
	memset(pRegHostHeader->HostInfo.RegisterMap, 0, 16);
	pRegHostHeader->HostInfo.CheckOutLotMapIndex = CheckOutLotMapIndex;
	pRegHostHeader->HostInfo.UnusedLotMapIndex = UnusedLotMapIndex;
	pRegHostHeader->HostInfo.UsedLotMapIndex= UsedLotMapIndex;

	md5digest_metadata(
		(uint8 *)&(pRegHostHeader->HostInfo), 
		( sizeof(XIDISK_HOST_INFO) - MD5DIGEST_SIZE), 
		pRegHostHeader->HostInfo.Digest
		);

	SECRUN::Write();



	/*
	 *	Make bitmap info
	 */

	NumBytesForLotMap = (unsigned int)((_LotCount + 7) >> 3);
	BitmapSize = SECTOR_ALIGNED_SIZE(_SectorSize, (NumBytesForLotMap + sizeof(XIDISK_BITMAP_DATA) -1));
		/* Checkout Lot Map */

	SECRUN::Relocate(SectorAddressOfLot(CheckOutLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());


	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();

	pMapHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pMapHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pMapHeader->LotHeader.Lock.LockHostSignature;
	pMapHeader->LotHeader.Lock.LockHostMacAddress;
	pMapHeader->LotHeader.LotInfo.BeginningLotIndex = CheckOutLotMapIndex;
	pMapHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.LotIndex = CheckOutLotMapIndex;
	pMapHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pMapHeader->LotHeader.LotInfo.LotTotalDataSize = 0;
	pMapHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pMapHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_BITMAP;
	pMapHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;
	
	md5digest_metadata(
		(uint8 *)&(pMapHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pMapHeader->LotHeader.Digest
		);

	pMapHeader->Map.BitMapBytes = NumBytesForLotMap;
	pMapHeader->Map.MapType = XIFS_MAP_CHECKOUTLOT;
	pMapHeader->Map.NumLots = _LotCount;
	
	md5digest_metadata(
		(uint8 *)&(pMapHeader->Map), 
		( sizeof(XIDISK_LOT_MAP_INFO) - MD5DIGEST_SIZE), 
		pMapHeader->Map.Digest
		);


	pBitmapData = (PXIDISK_BITMAP_DATA)((uint8 *)GetBuf() + XIDISK_MAP_LOT_SIZE);

	md5digest_metadata(
		(uint8 *)&(pBitmapData->SequenceNum),
		( BitmapSize - MD5DIGEST_SIZE),
		pBitmapData->Digest
		);

	SECRUN::Write();

		/* Used Lot Map */
	
	SECRUN::Relocate(SectorAddressOfLot(UsedLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	
	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();
	
	pMapHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pMapHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pMapHeader->LotHeader.Lock.LockHostSignature;
	pMapHeader->LotHeader.Lock.LockHostMacAddress;
	pMapHeader->LotHeader.LotInfo.BeginningLotIndex = UsedLotMapIndex;
	pMapHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.LotIndex = UsedLotMapIndex;
	pMapHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pMapHeader->LotHeader.LotInfo.LotTotalDataSize = 0;
	pMapHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pMapHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_BITMAP;
	pMapHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;
	
	md5digest_metadata(
		(uint8 *)&(pMapHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pMapHeader->LotHeader.Digest
		);

	pMapHeader->Map.BitMapBytes = NumBytesForLotMap;
	pMapHeader->Map.MapType = XIFS_MAP_CHECKOUTLOT;
	pMapHeader->Map.NumLots = _LotCount;
	
	md5digest_metadata(
		(uint8 *)&(pMapHeader->Map), 
		( sizeof(XIDISK_LOT_MAP_INFO) - MD5DIGEST_SIZE), 
		pMapHeader->Map.Digest
		);

	pBitmapData = (PXIDISK_BITMAP_DATA)((uint8 *)GetBuf() + XIDISK_MAP_LOT_SIZE);

	pData = (unsigned char *)&(pBitmapData->RealData[0]);

	for(i = 0; i<XIFS_RESERVED_LOT_SIZE; i++)
	{
		XixFsSetBit(i, (void *)pData );
	}
	XixFsSetBit(127,(void *)pData);


	md5digest_metadata(
		(uint8 *)&(pBitmapData->SequenceNum),
		( BitmapSize - MD5DIGEST_SIZE),
		pBitmapData->Digest
		);

	SECRUN::Write();


		/* Unused Lot Map */
	SECRUN::Relocate(SectorAddressOfLot(UnusedLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	
	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();
	
	pMapHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pMapHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pMapHeader->LotHeader.Lock.LockHostSignature;
	pMapHeader->LotHeader.Lock.LockHostMacAddress;
	pMapHeader->LotHeader.LotInfo.BeginningLotIndex = UnusedLotMapIndex;
	pMapHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pMapHeader->LotHeader.LotInfo.LotIndex = UnusedLotMapIndex;
	pMapHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pMapHeader->LotHeader.LotInfo.LotTotalDataSize = 0;
	pMapHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pMapHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_BITMAP;
	pMapHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;

	md5digest_metadata(
		(uint8 *)&(pMapHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pMapHeader->LotHeader.Digest
		);

	pMapHeader->Map.BitMapBytes = NumBytesForLotMap;
	pMapHeader->Map.MapType = XIFS_MAP_CHECKOUTLOT;
	pMapHeader->Map.NumLots = _LotCount;

	md5digest_metadata(
		(uint8 *)&(pMapHeader->Map), 
		( sizeof(XIDISK_LOT_MAP_INFO) - MD5DIGEST_SIZE), 
		pMapHeader->Map.Digest
		);


	pBitmapData = (PXIDISK_BITMAP_DATA)((uint8 *)GetBuf() + XIDISK_MAP_LOT_SIZE);

	pData = (unsigned char *)&(pBitmapData->RealData[0]);

	for(i =0; i < NumBytesForLotMap; i++)
	{
		pData[i] = 0xFF;
	}

	for(i = 0; i<XIFS_RESERVED_LOT_SIZE; i++)
	{
		XixFsClearBit(i, (void *)pData );
	}

	XixFsClearBit(127,(void *)pData);

	md5digest_metadata(
		(uint8 *)&(pBitmapData->SequenceNum),
		( BitmapSize - MD5DIGEST_SIZE),
		pBitmapData->Digest
		);

	SECRUN::Write();

	/*
	 *	Make Root Directory
	 */	

	SECRUN::Relocate(SectorAddressOfLot(RootDirectoryIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());

	pRootDirHeader = (PXIDISK_DIR_HEADER)GetBuf();
	memset(pRootDirHeader, 0, XIDISK_DIR_HEADER_SIZE);

	pRootDirHeader->LotHeader.Lock.LockState = XIDISK_LOCK_RELEASED;
	pRootDirHeader->LotHeader.Lock.LockAcquireTime = (unsigned __int64)_time.QuadPart;
	pRootDirHeader->LotHeader.Lock.LockHostSignature;
	pRootDirHeader->LotHeader.Lock.LockHostMacAddress;
	pRootDirHeader->LotHeader.LotInfo.BeginningLotIndex = RootDirectoryIndex;
	pRootDirHeader->LotHeader.LotInfo.PreviousLotIndex = 0;
	pRootDirHeader->LotHeader.LotInfo.NextLotIndex = 0;
	pRootDirHeader->LotHeader.LotInfo.LotIndex = RootDirectoryIndex;
	pRootDirHeader->LotHeader.LotInfo.LogicalStartOffset = 0;
	pRootDirHeader->LotHeader.LotInfo.LotTotalDataSize = _LotSize - XIDISK_FILE_HEADER_SIZE;
	pRootDirHeader->LotHeader.LotInfo.LotSignature = (unsigned int)_time.LowPart;
	pRootDirHeader->LotHeader.LotInfo.Type = LOT_INFO_TYPE_DIRECTORY;
	pRootDirHeader->LotHeader.LotInfo.Flags = LOT_FLAG_BEGIN;

	md5digest_metadata(
		(uint8 *)&(pRootDirHeader->LotHeader), 
		( sizeof(XIDISK_COMMON_LOT_HEADER) - MD5DIGEST_SIZE), 
		pRootDirHeader->LotHeader.Digest
		);

	pRootDirHeader->AddrInfo.LotNumber[0] = RootDirectoryIndex;

	
	pRootDirHeader->DirInfo.Type = XIFS_FD_TYPE_ROOT_DIRECTORY;
	pRootDirHeader->DirInfo.State = XIFS_FD_STATE_CREATE;
	pRootDirHeader->DirInfo.OwnHostId;
	

	pRootDirHeader->DirInfo.ParentDirLotIndex = 0;
	pRootDirHeader->DirInfo.LotIndex = RootDirectoryIndex;
	pRootDirHeader->DirInfo.AddressMapIndex = 0;

	pRootDirHeader->DirInfo.Access_time = (unsigned __int64)_time.QuadPart;
	pRootDirHeader->DirInfo.Change_time= (unsigned __int64)_time.QuadPart;
	pRootDirHeader->DirInfo.Create_time= (unsigned __int64)_time.QuadPart;
	pRootDirHeader->DirInfo.Modified_time= (unsigned __int64)_time.QuadPart;

	pRootDirHeader->DirInfo.FileAttribute = (FILE_ATTRIBUTE_NORMAL|FILE_ATTRIBUTE_DIRECTORY);
	pRootDirHeader->DirInfo.AccessFlags = FILE_SHARE_READ;
	pRootDirHeader->DirInfo.ACLState = 0;
	pRootDirHeader->DirInfo.AllocationSize = pRootDirHeader->LotHeader.LotInfo.LotTotalDataSize ;
	pRootDirHeader->DirInfo.FileSize = 0;
	pRootDirHeader->DirInfo.NameSize = 2;
	
	{
		PWCHAR	pData = NULL;
		pData = (PWCHAR)pRootDirHeader->DirInfo.Name;
		pData[0] = L'\\';
	}
	
	pRootDirHeader->DirInfo.LinkCount = 1;
	pRootDirHeader->DirInfo.childCount = 0;
	pRootDirHeader->DirInfo.ChildMap;
	
	md5digest_metadata(
		(uint8 *)&(pRootDirHeader->DirInfo), 
		( sizeof(XIDISK_DIR_INFO) - MD5DIGEST_SIZE), 
		pRootDirHeader->DirInfo.Digest
		);

	SECRUN::Write();



	SECRUN::Relocate(GetAddrOfFileDataSec(LOT_FLAG_BEGIN, RootDirectoryIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	
	pTable = (PXIDISK_HASH_VALUE_TABLE)GetBuf();
	memset(pTable, 0, XIDISK_HASH_VALUE_TABLE_SIZE);
	
	md5digest_metadata(
		(uint8 *)pTable, 
		( sizeof(XIDISK_HASH_VALUE_TABLE) - MD5DIGEST_SIZE), 
		pTable->Digest
		);

	SECRUN::Write();

	DebugPrint( "XIXFS_SA::Create2!");

	return TRUE;
}


BOOLEAN
XIXFS_SA::VerifyAndFix(
    IN      FIX_LEVEL   FixLevel,
    IN OUT  PMESSAGE    Message,
    IN      ULONG       Flags,
    IN      ULONG       LogFileSize,
    IN      USHORT      Algorithm,
    OUT     PULONG      ExitStatus,
    IN      PCWSTRING   DriveLetter
    )
{
	PXIDISK_VOLUME_LOT		pVolumeHeader = NULL;
	PXIDISK_HOST_REG_LOT	pRegHostHeader = NULL;
	PXIDISK_MAP_LOT			pMapHeader = NULL;
	PXIDISK_DIR_HEADER		pRootDirHeader = NULL;

	unsigned _int64			VolInfoLotIndex = 0;
	unsigned _int64			RegHostIndex = 2;
	unsigned _int64			CheckOutLotMapIndex = 4;
	unsigned _int64			UsedLotMapIndex = 6;
	unsigned _int64			UnusedLotMapIndex = 8;
	unsigned _int64			LogLotIndex = 10;
	unsigned _int64			RootDirectoryIndex = 20;
	unsigned int			NumBytesForLotMap = 0;
	unsigned char			*pData = NULL;
	unsigned int			i = 0;		
	unsigned int			LotSignature = 0;


	/*
	 *		check Volume info
	 */
	SECRUN::Relocate(SectorAddressOfLot(VolInfoLotIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());	
	SECRUN::Read();
	pVolumeHeader = (PXIDISK_VOLUME_LOT)GetBuf();
	
	if(	(pVolumeHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_VOLUME) || 
			(pVolumeHeader->VolInfo.VolumeSignature != XIFS_VOLUME_SIGNATURE) ||
			(pVolumeHeader->VolInfo.XifsVesion > XIFS_CURRENT_VERSION)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Volume info failed\n\n" );
		return FALSE;
	}

	/*
	 *	All Lot used after volume info generation use LotSignature.
	 */
	LotSignature = pVolumeHeader->VolInfo.LotSignature;
	
	


	/*
	 *	Check Host Reg
	 */
	SECRUN::Relocate(SectorAddressOfLot(RegHostIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Read();
	pRegHostHeader = (PXIDISK_HOST_REG_LOT)GetBuf();
	
	if((pRegHostHeader->LotHeader.LotInfo.LotSignature != LotSignature) ||
		(pRegHostHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_HOSTREG) ||
		(pRegHostHeader->LotHeader.LotInfo.Flags != LOT_FLAG_BEGIN)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Host Reg failed\n\n" );
		return FALSE;
	}


	
	/*
	 *	Check Bitmap Directory
	 */
	SECRUN::Relocate(SectorAddressOfLot(CheckOutLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Read();
	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();
	
	if( (pMapHeader->LotHeader.LotInfo.LotSignature != LotSignature) ||
			(pMapHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_BITMAP) ||
			(pMapHeader->LotHeader.LotInfo.Flags != LOT_FLAG_BEGIN)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Checkout Bitmap failed \n\n" );
		return FALSE;		
	}


	SECRUN::Relocate(SectorAddressOfLot(UsedLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Read();
	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();
	
	if( (pMapHeader->LotHeader.LotInfo.LotSignature != LotSignature) ||
			(pMapHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_BITMAP) ||
			(pMapHeader->LotHeader.LotInfo.Flags != LOT_FLAG_BEGIN)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Checkout Bitmap failed \n\n" );
		return FALSE;		
	}


	SECRUN::Relocate(SectorAddressOfLot(UnusedLotMapIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Read();
	pMapHeader = (PXIDISK_MAP_LOT)GetBuf();
	
	if( (pMapHeader->LotHeader.LotInfo.LotSignature != LotSignature) ||
			(pMapHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_BITMAP) ||
			(pMapHeader->LotHeader.LotInfo.Flags != LOT_FLAG_BEGIN)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Checkout Bitmap failed \n\n" );
		return FALSE;		
	}



	
	/*
	 *	Check Root Directory
	 */
	SECRUN::Relocate(SectorAddressOfLot(RootDirectoryIndex));
	memset(GetBuf(),0, SECRUN::QueryLength()*_drive->QuerySectorSize());
	SECRUN::Read();
	pRootDirHeader = (PXIDISK_DIR_HEADER)GetBuf();
	
	if( (pRootDirHeader->LotHeader.LotInfo.LotSignature != LotSignature) ||
		(pRootDirHeader->LotHeader.LotInfo.Type != LOT_INFO_TYPE_DIRECTORY) ||
		(pRootDirHeader->LotHeader.LotInfo.Flags != LOT_FLAG_BEGIN) ||
		!(pRootDirHeader->DirInfo.Type & (XIFS_FD_TYPE_ROOT_DIRECTORY| XIFS_FD_TYPE_DIRECTORY) ) ||
		(pRootDirHeader->DirInfo.State != XIFS_FD_STATE_CREATE)
	){
		DbgPrint( "\tXIXFS_SA::VerifyAndFix() : Read Root Dir failed \n\n" );
		return FALSE;		
	}


	return TRUE;
}

BOOLEAN
XIXFS_SA::RecoverFile(
    IN      PCWSTRING   FullPathFileName,
    IN OUT  PMESSAGE    Message
    )
{
	return FALSE;
}


PARTITION_SYSTEM_ID
XIXFS_SA::QuerySystemId(
    )CONST
{
	return SYSID_IFS;
}



VOID
XIXFS_SA::PrintFormatReport (
    IN OUT PMESSAGE Message,
    IN     PFILE_FS_SIZE_INFORMATION    FsSizeInfo,
    IN     PFILE_FS_VOLUME_INFORMATION  FsVolInfo
    ) 
{
	
}



// Private
unsigned __int64
XIXFS_SA::SectorAddressOfLot(
	IN	unsigned __int64	LotIndex
)
{
	unsigned __int64 PhyAddress = 0;
	
	PhyAddress = XIFS_OFFSET_VOLUME_LOT_LOC + _LotSize * LotIndex;
	PhyAddress = SECTOR_ALIGNED_COUNT(_SectorSize, PhyAddress);
	
	return PhyAddress;
}


unsigned __int64
XIXFS_SA::GetAddrOfFileDataSec(
		unsigned __int32	Flag,
		unsigned __int64	LotIndex		
)
{
	unsigned __int64 logicalAddress = 0;
	unsigned __int64 logicalSecNum = 0;


	if(Flag == LOT_FLAG_BEGIN){
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * _LotSize ) + XIDISK_FILE_HEADER_SIZE;
	}else{
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * _LotSize ) + XIDISK_DATA_LOT_SIZE;
	}
	
	logicalSecNum = SECTOR_ALIGNED_COUNT(_SectorSize,logicalAddress);

	return logicalSecNum ;
}