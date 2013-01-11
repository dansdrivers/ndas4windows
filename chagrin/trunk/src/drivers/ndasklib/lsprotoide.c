#include "ndasscsiproc.h"


#define IDENTIFY_DATA_LENGTH			(512)


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "LSProtoIde"

//////////////////////////////////////////////////////////////////////////
//
//	Interface functions
//

//
//	version 1.1
//
NTSTATUS
IdeLspSendRequest_V11(
		IN PLANSCSI_SESSION			LSS,
		IN PLANSCSI_PDU_POINTERS	pPdu,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
	);

NTSTATUS
IdeLspReadReply_V11(
		IN PLANSCSI_SESSION			LSS,
		OUT PCHAR					pBuffer,
		IN PLANSCSI_PDU_POINTERS	pPdu,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
	);

NTSTATUS
IdeLspLogin_V11(
		IN PLANSCSI_SESSION	LSS,
		IN PLSSLOGIN_INFO		LoginInfo,
		IN PLARGE_INTEGER		TimeOut
	);

NTSTATUS
IdeLspLogout_V11(
		IN PLANSCSI_SESSION	LSS,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
IdeLspTextTargetList_V11(
		IN PLANSCSI_SESSION	LSS, 
		IN PTARGETINFO_LIST	TargetList,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
IdeLspTextTargetData_V11(
		IN PLANSCSI_SESSION	LSS,
		IN BYTE				GetorSet,
		IN UINT32			TargetID,
		IN OUT PTARGET_DATA	TargetData,
		IN PLARGE_INTEGER	TimeOut
	);

NTSTATUS
IdeLspRequest_V11(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduReponse
	);

NTSTATUS
IdeLspPacketRequest(
		IN PLANSCSI_SESSION		LSS,
		IN OUT PLANSCSI_PDUDESC	PduDesc,
		OUT PBYTE				PduReponse,
		OUT PUCHAR				PduRegister
		);

NTSTATUS
IdeLspVendorRequest(
	IN PLANSCSI_SESSION		LSS,
	IN OUT PLANSCSI_PDUDESC	PduDesc,
	OUT PBYTE				PduResponse
);

LANSCSIPROTOCOL_INTERFACE	LsProtoIdeV11Interface = {
		sizeof(LANSCSIPROTOCOL_INTERFACE),
		LSSTRUC_TYPE_PROTOCOL_INTERFACE,
		LSPROTO_IDE_V11,					// version 1.1
		0,
		0,
		0,
		0,
		{
			IdeLspSendRequest_V11,
			IdeLspReadReply_V11,
			IdeLspLogin_V11,
			IdeLspLogout_V11,
			IdeLspTextTargetList_V11,
			IdeLspTextTargetData_V11,
			IdeLspRequest_V11,
			IdeLspPacketRequest,
			IdeLspVendorRequest,
		}
	};


//
//	version 1.0
//
//	shares SendRequest, ReadReply, Login, Logout, TextTargetList, TextTargetData with version 1.1
//
NTSTATUS
IdeLspRequest_V10(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduReponse
	);

LANSCSIPROTOCOL_INTERFACE	LsProtoIdeV10Interface = {
		sizeof(LANSCSIPROTOCOL_INTERFACE),
		LSSTRUC_TYPE_PROTOCOL_INTERFACE,
		LSPROTO_IDE_V10,				// version 1.0
		0,
		0,
		0,
		0,
		{
			IdeLspSendRequest_V11,
			IdeLspReadReply_V11,
			IdeLspLogin_V11,
			IdeLspLogout_V11,
			IdeLspTextTargetList_V11,
			IdeLspTextTargetData_V11,
			IdeLspRequest_V10,
			NULL,
			NULL
		}
	};


//////////////////////////////////////////////////////////////////////////
//
//	Util functions
//

//////////////////////////////////////////////////////////////////////////
//
// Transport operations common to all versions
//

//
//	Receive data
// NOTE: This function does not support asynchronous IO
//

NTSTATUS
RecvIt(
		IN PLSTRANS_CONNECTION_FILE ConnectionFile,
		OUT PCHAR					RecvBuff, 
		IN ULONG					RecvLen,
		OUT PULONG					TotalReceived,
		IN PLSTRANS_OVERLAPPED		OverlappedData,
		IN PLARGE_INTEGER			TimeOut
){
	int			len = RecvLen;
	NTSTATUS	ntStatus;
	LONG		result;

	//
	//	Parameter check
	//

	if(OverlappedData) {
		if(TotalReceived)
			return STATUS_INVALID_PARAMETER;
	}

	if(TotalReceived)
		*TotalReceived = 0;

	if(OverlappedData) {

		//
		//	Asynchronous
		//

		ntStatus = LstransReceive(
			ConnectionFile,
			RecvBuff,
			RecvLen,
			0,
			TimeOut,
			NULL,
			OverlappedData
			);
#if DBG
		if(ntStatus != STATUS_SUCCESS && ntStatus != STATUS_PENDING) {
			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(ConnectionFile, LANSCSI_SESSION, ConnectionFile);
			KDPrintM(DBG_PROTO_ERROR, ("Can't recv! %02x:%02x:%02x:%02x:%02x:%02x\n",
				LSS->LSNodeAddress.Address[0].Address.Address[2],
				LSS->LSNodeAddress.Address[0].Address.Address[3],
				LSS->LSNodeAddress.Address[0].Address.Address[4],
				LSS->LSNodeAddress.Address[0].Address.Address[5],
				LSS->LSNodeAddress.Address[0].Address.Address[6],
				LSS->LSNodeAddress.Address[0].Address.Address[7]));
		}
#endif
	} else {

		while (len > 0) {
			result = 0;
    		ntStatus = LstransReceive(
							ConnectionFile,
							RecvBuff,
							len,
							0,
							TimeOut,
							&result,
							NULL
						);
			if(ntStatus != STATUS_SUCCESS) {
				PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(ConnectionFile, LANSCSI_SESSION, ConnectionFile);
				KDPrintM(DBG_PROTO_ERROR, ("Can't Receive! %02x:%02x:%02x:%02x:%02x:%02x\n",
					LSS->LSNodeAddress.Address[0].Address.Address[2],
					LSS->LSNodeAddress.Address[0].Address.Address[3],
					LSS->LSNodeAddress.Address[0].Address.Address[4],
					LSS->LSNodeAddress.Address[0].Address.Address[5],
					LSS->LSNodeAddress.Address[0].Address.Address[6],
					LSS->LSNodeAddress.Address[0].Address.Address[7]));
				return ntStatus;
			}

			KDPrintM(DBG_PROTO_NOISE, ("len %d, result %d \n", len, result));

			//
			//	LstransReceive() must guarantee more than one byte is received
			//	when return SUCCESS
			//

			if(result <= 0) {
				return STATUS_UNSUCCESSFUL;
			}

			len -= result;
			RecvBuff += result;
			if(TotalReceived)
				*TotalReceived += result;
		}
	}

	return STATUS_SUCCESS;
}


//
//	Receive data
// NOTE: This function supports asynchronous IO
//

NTSTATUS
SendIt(
	   IN PLSTRANS_CONNECTION_FILE ConnectionFile,
	   IN PCHAR					SendBuff, 
	   IN int					SendLen,
	   OUT PULONG				TotalSent,
	   IN PLSTRANS_OVERLAPPED	OverlappedData,
	   IN PLARGE_INTEGER		TimeOut
){
	NTSTATUS	ntStatus;
	UINT32		result;

	//
	//	Parameter check
	//

	if(OverlappedData) {
		if(TotalSent)
			return STATUS_INVALID_PARAMETER;
	}


	//
	//	Init variables
	//

	if(TotalSent)
		*TotalSent = 0;
	result = 0;


	if(OverlappedData) {

		//
		//	Asynchronous
		//

		ntStatus = LstransSend(
					ConnectionFile,
					SendBuff,
					SendLen,
					0,
					TimeOut,
					NULL,
					OverlappedData
				);
#if DBG
		if(ntStatus != STATUS_SUCCESS && ntStatus != STATUS_PENDING) {
			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(ConnectionFile, LANSCSI_SESSION, ConnectionFile);
			KDPrintM(DBG_PROTO_ERROR, ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
				LSS->LSNodeAddress.Address[0].Address.Address[2],
				LSS->LSNodeAddress.Address[0].Address.Address[3],
				LSS->LSNodeAddress.Address[0].Address.Address[4],
				LSS->LSNodeAddress.Address[0].Address.Address[5],
				LSS->LSNodeAddress.Address[0].Address.Address[6],
				LSS->LSNodeAddress.Address[0].Address.Address[7]));
		}
#endif
	} else {


		//
		//	Synchronous
		//

		ntStatus = LstransSend(
					ConnectionFile,
					SendBuff,
					SendLen,
					0,
					TimeOut,
					&result,
					NULL
					);
		if(ntStatus == STATUS_SUCCESS) {

			if(result == SendLen) {
				if(TotalSent)
					*TotalSent += SendLen;
			} else {
				ntStatus = STATUS_UNSUCCESSFUL;
			}
		}
#if DBG
		else {
			PLANSCSI_SESSION LSS = (PLANSCSI_SESSION)CONTAINING_RECORD(ConnectionFile, LANSCSI_SESSION, ConnectionFile);
			KDPrintM(DBG_PROTO_ERROR, ("Can't send! %02x:%02x:%02x:%02x:%02x:%02x\n",
				LSS->LSNodeAddress.Address[0].Address.Address[2],
				LSS->LSNodeAddress.Address[0].Address.Address[3],
				LSS->LSNodeAddress.Address[0].Address.Address[4],
				LSS->LSNodeAddress.Address[0].Address.Address[5],
				LSS->LSNodeAddress.Address[0].Address.Address[6],
				LSS->LSNodeAddress.Address[0].Address.Address[7]));
		}
#endif
	}


	return ntStatus;
}



NTSTATUS
IdeLspReadReply_V11(
		PLANSCSI_SESSION			LSS,
		PCHAR						pBuffer,
		PLANSCSI_PDU_POINTERS		pPdu,
		PLSTRANS_OVERLAPPED			OverlappedData,
		PLARGE_INTEGER				TimeOut
){
	ULONG		ulTotalRecieved = 0;
	PCHAR		pPtr = pBuffer;
	NTSTATUS	status;
	ULONG		recv;
	UINT16		ahsLen;
	UINT32		dsLen;

	if(OverlappedData) {
		return STATUS_NOT_SUPPORTED;
	}


	// Read Header.
	status = RecvIt(
		&LSS->ConnectionFile,
		pPtr,
		sizeof(LANSCSI_R2H_PDU_HEADER),
		&recv,
		NULL,
		TimeOut
		);

	if(recv != sizeof(LANSCSI_R2H_PDU_HEADER)) {
		KDPrintM(DBG_PROTO_ERROR, ("Can't Recv Header...\n"));

		return status;
	} else 
		ulTotalRecieved += recv;

	pPdu->pR2HHeader = (PLANSCSI_R2H_PDU_HEADER)pPtr;

	pPtr += sizeof(LANSCSI_R2H_PDU_HEADER);
	
	//
	// Decrypt.
	//
	if(LSS->SessionPhase == FLAG_FULL_FEATURE_PHASE
		&& LSS->HeaderEncryptAlgo != 0) {
		Decrypt32SP(
			(unsigned char*)pPdu->pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&(LSS->DecryptIR[0])
			);
	}
	//
	//	Read Additional header segment length and Data segment length.
	//
	ahsLen = NTOHS(pPdu->pR2HHeader->AHSLen);
	dsLen = NTOHL(pPdu->pR2HHeader->DataSegLen);

	//
	// Read AHS.
	//
	if((ULONG)ahsLen >= MAX_REQUEST_SIZE - sizeof(LANSCSI_R2H_PDU_HEADER)) {
		return STATUS_UNSUCCESSFUL;
	}
	if(ahsLen) {

		status = RecvIt(
				&LSS->ConnectionFile,
				pPtr,
				NTOHS(pPdu->pR2HHeader->AHSLen),
				&recv,
				NULL,
				TimeOut
			);
		
		if(recv != ahsLen) {
			KDPrintM(DBG_PROTO_ERROR, ("Can't Recv AHS...\n"));

			return status;
		} else 
			ulTotalRecieved += recv;
	
		pPdu->pAHS = pPtr;

		pPtr += ahsLen;
		
		if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
			if(LSS->SessionPhase == FLAG_FULL_FEATURE_PHASE
				&& LSS->HeaderEncryptAlgo != 0) {
				Decrypt32SP(
					(unsigned char*)pPdu->pAHS,
					NTOHS(pPdu->pR2HHeader->AHSLen),
					(unsigned char *)&(LSS->DecryptIR[0])
					);
			}
		}

	}

	// Read Header Dig.
	pPdu->pHeaderDig = NULL;

	// Read Data segment.
	//
	//	Restrict Data segment size below 64K bytes
	//
	if((ULONG)dsLen >= MAX_REQUEST_SIZE - sizeof(LANSCSI_R2H_PDU_HEADER) - ahsLen) {
		return STATUS_UNSUCCESSFUL;
	}
	if(dsLen) {
		status = RecvIt(
				&LSS->ConnectionFile,
				pPtr,
				dsLen,
				&recv,
				NULL,
				TimeOut
			);
		if(recv != dsLen) {
			KDPrintM(DBG_PROTO_ERROR, ("Can't Recv Data segment...\n"));

			return status;
		} else 
			ulTotalRecieved += recv;
	
		pPdu->pDataSeg = pPtr;

		pPtr += NTOHL(pPdu->pR2HHeader->DataSegLen);

		//
		// Decrypt.
		//
		if(LSS->SessionPhase == FLAG_FULL_FEATURE_PHASE
			&& LSS->DataEncryptAlgo != 0) {
			
			Decrypt32SP(
				(unsigned char*)pPdu->pDataSeg,
				NTOHL(pPdu->pH2RHeader->DataSegLen),
				(unsigned char *)&(LSS->DecryptIR[0])
				);
		}
	}

	// Read Data Dig.
	pPdu->pDataDig = NULL;

	return STATUS_SUCCESS;
}


