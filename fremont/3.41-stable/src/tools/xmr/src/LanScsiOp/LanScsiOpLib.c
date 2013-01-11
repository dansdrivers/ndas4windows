#include "..\Inc\LanScsiOp.h"
#include <winsock2.h>
//////////////////////////////////////////////////////
//
// Debugging...
//


#ifdef _DEBUG	

#if defined(OUTPUT_PRINTF)
#define _OutputDebugString printf
#elif defined(OUTPUT_TRACE)
#define _OutputDebugString TRACE
#else
#define _OutputDebugString OutputDebugString
#endif

// DbgPrint
#define DEBUG_BUFFER_LENGTH 256

static CHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];

static VOID
DbgPrint(
		 IN PCHAR	DebugMessage,
		 ...
		 )
{
    va_list ap;
	
    va_start(ap, DebugMessage);
	
	_vsnprintf_s(DebugBuffer, DEBUG_BUFFER_LENGTH, _TRUNCATE, DebugMessage, ap);
	
	_OutputDebugString(DebugBuffer);
    
    va_end(ap);
}
		
static ULONG DebugPrintLevel = 2;

#define DebugPrint(_l_, _x_)			\
		do{								\
			if(_l_ < DebugPrintLevel)	\
				DbgPrint _x_;			\
		}	while(0)					\
		
#else	
#define DebugPrint(_l_, _x_)			\
		do{								\
		} while(0)
#define _OutputDebugString OutputDebugString
#endif

static void PrintError(
					   DWORD	ErrorCode,
					   LPTSTR strPrefix
					   )
{
	LPTSTR lpMsgBuf;
	
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
	_OutputDebugString(strPrefix);

	_OutputDebugString(lpMsgBuf);
	
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

//////////////////////////////////////////////////////
//
// Socket Operations...
//

int 
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
					RECV_TIME_OUT,
					TRUE
					);				
				if(dwError != WSA_WAIT_EVENT_0) {
					
					PrintError(dwError, "RecvIt: ");
					dwRecvDataLen = SOCKET_ERROR;
					
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
					PrintError(GetLastError(), TEXT("RecvIt: GetOverlappedResult Failed "));
					dwRecvDataLen = SOCKET_ERROR;
					goto Out;
				}

			} else {
				PrintError(dwError, TEXT("RecvIt: WSARecv Failed "));
				
				dwRecvDataLen = SOCKET_ERROR;
				goto Out;
			}
		}

		iReceived += dwRecvDataLen;

		WSAResetEvent(hEvent);
	}

Out:
	WSACloseEvent(hEvent);
	if(dwRecvDataLen == SOCKET_ERROR) return dwRecvDataLen;
	else return iReceived;
}

int 
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
			return SOCKET_ERROR;
		}
		len -= res;
		buf += res;
	}
	
	return size;
}

int
ReadReply(
		  SOCKET		connSock,
		  PLANSCSI_PATH	pPath,
		  PCHAR			pBuffer,
		  PLANSCSI_PDU_POINTERS	pPdu
		  )

{
	int		iResult, iTotalRecved = 0;
	PCHAR	pPtr = pBuffer;
	
	// Read Header.
	iResult = RecvIt(
		connSock,
		pPtr,
		sizeof(LANSCSI_R2H_PDU_HEADER)
		);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]ReadReply: Can't Recv Header...\n"));
		
		return iResult;
	} else 
		iTotalRecved += iResult;
	
	pPdu->pR2HHeader = (PLANSCSI_R2H_PDU_HEADER)pPtr;
	
	pPtr += sizeof(LANSCSI_R2H_PDU_HEADER);
	
	if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pPath->iHeaderEncryptAlgo != 0) {
		Decrypt32(
			(unsigned char*)pPdu->pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&pPath->CHAP_C,
			(unsigned char *)&pPath->iPassword
			);
	}

	// Read AHS.
	if(ntohs(pPdu->pR2HHeader->AHSLen) > 0) {

		DebugPrint(2, ("[LanScsiOpLib]ReadReply: AHSLen %d\n", ntohs(pPdu->pR2HHeader->AHSLen)));

		iResult = RecvIt(
			connSock,
			pPtr,
			ntohs(pPdu->pR2HHeader->AHSLen)
			);
		if(iResult == SOCKET_ERROR) {
			DebugPrint(1, ("[LanScsiOpLib]ReadReply: Can't Recv AHS...\n"));
			
			return iResult;
		} else 
			iTotalRecved += iResult;
		
		pPdu->pAHS = pPtr;
		
		pPtr += ntohs(pPdu->pR2HHeader->AHSLen);
		if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
			if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
				&& pPath->iHeaderEncryptAlgo != 0) {
				Decrypt32(
					(unsigned char*)pPdu->pAHS,
					ntohs(pPdu->pR2HHeader->AHSLen),
					(unsigned char *)&pPath->CHAP_C,
					(unsigned char *)&pPath->iPassword
					);
			}
		}
	}
	
	// Read Header Dig.
	pPdu->pHeaderDig = NULL;
	
	// Read Data segment.
	if(ntohl(pPdu->pR2HHeader->DataSegLen) > 0) {

		DebugPrint(3, ("[LanScsiOpLib]ReadReply: DataSegLen %d\n", ntohl(pPdu->pR2HHeader->DataSegLen)));

		iResult = RecvIt(
			connSock,
			pPtr,
			ntohl(pPdu->pR2HHeader->DataSegLen)
			);
		if(iResult == SOCKET_ERROR) {
			DebugPrint(1, ("[LanScsiOpLib]ReadReply: Can't Recv Data segment...\n"));
			
			return iResult;
		} else 
			iTotalRecved += iResult;
		
		pPdu->pDataSeg = pPtr;
		
		pPtr += ntohl(pPdu->pR2HHeader->DataSegLen);

		if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
			&& pPath->iDataEncryptAlgo != 0) {
			
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohl(pPdu->pH2RHeader->DataSegLen),
				(unsigned char *)&pPath->CHAP_C,
				(unsigned char *)&pPath->iPassword
				);
		}

	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	DebugPrint(4, ("[LanScsiOpLib]ReadReply: End\n"));

	return iTotalRecved;
}

int
SendRequest_V1(
			SOCKET			connSock,
			PLANSCSI_PATH	pPath,
			PLANSCSI_PDU_POINTERS	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iAHSLen, iDataSegLen, iResult;

	DebugPrint(7, ("[LanScsiOpLib]SendRequest_V1: Start\n"));

	pHeader = pPdu->pH2RHeader;
	iAHSLen = ntohs(pHeader->AHSLen);
	iDataSegLen = ntohl(pHeader->DataSegLen);

	//
	// Check Parameter.
	//
	if(iAHSLen < 0 || iDataSegLen < 0) {
		DebugPrint(7, ("SendRequest: Bad Parameter.\n"));
		return -1;
	}

	//
	// Encrypt Header.
	//
	if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pPath->iHeaderEncryptAlgo != 0) {
		
		Encrypt32(
			(unsigned char*)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&pPath->CHAP_C,
			(unsigned char*)&pPath->iPassword
			);
		
		if(iAHSLen > 0) {
			Encrypt32(
				(unsigned char*)pPdu->pAHS,
				iAHSLen,
				(unsigned char *)&pPath->CHAP_C,
				(unsigned char*)&pPath->iPassword
				);
		}

	}
	
	//
	// Encrypt Data.
	//
	if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pPath->iDataEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt32(
			(unsigned char*)pPdu->pDataSeg,
			iDataSegLen,
			(unsigned char *)&pPath->CHAP_C,
			(unsigned char*)&pPath->iPassword
			);
	}

	// Send Request.
	iResult = SendIt(
		connSock,
		(PCHAR)pHeader,
		sizeof(LANSCSI_H2R_PDU_HEADER) + iAHSLen + iDataSegLen
		);
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "SendRequest: Send Request ");
		return -1;
	}

	DebugPrint(7, ("[LanScsiOpLib]SendRequest_V1: End\n"));
	
	return 0;
}

