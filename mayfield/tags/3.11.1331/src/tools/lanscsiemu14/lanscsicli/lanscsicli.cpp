// LanScsiCli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define	_LPX_

#define	NR_MAX_TARGET			2
#define	MAX_DATA_BUFFER_SIZE	64 * 1024
#define BLOCK_SIZE				512
#define	MAX_TRANSFER_SIZE		(64 * 1024)
#define MAX_TRANSFER_BLOCKS		 MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC			(LONGLONG)(1000)
#define TIME_OUT	(SEC * 30)					// 5 min.

#define HTONLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
						| (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
						| (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
						| (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

typedef	struct _TARGET_DATA {
	BOOL	bPresent;
	_int8	NRRWHost;
	_int8	NRROHost;
	_int64	TargetData;
	
	// IDE Info.
	BOOL			bLBA;
	BOOL			bLBA48;
	unsigned _int64	SectorCount;
} TARGET_DATA, *PTARGET_DATA;

// Global Variable.
_int32			HPID;
_int16			RPID;
_int32			iTag;
int				NRTarget;
unsigned		CHAP_C;
unsigned		requestBlocks;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
unsigned _int16	HeaderEncryptAlgo;
unsigned _int16	DataEncryptAlgo;
int				iSessionPhase;
unsigned _int64	iPassword;

void
PrintError(
		   int		ErrorCode,
		   PCHAR	prefix
		   )
{
	LPVOID lpMsgBuf;
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	fprintf(stderr, "%s: %s", prefix, (LPCSTR)lpMsgBuf);
	
	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

void
PrintErrorCode(
			   PCHAR	prefix,
			   int		ErrorCode
			   )
{
	PrintError(ErrorCode, prefix);
}

inline int 
RecvIt(
	   SOCKET	sock,
	   PCHAR	buf, 
	   int		size
	   )
{
	int				iErrcode;
	int				len = size, iReceived;
	WSAOVERLAPPED	overlapped;
	WSABUF			buffer[1];
	DWORD			dwFlag;
	DWORD			dwRecvDataLen;
	WSAEVENT		hEvent;
	BOOL			bResult;

	// Overlapped event
	//
	hEvent = WSACreateEvent();
	
	//
	// Receive Reply Header.
	//
	memset(&overlapped, 0, sizeof(WSAOVERLAPPED));
	overlapped.hEvent = hEvent;
	
	iReceived = 0;

	while(iReceived < size) {
		if(size - iReceived >= 1024)
			buffer[0].len = 1024;
		else
			buffer[0].len = size - iReceived;

		buffer[0].buf = buf + iReceived;
		
		// Flag
		dwFlag = 0;
		
		iErrcode = WSARecv(
			sock,
			buffer,
			1, 
			&dwRecvDataLen,
			&dwFlag,
			&overlapped,
			NULL
			);
		
		if(iErrcode == SOCKET_ERROR) {
			DWORD dwError = WSAGetLastError();
			
			if(dwError == WSA_IO_PENDING) {
				DWORD	dwFlags;

				
				dwError = WSAWaitForMultipleEvents(
					1,
					&hEvent,
					TRUE,
					TIME_OUT,
					TRUE
					);				
				if(dwError != WSA_WAIT_EVENT_0) {
					
					PrintErrorCode(TEXT("[LanScsiCli]RecvIt: "), dwError);
					dwRecvDataLen = -1;
					
					printf("[LanScsiCli]RecvIt: Request %d, Received %d\n",
						size,
						iReceived
						);
					goto Out;
				}
				
				// Get Result...
				bResult = WSAGetOverlappedResult(
					sock,
					&overlapped,
					&dwRecvDataLen,
					TRUE,
					&dwFlags
					);
				if(bResult == FALSE) {
					PrintErrorCode(TEXT("[LanScsiCli]RecvIt: GetOverlappedResult Failed "), GetLastError());
					dwRecvDataLen = SOCKET_ERROR;
					goto Out;
				}
				
			} else {
				PrintErrorCode(TEXT("[LanScsiCli]RecvIt: WSARecv Failed "), dwError);
				
				dwRecvDataLen = -1;
				goto Out;
			}
		}

		iReceived += dwRecvDataLen;
		
		WSAResetEvent(hEvent);
	}

Out:
	WSACloseEvent(hEvent);
  
	return dwRecvDataLen;
}


inline int 
SendIt(
	   SOCKET	sock,
	   PCHAR	buf, 
	   int		size
	   )
{
	int res;
	int len = size;
	
	while (len > 0) {
		
		if ((res = send(sock, buf, len, 0)) <= 0) {
			PrintError(WSAGetLastError(), "SendIt: ");
			return res;
		}
		len -= res;
		buf += res;
	}
	
	return size;
}

int
ReadReply(
			SOCKET			connSock,
			PCHAR			pBuffer,
			PLANSCSI_PDU	pPdu
			)
{
	int		iResult, iTotalRecved = 0;
	PCHAR	pPtr = pBuffer;

	// Read Header.
	iResult = RecvIt(
		connSock,
		pPtr,
		sizeof(LANSCSI_H2R_PDU_HEADER)
		);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "ReadRequest: Can't Recv Header...\n");

		return iResult;
	} else if(iResult == 0) {
		fprintf(stderr, "ReadRequest: Disconnected...\n");
		
		return iResult;
	} else
		iTotalRecved += iResult;

	pPdu->pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

	pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		Decrypt32(
			(unsigned char*)pPdu->pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&CHAP_C,
			(unsigned char *)&iPassword
			);
	}

	// Read AHS.
	if(ntohs(pPdu->pH2RHeader->AHSLen) > 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			ntohs(pPdu->pH2RHeader->AHSLen)
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Recv AHS...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReadRequest: Disconnected...\n");

			return iResult;
		} else
			iTotalRecved += iResult;
	
		pPdu->pAHS = pPtr;

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
	}

	// Read Header Dig.
	pPdu->pHeaderDig = NULL;

	// Read Data segment.
	if(ntohl(pPdu->pH2RHeader->DataSegLen) > 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			ntohl(pPdu->pH2RHeader->DataSegLen)
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Recv Data segment...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReadRequest: Disconnected...\n");

			return iResult;
		} else 
			iTotalRecved += iResult;
		
		pPdu->pDataSeg = pPtr;
		
		pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);

		
		if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
			&& DataEncryptAlgo != 0) {
			
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohl(pPdu->pH2RHeader->DataSegLen),
				(unsigned char *)&CHAP_C,
				(unsigned char *)&iPassword
				);
		}

	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
}