NTSTATUS
IdeLspSendRequest_V11(
			PLANSCSI_SESSION		LSS,
			PLANSCSI_PDU_POINTERS	pPdu,
			PLSTRANS_OVERLAPPED		OverlappedData,
			PLARGE_INTEGER			TimeOut
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iAHSLen, iDataSegLen;
	NTSTATUS				ntStatus;
	ULONG					result;

	pHeader = pPdu->pH2RHeader;
	iAHSLen = NTOHS(pHeader->AHSLen);
	iDataSegLen = NTOHL(pHeader->DataSegLen);
	result = 0;

	//
	// Check Parameter.
	//
	if(iAHSLen < 0 || iDataSegLen < 0) {
		KDPrintM(DBG_PROTO_ERROR, ("Bad Parameter.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Encrypt Header seg.
	//
	if(LSS->SessionPhase == FLAG_FULL_FEATURE_PHASE
		&& LSS->HeaderEncryptAlgo != 0) {
		
	
		Encrypt32SP(
			(unsigned char*)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			&(LSS->EncryptIR[0])
			);

		if(iAHSLen > 0) {
		
		Encrypt32SP(
			(unsigned char*)pPdu->pAHS,
			iAHSLen,
			&(LSS->EncryptIR[0])
			);
		}
	}

	//
	// Encrypt Data seg.
	//
	if(LSS->SessionPhase == FLAG_FULL_FEATURE_PHASE
		&& LSS->DataEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt32SP(
			(unsigned char*)pPdu->pDataSeg,
			iDataSegLen,
			&(LSS->EncryptIR[0])
			);
		
	}

	// Send Request.
	if(OverlappedData) {

		ntStatus = SendIt(
				&LSS->ConnectionFile,
				(PCHAR)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSLen + iDataSegLen,
				NULL,
				OverlappedData,
				TimeOut
			);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request 0x%x %d\n", ntStatus, result));
		}

	} else {

		ntStatus = SendIt(
				&LSS->ConnectionFile,
				(PCHAR)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSLen + iDataSegLen,
				&result,
				NULL,
				TimeOut
			);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request 0x%x\n", ntStatus));
		} else if((result != (sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSLen + iDataSegLen))) {
			KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request 0x%x %d\n", ntStatus, result));
			ntStatus = STATUS_UNSUCCESSFUL;
		}
	}

	return ntStatus;
}

VOID
LspRequestCompletionRoutine(
	IN PLSTRANS_OVERLAPPED	OverlappedData
){

	if(OverlappedData->CompletionEvent)
		KeSetEvent(OverlappedData->CompletionEvent, IO_NETWORK_INCREMENT, FALSE);
}

VOID
__inline
LsprotoWaitForCompEvent(PKEVENT	RequestCompEvent, PKEVENT BodyCompEvent) {
	// Wait until the overlapped send-request finishes.
	if(RequestCompEvent)
		KeWaitForSingleObject(RequestCompEvent, Executive, KernelMode, FALSE, NULL);
	// Wait until the overlapped send-body finishes.
	if(BodyCompEvent)
		KeWaitForSingleObject(BodyCompEvent, Executive, KernelMode, FALSE, NULL);
}

/////////////////////////////////////////////////////////////
//
//	Lanscsi/ATA Protocol Version 1.0
//

//
// Send an ATA command request.
//

NTSTATUS
IdeLspRequest_V10(
	   IN PLANSCSI_SESSION	LSS,
	   IN PLANSCSI_PDUDESC	PduDesc,
	   OUT PBYTE			PduResponse
   )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS			pdu;
	NTSTATUS						ntStatus;
	ULONG							result;
	UCHAR							iCommandReg;
	BOOLEAN							sendData, recvData;
	KEVENT							sendRequestCompEvent;
	KEVENT							sendBodyCompEvent;

	ASSERT(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0);
	
	//
	// Check Parameters.
	//

	if(PduResponse == NULL) {
		KDPrintM(DBG_PROTO_ERROR, ("pResult is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(((PduDesc->Command == WIN_READ) || (PduDesc->Command == WIN_WRITE))
		&& (PduDesc->DataBuffer == NULL)) {
		KDPrintM(DBG_PROTO_ERROR, ("Buffer is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	
	//
	// Clear transmit log
	//

	sendData = FALSE;
	recvData = FALSE;

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = PduDesc->Opcode;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	LSS->CommandTag++;
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);
	pRequestHeader->TargetID = HTONL(PduDesc->TargetId);
	pRequestHeader->LUN = 0;

	// Using Target ID. LUN is always 0.
	if(PduDesc->TargetId == 0)
		pRequestHeader->DEV = 0;
	else 
		pRequestHeader->DEV = 1;

	switch(PduDesc->Command) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;

			recvData = TRUE;
			
			if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
				pRequestHeader->Command = WIN_READDMA_EXT;
			} else {
				pRequestHeader->Command = WIN_READDMA;
			}
		}
		break;
	case WIN_WRITE:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 1;
			
			sendData = TRUE;

			if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
				pRequestHeader->Command = WIN_WRITEDMA_EXT;
			} else {
				pRequestHeader->Command = WIN_WRITEDMA;
			}
		}
		break;
	case WIN_VERIFY:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
				pRequestHeader->Command = WIN_VERIFY_EXT;
			} else {
				pRequestHeader->Command = WIN_VERIFY;
			}
		}
		break;
	case WIN_IDENTIFY:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;

			recvData = TRUE;
			
			pRequestHeader->Command = WIN_IDENTIFY;

			if(PduDesc->DataBufferLength != IDENTIFY_DATA_LENGTH) {
				ASSERT(FALSE);
				return STATUS_INVALID_PARAMETER;
			}
		}
		break;
	case WIN_SETFEATURES:
		{
			//
			// NDAS chip 1.0 does support all features
			// even though it returns success status.
			//

			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature = PduDesc->Feature;
			pRequestHeader->SectorCount_Curr = (BYTE)PduDesc->BlockCount;
			pRequestHeader->Command = WIN_SETFEATURES;
		}
		break;
	default:
		KDPrintM(DBG_PROTO_ERROR, ("Not Supported IDE Command(%08x).\n", (ULONG)PduDesc->Command));
		return STATUS_NOT_SUPPORTED;
	}

	pRequestHeader->Feature = 0;

	//
	// Set up block address and count.
	//

	if((PduDesc->Command == WIN_READ)
		|| (PduDesc->Command == WIN_WRITE)
		|| (PduDesc->Command == WIN_VERIFY)){

		if(!(PduDesc->Flags & PDUDESC_FLAG_LBA)) {
			KDPrintM(DBG_PROTO_ERROR, ("LBA not supported. CHS not implemented.\n"));
			ASSERT(FALSE);
			return STATUS_INVALID_PARAMETER;
		}

		pRequestHeader->LBA = 1;
		
		if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
			
			pRequestHeader->LBALow_Curr = (_int8)(PduDesc->DestAddr);
			pRequestHeader->LBAMid_Curr = (_int8)(PduDesc->DestAddr >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(PduDesc->DestAddr >> 16);
			pRequestHeader->LBALow_Prev = (_int8)(PduDesc->DestAddr >> 24);
			pRequestHeader->LBAMid_Prev = (_int8)(PduDesc->DestAddr >> 32);
			pRequestHeader->LBAHigh_Prev = (_int8)(PduDesc->DestAddr >> 40);

			pRequestHeader->SectorCount_Curr = (_int8)PduDesc->BlockCount;
			pRequestHeader->SectorCount_Prev = (_int8)(PduDesc->BlockCount >> 8);
			
		} else {
			
			pRequestHeader->LBALow_Curr = (_int8)(PduDesc->DestAddr);
			pRequestHeader->LBAMid_Curr = (_int8)(PduDesc->DestAddr >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(PduDesc->DestAddr >> 16);
			pRequestHeader->LBAHeadNR = (_int8)(PduDesc->DestAddr >> 24);

			pRequestHeader->SectorCount_Curr = (_int8)PduDesc->BlockCount;
		}
	}

	//
	//	Set data transfer length
	//
	if(recvData || sendData) {
		pRequestHeader->DataTransferLength = HTONL(PduDesc->DataBufferLength);
	}

	// Save IDE command code.
	iCommandReg = pRequestHeader->Command;

	//
	// Send Request header
	//
	// We do not care about asynchronous result of the header,
	// so set Null overlapped data
	// IdeLspSendRequest() might encrypt the request depending on the configuration.
	// Do not use this header before receiving the reply
	//
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pRequestHeader = NULL;

	// Set up overlapped structure
	KeInitializeEvent(&sendRequestCompEvent, NotificationEvent, FALSE);
	LSS->RequestOverlappedData.IoCompleteRoutine = LspRequestCompletionRoutine;
	LSS->RequestOverlappedData.CompletionEvent = &sendRequestCompEvent;
	LSS->RequestOverlappedData.UserContext = LSS;

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, &LSS->RequestOverlappedData, NULL);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&sendRequestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request\n"));
		// Assume sending communication error as retransmit.
		PduDesc->Retransmits = 1;
		return STATUS_PORT_DISCONNECTED;
	}

	//
	// Send data
	//

	if(sendData) {

		PCHAR		ActualDataBuffer = PduDesc->DataBuffer;
		BOOLEAN		RecoverBuffer = FALSE;

		ASSERT(recvData == FALSE);

		//
		//	Protocol encryption
		//
		if(LSS->DataEncryptAlgo != 0) {
			//
			//	If the data buffer of PDUDESC is volatile, encrypt it directly. 
			//
			if(PduDesc->Flags & PDUDESC_FLAG_VOLATILE_BUFFER) {

				Encrypt32SP(
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						&(LSS->EncryptIR[0])
					);

			//
			//	If the encryption buffer is available, copy to the encryption buffer and encrypt it.
			//
			} else if(	LSS->EncryptBuffer &&
						LSS->EncryptBufferLength >= PduDesc->DataBufferLength) {

					Encrypt32SPAndCopy(
							LSS->EncryptBuffer,
							ActualDataBuffer,
							PduDesc->DataBufferLength,
							&(LSS->EncryptIR[0])
						);
					ActualDataBuffer = LSS->EncryptBuffer;

			} else {
				Encrypt32SP(
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						&(LSS->EncryptIR[0])
					);
				RecoverBuffer = TRUE;
			}
		}

		//
		//	Send body of data
		//

		// Set up overlapped structure
		KeInitializeEvent(&sendBodyCompEvent, NotificationEvent, FALSE);
		LSS->BodyOverlappedData.IoCompleteRoutine = LspRequestCompletionRoutine;
		LSS->BodyOverlappedData.CompletionEvent = &sendBodyCompEvent;
		LSS->BodyOverlappedData.UserContext = LSS;

		ntStatus = SendIt(
						&LSS->ConnectionFile,
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						NULL,
						&LSS->BodyOverlappedData,
						NULL
					);
		if(!NT_SUCCESS(ntStatus)) {
			LsprotoWaitForCompEvent(&sendRequestCompEvent, &sendBodyCompEvent);
			KDPrintM(DBG_PROTO_ERROR, ("Error when Send data\n"));
			// Assume sending communication error as retransmit.
			PduDesc->Retransmits = 1;
			return ntStatus;
		}

		//
		// Recover the original data.
		//
		if(	RecoverBuffer &&
			LSS->DataEncryptAlgo != 0) {

			Decrypt32SP(
					ActualDataBuffer,
					PduDesc->DataBufferLength,
					&(LSS->DecryptIR[0])
				);
		}
	}

	//
	// Receive data
	//

	if(recvData) {
		ASSERT(sendData == FALSE);

		ntStatus = RecvIt(
						&LSS->ConnectionFile,
						PduDesc->DataBuffer,
						PduDesc->DataBufferLength,
						&result,
						NULL,
						PduDesc->TimeOut
					);
		if(!NT_SUCCESS(ntStatus)
			|| result != PduDesc->DataBufferLength) {
			LsprotoWaitForCompEvent(&sendRequestCompEvent, NULL);
			KDPrintM(DBG_PROTO_ERROR, ("Error when Receive Data\n"));
			// Assume receiving communication error as packet loss.
			PduDesc->PacketLoss = 1;
			return STATUS_PORT_DISCONNECTED;
		}

		//
		// Decrypt Data.
		//
		if(LSS->DataEncryptAlgo != 0) {

			Decrypt32SP(
					PduDesc->DataBuffer,
					PduDesc->DataBufferLength,
					(unsigned char*)&(LSS->DecryptIR[0])
				);
		}
	}

	//
	// Receive reply header.
	//

	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)PduBuffer, &pdu, NULL, PduDesc->TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		// Assume receiving communication error as packet loss.
		PduDesc->PacketLoss = 1;
		return STATUS_PORT_DISCONNECTED;
	}

	// Check Reply Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != IDE_RESPONSE)
		|| (pReplyHeader->F == 0)) {

		LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header. 0x%x\n", pReplyHeader->Flags));
		return ntStatus;
	}