int
SendRequest(
			SOCKET			connSock,
			PLANSCSI_PATH	pPath,
			PLANSCSI_PDU_POINTERS	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		return SendRequest_V1(connSock, pPath, pPdu);
	}

	pHeader = pPdu->pH2RHeader;
	iDataSegLen = ntohl(pHeader->DataSegLen);

	//
	// Encrypt Header.
	//
	if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pPath->iHeaderEncryptAlgo != 0) {
		
		Encrypt32(
			(unsigned char*)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&pPath->CHAP_C,
			(unsigned char*)&pPath->iPassword
			);
	}
	
	//
	// Encrypt Header.
	//
	if(pPath->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pPath->iDataEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt32(
			(unsigned char*)pPdu->pDataSeg,
			iDataSegLen,
			(unsigned char *)&pPath->CHAP_C,
			(unsigned char*)&pPath->iPassword
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

/////////////////////////////////////////////////////////////
//
// Exported Functions
//

int
Login(
	  PLANSCSI_PATH				pPath,
	  UCHAR						cLoginType
	  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestPdu;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
	PBIN_PARAM_SECURITY					pParamSecu;
	PAUTH_PARAMETER_CHAP				pParamChap;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	LANSCSI_PDU_POINTERS				pdu;
	unsigned _int16						iSubSequence;
	int									iResult;
	unsigned							CHAP_I;
	
	// HWVersion is detected after 1st step
//	pPath->HWVersion = LANSCSIIDE_CURRENT_VERSION;
	pPath->HWProtoVersion = LSIDEPROTO_CURRENT_VERSION;

	DebugPrint(1, ("Login Type(0x%x), HWVersion(0x%x)\n", pPath->HWType, pPath->HWVersion));
	DebugPrint(1, ("Login Protocol Type(0x%x), Protocol Version(0x%x)\n", pPath->HWProtoType, pPath->HWProtoVersion));
	DebugPrint(3, ("pPath->HPID %d\n", pPath->HPID));
	//
	// Init.
	//
	iSubSequence = 0;
	pPath->iSessionPhase = FLAG_SECURITY_PHASE;
	pPath->iHeaderEncryptAlgo = 0;
	pPath->iDataEncryptAlgo = 0;
	
	// 
	// First Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(pPath->HPID);

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	}
	
	pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(pPath->iCommandTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	pLoginRequestPdu->VerMax = pPath->HWVersion;
	pLoginRequestPdu->VerMin = 0;

#ifdef __NDASCHIP20_ALPHA_SUPPORT__
	if(pLoginRequestPdu->VerMax == LANSCSIIDE_VERSION_2_0)
		pLoginRequestPdu->VerMax = LANSCSIIDE_VERSION_1_1;
#endif

	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pdu.pDataSeg = (char *)pParamSecu;
	}
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	}
	DebugPrint(7, ("Send First Request\n"));
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiOpLib]Login: Send First Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(7, ("[LanScsiOpLib]login: First Can't Read Reply.\n"));
		return -1;
	}

	// Check Reply Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		
		DebugPrint(1, ("[LanScsiOpLib]login: Opcode(0x%x)\nT(0x%x)CSG(0x%x)\nNSG(0x%x)\nVerActive(0x%x)\nParameterType(0x%x)\nParameterVer(ParameterVer)\n",
			pLoginReplyHeader->Opcode,
			pLoginReplyHeader->T,
			pLoginReplyHeader->CSG,
			pLoginReplyHeader->NSG,
			pLoginReplyHeader->VerActive,
			pLoginReplyHeader->ParameterType,
			pLoginReplyHeader->ParameterVer));
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD First Reply Header.\n"));
		return -1;
	}
	
	// Set HWVersion with detected one.
	pPath->HWVersion = pLoginReplyHeader->VerActive;
	if(pPath->HWVersion == LANSCSIIDE_VERSION_1_0) {
		pPath->HWProtoVersion = LSIDEPROTO_VERSION_1_0;
	} else if(pPath->HWVersion >= LANSCSIIDE_VERSION_1_1) {
		pPath->HWProtoVersion = LSIDEPROTO_VERSION_1_1;
	}

	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]login: First Failed. %02x\n", pLoginReplyHeader->Response));
		return -1;
	}
	
	// Check Data segment.
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REPLY)
		|| (pdu.pDataSeg == NULL)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD First Reply Data.\n"));
		return -1;
	}	
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if((ntohl(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REPLY)
			|| (pdu.pAHS == NULL)) {
		
			DebugPrint(1, ("[LanScsiOpLib]login: BAD First Reply Data.\n"));
			return -1;
		}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != cLoginType
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD First Reply Parameters.\n"));
		return -1;
	}

	// Store Data.
	pPath->RPID = ntohs(pLoginReplyHeader->RPID);
	
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	DebugPrint(2, ("[LanScsiOpLib]login: Version %d Auth %d\n", 
		pLoginReplyHeader->VerActive, 
		ntohs(pParamSecu->AuthMethod))
		);
	
	// 
	// Second Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(pPath->HPID);
	pLoginRequestPdu->RPID = htons(pPath->RPID);

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	}
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	}
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(pPath->iCommandTag);
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
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (char *)pParamSecu;
	}
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	}
	DebugPrint(7, ("Send Second Request\n"));
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiOpLib]Login: Send Second Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]login: Second Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Second Reply Header.\n"));
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]login: Second Failed. %02x\n", pLoginReplyHeader->Response));
		return -1;
	}
	
	// Check Data segment.
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REPLY)
		|| (pdu.pDataSeg == NULL)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Second Reply Data.\n"));
		return -1;
	}	
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if((ntohl(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REPLY)
			|| (pdu.pAHS == NULL)) {
		
			DebugPrint(1, ("[LanScsiOpLib]login: BAD Second Reply Data.\n"));
			return -1;
		}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}

	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != cLoginType
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Second Reply Parameters.\n"));
		return -1;
	}
	
	// Store Challenge.	
	pParamChap = &pParamSecu->ChapParam;
	CHAP_I = ntohl(pParamChap->CHAP_I);
	pPath->CHAP_C = ntohl(pParamChap->CHAP_C[0]);
	//memcpy(&CHAP_C, pParamChap->CHAP_C, CHAP_MAX_CHALLENGE_LENGTH);
	
	DebugPrint(2, ("[LanScsiOpLib]login: Hash %d, Challenge %d\n", 
		ntohl(pParamChap->CHAP_A), 
		pPath->CHAP_C)
		);
	
	// 
	// Third Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_SECURITY_PHASE;
	pLoginRequestPdu->NSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->HPID = htonl(pPath->HPID);
	pLoginRequestPdu->RPID = htons(pPath->RPID);

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	}
	
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(pPath->iCommandTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	pParamChap->CHAP_A = htonl(HASH_ALGORITHM_MD5);
	pParamChap->CHAP_I = htonl(CHAP_I);
	
	if(cLoginType == LOGIN_TYPE_NORMAL)
		pParamChap->CHAP_N = htonl(pPath->iUserID);
	else
		pParamChap->CHAP_N = 0;


	//
	// Hashin...
	//
	Hash32To128(
		(unsigned char*)&pPath->CHAP_C, 
		(unsigned char*)pParamChap->CHAP_R, 
		(unsigned char*)&pPath->iPassword
		);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (char *)pParamSecu;
	}
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamSecu;
	}
	DebugPrint(7, ("Send Third Request\n"));
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "Login: Send Third Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]login: Third Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Third Reply Header.\n"));
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]login: Third Failed. %02x\n", pLoginReplyHeader->Response));
		return -1;
	}
	
	// Check Data segment.
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REPLY)
		|| (pdu.pDataSeg == NULL)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Third Reply Data.\n"));
		return -1;
	}	
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if((ntohl(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REPLY)
			|| (pdu.pAHS == NULL)) {
		
			DebugPrint(1, ("[LanScsiOpLib]login: BAD Third Reply Data.\n"));
			return -1;
		}	
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParamSecu = (PBIN_PARAM_SECURITY)pdu.pAHS;
	}
	
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->LoginType != cLoginType
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Third Reply Parameters.\n"));
		return -1;
	}
	
	pPath->iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;

	// 
	// Fourth Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->NSG = FLAG_FULL_FEATURE_PHASE;
	pLoginRequestPdu->HPID = htonl(pPath->HPID);
	pLoginRequestPdu->RPID = htons(pPath->RPID);
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	}
	
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(pPath->iCommandTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	
	pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (char *)pParamNego;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (char *)pParamNego;
	}
	DebugPrint(7, ("Send Fourth Request\n"));
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "Login: Send Fourth Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]login: Fourth Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Fourth Reply Header.\n"));
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]login: Fourth Failed. %02x\n", pLoginReplyHeader->Response));
		return -1;
	}
	
	// Check Data segment.
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY)
		|| (pdu.pDataSeg == NULL)) {
		
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Fourth Reply Data.\n"));
		return -1;
	}	
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if((ntohl(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY)
			|| (pdu.pAHS == NULL)) {
		
			DebugPrint(1, ("[LanScsiOpLib]login: BAD Fourth Reply Data.\n"));
			return -1;
		}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
	}
	
	if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
		DebugPrint(1, ("[LanScsiOpLib]login: BAD Fourth Reply Parameters.\n"));
		return -1;
	}
	
	DebugPrint(1, ("[LanScsiOpLib]login: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n", 
		pParamNego->HWType, pParamNego->HWVersion,
		ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
		ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID))
		);
	
	pPath->HWType = pParamNego->HWType;
	pPath->HWVersion = pParamNego->HWVersion;
	pPath->HWProtoType = pParamNego->HWType;
	if(pParamNego->HWVersion == LANSCSIIDE_VERSION_1_0) {
		pPath->HWProtoVersion = LSIDEPROTO_VERSION_1_0;
	} else if(pParamNego->HWVersion >= LANSCSIIDE_VERSION_1_1) {
		pPath->HWProtoVersion = LSIDEPROTO_VERSION_1_1;
	}
	pPath->iNumberofSlot = ntohl(pParamNego->NRSlot);
	pPath->iMaxBlocks = ntohl(pParamNego->MaxBlocks);
	pPath->iMaxTargets = ntohl(pParamNego->MaxTargetID);
	pPath->iMaxLUs = ntohl(pParamNego->MaxLUNID);
	pPath->iHeaderEncryptAlgo = ntohs(pParamNego->HeaderEncryptAlgo);
	pPath->iDataEncryptAlgo = ntohs(pParamNego->DataEncryptAlgo);

	pPath->iSessionPhase = FLAG_FULL_FEATURE_PHASE;

	return 0;
}