int
SendRequest(
			SOCKET			connSock,
			PLANSCSI_PDU	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;
	iDataSegLen = ntohl(pHeader->DataSegLen);

	//
	// Encrypt Header.
	//
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		
		Encrypt32(
			(unsigned char*)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&CHAP_C,
			(unsigned char*)&iPassword
			);
	}
	
	//
	// Encrypt Data.
	//
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& DataEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt32(
			(unsigned char*)pPdu->pDataSeg,
			iDataSegLen,
			(unsigned char *)&CHAP_C,
			(unsigned char*)&iPassword
			);
	}

	// Send Request.
	iResult = SendIt(
		connSock,
		(PCHAR)pHeader,
		sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
		);
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "SendRequest: Send Request ");
		return -1;
	}
	
	return 0;
}

int
Login(
	  SOCKET			connsock,
	  UCHAR				cLoginType,
	  _int32			iUserID,
	  unsigned _int64	iKey
	  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestPdu;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
	PBIN_PARAM_SECURITY					pParamSecu;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	PAUTH_PARAMETER_CHAP				pParamChap;
	LANSCSI_PDU							pdu;
	int									iSubSequence;
	int									iResult;
	unsigned							CHAP_I;
	
	//
	// Init.
	//
	iSubSequence = 0;
	iSessionPhase = FLAG_SECURITY_PHASE;

	// 
	// First Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opocde = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
//	pLoginRequestPdu->VerMax = 0;
	pLoginRequestPdu->VerMax = 1;
	pLoginRequestPdu->VerMin = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "Login: Send First Request ");
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]login: First Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opocde != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD First Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]login: First Failed.\n");
		return -1;
	}
	
	// Store Data.
	RPID = ntohs(pLoginReplyHeader->RPID);
	
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	printf("[LanScsiCli]login: Version %d Auth %d\n", 
		pLoginReplyHeader->VerActive, 
		ntohs(pParamSecu->AuthMethod)
		);
	
	// 
	// Second Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opocde = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiCli]Login: Send Second Request ");
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opocde != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Second Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]login: Second Failed.\n");
		return -1;
	}
	
	// Check Data segment.
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
		|| (pdu.pDataSeg == NULL)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Second Reply Data.\n");
		return -1;
	}	
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != cLoginType
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Second Reply Parameters.\n");
		return -1;
	}
	
	// Store Challenge.	
	pParamChap = &pParamSecu->ChapParam;
	CHAP_I = ntohl(pParamChap->CHAP_I);
	CHAP_C = ntohl(pParamChap->CHAP_C[0]);
	
	printf("[LanScsiCli]login: Hash %d, Challenge %d\n", 
		ntohl(pParamChap->CHAP_A), 
		CHAP_C
		);
	
	// 
	// Third Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opocde = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_SECURITY_PHASE;
	pLoginRequestPdu->NSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	pParamChap->CHAP_A = htonl(HASH_ALGORITHM_MD5);
	pParamChap->CHAP_I = htonl(CHAP_I);
	pParamChap->CHAP_N = htonl(iUserID);
	
	Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->CHAP_R, (PUCHAR)&iKey);
	
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "Login: Send Third Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opocde != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Third Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]login: Third Failed.\n");
		return -1;
	}
	
	// Check Data segment.
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
		|| (pdu.pDataSeg == NULL)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Third Reply Data.\n");
		return -1;
	}	
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != cLoginType
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Third Reply Parameters.\n");
		return -1;
	}
	
	iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;

	// 
	// Fourth Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opocde = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->NSG = FLAG_FULL_FEATURE_PHASE;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	
	pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamNego;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "Login: Send Fourth Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]login: Fourth Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opocde != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Fourth Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]login: Fourth Failed.\n");
		return -1;
	}
	
	// Check Data segment.
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
		|| (pdu.pDataSeg == NULL)) {
		
		fprintf(stderr, "[LanScsiCli]login: BAD Fourth Reply Data.\n");
		return -1;
	}	
	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
		fprintf(stderr, "[LanScsiCli]login: BAD Fourth Reply Parameters.\n");
		return -1;
	}
	
	printf("[LanScsiCli]login: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n", 
		pParamNego->HWType, pParamNego->HWVersion,
		ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
		ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID)
		);
	printf("[LanScsiCli]login: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
		ntohs(pParamNego->HeaderEncryptAlgo),
		ntohs(pParamNego->HeaderDigestAlgo),
		ntohs(pParamNego->DataEncryptAlgo),
		ntohs(pParamNego->DataDigestAlgo)
		);

	requestBlocks = ntohl(pParamNego->MaxBlocks);
	HeaderEncryptAlgo = ntohs(pParamNego->HeaderEncryptAlgo);
	DataEncryptAlgo = ntohs(pParamNego->DataEncryptAlgo);

	iSessionPhase = FLAG_FULL_FEATURE_PHASE;

	return 0;
}