#if DBG
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Failed. Response %d %d Command/StatReg: 0x%x ErrReg=0x%x\n", 
			NTOHL(pReplyHeader->DataTransferLength),
			NTOHL(pReplyHeader->DataSegLen),
			pReplyHeader->Command,
			pReplyHeader->Feature));
	}
#endif

	//
	//	Set return values
	//

	*PduResponse = pReplyHeader->Response;

	//
	// Query transport statistics
	//

	ntStatus = LstransQueryInformation(
				&LSS->ConnectionFile,
				LPXTDI_QUERY_CONNECTION_TRANSSTAT,
				&LSS->TransStat,
				sizeof(LS_TRANS_STAT));
	if(NT_SUCCESS(ntStatus)) {
		PduDesc->Retransmits = LSS->TransStat.Retransmits;
		PduDesc->PacketLoss = LSS->TransStat.PacketLoss;
	} else {
		KDPrintM(DBG_PROTO_ERROR, ("LstransQueryInformation() failed. STATUS=%08lx\n"));
		PduDesc->Retransmits = 0;
		PduDesc->PacketLoss = 0;
	}

	//
	// Status register is not valid in NDAS chip 1.0.
	//
	PduDesc->Command = 0;						// Status register.
	PduDesc->Feature = pReplyHeader->Feature;	// Error register

	if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
		PduDesc->DestAddr = 
				((UINT64)pReplyHeader->LBAHigh_Prev << 40) +
				((UINT64)pReplyHeader->LBAMid_Prev  << 32) +
				((UINT64)pReplyHeader->LBALow_Prev  << 24) +
				((UINT64)pReplyHeader->LBAHigh_Curr << 16) +
				((UINT64)pReplyHeader->LBAMid_Curr  << 8) +
				((UINT64)pReplyHeader->LBALow_Curr);
		PduDesc->BlockCount = 
				((UINT32)pReplyHeader->SectorCount_Prev << 8) + 
				((UINT32)pReplyHeader->SectorCount_Curr);
	} else {
		PduDesc->BlockCount = pReplyHeader->SectorCount_Curr;
		PduDesc->DestAddr = 
				((UINT64)pReplyHeader->Device       << 24) +
				((UINT64)pReplyHeader->LBAHigh_Curr << 16) +
				((UINT64)pReplyHeader->LBAMid_Curr  << 8) +
				((UINT64)pReplyHeader->LBALow_Curr);
	}

	LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);

	//
	// Communication was successful no matter what the command error is.
	//

	return STATUS_SUCCESS;
}




