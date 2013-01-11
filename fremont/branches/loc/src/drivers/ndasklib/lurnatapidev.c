#include "ndasscsiproc.h"
#include "ntddmmc.h"
#include "ntddcdrm.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LurnIdeOdd"


//////////////////////////////////////////////////////////////////////////////////////////
//
//	IDE ODD interface
//

NTSTATUS
IdeOddLurnCreate (
	PLURELATION_NODE		Lurn,
	PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS 
ODD_Inquiry(
	PLURNEXT_IDE_DEVICE		IdeDisk
);

VOID
IdeOddLurnStop (
	PLURELATION_NODE Lurn
	);

NTSTATUS
IdeOddLurnRequest (
	PLURELATION_NODE Lurn,
	PCCB Ccb
	);

LURN_INTERFACE LurnAtapiInterface = {

	LSSTRUC_TYPE_LURN_INTERFACE,
	sizeof(LURN_INTERFACE),
	LURN_NULL,	// Support all LURN types with ATAPI interface.
	LURN_DEVICE_INTERFACE_ATAPI,
	0, 

	{
		IdeOddLurnCreate,
		IdeLurnClose,
		IdeOddLurnStop,
		IdeOddLurnRequest
	}
};

//////////////////////////////////////////////////////////////////////////
//
// Defines for CDROM commands
//

#define SCSIOP_PLEXTOR_CDDA 0xD8
#define SCSIOP_NEC_CDDA		0xD4


#define CD_MINS              74 /* max. minutes per CD, not really a limit */
#define CD_SECS              60 /* seconds per minute */
#define CD_BLOCKS_PER_SEC    75 /* blocks per second */
#define CD_BLOCK_SIZE_RAW	2352

//////////////////////////////////////////////////////////////////////////
//
//	Internal functions
//

NTSTATUS
ODD_GetStatus (
	PLURNEXT_IDE_DEVICE		IdeDisk,
	PCCB					Ccb,
	PSENSE_DATA				pSenseData,
	LONG					SenseDataLen,
	PLARGE_INTEGER			Timeout,
	PBYTE					Response
	);

VOID
LurnIdeOddThreadProc (
	IN	PVOID	Context
	);

NTSTATUS
AtapiWriteRequest (
	PLURELATION_NODE	Lurn,
	PLURNEXT_IDE_DEVICE	IdeDisk,
	PCCB				Ccb,
	PBYTE				PduResponse,
	PLARGE_INTEGER		Timeout,
	PUCHAR				PduRegister
	);

NTSTATUS
AtapiReadRequest (
	PLURELATION_NODE	Lurn,
	PLURNEXT_IDE_DEVICE	IdeDisk,
	PCCB				Ccb,
	PBYTE				PduResponse,
	PLARGE_INTEGER		Timeout,
	PUCHAR				PduRegister
	);


//	configure IDE ODD.

NTSTATUS
LurnIdeOddConfigure (
	IN	PLURELATION_NODE	Lurn,
	IN  PLARGE_INTEGER		Timeout,
	OUT	PBYTE				PduResponse

	)
{
	NTSTATUS			status;

	PLURNEXT_IDE_DEVICE	ideDisk;

	struct hd_driveid	info;
	char				buffer[41];

	LANSCSI_PDUDESC		pduDesc;

	BOOLEAN				setDmaMode;

	UCHAR				pioMode = 0;
	UCHAR				pioFeature = 0;


	ideDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;
	
	ideDisk->LuHwData.PduDescFlags &= ~( PDUDESC_FLAG_LBA | PDUDESC_FLAG_LBA48 | PDUDESC_FLAG_PIO|PDUDESC_FLAG_DMA | PDUDESC_FLAG_UDMA );

	do {
#if __USE_PRESET_PIO_MODE__
		//
		// Default PIO mode setting.
		// Some device requires PIO setting before IDENTIFY command.
		// Ex) SunPlus SPIF223A
		//

		pioMode = NDAS_DEFAULT_PIO_MODE;
		pioFeature = pioMode | 0x08;	
		LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, Timeout );

		pduDesc.Feature = SETFEATURES_XFER;
		pduDesc.BlockCount = pioFeature;

		status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

		if (status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed(1)...\n") );
			break;
		} else if(*PduResponse != LANSCSI_RESPONSE_SUCCESS) {
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed(2)...\n") );
			// Ignore the response. Proceed to Identify command.
		}
#endif

		// identify

		pduDesc.Feature = 0;
		pduDesc.BlockCount = 0;
		LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_PIDENTIFY, 0, 1, sizeof(struct hd_driveid), &info, Timeout );

		status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

	} while(FALSE);

	if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Identify Failed...\n") );

		return status;
	}

	// DMA/PIO Mode.

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
			  ("Major 0x%x, Minor 0x%x, Capa 0x%x\n", info.major_rev_num, info.minor_rev_num, info.capability) );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
			  ("DMA 0x%x, U-DMA 0x%x PIO bytecount=%d\n", info.dma_mword, info.dma_ultra, info.pio_bytecount) );


	//	Determine PIO Mode
	
	pioMode = 2;

	if (info.eide_pio_modes & 0x0001) {

		pioMode = 3;
	}

	if (info.eide_pio_modes & 0x0002) {

			pioMode = 4;
	}

	pioFeature = pioMode | 0x08;

	LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, Timeout );

	pduDesc.Feature = SETFEATURES_XFER;
	pduDesc.BlockCount = pioFeature;
	
	status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );

	if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
			
		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed...\n") );
			
		return status;
	}
	
	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.
	
	setDmaMode = FALSE;

	do {

		UCHAR	dmaFeature;
		UCHAR	dmaMode;

		dmaFeature = 0;
		dmaMode = 0;

		// Ultra DMA if NDAS chip is 2.0 or higher.
		
		// We don't support UDMA for 2.0 rev 0 due to the bug.
		// The bug : Written data using UDMA will be corrupted
		
		if ((info.dma_ultra & 0x00ff) &&
			((LANSCSIIDE_VERSION_2_0 < ideDisk->LuHwData.HwVersion) ||
			((LANSCSIIDE_VERSION_2_0 == ideDisk->LuHwData.HwVersion) && (0 != ideDisk->LuHwData.HwRevision)))) {

			// Find Fastest Mode.
			
			if (info.dma_ultra & 0x0001) {

				dmaMode = 0;
			}
		
			if (info.dma_ultra & 0x0002) {

				dmaMode = 1;
			}

			if (info.dma_ultra & 0x0004) {

				dmaMode = 2;
			}

			//	if Cable80, try higher Ultra Dma Mode.

#if __DETECT_CABLE80__
			if (info.hw_config & 0x2000) {
#endif
				if (info.dma_ultra & 0x0008) {

					dmaMode = 3;
				}

				if (info.dma_ultra & 0x0010) {

					dmaMode = 4;
				}

				if (info.dma_ultra & 0x0020) {

					  dmaMode = 5;
				}

				if (info.dma_ultra & 0x0040) {

					dmaMode = 6;
				}
				
				if (info.dma_ultra & 0x0080) {

					  dmaMode = 7;
				}

				// If the ndas device is version 2.0 revision 100Mbps,
				// Restrict UDMA to mode 2.
				
				if (ideDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_2_0 && 
					ideDisk->LuHwData.HwRevision == LANSCSIIDE_VER20_REV_100M) {

					if (dmaMode > 2) {
					
						dmaMode = 2;
					}
				}

				// Restrict UDMA mode when requested.
		
				if (Lurn->UDMARestrictValid) {
				
					if (dmaMode > Lurn->UDMARestrict) {
					
						dmaMode = Lurn->UDMARestrict;
					
						DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
									("UDMA restriction applied. UDMA=%d\n", (ULONG)dmaMode) );
					}
				}

#if __DETECT_CABLE80__
			}
#endif
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Ultra DMA %d detected.\n", (int)dmaMode) );
			
			dmaFeature = dmaMode | 0x40;	// Ultra DMA mode.
			
			ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA;

			setDmaMode = TRUE;

		//	DMA
	
		} else if (info.dma_mword & 0x00ff) {

			if (info.dma_mword & 0x0001) {

				dmaMode = 0;
			}

			if (info.dma_mword & 0x0002) {

				dmaMode = 1;
			}

			if (info.dma_mword & 0x0004) {

				dmaMode = 2;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("DMA mode %d detected.\n", (int)dmaMode) );
			
			dmaFeature = dmaMode | 0x20;

			ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_DMA;

			// Set DMA mode if needed
			
			if (!(info.dma_mword & (0x0100 << dmaMode))) {
			
				setDmaMode = TRUE;
			}
		}

		// Set DMA mode if needed.

		if (setDmaMode) {

			LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, 0, NULL, Timeout );
			
			pduDesc.Feature = 0x03;
			pduDesc.BlockCount = dmaFeature;
			
			status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );
			
			if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
				
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Set Feature Failed...\n") );

				return status;
			}

			// identify.
			LURNIDE_INITIALIZE_PDUDESC( ideDisk, &pduDesc, IDE_COMMAND, WIN_PIDENTIFY, 0, 1, sizeof(struct hd_driveid), &info, Timeout );

			status = LspRequest( ideDisk->LanScsiSession, &pduDesc, PduResponse, NULL, NULL, NULL );
			
			if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
				
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Identify Failed...\n") );
				return status;
			}

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					  ("After Set Feature DMA 0x%x, U-DMA 0x%x\n", 
						info.dma_mword, info.dma_ultra) );
		}

		if (ideDisk->LuHwData.PduDescFlags & PDUDESC_FLAG_DMA) {
			
			break;
		}

		//	PIO.

		NDAS_BUGON( FALSE );
		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("NetDisk does not support DMA mode. Turn to PIO mode.\n") );

		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_PIO;

	} while(0);


	//	Product strings.

	ConvertString( (PCHAR)buffer, (PCHAR)info.serial_no, 20 );
	RtlCopyMemory( ideDisk->LuHwData.Serial, buffer, 20 );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Serial No: %s\n", buffer) );

	ConvertString( (PCHAR)buffer, (PCHAR)info.fw_rev, 8 );
	RtlCopyMemory(ideDisk->LuHwData.FW_Rev, buffer, 8 );
	
	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Firmware rev: %s\n", buffer) );

	ConvertString( (PCHAR)buffer, (PCHAR)info.model, 40 );
	RtlCopyMemory( ideDisk->LuHwData.Model, buffer, 40 );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("Model No: %s\n", buffer) );

	ideDisk->DVD_TYPE = 0;

	if (Lurn->LurnType == LURN_CDROM) {

		if (strlen(IO_DATA_STR) == RtlCompareMemory(buffer,IO_DATA_STR, strlen(IO_DATA_STR))) {

			KDPrintM(DBG_LURN_INFO, ("Find: IO_DATA_DEVICE: %s\n", buffer));
			ideDisk->DVD_TYPE = IO_DATA;
		
		} else if(strlen(LOGITEC_STR) == RtlCompareMemory(buffer, LOGITEC_STR, strlen(LOGITEC_STR)) ) {

			KDPrintM(DBG_LURN_INFO, ("Find: LOGITEC: %s\n", buffer));
			ideDisk->DVD_TYPE = LOGITEC;

		} else if(strlen(IO_DATA_STR_9573) == RtlCompareMemory(buffer, IO_DATA_STR_9573, strlen(IO_DATA_STR_9573))) {
			
			KDPrintM(DBG_LURN_INFO, ("Find: LOGITEC: %s\n", buffer));
			ideDisk->DVD_TYPE = IO_DATA_9573;
		}
	}

	NDAS_BUGON( ideDisk->DVD_TYPE == 0 );

	// Support LBA?

	if (!(info.capability & 0x02)) {
	
		NDAS_BUGON( FALSE );
		ideDisk->LuHwData.PduDescFlags &= ~PDUDESC_FLAG_LBA;
	
	} else {
		
		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_LBA;
	}

	// Support LBA48.
	// Calc Capacity.
 
	if ((info.command_set_2 & 0x0400) && (info.cfs_enable_2 & 0x0400)) {	// Support LBA48bit
		
		ideDisk->LuHwData.PduDescFlags |= PDUDESC_FLAG_LBA48;
		ideDisk->LuHwData.SectorCount = info.lba_capacity_2;
	
	} else {
	
		ideDisk->LuHwData.PduDescFlags &= ~PDUDESC_FLAG_LBA48;

		if ((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
		
			ideDisk->LuHwData.SectorCount = info.lba_capacity;

		} else {

			ideDisk->LuHwData.SectorCount = info.cyls * info.heads * info.sectors;	
		}
	}

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
			    ("LBA %d, LBA48 %d, Number of Sectors: %I64d\n", 
				(ideDisk->LuHwData.PduDescFlags & PDUDESC_FLAG_LBA) != 0,
				(ideDisk->LuHwData.PduDescFlags & PDUDESC_FLAG_LBA48) != 0,
				ideDisk->LuHwData.SectorCount) );

	return STATUS_SUCCESS;
}

#if 0

static void TranslateScsi2Packet(PCCB Ccb)
{
	UCHAR * c, *sc; 
	UCHAR * buf, *sc_buf;
	RtlZeroMemory(Ccb->PKCMD,MAXIMUM_CDB_SIZE);
	RtlCopyMemory(Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength);
	Ccb->SecondaryBufferLength = Ccb->DataBufferLength;
	Ccb->SecondaryBuffer = Ccb->DataBuffer;

	c = Ccb->PKCMD;
	sc = Ccb->Cdb;
	sc_buf = Ccb->DataBuffer;

	//
	// Read/Write commands
	//
	if(c[0] == SCSIOP_READ6 || c[0] == SCSIOP_WRITE6){
		if(c[4] == 0)
		{
			c[7] = 1;
			c[8] = 0;
		}else{
			c[7] = 0;
			c[8] = c[4];
		}

		c[5] = c[3];	
		c[4] = c[2];
		c[3] = c[1] & 0x1f;	
		c[2] = 0;		
		c[1] &= 0xe0;
		c[0] += (SCSIOP_READ - SCSIOP_READ6);


	}

	//
	//	ModeSense/Select commands
	//
	if(c[0] == SCSIOP_MODE_SENSE || c[0] == SCSIOP_MODE_SELECT) {

		if(!sc_buf) return;
		buf = ExAllocatePoolWithTag(NonPagedPool, Ccb->DataBufferLength + 4, PACKETCMD_BUFFER_POOL_TAG);
		if(buf == NULL) {
			KDPrintM(DBG_LURN_ERROR, ("Can't Allocate Buffer for PKCMD 0x%02x\n", Ccb->Cdb[0]));
			return;
		}

		KDPrintM(DBG_LURN_ERROR, ("MODE_SENSE/SELECT PKCMD 0x%02x\n",c[0]));

		RtlZeroMemory(buf, Ccb->DataBufferLength + 4);
		RtlZeroMemory(c, MAXIMUM_CDB_SIZE);
		if(sc[0] == SCSIOP_MODE_SENSE){
			USHORT	size;
			c[0] = (sc[0] | 0x40);		c[1] = sc[1];		c[2] = sc[2];
			size = (USHORT)sc[4] + 4;
			c[8] = (UCHAR)(size & 0xFF);
			c[7] = (UCHAR)((size >> 8) & 0xFF);
			c[9] = sc[5];

		}

		if(sc[0] == SCSIOP_MODE_SELECT){
			USHORT	dataLenInCmd;

			KDPrintM(DBG_LURN_ERROR, ("First mode page:0x%02x Original DataBuffer size=%d, Data size in Command=%d\n", (int)sc_buf[4], Ccb->DataBufferLength, (int)sc[4]));

			c[0] = (sc[0] | 0x40);		c[1] = sc[1];
			dataLenInCmd = (USHORT)sc[4] + 4 ;
			c[8] = (UCHAR)(dataLenInCmd & 0xFF);
			c[7] = (UCHAR)((dataLenInCmd >> 8) & 0xFF);
			c[9] = sc[5];

			buf[1] = sc_buf[0];	// Mode data length
			buf[2] = sc_buf[1];	// Medium type
			buf[3] = sc_buf[2];	// DeviceSpecificParameter
			buf[7] = sc_buf[3];	// BlockDescriptorLength
			RtlCopyMemory(buf + 8, sc_buf + 4, Ccb->DataBufferLength-8/* -4 */);
		}
		Ccb->SecondaryBuffer = buf;
		Ccb->SecondaryBufferLength = Ccb->DataBufferLength + 4;

#if __BSRECORDER_HACK__
		//
		//	Fix the MODE_READ_WRITE_RECOVERY_PAGE in MODE_SELECT command.
		//	The command comes from B's clip eraser with DVD+RW.
		//
		if(	sc[0] == SCSIOP_MODE_SELECT ) {

			if(	buf[8] == 0x01 &&
				buf[9] == 0x06
			) {

			KDPrintM(DBG_LURN_ERROR, ("MODE_READ_WRITE_RECOVERY_PAGE(0x01) in length of 0x06! Fix it!!!!!!!\n"));
			c[8] -= 4;
			buf[9]  = 0xa;
			buf[16] = 0x1;
			buf[17] = buf[18] = buf[19] = 0;
			Ccb->SecondaryBufferLength -= 4;

			//
			//	Fix the MODE_WRITE_PARAMETER_PAGE in MODE_SELECT command.
			//	The command comes from B's clip eraser with DVD-RW.
			//
			} else if(	buf[8]== 0x05 &&
						sc[4] == 0x3b
					) {		// Parameter data size.

				KDPrintM(DBG_LURN_ERROR, ("MODE_WRITE_PARAMETER_PAGE(0x01) with the wrong legnth in the command! Fix it!!!!!!!\n"));
				c[8] -= 3;
				Ccb->SecondaryBufferLength -= 3;
			}
		}
#endif

	}

#if __BSRECORDER_HACK__
	//
	//	0xD8 vendor command to READ_CD.
	//
	if(c[0] == 0xD8) {
		ULONG	TransferLength;
		ULONG	DataBufferLength;

		TransferLength = ((ULONG)c[6]<<24) | ((ULONG)c[7]<<16) | ((ULONG)c[8]<<8) | c[9];
		DataBufferLength = Ccb->SecondaryBufferLength;

		if((DataBufferLength / TransferLength) == 2352) {
			KDPrintM(DBG_LURN_INFO, ("Vendor PKCMD 0x%02x to READ_CD(0xBE)\n",c[0]));

			c[0] = SCSIOP_READ_CD;
			c[1] = 0x04;		// Expected sector type: CD-DA
			c[6] = c[7];		// Transfer length
			c[7] = c[8];
			c[8] = c[9];
			c[9] = 0xf0;		// SYNC, All headers, UserData included.
		} else {
			ASSERT(FALSE);
		}
	}
#endif

}

#endif

NTSTATUS
IdeOddLurnCreate (
	PLURELATION_NODE		Lurn,
	PLURELATION_NODE_DESC	LurnDesc
	)
{
	NTSTATUS				status;
	PLURNEXT_IDE_DEVICE		IdeDisk;
	// Indicate job done.
	BOOLEAN					Connection, PrimaryConnection;
	BOOLEAN					LogIn, PrimaryLogin;
	BOOLEAN					ThreadRef;
	BOOLEAN					LssEncBuff;
	BOOLEAN					lockMgmtInit;

	OBJECT_ATTRIBUTES		objectAttributes;
	LSSLOGIN_INFO			loginInfo;
	LSPROTO_TYPE			LSProto;
	BYTE					response;
	LARGE_INTEGER			defaultTimeOut;
	ULONG					maxTransferLength;
	BOOLEAN					FirstInit;
	BOOLEAN					initialBuffControlState;

	DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
				("IN Lurn = %p, LurnDesc = %p\n", Lurn, LurnDesc) );


	Connection = FALSE;
	PrimaryConnection = FALSE;
	LogIn = FALSE;
	PrimaryLogin = FALSE;
	ThreadRef = FALSE;
	LssEncBuff = FALSE;
	lockMgmtInit = FALSE;

	// already alive

	if (LURN_STATUS_RUNNING == Lurn->LurnStatus) {

		NDAS_BUGON( FALSE );
		return STATUS_SUCCESS;
	}

	NDAS_BUGON( LurnDesc );

	// initializing cycle

	FirstInit = TRUE;
	ASSERT( !Lurn->LurnExtension );

	IdeDisk = ExAllocatePoolWithTag(NonPagedPool, sizeof(LURNEXT_IDE_DEVICE), LURNEXT_POOL_TAG);

	if (!IdeDisk) {

		return STATUS_INSUFFICIENT_RESOURCES;
	}

	do {

		//	Initialize fields

		Lurn->AccessRight = LurnDesc->AccessRight;

		Lurn->LurnExtension = IdeDisk;
		RtlZeroMemory(IdeDisk, sizeof(LURNEXT_IDE_DEVICE) );

		InitializeListHead( &IdeDisk->AllListEntry );

		IdeDisk->Lurn = Lurn;

		IdeDisk->ReConnectInterval.QuadPart = Lurn->ReconnInterval;

		IdeDisk->MaxStallInterval.QuadPart  =
			IDE_DISK_DEFAULT_MAX_STALL_INTERVAL(Lurn->ReconnTrial,
			IdeDisk->ReConnectInterval.QuadPart);

		if (!LURN_IS_ROOT_NODE(Lurn)) {

			IdeDisk->MaxStallInterval.QuadPart /= 4; 
		}

		IdeDisk->LuHwData.HwType = LurnDesc->LurnIde.HWType;
		IdeDisk->LuHwData.HwVersion = LurnDesc->LurnIde.HWVersion;
		IdeDisk->LuHwData.HwRevision = LurnDesc->LurnIde.HWRevision;
		IdeDisk->LuHwData.LanscsiTargetID = LurnDesc->LurnIde.LanscsiTargetID;
		IdeDisk->LuHwData.LanscsiLU = LurnDesc->LurnIde.LanscsiLU;
		IdeDisk->MaxDataSendLength = LurnDesc->MaxDataSendLength;
		IdeDisk->MaxDataRecvLength = LurnDesc->MaxDataRecvLength;

		// Data transfer length used for allocation of LSS encryption,
		// content encryption, and write check buffer.

		maxTransferLength = IdeDisk->MaxDataSendLength > IdeDisk->MaxDataRecvLength ? 
								IdeDisk->MaxDataSendLength :IdeDisk->MaxDataRecvLength;

		// Write check buffer if needed

		if (IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_2_0 &&
			IdeDisk->LuHwData.HwRevision == LANSCSIIDE_VER20_REV_1G_ORIGINAL) {

			IdeDisk->WriteCheckBuffer = ExAllocatePoolWithTag( NonPagedPool, 
															   maxTransferLength, 
															   LURNEXT_WRITECHECK_BUFFER_POOLTAG );
		
			if (!IdeDisk->WriteCheckBuffer) {
		
				// Continue without write check function

				DebugTrace( DBG_PROTO_ERROR, ("Error! Could not allocate the write-check buffer!\n"));
			}

		} else {

			IdeDisk->WriteCheckBuffer = NULL;
		}

		//	Update lowest h/w version among NDAS devices

		if (Lurn->Lur->LowestHwVer > LurnDesc->LurnIde.HWVersion) {
	
			Lurn->Lur->LowestHwVer = LurnDesc->LurnIde.HWVersion;
		}

		if (Lurn->Lur->MaxChildrenSectorCount < IdeDisk->LuHwData.SectorCount) {

			Lurn->Lur->MaxChildrenSectorCount = IdeDisk->LuHwData.SectorCount;
		}

		KeInitializeEvent( &IdeDisk->ThreadCcbArrivalEvent,
						   NotificationEvent,
						   FALSE );

		InitializeListHead( &IdeDisk->CcbQueue );

		KeInitializeEvent( &IdeDisk->ThreadReadyEvent,
						   NotificationEvent,
						   FALSE );

		//	Confirm address type.
		//	Connect to the NDAS Node.

		IdeDisk->AddressType = LurnDesc->LurnIde.BindingAddress.Address[0].AddressType;

		status = LspLookupProtocol( LurnDesc->LurnIde.HWType, LurnDesc->LurnIde.HWVersion, &LSProto );

		if (!NT_SUCCESS(status)) {

			NDAS_BUGON( FALSE );
			break;
		}

		if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			// Set default time out

			defaultTimeOut.QuadPart = -LURN_IDE_GENERIC_TIMEOUT;
			LspSetDefaultTimeOut( &IdeDisk->PrimaryLanScsiSession, &defaultTimeOut );

			NDAS_BUGON( IdeDisk->PrimaryLanScsiSession.LanscsiProtocol == NULL );

			status = LspConnectMultiBindAddr( &IdeDisk->PrimaryLanScsiSession,
											  &LurnDesc->LurnIde.BindingAddress,
											  NULL,
											  NULL,
											  &LurnDesc->LurnIde.TargetAddress,
											  TRUE,
											  NULL,
											  NULL,
											  NULL );


			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL );

				LurnRecordFault( Lurn, LURN_ERR_CONNECT, status, NULL );			
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("LspConnectMultiBindAddr(), Can't Connect to the LS node. STATUS:0x%08x\n", 
							 status) );
		
				break;
			}
	
			PrimaryConnection = TRUE;

			//	Login to the Lanscsi Node.

			loginInfo.LoginType	= LOGIN_TYPE_NORMAL;

			RtlCopyMemory( &loginInfo.UserID, &LurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH );
			RtlCopyMemory( &loginInfo.Password, &LurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH );
	
			loginInfo.MaxDataTransferLength = maxTransferLength;
	
			NDAS_BUGON( loginInfo.MaxDataTransferLength == (64*1024) || 
							 loginInfo.MaxDataTransferLength == (32*1024) );

			loginInfo.LanscsiTargetID = LurnDesc->LurnIde.LanscsiTargetID;
			loginInfo.LanscsiLU = LurnDesc->LurnIde.LanscsiLU;
			loginInfo.HWType = LurnDesc->LurnIde.HWType;
			loginInfo.HWVersion = LurnDesc->LurnIde.HWVersion;
			loginInfo.HWRevision = LurnDesc->LurnIde.HWRevision;
	
			if (Lurn->Lur->CntEcrMethod) {

				//	IdeLurn will have a encryption buffer.
		
				loginInfo.IsEncryptBuffer = FALSE;
				DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("IDE Lurn extension is supposed to have a encryption buffer.\n") );

			} else {

				//	IdeLurn will NOT have a encryption buffer.
				//	LSS should have a buffer to speed up.
				//
		
				loginInfo.IsEncryptBuffer = TRUE;
				DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("LSS is supposed to have a encryption buffer.\n"));
			}

			status = LspLogin( &IdeDisk->PrimaryLanScsiSession,
							   &loginInfo,
							   LSProto,
							   NULL,
							   TRUE );

			if (status != STATUS_SUCCESS) {
	
				LurnRecordFault(Lurn, LURN_ERR_LOGIN, IdeDisk->PrimaryLanScsiSession.LastLoginError, NULL);
		
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("LspLogin(), Can't login to the LS node"
							" with UserID:%08lx. STATUS:0x%08x.\n", loginInfo.UserID, status));
	
				status = STATUS_ACCESS_DENIED;
		
				break;
			}
	
			PrimaryLogin = TRUE;
		}

		// Set default time out

		defaultTimeOut.QuadPart = -LURN_IDE_GENERIC_TIMEOUT;
		LspSetDefaultTimeOut( &IdeDisk->CommLanScsiSession, &defaultTimeOut );

		NDAS_BUGON( IdeDisk->CommLanScsiSession.LanscsiProtocol == NULL );

		status = LspConnectMultiBindAddr( &IdeDisk->CommLanScsiSession,
										  &LurnDesc->LurnIde.BindingAddress,
										  NULL,
										  NULL,
										  &LurnDesc->LurnIde.TargetAddress,
										  TRUE,
										  NULL,
										  NULL,
										  NULL );


		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( NDAS_BUGON_NETWORK_FAIL );

			LurnRecordFault( Lurn, LURN_ERR_CONNECT, status, NULL );			
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("LspConnectMultiBindAddr(), Can't Connect to the LS node. STATUS:0x%08x\n", 
						 status) );
		
			break;
		}
	
		Connection = TRUE;

		loginInfo.LoginType	= LOGIN_TYPE_NORMAL;

		RtlCopyMemory( &loginInfo.UserID, &LurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH );
		loginInfo.UserID = CONVERT_TO_ROUSERID( loginInfo.UserID );

		RtlCopyMemory( &loginInfo.Password, &LurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH );
	
		loginInfo.MaxDataTransferLength = maxTransferLength;
	
		NDAS_BUGON( loginInfo.MaxDataTransferLength == (64*1024) || 
						 loginInfo.MaxDataTransferLength == (32*1024) );

		loginInfo.LanscsiTargetID = LurnDesc->LurnIde.LanscsiTargetID;
		loginInfo.LanscsiLU = LurnDesc->LurnIde.LanscsiLU;
		loginInfo.HWType = LurnDesc->LurnIde.HWType;
		loginInfo.HWVersion = LurnDesc->LurnIde.HWVersion;
		loginInfo.HWRevision = LurnDesc->LurnIde.HWRevision;
	
		if (Lurn->Lur->CntEcrMethod) {

			//	IdeLurn will have a encryption buffer.
		
			loginInfo.IsEncryptBuffer = FALSE;
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("IDE Lurn extension is supposed to have a encryption buffer.\n"));

		} else {

			//	IdeLurn will NOT have a encryption buffer.
			//	LSS should have a buffer to speed up.
			//
		
			loginInfo.IsEncryptBuffer = TRUE;
			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("LSS is supposed to have a encryption buffer.\n"));
		}

		status = LspLogin( &IdeDisk->CommLanScsiSession,
						   &loginInfo,
						   LSProto,
						   NULL,
						   TRUE );

		if (status != STATUS_SUCCESS) {
	
			LurnRecordFault(Lurn, LURN_ERR_LOGIN, IdeDisk->CommLanScsiSession.LastLoginError, NULL);
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("LspLogin(), Can't login to the LS node"
						" with UserID:%08lx. STATUS:0x%08x.\n", loginInfo.UserID, status));
	
			status = STATUS_ACCESS_DENIED;
		
			break;
		}

		LogIn = TRUE;

		IdeDisk->LanScsiSession = &IdeDisk->CommLanScsiSession;

		if (Lurn->IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_0) {

			if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

				IdeDisk->LanScsiSession = &IdeDisk->PrimaryLanScsiSession;
			}		
		}

		//	Configure the disk.

		status = LurnIdeOddConfigure( Lurn, NULL, &response );

		NDAS_BUGON( status == STATUS_SUCCESS || status == STATUS_PORT_DISCONNECTED );

		if (status != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {

			NDAS_BUGON( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
						("LurnIdeDiskConfigure() failed. Response:%d. STATUS:%08lx\n", response, status));
			
			status = STATUS_PORT_DISCONNECTED;

			break;
		}

		// Get the inquiry data.

		status = ODD_Inquiry(IdeDisk);
		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
				("ODD_Inquiry() failed. Response:%d. STATUS:%08lx\n", response, status));

			status = STATUS_PORT_DISCONNECTED;

			break;
		}

#if 0

		// Save power recycle count to detect power recycle during reconnection.

		if (IdeDisk->LuHwData.LudataFlags & LUDATA_FLAG_SMART_ENABLED) {
		
			status = LurnIdeDiskGetPowerRecycleCount( IdeDisk,
													  &IdeDisk->LuHwData.InitialDevPowerRecycleCount );

			if (status == STATUS_NOT_SUPPORTED) {
		
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("No Power recycle count. Initial power recycle count = zero\n") );

			} else if (status != STATUS_SUCCESS) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
						   ("LurnIdeDiskGetPowerRecycleCount() failed. Response:%d. STATUS:%08lx\n", response, status));
		
				break;

			} else {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("Initial power recycle count = %I64u\n", IdeDisk->LuHwData.InitialDevPowerRecycleCount) );
		
				//remove assert if disk is abnormal powercyclecount can be 0
				//NDAS_BUGON( IdeDisk->LuHwData.InitialDevPowerRecycleCount );
			}

		} else {

			// If SMART disabled, continue without power recycle count.
			// Disable power recycle detection using SMART.

			IdeDisk->LuHwData.InitialDevPowerRecycleCount = 0;
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("SMART disabled. Initial power recycle count = zero\n") );
		}

		if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {
		
			status = NdasTransRegisterDisconnectHandler( &IdeDisk->PrimaryLanScsiSession.NdastAddressFile,
													     IdeDiskDisconnectHandler,
														 Lurn );
			
			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( FALSE );
				break;
			}
		}

		status = NdasTransRegisterDisconnectHandler( &IdeDisk->CommLanScsiSession.NdastAddressFile,
												     IdeDiskDisconnectHandler,
													 Lurn );
			
		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
			break;
		}
