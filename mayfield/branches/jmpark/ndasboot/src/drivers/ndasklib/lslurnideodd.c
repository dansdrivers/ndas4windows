#ifdef __NDASBOOT__
#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include "ndasboot.h"
#endif

#include <ntddk.h>
#include <stdio.h>

#include "ver.h"
#include "LSKLib.h"
#include "basetsdex.h"
#include "cipher.h"
#include "hdreg.h"
#include "binparams.h"
#include "hash.h"

#include "KDebug.h"
#include "LSProto.h"
#include "LSLurn.h"
#include "LSLurnIde.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LurnIdeOdd"

//////////////////////////////////////////////////////////////////////////////////////////
//
//	IDE ODD interface
//
NTSTATUS
LurnIdeODDInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	);


NTSTATUS
LurnIdeODDRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	);

LURN_INTERFACE LurnIdeODDInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_IDE_ODD,
					0, {
						LurnIdeODDInitialize,
						LurnIdeDestroy,
						LurnIdeODDRequest
				}
	};

LURN_INTERFACE LurnIdeMOInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_IDE_MO,
					0, {
						LurnIdeODDInitialize,
						LurnIdeDestroy,
						LurnIdeODDRequest
				}
	};


/*
//
//	IDE VODD
//
NTSTATUS
LurnIdeVODDInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	);

NTSTATUS
LurnIdeVODDRequest(
		PLURELATION_NODE Lurn,
		PCCB Ccb
	);

LURN_INTERFACE LurnIdeVODDInterface = { 
					LSSTRUC_TYPE_LURN_INTERFACE,
					sizeof(LURN_INTERFACE),
					LURN_IDE_VODD,
					0, {
						LurnIdeVODDInitialize,
						LurnDestroyDefault,
						LurnIdeVODDRequest
				}
	};
*/



NTSTATUS 
ODD_Inquiry(
		PLURNEXT_IDE_DEVICE		IdeDisk
	);

//
//	confiure IDE ODD.
//
static
NTSTATUS
LurnIdeODDConfiure(
		IN	PLURELATION_NODE	Lurn,
		IN	UINT32				TargetId,
		OUT	PBYTE				PduResponse
)
{
	struct hd_driveid	info;
	char				buffer[41];
	NTSTATUS			ntStatus;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	PLURNEXT_IDE_DEVICE	IdeDisk;
	BOOLEAN				SetDmaMode;

	//
	// Init.
	//
	ASSERT(Lurn->LurnExtension);
//#if !DBG
	UNREFERENCED_PARAMETER(TargetId);
//#endif

	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;
	LSS = &IdeDisk->LanScsiSession;
	IdeDisk->LuData.PduDescFlags &= ~(PDUDESC_FLAG_LBA|PDUDESC_FLAG_LBA48|PDUDESC_FLAG_PIO|PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA);

	// identify.
	LURNIDE_INITIALIZE_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PIDENTIFY, 0, sizeof(struct hd_driveid), &info);
	ntStatus = LspRequest(LSS, &PduDesc, PduResponse);
	if(!NT_SUCCESS(ntStatus) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, ("Identify Failed...\n"));
		return ntStatus;
	}

	//
	// DMA/PIO Mode.
	//
	KDPrintM(DBG_LURN_INFO, ("Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
		TargetId, info.major_rev_num, info.minor_rev_num, info.capability)
		);
	KDPrintM(DBG_LURN_INFO, ("DMA 0x%x, U-DMA 0x%x\n", 
							info.dma_mword, 
							info.dma_ultra));


	//
	//	Default PIO Mode setting
	//
	{
		UCHAR PioMode = 0;
		UCHAR PioFeature = 0;
		PioMode = 2;
		if(info.eide_pio_modes & 0x0001)
			PioMode = 3;
		if(info.eide_pio_modes & 0x0002)
			PioMode = 4;

		PioFeature = PioMode|0x08;	
		LURNIDE_INITIALIZE_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, NULL);
		PduDesc.Feature = 0x03;
		PduDesc.Param8[0] = PioFeature;
		ntStatus = LspRequest(LSS, &PduDesc, PduResponse);
		if(!NT_SUCCESS(ntStatus) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR, ("Set Feature Failed...\n"));
			return ntStatus;
		}
		
	}
	//
	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.
	//
	SetDmaMode = FALSE;

	do {
		UCHAR	DmaFeature;
		UCHAR	DmaMode;

		DmaFeature = 0;
		DmaMode = 0;

		//
		// Ultra DMA if NDAS chip is 2.0 or higher.
		//
		if(IdeDisk->HwVersion >= LANSCSIIDE_VERSION_2_0 
			&& (info.dma_ultra & 0x00ff)
		/*	&& (Lurn->LurnType == LURN_IDE_ODD) */ 
		){
			// Find Fastest Mode.
			if(info.dma_ultra & 0x0001)
				DmaMode = 0;
			if(info.dma_ultra & 0x0002)
				DmaMode = 1;
			if(info.dma_ultra & 0x0004)
				DmaMode = 2;
			//	if Cable80, try higher Ultra Dma Mode.
#ifdef __DETECT_CABLE80__
			if(info.hw_config & 0x2000) {
#endif
				if(info.dma_ultra & 0x0008)
					DmaMode = 3;
				if(info.dma_ultra & 0x0010)
					DmaMode = 4;
				if(info.dma_ultra & 0x0020)
					DmaMode = 5;
				if(info.dma_ultra & 0x0040)
					DmaMode = 6;
				if(info.dma_ultra & 0x0080)
					DmaMode = 7;
#ifdef __DETECT_CABLE80__
			}
#endif
			KDPrintM(DBG_LURN_ERROR, ("Ultra DMA %d detected.\n", (int)DmaMode));
			DmaFeature = DmaMode | 0x40;	// Ultra DMA mode.
			IdeDisk->LuData.PduDescFlags |= PDUDESC_FLAG_DMA|PDUDESC_FLAG_UDMA;

			// Always set DMA mode again.
		//	if(!(info.dma_ultra & (0x0100 << DmaMode))) {
				SetDmaMode = TRUE;
		//	}

		//
		//	DMA
		//
		} else if(info.dma_mword & 0x00ff) {
			if(info.dma_mword & 0x0001)
				DmaMode = 0;
			if(info.dma_mword & 0x0002)
				DmaMode = 1;
			if(info.dma_mword & 0x0004)
				DmaMode = 2;

			KDPrintM(DBG_LURN_ERROR, ("DMA mode %d detected.\n", (int)DmaMode));
			DmaFeature = DmaMode | 0x20;
			IdeDisk->LuData.PduDescFlags |= PDUDESC_FLAG_DMA;

			// Always set DMA mode again.
		//	if(!(info.dma_mword & (0x0100 << DmaMode))) {
				SetDmaMode = TRUE;
		//	}

		}

		// Set DMA mode if needed.
		if(SetDmaMode) {
			LURNIDE_INITIALIZE_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_SETFEATURES, 0, 0, NULL);
			PduDesc.Feature = 0x03;
			PduDesc.Param8[0] = DmaFeature;
			ntStatus = LspRequest(LSS, &PduDesc, PduResponse);
			if(!NT_SUCCESS(ntStatus) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, ("Set Feature Failed...\n"));
				return ntStatus;
			}

			// identify.
			LURNIDE_INITIALIZE_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PIDENTIFY, 0, sizeof(struct hd_driveid), &info);
			ntStatus = LspRequest(LSS, &PduDesc, PduResponse);
			if(!NT_SUCCESS(ntStatus) || *PduResponse != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, ("Identify Failed...\n"));
				return ntStatus;
			}
			KDPrintM(DBG_LURN_INFO, ("After Set Feature DMA 0x%x, U-DMA 0x%x\n", 
							info.dma_mword, 
							info.dma_ultra));
		}
		if(IdeDisk->LuData.PduDescFlags & PDUDESC_FLAG_DMA) {
			break;
		}
		//
		//	PIO.
		//
		KDPrintM(DBG_LURN_ERROR, ("NetDisk does not support DMA mode. Turn to PIO mode.\n"));
		IdeDisk->LuData.PduDescFlags |= PDUDESC_FLAG_PIO;

	} while(0);


	//
	//	Product strings.
	//
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	RtlCopyMemory(IdeDisk->LuData.Serial, buffer, 20);
	KDPrintM(DBG_LURN_INFO, ("Serial No: %s\n", buffer));

	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	RtlCopyMemory(IdeDisk->LuData.FW_Rev, buffer, 8);
	KDPrintM(DBG_LURN_INFO, ("Firmware rev: %s\n", buffer));

	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	RtlCopyMemory(IdeDisk->LuData.Model, buffer, 40);
	KDPrintM(DBG_LURN_INFO, ("Model No: %s\n", buffer));

	IdeDisk->DVD_TYPE = 0;

	if(Lurn->LurnType == LURN_IDE_ODD)
	{
		if( strlen(IO_DATA_STR) == RtlCompareMemory(buffer,IO_DATA_STR, strlen(IO_DATA_STR)) )
		{
			KDPrintM(DBG_LURN_INFO, ("Find: IO_DATA_DEVICE: %s\n", buffer));
			IdeDisk->DVD_TYPE = IO_DATA;
		}else if( strlen(LOGITEC_STR) == RtlCompareMemory(buffer, LOGITEC_STR, strlen(LOGITEC_STR)) )
		{
			KDPrintM(DBG_LURN_INFO, ("Find: LOGITEC: %s\n", buffer));
			IdeDisk->DVD_TYPE = LOGITEC;
		}else if( strlen(IO_DATA_STR_9573) == RtlCompareMemory(buffer, IO_DATA_STR_9573, strlen(IO_DATA_STR_9573)) )
		{
			KDPrintM(DBG_LURN_INFO, ("Find: LOGITEC: %s\n", buffer));
			IdeDisk->DVD_TYPE = IO_DATA_9573;
		}
	}

	//
	// Support LBA?
	//
	if(!(info.capability & 0x02)) {
		IdeDisk->LuData.PduDescFlags &= ~PDUDESC_FLAG_LBA;
		ASSERT(FALSE);
	} else {
		IdeDisk->LuData.PduDescFlags |= PDUDESC_FLAG_LBA;
	}

	//
	// Support LBA48.
	// Calc Capacity.
	// 
	if((info.command_set_2 & 0x0400) && (info.cfs_enable_2 & 0x0400)) {	// Support LBA48bit
		IdeDisk->LuData.PduDescFlags |= PDUDESC_FLAG_LBA48;
		IdeDisk->LuData.SectorCount = info.lba_capacity_2;
	} else {
		IdeDisk->LuData.PduDescFlags &= ~PDUDESC_FLAG_LBA48;

		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			IdeDisk->LuData.SectorCount = info.lba_capacity;
		} else
			IdeDisk->LuData.SectorCount = info.cyls * info.heads * info.sectors;	
	}

	KDPrintM(DBG_LURN_INFO, ("LBA %d, LBA48 %d, Number of Sectors: %I64d\n", 
				(IdeDisk->LuData.PduDescFlags & PDUDESC_FLAG_LBA) != 0,
				(IdeDisk->LuData.PduDescFlags & PDUDESC_FLAG_LBA48) != 0,
				IdeDisk->LuData.SectorCount
		));