/////////////////////////////////////////////////////////////
//
//	Lanscsi/IDE Protocol Version 1.1. This fuction can cover version 1.0~2.0 chip.
//
NTSTATUS
IdeLspLogin_V11(
		PLANSCSI_SESSION	LSS,
		PLSSLOGIN_INFO		LoginInfo,
		PLARGE_INTEGER		TimeOut
	 )
{
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_SECURITY					pParamSecu;
	PAUTH_PARAMETER_CHAP				pParamChap;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	LANSCSI_PDU_POINTERS				pdu;
	UINT16								iSubSequence;
	UINT32								CHAP_I;
	NTSTATUS							ntStatus;

	UNREFERENCED_PARAMETER(LoginInfo);
	
	LSS->LastLoginError = LSS_LOGIN_SUCCESS;
	
	//
	// Init.
	//
	iSubSequence = 0;

	LSS->SessionPhase = FLAG_SECURITY_PHASE;
	LSS->HeaderEncryptAlgo = 0;
	LSS->HeaderDigestAlgo = 0;
	LSS->DataEncryptAlgo = 0;
	LSS->DataDigestAlgo = 0;

	KDPrintM(DBG_PROTO_TRACE, ("UserId:%08lx TargetId:%d HWProtoType:0x%x, HWProtoVersion:0x%x\n",
							LSS->UserID,
							LSS->LanscsiTargetID,
							LSS->HWProtoType,
							LSS->HWProtoVersion
						));


	//
	// First Packet.
	//
	// Version and Authentication method matching
	// Including login type
	//

	memset(LSS->PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)LSS->PduBuffer;
	
	pRequestHeader->Opcode = LOGIN_REQUEST;
	pRequestHeader->HPID = HTONL(LSS->HPID);

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	} else {
		KDPrintM(DBG_PROTO_ERROR, ("Invalid LSPROTO_VERSION.\n"));
		ASSERT(FALSE);
		LSS->LastLoginError = LSS_LOGIN_INTERNAL_ERROR;
		return STATUS_INVALID_PARAMETER;
	}

	pRequestHeader->CSubPacketSeq = HTONS(iSubSequence);
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);


	//
	//	Supported version parameter setting
	//	Supported authentication method setting
	//

	pRequestHeader->ParameterType = 1;
	pRequestHeader->ParameterVer = 0;
	pRequestHeader->VerMax = LSS->HWVersion;
	pRequestHeader->VerMin = 0;

	pParamSecu = (PBIN_PARAM_SECURITY)&LSS->PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = LSS->LoginType;
	pParamSecu->AuthMethod = HTONS(AUTH_METHOD_CHAP);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pdu.pDataSeg = (char *)pParamSecu;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	} else {
		LSS->LastLoginError = LSS_LOGIN_INTERNAL_ERROR;	
		return STATUS_NOT_IMPLEMENTED;
	}

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send First Request\n"));
		LSS->LastLoginError = LSS_LOGIN_SEND_FAIL;
		return ntStatus;
	}

	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)LSS->PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("First Can't Read Reply.\n"));
		return ntStatus;
	}

	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pReplyHeader->T != 0)
		|| (pReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pReplyHeader->NSG != FLAG_SECURITY_PHASE)
		|| (pReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		KDPrintM(DBG_PROTO_ERROR, ("BAD First Reply Header.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("First Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		if (pReplyHeader->Response == LANSCSI_RESPONSE_RI_VERSION_MISMATCH) {
			KDPrintM(DBG_PROTO_ERROR, ("Tried to login with invalid version of LSprotocol.\n"));
			return STATUS_INVALID_LOGON_TYPE;
		}
		LSS->LastLoginError = LSS_LOGIN_ERROR_REPLY_1;
		return STATUS_LOGON_FAILURE;
	}
	
	// Check Data segment.
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		if((NTOHL(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REPLY)
			|| (pdu.pDataSeg == NULL)) {
		
			KDPrintM(DBG_PROTO_ERROR, ("BAD First Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if((NTOHS(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REPLY)
			|| (pdu.pAHS == NULL)) {
		
			KDPrintM(DBG_PROTO_ERROR, ("BAD First Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}
	}

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != LSS->LoginType
		|| pParamSecu->AuthMethod != HTONS(AUTH_METHOD_CHAP)) {

		KDPrintM(DBG_PROTO_ERROR, ("BAD First Reply Parameters.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}

	// Store Data.
	LSS->RPID = NTOHS(pReplyHeader->RPID);
	LSS->HWRevision = NTOHS(pReplyHeader->Revision);
	
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	KDPrintM(DBG_PROTO_INFO, ("Version %d Revision %d Auth %d\n", 
		pReplyHeader->VerActive, 
		NTOHS(pReplyHeader->Revision),
		NTOHS(pParamSecu->AuthMethod))
		);

	iSubSequence++;


	// 
	//	Second Packet.
	//
	//	First Authentication stage.
	//
	//	Send challenge algorithm.
	//	Challenge values will be replied.
	//

	memset(LSS->PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)LSS->PduBuffer;

	pRequestHeader->Opcode = LOGIN_REQUEST;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	}

	pRequestHeader->CSubPacketSeq = HTONS(iSubSequence);
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);


	//
	//	Set up challenge algorithm.
	//	including login type.
	//

	pRequestHeader->ParameterType = 1;
	pRequestHeader->ParameterVer = 0;

	pParamSecu = (PBIN_PARAM_SECURITY)&LSS->PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = LSS->LoginType;
	pParamSecu->AuthMethod = HTONS(AUTH_METHOD_CHAP);

	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	pParamChap->CHAP_A = NTOHL(HASH_ALGORITHM_MD5);

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pdu.pDataSeg = (char *)pParamSecu;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	}

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Second Request\n"));
		LSS->LastLoginError = LSS_LOGIN_SEND_FAIL;
		return ntStatus;
	}
	
	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)LSS->PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Second Can't Read Reply.\n"));
		LSS->LastLoginError = LSS_LOGIN_RECV_FAIL;
		return ntStatus;
	}

	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pReplyHeader->T != 0)
		|| (pReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pReplyHeader->NSG != FLAG_SECURITY_PHASE)
		|| (pReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Second Reply Header.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Second Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		LSS->LastLoginError = LSS_LOGIN_ERROR_REPLY_2;
		return STATUS_LOGON_FAILURE;
	}
	
	// Check Data segment.
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		if((NTOHL(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REPLY)
			|| (pdu.pDataSeg == NULL)) {
		
			KDPrintM(DBG_PROTO_ERROR, ("BAD Second Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}	
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if((NTOHS(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REPLY)
			|| (pdu.pAHS == NULL)) {

			KDPrintM(DBG_PROTO_ERROR, ("BAD Second Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}
	}

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != LSS->LoginType
		|| pParamSecu->AuthMethod != HTONS(AUTH_METHOD_CHAP)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Second Reply Parameters.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	//
	// Save Challenge values
	//
	pParamChap = &pParamSecu->ChapParam;

	// the stamp that should returns to the remote without modification.
	CHAP_I = NTOHL(pParamChap->CHAP_I);
	// 32 bit challenge value
	LSS->CHAP_C = NTOHL(pParamChap->CHAP_C[0]);

	KDPrintM(DBG_PROTO_TRACE, ("Hash %d, Challenge %d\n", 
		NTOHL(pParamChap->CHAP_A), 
		LSS->CHAP_C)
		);

	iSubSequence++;

	// 
	// Third Packet.
	//
	// Last Authentication stage.
	//
	// User ID and password matching.
	//

	memset(LSS->PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)LSS->PduBuffer;
	pRequestHeader->Opcode = LOGIN_REQUEST;
	pRequestHeader->T = 1;
	pRequestHeader->CSG = FLAG_SECURITY_PHASE;
	pRequestHeader->NSG = FLAG_LOGIN_OPERATION_PHASE;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	}
	pRequestHeader->CSubPacketSeq = HTONS(iSubSequence);
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);
	pRequestHeader->ParameterType = 1;
	pRequestHeader->ParameterVer = 0;

	pParamSecu = (PBIN_PARAM_SECURITY)&LSS->PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = LSS->LoginType;
	pParamSecu->AuthMethod = HTONS(AUTH_METHOD_CHAP);

	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	pParamChap->CHAP_A = HTONL(HASH_ALGORITHM_MD5);
	pParamChap->CHAP_I = HTONL(CHAP_I);
	pParamChap->CHAP_N = HTONL(LSS->UserID);

	//
	// Hashing
	//
	Hash32To128(
		(unsigned char*)&LSS->CHAP_C, 
		(unsigned char*)pParamChap->CHAP_R, 
		(unsigned char*)&LSS->Password
		);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pdu.pDataSeg = (char *)pParamSecu;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	}
	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Third Request\n"));
		LSS->LastLoginError = LSS_LOGIN_SEND_FAIL;
		return ntStatus;
	}
	
	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)LSS->PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Third Can't Read Reply.\n"));
		LSS->LastLoginError = LSS_LOGIN_RECV_FAIL;		
		return ntStatus;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pReplyHeader->T == 0)
		|| (pReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
		|| (pReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Third Reply Header.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		// Until 2.0, no special error code is returned. Usually in this step, password mismatch is cause of failure.
		LSS->LastLoginError = LSS_LOGIN_INVALID_PASSWORD;

		KDPrintM(DBG_PROTO_ERROR, ("Third Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		return STATUS_LOGON_FAILURE;
	}
	
	// Check Data segment.
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		if((NTOHL(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REPLY)
			|| (pdu.pDataSeg == NULL)) {
		
			KDPrintM(DBG_PROTO_ERROR, ("BAD Third Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}	
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if((NTOHL(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REPLY)
			|| (pdu.pAHS == NULL)) {

			KDPrintM(DBG_PROTO_ERROR, ("BAD Third Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}	
	}

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != LSS->LoginType
		|| pParamSecu->AuthMethod != HTONS(AUTH_METHOD_CHAP)) {

		KDPrintM(DBG_PROTO_ERROR, ("BAD Third Reply Parameters.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}

	LSS->SessionPhase = FLAG_LOGIN_OPERATION_PHASE;
	
	iSubSequence++;


	// 
	// Fourth Packet.
	//
	// Negotiation protocol spec.
	//
	// For now, host passively receives and accepts the remote device's
	// negotiation information.
	//

	memset(LSS->PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)LSS->PduBuffer;
	pRequestHeader->Opcode = LOGIN_REQUEST;
	pRequestHeader->T = 1;
	pRequestHeader->CSG = FLAG_LOGIN_OPERATION_PHASE;
	pRequestHeader->NSG = FLAG_FULL_FEATURE_PHASE;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	}

	pRequestHeader->CSubPacketSeq = HTONS(iSubSequence);
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);
	pRequestHeader->ParameterType = 1;
	pRequestHeader->ParameterVer = 0;

	pParamNego = (PBIN_PARAM_NEGOTIATION)&LSS->PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pdu.pDataSeg = (char *)pParamNego;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamNego;
	}
	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Fourth Request\n"));
		LSS->LastLoginError = LSS_LOGIN_SEND_FAIL;
		return ntStatus;
	}

	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)LSS->PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Fourth Can't Read Reply.\n"));
		LSS->LastLoginError = LSS_LOGIN_RECV_FAIL;
		return ntStatus;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pReplyHeader->T == 0)
		|| ((pReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
		|| ((pReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
		|| (pReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Fourth Reply Header.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Fourth Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		LSS->LastLoginError = LSS_LOGIN_ERROR_REPLY_4;
		return STATUS_LOGON_FAILURE;
	}
	
	// Check Data segment.
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		if((NTOHL(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY)
			|| (pdu.pDataSeg == NULL)) {
		
			KDPrintM(DBG_PROTO_ERROR, ("BAD Fourth Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}	
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if((NTOHL(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY)
			|| (pdu.pAHS == NULL)) {

			KDPrintM(DBG_PROTO_ERROR, ("BAD Fourth Reply Data.\n"));
			LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
			return STATUS_INVALID_PARAMETER;
		}
	}

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	} else if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
	}

	if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
		KDPrintM(DBG_PROTO_ERROR, ("BAD Fourth Reply Parameters.\n"));
		LSS->LastLoginError = LSS_LOGIN_BAD_REPLY;
		return STATUS_INVALID_PARAMETER;
	}
	
	KDPrintM(DBG_PROTO_TRACE, ("Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n", 
		pParamNego->HWType, pParamNego->HWVersion,
		NTOHL(pParamNego->NRSlot), NTOHL(pParamNego->MaxBlocks),
		NTOHL(pParamNego->MaxTargetID), NTOHL(pParamNego->MaxLUNID))
		);
	
	LSS->HWType = pParamNego->HWType;
	LSS->HWVersion = pParamNego->HWVersion;
	// Revision is set earlier..
	LSS->NumberofSlot = NTOHL(pParamNego->NRSlot);
	LSS->MaxBlocks = NTOHL(pParamNego->MaxBlocks);
	LSS->MaxTargets = NTOHL(pParamNego->MaxTargetID);
	LSS->MaxLUs = NTOHL(pParamNego->MaxLUNID);
	LSS->HeaderEncryptAlgo = pParamNego->HeaderEncryptAlgo;
	LSS->DataEncryptAlgo = pParamNego->DataEncryptAlgo;
	LSS->HeaderDigestAlgo = 0;
	LSS->DataDigestAlgo = 0;

	KDPrintM(DBG_PROTO_INFO, ("HeaderEnc=%d DataEnc=%d\n", LSS->HeaderEncryptAlgo, LSS->DataEncryptAlgo));

	LSS->SessionPhase = FLAG_FULL_FEATURE_PHASE;

	//
	// Calculate Intermediate Result for the protocol encryption to speed up.
	//
	{
		PUCHAR	pPassword;
		PUCHAR	pKey;

		pPassword = (PUCHAR)&(LSS->Password);
		pKey = (PUCHAR)&(LSS->CHAP_C);

		LSS->EncryptIR[0] = pPassword[1] ^ pPassword[7] ^ pKey[3];
		LSS->EncryptIR[1] = pPassword[0] ^ pPassword[3] ^ pKey[0];
		LSS->EncryptIR[2] = pPassword[2] ^ pPassword[6] ^ pKey[2];
		LSS->EncryptIR[3] = pPassword[4] ^ pPassword[5] ^ pKey[1];

		LSS->DecryptIR[0] = ~(pPassword[0] ^ pPassword[3] ^ pKey[0]);
		LSS->DecryptIR[1] = pPassword[2] ^ pPassword[6] ^ pKey[2];
		LSS->DecryptIR[2] = pPassword[4] ^ pPassword[5] ^ ~(pKey[1]);
		LSS->DecryptIR[3] = pPassword[1] ^ pPassword[7] ^ pKey[3];
	}
	LSS->LastLoginError = LSS_LOGIN_SUCCESS;
	return STATUS_SUCCESS;
}

NTSTATUS
IdeLspLogout_V11(
		IN	PLANSCSI_SESSION	LSS,
		IN	PLARGE_INTEGER		TimeOut
	)
{
	PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	PFILE_OBJECT						connectionFileObject;
	NTSTATUS							ntStatus;
	
	//
	// Init.
	//
	connectionFileObject = LSS->ConnectionFile.ConnectionFileObject;
	
	memset(LSS->PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)LSS->PduBuffer;
	pRequestHeader->Opcode = LOGOUT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	LSS->CommandTag++;
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request\n"));
		return ntStatus;
	}
	
	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)LSS->PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		return ntStatus;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGOUT_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header.\n"));
		return ntStatus;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		return ntStatus;
	}

	LSS->SessionPhase = LOGOUT_PHASE;
	
	return STATUS_SUCCESS;
}

NTSTATUS
IdeLspTextTargetList_V11(
		PLANSCSI_SESSION	LSS,
		PTARGETINFO_LIST	TargetList,
		PLARGE_INTEGER		TimeOut
	)
{
	BYTE								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_LIST				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	int									i;
	PFILE_OBJECT						connectionFileObject;
	NTSTATUS							ntStatus;

	//
	// Init.
	//
	connectionFileObject = LSS->ConnectionFile.ConnectionFileObject;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	}
	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = HTONL(++LSS->CommandTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (char *)pParam;
	}
	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParam;
	}

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request\n"));
		return ntStatus;
	}
	
	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		return ntStatus;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header.\n"));
		return ntStatus;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		return ntStatus;
	}
	
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		KDPrintM(DBG_PROTO_ERROR, ("No Data Segment.\n"));
		return ntStatus;		
	}
	}

	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if(pReplyHeader->AHSLen < BIN_PARAM_SIZE_REPLY) {
			KDPrintM(DBG_PROTO_ERROR, ("No Data Segment.\n"));
			return ntStatus;		
		}
	}


	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
	}

	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
	}

	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
		KDPrintM(DBG_PROTO_ERROR, ("Bad Parameter Type.\n"));
		return ntStatus;			
	}
	KDPrintM(DBG_PROTO_INFO, ("NR Targets : %d\n", pParam->NRTarget));
	TargetList->TargetInfoEntryCnt = pParam->NRTarget;
	
	for(i = 0; i < pParam->NRTarget; i++) {
		PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
		int								iTargetId;
		
		pTarget = &pParam->PerTarget[i];
		iTargetId = NTOHL(pTarget->TargetID);
		
		KDPrintM(DBG_PROTO_INFO, ("NR Targets  %d: Target ID: 0x%x, NR_RW: %d, NR_RO: %d, Data:0x%x \n", i, 
			NTOHL(pTarget->TargetID), 
			pTarget->NRRWHost,
			pTarget->NRROHost,
			pTarget->TargetData)
			);

		TargetList->TargetInfoEntry[i].TargetID = iTargetId;
		TargetList->TargetInfoEntry[i].NRROHost = pTarget->NRROHost;
		TargetList->TargetInfoEntry[i].NRRWHost = pTarget->NRRWHost;
		TargetList->TargetInfoEntry[i].TargetData = pTarget->TargetData;
	}
	
	return STATUS_SUCCESS;
}

