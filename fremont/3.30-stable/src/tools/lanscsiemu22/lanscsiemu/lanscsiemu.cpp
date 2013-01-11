	// LanScsiEmu.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "socketlpx.h"

#define _LPX_

#define	NR_MAX_TARGET			2
#define MAX_DATA_BUFFER_SIZE	64 * 1024
#define	DISK_SIZE				1024 * 1024 * 256 // 256MB
//#define	DISK_SIZE				((INT64)1024 * 1024 * 1024 * 3) // 3Giga
#define	MAX_CONNECTION			16
#define DROP_RATE				0  // out of 1000 packets

#define HASH_KEY_READONLY HASH_KEY_USER 
#define HASH_KEY_READWRITE HASH_KEY_USER 

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	BOOL			bLBA;
	BOOL			bLBA48;
	_int8			NRRWHost;
	_int8			NRROHost;
	unsigned _int64	TargetData;
	char *			ExportDev;
	int			Export;
	
	// IDE Info.
	_int64	Size;
	int		dma_mword;
	
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

typedef struct _PROM_DATA {
	unsigned _int64 MaxConnTime;
	unsigned _int64 MaxRetTime;
	unsigned _int64 UserPasswd;
	unsigned _int64 SuperPasswd;
} PROM_DATA, *PPROM_DATA;
// Global Variable.
_int16			G_RPID = 0;
unsigned _int8	thisHWVersion = LANSCSI_CURRENT_VERSION;

int				NRTarget;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
SOCKET			listenSock;
SESSION_DATA	sessionData[MAX_CONNECTION];
PROM_DATA		Prom;

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

		if (thisHWVersion == LANSCSI_VERSION_1_1 ||
		    thisHWVersion == LANSCSI_VERSION_2_0) {
			if (pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
			    && HeaderEncryptAlgo != 0) {

				Decrypt32(
					(unsigned char*)pPdu->pAHS,
					ntohs(pPdu->pH2RHeader->AHSLen),
//					sizeof(pPdu->pR2HHeader->AHSLen),
					(unsigned char *)&pSessionData->CHAP_C,
					(unsigned char *)&pSessionData->iPassword
					);
			}
		}
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
//	int 				count, i;
	// to support version 1.1, 2.0
	UCHAR				ucParamType;
	
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
		pRequestHeader = pdu.pH2RHeader;
		if (pRequestHeader->Opcode!=IDE_COMMAND)
			fprintf(stderr, "Received - ");
		switch(pRequestHeader->Opcode) {
		case LOGIN_REQUEST:
			{
				PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestHeader;
				PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
				PBIN_PARAM_SECURITY					pSecurityParam;
				PAUTH_PARAMETER_CHAP				pAuthChapParam;
				PBIN_PARAM_NEGOTIATION				pParamNego;
				fprintf(stderr, "LOGIN_REQUEST ");
				pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)PduBuffer;
				pLoginRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pRequestHeader;

				if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: Bad Command. Invalid login phase\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeLoginReply;
				} 
				
				// Check Header.
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

						// to support version 1.1, 2.0
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
							|| (pdu.pDataSeg == NULL)) {							
							fprintf(stderr, "Session: BAD First Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
							goto MakeLoginReply;
						}	
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
								fprintf(stderr, "Session: BAD First Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSI_VERSION_1_0) {
						pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
						// end of supporting version

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
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
						}
						
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
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Second Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Second Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
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

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
						}
						// end of supporting version
						
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

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Third Request Data.\n"); 
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Third Request Data.\n"); 
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
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
								if(pSessionData->iUser & 0x00000002) {	// Use Target1
									if(PerTarget[1].bPresent == FALSE) {
										fprintf(stderr, "Session: No Target.\n");
										pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
										goto MakeLoginReply;
									}
								}

								// Select Passwd.
								if((pSessionData->iUser & 0x00010001)
									|| (pSessionData->iUser & 0x00020002)) {
									pSessionData->iPassword = HASH_KEY_READWRITE;
								} else {
									pSessionData->iPassword = HASH_KEY_READONLY;
								}

								// Increse Login User Count.
								if(pSessionData->iUser & 0x00000001) {	// Use Target0
									if(pSessionData->iUser &0x00010000) {
										if(PerTarget[0].NRRWHost > 0) {
#if 1
											fprintf(stderr, "Session: Already RW. Logined\n");
											pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
											goto MakeLoginReply;
#else
											fprintf(stderr, "Session: Already RW. But allowing new login\n");
#endif
										}
										PerTarget[0].NRRWHost++;
									} else {
										PerTarget[0].NRROHost++;
									}
								}
								if(pSessionData->iUser & 0x00000002) {	// Use Target0
									if(pSessionData->iUser &0x00020000) {
										if(PerTarget[1].NRRWHost > 0) {
#if 0
											fprintf(stderr, "Session: Already RW. Logined\n");
											pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
											goto MakeLoginReply;
#else
											fprintf(stderr, "Session: Already RW. But allowing new login\n");
#endif
										}
										PerTarget[1].NRRWHost++;
									} else {
										PerTarget[1].NRROHost++;
									}
								}
								pSessionData->bIncCount = TRUE;
							}
							break;
						case LOGIN_TYPE_DISCOVERY:
							{
								pSessionData->iPassword = HASH_KEY_READONLY;								
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
						
						// Set Phase.
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
						}
						// end of supporting version

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
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Fourth Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Fourth Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
						}
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

						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
						    thisHWVersion == LANSCSI_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
						}
						
						pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
						pParamNego->HWType = HW_TYPE_ASIC;