int
TextTargetList(
			   SOCKET	connsock
			   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_LIST				pParam;
	LANSCSI_PDU							pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opocde = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg = (char *)pParam;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "TextTargetList: Send First Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]TextTargetList: Can't Read Reply.\n");
		return -1;
	}
	
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	// Check Request Header.
	if((pReplyHeader->Opocde != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]TextTargetList: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]TextTargetList: Failed.\n");
		return -1;
	}
	
	if(ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
		fprintf(stderr, "[LanScsiCli]TextTargetList: No Data Segment.\n");
		return -1;		
	}

	pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
		fprintf(stderr, "TEXT: Bad Parameter Type.\n");
		return -1;			
	}
	printf("[LanScsiCli]TextTargetList: NR Targets : %d\n", pParam->NRTarget);
	NRTarget = pParam->NRTarget;
	
	for(int i = 0; i < pParam->NRTarget; i++) {
		PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
		int								iTargetId;
		
		pTarget = &pParam->PerTarget[i];
		iTargetId = ntohl(pTarget->TargetID);
		
		printf("[LanScsiCli]TextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: %I64d \n",  
			ntohl(pTarget->TargetID), 
			pTarget->NRRWHost,
			pTarget->NRROHost,
			pTarget->TargetData
			);
		
		PerTarget[iTargetId].bPresent = TRUE;
		PerTarget[iTargetId].NRRWHost = pTarget->NRRWHost;
		PerTarget[iTargetId].NRROHost = pTarget->NRROHost;
		PerTarget[iTargetId].TargetData = pTarget->TargetData;
	}
	
	return 0;
}

int
TextTargetData(
			   SOCKET	connsock,
			   UCHAR	cGetorSet,
			   UINT		TargetID
			   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_DATA				pParam;
	LANSCSI_PDU							pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opocde = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_DATA;
	pParam->GetOrSet = cGetorSet;
	pParam->TargetData = PerTarget[TargetID].TargetData;
	pParam->TargetID = htonl(TargetID);
	
	printf("TargetID %d, %I64d\n", ntohl(pParam->TargetID), pParam->TargetData);

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg = (char *)pParam;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "TextTargetData: Send First Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]TextTargetData: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opocde != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[LanScsiCli]TextTargetData: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]TextTargetData: Failed.\n");
		return -1;
	}
	
	if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		fprintf(stderr, "[LanScsiCli]TextTargetData: No Data Segment.\n");
		return -1;		
	}
	pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;

	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
		fprintf(stderr, "TextTargetData: Bad Parameter Type. %d\n", pParam->ParamType);
	//	return -1;			
	}

	PerTarget[TargetID].TargetData = pParam->TargetData;

	printf("[LanScsiCli]TextTargetList: TargetID : %d, GetorSet %d, Target Data %d\n", 
		ntohl(pParam->TargetID), pParam->GetOrSet, PerTarget[TargetID].TargetData);
	
	return 0;
}

int
VenderCommand(
			  SOCKET			connsock,
			  UCHAR				cOperation,
			  unsigned _int64	*pParameter
			  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDER_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDER_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU							pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_VENDER_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opocde = VENDER_SPECIFIC_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->VenderID = ntohs(NKC_VENDER_ID);
	pRequestHeader->VenderOpVersion = VENDER_OP_CURRENT_VERSION;
	pRequestHeader->VenderOp = cOperation;
	pRequestHeader->VenderParameter = *pParameter;
	
	printf("VenderCommand: Operation %d, Parameter %I64d\n", cOperation, NTOHLL(*pParameter));

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "VenderCommand: Send First Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]VenderCommand: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_VENDER_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opocde != VENDER_SPECIFIC_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		fprintf(stderr, "[LanScsiCli]VenderCommand: BAD Reply Header. %d 0x%x\n", pReplyHeader->Opocde, pReplyHeader->F);
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]VenderCommand: Failed.\n");
		return -1;
	}
	
	*pParameter = pReplyHeader->VenderParameter;

	return 0;
}

int
Logout(
	   SOCKET	connsock
	   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU							pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opocde = LOGOUT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(connsock, &pdu) != 0) {

		PrintError(WSAGetLastError(), "[LanScsiCli]Logout: Send Request ");
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]Logout: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if((pReplyHeader->Opocde != LOGOUT_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		fprintf(stderr, "[LanScsiCli]Logout: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]Logout: Failed.\n");
		return -1;
	}
	
	iSessionPhase = FLAG_SECURITY_PHASE;

	return 0;
}