NTSTATUS
IdeLspTextTargetData_V11(
			 PLANSCSI_SESSION	LSS,
			 BYTE				GetorSet,
			 UINT32				TargetID,
			 PTARGET_DATA		TargetData,
			 PLARGE_INTEGER		TimeOut
	)
{
	BYTE								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_DATA				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	PFILE_OBJECT						connectionFileObject;
	NTSTATUS							ntStatus;

	//
	// Init.
	//
	connectionFileObject = LSS->ConnectionFile.ConnectionFileObject;
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pRequestHeader->DataSegLen = HTONL(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	pRequestHeader->AHSLen = 0;
	}
	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = HTONS(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	}

	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = HTONL(++LSS->CommandTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_DATA;
	pParam->GetOrSet = GetorSet;

	if(GetorSet == PARAMETER_OP_SET)
		pParam->TargetData = *TargetData;

	pParam->TargetID = HTONL(TargetID);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (char *)pParam;
	}

	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParam;
	}

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Error When Send Request\n"));
		return ntStatus;
	}
	
	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)PduBuffer, &pdu, NULL, TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		return ntStatus;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header.\n"));
		return ntStatus;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Failed. RESPONSE:%x\n", (int)pReplyHeader->Response));
		return ntStatus;
	}
	
	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
			KDPrintM(DBG_PROTO_ERROR, ("No Data Segment.\n"));
			return ntStatus;		
		}
	}
	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		if(pReplyHeader->AHSLen < BIN_PARAM_SIZE_REPLY) {
			KDPrintM(DBG_PROTO_ERROR, ("No Data Segment.\n"));
			return ntStatus;		
		}
	}

	if(LSS->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
	}

	if(LSS->HWProtoVersion >= LSIDEPROTO_VERSION_1_1) {
		pParam = (PBIN_PARAM_TARGET_DATA)pdu.pAHS;
	}

	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
		KDPrintM(DBG_PROTO_ERROR, ("Bad Parameter Type.\n"));
		return ntStatus;			
	}

	*TargetData = pParam->TargetData;

	KDPrintM(DBG_PROTO_INFO, ("TargetID : %d, GetorSet %d, Target Data %d\n", 
		NTOHL(pParam->TargetID), pParam->GetOrSet, *TargetData)
		);
	
	return STATUS_SUCCESS;
}