/*
	ntStatus = ODD_Inquiry( IdeDisk );
	if(!NT_SUCCESS(ntStatus))
	{
		return STATUS_UNSUCCESSFUL;
	}
*/
	return STATUS_SUCCESS;
}



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

		KDPrintM(DBG_LURN_ERROR,("MODE_SENSE/SELECT PKCMD 0x%02x\n",c[0]));

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

			KDPrintM(DBG_LURN_ERROR,("First mode page:0x%02x Original DataBuffer size=%d, Data size in Command=%d\n", (int)sc_buf[4], Ccb->DataBufferLength, (int)sc[4]));

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

#ifdef __BSRECORDER_HACK__
		//
		//	Fix the MODE_READ_WRITE_RECOVERY_PAGE in MODE_SELECT command.
		//	The command comes from B's clip eraser with DVD+RW.
		//
		if(	sc[0] == SCSIOP_MODE_SELECT ) {

			if(	buf[8] == 0x01 &&
				buf[9] == 0x06
			) {

			KDPrintM(DBG_LURN_ERROR,("MODE_READ_WRITE_RECOVERY_PAGE(0x01) in length of 0x06! Fix it!!!!!!!\n"));
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

				KDPrintM(DBG_LURN_ERROR,("MODE_WRITE_PARAMETER_PAGE(0x01) with the wrong legnth in the command! Fix it!!!!!!!\n"));
				c[8] -= 3;
				Ccb->SecondaryBufferLength -= 3;
			}
		}
#endif

	}

#ifdef __BSRECORDER_HACK__
	//
	//	0xD8 vendor command to READ_CD.
	//
	if(c[0] == 0xD8) {
		ULONG	TransferLength;
		ULONG	DataBufferLength;

		TransferLength = ((ULONG)c[6]<<24) | ((ULONG)c[7]<<16) | ((ULONG)c[8]<<8) | c[9];
		DataBufferLength = Ccb->SecondaryBufferLength;

		if((DataBufferLength / TransferLength) == 2352) {
			KDPrintM(DBG_LURN_INFO,("Vendor PKCMD 0x%02x to READ_CD(0xBE)\n",c[0]));

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

#ifdef	__DVD_ENCRYPT_CONTENT__
	if(
		(IdeDisk->Lurn->LurnType == LURN_IDE_ODD)
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



	if(c[0] == 0x46)
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
			KDPrintM(DBG_LURN_ERROR,("CONGIFURATION: (%02x):(%02x):(%02x):(%02x): (%02x):(%02x):(%02x):(%02x)",
				sc_buf[i*8+0],sc_buf[i*8+1],sc_buf[i*8+2],sc_buf[i*8+3],sc_buf[i*8+4],sc_buf[i*8+5],sc_buf[i*8+6],sc_buf[i*8+7]));
		}
		
	}


	if(c[0] == 0x25)
	{
		int i,max = 1;
		for(i = 0; i< max; i++)
		{
			KDPrintM(DBG_LURN_ERROR,("CAPACITY: (%02x):(%02x):(%02x):(%02x): (%02x):(%02x):(%02x):(%02x)",
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
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			
		}
		*/

		
	}
	//	Additional processing  
	//	LOGITEC  (HITACHI-GSA-4082 MODEL):  READ_TOC : 0x43
	//	Because LOGITEC Make Problem for playing MUCIS CD burned with easy CD creator	


	if(IdeDisk->DVD_TYPE == LOGITEC)
	{

		if((c[0] == 0x43)
			&& (c[2] == 0x05))
		{
			PBYTE	buf;
			buf = Ccb->DataBuffer;
			buf[0] = 0x0;
			buf[1] = 0x2;
		}

	}

	if(buf && buf != sc_buf)
		ExFreePool(buf);

}




#define WAIT_DELAY		(FREQ_PER_SEC * 1)
static 
void
ODD_BusyWait(LONG time)
{


		LARGE_INTEGER TimeInterval = {0,0};

		TimeInterval.QuadPart = -(time * FREQ_PER_SEC);

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
static NTSTATUS
ODD_GetStatus(
		PLURNEXT_IDE_DEVICE		IdeDisk,
		PCCB					Ccb,
		PSENSE_DATA				pSenseData,
		LONG					SenseDataLen
			  )
{

	NTSTATUS			ntStatus;
	BYTE				response;
	CCB					tempCCB;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	LARGE_INTEGER		GenericTimeOut;
	UCHAR				Reg[2];

	UNREFERENCED_PARAMETER(Ccb);

	if(SenseDataLen > 255) {
		KDPrintM(DBG_LURN_ERROR, ("Too big SenseDataLen. %d\n", SenseDataLen));
		SenseDataLen = 255;
	}

	RtlZeroMemory(&tempCCB.Cdb,12);
	RtlZeroMemory((char *)pSenseData,sizeof(SENSE_DATA));

	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_REQUEST_SENSE;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.LogicalUnitNumber = IdeDisk->LuData.LanscsiLU;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.AllocationLength = (UCHAR)SenseDataLen;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Control = 0;

	RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 6);
	tempCCB.SecondaryBuffer = pSenseData;
	tempCCB.SecondaryBufferLength = SenseDataLen;


	IdeDisk->ODD_STATUS = NON_STATE;

	LSS = &IdeDisk->LanScsiSession;

	GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
	LspSetTimeOut(LSS, &GenericTimeOut);

	LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB);
	ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);
	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(LSS, &GenericTimeOut);

	if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
					IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
	}

	if(!NT_SUCCESS(ntStatus))
	{
		KDPrintM(DBG_LURN_ERROR, ("Command Fail 0x%x\n", ntStatus)); 
		return ntStatus;								
	}

	switch(pSenseData->SenseKey){
		case 0x0:
			{
				IdeDisk->ODD_STATUS = READY;
			}
			break;
		case 0x02:
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case 0x3A:
					{
						IdeDisk->ODD_STATUS = MEDIA_NOT_PRESENT;
					}
					break;
				case 0x30:
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
				case 0x04:
					{
						switch(pSenseData->AdditionalSenseCodeQualifier)
						{
						case 0x0:
							{
								IdeDisk->ODD_STATUS = NOT_READY;
							}
							break;
						case 0x04:
						case 0x07:
						case 0x08:
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
		case 0x03:
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
		case 0x05:
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case 0x20:
					IdeDisk->DVD_STATUS = INVALID_COMMAND;
					break;
				default:
					break;
				}
			}
			break;
		case 0x06:
			{
				switch(pSenseData->AdditionalSenseCode)
				{
				case 0x28:
					{
						IdeDisk->ODD_STATUS = NOT_READY_MEDIA;
					}
					break;
				case 0x29:
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
			)
{
	NTSTATUS			ntStatus;
	BYTE				response;
	CCB					tempCCB;
	LANSCSI_PDUDESC		PduDesc;
	PLANSCSI_SESSION	LSS;
	LARGE_INTEGER		GenericTimeOut;
	UCHAR				Data[512];
	UCHAR				Reg[2];


	RtlZeroMemory(tempCCB.Cdb, sizeof(tempCCB.Cdb));
	RtlZeroMemory((char *)Data,512);

	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.LogicalUnitNumber = IdeDisk->LuData.LanscsiLU;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Reserved1 = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.PageCode = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.IReserved = 0;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.AllocationLength = 36;
	((PCDB)&tempCCB.Cdb)->CDB6INQUIRY.Control = 0;

	RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 6);
	tempCCB.SecondaryBuffer = Data;
	tempCCB.SecondaryBufferLength = 36;

	LSS = &IdeDisk->LanScsiSession;

	GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
	LspSetTimeOut(LSS, &GenericTimeOut);

	LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB);
	ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(LSS, &GenericTimeOut);

	if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
				IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}



typedef struct _MY_GET_CONFIGURATION {
	UCHAR OperationCode;       // 0x46 - SCSIOP_GET_CONFIGURATION
	UCHAR RequestType : 1;     // SCSI_GET_CONFIGURATION_REQUEST_TYPE_*
	UCHAR Reserved1   : 7;     // includes obsolete LUN field
	UCHAR StartingFeature[2];
	UCHAR Reserved2[3];
	UCHAR AllocationLength[2];
	UCHAR Control;
} MY_GET_CONFIGURATION, *PMY_GET_CONFIGURATION;

typedef struct _GET_CONFIGURATION_HEADER {
	UCHAR DataLength[4];      // [0] == MSB, [3] == LSB
	UCHAR Reserved[2];
	UCHAR CurrentProfile[2];  // [0] == MSB, [1] == LSB
} GET_CONFIGURATION_HEADER, *PGET_CONFIGURATION_HEADER;

typedef struct _FEATURE_HEADER {
	UCHAR FeatureCode[2];     // [0] == MSB, [1] == LSB
	UCHAR Current    : 1;     // The feature is currently active
	UCHAR Persistent : 1;     // The feature is always current
	UCHAR Version    : 4;
	UCHAR Reserved0  : 2;
	UCHAR AdditionalLength;   // sizeof(Header) + AdditionalLength = size
} FEATURE_HEADER, *PFEATURE_HEADER;

// 0x0010 - FeatureRandomReadable
typedef struct _FEATURE_DATA_RANDOM_READABLE {
	FEATURE_HEADER Header;
	UCHAR LogicalBlockSize[4];
	UCHAR Blocking[2];
	UCHAR ErrorRecoveryPagePresent : 1;
	UCHAR Reserved1                : 7;
	UCHAR Reserved2;
} FEATURE_DATA_RANDOM_READABLE, *PFEATURE_DATA_RANDOM_READABLE;


typedef struct _MY_GET_RAWSECTORCOUT{
	GET_CONFIGURATION_HEADER ConfigHdr;
	FEATURE_DATA_RANDOM_READABLE	Data;
}MY_GET_RAWSECTORCOUT, *PMY_GET_RAWSECTORCOUT;

