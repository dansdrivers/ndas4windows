#include "stdafx.h"

extern int ActiveHwVersion; // set at login time
extern UINT16	HeaderEncryptAlgo;
extern UINT16	DataEncryptAlgo;
extern int		iTargetID;
extern unsigned _int64	iPassword_v1;
extern unsigned _int64	iSuperPassword_v1;
extern _int32			HPID;
extern _int16			RPID;
extern _int32			iTag;

typedef struct _SMART_REPLY {
	UCHAR Error;
	UCHAR SectorCount;
	UCHAR LbaLow;
	UCHAR LbaMid;
	UCHAR LbaHigh;
	UCHAR Device;
	UCHAR Status;
} SMART_REPLY, *PSMART_REPLY;

// Return value
//		0: OK
//		-1: General error
//		-2: CRC error
//
int
SmartCommand(
		   SOCKET	connsock,
		   UINT32	TargetId,
		   UINT32 	SectorCount,
		   UINT32	Feature,
			PSMART_REPLY SmartReply
		   )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];

	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	LANSCSI_PDU_POINTERS						pdu;
	int								iResult;
	unsigned _int8					iCommandReg;

//	unsigned						DataLength;
//	unsigned						crc;
	BOOL	CrcErrored = FALSE;

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->TargetID = htonl(TargetId);
	pRequestHeader->LUN = 0;
	
	// Using Target ID. LUN is always 0.
	pRequestHeader->DEV = TargetId;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0;


	pRequestHeader->R = 0;
	pRequestHeader->W = 0;
	
	pRequestHeader->LBAMid_Curr = SMART_LCYL_PASS;
	pRequestHeader->LBAHigh_Curr = SMART_HCYL_PASS;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= (UCHAR)Feature;
	pRequestHeader->SectorCount_Curr = (unsigned _int8)SectorCount;
	pRequestHeader->Command = WIN_SMART;

	// Backup Command.
	iCommandReg = pRequestHeader->Command;

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("IdeCommand: Send Request "));
		return -1;
	}

	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]IDECommand: Can't Read Reply.\n");
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		fprintf(stderr, "[NdasCli]IDECommand: Time out...\n");
		return WAIT_TIMEOUT;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;	
	if(pReplyHeader->Opcode != IDE_RESPONSE){
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header pReplyHeader->Opcode != IDE_RESPONSE . Opcode=0x%x, Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Opcode, pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	if(pReplyHeader->F == 0){		
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header pReplyHeader->F == 0 . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}

	if (pReplyHeader->Response == LANSCSI_RESPONSE_T_BROKEN_DATA) {
		fprintf(stderr, "Write-data CRC error.\n");
		CrcErrored = TRUE;
	} else if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			);
		fprintf(stderr, "Error register = 0x%x\n", pReplyHeader->Feature_Curr);
		fprintf(stderr, "Error register = 0x%x\n", pReplyHeader->Feature_Prev);
		
		return -1;
	}

	if (CrcErrored)
		return -2;

	SmartReply->Error = pReplyHeader->Feature_Curr;
	SmartReply->SectorCount = pReplyHeader->SectorCount_Curr;
	SmartReply->LbaLow = pReplyHeader->LBALow_Curr;
	SmartReply->LbaMid = pReplyHeader->LBAMid_Curr;
	SmartReply->LbaHigh = pReplyHeader->LBAHigh_Curr;
	SmartReply->Device = pReplyHeader->Device;
	SmartReply->Status = pReplyHeader->Command;

	return 0;
}


//
// No parameter.(right now...)
//
int CmdSmart(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int UserId;
//	UINT32 Param0, Param1, Param2;
//	int Option;
	int retval = 0;
	SMART_REPLY SmartReply;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		fprintf(stderr, "Unsupported NDAS HW\n");
		goto logout;
	}
	
	fprintf(stderr, "= Idenfity Information =\n");	
	if((iResult = GetDiskInfo(connsock, iTargetID, FALSE, FALSE)) != 0) {
		fprintf(stderr, "Identify Failed.\n");
		retval = -1;
		goto logout;
	}
	if (!PerTarget[iTargetID].bSmartSupported) {
		fprintf(stderr, "SMART is not supported.\n");
		retval = -1;
		goto logout;
	}

	if (!PerTarget[iTargetID].bSmartEnabled) {
		fprintf(stderr, "Enabling SMART\n");
		retval = SmartCommand(connsock, iTargetID, 1, SMART_ENABLE, &SmartReply);
		if (retval !=0) {
			fprintf(stderr, "Failed.\n");
			goto errout;
		}
	} else {
#if 0 // for enable/disable test...
		fprintf(stderr, "Disabling SMART\n");
		retval = SmartCommand(connsock, iTargetID, 1, SMART_DISABLE, &SmartReply);
		if (retval !=0) {
			fprintf(stderr, "Failed.\n");
			goto errout;
		}
#endif
	}
	
	fprintf(stderr, "= SMART Information =\n");	
	retval = SmartCommand(connsock, iTargetID, 0, SMART_STATUS, &SmartReply);
	if (retval !=0) {
		fprintf(stderr, "Failed.\n");
		goto errout;
	}
	if (SmartReply.LbaHigh == 0x0C2 && SmartReply.LbaMid == 0x04f) {
		fprintf(stderr, "No threshold exceeded. Disk is healthy.\n");
	} else if (SmartReply.LbaHigh == 0x02C && SmartReply.LbaMid == 0x0f4) {
		fprintf(stderr, "Threshold has exceeded. Disk is unhealthy.\n");
	} else {
		fprintf(stderr, "Error!! Unexpected status.\n");
		goto logout;
	}




logout:
	DisconnectFromNdas(connsock, UserId);
errout:
	return retval;
}