int
Logout(
	   PLANSCSI_PATH	pPath
	   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = LOGOUT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(pPath->HPID);
	pRequestHeader->RPID = htons(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++pPath->iCommandTag);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiOpLib]Logout: Send Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]Logout: Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != LOGOUT_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		DebugPrint(1, ("[LanScsiOpLib]Logout: BAD Reply Header.\n"));
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]Logout: Failed.\n"));
		return -1;
	}
	
	pPath->iSessionPhase = LOGOUT_PHASE;

	return 0;
}

int
TextTargetList(
			   PLANSCSI_PATH	pPath
			   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_LIST				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;
	int									i;
	
	DebugPrint(3, ("[LanScsiOpLib]TextTargetList: Entered.\n"));
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(pPath->HPID);
	pRequestHeader->RPID = htons(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	pRequestHeader->AHSLen = 0;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++pPath->iCommandTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (PCHAR)pParam;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (PCHAR)pParam;
	}
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiOpLib]TextTargetList: Send First Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetList: Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		DebugPrint(1, ("[LanScsiOpLib]TextTargetList: BAD Reply Header.\n"));
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetList: Failed.\n"));
		return -1;
	}
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetList: No Data Segment.\n"));
		return -1;		
	}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if(pReplyHeader->AHSLen < BIN_PARAM_SIZE_REPLY) {
			DebugPrint(7, ("[LanScsiOpLib]TextTargetList: No Data Segment.\n"));
			return -1;		
		}
	}
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
	}
	
	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
		DebugPrint(1, ("TEXT: Bad Parameter Type.\n"));
		return -1;			
	}
	DebugPrint(2, ("[LanScsiOpLib]TextTargetList: NR Targets : %d\n", pParam->NRTarget));

	// BUG BUG BUG !!!!!!!!!!!!!! since Bad NetDisk.
	if(pParam->NRTarget > 1)
		pParam->NRTarget = 1;

	pPath->iNRTargets = pParam->NRTarget;
	
	
	// Cleanup first.
	for(i = 0; i < NR_MAX_TARGET; i++) {
		pPath->PerTarget[i].bPresent = FALSE;
	}

	for(i = 0; i < pParam->NRTarget; i++) {
		PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
		int								iTargetId;
		
		pTarget = &pParam->PerTarget[i];
		iTargetId = ntohl(pTarget->TargetID);
		
		DebugPrint(2, ("[LanScsiOpLib]TextTargetList: NR Targets  %d: Target ID: 0x%x, NR_RW: %d, NR_RO: %d, Data:0x%x \n", i, 
			ntohl(pTarget->TargetID), 
			pTarget->NRRWHost,
			pTarget->NRROHost,
			pTarget->TargetData)
			);
		
		pPath->PerTarget[iTargetId].bPresent = TRUE;
		pPath->PerTarget[iTargetId].NRRWHost = pTarget->NRRWHost;
		pPath->PerTarget[iTargetId].NRROHost = pTarget->NRROHost;
		pPath->PerTarget[iTargetId].TargetData = pTarget->TargetData;
	}
	
	return 0;
}

