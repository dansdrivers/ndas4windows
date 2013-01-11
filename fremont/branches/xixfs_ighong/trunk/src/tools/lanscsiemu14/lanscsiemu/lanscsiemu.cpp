// LanScsiEmu.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#define _LPX_

#define	NR_MAX_TARGET			2
#define MAX_DATA_BUFFER_SIZE	64 * 1024
#define	DISK_SIZE				1024 * 1024 * 1024 // 1G
#define	MAX_CONNECTION			16
#define HASH_KEY				0x1F4A50731530EABB

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	_int8			NRRWHost;
	_int8			NRROHost;
	unsigned _int64	TargetData;
	
	// IDE Info.
	_int64	Size;

} TARGET_DATA, *PTARGET_DATA;

typedef struct	_SESSION_DATA {
	SOCKET			connSock;
	unsigned _int16	TargetID;
	unsigned _int32	LUN;
	int				iSessionPhase;
	unsigned _int16	CSubPacketSeq;
	unsigned _int8	iLoginType;
	unsigned _int16	CPSlot;
	unsigned		PathCommandTag;
	unsigned		iUser;
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	unsigned		CHAP_I;
	unsigned		CHAP_C;
	BOOL			bIncCount;
	unsigned _int64	iPassword;
			
} SESSION_DATA, *PSESSION_DATA;

// Global Variable.
_int16			G_RPID = 0;
int				iUnitDisk0, iUnitDisk1;	
int				NRTarget;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
BOOL			bLBA48;
SOCKET			listenSock;
SESSION_DATA	sessionData[MAX_CONNECTION];

unsigned _int16	HeaderEncryptAlgo = 1;
unsigned _int16	DataEncryptAlgo = 1;


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