//						pParamNego->HWVersion = HW_VERSION_CURRENT;
						// fixed to support version 1.1, 2.0
						pParamNego->HWVersion = thisHWVersion;
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
				fprintf(stderr, "LOGOUT_REQUEST\n");
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
				
				fprintf(stderr, "TEXT_REQUEST \n");

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

				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSI_VERSION_1_0) {
					if(ntohl(pRequestHeader->DataSegLen) < 4) {	// Minimum size.
						fprintf(stderr, "Session: TEXT No Data seg.\n");
						
						pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
						goto MakeTextReply;
					}
				}
				if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    thisHWVersion == LANSCSI_VERSION_2_0) {
					if(ntohs(pRequestHeader->AHSLen) < 4) {	// Minimum size.
						fprintf(stderr, "Session: TEXT No Data seg.\n");
					
						pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
						goto MakeTextReply;
					}
				}
				// end of supporting version

				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSI_VERSION_1_0) {
					ucParamType = ((PBIN_PARAM)pdu.pDataSeg)->ParamType;
				}
				if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    thisHWVersion == LANSCSI_VERSION_2_0) {
					ucParamType = ((PBIN_PARAM)pdu.pAHS)->ParamType;
				}
				// end of supporting version

//				switch(((PBIN_PARAM)pdu.pDataSeg)->ParamType) {
				switch(ucParamType) {
				case BIN_PARAM_TYPE_TARGET_LIST:
					{
						PBIN_PARAM_TARGET_LIST	pParam;
						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
						pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSI_VERSION_2_0) {
							pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
						}						
						pParam->NRTarget = NRTarget;	
						for(int i = 0; i < pParam->NRTarget; i++) {
							pParam->PerTarget[i].TargetID = htonl(i);
							pParam->PerTarget[i].NRRWHost = PerTarget[i].NRRWHost;
							pParam->PerTarget[i].NRROHost = PerTarget[i].NRROHost;
							pParam->PerTarget[i].TargetData = PerTarget[i].TargetData;
						}
						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
						pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
					}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSI_VERSION_2_0) {
							pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
						}
						// end of supporting version

					}
					break;
				case BIN_PARAM_TYPE_TARGET_DATA:
					{
						PBIN_PARAM_TARGET_DATA pParam;
						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSI_VERSION_2_0) {
							pParam = (PBIN_PARAM_TARGET_DATA)pdu.pAHS;
						}
						// end of supporting version
						
						if(pParam->GetOrSet == PARAMETER_OP_SET) {
							if(ntohl(pParam->TargetID) == 0) {
								if(!(pSessionData->iUser & 0x00000001) 
									||!(pSessionData->iUser & 0x00010000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[0].TargetData = pParam->TargetData;

							} else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002) 
									||!(pSessionData->iUser & 0x00020000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[1].TargetData = pParam->TargetData;
							} else {
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

							} else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
									goto MakeTextReply;
								}

								pParam->TargetData = PerTarget[1].TargetData;
							} else {
								fprintf(stderr, "No Access Right\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeTextReply;
							}
						}

						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSI_VERSION_1_0) {
							pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSI_VERSION_2_0) {
							pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
						}
						// end of supporting version
					}
					break;
				default:
					break;
				}
				
		
				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSI_VERSION_1_0) {
					pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
				}
				if (thisHWVersion == LANSCSI_VERSION_1_1 ||
		    		    thisHWVersion == LANSCSI_VERSION_2_0) {
					pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
				}
				// end of supporting version