#endif

		// Set bytes of block returned by configuration function.

		Lurn->ChildBlockBytes = IdeDisk->LuHwData.BlockBytes;
		Lurn->BlockBytes = Lurn->ChildBlockBytes;
		
		Lurn->MaxDataSendLength = IdeDisk->MaxDataSendLength;
		Lurn->MaxDataRecvLength = IdeDisk->MaxDataRecvLength;

		// Traffic control configuration for read/write operation
		// Start with max possible blocks.
	
		IdeDisk->WriteSplitSize	= IdeDisk->MaxDataSendLength;
		IdeDisk->ReadSplitSize	= IdeDisk->MaxDataRecvLength;
	
		ASSERT(IdeDisk->WriteSplitSize <= 65536);
		ASSERT(IdeDisk->WriteSplitSize >0);
		ASSERT(IdeDisk->ReadSplitSize <= 65536);
		ASSERT(IdeDisk->ReadSplitSize >0);

		//	Content encryption.

		IdeDisk->CntEcrMethod		= Lurn->Lur->CntEcrMethod;

		if (IdeDisk->CntEcrMethod) {

			PUCHAR	pPassword;
			PUCHAR	pKey;
			ULONG	encBuffLen;

			pPassword = (PUCHAR)&(LurnDesc->LurnIde.Password);
			pKey = Lurn->Lur->CntEcrKey;

			status = CreateCipher( &IdeDisk->CntEcrCipher,
								   IdeDisk->CntEcrMethod,
								   NCIPHER_MODE_ECB,
								   HASH_KEY_LENGTH,
								   pPassword );

			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( FALSE );			
				break;
			}

			status = CreateCipherKey( IdeDisk->CntEcrCipher,
									  &IdeDisk->CntEcrKey,
									  Lurn->Lur->CntEcrKeyLength,
									  pKey );

			if (status != STATUS_SUCCESS) {

				NDAS_BUGON( FALSE );			
				break;
			}

			//	Allocate the encryption buffer

			encBuffLen = IdeDisk->MaxDataSendLength > IdeDisk->MaxDataRecvLength ?
							IdeDisk->MaxDataSendLength:IdeDisk->MaxDataRecvLength;
		
			IdeDisk->CntEcrBuffer = ExAllocatePoolWithTag( NonPagedPool, 
														   encBuffLen, 
														   LURNEXT_ENCBUFF_TAG );

			if (IdeDisk->CntEcrBuffer) {

				IdeDisk->CntEcrBufferLength = encBuffLen;

			} else {

				NDAS_BUGON( FALSE );
				IdeDisk->CntEcrBufferLength = 0;
			}
		}

		// Initialize Lock management
	
		if (FlagOn(LurnDesc->LurnOptions, LURNOPTION_NO_BUFF_CONTROL)) {
		
			initialBuffControlState = FALSE;
	
		} else {
		
			initialBuffControlState = TRUE;
		}

		status = LMInitialize( Lurn, 
							   &IdeDisk->BuffLockCtl, 
							   initialBuffControlState );

		if (status != STATUS_SUCCESS) {
		
			NDAS_BUGON( FALSE );
			break;
		}


		lockMgmtInit = TRUE;

		NdasRaidRegisterIdeDisk( &NdasrGlobalData, IdeDisk );


		//	create worker thread
	
		InitializeObjectAttributes( &objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL );
	
		status = PsCreateSystemThread( &IdeDisk->ThreadHandle,
									   THREAD_ALL_ACCESS,
									   &objectAttributes,
									   NULL,
									   NULL,
									   LurnIdeOddThreadProc,
									   Lurn );

		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
			break;
		}

		ASSERT( IdeDisk->ThreadHandle );
	
		status = ObReferenceObjectByHandle( IdeDisk->ThreadHandle,
											GENERIC_ALL,
											NULL,
											KernelMode,
											&IdeDisk->ThreadObject,
											NULL );

		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
			break;
		}

		ThreadRef = TRUE;

		status = KeWaitForSingleObject( &IdeDisk->ThreadReadyEvent,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( FALSE );
			break;
		}
	
	} while (0);

	if (status != STATUS_SUCCESS) {

		NdasRaidUnregisterIdeDisk( &NdasrGlobalData, IdeDisk );

		if (ThreadRef) {

			ObDereferenceObject( &IdeDisk->ThreadObject );
		}

		if (lockMgmtInit) {

			LMDestroy( &IdeDisk->BuffLockCtl );
		}

		if (LogIn) {

			LspLogout( &IdeDisk->CommLanScsiSession, NULL );
		}

		if (Connection) {
		
			LspDisconnect( &IdeDisk->CommLanScsiSession );
		}

		if (PrimaryLogin) {

			LspLogout( &IdeDisk->PrimaryLanScsiSession, NULL );
		}

		if (PrimaryConnection) {

			LspDisconnect( &IdeDisk->PrimaryLanScsiSession );
		}

		if (FirstInit && Lurn->LurnExtension) {
		
			PLURNEXT_IDE_DEVICE		tmpIdeDisk;

			tmpIdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

			if (tmpIdeDisk->WriteCheckBuffer) {
		
				ExFreePoolWithTag(tmpIdeDisk->WriteCheckBuffer, LURNEXT_WRITECHECK_BUFFER_POOLTAG);
				tmpIdeDisk->WriteCheckBuffer = NULL;
			}

			if (tmpIdeDisk->CntEcrKey) {
		
				CloseCipherKey(tmpIdeDisk->CntEcrKey);
				tmpIdeDisk->CntEcrKey = NULL;
			}

			if (tmpIdeDisk->CntEcrCipher) {
			
				CloseCipher(tmpIdeDisk->CntEcrCipher);
				tmpIdeDisk->CntEcrCipher = NULL;
			}

			ExFreePoolWithTag(Lurn->LurnExtension, LURNEXT_POOL_TAG);
			Lurn->LurnExtension = NULL;
		}
	}

	return status;
}

IdeOddLurnStop2 (
	PLURELATION_NODE	Lurn
	)
{
	NTSTATUS	status = STATUS_SUCCESS;
	KIRQL		oldIrql;
	

	NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );
	NDAS_BUGON( Lurn->IdeDisk->ThreadHandle != NULL );

	NdasRaidUnregisterIdeDisk( &NdasrGlobalData, Lurn->IdeDisk );
	
	ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

	Lurn->IdeDisk->RequestToTerminate = TRUE;
	
	if (Lurn->IdeDisk->ThreadHandle) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("KeSetEvent\n") );

		KeSetEvent( &Lurn->IdeDisk->ThreadCcbArrivalEvent,IO_NO_INCREMENT, FALSE );
	}
	
	RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

	if (Lurn->IdeDisk->ThreadHandle) {

		status = KeWaitForSingleObject( Lurn->IdeDisk->ThreadObject,
										Executive,
										KernelMode,
										FALSE,
										NULL );

		ASSERT( status == STATUS_SUCCESS );

		ObDereferenceObject( Lurn->IdeDisk->ThreadObject );
		ZwClose( Lurn->IdeDisk->ThreadHandle );
	}

	ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

	Lurn->IdeDisk->ThreadObject = NULL;
	Lurn->IdeDisk->ThreadHandle = NULL;

	RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

	LspLogout( Lurn->IdeDisk->LanScsiSession, NULL );
	LspDisconnect( Lurn->IdeDisk->LanScsiSession );

	if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {
		
		LspLogout( &Lurn->IdeDisk->PrimaryLanScsiSession, NULL );
		LspDisconnect( &Lurn->IdeDisk->PrimaryLanScsiSession );
	}

	DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("stopped completely\n") );

	return status;
}


NTSTATUS
IdeOddLurnRequest (
	IN	PLURELATION_NODE	Lurn,
	IN	PCCB				Ccb
	)
{
	NTSTATUS		status;
	KIRQL			oldIrql;

	if (Lurn->IdeDisk == NULL) {

		NDAS_BUGON( Lurn->LurnStatus == LURN_STATUS_INIT || Lurn->LurnStatus == LURN_STATUS_DESTROYING );

		LsCcbSetStatus( Ccb, CCB_STATUS_NOT_EXIST );
		LsCcbCompleteCcb( Ccb );

		return STATUS_SUCCESS;
	}

	if (Ccb->OperationCode == CCB_OPCODE_SHUTDOWN) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("CCB_OPCODE_SHUTDOWN, Ccb->Srb = %08x, Ccb->AssociateID = %d\n",
					 Ccb->Srb, Ccb->AssociateID) );

		// Nothing to do for IDE disk		
		
		Ccb->CcbStatus =CCB_STATUS_SUCCESS;
		LsCcbCompleteCcb( Ccb );

		return STATUS_SUCCESS;
	
	}
	
	if (Ccb->OperationCode == CCB_OPCODE_STOP) {

		NDAS_BUGON( KeGetCurrentIrql() ==  PASSIVE_LEVEL );

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("CCB_OPCODE_STOP\n") );

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
		
		if (FlagOn(Lurn->LurnStatus, LURN_STATUS_STOP)) {

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );		

			status = IdeOddLurnStop2( Lurn );
			NDAS_BUGON( status == STATUS_SUCCESS );

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			LsCcbCompleteCcb( Ccb );

			return STATUS_SUCCESS;	
		}
	
		SetFlag( Lurn->LurnStatus, LURN_STATUS_STOP_PENDING );

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		status = IdeOddLurnStop2( Lurn );

		NDAS_BUGON( status == STATUS_SUCCESS );

		SetFlag( Lurn->LurnStatus, LURN_STATUS_STOP );

		LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		LsCcbCompleteCcb( Ccb );

		return STATUS_SUCCESS;
	}

	ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

	if (!LURN_IS_RUNNING(Lurn->LurnStatus)) {

		if (Lurn->LurnStatus == LURN_STATUS_STOP_PENDING) {
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("LURN%d(STATUS %08lx) "
						 "is in STOP_PENDING. Return with CCB_STATUS_STOP.\n", 
						 Lurn->LurnId, Lurn->LurnStatus) );
	
			LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_BUSCHANGE | CCBSTATUS_FLAG_LURN_STOP );
			Ccb->CcbStatus = CCB_STATUS_STOP;

			Lurn->LurnStatus = LURN_STATUS_STOP;

		} else {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("LURN%d(STATUS %08lx) "
						 "is not running. Return with NOT_EXIST.\n", 
						 Lurn->LurnId, Lurn->LurnStatus) );

			LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_LURN_STOP );
			Ccb->CcbStatus = CCB_STATUS_NOT_EXIST;
		}

		RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);

		LsCcbCompleteCcb(Ccb);

		return STATUS_SUCCESS;
	}

	InsertTailList( &Lurn->IdeDisk->CcbQueue, &Ccb->ListEntry );
	KeSetEvent( &Lurn->IdeDisk->ThreadCcbArrivalEvent, IO_NO_INCREMENT, FALSE );

	LsCcbMarkCcbAsPending( Ccb );

	RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

	return STATUS_PENDING;	
}

#if 0
static void TranslatePacket2Scsi(
		PLURNEXT_IDE_DEVICE		IdeDisk,
		PCCB Ccb)
{
	UCHAR * c, * sc; 
	UCHAR * buf, * sc_buf;

	buf = Ccb->SecondaryBuffer;
	sc_buf = Ccb->DataBuffer;
	c = Ccb->PKCMD;
	sc = Ccb->Cdb;

#if	__DVD_ENCRYPT_CONTENT__
	if(
		(IdeDisk->Lurn->LurnType == LURN_CDROM)
		&&(
			(c[0] == SCSIOP_READ)
				|| (c[0] == 0xA8)
				|| (c[0] == 0xBE)
				|| (c[0] == 0xB9) )
	)
	{
		ULONG	length = Ccb->PKDataBufferLength;
			Decrypt32SP(
				buf,
				length,
				&(IdeDisk->CntDcr_IR[0])
				);		
	}
#endif //#ifdef	__DVD_ENCRYPT_CONTENT__


	if(c[0] == SCSIOP_MODE_SENSE10 && sc[0] == SCSIOP_MODE_SENSE)
	{
		sc_buf[0] = buf[1];
		sc_buf[1] = buf[2];
		sc_buf[2] = buf[3];
		sc_buf[3] = buf[7];
		RtlCopyMemory(sc_buf + 4, buf + 8, Ccb->DataBufferLength - 4);

	}



	if(c[0] == SCSIOP_INQUIRY)
	{

		sc_buf[2] |= 2;  //SCSI 2 version
		sc_buf[3] = (sc_buf[3] & 0xf0) | 2; 
	}



	if(c[0] == SCSIOP_GET_CONFIGURATION)
	{
		int i,max = 1;

		if((c[2] == 0x0)
			&&(c[3] == 0x0))
		{
			IdeDisk->DVD_MEDIA_TYPE |= (sc_buf[6] << 8);
			IdeDisk->DVD_MEDIA_TYPE = sc_buf[7];
				
		}
		
		

		for(i = 0; i< max; i++)
		{
			KDPrintM(DBG_LURN_INFO, ("CONGIFURATION: (%02x):(%02x):(%02x):(%02x): (%02x):(%02x):(%02x):(%02x)\n",
				sc_buf[i*8+0],sc_buf[i*8+1],sc_buf[i*8+2],sc_buf[i*8+3],sc_buf[i*8+4],sc_buf[i*8+5],sc_buf[i*8+6],sc_buf[i*8+7]));
		}
		
	}


	if(c[0] == SCSIOP_READ_CAPACITY)
	{
		int i,max = 1;
		for(i = 0; i< max; i++)
		{
			KDPrintM(DBG_LURN_INFO, ("CAPACITY: (%02x):(%02x):(%02x):(%02x): (%02x):(%02x):(%02x):(%02x)\n",
				sc_buf[i*8+0],sc_buf[i*8+1],sc_buf[i*8+2],sc_buf[i*8+3],sc_buf[i*8+4],sc_buf[i*8+5],sc_buf[i*8+6],sc_buf[i*8+7]));
		}
		/*
		if(Ccb->CcbStatus != CCB_STATUS_SUCCESS)
		{
			if(IdeDisk->DVD_MEDIA_TYPE == 0x12)
			{
				
					sc_buf[0] = 0x0;
					sc_buf[1] = 0x22;
					sc_buf[2] = 0x21;
					sc_buf[3] = 0x1f;					
				
				sc_buf[4] = 0x0;
				sc_buf[5] = 0x0;
				sc_buf[6] = 0x08;
				sc_buf[7] = 0x0;
			}
			
			LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
			LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			
		}
		*/

		
	}
	//	Additional processing  
	//	LOGITEC  (HITACHI-GSA-4082 MODEL):  READ_TOC : 0x43
	//	Because LOGITEC Make Problem for playing MUCIS CD burned with easy CD creator	


	if(IdeDisk->DVD_TYPE == LOGITEC)
	{

		if((c[0] == SCSIOP_READ_TOC)
			&& (c[2] == 0x05))
		{
			PBYTE	ccbDataBuf;
			ccbDataBuf = Ccb->DataBuffer;
			ccbDataBuf[0] = 0x0;
			ccbDataBuf[1] = 0x2;
		}

	}

	if(buf && buf != sc_buf)
		ExFreePool(buf);

}

#endif


static 
void
ODD_BusyWait(LONG time)
{


		LARGE_INTEGER TimeInterval = {0,0};

		TimeInterval.QuadPart = -(time * NANO100_PER_SEC);

		KDPrintM(DBG_LURN_NOISE, 
				("Entered\n"));

		KeDelayExecutionThread(
			KernelMode,
			FALSE,
			&(TimeInterval)
			);


}


//
//	Using Request Sense command and get Status information
//

NTSTATUS
ODD_GetStatus (
	PLURNEXT_IDE_DEVICE		IdeDisk,
	PCCB					Ccb,
	PSENSE_DATA				pSenseData,
	LONG					SenseDataLen,
	PLARGE_INTEGER			Timeout,
	PBYTE					Response
	)
{

	NTSTATUS			ntStatus;
	BYTE				response;
	CCB					tempCCB;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	UCHAR				Reg[2];

	UNREFERENCED_PARAMETER( Ccb );

	NDAS_BUGON( SenseDataLen >= sizeof(SENSE_DATA) );

	if (SenseDataLen > 255) {

		KDPrintM( DBG_LURN_ERROR, ("Too big SenseDataLen. %d\n", SenseDataLen) );
		SenseDataLen = 255;
	}

	RtlZeroMemory( &tempCCB.Cdb,12 );
	RtlZeroMemory( (char *)pSenseData, sizeof(SENSE_DATA) );

	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.LogicalUnitNumber = IdeDisk->LuHwData.LanscsiLU;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.AllocationLength = (UCHAR)SenseDataLen;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Control = 0;

	RtlCopyMemory( tempCCB.PKCMD, tempCCB.Cdb, 6 );

	tempCCB.SecondaryBuffer = pSenseData;
	tempCCB.SecondaryBufferLength = SenseDataLen;

	IdeDisk->ODD_STATUS = NON_STATE;

	LSS = IdeDisk->LanScsiSession;

	LURNIDE_ATAPI_PDUDESC( IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB, Timeout );

	ntStatus = LspPacketRequest( LSS,
								 &PduDesc,
								 &response,
								 Reg );

	if (ntStatus != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {
	
		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
				     IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS) );
	}

	if (ntStatus != STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Command Fail 0x%x\n", ntStatus) ); 
		return ntStatus;								
	}

	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	if (Response) {

		*Response = response;
	}

	switch(pSenseData->SenseKey){
		case SCSI_SENSE_NO_SENSE:	// 0x00
			{
				IdeDisk->ODD_STATUS = READY;
			}
			break;
		case SCSI_SENSE_NOT_READY:	// 0x02
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case SCSI_ADSENSE_NO_MEDIA_IN_DEVICE:	// 0x3A
					{
						IdeDisk->ODD_STATUS = MEDIA_NOT_PRESENT;
					}
					break;
				case SCSI_ADSENSE_INVALID_MEDIA:		// 0x30
					{
						IdeDisk->ODD_STATUS = BAD_MEDIA;
					}
					break;
				case 0x06:
					{
						if(pSenseData->AdditionalSenseCodeQualifier  == 0x0)
							IdeDisk->ODD_STATUS = BAD_MEDIA;
					}
					break;
				case SCSI_ADSENSE_LUN_NOT_READY:		// 0x04
					{
						switch(pSenseData->AdditionalSenseCodeQualifier)
						{
						case SCSI_SENSEQ_CAUSE_NOT_REPORTABLE:	// 0x00
							{
								IdeDisk->ODD_STATUS = NOT_READY;
							}
							break;
						case SCSI_SENSEQ_FORMAT_IN_PROGRESS:	// 0x04
						case SCSI_SENSEQ_OPERATION_IN_PROGRESS:	// 0x07
						case SCSI_SENSEQ_LONG_WRITE_IN_PROGRESS:// 0x08
							{
								IdeDisk->ODD_STATUS = NOT_READY_PRESENT;
							}break;
						default:
							break;
						}
					}
					break;
				}
			}
			break;
		case SCSI_SENSE_MEDIUM_ERROR: // 0x03
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case 0x51:
					{
						switch(pSenseData->AdditionalSenseCodeQualifier)
						{
						case 0x00:
							IdeDisk->DVD_STATUS = MEDIA_ERASE_ERROR;
							break;
						default:
							break;
						}
					}
					break;
				default:
					break;
				}
				
			}
			break;
		case SCSI_SENSE_ILLEGAL_REQUEST:			// 0x05
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case SCSI_ADSENSE_ILLEGAL_COMMAND:	// 0x20
					IdeDisk->DVD_STATUS = INVALID_COMMAND;
					break;
				default:
					break;
				}
			}
			break;
		case SCSI_SENSE_UNIT_ATTENTION:				// 0x06
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case SCSI_ADSENSE_MEDIUM_CHANGED:	// 0x28
					{
						IdeDisk->ODD_STATUS = NOT_READY_MEDIA;
					}
					break;
				case SCSI_ADSENSE_BUS_RESET:		// 0x29
					{
						if(pSenseData->AdditionalSenseCodeQualifier == 0x00)
						{
							IdeDisk->ODD_STATUS = RESET_POWERUP;
						}

					}
					break;
				}
			}
			break;
		default:
			break;
		}

	return STATUS_SUCCESS;

}


NTSTATUS 
ODD_Inquiry(
	PLURNEXT_IDE_DEVICE		IdeDisk
){
	NTSTATUS			ntStatus;
	BYTE				response;
	CCB					tempCCB;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	LARGE_INTEGER		longTimeOut;
	UCHAR				Reg[2];


	RtlZeroMemory(tempCCB.Cdb, sizeof(tempCCB.Cdb));
	RtlZeroMemory((PBYTE)&IdeDisk->LuHwData.InquiryData,INQUIRYDATABUFFERSIZE);

	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.LogicalUnitNumber = IdeDisk->LuHwData.LanscsiLU;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Control = 0;

	RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 6);
	tempCCB.SecondaryBuffer = (PBYTE)&IdeDisk->LuHwData.InquiryData;
	tempCCB.SecondaryBufferLength = 36;

	LSS = IdeDisk->LanScsiSession;

	longTimeOut.QuadPart = - LURN_IDE_ODD_TIMEOUT;

	LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB, &longTimeOut);
	ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
				IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}



#if 0

static void
ODD_GetSectorCount(
		PLURNEXT_IDE_DEVICE		IdeDisk
			)
{
	NTSTATUS			ntStatus;
	BYTE				response;
	CCB					tempCCB;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	LARGE_INTEGER		longTimeOut;

	PMY_GET_CONFIGURATION   pConfiguration;
	MY_GET_RAWSECTORCOUT RowSectorCount;
	ULONG			 BlkSize = 0;
	UCHAR			 Reg[2];

	RtlZeroMemory(&tempCCB, sizeof(CCB));
	RtlZeroMemory((char *)&RowSectorCount,sizeof(MY_GET_RAWSECTORCOUT));

	pConfiguration = (PMY_GET_CONFIGURATION)&tempCCB.Cdb;
	pConfiguration->OperationCode = 0x46;
	pConfiguration->StartingFeature[0] = 0x0;
	pConfiguration->StartingFeature[1] = 0x10;
	pConfiguration->AllocationLength[0] = 0;
	pConfiguration->AllocationLength[1] = sizeof(MY_GET_RAWSECTORCOUT);
	pConfiguration->Control = 0;


	RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 10);
	tempCCB.SecondaryBuffer = &RowSectorCount;
	tempCCB.SecondaryBufferLength = sizeof(MY_GET_RAWSECTORCOUT);

	LSS = IdeDisk->LanScsiSession;


	longTimeOut.QuadPart = -30 * NANO100_PER_SEC;
	LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB, &longTimeOut);
	ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
					IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
		return;
	}



	if(
		(RowSectorCount.Data.Header.FeatureCode[0] == 0x0)
		&&(RowSectorCount.Data.Header.FeatureCode[1] == 0x10)
		)
	{
		BlkSize |= 	(RowSectorCount.Data.LogicalBlockSize[0] << 24);	
		BlkSize |=  (RowSectorCount.Data.LogicalBlockSize[1] << 16);
		BlkSize |=  (RowSectorCount.Data.LogicalBlockSize[2] << 8);
		BlkSize |=  RowSectorCount.Data.LogicalBlockSize[3];

		IdeDisk->BytesPerBlock = BlkSize;
		KDPrintM(DBG_LURN_ERROR, ("SECTOR SIZE %d\n",BlkSize));
	}



	return;

}

#endif

NTSTATUS
OddGetBlockSize (
	PLURNEXT_IDE_DEVICE		IdeDisk
	)
{
	NTSTATUS				status;

	BYTE					response;

	CCB						ccb;
	PCDB					cdb;
	READ_CAPACITY_DATA		readCapacityData;

	LANSCSI_PDUDESC			pduDesc;

	LARGE_INTEGER			timeOut;
	UCHAR					oddRegister[2];

	NDAS_BUGON( FALSE );

	RtlZeroMemory( &ccb, sizeof(CCB) );

	RtlZeroMemory( &readCapacityData, sizeof(READ_CAPACITY_DATA) );

	cdb = (PCDB)&ccb.Cdb;

	cdb->GET_CONFIGURATION.OperationCode = SCSIOP_READ_CAPACITY;
	
	RtlCopyMemory( ccb.PKCMD, ccb.Cdb, 10 );

	ccb.SecondaryBuffer = &readCapacityData;
	ccb.SecondaryBufferLength = sizeof(READ_CAPACITY_DATA);

	timeOut.QuadPart = -LURN_IDE_ODD_TIMEOUT;

	LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, &ccb, &timeOut );
	
	status = LspPacketRequest( IdeDisk->LanScsiSession,
							   &pduDesc,
							   &response,
							   oddRegister );

	IdeDisk->RegError = oddRegister[0];
	IdeDisk->RegStatus = oddRegister[1];

	if (status != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
				   ("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
				    IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
	}

	if (status != STATUS_SUCCESS) {
		
		NDAS_BUGON( STATUS_PORT_DISCONNECTED );
		return status;
	}

	if (response != LANSCSI_RESPONSE_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Error response. %x\n", response));
		return STATUS_UNSUCCESSFUL;
	}

	if (IdeDisk->BytesPerBlock != NTOHL(readCapacityData.BytesPerBlock) ||
		IdeDisk->LogicalBlockAddress != NTOHL(readCapacityData.LogicalBlockAddress)) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("SCSIOP_READ_CAPACITY: %u : %u\n", 
					NTOHL(readCapacityData.LogicalBlockAddress), NTOHL(readCapacityData.BytesPerBlock)) );
	}

	IdeDisk->BytesPerBlock = NTOHL(readCapacityData.BytesPerBlock);
	IdeDisk->LogicalBlockAddress = NTOHL(readCapacityData.LogicalBlockAddress);
	IdeDisk->LuHwData.BlockBytes = IdeDisk->BytesPerBlock;
	IdeDisk->LuHwData.SectorCount = IdeDisk->LogicalBlockAddress + 1;

	return status;
}

#define SIZE_OF_GETCONF_RANDOMREADABLE (FIELD_OFFSET(GET_CONFIGURATION_HEADER, Data) + sizeof(FEATURE_DATA_RANDOM_READABLE))
#define SIZE_OF_GETCONF_RANDOMWRITABLE (FIELD_OFFSET(GET_CONFIGURATION_HEADER, Data) + sizeof(FEATURE_DATA_RANDOM_WRITABLE))


NTSTATUS
OddGetRandomReadableCapacity (
	PLURNEXT_IDE_DEVICE		IdeDisk
	)
{
	NTSTATUS				status;

	BYTE					response;

	CCB						ccb;
	PCDB					cdbGetConf;

	LANSCSI_PDUDESC			pduDesc;

	LARGE_INTEGER			timeOut;
	UCHAR					oddRegister[2];

	BYTE	dataBuffer[SIZE_OF_GETCONF_RANDOMREADABLE];
	PGET_CONFIGURATION_HEADER		rawSectorCount;
	PFEATURE_DATA_RANDOM_READABLE	rawSectorCountData;

	RtlZeroMemory( &ccb, sizeof(CCB) );

	rawSectorCount = (PGET_CONFIGURATION_HEADER)dataBuffer;
	rawSectorCountData = (PFEATURE_DATA_RANDOM_READABLE)(rawSectorCount + 1);
	RtlZeroMemory( rawSectorCount, SIZE_OF_GETCONF_RANDOMREADABLE );

	cdbGetConf = (PCDB)&ccb.Cdb;

	cdbGetConf->GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
	
	cdbGetConf->GET_CONFIGURATION.StartingFeature[0] = 0x00;
	cdbGetConf->GET_CONFIGURATION.StartingFeature[1] = FeatureRandomReadable;

	cdbGetConf->GET_CONFIGURATION.AllocationLength[0] = 0;
	cdbGetConf->GET_CONFIGURATION.AllocationLength[1] = SIZE_OF_GETCONF_RANDOMREADABLE;

	cdbGetConf->GET_CONFIGURATION.Control = 0;

	RtlCopyMemory( ccb.PKCMD, ccb.Cdb, 10 );

	ccb.SecondaryBuffer = rawSectorCount;
	ccb.SecondaryBufferLength = SIZE_OF_GETCONF_RANDOMREADABLE;

	timeOut.QuadPart = -30 * NANO100_PER_SEC;

	LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, &ccb, &timeOut );
	
	status = LspPacketRequest( IdeDisk->LanScsiSession,
							   &pduDesc,
							   &response,
							   oddRegister );

	IdeDisk->RegError = oddRegister[0];
	IdeDisk->RegStatus = oddRegister[1];

	if (status != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
				   ("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
				    IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
	}

	if (status != STATUS_SUCCESS) {
		
		NDAS_BUGON( STATUS_PORT_DISCONNECTED );
		return status;
	}

	if (response != LANSCSI_RESPONSE_SUCCESS) {

		NDAS_BUGON( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	if (rawSectorCountData->Header.FeatureCode[0] == 0x0 && rawSectorCountData->Header.FeatureCode[1] == 0x10) {

		IdeDisk->BytesPerBlock = 0;

		IdeDisk->BytesPerBlock |= 	(rawSectorCountData->LogicalBlockSize[0] << 24);	
		IdeDisk->BytesPerBlock |=  (rawSectorCountData->LogicalBlockSize[1] << 16);
		IdeDisk->BytesPerBlock |=  (rawSectorCountData->LogicalBlockSize[2] << 8);
		IdeDisk->BytesPerBlock |=  rawSectorCountData->LogicalBlockSize[3];

		IdeDisk->LuHwData.BlockBytes = IdeDisk->BytesPerBlock;
		IdeDisk->RandomReadableBlockBytes = IdeDisk->BytesPerBlock;
		
		NDAS_BUGON( IdeDisk->BytesPerBlock == 2048 );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("SECTOR SIZE %d\n", IdeDisk->BytesPerBlock) );
	
	} else {

		NDAS_BUGON( FALSE );
		return STATUS_UNSUCCESSFUL;
	}

	return status;
}

static void
OddGetRandomWritableCapacity(
		PLURNEXT_IDE_DEVICE		IdeDisk
			  )
{
		NTSTATUS			ntStatus;
		BYTE				response;
		CCB					tempCCB;
		LANSCSI_PDUDESC		PduDesc;
		PLANSCSI_SESSION	LSS;
		LARGE_INTEGER		longTimeOut;

		PCDB			cdbGetConf;
		ULONG			BlkSize = 0;
		ULONG			LBA	= 0;
		UCHAR			Reg[2];
		BYTE			dataBuffer[SIZE_OF_GETCONF_RANDOMWRITABLE];
		PGET_CONFIGURATION_HEADER		getConfHeader;
		PFEATURE_DATA_RANDOM_WRITABLE	featureDataRandomWritable;

		getConfHeader = (PGET_CONFIGURATION_HEADER)dataBuffer;
		featureDataRandomWritable = (PFEATURE_DATA_RANDOM_WRITABLE)(getConfHeader + 1);
		RtlZeroMemory(&tempCCB, sizeof(CCB));
		RtlZeroMemory(getConfHeader, SIZE_OF_GETCONF_RANDOMWRITABLE);
		
		cdbGetConf = (PCDB)&tempCCB.Cdb;
		cdbGetConf->GET_CONFIGURATION.OperationCode = SCSIOP_GET_CONFIGURATION;
		cdbGetConf->GET_CONFIGURATION.StartingFeature[0] = 0x0;
		cdbGetConf->GET_CONFIGURATION.StartingFeature[1] = FeatureRandomWritable;
		cdbGetConf->GET_CONFIGURATION.AllocationLength[0] = 0;
		cdbGetConf->GET_CONFIGURATION.AllocationLength[1] = sizeof(SIZE_OF_GETCONF_RANDOMWRITABLE);
		cdbGetConf->GET_CONFIGURATION.Control = 0;

		
		RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 10);
		tempCCB.SecondaryBuffer = getConfHeader;
		tempCCB.SecondaryBufferLength = SIZE_OF_GETCONF_RANDOMWRITABLE;
		
		LSS = IdeDisk->LanScsiSession;

		longTimeOut.QuadPart = -30 * NANO100_PER_SEC;
		LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB, &longTimeOut);
		ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

		IdeDisk->RegError = Reg[0];
		IdeDisk->RegStatus = Reg[1];

		if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR, 
				("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
						IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
			return;
		}
	
		
		KDPrintM( DBG_LURN_INFO, 
				("%x, %x\n", featureDataRandomWritable->Header.FeatureCode[0], featureDataRandomWritable->Header.FeatureCode[1]) );

		if(
			(featureDataRandomWritable->Header.FeatureCode[0] == 0x0)
			&&(featureDataRandomWritable->Header.FeatureCode[1] == 0x20)
			)
		{
			LBA |= 	(featureDataRandomWritable->LastLBA[0] << 24);	
			LBA |=  (featureDataRandomWritable->LastLBA[1] << 16);
			LBA |=  (featureDataRandomWritable->LastLBA[2] << 8);
			LBA |=  featureDataRandomWritable->LastLBA[3];

			BlkSize |= 	(featureDataRandomWritable->LogicalBlockSize[0] << 24);	
			BlkSize |=  (featureDataRandomWritable->LogicalBlockSize[1] << 16);
			BlkSize |=  (featureDataRandomWritable->LogicalBlockSize[2] << 8);
			BlkSize |=  featureDataRandomWritable->LogicalBlockSize[3];
#if DBG
			if (IdeDisk->BytesPerBlock != BlkSize || IdeDisk->LogicalBlockAddress != LBA) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("GET_CONFIGURATION: %u : %u IdeDisk->LogicalBlockAddress %u IdeDisk->BytesPerBlock %u \n", 
							 LBA, BlkSize, IdeDisk->LogicalBlockAddress, IdeDisk->BytesPerBlock) );
			}