int
TextTargetData(
			   PLANSCSI_PATH	pPath,
			   UCHAR			cGetorSet,
			   UINT				TargetID,
			   unsigned _int64	*pData
			   )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_DATA				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;
	
	DebugPrint(3, ("[LanScsiOpLib]TextTargetData: Entered.\n"));
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(pPath->HPID);
	pRequestHeader->RPID = htons(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	pRequestHeader->AHSLen = 0;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++pPath->iCommandTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_DATA;
	pParam->GetOrSet = cGetorSet;
	
	if(cGetorSet == PARAMETER_OP_SET)
		pParam->TargetData = *pData;

	pParam->TargetID = htonl(TargetID);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pdu.pDataSeg = (PCHAR)pParam;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pdu.pAHS = (PCHAR)pParam;
	}

	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "[LanScsiOpLib]TextTargetData: Send First Request ");
		return -1;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetData: Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		DebugPrint(1, ("[LanScsiOpLib]TextTargetData: BAD Reply Header.\n"));
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetData: Failed.\n"));
		return -1;
	}
	
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		DebugPrint(1, ("[LanScsiOpLib]TextTargetData: No Data Segment.\n"));
		return -1;		
	}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		if(pReplyHeader->AHSLen < BIN_PARAM_SIZE_REPLY) {
			DebugPrint(1, ("[LanScsiOpLib]TextTargetData: No Data Segment.\n"));
			return -1;		
		}
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_0) {
	pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
	}

	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		pParam = (PBIN_PARAM_TARGET_DATA)pdu.pAHS;
	}
	
	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
		DebugPrint(1, ("TextTargetData: Bad Parameter Type.\n"));
		return -1;			
	}

	*pData = pParam->TargetData;

	DebugPrint(2, ("[LanScsiOpLib]TextTargetList: TargetID : %d, GetorSet %d, Target Data %d\n", 
		ntohl(pParam->TargetID), pParam->GetOrSet, *pData)
		);
	
	return 0;
}

int
IdeCommand_V1(
		   PLANSCSI_PATH	pPath,
		   _int32			TargetId,
		   _int64			LUN,
		   UCHAR			Command,
		   _int64			Location,
		   _int16			SectorCount,
		   _int8			Feature,
		   PCHAR			pData,
		   unsigned _int8	*pResponse
		   )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	int								iResult;
	unsigned _int8					iCommandReg;
	

	DebugPrint(4, ("[LanScsiOpLib]IdeCommand_V1: Start\n"));
	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(pPath->HPID);
	pRequestHeader->RPID = htons(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++pPath->iCommandTag);
	pRequestHeader->TargetID = htonl(TargetId);
	pRequestHeader->LUN = 0;
	
	// Using Target ID. LUN is always 0.
	if(TargetId == 0)
		pRequestHeader->DEV = 0;
	else 
		pRequestHeader->DEV = 1;
	
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0;
	pRequestHeader->COM_TYPE_P = 0;
	
	switch(Command) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
#ifdef __NDASCHIP20_ALPHA_SUPPORT__
			//
			//	Execute only PIO read with NDAS chip 2.0 alpha.
			//
			if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
				DebugPrint(1,("WIN_READ_EXT\n"));
				pRequestHeader->Command = WIN_READ_EXT;
				pRequestHeader->COM_TYPE_E = 1;
			} else {
				DebugPrint(1,("WIN_READ\n"));
				pRequestHeader->Command = WIN_READ;
			}
			pRequestHeader->COM_TYPE_D_P = 0;
#else
			if(pPath->PerTarget[TargetId].bDma) {
				if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
					DebugPrint(1,("WIN_READDMA_EXT\n"));
					pRequestHeader->Command = WIN_READDMA_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					DebugPrint(1,("WIN_READDMA\n"));
					pRequestHeader->Command = WIN_READDMA;
				}
				pRequestHeader->COM_TYPE_D_P = 1;
			} else {
				if(!pPath->PerTarget[TargetId].bPIO) {
					DebugPrint(1,("[LanscsiOp] IdeCommand_V1: WIN_READ: Could not determine I/O mode.\n"));
					return -1;
				}
				if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
					DebugPrint(1,("WIN_READ_EXT\n"));
					pRequestHeader->Command = WIN_READ_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					DebugPrint(1,("WIN_READ\n"));
					pRequestHeader->Command = WIN_READ;
				}
				pRequestHeader->COM_TYPE_D_P = 0;
			}