typedef struct _FEATURE_DATA_RANDOM_WRITABLE {
	FEATURE_HEADER Header;
	UCHAR LBA[4];
	UCHAR LogicalBlockSize[4];
	UCHAR Blocking[2];
	UCHAR ErrorRecoveryPagePresent : 1;
	UCHAR Reserved1                : 7;
	UCHAR Reserved2;
} FEATURE_DATA_RANDOM_WRITABLE, *PFEATURE_DATA_RANDOM_WRITABLE;

typedef struct _MY_GET_CAPACITY{
	GET_CONFIGURATION_HEADER ConfigHdr;
	FEATURE_DATA_RANDOM_WRITABLE	Data;
}MY_GET_CAPACITY, *PMY_GET_CAPACITY;

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
	LARGE_INTEGER		GenericTimeOut;

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

	LSS = &IdeDisk->LanScsiSession;

	// initialize
	IdeDisk->DataSectorSize = 0;

	GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
	LspSetTimeOut(LSS, &GenericTimeOut);

	LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB);		
	ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

	IdeDisk->RegError = Reg[0];
	IdeDisk->RegStatus = Reg[1];

	GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(LSS, &GenericTimeOut);

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

		IdeDisk->DataSectorSize = BlkSize;
		KDPrintM(DBG_LURN_ERROR,("SECTOR SIZE %d\n",BlkSize));
	}



	return;

}

static void
ODD_GetCapacity(
		PLURNEXT_IDE_DEVICE		IdeDisk
			  )
{
		NTSTATUS			ntStatus;
		BYTE				response;
		CCB					tempCCB;
		LANSCSI_PDUDESC		PduDesc;
		PLANSCSI_SESSION	LSS;
		LARGE_INTEGER		GenericTimeOut;
		
		PMY_GET_CONFIGURATION   pConfiguration;
		MY_GET_CAPACITY			RowCapacity;
		ULONG			 BlkSize = 0;
		ULONG			 LBA	= 0;
		UCHAR			 Reg[2];

		RtlZeroMemory(&tempCCB, sizeof(CCB));
		RtlZeroMemory((char *)&RowCapacity,sizeof(MY_GET_CAPACITY));
		
		pConfiguration = (PMY_GET_CONFIGURATION)&tempCCB.Cdb;
		pConfiguration->OperationCode = 0x46;
		pConfiguration->StartingFeature[0] = 0x0;
		pConfiguration->StartingFeature[1] = 0x20;
		pConfiguration->AllocationLength[0] = 0;
		pConfiguration->AllocationLength[1] = sizeof(MY_GET_CAPACITY);
		pConfiguration->Control = 0;
		
		
		RtlCopyMemory(tempCCB.PKCMD, tempCCB.Cdb, 10);
		tempCCB.SecondaryBuffer = &RowCapacity;
		tempCCB.SecondaryBufferLength = sizeof(MY_GET_CAPACITY);
		
		LSS = &IdeDisk->LanScsiSession;

		// initialize
		IdeDisk->DataSectorSize = 0;

		GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
		LspSetTimeOut(LSS, &GenericTimeOut);

		LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, &tempCCB);		
		ntStatus = LspPacketRequest(
							LSS,
							&PduDesc,
							&response,
							Reg
							);

		IdeDisk->RegError = Reg[0];
		IdeDisk->RegStatus = Reg[1];
		
		GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
		LspSetTimeOut(LSS, &GenericTimeOut);

		if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
			KDPrintM(DBG_LURN_ERROR, 
				("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
						IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
			return;
		}
	
		
		
		if(
			(RowCapacity.Data.Header.FeatureCode[0] == 0x0)
			&&(RowCapacity.Data.Header.FeatureCode[1] == 0x20)
			)
		{
			LBA |= 	(RowCapacity.Data.LBA[0] << 24);	
			LBA |=  (RowCapacity.Data.LBA[1] << 16);
			LBA |=  (RowCapacity.Data.LBA[2] << 8);
			LBA |=  RowCapacity.Data.LBA[3];
			IdeDisk->DataSectorCount = LBA;
			KDPrintM(DBG_LURN_ERROR,("LBA SIZE %x\n",LBA));

			BlkSize |= 	(RowCapacity.Data.LogicalBlockSize[0] << 24);	
			BlkSize |=  (RowCapacity.Data.LogicalBlockSize[1] << 16);
			BlkSize |=  (RowCapacity.Data.LogicalBlockSize[2] << 8);
			BlkSize |=  RowCapacity.Data.LogicalBlockSize[3];
			
			IdeDisk->DataSectorSize = BlkSize;
			KDPrintM(DBG_LURN_ERROR,("SECTOR SIZE %x\n",BlkSize));
		}



		return;

}

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

#define MAX_WAIT_COUNT	5
//
//
//
static
NTSTATUS
LurnIdeODDExecute(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeDisk,
		IN	PCCB				Ccb
	)
{

	NTSTATUS			ntStatus;
	ULONG				NeedRepeat;
	PLANSCSI_SESSION	LSS;

	LSS = &IdeDisk->LanScsiSession;
	KDPrintM(DBG_LURN_NOISE, 
			("Entered\n"));

	KDPrintM(DBG_LURN_NOISE, 
			("CMD 0x%02x\n", Ccb->Cdb[0]));

	NeedRepeat = 0;


	// Processing Busy State
	if(IdeDisk->RegStatus & BUSY_STAT)
	{
		SENSE_DATA SenseData;
		ULONG WaitCount = 0;
BUSY_WAIT:
		WaitCount++;

		if(WaitCount < MAX_WAIT_COUNT) 
		{
			KDPrintM(DBG_LURN_ERROR,("time Waiting command 0x%02x Waiting Count %d\n", IdeDisk->PrevRequest, WaitCount));
			ODD_BusyWait(1);
			ntStatus = ODD_GetStatus(
				IdeDisk,
				Ccb,
				&SenseData,
				sizeof(SENSE_DATA)
				);

			if(IdeDisk->RegStatus & BUSY_STAT)	goto BUSY_WAIT;
		}
	}



	// check Long-term operation done
	if(
		(IdeDisk->PrevRequest == 0x04)
		|| (IdeDisk->PrevRequest == 0xa1)
		|| (IdeDisk->PrevRequest == 0x35)
		|| (IdeDisk->PrevRequest == 0x5b)
		|| (IdeDisk->PrevRequest == 0x53)
		)
	{

		if((Ccb->Cdb[0] != 0x0)
			&& (Ccb->Cdb[0] != 0x4a)
			&& (Ccb->Cdb[0] != 0x46)
			&& (Ccb->Cdb[0] != 0x03)
			&& (Ccb->Cdb[0] != 0x5c)
			)
		{
			SENSE_DATA		SenseData;

			ntStatus = ODD_GetStatus(
				IdeDisk,
				Ccb,
				&SenseData,
				sizeof(SENSE_DATA)
				);

			if(IdeDisk->DVD_REPEAT_COUNT < MAX_REFEAT_COUNT )
			{
				if(IdeDisk->ODD_STATUS == NOT_READY_PRESENT){
					IdeDisk->DVD_REPEAT_COUNT ++;
					ODD_BusyWait(2);
					KDPrintM(DBG_LURN_ERROR,("Retry command 0x%02x Retry Count %d\n", Ccb->PKCMD[0], IdeDisk->DVD_REPEAT_COUNT));	
					//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
					LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
					return STATUS_SUCCESS;			
				}
			}	
			IdeDisk->DVD_REPEAT_COUNT = 0;
		}
	}


	if(Ccb->Cdb[0] == SCSIOP_READ_CAPACITY)
	{
//		ODD_GetSectorCount(IdeDisk);
		ODD_GetCapacity(IdeDisk);
	}


	TranslateScsi2Packet(Ccb);
	ntStatus = ProcessIdePacketRequest(Lurn, IdeDisk, Ccb, &NeedRepeat);

	TranslatePacket2Scsi(IdeDisk,Ccb);

	if((Ccb->Cdb[0] == SCSIOP_INQUIRY)
		|| (Ccb->Cdb[0] == SCSIOP_START_STOP_UNIT)
		|| (Ccb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE)
		|| (Ccb->Cdb[0] == SCSIOP_SEEK)
		|| (Ccb->Cdb[0] == SCSIOP_TEST_UNIT_READY)
		)
	{
		LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_TIMER_COMPLETE);
	}

	if(NeedRepeat == 1)
	{
		IdeDisk->DVD_REPEAT_COUNT ++;
		KDPrintM(DBG_LURN_ERROR,("Retry command 0x%02x Retry Count %d\n", Ccb->PKCMD[0], IdeDisk->DVD_REPEAT_COUNT));	
		return STATUS_SUCCESS;
	}else{
		IdeDisk->DVD_REPEAT_COUNT = 0;
	}

	IdeDisk->PrevRequest = Ccb->Cdb[0];


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
	LARGE_INTEGER		GenericTimeOut;
	UCHAR				Reg[2];


	LSS = &IdeDisk->LanScsiSession;
	*NeedRepeat = 0;
	Command = Ccb->PKCMD[0];



	KDPrintM(DBG_LURN_TRACE,("PKCMD [0x%02x]\n", Command));

//	if((Ccb->PKCMD[0] != 0x0)
//		&& (Ccb->PKCMD[0] != 0x4a)
//		&& (Ccb->PKCMD[0] != SCSIOP_START_STOP_UNIT)
//		&& (Ccb->PKCMD[0] != 0xa1)
//		)
	{
		UCHAR	* buf;
		PSCSI_REQUEST_BLOCK Srb;
		buf = Ccb->PKCMD;
		KDPrintM(DBG_LURN_ERROR,(
						"Ccb=%p:%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x : PKDataBufferlen (%d)\n",
						Ccb,buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],Ccb->SecondaryBufferLength));
		if(Ccb->Srb)
		{
			Srb = Ccb->Srb;
#if DBG
			if(Srb->TimeOutValue < 20)
				KDPrintM(DBG_LURN_INFO,("Max time out value %d\n",Srb->TimeOutValue));
			if(Srb->SenseInfoBufferLength < FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) {
				KDPrintM(DBG_LURN_INFO,("SenseInfoBufferSize:%d sizeof(SENSE_DATA):%d Mandatory size:%d\n",Srb->SenseInfoBufferLength, sizeof(SENSE_DATA), FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)));
			}