#endif
			//NDAS_BUGON( BlkSize == 2048 ); 

			IdeDisk->BytesPerBlock = BlkSize;
			IdeDisk->RandomWritableBlockBytes = IdeDisk->BytesPerBlock;

			IdeDisk->LuHwData.BlockBytes = BlkSize;
			IdeDisk->LuHwData.SectorCount = LBA + 1;
		}

		return;
}

#if 0

static NTSTATUS
ProcessIdePacketRequest(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeDisk,
		PCCB					Ccb,
		PULONG				NeedRepeat
		);


/*
static NTSTATUS
ODD_TestUnit(		
			PLURELATION_NODE		Lurn,
			PLURNEXT_IDE_DEVICE		IdeDisk
			)
{
	NTSTATUS			ntStatus;
	CCB					tempCCB;
	SENSE_DATA			SenseData;
	ULONG				Repeat = 0;

	RtlZeroMemory(&tempCCB.Cdb,sizeof(CCB));
	RtlZeroMemory((char *)&SenseData,sizeof(SENSE_DATA));
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_TEST_UNIT_READY;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.LogicalUnitNumber = IdeDisk->LuData.LanscsiLU;;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.AllocationLength = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Control = 0;

	RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 6);
	tempCCB.PKDataBuffer = 0;
	tempCCB.PKDataBufferLength = 0;
	tempCCB.SenseBuffer = &SenseData;
	tempCCB.SenseDataLength = sizeof(SENSE_DATA);

	ntStatus = ProcessIdePacketRequest(Lurn,IdeDisk,&tempCCB,&Repeat);

	return ntStatus;

}
*/

#endif

NTSTATUS
ProcessErrorCode (
	IN  PLURELATION_NODE	Lurn,
	IN  PLURNEXT_IDE_DEVICE	IdeDisk,
	IN  PCCB				Ccb,
	IN  PLARGE_INTEGER		Timeout,
	OUT PBYTE				Response
	)
{
	NTSTATUS	status;
	PSENSE_DATA senseData;
	BYTE		regStatus;
	BYTE		regError;
	BYTE		response;

	UNREFERENCED_PARAMETER( Lurn );

	regStatus = IdeDisk->RegStatus;
	regError = IdeDisk->RegError;

	status = ODD_GetStatus( IdeDisk,
							Ccb,
							(PSENSE_DATA)Ccb->SenseBuffer,
							Ccb->SenseDataLength,
							Timeout,
							&response );

	if (Response) {

		*Response = response;
	}

	if (status != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {

		NDAS_BUGON( status == STATUS_PORT_DISCONNECTED || status == STATUS_CANCELLED );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("CMD(0x%02x), MEDIA_TYPE(0x%02x), regStatus (0x%02x), regError(0x%02x)\n",
					  Ccb->PKCMD[0], IdeDisk->DVD_MEDIA_TYPE, regStatus, regError) );

		return status;
	}

	senseData = (PSENSE_DATA)(Ccb->SenseBuffer);

	NDAS_BUGON( IdeDisk->RegStatus == 0x00 && IdeDisk->RegError == 0x00 );

	if (1																								&&
		!(Ccb->Cdb[0] == SCSIOP_TEST_UNIT_READY && senseData->SenseKey == 0x02 && 
		  senseData->AdditionalSenseCode == 0x3A && senseData->AdditionalSenseCodeQualifier == 0x00)	&&
		  !(Ccb->Cdb[0] == SCSIOP_TEST_UNIT_READY && senseData->SenseKey == 0x02 && 
		  senseData->AdditionalSenseCode == 0x3A && senseData->AdditionalSenseCodeQualifier == 0x02)	&&
		1) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("CMD(0x%02x), MEDIA_TYPE(0x%02x), STATUS(0x%02x), "
					 "regStatus (0x%02x), regError(0x%02x), ErrorCode(0x%02x), "
					 "SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
					  Ccb->PKCMD[0], IdeDisk->DVD_MEDIA_TYPE, IdeDisk->ODD_STATUS, regStatus,
					  regError, senseData->ErrorCode, senseData->SenseKey,
					  senseData->AdditionalSenseCode, senseData->AdditionalSenseCodeQualifier) );
	}

//	NDAS_BUGON( !FlagOn(IdeDisk->RegStatus, BUSY_STAT) );

	do {

		if (senseData->SenseKey == SCSI_SENSE_NO_SENSE				&& // 0x00
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_SENSE	&& // 0x00
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Logical unit is in process of becoming ready

			if (Ccb->PKCMD[0] == SCSIOP_READ			||
				Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY	||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY	||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC		||
				Ccb->PKCMD[0] == SCSIOP_GET_CONFIGURATION) {

#if 0
				//
				// Busy bit is not valid for packet command ( 0xa0 ) ( 0xa0 )
				// due to NDAS chip 1.1, 2.0, 2.0g bug.
				//
				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );
#endif

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NO_SENSE		&& // 0x00
			senseData->AdditionalSenseCode == 0xD0			&&
			senseData->AdditionalSenseCodeQualifier == 0x0F) {

			// Vendor Specific

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_RECOVERED_ERROR	&& // 0x01
			senseData->AdditionalSenseCode == 0xD0				&&
			senseData->AdditionalSenseCodeQualifier == 0x3F) {

			// Vendor Specific

			if (Ccb->PKCMD[0] == SCSIOP_READ_TOC) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY		&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY && // 0x04
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_CAUSE_NOT_REPORTABLE) { // 0x00

				// Logical unit is not ready, cause not reportable

			if (Ccb->PKCMD[0] == SCSIOP_READ_CD) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY		&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY && // 0x04
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_BECOMING_READY) { // 0x01

			// Logical unit is in process of becoming ready

			if (Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY			||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY			||
				Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION	||
				Ccb->PKCMD[0] == SCSIOP_MEDIUM_REMOVAL			||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE	||
				Ccb->PKCMD[0] == SCSIOP_GET_PERFORMANCE		||
				Ccb->PKCMD[0] == SCSIOP_SET_CD_SPEED) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY	&& // 0x04
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_OPERATION_IN_PROGRESS) { // 0x07

			// Logical unit is not ready, operation in process

			if (Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY ||
				Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_LUN_NOT_READY			&& // 0x04
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_LONG_WRITE_IN_PROGRESS) { // 0x08

			// Logical unit is not ready, long write in process

			if (Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY ||
				Ccb->PKCMD[0] == SCSIOP_READ_TRACK_INFORMATION) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_SET_CD_SPEED) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				// The CCB will execute again.
				// Don't set ResidualDataLength in full.
				// It will make transfer length zero when the CCB re-enter.

				Ccb->ResidualDataLength = 0;
				LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				// The CCB will execute again.
				// Don't set ResidualDataLength in full.
				// It will make transfer length zero when the CCB re-enter.

				Ccb->ResidualDataLength = 0;
				LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_INVALID_MEDIA			&& // 0x30
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_INCOMPATIBLE_MEDIA_INSTALLED) { // 0x00

			// Incompatible medium installed

			if (Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY	||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_SEND_KEY	||
				Ccb->PKCMD[0] == SCSIOP_REPORT_KEY) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_MEDIA_IN_DEVICE			&& // 0x3A
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Medium not present

			if (Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY			||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY			||
				Ccb->PKCMD[0] == SCSIOP_MODE_SELECT10			||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC				||
				Ccb->PKCMD[0] == SCSIOP_SEEK					||
				Ccb->PKCMD[0] == SCSIOP_READ_SUB_CHANNEL		||
				Ccb->PKCMD[0] == SCSIOP_READ					||
				Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION	||
				Ccb->PKCMD[0] == SCSIOP_LOAD_UNLOAD				||
				Ccb->PKCMD[0] == SCSIOP_GET_PERFORMANCE) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE	||
				Ccb->PKCMD[0] == SCSIOP_READ_CD				||
				Ccb->PKCMD[0] == 0xED						||
				Ccb->PKCMD[0] == SCSIOP_DENON_READ_TOC) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_MEDIA_IN_DEVICE			&&	// 0x3A
			senseData->AdditionalSenseCodeQualifier == 0x01) {

				// Medium not present - Tray Closed

			if (Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY	||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC		||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY ||
				Ccb->PKCMD[0] == SCSIOP_GET_PERFORMANCE ||
				Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE ||
				Ccb->PKCMD[0] == SCSIOP_GET_EVENT_STATUS ||
				Ccb->PKCMD[0] == SCSIOP_READ_DISK_INFORMATION) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );
				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_NOT_READY						&& // 0x02
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_MEDIA_IN_DEVICE			&& //0x3a
			senseData->AdditionalSenseCodeQualifier == 0x02) {

				// Medium not present - Tray Open

			if (Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY			||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY			||
				Ccb->PKCMD[0] == SCSIOP_READ					||
				Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION	||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE	||
				Ccb->PKCMD[0] == SCSIOP_REPORT_KEY			||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_SEEK_COMPLETE			&& // 0x02
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// No seek complete

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == 0x10			&&
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// ID CRC OR ECC Error

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}
			
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == 0x11			&&
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// unrecovered read error

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_CD) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == 0x11			&&
			senseData->AdditionalSenseCodeQualifier == 0x08) {

			// incomplete block read

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == SCSI_ADSENSE_WRITE_ERROR			&& // 0x0c
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// write error

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_WRITE12) {

				// The CCB will execute again.
				// Don't set ResidualDataLength in full.
				// It will make transfer length zero when the CCB re-enter.

				Ccb->ResidualDataLength = 0;
				LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_MEDIUM_ERROR						&& // 0x03
			senseData->AdditionalSenseCode == 0x51			&&
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Erase Failure

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_HARDWARE_ERROR						&& // 0x04
			senseData->AdditionalSenseCode == 0x3E			&&
			senseData->AdditionalSenseCodeQualifier == 0x02) {

			// Timeout on logical unit

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == 0xFC) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_ILLEGAL_COMMAND			&& // 0x20
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Invalid command operation code

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == 0xED						||
				Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE	||
				Ccb->PKCMD[0] == SCSIOP_DENON_READ_TOC		||
				Ccb->PKCMD[0] == 0xF9						||
				Ccb->PKCMD[0] == 0xFF						||
				Ccb->PKCMD[0] == SCSIOP_REPORT_KEY			||
				Ccb->PKCMD[0] == SCSIOP_GET_CONFIGURATION) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_ILLEGAL_BLOCK			&& // 0x21
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// logical block address out of range

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_ILLEGAL_BLOCK			&& // 0x21
			senseData->AdditionalSenseCodeQualifier == 0x02) {

			// Invalid Address For Write

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_INVALID_CDB			&& // 0x24
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Invalid Field in CDB

			if (Ccb->PKCMD[0] == SCSIOP_INQUIRY					||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC				||
				Ccb->PKCMD[0] == SCSIOP_READ_TRACK_INFORMATION	||
				Ccb->PKCMD[0] == SCSIOP_MODE_SELECT10			||
				Ccb->PKCMD[0] == SCSIOP_MODE_SENSE10			||
				Ccb->PKCMD[0] == SCSIOP_GET_EVENT_STATUS) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_CD				||
				Ccb->PKCMD[0] == SCSIOP_GET_PERFORMANCE		||
				Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE	||
				Ccb->PKCMD[0] == 0xDF) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST								&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST		&& // 0x26
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// invalid field in parameter list

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_MODE_SELECT10) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_INVALID_MEDIA			&& // 0x30
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_INCOMPATIBLE_MEDIA_INSTALLED) { // 0x00

			// wrong code cannot read medium incompatible format

			NDAS_BUGON( FALSE );

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == 0xF9) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode ==SCSI_ADSENSE_INVALID_MEDIA			&& // 0x30
			senseData->AdditionalSenseCodeQualifier == SCSI_SENSEQ_INCOMPATIBLE_FORMAT) { // 0x02

			// cannot read medium incompatible format

			if (Ccb->PKCMD[0] == SCSIOP_SEND_EVENT ||
				Ccb->PKCMD[0] == SCSIOP_READ_SUB_CHANNEL) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_DVD_STRUCTURE) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_INVALID_MEDIA		&& // 0x30
			senseData->AdditionalSenseCodeQualifier == 0x06) {

			// cannot format medium - incompatible medium

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_WRITE12) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == 0x53			&&
			senseData->AdditionalSenseCodeQualifier == 0x02) {

			// Medium removal prevented.

//			NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_START_STOP_UNIT ||
				Ccb->PKCMD[0] == SCSIOP_GET_EVENT_STATUS) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == 0x63			&&
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// end of user data encouted on this track

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_ILLEGAL_REQUEST						&& // 0x05
			senseData->AdditionalSenseCode == SCSI_ADSENSE_ILLEGAL_MODE_FOR_THIS_TRACK			&& // 0x64
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Illegal mode for the track

			if (Ccb->PKCMD[0] == SCSIOP_READ) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			if (Ccb->PKCMD[0] == 0xED ||
				Ccb->PKCMD[0] == SCSIOP_DENON_READ_TOC) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_UNIT_ATTENTION						&& // 0x06
			senseData->AdditionalSenseCode == SCSI_ADSENSE_MEDIUM_CHANGED			&& // 0x28
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// not ready to change, format layer may have changed

			if (Ccb->PKCMD[0] == SCSIOP_MODE_SENSE10	||
				Ccb->PKCMD[0] == SCSIOP_SEND_EVENT		||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY	||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC		||
				Ccb->PKCMD[0] == SCSIOP_READ			||
				Ccb->PKCMD[0] == SCSIOP_WRITE			||
				Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_CD ||
				Ccb->PKCMD[0] == SCSIOP_SET_CD_SPEED) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		if (senseData->SenseKey == SCSI_SENSE_UNIT_ATTENTION						&& // 0x06
			senseData->AdditionalSenseCode == SCSI_ADSENSE_BUS_RESET			&& // 0x29
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// power on, reset, or bus device reset occured

			if (Ccb->PKCMD[0] == SCSIOP_INQUIRY					||
				Ccb->PKCMD[0] == SCSIOP_READ_CAPACITY			||
				Ccb->PKCMD[0] == SCSIOP_TEST_UNIT_READY			||
				Ccb->PKCMD[0] == SCSIOP_MODE_SENSE10			||
				Ccb->PKCMD[0] == SCSIOP_READ_DISC_INFORMATION	||
				Ccb->PKCMD[0] == SCSIOP_WRITE					||
				Ccb->PKCMD[0] == SCSIOP_READ_TOC				||
				Ccb->PKCMD[0] == SCSIOP_READ					||
				Ccb->PKCMD[0] == SCSIOP_MODE_SELECT10) {

//				NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

				// The CCB will execute again.
				// Don't set ResidualDataLength in full.
				// It will make transfer length zero when the CCB re-enter.

				Ccb->ResidualDataLength = 0;
				LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

				break;
			}

			if (Ccb->PKCMD[0] == SCSIOP_READ_CD	||
				Ccb->PKCMD[0] == SCSIOP_GET_PERFORMANCE) {

//				NDAS_BUGON( !FlagOn(regStatus, BUSY_STAT) );

				// The CCB will execute again.
				// Don't set ResidualDataLength in full.
				// It will make transfer length zero when the CCB re-enter.

				Ccb->ResidualDataLength = 0;
				LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

				break;
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			// The CCB will execute again.
			// Don't set ResidualDataLength in full.
			// It will make transfer length zero when the CCB re-enter.

			Ccb->ResidualDataLength = 0;
			LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );

			break;					
		}

		if (senseData->SenseKey == SCSI_SENSE_BLANK_CHECK						&& // 0x08
			senseData->AdditionalSenseCode == SCSI_ADSENSE_NO_SENSE			&& // 0x00
			senseData->AdditionalSenseCodeQualifier == 0x00) {

			// Blank Check

//			NDAS_BUGON( FlagOn(regStatus, BUSY_STAT) );

			if (Ccb->PKCMD[0] == SCSIOP_READ ||
				Ccb->PKCMD[0] == SCSIOP_READ_SUB_CHANNEL) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				break;					
			}

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

		NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

		Ccb->ResidualDataLength = Ccb->DataBufferLength;
		LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
					
		break;
	
	} while (0);

	return status;
}


NTSTATUS
LurnIdeOddExecute (
	IN PLURELATION_NODE		Lurn,
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PCCB					Ccb
	)
{
	NTSTATUS			status;

	BYTE				response;
	LANSCSI_PDUDESC		pduDesc;

	LARGE_INTEGER		timeOut;
	UCHAR				oddRegister[2];

	SENSE_DATA			senseData = {0};
	ULONG				waitCount = 0;
	BOOLEAN				ndasBufferRequiredOperation = FALSE;

	DebugTrace( NDASSCSI_DBG_LURN_IDE_NOISE, ("Enter") );

#if DBG
	if(	Ccb->Cdb[0] != SCSIOP_READ &&
		Ccb->Cdb[0] != SCSIOP_READ12 &&
		Ccb->Cdb[0] != SCSIOP_WRITE &&
		Ccb->Cdb[0] != SCSIOP_WRITE12
		) {
		NDAS_BUGON( Ccb->DataBufferLength <= IdeDisk->MaxDataSendLength );
		NDAS_BUGON( Ccb->DataBufferLength <= IdeDisk->MaxDataRecvLength );
	}
#endif
	if (1												&&
		(Ccb->Cdb[0] != SCSIOP_TEST_UNIT_READY)			&&
		(Ccb->Cdb[0] != SCSIOP_GET_EVENT_STATUS)		&&
		(Ccb->Cdb[0] != SCSIOP_GET_CONFIGURATION)		&&
		(Ccb->Cdb[0] != SCSIOP_READ_DISC_INFORMATION)	&&
		(Ccb->Cdb[0] != SCSIOP_READ_TRACK_INFORMATION)	&&
		(Ccb->Cdb[0] != SCSIOP_READ_TOC)				&&
		(Ccb->Cdb[0] != SCSIOP_READ_CD)					&&
		(Ccb->Cdb[0] != SCSIOP_INQUIRY)					&&
		(Ccb->Cdb[0] != SCSIOP_READ_CAPACITY)			&&
		(Ccb->Cdb[0] != SCSIOP_MODE_SELECT10)			&&
		(Ccb->Cdb[0] != SCSIOP_SET_CD_SPEED)			&&
		(Ccb->Cdb[0] != SCSIOP_READ_BUFFER_CAPACITY)	&&
		(Ccb->Cdb[0] != SCSIOP_MODE_SENSE10)			&&
		(Ccb->Cdb[0] != SCSIOP_WRITE)					&&
		(Ccb->Cdb[0] != SCSIOP_READ6)					&&
		(Ccb->Cdb[0] != SCSIOP_READ)					&&
		1) {
		
		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
					 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
					 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
					 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );
	}


	switch (Ccb->Cdb[0]) {

	case SCSIOP_TEST_UNIT_READY: {	//0x00

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_REZERO_UNIT: {	//0x01

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_REQUEST_SENSE: {	// 0x03

		UINT8 transferLen = Ccb->Cdb[4];

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (transferLen != Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			if (transferLen > Ccb->SecondaryBufferLength) {

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				//Ccb->SecondaryBufferLength = transferLen;
			}
		}

		break;
	}

	case SCSIOP_READ6: {	//0x08 SCSIOP_RECEIVE

		UINT32	logicalBlock;	
		UINT32	transferBlockLength;
		UINT32	transferByteLength = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PacketCdb.READ12.OperationCode = SCSIOP_READ12;

		Ccb->PacketCdb.READ12.LogicalUnitNumber = Ccb->SrbCdb.CDB6READWRITE.LogicalUnitNumber;

		logicalBlock = Ccb->SrbCdb.CDB6READWRITE.LogicalBlockMsb1 << 16;
		logicalBlock |= Ccb->SrbCdb.CDB6READWRITE.LogicalBlockMsb0 << 8;
		logicalBlock |= Ccb->SrbCdb.CDB6READWRITE.LogicalBlockLsb;

		Ccb->PacketCdb.READ12.LogicalBlock[0] = (logicalBlock >> 24) & 0xFF;
		Ccb->PacketCdb.READ12.LogicalBlock[1] = (logicalBlock >> 16) & 0xFF;
		Ccb->PacketCdb.READ12.LogicalBlock[2] = (logicalBlock >> 8) & 0xFF;
		Ccb->PacketCdb.READ12.LogicalBlock[3] = (logicalBlock) & 0xFF;

		transferBlockLength = Ccb->SrbCdb.CDB6READWRITE.TransferBlocks;

		Ccb->PacketCdb.READ12.TransferLength[0] = (transferBlockLength >> 24) & 0xFF;
		Ccb->PacketCdb.READ12.TransferLength[1] = (transferBlockLength >> 16) & 0xFF;
		Ccb->PacketCdb.READ12.TransferLength[2] = (transferBlockLength >> 8) & 0xFF;
		Ccb->PacketCdb.READ12.TransferLength[3] = (transferBlockLength) & 0xFF;

		Ccb->PacketCdb.READ12.Control = Ccb->SrbCdb.CDB6READWRITE.Control;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
					("Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
					 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
					 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
					 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( logicalBlockAddress & 0x80000000 ||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_WRITE6: {	//0x08 SCSIOP_PRINT SCSIOP_SEND

		UINT32	logicalBlock;	
		UINT32	transferBlockLength;
		UINT32	transferByteLength = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PacketCdb.WRITE12.OperationCode = SCSIOP_WRITE12;

		Ccb->PacketCdb.WRITE12.LogicalUnitNumber = Ccb->SrbCdb.CDB6READWRITE.LogicalUnitNumber;

		logicalBlock = Ccb->SrbCdb.CDB6READWRITE.LogicalBlockMsb1 << 16;
		logicalBlock |= Ccb->SrbCdb.CDB6READWRITE.LogicalBlockMsb0 << 8;
		logicalBlock |= Ccb->SrbCdb.CDB6READWRITE.LogicalBlockLsb;

		Ccb->PacketCdb.WRITE12.LogicalBlock[0] = (logicalBlock >> 24) & 0xFF;
		Ccb->PacketCdb.WRITE12.LogicalBlock[1] = (logicalBlock >> 16) & 0xFF;
		Ccb->PacketCdb.WRITE12.LogicalBlock[2] = (logicalBlock >> 8) & 0xFF;
		Ccb->PacketCdb.WRITE12.LogicalBlock[3] = (logicalBlock) & 0xFF;

		transferBlockLength = Ccb->SrbCdb.CDB6READWRITE.TransferBlocks;

		Ccb->PacketCdb.WRITE12.TransferLength[0] = (transferBlockLength >> 24) & 0xFF;
		Ccb->PacketCdb.WRITE12.TransferLength[1] = (transferBlockLength >> 16) & 0xFF;
		Ccb->PacketCdb.WRITE12.TransferLength[2] = (transferBlockLength >> 8) & 0xFF;
		Ccb->PacketCdb.WRITE12.TransferLength[3] = (transferBlockLength) & 0xFF;

		Ccb->PacketCdb.WRITE12.Control = Ccb->SrbCdb.CDB6READWRITE.Control;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
					("Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
					 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
					 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
					 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( logicalBlockAddress & 0x80000000 ||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_SEEK6: {	//0x0B SCSIOP_TRACK_SELECT SCSIOP_SLEW_PRINT SCSIOP_SET_CAPACITY 

		ULONG	logicalBlockAddress;	

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PacketCdb.SEEK.OperationCode = SCSIOP_SEEK;

		Ccb->PacketCdb.SEEK.LogicalUnitNumber = Ccb->SrbCdb.CDB6GENERIC.LogicalUnitNumber;

		logicalBlockAddress = Ccb->SrbCdb.CDB6GENERIC.CommandUniqueBytes[0] << 16;
		logicalBlockAddress |= Ccb->SrbCdb.CDB6GENERIC.CommandUniqueBytes[1] << 8;
		logicalBlockAddress |= Ccb->SrbCdb.CDB6GENERIC.CommandUniqueBytes[2];

		Ccb->PacketCdb.SEEK.LogicalBlockAddress[0] = (logicalBlockAddress >> 24) & 0xFF;
		Ccb->PacketCdb.SEEK.LogicalBlockAddress[1] = (logicalBlockAddress >> 16) & 0xFF;
		Ccb->PacketCdb.SEEK.LogicalBlockAddress[2] = (logicalBlockAddress >> 8) & 0xFF;
		Ccb->PacketCdb.SEEK.LogicalBlockAddress[3] = (logicalBlockAddress) & 0xFF;

		Ccb->PacketCdb.SEEK.Control = Ccb->SrbCdb.AsByte[5];

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
					 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
					 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
					 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_INQUIRY: {	//0x12

		UINT8 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.CDB6INQUIRY3.AllocationLength;

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

		break;
	}

	case SCSIOP_MODE_SELECT: {	//0x15

		UINT16 parameterListLength = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (Ccb->DataBufferLength < 4) {
			
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
		
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
			status = STATUS_SUCCESS;
	
			goto ErrorOut;
		}

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			if (((PUINT8)Ccb->DataBuffer)[4] == MODE_PAGE_WRITE_PARAMETERS) {

				if ((Ccb->SenseBuffer) && (Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))) {

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
				}

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				status = STATUS_SUCCESS;
				goto ErrorOut;
			}
		}

		Ccb->SecondaryBuffer	   = IdeDisk->Buffer;
		Ccb->SecondaryBufferLength = Ccb->DataBufferLength + 4;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );
			
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PacketCdb.MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;

		Ccb->PacketCdb.MODE_SELECT10.SPBit = Ccb->SrbCdb.MODE_SELECT.SPBit;
		Ccb->PacketCdb.MODE_SELECT10.PFBit = Ccb->SrbCdb.MODE_SELECT.PFBit;
		Ccb->PacketCdb.MODE_SELECT10.LogicalUnitNumber = Ccb->SrbCdb.MODE_SELECT.LogicalUnitNumber;
			
		parameterListLength = Ccb->SrbCdb.MODE_SELECT.ParameterListLength;
		NDAS_BUGON( parameterListLength == Ccb->DataBufferLength );

		parameterListLength += 4;
		NDAS_BUGON( parameterListLength == Ccb->SecondaryBufferLength );

		Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[0] = (UCHAR)((parameterListLength >> 8) & 0xFF);
		Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[1] = (UCHAR)(parameterListLength & 0xFF);

		Ccb->PacketCdb.MODE_SELECT10.Control = Ccb->SrbCdb.MODE_SELECT.Control;

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], parameterListLength, Ccb->SecondaryBufferLength) );
		
			if (parameterListLength > MAX_PIO_LENGTH_4096) {

				parameterListLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[0] = (UCHAR)((parameterListLength >> 8) & 0xFF);
				Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[1] = (UCHAR)(parameterListLength & 0xFF);
		
				Ccb->SecondaryBufferLength = parameterListLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		NDAS_BUGON( sizeof(IdeDisk->Buffer) >= Ccb->SecondaryBufferLength );

		((PUINT8)Ccb->SecondaryBuffer)[1] = ((PUINT8)Ccb->DataBuffer)[0];	// Mode data length
		((PUINT8)Ccb->SecondaryBuffer)[2] = ((PUINT8)Ccb->DataBuffer)[1];	// Medium type
		((PUINT8)Ccb->SecondaryBuffer)[3] = ((PUINT8)Ccb->DataBuffer)[2];	// DeviceSpecificParameter
		((PUINT8)Ccb->SecondaryBuffer)[7] = ((PUINT8)Ccb->DataBuffer)[3];	// BlockDescriptorLength

		RtlCopyMemory( ((PUINT8)Ccb->SecondaryBuffer) + 8, 
						((PUINT8)Ccb->DataBuffer) + 4, 
						(Ccb->DataBufferLength - 4) < (Ccb->SecondaryBufferLength - 8) ?
						(Ccb->DataBufferLength - 4) : (Ccb->SecondaryBufferLength - 8));
	
		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_MODE_SENSE: {	//0x1A

		UINT16 allocationLength = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (Ccb->DataBufferLength < 4) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
			
			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
		
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
			status = STATUS_SUCCESS;
	
			goto ErrorOut;
		}

		Ccb->SecondaryBuffer	   = IdeDisk->Buffer;
		Ccb->SecondaryBufferLength = Ccb->DataBufferLength + 4;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );
			
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PacketCdb.MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;

		Ccb->PacketCdb.MODE_SENSE10.Dbd = Ccb->SrbCdb.MODE_SENSE.Dbd;
		Ccb->PacketCdb.MODE_SENSE10.LogicalUnitNumber = Ccb->SrbCdb.MODE_SENSE.LogicalUnitNumber;
		Ccb->PacketCdb.MODE_SENSE10.PageCode = Ccb->SrbCdb.MODE_SENSE.PageCode;
		Ccb->PacketCdb.MODE_SENSE10.Pc = Ccb->SrbCdb.MODE_SENSE.Pc;
			
		allocationLength = Ccb->SrbCdb.MODE_SENSE.AllocationLength;
		NDAS_BUGON( allocationLength == Ccb->DataBufferLength );

		allocationLength += 4;
		NDAS_BUGON( allocationLength == Ccb->SecondaryBufferLength );

		Ccb->PacketCdb.MODE_SENSE10.AllocationLength[0] = (UCHAR)((allocationLength >> 8) & 0xFF);
		Ccb->PacketCdb.MODE_SENSE10.AllocationLength[1] = (UCHAR)(allocationLength & 0xFF);

		Ccb->PacketCdb.MODE_SENSE10.Control = Ccb->SrbCdb.MODE_SENSE.Control;

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.MODE_SENSE10.AllocationLength[0] = (UCHAR)((allocationLength >> 8) & 0xFF);
				Ccb->PacketCdb.MODE_SENSE10.AllocationLength[1] = (UCHAR)(allocationLength & 0xFF);

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		NDAS_BUGON( sizeof(IdeDisk->Buffer) >= Ccb->SecondaryBufferLength );

		//NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_START_STOP_UNIT: {	//0x1B SCSIOP_STOP_PRINT SCSIOP_LOAD_UNLOAD

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_MEDIUM_REMOVAL: {	//0x1E

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_READ_FORMATTED_CAPACITY: { //0x23

		UINT16 transferLen = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );

		transferLen = (Ccb->PKCMD[7] << 8);
		transferLen |= (Ccb->PKCMD[8]);

		if (transferLen != Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			if (transferLen > Ccb->SecondaryBufferLength) {

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				//Ccb->SecondaryBufferLength = transferLen;
			}
		}

		if (transferLen > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",Ccb->Cdb[0], transferLen) );
		
			transferLen = MAX_PIO_LENGTH_4096;

			Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
			Ccb->PKCMD[8] = (transferLen & 0xff);
			Ccb->SecondaryBufferLength = transferLen;
		}

		break;
	}

	case SCSIOP_READ_CAPACITY: {	//0x25

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );

		if (8 != Ccb->SecondaryBufferLength) {

			if (8 > Ccb->SecondaryBufferLength) {

				NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				RtlZeroMemory( Ccb->DataBuffer, Ccb->DataBufferLength );

				Ccb->SecondaryBufferLength = 8;
			}
		}

		if(IdeDisk->LuHwData.InquiryData.DeviceType != DIRECT_ACCESS_DEVICE) {
			OddGetRandomReadableCapacity( IdeDisk );
			OddGetRandomWritableCapacity( IdeDisk );
		}

		break;
	}

	case SCSIOP_READ: {	//0x28

		ULONG	logicalBlockAddress = 0;	
		ULONG	transferBlocks = 0;
		ULONG	transferLength = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		logicalBlockAddress |= (Ccb->PKCMD[2] << 24);
		logicalBlockAddress |= (Ccb->PKCMD[3] << 16);
		logicalBlockAddress |= (Ccb->PKCMD[4] << 8);
		logicalBlockAddress |= Ccb->PKCMD[5];

		transferBlocks = (Ccb->PKCMD[7] << 8);
		transferBlocks |= Ccb->PKCMD[8];

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( logicalBlockAddress & 0x80000000 ||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
								 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
								 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
								 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) transferBlocks %x, IdeDisk->BytesPerBlock = %x, transferLength = %d Length change from %d to %d\n", 
								 Ccb->Cdb[0], transferBlocks, IdeDisk->BytesPerBlock, transferLength, Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_WRITE: {	//0x2A

		ULONG	logicalBlockAddress = 0;	
		ULONG	transferBlocks = 0;
		ULONG	transferLength = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		logicalBlockAddress = (Ccb->PKCMD[2] << 24);
		logicalBlockAddress |= (Ccb->PKCMD[3] << 16);
		logicalBlockAddress |= (Ccb->PKCMD[4] << 8);
		logicalBlockAddress |= Ccb->PKCMD[5];

		transferBlocks = (Ccb->PKCMD[7] << 8);
		transferBlocks |= Ccb->PKCMD[8];

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( IdeDisk->LogicalBlockAddress == 0	||
					 logicalBlockAddress & 0x80000000	||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_SEEK: {	//0x2B

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_SYNCHRONIZE_CACHE: {	//0x35

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->DataBufferLength == 0 );

		break;
	}

	case SCSIOP_READ_SUB_CHANNEL: {	//0x42

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.SUBCHANNEL.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.SUBCHANNEL.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.SUBCHANNEL.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.SUBCHANNEL.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_READ_TOC: { //0x43

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if ((IdeDisk->DVD_TYPE == LOGITEC) && (Ccb->PKCMD[9] & 0x80)) {

			NDAS_BUGON( FALSE );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("[READ_TOC CHANGE]Ccb->PKCMD[9] = (Ccb->PKCMD[9] & 0x3F)\n") );
					
			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0 );
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );	

			goto ErrorOut;
		}

		allocationLength = Ccb->PacketCdb.READ_TOC.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.READ_TOC.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			//NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.READ_TOC.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.READ_TOC.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_GET_CONFIGURATION: {	//0x46

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.GET_CONFIGURATION.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.GET_CONFIGURATION.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			//NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.GET_CONFIGURATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.GET_CONFIGURATION.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_GET_EVENT_STATUS: {	//0x4A

		UINT16 eventListLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		eventListLength = Ccb->PacketCdb.GET_EVENT_STATUS_NOTIFICATION.EventListLength[0] << 8;
		eventListLength |= Ccb->PacketCdb.GET_EVENT_STATUS_NOTIFICATION.EventListLength[1];

		if (eventListLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = eventListLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_256) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], eventListLength, Ccb->SecondaryBufferLength) );
		
			if (eventListLength > MAX_PIO_LENGTH_256) {

				eventListLength = MAX_PIO_LENGTH_256;

				Ccb->PacketCdb.GET_EVENT_STATUS_NOTIFICATION.EventListLength[0] = (eventListLength >> 8) & 0xFF;
				Ccb->PacketCdb.GET_EVENT_STATUS_NOTIFICATION.EventListLength[1] = eventListLength & 0xFF;

				Ccb->SecondaryBufferLength = eventListLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_256;
			}
		}

#endif

		break;
	}

	case SCSIOP_STOP_PLAY_SCAN: {	//0x4E

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_READ_DISC_INFORMATION: {	//0x51	SCSIOP_XPWRITE SCSIOP_READ_DISK_INFORMATION

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.READ_DISK_INFORMATION.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.READ_DISK_INFORMATION.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.READ_DISK_INFORMATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.READ_DISK_INFORMATION.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_READ_TRACK_INFORMATION: { //0x52

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.READ_TRACK_INFORMATION.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.READ_TRACK_INFORMATION.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.READ_TRACK_INFORMATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.READ_TRACK_INFORMATION.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_RESERVE_TRACK_RZONE: {	//0x53  SCSIOP_XDWRITE_READ

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_SEND_OPC_INFORMATION: {	//0x54

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_MODE_SELECT10: {	// 0x55

		USHORT parameterListLength;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (Ccb->DataBufferLength < 8) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
			
			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
		
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
			status = STATUS_SUCCESS;
	
			goto ErrorOut;
		}

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			if (((PUINT8)Ccb->DataBuffer)[8] == MODE_PAGE_WRITE_PARAMETERS) {

				if ((Ccb->SenseBuffer) && (Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))) {

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
				}

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				status = STATUS_SUCCESS;
				goto ErrorOut;
			}
		}

		parameterListLength = Ccb->SrbCdb.MODE_SELECT10.ParameterListLength[0] << 8;
		parameterListLength |= Ccb->SrbCdb.MODE_SELECT10.ParameterListLength[1];

		NDAS_BUGON( parameterListLength == Ccb->DataBufferLength );

		if (parameterListLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = parameterListLength;

			RtlCopyMemory( Ccb->SecondaryBuffer, Ccb->DataBuffer, Ccb->DataBufferLength );
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], parameterListLength, Ccb->SecondaryBufferLength) );
		
			if (parameterListLength > MAX_PIO_LENGTH_4096) {

				parameterListLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[0] = (UCHAR)((parameterListLength >> 8) & 0xFF);
				Ccb->PacketCdb.MODE_SELECT10.ParameterListLength[1] = (UCHAR)(parameterListLength & 0xFF);

				Ccb->SecondaryBufferLength = parameterListLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_MODE_SENSE10: {	//0x5A

		UINT16 allocationLength;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (Ccb->DataBufferLength < 8) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
			
			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
		
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
			status = STATUS_SUCCESS;
	
			goto ErrorOut;
		}

		allocationLength = Ccb->SrbCdb.MODE_SENSE10.AllocationLength[0] << 8;
		allocationLength |= Ccb->SrbCdb.MODE_SENSE10.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			//NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.MODE_SENSE10.AllocationLength[0] = (UCHAR)((allocationLength >> 8) & 0xFF);
				Ccb->PacketCdb.MODE_SENSE10.AllocationLength[1] = (UCHAR)(allocationLength & 0xFF);

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		//NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_CLOSE_TRACK_SESSION: {	//0x5B

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->DataBufferLength == 0 );

		break;
	}

	case SCSIOP_READ_BUFFER_CAPACITY: {	//0x5C

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.READ_BUFFER_CAPACITY.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.READ_BUFFER_CAPACITY.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.READ_BUFFER_CAPACITY.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.READ_BUFFER_CAPACITY.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_SEND_CUE_SHEET: {	//0x5D

		UINT32 cueSheetSize;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		cueSheetSize = Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[0] << 16;
		cueSheetSize |= Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[1] << 8;
		cueSheetSize |= Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[2];

		if (cueSheetSize > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = cueSheetSize;

			RtlCopyMemory( Ccb->SecondaryBuffer, Ccb->DataBuffer, Ccb->DataBufferLength );
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], cueSheetSize, Ccb->SecondaryBufferLength) );
		
			if (cueSheetSize > MAX_PIO_LENGTH_4096) {

				cueSheetSize = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[0] = (cueSheetSize >> 16) & 0xFF;
				Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[1] = (cueSheetSize >> 8) & 0xFF;
				Ccb->PacketCdb.SEND_CUE_SHEET.CueSheetSize[2] = cueSheetSize & 0xFF;

				Ccb->SecondaryBufferLength = cueSheetSize;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_REPORT_LUNS: {	// 0xA0

		PLUN_LIST	lunlist;
		UINT32		luncount = 1;
		UINT32		lunsize = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );

		if (sizeof(LUN_LIST) != Ccb->SecondaryBufferLength) {

			if (sizeof(LUN_LIST) > Ccb->SecondaryBufferLength) {

				NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				Ccb->SecondaryBufferLength = sizeof(LUN_LIST);
			}
		}

		lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[0] << 24);
		lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[1] << 16);
		lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[2] << 8);
		lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[3] );

		KDPrintM(DBG_LURN_INFO, ("size of allocated buffer %d\n",lunsize));

		luncount = luncount * 8;

		lunlist = (PLUN_LIST)Ccb->SecondaryBuffer;
		lunlist->LunListLength[0] = ((luncount >> 24)  & 0xff);
		lunlist->LunListLength[1] = ((luncount >> 16)  & 0xff);
		lunlist->LunListLength[2] = ((luncount >> 8) & 0xff);
		lunlist->LunListLength[3] = ((luncount >> 0) & 0xff); 

		RtlZeroMemory( lunlist->Lun, 8 );

		LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
		status = STATUS_SUCCESS;

		goto ErrorOut;

		break;
	}

	case SCSIOP_BLANK: {	// 0xA1 SCSIOP_ATA_PASSTHROUGH12

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );

			if ((Ccb->SenseBuffer) && ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))) {

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0 );
			}

			LsCcbSetStatus( Ccb,	CCB_STATUS_COMMAND_FAILED );

			status = STATUS_SUCCESS;
			goto ErrorOut;
		}

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

	case SCSIOP_SEND_EVENT: {	//0xA2

		UINT16 parameterListLength = 0;
		UINT16 transferLen = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		RtlZeroMemory( Ccb->SecondaryBuffer, Ccb->SecondaryBufferLength );

		parameterListLength |= (Ccb->PKCMD[8] << 8);
		parameterListLength |= (Ccb->PKCMD[9]);

		if (parameterListLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = parameterListLength;

			RtlCopyMemory( Ccb->SecondaryBuffer, Ccb->DataBuffer, Ccb->DataBufferLength );
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], parameterListLength, Ccb->SecondaryBufferLength) );
		
			if (parameterListLength > MAX_PIO_LENGTH_4096) {

				parameterListLength = MAX_PIO_LENGTH_4096;

				Ccb->PKCMD[8] = (parameterListLength >> 8) & 0xFF;
				Ccb->PKCMD[9] = parameterListLength & 0xFF;

				Ccb->SecondaryBufferLength = parameterListLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_SEND_KEY: {	//0xA3 SCSIOP_MAINTENANCE_IN

		USHORT parameterListLength;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		parameterListLength = Ccb->SrbCdb.SEND_KEY.ParameterListLength[0] << 8;
		parameterListLength |= Ccb->SrbCdb.SEND_KEY.ParameterListLength[1];

		NDAS_BUGON( parameterListLength == Ccb->DataBufferLength );

		if (parameterListLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = parameterListLength;

			RtlCopyMemory( Ccb->SecondaryBuffer, Ccb->DataBuffer, Ccb->DataBufferLength );
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], parameterListLength, Ccb->SecondaryBufferLength) );
		
			if (parameterListLength > MAX_PIO_LENGTH_4096) {

				parameterListLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.SEND_KEY.ParameterListLength[0] = (UCHAR)((parameterListLength >> 8) & 0xFF);
				Ccb->PacketCdb.SEND_KEY.ParameterListLength[1] = (UCHAR)(parameterListLength & 0xFF);

				Ccb->SecondaryBufferLength = parameterListLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_REPORT_KEY: {	//0xA4 SCSIOP_MAINTENANCE_OUT

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.REPORT_KEY.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.REPORT_KEY.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.REPORT_KEY.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.REPORT_KEY.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

		break;
	}

	case SCSIOP_READ12: { //0xA8 SCSIOP_GET_MESSAGE

		ULONG	logicalBlockAddress = 0;	
		ULONG	transferBlocks = 0;
		ULONG	transferLength = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		logicalBlockAddress |= (Ccb->PKCMD[2] << 24);
		logicalBlockAddress |= (Ccb->PKCMD[3] << 16);
		logicalBlockAddress |= (Ccb->PKCMD[4] << 8);
		logicalBlockAddress |= Ccb->PKCMD[5];

		transferBlocks |= (Ccb->PKCMD[6] << 24);
		transferBlocks |= (Ccb->PKCMD[7] << 16);
		transferBlocks |= (Ccb->PKCMD[8] << 8);
		transferBlocks |= (Ccb->PKCMD[9]);

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( logicalBlockAddress & 0x80000000 ||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_WRITE12: {	//0xAA

		ULONG	logicalBlockAddress = 0;	
		ULONG	transferBlocks = 0;
		ULONG	transferLength = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			if (!LsCcbIsFlagOn(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS)) {

				NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
				status = STATUS_SUCCESS;

				goto ErrorOut;
			}
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		logicalBlockAddress = (Ccb->PKCMD[2] << 24);
		logicalBlockAddress |= (Ccb->PKCMD[3] << 16);
		logicalBlockAddress |= (Ccb->PKCMD[4] << 8);
		logicalBlockAddress |= Ccb->PKCMD[5];

		transferBlocks = (Ccb->PKCMD[6] << 24);
		transferBlocks |= (Ccb->PKCMD[7] << 16);
		transferBlocks |= (Ccb->PKCMD[8] << 8);
		transferBlocks |= (Ccb->PKCMD[9]);

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( IdeDisk->LogicalBlockAddress == 0	||
					 logicalBlockAddress & 0x80000000	||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}
		
		} else {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );
		}

#endif
#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_GET_PERFORMANCE: {	//0xAC

		UINT16 maximumNumberOfDescriptors;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		maximumNumberOfDescriptors = Ccb->PacketCdb.GET_PERFORMANCE.MaximumNumberOfDescriptors[0] << 8;
		maximumNumberOfDescriptors |= Ccb->PacketCdb.GET_PERFORMANCE.MaximumNumberOfDescriptors[1];

#if 0

		if (MaxNumberOfDescriptor != Ccb->SecondaryBufferLength) {

			NDAS_BUGON( FALSE );

			if (transferLen > Ccb->SecondaryBufferLength) {

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				Ccb->SecondaryBufferLength = transferLen;
			}
		}

#endif

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], maximumNumberOfDescriptors, Ccb->SecondaryBufferLength) );
		
			Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
		}

