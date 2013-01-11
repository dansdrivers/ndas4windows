/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"

#define _LPX_

#define	NR_MAX_TARGET			2
#define MAX_DATA_BUFFER_SIZE	64 * 1024
#define	DISK_SIZE				((INT64)4 * 1024 * 1024 * 1024) // 4Giga
//#define	DISK_SIZE				((INT64)1024 * 1024 * 1024 * 10) // 10Giga
//#define	DISK_SIZE				((INT64)1024 * 1024 * 1024 * 300) // 300Giga
//#define	DISK_SIZE				((INT64)1024 * 1024 * 1024 * 1024 * 2) // 2 Tera
//#define	DISK_SIZE				((INT64)1024 * 1024 * 1024 * 1024 * 4) // 4 Tera
#define	MAX_CONNECTION			16
#define DROP_RATE				0  // out of 1000 packets

#define HASH_KEY_READONLY HASH_KEY_USER
#define HASH_KEY_READWRITE HASH_KEY_USER

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	INT8			NRRWHost;
	INT8			NRROHost;
	UINT64			TargetData;

	// Devices
	NDEMU_DEV		Devices[2];
	PNDEMU_DEV		HighestDev;

} TARGET_DATA, *PTARGET_DATA;


typedef struct	_SESSION_DATA {
	SOCKET	connSock;
	UINT16	TargetID;
	UINT32	LUN;
	INT32	iSessionPhase;
	UINT16	CSubPacketSeq;
	UINT8	iLoginType;
	UINT16	CPSlot;
	UINT32	PathCommandTag;
	UINT32	HPID;
	UINT16	RPID;
	UINT64	SessionId;
	BOOL	bIncCount;
	UINT32	iUser;

	//
	//	Client host's MAC address.
	//	TODO: not yet implemented.
	//

	UCHAR	HostMacAddress[6];

	PUCHAR	DataBuffer;

	ENCRYPTION_INFO	EncryptInfo;
	NDASDIGEST_INFO	DigestInfo;

} SESSION_DATA, *PSESSION_DATA;


// Global Variable.
INT16			G_RPID = 0;
UINT8			thisHWVersion = LANSCSIIDE_CURRENT_VERSION;

INT32			NRTarget;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
SOCKET			listenSock;
SESSION_DATA	sessionData[MAX_CONNECTION];
PROM_DATA		Prom;
RAM_DATA		RamData;

//UINT16	HeaderEncryptAlgo = 1;
//UINT16	DataEncryptAlgo = 1;
UINT16	HeaderEncryptAlgo = 0;
UINT16	DataEncryptAlgo = 0;


//
//	Get session data with session id
//

PSESSION_DATA
GetSessionData(
	IN UINT64	SessionId
){
	return (PSESSION_DATA)(ULONG_PTR)SessionId;
}

VOID
GetHostMacAddressOfSession(
	IN UINT64 SessionId,
	OUT PUCHAR HostMacAddress
){
	PSESSION_DATA	sessionData = GetSessionData(SessionId);

	memcpy(HostMacAddress, sessionData->HostMacAddress, 6);
}

BOOL
RetrieveSectorSize(
	IN PNDEMU_DEV		EmuDev,
	OUT PULONG			SectorSize
){
	BOOL			bret;
	ATADISK_INFO	diskInfo;

	bret = RetrieveAtaDiskInfo(
			EmuDev,
			&diskInfo,
			sizeof(diskInfo)
		);
	if(bret == FALSE) {
		return FALSE;
	}

	*SectorSize = diskInfo.sector_bytes;
	return TRUE;
}

//
//	Session procedure
//