#endif
		}
	}

	// Pre processing
	// step 1 check if current is w processing
	if(Lurn->LurnType == LURN_IDE_ODD)
	{
		switch(Command) {
		case SCSIOP_FORMAT_UNIT:
		case SCSIOP_WRITE:
		case SCSIOP_WRITE_VERIFY:
		case 0xaa:
		case SCSIOP_MODE_SENSE10:
		case SCSIOP_READ:
		case SCSIOP_READ_DATA_BUFF:	
		case SCSIOP_READ_CD:
		case SCSIOP_READ_CD_MSF:
		case 0xa8:
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
	case 0x5b:	//	CLOSE TRACK/SESSION/RZONE
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
	case 0xa1:	//	BLANK
		{
			//
			// If not give Immediate flag, 
			// the SW9583-C device reset in erase process of B's recorder gold7.
			//
//			Ccb->PKCMD[1] = Ccb->PKCMD[1] & 0xEF;
			Ccb->SecondaryBufferLength = 0;
			
			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				PSENSE_DATA	senseData;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					senseData = Ccb->SenseBuffer;

					senseData->ErrorCode = 0x70;
					senseData->Valid = 1;
					senseData->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;	//SCSI_SENSE_MISCOMPARE;
					senseData->AdditionalSenseLength = 0xb;
					senseData->AdditionalSenseCode = 0x26;
					senseData->AdditionalSenseCodeQualifier = 0;

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
			KDPrintM(DBG_LURN_ERROR,("[SCSIOP FORMAT UNIT DATA]"
					"%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x : SecondaryBufferlen (%d)\n",
					buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11],Ccb->SecondaryBufferLength));

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				PSENSE_DATA	senseData;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					senseData = Ccb->SenseBuffer;

					senseData->ErrorCode = 0x70;
					senseData->Valid = 1;
					senseData->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;	//SCSI_SENSE_MISCOMPARE;
					senseData->AdditionalSenseLength = 0xb;
					senseData->AdditionalSenseCode = 0x26;
					senseData->AdditionalSenseCodeQualifier = 0;

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
							LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
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
//				[SCSIOP FORMAT UNIT DATA](0)[0x00],(1)[0x82],(2)[0x00],(3)[0x08],(4)[0x00],(5)[0x23],(6)[0x05],(7)[0x40],(8)[0x98],(9)[0x00],(10)[0x00],(11)[0x00]
//			and Format use this parameter
//			IO-data model SW-9583_C support but GSA-4082B not support
//
//
				FormatType = (buf[8] >> 2);
				KDPrintM(DBG_LURN_ERROR,("FormatType 0x%02x\n",FormatType));

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
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);	
								return STATUS_SUCCESS;
						}

						LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
				PSENSE_DATA	senseData;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Error Read only Access !!!\n",Command));

				if( (Ccb->SenseBuffer)
					&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
				{
					senseData = Ccb->SenseBuffer;

					senseData->ErrorCode = 0x70;
					senseData->Valid = 1;
					senseData->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;	//SCSI_SENSE_MISCOMPARE;
					senseData->AdditionalSenseLength = 0xb;
					senseData->AdditionalSenseCode = 0x26;
					senseData->AdditionalSenseCodeQualifier = 0;

					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}

				LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
						LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);	
						return STATUS_SUCCESS;
					}

					LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
					return STATUS_SUCCESS;

				}
			}

			if(transferLen >4096)
			{
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
				KDPrintM(DBG_LURN_NOISE,("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
				transferLen = 4096;
				Ccb->PKCMD[8] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[9] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
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
	case 0xaa:
		{

			if(!(Lurn->AccessRight & GENERIC_WRITE))
			{
				KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Read only Access !!!\n",Command));
				if(Lurn->Lur->LurFlags & LURFLAG_FAKEWRITE)
				{
					LSCcbSetStatus(Ccb,	CCB_STATUS_SUCCESS);
					return STATUS_SUCCESS;
				}else{
					ASSERT(FALSE);
					LSCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}
			}


#ifdef	__DVD_ENCRYPT_CONTENT__
		if(Lurn->LurnType == LURN_IDE_ODD)
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
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Error Read only Access !!!\n",Command));

				if(buf[8] == 0x05)
				{
					PSENSE_DATA	senseData;

					if( (Ccb->SenseBuffer)
						&& ( Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode)) )
					{
						senseData = Ccb->SenseBuffer;

						senseData->ErrorCode = 0x70;
						senseData->Valid = 1;
						senseData->SenseKey = SCSI_SENSE_ILLEGAL_REQUEST;	//SCSI_SENSE_MISCOMPARE;
						senseData->AdditionalSenseLength = 0xb;
						senseData->AdditionalSenseCode = 0x26;
						senseData->AdditionalSenseCodeQualifier = 0;

						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCcbSetStatus(Ccb,	CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;
					}

					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
				}
			}

			for(i=0; i<length; i++)
				KDPrintM(DBG_LURN_ERROR,("[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x):[%d](%02x)\n",
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


				if(MinimumLen >4096)
				{
					KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, MinimumLen));
					KDPrintM(DBG_LURN_NOISE,("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
					MinimumLen = 4096;
					Ccb->PKCMD[7] = (BYTE)((MinimumLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (BYTE)(MinimumLen & 0xff);
					if(PageCodeLen != 0)
					{
						buf[0] = (BYTE)(((MinimumLen -2) >> 8) & 0xff);
						buf[1] = (BYTE)((MinimumLen -2) & 0xff);
						buf[9] = (BYTE)(MinimumLen - (10));
					}
					Ccb->PKDataBufferLength = MinimumLen;
					KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, MinimumLen));
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
	case 0x46:	// Get Configuration
	case SCSIOP_READ_TOC:
	case SCSIOP_READ_TRACK_INFORMATION:
	case SCSIOP_READ_SUB_CHANNEL:
		{

			USHORT transferLen = 0;
			transferLen |= (Ccb->PKCMD[7] << 8);
			transferLen |= (Ccb->PKCMD[8]);

			if(Command == SCSIOP_READ_TOC)
			{

				if((IdeDisk->DVD_TYPE == LOGITEC)
					&& (Ccb->PKCMD[9] & 0x80 ))
				{
					KDPrintM(DBG_LURN_ERROR,("[READ_TOC CHANGE]Ccb->PKCMD[9] = (Ccb->PKCMD[9] & 0x3F)\n"));
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);	
					return STATUS_SUCCESS;
				}

			}


			if(transferLen >4096)
			{
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
				KDPrintM(DBG_LURN_NOISE,("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
				transferLen = 4096;
				Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[8] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
			}


			if(transferLen != Ccb->SecondaryBufferLength)
			{

				if(Ccb->SecondaryBufferLength < transferLen)
				{
					KDPrintM(DBG_LURN_INFO,("Set new transfer length into the scsi command:%s. From %d to %d.\n",
											CdbOperationString(Command),
											transferLen,
											Ccb->SecondaryBufferLength
									));
					transferLen = (USHORT)Ccb->SecondaryBufferLength;
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);

				}else if(Ccb->SecondaryBufferLength > transferLen)
				{
					KDPrintM(DBG_LURN_INFO,("Set new transfer length into the scsi command:%s. From %d to %d.\n",
											CdbOperationString(Command),
											Ccb->SecondaryBufferLength,
											transferLen
									));
					Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
					Ccb->PKCMD[8] = (transferLen & 0xff);
					Ccb->SecondaryBufferLength = transferLen;
				}
			}
		}
		break;

	case SCSIOP_READ_CAPACITY:
		if(Ccb->SecondaryBufferLength > 8) Ccb->SecondaryBufferLength = 8;
		break;

	case 0x4a: // GET EVENT NOTIFICATION
		{
			UINT16 transferLen = 0;
			transferLen |= (Ccb->PKCMD[7] << 8);
			transferLen |= (Ccb->PKCMD[8]);


			if(transferLen > 256)
			{
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
				KDPrintM(DBG_LURN_NOISE,("PKCMD 0x%02x : buffer size %d\n", Command, transferLen));
				transferLen = 256;
				Ccb->PKCMD[7] = ((transferLen >> 8) & 0xff);
				Ccb->PKCMD[8] = (transferLen & 0xff);
				Ccb->SecondaryBufferLength = transferLen;
				KDPrintM(DBG_LURN_INFO,("Command (0x%02x) Length change Transfer length %d!!!\n",Command, transferLen));
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

#ifdef __BSRECORDER_HACK__
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
						KDPrintM(DBG_LURN_ERROR,("[SCSIOP_READ] changed size %d\n",TransferSize));
						Ccb->SecondaryBufferLength = TransferSize;
					}

				}
			}
		}
		break;

	case 0xA8:
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
						KDPrintM(DBG_LURN_ERROR,("[SCSIOP_READ10] changed size %d\n",TransferSize));
						Ccb->SecondaryBufferLength = TransferSize;
					}

				}
			}
		}
		break;