int
IdeCommand(
		   SOCKET	connsock,
		   _int32	TargetId,
		   _int64	LUN,
		   UCHAR	Command,
		   _int64	Location,
		   _int16	SectorCount,
		   _int8	Feature,
		   PCHAR	pData
		   )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU						pdu;
	int								iResult;
	unsigned _int8					iCommandReg;
	
	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opocde = IDE_COMMAND;
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
	pRequestHeader->Feature = 0;			

	switch(Command) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			
			if(PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			if(PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			if(PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			pRequestHeader->Command = WIN_IDENTIFY;
		}
		break;
	case WIN_SETFEATURES:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature = Feature;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)SectorCount;
			pRequestHeader->Command = WIN_SETFEATURES;

			fprintf(stderr, "[LanScsiCli]IDECommand: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr);
		}
		break;
	default:
		fprintf(stderr, "[LanScsiCli]IDECommand: Not Supported IDE Command.\n");
		return -1;
	}
		
	if((Command == WIN_READ)
		|| (Command == WIN_WRITE)
		|| (Command == WIN_VERIFY)){
		
		if(PerTarget[TargetId].bLBA == FALSE) {
			fprintf(stderr, "[LanScsiCli]IDECommand: CHS not supported...\n");
			return -1;
		}
		
		pRequestHeader->LBA = 1;
		
		if(PerTarget[TargetId].bLBA48 == TRUE) {
			
			pRequestHeader->LBALow_Curr = (_int8)(Location);
			pRequestHeader->LBAMid_Curr = (_int8)(Location >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(Location >> 16);
			pRequestHeader->LBALow_Prev = (_int8)(Location >> 24);
			pRequestHeader->LBAMid_Prev = (_int8)(Location >> 32);
			pRequestHeader->LBAHigh_Prev = (_int8)(Location >> 40);
			
			pRequestHeader->SectorCount_Curr = (_int8)SectorCount;
			pRequestHeader->SectorCount_Prev = (_int8)(SectorCount >> 8);
			
		} else {
			
			pRequestHeader->LBALow_Curr = (_int8)(Location);
			pRequestHeader->LBAMid_Curr = (_int8)(Location >> 8);
			pRequestHeader->LBAHigh_Curr = (_int8)(Location >> 16);
			pRequestHeader->LBAHeadNR = (_int8)(Location >> 24);
			
			pRequestHeader->SectorCount_Curr = (_int8)SectorCount;
		}
	}

	// Backup Command.
	iCommandReg = pRequestHeader->Command;

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), "IdeCommand: Send Request ");
		return -1;
	}
	
	// If Write, Send Data.
	if(Command == WIN_WRITE) {
		unsigned _int64	iKey;
		unsigned char	key[8];

		//
		// Encrypt Data.
		//
		if(DataEncryptAlgo != 0) {
			iKey = HASH_KEY_USER;
			memcpy(key, &iKey, 8);
			
			Encrypt32(
				(unsigned char*)pData,
				SectorCount * 512,
				(unsigned char *)&CHAP_C,
				key
				);
		}

		iResult = SendIt(
			connsock,
			pData,
			SectorCount * 512
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "IdeCommand: Send data for WRITE ");
			return -1;
		}
	}
	
	// If Read, Identify Op... Read Data.
	switch(Command) {
	case WIN_READ:
		{
			iResult = RecvIt(
				connsock, 
				pData, 
				SectorCount * 512
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), "IdeCommand: Receive Data for READ ");
				return -1;
			}

			//
			// Decrypt Data.
			//
			if(DataEncryptAlgo != 0) {
				
				Decrypt32(
					(unsigned char*)pData,
					SectorCount * 512,
					(unsigned char*)&CHAP_C,
					(unsigned char*)&iPassword
					);
			}
		}
		break;
	case WIN_IDENTIFY:
		{

			iResult = RecvIt(
				connsock, 
				pData, 
				512
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), "IdeCommand: Receive Data for IDENTIFY ");
				return -1;
			}

			//
			// Decrypt Data.
			//
			if(DataEncryptAlgo != 0) {

				Decrypt32(
					(unsigned char*)pData,
					512,
					(unsigned char*)&CHAP_C,
					(unsigned char*)&iPassword
					);
			}
		}
		break;
	default:
		break;
	}
	
	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[LanScsiCli]IDECommand: Can't Read Reply.\n");
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		fprintf(stderr, "[LanScsiCli]IDECommand: Time out...\n");
		return WAIT_TIMEOUT;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if((pReplyHeader->Opocde != IDE_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->Command != iCommandReg)) {
		
		fprintf(stderr, "[LanScsiCli]IDECommand: BAD Reply Header. Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[LanScsiCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			);
		return -1;
	}
	
	return 0;
}

void
ConvertString(
			  PCHAR	result,
			  PCHAR	source,
			  int	size
			  )
{
	for(int i = 0; i < size / 2; i++) {
		result[i * 2] = source[i * 2 + 1];
		result[i * 2 + 1] = source[i * 2];
	}
	result[size] = '\0';
	
}

