#include "stdafx.h"
#include <ndas/ndasid.h>

//////////////////////////////////////////////////////////////////////////
//
//	Encryption keys for NetDisk ID and write key.
//
const static BYTE NDIDV1Key1[8] = {0x45,0x32,0x56,0x2f,0xec,0x4a,0x38,0x53};
const static BYTE NDIDV1Key2[8] = {0x1e,0x4e,0x0f,0xeb,0x33,0x27,0x50,0xc1};
const static BYTE NDIDV1VID		= 0x01;
const static BYTE NDIDV1Reserved[2] = { 0xff, 0xff };
const static BYTE NDIDV1Random	= 0xcd;


//////////////////////////////////////////////////////////////////////////
//
//	Transport support
//
#define	_LPX_

#define	NR_MAX_TARGET			2
#define	MAX_DATA_BUFFER_SIZE	64 * 1024
#define BLOCK_SIZE				512
#define	MAX_TRANSFER_SIZE		(64 * 1024)
#define MAX_TRANSFER_BLOCKS		 MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC			(LONGLONG)(1000)
#define TIME_OUT	(SEC * 30*2*5)					// 5 min.

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
PrintErrorCode(
			   PTCHAR	prefix,
			   int		ErrorCode
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
	_ftprintf(stderr, _T("%s: %s"), prefix, (LPCSTR)lpMsgBuf);

	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}


//////////////////////////////////////////////////////////////////////////
//
//	Transport support
//
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

					PrintErrorCode(_T("RecvIt: "), dwError);
					dwRecvDataLen = -1;

					printf("RecvIt: Request %d, Received %d\n",
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
					PrintErrorCode(_T("RecvIt: GetOverlappedResult Failed "), GetLastError());
					dwRecvDataLen = SOCKET_ERROR;
					goto Out;
				}

			} else {
				PrintErrorCode(_T("RecvIt: WSARecv Failed "), dwError);

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
#if DBG
			PrintErrorCode(_T("SendIt"), WSAGetLastError());
#endif
			return res;
		}
		len -= res;
		buf += res;
	}

	return size;
}

//////////////////////////////////////////////////////////////////////////
//
//	Lanscsi protocol support
//
int
ReadReply(
		  SOCKET			connSock,
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
		sizeof(LANSCSI_H2R_PDU_HEADER)
		);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("ReadRequest: Can't Recv Header...\n"));

		return iResult;
	} else if(iResult == 0) {
		_ftprintf(stderr, _T("ReadRequest: Disconnected...\n"));

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
			//_ftprintf(stderr, _T("ReadRequest: Decrypt Header 1 !!!!!!!!!!!!!!!...\n"));
	}

	// Read AHS.
	if(ntohs(pPdu->pH2RHeader->AHSLen) > 0) {
		iResult = RecvIt(
			connSock,
				pPtr,
			ntohs(pPdu->pH2RHeader->AHSLen)
			);
		if(iResult == SOCKET_ERROR) {
			_ftprintf(stderr, _T("ReadRequest: Can't Recv AHS...\n"));

			return iResult;
		} else if(iResult == 0) {
			_ftprintf(stderr, _T("ReadRequest: Disconnected...\n"));

			return iResult;
		} else
			iTotalRecved += iResult;

		pPdu->pDataSeg = pPtr;

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
	}

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
			//&& DataEncryptAlgo != 0) {	//by limbear
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&CHAP_C,
				(unsigned char *)&iPassword
				);
			//_ftprintf(stderr, _T("ReadRequest: Decrypt Header 2 !!!!!!!!!!!!!!!...\n"));
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
			_ftprintf(stderr, _T("ReadRequest: Can't Recv Data segment...\n"));

			return iResult;
		} else if(iResult == 0) {
			_ftprintf(stderr, _T("ReadRequest: Disconnected...\n"));

			return iResult;
		} else
			iTotalRecved += iResult;

		pPdu->pDataSeg = pPtr;

		pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);


		if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
			&& HeaderEncryptAlgo != 0) {
			//&& DataEncryptAlgo != 0) {	//by limbear

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
			PLANSCSI_PDU_POINTERS	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;
	// changed by ILGU 2003_0819
	//	old
	//	iDataSegLen = ntohl(pHeader->DataSegLen);
	//	new
	iDataSegLen = ntohs(pHeader->AHSLen);
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
			//_ftprintf(stderr, _T("SendRequest: Encrypt Header 1 !!!!!!!!!!!!!!!...\n"));
	}

	//
	// Encrypt Data.
	//
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		//&& DataEncryptAlgo != 0	by limbear
		&& HeaderEncryptAlgo != 0
		&& iDataSegLen > 0) {

		Encrypt32(
				(unsigned char*)pPdu->pDataSeg,
				iDataSegLen,
				(unsigned char *)&CHAP_C,
				(unsigned char*)&iPassword
				);
			//_ftprintf(stderr, _T("SendRequest: Encrypt Data 2 !!!!!!!!!!!!!!!...\n"));
	}

	// Send Request.
	iResult = SendIt(
		connSock,
		(PCHAR)pHeader,
		sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
		);
	if(iResult == SOCKET_ERROR) {
#if DBG
		PrintErrorCode(_T("SendRequest"), WSAGetLastError());
#endif
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
	LANSCSI_PDU_POINTERS							pdu;
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

	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(HPID);
	//	changed by ilgu 2003_0819
	//	old
	//	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	//	new
	pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons((u_short)iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	//	changed by ilgu 2003_0819
	//	old
	//	pLoginRequestPdu->VerMax = 0;
	//	new
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
#if DBG
		PrintErrorCode(_T("login: Sending First Request "), WSAGetLastError());
#endif
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("login: First Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
		//	changed by ILGU 2003_0819
		//	old
		//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		//	new
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION )
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		_ftprintf(stderr, _T("login: BAD First Reply Header.\n"));
		return -1;
	}

	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("login: First Failed.\n"));
		return -1;
	}

	// Store Data.
	RPID = ntohs(pLoginReplyHeader->RPID);

	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
#if DBG
	_ftprintf(stderr, _T("login: Version %d Auth %d\n"),
			pLoginReplyHeader->VerActive,
			ntohs(pParamSecu->AuthMethod)
			);
#endif
	if(pLoginReplyHeader->VerActive < LANSCSIIDE_VERSION_1_1) {
		_ftprintf(stderr, _T("Failed to log in. Hardware is an old version.\n"));
		return -2;
	}

	//
	// Second Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;

	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	//	changed by ILGU 2003_0819
	//	old
	//	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	//	new
	pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;

	//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = 1;
	pLoginRequestPdu->VerMin = 0;

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
		PrintErrorCode(_T("Login: Send Second Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("login: Second Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
		//	changed by ILGU 2003_0819
		//	old
		//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
		//	new
		|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		_ftprintf(stderr, _T("login: BAD Second Reply Header.\n"));
		return -1;
	}

	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("login: Second Failed.\n"));
		return -1;
	}

	// Check Data segment.
	//	changed by ILGU 2003_0819
	//	old
	//	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
	//	new
	if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
		|| (pdu.pDataSeg == NULL)) {

			_ftprintf(stderr, _T("login: BAD Second Reply Data.\n"));
			return -1;
	}
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		//|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)
		//|| pParamSecu->AuthMethod != htons(0)
		|| pParamSecu->LoginType != cLoginType) {

			_ftprintf(stderr, _T("login: BAD Second Reply Parameters.\n"));
			return -1;
	}

	// Store Challenge.
	pParamChap = &pParamSecu->ChapParam;
	CHAP_I = ntohl(pParamChap->CHAP_I);
	CHAP_C = ntohl(pParamChap->CHAP_C[0]);