#endif // __BSRECORDER_HACK__

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

			KDPrintM(DBG_LURN_INFO,("size of allocated buffer %d\n",lunsize));

			luncount = luncount * 8;

			lunlist = (PLUN_LIST)Ccb->SecondaryBuffer;
			lunlist->LunListLength[0] = ((luncount >> 24)  & 0xff);
			lunlist->LunListLength[1] = ((luncount >> 16)  & 0xff);
			lunlist->LunListLength[2] = ((luncount >> 8) & 0xff);
			lunlist->LunListLength[3] = ((luncount >> 0) & 0xff); 

			RtlZeroMemory(lunlist->Lun,8);
			LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
			return STATUS_SUCCESS;


		}
		break;


	case SCSIOP_READ_CAPACITY:
		{
			LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb);

			GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
			LspSetTimeOut(LSS, &GenericTimeOut);

			ntStatus = LspPacketRequest(
								LSS,
								&PduDesc,
								&response,
								Reg
								);	

			IdeDisk->RegError = Reg[0];
			IdeDisk->RegStatus = Reg[1];

			GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
			LspSetTimeOut(LSS, &GenericTimeOut);

			if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, 
					("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
							IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
			}

			if(!NT_SUCCESS(ntStatus))
			{
				if(ntStatus == STATUS_REQUEST_ABORTED){
					KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					IdeDisk->ODD_STATUS = NON_STATE;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					return STATUS_REQUEST_ABORTED;
				}
				KDPrintM(DBG_LURN_ERROR,("Communication error! NTSTATUS:%08lx\n",ntStatus));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
				return ntStatus;
			}else{

				RegError = IdeDisk->RegError;
				RegStatus = IdeDisk->RegStatus;

				if(response != LANSCSI_RESPONSE_SUCCESS)
					KDPrintM(DBG_LURN_ERROR, 
						("Error response Command 0x%02x : response 0x%x\n", Command,response));

				switch(response) {
				case LANSCSI_RESPONSE_SUCCESS:
					{

						LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
						return STATUS_SUCCESS;
					}
					break;
				case LANSCSI_RESPONSE_T_NOT_EXIST:
					LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BAD_COMMAND:
					LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BROKEN_DATA:
				case LANSCSI_RESPONSE_T_COMMAND_FAILED:
					{
						if((Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))
							&& (Ccb->SenseBuffer)
							)
						{
							PSENSE_DATA pSenseData;

							NTSTATUS status;

							status = ODD_GetStatus(
											IdeDisk,
											Ccb,
											(PSENSE_DATA)Ccb->SenseBuffer,
											Ccb->SenseDataLength
											);
							if(!NT_SUCCESS(status))
							{
								if(status == STATUS_REQUEST_ABORTED){
									KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
									IdeDisk->ODD_STATUS = NON_STATE;
									LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
									return STATUS_SUCCESS;
								}
								KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
								return status;
							}

							pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);


							KDPrintM(DBG_LURN_INFO,("PKCMD (0x%02x), RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
								Ccb->PKCMD[0], RegStatus, RegError,pSenseData->ErrorCode, pSenseData->SenseKey,
								pSenseData->AdditionalSenseCode, pSenseData->AdditionalSenseCodeQualifier));

							if(IdeDisk->ODD_STATUS == RESET_POWERUP)
							{
								KDPrintM(DBG_LURN_INFO,("POWER RESET 4 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								return STATUS_SUCCESS;
							}


							
							if(IdeDisk->ODD_STATUS == NOT_READY_MEDIA)
							{
								KDPrintM(DBG_LURN_INFO,("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								return STATUS_SUCCESS;
							}

							if(IdeDisk->ODD_STATUS == NOT_READY_PRESENT)				
							{
								if(IdeDisk->DVD_MEDIA_TYPE != 0x12){
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								}else{
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
								}
								return STATUS_SUCCESS;
							}
							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							
							if(IdeDisk->DVD_MEDIA_TYPE != 0x12){
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
							}else{
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
							}
							
							return STATUS_SUCCESS;
		

						}
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
						return STATUS_SUCCESS;
					}
				default:
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					return STATUS_SUCCESS;
					break;
				}
			}


		}
		break;

	case 0x5d: // SEND_CUE_SHEET
	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case 0xaa:	// WRITE12
	case 0x51:
	case 0x52:
		{
			PSCSI_REQUEST_BLOCK Srb;

			LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb);
			Srb = Ccb->Srb;

			GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
			LspSetTimeOut(LSS, &GenericTimeOut);


			ntStatus = LspPacketRequest(
								LSS,
								&PduDesc,
								&response,
								Reg
								);

			IdeDisk->RegError = Reg[0];
			IdeDisk->RegStatus = Reg[1];

			GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
			LspSetTimeOut(LSS, &GenericTimeOut);

			if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, 
					("PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
							IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
			}

			if(!NT_SUCCESS(ntStatus))
			{
				if(ntStatus == STATUS_REQUEST_ABORTED){
					KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));					
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					IdeDisk->ODD_STATUS = NON_STATE;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					return STATUS_REQUEST_ABORTED;
				}
				KDPrintM(DBG_LURN_ERROR,("Communication error! NTSTATUS:%08lx\n",ntStatus));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
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
					LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_NOT_EXIST:
					LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BAD_COMMAND:
					LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
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
									KDPrintM(DBG_LURN_ERROR,("Waiting for a while!!\n",Command));
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
									KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
									IdeDisk->ODD_STATUS = NON_STATE;
									LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
									return STATUS_SUCCESS;
								}
								KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
								return status;
							}

							pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);

							KDPrintM(DBG_LURN_INFO,("ProcessIdePacketRequest: PKCMD (0x%02x), RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
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

									*NeedRepeat = 1;
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);

									if(Srb){
										KDPrintM(DBG_LURN_ERROR,("Srb Timeout Value %d\n",Srb->TimeOutValue));
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
								KDPrintM(DBG_LURN_INFO,("POWER RESET 3 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								pSenseData->ErrorCode = 0x0;
								LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
							    return STATUS_SUCCESS;

							}else if(IdeDisk->ODD_STATUS == NOT_READY_MEDIA)
							{
								KDPrintM(DBG_LURN_INFO,("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								return STATUS_SUCCESS;

							}else if ((pSenseData->SenseKey == 0x05)
											&& (pSenseData->AdditionalSenseCode == 0x21)
											&& ( pSenseData->AdditionalSenseCodeQualifier == 0x02)
											)
							{
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								return STATUS_SUCCESS;								
							}

							LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
							return STATUS_SUCCESS;		

						}
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
						return STATUS_SUCCESS;
					}
				default:
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
						return STATUS_SUCCESS;
					break;
				}
			}
		}
		break;
	default:
		{

			PSCSI_REQUEST_BLOCK Srb;

			LURNIDE_ATAPI_PDUDESC(IdeDisk, &PduDesc, IDE_COMMAND, WIN_PACKETCMD, Ccb);

			Srb = Ccb->Srb;
			if(
				(Command == 0xA1)
				|| (Command ==SCSIOP_SYNCHRONIZE_CACHE)
				|| (Command ==SCSIOP_FORMAT_UNIT )
				|| (Command == 0x53)
				//|| (Command == 0x5B)
				)
			{
				GenericTimeOut.QuadPart = 600 * NANO100_PER_SEC; //10 minute
				LspSetTimeOut(LSS, &GenericTimeOut);
			}else if(Command == 0x5B)
			{
				GenericTimeOut.QuadPart = 600 * NANO100_PER_SEC;
				LspSetTimeOut(LSS, &GenericTimeOut);
			}else{
				GenericTimeOut.QuadPart = 30 * NANO100_PER_SEC;
				LspSetTimeOut(LSS, &GenericTimeOut);
			}


			ntStatus = LspPacketRequest(
								LSS,
								&PduDesc,
								&response,
								Reg
								);

			IdeDisk->RegError = Reg[0];
			IdeDisk->RegStatus = Reg[1];

			GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
			LspSetTimeOut(LSS, &GenericTimeOut);

			if(!NT_SUCCESS(ntStatus) || response != LANSCSI_RESPONSE_SUCCESS) {
				KDPrintM(DBG_LURN_ERROR, 
					("Request 0x%02x,PrevRequest 0x%02x : RegStatus 0x%02x: RegError 0x%02x ODD_Status 0x%x\n", 
							Command,IdeDisk->PrevRequest, IdeDisk->RegStatus, IdeDisk->RegError, IdeDisk->ODD_STATUS));
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

			if(!NT_SUCCESS(ntStatus))
			{
				if(ntStatus == STATUS_REQUEST_ABORTED){
					KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					IdeDisk->ODD_STATUS = NON_STATE;
					LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					return STATUS_REQUEST_ABORTED;
				}
				KDPrintM(DBG_LURN_ERROR,("Communication error! NTSTATUS:%08lx\n",ntStatus));
				LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
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
					{
						LSCcbSetStatus(Ccb, CCB_STATUS_SUCCESS);
						if(Command == SCSIOP_MODE_SENSE10)
						{
							//
							//
							//		Write Protect for MO 
							//			
							//			this code is unstable only for MO model MOS3400E
							//			hacking MO mode sense of Vender specific code page

							if((!(Lurn->AccessRight & GENERIC_WRITE))
								&& (Lurn->LurnType == LURN_IDE_MO) )
							{
								PBYTE buf;
								if(Ccb->SecondaryBufferLength > 8)
								{
									buf = Ccb->SecondaryBuffer;
									if((buf[0]  == 0x0) && (buf[1] == 0x6f) && (buf[2] == 0x03))
									{
										buf[3] = 0x80;
									}
								}
							}
						}
						return STATUS_SUCCESS;
					}
					break;
				case LANSCSI_RESPONSE_T_NOT_EXIST:
					LSCcbSetStatus(Ccb, CCB_STATUS_NOT_EXIST);
					KDPrintM(DBG_LURN_TRACE, 
						("LANSCSI_RESPONSE_T_NOT_EXIST response Command 0x%02x : response %x\n", Command,response));
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BAD_COMMAND:
					LSCcbSetStatus(Ccb, CCB_STATUS_INVALID_COMMAND);
					KDPrintM(DBG_LURN_TRACE, 
						("LANSCSI_RESPONSE_T_BAD_COMMAND response Command 0x%02x : response %x\n", Command,response));
					return STATUS_SUCCESS;
					break;
				case LANSCSI_RESPONSE_T_BROKEN_DATA:
				case LANSCSI_RESPONSE_T_COMMAND_FAILED:
					{
						if((Ccb->SenseDataLength >= FIELD_OFFSET(SENSE_DATA, FieldReplaceableUnitCode))
							&& (Ccb->SenseBuffer)
							)
						{
							PSENSE_DATA pSenseData;

							NTSTATUS status;


							status = ODD_GetStatus(
											IdeDisk,
											Ccb,
											(PSENSE_DATA)Ccb->SenseBuffer,
											Ccb->SenseDataLength
											);
							if(!NT_SUCCESS(status))
							{
								if(status == STATUS_REQUEST_ABORTED){
									KDPrintM(DBG_LURN_ERROR,("Command (0x%02x) Error Request Aborted!!\n",Command));
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
									IdeDisk->ODD_STATUS = NON_STATE;
									LSCCB_SETSENSE(Ccb, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
									return STATUS_SUCCESS;
								}
								KDPrintM(DBG_LURN_ERROR, ("Command 0x%02x Fail ODD_GETStatus\n", Command));
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMUNICATION_ERROR);
								return status;
							}

							pSenseData	= (PSENSE_DATA)(Ccb->SenseBuffer);

							KDPrintM(DBG_LURN_INFO,("PKCMD (0x%02x), RegStatus(0x%02x), RegError(0x%02x),ErrorCode(0x%02x),SenseKey(0x%02x), ASC(0x%02x), ASCQ(0x%02x)\n",
								Ccb->PKCMD[0], RegStatus, RegError,pSenseData->ErrorCode, pSenseData->SenseKey,
								pSenseData->AdditionalSenseCode, pSenseData->AdditionalSenseCodeQualifier));

							ASSERT(!(pSenseData->SenseKey == 0x05 &&
									pSenseData->AdditionalSenseCode == 0x1a &&
									pSenseData->AdditionalSenseCodeQualifier == 0x00
									));
							ASSERT(!(pSenseData->SenseKey == 0x05 &&
									pSenseData->AdditionalSenseCode == 0x26 &&
									pSenseData->AdditionalSenseCodeQualifier == 0x00
									));

							if(IdeDisk->ODD_STATUS == RESET_POWERUP)
							{
								if(Ccb->PKCMD[0] != 0x0)
								{
									KDPrintM(DBG_LURN_INFO,("POWER RESET 1 CMD %02x\n",Ccb->PKCMD[0]));
									*NeedRepeat = TRUE;
									Ccb->ResidualDataLength = Ccb->DataBufferLength;
									//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
									pSenseData->ErrorCode = 0x0;
									LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
									LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
									return STATUS_SUCCESS;
								}else{
									KDPrintM(DBG_LURN_INFO,("POWER RESET 2 CMD %02x\n",Ccb->PKCMD[0]));
									pSenseData->ErrorCode = 0x0;
									LSCCB_SETSENSE(Ccb, 0x0 , 0x0, 0x0);
									LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);

									return STATUS_SUCCESS;
								}
							}

							if((IdeDisk->ODD_STATUS == NOT_READY_MEDIA) &&
								(Ccb->PKCMD[0] != 0x0))
							{
								KDPrintM(DBG_LURN_INFO,("NOT_READY_MEDIA 4 CMD %02x\n",Ccb->PKCMD[0]));
								//*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
								return STATUS_SUCCESS;
							}

							if(IdeDisk->ODD_STATUS == MEDIA_ERASE_ERROR)
							{
								KDPrintM(DBG_LURN_INFO,("MEDIA_ERASE_ERROR 4 CMD %02x\n",Ccb->PKCMD[0]));
								*NeedRepeat = TRUE;
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								//LSCCB_SETSENSE(Ccb, SCSI_SENSE_NOT_READY , SCSI_ADSENSE_LUN_NOT_READY, 0x08);
								RtlZeroMemory(pSenseData, sizeof(Ccb->SenseDataLength));
								LSCcbSetStatus(Ccb, CCB_STATUS_BUSY);
							}


							if(
								(IdeDisk->ODD_STATUS == NOT_READY_PRESENT)
								&&(Ccb->PKCMD[0] == 0x0)
								)
							{
								if(IdeDisk->DVD_MEDIA_TYPE != 0x12)
								{
									//ODD_BusyWait(4);
								}else{
									LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
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
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
								return STATUS_SUCCESS;
							}
*/

							if(IdeDisk->ODD_STATUS == INVALID_COMMAND)
							{
								Ccb->ResidualDataLength = Ccb->DataBufferLength;
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
								return STATUS_SUCCESS;
							}


							Ccb->ResidualDataLength = Ccb->DataBufferLength;
							if(IdeDisk->DVD_MEDIA_TYPE != 0x12){
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
							}
							else
							{
								LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
							}
							return STATUS_SUCCESS;

						}
						Ccb->ResidualDataLength = Ccb->DataBufferLength;
						LSCcbSetStatus(Ccb, CCB_STATUS_COMMMAND_DONE_SENSE);
						return STATUS_SUCCESS;
					}
				default:
					Ccb->ResidualDataLength = Ccb->DataBufferLength;
					LSCcbSetStatus(Ccb, CCB_STATUS_COMMAND_FAILED);
					KDPrintM(DBG_LURN_TRACE, 
						("NON SPECIFIED ERROR response Command 0x%02x : response %x\n", Command,response));
					return STATUS_SUCCESS;
					break;
				}
			}
		}
		break;
	}


	return ntStatus;

}



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
	((PCDB)&tempCCB.Cdb)->SYNCHRONIZE_CACHE10.Lun = IdeDisk->LuData.LanscsiLU;

	tempCCB.DataBuffer = NULL;
	tempCCB.DataBufferLength = 0;
	tempCCB.SenseBuffer = &tempSenseBuffer;
	tempCCB.SenseDataLength = sizeof(SENSE_DATA);


	KDPrintM(DBG_LURN_INFO,("Reconnection : Send Syncronize cache command for flush controler buffer\n"));
	ntSatus = LurnIdeODDExecute(Lurn, Lurn->LurnExtension,&tempCCB);
	return ntSatus;	

}