MakeTextReply:
				pReplyHeader->Opocde = TEXT_RESPONSE;
			}
			break;
		case DATA_H2R:
			{
				fprintf(stderr, "DATA_H2R ");
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
				}
			}
			break;
		case IDE_COMMAND:
			{
			if (thisHWVersion == LANSCSI_VERSION_1_0) {
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
				if(ntohl(pRequestHeader->TargetID) == 0) {
					iUnitDisk = PerTarget[0].Export;
				} else {
					iUnitDisk = PerTarget[1].Export;
				}

				if(PerTarget[iUnitDisk].bLBA48 == TRUE) {
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
				if((PerTarget[ntohl(pRequestHeader->TargetID)].bLBA48 == FALSE) &&
					((pRequestHeader->Command == WIN_READDMA_EXT)
					|| (pRequestHeader->Command == WIN_WRITEDMA_EXT))) {
					fprintf(stderr, "Bad Command. LBA48 command to non-LBA48 device\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeIDEReply;
				}
				
				if(ntohl(pRequestHeader->TargetID) == 0) {
					iUnitDisk = PerTarget[0].Export;
				} else {
					iUnitDisk = PerTarget[1].Export;
				}

				switch(pRequestHeader->Command) {
				case WIN_READDMA:
				case WIN_READDMA_EXT:
					{
//						int i;
						UCHAR	data2[MAX_DATA_BUFFER_SIZE] = { 0 };
//						fprintf(stderr, "R");
//						fprintf(stderr, "READ: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
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
//						fprintf(stderr, "W");
//						fprintf(stderr, "WRITE: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
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
				case WIN_VERIFY_EXT:
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
				case WIN_SETFEATURES:
					{
						struct	hd_driveid	*pInfo;
						int Feature, Mode;

						fprintf(stderr, "set features ");
						pInfo = (struct hd_driveid *)data;
						Feature = pRequestHeader->Feature_Curr;
						Mode = pRequestHeader->SectorCount_Curr;
						switch(Feature) {
							case SETFEATURES_XFER: 
								if(Mode == XFER_MW_DMA_2) {
									fprintf(stderr, "XFER DMA2\n");
									if (ntohl(pRequestHeader->TargetID) == 0) {
										PerTarget[0].dma_mword &= 0xf8ff;
										PerTarget[0].dma_mword |= (1 << ((Mode & 0x7) + 8));
									} else {
										PerTarget[1].dma_mword &= 0xf8ff;
										PerTarget[1].dma_mword |= (1 << ((Mode & 0x7) + 8));
									}
								} else {
									fprintf(stderr, "XFER unknown mode %d\n", Mode);
								}
								break;
							default:
								fprintf(stderr, "Unknown feature %d\n", Feature);
								break;
						}
					}
					break;
				case WIN_IDENTIFY:
					{
						struct	hd_driveid	*pInfo;
						char	serial_no[20] = { '2', '1', '0', '3', 0};
						char	firmware_rev[8] = {'.', '1', 0, '0', 0, };
						char	model[40] = { 'a', 'L', 's', 'n', 's', 'c', 'E', 'i', 'u', 'm', 0, };
						int 	iUnitDisk = ntohl(pRequestHeader->TargetID);
						
						fprintf(stderr, "Identify:\n");
						
						pInfo = (struct hd_driveid *)data;
						
						pInfo->lba_capacity = (unsigned int)PerTarget[iUnitDisk].Size / 512;	// Obsoleted field
						pInfo->lba_capacity_2 = PerTarget[iUnitDisk].Size /512;
						pInfo->heads = 255;
						pInfo->sectors = 63;
						pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
						if(PerTarget[iUnitDisk].bLBA) pInfo->capability |= 0x0002;	// LBA
						if(PerTarget[iUnitDisk].bLBA48) { // LBA48
							pInfo->cfs_enable_2 |= 0x0400;
							pInfo->command_set_2 |= 0x0400;
						}
						pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;	// ATAPI 5
						pInfo->dma_mword = PerTarget[iUnitDisk].dma_mword;
						memcpy(pInfo->serial_no, serial_no, 20);
						memcpy(pInfo->fw_rev, firmware_rev, 8);
						memcpy(pInfo->model, model, 40);
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
				

			} else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				   thisHWVersion == LANSCSI_VERSION_2_0) {
				PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
				PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
				UCHAR	data[MAX_DATA_BUFFER_SIZE] = { 0 };
				_int64	Location;
				unsigned SectorCount;
				int	iUnitDisk;
				
				pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)PduBuffer;
				pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;				
				
				//
				// Convert Location and Sector Count.
				//
				Location = 0;
				SectorCount = 0;

				if(ntohl(pRequestHeader->TargetID) == 0) {
					iUnitDisk = PerTarget[0].Export;
				} else {
					iUnitDisk = PerTarget[1].Export;
				}

				if(PerTarget[iUnitDisk].bLBA48 == TRUE) {
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
					
					goto MakeIDEReply1;
				}
				
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
					
					goto MakeIDEReply1;
				}
				
				// Check Header.
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: IDE Bad Port parameter.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeIDEReply1;
				}
				
				// Request for existed target?
				if(PerTarget[ntohl(pRequestHeader->TargetID)].bPresent == FALSE) {
					fprintf(stderr, "Session2: Target Not exist.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
					goto MakeIDEReply1;
				}
				
				// LBA48 command? 
				if((PerTarget[ntohl(pRequestHeader->TargetID)].bLBA48 == FALSE) &&
					((pRequestHeader->Command == WIN_READDMA_EXT)
					|| (pRequestHeader->Command == WIN_WRITEDMA_EXT))) {
					fprintf(stderr, "Session2: Bad Command. LBA48 command to non-LBA48 device\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeIDEReply1;
				}
				

				switch(pRequestHeader->Command) {
				case WIN_READDMA:
				case WIN_READDMA_EXT:
					{
						// for debug
//						int i;
						UCHAR	data2[MAX_DATA_BUFFER_SIZE] = { 0 };
//						fprintf(stderr, "R");
//						fprintf(stderr, "READ: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "READ: Location = %lld, Sector_Size = %lld, TargetID = %d, Out of bound\n", Location + SectorCount, PerTarget[ntohl(pRequestHeader->TargetID)].Size >> 9, ntohl(pRequestHeader->TargetID));
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply1;
						}
						_lseeki64(iUnitDisk, Location * 512, SEEK_SET);
						read(iUnitDisk, data, SectorCount * 512);
#if 0
						memcpy(data2, data, SectorCount*512);
							Decrypt32(
								(unsigned char*)data2,
								SectorCount * 512,
								(unsigned char *)&pSessionData->CHAP_C,
								(unsigned char *)&pSessionData->iPassword
								);

						for (i = 0; i < SectorCount * 512; i++) {
							printf("%02X ", data2[i]);
							if ((i+1)%16 == 0) printf("\n");
						}
						printf("\n\n"); 
#endif
					}
					break;
				case WIN_WRITEDMA:
				case WIN_WRITEDMA_EXT:
					{
//						fprintf(stderr, "W");
//						fprintf(stderr, "WRITE: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check access right.
						//
						if((pSessionData->iUser != FIRST_TARGET_RW_USER)
							&& (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
							fprintf(stderr, "Session2: No Write right...\n");
							
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply1;
						}
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "WRITE: Out of bound\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply1;
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
				case WIN_VERIFY_EXT:
					{
						fprintf(stderr, "Verify: Location %I64d, Sector Count %d...\n", Location, SectorCount);
						
						//
						// Check Bound.
						//
						if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
						{
							fprintf(stderr, "Verify: Out of bound\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply1;
						}
					}
					break;
				case WIN_SETFEATURES:
					{
						struct	hd_driveid	*pInfo;
						int Feature, Mode;

						fprintf(stderr, "set features ");
						pInfo = (struct hd_driveid *)data;
//						Feature = pRequestHeader->Feature;
						// fixed to support version 1.1, 2.0
						Feature = pRequestHeader->Feature_Curr;
						Mode = pRequestHeader->SectorCount_Curr;
						switch(Feature) {
							case SETFEATURES_XFER: 
								if(Mode == XFER_MW_DMA_2) {
									fprintf(stderr, "XFER DMA2\n");
									if (ntohl(pRequestHeader->TargetID) == 0) {
										PerTarget[0].dma_mword &= 0xf8ff;
										PerTarget[0].dma_mword |= (1 << ((Mode & 0x7) + 8));
									} else {
										PerTarget[1].dma_mword &= 0xf8ff;
										PerTarget[1].dma_mword |= (1 << ((Mode & 0x7) + 8));
									}
								} else {
									fprintf(stderr, "XFER unknown mode 0x%x\n", Mode);
								}
								break;
							default:
								fprintf(stderr, "Unknown feature %d\n", Feature);
								break;
						}					
					}
					break;
				// to support version 1.1, 2.0
				case WIN_SETMULT:
					{
						fprintf(stderr, "set multiple mode\n");
					}
					break;
				case WIN_CHECKPOWERMODE1:
					{
						int Mode;

						Mode = pRequestHeader->SectorCount_Curr;
						fprintf(stderr, "check power mode = 0x%02x\n", Mode);
					}
					break;
				case WIN_STANDBY:
					{
						fprintf(stderr, "standby\n");
					}
					break;
				case WIN_IDENTIFY:
				case WIN_PIDENTIFY:
					{
						struct	hd_driveid	*pInfo;
						char	serial_no[20] = { '2', '1', '0', '3', 0};
						char	firmware_rev[8] = {'.', '1', 0, '0', 0, };
						char	model[40] = { 'a', 'L', 's', 'n', 's', 'c', 'E', 'i', 'u', 'm', 0, };
						int 	iUnitDisk = ntohl(pRequestHeader->TargetID);
						
						fprintf(stderr, "Identify:\n");
						
						pInfo = (struct hd_driveid *)data;
						pInfo->lba_capacity = (unsigned int)PerTarget[iUnitDisk].Size / 512;
						pInfo->lba_capacity_2 = PerTarget[iUnitDisk].Size /512;
						pInfo->heads = 255;
						pInfo->sectors = 63;
						pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
						if(PerTarget[iUnitDisk].bLBA) pInfo->capability |= 0x0002;	// LBA
						if(PerTarget[iUnitDisk].bLBA48) { // LBA48
							pInfo->cfs_enable_2 |= 0x0400;
							pInfo->command_set_2 |= 0x0400;
						}
						pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;	// ATAPI 5
						pInfo->dma_mword = PerTarget[iUnitDisk].dma_mword;
						memcpy(pInfo->serial_no, serial_no, 20);
						memcpy(pInfo->fw_rev, firmware_rev, 8);
						memcpy(pInfo->model, model, 40);
					}
					break;
				default:
					fprintf(stderr, "Not Supported Command1 0x%x\n", pRequestHeader->Command);
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeIDEReply1;
				}
				
				pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply1:
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
				
			}
			break;

		case VENDER_SPECIFIC_COMMAND:
			{
				PLANSCSI_VENDER_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_VENDER_REPLY_PDU_HEADER	pReplyHeader;
				fprintf(stderr, "VENDER_SPECIFIC_COMMAND\n");
				pReplyHeader = (PLANSCSI_VENDER_REPLY_PDU_HEADER)PduBuffer;
				pRequestHeader = (PLANSCSI_VENDER_REQUEST_PDU_HEADER)PduBuffer;				
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: Vender Bad Port parameter.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeVenderReply;
				}

				if( (pRequestHeader->VenderID != htons(NKC_VENDER_ID)) ||
				    (pRequestHeader->VenderOpVersion != VENDER_OP_CURRENT_VERSION) 
				) {
					fprintf(stderr, "Session2: Vender Version don't match.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeVenderReply;

				}
				switch(pRequestHeader->VenderOp) {
					case VENDER_OP_SET_MAX_RET_TIME:
						Prom.MaxRetTime = NTOHLL(pRequestHeader->VenderParameter);
						break;
					case VENDER_OP_SET_MAX_CONN_TIME:
						Prom.MaxConnTime = NTOHLL(pRequestHeader->VenderParameter);
						break;
					case VENDER_OP_GET_MAX_RET_TIME:
						pRequestHeader->VenderParameter = HTONLL(Prom.MaxRetTime);
						break;
					case VENDER_OP_GET_MAX_CONN_TIME:
						pRequestHeader->VenderParameter = HTONLL(Prom.MaxConnTime);
						break;
					case VENDER_OP_SET_SUPERVISOR_PW:
						Prom.SuperPasswd = NTOHLL(pRequestHeader->VenderParameter);
						break;
					case VENDER_OP_SET_USER_PW:
						Prom.UserPasswd = NTOHLL(pRequestHeader->VenderParameter);
						break;
					case VENDER_OP_RESET:
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						break;
					default:
						break;
				}
MakeVenderReply:
				pReplyHeader->Opocde = VENDER_SPECIFIC_RESPONSE;
			}
			break;
	
		case NOP_H2R:
			fprintf(stderr, "NOP\n");
			// no op. Do not send reply
			break;
		default:
			fprintf(stderr, "Bad opcode:%d\n", pRequestHeader->Opcode);
			// Bad Command...
			break;
		}
		if (pRequestHeader->Opcode==NOP_H2R) {
			// Do not send reply
			continue;
		}
		{
			unsigned _int32	iTemp;

			// Send Reply.
			pReplyHeader = (PLANSCSI_R2H_PDU_HEADER)PduBuffer;
			
			pReplyHeader->HPID = htonl(pSessionData->HPID);
			pReplyHeader->RPID = htons(pSessionData->RPID);
			pReplyHeader->CPSlot = htons(pSessionData->CPSlot);

			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSI_VERSION_1_0) {
			pReplyHeader->AHSLen = 0;
			}
			if (thisHWVersion == LANSCSI_VERSION_1_1 ||
			    thisHWVersion == LANSCSI_VERSION_2_0) {
				pReplyHeader->DataSegLen = 0;
			}
			// end of supporting version

			pReplyHeader->CSubPacketSeq = htons(pSessionData->CSubPacketSeq);
			pReplyHeader->PathCommandTag = htonl(pSessionData->PathCommandTag);
			

			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSI_VERSION_1_0) {
			iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + ntohl(pReplyHeader->DataSegLen);
			}
			if (thisHWVersion == LANSCSI_VERSION_1_1 ||
			    thisHWVersion == LANSCSI_VERSION_2_0) {
				iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + ntohs(pReplyHeader->AHSLen);
			}
			// end of supporting version

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

				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSI_VERSION_1_0) {
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
				if (thisHWVersion == LANSCSI_VERSION_1_1 ||
				    thisHWVersion == LANSCSI_VERSION_2_0) {
					// modified 20040608 by shlee
					if(HeaderEncryptAlgo != 0 
//					if(DataEncryptAlgo != 0 
						&& ntohs(pReplyHeader->AHSLen) != 0) {
						Encrypt32(
							(unsigned char*)PduBuffer + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
							iTemp,
							(unsigned char *)&pSessionData->CHAP_C,
							(unsigned char *)&pSessionData->iPassword
							);
					}
				}
				// end of supporting version

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
					} else {
						PerTarget[0].NRROHost--;
					}
				}
				if(pSessionData->iUser & 0x00000002) {	// Use Target0
					if(pSessionData->iUser &0x00020000) {
						PerTarget[1].NRRWHost--;
					} else {
						PerTarget[1].NRROHost--;
					}
				}
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



typedef struct _PNP_MESSAGE {
	BYTE ucType;
	BYTE ucVersion;
} PNP_MESSAGE, *PPNP_MESSAGE;

//
// for broadcast
//
void GenPnpMessage(void)
{
	SOCKADDR_LPX slpx;
	int result;
	int broadcastPermission;
	PNP_MESSAGE message;
	int i = 0;
	SOCKET			sock;
//	SOCKADDR_LPX	address;

	fprintf(stderr, "Starting PNP broadcasting\n");

	// Create Listen Socket.
	sock = socket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), "socket");
		return;
	}

	broadcastPermission = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			(const char*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
		fprintf(stderr, "Can't setsockopt for broadcast: %s\n", strerror(errno));
		return ;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family = AF_LPX;
	slpx.LpxAddress.Port = htons(LPX_BROADCAST_PORT_NUMBER);

//	memcpy(slpx.slpx_node, LPX_BROADCAST_NODE, LPX_NODE_LEN);
#if 0 
        slpx.slpx_node[0] = 0xFF;
        slpx.slpx_node[1] = 0xFF;
        slpx.slpx_node[2] = 0xFF;
        slpx.slpx_node[3] = 0xFF;
        slpx.slpx_node[4] = 0xFF;
        slpx.slpx_node[5] = 0xFF;
#endif

	memset(&message, 0, sizeof(message));
	message.ucType = 0;
	message.ucVersion = thisHWVersion;

	result = bind(sock, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result < 0) {
		fprintf(stderr, "Error! when binding...: %s\n", strerror(errno));
		return;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family  = AF_LPX;
	slpx.LpxAddress.Port = htons(LPX_BROADCAST_PORT_NUMBER+1);
#if 1 
        slpx.LpxAddress.Node[0] = 0xFF;
        slpx.LpxAddress.Node[1] = 0xFF;
        slpx.LpxAddress.Node[2] = 0xFF;
        slpx.LpxAddress.Node[3] = 0xFF;
        slpx.LpxAddress.Node[4] = 0xFF;
        slpx.LpxAddress.Node[5] = 0xFF;
#endif

	while(1) {
//		fprintf(stderr, "Sending broadcast(%d)\n", i++);
		result = sendto(sock, (const char*)&message, sizeof(message),
				0, (struct sockaddr *)&slpx, sizeof(slpx));
		if (result < 0) {
			fprintf(stderr, "Can't send broadcast message: %s\n", strerror(errno));
			return;
		}
		Sleep(2000);
	}
}

DWORD WINAPI 
BroadcasthreadProc(
					LPVOID lpParameter   // thread data
					)
{
	GenPnpMessage();
	return 0;
}

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
	HANDLE				hBThread;

	// Setting packet drop rate
	{
		HANDLE deviceHandle;
		ULONG Param;
		DWORD dwReturn;
		BOOL bRet;
		deviceHandle = CreateFile (
		            TEXT("\\\\.\\SocketLpx"), //LPXTCP_DOSDEVICE, //TEXT("\\\\.\\SocketLpx"), //TEXT("\\\\.\\SocketLpx"), //SOCKETLPX_DOSDEVICE_NAME,
		            GENERIC_READ, // GENERIC_READ | GENERIC_WRITE,
		            0, //FILE_ATTRIBUTE_DEVICE, // | FILE_SHARE_WRITE,
		            NULL,
		            OPEN_EXISTING,
		            FILE_FLAG_OVERLAPPED,
		            0
		     );
                     
		if( INVALID_HANDLE_VALUE == deviceHandle ) {
			fprintf(stderr, "CreateFile Error\n");
		} else {
			Param = DROP_RATE;
			bRet = DeviceIoControl(
	                 deviceHandle,
	                 IOCTL_LPX_SET_DROP_RATE,
	                 &Param,   
	                 sizeof(Param), 
	                 NULL,
	                 NULL,
	                 &dwReturn,	
	                 NULL        // Overlapped
	                 );	
			if (bRet == FALSE) {
				fprintf(stderr, "Failed to set drop rate\n");
			} else {
				fprintf(stderr, "Set drop rate to %d\n", Param);
			}
		}
	}

	// Open UnitDisk.
	if((PerTarget[0].Export = open("UnitDisk0", _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
		char	buffer[512];
		_int64	loc;

		if((PerTarget[0].Export = open("UnitDisk0", _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
			printf("Can not open ND\n");
			return 1;
		}
		
		memset(buffer, 0, 512);
		loc = _lseeki64(PerTarget[0].Export, DISK_SIZE - 512, SEEK_END);

		printf("Loc : %I64d\n", loc);
		if(write(PerTarget[0].Export, buffer, 512) == -1) {
			perror( "Can not write ND" );
			return 1;
		}
	} 
	
	_lseeki64(PerTarget[0].Export, 0, SEEK_SET);

	// UnitDisk 1
	if((PerTarget[1].Export = open("UnitDisk1", _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
		char	buffer[512];
		_int64	loc;

		if((PerTarget[1].Export = open("UnitDisk1", _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE)) < 0) {
			printf("Can not open ND\n");
			return 1;
		}
		
		memset(buffer, 0, 512);
		loc = _lseeki64(PerTarget[1].Export, DISK_SIZE - 512, SEEK_END);

		printf("Loc : %I64d\n", loc);
		if(write(PerTarget[1].Export, buffer, 512) == -1) {
			perror( "Can not write ND" );
			return 1;
		}
	} 
	
	_lseeki64(PerTarget[1].Export, 0, SEEK_SET);

	// Init.
	NRTarget = 2;
	PerTarget[0].bLBA = TRUE;
	PerTarget[0].bLBA48 = TRUE;
	PerTarget[0].bPresent = TRUE;
	PerTarget[0].NRRWHost = 0;
	PerTarget[0].NRROHost = 0;
	PerTarget[0].TargetData = 0;
	PerTarget[0].Size = DISK_SIZE;
	PerTarget[0].dma_mword = 0x707; // Support up to DMA2, current mode is DMA 2

	PerTarget[1].bLBA = TRUE;
	PerTarget[1].bLBA48 = TRUE;
	PerTarget[1].bPresent = TRUE;
	PerTarget[1].NRRWHost = 0;
	PerTarget[1].NRROHost = 0;
	PerTarget[1].TargetData = 0;
	PerTarget[1].Size = DISK_SIZE;
	PerTarget[1].dma_mword = 0x707; // Support up to DMA2, current mode is DMA 2

	Prom.MaxConnTime = 4999; // 5 sec
	Prom.MaxRetTime = 63; // 63 ms
	Prom.UserPasswd = HASH_KEY_USER;
	Prom.SuperPasswd = HASH_KEY_SUPER;
	Prom.MaxConnTime = 4999; // 5 sec
	Prom.MaxRetTime = 63; // 63 ms
	Prom.UserPasswd = HASH_KEY_USER;
	Prom.SuperPasswd = HASH_KEY_SUPER;
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