#if DBG
	printf("login: Hash %d, Challenge %d\n",
					ntohl(pParamChap->CHAP_A),
					CHAP_C
					);
#endif

	//
	// Third Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_SECURITY_PHASE;
	pLoginRequestPdu->NSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	//	changed by ILGU 2003_0819
	//	old
	//	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	//	new
	pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;

	//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = 1;
	pLoginRequestPdu->VerMin = 0;
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
		PrintErrorCode(_T("Login: Send Third Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("login: Second Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
			|| (pLoginReplyHeader->T == 0)
			|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
			|| (pLoginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
			//	changed by ILGU 2003_0819
			//	old
			//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
			//	new
			|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
			|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
			|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		_ftprintf(stderr, _T("login: BAD Third Reply Header.\n"));
		return -1;
	}

	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("login: Third Failed. RESPONSE: %x\n"), pLoginReplyHeader->Response);
		return -1;
	}

	// Check Data segment.
	//	changed by ILGU 2003_0819
	//	old
	//	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
	//	new
	if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
		|| (pdu.pDataSeg == NULL)) {

		_ftprintf(stderr, _T("login: BAD Third Reply Data.\n"));
		return -1;
	}
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
			//|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)
			//|| pParamSecu->AuthMethod != htons(0)
			|| pParamSecu->LoginType != cLoginType){

		_ftprintf(stderr, _T("login: BAD Third Reply Parameters.\n"));
		return -1;
	}

	iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;

	//
	// Fourth Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->T = 1;
	pLoginRequestPdu->CSG = FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->NSG = FLAG_FULL_FEATURE_PHASE;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	//	changed by ILGU 2003_0819
	//	old
	//	pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	//	new
	pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	pLoginRequestPdu->CSubPacketSeq = htons(++iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
	//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = 1;
	pLoginRequestPdu->VerMin = 0;
	pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];

	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamNego;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintErrorCode( _T("Login: Send Fourth Request "), WSAGetLastError());
		return -1;
	}

	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("login: Fourth Can't Read Reply.\n"));
		return -1;
	}

	// Check Reply Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
			|| (pLoginReplyHeader->T == 0)
			|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
			|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
			//	changed by ILGU 2003_0819
			//	old
			//		|| (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
			//	new
			|| (pLoginReplyHeader->VerActive > LANSCSIIDE_CURRENT_VERSION)
			|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
			|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

		_ftprintf(stderr, _T("login: BAD Fourth Reply Header.\n"));
		return -1;
	}

	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("login: Fourth Failed.\n"));
		return -1;
	}

	// Check Data segment.
	//	changed by ILGU 2003_0819
	//	old
	//	if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
	//	new
	if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
						|| (pdu.pDataSeg == NULL)) {

		_ftprintf(stderr, _T("login: BAD Fourth Reply Data.\n"));
		return -1;
	}
	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
		_ftprintf(stderr, _T("login: BAD Fourth Reply Parameters.\n"));
		return -1;
	}

#if DBG
	printf("login: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n",
			pParamNego->HWType, pParamNego->HWVersion,
			ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
			ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID)
		);
	printf("login: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
			ntohs(pParamNego->HeaderEncryptAlgo),
			ntohs(pParamNego->HeaderDigestAlgo),
			ntohs(pParamNego->DataEncryptAlgo),
			ntohs(pParamNego->DataDigestAlgo)
		);
#endif
	requestBlocks = ntohl(pParamNego->MaxBlocks);

#if 1 // limbear book mark
	HeaderEncryptAlgo = ntohs(pParamNego->HeaderEncryptAlgo);
	DataEncryptAlgo = ntohs(pParamNego->DataEncryptAlgo);

#else
	HeaderEncryptAlgo = 0;
	DataEncryptAlgo = 0;
#endif
#if DBG
	_ftprintf(stderr, _T("HeaderEncryptAlgo = 0x%x, DataEncryptAlgo = 0x%x\n"),
							HeaderEncryptAlgo, DataEncryptAlgo);
#endif
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
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	//	changed by ILGU 2003_0819
	//	old
	//	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	//	pRequestHeader->AHSLen = 0;
	//	new
	pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
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
		PrintErrorCode(_T("TextTargetList: Send First Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("TextTargetList: Can't Read Reply.\n"));
		return -1;
	}
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;


	// Check Request Header.
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

			_ftprintf(stderr, _T("TextTargetList: BAD Reply Header.\n"));
			return -1;
		}

		if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
			_ftprintf(stderr, _T("TextTargetList: Failed.\n"));
			return -1;
		}
		//	changed by ILGU 2003_0819
		//	old
		//	if(ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
		//	new
		if(ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
			_ftprintf(stderr, _T("TextTargetList: No Data Segment.\n"));
			return -1;
		}

		pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
		if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
			_ftprintf(stderr, _T("TEXT: Bad Parameter Type.: %d\n"),pParam->ParamType);
			return -1;
		}
#if DBG
		_ftprintf(stderr, _T("TextTargetList: NR Targets : %d\n"), pParam->NRTarget);
#endif
		NRTarget = pParam->NRTarget;

		for(int i = 0; i < pParam->NRTarget; i++) {
			PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
			int								iTargetId;

			pTarget = &pParam->PerTarget[i];
			iTargetId = ntohl(pTarget->TargetID);

#if DBG
			_ftprintf(stderr, _T("TextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: %I64d \n"),
				ntohl(pTarget->TargetID),
				pTarget->NRRWHost,
				pTarget->NRROHost,
				pTarget->TargetData
				);
#endif

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
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = TEXT_REQUEST;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	//	changed by ILGU 2003_0819
	//	old
	//	pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	//	pRequestHeader->AHSLen = 0;
	//	new
	pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
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
		PrintErrorCode(_T("TextTargetData: Send First Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("TextTargetData: Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {

			_ftprintf(stderr, _T("TextTargetData: BAD Reply Header.\n"));
			return -1;
		}

		if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
			_ftprintf(stderr, _T("TextTargetData: Failed.\n"));
			return -1;
		}
		//	changed by ILGU 2003_0819
		//	old
		//	if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		//	new
		if(ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
			_ftprintf(stderr, _T("TextTargetData: No Data Segment.\n"));
			return -1;
		}
		pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;

		if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
			_ftprintf(stderr, _T("TextTargetData: Bad Parameter Type. %d\n"), pParam->ParamType);
			//	return -1;
		}

		PerTarget[TargetID].TargetData = pParam->TargetData;

		printf("TextTargetList: TargetID : %d, GetorSet %d, Target Data %d\n",
			ntohl(pParam->TargetID), pParam->GetOrSet, PerTarget[TargetID].TargetData);

		return 0;
}

int
VendorCommand(
			  SOCKET			connsock,
			  UCHAR				cOperation,
			  unsigned _int64	*pParameter
			  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = VENDOR_SPECIFIC_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->VendorID = ntohs(NKC_VENDOR_ID);
	pRequestHeader->VendorOpVersion = VENDOR_OP_CURRENT_VERSION;
	pRequestHeader->VendorOp = cOperation;
	pRequestHeader->VendorParameter = *pParameter;

#if DBG
	_ftprintf(stderr, _T("VendorCommand: Operation %d, Parameter %I64x\n"), cOperation, NTOHLL(*pParameter));
#endif
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintErrorCode(_T("VendorCommand: Send First Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("VendorCommand: Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE)
		|| (pReplyHeader->F == 0)) {

			_ftprintf(stderr, _T("VendorCommand: BAD Reply Header. %d 0x%x\n"), pReplyHeader->Opcode, pReplyHeader->F);
			return -1;
		}

		if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
			_ftprintf(stderr, _T("VendorCommand: Failed.\n"));
			//exit(0);
			return -1;
		}

		*pParameter = pReplyHeader->VendorParameter;