NTSTATUS
IdeLspRequest_V11(
	   IN PLANSCSI_SESSION	LSS,
	   IN PLANSCSI_PDUDESC	PduDesc,
	   OUT PBYTE			PduResponse
	)
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	NTSTATUS							ntStatus;
	ULONG								result;
	unsigned _int8						iCommandReg;
	BOOLEAN								sendData, recvData;
	KEVENT								sendRequestCompEvent;
	KEVENT								sendBodyCompEvent;

	//
	// Check Parameters.
	//
	if(PduResponse == NULL) {
		KDPrintM(DBG_PROTO_ERROR, ("pResult is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	if(((PduDesc->Command == WIN_READ) || (PduDesc->Command == WIN_WRITE))
		&& (PduDesc->DataBuffer == NULL)) {
		KDPrintM(DBG_PROTO_ERROR, ("Buffer is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	//
	// Clear transmit log
	//

	sendData = FALSE;
	recvData = FALSE;

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = HTONL(++LSS->CommandTag);
	pRequestHeader->TargetID = HTONL(PduDesc->TargetId);
	pRequestHeader->LUN = 0;
	
	// Using Target ID. LUN is always 0.
	if(PduDesc->TargetId == 0)
		pRequestHeader->DEV = 0;
	else 
		pRequestHeader->DEV = 1;
	
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0;

	switch(PduDesc->Command) {
	case WIN_READ:
		{

			pRequestHeader->R = 1;
			pRequestHeader->W = 0;

			recvData = TRUE;

			if(PduDesc->Flags & PDUDESC_FLAG_DMA) {
				if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
					pRequestHeader->Command = WIN_READDMA_EXT;
				
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_READDMA;
				}
				pRequestHeader->COM_TYPE_D_P = 1;

			} else {
				if(!(PduDesc->Flags & PDUDESC_FLAG_PIO)) {
					KDPrintM(DBG_PROTO_ERROR, ("WIN_READ: I/O Mode is not determined. 0x%x\n", PduDesc->Flags));
					return STATUS_INVALID_PARAMETER;
				}

				if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
					pRequestHeader->Command = WIN_READ_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_READ;
				}
				pRequestHeader->COM_TYPE_D_P = 0;
			}

			pRequestHeader->COM_TYPE_R = 1;
			pRequestHeader->COM_LENG = (HTONL(PduDesc->DataBufferLength) >> 8);
		}
		break;
	case WIN_WRITE:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 1;

			sendData = TRUE;

			if(PduDesc->Flags & PDUDESC_FLAG_DMA) {
				if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {				
					pRequestHeader->Command = WIN_WRITEDMA_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITEDMA;
				}
				pRequestHeader->COM_TYPE_D_P = 1;
			} else {
				if(!(PduDesc->Flags & PDUDESC_FLAG_PIO)) {
					KDPrintM(DBG_PROTO_ERROR, ("WIN_WRITE: I/O Mode is not determined. 0x%x\n", PduDesc->Flags));
					return STATUS_INVALID_PARAMETER;
				}

				if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
					pRequestHeader->Command = WIN_WRITE_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITE;
				}
				pRequestHeader->COM_TYPE_D_P = 0;
			}
			pRequestHeader->COM_TYPE_W = 1;
			pRequestHeader->COM_LENG = (HTONL(PduDesc->DataBufferLength) >> 8);
		}
		break;
	case WIN_VERIFY:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
				pRequestHeader->Command = WIN_VERIFY_EXT;
				pRequestHeader->COM_TYPE_E = 1;
			} else {
				pRequestHeader->Command = WIN_VERIFY;
			}
		}
		break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			
			recvData = TRUE;
			pRequestHeader->Command = PduDesc->Command;

			pRequestHeader->COM_TYPE_R = 1;
			pRequestHeader->COM_LENG = (HTONL(IDENTIFY_DATA_LENGTH) >> 8);

			if(PduDesc->DataBufferLength != IDENTIFY_DATA_LENGTH) {
				ASSERT(FALSE);
				return STATUS_INVALID_PARAMETER;
			}
		}
		break;
	case WIN_SETFEATURES:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = PduDesc->Feature;
			pRequestHeader->SectorCount_Curr = (BYTE)PduDesc->BlockCount;
			pRequestHeader->Command = WIN_SETFEATURES;

			KDPrintM(DBG_PROTO_TRACE, ("SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr));
		}
		break;
	case WIN_SETMULT:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = (BYTE)PduDesc->BlockCount;
			pRequestHeader->Command = WIN_SETMULT;
		}
		break;
	case WIN_CHECKPOWERMODE1:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_CHECKPOWERMODE1;
		}
		break;
	case WIN_STANDBY:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_STANDBY;
		}
		break;
	case WIN_FLUSH_CACHE:
	case WIN_FLUSH_CACHE_EXT:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;

			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = (PduDesc->Flags & PDUDESC_FLAG_LBA48) ? 
				WIN_FLUSH_CACHE_EXT : WIN_FLUSH_CACHE;
		}
		break;
	case WIN_SMART:
		{
			if(PduDesc->Flags & PDUDESC_FLAG_DATA_IN) {
				if(!(PduDesc->Flags & PDUDESC_FLAG_PIO)) {
					KDPrintM(DBG_PROTO_ERROR, ("WIN_SMART: I/O Mode is not determined. 0x%x\n", PduDesc->Flags));
					return STATUS_INVALID_PARAMETER;
				}
				pRequestHeader->R = 1;
				recvData = TRUE;
			}

			pRequestHeader->Feature_Curr = PduDesc->Feature;
			pRequestHeader->SectorCount_Curr = (BYTE)PduDesc->BlockCount;
			pRequestHeader->LBALow_Curr = PduDesc->Param8[0];
			pRequestHeader->LBAMid_Curr = PduDesc->Param8[1];
			pRequestHeader->LBAHigh_Curr = PduDesc->Param8[2];
			pRequestHeader->Device = PduDesc->Param8[3];
			pRequestHeader->Command = WIN_SMART;

			if(PduDesc->Flags & PDUDESC_FLAG_DATA_IN) {
				pRequestHeader->COM_TYPE_R = 1;
				pRequestHeader->COM_LENG = (HTONL(PduDesc->DataBufferLength) >> 8);
			}
		}
		break;
	default:
		KDPrintM(DBG_PROTO_ERROR, ("Not Supported IDE Command(%08x).\n", (ULONG)PduDesc->Command));
		return STATUS_NOT_SUPPORTED;
	}

	//
	// Set up block address and count.
	//

	if((PduDesc->Command == WIN_READ)
		|| (PduDesc->Command == WIN_WRITE)
		|| (PduDesc->Command == WIN_VERIFY)){

		if(!(PduDesc->Flags & PDUDESC_FLAG_LBA)) {
			KDPrintM(DBG_PROTO_ERROR, ("CHS not supported...\n"));
			return STATUS_INVALID_PARAMETER;
		}
#if DBG
		if(PduDesc->Flags & PDUDESC_FLAG_PIO)
			KDPrintM(DBG_PROTO_ERROR, ("PIO %d\n", (int)pRequestHeader->Command));
#endif
		pRequestHeader->LBA = 1;
		
		if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
			
			pRequestHeader->LBALow_Curr = (_int8)(PduDesc->DestAddr);
			pRequestHeader->LBAMid_Curr = (_int8)(PduDesc->DestAddr >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(PduDesc->DestAddr >> 16);
			pRequestHeader->LBALow_Prev = (_int8)(PduDesc->DestAddr >> 24);
			pRequestHeader->LBAMid_Prev = (_int8)(PduDesc->DestAddr >> 32);
			pRequestHeader->LBAHigh_Prev = (_int8)(PduDesc->DestAddr >> 40);

			pRequestHeader->SectorCount_Curr = (_int8)PduDesc->BlockCount;
			pRequestHeader->SectorCount_Prev = (_int8)(PduDesc->BlockCount >> 8);

		} else {
			
			pRequestHeader->LBALow_Curr = (_int8)(PduDesc->DestAddr);
			pRequestHeader->LBAMid_Curr = (_int8)(PduDesc->DestAddr >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(PduDesc->DestAddr >> 16);
			pRequestHeader->LBAHeadNR = (_int8)(PduDesc->DestAddr >> 24);
			
			pRequestHeader->SectorCount_Curr = (_int8)PduDesc->BlockCount;
		}
	}
	
	//
	//	Set data transfer length
	//
	if(recvData || sendData) {
		pRequestHeader->DataTransferLength = HTONL(PduDesc->DataBufferLength);
	}

	// Save IDE command code.
	iCommandReg = pRequestHeader->Command;

	//
	// Send Request.
	// We do not care about asynchronous result of the header.
	// Just go ahead to send data. Error will be detected at the last moment.
	//
	// NOTE: IdeLspSendRequest() might encrypt the request depending on the configuration.
	// Do not use this header before receiving the reply
	//

	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pRequestHeader = NULL;

	// Set up overlapped structure
	KeInitializeEvent(&sendRequestCompEvent, NotificationEvent, FALSE);
	LSS->RequestOverlappedData.IoCompleteRoutine = LspRequestCompletionRoutine;
	LSS->RequestOverlappedData.CompletionEvent = &sendRequestCompEvent;
	LSS->RequestOverlappedData.UserContext = LSS;

	ntStatus = IdeLspSendRequest_V11(LSS, &pdu, &LSS->RequestOverlappedData, NULL);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&sendRequestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request\n"));
		// Assume sending communication error as retransmit.
		PduDesc->Retransmits = 1;
		return STATUS_PORT_DISCONNECTED;
	}

	//
	// Send data
	//

	if(sendData){

		PCHAR		ActualDataBuffer = PduDesc->DataBuffer;
		BOOLEAN		RecoverBuffer = FALSE;

		ASSERT(recvData == FALSE);
		//
		//	Protocol encryption
		//
		if(LSS->DataEncryptAlgo != 0) {
			//
			//	If the data buffer of PDUDESC is volatile, encrypt it directly. 
			//
			if(PduDesc->Flags & PDUDESC_FLAG_VOLATILE_BUFFER) {

				Encrypt32SP(
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						&(LSS->EncryptIR[0])
					);

			//
			//	If the encryption buffer is available, copy to the encryption buffer and encrypt it.
			//
			} else if(	LSS->EncryptBuffer &&
						LSS->EncryptBufferLength >= PduDesc->DataBufferLength) {

					Encrypt32SPAndCopy(
							LSS->EncryptBuffer,
							ActualDataBuffer,
							PduDesc->DataBufferLength,
							&(LSS->EncryptIR[0])
						);
					ActualDataBuffer = LSS->EncryptBuffer;

			} else {
				Encrypt32SP(
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						&(LSS->EncryptIR[0])
					);
				RecoverBuffer = TRUE;
			}
		}


		//
		//	Send body of data
		//
		// Set up overlapped structure
		KeInitializeEvent(&sendBodyCompEvent, NotificationEvent, FALSE);
		LSS->BodyOverlappedData.IoCompleteRoutine = LspRequestCompletionRoutine;
		LSS->BodyOverlappedData.CompletionEvent = &sendBodyCompEvent;
		LSS->BodyOverlappedData.UserContext = LSS;

		ntStatus = SendIt(
						&LSS->ConnectionFile,
						ActualDataBuffer,
						PduDesc->DataBufferLength,
						NULL,
						&LSS->BodyOverlappedData,
						NULL
					);
		if(!NT_SUCCESS(ntStatus)) {

			LsprotoWaitForCompEvent(&sendRequestCompEvent, &sendBodyCompEvent);
			KDPrintM(DBG_PROTO_ERROR, ("Error when Send data\n"));
			// Assume sending communication error as retransmit.
			PduDesc->Retransmits = 1;
			return ntStatus;
		}

		//
		// Recover the original data.
		//

		if(	RecoverBuffer &&
			LSS->DataEncryptAlgo != 0) {

			Decrypt32SP(
					ActualDataBuffer,
					PduDesc->DataBufferLength,
					&(LSS->DecryptIR[0])
				);
		}
	}

	//
	// Receive data
	//

	if(recvData) {
		ASSERT(sendData == FALSE);

		ntStatus = RecvIt(
			&LSS->ConnectionFile, 
			PduDesc->DataBuffer,
			PduDesc->DataBufferLength,
			&result,
			NULL,
			PduDesc->TimeOut
			);
		if(!NT_SUCCESS(ntStatus)) {
			LsprotoWaitForCompEvent(&sendRequestCompEvent, NULL);
			KDPrintM(DBG_PROTO_ERROR, ("Error when Receive body Data\n"));
			// Assume receiving communication error as packet loss.
			PduDesc->PacketLoss = 1;
			return STATUS_PORT_DISCONNECTED;
		}

		//
		// Decrypt Data.
		//

		if(LSS->DataEncryptAlgo != 0) {

			Decrypt32SP(
					PduDesc->DataBuffer,
					PduDesc->DataBufferLength,
					(unsigned char*)&(LSS->DecryptIR[0])
				);
		}
	}

	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)PduBuffer, &pdu, NULL, PduDesc->TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		// Assume receiving communication error as packet loss.
		PduDesc->PacketLoss = 1;
		return STATUS_PORT_DISCONNECTED;
	}

	//
	// Check Reply Header.
	//	If the reply's consistency is not kept, return failure.
	//	If else, return success with header's response code.
	//

	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;	
	if(pReplyHeader->Opcode != IDE_RESPONSE){		
		LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Reply's Opcode BAD. Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command));
		return STATUS_UNSUCCESSFUL;
	}
	if(pReplyHeader->F == 0){		
		LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Reply's F option BAD. Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command));
		return STATUS_UNSUCCESSFUL;
	}

#if DBG
	if(PduDesc->Command == WIN_CHECKPOWERMODE1){
		KDPrintM(DBG_PROTO_TRACE, ("Check Power mode = 0x%02x\n", (unsigned char)(pReplyHeader->SectorCount_Curr)));
	}

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_ERROR, ("Failed. Response 0x%x 0x%x %d %d Command:0x%x Command/StatReg: 0x%x ErrReg=0x%x\n", 
			pReplyHeader->Response,
			pReplyHeader->Status,
			NTOHL(pReplyHeader->DataTransferLength),
			NTOHL(pReplyHeader->DataSegLen),
			iCommandReg,
			pReplyHeader->Command,
			pReplyHeader->Feature_Curr));
	}