#endif

		break;
	}

	case SCSIOP_READ_DVD_STRUCTURE: {	//0xAD

		UINT16 allocationLength;
		
		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		allocationLength = Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[0] << 8;
		allocationLength |= Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[1];

		if (allocationLength > Ccb->SecondaryBufferLength) {

			Ccb->SecondaryBuffer		= IdeDisk->Buffer;
			Ccb->SecondaryBufferLength  = allocationLength;
		}

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		if (Ccb->SecondaryBufferLength > MAX_PIO_LENGTH_4096) {

			//NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("Command (0x%02x) Length change Transfer length %d %d\n",
						 Ccb->Cdb[0], allocationLength, Ccb->SecondaryBufferLength) );
		
			if (allocationLength > MAX_PIO_LENGTH_4096) {

				allocationLength = MAX_PIO_LENGTH_4096;

				Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
				Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[1] = allocationLength & 0xFF;

				Ccb->SecondaryBufferLength = allocationLength;
			
			} else {

				Ccb->SecondaryBufferLength = MAX_PIO_LENGTH_4096;
			}
		}

#endif

#if __NDAS_CHIP_PIO_READ_AND_WRITE__

		allocationLength = MAX_PIO_LENGTH_4096;

		Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
		Ccb->PacketCdb.READ_DVD_STRUCTURE.AllocationLength[1] = allocationLength & 0xFF;

		Ccb->SecondaryBuffer = IdeDisk->Buffer;
		Ccb->SecondaryBufferLength = allocationLength;

#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_SEND_VOLUME_TAG: {	//0xB6

		UINT16 transferLen = 0;

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			NDAS_BUGON( FALSE );
		}

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		transferLen = (Ccb->PKCMD[9] << 8);
		transferLen |= (Ccb->PKCMD[10]);

		if (transferLen != Ccb->SecondaryBufferLength) {

			NDAS_BUGON( FALSE );

			if (transferLen > Ccb->SecondaryBufferLength) {

				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				status = STATUS_SUCCESS;

				goto ErrorOut;

			} else {

				//Ccb->SecondaryBufferLength = transferLen;
			}
		}

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_SET_CD_SPEED: {	//0xBB

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength == 0 );

		break;
	}

#if __BSRECORDER_HACK__

	case 0xD8: {	

		ULONG	transferLength;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		transferLength = ((ULONG)Ccb->Cdb[6] << 24) | ((ULONG)Ccb->Cdb[7] << 16) | ((ULONG)Ccb->Cdb[8] << 8) | Ccb->Cdb[9];

		if ((Ccb->SecondaryBufferLength / transferLength) == 2352) {

			NDAS_BUGON( FALSE );

#if 0
			KDPrintM(DBG_LURN_INFO, ("Vendor PKCMD 0x%02x to READ_CD(0xBE)\n",Ccb->Cdb[0]));

			Ccb->Cdb[0] = SCSIOP_READ_CD;
			Ccb->Cdb[1] = 0x04;			// Expected sector type: CD-DA
			Ccb->Cdb[6] = Ccb->Cdb[7];	// Transfer length
			Ccb->Cdb[7] = Ccb->Cdb[8];
			Ccb->Cdb[8] = Ccb->Cdb[9];
			Ccb->Cdb[9] = 0xf0;			// SYNC, All headers, UserData included.
#endif		
		}

		break;
	}

#endif

	case SCSIOP_READ_CD: { // 0xBE SCSIOP_VOLUME_SET_IN		

		ULONG	logicalBlockAddress = 0;	
		ULONG	transferBlocks = 0;
		ULONG	transferLength = 0;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		logicalBlockAddress = (Ccb->PKCMD[2] << 24);
		logicalBlockAddress |= (Ccb->PKCMD[3] << 16);
		logicalBlockAddress |= (Ccb->PKCMD[4] << 8);
		logicalBlockAddress |= Ccb->PKCMD[5];

		transferBlocks = (Ccb->PKCMD[6] << 16);
		transferBlocks |= (Ccb->PKCMD[7] << 8);
		transferBlocks |= Ccb->PKCMD[8];

#if 0
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (transferLength != Ccb->SecondaryBufferLength || IdeDisk->LogicalBlockAddress == 0) {

			OddGetBlockSize( IdeDisk );
			transferLength = transferBlocks * (IdeDisk->BytesPerBlock);
		}

		NDAS_BUGON( logicalBlockAddress & 0x80000000 ||
					 logicalBlockAddress + transferBlocks <= (IdeDisk->LogicalBlockAddress + 1) );
#else

#if 0
		if (IdeDisk->BytesPerBlock == 0) {

			OddGetBlockSize( IdeDisk );
		}

		NDAS_BUGON( IdeDisk->BytesPerBlock );
		transferLength = transferBlocks * (IdeDisk->BytesPerBlock);

		if (IdeDisk->BytesPerBlock) {

			if (transferLength != Ccb->SecondaryBufferLength) {
		
				if (transferLength > Ccb->SecondaryBufferLength) {

					NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

					Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
					status = STATUS_SUCCESS;

					goto ErrorOut;

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
								("Command (0x%02x) Length change from %d to %d\n", 
								 Ccb->Cdb[0], Ccb->SecondaryBufferLength, transferLength) );

					//Ccb->SecondaryBufferLength = transferLength;
				}
			}	
		}

#endif
#endif

#if 0

#if __NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

		Ccb->PKCMD[0] = SCSIOP_READ12;

		Ccb->PKCMD[2] = Ccb->Cdb[2];
		Ccb->PKCMD[3] = Ccb->Cdb[3];
		Ccb->PKCMD[4] = Ccb->Cdb[4];
		Ccb->PKCMD[5] = Ccb->Cdb[5];

		Ccb->PKCMD[6] = 0;
		Ccb->PKCMD[7] = Ccb->Cdb[6];
		Ccb->PKCMD[8] = Ccb->Cdb[7];
		Ccb->PKCMD[9] = Ccb->Cdb[8];

		Ccb->PKCMD[11] = Ccb->Cdb[11];	
	
#else

		if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

			RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );

			Ccb->PKCMD[0] = SCSIOP_READ12;

			Ccb->PKCMD[2] = Ccb->Cdb[2];
			Ccb->PKCMD[3] = Ccb->Cdb[3];
			Ccb->PKCMD[4] = Ccb->Cdb[4];
			Ccb->PKCMD[5] = Ccb->Cdb[5];

			Ccb->PKCMD[6] = 0;
			Ccb->PKCMD[7] = Ccb->Cdb[6];
			Ccb->PKCMD[8] = Ccb->Cdb[7];
			Ccb->PKCMD[9] = Ccb->Cdb[8];

			Ccb->PKCMD[11] = Ccb->Cdb[11];	
		}

#endif

#endif

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xDF: { // security erase

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case SCSIOP_DENON_READ_TOC: {	// 0xE9

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xED: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xF0: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xF9: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xFA: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xFC: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xFD: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xFE: {

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		//NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	case 0xFF: { // security erase

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		//NDAS_BUGON( Ccb->SecondaryBufferLength % 4 == 0 );

		break;
	}

	default: {

		NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

		// Data transfer direction
		ndasBufferRequiredOperation = TRUE;

		RtlZeroMemory( Ccb->PKCMD, sizeof(CDB) );
		RtlCopyMemory( Ccb->PKCMD, Ccb->Cdb, Ccb->CdbLength );

		Ccb->SecondaryBuffer		= Ccb->DataBuffer;
		Ccb->SecondaryBufferLength  = Ccb->DataBufferLength;

		break;
	}
	}

	waitCount = 0;

#if 0

	while (waitCount ++ < 10*60) {

		status = ODD_GetStatus( IdeDisk, Ccb, &senseData, sizeof(SENSE_DATA) );

		if (status != STATUS_SUCCESS) {

			NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMUNICATION_ERROR );

			goto ErrorOut;
		}

		if (!FlagOn(IdeDisk->RegStatus, BUSY_STAT)) {

			break;
		}

		NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

		waitCount ++;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("time Waiting command 0x%02x Waiting Count %d\n", IdeDisk->PrevRequest, waitCount) );

		ODD_BusyWait(1);
	} 

	NDAS_BUGON( waitCount < 10*60 );

#endif

#if 0
	// sanity check

	switch (Ccb->PacketCdb.AsByte[0]) {

	case SCSIOP_MODE_SELECT10: {

		PMODE_PARAMETER_HEADER10	modeParameterHeader = (PMODE_PARAMETER_HEADER10)(Ccb->SecondaryBuffer);
		UCHAR						pageCode;
		LONG						pageLength;
		USHORT						modeDataLength;
		USHORT						blockDescriptorLength;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("SCSIOP_MODE_SELECT10: Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
					Ccb, ((PUINT8)Ccb->SecondaryBuffer)[0], ((PUINT8)Ccb->SecondaryBuffer)[1], 
					((PUINT8)Ccb->SecondaryBuffer)[2], ((PUINT8)Ccb->SecondaryBuffer)[3], ((PUINT8)Ccb->SecondaryBuffer)[4], 
					((PUINT8)Ccb->SecondaryBuffer)[5], ((PUINT8)Ccb->SecondaryBuffer)[6], ((PUINT8)Ccb->SecondaryBuffer)[7], 
					Ccb->SecondaryBufferLength) );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("SCSIOP_MODE_SELECT10: Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
					Ccb, ((PUINT8)Ccb->SecondaryBuffer)[8], ((PUINT8)Ccb->SecondaryBuffer)[9], 
					((PUINT8)Ccb->SecondaryBuffer)[10], ((PUINT8)Ccb->SecondaryBuffer)[11], ((PUINT8)Ccb->SecondaryBuffer)[12], 
					((PUINT8)Ccb->SecondaryBuffer)[13], ((PUINT8)Ccb->SecondaryBuffer)[14], ((PUINT8)Ccb->SecondaryBuffer)[15], 
					Ccb->SecondaryBufferLength) );

		modeDataLength = modeParameterHeader->ModeDataLength[0] << 8;
		modeDataLength += modeParameterHeader->ModeDataLength[1];

		blockDescriptorLength = modeParameterHeader->BlockDescriptorLength[0] << 8;
		blockDescriptorLength += modeParameterHeader->BlockDescriptorLength[1];

		NDAS_BUGON( blockDescriptorLength == 0 );
		NDAS_BUGON( modeDataLength == 0 );
		NDAS_BUGON( modeDataLength <= MAX_PIO_LENGTH_4096 );

		pageCode = ((PUINT8)(Ccb->SecondaryBuffer))[sizeof(MODE_PARAMETER_HEADER10)] & 0x3F;
		pageLength = ((PUINT8)(Ccb->SecondaryBuffer))[sizeof(MODE_PARAMETER_HEADER10)+1];

#if 0 //__NDAS_CHIP_PIO_LIMIT_BUFFER_SIZE__

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("modeDataLength = %d, blockDescriptorLength = %d, pageLength = %d \n", 
					 modeDataLength, blockDescriptorLength, pageLength) );

		switch (pageCode) {

		case MODE_PAGE_WRITE_PARAMETERS: {

			NDAS_BUGON( pageLength == Ccb->SecondaryBufferLength - 10 );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("SCSIOP_MODE_SELECT10: filtering MODE_PAGE_WRITE_PARAMETERS\n") );

			//Ccb->ResidualDataLength = Ccb->DataBufferLength;

			//LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 
			//				SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 
			//				SCSI_SENSEQ_CAUSE_NOT_REPORTABLE );

			//LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );
			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );

			status = STATUS_SUCCESS;

			goto ErrorOut;

			break;
		}

		default:

			NDAS_BUGON( pageLength == Ccb->SecondaryBufferLength - 10 );

			break;
		}


		while ((pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0)) {

			pageCode = ((PUINT8)(Ccb->SecondaryBuffer))[pageLength] & 0x3F;

			if (pageCode == MODE_PAGE_CAPABILITIES) { // 0x2A

				PCDVD_CAPABILITIES_PAGE	cdvdCapabilitiesPage;

				cdvdCapabilitiesPage = (PCDVD_CAPABILITIES_PAGE)(&((PUINT8)Ccb->SecondaryBuffer)[pageLength]);

				NDAS_BUGON( cdvdCapabilitiesPage->PageCode == MODE_PAGE_CAPABILITIES );

				DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("change capacity\n") );

				cdvdCapabilitiesPage->CDRWrite = 0;
				cdvdCapabilitiesPage->CDEWrite = 0;
				cdvdCapabilitiesPage->TestWrite = 0;
				cdvdCapabilitiesPage->DVDRWrite = 0;
				cdvdCapabilitiesPage->DVDRAMWrite = 0;

				cdvdCapabilitiesPage->RWSupported = 0;

				break;
			}

			if (((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1] == 0) {
							
				NDAS_BUGON( FALSE );
				break;
			}

			pageLength += ((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1];
			pageLength += 2;
		}

		NDAS_BUGON( (pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0) );

#endif

		break;
	}

	default:

		break;

	}

#endif

	timeOut.QuadPart = -LURN_IDE_ODD_TIMEOUT;

	switch (Ccb->Cdb[0]) {

	case SCSIOP_READ6:
	case SCSIOP_READ:
	case SCSIOP_READ12:
	case SCSIOP_READ16:
	case SCSIOP_READ_CD:
	case SCSIOP_PLEXTOR_CDDA:
	case SCSIOP_NEC_CDDA:
		{

		status = AtapiReadRequest( Lurn, IdeDisk, Ccb, &response, &timeOut, oddRegister );
		break;
	}

	case SCSIOP_WRITE6:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE12:
	case SCSIOP_WRITE16:
	case SCSIOP_WRITE_VERIFY:
	case SCSIOP_WRITE_VERIFY12:
	case SCSIOP_WRITE_VERIFY16:	{
		
		status = AtapiWriteRequest( Lurn, IdeDisk, Ccb, &response, &timeOut, oddRegister );
		break;
	}

	default: {
		LARGE_INTEGER shortTimeout;

		shortTimeout.QuadPart = -10 * NANO100_PER_SEC; 

		//
		// Acquire the buffer lock allocated for this unit device.
		// Even though the device access mode is read-only,
		// we still need to acquire the buffer lock.
		// Data-sending commands still come and go in read-only access mode.
		//

		if(ndasBufferRequiredOperation) {

			StopIoIdleTimer( &IdeDisk->BuffLockCtl );

			status = NdasAcquireBufferLock( &IdeDisk->BuffLockCtl,
				IdeDisk->LanScsiSession,
				&IdeDisk->LuHwData,
				NULL,
				&shortTimeout );

			if (status == STATUS_NOT_SUPPORTED) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Lock is not supported\n") );

			} else if (status != STATUS_SUCCESS) {

				NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Failed to acquire the buffer lock\n") );

				Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
				break;
			}
		}

		LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &timeOut );

		status = LspPacketRequest( IdeDisk->LanScsiSession, &pduDesc, &response, oddRegister );
	
		//	Release the buffer lock allocated for this unit device.
		// Release the buffer lock only when the connection is alive.
		if(ndasBufferRequiredOperation) {

			if(NT_SUCCESS(status)) {


				NdasReleaseBufferLock( &IdeDisk->BuffLockCtl,
					IdeDisk->LanScsiSession,
					&IdeDisk->LuHwData,
					NULL,
					&shortTimeout,
					FALSE,
					Ccb->SecondaryBufferLength );

				StartIoIdleTimer( &IdeDisk->BuffLockCtl );
			}

		}

		break;
		}
	}

#if 0
	timeOut.QuadPart = -20 * 60 * NANO100_PER_SEC;		// 20 Minutes

	LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &timeOut );
			
	status = LspPacketRequest( IdeDisk->LanScsiSession, &pduDesc, &response, oddRegister );
#endif

	IdeDisk->RegError	= oddRegister[0];
	IdeDisk->RegStatus	= oddRegister[1];

	if (status != STATUS_SUCCESS) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
					 IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS) );
	}

	if (status != STATUS_SUCCESS) {

#if __NDAS_CHIP_BUG_AVOID__

		NDAS_BUGON( status == STATUS_PORT_DISCONNECTED || status == STATUS_CANCELLED );

		if (status == STATUS_CANCELLED) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
						("__NDAS_CHIP_BUG_AVOID__ "
						 "Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
						 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
						 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
						 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

			NDAS_BUGON( Ccb->SrbCdb.AsByte[0] == SCSIOP_GET_PERFORMANCE/*		||
						 Ccb->SrbCdb.AsByte[0] == 0xFC							|| 
						 Ccb->SrbCdb.AsByte[0] == SCSIOP_READ_DVD_STRUCTURE */);

			if (IdeDisk->CanceledCount) {

				NDAS_BUGON( Ccb->SrbCdb.AsByte[0] == IdeDisk->PrevRequest );
			}

			IdeDisk->CanceledCount ++;

			if (IdeDisk->CanceledCount > MAX_CANCEL_COUNT) { // Caution !! MAX_CANCEL_COUNT = 0

				IdeDisk->CanceledCount = 0;

				Ccb->ResidualDataLength = Ccb->DataBufferLength;

				LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 
								SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 
								SCSI_SENSEQ_CAUSE_NOT_REPORTABLE );

				LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

				goto ErrorOut;			
			}		
		}
#else
		NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