#if DBG
		_ftprintf(stderr, _T("VendorCommand: After Operation %d, Parameter %I64x\n"), cOperation, NTOHLL(*pParameter));
#endif

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
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);

	pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = LOGOUT_REQUEST;
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

		PrintErrorCode(_T("Logout: Send Request "), WSAGetLastError());
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("Logout: Can't Read Reply.\n"));
		return -1;
	}

	// Check Request Header.
	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if((pReplyHeader->Opcode != LOGOUT_RESPONSE)
		|| (pReplyHeader->F == 0)) {

			_ftprintf(stderr, _T("Logout: BAD Reply Header.\n"));
			return -1;
	}

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("Logout: Failed.\n"));
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
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;
	unsigned _int8						iCommandReg;

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

	switch(Command) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;

#if 0
			if(PerTarget[TargetId].bLBA48 == TRUE) {
				pRequestHeader->Command = WIN_READDMA_EXT;
				pRequestHeader->COM_TYPE_E = '1';
			} else {
				pRequestHeader->Command = WIN_READDMA;
			}
			pRequestHeader->COM_TYPE_D_P = '1';
#else
			if(PerTarget[TargetId].bLBA48 == TRUE) {
				pRequestHeader->Command = 0x24;
				pRequestHeader->COM_TYPE_E = '1';
			} else {
				pRequestHeader->Command = 0x20;
			}
			pRequestHeader->COM_TYPE_D_P = '0';
#endif


			pRequestHeader->COM_TYPE_R = '1';
			pRequestHeader->COM_LENG = (htonl(SectorCount*512) >> 8);
		}
		break;
	case WIN_WRITE:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 1;

#if 0
			if(PerTarget[TargetId].bLBA48 == TRUE) {
				pRequestHeader->Command = WIN_WRITEDMA_EXT;
				pRequestHeader->COM_TYPE_E = '1';
			} else {
				pRequestHeader->Command = WIN_WRITEDMA;
			}
			pRequestHeader->COM_TYPE_D_P = '1';
#else
			if(PerTarget[TargetId].bLBA48 == TRUE) {
				pRequestHeader->Command = 0x34;
				pRequestHeader->COM_TYPE_E = '1';
			} else {
				pRequestHeader->Command = 0x30;
			}
			pRequestHeader->COM_TYPE_D_P = '0';
#endif
			pRequestHeader->COM_TYPE_W = '1';
			pRequestHeader->COM_LENG = (htonl(SectorCount*512) >> 8);
		}
		break;
	case WIN_VERIFY:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;

			if(PerTarget[TargetId].bLBA48 == TRUE) {
				pRequestHeader->Command = WIN_VERIFY_EXT;
				pRequestHeader->COM_TYPE_E = '1';
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

			pRequestHeader->Command = Command;
			//pRequestHeader->Command = 0xa1;

			pRequestHeader->COM_TYPE_R = '1';
			pRequestHeader->COM_LENG = (htonl(1*512) >> 8);
		}
		break;
	case WIN_STANDBYNOW1:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;

			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_STANDBYNOW1;

			_ftprintf(stderr, _T("IDECommand: WIN_STANDBYNOW1\n"));
		}
		break;
	case WIN_SETFEATURES:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;

			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr = Feature;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)SectorCount;
			pRequestHeader->Command = WIN_SETFEATURES;

			_ftprintf(stderr, _T("IDECommand: SET Features Sector Count 0x%x\n"), pRequestHeader->SectorCount_Curr);
		}
		break;
	case WIN_SETMULT:
		{
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
	default:
		_ftprintf(stderr, _T("IDECommand: Not Supported IDE Command.\n"));
		return -1;
	}

	if((Command == WIN_READ)
		|| (Command == WIN_WRITE)
		|| (Command == WIN_VERIFY)){

			if(PerTarget[TargetId].bLBA == FALSE) {
				_ftprintf(stderr, _T("IDECommand: CHS not supported...\n"));
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
			PrintErrorCode(_T("IdeCommand: Send Request "), WSAGetLastError());
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
				//_ftprintf(stderr, _T("IdeCommand: WIN_WRITE Encrypt data 1 !!!!!!!!!!!!!!!...\n"));

			}

			iResult = SendIt(
				connsock,
				pData,
				SectorCount * 512
				);
			if(iResult == SOCKET_ERROR) {
				PrintErrorCode(_T("IdeCommand: Send data for WRITE "), WSAGetLastError());
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
				PrintErrorCode(_T("IdeCommand: Receive Data for READ "), WSAGetLastError());
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
				//_ftprintf(stderr, _T("IdeCommand: WIN_READ Encrypt data 1 !!!!!!!!!!!!!!!...\n"));
			}


		}
		break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{

			iResult = RecvIt(
				connsock,
				pData,
				512
				);
			if(iResult <= 0) {
				PrintErrorCode(_T("IdeCommand: Receive Data for IDENTIFY "), WSAGetLastError());
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
				//_ftprintf(stderr, _T("IdeCommand: WIN_IDENTIFY Encrypt data 1 !!!!!!!!!!!!!!!...\n"));
			}

		}
		break;
	default:
		break;
	}

	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("IDECommand: Can't Read Reply.\n"));
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		_ftprintf(stderr, _T("IDECommand: Time out...\n"));
		return WAIT_TIMEOUT;
	}

	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;
	if(pReplyHeader->Opcode != IDE_RESPONSE){
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header pReplyHeader->Opcode != IDE_RESPONSE . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	if(pReplyHeader->F == 0){
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header pReplyHeader->F == 0 . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	if(pReplyHeader->Command != iCommandReg) {
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header pReplyHeader->Command != iCommandReg . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			);
		_ftprintf(stderr, _T("Error register = 0x%x\n"), pReplyHeader->Feature_Curr);

		return -1;
	}

	if(Command == WIN_CHECKPOWERMODE1){
		printf("Check Power mode = 0x%02x\n", (unsigned char)(pReplyHeader->SectorCount_Curr));
	}
	return 0;
}