static
NTSTATUS
LurnIdeODDDispatchCcb(
		PLURELATION_NODE		Lurn,
		PLURNEXT_IDE_DEVICE		IdeDisk
	) {
	PLIST_ENTRY	ListEntry;
	KIRQL		oldIrql;
	PCCB		Ccb;
	NTSTATUS	status;
	BYTE		PduResponse;

	status = STATUS_SUCCESS;

	while(1) {
		//
		//	dequeue one entry.
		//
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

		if(	!(Lurn->LurnStatus == LURN_STATUS_RUNNING) &&
			!(Lurn->LurnStatus == LURN_STATUS_STALL)
			) {
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			KDPrintM(DBG_LURN_TRACE, ("not LURN_STATUS_RUNNING nor LURN_STATUS_STALL.\n"));

			return STATUS_UNSUCCESSFUL;
		}

		ListEntry = RemoveHeadList(&IdeDisk->ThreadCcbQueue);

		if(ListEntry == (&IdeDisk->ThreadCcbQueue)) {
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			KDPrintM(DBG_LURN_TRACE, ("Empty.\n"));

			break;
		}

		Ccb = CONTAINING_RECORD(ListEntry, CCB, ListEntry);

		if(Lurn->LurnStatus == LURN_STATUS_STALL  &&
			!LSCcbIsFlagOn(Ccb, CCB_FLAG_RETRY_NOT_ALLOWED)) {
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			Ccb->CcbStatus = CCB_STATUS_COMMUNICATION_ERROR;
			status = STATUS_PORT_DISCONNECTED;

			KDPrintM(DBG_LURN_TRACE, ("LURN_STATUS_STALL! going to reconnecting process.\n"));

			goto try_reconnect;
		}

		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		//
		//	dispatch a CCB
		//
		switch(Ccb->OperationCode) {
		case CCB_OPCODE_UPDATE:


			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_UPDATE!\n"));

			status = LurnIdeUpdate(Lurn, IdeDisk, Ccb);
			//
			//	Configure the disk
			//
			if(NT_SUCCESS(status) && Ccb->CcbStatus == CCB_STATUS_SUCCESS) {
				LurnIdeODDConfiure(Lurn, IdeDisk->LuData.LanscsiTargetID, &PduResponse);
			}

			break;

		case CCB_OPCODE_RESETBUS:

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_RESETBUS!\n"));
			status = LurnIdeResetBus(Lurn, Ccb);
			break;

		case CCB_OPCODE_STOP:

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_STOP!\n"));
			LurnIdeStop(Lurn, Ccb);
			return STATUS_UNSUCCESSFUL;
			break;

		case CCB_OPCODE_RESTART:

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_RESTART!\n"));
			status = LurnIdeRestart(Lurn, Ccb);
			break;

		case CCB_OPCODE_ABORT_COMMAND:

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_ABORT_COMMAND!\n"));
			status = LurnIdeAbortCommand(Lurn, Ccb);
			break;

		case CCB_OPCODE_EXECUTE:

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_EXECUTE!\n"));
			status = LurnIdeODDExecute(Lurn, Lurn->LurnExtension, Ccb);
		//
		//	detect Aborted Command 
		//
			if(status == STATUS_REQUEST_ABORTED)
			{
				{
					UCHAR	* buf;
					buf = Ccb->Cdb;
					KDPrintM(DBG_LURN_ERROR,("[ERROR ABORT]\n"
									"%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x\n",
									buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9],buf[10],buf[11]));

					KDPrintM(DBG_LURN_ERROR,("[ERROR ABORT] Data bufferlen %d\n",Ccb->DataBufferLength));


				}
//				status = LurnIdeDVDSyncache(Lurn, IdeDisk);

			}

			break;

		case CCB_OPCODE_NOOP:

			KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_NOOP!\n"));
			status = LSLurnIdeNoop(Lurn, Lurn->LurnExtension, Ccb);
			break;

		default:
			break;
		}


try_reconnect:
		//
		//	detect disconnection
		//
		if(status == STATUS_PORT_DISCONNECTED ||
			Ccb->CcbStatus == CCB_STATUS_COMMUNICATION_ERROR) {
			//
			//	Complete with Busy status 
			//	so that ScsiPort will send the CCB again and prevent SRB timeout.
			//	
			//
			KDPrintM(DBG_LURN_ERROR, ("RECONN: try reconnecting!\n"));

			//
			//	Set the reconnection flag to notify reconnection to Miniport.
			//	Complete the CCB.b
			//
			{
				UCHAR	* buf;
				buf = Ccb->Cdb;
				KDPrintM(DBG_LURN_ERROR,("[ERROR RECONN]\n"
								"%02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x\n",
								buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7],buf[8],buf[9], buf[10], buf[11]));

			}

			LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_RECONNECTING);
			ASSERT(Ccb->Srb);
			Ccb->CcbStatus = CCB_STATUS_BUSY;
			LSCcbCompleteCcb(Ccb);
			KDPrintM(DBG_LURN_ERROR, ("RECONN: completed Ccb:%x.\n", Ccb));

			status = LSLurnIdeReconnect(Lurn, IdeDisk);
			if(status == STATUS_CONNECTION_COUNT_LIMIT) {
				//
				//	terminate the thread
				//
				return STATUS_UNSUCCESSFUL;
			} else if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("RECONN: reconnection failed.\n"));

			} else {
				//
				//	Configure the disk and get the next CCB
				//
				status = LurnIdeODDConfiure(Lurn, IdeDisk->LuData.LanscsiTargetID, &PduResponse);
				if(!NT_SUCCESS(status)) {
					ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
					Lurn->LurnStatus = LURN_STATUS_STALL;
					RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
					KDPrintM(DBG_LURN_ERROR, ("RECONN: LurnIdeDiskConfigure() failed.\n"));

				}

				//status = LurnIdeDVDSyncache(Lurn, IdeDisk);	

			}

			continue;
		}

		//
		//	complete a CCB
		//
		LSCcbCompleteCcb(Ccb);
		KDPrintM(DBG_LURN_TRACE, ("Completed Ccb:%p\n", Ccb));
	}

	return STATUS_SUCCESS;
}