#endif
		LsCcbSetStatus( Ccb, CCB_STATUS_COMMUNICATION_ERROR );

	} else {

		NDAS_BUGON( response == LANSCSI_RESPONSE_SUCCESS || response == LANSCSI_RESPONSE_T_COMMAND_FAILED );

		switch (response) {

		case LANSCSI_RESPONSE_SUCCESS: {

			LsCcbSetStatus( Ccb, CCB_STATUS_SUCCESS );
			break;
		}

		case LANSCSI_RESPONSE_T_NOT_EXIST:

			LsCcbSetStatus( Ccb, CCB_STATUS_NOT_EXIST );
			break;

		case LANSCSI_RESPONSE_T_BAD_COMMAND:

			LsCcbSetStatus( Ccb, CCB_STATUS_INVALID_COMMAND );
			break;

		case LANSCSI_RESPONSE_T_BROKEN_DATA:
		case LANSCSI_RESPONSE_T_COMMAND_FAILED:{
		
			if (Ccb->SenseBuffer && (Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))) {

				LARGE_INTEGER	timeout;
				BYTE			response;

#if __NDAS_CHIP_BUG_AVOID__
				if (Ccb->PacketCdb.AsByte[0] == SCSIOP_MODE_SELECT10 ||
					Ccb->PacketCdb.AsByte[0] == SCSIOP_WRITE ||
					Ccb->PacketCdb.AsByte[0] == SCSIOP_WRITE12 ||
					Ccb->PacketCdb.AsByte[0] == SCSIOP_WRITE16) {

					timeout.QuadPart = -10 * NANO100_PER_SEC; 
				
				} else {

					timeout.QuadPart = -LURN_IDE_ODD_TIMEOUT;
				}
#else
				timeout.QuadPart = -LURN_IDE_ODD_TIMEOUT;
#endif

				status = ProcessErrorCode( Lurn, IdeDisk, Ccb, &timeout, &response );

				if (status == STATUS_SUCCESS && response == LANSCSI_RESPONSE_SUCCESS) {
					
					if (Ccb->CcbStatus == CCB_STATUS_BUSY) {

						if (IdeDisk->DVD_REPEAT_COUNT == MAX_REFEAT_COUNT) {

							NDAS_BUGON( FALSE );
		
							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							Ccb->CcbStatus = CCB_STATUS_COMMMAND_DONE_SENSE;

						} else {

							IdeDisk->DVD_REPEAT_COUNT ++;
							ODD_BusyWait( 5 );
						}
					}
				
				} else {

#if __NDAS_CHIP_BUG_AVOID__

					NDAS_BUGON( status == STATUS_PORT_DISCONNECTED || status == STATUS_CANCELLED );

					if (status == STATUS_CANCELLED) {

						DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
									("__NDAS_CHIP_BUG_AVOID__ "
									 "Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x: BufferLength = %d\n",
									 Ccb, Ccb->Cdb[0], Ccb->Cdb[1], Ccb->Cdb[2], Ccb->Cdb[3], Ccb->Cdb[4], Ccb->Cdb[5], Ccb->Cdb[6], 
									 Ccb->Cdb[7], Ccb->Cdb[8], Ccb->Cdb[9], Ccb->Cdb[10], Ccb->Cdb[11], Ccb->Cdb[12], Ccb->Cdb[13], 
									 Ccb->Cdb[14], Ccb->Cdb[15], Ccb->DataBufferLength) );

						if (Ccb->SecondaryBufferLength >= 16) {

							DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
									    ("__NDAS_CHIP_BUG_AVOID__ "
										"Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
										 Ccb, ((PUINT8)Ccb->SecondaryBuffer)[0], ((PUINT8)Ccb->SecondaryBuffer)[1], 
										 ((PUINT8)Ccb->SecondaryBuffer)[2], ((PUINT8)Ccb->SecondaryBuffer)[3], ((PUINT8)Ccb->SecondaryBuffer)[4], 
										 ((PUINT8)Ccb->SecondaryBuffer)[5], ((PUINT8)Ccb->SecondaryBuffer)[6], ((PUINT8)Ccb->SecondaryBuffer)[7], 
										 Ccb->SecondaryBufferLength) );

							DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
									    ("__NDAS_CHIP_BUG_AVOID__ "
										"Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
										 Ccb, ((PUINT8)Ccb->SecondaryBuffer)[8], ((PUINT8)Ccb->SecondaryBuffer)[9], 
										 ((PUINT8)Ccb->SecondaryBuffer)[10], ((PUINT8)Ccb->SecondaryBuffer)[11], ((PUINT8)Ccb->SecondaryBuffer)[12], 
										 ((PUINT8)Ccb->SecondaryBuffer)[13], ((PUINT8)Ccb->SecondaryBuffer)[14], ((PUINT8)Ccb->SecondaryBuffer)[15], 
										 Ccb->SecondaryBufferLength) );
						}

						NDAS_BUGON( Ccb->SrbCdb.AsByte[0] == SCSIOP_MODE_SELECT10	||
									 /*Ccb->SrbCdb.AsByte[0] == SCSIOP_GET_CONFIGURATION	|| 
									 Ccb->SrbCdb.AsByte[0] == SCSIOP_READ_DVD_STRUCTURE || */
									Ccb->SrbCdb.AsByte[0] == SCSIOP_WRITE ||
									Ccb->SrbCdb.AsByte[0] == SCSIOP_WRITE12 ||
									Ccb->SrbCdb.AsByte[0] == SCSIOP_WRITE16
									 );

						if (IdeDisk->CanceledCount) {

							NDAS_BUGON( Ccb->SrbCdb.AsByte[0] == IdeDisk->PrevRequest );
						}

						IdeDisk->CanceledCount ++;

						if (IdeDisk->CanceledCount > MAX_CANCEL_COUNT) { // Caution !! MAX_CANCEL_COUNT = 0

							IdeDisk->CanceledCount = 0;

							Ccb->ResidualDataLength = Ccb->DataBufferLength;

							LSCCB_SETSENSE( Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 
											SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 
											SCSI_SENSEQ_CAUSE_NOT_REPORTABLE );

							LsCcbSetStatus( Ccb, CCB_STATUS_COMMMAND_DONE_SENSE );

							goto ErrorOut;			
						}
					}
#else
					NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
#endif
					LsCcbSetStatus( Ccb, CCB_STATUS_COMMUNICATION_ERROR );
					status = STATUS_PORT_DISCONNECTED;
				}

			} else {

				NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );
			}

			break;
		}

		default:
				
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			Ccb->ResidualDataLength = Ccb->DataBufferLength;
			LsCcbSetStatus( Ccb, CCB_STATUS_COMMAND_FAILED );

			break;
		}

		if (IdeDisk->DVD_REPEAT_COUNT > 1) {

			NDAS_BUGON( IdeDisk->PrevRequest == Ccb->Cdb[0] );
		}

		if (Ccb->CcbStatus != CCB_STATUS_BUSY) {

			IdeDisk->CanceledCount = 0;
			IdeDisk->DVD_REPEAT_COUNT = 0;
		}

		IdeDisk->PrevRequest = Ccb->Cdb[0];
	}

	if (status == STATUS_SUCCESS && Ccb->CcbStatus == CCB_STATUS_SUCCESS) {

		if (Ccb->DataBufferLength > Ccb->SecondaryBufferLength) {

			RtlZeroMemory( (PUINT8)Ccb->DataBuffer + Ccb->SecondaryBufferLength, 
						   Ccb->DataBufferLength - Ccb->SecondaryBufferLength );
		}

		if (Ccb->DataBuffer != Ccb->SecondaryBuffer) {

			RtlCopyMemory( Ccb->DataBuffer, Ccb->SecondaryBuffer,
						   Ccb->DataBufferLength < Ccb->SecondaryBufferLength ?
								Ccb->DataBufferLength : Ccb->SecondaryBufferLength );
		}

		switch (Ccb->PKCMD[0]) {

		case SCSIOP_TEST_UNIT_READY: {	//0x00

			break;
		}

		case SCSIOP_REZERO_UNIT: {	//0x01

			break;
		}

		case SCSIOP_REQUEST_SENSE: {	// 0x03

			break;
		}

		case SCSIOP_READ6: {	//0x08 SCSIOP_RECEIVE

			break;
		}

		case SCSIOP_WRITE6: {	//0x08 SCSIOP_PRINT SCSIOP_SEND

			break;
		}

		case SCSIOP_SEEK6: {	//0x0B SCSIOP_TRACK_SELECT SCSIOP_SLEW_PRINT SCSIOP_SET_CAPACITY 

			break;
		}

		case SCSIOP_INQUIRY: {	//0x12

			PINQUIRYDATA	inquiryData = (PINQUIRYDATA)Ccb->DataBuffer;

			DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
						("SCSIOP_INQUIRY: Ccb=%p : %02X %02X %02X %02X  %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
						Ccb, inquiryData->DeviceType, inquiryData->DeviceTypeQualifier, inquiryData->DeviceTypeModifier, inquiryData->AdditionalLength,
						((PUINT8)Ccb->DataBuffer)[0], ((PUINT8)Ccb->DataBuffer)[1], 
						((PUINT8)Ccb->DataBuffer)[2], ((PUINT8)Ccb->DataBuffer)[3], ((PUINT8)Ccb->DataBuffer)[4], 
						((PUINT8)Ccb->DataBuffer)[5], ((PUINT8)Ccb->DataBuffer)[6], ((PUINT8)Ccb->DataBuffer)[7], 
						Ccb->DataBufferLength) );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
						("SCSIOP_INQUIRY: Ccb=%p : %02x %02x %02x %02x  %02x %02x %02x %02x  BufferLength = %d\n",
						Ccb, ((PUINT8)Ccb->DataBuffer)[8], ((PUINT8)Ccb->DataBuffer)[9], 
						((PUINT8)Ccb->DataBuffer)[10], ((PUINT8)Ccb->DataBuffer)[11], ((PUINT8)Ccb->DataBuffer)[12], 
						((PUINT8)Ccb->DataBuffer)[13], ((PUINT8)Ccb->DataBuffer)[14], ((PUINT8)Ccb->DataBuffer)[15], 
						Ccb->SecondaryBufferLength) );

			//inquiryData->AdditionalLength = Ccb->DataBufferLength - 5;

			//((PUINT8)Ccb->DataBuffer)[2] |= 2;
			//((PUINT8)Ccb->DataBuffer)[3] = (((PUINT8)Ccb->DataBuffer)[3] & 0xf0) | 2;

			break;
		}

		case SCSIOP_START_STOP_UNIT: {	//0x1B SCSIOP_STOP_PRINT SCSIOP_LOAD_UNLOAD

			break;
		}

		case SCSIOP_MEDIUM_REMOVAL: {	//0x1E

			break;
		}

		case SCSIOP_READ_FORMATTED_CAPACITY: { //0x23

			break;
		}

		case SCSIOP_READ_CAPACITY: {	//0x25

			PREAD_CAPACITY_DATA readCapacityData;

			readCapacityData = Ccb->SecondaryBuffer;

			if (IdeDisk->BytesPerBlock != NTOHL(readCapacityData->BytesPerBlock) ||
				IdeDisk->LogicalBlockAddress != NTOHL(readCapacityData->LogicalBlockAddress)) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
							("SCSIOP_READ_CAPACITY: %u : %u\n", 
							NTOHL(readCapacityData->LogicalBlockAddress), NTOHL(readCapacityData->BytesPerBlock)) );
			}

			NDAS_BUGON(
				NTOHL(readCapacityData->BytesPerBlock) == 2048 ||
				NTOHL(readCapacityData->BytesPerBlock) == 512 ||
				NTOHL(readCapacityData->BytesPerBlock == 0) ); 

			IdeDisk->ReadCapacityBlockBytes = NTOHL(readCapacityData->BytesPerBlock);
			IdeDisk->LogicalBlockAddress = NTOHL(readCapacityData->LogicalBlockAddress);

			break;
		}

		case SCSIOP_READ: {	//0x28

			break;
		}

		case SCSIOP_WRITE: {	//0x2A

			break;
		}

		case SCSIOP_SEEK: {	//0x2B

			break;
		}

		case SCSIOP_SYNCHRONIZE_CACHE: {	//0x35

			break;
		}

		case SCSIOP_READ_SUB_CHANNEL: {	//0x42

			break;
		}

		case SCSIOP_READ_TOC: { //0x43

			if (IdeDisk->DVD_TYPE == LOGITEC) {

				NDAS_BUGON( FALSE );

				if (((PUINT8)Ccb->SecondaryBuffer)[2] == 0x05) {

					((PUINT8)Ccb->DataBuffer)[0] = 0x0;
					((PUINT8)Ccb->DataBuffer)[1] = 0x2;
				}
			}

			break;
		}

		case SCSIOP_GET_CONFIGURATION: {	//0x46

			if (Ccb->PKCMD[2] == 0x0 && Ccb->PKCMD[3] == 0x0) {

				USHORT	dvdMediaType;

				dvdMediaType = ((PUINT8)Ccb->DataBuffer)[6] << 8;
				dvdMediaType |= ((PUINT8)Ccb->DataBuffer)[7];

				if (dvdMediaType != IdeDisk->DVD_MEDIA_TYPE) {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
								("IdeDisk->DVD_MEDIA_TYPE = %x\n", IdeDisk->DVD_MEDIA_TYPE) );
				}

				IdeDisk->DVD_MEDIA_TYPE = dvdMediaType;

				// 0x08	CD-ROM
				// 0x09 CD-R
				// 0x0A	CD-RW

				// 0x10	DVD-ROM
				// 0x11 DVD-R
				// 0x12 DVD-RAM
				// 0x13 DVD-RW
				// 0x14 DVD-RW
			}

			break;
		}

		case SCSIOP_GET_EVENT_STATUS: {	//0x4A

			break;
		}

		case SCSIOP_STOP_PLAY_SCAN: {	//0x4E

			break;
		}

		case SCSIOP_READ_DISC_INFORMATION: {	//0x51	SCSIOP_XPWRITE SCSIOP_READ_DISK_INFORMATION

			break;
		}

		case SCSIOP_READ_TRACK_INFORMATION: { //0x52

			break;
		}

		case SCSIOP_RESERVE_TRACK_RZONE: {	//0x53  SCSIOP_XDWRITE_READ

			break;
		}

		case SCSIOP_SEND_OPC_INFORMATION: {	//0x54

			break;
		}

		case SCSIOP_MODE_SELECT10: {	// 0x55

			break;
		}

		case SCSIOP_MODE_SENSE10: {	//0x5A

			//	Write Protect for MO 
			//			
			//	this code is unstable only for MO model MOS3400E
			//	hacking MO mode sense of Vender specific code page

					
			if (Lurn->LurnType == LURN_OPTICAL_MEMORY) {

				NDAS_BUGON( FALSE );

				if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

					if (Ccb->SecondaryBufferLength > 8) {

						if (((PUINT8)Ccb->SecondaryBuffer)[0] == 0x0  && 
							((PUINT8)Ccb->SecondaryBuffer)[1] == 0x6F && 
							((PUINT8)Ccb->SecondaryBuffer)[2] == 0x03) {

							((PUINT8)Ccb->SecondaryBuffer)[3] = 0x80;
						}
					}
				}
			}

			if (Ccb->SecondaryBufferLength > 9) {

				if (Ccb->PacketCdb.MODE_SENSE10.PageCode != MODE_SENSE_RETURN_ALL &&
					Ccb->PacketCdb.MODE_SENSE10.PageCode != MODE_PAGE_FAULT_REPORTING &&
					Ccb->PacketCdb.MODE_SENSE10.PageCode != MODE_PAGE_CACHING) {

					NDAS_BUGON( Ccb->PacketCdb.MODE_SENSE10.PageCode == (((PUINT8)(Ccb->SecondaryBuffer))[8] & 0x3F) );
				}
			}

			if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

				if (Ccb->PacketCdb.MODE_SENSE10.PageCode == MODE_PAGE_CAPABILITIES) { // 0x2A

					PCDVD_CAPABILITIES_PAGE	cdvdCapabilitiesPage;

					cdvdCapabilitiesPage = (PCDVD_CAPABILITIES_PAGE)(&((PUINT8)Ccb->SecondaryBuffer)[8]);

					NDAS_BUGON( cdvdCapabilitiesPage->PageCode == MODE_PAGE_CAPABILITIES );

					cdvdCapabilitiesPage->CDRWrite = 0;
					cdvdCapabilitiesPage->CDEWrite = 0;
					cdvdCapabilitiesPage->TestWrite = 0;
					cdvdCapabilitiesPage->DVDRWrite = 0;
					cdvdCapabilitiesPage->DVDRAMWrite = 0;

					cdvdCapabilitiesPage->RWSupported = 0;
				}

				if (Ccb->PacketCdb.MODE_SENSE10.PageCode == MODE_SENSE_RETURN_ALL) {

					PMODE_PARAMETER_HEADER10	modeParameterHeader = (PMODE_PARAMETER_HEADER10)(Ccb->SecondaryBuffer);
					UCHAR						pageCode;
					LONG						pageLength;
					USHORT						modeDataLength;

					modeDataLength = modeParameterHeader->ModeDataLength[0] << 8;
					modeDataLength += modeParameterHeader->ModeDataLength[1];

					NDAS_BUGON( modeDataLength <= MAX_PIO_LENGTH_4096 );

					pageLength = 8;

					while ((pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0)) {

						pageCode = ((PUINT8)(Ccb->SecondaryBuffer))[pageLength] & 0x3F;

						if (pageCode == MODE_PAGE_CAPABILITIES) { // 0x2A

							PCDVD_CAPABILITIES_PAGE	cdvdCapabilitiesPage;

							cdvdCapabilitiesPage = (PCDVD_CAPABILITIES_PAGE)(&((PUINT8)Ccb->SecondaryBuffer)[pageLength]);

							NDAS_BUGON( cdvdCapabilitiesPage->PageCode == MODE_PAGE_CAPABILITIES );

							DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("change capacity\n") );

							cdvdCapabilitiesPage->CDRWrite = 0;
							cdvdCapabilitiesPage->CDEWrite = 0;
							cdvdCapabilitiesPage->TestWrite = 0;
							cdvdCapabilitiesPage->DVDRWrite = 0;
							cdvdCapabilitiesPage->DVDRAMWrite = 0;

							cdvdCapabilitiesPage->RWSupported = 0;

							break;
						}

						if (((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1] == 0) {
							
							NDAS_BUGON( FALSE );
							break;
						}

						pageLength += ((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1];
						pageLength += 2;
					}

					NDAS_BUGON( (pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0) );
				}
			}

#if 0
			//if (!FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

				if (Ccb->PacketCdb.MODE_SENSE10.PageCode == MODE_PAGE_CAPABILITIES) { // 0x2A

					PCDVD_CAPABILITIES_PAGE	cdvdCapabilitiesPage;

					cdvdCapabilitiesPage = (PCDVD_CAPABILITIES_PAGE)(&((PUINT8)Ccb->SecondaryBuffer)[8]);

					NDAS_BUGON( cdvdCapabilitiesPage->PageCode == MODE_PAGE_CAPABILITIES );

					//cdvdCapabilitiesPage->CDRWrite = 0;
					//cdvdCapabilitiesPage->CDEWrite = 0;
					//cdvdCapabilitiesPage->TestWrite = 0;
					cdvdCapabilitiesPage->DVDRWrite = 0;
					cdvdCapabilitiesPage->DVDRAMWrite = 0;
				}

				if (Ccb->PacketCdb.MODE_SENSE10.PageCode == MODE_SENSE_RETURN_ALL) {

					PMODE_PARAMETER_HEADER10	modeParameterHeader = (PMODE_PARAMETER_HEADER10)(Ccb->SecondaryBuffer);
					UCHAR						pageCode;
					LONG						pageLength;
					USHORT						modeDataLength;

					modeDataLength = modeParameterHeader->ModeDataLength[0] << 8;
					modeDataLength += modeParameterHeader->ModeDataLength[1];

					NDAS_BUGON( modeDataLength <= MAX_PIO_LENGTH_4096 );

					pageLength = 8;

					while ((pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0)) {

						pageCode = ((PUINT8)(Ccb->SecondaryBuffer))[pageLength] & 0x3F;

						if (pageCode == MODE_PAGE_CAPABILITIES) { // 0x2A

							PCDVD_CAPABILITIES_PAGE	cdvdCapabilitiesPage;

							cdvdCapabilitiesPage = (PCDVD_CAPABILITIES_PAGE)(&((PUINT8)Ccb->SecondaryBuffer)[pageLength]);

							NDAS_BUGON( cdvdCapabilitiesPage->PageCode == MODE_PAGE_CAPABILITIES );

							DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("change capacity\n") );

							//cdvdCapabilitiesPage->CDRWrite = 0;
							//cdvdCapabilitiesPage->CDEWrite = 0;
							//cdvdCapabilitiesPage->TestWrite = 0;
							cdvdCapabilitiesPage->DVDRWrite = 0;
							cdvdCapabilitiesPage->DVDRAMWrite = 0;

							break;
						}

						if (((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1] == 0) {
							
							NDAS_BUGON( FALSE );
							break;
						}

						pageLength += ((PUINT8)Ccb->SecondaryBuffer)[pageLength + 1];
						pageLength += 2;
					}

					NDAS_BUGON( (pageLength - 8) < (modeDataLength <= MAX_PIO_LENGTH_4096 ? modeDataLength : 0) );
				}
			//}

#endif

			if (Ccb->Cdb[0] == SCSIOP_MODE_SENSE) {

				((PUINT8)Ccb->DataBuffer)[0] = ((PUINT8)Ccb->SecondaryBuffer)[1];
				((PUINT8)Ccb->DataBuffer)[1] = ((PUINT8)Ccb->SecondaryBuffer)[2];
				((PUINT8)Ccb->DataBuffer)[2] = ((PUINT8)Ccb->SecondaryBuffer)[3];
				((PUINT8)Ccb->DataBuffer)[3] = ((PUINT8)Ccb->SecondaryBuffer)[7];
				
				RtlCopyMemory( ((PUINT8)Ccb->DataBuffer) + 4, 
								((PUINT8)Ccb->SecondaryBuffer) + 8, 
								(Ccb->DataBufferLength - 4) < (Ccb->SecondaryBufferLength - 8) ?
								(Ccb->DataBufferLength - 4) : (Ccb->SecondaryBufferLength - 8) );
			
			} else {

				RtlCopyMemory( Ccb->DataBuffer, 
							   Ccb->SecondaryBuffer,
							   Ccb->DataBufferLength < Ccb->SecondaryBufferLength ?
							   Ccb->DataBufferLength : Ccb->SecondaryBufferLength );
			}

			break;
		}

		case SCSIOP_CLOSE_TRACK_SESSION: {	//0x5B

			break;
		}

		case SCSIOP_READ_BUFFER_CAPACITY: {	//0x5C

			break;
		}

		case SCSIOP_SEND_CUE_SHEET: {	//0x5D

			break;
		}

		case SCSIOP_REPORT_LUNS: {	//0xA0

			break;
		}

		case SCSIOP_BLANK: {	// 0xA1 SCSIOP_ATA_PASSTHROUGH12


			break;
	   }

		case SCSIOP_SEND_EVENT: {	//0x42

			break;
		}

		case SCSIOP_SEND_KEY: {	//0xA3 SCSIOP_MAINTENANCE_IN

			break;
		}

		case SCSIOP_REPORT_KEY: {	// 0xA4 SCSIOP_MAINTENANCE_OUT

			break;
		}

		case SCSIOP_READ12: { //0xA8 SCSIOP_GET_MESSAGE

			break;
		}

		case SCSIOP_WRITE12: {	//0xAA

			break;
		}

		case SCSIOP_GET_PERFORMANCE: {	//0xAC

			break;
		}

		case SCSIOP_READ_DVD_STRUCTURE: {	//0xAD

			break;
		}

		case SCSIOP_SEND_VOLUME_TAG: {	//0xB6

			break;
		}

		case SCSIOP_SET_CD_SPEED: {	//0xBB

			break;
		}

		case SCSIOP_READ_CD: { // 0xBE SCSIOP_VOLUME_SET_IN		

			break;
		}

		case 0xDF: {

			break;
		}

		case SCSIOP_DENON_READ_TOC: {	// 0xE9

			break;
		}

		case 0xED: {

			break;
		}

		case 0xF0: {

			break;
		}

		case 0xF9: {

			break;
		}

		case 0xFA: {

			break;
		}

		case 0xFC: {

			break;
		}

		case 0xFD: {

			break;
		}

		case 0xFE: {

			break;
		}

		case 0xFF: {

			break;
		}

		default: {
	
			NDAS_BUGON( NDAS_BUGON_PAKCET_TEST );

			break;
		}
		}
	}

ErrorOut:

	if ((Ccb->Cdb[0] == SCSIOP_INQUIRY)				|| 
		(Ccb->Cdb[0] == SCSIOP_START_STOP_UNIT)		|| 
		(Ccb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE)	|| 
		(Ccb->Cdb[0] == SCSIOP_SEEK)) {

		LsCcbSetStatusFlag( Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE );
	}

	return status;
}

#if 0

#define MAX_WAIT_COUNT	5

NTSTATUS
LurnIdeOddExecute1 (
	IN PLURELATION_NODE		Lurn,
	IN PLURNEXT_IDE_DEVICE	IdeDisk,
	IN PCCB					Ccb
	)
{
	NTSTATUS			ntStatus;

	ULONG				NeedRepeat;
	PLANSCSI_SESSION	LSS;

	// check SCSIOP_REPORT_KEY 

	if (Ccb->Cdb[0] == SCSIOP_TEST_UNIT_READY		||
		Ccb->Cdb[0] == SCSIOP_GET_EVENT_STATUS		||
		Ccb->Cdb[0] == SCSIOP_MODE_SENSE10			||
		Ccb->Cdb[0] == SCSIOP_READ_TOC				||
		Ccb->Cdb[0] == SCSIOP_SET_CD_SPEED			||
		Ccb->Cdb[0] == SCSIOP_INQUIRY				||
		Ccb->Cdb[0] == SCSIOP_GET_CONFIGURATION		||
		Ccb->Cdb[0] == SCSIOP_READ_TRACK_INFORMATION||
		Ccb->Cdb[0] == SCSIOP_BLANK					||
		Ccb->Cdb[0] == SCSIOP_MEDIUM_REMOVAL		||
		Ccb->Cdb[0] == SCSIOP_REZERO_UNIT			||
		Ccb->Cdb[0] == SCSIOP_REPORT_LUNS			||
		Ccb->Cdb[0] == SCSIOP_READ_DISC_INFORMATION	||
		Ccb->Cdb[0] == SCSIOP_SEND_CUE_SHEET		||
		Ccb->Cdb[0] == SCSIOP_START_STOP_UNIT		||
		Ccb->Cdb[0] == SCSIOP_READ_CAPACITY			||
		Ccb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE		||
		Ccb->Cdb[0] == SCSIOP_REPORT_KEY			||
		Ccb->Cdb[0] == SCSIOP_MODE_SELECT10			||
		Ccb->Cdb[0] == SCSIOP_SEND_OPC_INFORMATION	||
		Ccb->Cdb[0] == SCSIOP_CLOSE_TRACK_SESSION	||
		Ccb->Cdb[0] == SCSIOP_STOP_PLAY_SCAN		||
		Ccb->Cdb[0] == SCSIOP_READ_BUFFER_CAPACITY	||
		Ccb->Cdb[0] == SCSIOP_WRITE					||
		Ccb->Cdb[0] == SCSIOP_SEEK6					||
		Ccb->Cdb[0] == SCSIOP_READ_DVD_STRUCTURE	||
		Ccb->Cdb[0] == SCSIOP_MODE_SENSE			||
		Ccb->Cdb[0] == SCSIOP_MODE_SELECT			||
		Ccb->Cdb[0] == SCSIOP_READ) {

		return LurnIdeOddExecute2( Lurn, IdeDisk, Ccb );
	}

	KDPrintM( DBG_LURN_ERROR, ("CMD 0x%02x\n", Ccb->Cdb[0]) );

	if (Ccb->Cdb[0] != SCSIOP_MODE_SENSE &&
		Ccb->Cdb[0] != SCSIOP_MODE_SELECT) {

		NDAS_BUGON( FALSE );
	}

	LSS = IdeDisk->LanScsiSession;

	KDPrintM( DBG_LURN_NOISE, ("Entered\n") );

	KDPrintM( DBG_LURN_NOISE, ("CMD 0x%02x\n", Ccb->Cdb[0]) );

	NeedRepeat = 0;

	// Processing Busy State

	if (IdeDisk->RegStatus & BUSY_STAT) {

		SENSE_DATA	SenseData;
		ULONG		WaitCount = 0;

		NDAS_BUGON( FALSE );

		while (++WaitCount < MAX_WAIT_COUNT) {

			KDPrintM( DBG_LURN_ERROR,
					  ("time Waiting command 0x%02x Waiting Count %d\n", IdeDisk->PrevRequest, WaitCount) );
			
			ODD_BusyWait(1);

			ntStatus = ODD_GetStatus( IdeDisk, Ccb, &SenseData, sizeof(SENSE_DATA) );

			if (IdeDisk->RegStatus & BUSY_STAT) {

				continue;
			}
		} 
	
		NDAS_BUGON( WaitCount != MAX_WAIT_COUNT );
	}

	// check Long-term operation done
	
	if ((IdeDisk->PrevRequest == SCSIOP_FORMAT_UNIT)			|| 
		(IdeDisk->PrevRequest == SCSIOP_BLANK)					|| 
		(IdeDisk->PrevRequest == SCSIOP_SYNCHRONIZE_CACHE)		|| 
		(IdeDisk->PrevRequest == SCSIOP_CLOSE_TRACK_SESSION)	|| 
		(IdeDisk->PrevRequest == SCSIOP_XDWRITE_READ)) {

		if ((Ccb->Cdb[0] != SCSIOP_TEST_UNIT_READY)		&& 
			(Ccb->Cdb[0] != SCSIOP_GET_EVENT_STATUS)	&& 
			(Ccb->Cdb[0] != SCSIOP_GET_CONFIGURATION)	&& 
			(Ccb->Cdb[0] != SCSIOP_REQUEST_SENSE)		&& 
			(Ccb->Cdb[0] != SCSIOP_READ_BUFFER_CAPACITY)) {

			SENSE_DATA		SenseData;

			ntStatus = ODD_GetStatus( IdeDisk,
									  Ccb,
									  &SenseData,
									  sizeof(SENSE_DATA) );

			if (IdeDisk->DVD_REPEAT_COUNT < MAX_REFEAT_COUNT) {

				if (IdeDisk->ODD_STATUS == NOT_READY_PRESENT) {
				
					NDAS_BUGON( FALSE );

					IdeDisk->DVD_REPEAT_COUNT ++;

					ODD_BusyWait(2);
					
					KDPrintM( DBG_LURN_ERROR,
							  ("Retry command 0x%02x Retry Count %d\n", Ccb->PKCMD[0], IdeDisk->DVD_REPEAT_COUNT) );

					//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
					
					LsCcbSetStatus( Ccb, CCB_STATUS_BUSY );
					
					return STATUS_SUCCESS;			
				}
			}

			IdeDisk->DVD_REPEAT_COUNT = 0;
		}
	}


	if (Ccb->Cdb[0] == SCSIOP_READ_CAPACITY) {

//		ODD_GetSectorCount(IdeDisk);
		ODD_GetCapacity(IdeDisk);
	}

	TranslateScsi2Packet( Ccb );
	
	ntStatus = ProcessIdePacketRequest( Lurn, IdeDisk, Ccb, &NeedRepeat );

	TranslatePacket2Scsi( IdeDisk,Ccb );

	if ((Ccb->Cdb[0] == SCSIOP_INQUIRY)				|| 
		(Ccb->Cdb[0] == SCSIOP_START_STOP_UNIT)		|| 
		(Ccb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE)	|| 
		(Ccb->Cdb[0] == SCSIOP_SEEK)) {

		LsCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
	}

	if (NeedRepeat == TRUE) {

		NDAS_BUGON( Ccb->CcbStatus == CCB_STATUS_BUSY );

		if (IdeDisk->DVD_REPEAT_COUNT) {

			NDAS_BUGON( IdeDisk->PrevRequest == Ccb->Cdb[0] );
		}

		IdeDisk->DVD_REPEAT_COUNT ++;

		KDPrintM( DBG_LURN_ERROR,
				("Retry command 0x%02x Retry Count %d\n", Ccb->PKCMD[0], IdeDisk->DVD_REPEAT_COUNT) );	
		
		ntStatus = STATUS_SUCCESS;
	
	} else {

		IdeDisk->DVD_REPEAT_COUNT = 0;
		IdeDisk->PrevRequest = Ccb->Cdb[0];
	}

	return ntStatus;
}


typedef struct _MY_START_STOP {
    UCHAR OperationCode;
    UCHAR Immediate: 1;
    UCHAR Reserved1 : 4;
    UCHAR LogicalUnitNumber : 3;
    UCHAR Reserved2[2];
    UCHAR Start : 1;
    UCHAR LoadEject : 1;
    UCHAR Reserved3 : 6;
    UCHAR Control;
} MY_START_STOP, *PMY_START_STOP;

NTSTATUS
ProcessIdePacketRequest(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeDisk,
		PCCB					Ccb,
		PULONG				NeedRepeat
						)
{

	NTSTATUS	ntStatus = STATUS_SUCCESS;
	BYTE		response;
	BYTE		Command;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	BYTE				RegError = 0;
	BYTE				RegStatus = 0;
	LARGE_INTEGER		longTimeOut;
	UCHAR				Reg[2];


	LSS = IdeDisk->LanScsiSession;
	*NeedRepeat = 0;
	Command = Ccb->PKCMD[0];

	NDAS_BUGON( Ccb->SecondaryBufferLength <= IdeDisk->MaxDataSendLength );

	KDPrintM(DBG_LURN_TRACE, ("PKCMD [0x%02x]\n", Command));

	if (1											&&
		(Command != SCSIOP_TEST_UNIT_READY)			&&
		(Command != SCSIOP_GET_EVENT_STATUS)		&&
		(Command != SCSIOP_MODE_SENSE10)			&&
		(Command != SCSIOP_SET_CD_SPEED)			&&
		(Command != SCSIOP_READ_BUFFER_CAPACITY)	&&
		(Command != SCSIOP_WRITE)					&&
		(Command != SCSIOP_READ)					&&
		1) {

		UCHAR	* buf;
		
		buf = Ccb->PKCMD;
		
		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("Ccb=%p:%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x : PKDataBufferlen (%d)\n",
					 Ccb,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],
					 Ccb->SecondaryBufferLength) );
	}

	// Pre processing
	// step 1 check if current is w processing
	if(Lurn->LurnType == LURN_CDROM)
	{
		switch(Command) {
		case SCSIOP_FORMAT_UNIT:
		case SCSIOP_WRITE:
		case SCSIOP_WRITE_VERIFY:
		case SCSIOP_WRITE12:
		case SCSIOP_MODE_SENSE10:
		case SCSIOP_READ:
		case SCSIOP_READ_DATA_BUFF:	
		case SCSIOP_READ_CD:
		case SCSIOP_READ_CD_MSF:
		case SCSIOP_READ12:
			{
				LARGE_INTEGER		CurrentTime;
				KeQuerySystemTime(&CurrentTime);
				IdeDisk->DVD_Acess_Time.QuadPart = CurrentTime.QuadPart;
				IdeDisk->DVD_STATUS = DVD_W_OP;
			}
			break;
		default:
			{
				LARGE_INTEGER		CurrentTime;
				UINT32				Delta = 15;
				KeQuerySystemTime(&CurrentTime);
				if( (IdeDisk->DVD_STATUS == DVD_W_OP) 
					&& ((UINT32)((CurrentTime.QuadPart - IdeDisk->DVD_Acess_Time.QuadPart)/ 10000000) > Delta) )
				{
					IdeDisk->DVD_STATUS = DVD_NO_W_OP;
				}

			}
			break;
		}
	}



	//	step 2	parameter check and rw proctect
	//
	//	check parameter and adjust
	//
	//		Our device have a problem with some device 
	//					using DMA/PIO operation. 
	//		Some program generates too big parameter 
	//					
	switch(Command)
	{


//
//		change to block operation (immediate disable)
//
	case SCSIOP_CLOSE_TRACK_SESSION:	//	CLOSE TRACK/SESSION/RZONE
		{
//			Ccb->PKCMD[1] = 0;
			Ccb->SecondaryBufferLength = 0;
		}
		break;
	case SCSIOP_SYNCHRONIZE_CACHE:
		{
//			Ccb->PKCMD[1] = Ccb->PKCMD[1] & 0xFD;
			Ccb->SecondaryBufferLength = 0;
		}
		break;
	case SCSIOP_BLANK:	//	BLANK
		{
			//
			// If not give Immediate flag, 
			// the SW9583-C device reset in erase process of B's recorder gold7.
			//
//			Ccb->PKCMD[1] = Ccb->PKCMD[1] & 0xEF;
			Ccb->SecondaryBufferLength = 0;
			
			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0);
					LsCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				return STATUS_SUCCESS;
				
			}

		}
		break;