int
PacketCommand(
			  SOCKET	connsock,
			  _int32	TargetId,
			  _int64	LUN,
			  UCHAR	Command,
			  _int64	Location,
			  _int16	SectorCount,
			  _int8	Feature,
			  PCHAR	pData,
			  int index
			  )
{
	char								data2[1024];
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	int									iResult;
	unsigned _int8						iCommandReg;
	PPACKET_COMMAND						pPCommand;
	int additional;
	int read = 0;
	int write = 0;

	int xxx;

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
	//	changed by ILGU	2003_0819
	//	old
	//	pRequestHeader->DataSegLen = htonl(sizeof(PACKET_COMMAND));
	//	pRequestHeader->AHSLen = 0;
	//	new
	pRequestHeader->AHSLen = htons(sizeof(PACKET_COMMAND));
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->TargetID = htonl(TargetId);
	pRequestHeader->LUN = 0;
	// Using Target ID. LUN is always 0.
	pRequestHeader->DEV = TargetId;


	pPCommand = (PPACKET_COMMAND)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];

	/* set command */
	// set IDE registers


	pRequestHeader->R = 0;
	pRequestHeader->W = 0;


	pRequestHeader->SectorCount_Curr = 0x00;

	pRequestHeader->Command = 0xa0;



	/*
	pRequestHeader->Command = 0xa1;
	additional = 512;
	*/
	// set packet command
	/*open*
	pPCommand->Command[0] = 0x1b;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x02;
	pPCommand->Command[5] = 0x00;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x00;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	additional = 0;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;
	pRequestHeader->LBAHigh_Curr = 0x00;
	/**/
	/*close*
	pPCommand->Command[0] = 0x1b;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x03;
	pPCommand->Command[5] = 0x00;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x00;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	additional = 0;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;
	pRequestHeader->LBAHigh_Curr = 0x00;
	/**/

	/* Read PIO*
	pPCommand->Command[0] = 0x43;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x00;
	pPCommand->Command[5] = 0x00;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x04;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 24;
	read = 1;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x18;
	pRequestHeader->LBAHigh_Curr = 0x00;
	/**/



	//#if 1
	/* Write DMA*/
	if(index == 1)
	{
		/*
		int x;
		x = 31;
		pPCommand->Command[0] = 0x2a;
		pPCommand->Command[1] = 0x00;
		pPCommand->Command[2] = 0x00;
		pPCommand->Command[3] = 0x00;
		pPCommand->Command[4] = 0x01;
		pPCommand->Command[5] = 0x09;
		pPCommand->Command[6] = 0x00;
		pPCommand->Command[7] = 0x00;
		//pPCommand->Command[8] = 0x1f;
		pPCommand->Command[8] = (char)x;
		pPCommand->Command[9] = 0x00;
		pPCommand->Command[10] = 0x00;
		pPCommand->Command[11] = 0x00;

		pRequestHeader->COM_TYPE_P = '1';
		pRequestHeader->COM_TYPE_D_P = '1';
		//pRequestHeader->COM_TYPE_D_P = '0';
		pRequestHeader->COM_TYPE_W = '1';
		//additional = 31*2048;
		additional = x*2048;
		write = 1;

		pRequestHeader->Feature_Prev = 0;
		//pRequestHeader->Feature_Curr = 0x00;
		pRequestHeader->Feature_Curr = 0x01;
		pRequestHeader->LBALow_Curr = 0x00;
		pRequestHeader->LBAMid_Curr = 0x00;
		//pRequestHeader->LBAHigh_Curr = 0xf8;
		pRequestHeader->LBAHigh_Curr = (x*2048) >> 8;
		*/
		pPCommand->Command[0] = 0xa3;
		pPCommand->Command[1] = 0x00;
		pPCommand->Command[2] = 0x00;
		pPCommand->Command[3] = 0x00;
		pPCommand->Command[4] = 0x00;
		pPCommand->Command[5] = 0x00;
		pPCommand->Command[6] = 0x00;
		pPCommand->Command[7] = 0x00;
		pPCommand->Command[8] = 0x00;
		pPCommand->Command[9] = 0x10;
		pPCommand->Command[10] = 0xc1;
		pPCommand->Command[11] = 0x00;

		data2[0] = 0x00;
		data2[1] = 0x0e;
		data2[2] = 0x00;
		data2[3] = 0x00;
		data2[4] = 0x09;
		data2[5] = 0x08;
		data2[6] = 0x07;
		data2[7] = 0x06;
		data2[8] = 0x05;
		data2[9] = 0x04;
		data2[10] = 0x03;
		data2[11] = 0x02;
		data2[12] = 0x01;
		data2[13] = 0x00;
		data2[14] = 0x00;
		data2[15] = 0x00;

		pRequestHeader->COM_TYPE_P = '1';
		pRequestHeader->COM_TYPE_D_P = '0';
		pRequestHeader->COM_TYPE_W = '1';

		additional = 16;
		write = 1;

		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr = 0x00;
		pRequestHeader->LBALow_Curr = 0x00;
		pRequestHeader->LBAMid_Curr = 0x10;
		pRequestHeader->LBAHigh_Curr = 0x00;
	}
	/**/
	//#else
	/* Read PIO*
	else{
	pPCommand->Command[0] = 0x5c;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x00;
	pPCommand->Command[5] = 0x00;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x0c;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 12;
	read = 1;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x0c;
	pRequestHeader->LBAHigh_Curr = 0x00;
	}
	/**/
	//#endif




	/* Read DMA*
	pPCommand->Command[0] = 0x28;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x00;
	pPCommand->Command[5] = 0xaf;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x01;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '1';
	pRequestHeader->COM_TYPE_R = '1';
	additional = 2048;

	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x01;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;
	pRequestHeader->LBAHigh_Curr = 0x80;
	/**/

	/* Read PIO*
	pPCommand->Command[0] = 0x28;
	pPCommand->Command[1] = 0x00;
	pPCommand->Command[2] = 0x00;
	pPCommand->Command[3] = 0x00;
	pPCommand->Command[4] = 0x00;
	pPCommand->Command[5] = 0xaf;
	pPCommand->Command[6] = 0x00;
	pPCommand->Command[7] = 0x00;
	pPCommand->Command[8] = 0x01;
	pPCommand->Command[9] = 0x00;
	pPCommand->Command[10] = 0x00;
	pPCommand->Command[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';
	additional = 2048;

	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr = 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;
	pRequestHeader->LBAHigh_Curr = 0x80;
	/**/


	// READ KEY
	else if(2){

		pPCommand->Command[0] = 0xa4;
		pPCommand->Command[1] = 0x00;
		pPCommand->Command[2] = 0x00;
		pPCommand->Command[3] = 0x00;
		pPCommand->Command[4] = 0x00;
		pPCommand->Command[5] = 0x00;
		pPCommand->Command[6] = 0x00;
		pPCommand->Command[7] = 0x00;
		pPCommand->Command[8] = 0x00;
		pPCommand->Command[9] = 0x0c;
		pPCommand->Command[10] = 0xc2;
		pPCommand->Command[11] = 0x00;

		pRequestHeader->COM_TYPE_P = '1';
		pRequestHeader->COM_TYPE_D_P = '0';
		pRequestHeader->COM_TYPE_R = '1';

		additional = 12;
		read = 1;

		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr = 0x00;
		pRequestHeader->LBALow_Curr = 0x00;
		pRequestHeader->LBAMid_Curr = 0x0c;
		pRequestHeader->LBAHigh_Curr = 0x00;

	}else if(3){
		pPCommand->Command[0] = 0xa4;
		pPCommand->Command[1] = 0x00;
		pPCommand->Command[2] = 0x00;
		pPCommand->Command[3] = 0x00;
		pPCommand->Command[4] = 0x00;
		pPCommand->Command[5] = 0x00;
		pPCommand->Command[6] = 0x00;
		pPCommand->Command[7] = 0x00;
		pPCommand->Command[8] = 0x00;
		pPCommand->Command[9] = 0x08;
		pPCommand->Command[10] = 0x00;
		pPCommand->Command[11] = 0x00;

		pRequestHeader->COM_TYPE_P = '1';
		pRequestHeader->COM_TYPE_D_P = '0';
		pRequestHeader->COM_TYPE_R = '1';

		additional = 8;
		read = 1;
		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr = 0x00;
		pRequestHeader->LBALow_Curr = 0x00;
		pRequestHeader->LBAMid_Curr = 0x08;
		pRequestHeader->LBAHigh_Curr = 0x00;
	}else if(4){
		pPCommand->Command[0] = 0xa4;
		pPCommand->Command[1] = 0x00;
		pPCommand->Command[2] = 0x00;
		pPCommand->Command[3] = 0x00;
		pPCommand->Command[4] = 0x00;
		pPCommand->Command[5] = 0x00;
		pPCommand->Command[6] = 0x00;
		pPCommand->Command[7] = 0x00;
		pPCommand->Command[8] = 0x00;
		pPCommand->Command[9] = 0x08;
		pPCommand->Command[10] = 0x05;
		pPCommand->Command[11] = 0x00;

		pRequestHeader->COM_TYPE_P = '1';
		pRequestHeader->COM_TYPE_D_P = '0';
		pRequestHeader->COM_TYPE_R = '1';

		additional = 8;
		read = 1;
		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr = 0x00;
		pRequestHeader->LBALow_Curr = 0x00;
		pRequestHeader->LBAMid_Curr = 0x08;
		pRequestHeader->LBAHigh_Curr = 0x00;
	}



	/* set command end */
	pRequestHeader->COM_LENG = (htonl(additional) >> 8);

	// Backup Command.
	iCommandReg = pRequestHeader->Command;

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg = (char *)pPCommand;

	xxx = clock();
	if(SendRequest(connsock, &pdu) != 0) {
		PrintErrorCode(_T("IdeCommand: Send Request "), WSAGetLastError());
		return -1;
	}

	if((additional > 0) && (write)){
		//char pData[64*1024];

		iResult = SendIt(
			connsock,
			data2,
			additional
			);
		if(iResult == SOCKET_ERROR) {
			PrintErrorCode(_T("IdeCommand: Send data for WRITE "), WSAGetLastError());
			return -1;
		}
	}


	// READ additional data
	if((additional > 0) && (read)){
		int i;

		printf("XXXXXXX\n");
		iResult = RecvIt(connsock, pData, additional);
		if(iResult <= 0) {
			PrintErrorCode(_T("PacketCommand: Receive additional data"), WSAGetLastError());
			return -1;
		}
		for(i = 0 ; i < additional ; i++){
			printf("%02x :" , (unsigned char)((char*)pData)[i]);
			//printf("%c : " , (unsigned char)((char*)pData)[i]);
			if(!((i+1) % 16)){
				printf("\n");
			}
			if(!((i+1) % 2)){
				//printf("%02x" , (unsigned char)((char*)pData)[i-1]);
				//printf("\n");
			}
			else{
				//printf("%d : ", i/2);
				//printf("%02x" , (unsigned char)((char*)pData)[i+1]);
			}
		}
		printf("\n");
	}

	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("IDECommand: Can't Read Reply.\n"));
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		_ftprintf(stderr, _T("IDECommand: Time out...\n"));
		return WAIT_TIMEOUT;
	}

	xxx = clock() - xxx;
	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;

	//printf("path command tag %0x\n", ntohl(pReplyHeader->PathCommandTag));

	if(pReplyHeader->Opcode != IDE_RESPONSE){
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header. OP Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header. OP 0x%x\n"), pReplyHeader->Opcode);
		return -1;
	}
	else if(pReplyHeader->F == 0){
		_ftprintf(stderr, _T("IDECommand: BAD Reply Header. F Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	/*
	else if(pReplyHeader->Command != iCommandReg) {

	_ftprintf(stderr, _T("IDECommand: BAD Reply Header. Command Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n"),
	pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
	return -1;
	}
	*/
	printf("time == %d \n", xxx);
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		_ftprintf(stderr, _T("IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n"),
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			);
		_ftprintf(stderr, _T("ErrReg 0x%02x\n"), pReplyHeader->Feature_Curr);
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
		_ftprintf(stderr, _T("!!!! Capacity reversed.... !!!!!!!!\n"));
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
		_ftprintf(stderr, _T("GetDiskInfo: Identify Failed...\n"));
		return iResult;
	}

	//printf("0 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[1]), (unsigned char)(((PCHAR)&info)[0]));
	//printf("2 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[5]), (unsigned char)(((PCHAR)&info)[4]));
	//printf("10 words  0x%c%c\n", (unsigned char)(((PCHAR)&info)[21]), (unsigned char)(((PCHAR)&info)[20]));
	//printf("47 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[95]), (unsigned char)(((PCHAR)&info)[94]));
	printf("49 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[99]), (unsigned char)(((PCHAR)&info)[98]));
	//printf("59 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[119]), (unsigned char)(((PCHAR)&info)[118]));

	//if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETMULT, 0, 0x08, 0, NULL)) != 0) {
	//		_ftprintf(stderr, "GetDiskInfo: Set Feature Failed...\n");
	//		return iResult;
	//}
	printf("47 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[95]), (unsigned char)(((PCHAR)&info)[94]));
	printf("59 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[119]), (unsigned char)(((PCHAR)&info)[118]));

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_CHECKPOWERMODE1, 0, 0, 0, NULL)) != 0) {
		_ftprintf(stderr, _T("GetDiskInfo: Set Feature Failed...\n"));
		return iResult;
	}
#endif

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_STANDBY, 0, 0, 0, NULL)) != 0) {
		_ftprintf(stderr, _T("GetDiskInfo: Set Feature Failed...\n"));
		return iResult;
	}