DWORD
WINAPI 
SessionThreadProc(
	LPVOID lpParameter   // thread data
){
	PSESSION_DATA			pSessionData = (PSESSION_DATA)lpParameter;
	INT32						iResult;
	UCHAR					pduBuffer[MAX_REQUEST_SIZE];
	LANSCSI_PDU_POINTERS	pdu;
//	PLANSCSI_H2R_PDU_HEADER	pRequestHeader;
//	PLANSCSI_R2H_PDU_HEADER	pReplyHeader;
	// to support version 1.1, 2.0
	UCHAR					ucParamType;

	//
	// Init variables...
	//
	pSessionData->CSubPacketSeq = 0;
	pSessionData->bIncCount = FALSE;
	pSessionData->iSessionPhase = FLAG_SECURITY_PHASE;
	pSessionData->iLoginType = LOGIN_TYPE_DISCOVERY;

	pSessionData->EncryptInfo.HeaderEncryptAlgo = HeaderEncryptAlgo;
	pSessionData->EncryptInfo.BodyEncryptAlgo = DataEncryptAlgo;

	//
	//	Allocate data buffer for this session
	//

	pSessionData->DataBuffer = (PUCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_DATA_BUFFER_SIZE);
	if(pSessionData->DataBuffer == NULL) {
		fprintf(stderr, "SessionThreadProc: Insufficient resource.\n");
		goto EndSession;
	}

	pdu.pBufferBase = pduBuffer;


	while(pSessionData->iSessionPhase != LOGOUT_PHASE) {

		//
		// Read Request.
		//

		if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
			//	With encryption
			iResult = ReceivePdu(	pSessionData->connSock,
									&pSessionData->EncryptInfo,
									&pSessionData->DigestInfo,
									&pdu);
		} else {
			//	Without encryption
			iResult = ReceivePdu(	pSessionData->connSock,
									NULL,
									NULL,
									&pdu);
		}
		if(iResult <= 0) {
			fprintf(stderr, "Session: Can't Read Request.\n");
			
			pSessionData->iSessionPhase = LOGOUT_PHASE;
			continue;
		}
		switch(pdu.pH2RHeader->Opcode) {
		case LOGIN_REQUEST:
			{
				PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestHeader;
				PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
				PBIN_PARAM_SECURITY					pSecurityParam;
				PAUTH_PARAMETER_CHAP				pAuthChapParam;
				PBIN_PARAM_NEGOTIATION				pParamNego;
				fprintf(stderr, "LOGIN_REQUEST received.\n");

				pLoginRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pdu.pH2RHeader;
				pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

				if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: Bad Command. Invalid login phase\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					
					goto MakeLoginReply;
				} 

				// Check Header.
				if((pLoginRequestHeader->VerMin > thisHWVersion)
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
					
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					goto MakeLoginReply;
				}
				
				// Check Port...
				if(pdu.pH2RHeader->CSubPacketSeq > 0) {
					if((pSessionData->HPID != (UINT32)ntohl(pLoginRequestHeader->HPID))
						|| (pSessionData->RPID != ntohs(pLoginRequestHeader->RPID))
						|| (pSessionData->CPSlot != ntohs(pLoginRequestHeader->CPSlot))
						|| (pSessionData->PathCommandTag != (UINT32)ntohl(pLoginRequestHeader->PathCommandTag))) {
						
						fprintf(stderr, "Session2: Bad Port parameter.\n");
						
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
						goto MakeLoginReply;
					}
				}
				
				switch(ntohs(pdu.pH2RHeader->CSubPacketSeq)) {
				case 0:
					{
						fprintf(stderr, "*** First ***\n");
						// Check Flag.
						if((pLoginRequestHeader->T != 0)
							|| (pLoginRequestHeader->CSG != FLAG_SECURITY_PHASE)
							|| (pLoginRequestHeader->NSG != FLAG_SECURITY_PHASE)) {
							fprintf(stderr, "Session: BAD First Flag.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Check Parameter.

						// to support version 1.1, 2.0
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
							|| (pdu.pDataSeg == NULL)) {							
							fprintf(stderr, "Session: BAD First Request Data.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}	
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
								fprintf(stderr, "Session: BAD First Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {

							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0) {

							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
						// end of supporting version

						if(pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) {

							fprintf(stderr, "Session: BAD First Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Login Type.
						if((pSecurityParam->LoginType != LOGIN_TYPE_NORMAL) 
							&& (pSecurityParam->LoginType != LOGIN_TYPE_DISCOVERY)) {

							fprintf(stderr, "Session: BAD First Login Type.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Auth Type.
						if(!(ntohs(pSecurityParam->AuthMethod) & AUTH_METHOD_CHAP)) {

							fprintf(stderr, "Session: BAD First Auth Method.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Store Data.
						pSessionData->HPID = ntohl(pLoginRequestHeader->HPID);
						pSessionData->CPSlot = ntohs(pLoginRequestHeader->CPSlot);
						pSessionData->PathCommandTag = ntohl(pLoginRequestHeader->PathCommandTag);
						
						pSessionData->iLoginType = pSecurityParam->LoginType;
						
						// Assign RPID...
						pSessionData->RPID = G_RPID;
						
						fprintf(stderr, "Version Min %d, Auth Method %d, Login Type %d\n",
							pLoginRequestHeader->VerMin, ntohs(pSecurityParam->AuthMethod), pSecurityParam->LoginType);
						
						// Make Reply.
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->T = 0;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_SECURITY_PHASE;

						pSecurityParam = (PBIN_PARAM_SECURITY)&pdu.pBufferBase[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pSecurityParam->AuthMethod = htons(AUTH_METHOD_CHAP);

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pLoginReplyHeader->AHSLen = 0;
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
							pdu.pAHS = NULL;
							pdu.pDataSeg = (PCHAR)pSecurityParam;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
							pLoginReplyHeader->DataSegLen = 0;
							pdu.pAHS = (PCHAR)pSecurityParam;
							pdu.pDataSeg = NULL;
						}
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
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}
						
						// Check Parameter.
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Second Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Second Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
						if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
							|| (pSecurityParam->LoginType != pSessionData->iLoginType)
							|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
							
							fprintf(stderr, "Session: BAD Second Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}

						// Hash Algorithm.
						pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
						if(!(ntohl(pAuthChapParam->CHAP_A) & HASH_ALGORITHM_MD5)) {
							fprintf(stderr, "Session: Not Supported HASH Algorithm.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}

						// Store Data.
						pSessionData->EncryptInfo.CHAP_I = ntohl(pAuthChapParam->CHAP_I);

						// Create Challenge
						pSessionData->EncryptInfo.CHAP_C = (rand() << 16) + rand();

						// Make Header
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						pLoginReplyHeader->T = 0;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_SECURITY_PHASE;

						pSecurityParam = (PBIN_PARAM_SECURITY)&pdu.pBufferBase[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						pAuthChapParam = &pSecurityParam->ChapParam;
						pSecurityParam->ChapParam.CHAP_A = htonl(HASH_ALGORITHM_MD5);
						pSecurityParam->ChapParam.CHAP_C[0] = htonl(pSessionData->EncryptInfo.CHAP_C);

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pLoginReplyHeader->AHSLen = 0;
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
							pdu.pAHS = NULL;
							pdu.pDataSeg = (PCHAR)pSecurityParam;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
							pLoginReplyHeader->DataSegLen = 0;
							pdu.pAHS = (PCHAR)pSecurityParam;
							pdu.pDataSeg = NULL;
						}
						// end of supporting version


						printf("CHAP_C %x\n", pSessionData->EncryptInfo.CHAP_C);
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
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}

						// Check Parameter.

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Third Request Data.\n"); 
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Third Request Data.\n"); 
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
						}
						if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
							|| (pSecurityParam->LoginType != pSessionData->iLoginType)
							|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
							
							fprintf(stderr, "Session: BAD Third Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
							goto MakeLoginReply;
						}
						pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
						if(!(ntohl(pAuthChapParam->CHAP_A) == HASH_ALGORITHM_MD5)) {
							fprintf(stderr, "Session: Not Supported HASH Algorithm.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						if((UINT32)ntohl(pAuthChapParam->CHAP_I) != pSessionData->EncryptInfo.CHAP_I) {
							fprintf(stderr, "Session: Bad CHAP_I.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}

						// Store User ID(Name)
						pSessionData->iUser = ntohl(pAuthChapParam->CHAP_N);

						switch(pSessionData->iLoginType) {
						case LOGIN_TYPE_NORMAL:
							{
								BOOL	bRW = FALSE;

								// Target exist?
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

								// Select password.
								if((pSessionData->iUser & 0x00010001)
									|| (pSessionData->iUser & 0x00020002)) {
									pSessionData->EncryptInfo.Password64 = HASH_KEY_READWRITE;
								} else {
									pSessionData->EncryptInfo.Password64 = HASH_KEY_READONLY;
								}

								// Increase Login User Count.
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
								pSessionData->EncryptInfo.Password64 = HASH_KEY_READONLY;								
							}
							break;
						default:
							break;
						}
						
						//
						// Check CHAP_R
						//
						{
							UCHAR	result[16] = { 0 };

							Hash32To128(
								(PUCHAR)&pSessionData->EncryptInfo.CHAP_C, 
								result, 
								(PUCHAR)&pSessionData->EncryptInfo.Password64
								);

							if(memcmp(result, pAuthChapParam->CHAP_R, 16) != 0) {
								fprintf(stderr, "Auth Failed.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeLoginReply;							
							}
						}
						
						// Make Reply.
						pLoginReplyHeader->T = 1;
						pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
						pLoginReplyHeader->NSG = FLAG_LOGIN_OPERATION_PHASE;
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pLoginReplyHeader->AHSLen = 0;
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
							pdu.pAHS = NULL;
							pdu.pDataSeg = (PCHAR)&pdu.pBufferBase[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
							pLoginReplyHeader->DataSegLen = 0;
							pdu.pAHS = (PCHAR)&pdu.pBufferBase[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
							pdu.pDataSeg = NULL;
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
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}

						// Check Parameter.
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pDataSeg == NULL)) {
							
								fprintf(stderr, "Session: BAD Fourth Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeLoginReply;
							}	
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
								|| (pdu.pAHS == NULL)) {
							
								fprintf(stderr, "Session: BAD Fourth Request Data.\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeLoginReply;
							}	
						}

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
						    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
						}
						if((pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION)) {
							fprintf(stderr, "Session: BAD Fourth Request Parameter.\n");
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeLoginReply;
						}
						
						// Make Reply.
						pLoginReplyHeader->T = 1;
						pLoginReplyHeader->CSG = FLAG_LOGIN_OPERATION_PHASE;
						pLoginReplyHeader->NSG = FLAG_FULL_FEATURE_PHASE;
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

						
						pParamNego = (PBIN_PARAM_NEGOTIATION)&pdu.pBufferBase[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
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

						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {

							pLoginReplyHeader->AHSLen = 0;
							pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
							pdu.pAHS = NULL;
							pdu.pDataSeg = (PCHAR)pParamNego;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0) {

							pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
							pLoginReplyHeader->DataSegLen = 0;
							pdu.pAHS = (PCHAR)pParamNego;
							pdu.pDataSeg = NULL;
						}
					}
					break;
				default:
					{
						fprintf(stderr, "Session: BAD Sub-Packet Sequence.\n");
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
						goto MakeLoginReply;
					}
					break;
				}
MakeLoginReply:
				pSessionData->CSubPacketSeq = ntohs(pLoginRequestHeader->CSubPacketSeq) + 1;
				
				pLoginReplyHeader->Opcode = LOGIN_RESPONSE;
				pLoginReplyHeader->VerMax = thisHWVersion;
				pLoginReplyHeader->VerActive = thisHWVersion;
				pLoginReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
				pLoginReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
			}
			break;
		case LOGOUT_REQUEST:
			{
				PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pLogoutRequestHeader;
				PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pLogoutReplyHeader;
				fprintf(stderr, "LOGOUT_REQUEST received.\n");

				pLogoutRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)pdu.pH2RHeader;
				pLogoutReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: LOGOUT Bad Command.\n");
					pLogoutReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					
					goto MakeLogoutReply;
				}

				// Check Header.
				if((pLogoutRequestHeader->F == 0)
					|| (pSessionData->HPID != (UINT32)ntohl(pLogoutRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pLogoutRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pLogoutRequestHeader->CPSlot))
					|| (0 != ntohs(pLogoutRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: LOGOUT Bad Port parameter.\n");
					
					pLogoutReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
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

MakeLogoutReply:
				pLogoutReplyHeader->Opcode = LOGOUT_RESPONSE;
			}
			break;
		case TEXT_REQUEST:
			{
				PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;

				pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)pdu.pH2RHeader;
				pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;

				fprintf(stderr, "TEXT_REQUEST received.\n");

				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: TEXT_REQUEST Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
					
					goto MakeTextReply;
				}
				
				// Check Header.
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (UINT32)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: TEXT Bad Port parameter.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeTextReply;
				}
				
				// Check Parameter.

				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
					if(ntohl(pRequestHeader->DataSegLen) < 4) {	// Minimum size.
						fprintf(stderr, "Session: TEXT No Data seg.\n");

						pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
						goto MakeTextReply;
					}
				}
				if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
					if(ntohs(pRequestHeader->AHSLen) < 4) {	// Minimum size.
						fprintf(stderr, "Session: TEXT No Data seg.\n");
					
						pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
						goto MakeTextReply;
					}
				}
				// end of supporting version

				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
					ucParamType = ((PBIN_PARAM)pdu.pDataSeg)->ParamType;
				}
				if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
					ucParamType = ((PBIN_PARAM)pdu.pAHS)->ParamType;
				}
				// end of supporting version

//				switch(((PBIN_PARAM)pdu.pDataSeg)->ParamType) {
				switch(ucParamType) {
				case BIN_PARAM_TYPE_TARGET_LIST:
					{
						PBIN_PARAM_TARGET_LIST	pParam;

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
						pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
						}						
						pParam->NRTarget = (UCHAR)NRTarget;	
						for(INT32 i = 0; i < pParam->NRTarget; i++) {
							pParam->PerTarget[i].TargetID = htonl(i);
							pParam->PerTarget[i].NRRWHost = PerTarget[i].NRRWHost;
							pParam->PerTarget[i].NRROHost = PerTarget[i].NRROHost;
							pParam->PerTarget[i].TargetData = PerTarget[i].TargetData;
						}

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pReplyHeader->AHSLen = 0;
							pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
							pReplyHeader->DataSegLen = 0;
						}
						// end of supporting version

					}
					break;
				case BIN_PARAM_TYPE_TARGET_DATA:
					{
						PBIN_PARAM_TARGET_DATA pParam;

						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pParam = (PBIN_PARAM_TARGET_DATA)pdu.pAHS;
						}
						// end of supporting version
						
						if(pParam->GetOrSet == PARAMETER_OP_SET) {
							if(ntohl(pParam->TargetID) == 0) {
								if(!(pSessionData->iUser & 0x00000001) 
									||!(pSessionData->iUser & 0x00010000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[0].TargetData = pParam->TargetData;

							} else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002) 
									||!(pSessionData->iUser & 0x00020000)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeTextReply;
								}

								PerTarget[1].TargetData = pParam->TargetData;
							} else {
								fprintf(stderr, "No Access Right\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeTextReply;
							}
						} else {
							if(ntohl(pParam->TargetID) == 0) {
								if(!(pSessionData->iUser & 0x00000001)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeTextReply;
								}

								pParam->TargetData = PerTarget[0].TargetData;

							} else if(ntohl(pParam->TargetID) == 1) {
								if(!(pSessionData->iUser & 0x00000002)) {
									fprintf(stderr, "No Access Right\n");
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeTextReply;
								}

								pParam->TargetData = PerTarget[1].TargetData;
							} else {
								fprintf(stderr, "No Access Right\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeTextReply;
							}
						}

						
						// to support version 1.1, 2.0 
						if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
							pReplyHeader->AHSLen = 0;
							pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
						}
						if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				    		    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
							pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
							pReplyHeader->DataSegLen = 0;
						}
						// end of supporting version
					}
					break;
				default:
					break;
				}
				
		
				// to support version 1.1, 2.0 
				if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
					pReplyHeader->AHSLen = 0;
					pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
				}
				if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
		    		    thisHWVersion == LANSCSIIDE_VERSION_2_0) {
					pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
					pReplyHeader->DataSegLen = 0;
				}
				// end of supporting version
MakeTextReply:
				pReplyHeader->Opcode = TEXT_RESPONSE;
			}
			break;
		case IDE_COMMAND:
			{
//				fprintf(stderr, "IDE_COMMAND received.\n");

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader;
				ULONG							dataTransferLength;
				PNDEMU_DEV						targetDev;
				ATA_COMMAND						ataCommand;

				pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)pdu.pH2RHeader;
				pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.pR2HHeader;

				if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Not Normal Login.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;

					goto MakeIDEReply;
				}
				
				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;

					goto MakeIDEReply;
				}

				// Check Header.
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (UINT32)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
					
					fprintf(stderr, "Session2: IDE Bad Port parameter.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeIDEReply;
				}
				
				//
				//	Look up the target ATA Disk
				// Request for existed target?
				//
				if(PerTarget[ntohl(pRequestHeader->TargetID)].bPresent == FALSE) {
					fprintf(stderr, "Session2: Target Not exist.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
					goto MakeIDEReply;
				}

				targetDev = PerTarget[ntohl(pRequestHeader->TargetID)].HighestDev;

				//
				// Calculate data transfer length
				//	TODO: Use DataTransferLength instead of register fields
				//

				if(pRequestHeader->DataTransferLength) {
					dataTransferLength = ntohl(pRequestHeader->DataTransferLength);
				} else {
					fprintf(stderr, "Session2: No DataTransferLength\n");


					if(	pRequestHeader->Command == WIN_IDENTIFY ||
						pRequestHeader->Command == WIN_PIDENTIFY) {

							dataTransferLength = 512;
					} else {
						BOOL	bret;
						ULONG	sectorBytes;

						bret =  RetrieveSectorSize(targetDev, &sectorBytes);
						if(bret == FALSE) {
							fprintf(stderr, "Session2: Failed to get sector size.\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
						}

						dataTransferLength = (((UINT32)pRequestHeader->SectorCount_Prev << 8)
							+ (pRequestHeader->SectorCount_Curr)) * sectorBytes;
					}
				}

				//
				//	Receive data if followed
				//

				if(pRequestHeader->W) {
					// Check access right.
					if((pSessionData->iUser != FIRST_TARGET_RW_USER)
						&& (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
							fprintf(stderr, "Session2: No Write right...\n");

							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply;
					}
					// Receive Data.
					iResult = ReceiveBody(
							pSessionData->connSock,
							&pSessionData->EncryptInfo,
							NULL,
							dataTransferLength,
							MAX_DATA_BUFFER_SIZE,
							pSessionData->DataBuffer
						);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "ReceiveBody: Can't Recv Data...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						continue;
					}

				}


				//
				//	Convert PDU IDE command to ATA command
				//	Pass the command to the target ATA disk
				//

				ConvertPDUIde2ATACommand(pRequestHeader, pSessionData->DataBuffer, dataTransferLength, &ataCommand);
				NdemuDevRequest(targetDev, &ataCommand);
				SetPDUIdeResult(&ataCommand, pRequestHeader);

				if(ataCommand.Command & ERR_STAT) {
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				} else {
					pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
				}

MakeIDEReply:
				if(pRequestHeader->R) {

					// Send Data.
					iResult = SendBody(
						pSessionData->connSock,
						&pSessionData->EncryptInfo,
						NULL,
						dataTransferLength,
						MAX_DATA_BUFFER_SIZE,
						pSessionData->DataBuffer);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "SendBody: Can't Send reply Data...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						continue;
					}
				}

				pReplyHeader->Opcode = IDE_RESPONSE;


			} else if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				   thisHWVersion == LANSCSIIDE_VERSION_2_0) {
				PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
				PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
				ULONG								dataTransferLength;
				PNDEMU_DEV							targetDev;
				ATA_COMMAND							ataCommand;

				pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)pdu.pH2RHeader;
				pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;

				if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Not Normal Login.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;

					goto MakeIDEReply1;
				}

				if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
					// Bad Command...
					fprintf(stderr, "Session2: IDE_COMMAND Bad Command.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;

					goto MakeIDEReply1;
				}

				// Check Header.
				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (UINT32)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {

					fprintf(stderr, "Session2: IDE Bad Port parameter.\n");

					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeIDEReply1;
				}

				//
				//	Look up the target ATA Disk
				// Request for existed target?
				//
				if(PerTarget[ntohl(pRequestHeader->TargetID)].bPresent == FALSE) {
					fprintf(stderr, "Session2: Target Not exist.\n");
					
					pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
					goto MakeIDEReply1;
				}

				targetDev = PerTarget[ntohl(pRequestHeader->TargetID)].HighestDev;

				//
				// Calculate data transfer length
				//	TODO: Use DataTransferLength instead of register fields
				//

				if(pRequestHeader->DataTransferLength) {
					dataTransferLength = ntohl(pRequestHeader->DataTransferLength);
				} else {

					if(	pRequestHeader->Command == WIN_IDENTIFY ||
						pRequestHeader->Command == WIN_PIDENTIFY) {

						fprintf(stderr, "Session2: No DataTransferLength\n");
						dataTransferLength = 512;
					} else {

						if(pRequestHeader->Command != WIN_SETFEATURES) {

							BOOL	bret;
							ULONG	sectorBytes;

							fprintf(stderr, "Session2: No DataTransferLength\n");

							bret =  RetrieveSectorSize(targetDev, &sectorBytes);
							if(bret == FALSE) {
								fprintf(stderr, "Session2: Failed to get sector size.\n");
								pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeIDEReply;
							}

							dataTransferLength = (((UINT32)pRequestHeader->SectorCount_Prev << 8)
								+ (pRequestHeader->SectorCount_Curr)) * sectorBytes;
						}
					}
				}

				//
				//	Receive data if followed
				//

				if(pRequestHeader->W) {
					// Check access right.
					if((pSessionData->iUser != FIRST_TARGET_RW_USER)
						&& (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
							fprintf(stderr, "Session2: No Write right...\n");

							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeIDEReply1;
					}
					// Receive Data.
					iResult = ReceiveBody(
							pSessionData->connSock,
							&pSessionData->EncryptInfo,
							NULL,
							dataTransferLength,
							MAX_DATA_BUFFER_SIZE,
							pSessionData->DataBuffer
						);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "ReceiveBody: Can't Recv Data...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						continue;
					}

				}


				//
				//	Convert PDU IDE command to ATA command
				//	Pass the command to the target ATA disk
				//

				ConvertPDUIde2ATACommandV1(pRequestHeader, pSessionData->DataBuffer, dataTransferLength, &ataCommand);
				NdemuDevRequest(targetDev, &ataCommand);
				SetPDUIdeResultV1(&ataCommand, pRequestHeader);

				if(ataCommand.Command & ERR_STAT) {
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				} else {
					pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
				}