#endif

	//
	//	Set return values
	//

	*PduResponse = pReplyHeader->Response;

	//
	// Query transport statistics
	//

	ntStatus = LstransQueryInformation(
		&LSS->ConnectionFile,
		LPXTDI_QUERY_CONNECTION_TRANSSTAT,
		&LSS->TransStat,
		sizeof(LS_TRANS_STAT));
	if(NT_SUCCESS(ntStatus)) {
		PduDesc->Retransmits = LSS->TransStat.Retransmits;
		PduDesc->PacketLoss = LSS->TransStat.PacketLoss;
	} else {
		KDPrintM(DBG_PROTO_ERROR, ("LstransQueryInformation() failed. STATUS=%08lx\n"));
		PduDesc->Retransmits = 0;
		PduDesc->PacketLoss = 0;
	}

	//
	// Register return values
	//
	// Only error bit is valid, but it ORs command code.
	// So, error bit might not correct depending on command code.
	//

	PduDesc->Command = (pReplyHeader->Command & ~(iCommandReg)) & 0x1; // Status register.
	PduDesc->Feature = pReplyHeader->Feature_Curr;				// Error register
	if(PduDesc->Flags & PDUDESC_FLAG_LBA48) {
		PduDesc->DestAddr = 
				((UINT64)pReplyHeader->LBAHigh_Prev << 40) +
				((UINT64)pReplyHeader->LBAMid_Prev  << 32) +
				((UINT64)pReplyHeader->LBALow_Prev  << 24) +
				((UINT64)pReplyHeader->LBAHigh_Curr << 16) +
				((UINT64)pReplyHeader->LBAMid_Curr  << 8) +
				((UINT64)pReplyHeader->LBALow_Curr);
		PduDesc->BlockCount = 
				((UINT32)pReplyHeader->SectorCount_Prev << 8) + 
				((UINT32)pReplyHeader->SectorCount_Curr);
	} else {
		PduDesc->BlockCount = pReplyHeader->SectorCount_Curr;
		PduDesc->DestAddr = 
				((UINT64)pReplyHeader->Device       << 24) +
				((UINT64)pReplyHeader->LBAHigh_Curr << 16) +
				((UINT64)pReplyHeader->LBAMid_Curr  << 8) +
				((UINT64)pReplyHeader->LBALow_Curr);
	}

	LsprotoWaitForCompEvent(&sendRequestCompEvent, sendData?(&sendBodyCompEvent):NULL);

	//
	// Communication was successful no matter what the command error is.
	//

	return STATUS_SUCCESS;
}





#define IO_DATA					0x00000001
#define LOGITEC					0x00000002
#define IO_DATA_9573			0x00000003

static 
BOOLEAN 
DVDIsDMA(
		PLANSCSI_SESSION	LSS,
		PLANSCSI_PDUDESC	PduDesc,
		BYTE				com
		)
{


	UNREFERENCED_PARAMETER(LSS);

	if(PduDesc->Flags & PDUDESC_FLAG_PIO) 
		return FALSE;
	
	switch(com)
	{
	//
	//	PIO operations.
	//
	case SCSIOP_SEND_KEY:
	case 0xbf :	//	case SCSIOP_SEND_DVD_STRUCTURE:
	case 0xbb : //	case SCSIOP_SET_CD_SPEED:
	case 0xa2 ://	case SCSIOP_SEND_EVENT: //0xA2
	case 0x54 : //	case SCSIOP_SEND_OPC_INFORMATION:
	case SCSIOP_SET_READ_AHEAD:
	case SCSIOP_SEND_VOLUME_TAG: //Set Streaming
		{
			return FALSE;
		}
		break;
	case SCSIOP_FORMAT_UNIT:
		{
			if((PduDesc->DVD_TYPE == LOGITEC))
			{
				return TRUE;
			}
			else{
				return FALSE;

			}
		}
		break;
	case SCSIOP_MODE_SELECT10:
		{
			if((PduDesc->DVD_TYPE == IO_DATA)
				|| (PduDesc->DVD_TYPE == LOGITEC)
				|| (PduDesc->DVD_TYPE == IO_DATA_9573)
				)
			{
				return FALSE;
			}else return FALSE;
		}
		break;
	case 0x5d:
		{
			if(
				(PduDesc->DVD_TYPE == IO_DATA)
				|| (PduDesc->DVD_TYPE == IO_DATA_9573)
				)
			{
				return FALSE;
			}else return TRUE;
		}
		break;
	//
	//	DMA operations.
	//
	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case 0xaa:						// WRITE(12)
	case SCSIOP_READ:
	case SCSIOP_READ_DATA_BUFF:
	case SCSIOP_READ_CD:
	case SCSIOP_READ_CD_MSF:
	case 0xa8:						// READ(12)
		{
			return TRUE;
		}
		break;
	default:
		{
			return FALSE;
		}
		break;
	}

	return FALSE;
}