#endif

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_CHECKPOWERMODE1, 0, 0, 0, NULL)) != 0) {
		_ftprintf(stderr, _T("GetDiskInfo: Set Feature Failed...\n"));
		return iResult;
	}
#endif
	printf("GetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n",
		TargetId, info.major_rev_num, info.minor_rev_num, info.capability);

	printf("GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n",
		info.dma_mword,
		info.dma_ultra);

	//
	// DMA Mode.
	//
	if(!(info.dma_mword & 0x0004)) {
		_ftprintf(stderr, _T("Not Support DMA mode 2...\n"));
		return -1;
	}

	if(!(info.dma_mword & 0x0400)) {
		// Set to DMA mode 2
		if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0x22, 0x03, NULL)) != 0) {
			_ftprintf(stderr, _T("GetDiskInfo: Set Feature Failed...\n"));
			_ftprintf(stderr, _T("GetDiskInfo: Can't set to DMA mode 2\n"));
			//return iResult;
		}

		// identify.
		if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info)) != 0) {
			_ftprintf(stderr, _T("GetDiskInfo: Identify Failed...\n"));
			return iResult;
		}

		printf("GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n",
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
	if(info.capability &= 0x02)
		PerTarget[TargetId].bLBA = TRUE;
	else
		PerTarget[TargetId].bLBA = FALSE;

	//
	// Calc Capacity.
	//
	if(info.command_set_2 & 0x0400 && info.cfs_enable_2 * 0x0400) {	// Support LBA48bit
		PerTarget[TargetId].bLBA48 = TRUE;
		PerTarget[TargetId].SectorCount = info.lba_capacity_2;
		printf("Big LBA\n");
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

	//	PerTarget[TargetId].bLBA = TRUE;
	//	PerTarget[TargetId].SectorCount = 1024*1024*1024;

	return 0;
}


int
GetDiskInfo2(
			 SOCKET	connsock,
			 UINT	TargetId
			 )
{
	struct hd_driveid	info;
	int					iResult;
	char				buffer[41];

	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_PIDENTIFY, 0, 0, 0, (PCHAR)&info)) != 0) {
		_ftprintf(stderr, _T("GetDiskInfo: PIdentify Failed...\n"));
		return iResult;
	}

	printf("GetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, \n",
		TargetId, info.major_rev_num, info.minor_rev_num);

	printf("GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n",
		info.dma_mword,
		info.dma_ultra);

	//
	// DMA Mode.
	//
	if(!(info.dma_mword & 0x0004)) {
		_ftprintf(stderr, _T("Not Support DMA mode 2...\n"));
		return -1;
	}



	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	printf("Serial No: %s\n", buffer);

	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	printf("Firmware rev: %s\n", buffer);

	memset(buffer, 0, 41);
	strncpy(buffer, (PCHAR)info.model, 40);
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	printf("Model No: %s\n", buffer);

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
#if DBG
	_ftprintf(stderr, _T("Discovery: Before Login \n"));
#endif
	if((iResult = Login(connsock, LOGIN_TYPE_DISCOVERY, 0, HASH_KEY_USER)) != 0) {
		_ftprintf(stderr, _T("Discovery: Login Failed...\n"));
		return iResult;
	}

#if DBG
	_ftprintf(stderr, _T("Discovery: After Login \n"));
#endif
	if((iResult = TextTargetList(connsock)) != 0) {
		_ftprintf(stderr, _T("Discovery: Text Failed\n"));
		return iResult;
	}
#if DBG
	_ftprintf(stderr, _T("Discovery: After Text \n"));
