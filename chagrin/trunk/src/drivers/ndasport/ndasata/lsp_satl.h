#pragma once
#include "port.h"
#include <lspx/lsp.h>

/* SCSI/ATA Translation Layer */
/* ANSI Working Draft Project T10/1711-D */
/* Revision 08 17 January 2006 */

VOID
FORCEINLINE
LspSatlSetSimpleSenseKey(
	PSCSI_REQUEST_BLOCK Srb, 
	UCHAR SenseKey,
	UCHAR AdSenseCode)
{
	PSENSE_DATA SenseData = (PSENSE_DATA) Srb->SenseInfoBuffer;

	if (SenseData == NULL || Srb->SenseInfoBufferLength < sizeof(SENSE_DATA))
	{
		return;
	}
	SenseData->ErrorCode = 0x70; /* 70h Current or 71h Deferred */
	SenseData->Valid = 1;
	SenseData->SenseKey = SenseKey;
	SenseData->AdditionalSenseLength = 0x0B;
	SenseData->AdditionalSenseCode = AdSenseCode;
	Srb->SrbStatus |= SRB_STATUS_AUTOSENSE_VALID;
}

VOID
FORCEINLINE
LspSatlSetScsiError(
	PSCSI_REQUEST_BLOCK Srb,
	UCHAR SenseKey,
	UCHAR AdSenseCode)
{
	Srb->SrbStatus = SRB_STATUS_ERROR;
	Srb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
	LspSatlSetSimpleSenseKey(Srb, SenseKey, AdSenseCode);
}

BOOLEAN
FORCEINLINE
LspSatlIsValidCdbControlByte(PSCSI_REQUEST_BLOCK Srb)
{
	typedef struct _CDB_CONTROL_BYTE {
		UCHAR Link : 1;
		UCHAR Obsolete : 1;
		UCHAR NACA : 1;
		UCHAR Reserved : 3;
		UCHAR VendorSpecific : 2;
	} CDB_CONTROL_BYTE, *PCDB_CONTROL_BYTE;

	PCDB Cdb = (PCDB) Srb->Cdb;
	PCDB_CONTROL_BYTE Control = (PCDB_CONTROL_BYTE) Cdb->CDB10.Control;

	/* NACA and LINK shall be zero */
	if (Cdb->CDB10.Control)
	{
		LspSatlSetScsiError(Srb, 
			SCSI_SENSE_ILLEGAL_REQUEST, 
			SCSI_ADSENSE_INVALID_CDB);
		return FALSE;
	}
	return TRUE;
}

BOOLEAN
FORCEINLINE
LspSatl_Inquiry(PSCSI_REQUEST_BLOCK Srb, lsp_ide_register_param_t* IdeReg)
{
	ASSERT(SCSIOP_INQUIRY == Srb->Cdb[0]);
	IdeReg->command.command = 0xEC;
	if (!LspSatlIsValidCdbControlByte(Srb)) return FALSE;
	return TRUE;
}

BOOLEAN
FORCEINLINE
LspSatl_TestUnitReady(PSCSI_REQUEST_BLOCK Srb, lsp_ide_register_param_t* IdeReg)
{
	LspSatlSetScsiError(Srb, 
		SCSI_SENSE_NOT_READY, 
		SCSI_ADSENSE_LUN_NOT_READY);

	LspSatlSetScsiError(Srb, 
		SCSI_SENSE_NOT_READY,
		SCSI_ADSENSE_NO_MEDIA_IN_DEVICE);

	LspSatlSetScsiError(Srb, 
		SCSI_SENSE_HARDWARE_ERROR,
		SCSI_ADSENSE_LUN_NOT_READY);

	LspSatlSetScsiError(Srb, 
		SCSI_SENSE_NOT_READY,
		SCSI_ADSENSE_NO_MEDIA_IN_DEVICE);

	LspSatlSetScsiError(Srb, 
		SCSI_SENSE_NOT_READY,
		SCSI_ADSENSE_LUN_NOT_READY);
		// SCSI_SENSEQ_CAUSE_NOT_REPORTABLE);

	return TRUE;
}

BOOLEAN
FORCEINLINE
LspSatl_Read(PSCSI_REQUEST_BLOCK Srb, lsp_ide_register_param_t* IdeReg)
{
	if (!LspSatlIsValidCdbControlByte(Srb)) return FALSE;
	return FALSE;
}