//
//		change to block operation (immediate disable) end
//
	case SCSIOP_FORMAT_UNIT:
		{


			UCHAR * buf = Ccb->SecondaryBuffer;
			BYTE	FormatType = 0;
			KDPrintM(DBG_LURN_ERROR, ("[SCSIOP FORMAT UNIT DATA]"
					"%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x : SecondaryBufferlen (%d)\n",
					buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],Ccb->SecondaryBufferLength));

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0);
					LsCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				return STATUS_SUCCESS;
				
			}
/*
			if((buf[8] == 0)
				&& (IdeDisk->DVD_MEDIA_TYPE == 0x12))
				//&&(IdeDisk->PrevRequest != 0x23))
			{
					if( (Ccb->SenseBuffer)
						&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
					{
							LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
							return STATUS_SUCCESS;
					}
			}
*/
/*	
			if(IdeDisk->DVD_TYPE == LOGITEC)
			{
//
//
//			Read format capacity return
//				[SCSIOP FORMAT UNIT DATA](0)[0x00], (1)[0x82], (2)[0x00], (3)[0x08], (4)[0x00], (5)[0x23], (6)[0x05], (7)[0x40], (8)[0x98], (9)[0x00], (10)[0x00], (11)[0x00]
//			and Format use this parameter
//			IO-data model SW-9583_C support but GSA-4082B not support
//
//
				FormatType = (buf[8] >> 2);
				KDPrintM(DBG_LURN_ERROR, ("FormatType 0x%02x\n",FormatType));

				switch(FormatType) {
				case 0x0:
				case 0x01:
				case 0x04:
				case 0x05:
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
				case 0x14:
				case 0x15:
				case 0x20:
					break;
				default:
					{
						if( (Ccb->SenseBuffer)
							&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
						{
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
								LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);	
								return STATUS_SUCCESS;
						}

						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;

					}
					break;
				}


			}

*/
		}
		break;

	case SCSIOP_TEST_UNIT_READY:
	case SCSIOP_START_STOP_UNIT:
		{
			PMY_START_STOP		StartStop = (PMY_START_STOP)(Ccb->PKCMD);
			Ccb->SecondaryBufferLength = 0;
			if(
				(!(Lurn->AccessRight & GENERIC_WRITE))
				&& (StartStop->Start == 0)
				&& (StartStop->LoadEject == 1) 
				)
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0);
					LsCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				return STATUS_SUCCESS;
				
			}
		}
		break;

	case SCSIOP_INQUIRY:
		{
			BYTE transferLen = Ccb->PKCMD[4];

			if((ULONG)transferLen != Ccb->SecondaryBufferLength)
			{

				if(Ccb->SecondaryBufferLength < (ULONG)transferLen)
				{
					transferLen = (BYTE)Ccb->SecondaryBufferLength;
					Ccb->PKCMD[4] = (transferLen & 0xff);

				}else if(Ccb->SecondaryBufferLength > (ULONG)transferLen)
				{
					Ccb->PKCMD[4] = (transferLen & 0xff);
					Ccb->SecondaryBufferLength = transferLen;
				}
			}

		}
		break;

	case SCSIOP_READ_DVD_STRUCTURE:
		{
			USHORT transferLen = 0;
			transferLen |= (Ccb->PKCMD[8] << 8);
			transferLen |= (Ccb->PKCMD[9]);


			if(IdeDisk->DVD_TYPE == LOGITEC)
			{
				if(Ccb->PKCMD[7] == 0x0C){

					if( (Ccb->SenseBuffer)
						&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
					{
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);	
						return STATUS_SUCCESS;
					}

					LsCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
					return STATUS_SUCCESS;

				}
			}

			if(transferLen >MAX_PIO_LENGTH_4096)
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
				KDPrintM(DBG_LURN_NOISE, ("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
				transferLen = MAX_PIO_LENGTH_4096;
				Ccb->PKCMD[8] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[9] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
			}


			if(transferLen != Ccb->SecondaryBufferLength)
			{

				if(Ccb->SecondaryBufferLength < transferLen)
				{
					transferLen = (USHORT)Ccb->SecondaryBufferLength;
					Ccb->PKCMD[8] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[9] = (transferLen & 0xff);

				}else if(Ccb->SecondaryBufferLength > transferLen)
				{
					Ccb->PKCMD[8] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[9] = (transferLen & 0xff);
					Ccb->SecondaryBufferLength = transferLen;
				}
			}
		}
		break;

	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case SCSIOP_WRITE12:
		{

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_ERROR, ("Command (0x%02x) Error Read only Access !!!\n",Command));
				if(!LsCcbIsFlagOn(Ccb, CCB_FLAG_ALLOW_WRITE_IN_RO_ACCESS)) {
					ASSERT(FALSE);
					LsCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}
			}


#if	__DVD_ENCRYPT_CONTENT__
		if(Lurn->LurnType == LURN_CDROM)
		{
			PBYTE	buf = Ccb->PKDataBuffer;
			ULONG	length = Ccb->PKDataBufferLength;
			Encrypt32SP(
					buf,
					length,
					&(IdeDisk->CntEcr_IR[0])
					);
		}
#endif //#ifdef	__DVD_ENCRYPT_CONTENT__
		}
		break;
	case SCSIOP_READ_DISK_INFORMATION:
		{
			USHORT transferLen = 0;
			transferLen |= (Ccb->PKCMD[7] << 8);
			transferLen |= (Ccb->PKCMD[8]);

			if(transferLen != Ccb->SecondaryBufferLength)
			{

				if(Ccb->SecondaryBufferLength < transferLen)
				{
					transferLen = (USHORT)Ccb->SecondaryBufferLength;
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);

				}else if(Ccb->SecondaryBufferLength > transferLen)
				{
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);
					Ccb->SecondaryBufferLength = transferLen;
				}
			}
		}
		break;

	case SCSIOP_MODE_SELECT10:
		{

			PBYTE	buf = Ccb->SecondaryBuffer;
			int		length = Ccb->SecondaryBufferLength/8;
			int i = 0;

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Error Read only Access !!!\n",Command));

				if(buf[8] == 0x05)
				{
					if( (Ccb->SenseBuffer)
						&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
					{
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, 0x26, 0);
						LsCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;
					}

					LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}
			}

			for(i=0; i<length; i++)
				KDPrintM(DBG_LURN_ERROR, ("[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x)\n",
					i*8,buf[i*8],i*8+1,buf[i*8+1],i*8+2,buf[i*8+2],i*8+3,buf[i*8+3],i*8+4,buf[i*8+4],i*8+5,buf[i*8+5],i*8+6,buf[i*8+6],i*8+7,buf[i*8+7]));

/*
			{
				// Length check
				ULONG transferLen = 0;
				ULONG Datalen = 0;
				ULONG MinimumLen = 0;
				BYTE  PageCode;
				BYTE  PageCodeLen = 0;
				buf = Ccb->PKDataBuffer;

				transferLen |= (Ccb->PKCMD[7] << 8);
				transferLen |= (Ccb->PKCMD[8]);

				PageCode = (Ccb->PKCMD[2] & 0x3F);

				switch(PageCode) {
				case 0x01:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x05:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x0E:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x1A:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x1C:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x1D:
					PageCodeLen = buf[9];
					Datalen = (0x8 + PageCodeLen + 0x02);
					break;
				case 0x2A:
					PageCodeLen = buf[9];
					MinimumLen = (0x8 + PageCodeLen + 0x02);
					break;
				default:
					Datalen = transferLen;
					break;
				}



				if(transferLen < Datalen)
				{
					MinimumLen = transferLen;
				}else{
					MinimumLen = Datalen;
				}


				if(MinimumLen >MAX_PIO_LENGTH_4096)
				{
					KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, MinimumLen));
					KDPrintM(DBG_LURN_NOISE, ("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
					MinimumLen = MAX_PIO_LENGTH_4096;
					Ccb->PKCMD[7] = (BYTE)((MinimumLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (BYTE)(MinimumLen & 0xff);
					if(PageCodeLen != 0)
					{
						buf[0] = (BYTE)(((MinimumLen -2) >> 8) & 0xff);
						buf[1] = (BYTE)((MinimumLen -2) & 0xff);
						buf[9] = (BYTE)(MinimumLen - (10));
					}
					Ccb->PKDataBufferLength = MinimumLen;
					KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, MinimumLen));
				}


				if(MinimumLen != Ccb->PKDataBufferLength)
				{

					if(Ccb->PKDataBufferLength < MinimumLen)
					{
						MinimumLen = Ccb->PKDataBufferLength;
						Ccb->PKCMD[7] = (BYTE)((MinimumLen >> 8) & 0xff);
						Ccb->PKCMD[8] = (BYTE)(MinimumLen & 0xff);

						if(PageCodeLen != 0)
						{
							buf[0] = (BYTE)(((MinimumLen -2) >> 8) & 0xff);
							buf[1] = (BYTE)((MinimumLen -2) & 0xff);
							buf[9] = (BYTE)(MinimumLen - (10));
						}

					}else if(Ccb->PKDataBufferLength > MinimumLen)
					{
						Ccb->PKCMD[7] = (BYTE)((MinimumLen >> 8) & 0xff);
						Ccb->PKCMD[8] = (BYTE)(MinimumLen & 0xff);
						if(PageCodeLen != 0)
						{
							buf[0] = (BYTE)(((MinimumLen -2) >> 8) & 0xff);
							buf[1] = (BYTE)((MinimumLen -2) & 0xff);
							buf[9] = (BYTE)(MinimumLen - (10));
						}
						Ccb->PKDataBufferLength = MinimumLen;
					}
				}
			}

*/



		}
//		break;
	case SCSIOP_MODE_SENSE10:
	case SCSIOP_GET_CONFIGURATION:	// Get Configuration
	case SCSIOP_READ_TOC:
	case SCSIOP_READ_TRACK_INFORMATION:
	case SCSIOP_READ_SUB_CHANNEL: {


		USHORT transferLen = 0;
		
		transferLen |= (Ccb->PKCMD[7] << 8);
		transferLen |= (Ccb->PKCMD[8]);


		if (Command == SCSIOP_READ_TOC) {

			if ((IdeDisk->DVD_TYPE == LOGITEC) && (Ccb->PKCMD[9] & 0x80)) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("[READ_TOC CHANGE]Ccb->PKCMD[9] = (Ccb->PKCMD[9] & 0x3F)\n") );
					
				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);	

				return STATUS_SUCCESS;
			}
		}

		if (transferLen > MAX_PIO_LENGTH_4096) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen) );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("PKCMD 0x%02x : buffer size %d\n", Command, transferLen) );
			
			transferLen = MAX_PIO_LENGTH_4096;
			Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
			Ccb->PKCMD[8] = (transferLen & 0xff);
			Ccb->SecondaryBufferLength = transferLen;
		}

		if (transferLen != Ccb->SecondaryBufferLength) {

			if (Ccb->SecondaryBufferLength < transferLen) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("Set new transfer length into the scsi command:%s. From %d to %d.\n",
							 CdbOperationString(Command),
							 transferLen,
							 Ccb->SecondaryBufferLength) );

				transferLen = (USHORT)Ccb->SecondaryBufferLength;
				Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[8] = (transferLen & 0xff);

			} else if (Ccb->SecondaryBufferLength > transferLen) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						  ("Set new transfer length into the scsi command:%s. From %d to %d.\n",
							CdbOperationString(Command),
							Ccb->SecondaryBufferLength,
							transferLen) );

				Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[8] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
			}
		}

		break;
	}

	case SCSIOP_READ_CAPACITY:
		if(Ccb->SecondaryBufferLength > 8) Ccb->SecondaryBufferLength = 8;
		break;

	case SCSIOP_GET_EVENT_STATUS:
		{
			UINT16 transferLen = 0;
			transferLen |= (Ccb->PKCMD[7] << 8);
			transferLen |= (Ccb->PKCMD[8]);


			if(transferLen > 256)
			{
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
				KDPrintM(DBG_LURN_NOISE, ("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
				transferLen = 256;
				Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[8] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
				KDPrintM(DBG_LURN_INFO, ("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
			}


			if(transferLen != Ccb->SecondaryBufferLength)
			{

				if(Ccb->SecondaryBufferLength < transferLen)
				{
					transferLen = (USHORT)Ccb->SecondaryBufferLength;
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);

				}else if(Ccb->SecondaryBufferLength > transferLen)
				{
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);
					Ccb->SecondaryBufferLength = transferLen;
				}
			}

		}

		break;

#if __BSRECORDER_HACK__
	case SCSIOP_READ:
		{
			if(IdeDisk->DVD_TYPE == LOGITEC)
			{
				if(IdeDisk->DataSectorSize > 0)
				{
					USHORT SectorCount = 0;
					ULONG  TransferSize = 0;

					SectorCount |= (Ccb->PKCMD[7] << 8);
					SectorCount |= Ccb->PKCMD[8];

					TransferSize = SectorCount *(IdeDisk->DataSectorSize);
					if(Ccb->SecondaryBufferLength > TransferSize)
					{
						KDPrintM(DBG_LURN_ERROR, ("[SCSIOP_READ] changed size %d\n",TransferSize));
						Ccb->SecondaryBufferLength = TransferSize;
					}

				}
			}
		}
		break;

	case SCSIOP_READ12:
		{
			if(IdeDisk->DVD_TYPE == LOGITEC)
			{
				if(IdeDisk->DataSectorSize > 0)
				{
					ULONG	SectorCount = 0;
					ULONG  TransferSize = 0;

					SectorCount |= (Ccb->PKCMD[6] << 24);
					SectorCount |= (Ccb->PKCMD[7] << 16);
					SectorCount |= (Ccb->PKCMD[8] << 8);
					SectorCount |= Ccb->PKCMD[9];

					TransferSize = SectorCount *(IdeDisk->DataSectorSize);
					if(Ccb->SecondaryBufferLength > TransferSize)
					{
						KDPrintM(DBG_LURN_ERROR, ("[SCSIOP_READ10] changed size %d\n",TransferSize));
						Ccb->SecondaryBufferLength = TransferSize;
					}

				}
			}
		}
		break;
#endif // __BSRECORDER_HACK__

	case SCSIOP_REPORT_LUNS:
	case SCSIOP_SET_CD_SPEED:
	case SCSIOP_MEDIUM_REMOVAL:
	case SCSIOP_REZERO_UNIT:
	case SCSIOP_SEND_CUE_SHEET:
	case SCSIOP_READ_BUFFER_CAPACITY:

		break;

	default:

		;
	}









	// Step 3 Processing ATAPI request
	//		Some operation must redo because previous operation takes long time 
	//		 --> NOT_READY_PRESENT ODD_STATUS
	//
	switch(Command) {

	case SCSIOP_REPORT_LUNS:
		{


			PLUN_LIST	lunlist;
			UINT32		luncount = 1;
			UINT32		lunsize = 0;

			lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[0] << 24);
			lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[1] << 16);
			lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[2] << 8);
			lunsize |= (((PCDB)Ccb->Cdb)->REPORT_LUNS.AllocationLength[3] );

			KDPrintM(DBG_LURN_INFO, ("size of allocated buffer %d\n",lunsize));

			luncount = luncount * 8;

			lunlist = (PLUN_LIST)Ccb->SecondaryBuffer;
			lunlist->LunListLength[0] = ((luncount >> 24)  & 0xff);
			lunlist->LunListLength[1] = ((luncount >> 16)  & 0xff);
			lunlist->LunListLength[2] = ((luncount >> 8) & 0xff);
			lunlist->LunListLength[3] = ((luncount >> 0) & 0xff); 

			RtlZeroMemory(lunlist->Lun,8);
			LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			return STATUS_SUCCESS;


		}
		break;


	case SCSIOP_READ_CAPACITY: {

		longTimeOut.QuadPart = -30 * NANO100_PER_SEC;
		LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &longTimeOut);

		ntStatus = LspPacketRequest( LSS, &PduDesc, &response, Reg );	

		IdeDisk->RegError = Reg[0];
		IdeDisk->RegStatus = Reg[1];

		if (ntStatus != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					  ("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
						IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS) );
		}

		if (ntStatus != STATUS_SUCCESS) {

			if (ntStatus == STATUS_REQUEST_ABORTED) {
			
				NDAS_BUGON( FALSE );

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						    ("Command (0x%02x) Error Request Aborted!!\n",Command) );
					
				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				IdeDisk->ODD_STATUS = NON_STATE;
				LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);

				return STATUS_REQUEST_ABORTED;
			}

			NDAS_BUGON( ntStatus == STATUS_PORT_DISCONNECTED );
			
			LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);

			return ntStatus;

		} else {

			RegError = IdeDisk->RegError;
			RegStatus = IdeDisk->RegStatus;

			if (response != LANSCSI_RESPONSE_SUCCESS) {
	
				NDAS_BUGON( response == LANSCSI_RESPONSE_T_COMMAND_FAILED );

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						   ("Error response Command 0x%02x : response 0x%x\n", Command,response) );
			}
			
			switch (response) {
				
			case LANSCSI_RESPONSE_SUCCESS: {

				LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				return STATUS_SUCCESS;
			}

			break;
			
			case LANSCSI_RESPONSE_T_NOT_EXIST:

				LsCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
				return STATUS_SUCCESS;
				break;

			case LANSCSI_RESPONSE_T_BAD_COMMAND:

				LsCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
				return STATUS_SUCCESS;
				break;

			case LANSCSI_RESPONSE_T_BROKEN_DATA:
			case LANSCSI_RESPONSE_T_COMMAND_FAILED:{

				if ((Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) && (Ccb->SenseBuffer)) {

					PSENSE_DATA pSenseData;
					NTSTATUS status;

					status = ODD_GetStatus( IdeDisk,
											Ccb,
											(PSENSE_DATA)Ccb->SenseBuffer,
											Ccb->SenseDataLength );

					if (status != STATUS_SUCCESS) {

						NDAS_BUGON( ntStatus == STATUS_PORT_DISCONNECTED );

						if (status == STATUS_REQUEST_ABORTED) {

							KDPrintM(DBG_LURN_ERROR, ("Command (0x%02x) Error Request Aborted!!\n",Command));
							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
							IdeDisk->ODD_STATUS = NON_STATE;
							LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
							return STATUS_SUCCESS;
						}
						
						KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
						return status;
					}

					pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);

					DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
							   ("PKCMD (0x%02x), IdeDisk->ODD_STATUS = %x, RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
								Ccb->PKCMD[0], IdeDisk->ODD_STATUS, RegStatus, RegError,pSenseData->ErrorCode, pSenseData->SenseKey,
								pSenseData->AdditionalSenseCode, pSenseData->AdditionalSenseCodeQualifier) );


					if (IdeDisk->ODD_STATUS == RESET_POWERUP) {

						//NDAS_BUGON( FALSE );

						DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
									("POWER RESET 4 CMD %02x\n",Ccb->PKCMD[0]));

						*NeedRepeat = TRUE;
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
						LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
						return STATUS_SUCCESS;
					}
							
					if (IdeDisk->ODD_STATUS == NOT_READY_MEDIA) {

						DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
									("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]) );

						*NeedRepeat = TRUE;
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
						RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
						LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
						return STATUS_SUCCESS;
					}

					if (IdeDisk->ODD_STATUS == NOT_READY_PRESENT) {

						if (IdeDisk->DVD_MEDIA_TYPE != 0x12) {

							LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
						
						} else {
							
							LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
						}
						
						return STATUS_SUCCESS;
					}

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
							
					if (IdeDisk->DVD_MEDIA_TYPE != 0x12) {

							LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
					
					} else {
					
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					}
							
					return STATUS_SUCCESS;
				}

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
				return STATUS_SUCCESS;
			}

			default:

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
				return STATUS_SUCCESS;
				break;
			}
		}

		break;
	}

	case SCSIOP_SEND_CUE_SHEET: // SEND_CUE_SHEET
	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case SCSIOP_WRITE12:	// WRITE12
	case SCSIOP_READ_DISC_INFORMATION:
	case SCSIOP_READ_TRACK_INFORMATION:
		{
			PSCSI_REQUEST_BLOCK Srb;

			Srb = Ccb->Srb;

			longTimeOut.QuadPart = -30 * NANO100_PER_SEC;
			LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &longTimeOut);

			ntStatus = LspPacketRequest(
								LSS,
								&PduDesc,
								&response,
								Reg
								);

			IdeDisk->RegError = Reg[0];
			IdeDisk->RegStatus = Reg[1];

			if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, 
					("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
							IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
			}

			if(!NT_SUCCESS(ntStatus))
			{
				if(ntStatus == STATUS_REQUEST_ABORTED){
					KDPrintM(DBG_LURN_ERROR, ("Command (0x%02x) Error Request Aborted!!\n",Command));					
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					IdeDisk->ODD_STATUS = NON_STATE;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					return STATUS_REQUEST_ABORTED;
				}
				KDPrintM(DBG_LURN_ERROR, ("Communication error! NTSTATUS:%08lx\n",ntStatus));
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
				return ntStatus;
			}else{

				RegError = IdeDisk->RegError;
				RegStatus = IdeDisk->RegStatus;

				if(response != LANSCSI_RESPONSE_SUCCESS) {
					KDPrintM(DBG_LURN_ERROR, 
						("Error response Command 0x%02x : response %x\n", Command,response));
				}

				switch(response) {
				case LANSCSI_RESPONSE_SUCCESS:
					LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_NOT_EXIST:
					LsCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BAD_COMMAND:
					LsCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BROKEN_DATA:
				case LANSCSI_RESPONSE_T_COMMAND_FAILED:
					{

						if( (Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))
							&& (Ccb->SenseBuffer)
							)
						{
							PSENSE_DATA pSenseData;

							NTSTATUS status;

							if(
								(Ccb->PKCMD[0] == 0x5d )
								|| (Ccb->PKCMD[0] == SCSIOP_WRITE)
								|| (Ccb->PKCMD[0] == SCSIOP_WRITE_VERIFY)
								|| (Ccb->PKCMD[0] == 0xaa)
								)
							{
								if(RegStatus & BUSY_STAT)
								{
									KDPrintM(DBG_LURN_ERROR, ("Waiting for a while!!\n",Command));
									ODD_BusyWait(2
										);
								}
							}

							status = ODD_GetStatus(
											IdeDisk,
											Ccb,
											(PSENSE_DATA)Ccb->SenseBuffer,
											Ccb->SenseDataLength
											);

							if(!NT_SUCCESS(status))
							{
								if(status == STATUS_REQUEST_ABORTED){
//
//									Some Problem with device : 
//											device don't reply with REQUEST SENSE Command!!!
//											so change ABORT to RETRY!
//
									KDPrintM(DBG_LURN_ERROR, ("Command (0x%02x) Error Request Aborted!!\n",Command));
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
									IdeDisk->ODD_STATUS = NON_STATE;
									LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
									return STATUS_SUCCESS;
								}
								KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
								LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
								return status;
							}

							pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);

							KDPrintM(DBG_LURN_INFO, ("ProcessIdePacketRequest: PKCMD (0x%02x), RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
								Ccb->PKCMD[0], RegStatus, RegError,pSenseData->ErrorCode, pSenseData->SenseKey,
								pSenseData->AdditionalSenseCode, pSenseData->AdditionalSenseCodeQualifier));


							if( (RegStatus & ERR_STAT)
								&& (IdeDisk->ODD_STATUS == NOT_READY_PRESENT) )
							{
								if(
									((IdeDisk->DVD_TYPE == LOGITEC)
									&&(IdeDisk->RegError & ABRT_ERR))
									|| (IdeDisk->DVD_TYPE == IO_DATA)
									|| (IdeDisk->DVD_TYPE == IO_DATA_9573)
									)

								{

									*NeedRepeat = TRUE;
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);

									if(Srb){
										KDPrintM(DBG_LURN_ERROR, ("Srb Timeout Value %d\n",Srb->TimeOutValue));
										if(Srb->TimeOutValue > 10)
										{
											if(Srb->TimeOutValue > 30)
												ODD_BusyWait((20));
											else ODD_BusyWait((Srb->TimeOutValue -2));
										}else{
											ODD_BusyWait(8);
										}

									}else{
										ODD_BusyWait(8);
									}

									return STATUS_SUCCESS;
								}
							}

							if(IdeDisk->ODD_STATUS == RESET_POWERUP)
							{
								KDPrintM(DBG_LURN_INFO, ("POWER RESET 3 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								pSenseData->ErrorCode = 0x0;
								LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
								LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
							    return STATUS_SUCCESS;

							}else if(IdeDisk->ODD_STATUS == NOT_READY_MEDIA)
							{
								KDPrintM(DBG_LURN_INFO, ("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
								LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								return STATUS_SUCCESS;

							}else if ((pSenseData->SenseKey == 0x05)
											&& (pSenseData->AdditionalSenseCode == 0x21)
											&& ( pSenseData->AdditionalSenseCodeQualifier == 0x02)
											)
							{
								LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								return STATUS_SUCCESS;								
							}

							LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
							return STATUS_SUCCESS;		

						}
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
						return STATUS_SUCCESS;
					}
				default:
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;
					break;
				}
			}
		}
		break;

	default: {

		PSCSI_REQUEST_BLOCK Srb;

		Srb = Ccb->Srb;
	
		if ((Command == 0xA1) || 
			(Command ==SCSIOP_SYNCHRONIZE_CACHE) || 
			(Command ==SCSIOP_FORMAT_UNIT ) || 
			(Command == 0x53) 
			/*(Command == 0x5B)*/ ) {

			longTimeOut.QuadPart = -600 * NANO100_PER_SEC; //10 minute
		
		} else if(Command == 0x5B) {

			longTimeOut.QuadPart = -600 * NANO100_PER_SEC;
		
		} else {
			longTimeOut.QuadPart = -30 * NANO100_PER_SEC;
		}

		LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &longTimeOut);
			
		//NDAS_BUGON( PduDesc.TimeOut->QuadPart < 0 );

		ntStatus = LspPacketRequest( LSS, &PduDesc, &response, Reg );

		IdeDisk->RegError = Reg[0];
		IdeDisk->RegStatus = Reg[1];

		if (ntStatus != STATUS_SUCCESS || response != LANSCSI_RESPONSE_SUCCESS) {
		
			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					  ("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
						IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS) );
		}

//			ASSERT(Ccb->PKCMD[0] != SCSIOP_READ_CD);
/*			ASSERT(Ccb->PKCMD[0] != 0x46); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_INQUIRY); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_READ_SUB_CHANNEL); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_READ_TOC); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_MODE_SENSE10); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_MODE_SELECT10); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_READ_DISK_INFORMATION); */
/*			ASSERT(Ccb->PKCMD[0] != SCSIOP_READ_TRACK_INFORMATION); */

		if (ntStatus != STATUS_SUCCESS) {

			if (ntStatus == STATUS_REQUEST_ABORTED) {
			
				NDAS_BUGON( FALSE );

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						    ("Command (0x%02x) Error Request Aborted!!\n",Command) );
					
				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				IdeDisk->ODD_STATUS = NON_STATE;
				LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);

				return STATUS_REQUEST_ABORTED;
			}

			NDAS_BUGON( ntStatus == STATUS_PORT_DISCONNECTED );
			
			LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);

			return ntStatus;

		} else {

			RegError = IdeDisk->RegError;
			RegStatus = IdeDisk->RegStatus;

			if (response != LANSCSI_RESPONSE_SUCCESS) {
	
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						   ("Error response Command 0x%02x : response 0x%x\n", Command,response) );

				NDAS_BUGON( response == LANSCSI_RESPONSE_T_COMMAND_FAILED );
			}
			
			switch(response) {

			case LANSCSI_RESPONSE_SUCCESS: {

				if (Command == SCSIOP_MODE_SENSE10) {

					//
					//
					//		Write Protect for MO 
					//			
					//			this code is unstable only for MO model MOS3400E
					//			hacking MO mode sense of Vender specific code page

					
					if ((!(Lurn->AccessRight & GENERIC_WRITE)) && (Lurn->LurnType == LURN_OPTICAL_MEMORY)) {

						PBYTE buf;
						
						if (Ccb->SecondaryBufferLength > 8) {

							buf = Ccb->SecondaryBuffer;
							if ((buf[0]  == 0x0) && (buf[1] == 0x6f) && (buf[2] == 0x03)) {

								buf[3] = 0x80;
							}
						}
					}
				}
				
				LsCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
				return STATUS_SUCCESS;
			
				break;
			}

			case LANSCSI_RESPONSE_T_NOT_EXIST:

				LsCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);

				KDPrintM(DBG_LURN_TRACE, 
						 ("LANSCSI_RESPONSE_T_NOT_EXIST response Command 0x%02x : response %x\n", Command,response));
					
				return STATUS_SUCCESS;
				
				break;

			case LANSCSI_RESPONSE_T_BAD_COMMAND:

				LsCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);

				KDPrintM(DBG_LURN_TRACE, 
						("LANSCSI_RESPONSE_T_BAD_COMMAND response Command 0x%02x : response %x\n", Command,response));

				return STATUS_SUCCESS;
				break;

			case LANSCSI_RESPONSE_T_BROKEN_DATA:
			case LANSCSI_RESPONSE_T_COMMAND_FAILED:{

				if ((Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) && (Ccb->SenseBuffer)) {

					PSENSE_DATA pSenseData;
					NTSTATUS status;

					status = ODD_GetStatus( IdeDisk,
											Ccb,
											(PSENSE_DATA)Ccb->SenseBuffer,
											Ccb->SenseDataLength );

					if (status != STATUS_SUCCESS) {

						NDAS_BUGON( ntStatus == STATUS_PORT_DISCONNECTED );

						if (status == STATUS_REQUEST_ABORTED) {

							KDPrintM(DBG_LURN_ERROR, ("Command (0x%02x) Error Request Aborted!!\n",Command));
							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
							IdeDisk->ODD_STATUS = NON_STATE;
							LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
							return STATUS_SUCCESS;
						}
						
						KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
						return status;
					}

					pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);

					DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
							   ("PKCMD (0x%02x), IdeDisk->DVD_MEDIA_TYPE = %x, IdeDisk->ODD_STATUS = %x, RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
								Ccb->PKCMD[0], IdeDisk->DVD_MEDIA_TYPE, IdeDisk->ODD_STATUS, RegStatus, RegError,pSenseData->ErrorCode, pSenseData->SenseKey,
								pSenseData->AdditionalSenseCode, pSenseData->AdditionalSenseCodeQualifier) );

					ASSERT (!(pSenseData->SenseKey == 0x05 &&
							  pSenseData->AdditionalSenseCode == 0x1a &&
							  pSenseData->AdditionalSenseCodeQualifier == 0x00) );

					ASSERT (!(pSenseData->SenseKey == 0x05 &&
							  pSenseData->AdditionalSenseCode == 0x26 &&
							  pSenseData->AdditionalSenseCodeQualifier == 0x00) );

					if (IdeDisk->ODD_STATUS == RESET_POWERUP) {

						if (Ccb->PKCMD[0] != 0x0) {

							KDPrintM(DBG_LURN_INFO, ("POWER RESET 1 CMD %02x\n",Ccb->PKCMD[0]));
							*NeedRepeat = TRUE;
							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
							pSenseData->ErrorCode = 0x0;
							LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
							LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
							return STATUS_SUCCESS;
						
						} else {
							
							KDPrintM(DBG_LURN_INFO, ("POWER RESET 2 CMD %02x\n",Ccb->PKCMD[0]));
							pSenseData->ErrorCode = 0x0;
							LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
							LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);

							return STATUS_SUCCESS;
						}
					}

					if ((IdeDisk->ODD_STATUS == NOT_READY_MEDIA) && (Ccb->PKCMD[0] != 0x0)) {

						KDPrintM(DBG_LURN_INFO, ("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]));
						
						//*NeedRepeat = TRUE;

						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
						RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
						LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
						return STATUS_SUCCESS;
					}

					if (IdeDisk->ODD_STATUS == MEDIA_ERASE_ERROR) {

						KDPrintM(DBG_LURN_INFO, ("MEDIA_ERASE_ERROR 4 CMD %02x\n",Ccb->PKCMD[0]));
						*NeedRepeat = TRUE;
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
						RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
						LsCcbSetStatus(Ccb, CCB_STATUS_BUSY);
					}

					if ((IdeDisk->ODD_STATUS == NOT_READY_PRESENT) &&(Ccb->PKCMD[0] == 0x0)) {

						if (IdeDisk->DVD_MEDIA_TYPE != 0x12) {

							//ODD_BusyWait(4);
								
						} else {
							
							LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
							return STATUS_SUCCESS;
						}		
					}

/*
							if( (RegStatus & ERR_STAT)
								//&&( LSS->RegError & ABRT_ERR)
								&& (LSS->ODD_STATUS == NOT_READY_PRESENT)
								&& (Ccb->PKCMD[0] != 0x04)
								&& (Ccb->PKCMD[0] != 0xa1)
								&& (Ccb->PKCMD[0] != 0x35)
								&& (Ccb->PKCMD[0] != 0x00)
								&& (Ccb->PKCMD[0] != 0x4a)
								&& (Ccb->PKCMD[0] != 0x46)
								&& (Ccb->PKCMD[0] != 0x03)
								)
							{
								*NeedRepeat = 1;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								return STATUS_SUCCESS;
							}
*/

					if (IdeDisk->ODD_STATUS == INVALID_COMMAND) {

						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;
					}

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
						
					if (IdeDisk->DVD_MEDIA_TYPE != 0x12) {

						LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
					
					} else {
							
						LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					}
						
					return STATUS_SUCCESS;
				}

				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
				return STATUS_SUCCESS;
			}
				
			default:
				
				Ccb->ResidualDataLength = Ccb->DataBufferLength;
				LsCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);

				KDPrintM(DBG_LURN_TRACE, 
						("NON SPECIFIED ERROR response Command 0x%02x : response %x\n", Command,response));

				return STATUS_SUCCESS;
				break;
			}
			break;
		}
		}
	}

	return ntStatus;

}
#endif