MakeIDEReply1:
				if(pRequestHeader->R) {

					iResult = SendBody(
						pSessionData->connSock,
						&pSessionData->EncryptInfo,
						NULL,
						dataTransferLength,
						MAX_DATA_BUFFER_SIZE,
						pSessionData->DataBuffer);
					if(iResult == SOCKET_ERROR) {
						fprintf(stderr, "SessionThreadProc: Can't Send body...\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;

						continue;
					}

				}

				pReplyHeader->Opcode = IDE_RESPONSE;
			}

			}
			break;

		case VENDOR_SPECIFIC_COMMAND:
			{
				PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
				PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;

				fprintf(stderr, "VENDOR_SPECIFIC_COMMAND received.\n");
				pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)pdu.pH2RHeader;
				pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.pR2HHeader;

				// Set initial response status
				pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

				if((pRequestHeader->F == 0)
					|| (pSessionData->HPID != (UINT32)ntohl(pRequestHeader->HPID))
					|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
					|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
					|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {

					fprintf(stderr, "Session2: Vendor Bad Port parameter.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeVendorReply;
				}

				if( (pRequestHeader->VendorID != htons(NKC_VENDOR_ID)) ||
				    (pRequestHeader->VendorOpVersion != VENDOR_OP_CURRENT_VERSION) 
				) {
					fprintf(stderr, "Session2: Vendor Version don't match.\n");
					pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeVendorReply;
				}
				switch(pRequestHeader->VendorOp) {
					case VENDOR_OP_SET_MAX_RET_TIME:
						fprintf(stderr, "Vendor: SET_MAX_RET_TIME\n");
						Prom.MaxRetTime = NTOHLL(pRequestHeader->VendorParameter);
						break;
					case VENDOR_OP_SET_MAX_CONN_TIME:
						fprintf(stderr, "Vendor: SET_MAX_CONN_TIME\n");
						Prom.MaxConnTime = NTOHLL(pRequestHeader->VendorParameter);
						break;
					case VENDOR_OP_GET_MAX_RET_TIME:
						fprintf(stderr, "Vendor: SET_MAX_RET_TIME\n");
						pRequestHeader->VendorParameter = HTONLL(Prom.MaxRetTime);
						break;
					case VENDOR_OP_GET_MAX_CONN_TIME:
						fprintf(stderr, "Vendor: GET_MAX_CONN_TIME\n");
						pRequestHeader->VendorParameter = HTONLL(Prom.MaxConnTime);
						break;
					case VENDOR_OP_SET_SEMA: {
						BOOL	bret;

						fprintf(stderr, "Vendor: SET_SEMA\n");
						if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

							bret = VendorSetLock11(
											&RamData,
											pSessionData->SessionId,
											pRequestHeader,
											pReplyHeader);
							if(bret == FALSE) {
								pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
								goto MakeVendorReply;
							}

						} else {
							fprintf(stderr, "Session2: Vendor version don't match.\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeVendorReply;
						}
						break;
					}
					case VENDOR_OP_FREE_SEMA: {
						BOOL	bret;

						fprintf(stderr, "Vendor: FREE_SEMA\n");
						if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

								bret = VendorFreeLock11(
									&RamData,
									pSessionData->SessionId,
									pRequestHeader,
									pReplyHeader);
								if(bret == FALSE) {
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeVendorReply;
								}

						} else {
							fprintf(stderr, "Session2: Vendor version don't match.\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeVendorReply;
						}
						break;
					}
					case VENDOR_OP_GET_SEMA: {
						BOOL	bret;

						fprintf(stderr, "Vendor: GET_SEMA\n");
						if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

								bret = VendorGetLock11(
									&RamData,
									pRequestHeader,
									pReplyHeader);
								if(bret == FALSE) {
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeVendorReply;
								}

						} else {
							fprintf(stderr, "Session2: Vendor version don't match.\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeVendorReply;
						}
						break;
					}
					case VENDOR_OP_OWNER_SEMA: {
						BOOL	bret;

						fprintf(stderr, "Vendor: OWNER_SEMA\n");
						if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
							thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

								bret = VendorGetLockOwner11(
									&RamData,
									pSessionData->SessionId,
									pRequestHeader,
									pReplyHeader);
								if(bret == FALSE) {
									pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeVendorReply;
								}

						} else {
							fprintf(stderr, "Session2: Vendor version don't match.\n");
							pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
							goto MakeVendorReply;
						}
						break;
					}
					case VENDOR_OP_SET_SUPERVISOR_PW:
						fprintf(stderr, "Vendor: SET_SUPERVISOR_PW\n");
						Prom.SuperPasswd = NTOHLL(pRequestHeader->VendorParameter);
						break;
					case VENDOR_OP_SET_USER_PW:
						fprintf(stderr, "Vendor: SET_USER_PW\n");
						Prom.UserPasswd = NTOHLL(pRequestHeader->VendorParameter);
						break;
					case VENDOR_OP_RESET:
						fprintf(stderr, "Vendor: RESET\n");
						pSessionData->iSessionPhase = LOGOUT_PHASE;
						break;
					default:
						break;
				}
MakeVendorReply:
				pReplyHeader->Opcode = VENDOR_SPECIFIC_RESPONSE;
			}
			break;

		case NOP_H2R:
			fprintf(stderr, "NOP received.\n");
			// no op. Do not send reply
			break;
		default:
			fprintf(stderr, "!! Bad opcode:%d\n", pdu.pH2RHeader->Opcode);
			// Bad Command...
			break;
		}


		//
		//	Send reply
		//

		if (pdu.pH2RHeader->Opcode==NOP_H2R) {
			// Do not send reply
			continue;
		}
		{

			// Send Reply.
			pdu.pR2HHeader->HPID = htonl(pSessionData->HPID);
			pdu.pR2HHeader->RPID = htons(pSessionData->RPID);
			pdu.pR2HHeader->CPSlot = htons(pSessionData->CPSlot);
			pdu.pR2HHeader->CSubPacketSeq = htons(pSessionData->CSubPacketSeq);
			pdu.pR2HHeader->PathCommandTag = htonl(pSessionData->PathCommandTag);

			if(	pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE ||
				pSessionData->iSessionPhase == LOGOUT_PHASE)
				iResult = SendPdu(pSessionData->connSock,
									&pSessionData->EncryptInfo,
									&pSessionData->DigestInfo,
									&pdu);
			else	
				iResult = SendPdu(pSessionData->connSock, NULL, NULL, &pdu);

			if(iResult == SOCKET_ERROR) {
				fprintf(stderr, "SendPdu: Can't Send Reply PDU\n");
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				continue;
			}
		}

		//
		//	LogIn PDU post-process
		//

		if((pdu.pR2HHeader->Opcode == LOGIN_RESPONSE)
			&& (pSessionData->CSubPacketSeq == 4)) {
			pSessionData->CSubPacketSeq = 0;
			pSessionData->iSessionPhase = FLAG_FULL_FEATURE_PHASE;
		}
	}
	