int
Lba_capacity_is_ok(
				   struct hd_driveid *id
				   )
{
	unsigned _int32	lba_sects, chs_sects, head, tail;

	if((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {
		// 48 Bit Drive.
		return 1;
	}

	/*
		The ATA spec tells large drivers to return
		C/H/S = 16383/16/63 independent of their size.
		Some drives can be jumpered to use 15 heads instead of 16.
		Some drives can be jumpered to use 4092 cyls instead of 16383
	*/
	if((id->cyls == 16383 || (id->cyls == 4092 && id->cur_cyls== 16383)) 
		&& id->sectors == 63 
		&& (id->heads == 15 || id->heads == 16)
		&& id->lba_capacity >= (unsigned)(16383 * 63 * id->heads))
		return 1;
	
	lba_sects = id->lba_capacity;
	chs_sects = id->cyls * id->heads * id->sectors;

	/* Perform a rough sanity check on lba_sects: within 10% is OK */
	if((lba_sects - chs_sects) < chs_sects / 10) {
		return 1;
	}

	/* Some drives have the word order reversed */
	head = ((lba_sects >> 16) & 0xffff);
	tail = (lba_sects & 0xffff);
	lba_sects = (head | (tail << 16));
	if((lba_sects - chs_sects) < chs_sects / 10) {
		id->lba_capacity = lba_sects;
		fprintf(stderr, "!!!! Capacity reversed.... !!!!!!!!\n");
		return 1;
	}

	return 0;
}

int
GetDiskInfo(
			SOCKET	connsock,
			UINT	TargetId
			)
{
	struct hd_driveid	info;
	int					iResult;
	char				buffer[41];
	
	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info)) != 0) {
		fprintf(stderr, "[LanScsiCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}
	
	printf("[LanScsiCli]GetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
		TargetId, info.major_rev_num, info.minor_rev_num, info.capability);
	
	printf("[LanScsiCli]GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
		info.dma_mword, 
		info.dma_ultra);
	
	//
	// DMA Mode.
	//
	if(!(info.dma_mword & 0x0004)) {
		fprintf(stderr, "Not Support DMA mode 2...\n");
		return -1;
	}

	if(!(info.dma_mword & 0x0400)) {
		// Set to DMA mode 2
		if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0x22, 0x03, NULL)) != 0) {
			fprintf(stderr, "[LanScsiCli]GetDiskInfo: Set Feature Failed...\n");
			return iResult;
		}

		// identify.
		if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info)) != 0) {
			fprintf(stderr, "[LanScsiCli]GetDiskInfo: Identify Failed...\n");
			return iResult;
		}

		printf("[LanScsiCli]GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
			info.dma_mword, 
			info.dma_ultra);
	}

	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	printf("Serial No: %s\n", buffer);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	printf("Firmware rev: %s\n", buffer);
	
	memset(buffer, 0, 41);
	strncpy(buffer, (PCHAR)info.model, 40);
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	printf("Model No: %s\n", buffer);
	
	//
	// Support LBA?
	//
	if(info.capability & 0x02)
		PerTarget[TargetId].bLBA = TRUE;
	else
		PerTarget[TargetId].bLBA = FALSE;
	
	//
	// Calc Capacity.
	// 
	if(info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400) {	// Support LBA48bit
		PerTarget[TargetId].bLBA48 = TRUE;
		PerTarget[TargetId].SectorCount = info.lba_capacity_2;
	} else {
		PerTarget[TargetId].bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			PerTarget[TargetId].SectorCount = info.lba_capacity;
		}
		
		PerTarget[TargetId].SectorCount = info.lba_capacity;	
	}

	printf("CAP 2 %I64d, CAP %d\n",
		info.lba_capacity_2,
		info.lba_capacity
		);

	printf("LBA %d, LBA48 %d, Number of Sectors: %I64d\n", 
		PerTarget[TargetId].bLBA, 
		PerTarget[TargetId].bLBA48, 
		PerTarget[TargetId].SectorCount);
	
	return 0;
}

//
// Discovery
//
int
Discovery(
		  SOCKET	connsock
		  )
{
	int	iResult;
	
	//////////////////////////////////////////////////////////
	//
	// Login Phase...
	//
	if((iResult = Login(connsock, LOGIN_TYPE_DISCOVERY, 0, HASH_KEY_USER)) != 0) {
		fprintf(stderr, "[LanScsiCli]Discovery: Login Failed...\n");
		return iResult;
	}
	
	if((iResult = TextTargetList(connsock)) != 0) {
		fprintf(stderr, "[LanScsiCli]Discovery: Text Failed\n");
		return iResult;
	}

	///////////////////////////////////////////////////////////////
	//
	// Logout Packet.
	//
	if((iResult = Logout(connsock)) != 0) {
		fprintf(stderr, "[LanScsiCli]Discovery: Logout Failed...\n");
		return iResult;
	}
	
	return 0;
}

int 
GetInterfaceList(
				 LPSOCKET_ADDRESS_LIST	socketAddressList,
				 DWORD					socketAddressListLength
				 )
{
	int					iErrcode;
	SOCKET				sock;
	DWORD				outputBytes;
	
	
	sock = socket(AF_LPX, SOCK_STREAM, IPPROTO_LPXTCP);
	
	if(sock == INVALID_SOCKET) {
		PrintError(WSAGetLastError(), "GetInterfaceList: socket ");
		return SOCKET_ERROR;
	}
	
	outputBytes = 0;
	
	iErrcode = WSAIoctl(
		sock,							// SOCKET s,
		SIO_ADDRESS_LIST_QUERY, 		// DWORD dwIoControlCode,
		NULL,							// LPVOID lpvInBuffer,
		0,								// DWORD cbInBuffer,
		socketAddressList,				// LPVOID lpvOutBuffer,
		socketAddressListLength,		// DWORD cbOutBuffer,
		&outputBytes,					// LPDWORD lpcbBytesReturned,
		NULL,							// LPWSAOVERLAPPED lpOverlapped,
		NULL							// LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
		);
	
	if(iErrcode == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "GetInterfaceList: WSAIoctl ");
	}
	
	closesocket(sock);
	
	return iErrcode;
}

BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   OUT	SOCKET				*pSocketData
			   )
{
	int						iErrcode;
	SOCKADDR_LPX			socketLpx;
	SOCKADDR_LPX			serverSocketLpx;
	LPSOCKET_ADDRESS_LIST	socketAddressList;
	DWORD					socketAddressListLength;
	int						i;
	SOCKET					sock;
	
	socketAddressListLength = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)
		+ sizeof(SOCKET_ADDRESS) * MAX_SOCKETLPX_INTERFACE
		+ sizeof(SOCKADDR_LPX) * MAX_SOCKETLPX_INTERFACE;
	
	socketAddressList = (LPSOCKET_ADDRESS_LIST)malloc(socketAddressListLength);
	
	//
	// Get NICs
	//
	iErrcode = GetInterfaceList(
		socketAddressList,
		socketAddressListLength
		);
	
	if(iErrcode != 0) {
		fprintf(stderr, "[LanScsiCli]MakeConnection: Error When Get NIC List!!!!!!!!!!\n");
		
		return FALSE;
	} else {
		fprintf(stderr, "[LanScsiCli]MakeConnection: Number of NICs : %d\n", socketAddressList->iAddressCount);
	}
	
	//
	// Find NIC that is connected to LanDisk.
	//
	for(i = 0; i < socketAddressList->iAddressCount; i++) {
		
		socketLpx = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);
		
		printf("[LanScsiCli]MakeConnection: NIC %02d: Address %02X:%02X:%02X:%02X:%02X;%02X\n",
			i,
			socketLpx.LpxAddress.Node[0],
			socketLpx.LpxAddress.Node[1],
			socketLpx.LpxAddress.Node[2],
			socketLpx.LpxAddress.Node[3],
			socketLpx.LpxAddress.Node[4],
			socketLpx.LpxAddress.Node[5]
			);
		
		sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		if(sock == INVALID_SOCKET) {
			PrintError(WSAGetLastError(), "MakeConnection: socket ");
			return FALSE;
		}
		
		socketLpx.LpxAddress.Port = 0; // unspecified
		
		// Bind NIC.
		iErrcode = bind(sock, (struct sockaddr *)&socketLpx, sizeof(socketLpx));
		if(iErrcode == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "MakeConnection: bind ");
			closesocket(sock);
			sock = INVALID_SOCKET;
			
			continue;
		}
		
		// LanDisk Address.
		memset(&serverSocketLpx, 0, sizeof(serverSocketLpx));
		serverSocketLpx.sin_family = AF_LPX;
		memcpy(serverSocketLpx.LpxAddress.Node, pAddress->Node, 6);
		serverSocketLpx.LpxAddress.Port = htons(LPX_PORT_NUMBER);
		
		iErrcode = connect(sock, (struct sockaddr *)&serverSocketLpx, sizeof(serverSocketLpx));
		if(iErrcode == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "MakeConnection: connect ");
			closesocket(sock);
			sock = INVALID_SOCKET;
			
			fprintf(stderr, "[LanScsiCli]MakeConnection: LanDisk is not connected with NIC Number %d\n", i);
			
			continue;
		} else {
			*pSocketData = sock;
			
			break;
		}
	}
	
	if(sock == INVALID_SOCKET) {
		fprintf(stderr, "[LanScsiCli]MakeConnection: No LanDisk!!!\n");
		
		return FALSE;
	}
	
	return TRUE;
}

BOOL
lpx_addr(
		 PCHAR			pStr,
		 PLPX_ADDRESS	pAddr
		 )
{
	PCHAR	pStart, pEnd;

	if(pStr == NULL)
		return FALSE;

	pStart = pStr;

	for(int i = 0; i < 6; i++) {
		
		pAddr->Node[i] = (UCHAR)strtoul(pStart, &pEnd, 16);
		
		pStart += 3;
	}

	return TRUE;
}

int main(int argc, char* argv[])
{
	
	SOCKET				connsock;
	WORD				wVersionRequested;
	WSADATA				wsaData;
	int					err;
	
	UCHAR				data[MAX_DATA_BUFFER_SIZE], data2[MAX_DATA_BUFFER_SIZE];
	int					iResult;
	unsigned _int64		i;
	int					iTargetID;
	unsigned			UserID;
	int					start_time, finish_time;
	unsigned _int8		cCommand;

#ifdef _LPX_
	LPX_ADDRESS			address;
#else
	struct sockaddr_in	servaddr;
#endif
	
	if(argc >= 5) {
		fprintf(stderr, "[LanScsiCli]Usage: %s SERVER_ADDRESS TargetID\n", argv[0]);
		return -1;
	}
	
	wVersionRequested = MAKEWORD( 2, 2 );
	
	err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0) {
		PrintError(WSAGetLastError(), "main: WSAStartup ");
		return -1;
	}