#endif
	///////////////////////////////////////////////////////////////
	//
	// Logout Packet.
	//
	if((iResult = Logout(connsock)) != 0) {
		_ftprintf(stderr, _T("Discovery: Logout Failed...\n"));
		return iResult;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//	LPX support
//
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
		PrintErrorCode(_T("GetInterfaceList: socket "), WSAGetLastError());
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
		PrintErrorCode(_T("GetInterfaceList: WSAIoctl "), WSAGetLastError());
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
#if DBG
	_ftprintf(stderr, _T("MakeConnection: Destination Address: %02X:%02X:%02X:%02X:%02X:%02X\n"),
		pAddress->Node[0],
		pAddress->Node[1],
		pAddress->Node[2],
		pAddress->Node[3],
		pAddress->Node[4],
		pAddress->Node[5]
		);
#endif
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
			_ftprintf(stderr, _T("MakeConnection: Error When Get NIC List!!!!!!!!!!\n"));

			return FALSE;
		} else {
			_ftprintf(stderr, _T("MakeConnection: Number of NICs : %d\n"), socketAddressList->iAddressCount);
		}

		//
		// Find NIC that is connected to LanDisk.
		//
		for(i = 0; i < socketAddressList->iAddressCount; i++) {

			socketLpx = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);

			printf("MakeConnection: NIC %02d: Address %02X:%02X:%02X:%02X:%02X:%02X\n",
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
					PrintErrorCode(_T("MakeConnection: socket "), WSAGetLastError());
					return FALSE;
				}

				socketLpx.LpxAddress.Port = 0; // unspecified

				// Bind NIC.
				iErrcode = bind(sock, (struct sockaddr *)&socketLpx, sizeof(socketLpx));
				if(iErrcode == SOCKET_ERROR) {
					PrintErrorCode(_T("MakeConnection: bind "), WSAGetLastError());
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
#if DBG
					PrintErrorCode(_T("MakeConnection: connect"), WSAGetLastError());
#endif
					closesocket(sock);
					sock = INVALID_SOCKET;

					continue;
				} else {
					*pSocketData = sock;

					break;
				}
		}

		if(sock == INVALID_SOCKET) {
#if DBG
			_ftprintf(stderr, _T("Could not establish Connection to NetDisk.\n"));
#endif
			return FALSE;
		}

		_ftprintf(stderr, _T("Connection to NetDisk established.\n"));

		return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
//	NetDisk ID support.
//

//int
//ConvertNDIDStringToChar(
//		PTCHAR NetDiskIDStr,
//		PTCHAR WriteKeyStr,
//		PCHAR NetDiskID,
//		PCHAR WriteKey
//) {
//	BOOL				bret;
//	NDAS_ID_KEY_INFO	keyInfo;
//	LONG				idIdx;
//#ifdef _UNICODE
//	CHAR				Mbcs[4];
//#endif
//
//	// NetDisk ID
//	for(idIdx = 0; idIdx < 20; idIdx++) {
//		if(NetDiskIDStr[idIdx] == 0) {
//			_ftprintf(stderr, _T("ERROR: NetDisk ID is too short.\n"));
//			return -1;
//		}
//
//#ifdef _UNICODE
//		if( wctomb(Mbcs, NetDiskIDStr[idIdx]) != 1) {
//			_ftprintf(stderr, _T("ERROR: NetDisk ID has invalid characters.\n"));
//			return -1;
//		}
//		NetDiskID[idIdx] = (CHAR)toupper(Mbcs[0]);
//#else
//		NetDiskID[idIdx] = _totupper(NetDiskIDStr[idIdx]);
//#endif
//	}
//	// Write key
//	for(idIdx = 0; idIdx < 5; idIdx++) {
//		if(WriteKeyStr[idIdx] == 0) {
//			_ftprintf(stderr, _T("ERROR: Write key is too short.\n"));
//			return -1;
//		}
//#ifdef _UNICODE
//		if( wctomb(Mbcs, WriteKeyStr[idIdx]) != 1) {
//			_ftprintf(stderr, _T("ERROR: Write key has invalid characters.\n"));
//			return -1;
//		}
//		WriteKey[idIdx] = Mbcs[0];
//#else
//		WriteKey[idIdx] = WriteKeyStr[idIdx];
//#endif
//	}
//
//	return 0;
//}



//int
//VerifyNDID(
//		PTCHAR				NetDiskIDStr,
//		PTCHAR				WriteKeyStr,
//		PNDAS_ID_KEY_INFO	KeyInfo
//){
//	BOOL				bret;
//	int					iret;
//
//	iret = ConvertNDIDStringToChar(
//							NetDiskIDStr,
//							WriteKeyStr,
//							(PCHAR)(KeyInfo->serialNo),
//							KeyInfo->writeKey
//						);
//	if(iret != 0) {
//		return iret;
//	}
//
//	//	set encryption keys.
//	CopyMemory(KeyInfo->key1,NDIDV1Key1,8);
//	CopyMemory(KeyInfo->key2,NDIDV1Key2,8);
//	KeyInfo->vid		=NDIDV1VID;
//	KeyInfo->random	=NDIDV1Random;
//
//	//
//	//	Verify the NetDisk ID and write key.
//	//
//	bret = NdasIdKey_Decrypt(KeyInfo);
//	if(bret == FALSE) {
//		_ftprintf(stderr, _T("ERROR: NetDisk ID or Write key is invalid.\n"));
//		return -1;
//	}
//
//	return 0;
//}