static
VOID
LurnIdeODDThreadProc(
		IN	PVOID	Context
)
{
	PLURELATION_NODE		Lurn;
	PLURNEXT_IDE_DEVICE		IdeDisk;
	NTSTATUS				status;
	KIRQL					oldIrql;
	LARGE_INTEGER			TimeOut;

	KDPrintM(DBG_LURN_TRACE, ("Entered\n"));

	Lurn = (PLURELATION_NODE)Context;
	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;
	TimeOut.QuadPart = - LURNIDE_IDLE_TIMEOUT;

	//
	//	Raise the thread's current priority level (same level to Modified page writer).
	//	Set ready signal.
	//
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);
	KeSetEvent (&IdeDisk->ThreadReadyEvent, IO_NO_INCREMENT, FALSE);


	do {
		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

		status = KeWaitForSingleObject(
							&IdeDisk->ThreadCcbArrivalEvent,
							Executive,
							KernelMode,
							FALSE,
							&TimeOut
							);

		if(!NT_SUCCESS(status)){
			KDPrintM(DBG_LURN_ERROR, ("KeWaitForSingleObject Fail\n"));
			break;
		}

		switch(status)
		{
		case STATUS_SUCCESS:
			KeClearEvent(&IdeDisk->ThreadCcbArrivalEvent);
			status = LurnIdeODDDispatchCcb(Lurn, IdeDisk);
			if(!NT_SUCCESS(status)) {
				KDPrintM(DBG_LURN_ERROR, ("LurnIdeDiskDispatchCcb() failed. NTSTATUS:%08lx\n", status));
				goto terminate_thread;
			}
		break;
		case STATUS_TIMEOUT: {
			BYTE	pdu_response;

			//
			//	Send NOOP to make sure that the Lanscsi Node is reachable.
			//
			status = LspNoOperation(&IdeDisk->LanScsiSession, IdeDisk->LuData.LanscsiTargetID, &pdu_response);
			if(!NT_SUCCESS(status) || pdu_response != LANSCSI_RESPONSE_SUCCESS) {
				ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
				if(Lurn->LurnStatus != LURN_STATUS_STALL) {
					LURN_EVENT	LurnEvent;

					Lurn->LurnStatus = LURN_STATUS_STALL;
					RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

					//
					//	Set a event to make Scsi Port send down NoOperation 
					//		to initiate the reconnection process.
					//
					LurnEvent.LurnId = Lurn->LurnId;
					LurnEvent.LurnEventClass = LURN_REQUEST_NOOP_EVENT;
					LurCallBack(Lurn->Lur, &LurnEvent);

					KDPrintM(DBG_LURN_ERROR, ("Lurn%d:"
											" IDLE_TIMEOUT: NoOperation failed. Start reconnecting.\n",
											Lurn->LurnId));
				} else {
					RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
				}
			} else {
				KDPrintM(DBG_LURN_TRACE, ("Lurn%d:"
											" IDLE_TIMEOUT: NoOperation is successful.\n", Lurn->LurnId));
			}

		break;
		}
		default:
		break;
		}

	} while(1);


terminate_thread:
	//
	//	Set LurnStatus and empty ccb queue
	//
	KDPrintM(DBG_LURN_TRACE, ("The thread will be terminiated.\n"));
	ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);
	Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
	CcbCompleteList(
			&IdeDisk->ThreadCcbQueue,
			CCB_STATUS_NOT_EXIST,
			CCBSTATUS_FLAG_LURN_STOP
		);
	RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

	KDPrintM(DBG_LURN_ERROR, ("Terminated\n"));
	PsTerminateSystemThread(STATUS_SUCCESS);

	return;
}


NTSTATUS
LurnIdeODDInitialize(
		PLURELATION_NODE		Lurn,
		PLURELATION_NODE_DESC	LurnDesc
	) 
{

	PLURNEXT_IDE_DEVICE		IdeDisk;
	NTSTATUS				status;
	OBJECT_ATTRIBUTES		objectAttributes;
	LARGE_INTEGER			timeout;
	BOOLEAN					Connection;
	BOOLEAN					LogIn;
	BOOLEAN					ThreadRef;
	LSSLOGIN_INFO			LoginInfo;
	LSPROTO_TYPE			LSProto;
	BYTE					response;
	LARGE_INTEGER			GenericTimeOut;

	Connection = FALSE;
	LogIn = FALSE;
	ThreadRef = FALSE;

	if((Lurn->LurnType != LURN_IDE_ODD) && (Lurn->LurnType != LURN_IDE_MO)){
		KDPrintM(DBG_LURN_ERROR, ("Not DVD LurnType %x\n", Lurn->LurnType));
		return STATUS_UNSUCCESSFUL;
	}

	IdeDisk = ExAllocatePoolWithTag(NonPagedPool, sizeof(LURNEXT_IDE_DEVICE), LURNEXT_POOL_TAG);
	if(!IdeDisk) {
		KDPrintM(DBG_LURN_ERROR, ("Can't Allocate IdeDisk struecture\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//	Initialize fields
	//
	Lurn->LurnExtension = IdeDisk;
	RtlZeroMemory(IdeDisk, sizeof(LURNEXT_IDE_DEVICE) );

	IdeDisk->Lurn = Lurn;
	IdeDisk->HwType = LurnDesc->LurnIde.HWType;
	IdeDisk->HwVersion = LurnDesc->LurnIde.HWVersion;
	IdeDisk->LuData.LanscsiTargetID = LurnDesc->LurnIde.LanscsiTargetID;
	IdeDisk->LuData.LanscsiLU = LurnDesc->LurnIde.LanscsiLU;

	KeInitializeEvent(
			&IdeDisk->ThreadCcbArrivalEvent,
			NotificationEvent,
			FALSE
		);
	InitializeListHead(&IdeDisk->ThreadCcbQueue);

	KeInitializeEvent(
			&IdeDisk->ThreadReadyEvent,
			NotificationEvent,
			FALSE
		);

	//
	//	Confirm address type.
	//	Connect to the Lanscsi Node.
	//
	status = LstransAddrTypeToTransType( LurnDesc->LurnIde.BindingAddress.Address[0].AddressType, &IdeDisk->LstransType);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("LstransAddrTypeToTransType(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}

	//
	//	Set timeouts.
	//
	GenericTimeOut.QuadPart = LURNIDE_GENERIC_TIMEOUT;
	LspSetTimeOut(&IdeDisk->LanScsiSession, &GenericTimeOut);

	status = LspConnect(
					&IdeDisk->LanScsiSession,
					&LurnDesc->LurnIde.BindingAddress,
					&LurnDesc->LurnIde.TargetAddress
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("LspConnect(), Can't Connect to the LS node. STATUS:0x%08x\n", status));
		goto error_out;
	}
	Connection = TRUE;

	//
	//	Login to the Lanscsi Node.
	//
	Lurn->AccessRight = LurnDesc->AccessRight;
	if(!(LurnDesc->AccessRight & GENERIC_WRITE)) {
		LurnDesc->LurnIde.UserID = CONVERT_TO_ROUSERID(LurnDesc->LurnIde.UserID);
	}

	LoginInfo.LoginType	= LOGIN_TYPE_NORMAL;
	RtlCopyMemory(&LoginInfo.UserID, &LurnDesc->LurnIde.UserID, LSPROTO_USERID_LENGTH);
	RtlCopyMemory(&LoginInfo.Password, &LurnDesc->LurnIde.Password, LSPROTO_PASSWORD_LENGTH);

	LoginInfo.MaxBlocksPerRequest = LurnDesc->MaxBlocksPerRequest;
	LoginInfo.LanscsiTargetID = LurnDesc->LurnIde.LanscsiTargetID;
	LoginInfo.LanscsiLU = LurnDesc->LurnIde.LanscsiLU;
	LoginInfo.HWType = LurnDesc->LurnIde.HWType;
	LoginInfo.HWVersion = LurnDesc->LurnIde.HWVersion;
	LoginInfo.BlockInBytes = BLOCK_SIZE;
	status = LspLookupProtocol(LurnDesc->LurnIde.HWType, LurnDesc->LurnIde.HWVersion, &LSProto);
	if(!NT_SUCCESS(status)) {
		goto error_out;
	}
	if(Lurn->Lur->CntEcrMethod) {
		//
		//	IdeLurn will have a encryption buffer.
		//
		LoginInfo.IsEncryptBuffer = FALSE;
		KDPrintM(DBG_LURN_INFO, ("IDE Lurn extension is supposed to have a encryption buffer.\n"));
	} else {
		//
		//	IdeLurn will NOT have a encryption buffer.
		//	LSS should have a buffer to speed up.
		//
		LoginInfo.IsEncryptBuffer = TRUE;
		KDPrintM(DBG_LURN_INFO, ("LSS is supposed to have a encryption buffer.\n"));
	}

	status = LspLogin(
					&IdeDisk->LanScsiSession,
					&LoginInfo,
					LSProto,
					TRUE
				);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("LspLogin(), Can't login to the LS node with UserID:%08lx. STATUS:0x%08x.\n", LoginInfo.UserID, status));
		status = STATUS_ACCESS_DENIED;
		goto error_out;
	}
	LogIn = TRUE;

	//
	//	Configure the disk.
	//
	status = LurnIdeODDConfiure(Lurn, LurnDesc->LurnIde.LanscsiTargetID, &response);
	if(!NT_SUCCESS(status) || response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, 
			("GetDiskInfo() failed. Response:%d. NTSTATUS:%d\n", response, status));
	}

	//
	//	create worker thread
	//
	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
	status = PsCreateSystemThread(
						&IdeDisk->ThreadHandle,
						THREAD_ALL_ACCESS,
						&objectAttributes,
						NULL,
						NULL,
						LurnIdeODDThreadProc,
						Lurn
					);
	if(!NT_SUCCESS(status)) {
		KDPrintM(DBG_LURN_ERROR, ("Can't Create Process. STATUS:0x%08x\n", status));
		goto error_out;
	}

	ASSERT(IdeDisk->ThreadHandle);
	status = ObReferenceObjectByHandle(
					IdeDisk->ThreadHandle,
					GENERIC_ALL,
					NULL,
					KernelMode,
					&IdeDisk->ThreadObject,
					NULL
				);
	if(status != STATUS_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, ("ObReferenceObjectByHandle() Failed. 0x%x\n", status));
		status = STATUS_UNSUCCESSFUL;
		goto error_out;
	}

	ThreadRef = TRUE;

	timeout.QuadPart = -LURNIDE_THREAD_START_TIME_OUT;

	status = KeWaitForSingleObject(
				&IdeDisk->ThreadReadyEvent,
				Executive,
				KernelMode,
				FALSE,
				&timeout
				);
	if(status != STATUS_SUCCESS) {
		KDPrintM(DBG_LURN_ERROR, ("Wait for event Failed. 0x%x\n", status));
		status = STATUS_UNSUCCESSFUL;
		goto error_out;
	}


	if(Lurn->LurnType == LURN_IDE_ODD)
	{

#ifdef	__DVD_ENCRYPT_CONTENT__
		PUCHAR	pPassword;
		PUCHAR	pKey;
		PLANSCSI_SESSION LSS;
		LSS = &IdeDisk->LanScsiSession;

		pPassword = (PUCHAR)&(LSS->Password);
		pKey = IdeDisk->CntEcrKey;

		IdeDisk->CntEcrLength = 4;

		IdeDisk->CntEcr_IR[0] = pPassword[1] ^ pPassword[7] ^ pKey[3];
		IdeDisk->CntEcr_IR[1] = pPassword[0] ^ pPassword[3] ^ pKey[0];
		IdeDisk->CntEcr_IR[2] = pPassword[2] ^ pPassword[6] ^ pKey[2];
		IdeDisk->CntEcr_IR[3] = pPassword[4] ^ pPassword[5] ^ pKey[1];

		IdeDisk->CntDcr_IR[0] = ~(pPassword[0] ^ pPassword[3] ^ pKey[0]);
		IdeDisk->CntDcr_IR[1] = pPassword[2] ^ pPassword[6] ^ pKey[2];
		IdeDisk->CntDcr_IR[2] = pPassword[4] ^ pPassword[5] ^ ~(pKey[1]);
		IdeDisk->CntDcr_IR[3] = pPassword[1] ^ pPassword[7] ^ pKey[3];
#endif //__DVD_ENCRYPT_CONTENT__		
	}



	return STATUS_SUCCESS;