#ifdef _LPX_
	// LanDisk Address.
	if(argc == 1) {
		address.Node[0] = 0x00;
		address.Node[1] = 0xF0;
		address.Node[2] = 0x0F;
		address.Node[3] = 0x00;
		address.Node[4] = 0xFF;
		address.Node[5] = 0xD0;
	} else {
		lpx_addr(argv[1], &address);
	}	

	//
	// Make Connection.
	//
	if(MakeConnection(&address, &connsock) == FALSE) {
		fprintf(stderr, "[LanScsiCli]main: Can't Make Connection to LanDisk!!!\n");
	
		return -1;
	}
#else
	
	connsock = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == connsock) {
		PrintError(WSAGetLastError(), "main: socket ");
		return -1;
	}
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	
	if(argc == 1) 
		servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	else
		servaddr.sin_addr.s_addr = inet_addr(argv[1]);
	
	servaddr.sin_port = htons(LPX_PORT_NUMBER);
	
	err = connect(connsock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "main: connect ");
		return -1;
	}
	
#endif
	
	// Target ID.
	if(argc >= 3) {
		iTargetID = atoi(argv[2]);
	} else {
		iTargetID = 0;
	}

	//
	// Init.
	//
	iTag = 0;
	HPID = 0;
	HeaderEncryptAlgo = 0;
	DataEncryptAlgo = 0;
	iPassword = HASH_KEY_USER;

	if(Discovery(connsock) != 0)
		return -1;
	
	//
	// Close Socket.
	//
	err = closesocket(connsock);
	if(err == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "main: closesocket ");
	}

	/////////////////////////////////////////////////////
	//
	// Normal 
	//

#ifdef _LPX_
	
	//
	// Make Connection.
	//
	if(MakeConnection(&address, &connsock) == FALSE) {
		fprintf(stderr, "[LanScsiCli]main: Can't Make Connection to LanDisk!!!\n");
					   
		return -1;
	}
	
#else
	
	connsock = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == connsock) {
		PrintError(WSAGetLastError(), "main: socket ");
		return -1;
	}
	
	err = connect(connsock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "main: connect ");
		return -1;
	}
	