#endif
			pRequestHeader->COM_TYPE_R = 1;
			pRequestHeader->COM_LENG = (htonl(SectorCount*512) >> 8);

		}
		break;
	case WIN_WRITE:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 1;
			if(pPath->PerTarget[TargetId].bDma) {
				if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
					pRequestHeader->Command = WIN_WRITEDMA_EXT;
					DebugPrint(1,("WIN_WRITEDMA_EXT\n"));
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITEDMA;					
					DebugPrint(1,("WIN_WRITEDMA\n"));
				}
				pRequestHeader->COM_TYPE_D_P = 1;
			} else {
				if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {

					if(!pPath->PerTarget[TargetId].bPIO) {
						DebugPrint(1,("[LanscsiOp] IdeCommand_V1: WIN_WRITE: Could not determine I/O mode.\n"));
						return -1;
					}

					DebugPrint(1,("WIN_WRITE_EXT\n"));
					pRequestHeader->Command = WIN_WRITE_EXT;
					pRequestHeader->COM_TYPE_E = 1;
				} else {
					pRequestHeader->Command = WIN_WRITE;				
					DebugPrint(1,("WIN_WRITE\n"));
				}
				pRequestHeader->COM_TYPE_D_P = 0;
			}
			pRequestHeader->COM_TYPE_W = 1;
			pRequestHeader->COM_LENG = (htonl(SectorCount*512) >> 8);
		}

		break;
	case WIN_VERIFY:
		{
			DebugPrint(7,("WIN_VERIFY\n"));
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
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
			DebugPrint(7,("WIN_IDENTIFY\n"));
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			
			pRequestHeader->Command = Command;

			pRequestHeader->COM_TYPE_R = 1;			
			pRequestHeader->COM_LENG = (htonl(1*512) >> 8);

		}
		break;
	case WIN_SETFEATURES:
		{
			DebugPrint(7,("WIN_SETFEATURES\n"));
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = Feature;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)SectorCount;
			pRequestHeader->Command = WIN_SETFEATURES;

			DebugPrint(7, ("[LanScsiOpLib]IdeCommand_V1: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr));
		}
		break;
	case WIN_SETMULT:
		{
			DebugPrint(7,("WIN_SETMULT\n"));
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)SectorCount;
			pRequestHeader->Command = WIN_SETMULT;
		}
		break;
	case WIN_CHECKPOWERMODE1:
		{
			DebugPrint(7,("WIN_CHECKPOWERMODE1\n"));
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
			DebugPrint(7,("WIN_STANDBY\n"));
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_STANDBY;
		}
		break;
	default:
		DebugPrint(7, ("[LanScsiOpLib]IdeCommand_V1: Not Supported IDE Command.\n"));
		return -1;
	}
		
	if((Command == WIN_READ)
		|| (Command == WIN_WRITE)
		|| (Command == WIN_VERIFY)){
		
		if(pPath->PerTarget[TargetId].bLBA == FALSE) {
			DebugPrint(1, ("[LanScsiOpLib]IdeCommand_V1: CHS not supported...\n"));
			return -1;
		}
		
		pRequestHeader->LBA = 1;
		
		if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
			
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
	
	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "IdeCommand_V1: Send Request ");
		return -1;
	}
	
	// If Write, Send Data.
	if(Command == WIN_WRITE) {
		//
		// Encrypt Data.
		//
		if(pPath->iDataEncryptAlgo != 0) {
			Encrypt32(
				(unsigned char*)pData,
				SectorCount * 512,
				(unsigned char *)&pPath->CHAP_C,
				(unsigned char *)&pPath->iPassword
				);
		}

		iResult = SendIt(
			pPath->connsock,
			pData,
			SectorCount * 512
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "IdeCommand_V1: Send data for WRITE ");
			return -1;
		}
	}
	
	// If Read, Identify Op... Read Data.
	switch(Command) {
	case WIN_READ:
		{
			iResult = RecvIt(
				pPath->connsock, 
				pData, 
				SectorCount * 512
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), "IdeCommand_V1: Receive Data for READ ");
				return -1;
			}

			//
			// Decrypt Data.
			//
			
			if(pPath->iDataEncryptAlgo != 0) {
				
				Decrypt32(
					(unsigned char*)pData,
					SectorCount * 512,
					(unsigned char*)&pPath->CHAP_C,
					(unsigned char*)&pPath->iPassword
					);
			}
		
			
		}
		break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{

			iResult = RecvIt(
				pPath->connsock, 
				pData, 
				512
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), "IdeCommand_V1: Receive Data for IDENTIFY ");
				return -1;
			}

			//
			// Decrypt Data.
			//
			
			if(pPath->iDataEncryptAlgo != 0) {

				Decrypt32(
					(unsigned char*)pData,
					512,
					(unsigned char*)&pPath->CHAP_C,
					(unsigned char*)&pPath->iPassword
					);
			}
			
		}
		break;
	default:
		break;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]IdeCommand_V1: Can't Read Reply.\n"));
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		DebugPrint(7, ("[LanScsiOpLib]IdeCommand_V1: Time out...\n"));
		return WAIT_TIMEOUT;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;	
	if(pReplyHeader->Opcode != IDE_RESPONSE){		
		DebugPrint(1, ("[LanScsiOpLib]IdeCommand_V1: BAD Reply Header pReplyHeader->Opcode != IDE_RESPONSE . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command));
		return -1;
	}
	if(pReplyHeader->F == 0){		
		DebugPrint(7, ("[LanScsiOpLib]IdeCommand_V1: BAD Reply Header pReplyHeader->F == 0 . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command));
		return -1;
	}
	/*
	if(pReplyHeader->Command != iCommandReg) {		
	DebugPrint(7, ("[LanScsiOpLib]IdeCommand_V1: BAD Reply Header pReplyHeader->Command != iCommandReg . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
*/
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		DebugPrint(1, ("[LanScsiOpLib]IdeCommand_V1: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			));
		DebugPrint(7, ("Error register = 0x%x\n", pReplyHeader->Feature_Curr));
		
		return -1;
	}

	if(Command == WIN_CHECKPOWERMODE1){
		DebugPrint(1, ("Check Power mode = 0x%02x\n", (unsigned char)(pReplyHeader->SectorCount_Curr)));
	}

	*pResponse = pReplyHeader->Response;

	DebugPrint(3, ("[LanScsiOpLib]IdeCommand_V1: End\n"));

	return 0;
}

int
IdeCommand(
		   PLANSCSI_PATH	pPath,
		   _int32			TargetId,
		   _int64			LUN,
		   UCHAR			Command,
		   _int64			Location,
		   _int16			SectorCount,
		   _int8			Feature,
		   PCHAR			pData,
		   unsigned _int8	*pResponse
		   )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS			pdu;
	int								iResult;
	
	DebugPrint(3, ("[LanScsiOpLib]IDECommand: Entered.\n"));
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {
		return IdeCommand_V1(pPath, TargetId, LUN, Command, Location, SectorCount, Feature, pData, pResponse);
	}

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(pPath->HPID);
	pRequestHeader->RPID = htons(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++pPath->iCommandTag);
	pRequestHeader->TargetID = htonl(TargetId);
	pRequestHeader->LUN = 0;
	
	// Using Target ID. LUN is always 0.
	if(TargetId == 0)
		pRequestHeader->DEV = 0;
	else 
		pRequestHeader->DEV = 1;
	
	switch(Command) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			
			if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
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
			
			DebugPrint(2, ("[LanScsiOpLib]IDECommand: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr));
		}
		break;

	default:
		DebugPrint(1, ("[LanScsiOpLib]IDECommand: Not Supported IDE Command.\n"));
		return -1;
	}
	
	pRequestHeader->Feature = 0;			

	if((Command == WIN_READ)
		|| (Command == WIN_WRITE)
		|| (Command == WIN_VERIFY)){
		
		if(pPath->PerTarget[TargetId].bLBA == FALSE) {
			DebugPrint(1, ("[LanScsiOpLib]IDECommand: CHS not supported...\n"));
			return -1;
		}
		
		pRequestHeader->LBA = 1;
		
		if(pPath->PerTarget[TargetId].bLBA48 == TRUE) {
			
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
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(pPath->connsock, pPath, &pdu) != 0) {
		PrintError(WSAGetLastError(), "IdeCommand: Send Request ");
		return -1;
	}

	DebugPrint(3, ("[LanScsiOpLib]IDECommand: Send Request...\n"));

	// If Write, Send Data.
	if(Command == WIN_WRITE) {
		//
		// Encrypt Data.
		//
		if(pPath->iDataEncryptAlgo != 0) {
			Encrypt32(
				(unsigned char*)pData,
				SectorCount * 512,
				(unsigned char *)&pPath->CHAP_C,
				(unsigned char*)&pPath->iPassword
				);
		}

		iResult = SendIt(
			pPath->connsock,
			pData,
			SectorCount * 512
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "IdeCommand: Send data for WRITE ");
			return -1;
		}

		if(pPath->iDataEncryptAlgo != 0) {
			Decrypt32(
				(unsigned char*)pData,
				SectorCount * 512,
				(unsigned char *)&pPath->CHAP_C,
				(unsigned char*)&pPath->iPassword
				);
		}
	}
	
	// If Read, Identify Op... Read Data.
	switch(Command) {
	case WIN_READ:
		{
			iResult = RecvIt(
				pPath->connsock, 
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
			if(pPath->iDataEncryptAlgo != 0) {
				Decrypt32(
					(unsigned char*)pData,
					SectorCount * 512,
					(unsigned char *)&pPath->CHAP_C,
					(unsigned char*)&pPath->iPassword
					);
			}
		}
		break;
	case WIN_IDENTIFY:
		{
			iResult = RecvIt(
				pPath->connsock, 
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
			if(pPath->iDataEncryptAlgo != 0) {
				Decrypt32(
					(unsigned char*)pData,
					512,
					(unsigned char *)&pPath->CHAP_C,
					(unsigned char*)&pPath->iPassword
					);
			}
		}
		break;
	default:
		break;
	}
	
	// Read Reply.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("[LanScsiOpLib]IDECommand: Can't Read Reply.\n"));
		return -1;
	}
	
	// Check Reply Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pReplyHeader->Opcode != IDE_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		DebugPrint(1, ("[LanScsiOpLib]IDECommand: BAD Reply Header. 0x%x\n", pReplyHeader->Flags));
		return -1;
	}
	
	*pResponse = pReplyHeader->Response;
	
	return 0;
}


///////////////////////////////////////////////////////////////////
//
//
//		Vender specific command
//
//
///////////////////////////////////////////////////////////////////
int
VenderCommand(
				PLANSCSI_PATH	pPath,
				UCHAR			cOperation,
				unsigned _int64	*pParameter
			  )
{
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDER_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDER_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;

	
	if (pPath->HWProtoVersion != LSIDEPROTO_VERSION_1_1 ) {
		return -1;
	}

	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_VENDER_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = VENDER_SPECIFIC_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = HTONL(pPath->HPID);
	pRequestHeader->RPID = HTONS(pPath->RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag =  HTONL(++pPath->iCommandTag);
	pRequestHeader->VenderID = HTONS(NKC_VENDER_ID);
	pRequestHeader->VenderOpVersion = VENDER_OP_CURRENT_VERSION;
	pRequestHeader->VenderOp = cOperation;
	pRequestHeader->VenderParameter = *pParameter;

	DebugPrint(1,("VendorCommand: Operation %d, Parameter %I64x\n", cOperation, NTOHLL(*pParameter)));

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	iResult = SendRequest(pPath->connsock, pPath, &pdu);
	if(iResult != 0){

		DebugPrint(1, ("VendorCommand: Send First Request "));
		return -1;
	}

	// Read Request.
	iResult = ReadReply(pPath->connsock, pPath, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		DebugPrint(1, ("VendorCommand: Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pReplyHeader = (PLANSCSI_VENDER_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opcode != VENDER_SPECIFIC_RESPONSE)
		|| (pReplyHeader->F == 0)) {

			DebugPrint(1, ("VendorCommand: BAD Reply Header. %d 0x%x\n", pReplyHeader->Opcode, pReplyHeader->F));
			return -1;
		}

		if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
			DebugPrint(1, ("VendorCommand: Failed.\n"));
			return -1;
		}

		*pParameter = pReplyHeader->VenderParameter;
#if DBG
		DebugPrint(1, ("VendorCommand: After Operation %d, Parameter %I64x\n", cOperation, NTOHLL(*pParameter) ));
#endif

		return 0;
}
//
// Discovery
//
int
Discovery(
		  PLANSCSI_PATH		pPath
		  )
{
	int	iResult = 0, iResult2;
	
	//////////////////////////////////////////////////////////
	//
	// Login Phase...
	//
	if((iResult = Login(pPath, LOGIN_TYPE_DISCOVERY)) != 0) {
		DebugPrint(1, ("[LanScsiOpLib]Discovery: Login Failed... %02x:%02x:%02x:%02x:%02x:%02x\n",
			(int)pPath->address.Node[0],
			(int)pPath->address.Node[1],
			(int)pPath->address.Node[2],
			(int)pPath->address.Node[3],
			(int)pPath->address.Node[4],
			(int)pPath->address.Node[5]));
		return iResult;
	}
	
	if((iResult2 = TextTargetList(pPath)) != 0) {
		DebugPrint(1, ("[LanScsiOpLib]Discovery: Text Failed... %02x:%02x:%02x:%02x:%02x:%02x\n",
			(int)pPath->address.Node[0],
			(int)pPath->address.Node[1],
			(int)pPath->address.Node[2],
			(int)pPath->address.Node[3],
			(int)pPath->address.Node[4],
			(int)pPath->address.Node[5]));
	}

	///////////////////////////////////////////////////////////////
	//
	// Logout Packet.
	//
	if((iResult = Logout(pPath)) != 0) {
		DebugPrint(1, ("[LanScsiOpLib]Discovery: Logout Failed... %02x:%02x:%02x:%02x:%02x:%02x\n",
			(int)pPath->address.Node[0],
			(int)pPath->address.Node[1],
			(int)pPath->address.Node[2],
			(int)pPath->address.Node[3],
			(int)pPath->address.Node[4],
			(int)pPath->address.Node[5]));
		return iResult;
	}
	
	return iResult2;
}

void
ConvertString(
			  PCHAR	result,
			  PCHAR	source,
			  int	size
			  )
{
	int	i;

	for(i = 0; i < size / 2; i++) {
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
		DebugPrint(7, ("!!!! Capacity reversed.... !!!!!!!!\n"));
		return 1;
	}

	return 0;
}

int
GetDiskInfo_V1(
			PLANSCSI_PATH	pPath,
			UINT			TargetId
			)
{
	struct hd_driveid	info;
	int					iResult;
	char				buffer[41];
	unsigned _int8		response;
	BOOL				SetDmaMode;
	DebugPrint(4, ("[LanScsiOpLib]GetDiskInfo_V1: Start\n"));
	
	pPath->PerTarget[TargetId].bLBA = FALSE;
	pPath->PerTarget[TargetId].bLBA48 = FALSE;
	pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_UNKNOWN_DEVICE;
	// identify.
	if(0 == (iResult = IdeCommand(pPath, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info, &response)))
	{
		pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_BLOCK_DEVICE;
		DebugPrint(4, ("[LanScsiOpLib]GetDiskInfo: Block device...\n"));
	}
	else
	{
		DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: WIN_IDENTIFY Failed...\n"));
		if(0 == (iResult = IdeCommand(pPath, TargetId, 0, WIN_PIDENTIFY, 0, 0, 0, (PCHAR)&info, &response)))
		{
			switch((info.config >> 8) & 0x1f) // Bits(12:8)
			{
			case 0x05: // CD-ROM device
				pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_CDROM_DEVICE;
				break;
			case 0x07: // Optical memory device
				pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_OPMEM_DEVICE;
				break;
			case 0x00: // Direct-access device
			case 0x01: // Sequential-access device
			case 0x02: // Printer device
			case 0x03: // Processor device
			case 0x04: // Write-once device
			case 0x06: // Scanner device
			case 0x08: // Medium changer device
			case 0x09: // Communications device
			case 0x0A:
			case 0x0B: // Reserved for ACS IT8 (Graphic arts pre-press devices)
			case 0x0C: // Array controller device
			case 0x0D: // Enclosure services device
			case 0x0E: // Reduced block command devices
			case 0x0F: // Optical card reader/writer device
			case 0x1F: // Unknown or no device type
			default:
				pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_UNKNOWN_DEVICE;
				break;
			}
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: Packet device...\n"));
		}
		else
		{
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: WIN_PIDENTIFY Failed...\n"));

			return iResult;
		}
	}
	
	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: Before Set Feature DMA 0x%x, U-DMA 0x%x\n", 
		info.dma_mword, 
		info.dma_ultra));
	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
		TargetId, info.major_rev_num, info.minor_rev_num, info.capability)
		);

	//
	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions and disk capacity.
	//
	do {
		UCHAR	DmaFeature;
		UCHAR	DmaMode;

		SetDmaMode = FALSE;
		DmaFeature = 0;
		DmaMode = 0;

		//
		// Ultra DMA if NDAS chip is 2.0 or higher.
		//
		if((pPath->HWVersion >= LANSCSIIDE_VERSION_2_0) 
			&& (info.dma_ultra & 0x00ff)
//			&&(pPath->PerTarget[TargetId].MediaType != MEDIA_TYPE_CDROM_DEVICE)
			) {
			// Find Fastest Mode.
			if(info.dma_ultra & 0x0001)
				DmaMode = 0;
			if(info.dma_ultra & 0x0002)
				DmaMode = 1;
			if(info.dma_ultra & 0x0004)
				DmaMode = 2;
			//	if Cable80, try higher Ultra Dma Mode.
#ifdef __NDASCHIP20_DETECT_CABLE80__
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
#ifdef __NDASCHIP20_DETECT_CABLE80__
			} else {
				DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: IOMode: Could not detect Cable80\n"));
			}
#endif
	
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: IOMode: Ultra DMA mode %d detected.\n", (int)DmaMode));


			// Set DMA mode if needed
			if(!(info.dma_ultra & (0x0100 << DmaMode))) {
				DmaFeature = DmaMode | 0x40;	// Ultra DMA mode.
				SetDmaMode = TRUE;
			}
			pPath->PerTarget[TargetId].bPIO = FALSE;
			pPath->PerTarget[TargetId].bDma = TRUE;
			pPath->PerTarget[TargetId].bUDma = TRUE;

		//
		// DMA
		//
		} else if(info.dma_mword & 0x00ff) {

			if(info.dma_mword & 0x0001)
				DmaMode = 0;
			if(info.dma_mword & 0x0002)
				DmaMode = 1;
			if(info.dma_mword & 0x0004)
				DmaMode = 2;

			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: IOMode: DMA mode %d detected.\n", (int)DmaMode));
			pPath->PerTarget[TargetId].bPIO = FALSE;
			pPath->PerTarget[TargetId].bUDma = FALSE;
			pPath->PerTarget[TargetId].bDma = TRUE;
			// Set DMA mode if needed.
			if(!(info.dma_mword & (0x0100 << DmaMode))) {
				DmaFeature = DmaMode | 0x20;	// DMA mode.
				SetDmaMode = TRUE;
			}

		}

		//
		//	Set DMA mode if needed.
		//
		if(SetDmaMode) {

			if((iResult = IdeCommand(pPath, TargetId, 0, WIN_SETFEATURES, 0, DmaFeature, 0x3, (PCHAR)&info, &response)) != 0) {
				DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: Set Feature Failed...\n"));
				return iResult;
			}

			// identify again.
			if((iResult = IdeCommand(pPath, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info, &response)) != 0) {
				DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: Identify Failed...\n"));
				return iResult;
			}
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: After Set Feature DMA 0x%x, U-DMA 0x%x\n", 
							info.dma_mword, 
							info.dma_ultra));
		}
		if(pPath->PerTarget[TargetId].bDma) {
			break;
		}

		//
		//	PIO
		//
		pPath->PerTarget[TargetId].bPIO = TRUE;
		DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: IOMode: PIO detected.\n"));

	} while(0);


	//
	//	Product strings.
	//
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	DebugPrint(7, ("[LanScsiOpLib]GetDiskInfo_V1: Serial No: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].SerialNo, buffer, 20);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	DebugPrint(7, ("[LanScsiOpLib]GetDiskInfo_V1: Firmware rev: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].FwRev, buffer, 8);

	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	DebugPrint(7, ("[LanScsiOpLib]GetDiskInfo_V1: Model No: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].Model, buffer, 40);

	//
	// Support LBA?
	//
	if(info.capability &= 0x02)
		pPath->PerTarget[TargetId].bLBA = TRUE;
	else
		pPath->PerTarget[TargetId].bLBA = FALSE;
	
	//
	// Calc Capacity.
	// 
	if(info.command_set_2 & 0x0400 && info.cfs_enable_2 * 0x0400) {	// Support LBA48bit
		pPath->PerTarget[TargetId].bLBA48 = TRUE;
		pPath->PerTarget[TargetId].SectorCount = info.lba_capacity_2;
	} else {
		pPath->PerTarget[TargetId].bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			pPath->PerTarget[TargetId].SectorCount = info.lba_capacity;
		} else {
			pPath->PerTarget[TargetId].SectorCount = info.cyls * info.heads * info.sectors;	
		}
	}
	
	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1: LBA48 %d, Number of Sectors: %I64d\n", 
		pPath->PerTarget[TargetId].bLBA48, 
		pPath->PerTarget[TargetId].SectorCount)
		);