inline int 
RecvIt(
	   SOCKET	sock,
	   PCHAR	buf, 
	   int		size
	   )
{
	int res;
	int len = size;

	while (len > 0) {
		if ((res = recv(sock, buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "RecvIt");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "RecvIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}

	return size;
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
		if ((res = send(sock, buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "SendIt");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "SendIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}

	return size;
}

int
ReadRequest(
			SOCKET			connSock,
			PCHAR			pBuffer,
			PLANSCSI_PDU	pPdu,
			PSESSION_DATA	pSessionData
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

	if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {

		Decrypt32(
			(unsigned char*)pPdu->pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&pSessionData->CHAP_C,
			(unsigned char *)&pSessionData->iPassword
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

		
		if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
			&& DataEncryptAlgo != 0) {
			
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohl(pPdu->pH2RHeader->DataSegLen),
				(unsigned char *)&pSessionData->CHAP_C,
				(unsigned char *)&pSessionData->iPassword
				);
		}

	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
}

DWORD WINAPI 
SessionThreadProc(
				  LPVOID lpParameter   // thread data
				  )
{
	PSESSION_DATA			pSessionData = (PSESSION_DATA)lpParameter;
	int						iResult;
	CHAR					PduBuffer[MAX_REQUEST_SIZE];
	LANSCSI_PDU				pdu;
	PLANSCSI_H2R_PDU_HEADER	pRequestHeader;
	PLANSCSI_R2H_PDU_HEADER	pReplyHeader;
	
	//
	// Init variables...
	//
	pSessionData->CSubPacketSeq = 0;	
	pSessionData->bIncCount = FALSE;
	pSessionData->iSessionPhase = FLAG_SECURITY_PHASE;
		
	while(pSessionData->iSessionPhase != LOGOUT_PHASE) {

		//
		// Read Request.
		//
		iResult = ReadRequest(pSessionData->connSock, PduBuffer, &pdu, pSessionData);
		if(iResult <= 0) {
			fprintf(stderr, "Session: Can't Read Request.\n");
			
			pSessionData->iSessionPhase = LOGOUT_PHASE;
			continue;
			//goto EndSession;
		}

		fprintf(stderr, "Session: Recevived...\n");

		pRequestHeader = pdu.pH2RHeader;
		switch(pRequestHeader->Opocde) {
		case LOGIN_REQUEST:
			{
				PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestHeader;
				PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
				PBIN_PARAM_SECURITY					pSecurityParam;
				PAUTH_PARAMETER_CHAP				pAuthChapParam;
				PBIN_PARAM_NEGOTIATION				pParamNego;
				
				pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)PduBuffer;
				
				if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: Bad Command.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeLoginReply;
				} 
				
				// Check Header.
				pLoginRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pRequestHeader;
				if((pLoginRequestHeader->VerMin > LANSCSI_CURRENT_VERSION)
					|| (pLoginRequestHeader->ParameterType != PARAMETER_TYPE_BINARY)
					|| (pLoginRequestHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
					// Bad Parameter...
					fprintf(stderr, "Session2: Bad Parameter.\n");
					
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_VERSION_MISMATCH;
					goto MakeLoginReply;
				}
				
				// Check Sub Packet Sequence.
				if(ntohs(pLoginRequestHeader->CSubPacketSeq) != pSessionData->CSubPacketSeq) {
					// Bad Sub Sequence...
					fprintf(stderr, "Session2: Bad Sub Packet Sequence. H %d R %d\n",
						pSessionData->CSubPacketSeq,
						ntohs(pLoginRequestHeader->CSubPacketSeq));
					
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					goto MakeLoginReply;
				}
				
				// Check Port...
				if(pRequestHeader->CSubPacketSeq > 0) {
					if((pSessionData->HPID != (unsigned)ntohl(pLoginRequestHeader->HPID))
						|| (pSessionData->RPID != ntohs(pLoginRequestHeader->RPID))
						|| (pSessionData->CPSlot != ntohs(pLoginRequestHeader->CPSlot))
						|| (pSessionData->PathCommandTag != (unsigned)ntohl(pLoginRequestHeader->PathCommandTag))) {
						
						fprintf(stderr, "Session2: Bad Port parameter.\n");
						
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
						goto MakeLoginReply;
					}
				}
				
				switch(ntohs(pRequestHeader->CSubPacketSeq)) {
				case 0:
					{
						fprintf(stderr, "*** First ***\n");
						// Check Flag.
						if((pLoginRequestHeader->T != 0)
							|| (pLoginRequestHeader->CSG != FLAG_SECURITY_PHASE)
							|| (pLoginRequestHeader->NSG != FLAG_SECURITY_PHASE)) {
							fprintf(stderr, "Session: BAD First Flag.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Check Parameter.
						if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParamter[1]
							|| (pdu.pDataSeg == NULL)) {							
							fprintf(stderr, "Session: BAD First Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}	
						pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						if(pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) {
							fprintf(stderr, "Session: BAD First Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Login Type.
						if((pSecurityParam->LoginType != LOGIN_TYPE_NORMAL) 
							&& (pSecurityParam->LoginType != LOGIN_TYPE_DISCOVERY)) {
							fprintf(stderr, "Session: BAD First Login Type.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Auth Type.
						if(!(ntohs(pSecurityParam->AuthMethod) & AUTH_METHOD_CHAP)) {
							fprintf(stderr, "Session: BAD First Auth Method.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Store Data.
						pSessionData->HPID = ntohl(pLoginRequestHeader->HPID);
						pSessionData->CPSlot = ntohs(pLoginRequestHeader->CPSlot);
						pSessionData->PathCommandTag = ntohl(pLoginRequestHeader->PathCommandTag);
						
						pSessionData->iLoginType = pSecurityParam->LoginType;
						
						// Assign RPID...
						pSessionData->RPID = G_RPID;
						
						fprintf(stderr, "[LanScsiEmu] Version Min %d, Auth Method %d, Login Type %d\n",
							pLoginRequestHeader->VerMin, ntohs(pSecurityParam->AuthMethod), pSecurityParam->LoginType);
						
						// Make Reply.
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->T = 0;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						
						pSecurityParam = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pSecurityParam->AuthMethod = htons(AUTH_METHOD_CHAP);
					}
					break;
				case 1:
					{
						fprintf(stderr, "*** Second ***\n");
						// Check Flag.
						if((pLoginRequestHeader->T != 0)
							|| (pLoginRequestHeader->CSG != FLAG_SECURITY_PHASE)
							|| (pLoginRequestHeader->NSG != FLAG_SECURITY_PHASE)) {
							fprintf(stderr, "Session: BAD Second Flag.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Check Parameter.
						if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParamter[1]
							|| (pdu.pDataSeg == NULL)) {
							
							fprintf(stderr, "Session: BAD Second Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}	
						pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
							|| (pSecurityParam->LoginType != pSessionData->iLoginType)
							|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
							
							fprintf(stderr, "Session: BAD Second Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Hash Algorithm.
						pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
						if(!(ntohl(pAuthChapParam->CHAP_A) & HASH_ALGORITHM_MD5)) {
							fprintf(stderr, "Session: Not Supported HASH Algorithm.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Store Data.
						pSessionData->CHAP_I = ntohl(pAuthChapParam->CHAP_I);
						
						// Create Challenge
						pSessionData->CHAP_C = (rand() << 16) + rand();
						
						// Make Header
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->T = 0;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						
						pSecurityParam = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pAuthChapParam = &pSecurityParam->ChapParam;
						pSecurityParam->ChapParam.CHAP_A = htonl(HASH_ALGORITHM_MD5);
						pSecurityParam->ChapParam.CHAP_C[0] = htonl(pSessionData->CHAP_C);
						
						printf("CHAP_C %d\n", pSessionData->CHAP_C);
					}
					break;
				case 2:
					{						
						fprintf(stderr, "*** Third ***\n");
						// Check Flag.
						if((pLoginRequestHeader->T == 0)
							|| (pLoginRequestHeader->CSG != FLAG_SECURITY_PHASE)
							|| (pLoginRequestHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)) {
							fprintf(stderr, "Session: BAD Third Flag.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Check Parameter.
						if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParamter[1]
							|| (pdu.pDataSeg == NULL)) {
							
							fprintf(stderr, "Session: BAD Third Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}	
						pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
							|| (pSecurityParam->LoginType != pSessionData->iLoginType)
							|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
							
							fprintf(stderr, "Session: BAD Third Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}
						pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
						if(!(ntohl(pAuthChapParam->CHAP_A) == HASH_ALGORITHM_MD5)) {
							fprintf(stderr, "Session: Not Supported HASH Algorithm.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						if((unsigned)ntohl(pAuthChapParam->CHAP_I) != pSessionData->CHAP_I) {
							fprintf(stderr, "Session: Bad CHAP_I.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Store User ID(Name)
						pSessionData->iUser = ntohl(pAuthChapParam->CHAP_N);
						
						switch(pSessionData->iLoginType) {
						case LOGIN_TYPE_NORMAL:
							{
								BOOL	bRW = FALSE;

								// Target Exist?
								if(pSessionData->iUser & 0x00000001) {	// Use Target0
									if(PerTarget[0].bPresent == FALSE) {
										fprintf(stderr, "Session: No Target.\n");
										pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
										goto MakeLoginReply;
									}

								}
/*
								if(pSessionData->iUser & 0x00000002) {	// Use Target1
									if(PerTarget[1].bPresent == FALSE) {
										fprintf(stderr, "Session: No Target.\n");
										pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
										goto MakeLoginReply;
									}
								}
*/
								// Select Passwd.
								if((pSessionData->iUser & 0x00010001)
									|| (pSessionData->iUser & 0x00020002)) {
									pSessionData->iPassword = HASH_KEY;
								} else {
									pSessionData->iPassword = HASH_KEY;
								}

								// Increse Login User Count.
								if(pSessionData->iUser & 0x00000001) {	// Use Target0
									if(pSessionData->iUser &0x00010000) {
										if(PerTarget[0].NRRWHost > 0) {
											fprintf(stderr, "Session: Already RW. Logined\n");
											pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
											goto MakeLoginReply;
										}
										PerTarget[0].NRRWHost++;
									} else {
										PerTarget[0].NRROHost++;
									}
								}
/*
								if(pSessionData->iUser & 0x00000002) {	// Use Target0
									if(pSessionData->iUser &0x00020000) {
										if(PerTarget[1].NRRWHost > 0) {
											fprintf(stderr, "Session: Already RW. Logined\n");
											pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
											goto MakeLoginReply;
										}
										PerTarget[1].NRRWHost++;
									} else {
										PerTarget[1].NRROHost++;
									}
								}
*/
								pSessionData->bIncCount = TRUE;
							}

							break;
						case LOGIN_TYPE_DISCOVERY:
							{
								pSessionData->iPassword = HASH_KEY;								
							}
							break;
						default:
							break;
						}
						
						//
						// Check CHAP_R
						//
						{
							unsigned char	result[16] = { 0 };

							Hash32To128(
								(unsigned char*)&pSessionData->CHAP_C, 
								result, 
								(unsigned char*)&pSessionData->iPassword
								);

							if(memcmp(result, pAuthChapParam->CHAP_R, 16) != 0) {
								fprintf(stderr, "Auth Failed.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_AUTH_FAILED;
								goto MakeLoginReply;							
							}
						}
						
						// Make Reply.
						pLoginReplyHeader->T = 1;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_LOGIN_OPERATION_PHASE;
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						
						// Set Phase.
						pSessionData->iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;
					}
					break;
				case 3:
					{
						fprintf(stderr, "*** Fourth ***\n");
						// Check Flag.
						if((pLoginRequestHeader->T == 0)
							|| (pLoginRequestHeader->CSG != FLAG_LOGIN_OPERATION_PHASE)
							|| ((pLoginRequestHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)) {
							fprintf(stderr, "Session: BAD Fourth Flag.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}

						// Check Parameter.
						if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParamter[1]
							|| (pdu.pDataSeg == NULL)) {
							
							fprintf(stderr, "Session: BAD Fourth Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}	
						pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
						if((pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION)) {
							fprintf(stderr, "Session: BAD Fourth Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Make Reply.
						pLoginReplyHeader->T = 1;
						pLoginReplyHeader->CSG = FLAG_LOGIN_OPERATION_PHASE;
						pLoginReplyHeader->NSG = FLAG_FULL_FEATURE_PHASE;
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						
						pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
						pParamNego->HWType = HW_TYPE_ASIC;
						pParamNego->HWVersion = HW_VERSION_CURRENT;
						pParamNego->NRSlot = htonl(1);
						pParamNego->MaxBlocks = htonl(128);
						pParamNego->MaxTargetID = htonl(2);
						pParamNego->MaxLUNID = htonl(1);
						pParamNego->HeaderEncryptAlgo = htons(HeaderEncryptAlgo);
						pParamNego->HeaderDigestAlgo = 0;
						pParamNego->DataEncryptAlgo = htons(DataEncryptAlgo);
						pParamNego->DataDigestAlgo = 0;
					}
					break;
				default:
					{
						fprintf(stderr, "Session: BAD Sub-Packet Sequence.\n");
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
						goto MakeLoginReply;
					}
					break;
				}
MakeLoginReply:
				pSessionData->CSubPacketSeq = ntohs(pLoginRequestHeader->CSubPacketSeq) + 1;
				
				pLoginReplyHeader->Opocde = LOGIN_RESPONSE;
				pLoginReplyHeader->VerMax = LANSCSI_CURRENT_VERSION;
				pLoginReplyHeader->VerActive = LANSCSI_CURRENT_VERSION;
				pLoginReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
				pLoginReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
			}
			break;
		case LOGOUT_REQUEST:
			{
				PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pLogoutRequestHeader;
				PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pLogoutReplyHeader;
				
				pLogoutReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)PduBuffer;
				
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: LOGOUT Bad Command.\n");
					pLogoutReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeLogoutReply;
				} 
				
				// Check Header.
				pLogoutRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)pRequestHeader;
				if((pLogoutRequestHeader->F == 0)
					|| (pSessionData->HPID != (unsigned)ntohl(pLogoutRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pLogoutRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pLogoutRequestHeader->CPSlot))
					|| (0 != ntohs(pLogoutRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: LOGOUT Bad Port parameter.\n");
					
					pLogoutReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeLogoutReply;
				}
				
				// Do Logout.
				if(pSessionData->iLoginType == LOGIN_TYPE_NORMAL) {
					// Something to do...
				}
				
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				
				// Make Reply.
				pLogoutReplyHeader->F = 1;
				pLogoutReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
				pLogoutReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //0;
				
MakeLogoutReply:
				pLogoutReplyHeader->Opocde = LOGOUT_RESPONSE;
			}
			break;
		case TEXT_REQUEST:
			{
				PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
				
				pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)PduBuffer;
				
				fprintf(stderr, "Session: Text request\n");

				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: TEXT_REQUEST Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeTextReply;
				}
				
				// Check Header.
				pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;				
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: TEXT Bad Port parameter.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeTextReply;
				}
				
				// Check Parameter.
				if(ntohl(pRequestHeader->DataSegLen) < 4) {	// Minimum size.
					fprintf(stderr, "Session: TEXT No Data seg.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeTextReply;
				}
				switch(((PBIN_PARAM)pdu.pDataSeg)->ParamType) {
				case BIN_PARAM_TYPE_TARGET_LIST:
					{
						PBIN_PARAM_TARGET_LIST	pParam;
						
						pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
						
						pParam->NRTarget = NRTarget;	
						for(int i = 0; i < NRTarget; i++) {
							pParam->PerTarget[i].TargetID = htonl(i);
							pParam->PerTarget[i].NRRWHost = PerTarget[i].NRRWHost;
							pParam->PerTarget[i].NRROHost = PerTarget[i].NRROHost;
							pParam->PerTarget[i].TargetData = PerTarget[i].TargetData;
						}
						
						pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
					}
					break;
				case BIN_PARAM_TYPE_TARGET_DATA:
					{
						PBIN_PARAM_TARGET_DATA pParam;
						
						pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
						
						if(pParam->GetOrSet == PARAMETER_OP_SET) {
							if(ntohl(pParam->TargetID) == 0) {
								if(!(pSessionData->iUser & 0x00000001) 
									||!(pSessionData->iUser & 0x00010000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[0].TargetData = pParam->TargetData;

							}
/*							
							else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002) 
									||!(pSessionData->iUser & 0x00020000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[1].TargetData = pParam->TargetData;
							}
*/							
							else {
								fprintf(stderr, "No Access Right\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeTextReply;
							}
						} else {
							if(ntohl(pParam->TargetID) == 0) {
								if(!(pSessionData->iUser & 0x00000001)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								pParam->TargetData = PerTarget[0].TargetData;

							}
/*							
							else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								pParam->TargetData = PerTarget[1].TargetData;
							}
*/							
							else {
								fprintf(stderr, "No Access Right\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeTextReply;
							}
						}

						pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
					}
					break;
				default:
					break;
				}
				
				pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
MakeTextReply:
				pReplyHeader->Opocde = TEXT_RESPONSE;
				
			}
			break;
		case DATA_H2R:
			{
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
				}
			}
			break;
		case IDE_COMMAND:
			{
				PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader;
				PCHAR							data[MAX_DATA_BUFFER_SIZE] = { 0 };
				_int64							Location;
				unsigned						SectorCount;
				int								iUnitDisk;
				
				pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)PduBuffer;
				pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;				
				
				//
				// Convert Location and Sector Count.
				//
				Location = 0;
				SectorCount = 0;
				if(bLBA48 == TRUE) {
					Location = ((_int64)pRequestHeader->LBAHigh_Prev << 40)
						+ ((_int64)pRequestHeader->LBAMid_Prev << 32)
						+ ((_int64)pRequestHeader->LBALow_Prev << 24)
						+ ((_int64)pRequestHeader->LBAHigh_Curr << 16)
						+ ((_int64)pRequestHeader->LBAMid_Curr << 8)
						+ ((_int64)pRequestHeader->LBALow_Curr);
					SectorCount = ((unsigned)pRequestHeader->SectorCount_Prev << 8)
						+ (pRequestHeader->SectorCount_Curr);
				} else {
					Location = ((_int64)pRequestHeader->LBAHeadNR << 24)
						+ ((_int64)pRequestHeader->LBAHigh_Curr << 16)
						+ ((_int64)pRequestHeader->LBAMid_Curr << 8)
						+ ((_int64)pRequestHeader->LBALow_Curr);
					
					SectorCount = pRequestHeader->SectorCount_Curr;
				}
				
				if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Not Normal Login.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeIDEReply;
				}
				
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeIDEReply;
				}
				
				// Check Header.
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: IDE Bad Port parameter.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeIDEReply;
				}
				
				// Request for existed target?
				if(PerTarget[ntohl(pRequestHeader->TargetID)].bPresent == FALSE) {
					fprintf(stderr, "Session2: Target Not exist.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
					goto MakeIDEReply;
				}
				
				// LBA48 command? 
				if((bLBA48 == FALSE) &&
					((pRequestHeader->Command == WIN_READDMA_EXT)
					|| (pRequestHeader->Command == WIN_WRITEDMA_EXT))) {
					fprintf(stderr, "Session2: Bad Command.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeIDEReply;
				}
				
				if(ntohl(pRequestHeader->TargetID) == 0) {
					iUnitDisk = iUnitDisk0;
				} else {
					iUnitDisk = iUnitDisk1;
				}

				switch(pRequestHeader->Command) {
				case WIN_READDMA:
				case WIN_READDMA_EXT:
					{
						fprintf(stderr, "READ: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "READ: Out of bound\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
						}
						
						_lseeki64(iUnitDisk, Location * 512, SEEK_SET);
						read(iUnitDisk, data, SectorCount * 512);
					}
					break;
				case WIN_WRITEDMA:
				case WIN_WRITEDMA_EXT:
					{
						fprintf(stderr, "WRITE: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check access right.
						//
						if((pSessionData->iUser != FIRST_TARGET_RW_USER)
							&& (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
							fprintf(stderr, "Session2: No Write right...\n");
							
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
						}
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "WRITE: Out of bound\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
						}
						
						// Receive Data.
						iResult = RecvIt(
							pSessionData->connSock,
							(PCHAR)data,
							SectorCount * 512
							);
						if(iResult == SOCKET_ERROR) {
							fprintf(stderr, "ReadRequest: Can't Recv Data...\n");
							pSessionData->iSessionPhase = LOGOUT_PHASE;
							continue;
							//goto EndSession;
						}

						//
						// Decrypt Data.
						//
						if(DataEncryptAlgo != 0) {
							Decrypt32(
								(unsigned char*)data,
								SectorCount * 512,
								(unsigned char *)&pSessionData->CHAP_C,
								(unsigned char *)&pSessionData->iPassword
								);
						}
						_lseeki64(iUnitDisk, Location * 512, SEEK_SET);
						write(iUnitDisk, data, SectorCount * 512);
					}
					break;
				case WIN_VERIFY:
					{
						fprintf(stderr, "Verify: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "Verify: Out of bound\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
						}
					}
					break;
				case WIN_IDENTIFY:
					{
						struct	hd_driveid	*pInfo;
						char	serial_no[20] = { '2', '1', '0', '3', 0};
						char	firmware_rev[8] = {'.', '1', 0, '0', 0, };
						char	model[40] = { 'a', 'L', 's', 'n', 's', 'c', 'E', 'i', 'u', 'm', 0, };
						
						fprintf(stderr, "Identify:\n");
						
						pInfo = (struct hd_driveid *)data;
						
						pInfo->lba_capacity = (unsigned)PerTarget[ntohl(pRequestHeader->TargetID)].Size / 512;
						pInfo->lba_capacity_2 = PerTarget[ntohl(pRequestHeader->TargetID)].Size /512;
						pInfo->capability |= 0x0002;	// LBA
						pInfo->cfs_enable_2 |= 0x0400;
						pInfo->command_set_2 |= 0x0400;
						pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;	// ATAPI 5
						pInfo->dma_mword = 0x0407;
						memcpy(pInfo->serial_no, serial_no, 20);
						memcpy(pInfo->fw_rev, firmware_rev, 8);
						memcpy(pInfo->model, model, 40);
						bLBA48 = TRUE;
					}
					break;
				default:
					fprintf(stderr, "Not Supported Command 0x%x\n", pRequestHeader->Command);
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeIDEReply;
				}
				
				pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply:
				if(pRequestHeader->Command == WIN_IDENTIFY) {

					//
					// Encrption.
					//
					if(DataEncryptAlgo != 0) {
						Encrypt32(
							(unsigned char*)data,
							512,
							(unsigned char *)&pSessionData->CHAP_C,
							(unsigned char *)&pSessionData->iPassword
							);
					}

					// Send Data.
					iResult = SendIt(
						pSessionData->connSock,
						(PCHAR)data,
						512
						);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "ReadRequest: Can't Send Identify Data...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						continue;
						//goto EndSession;
					}
					
				}
				
				if((pRequestHeader->Command == WIN_READDMA)
					|| (pRequestHeader->Command == WIN_READDMA_EXT)) {	
					//
					// Encrption.
					//
					if(DataEncryptAlgo != 0) {
						Encrypt32(
							(unsigned char*)data,
							SectorCount * 512,
							(unsigned char *)&pSessionData->CHAP_C,
							(unsigned char *)&pSessionData->iPassword
							);
					}
					// Send Data.
					iResult = SendIt(
						pSessionData->connSock,
						(PCHAR)data,
						SectorCount * 512
						);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "ReadRequest: Can't Send READ Data...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						continue;
						//goto EndSession;
					}
					
				}
				pReplyHeader->Opocde = IDE_RESPONSE;
				
			}
			break;
		default:
			// Bad Command...
			break;
		}
		
		{
			unsigned _int32	iTemp;

			// Send Reply.
			pReplyHeader = (PLANSCSI_R2H_PDU_HEADER)PduBuffer;
			
			pReplyHeader->HPID = htonl(pSessionData->HPID);
			pReplyHeader->RPID = htons(pSessionData->RPID);
			pReplyHeader->CPSlot = htons(pSessionData->CPSlot);
			pReplyHeader->AHSLen = 0;
			pReplyHeader->CSubPacketSeq = htons(pSessionData->CSubPacketSeq);
			pReplyHeader->PathCommandTag = htonl(pSessionData->PathCommandTag);
			
			iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + ntohl(pReplyHeader->DataSegLen);
			//
			// Encrypt.
			//
			if(pReplyHeader->Opocde != LOGIN_RESPONSE) {
				if(HeaderEncryptAlgo != 0) {
					Encrypt32(
						(unsigned char*)PduBuffer,
						sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
						(unsigned char *)&pSessionData->CHAP_C,
						(unsigned char *)&pSessionData->iPassword
						);			
				}
				if(DataEncryptAlgo != 0 
					&& ntohl(pReplyHeader->DataSegLen) != 0) {
					Encrypt32(
						(unsigned char*)PduBuffer + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
						iTemp,
						(unsigned char *)&pSessionData->CHAP_C,
						(unsigned char *)&pSessionData->iPassword
						);
				}
			}
			
			iResult = SendIt(
				pSessionData->connSock,
				PduBuffer,
				iTemp
				);
			if(iResult == SOCKET_ERROR) {
				fprintf(stderr, "ReadRequest: Can't Send First Reply...\n");
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				continue;
				//goto EndSession;
			}
		}
		
		if((pReplyHeader->Opocde == LOGIN_RESPONSE)
			&& (pSessionData->CSubPacketSeq == 4)) {
			pSessionData->CSubPacketSeq = 0;
			pSessionData->iSessionPhase = FLAG_FULL_FEATURE_PHASE;
		}
	}
	
//EndSession:

	fprintf(stderr, "Session2: Logout Phase.\n");
	
	switch(pSessionData->iLoginType) {
	case LOGIN_TYPE_NORMAL:
		{
			if(pSessionData->bIncCount == TRUE) {
				
				// Decrese Login User Count.
				if(pSessionData->iUser & 0x00000001) {	// Use Target0
					if(pSessionData->iUser &0x00010000) {
						PerTarget[0].NRRWHost--;
					}					
					else {
						PerTarget[0].NRROHost--;
					}
				}
/*
				if(pSessionData->iUser & 0x00000002) {	// Use Target0
					if(pSessionData->iUser &0x00020000) {
						PerTarget[1].NRRWHost--;
					} else {
						PerTarget[1].NRROHost--;
					}
				}
*/
			}
		}
		break;
	case LOGIN_TYPE_DISCOVERY:
	default:
		break;
	}

	closesocket(pSessionData->connSock);
	pSessionData->connSock = INVALID_SOCKET;

	return 0;
}
/*
DWORD WINAPI 
BroadcastThreadProc(
					LPVOID lpParameter   // thread data
					)
{
	SOCKET			sock;
	SOCKADDR_LPX	address;


	// Create Listen Socket.
	sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), "socket");
		goto Out;
	}
	
	// Bind Port 15000
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_LPX;
	address.LpxAddress.Port = htons(LPX_PORT_NUMBER);
	
	err = bind(sock, (struct sockaddr *)&address, sizeof(address));
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "bind");
		goto Out;
	}

}
*/
int main(int argc, char* argv[])
{
	WORD				wVersionRequested;
	WSADATA				wsaData;
	int					err;
	int					i;
#ifdef _LPX_
	SOCKADDR_LPX		address;
#else
	struct sockaddr_in	servaddr;
#endif
//	HANDLE				hBThread;

	// Open UnitDisk.
	if((iUnitDisk0 = open("UnitDisk0", _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
		char	buffer[512];
		_int64	loc;

		if((iUnitDisk0 = open("UnitDisk0", _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
			printf("Can not open ND\n");
			return 1;
		}
		
		memset(buffer, 0, 512);
		loc = _lseeki64(iUnitDisk0, DISK_SIZE - 512, SEEK_END);

		printf("Loc : %d\n", loc);
		if(write(iUnitDisk0, buffer, 512) == -1) {
			perror( "Can not write ND" );
			return 1;
		}
	} 
	
	_lseeki64(iUnitDisk0, 0, SEEK_SET);
/*
	// UnitDisk 1
	if((iUnitDisk1 = open("UnitDisk1", _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
		char	buffer[512];
		_int64	loc;

		if((iUnitDisk1 = open("UnitDisk1", _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
			printf("Can not open ND\n");
			return 1;
		}
		
		memset(buffer, 0, 512);
		loc = _lseeki64(iUnitDisk1, DISK_SIZE - 512, SEEK_END);

		printf("Loc : %d\n", loc);
		if(write(iUnitDisk1, buffer, 512) == -1) {
			perror( "Can not write ND" );
			return 1;
		}
	} 
	
	_lseeki64(iUnitDisk1, 0, SEEK_SET);
*/
	// Init.
	bLBA48 = FALSE;
	NRTarget = 1;
	PerTarget[0].bPresent = TRUE;
	PerTarget[0].NRRWHost = 0;
	PerTarget[0].NRROHost = 0;
	PerTarget[0].TargetData = 0;
	PerTarget[0].Size = DISK_SIZE;
/*	
	PerTarget[1].bPresent = TRUE;
	PerTarget[1].NRRWHost = 0;
	PerTarget[1].NRROHost = 0;
	PerTarget[1].TargetData = 0;
	PerTarget[1].Size = DISK_SIZE;
*/
	srand((unsigned)time(NULL));

	// Sockets
	listenSock = INVALID_SOCKET;
	
	//
	// Init Session.
	//
	for(i = 0; i < MAX_CONNECTION; i++) {
		sessionData[i].connSock = INVALID_SOCKET;
	}

	// Startup Socket.
	wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0) {
		PrintError(WSAGetLastError(), "WSAStartup");
		return -1;
	}
/*
	//
	// Add Broadcaster.
	//
	hBThread = CreateThread(
		NULL,
		0,
		BroadcasthreadProc,
		NULL,
		NULL,
		NULL
		);
*/
#ifdef _LPX_

	// Create Listen Socket.
	listenSock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
	if(INVALID_SOCKET == listenSock) {
		PrintError(WSAGetLastError(), "socket");
		goto Out;
	}

	// Bind Port 10000
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_LPX;
	address.LpxAddress.Port = htons(LPX_PORT_NUMBER);

	err = bind(listenSock, (struct sockaddr *)&address, sizeof(address));
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "bind");
		goto Out;
	}
#else

	// Create Listen Socket.
	listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if(INVALID_SOCKET == listenSock) {
		PrintError(WSAGetLastError(), "socket");
		goto Out;
	}

	// Bind Port 10000
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(LPX_PORT_NUMBER);

	err = bind(listenSock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "bind");
		goto Out;
	}

#endif

	// Listen...
	err = listen(listenSock, 0);//MAX_CONNECTION);
	if(SOCKET_ERROR == err) {
		PrintError(WSAGetLastError(), "listen");
		goto Out;
	}

	// Main loop...
	for(;;) {
		// New Connection.
		SOCKET	tempSock;
		int		iptr;
		HANDLE	hThread;
		
		tempSock = accept(listenSock, NULL, NULL);
		if(INVALID_SOCKET == tempSock) {
			PrintError(WSAGetLastError(), "accept");
			goto Out;
		}
		
		// Find Empty connSock.
		iptr = MAX_CONNECTION;
		for(i = 0; i < MAX_CONNECTION; i++) {
			if(sessionData[i].connSock == INVALID_SOCKET) {
				iptr = i;
				break;
			}
		}
		
		if(iptr == MAX_CONNECTION) {
			// No Empty Slot.
			printf("No Empty slot\n");
			closesocket(tempSock);
			continue;
		}
		
		sessionData[iptr].connSock = tempSock;

		//
		// Create Thread.
		//
		hThread = CreateThread(
			NULL,
			0,
			SessionThreadProc,
			&sessionData[iptr],
			NULL,
			NULL
			);
	}
	
Out:
	if(listenSock != INVALID_SOCKET)
		closesocket(listenSock);

	for(i = 0; i > MAX_CONNECTION; i++) {
		if(sessionData[i].connSock != INVALID_SOCKET)
			closesocket(sessionData[i].connSock);
	}

	// Cleanup Socket.
	err = WSACleanup();
	if(err != 0) {
		PrintError(WSAGetLastError(), "WSACleanup");
		return -1;
	}
	
	return 0;
}