#if 0

static 
NTSTATUS
LurnIdeDVDSyncache(
				PLURELATION_NODE		Lurn,
				PLURNEXT_IDE_DEVICE		IdeDisk
				)
{
	NTSTATUS		ntSatus;
	CCB				tempCCB;
	SENSE_DATA		tempSenseBuffer;

	RtlZeroMemory(&tempCCB, sizeof(CCB));
	RtlZeroMemory(&tempSenseBuffer, sizeof(SENSE_DATA));
	((PCDB)&tempCCB.Cdb)->SYNCHRONIZE_CACHE10.OperationCode = SCSIOP_SYNCHRONIZE_CACHE;
	((PCDB)&tempCCB.Cdb)->SYNCHRONIZE_CACHE10.Lun = IdeDisk->LuHwData.LanscsiLU;

	tempCCB.DataBuffer = NULL;
	tempCCB.DataBufferLength = 0;
	tempCCB.SenseBuffer = &tempSenseBuffer;
	tempCCB.SenseDataLength = sizeof(SENSE_DATA);


	KDPrintM(DBG_LURN_INFO, ("Reconnection : Send Syncronize cache command for flush controler buffer\n"));
	ntSatus = LurnIdeOddExecute(Lurn, Lurn->LurnExtension,&tempCCB);
	return ntSatus;	

}

#endif


NTSTATUS
LurnIdeOddNoop (
	PLURELATION_NODE		Lurn,
	PLURNEXT_IDE_DEVICE		IdeDisk,
	PCCB					Ccb
	) 
{
	BYTE		PduResponse;
	NTSTATUS	status;


	KDPrintM( DBG_LURN_TRACE, ("Send NOOP to Remote.\n") );

	UNREFERENCED_PARAMETER( Lurn );

	if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

		status = LspNoOperation( &IdeDisk->PrimaryLanScsiSession, 
								 IdeDisk->LuHwData.LanscsiTargetID, 
								 &PduResponse, 
								 NULL );
	
	} else {

		status = LspNoOperation( &IdeDisk->CommLanScsiSession, 
								 IdeDisk->LuHwData.LanscsiTargetID, 
								 &PduResponse, 
								 NULL );
	}

	if (status != STATUS_SUCCESS) {
	
		KDPrintM( DBG_LURN_ERROR, ("LspNoOperation() failed. NTSTATUS:%08lx\n", status) );

		if(Ccb) Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
		return STATUS_PORT_DISCONNECTED;

	} else if (PduResponse != LANSCSI_RESPONSE_SUCCESS) {
	
		KDPrintM( DBG_LURN_ERROR, ("Failure reply. PDUSTATUS:%08lx\n", (int)PduResponse) );

		if(Ccb) Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
		return STATUS_PORT_DISCONNECTED;
	}

	if (Ccb) {
		
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
LurnIdeOddDispatchCcb (
	PLURELATION_NODE		Lurn,
	PLURNEXT_IDE_DEVICE		IdeDisk,
	PCCB					Ccb
	) 
{
	NTSTATUS	status;

	status = STATUS_SUCCESS;

	if (Lurn->LurnStatus != LURN_STATUS_RUNNING && Lurn->LurnStatus != LURN_STATUS_STOP_PENDING) {
		
		NDAS_BUGON( Lurn->LurnStatus == LURN_STATUS_STALL );

		if (Ccb->OperationCode != CCB_OPCODE_STOP && !LsCcbIsFlagOn(Ccb, CCB_FLAG_MUST_SUCCEED)) {

			Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
			return STATUS_PORT_DISCONNECTED;
		}
	}

	// Try reconnect except stop request.
	// Stop or must-succeed request must be performed any time.

	switch (Ccb->OperationCode) {

	case CCB_OPCODE_RESETBUS:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("CCB_OPCODE_RESETBUS! Nothing to do.\n") );

		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		break;

	case CCB_OPCODE_ABORT_COMMAND:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_ABORT_COMMAND!\n") );

		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		status = STATUS_SUCCESS;

		break;

	case CCB_OPCODE_EXECUTE:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_EXECUTE!\n") );

		status = LurnIdeOddExecute( Lurn, Lurn->LurnExtension, Ccb );
		break;

	case CCB_OPCODE_FLUSH:
		
		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("CCB_OPCODE_FLUSH, Ccb->Srb = %08x, Ccb->AssociateID = %d\n",
					 Ccb->Srb, Ccb->AssociateID) );

		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		break;

	case CCB_OPCODE_STOP:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_STOP!\n") );
			
		NDAS_BUGON( FALSE );

		//LurnIdeStop( Lurn, Ccb );
		
		//	complete a CCB
			
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		break;

	case CCB_OPCODE_RESTART:

		NDAS_BUGON( FALSE );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_RESTART!\n") );
			
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		status = STATUS_SUCCESS;

		break;

	case CCB_OPCODE_QUERY: {

//		PLUR_QUERY			LurQuery;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_QUERY\n") );
#if 0
		//	Do not support LurPrimaryLurnInformation for Lfsfilt.

		LurQuery = (PLUR_QUERY)Ccb->DataBuffer;

		if (LurQuery->InfoClass == LurPrimaryLurnInformation) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Do not support LurPrimaryLurnInformation for Lfsfilt.\n"));
		
			Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
			status = STATUS_SUCCESS;
		
		} else
#endif
		{

			status = LurnIdeQuery( Lurn, IdeDisk, Ccb );
		}

		break;
	}

	case CCB_OPCODE_UPDATE: {

		//BYTE		PduResponse;

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, ("CCB_OPCODE_UPDATE!\n") );

#if 1
		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		status = STATUS_SUCCESS;
#else
		status = LurnIdeDiskUpdate( Lurn, IdeDisk, Ccb );
	
		//	Configure the disk

		if (status == STATUS_SUCCESS && Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
				
			LurnIdeDiskConfigure( Lurn, &PduResponse );
		}
#endif
		break;
	}

	case CCB_OPCODE_NOOP:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_NOOP!\n") );

		status = LurnIdeOddNoop( Lurn, Lurn->LurnExtension, Ccb );
		break;

	case CCB_OPCODE_SMART:

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_SMART\n") );
		
#if 1
		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		status = STATUS_SUCCESS;
#else	
		status = LurnIdeDiskSmart( Lurn, Lurn->LurnExtension, Ccb );
#endif
		break;
		
	case CCB_OPCODE_DEVLOCK:
		
		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("CCB_OPCODE_SMART\n"));
#if 1
		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
		status = STATUS_SUCCESS;
#else	
		status = LurnIdeDiskDeviceLockControl( Lurn, Lurn->LurnExtension, Ccb );
#endif
		break;

	case CCB_OPCODE_DVD_STATUS: {

		PLURN_DVD_STATUS	PDvdStatus;
		LARGE_INTEGER		CurrentTime;
		ULONG				Delta = 15;

		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_DVD_STATUS\n"));
		KeQuerySystemTime(&CurrentTime);

		PDvdStatus = (PLURN_DVD_STATUS)(Ccb->DataBuffer);

		if ((IdeDisk->DVD_STATUS == DVD_NO_W_OP) || 
			((IdeDisk->DVD_STATUS == DVD_W_OP) && 
			 ((ULONG)((CurrentTime.QuadPart - IdeDisk->DVD_Acess_Time.QuadPart) / 10000000 ) > Delta))) {

			PDvdStatus->Last_Access_Time.QuadPart = IdeDisk->DVD_Acess_Time.QuadPart;
			PDvdStatus->Status = DVD_NO_W_OP;
		
		} else {
		
			PDvdStatus->Status = DVD_W_OP;
		}

		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		status = STATUS_SUCCESS;

		break;
	}
	
	default:
			
		NDAS_BUGON( FALSE );
		break;
	}

	return status;
}

NTSTATUS 
LurnIdeOddHandleCcb (
	PLURELATION_NODE		Lurn,
	PLURNEXT_IDE_DEVICE		IdeDisk
	) 
{
	NTSTATUS	status;
	KIRQL		oldIrql;

	PLIST_ENTRY listEntry;
	PCCB		ccb;


	status = STATUS_SUCCESS;

	ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

	listEntry = IdeDisk->CcbQueue.Flink;

	while (listEntry != &IdeDisk->CcbQueue) {

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

		ccb->CcbStatus = CCB_STATUS_UNKNOWN_STATUS;

		if (Lurn->LurnStatus == LURN_STATUS_STOP_PENDING) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("Stop pending. Go ahead to process the rest of requests.\n") );

		} else if (!(Lurn->LurnStatus == LURN_STATUS_RUNNING) &&
				   !(Lurn->LurnStatus == LURN_STATUS_STALL)) {

			NDAS_BUGON( FALSE );

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
						("not LURN_STATUS_RUNNING nor LURN_STATUS_STALL.\n") );

			listEntry = listEntry->Flink;
			RemoveEntryList( &ccb->ListEntry );

			LsCcbSetStatus( ccb, CCB_STATUS_STOP );
			LsCcbSetStatusFlag( ccb, 
								CCBSTATUS_FLAG_TIMER_COMPLETE |  
								CCBSTATUS_FLAG_LURN_STOP	  |
								CCBSTATUS_FLAG_BUSCHANGE );

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

			LsCcbCompleteCcb( ccb );

			ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

			continue;
		}

		NDAS_BUGON( LURN_IS_RUNNING(Lurn->LurnStatus) );

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		status = LurnIdeOddDispatchCcb( Lurn, IdeDisk, ccb );

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

		if (status == STATUS_SUCCESS) {

			if (ccb->OperationCode != CCB_OPCODE_DEVLOCK) {

				NDAS_BUGON( ccb->CcbStatus == CCB_STATUS_SUCCESS				||
							 ccb->CcbStatus == CCB_STATUS_DATA_OVERRUN			||
							 ccb->CcbStatus == CCB_STATUS_INVALID_COMMAND		||
							 ccb->CcbStatus == CCB_STATUS_COMMMAND_DONE_SENSE	||
							 ccb->CcbStatus == CCB_STATUS_BUSY					||
							 ccb->CcbStatus == CCB_STATUS_NO_ACCESS );
			}

			listEntry = listEntry->Flink;
			RemoveEntryList( &ccb->ListEntry );

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

			LsCcbCompleteCcb( ccb );

			ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

			continue;
		}

#if __NDAS_CHIP_BUG_AVOID__

		if (status == STATUS_CANCELLED) {

			listEntry = listEntry->Flink;
			RemoveEntryList( &ccb->ListEntry );

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

			LsCcbCompleteCcb( ccb );

			ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

			status = STATUS_PORT_DISCONNECTED;
		
		} else {

			NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );

			if (ccb->OperationCode != CCB_OPCODE_DEVLOCK) {
	
				NDAS_BUGON( ccb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR );
			}

			NDAS_BUGON( !LsCcbIsFlagOn(ccb, CCB_FLAG_MUST_SUCCEED) );

			listEntry = listEntry->Flink;
			RemoveEntryList( &ccb->ListEntry );

			RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

			LsCcbSetStatusFlag( ccb, CCBSTATUS_FLAG_RECONNECTING );
			ccb->CcbStatus = CCB_STATUS_BUSY;
			LsCcbCompleteCcb( ccb );

			ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
		}
#else 

		NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );

		if (ccb->OperationCode != CCB_OPCODE_DEVLOCK) {
	
			NDAS_BUGON( ccb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR );
		}

		NDAS_BUGON( !LsCcbIsFlagOn(ccb, CCB_FLAG_MUST_SUCCEED) );

		listEntry = listEntry->Flink;
		RemoveEntryList( &ccb->ListEntry );

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		LsCcbSetStatusFlag( ccb, CCBSTATUS_FLAG_RECONNECTING );
		ccb->CcbStatus = CCB_STATUS_BUSY;
		LsCcbCompleteCcb( ccb );

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

#endif

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
					("NdasRaidClientHandleCcb failed. NTSTATUS:%08lx\n", status) );
	}

	RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

	return status;
}

VOID
LurnIdeOddThreadProc (
	IN PLURELATION_NODE	Lurn
	)
{
	PLURNEXT_IDE_DEVICE		IdeDisk;
	NTSTATUS				status;
	KIRQL					oldIrql;
	LARGE_INTEGER			timeOut;

	PVOID					events[2];
	ULONG					eventCount;

	BOOLEAN					terminateThread = FALSE;
	BYTE					pdu_response;

	LURN_EVENT				LurnEvent;

	PLIST_ENTRY				listEntry;
	PCCB					ccb;

	DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("Entered\n") );

	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;

	eventCount = 0;
	events[eventCount++] = &IdeDisk->ThreadCcbArrivalEvent;
	events[eventCount++] = &IdeDisk->BuffLockCtl.IoIdleTimer;

	timeOut.QuadPart = -LURNIDE_IDLE_TIMEOUT;

	//	Raise the thread's current priority level (same level to Modified page writer).
	//	Set ready signal.

	SetFlag( IdeDisk->ThreadFlasg, IDE_DEVICE_THREAD_FLAG_RUNNING );

	Lurn->LurnStatus = LURN_STATUS_RUNNING;

	KeSetPriorityThread( KeGetCurrentThread(), LOW_REALTIME_PRIORITY );
	KeSetEvent( &IdeDisk->ThreadReadyEvent, IO_NO_INCREMENT, FALSE );

	do {

		NDAS_BUGON( KeGetCurrentIrql() == PASSIVE_LEVEL );

		if (Lurn->LurnStatus != LURN_STATUS_RUNNING || IsListEmpty(&IdeDisk->CcbQueue)) {
		
			status = KeWaitForMultipleObjects( eventCount,
											   events,
											   WaitAny,
											   Executive,
											   KernelMode,
											   FALSE,
											   &timeOut,
											   NULL );

			if (!NT_SUCCESS(status)) {
		
				NDAS_BUGON( FALSE );

				terminateThread = TRUE;
				continue;
			}

			if (status != STATUS_WAIT_0 && status != STATUS_WAIT_1 && status != STATUS_TIMEOUT) {

				NDAS_BUGON( FALSE );
					
				terminateThread = TRUE;
				continue;
			}
		}
	
		KeClearEvent(events[0]);

		if (Lurn->LurnStatus == LURN_STATUS_RUNNING) {

			status = LurnIdeOddHandleCcb( Lurn, IdeDisk );

			if (status == STATUS_SUCCESS) {

			} else if (status == STATUS_PORT_DISCONNECTED) {

				Lurn->LurnStatus = LURN_STATUS_STALL;

				IdeDisk->MaxStallTimeOut.QuadPart = NdasCurrentTime().QuadPart + IdeDisk->MaxStallInterval.QuadPart;
				IdeDisk->ReConnectTimeOut.QuadPart = NdasCurrentTime().QuadPart;
			
			} else {
				
				NDAS_BUGON( FALSE );

				terminateThread = TRUE;
				continue;
			}
		} 

		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

		if (!IsListEmpty(&IdeDisk->CcbQueue)) {

			listEntry = IdeDisk->CcbQueue.Flink;

			ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

			if (ccb->OperationCode == CCB_OPCODE_EXECUTE) {

				if (ccb->Cdb[0] == SCSIOP_START_STOP_UNIT) {

					PCDB cdb = (PCDB)(ccb->Cdb);

					if (cdb->START_STOP.Start == STOP_UNIT_CODE) {

						IdeDisk->LURNHostStop = TRUE;

						RemoveEntryList( listEntry );

						LsCcbSetStatusFlag( ccb, CCBSTATUS_FLAG_TIMER_COMPLETE );
						ccb->CcbStatus = CCB_STATUS_SUCCESS;

						RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );
						LsCcbCompleteCcb( ccb );
						ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
					}
				}
			}
		}

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

		if (Lurn->IdeDisk->RequestToTerminate == TRUE) {

			NDAS_BUGON( IsListEmpty(&IdeDisk->CcbQueue) );

			terminateThread = TRUE;
			continue;
		}


		if (KeReadStateEvent(events[1])) {
	
			status = EnterBufferLockIoIdle( &IdeDisk->BuffLockCtl,
											IdeDisk->LanScsiSession,
											&IdeDisk->LuHwData );
			
			if (status != STATUS_SUCCESS) {
			
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
							("EnterBufferLockIoIdle() failed. NTSTATUS:%08lx\n", status) );
			}

			KeClearEvent( events[1] );

			if (status == STATUS_SUCCESS) {

			} else if (status == STATUS_PORT_DISCONNECTED) {

				Lurn->LurnStatus = LURN_STATUS_STALL;

				IdeDisk->MaxStallTimeOut.QuadPart = NdasCurrentTime().QuadPart + IdeDisk->MaxStallInterval.QuadPart;
				IdeDisk->ReConnectTimeOut.QuadPart = NdasCurrentTime().QuadPart;
			
			} else {
				
				NDAS_BUGON( FALSE );
				terminateThread = TRUE;

				continue;
			}
		}



		if (Lurn->LurnStatus == LURN_STATUS_RUNNING) {

			if (Lurn->IdeDisk->DisConnectOccured == TRUE) {

				Lurn->LurnStatus = LURN_STATUS_STALL;

				IdeDisk->MaxStallTimeOut.QuadPart = NdasCurrentTime().QuadPart + IdeDisk->MaxStallInterval.QuadPart;
				IdeDisk->ReConnectTimeOut.QuadPart = NdasCurrentTime().QuadPart;

				Lurn->IdeDisk->DisConnectOccured = FALSE;
			}
		}

#if 0
		if (Lurn->LurnStatus == LURN_STATUS_RUNNING) {

			if (Lurn->IdeDisk->AddAddressOccured == TRUE) {

				do {

					//BOOLEAN					lockExists;

					if (Lurn->IdeDisk->LuHwData.HwVersion == LANSCSIIDE_VERSION_1_0) {

						break;
					}

					if (Lurn->LurnStatus != LURN_STATUS_RUNNING) {
					
						break;
					}

					status = LurnIdeDiskChangeConnection( Lurn, IdeDisk );

					if (status != STATUS_SUCCESS) {

						NDAS_BUGON( FALSE );
						break;
					}

					if (IdeDisk->CommLanScsiSession.MediumSpeed == IdeDisk->TempLanScsiSession.MediumSpeed) {

						LspLogout( &IdeDisk->TempLanScsiSession, NULL );
						LspDisconnect( &IdeDisk->TempLanScsiSession );

						break;
					}

					if (RtlEqualMemory(((PTDI_ADDRESS_LPX)IdeDisk->CommLanScsiSession.NdasBindAddress.Address)->Node,
									   ((PTDI_ADDRESS_LPX)IdeDisk->TempLanScsiSession.NdasBindAddress.Address)->Node,
									   6)) {

						LspLogout( &IdeDisk->TempLanScsiSession, NULL );
						LspDisconnect( &IdeDisk->TempLanScsiSession );

						break;
					}

					// if (non buffer lock is acquired)
					//	break;

					lockExists = LockCacheAcquiredLocksExistsExceptForBufferLock( &IdeDisk->LuHwData );
					
					if (lockExists == TRUE) {
	
						LspLogout( &IdeDisk->TempLanScsiSession, NULL );
						LspDisconnect( &IdeDisk->TempLanScsiSession );

						break;
					}

					// if (buffer lock is acquired)
					// release buffer lock
				
					if (LockCacheIsAcquired(&IdeDisk->LuHwData, LURNDEVLOCK_ID_BUFFLOCK)) {
					
						status = NdasReleaseBufferLock( &IdeDisk->BuffLockCtl,
														IdeDisk->LanScsiSession,
														&IdeDisk->LuHwData,
														NULL,
														NULL,
														TRUE,
														0 );
						
						if (status != STATUS_SUCCESS) {
						
							LspLogout( &IdeDisk->TempLanScsiSession, NULL );
							LspDisconnect( &IdeDisk->TempLanScsiSession );

							break;
						}
					}

					// It's recommended test Medium Speed

					LspLogout( &IdeDisk->CommLanScsiSession, NULL );
					LspDisconnect( &IdeDisk->CommLanScsiSession );

					LspCopy( &IdeDisk->CommLanScsiSession, &IdeDisk->TempLanScsiSession, TRUE );
				
				} while (0);

				status = STATUS_SUCCESS;
				Lurn->IdeDisk->AddAddressOccured = FALSE;
			}
		}

#endif

		if (Lurn->LurnStatus == LURN_STATUS_STALL) {

			NDAS_BUGON( IdeDisk->MaxStallTimeOut.QuadPart != 0 );
			NDAS_BUGON( IdeDisk->ReConnectTimeOut.QuadPart != 0 );

			if (IdeDisk->MaxStallTimeOut.QuadPart < NdasCurrentTime().QuadPart) {

				Lurn->LurnStopReasonCcbStatusFlags |= CCBSTATUS_FLAG_RECONNECT_REACH_MAX;

				terminateThread = TRUE;
				continue;
			}

			if (IdeDisk->ReConnectTimeOut.QuadPart > NdasCurrentTime().QuadPart) {

				continue;
			}

			IdeDisk->ReConnectTimeOut.QuadPart = NdasCurrentTime().QuadPart + IdeDisk->ReConnectInterval.QuadPart;
	
			do {

				BYTE		PduResponse;
				UINT64		SectorCount;
				CHAR		Serial[20];
				CHAR		FW_Rev[8];
				CHAR		Model[40];

				BOOLEAN			LssEncBuff;

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("RECONN: try reconnecting!\n") );

				LockCacheAllLocksLost( &IdeDisk->LuHwData );

				status = STATUS_SUCCESS;

				if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {

					if (IdeDisk->PrimaryLanScsiSession.LanscsiProtocol == NULL) {

						status = STATUS_PORT_DISCONNECTED;
					
					} else {

						status = LspNoOperation( &IdeDisk->PrimaryLanScsiSession, 
												 IdeDisk->LuHwData.LanscsiTargetID, 
												 &pdu_response, 
												 NULL );

						if (status != STATUS_SUCCESS) {

							LspLogout( &IdeDisk->PrimaryLanScsiSession, NULL );
							LspDisconnect( &IdeDisk->PrimaryLanScsiSession );
						}
					}

					NDAS_BUGON( status == STATUS_SUCCESS || status == STATUS_PORT_DISCONNECTED );

					if (status != STATUS_SUCCESS) {

						if (IdeDisk->CntEcrBufferLength &&IdeDisk->CntEcrBuffer) {
	
							LssEncBuff = FALSE;

						} else {

							LssEncBuff = TRUE;
						}

						status = LspReconnect( &IdeDisk->PrimaryLanScsiSession, 
											   NULL, 
											   NULL, 
											   NULL );

						if (status != STATUS_SUCCESS) {

							LurnRecordFault( Lurn, LURN_ERR_CONNECT, status, NULL );
							DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("failed.\n") );
						
							break;
						}
						
						status = LspRelogin( &IdeDisk->PrimaryLanScsiSession, LssEncBuff, NULL );

						if (status != STATUS_SUCCESS) {

							LurnRecordFault( Lurn, 
											 LURN_ERR_LOGIN, 
											 IdeDisk->PrimaryLanScsiSession.LastLoginError, 
											 NULL );

							DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Login failed.\n") );

							break;
						}
					}	 
				}

				NDAS_BUGON( status == STATUS_SUCCESS );
				
				if (IdeDisk->CommLanScsiSession.LanscsiProtocol == NULL) {

					status = STATUS_PORT_DISCONNECTED;
					
				} else {

					status = LspNoOperation( &IdeDisk->CommLanScsiSession, 
											 IdeDisk->LuHwData.LanscsiTargetID, 
											 &pdu_response, 
											 NULL );

					if (status != STATUS_SUCCESS) {

						LspLogout( &IdeDisk->CommLanScsiSession, NULL );
						LspDisconnect( &IdeDisk->CommLanScsiSession );
					}
				}

				NDAS_BUGON( status == STATUS_SUCCESS || status == STATUS_PORT_DISCONNECTED );

				if (status != STATUS_SUCCESS) {

					if (IdeDisk->CntEcrBufferLength &&IdeDisk->CntEcrBuffer) {
	
						LssEncBuff = FALSE;

					} else {

						LssEncBuff = TRUE;
					}

					status = LspReconnect( &IdeDisk->CommLanScsiSession, 
										   NULL, 
										   NULL, 
										   NULL );

					if (status != STATUS_SUCCESS) {

						LurnRecordFault( Lurn, LURN_ERR_CONNECT, status, NULL );
						DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("failed.\n") );
						
						break;

					} 
					
					status = LspRelogin( &IdeDisk->CommLanScsiSession, LssEncBuff, NULL );

					if (status != STATUS_SUCCESS) {

						LurnRecordFault( Lurn, 
										 LURN_ERR_LOGIN, 
										 IdeDisk->CommLanScsiSession.LastLoginError, 
										 NULL );

						DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Login failed.\n") );

						break;
					}
				}

				NDAS_BUGON( status == STATUS_SUCCESS );

				// Save HDD's info before reconfiguration.

				SectorCount = IdeDisk->LuHwData.SectorCount;

				RtlCopyMemory( Serial, IdeDisk->LuHwData.Serial, sizeof(Serial) );
				RtlCopyMemory( FW_Rev, IdeDisk->LuHwData.FW_Rev, sizeof(FW_Rev) );
				RtlCopyMemory( Model, IdeDisk->LuHwData.Model, sizeof(Model) );				
	
#if 0
			{
				SENSE_DATA			senseData;

				status = ODD_GetStatus( Lurn->IdeDisk,
										NULL,
										&senseData,
										sizeof(SENSE_DATA),
										&timeOut,
										&PduResponse );

				if (status != STATUS_SUCCESS || PduResponse != LANSCSI_RESPONSE_SUCCESS) {

					NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );	

					DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("get Status Failed...\n") );

					status = STATUS_PORT_DISCONNECTED;
					break;
				}
			}
#endif

				//	Configure the disk and get the next CCB
				
				timeOut.QuadPart = -10 * NANO100_PER_SEC;

				status = LurnIdeOddConfigure( Lurn, &timeOut, &PduResponse );

				NDAS_BUGON( status == STATUS_SUCCESS || status == STATUS_PORT_DISCONNECTED || status == STATUS_CANCELLED );

				if (status != STATUS_SUCCESS || PduResponse != LANSCSI_RESPONSE_SUCCESS) {

					NDAS_BUGON( status == STATUS_CANCELLED || status == STATUS_PORT_DISCONNECTED );
				
					DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
								("RECONN: LurnIdeDiskConfigure() failed.\n") );

					status = STATUS_PORT_DISCONNECTED;
					break;
				}

				// Check reconnected disk is not replaced.
	
				if (IdeDisk->LuHwData.SectorCount != SectorCount ||
					RtlCompareMemory( Serial, IdeDisk->LuHwData.Serial, sizeof(Serial)) != sizeof(Serial) ||
					RtlCompareMemory( FW_Rev, IdeDisk->LuHwData.FW_Rev, sizeof(FW_Rev)) != sizeof(FW_Rev) ||					
					RtlCompareMemory( Model, IdeDisk->LuHwData.Model, sizeof(Model)) != sizeof(Model)) {

					status = STATUS_UNSUCCESSFUL;
					
					NDAS_BUGON( FALSE );

					LspLogout( IdeDisk->LanScsiSession, NULL );
					LspDisconnect( IdeDisk->LanScsiSession );

					Lurn->LurnStopReasonCcbStatusFlags |= CCBSTATUS_FLAG_DISK_REPLACED;
					
					terminateThread = TRUE;
					break;
				}

#if 0

				// If the disk is dirty, determine if power recycle occurs after disconnection.
				// When the disk is dirty and power recycle occurs,
				// We think the data in the disk write cache might be lost.
				// We can also assume whether the disk write cache is disabled by checking
				// LU hardware data, but other host might enable the cache.
				// Therefore, we should not assume the LU hardware data has a exact information.
				// If the count is zero that means there was no initial power recycle count,
				// Can not perform power recycle detection using SMART.
		
				if(IdeDisk->DiskWriteCacheDirty) {
		
					DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Disk write cache is dirty.\n") );

				} else {

					DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Disk write cache is clean.\n") );
				}

				if (IdeDisk->LuHwData.InitialDevPowerRecycleCount != 0) {

					UINT64		powerRecycleCount;

					status = LurnIdeDiskGetPowerRecycleCount( IdeDisk, &powerRecycleCount );

					if (status != STATUS_SUCCESS) {

						NDAS_BUGON( FALSE );

						DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
									("RECONN: Power recycle detection"
									 "failed. STATUS=%08lx. Continue to the next.\n",
									  status) );

						break;
					} 
					
					DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
								("RECONN: Current power recycle count = %I64u\n", powerRecycleCount) );

					if (IdeDisk->DiskWriteCacheDirty) {

						if (IdeDisk->LuHwData.InitialDevPowerRecycleCount != powerRecycleCount) {

							status = STATUS_UNSUCCESSFUL;

							DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, 
										("RECONN: HDD got power-recycled. Stop the LURN.\n") );
					
							LspLogout( IdeDisk->LanScsiSession, NULL );						
							LspDisconnect( IdeDisk->LanScsiSession );
								
							//	terminate the thread
														
							Lurn->LurnStopReasonCcbStatusFlags |= CCBSTATUS_FLAG_POWERRECYLE_OCCUR;

							terminateThread = TRUE;
							break;
						}
					}

					if (FlagOn(Lurn->AccessRight, GENERIC_WRITE)) {
		
						status = NdasTransRegisterDisconnectHandler( &IdeDisk->PrimaryLanScsiSession.NdastAddressFile,
																	 IdeDiskDisconnectHandler,
																	 Lurn );
			
						if (status != STATUS_SUCCESS) {

							NDAS_BUGON( FALSE );
							break;
						}
					}

					status = NdasTransRegisterDisconnectHandler( &IdeDisk->CommLanScsiSession.NdastAddressFile,
															     IdeDiskDisconnectHandler,
																 Lurn );
			
					if (status != STATUS_SUCCESS) {

						NDAS_BUGON( FALSE );
						break;
					}

					IdeDisk->LuHwData.InitialDevPowerRecycleCount = powerRecycleCount;
				}

#endif
				NDAS_BUGON( status == STATUS_SUCCESS );

				ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
				Lurn->LurnStatus = LURN_STATUS_RUNNING;
				RELEASE_SPIN_LOCK(&Lurn->SpinLock, oldIrql);

				Lurn->IdeDisk->DisConnectOccured = FALSE;
			
			} while (0);

			if (IsListEmpty(&IdeDisk->CcbQueue)) {

				//	Set a event to make NDASSCSI fire NoOperation.

				LurnEvent.LurnId = Lurn->LurnId;
				LurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;

				LurCallBack( Lurn->Lur, &LurnEvent );
			}
		}

	} while (terminateThread == FALSE);

	//	Set LurnStatus and empty ccb queue

	DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, ("The thread will be terminated.\n") );
	
	ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );

#if DBG

	if (Lurn->LurnStatus == LURN_STATUS_RUNNING) {

		NDAS_BUGON( IsListEmpty(&IdeDisk->CcbQueue) );
	}

#endif

	ClearFlag( IdeDisk->ThreadFlasg, IDE_DEVICE_THREAD_FLAG_RUNNING );
	SetFlag( IdeDisk->ThreadFlasg, IDE_DEVICE_THREAD_FLAG_STOPPED );

	Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;


	while (!IsListEmpty(&IdeDisk->CcbQueue)) {

		listEntry = RemoveHeadList(&IdeDisk->CcbQueue);

		ccb = CONTAINING_RECORD( listEntry, CCB, ListEntry );

		DebugTrace( NDASSCSI_DBG_LURN_IDE_INFO, 
					("teminated ccb = %p, ccb->OperationCode = %s, ccb->Cdb[0] =%s\n", 
					 ccb, CcbOperationCodeString(ccb->OperationCode), CdbOperationString(ccb->Cdb[0])) );

		ccb->CcbStatus = CCB_STATUS_STOP;

		LsCcbSetStatusFlag( ccb, 
							CCBSTATUS_FLAG_TIMER_COMPLETE | 
							((CCBSTATUS_FLAG_LURN_STOP | CCBSTATUS_FLAG_BUSCHANGE | Lurn->LurnStopReasonCcbStatusFlags) & 
							 CCBSTATUS_FLAG_ASSIGNMASK) );

		RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );
		LsCcbCompleteCcb( ccb );
		ACQUIRE_SPIN_LOCK( &Lurn->SpinLock, &oldIrql );
	}

	RELEASE_SPIN_LOCK( &Lurn->SpinLock, oldIrql );

	DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Terminated\n") );

	PsTerminateSystemThread( STATUS_SUCCESS );

	return;
}




