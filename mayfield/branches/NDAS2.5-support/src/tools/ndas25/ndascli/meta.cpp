//
// View or change NDAS meta data such as DIB and RMD.
//
#include "stdafx.h"
#include "ndas/ndasdib.h"
#include "scrc32.h"
extern int ActiveHwVersion; // set at login time
extern UINT16	HeaderEncryptAlgo;
extern UINT16	DataEncryptAlgo;
extern int		iTargetID;
extern unsigned _int64	iPassword_v1;
extern unsigned _int64	iSuperPassword_v1;
extern _int32			HPID;
extern _int16			RPID;
extern _int32			iTag;
extern TARGET_DATA		PerTarget[];

#define BLOCK_SIZE 512

//
// Target can be file name
// Arg[0]: Device number. Optional
//
// From NdasOpReadDIB, LurnRMDRead
//
int CmdViewMeta(char* target, char* arg[])
{
	UINT64 Pos = 10;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	FILE*	file = NULL;
	NDAS_DIB_V2 DIB_V2;
	NDAS_RAID_META_DATA Rmd;
	NDAS_LAST_WRITTEN_REGION_BLOCK Lwr;
	UINT32 i, j;
	
	BOOL IsTargetFile;
	UINT64 DiskSectorCount;
	char* str;
	PNDAS_UNIT_META_DATA UnitMeta;
	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	if (strcspn(target, ":") >=strlen(target)) {
		IsTargetFile = TRUE;
	} else {
		IsTargetFile = FALSE;
	}
	if (IsTargetFile) {
		struct __stat64 statbuff;

		file = fopen(target, "r");
		if (file ==NULL) {
			fprintf(stderr, "Failed to open file\n");
			retval = GetLastError();
			goto errout; 
		}
		iResult = _stat64(target, &statbuff);
		if (iResult !=0) {
			fprintf(stderr, "Failed to get stat file\n");
			goto errout;
		}
		DiskSectorCount = statbuff.st_size/BLOCK_SIZE;
	} else {
		int dev;
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

		if (arg[0])
			dev = (int) _strtoi64(arg[0], NULL, 0);
		else 
			dev = 0;
		if (dev==0) {
			//
		} else {
			UserId |= 0x100;
		}

		if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
			goto errout;
		// Need to get disk info before IO
		if((iResult = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
			fprintf(stderr, "GetDiskInfo Failed...\n");
			retval = iResult;
			goto errout;
		}
		DiskSectorCount = PerTarget[iTargetID].SectorCount;
	}
	fprintf(stderr,"Total sector count = %I64d\n", DiskSectorCount);

	// Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2
	if (IsTargetFile) {
		iResult = fseek(file, (long)((DiskSectorCount + NDAS_BLOCK_LOCATION_DIB_V2)*BLOCK_SIZE), SEEK_SET);
		if (iResult != 0) {
			fprintf(stderr, "fseek error\n");
			goto errout;
		}
		iResult = (int)fread((void*)&DIB_V2, 512, 1, file);
		if (iResult == 0) {
			fprintf(stderr, "fread error\n");
			goto errout;
		}
	} else {
		iResult = IdeCommand(connsock, iTargetID, 0, WIN_READ, DiskSectorCount + NDAS_BLOCK_LOCATION_DIB_V2, 1, 0, sizeof(DIB_V2), (PCHAR)&DIB_V2, 0, 0);
		if (iResult != 0) {
			fprintf(stderr, "READ Failed.\n");
			goto errout;
		}
	}

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct
	if(NDAS_DIB_V2_SIGNATURE != DIB_V2.Signature)
	{
		if (DIB_V2.Signature == 0) {
			fprintf(stderr, "No DIBv2 signature. Single disk or uninitialized disk.\n");
		} else {
			fprintf(stderr, "V2 Signature mismatch\n");
		}
		goto errout;
		//goto process_v1;
	}

	if(!IS_DIB_CRC_VALID(crc32_calc, DIB_V2))
	{
		fprintf(stderr, "CRC mismatch\n");
		goto errout;
//		goto process_v1;
	}

	if(NDAS_BLOCK_SIZE_XAREA != DIB_V2.sizeXArea &&
		NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DIB_V2.sizeXArea)
	{
		fprintf(stderr, "Reserved size mismatch\n");
		goto errout;
//		goto process_v1;
	}

	if(DIB_V2.sizeUserSpace + DIB_V2.sizeXArea > DiskSectorCount)
	{
		fprintf(stderr, "Disk size mistmatch\n");
		goto errout;
//		goto process_v1;
	}

#if 0
	nTotalDiskCount = DIB_V2.nDiskCount + DIB_V2.nSpareCount;
	// check DIB_V2.nDiskCount
	if(!NdasOpVerifyDiskCount(DIB_V2.iMediaType, DIB_V2.nDiskCount))
		goto process_v1;
#endif
	// Dump DIB info
	printf("DIBV2 Major Version = %d, Minor version = %d\n", DIB_V2.MajorVersion, DIB_V2.MinorVersion);
	printf("User Space in sectors=%I64d, X space=%I64d\n", DIB_V2.sizeUserSpace, DIB_V2.sizeXArea);
	printf("Bitmap sector per bit=%d(%fMbyte)\n", DIB_V2.iSectorsPerBit, DIB_V2.iSectorsPerBit / 2.0/1024);
	switch(DIB_V2.iMediaType) {
	case NMT_SINGLE:		str="Single"; break;
	case NMT_MIRROR:		str="Mirror without repair info"; break;
	case NMT_AGGREGATE:		str="Aggregation"; break;
	case NMT_RAID0:			str="RAID0"; break;
	case NMT_RAID1:			str="RAID1(~3.10)"; break;
	case NMT_RAID4:			str="RAID4(~3.10)"; break;
	case NMT_RAID1R2:		str="RAID1(3.11~)"; break;
	case NMT_RAID4R2:		str="RAID4(3.11~)"; break;
	case NMT_RAID1R3:		str="RAID1(3.20~)"; break;
	case NMT_RAID4R3:		str="RAID4(3.20~)"; break;
	case NMT_AOD:			str="Append only disk"; break;
	case NMT_VDVD:			str="Virtual DVD"; break;
	case NMT_CDROM:			str="packet device, CD / DVD"; break;
	case NMT_OPMEM:			str="packet device, Magnetic Optical"; break;
	case NMT_FLASH:			str="flash card"; break;
	default: str="Unknown"; break;
	}
	printf("Media type=%d(%s)\n", DIB_V2.iMediaType, str);

	printf("Diskcount=%d, Sequence=%d, SpareCount=%d\n", 
		DIB_V2.nDiskCount, DIB_V2.iSequence, DIB_V2.nSpareCount);

	for(i=0;i<DIB_V2.nDiskCount+DIB_V2.nSpareCount;i++) {
		printf(" * Unit disk %d: ", i);
		printf("MAC %02x:%02x:%02x:%02x:%02x:%02x, VID: %x, UnitNumber=%d, HwVersion=%d\n",
			DIB_V2.UnitDisks[i].MACAddr[0], DIB_V2.UnitDisks[i].MACAddr[1], DIB_V2.UnitDisks[i].MACAddr[2],
			DIB_V2.UnitDisks[i].MACAddr[3], DIB_V2.UnitDisks[i].MACAddr[4], DIB_V2.UnitDisks[i].MACAddr[5],
			DIB_V2.UnitDisks[i].VID,
			DIB_V2.UnitDisks[i].UnitNumber,
			DIB_V2.UnitDiskInfos[i].HwVersion
			);
	}

	// Show RMD info 

	if (IsTargetFile) {
		iResult = fseek(file, (long)((DiskSectorCount + NDAS_BLOCK_LOCATION_RMD)*BLOCK_SIZE), SEEK_SET);
		if (iResult != 0) {
			fprintf(stderr, "fseek error\n");
			goto errout;
		}
		iResult = (int)fread((void*)&Rmd, 512, 1, file);
		if (iResult == 0) {
			fprintf(stderr, "fread error\n");
			goto errout;
		}
	} else {
		iResult = IdeCommand(connsock, iTargetID, 0, WIN_READ, DiskSectorCount + NDAS_BLOCK_LOCATION_RMD, 1, 0, sizeof(Rmd), (PCHAR)&Rmd, 0, 0);
		if (iResult != 0) {
			fprintf(stderr, "READ Failed.\n");
			goto errout;
		}
	}

	// Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct
	if(NDAS_RAID_META_DATA_SIGNATURE != Rmd.Signature)
	{
		fprintf(stderr, "RMD Signature mismatch: %I64x\n", Rmd.Signature);
		goto errout;
		//goto process_v1;
	}

	if(!IS_RMD_CRC_VALID(crc32_calc, Rmd))
	{
		fprintf(stderr, "RMD CRC mismatch\n");
		goto errout;
//		goto process_v1;
	}
	printf("RAID Set ID  = %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n",
		Rmd.guid.Data1, Rmd.guid.Data2, Rmd.guid.Data3,
		Rmd.guid.Data4[0], Rmd.guid.Data4[1], Rmd.guid.Data4[2], Rmd.guid.Data4[3], 
		Rmd.guid.Data4[4], Rmd.guid.Data4[5], Rmd.guid.Data4[6], Rmd.guid.Data4[7]
	);
	printf("Config Set ID= %08x-%04x-%04x-%02x%02x%02x%02x%02x%02x%02x%02x\n",
		Rmd.ConfigSetId.Data1, Rmd.ConfigSetId.Data2, Rmd.ConfigSetId.Data3,
		Rmd.ConfigSetId.Data4[0], Rmd.ConfigSetId.Data4[1], Rmd.ConfigSetId.Data4[2], Rmd.ConfigSetId.Data4[3], 
		Rmd.ConfigSetId.Data4[4], Rmd.ConfigSetId.Data4[5], Rmd.ConfigSetId.Data4[6], Rmd.ConfigSetId.Data4[7]
	);
	printf("USN: %d\n", Rmd.uiUSN);
	
	switch(Rmd.state & (NDAS_RAID_META_DATA_STATE_MOUNTED | NDAS_RAID_META_DATA_STATE_UNMOUNTED)) {
		case NDAS_RAID_META_DATA_STATE_MOUNTED:	str= "Mounted"; break;
		case NDAS_RAID_META_DATA_STATE_UNMOUNTED:	str= "Unmounted"; break;
		default: str = "Unknown/Never Mounted"; break;
	}
	if (Rmd.state & NDAS_RAID_META_DATA_STATE_USED_IN_DEGRADED) {
		printf("This member is used in degraded mode\n");
	}

	printf("State: %d(%s)\n", Rmd.state, str);
	for(i=0;i<NDAS_DRAID_ARBITER_ADDR_COUNT;i++) {
		if (Rmd.ArbiterInfo[i].Type == 0)
			break;
		printf("DRAID Listen Address: Type %d - %02x:%02x:%02x:%02x:%02x:%02x\n", 
			Rmd.ArbiterInfo[i].Type,
			Rmd.ArbiterInfo[i].Addr[0], Rmd.ArbiterInfo[i].Addr[1], Rmd.ArbiterInfo[i].Addr[2],
			Rmd.ArbiterInfo[i].Addr[3], Rmd.ArbiterInfo[i].Addr[4], Rmd.ArbiterInfo[i].Addr[5]
		);
	}
	for(i=0;i<DIB_V2.nDiskCount+DIB_V2.nSpareCount;i++) {
		char* DefectStr;
		printf(" * Role %d: ", i);
		UnitMeta = &Rmd.UnitMetaData[i];

		printf("Unit index=%d  Status=%d(%s%s%s%s%s%s)\n",
			UnitMeta->iUnitDeviceIdx, UnitMeta->UnitDeviceStatus, 
			(UnitMeta->UnitDeviceStatus==0)?"Normal ":"",
			(UnitMeta->UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)?"Out-of-sync ":"",
			(UnitMeta->UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_SPARE)?"Spare ":"",
			(UnitMeta->UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_DISK)?"Bad disk ":"",
			(UnitMeta->UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)?"Bad sector ":"",
			(UnitMeta->UnitDeviceStatus & NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)?"Replaced by spare ":""
			);
	}

	//
	// Dump bitmap
	// 
	if (DIB_V2.iMediaType == NMT_RAID1R3) 
	{
		UINT32 BitCount = (UINT32)((DIB_V2.sizeUserSpace + DIB_V2.iSectorsPerBit - 1)/DIB_V2.iSectorsPerBit);
		UINT32 BmpSectorCount = (BitCount + NDAS_BIT_PER_OOS_BITMAP_BLOCK -1)/NDAS_BIT_PER_OOS_BITMAP_BLOCK;
		UINT32 CurBitCount;
		PNDAS_OOS_BITMAP_BLOCK BmpBuffer;
		UCHAR OnBits[] = {1<<0, 1<<1, 1<<2, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7};
		UINT32 BitOn, BitOnStart, BitOnEnd;

		printf("\nBitmap sector per bit=0x%x, Bit count =0x%x, BmpSector count=0x%x\n", 
			DIB_V2.iSectorsPerBit, BitCount, BmpSectorCount);
		BmpBuffer = (PNDAS_OOS_BITMAP_BLOCK) malloc(BmpSectorCount* 512);
		iResult = IdeCommand(connsock, iTargetID, 0, WIN_READ, DiskSectorCount + NDAS_BLOCK_LOCATION_BITMAP, (_int16)BmpSectorCount, 0, BmpSectorCount* 512, (PCHAR)BmpBuffer, 0, 0);
		if (iResult != 0) {
			fprintf(stderr, "READ Failed.\n");
			free(BmpBuffer);
			goto errout;
		}
		CurBitCount = 0;
		for(i=0;i<BmpSectorCount;i++) {
			printf("  Bitmap sector %d, Seq head=%I64x, tail=%I64x\n", i, BmpBuffer[i].SequenceNumHead,BmpBuffer[i].SequenceNumTail);
			BitOn = FALSE;
			BitOnStart = BitOnEnd = 0;
			for(j=0;j<NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8;j++) {
				if (BitOn == FALSE && (BmpBuffer[i].Bits[j/8] & OnBits[j%8])) {
					BitOn = TRUE;
					BitOnStart = i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j;
					printf("    Bit on from bit %x ~ ", BitOnStart);
				}
				if (BitOn == TRUE && (BmpBuffer[i].Bits[j/8] & OnBits[j%8]) == 0) {
					BitOn = FALSE;
					BitOnEnd = i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j;
					printf("%x\n", BitOnEnd-1);
				}
				CurBitCount++;
				if (CurBitCount >= BitCount)
					break;
			}
			if (BitOn == TRUE) {
				printf("%x\n", i * NDAS_BYTE_PER_OOS_BITMAP_BLOCK * 8 + j);
			}
		}
		free(BmpBuffer);
	}
	printf("\n");
	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;
}