#endif

	//
	// Login...
	//

	switch(iTargetID) {
	case 0:
		UserID = FIRST_TARGET_RO_USER;
		iTargetID = 0;
		break;
	case 1:
		UserID = FIRST_TARGET_RW_USER;
		iTargetID = 0;
		break;
	case 10:
		UserID = SECOND_TARGET_RO_USER;
		iTargetID = 1;
		break;
	case 11:
		UserID = SECOND_TARGET_RW_USER;
		iTargetID = 1;
		break;
	case 101:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_SET_MAX_RET_TIME;
		break;
	case 102:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_SET_MAX_CONN_TIME;
		break;
	case 103:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_GET_MAX_RET_TIME;
		break;
	case 104:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_GET_MAX_CONN_TIME;
		break;
	case 111:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_SET_SUPERVISOR_PW;
		break;
	case 112:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_SET_USER_PW;
		break;
	case 199:
		UserID = SUPERVISOR;
		cCommand = VENDER_OP_RESET;
		break;
	default:
		fprintf(stderr, "[LanScsiCli]main: Bad Target ID...\n");
		return -1 ;

	}

	printf("Press Enter...\n");
	getchar();

	if(UserID == SUPERVISOR) {

		unsigned _int64	Parameter = 0;

		fprintf(stderr, "SUPERVISOR MODE!!!\n");

		if((iResult = Login(connsock, LOGIN_TYPE_NORMAL, SUPERVISOR, HASH_KEY_USER)) != 0) {
			fprintf(stderr, "[LanScsiCli]main: Normal Key Login Failed...\n");
			
			if((iResult = Login(connsock, LOGIN_TYPE_NORMAL, SUPERVISOR, HASH_KEY_SUPER)) != 0) {
				fprintf(stderr, "[LanScsiCli]main: Super Key Login Failed...\n");
				return iResult;
			}
		}

		fprintf(stderr, "[LanScsiCli]main: End Login...\n");

		switch(cCommand) {
		case VENDER_OP_SET_MAX_RET_TIME:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_SET_MAX_RET_TIME...\n");
			Parameter = atoi(argv[3]);
			Parameter = HTONLL(Parameter);
			break;
		case VENDER_OP_SET_MAX_CONN_TIME:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_SET_MAX_CONN_TIME...\n");
			Parameter = atoi(argv[3]);
			Parameter = HTONLL(Parameter);
			break;
		case VENDER_OP_GET_MAX_RET_TIME:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_GET_MAX_RET_TIME...\n");
			break;
		case VENDER_OP_GET_MAX_CONN_TIME:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_GET_MAX_CONN_TIME...\n");
			break;
		case VENDER_OP_SET_SUPERVISOR_PW:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_SET_SUPERVISOR_PW...\n");
			Parameter = HTONLL(HASH_KEY_USER);
			break;
		case VENDER_OP_SET_USER_PW:
			fprintf(stderr, "[LanScsiCli]main: VENDER_OP_SET_USER_PW...\n");
			Parameter = HTONLL(HASH_KEY_SUPER);
			break;
		case VENDER_OP_RESET:
			fprintf(stderr, "[LanScsiCli]main: Reset...\n");
			VenderCommand(connsock, VENDER_OP_RESET, &Parameter);
			return 0;
		default:
			fprintf(stderr, "[LanScsiCli]main: Bad Command...%d\n", cCommand);
			return -1;
		}
	
		// Perform Operation.
		VenderCommand(connsock, cCommand, &Parameter);

		fprintf(stderr, "[LanScsiCli]main: End Vender Command...%I64d\n", NTOHLL(Parameter));
		
		// Must Reset.
		VenderCommand(connsock, VENDER_OP_RESET, &Parameter);
		
		return 0;

	}

	if((iResult = Login(connsock, LOGIN_TYPE_NORMAL, UserID, HASH_KEY_USER)) != 0) {
		fprintf(stderr, "[LanScsiCli]main: Login Failed...\n");
		return iResult;
	}

	printf("Press Enter...\n");
	getchar();


	for(i = 0; i < 10; i++) {
		PerTarget[iTargetID].TargetData = i;
		
		if(UserID & 0x00010000 || UserID & 0x00020000) {		
			if((iResult = TextTargetData(connsock, PARAMETER_OP_SET, iTargetID)) != 0) {
				fprintf(stderr, "[LanScsiCli]main: Text TargetData set Failed...\n");
				return iResult;
				
			}
		}
		
		if((iResult = TextTargetData(connsock, PARAMETER_OP_GET, iTargetID)) != 0) {
			fprintf(stderr, "[LanScsiCli]main: Text TargetData get Failed...\n");
			return iResult;
			
		}
		
		if(i != (int)PerTarget[iTargetID].TargetData) {
			printf("Mismatch; i %d %d\n", i, PerTarget[iTargetID].TargetData);
		}
	}

	if((iResult = GetDiskInfo(connsock, iTargetID)) != 0) {
		fprintf(stderr, "[LanScsiCli]main: Identify Failed... Master\n");
	} else {
		unsigned _int32	Source, a;

		start_time = (int) time((time_t *) NULL);
		
		printf("Max Blocks %d\n", requestBlocks);
		
		//requestBlocks = 8;
		
		Source = 0;
		a = 0;
		a -= 1;
		
		for(i = (PerTarget[iTargetID].SectorCount / requestBlocks/ 8) * (PerTarget[iTargetID].NRROHost);
		i < (PerTarget[iTargetID].SectorCount / requestBlocks); i++) {
			UCHAR	buffer[MAX_DATA_BUFFER_SIZE];
			
			for(unsigned int j = 0; j < requestBlocks * 512; j++)
				data[j] = (CHAR)i;
			data[0] = (UCHAR)iTargetID;

			memcpy(buffer, data, requestBlocks * 512);
			
			if(UserID & 0x00010000 || UserID & 0x00020000) {
				// Write.
				if((iResult = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, i * requestBlocks, requestBlocks, 0, (PCHAR)data)) != 0) {
					fprintf(stderr, "[LanScsiCli]main: WRITE Failed... Sector %d\n", i);
					return iResult;
				}

			} else {
				
			/*	
				if((iResult = IdeCommand(connsock, iTargetID, 0, WIN_IDENTIFY, i * requestBlocks, requestBlocks, 0, (PCHAR)data2)) != 0) {
					fprintf(stderr, "[LanScsiCli]main: READ Failed... Sector %d\n", i);
					return iResult;
				}
			*/
				/*
				// Verify.
				if((iResult = IdeCommand(connsock, iTargetID, 0, WIN_VERIFY, i * requestBlocks, requestBlocks, 0, NULL)) != 0) {
					if(iResult == WAIT_TIMEOUT) {
						fprintf(stderr, "[LanScsiCli]main: Retry...\n");
						return iResult;
					} else {
						fprintf(stderr, "[LanScsiCli]main: VERIFY Failed... Sector %d\n", i);
						
						return iResult;
					}
				}
				*/
				// Read.
				if((iResult = IdeCommand(connsock, iTargetID, 0, WIN_READ, i * requestBlocks, requestBlocks, 0, (PCHAR)data2)) != 0) {
					fprintf(stderr, "[LanScsiCli]main: READ Failed... Sector %d\n", i);
					return iResult;
				}
				
				/*
				if(data2[0] != (UCHAR)iTargetID) {
					fprintf(stderr, "\n[LanScsiCli]main: Target ID MisMatch... My ID 0x%x, received 0x%x\n", iTargetID, data2[0]);
					for(int k = 0; k < 512; k++) {
						printf("%x ", data2[k]);
					}
					printf("\n");
					return -1;
				}
				
				for(j = 1; j < requestBlocks * 512; j++) {
					if(buffer[j] != data2[j]) {
						fprintf(stderr, "\n[LanScsiCli]main: MisMatch... w 0x%x, r 0x%x\n", buffer[j], data2[j]);
						for(int k = 0; k < 512; k++) {
							printf("%x ", data2[k]);
						}
						printf("\n");
						return -1;
					}
				}
				*/
			}
			printf("%d\n", i);
		}
		
		finish_time = (int) time((time_t *) NULL);
		
		printf("----> Result) while %d seconds \n\n", finish_time - start_time);

	}
	
	//
	// Logout Packet.
	//
	if((iResult = Logout(connsock)) != 0) {
		fprintf(stderr, "[LanScsiCli]main: Logout Failed...\n");
		return iResult;
	}
	
	fprintf(stderr, "[LanScsiCli]main: Logout..\n");

	//
	// Close Socket.
	//
	closesocket(connsock);
	
	err = WSACleanup();
	if(err != 0) {
		PrintError(WSAGetLastError(), "main: WSACleanup ");
		return -1;
	}
	
	return 0;
}