#ifdef __NDASCHIP20_ALPHA_SUPPORT__

	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo_V1:: NDASCHIP20_ALPHA_SUPPORT enabled.!!! Step2\n"));
	//
	//	Write one sector into 4096th sector form the last sector.
	//
	if(		pPath->HWVersion == LANSCSIIDE_VERSION_2_0 &&
			pPath->PerTarget[TargetId].bUDma &&
			pPath->PerTarget[TargetId].MediaType == MEDIA_TYPE_BLOCK_DEVICE &&
			SetDmaMode
		) {
		ULONG				logicalBlockAddress;
		USHORT				transferBlocks;
		UCHAR				response;
		UCHAR				OneSector[512];

		ZeroMemory(OneSector, 512);
		logicalBlockAddress = (ULONG)pPath->PerTarget[TargetId].SectorCount - 4097;
		transferBlocks = 1;
		iResult = IdeCommand(pPath, TargetId, 0, WIN_WRITE, logicalBlockAddress, 1, 0, OneSector, &response);
		if(iResult != 0 || response != LANSCSI_RESPONSE_SUCCESS) {
			DebugPrint(1, 
				("[LanScsiOpLib]GetDiskInfo_V1:: WIN_WRITE Error: logicalBlockAddress = %x, transferBlocks = %x\n", 
				logicalBlockAddress, transferBlocks));
			return -1;
		} else {
			DebugPrint(1, 
				("[LanScsiOpLib]GetDiskInfo_V1:: WIN_WRITE : wrote a sector to logicalBlockAddress = %x, transferBlocks = %x\n", 
				logicalBlockAddress, transferBlocks));
		}
	}