error_out:
	if(ThreadRef)
		ObDereferenceObject(&IdeDisk->ThreadObject);
	if(LogIn)
		LspLogout(&IdeDisk->LanScsiSession);
	if(Connection) {
		LspDisconnect(&IdeDisk->LanScsiSession);
	}
	if(IdeDisk) {
		ExFreePoolWithTag(IdeDisk, LURNEXT_POOL_TAG);
	}

	return status;
}


NTSTATUS
LurnIdeODDRequest(
		PLURELATION_NODE	Lurn,
		PCCB				Ccb
	)
{
	PLURNEXT_IDE_DEVICE		IdeDisk;
	KIRQL					oldIrql;
	NTSTATUS				status;

	IdeDisk = (PLURNEXT_IDE_DEVICE)Lurn->LurnExtension;
	KDPrintM(DBG_LURN_TRACE, ("Ccb->OperationCode 0x%x\n",Ccb->OperationCode));
	//
	//	dispatch a request
	//
	switch(Ccb->OperationCode) {
	case CCB_OPCODE_RESETBUS:
		//
		//	clear all CCB in the queue and queue the CCB to the thread. 
		//
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_RESETBUS\n"));
		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

//		Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
		CcbCompleteList(&IdeDisk->ThreadCcbQueue, CCB_STATUS_RESET, 0);
		if(	LURN_IS_RUNNING(Lurn->LurnStatus) ) {
			InsertHeadList(&IdeDisk->ThreadCcbQueue, &Ccb->ListEntry);
			KeSetEvent(&IdeDisk->ThreadCcbArrivalEvent, IO_NO_INCREMENT, FALSE);
			LSCcbMarkCcbAsPending(Ccb);

			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			return STATUS_PENDING;
		}

		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
		Ccb->CcbStatus = CCB_STATUS_SUCCESS;
		LSCcbCompleteCcb(Ccb);
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_RESETBUS: Completed Ccb:%p\n", Ccb));
		return STATUS_SUCCESS;

	case CCB_OPCODE_STOP:
		//
		//	clear all CCB in the queue and set LURN_STATUS_STOP_PENDING
		//
		KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_STOP\n"));

		ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);
		ASSERT(Ccb->Flags & CCB_FLAG_SYNCHRONOUS);

		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

		CcbCompleteList(&IdeDisk->ThreadCcbQueue, CCB_STATUS_NOT_EXIST, CCBSTATUS_FLAG_LURN_STOP);

		if(	LURN_IS_RUNNING(Lurn->LurnStatus)) {

			LARGE_INTEGER			TimeOut;

			KDPrintM(DBG_LURN_INFO, ("CCB_OPCODE_STOP: queue CCB_OPCODE_STOP to the thread. LurnStatus=%08lx\n",
															Lurn->LurnStatus
														));

			InsertHeadList(&IdeDisk->ThreadCcbQueue, &Ccb->ListEntry);
			KeSetEvent(&IdeDisk->ThreadCcbArrivalEvent, IO_NO_INCREMENT, FALSE);
			Lurn->LurnStatus = LURN_STATUS_STOP_PENDING;
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);
			TimeOut.QuadPart = - NANO100_PER_SEC * 120;
			status = KeWaitForSingleObject(
						IdeDisk->ThreadObject,
						Executive,
						KernelMode,
						FALSE,
						&TimeOut
					);
			ASSERT(status == STATUS_SUCCESS);
		} else {
			RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

			Ccb->CcbStatus =CCB_STATUS_COMMAND_FAILED;
			LSCcbCompleteCcb(Ccb);
		}

		ObDereferenceObject(IdeDisk->ThreadObject);

		ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

		IdeDisk->ThreadObject = NULL;
		IdeDisk->ThreadHandle = NULL;

		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		LspLogout(&IdeDisk->LanScsiSession);
		LspDisconnect(&IdeDisk->LanScsiSession);

		return STATUS_SUCCESS;

	case CCB_OPCODE_QUERY: {

			PLUR_QUERY			LurQuery;

			//
			//	Do not support LurPrimaryLurnInformation for Lfsfilt.
			//
			LurQuery = (PLUR_QUERY)Ccb->DataBuffer;
			if(LurQuery->InfoClass == LurPrimaryLurnInformation) {
				Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
				LSCcbCompleteCcb(Ccb);
				return STATUS_SUCCESS;
			}

			status = LurnIdeQuery(Lurn, IdeDisk, Ccb);
			if(	status != STATUS_MORE_PROCESSING_REQUIRED) {
#if DBG
				if(NT_SUCCESS(status)) {
					KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_QUERY: Completed Ccb:%p\n", Ccb));
				}
#endif
				LSCcbCompleteCcb(Ccb);
				return STATUS_SUCCESS;
			}
		}

		break;

	case CCB_OPCODE_RESTART:
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_RESTART\n"));
		Ccb->CcbStatus =CCB_STATUS_COMMAND_FAILED;
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;

	case CCB_OPCODE_ABORT_COMMAND:
		KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_ABORT_COMMAND\n"));
		Ccb->CcbStatus =CCB_STATUS_COMMAND_FAILED;
		LSCcbCompleteCcb(Ccb);
		return STATUS_SUCCESS;
	case CCB_OPCODE_DVD_STATUS:
		{
			PLURN_DVD_STATUS	PDvdStatus;
			LARGE_INTEGER		CurrentTime;
			ULONG				Delta = 15;

			KDPrintM(DBG_LURN_TRACE, ("CCB_OPCODE_DVD_STATUS\n"));
			KeQuerySystemTime(&CurrentTime);

			PDvdStatus = (PLURN_DVD_STATUS)(Ccb->DataBuffer);

			if( (IdeDisk->DVD_STATUS == DVD_NO_W_OP)
				|| ( ( IdeDisk->DVD_STATUS == DVD_W_OP)
						&& ( (ULONG)((CurrentTime.QuadPart - IdeDisk->DVD_Acess_Time.QuadPart) / 10000000 ) > Delta ) )
						)
			{
				PDvdStatus->Last_Access_Time.QuadPart = IdeDisk->DVD_Acess_Time.QuadPart;
				PDvdStatus->Status = DVD_NO_W_OP;
			}else {
				PDvdStatus->Status = DVD_W_OP;
			}
			Ccb->CcbStatus = CCB_STATUS_SUCCESS;
			LSCcbCompleteCcb(Ccb);
			return STATUS_SUCCESS;

		}
		break;
	default:
		break;
	}


	//
	//	check the status and queue a Ccb to the worker thread
	//
	ACQUIRE_SPIN_LOCK(&Lurn->LurnSpinLock, &oldIrql);

	if(	!LURN_IS_RUNNING(Lurn->LurnStatus) ) {

		if(Lurn->LurnStatus == LURN_STATUS_STOP_PENDING) {
			KDPrintM(DBG_LURN_ERROR, ("LURN(%08lx) is in STOP_PENDING. Return with CCB_STATUS_STOP.\n", Lurn->LurnStatus));
			LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_BUSRESET_REQUIRED|CCBSTATUS_FLAG_LURN_STOP);
			Ccb->CcbStatus = CCB_STATUS_STOP;
			Lurn->LurnStatus = LURN_STATUS_STOP;
		}
		else {
			KDPrintM(DBG_LURN_ERROR, ("LURN(%08lx) is not running. Return with NOT_EXIST.\n", Lurn->LurnStatus));
			LSCcbSetStatusFlag(Ccb, CCBSTATUS_FLAG_LURN_STOP);
			Ccb->CcbStatus = CCB_STATUS_NOT_EXIST;
		}
		RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

		LSCcbCompleteCcb(Ccb);

		return STATUS_SUCCESS;

	}


	if(Ccb->Flags & CCB_FLAG_URGENT) {
		KDPrintM(DBG_LURN_INFO, ("URGENT. Queue to the head and mark pending.\n"));

		InsertHeadList(&IdeDisk->ThreadCcbQueue, &Ccb->ListEntry);
		KeSetEvent(&IdeDisk->ThreadCcbArrivalEvent, IO_DISK_INCREMENT, FALSE);
		LSCcbMarkCcbAsPending(Ccb);
	} else {
		KDPrintM(DBG_LURN_TRACE, ("Queue to the tail and mark pending.\n"));

		InsertTailList(&IdeDisk->ThreadCcbQueue, &Ccb->ListEntry);
		KeSetEvent(&IdeDisk->ThreadCcbArrivalEvent, IO_DISK_INCREMENT, FALSE);
		LSCcbMarkCcbAsPending(Ccb);
	}

	RELEASE_SPIN_LOCK(&Lurn->LurnSpinLock, oldIrql);

	return STATUS_PENDING;
}