EndSession:

	fprintf(stderr, "Session2: Logout Phase.\n");

	switch(pSessionData->iLoginType) {
	case LOGIN_TYPE_NORMAL:
		{
			if(pSessionData->bIncCount == TRUE) {

				// Decrease Login User Count.
				// TODO: use atomic operation
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

	if(pSessionData->DataBuffer)
		HeapFree(GetProcessHeap(), 0, pSessionData->DataBuffer);
	CleanupLock11(&RamData,pSessionData->SessionId);
	closesocket(pSessionData->connSock);
	pSessionData->connSock = INVALID_SOCKET;

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//	PnP Message broadcasting
//

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
	INT32 result;
	INT32 broadcastPermission;
	PNP_MESSAGE message;
	INT32 i = 0;
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
	slpx.LpxAddress.Port = htons(BROADCAST_SOURCEPORT_NUMBER);

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
	slpx.LpxAddress.Port = htons(BROADCAST_DESTPORT_NUMBER);
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

		//
		//	Delay 2 seconds
		//

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


//////////////////////////////////////////////////////////////////////////
//
//	Main: Configure emulation modules
//

INT32
__cdecl
main(INT32 argc, char* argv[])
{
	WORD				wVersionRequested;
	WSADATA				wsaData;
	INT32					err;
	INT32					i;
	BOOL				bRet;
#ifdef _LPX_
	SOCKADDR_LPX		address;
#else
	struct sockaddr_in	servaddr;
#endif
	HANDLE				hBThread;
	NDEMU_ATADISK_INIT	ataDiskInit;

#ifdef __TMD_SUPPORT__
	NDEMU_TMDISK_INIT	tmDiskInit;
#endif

	// Setting packet drop rate
	bRet = LpxSetDropRate(DROP_RATE);
	if (bRet == FALSE) {
		fprintf(stderr, "Failed to set drop rate\n");
	} else {
		fprintf(stderr, "Set drop rate to %d\n", DROP_RATE);
	}


	// Init.
	NRTarget = 1;
	RamData.LockMutex = CreateMutex(NULL, FALSE, NULL);
	if(RamData.LockMutex == NULL) {
		PrintError(GetLastError(), "CreateMutex");
		return -1;
	}


	//
	//	Target 0
	//

	PerTarget[0].bPresent = TRUE;
	PerTarget[0].NRRWHost = 0;
	PerTarget[0].NRROHost = 0;
	PerTarget[0].TargetData = 0;

	// ATA Disk
	ataDiskInit.DeviceId = 0;
#ifndef __ENABLE_SPARSE_FILE__
	ataDiskInit.UseSparseFile = FALSE;
#else
	if(GetCurrentVolumeSparseFlag() == TRUE) {
		ataDiskInit.UseSparseFile = TRUE;
	} else {
		_ftprintf(stderr, _T("Current FS volume does not support sparse file.\n"));
	}
#endif

#ifndef __ENABLE_SECTOR_4096__
	ataDiskInit.BytesInBlock = 512;
	ataDiskInit.BytesInBlockBitShift = 9;
#else
	ataDiskInit.BytesInBlock = 4096;
	ataDiskInit.BytesInBlockBitShift = 12;
#endif

	ataDiskInit.Capacity = DISK_SIZE / ataDiskInit.BytesInBlock;

	bRet = NdemuDevInitialize(&PerTarget[0].Devices[0], NULL, &ataDiskInit, NDEMU_DEVTYPE_ATADISK);
	if(bRet == FALSE) {
		_tprintf(_T("Could not initalize disk device\n"));
		return -1;
	}

	PerTarget[0].HighestDev = &PerTarget[0].Devices[0];

#ifdef __TMD_SUPPORT__
	// TM Disk filter
	tmDiskInit.DeviceId = 0;

	bRet = NdemuDevInitialize(&PerTarget[0].Devices[1], &PerTarget[0].Devices[0], &tmDiskInit, NDEMU_DEVTYPE_TMDISK);
	if(bRet == FALSE) {
		_tprintf(_T("Could not initalize disk device\n"));
		return -1;
	}

	PerTarget[0].HighestDev = &PerTarget[0].Devices[1];
#endif

#ifdef __TWO_DISK__
	//
	//	Target 1
	//
	NRTarget ++;

	PerTarget[1].bPresent = TRUE;
	PerTarget[1].NRRWHost = 0;
	PerTarget[1].NRROHost = 0;
	PerTarget[1].TargetData = 0;

	// ATA Disk
	ataDiskInit.DeviceId = 1;
	ataDiskInit.Capacity = DISK_SIZE / ataDiskInit.BytesInBlock;

	bRet = NdemuDevInitialize(&PerTarget[1].Devices[0], NULL, &ataDiskInit, NDEMU_DEVTYPE_ATADISK);
	if(bRet == FALSE) {
		_tprintf(_T("Could not initalize disk device\n"));
		return -1;
	}

	PerTarget[1].HighestDev = &PerTarget[1].Devices[0];

#ifdef __TMD_SUPPORT__
	// TM Disk filter
	tmDiskInit.DeviceId = 1;

	bRet = NdemuDevInitialize(&PerTarget[1].Devices[1], &PerTarget[1].Devices[0], &tmDiskInit, NDEMU_DEVTYPE_TMDISK);
	if(bRet == FALSE) {
		_tprintf(_T("Could not initalize disk device\n"));
		return -1;
	}

	PerTarget[1].HighestDev = &PerTarget[1].Devices[1];
#endif
#else

	PerTarget[1].bPresent = FALSE;
	PerTarget[1].NRRWHost = 0;
	PerTarget[1].NRROHost = 0;
	PerTarget[1].TargetData = 0;

#endif

	//
	// EEPROM
	//

	Prom.MaxConnTime = 4999; // 5 sec
	Prom.MaxRetTime = 63; // 63 ms
	Prom.UserPasswd = HASH_KEY_USER;
	Prom.SuperPasswd = HASH_KEY_SUPER;
	Prom.MaxConnTime = 4999; // 5 sec
	Prom.MaxRetTime = 63; // 63 ms
	Prom.UserPasswd = HASH_KEY_USER;
	Prom.SuperPasswd = HASH_KEY_SUPER;
	srand((UINT32)time(NULL));

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
	address.LpxAddress.Port = htons(NDASDEV_LISTENPORT_NUMBER);

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
	servaddr.sin_port = htons(NDASDEV_LISTENPORT_NUMBER);

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
		INT32		iptr;
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

		//
		//	Init session data
		//

		sessionData[iptr].connSock = tempSock;

		// Create session ID
		sessionData[iptr].SessionId = (UINT64)(ULONG_PTR)&sessionData[iptr];

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

	NdemuDevDestroy(&PerTarget[1].Devices[0]);
	NdemuDevDestroy(&PerTarget[0].Devices[0]);

	// Clean RamData

	CloseHandle(RamData.LockMutex);

	return 0;
}