VOID
IdeOddLurnStop (
	PLURELATION_NODE Lurn
	)
{
	UNREFERENCED_PARAMETER( Lurn );
	NDAS_BUGON( FALSE );
}


#define CDB_GET_LEAST_FROM_UINT32(VALUE, ORDER) ((BYTE)(((VALUE) & ((UINT32)0xff << (ORDER) * 8)) >> (ORDER) * 8))
#define CDB_GET_LEAST_FROM_UINT64(VALUE, ORDER) ((BYTE)(((VALUE) & ((UINT64)0xff << (ORDER) * 8)) >> (ORDER) * 8))

NTSTATUS
CdbSetAddrLen(
	__in __out PCDB Cdb,
	__in UINT64 BlockAddress,
	__in UINT32 TransferBlocks
){
	switch(Cdb->AsByte[0]) {
		case SCSIOP_READ6:
		case SCSIOP_WRITE6:
			Cdb->CDB6READWRITE.LogicalBlockMsb1 = (UCHAR)((BlockAddress & 0x1f0000) >> 16);
			Cdb->CDB6READWRITE.LogicalBlockMsb0 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->CDB6READWRITE.LogicalBlockLsb  = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->CDB6READWRITE.TransferBlocks = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_VERIFY6:
			Cdb->CDB6VERIFY.VerificationLength[0] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 2);
			Cdb->CDB6VERIFY.VerificationLength[1] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->CDB6VERIFY.VerificationLength[2] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_READ:
		case SCSIOP_WRITE:
		case SCSIOP_WRITE_VERIFY:
		case SCSIOP_VERIFY:
			Cdb->CDB10.LogicalBlockByte0 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->CDB10.LogicalBlockByte1 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->CDB10.LogicalBlockByte2 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->CDB10.LogicalBlockByte3 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->CDB10.TransferBlocksMsb = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->CDB10.TransferBlocksLsb = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_READ12:
		case SCSIOP_WRITE12:
		case SCSIOP_WRITE_VERIFY12:
		case SCSIOP_VERIFY12:
			Cdb->CDB12.LogicalBlock[0] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->CDB12.LogicalBlock[1] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->CDB12.LogicalBlock[2] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->CDB12.LogicalBlock[3] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->CDB12.TransferLength[0] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 3);
			Cdb->CDB12.TransferLength[1] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 2);
			Cdb->CDB12.TransferLength[2] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->CDB12.TransferLength[3] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_READ_CD:
			Cdb->READ_CD.StartingLBA[0] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->READ_CD.StartingLBA[1] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->READ_CD.StartingLBA[2] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->READ_CD.StartingLBA[3] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->READ_CD.TransferBlocks[0] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 2);
			Cdb->READ_CD.TransferBlocks[1] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->READ_CD.TransferBlocks[2] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_PLEXTOR_CDDA:
			Cdb->PLXTR_READ_CDDA.LogicalBlockByte0 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->PLXTR_READ_CDDA.LogicalBlockByte1 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->PLXTR_READ_CDDA.LogicalBlockByte2 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->PLXTR_READ_CDDA.LogicalBlockByte3 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->PLXTR_READ_CDDA.TransferBlockByte0 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 3);
			Cdb->PLXTR_READ_CDDA.TransferBlockByte1 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 2);
			Cdb->PLXTR_READ_CDDA.TransferBlockByte2 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->PLXTR_READ_CDDA.TransferBlockByte3 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_NEC_CDDA:
			Cdb->NEC_READ_CDDA.LogicalBlockByte0 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->NEC_READ_CDDA.LogicalBlockByte1 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->NEC_READ_CDDA.LogicalBlockByte2 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->NEC_READ_CDDA.LogicalBlockByte3 = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->NEC_READ_CDDA.TransferBlockByte0 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->NEC_READ_CDDA.TransferBlockByte1 = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		case SCSIOP_READ16:
		case SCSIOP_WRITE16:
		case SCSIOP_WRITE_VERIFY16:
		case SCSIOP_VERIFY16:
			Cdb->CDB16.LogicalBlock[0] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 7);
			Cdb->CDB16.LogicalBlock[1] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 6);
			Cdb->CDB16.LogicalBlock[2] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 5);
			Cdb->CDB16.LogicalBlock[3] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 4);
			Cdb->CDB16.LogicalBlock[4] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 3);
			Cdb->CDB16.LogicalBlock[5] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 2);
			Cdb->CDB16.LogicalBlock[6] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 1);
			Cdb->CDB16.LogicalBlock[7] = CDB_GET_LEAST_FROM_UINT64(BlockAddress, 0);
			Cdb->CDB16.TransferLength[0] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 3);
			Cdb->CDB16.TransferLength[1] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 2);
			Cdb->CDB16.TransferLength[2] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 1);
			Cdb->CDB16.TransferLength[3] = CDB_GET_LEAST_FROM_UINT32(TransferBlocks, 0);
			break;
		default: 
			ASSERT(FALSE);
			return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}


#define CDB_MAKE_LEAST_TO_UINT64(VALUE, ORDER) (((UINT64)((VALUE) & 0xff) << 8 * (ORDER)))
#define CDB_MAKE_LEAST_TO_UINT32(VALUE, ORDER) (((UINT32)((VALUE) & 0xff) << 8 * (ORDER)))

NTSTATUS
CdbGetAddrLen(
	__in __out PCDB Cdb,
	__out PUINT64 BlockAddress,
	__out PUINT32 TransferBlocks
){
	UINT64 blockAddr;
	UINT32 transferBlocks;

	switch(Cdb->AsByte[0]) {
		case SCSIOP_READ6:
		case SCSIOP_WRITE6:
			blockAddr =
				((UINT64)(Cdb->CDB6READWRITE.LogicalBlockMsb1 & 0x1f) << 16) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB6READWRITE.LogicalBlockMsb0, 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB6READWRITE.LogicalBlockLsb, 0);
			transferBlocks = CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB6READWRITE.TransferBlocks, 0);
			break;
		case SCSIOP_VERIFY6:
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB6VERIFY.VerificationLength[0], 2) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB6VERIFY.VerificationLength[1], 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB6VERIFY.VerificationLength[2], 0);
			break;
		case SCSIOP_READ:
		case SCSIOP_WRITE:
		case SCSIOP_WRITE_VERIFY:
		case SCSIOP_VERIFY:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB10.LogicalBlockByte0, 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB10.LogicalBlockByte1, 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB10.LogicalBlockByte2, 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB10.LogicalBlockByte3, 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB10.TransferBlocksMsb, 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB10.TransferBlocksLsb, 0);
			break;
		case SCSIOP_READ12:
		case SCSIOP_WRITE12:
		case SCSIOP_WRITE_VERIFY12:
		case SCSIOP_VERIFY12:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB12.LogicalBlock[0], 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB12.LogicalBlock[1], 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB12.LogicalBlock[2], 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB12.LogicalBlock[3], 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB12.TransferLength[0], 3) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB12.TransferLength[1], 2) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB12.TransferLength[2], 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB12.TransferLength[3], 0);
			break;
		case SCSIOP_READ_CD:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->READ_CD.StartingLBA[0], 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->READ_CD.StartingLBA[1], 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->READ_CD.StartingLBA[2], 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->READ_CD.StartingLBA[3], 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->READ_CD.TransferBlocks[0], 2) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->READ_CD.TransferBlocks[1], 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->READ_CD.TransferBlocks[2], 0);
			break;
		case SCSIOP_PLEXTOR_CDDA:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->PLXTR_READ_CDDA.LogicalBlockByte0, 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->PLXTR_READ_CDDA.LogicalBlockByte1, 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->PLXTR_READ_CDDA.LogicalBlockByte2, 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->PLXTR_READ_CDDA.LogicalBlockByte3, 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->PLXTR_READ_CDDA.TransferBlockByte0, 3) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->PLXTR_READ_CDDA.TransferBlockByte1, 2) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->PLXTR_READ_CDDA.TransferBlockByte2, 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->PLXTR_READ_CDDA.TransferBlockByte3, 0);
			break;
		case SCSIOP_NEC_CDDA:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->NEC_READ_CDDA.LogicalBlockByte0, 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->NEC_READ_CDDA.LogicalBlockByte1, 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->NEC_READ_CDDA.LogicalBlockByte2, 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->NEC_READ_CDDA.LogicalBlockByte3, 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->NEC_READ_CDDA.TransferBlockByte0, 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->NEC_READ_CDDA.TransferBlockByte1, 0);
			break;
		case SCSIOP_READ16:
		case SCSIOP_WRITE16:
		case SCSIOP_WRITE_VERIFY16:
		case SCSIOP_VERIFY16:
			blockAddr =
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[0], 7) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[1], 6) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[2], 5) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[3], 4) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[4], 3) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[5], 2) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[6], 1) +
				CDB_MAKE_LEAST_TO_UINT64(Cdb->CDB16.LogicalBlock[7], 0);
			transferBlocks =
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB16.TransferLength[0], 3) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB16.TransferLength[1], 2) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB16.TransferLength[2], 1) +
				CDB_MAKE_LEAST_TO_UINT32(Cdb->CDB16.TransferLength[3], 0);
			break;
		default: 
			ASSERT(FALSE);
			return STATUS_UNSUCCESSFUL;
	}
	if(BlockAddress)
		*BlockAddress = blockAddr;
	if(TransferBlocks)
		*TransferBlocks = transferBlocks;

	return STATUS_SUCCESS;
}

//
// SCSIOP_WRITE6
// SCSIOP_WRITE
// SCSIOP_WRITE12
// SCSIOP_WRITE16
// SCSIOP_WRITE_VERIFY
// SCSIOP_WRITE_VERIFY12
// SCSIOP_WRITE_VERIFY16
//

NTSTATUS
AtapiWriteRequest (
	PLURELATION_NODE	Lurn,
	PLURNEXT_IDE_DEVICE	IdeDisk,
	PCCB				Ccb,
	PBYTE				PduResponse,
	PLARGE_INTEGER		Timeout,
	PUCHAR				PduRegister
	)
{
	NTSTATUS	status;
	BOOLEAN				recoverBuffer;
	PBYTE				l_DataBuffer;
	UINT32				l_Flags;
	PSCSI_REQUEST_BLOCK Srb;
	LARGE_INTEGER	longTimeOut;
	UCHAR			Command = Ccb->PKCMD[0];
	PLANSCSI_SESSION	LSS = IdeDisk->LanScsiSession;
	LANSCSI_PDUDESC	pduDesc;
	UINT64		blockAddr;
	UINT32		transferBlocks;
	ULONG		writeSplitBlockBytes = IdeDisk->WriteSplitSize;
	ULONG				transferedBlocks;
	ULONG				blocksToTransfer;
	UINT32				splitBlocks;
	BOOLEAN				lostLockIO;
	BOOLEAN				doSplit;
	ULONG		bytesOfBlock;

	UINT32				chooseDataLength = 0;
	LARGE_INTEGER		startTime;
	LARGE_INTEGER		endTime;

#if __ENABLE_PERFORMACE_CHECK__
	LARGE_INTEGER		pFStartTime;
	LARGE_INTEGER		pFCurrentTime;

	pFStartTime = KeQueryPerformanceCounter(NULL);
	pFCurrentTime.QuadPart = pFStartTime.QuadPart;
#endif

	UNREFERENCED_PARAMETER(Lurn);

	Srb = Ccb->Srb;
	if (Timeout) {

		longTimeOut.QuadPart = Timeout->QuadPart;

	} else {

		longTimeOut.QuadPart = - LURN_IDE_ODD_TIMEOUT;
	}

	status = CdbGetAddrLen((PCDB)Ccb->PKCMD, &blockAddr, &transferBlocks);
	if(!NT_SUCCESS(status)) {

		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;

		return STATUS_SUCCESS;
	}

	if (transferBlocks == 0) {

		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;

		return STATUS_SUCCESS;
	}

	//
	// Determine if we can send split IOs.
	//
	if(IdeDisk->LuHwData.InquiryData.DeviceType != DIRECT_ACCESS_DEVICE) {
		if (IdeDisk->RandomWritableBlockBytes == 0) {
			bytesOfBlock = 0;
			doSplit = FALSE;
		} else {
			bytesOfBlock = IdeDisk->RandomWritableBlockBytes;
			doSplit = TRUE;
		}
	} else {
		bytesOfBlock = IdeDisk->ReadCapacityBlockBytes;
		doSplit = TRUE;
	}

	status = STATUS_SUCCESS;

	// Check to see if this IO requires a lost device lock.

	lostLockIO = LockCacheCheckLostLockIORange(
		&IdeDisk->LuHwData, 
		blockAddr, 
		blockAddr + transferBlocks - 1 );

	if (lostLockIO) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Blocked by a lost lock.\n") );
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		return STATUS_SUCCESS;
	}

	// Clear flow control stats.

//	LurnIdeDiskResetProtocolCounters( IdeDisk );

	// Stop the IO idle timer

	StopIoIdleTimer( &IdeDisk->BuffLockCtl );

	//
	//	Try to encrypt the content.
	//
	l_Flags = 0;
	l_DataBuffer = Ccb->SecondaryBuffer;
	recoverBuffer = FALSE;
	if(IdeDisk->CntEcrMethod && bytesOfBlock) {

		//
		//	Encrypt the content before writing.
		//	If the primary buffer is volatile, encrypt it directly.
		//
		if(LsCcbIsFlagOn(Ccb, CCB_FLAG_VOLATILE_PRIMARY_BUFFER)) {
			EncryptBlock(
				IdeDisk->CntEcrCipher,
				IdeDisk->CntEcrKey,
				transferBlocks * bytesOfBlock,
				Ccb->SecondaryBuffer,
				Ccb->SecondaryBuffer
				);
			//
			//	If the encryption buffer is available, encrypt it by copying to the encryption buffer.
			//
		} else if(	IdeDisk->CntEcrBuffer &&
			IdeDisk->CntEcrBufferLength >= (transferBlocks * bytesOfBlock)
			) {

				EncryptBlock(
					IdeDisk->CntEcrCipher,
					IdeDisk->CntEcrKey,
					transferBlocks * bytesOfBlock,
					Ccb->SecondaryBuffer,
					IdeDisk->CntEcrBuffer
					);
				l_DataBuffer = IdeDisk->CntEcrBuffer;
				l_Flags = PDUDESC_FLAG_VOLATILE_BUFFER;

				//
				//	There is no usable buffer, encrypt it directly and decrypt it later.
				//
		} else {

			EncryptBlock(
				IdeDisk->CntEcrCipher,
				IdeDisk->CntEcrKey,
				transferBlocks * bytesOfBlock,
				Ccb->SecondaryBuffer,
				Ccb->SecondaryBuffer
				);
			recoverBuffer = TRUE;
		}
#if DBG
		if(	!IdeDisk->CntEcrBuffer ||
			IdeDisk->CntEcrBufferLength < (transferBlocks * bytesOfBlock)
			) {
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
					("Encryption buffer too small! %p %d\n",
					IdeDisk->CntEcrBuffer, IdeDisk->CntEcrBufferLength));
		}
#endif
	}

	//	Send write LSP requests split by SplitBlock factor.

	NDAS_BUGON( writeSplitBlockBytes == (32*1024) || writeSplitBlockBytes == (64*1024) );
	// Calculate the number of split blocks
	if(doSplit) {
		if (transferBlocks * bytesOfBlock >= 8*1024) {
			UINT32	maxSize = transferBlocks * IdeDisk->LuHwData.BlockBytes;

			if(maxSize > IdeDisk->MaxDataSendLength)
				maxSize = IdeDisk->MaxDataSendLength;

			chooseDataLength = NdasFcChooseSendSize( &IdeDisk->LanScsiSession->SendNdasFcStatistics, 
				maxSize );

			startTime = NdasCurrentTime();

			splitBlocks = (chooseDataLength + bytesOfBlock - 1) / bytesOfBlock;

		} else {

			splitBlocks = transferBlocks;
		}
	} else {

		splitBlocks = transferBlocks;
	}

	transferedBlocks = 0; 

#if DBG
	if(doSplit) {
		NDAS_BUGON( splitBlocks * bytesOfBlock <= 65536 );
		NDAS_BUGON( (Ccb->SecondaryBufferLength % bytesOfBlock) == 0 );
	}
	else {
		NDAS_BUGON(Ccb->SecondaryBufferLength <= 65536);
	}
#endif

	//NDAS_BUGON( FlagOn(IdeDisk->Lurn->AccessRight, GENERIC_WRITE) );

	if (transferBlocks >= splitBlocks ) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE,
			("TransferBlocks = %d, splitBlocks = %d\n", transferBlocks, splitBlocks) );
	}
	while (transferedBlocks < transferBlocks) {

		blocksToTransfer = ((transferBlocks - transferedBlocks) < splitBlocks) ? (transferBlocks - transferedBlocks) : splitBlocks;

		NDAS_BUGON( blocksToTransfer > 0 );

#if 0
		//
		// Acquire the buffer lock for ATAPI devices regardless of CCB flags.
		// ATAPI devices's commands send data whether the access mode is read-only or not.
		//

		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK)) {
#endif
			//	Acquire the buffer lock allocated for this unit device.

			status = NdasAcquireBufferLock( &IdeDisk->BuffLockCtl,
				IdeDisk->LanScsiSession,
				&IdeDisk->LuHwData,
				NULL,
				NULL );

			if (status == STATUS_NOT_SUPPORTED) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Lock is not supported\n") );

			} else if (status != STATUS_SUCCESS) {

				NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Failed to acquire the buffer lock\n") );

				Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
				break;
			}
#if 0
		}
#endif

		if(transferBlocks != splitBlocks) {
			CdbSetAddrLen((PCDB)Ccb->PKCMD, blockAddr + transferedBlocks, blocksToTransfer);
			SETUP_ATAPI_PDUDESC(
				IdeDisk,
				&pduDesc,
				Ccb->PKCMD,
				(PBYTE)l_DataBuffer + transferedBlocks * bytesOfBlock,
				blocksToTransfer * bytesOfBlock,
				&longTimeOut);
		} else {
			LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &longTimeOut );
		}
		pduDesc.Flags |= l_Flags;

		status = LspPacketRequest(
			LSS,
			&pduDesc,
			PduResponse,
			PduRegister
			);
#if 0
		//	Release the lock in the NDAS device.

		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK)) {
#endif

			NdasReleaseBufferLock( &IdeDisk->BuffLockCtl,
				IdeDisk->LanScsiSession,
				&IdeDisk->LuHwData,
				NULL,
				NULL,
				FALSE,
				blocksToTransfer * bytesOfBlock );
#if 0
		}
#endif

		if(status != STATUS_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
				("Communication error. STATUS=%08lx, response=%x,logicalBlockAddress = %I64x, transferBlocks = %d, transferedBlocks = %d splitBlocks = %d\n",
				status, *PduResponse, blockAddr, transferBlocks, transferedBlocks, splitBlocks) );
			//
			// No retry for the non-direct-access device
			// if the write fails during the split IOs.
			//
			if(IdeDisk->LuHwData.InquiryData.DeviceType != DIRECT_ACCESS_DEVICE) {
				if(transferedBlocks != 0 && transferedBlocks < transferBlocks) {
					Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
					status = STATUS_SUCCESS;
				}
			}

			break;
		} else if(*PduResponse != LANSCSI_RESPONSE_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
				("Device returned error. STATUS=%08lx, response=%x LBA = %I64x, transfer = %d, transfered = %d split = %d StatReg=%x ErrReg=%x\n",
				status, *PduResponse, blockAddr, transferBlocks, transferedBlocks, splitBlocks, PduRegister[1], PduRegister[0]) );
			break;
		}

		// Get the transfer log

		IdeDisk->Retransmits += pduDesc.Retransmits;
		IdeDisk->PacketLoss	+= pduDesc.PacketLoss;

		transferedBlocks += blocksToTransfer;
	}

	//
	// Recover the primary buffer encrypted for content encryption.
	//

	if(recoverBuffer && pduDesc.DataBuffer) {
		ASSERT(FALSE);
		DecryptBlock(
			IdeDisk->CntEcrCipher,
			IdeDisk->CntEcrKey,
			transferBlocks * bytesOfBlock,
			Ccb->SecondaryBuffer,
			Ccb->SecondaryBuffer
			);
	}
	if (status == STATUS_SUCCESS) {

		if(doSplit) {
			if (transferBlocks * bytesOfBlock >= 8*1024) {
	
				endTime = NdasCurrentTime();
	
				NdasFcUpdateSendSize( &IdeDisk->LanScsiSession->SendNdasFcStatistics, 
					chooseDataLength, 
					transferBlocks * bytesOfBlock,
					startTime, 
					endTime );
			}
		}

		StartIoIdleTimer( &IdeDisk->BuffLockCtl );
	}


#if __ENABLE_PERFORMACE_CHECK__

	pPFCurrentTime = KeQueryPerformanceCounter(NULL);

	if (pFCurrentTime.QuadPart - pFStartTime.QuadPart > 10000) {

		DbgPrint( "=PF= addr:%d blk=%d elaps:%I64d\n", 
			LogicalBlockAddress, TransferBlocks, pFCurrentTime.QuadPart - pFStartTime.QuadPart );
	}

#endif

	IdeDisk->Lurn->LastAccessedAddress = blockAddr + transferBlocks;

	return status;
}

//
// SCSIOP_READ6
// SCSIOP_READ
// SCSIOP_READ12
// SCSIOP_READ16
// SCSIOP_READ_CD
// SCSIOP_NEC_CDDA
// SCSIOP_PLEXTOR_CDDA
//
static
LONG
OddGetReadCdBlockSize(PCDB Cdb, UINT64 BlockAddress)
{
	LONG blockSize;


	blockSize = 0;
	switch(Cdb->AsByte[0]) {
		case SCSIOP_READ_CD:
			if(BlockAddress == 0xf0000000 ||
				BlockAddress == 0xffffffff) {
				// CD-Text command.
				blockSize = 0;
				break;
			}
			// Main channel
			blockSize = CD_BLOCK_SIZE_RAW;

			// C2 error information
			if(Cdb->READ_CD.ErrorFlags == 1) {
				blockSize += 294; // C2 block error bytes
			} else if(Cdb->READ_CD.ErrorFlags == 2) {
				blockSize += 296; // C@ block error bytes + C2 error flags.
			}

			// Sub channel
			if(Cdb->READ_CD.SubChannelSelection == 1 ||
				Cdb->READ_CD.SubChannelSelection == 4) {
				blockSize += 96;
			} else if(Cdb->READ_CD.SubChannelSelection == 2) {
				blockSize += 16;
			}
			break;
		case SCSIOP_PLEXTOR_CDDA:
		case SCSIOP_NEC_CDDA:
			blockSize = CD_BLOCK_SIZE_RAW;
			break;
		default:
			ASSERT(FALSE);
			blockSize = 0;
			break;
	}

	return blockSize;
}

NTSTATUS
AtapiReadRequest (
	PLURELATION_NODE	Lurn,
	PLURNEXT_IDE_DEVICE	IdeDisk,
	PCCB				Ccb,
	PBYTE				PduResponse,
	PLARGE_INTEGER		Timeout,
	PUCHAR				PduRegister
	)
{
	NTSTATUS	status;
	PSCSI_REQUEST_BLOCK Srb;
	LARGE_INTEGER	longTimeOut;
	UCHAR			Command = Ccb->PKCMD[0];
	PLANSCSI_SESSION	LSS = IdeDisk->LanScsiSession;
	LANSCSI_PDUDESC	pduDesc;
	UINT64		blockAddr;
	UINT32		transferBlocks;
	ULONG		readSplitBlockBytes = IdeDisk->ReadSplitSize;
	ULONG		transferedBlocks;
	ULONG		blocksToTransfer;
	UINT32		splitBlocks;
	BOOLEAN		isLongTimeout;
	BOOLEAN		lostLockIO;
	ULONG		bytesOfBlock;

	UINT32				chooseDataLength = 0;
	LARGE_INTEGER		startTime;
	LARGE_INTEGER		endTime;
	BOOLEAN				doSplit;

#if __ENABLE_PERFORMACE_CHECK__
	LARGE_INTEGER		pFStartTime;
	LARGE_INTEGER		pFCurrentTime;

	pFStartTime = KeQueryPerformanceCounter(NULL);
	pFCurrentTime.QuadPart = pFStartTime.QuadPart;
#endif

	UNREFERENCED_PARAMETER(Lurn);

	Srb = Ccb->Srb;

	if (Timeout) {

		longTimeOut.QuadPart = Timeout->QuadPart;
	
	} else {

		longTimeOut.QuadPart = - LURN_IDE_ODD_TIMEOUT;
	}

	status = CdbGetAddrLen((PCDB)Ccb->PKCMD, &blockAddr, &transferBlocks);
	if(!NT_SUCCESS(status)) {
		return status;
	}

	if (transferBlocks == 0) {

		NDAS_BUGON( FALSE );
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;

		return STATUS_SUCCESS;
	}

	//
	// Determine if we can send split IOs.
	//
	if(IdeDisk->LuHwData.InquiryData.DeviceType != DIRECT_ACCESS_DEVICE) {
		if(Ccb->PKCMD[0] == SCSIOP_READ_CD ||
			Ccb->PKCMD[0] == SCSIOP_PLEXTOR_CDDA ||
			Ccb->PKCMD[0] == SCSIOP_NEC_CDDA)
		{
			bytesOfBlock = OddGetReadCdBlockSize((PCDB)Ccb->PKCMD, blockAddr);
			if(bytesOfBlock) {
				ASSERT((Ccb->SecondaryBufferLength % bytesOfBlock) == 0);
				ASSERT((Ccb->SecondaryBufferLength == transferBlocks * bytesOfBlock));
				doSplit = TRUE;
			} else {
				doSplit = FALSE;
			}
		} else {
			if (IdeDisk->RandomReadableBlockBytes == 0) {
				bytesOfBlock = 0;
				doSplit = FALSE;
			} else {
				bytesOfBlock = IdeDisk->RandomReadableBlockBytes;
				doSplit = TRUE;
			}
		}
	} else {
		//
		// If the device is direct access device, use the block size retrieved
		// by read capacity command.
		//
		bytesOfBlock = IdeDisk->ReadCapacityBlockBytes;
		doSplit = TRUE;
	}

	status = STATUS_SUCCESS;

	// Check to see if this IO requires a lost device lock.

	lostLockIO = LockCacheCheckLostLockIORange(
		&IdeDisk->LuHwData, 
		blockAddr, 
		blockAddr + transferBlocks - 1 );

	if (lostLockIO) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Blocked by a lost lock.\n") );
		Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
		return STATUS_SUCCESS;
	}

	// Long time out

	isLongTimeout = (Ccb->CcbStatusFlags & CCBSTATUS_FLAG_LONG_COMM_TIMEOUT) != 0;

	// Clear flow control stats.

//	LurnIdeDiskResetProtocolCounters( IdeDisk );

	// Stop the IO idle timer

	StopIoIdleTimer( &IdeDisk->BuffLockCtl );

	//	Send read LSP requests split by SplitBlock factor.

	NDAS_BUGON( readSplitBlockBytes == (32*1024) || readSplitBlockBytes == (64*1024) );

	// Calculate the number of split blocks
	if(doSplit) {
		if (transferBlocks * bytesOfBlock >= 8*1024) {
			UINT32	maxSize = transferBlocks * IdeDisk->LuHwData.BlockBytes;

			if(maxSize > IdeDisk->MaxDataRecvLength)
				maxSize = IdeDisk->MaxDataRecvLength;

			chooseDataLength = NdasFcChooseSendSize( &IdeDisk->LanScsiSession->RecvNdasFcStatistics, 
				maxSize );

			startTime = NdasCurrentTime();

			splitBlocks = (chooseDataLength + bytesOfBlock - 1) / bytesOfBlock;

		} else {

			splitBlocks = transferBlocks;
		}
	} else {
		splitBlocks = transferBlocks;
	}

#if DBG
	if(doSplit) {
		NDAS_BUGON( splitBlocks * bytesOfBlock <= 65536 );
		NDAS_BUGON( (Ccb->SecondaryBufferLength % bytesOfBlock) == 0 );
	}
	else {
		NDAS_BUGON(Ccb->SecondaryBufferLength <= 65536);
	}
#endif

	if (transferBlocks >= splitBlocks ) {

		DebugTrace( NDASSCSI_DBG_LURN_IDE_TRACE, 
			("TransferBlocks = %d, splitBlocks = %d\n", transferBlocks, splitBlocks) );
	}

	transferedBlocks = 0; 
//	retryCount = 0;


	//NDAS_BUGON( FlagOn(IdeDisk->Lurn->AccessRight, GENERIC_WRITE) );

	while (transferedBlocks < transferBlocks) {

		blocksToTransfer = ((transferBlocks - transferedBlocks) < splitBlocks) ? (transferBlocks - transferedBlocks) : splitBlocks;

		NDAS_BUGON( blocksToTransfer > 0 );

#if 0
		//
		// Acquire the buffer lock for ATAPI devices regardless of CCB flags.
		// ATAPI devices's commands send data whether the access mode is read-only or not.
		//

		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK)) {
#endif
			//	Acquire the buffer lock allocated for this unit device.

			status = NdasAcquireBufferLock( &IdeDisk->BuffLockCtl,
				IdeDisk->LanScsiSession,
				&IdeDisk->LuHwData,
				NULL,
				NULL );

			if (status == STATUS_NOT_SUPPORTED) {

				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Lock is not supported\n") );

			} else if (status != STATUS_SUCCESS) {

				NDAS_BUGON( status == STATUS_PORT_DISCONNECTED );
				DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR, ("Failed to acquire the buffer lock\n") );

				Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
				break;
			}
#if 0
		}
#endif

		if(transferBlocks != splitBlocks) {
			CdbSetAddrLen((PCDB)Ccb->PKCMD, blockAddr + transferedBlocks, blocksToTransfer);
			SETUP_ATAPI_PDUDESC(
				IdeDisk,
				&pduDesc,
				Ccb->PKCMD,
				(PBYTE)Ccb->SecondaryBuffer + transferedBlocks * bytesOfBlock,
				blocksToTransfer * bytesOfBlock,
				&longTimeOut);
		} else {
			LURNIDE_ATAPI_PDUDESC( IdeDisk, &pduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb, &longTimeOut );
		}
#if 0
		if (isLongTimeout && transferedBlocks == 0) {

			longTimeout.QuadPart = LURNIDE_GENERIC_TIMEOUT * 3;
			pduDesc.TimeOut = &longTimeout;

		} else {

			pduDesc.TimeOut = NULL;
		}
#endif
		status = LspPacketRequest(
			LSS,
			&pduDesc,
			PduResponse,
			PduRegister
			);


#if 0
		//
		// Release the lock in the NDAS device.
		//

		if (LsCcbIsFlagOn(Ccb, CCB_FLAG_ACQUIRE_BUFLOCK)) {
#endif
			NdasReleaseBufferLock( &IdeDisk->BuffLockCtl,
				IdeDisk->LanScsiSession,
				&IdeDisk->LuHwData,
				NULL,
				NULL,
				FALSE,
				blocksToTransfer * bytesOfBlock );
#if 0
		}
#endif

		if (status != STATUS_SUCCESS || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {

			DebugTrace( NDASSCSI_DBG_LURN_IDE_ERROR,
				("STATUS=%08lx, response=%x,logicalBlockAddress = %I64x, transferBlocks = %d, transferedBlocks = %d\n",
				status, *PduResponse, blockAddr, transferBlocks, transferedBlocks) );

			break;
		}

		// Get the transfer log

		IdeDisk->Retransmits += pduDesc.Retransmits;
		IdeDisk->PacketLoss	+= pduDesc.PacketLoss;

		transferedBlocks += blocksToTransfer;
	}

	if (status == STATUS_SUCCESS) {

		if(doSplit) {
			if (transferBlocks * bytesOfBlock >= 8*1024) {

				endTime = NdasCurrentTime();

				NdasFcUpdateSendSize( &IdeDisk->LanScsiSession->RecvNdasFcStatistics, 
					chooseDataLength, 
					transferBlocks * bytesOfBlock,
					startTime, 
					endTime );
			}
		}

		StartIoIdleTimer( &IdeDisk->BuffLockCtl );

		if(*PduResponse == LANSCSI_RESPONSE_SUCCESS) {

			NDAS_BUGON( transferedBlocks == transferBlocks );

			if (IdeDisk->CntEcrMethod) {

				DecryptBlock(
					IdeDisk->CntEcrCipher,
					IdeDisk->CntEcrKey,
					Ccb->SecondaryBufferLength,
					Ccb->SecondaryBuffer,
					Ccb->SecondaryBuffer );
			}
		}
	}


#if __ENABLE_PERFORMACE_CHECK__

	pPFCurrentTime = KeQueryPerformanceCounter(NULL);

	if (pFCurrentTime.QuadPart - pFStartTime.QuadPart > 10000) {

		DbgPrint( "=PF= addr:%d blk=%d elaps:%I64d\n", 
			LogicalBlockAddress, TransferBlocks, pFCurrentTime.QuadPart - pFStartTime.QuadPart );
	}

#endif

	IdeDisk->Lurn->LastAccessedAddress = blockAddr + transferBlocks;

	return status;
}