NTSTATUS
IdeLspPacketRequest(
		IN PLANSCSI_SESSION	LSS,
		IN PLANSCSI_PDUDESC	PduDesc,
		PBYTE				PduReponse,
		PUCHAR				PduRegister
	)
{

	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_PACKET_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_PACKET_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	NTSTATUS							ntStatus;
	ULONG								result;
	UINT32								len;
	UINT32								buflen;
	BYTE								com;
	BOOLEAN								IsDMA = FALSE;

	//
	// Check Parameters.
	//
	if(PduReponse == NULL) {
		KDPrintM(DBG_PROTO_ERROR, ("pResult is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}
	
	if(PduDesc->PKCMD == NULL){
		KDPrintM(DBG_PROTO_ERROR, ("PKCMD is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	RtlZeroMemory(PduBuffer, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_PACKET_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = HTONS(12);
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = HTONL(++LSS->CommandTag);
	pRequestHeader->TargetID = HTONL(PduDesc->TargetId);
	pRequestHeader->LUN = 0;

	//Using Target ID, LUN is always 0
	if(PduDesc->TargetId == 0){
		pRequestHeader->DEV = 0;
	}else{ 
		pRequestHeader->DEV = 1;
	}
	// real data len 
	len = PduDesc->PKDataBufferLength;

	// Register setting
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0;
	pRequestHeader->Command = WIN_PACKETCMD;
	pRequestHeader->SectorCount_Curr = 0;
	pRequestHeader->LBALow_Curr = 0;
	pRequestHeader->LBAMid_Curr = (_int8)(len & 0xff);
	pRequestHeader->LBAHigh_Curr = (_int8)((len >> 8) & 0xff);
	


	// align 4byte data
	buflen = PduDesc->PKDataBufferLength + 3;
	buflen = (int)(buflen /4) * 4;
	pRequestHeader->COM_LENG = ( HTONL(buflen) >> 8 );
	RtlCopyMemory(pRequestHeader->PKCMD, PduDesc->PKCMD, MAXIMUM_CDB_SIZE);

	// make Pdu header	
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pAHS = (PCHAR)((PCHAR)pRequestHeader + sizeof(LANSCSI_H2R_PDU_HEADER));	

	
	
	com = PduDesc->PKCMD[0];
	
	// Processing Send request and data

	// Make ATAPI Packet request
	//
	//		Processing ATAPI command based on 3 protocol
	//
	//			1. Send Protocol
	//				H -> LU (PC)
	//				H -> LU (DATA)
	//
	//			2. Receive Protocol
	//				H -> LU (PC)
	//				H <- LU (DATA)
	//
	//
	//			3. Execute Protocol
	//
	//				H -> LU(PC)
	//
	switch(com)
	{
	// 1. Send Protocol
	case SCSIOP_SEND_KEY:
		{
			pRequestHeader->COM_TYPE_K = 1;
		}
	case 0xbf :	//	case SCSIOP_SEND_DVD_STRUCTURE:
	case 0xbb : //	case SCSIOP_SET_CD_SPEED:
	case 0xa2 ://	case SCSIOP_SEND_EVENT: //0xA2
	case 0x54 : //	case SCSIOP_SEND_OPC_INFORMATION:
	case SCSIOP_SET_READ_AHEAD:
	case SCSIOP_SEND_VOLUME_TAG: //Set Streaming
	case SCSIOP_FORMAT_UNIT:
	case SCSIOP_MODE_SELECT10:	
	case 0x5d : //	case SCSIOP_SEND_CUE_SHEET:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case 0xaa:
		{
			PBYTE		pData;


			pRequestHeader->COM_TYPE_P = 1;
			
			IsDMA = DVDIsDMA(LSS,PduDesc,com);

			if(TRUE == IsDMA)
			{
				pRequestHeader->COM_TYPE_D_P = 1;
				pRequestHeader->Feature_Curr = 0x1;
			}else{
				pRequestHeader->COM_TYPE_D_P = 0;
				pRequestHeader->Feature_Curr = 0x0;
			}

			if(len > 0){
				pRequestHeader->COM_TYPE_W = 1;
				pRequestHeader->W = 1;
			
			}

			ntStatus = IdeLspSendRequest_V11(LSS, &pdu, NULL, PduDesc->TimeOut);
			if(!NT_SUCCESS(ntStatus)) {
				KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request  0x%02x\n", com));
				return ntStatus;
			}

			if(len > 0)
			{
				pData = LSS->EncryptBuffer;
				if(pData == NULL)
				{
					KDPrintM(DBG_PROTO_ERROR, ("No buffer for send data  0x%02x\n", com));
					return STATUS_UNSUCCESSFUL;
				}

				RtlZeroMemory(pData,buflen);

				if(LSS->DataEncryptAlgo != 0)
				{
					
					if(len == buflen)
					{
						Encrypt32SPAndCopy(
							pData,
							PduDesc->PKDataBuffer,
							buflen,
							&(LSS->EncryptIR[0])
							);
					}else{
						RtlCopyMemory(pData,PduDesc->PKDataBuffer, len);
						Encrypt32SP(
							pData,
							buflen,
							&(LSS->EncryptIR[0])
							);
					}

				}else{
					
					RtlCopyMemory(pData,PduDesc->PKDataBuffer, len);
				}

				ntStatus = SendIt(
							&LSS->ConnectionFile,
							pData,
							buflen,
							&result,
							NULL,
							PduDesc->TimeOut
							);
				if(!NT_SUCCESS(ntStatus) 
					|| result != buflen) {
					KDPrintM(DBG_PROTO_ERROR, ("Error when Send data for WRITE  Com 0x%02x",com));
					return ntStatus;
				}
			}	
		}
		break;
	// 2. Receive Protocol
	case SCSIOP_REPORT_KEY:
	case SCSIOP_MODE_SENSE10:
	case SCSIOP_READ:
	case SCSIOP_READ_DATA_BUFF:
	case SCSIOP_READ_CD:
	case SCSIOP_READ_CD_MSF:
	case 0xa8:
	case SCSIOP_READ_CAPACITY:
	case 0x46:
		{
			pRequestHeader->COM_TYPE_P = 1;
			
			IsDMA = DVDIsDMA(LSS,PduDesc,com);

			if(TRUE == IsDMA)
			{
				pRequestHeader->COM_TYPE_D_P = 1;
				pRequestHeader->Feature_Curr = 0x1;
			}else{
				pRequestHeader->COM_TYPE_D_P = 0;
				pRequestHeader->Feature_Curr = 0x0;
			}

			if(len > 0){
				pRequestHeader->COM_TYPE_R = 1;
				pRequestHeader->R = 1;
			}

			ntStatus = IdeLspSendRequest_V11(LSS,&pdu, NULL, PduDesc->TimeOut);
			if(!NT_SUCCESS(ntStatus)) {
				KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request  0x%02x\n", com));
				return ntStatus;
			}

		}
		break;
		// Mix Receive Protocol and Execute Protocol 
		//		Afterdays more processing may be needed...
	default:
		{
			pRequestHeader->COM_TYPE_P = 1;
			
			IsDMA = DVDIsDMA(LSS,PduDesc,com);

			if(TRUE == IsDMA)
			{
				pRequestHeader->COM_TYPE_D_P = 1;
				pRequestHeader->Feature_Curr = 0x1;
			}else{
				pRequestHeader->COM_TYPE_D_P = 0;
				pRequestHeader->Feature_Curr = 0x0;
			}

			if(len > 0){
				pRequestHeader->COM_TYPE_R = 1;
				pRequestHeader->R = 1;
			}

			ntStatus = IdeLspSendRequest_V11(LSS,&pdu, NULL, PduDesc->TimeOut);
			if(!NT_SUCCESS(ntStatus)) {
				KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request  0x%02x\n", com));
				return ntStatus;
			}			
		}
		break;
	}

	// Processing read data

	switch(com)
	{
	//	1. Send protocol No data receive return.
	case SCSIOP_SEND_KEY:
	case SCSIOP_MODE_SELECT10:
	case 0xbf :	//	case SCSIOP_SEND_DVD_STRUCTURE:
	case 0xbb : //	case SCSIOP_SET_CD_SPEED:
	case 0xa2 : //	case SCSIOP_SEND_EVENT: //0xA2
	case 0x54 : //	case SCSIOP_SEND_OPC_INFORMATION:
	case 0x5d : //	case SCSIOP_SEND_CUE_SHEET:
	case SCSIOP_SET_READ_AHEAD:
	case SCSIOP_SEND_VOLUME_TAG:
	case SCSIOP_FORMAT_UNIT:
	case SCSIOP_WRITE:
	case SCSIOP_WRITE_VERIFY:
	case 0xaa:
		break;
	
	//	2. Receive Protocol Receive data
	case SCSIOP_MODE_SENSE10:
	case SCSIOP_READ:
	case SCSIOP_READ_DATA_BUFF:	
	case SCSIOP_READ_CD:
	case SCSIOP_READ_CD_MSF:
	case 0xa8:
	case SCSIOP_READ_CAPACITY:
	case 0x46:
		{
			PBYTE			pData;
			
			if(len > 0)
			{
				pData = LSS->EncryptBuffer;
				if(pData == NULL)
				{
					KDPrintM(DBG_PROTO_ERROR, ("No buffer for Receive data  0x%02x\n", com));
					return STATUS_UNSUCCESSFUL;
				}

				ntStatus = RecvIt(
					&LSS->ConnectionFile, 
					pData,
					buflen,
					&result,
					NULL,
					PduDesc->TimeOut
					);

				if(!NT_SUCCESS(ntStatus) 
					|| result != buflen) {
					KDPrintM(DBG_PROTO_ERROR, ("Error when Receive Data for READ Com 0x%02x\n",com));
					return ntStatus;
				}
				
				if(LSS->DataEncryptAlgo != 0)
				{

					Decrypt32SP(
						pData,
						buflen,
						&(LSS->DecryptIR[0])
						);
				}

				RtlCopyMemory(PduDesc->PKDataBuffer, pData, len);

			}
		}
		break;
	default:
		{
			PBYTE			pData;
			
			if(len > 0)
			{
				pData = LSS->EncryptBuffer;
				if(pData == NULL)
				{
					KDPrintM(DBG_PROTO_ERROR, ("No buffer for Receive data  0x%02x\n", com));
					return STATUS_UNSUCCESSFUL;
				}

				ntStatus = RecvIt(
					&LSS->ConnectionFile, 
					pData,
					buflen,
					&result,
					NULL,
					PduDesc->TimeOut
					);

				if(!NT_SUCCESS(ntStatus) 
					|| result != buflen) {
					KDPrintM(DBG_PROTO_ERROR, ("Error when Receive Data for READ Com 0x%02x\n",com));
					return ntStatus;
				}
				
				if(LSS->DataEncryptAlgo != 0)
				{

					Decrypt32SP(
						pData,
						buflen,
						&(LSS->DecryptIR[0])
						);
				}
				
				RtlCopyMemory(PduDesc->PKDataBuffer, pData, len);
			


				

			}
		}
		break;
	}


	// Read Reply.
	ntStatus = IdeLspReadReply_V11(LSS, (PCHAR)PduBuffer, &pdu, NULL, PduDesc->TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply. Com 0x%02x\n", com));
		return ntStatus;
	}

	// Check reply header
	pReplyHeader = (PLANSCSI_PACKET_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if( (pReplyHeader->Opcode != IDE_RESPONSE)
		|| (pReplyHeader->F == 0)) 
	{
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header pReplyHeader->Opcode != IDE_RESPONSE . Flag: 0x%x,  Command: 0x%x\n", 
				pReplyHeader->Flags,  com));
		return STATUS_UNSUCCESSFUL;
	}

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS)
	{
		KDPrintM(DBG_PROTO_TRACE, ("BAD Reply Header pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS . response: 0x%x,  Command: 0x%x\n", 
				pReplyHeader->Response,  com));
		PduRegister[0] = pReplyHeader->Feature_Curr; //	error
		PduRegister[1] = pReplyHeader->Command; //	status
	}else{
		PduRegister[0] = 0;	// error
		PduRegister[1] = 0; // status
	}

	*PduReponse = pReplyHeader->Response;

	KDPrintM(DBG_PROTO_TRACE, ("Com 0x%02x RegError 0x%02x, RegStatus 0x%02x\n", 
				com, PduRegister[0], PduRegister[1]));
	
	return STATUS_SUCCESS;
}


//
//
//	Output values: 
//			PduResponse - valid NDAS device's return value
//						when communication is successful.
//
//	return values: STATUS_SUCCESS when communication is successful.
//

NTSTATUS
IdeLspVendorRequest(
	PLANSCSI_SESSION	LSS,
	PLANSCSI_PDUDESC	PduDesc,
	PBYTE				PduResponse
){
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	NTSTATUS							ntStatus;
	unsigned _int8						iCommandReg;
	KEVENT								requestCompEvent;

	//
	// Check Parameters.
	//
	if(PduResponse == NULL) {
		KDPrintM(DBG_PROTO_ERROR, ("pResult is NULL!!!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = VENDOR_SPECIFIC_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(LSS->HPID);
	pRequestHeader->RPID = HTONS(LSS->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	LSS->CommandTag++;
	pRequestHeader->PathCommandTag = HTONL(LSS->CommandTag);
	pRequestHeader->VendorID = HTONS(NKC_VENDOR_ID);
	pRequestHeader->VendorOpVersion = VENDOR_OP_CURRENT_VERSION;
	pRequestHeader->VendorOp = PduDesc->Command;
	pRequestHeader->VendorParameter0 = PduDesc->Param32[0];
	pRequestHeader->VendorParameter1 = PduDesc->Param32[1];
	pRequestHeader->VendorParameter2 = PduDesc->Param32[2];

	// Backup Command.
	iCommandReg = pRequestHeader->VendorOp;

	//
	// Send Request.
	//
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	//
	// Send Request.
	// We do not care about asynchronous result of the header.
	// Just go ahead to send data. Error will be detected at the last moment.
	//
	KeInitializeEvent(&requestCompEvent, NotificationEvent, FALSE);
	LSS->RequestOverlappedData.IoCompleteRoutine = LspRequestCompletionRoutine;
	LSS->RequestOverlappedData.CompletionEvent = &requestCompEvent;
	LSS->RequestOverlappedData.UserContext = LSS;
	ntStatus = LspSendRequest(LSS, &pdu, &LSS->RequestOverlappedData, NULL);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&requestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Error when Send Request\n"));
		return STATUS_PORT_DISCONNECTED;
	}

	// Read Reply.
	ntStatus = LspReadReply(LSS, (PCHAR)PduBuffer, &pdu, NULL, PduDesc->TimeOut);
	if(!NT_SUCCESS(ntStatus)) {
		LsprotoWaitForCompEvent(&requestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("Can't Read Reply.\n"));
		return STATUS_PORT_DISCONNECTED;
	}

	// Check Reply Header.
	pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if(pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE){		
		LsprotoWaitForCompEvent(&requestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE . Flag: 0x%x, Req. VendorOp: 0x%x Rep. VendorOp: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->VendorOp));
		return STATUS_UNSUCCESSFUL;
	}
	if(pReplyHeader->F == 0){		
		LsprotoWaitForCompEvent(&requestCompEvent, NULL);
		KDPrintM(DBG_PROTO_ERROR, ("BAD Reply Header pReplyHeader->F == 0 . Flag: 0x%x, Req. VendorOp: 0x%x Rep. VendorOp: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->VendorOp));
		return STATUS_UNSUCCESSFUL;
	}

	//
	//	Set return values
	//

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		KDPrintM(DBG_PROTO_TRACE, ("Failed. Response 0x%x Req. VendorOp: 0x%x Rep. VendorOp: 0x%x\n", 
			pReplyHeader->Response, iCommandReg, pReplyHeader->VendorOp
			));
	}

	PduDesc->Param32[0] = pRequestHeader->VendorParameter0;
	PduDesc->Param32[1] = pRequestHeader->VendorParameter1;
	PduDesc->Param32[2] = pRequestHeader->VendorParameter2;

	*PduResponse = pReplyHeader->Response;

	LsprotoWaitForCompEvent(&requestCompEvent, NULL);

	return STATUS_SUCCESS;
}