#endif

	DebugPrint(4, ("[LanScsiOpLib]GetDiskInfo_V1: End\n"));
	
	return 0;
}


int
GetDiskInfo(
			PLANSCSI_PATH	pPath,
			UINT			TargetId
			)
{
	struct hd_driveid	info;
	int					iResult;
	char				buffer[41];
	unsigned _int8		response;
		
	DebugPrint(3, ("[LanScsiOpLib]GetDiskInfo: Entered.\n"));
	pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_UNKNOWN_DEVICE;
	if(pPath->HWProtoVersion == LSIDEPROTO_VERSION_1_1) {	
		return GetDiskInfo_V1(pPath, TargetId) ;
	}	

	// identify.
	if((iResult = IdeCommand(pPath, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info, &response)) != 0) {
		DebugPrint(7, ("[LanScsiOpLib]GetDiskInfo: Identify Failed...\n"));
		return iResult;
	}

	pPath->PerTarget[TargetId].MediaType = MEDIA_TYPE_BLOCK_DEVICE; // V1.0 does not support packet device

	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: DMA 0x%x, U-DMA 0x%x before set-feature.\n", 
			info.dma_mword, 
			info.dma_ultra));

	//
	// DMA Mode.
	//
	if(!(info.dma_mword & 0x0004)) {
		DebugPrint(1, ("[LanScsiOpLib]Not Support DMA mode 2...\n"));
		return -1;
	}

	DebugPrint(2, ("[LanScsiOpLib]GetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
		TargetId, info.major_rev_num, info.minor_rev_num, info.capability)
		);
	
	if(!(info.dma_mword & 0x0400)) {
		// Set to DMA mode 2
		if((iResult = IdeCommand(pPath, TargetId, 0, WIN_SETFEATURES, 0, 0x22, 0x3, (PCHAR)&info, &response)) != 0) {
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: Set Feature Failed...\n"));
			return iResult;
		}
		
		// identify.
		if((iResult = IdeCommand(pPath, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info, &response)) != 0) {
			DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: Identify Failed...\n"));
			return iResult;
		}
		
		DebugPrint(2, ("[LanScsiOpLib]GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
			info.dma_mword, 
			info.dma_ultra));
	}
	
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	DebugPrint(2, ("[LanScsiOpLib]GetDiskInfo: Serial No: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].SerialNo, buffer, 20);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	DebugPrint(2, ("[LanScsiOpLib]GetDiskInfo: Firmware rev: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].FwRev, buffer, 8);

	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	DebugPrint(2, ("[LanScsiOpLib]GetDiskInfo: Model No: %s\n", buffer));
	memcpy(pPath->PerTarget[TargetId].Model, buffer, 40);

	//
	// Support LBA?
	//
	if(info.capability &= 0x02)
		pPath->PerTarget[TargetId].bLBA = TRUE;
	else
		pPath->PerTarget[TargetId].bLBA = FALSE;
	
	//
	// Calc Capacity.
	// 
	if(info.command_set_2 & 0x0400 && info.cfs_enable_2 * 0x0400) {	// Support LBA48bit
		pPath->PerTarget[TargetId].bLBA48 = TRUE;
		pPath->PerTarget[TargetId].SectorCount = info.lba_capacity_2;
	} else {
		pPath->PerTarget[TargetId].bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			pPath->PerTarget[TargetId].SectorCount = info.lba_capacity;
		} else {
			pPath->PerTarget[TargetId].SectorCount = info.cyls * info.heads * info.sectors;	
		}
	}
	
	DebugPrint(1, ("[LanScsiOpLib]GetDiskInfo: LBA48 %d, Number of Sectors: %I64d\n", 
		pPath->PerTarget[TargetId].bLBA48, 
		pPath->PerTarget[TargetId].SectorCount)
		);
	
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
			   IN OUT	PLANSCSI_PATH		pPath
			   )
{
	int						iErrcode;
	SOCKADDR_LPX			socketLpx;
	SOCKADDR_LPX			serverSocketLpx;
	LPSOCKET_ADDRESS_LIST	socketAddressList;
	DWORD					socketAddressListLength;
	HLOCAL					hMemorySocketAddressList;
	int						i;
	SOCKET					sock;

	CopyMemory(&pPath->address, pAddress, sizeof(LPX_ADDRESS));

	socketAddressListLength = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)
		+ sizeof(SOCKET_ADDRESS) * MAX_SOCKETLPX_INTERFACE
		+ sizeof(SOCKADDR_LPX) * MAX_SOCKETLPX_INTERFACE;
	
	hMemorySocketAddressList = LocalAlloc(LMEM_FIXED, socketAddressListLength);
	socketAddressList = LocalLock(hMemorySocketAddressList);