//////////////////////////////////////////////////////////////////////////
//
//	Option callbacks.
//
CpSetDeviceStandby(int argc, _TCHAR* argv[])
{
	int					iret;
	BOOL				bret;
	LONG				StandByTimeOut;
	SOCKET				connsock;
	UINT64				parameter;
	UINT32				*param32;
	WORD				wVersionRequested;
	WSADATA				wsaData;
	LPX_ADDRESS			address;
	NDAS_DEVICE_ID		deviceID;

	//
	//	Get arguments.
	//
	if(argc < 3) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	//
	// Validate and Convert
	//
	if (!NdasIdValidate(argv[0], argv[1]) || 
		!NdasIdStringToDevice(argv[0], &deviceID))
	{
		_ftprintf(stderr, _T("ERROR: NDAS ID or Write Key is invalid.\n"));
		return -1;
	}

	StandByTimeOut = _tstol(argv[2]);
	if(StandByTimeOut == 0) {
		//
		//	Check to see if a user inputs the zero.
		//
		if( argv[2][0] != _T('0') ||
			argv[2][1] != _T('\0')
			) {
			_ftprintf(stderr, _T("ERROR: Invalid timeout value.\n"));
			return -1;
		}
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));

	iTag = 0;
	HPID = 0;
	HeaderEncryptAlgo = 0;
	DataEncryptAlgo = 0;
	iPassword = HASH_KEY_SUPER;
	CopyMemory(address.Node, deviceID.Node, LPXADDR_NODE_LENGTH);

	//
	//	Set the timeout value to the parameter.
	//
	param32 = (PUINT32)&parameter;

	if(StandByTimeOut) {
		param32[0] = 0;
		param32[1] = htonl(StandByTimeOut|0x80000000);
	} else {
		param32[0] = 0;
		param32[1] = 0;
		_ftprintf(stderr, _T("Timeout value is zero. Disable standby mode.\n"));
	}

	//
	//	Start WinSock2
	//
	wVersionRequested = MAKEWORD( 2, 2 );
	iret = WSAStartup(wVersionRequested, &wsaData);
	if(iret != 0) {
		PrintErrorCode(_T("main: WSAStartup "), WSAGetLastError());
		return -1;
	}

	//
	//	Make Connection to NetDisk.
	//	Discover NetDisk.
	//	Login NetDisk.
	//	Send a Vendor command.
	//	Reset the device
	//	Logout.
	//	Close connection.
	//
	bret = MakeConnection(&address, &connsock);
	if(bret == FALSE) {
		_ftprintf(stderr, _T("Could not establish a connection to NetDisk.\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	iret = Discovery(connsock);
	if(iret != 0) {
		_ftprintf(stderr, _T("Discovery Failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	_ftprintf(stderr, _T("Logging in...\n"));
	iret = Login(connsock, LOGIN_TYPE_NORMAL, NDAS_SUPERVISOR, HASH_KEY_SUPER_V1);
	if(iret != 0) {
		_ftprintf(stderr, _T("Supervisor Login Failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	_ftprintf(stderr, _T("Applying setting to the device...\n"));
	iret = VendorCommand(connsock, VENDOR_OP_SET_STANBY_TIMER, &parameter);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	_ftprintf(stderr, _T("Resetting the device...\n"));
	parameter = 0;
	iret = VendorCommand(connsock, VENDOR_OP_RESET, &parameter);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	//
	// Close Socket.
	//
	closesocket(connsock);

	iret = WSACleanup();
	if(iret != 0) {
		PrintErrorCode( _T(""), WSAGetLastError());
		return -1;
	}

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return 0;
}


int
CpQueryDeviceStandby(int argc, _TCHAR* argv[])
{
	int					iret;
	BOOL				bret;
	SOCKET				connsock;
	UINT64				parameter;
	UINT32				*param32;
	WORD				wVersionRequested;
	WSADATA				wsaData;
	LPX_ADDRESS			address;
	NDAS_DEVICE_ID		deviceID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	//
	// Validate and Convert
	//
	if (!NdasIdValidate(argv[0], argv[1]) || 
		!NdasIdStringToDevice(argv[0], &deviceID))
	{
		_ftprintf(stderr, _T("ERROR: NDAS ID or Write Key is invalid.\n"));
		return -1;
	}


	_ftprintf(stdout, _T("Starting the operation...\n"));

	iTag = 0;
	HPID = 0;
	HeaderEncryptAlgo = 0;
	DataEncryptAlgo = 0;
	iPassword = HASH_KEY_USER;
	param32 = (PUINT32)&parameter;
	param32[0] = htonl(1);
	param32[1] = 0;
	CopyMemory(address.Node, deviceID.Node, LPXADDR_NODE_LENGTH);

	//
	//	Start WinSock2
	//
	wVersionRequested = MAKEWORD( 2, 2 );
	iret = WSAStartup(wVersionRequested, &wsaData);
	if(iret != 0) {
		PrintErrorCode(_T("main: WSAStartup "), WSAGetLastError());
		return -1;
	}

	//
	//	Make Connection to NetDisk.
	//	Discover NetDisk.
	//	Login NetDisk.
	//	Send a Vendor command.
	//	Reset the device
	//	Logout.
	//	Close connection.
	//
	bret = MakeConnection(&address, &connsock);
	if(bret == FALSE) {
		_ftprintf(stderr, _T("Could not establish a connection to NetDisk.\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	iret = Discovery(connsock);
	if(iret != 0) {
		_ftprintf(stderr, _T("Discovery Failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	_ftprintf(stderr, _T("Logging in...\n"));
//	iret = Login(connsock, LOGIN_TYPE_NORMAL, NDAS_SUPERVISOR, HASH_KEY_SUPER_V1);
	iret = Login(connsock, LOGIN_TYPE_NORMAL, FIRST_TARGET_RO_USER, HASH_KEY_USER);
	if(iret != 0) {
		_ftprintf(stderr, _T("RO user login failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	_ftprintf(stderr, _T("Querying values to the device...\n"));
	iret = VendorCommand(connsock, VENDOR_OP_GET_STANBY_TIMER, &parameter);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	_ftprintf(stderr, _T("Logging out...\n"));

	iret = Logout(connsock);
	if(iret != 0) {
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	//
	// Close Socket.
	//
	closesocket(connsock);

	iret = WSACleanup();
	if(iret != 0) {
		PrintErrorCode( _T(""), WSAGetLastError());
		return -1;
	}

	_ftprintf(stdout, _T("Finished the operation.\n"));
#if DBG
	_ftprintf(stderr, _T("\nStandby timeout with enable/disable flag: %08lx\n"), ntohl(param32[1]));
#endif
	if(ntohl(param32[1]) & 0x80000000) {
		_ftprintf(stderr, _T("\nStandby timeout: %d minutes.\n"), ntohl(param32[1])&0x7fffffff);
	} else {
		_ftprintf(stderr, _T("\nStandby timeout: Disabled.\n"));
	}

	return 0;
}

CpDeviceStandby(int argc, _TCHAR* argv[])
{
	int					iret;
	BOOL				bret;
//	LONG				StandByTimeOut;
	SOCKET				connsock;
	UINT64				parameter;
	UINT32				*param32;
	WORD				wVersionRequested;
	WSADATA				wsaData;
	LPX_ADDRESS			address;
	NDAS_DEVICE_ID		deviceID;

	//
	//	Get arguments.
	//
	if(argc < 2) {
		_ftprintf(stderr, _T("ERROR: More parameter needed.\n"));
		return -1;
	}

	//
	// Validate and Convert
	//
	if (!NdasIdValidate(argv[0], argv[1]) || 
		!NdasIdStringToDevice(argv[0], &deviceID))
	{
		_ftprintf(stderr, _T("ERROR: NDAS ID or Write Key is invalid.\n"));
		return -1;
	}

	_ftprintf(stdout, _T("Starting the operation...\n"));

	iTag = 0;
	HPID = 0;
	HeaderEncryptAlgo = 0;
	DataEncryptAlgo = 0;
	iPassword = HASH_KEY_USER;
	param32 = (PUINT32)&parameter;
	param32[0] = htonl(1);
	param32[1] = 0;
	CopyMemory(address.Node, deviceID.Node, LPXADDR_NODE_LENGTH);

	//
	//	Start WinSock2
	//
	wVersionRequested = MAKEWORD( 2, 2 );
	iret = WSAStartup(wVersionRequested, &wsaData);
	if(iret != 0) {
		PrintErrorCode(_T("main: WSAStartup "), WSAGetLastError());
		return -1;
	}

	//
	//	Make Connection to NetDisk.
	//	Discover NetDisk.
	//	Login NetDisk.
	//	Send a Vendor command.
	//	Reset the device
	//	Logout.
	//	Close connection.
	//
	bret = MakeConnection(&address, &connsock);
	if(bret == FALSE) {
		_ftprintf(stderr, _T("Could not establish a connection to NetDisk.\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	iret = Discovery(connsock);
	if(iret != 0) {
		_ftprintf(stderr, _T("Discovery Failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return -1;
	}

	_ftprintf(stderr, _T("Logging in...\n"));
//	iret = Login(connsock, LOGIN_TYPE_NORMAL, NDAS_SUPERVISOR, HASH_KEY_SUPER_V1);
	iret = Login(connsock, LOGIN_TYPE_NORMAL, FIRST_TARGET_RO_USER, HASH_KEY_USER);
	if(iret != 0) {
		_ftprintf(stderr, _T("RO user login failed...\n"));
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	_ftprintf(stderr, _T("-------------------------------\n"));
/*
	_ftprintf(stderr, _T("Querying values to the device...\n"));
	iret = VendorCommand(connsock, VENDOR_OP_GET_STANBY_TIMER, &parameter);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}
*/

	iret = IdeCommand(connsock, 0, 0, WIN_STANDBYNOW1, 0, 0, 0, NULL);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}



	_ftprintf(stderr, _T("-------------------------------\n"));
/*
	struct hd_driveid	info;

	// identify.
	iret = IdeCommand(connsock, 0, 0, WIN_IDENTIFY, 0, 0, 0, (PCHAR)&info);
	if(iret != 0) {
		Logout(connsock);
		closesocket(connsock);
		WSACleanup();
		return iret;
	}
	_ftprintf(stderr, _T("-------------------------------\n"));
*/

	_ftprintf(stderr, _T("Logging out...\n"));

	iret = Logout(connsock);
	if(iret != 0) {
		closesocket(connsock);
		WSACleanup();
		return iret;
	}

	//
	// Close Socket.
	//
	closesocket(connsock);

	iret = WSACleanup();
	if(iret != 0) {
		PrintErrorCode( _T(""), WSAGetLastError());
		return -1;
	}

	_ftprintf(stdout, _T("Finished the operation.\n"));

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// command line client for NDAS device management
//
// usage:
// ndascmd
//
// help or ?
// set standby
// query standby
//

typedef int (*CMDPROC)(int argc, _TCHAR* argv[]);

typedef struct _CPROC_ENTRY {
	LPCTSTR* szCommands;
	CMDPROC proc;
	DWORD nParamMin;
	DWORD nParamMax;
} CPROC_ENTRY, *PCPROC_ENTRY, *CPROC_TABLE;

#define DEF_COMMAND_1(c,x,h) LPCTSTR c [] = {_T(x), NULL, _T(h), NULL};
#define DEF_COMMAND_2(c,x,y,h) LPCTSTR c [] = {_T(x), _T(y), NULL, _T(h), NULL};
#define DEF_COMMAND_3(c,x,y,z,h) LPCTSTR c [] = {_T(x), _T(y), _T(z), NULL, _T(h), NULL};
#define DEF_COMMAND_4(c,x,y,z,w,h) LPCTSTR c[] = {_T(x), _T(y), _T(z), _T(w), NULL, _T(h), NULL};

DEF_COMMAND_2(_c_set_standby, "set", "standby", "<NetDisk ID> <Write key> <timeout minutes>")
DEF_COMMAND_2(_c_query_standby, "query", "standby", "<NetDisk ID> <Write key>")
DEF_COMMAND_2(_c_standby_now, "now", "standby", "<NetDisk ID> <Write key>")

// DEF_COMMAND_1(_c_
static const CPROC_ENTRY _cproc_table[] = {
	{ _c_set_standby, CpSetDeviceStandby, 3, 3},
	{ _c_query_standby, CpQueryDeviceStandby, 2, 2},
	{ _c_standby_now, CpDeviceStandby, 2, 2},
};

DWORD GetStringMatchLength(LPCTSTR szToken, LPCTSTR szCommand)
{
	DWORD i = 0;
	for (; szToken[i] != _T('\0') && szCommand[i] != _T('\0'); ++i) {
		TCHAR x[2] = { szToken[i], 0};
		TCHAR y[2] = { szCommand[i], 0};
		if (*CharLower(x) != *CharLower(y))
			if (szToken[i] == _T('\0')) return i;
			else return 0;
	}
	if (szToken[i] != _T('\0')) {
		return 0;
	}
	return i;
}

void FindPartialMatchEntries(
							 LPCTSTR szToken, DWORD dwLevel,
							 SIZE_T* pCand, SIZE_T* pcCand)
{
	DWORD maxMatchLen = 1, matchLen = 0;
	LPCTSTR szCommand = NULL;
	SIZE_T* pNewCand = pCand;
	SIZE_T* pCurNewCand = pCand;
	SIZE_T cNewCand = 0;
	SIZE_T i;

	for (i = 0; i < *pcCand; ++i) {
		szCommand = _cproc_table[pCand[i]].szCommands[dwLevel];
		matchLen = GetStringMatchLength(szToken, szCommand);
		if (matchLen > maxMatchLen) {
			maxMatchLen = matchLen;
			pCurNewCand = pNewCand;
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			cNewCand = 1;
		} else if (matchLen == maxMatchLen) {
			*pCurNewCand = pCand[i];
			++pCurNewCand;
			++cNewCand;
		}
	}

	*pcCand = cNewCand;

	return;
}

void PrintCmd(SIZE_T index)
{
	SIZE_T j = 0;
	_tprintf(_T("%s"), _cproc_table[index].szCommands[j]);
	for (j = 1; _cproc_table[index].szCommands[j]; ++j) {
		_tprintf(_T(" %s"), _cproc_table[index].szCommands[j]);
	}
}

void PrintOpt(SIZE_T index)
{
	LPCTSTR* ppsz = _cproc_table[index].szCommands;

	for (; NULL != *ppsz; ++ppsz) {
		__noop;
	}

	_tprintf(_T("%s"), *(++ppsz));

}

void PrintCand(const SIZE_T* pCand, SIZE_T cCand)
{
	SIZE_T i;

	for (i = 0; i < cCand; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(pCand[i]);
		_tprintf(_T("\n"));
	}
}

void usage()
{
	const SIZE_T nCommands =
		sizeof(_cproc_table) / sizeof(_cproc_table[0]);
	SIZE_T i;

	_tprintf(
		_T("Copyright (C) 2003-2005 XIMETA, Inc.\n")
		_T("vendorctl: command line vendor-control for NDAS device\n")
		_T("\n")
		_T(" usage: vendorctl <command> [options]\n")
		_T("\n"));

	for (i = 0; i < nCommands; ++i) {
		_tprintf(_T(" - "));
		PrintCmd(i);
		_tprintf(_T(" "));
		PrintOpt(i);
		_tprintf(_T("\n"));
	}

	_tprintf(_T("\n<NetDisk ID> : 20 characters\n"));
	_tprintf(_T("<WriteKey>   : 5 characters\n"));
}


#define MAX_CANDIDATES RTL_NUMBER_OF(_cproc_table)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	SIZE_T candIndices[MAX_CANDIDATES] = {0};
	SIZE_T cCandIndices = MAX_CANDIDATES;
	DWORD dwLevel = 0;
	SIZE_T i;

	if (argc < 2) {
		usage();
		return -1;
	}

	for (i = 0; i < cCandIndices; ++i) {
		candIndices[i] = i;
	}

	for (dwLevel = 0; dwLevel + 1 < (DWORD) argc; ++dwLevel) {

		FindPartialMatchEntries(
			argv[1 + dwLevel],
			dwLevel,
			candIndices,
			&cCandIndices);

		if (cCandIndices == 1) {

#if _DEBUG
			_tprintf(_T("Running: "));
			PrintCand(candIndices, cCandIndices);
#endif

			return _cproc_table[candIndices[0]].
				proc(argc - dwLevel - 3, &argv[dwLevel + 3]);

		} else if (cCandIndices == 0) {

			_tprintf(_T("Error: Unknown command.\n\n"));
			usage();
			break;

		} else if (cCandIndices > 1) {

			BOOL fStop = FALSE;
			SIZE_T i;

			// if the current command parts are same, proceed,
			// otherwise emit error
			for ( i = 1; i < cCandIndices; ++i) {
				if (0 != lstrcmpi(
					_cproc_table[candIndices[0]].szCommands[dwLevel],
					_cproc_table[candIndices[i]].szCommands[dwLevel]))
				{
					_tprintf(_T("Error: Ambiguous command:\n\n"));
					PrintCand(candIndices, cCandIndices);
					_tprintf(_T("\n"));
					fStop = TRUE;
					break;
				}
			}

			if (fStop) {
				break;
			}
		}

		// more search

		if (dwLevel + 2 >= (DWORD) argc) {
			_tprintf(_T("Error: Incomplete command:\n\n"));
			PrintCand(candIndices, cCandIndices);
			_tprintf(_T("\n"));
		}
	}

	return -1;
}