//	socketAddressList = (LPSOCKET_ADDRESS_LIST)malloc(socketAddressListLength);
	
	//
	// Get NICs
	//
	iErrcode = GetInterfaceList(
		socketAddressList,
		socketAddressListLength
		);
	
	if(iErrcode != 0) {
		DebugPrint(1, ("[LanScsiCli]MakeConnection: Error When Get NIC List!!!!!!!!!!\n"));
		
		goto END_FUNCTION;
	} else {
		DebugPrint(4, ("[LanScsiCli]MakeConnection: Number of NICs : %d\n", socketAddressList->iAddressCount));
	}
	
	//
	// Find NIC that is connected to LanDisk.
	//
	for(i = 0; i < socketAddressList->iAddressCount; i++) {
		
		socketLpx = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);
		
		DebugPrint(4, ("[LanScsiCli]MakeConnection: NIC %02d: Address %02X:%02X:%02X:%02X:%02X;%02X\n",
			i,
			socketLpx.LpxAddress.Node[0],
			socketLpx.LpxAddress.Node[1],
			socketLpx.LpxAddress.Node[2],
			socketLpx.LpxAddress.Node[3],
			socketLpx.LpxAddress.Node[4],
			socketLpx.LpxAddress.Node[5])
			);
		
		sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		if(sock == INVALID_SOCKET) {
			PrintError(WSAGetLastError(), "MakeConnection: socket ");
			goto END_FUNCTION;
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
			
			DebugPrint(1, ("[LanScsiCli]MakeConnection: LanDisk is not connected with NIC Number %d\n", i));
			
			continue;
		} else {
			pPath->connsock = sock;

			DebugPrint(3, ("[LanScsiCli]MakeConnection: LanDisk is connected with NIC Number %d\n", i));
			
			break;
		}
	}

	if(sock == INVALID_SOCKET) {
		DebugPrint(1, ("[LanScsiCli]MakeConnection: No LanDisk (%02X:%02X:%02X:%02X:%02X:%02X)!!!\n",
			(int)pAddress->Node[0],
			(int)pAddress->Node[1],
			(int)pAddress->Node[2],
			(int)pAddress->Node[3],
			(int)pAddress->Node[4],
			(int)pAddress->Node[5]
			));
		
		goto END_FUNCTION;
	}
	
END_FUNCTION :
	LocalUnlock(hMemorySocketAddressList);
	LocalFree(hMemorySocketAddressList);
//	free(socketAddressList);
	return TRUE;
}
