// ndascli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


// NdasCli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <time.h>
#include <stdio.h>
#include <tCHAR.h>


//
// Define BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX and for BUILD_FOR_DIST
//		to build ndascli version that fixes incorrect 1.1 NDAS EEPROM problem.
//
//#define BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX
//#define BUILD_FOR_DIST

//#define SAMPLE_MAC " 00:0b:d0:00:ff:d1 "
//#define SAMPLE_MAC " 00:0b:d0:fe:02:3c "
#define SAMPLE_MAC " 00:0c:29:21:26:ea " // vmware

#define Decrypt32 Decrypt32_l
#define Encrypt32 Encrypt32_l
#define Hash32To128 Hash32To128_l


// Global Variable.
extern _int32			HPID;
extern _int16			RPID;
extern _int32			iTag;
int				NRTarget;
extern UINT		CHAP_C[];

extern UINT16	requestBlocks;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
UINT16  MaxPendingTasks = 0;
extern UINT16	HeaderEncryptAlgo;
extern UINT16	DataEncryptAlgo;
extern UINT16	HeaderDigestAlgo;
extern UINT16	DataDigestAlgo;
extern INT		iSessionPhase;
extern int		ActiveHwVersion; // set at login time

extern INT		ActiveHwRevision;
extern UINT32	ActiveUserId;
extern INT		iTargetID;


UCHAR def_password0[] = {0x1f, 0x4a, 0x50, 0x73, 0x15, 0x30, 0xea, 0xbb,
	0x3e, 0x2b, 0x32, 0x1a, 0x47, 0x50, 0x13, 0x1e};

UCHAR def_supervisor_password[] = {
	0xA3, 0x07, 0xA9, 0xAA, 0x33, 0xC5, 0x18, 0xA8,
	0x64, 0x94, 0x2A, 0xD1, 0x15, 0x7A, 0x1B, 0x30
};

extern UCHAR cur_password[];

VOID
PrintHex (
	UCHAR	*Buf, 
	INT		len
	)
{
	INT i;
	
	for(i=0;i<len;i++) {
	
		printf( "%02X", Buf[i] );

		if ((i+1)%4==0) {

			printf(" ");
		}

		if ((i+1)%32==0) {

			printf("\n");
		}
	}
}

VOID
PrintError (
	INT		ErrorCode,
	PCHAR	prefix
	);

VOID
PrintErrorCode (
	PCHAR	prefix,
	INT		ErrorCode
	);

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


//	fprintf(stderr, "RecvIt %d ", size);

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
					
					PrintErrorCode(TEXT("[NdasCli]RecvIt: "), dwError);
					dwRecvDataLen = -1;
					
					printf("[NdasCli]RecvIt: Request %d, Received %d\n",
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
					PrintErrorCode(TEXT("[NdasCli]RecvIt: GetOverlappedResult Failed "), GetLastError());
					dwRecvDataLen = SOCKET_ERROR;
					goto Out;
				}
				
			} else {
				PrintErrorCode(TEXT("[NdasCli]RecvIt: WSARecv Failed "), dwError);
				
				dwRecvDataLen = -1;
				goto Out;
			}
		}

		iReceived += dwRecvDataLen;
		
		WSAResetEvent(hEvent);
	}

Out:
	WSACloseEvent(hEvent);	

//	fprintf(stderr, "-- done \n");
  
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

//	fprintf(stderr, "SendIt %d ", size);	
	
	while (len > 0) {
		
		if ((res = send(sock, buf, len, 0)) <= 0) {
			PrintError(WSAGetLastError(), _T("SendIt "));
			return res;
		}
		len -= res;
		buf += res;
	}

//	fprintf(stderr, "-- done \n");
	
	return size;
}


int
SendBadRequest(
			SOCKET			connSock,
			PLANSCSI_PDU_POINTERS	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;
	iDataSegLen = ntohs(pHeader->AHSLen);

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderDigestAlgo != 0) {

		CRC32(
			(UCHAR*)pHeader,
			&(((UCHAR*)pHeader)[sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen]),
			sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
			);
		iDataSegLen += 4;
	}

	// Corrupt header data
	pHeader->Reserved1 ^=0x1;

	//
	// Encrypt Header.
	//
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			Encrypt128(
				(UCHAR*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(UCHAR *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "SendRequest: Encrypt Header 1 !!!!!!!!!!!!!!!...\n");
		} else {
			Encrypt32(
				(UCHAR*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(UCHAR *)&CHAP_C,
				(UCHAR*)&cur_password
				);
		}
	}
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		// Send Request.
		iResult = SendIt(
			connSock,
			(PCHAR)pHeader,
			(sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + 15) & 0xfffffff0 // Align 16 byte.
			);
	} else {
		// Send Request.
		iResult = SendIt(
			connSock,
			(PCHAR)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
			);
	}
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), _T("SendRequest: Send Request "));
		return -1;
	}
	return 0;
}





int
NopCommand(
			   SOCKET	connsock
)
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_H2R_PDU_HEADER	pRequestHeader;
	PLANSCSI_R2H_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_H2R_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = NOP_H2R;
//	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->PathCommandTag = htonl(++iTag);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("NOP: Send First Request "));
		return -1;
	}
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		// Read Request.
		iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "[NdasCli]NOP: Can't Read Reply.\n");
			return -1;
		}
		
		// Check Request Header.
		pReplyHeader = (PLANSCSI_R2H_PDU_HEADER)pdu.pR2HHeader;


		if(pReplyHeader->Opcode != NOP_R2H) {
			fprintf(stderr, "[NdasCli]NOP: BAD Reply Header.\n");
			return -1;
		}
		
		if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
			fprintf(stderr, "[NdasCli]NOP: Failed.\n");
			return -1;
		}
	} else {
		fprintf(stderr, "<2.5 chip. Do not wait NOP reply\n");
	}
	return 0;
}

int
SendIdeCommandRequestAndData(
		   SOCKET	connsock,
		   PNDASCLI_TASK Task
){
	_int8							PduBuffer[MAX_REQUEST_SIZE];

	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REQUEST_PDU_HEADER	pRequestHeader_v0;
	LANSCSI_PDU_POINTERS						pdu;
	int								iResult;
	//unsigned						DataLength;
	//unsigned						crc;

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;
	pRequestHeader_v0 = (PLANSCSI_IDE_REQUEST_PDU_HEADER) PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	pRequestHeader->DataSegLen = 0;
	pRequestHeader->AHSLen = 0;
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	Task->TaskTag = iTag;
	pRequestHeader->TargetID = htonl(Task->TargetId);
	pRequestHeader->LUN = 0;
	
//	fprintf(stderr, "Task->IdeCoammand = %8x, Task->Option = %8x, Task->Location = %lld, Task->SectorCount = %d, Task->TargetID = %d\n", 
//		Task->IdeCommand, Task->Option, Task->Location, Task->SectorCount, Task->TargetId);

	// Using Target ID. LUN is always 0.
	pRequestHeader->DEV = Task->TargetId;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pRequestHeader_v0->Feature = 0;
	} else {
		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr = 0;
		if (Task->Option & IDECMD_OPT_UNLOCK_BUFFER_LOCK) {
			pRequestHeader->U = 1;
		}
	}

	switch(Task->IdeCommand) {
	case WIN_READ:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
					pRequestHeader_v0->Command = WIN_READDMA_EXT;
				} else {
					pRequestHeader_v0->Command = WIN_READDMA;
				}
			} else {
				if (PerTarget[Task->TargetId].bPIO == TRUE) {
					if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
						pRequestHeader->Command = WIN_READ_EXT;
						pRequestHeader->COM_TYPE_E = '1';
					} else {
						pRequestHeader->Command = WIN_READ;
					}
					pRequestHeader->COM_TYPE_D_P = '0';	
				} else {
					if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
						pRequestHeader->Command = WIN_READDMA_EXT;
						pRequestHeader->COM_TYPE_E = '1';
					} else {
						pRequestHeader->Command = WIN_READDMA;
					}
					pRequestHeader->COM_TYPE_D_P = '1';
				}

				pRequestHeader->COM_TYPE_R = '1';
				pRequestHeader->COM_LENG = (htonl(Task->SectorCount*512) >> 8);
				pRequestHeader->DataTransferLength = htonl(Task->SectorCount*512);
			}

		}
		break;
	case WIN_WRITE:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 1;
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
					pRequestHeader_v0->Command = WIN_WRITEDMA_EXT;
				} else {
					pRequestHeader_v0->Command = WIN_WRITEDMA;
				}
			} else {
				if (PerTarget[Task->TargetId].bPIO == TRUE) {
					if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
						pRequestHeader->Command = WIN_WRITE_EXT;
						pRequestHeader->COM_TYPE_E = '1';
					} else {
						pRequestHeader->Command = WIN_WRITE;
					}
					pRequestHeader->COM_TYPE_D_P = '0';	
				} else {
					if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
						pRequestHeader->Command = WIN_WRITEDMA_EXT;
						pRequestHeader->COM_TYPE_E = '1';
					} else {
						pRequestHeader->Command = WIN_WRITEDMA;
						pRequestHeader->COM_TYPE_E = '0';
					}
					pRequestHeader->COM_TYPE_D_P = '1';
				}
				pRequestHeader->COM_TYPE_W = '1';
			
#if WRITE_BEBUG
				pRequestHeader->COM_LENG = (htonl(Task->SectorCount*512*2) >> 8);
#else
				pRequestHeader->COM_LENG = (htonl(Task->SectorCount*512) >> 8);
#endif
				pRequestHeader->DataTransferLength = htonl(Task->SectorCount*512);
			}
		}
		break;
	case WIN_VERIFY:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
					pRequestHeader_v0->Command = WIN_VERIFY_EXT;
				} else {
					pRequestHeader_v0->Command = WIN_VERIFY;
				}
			} else {
				if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
					pRequestHeader->Command = WIN_VERIFY_EXT;
					pRequestHeader->COM_TYPE_E = '1';
				} else {
					pRequestHeader->Command = WIN_VERIFY;
				}
			}
		}
		break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{
			pRequestHeader->R = 1;
			pRequestHeader->W = 0;
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				pRequestHeader_v0->Command = WIN_IDENTIFY;
			} else {
				pRequestHeader->Command = Task->IdeCommand;
				//pRequestHeader->Command = 0xa1;

				pRequestHeader->COM_TYPE_R = '1';
				pRequestHeader->COM_LENG = (htonl(1*512) >> 8);	
				pRequestHeader->DataTransferLength = htonl(1*512);
			}			
		}
		break;
	case WIN_DEV_CONFIG: {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
			return -1;
		}
		pRequestHeader->Feature_Prev = 0;
		pRequestHeader->Feature_Curr= Task->Feature;
		pRequestHeader->SectorCount_Curr = (unsigned _int8)Task->SectorCount;
		pRequestHeader->Command = Task->IdeCommand;

		if(Task->Feature == DEVCONFIG_CONFIG_IDENTIFY) {

			pRequestHeader->R = 1;
			pRequestHeader->W = 0;

			pRequestHeader->COM_TYPE_R = '1';
			pRequestHeader->COM_LENG = (htonl(1*512) >> 8);	
			pRequestHeader->DataTransferLength = htonl(1*512);

		} if(Task->Feature == DEVCONFIG_CONFIG_SET) {

			pRequestHeader->R = 0;
			pRequestHeader->W = 1;

			pRequestHeader->COM_TYPE_W = '1';
			pRequestHeader->COM_LENG = (htonl(1*512) >> 8);	
			pRequestHeader->DataTransferLength = htonl(1*512);

		} else {

			pRequestHeader->R = 0;
			pRequestHeader->W = 0;

		}

	}
	case WIN_SETFEATURES:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				pRequestHeader_v0->Feature = Task->Feature;
				pRequestHeader_v0->SectorCount_Curr = (unsigned _int8)Task->SectorCount;
				pRequestHeader_v0->Command = WIN_SETFEATURES;
			} else {
				pRequestHeader->Feature_Prev = 0;
				pRequestHeader->Feature_Curr= Task->Feature;
				pRequestHeader->SectorCount_Curr = (unsigned _int8)Task->SectorCount;
				pRequestHeader->Command = WIN_SETFEATURES;
			}
//			fprintf(stderr, "[NdasCli]IDECommand: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr);
		}
		break;
	case WIN_SETMULT:
		{
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr= 0;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)Task->SectorCount;
			pRequestHeader->Command = WIN_SETMULT;
		}
		break;
	case WIN_CHECKPOWERMODE1:
		{
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				fprintf(stderr, "NDAS 1.0 does not support command %x\n", Task->IdeCommand);
			}
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr= 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = WIN_CHECKPOWERMODE1;
		}
		break;
	case WIN_STANDBY:
	case WIN_STANDBYNOW1:
	case WIN_FLUSH_CACHE:
	case WIN_READ_NATIVE_MAX:
		{
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				fprintf(stderr, "NDAS 1.0 does not support command %x\n", Task->IdeCommand);
			}
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr= 0;
			pRequestHeader->SectorCount_Curr = 0;
			pRequestHeader->Command = Task->IdeCommand;
		}
		break;
	case WIN_READ_NATIVE_MAX_EXT:
		{
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				fprintf(stderr, "NDAS 1.0 does not support command %x\n", Task->IdeCommand);
			}
			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			pRequestHeader->COM_TYPE_E = 1;
		
			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr= 0;
			pRequestHeader->SectorCount_Curr = 0;

			pRequestHeader->Command = Task->IdeCommand;
		}
		break;

	case WIN_SMART:
		{
			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
				fprintf(stderr, "NDAS 1.0 does not support command %x\n", Task->IdeCommand);
			}

			pRequestHeader->R = 0;
			pRequestHeader->W = 0;
			
			pRequestHeader->LBAMid_Curr = SMART_LCYL_PASS;
			pRequestHeader->LBAHigh_Curr = SMART_HCYL_PASS;

			pRequestHeader->Feature_Prev = 0;
			pRequestHeader->Feature_Curr= Task->Feature;
			pRequestHeader->SectorCount_Curr = (unsigned _int8)Task->SectorCount;
			pRequestHeader->Command = Task->IdeCommand;
		}
		break;
	default:
		fprintf(stderr, "[NdasCli]IDECommand: Not Supported IDE Command.\n");
		return -1;
	}
		
	if((Task->IdeCommand == WIN_READ)
		|| (Task->IdeCommand == WIN_WRITE)
		|| (Task->IdeCommand == WIN_VERIFY)){
		
		if(PerTarget[Task->TargetId].bLBA == FALSE) {
			fprintf(stderr, "[NdasCli]IDECommand: CHS not supported...\n");
			return -1;
		}

		if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
			pRequestHeader_v0->LBA = 1;
			
			if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
				pRequestHeader_v0->LBALow_Curr = (_int8)(Task->Location);
				pRequestHeader_v0->LBAMid_Curr = (_int8)(Task->Location >> 8);
				pRequestHeader_v0->LBAHigh_Curr = (_int8)(Task->Location >> 16);
				pRequestHeader_v0->LBALow_Prev = (_int8)(Task->Location >> 24);
				pRequestHeader_v0->LBAMid_Prev = (_int8)(Task->Location >> 32);
				pRequestHeader_v0->LBAHigh_Prev = (_int8)(Task->Location >> 40);
				
				pRequestHeader_v0->SectorCount_Curr = (_int8)Task->SectorCount;
				pRequestHeader_v0->SectorCount_Prev = (_int8)(Task->SectorCount >> 8);
			} else {
				pRequestHeader_v0->LBALow_Curr = (_int8)(Task->Location);
				pRequestHeader_v0->LBAMid_Curr = (_int8)(Task->Location >> 8);
				pRequestHeader_v0->LBAHigh_Curr = (_int8)(Task->Location >> 16);
				pRequestHeader_v0->LBAHeadNR = (_int8)(Task->Location >> 24);
				
				pRequestHeader_v0->SectorCount_Curr = (_int8)Task->SectorCount;
			}		
			// Backup Command.
			Task->SentIdeCommand = pRequestHeader_v0->Command;

		} else {
			pRequestHeader->LBA = 1;

			if(PerTarget[Task->TargetId].bLBA48 == TRUE) {
				
				pRequestHeader->LBALow_Curr = (_int8)(Task->Location);
				pRequestHeader->LBAMid_Curr = (_int8)(Task->Location >> 8);
				pRequestHeader->LBAHigh_Curr = (_int8)(Task->Location >> 16);
				pRequestHeader->LBALow_Prev = (_int8)(Task->Location >> 24);
				pRequestHeader->LBAMid_Prev = (_int8)(Task->Location >> 32);
				pRequestHeader->LBAHigh_Prev = (_int8)(Task->Location >> 40);

	#if WRITE_BEBUG
				if (Command == WIN_WRITE){
					pRequestHeader->SectorCount_Curr = (_int8)(Task->SectorCount << 1);
					pRequestHeader->SectorCount_Prev = (_int8)(Task->SectorCount >> 7);
				}
				else {
					pRequestHeader->SectorCount_Curr = (_int8)Task->SectorCount;
					pRequestHeader->SectorCount_Prev = (_int8)(Task->SectorCount >> 8);
				}
	#else
				pRequestHeader->SectorCount_Curr = (_int8)Task->SectorCount;
				pRequestHeader->SectorCount_Prev = (_int8)(Task->SectorCount >> 8);
	#endif

			} else {
				
				pRequestHeader->LBALow_Curr = (_int8)(Task->Location);
				pRequestHeader->LBAMid_Curr = (_int8)(Task->Location >> 8);
				pRequestHeader->LBAHigh_Curr = (_int8)(Task->Location >> 16);
				pRequestHeader->LBAHeadNR = (_int8)(Task->Location >> 24);
				
	#if WRITE_BEBUG
				if (Command == WIN_WRITE){
					pRequestHeader->SectorCount_Curr = (_int8)(Task->SectorCount << 1);
				}
				else {
					pRequestHeader->SectorCount_Curr = (_int8)Task->SectorCount;
				}
				
	#else
				pRequestHeader->SectorCount_Curr = (_int8)Task->SectorCount;
	#endif
			}
			// Backup Command.
			Task->SentIdeCommand = pRequestHeader->Command;
		}
	}


	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if (Task->Option & IDECMD_OPT_BAD_HEADER_CRC) {
		if(SendBadRequest(connsock, &pdu) != 0) {
			PrintError(WSAGetLastError(), _T("IdeCommand: Send Request "));
			return -1;
		}
	} else {
		if(SendRequest(connsock, &pdu) != 0) {
			PrintError(WSAGetLastError(), _T("IdeCommand: Send Request "));
			return -1;
		}
	}

	// If Write, Send Data.
	if(Task->IdeCommand == WIN_WRITE) {
		unsigned DataLength = Task->SectorCount * 512;

#if 1 // send CRC attached to data.
		PUCHAR wbuf = (PUCHAR) Task->Buffer;
		if (DataDigestAlgo !=0) {// Need more room for CRC
			wbuf = (PUCHAR)malloc(DataLength+16); // Need 16 byte to add CRC
			memcpy(wbuf, Task->Buffer, DataLength);
			CRC32(
				(UCHAR *)wbuf,
				&(((UCHAR *)wbuf)[DataLength]), 
				DataLength
			);
			DataLength += 16; //CRC + Padding for 16 byte align.
		}

		//
		// Encrypt Data.
		//
		if(DataEncryptAlgo != 0) {
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Encrypt128(
					(UCHAR*)wbuf,
					DataLength,
					(UCHAR *)&CHAP_C,
					cur_password
					);
			} else {
				Encrypt32(
					(UCHAR*)wbuf,
					DataLength,
					(UCHAR *)&CHAP_C,
					(UCHAR*)&cur_password
					);
			}
			//fprintf(stderr, "IdeCommand: WIN_WRITE Encrypt data 1 !!!!!!!!!!!!!!!...\n");
		}

		// Corrupt some data before send
		if (Task->Option & IDECMD_OPT_BAD_DATA_CRC) {
			// Corrupt some of the header
			wbuf[66] ^= 0x1;
		}

		iResult = SendIt(
			connsock,
			(PCHAR)wbuf,
			DataLength
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), _T("IdeCommand: Send data for WRITE "));
			if (DataEncryptAlgo != 0)
				free(wbuf);
			return -1;
		}
		if (DataDigestAlgo != 0)
			free(wbuf);
#else // Send CRC in seperate lpx packet.
		PUCHAR wbuf = (PUCHAR) pData;
		UCHAR CrcBuf[16] = {0};

		// Calc CRC before encrypt
		// Send CRC
		if (DataDigestAlgo !=0) {
			CRC32(wbuf,	CrcBuf,	DataLength);
		}

		//
		// Encrypt Data.
		//
		if(DataEncryptAlgo != 0) {
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Encrypt128(
					(UCHAR*)wbuf,
					DataLength,
					(UCHAR *)&CHAP_C,
					cur_password
					);
				if (DataDigestAlgo !=0) {
					Encrypt128(
						(UCHAR*)CrcBuf,
						16,
						(UCHAR *)&CHAP_C,
						cur_password
						);			
				}
			} else {
				Encrypt32(
					(UCHAR*)wbuf,
					DataLength,
					(UCHAR *)&CHAP_C,
					(UCHAR*)&iPassword_v1
					);
			}
			//fprintf(stderr, "IdeCommand: WIN_WRITE Encrypt data 1 !!!!!!!!!!!!!!!...\n");
		}

		// Corrupt some data before send
		if (Option & IDECMD_OPT_BAD_DATA_CRC) {
			// Corrupt some of the data
			wbuf[66] ^= 0x1;
		}

		iResult = SendIt(
			connsock,
			(PCHAR)wbuf,
			DataLength
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), _T("IdeCommand: Send data for WRITE "));
			return -1;
		}

		// Send CRC
		if (DataDigestAlgo !=0) {
			iResult = SendIt(
				connsock,
				(PCHAR)CrcBuf,
				16
				);
			if(iResult == SOCKET_ERROR) {
				PrintError(WSAGetLastError(), _T("IdeCommand: Send CRC for WRITE "));
				return -1;
			}
		}	
#endif
	}
	
	return 0;
}


int
ReceiveIdeCommandReplyAndData(
		   SOCKET	connsock,
		   PNDASCLI_TASK Task
){
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER	pReplyHeader_v0;
	LANSCSI_PDU_POINTERS						pdu;
	int								iResult;
	unsigned						DataLength;
	unsigned						crc;
	BOOL	CrcErrored = FALSE;

	// If Read, Identify Op... Read Data.
	switch(Task->IdeCommand) {
	case WIN_READ:
		{
			PUCHAR Buf;
			if(DataDigestAlgo != 0) {
				DataLength = Task->SectorCount * 512 + 16;
				Buf = (PUCHAR) malloc(DataLength);
			} else {
				DataLength = Task->SectorCount * 512;
				Buf =  (PUCHAR)Task->Buffer;
			}
			iResult = RecvIt(
				connsock,
				(PCHAR)Buf, 
				DataLength
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), _T("IdeCommand: Receive Data for READ "));
				printf("RR\n");
				if (DataDigestAlgo !=0)
					free(Buf);
				return -1;
			}

			//
			// Decrypt Data.
			//
			if(DataEncryptAlgo != 0) {
				if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
					Decrypt128(
						(UCHAR*)Buf,
						DataLength,
						(UCHAR*)&CHAP_C,
						cur_password
						);
				} else {
					Decrypt32(
						(UCHAR*)Buf,
						DataLength,
						(UCHAR*)&CHAP_C,
						(UCHAR*)&cur_password
						);
				}
			//fprintf(stderr, "IdeCommand: WIN_READ Encrypt data 1 !!!!!!!!!!!!!!!...\n");
			}

			if(DataDigestAlgo != 0) {
				crc = ((unsigned *)Buf)[Task->SectorCount * 128];

				CRC32(
					(UCHAR *)Buf,
					&(((UCHAR *)Buf)[Task->SectorCount * 512]),
					Task->SectorCount * 512
				);

				if(crc != ((unsigned *)Buf)[Task->SectorCount * 128]) {
					fprintf(stderr, "Read data Digest Error !!!!!!!!!!!!!!!...\n");
					CrcErrored = TRUE;
				}
				memcpy(Task->Buffer, Buf, Task->SectorCount * 512);
				free(Buf);
			}
		}
		break;
	//case WIN_WRITE :
	//	closesocket(connsock);		
	//	break;
	case WIN_IDENTIFY:
	case WIN_PIDENTIFY:
		{
			UCHAR Buf[512+16];
			DataLength = 512;
			if(DataDigestAlgo != 0) DataLength += 16;

			iResult = RecvIt(
				connsock, 
				(PCHAR)Buf, 
				DataLength
				);
			if(iResult <= 0) {
				PrintError(WSAGetLastError(), _T("IdeCommand: Receive Data for IDENTIFY "));
				printf("RI\n");
				return -1;
			}

			//
			// Decrypt Data.
			//
			
			if(DataEncryptAlgo != 0) {
				if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
					Decrypt128(
						(UCHAR*)Buf,
						DataLength,
						(UCHAR*)&CHAP_C,
						cur_password
						);
				} else {
					Decrypt32(
						(UCHAR*)Buf,
						DataLength,
						(UCHAR*)&CHAP_C,
						(UCHAR*)&cur_password
						);
				}
				//fprintf(stderr, "IdeCommand: WIN_IDENTIFY Encrypt data 1 !!!!!!!!!!!!!!!...\n");
			}

			if(DataDigestAlgo != 0) {
				crc = ((unsigned *)Buf)[128];

				CRC32(
					(UCHAR*)Buf,
					&(((UCHAR*)Buf)[512]),
					512
				);

				if(crc != ((unsigned *)Buf)[128]) {
					fprintf(stderr, "Data Digest Error !!!!!!!!!!!!!!!...\n");
					CrcErrored = TRUE;
				}
			}
			memcpy(Task->Buffer, Buf, 512);

			//printf("o\n");

			//for (int i = 0; i <= 92; i++) {
			//	unsigned short tmp = (pData[2*i+1] << 8) | pData[2*i];
			//	printf("%3d: %4x\n", i, tmp);
			//}
			
		}
		break;
	default:
		break;
	}

	// Read Reply.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]IDECommand: Can't Read Reply.\n");

		switch(Task->IdeCommand) {
		case WIN_READ:
			printf("R\n");
		break;
		case WIN_WRITE:
			printf("W\n");
		break;
		case WIN_VERIFY:
			printf("V\n");
		break;
		case WIN_IDENTIFY:
		case WIN_PIDENTIFY:
			printf("I\n");
		break;
		}
		return -1;
	} else if(iResult == WAIT_TIMEOUT) {
		fprintf(stderr, "[NdasCli]IDECommand: Time out...\n");
		return WAIT_TIMEOUT;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)pdu.pR2HHeader;	
	pReplyHeader_v0 = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if(pReplyHeader->Opcode != IDE_RESPONSE){
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header pReplyHeader->Opcode != IDE_RESPONSE . Opcode=0x%x, Flag: 0x%x, Req. Command: 0x%x Rep. Command(Error): 0x%x\n", 
			pReplyHeader->Opcode, pReplyHeader->Flags, Task->SentIdeCommand, pReplyHeader->Command);
		return -1;
	}
	if(pReplyHeader->F == 0){		
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header pReplyHeader->F == 0 . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, Task->SentIdeCommand, pReplyHeader->Command);
		return -1;
	}

//	if(pReplyHeader->Command != iCommandReg) {		
//		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header pReplyHeader->Command != iCommandReg . Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
//			pReplyHeader->Flags, Task->SentIdeCommand, pReplyHeader->Command);
//		return -1;
//	}

	if (pReplyHeader->Response == LANSCSI_RESPONSE_T_BROKEN_DATA) {
		fprintf(stderr, "Write-data CRC error.\n");
		CrcErrored = TRUE;
	} else if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
			fprintf(stderr, "[NdasCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x, Feature: 0x%x\n", 
				pReplyHeader_v0->Response, ntohl(pReplyHeader_v0->DataTransferLength), ntohl(pReplyHeader_v0->DataSegLen),
				Task->SentIdeCommand, pReplyHeader_v0->Command, pReplyHeader_v0->Feature
				);
		} else {
			fprintf(stderr, "[NdasCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
				pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
				Task->SentIdeCommand, pReplyHeader->Command
				);
			fprintf(stderr, "Status register = 0x%x\n", (pReplyHeader->Command & (~Task->SentIdeCommand)));
			fprintf(stderr, "Error register Curr = 0x%x\n", pReplyHeader->Feature_Curr);
			fprintf(stderr, "Error register Prev = 0x%x\n", pReplyHeader->Feature_Prev);
		}		
		return -1;
	}

	if(Task->IdeCommand == WIN_WRITE) {
		Task->Info = pReplyHeader->PendingWriteCount;
//		fprintf(stderr, "IdeCommand:Pending Write Count=%d\n", pReplyHeader->PendingWriteCount);
	}

	if(pReplyHeader->RetransmitCount) {
		fprintf(stderr, "IdeCommand:Retransmit count=%d\n", pReplyHeader->RetransmitCount);	
	}

	if(Task->IdeCommand == WIN_CHECKPOWERMODE1){
		printf("Check Power mode = 0x%02x", (UCHAR)(pReplyHeader->SectorCount_Curr));
		switch((UCHAR)(pReplyHeader->SectorCount_Curr)) {
			case 0: printf("(Standby)"); break;
			case 0x80: printf("(Idle)"); break;
			case 0xFF: printf("(Active or Idle)"); break;
			default: printf("(Unknown)"); break;
		}
		printf("\n");
	}
	if (Task->IdeCommand == WIN_READ_NATIVE_MAX_EXT) {
		printf("Native Max address: %02x %02x %02x %02x %02x %02x\n",
			pReplyHeader->LBAHigh_Prev,
			pReplyHeader->LBAMid_Prev,
			pReplyHeader->LBALow_Prev,
			pReplyHeader->LBAHigh_Curr,
			pReplyHeader->LBAMid_Curr,
			pReplyHeader->LBALow_Curr
		);
	}
	if (Task->IdeCommand == WIN_READ_NATIVE_MAX) {
		printf("Native Max address: %02x %02x %02x %02x\n",
			pReplyHeader->Device ,
			pReplyHeader->LBAHigh_Curr,
			pReplyHeader->LBAMid_Curr,
			pReplyHeader->LBALow_Curr
		);
	}

	if (CrcErrored)
		return -2;
	return 0;
}



// Return value
//		0: OK
//		-1: General error
//		-2: CRC error
//
int
IdeCommand(
		   SOCKET	connsock,
		   _int32	TargetId,
		   _int8	LUN,
		   UCHAR	Command,
		   _int64	Location,
		   _int16	SectorCount,
		   _int8	Feature,
		   UINT32   pDataLen,
		   PCHAR	pData,
		   UINT32	Option,
			UINT32	*Info
		   )
{
	int				iResult;
	NDASCLI_TASK	task;

	task.TargetId = TargetId;
	task.LUN = LUN;
	task.BufferLength = pDataLen;
	task.Buffer = pData;
	task.Option = Option;
	task.TaskTag = 0;
	task.IdeCommand = Command;
	task.SentIdeCommand = 0;
	task.Location = Location;
	task.SectorCount = SectorCount;
	task.Feature = Feature;
	task.Info = 0;
	
	iResult = SendIdeCommandRequestAndData(connsock, &task);
	if(iResult)
		return iResult;
	iResult = ReceiveIdeCommandReplyAndData(connsock,&task);
	if(Info)
		*Info = task.Info;
	return iResult;
}


int
PacketCommand(
		   SOCKET	connsock,
		   _int32	TargetId,
		   _int8	LUN,
		   UCHAR	Command,
		   _int64	Location,
		   _int16	SectorCount,
		   _int8	Feature,
		   PCHAR	pData,
		   int index
		   )
{
	CHAR							data2[1024];
	_int8							PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_PACKET_REQUEST_PDU_HEADER pRequestHeader;
	PLANSCSI_PACKET_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS						pdu;
	int								iResult;
	unsigned _int8					iCommandReg;
//	PPACKET_COMMAND					pPCommand;
	int additional;
	int read = 0;
	int write = 0;

	int xxx;

	//
	// Make Request.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader = (PLANSCSI_PACKET_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = IDE_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pRequestHeader->DataSegLen = htonl(sizeof(pRequestHeader->PKCMD));
		pRequestHeader->AHSLen = 0;
	} else {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons(sizeof(pRequestHeader->PKCMD));
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->TargetID = htonl(TargetId);
	pRequestHeader->LUN = 0;
	// Using Target ID. LUN is always 0.
	pRequestHeader->DEV = TargetId;
	

//	pPCommand = (PPACKET_COMMAND)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];

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
	pRequestHeader->PKCMD[0] = 0x1b;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x02;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';	
	additional = 0;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;	
	pRequestHeader->LBAHigh_Curr = 0x00;
/**/
/*close*
	pRequestHeader->PKCMD[0] = 0x1b;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x03;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	additional = 0;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;	
	pRequestHeader->LBAHigh_Curr = 0x00;
/**/

/* Read PIO*
	pRequestHeader->PKCMD[0] = 0x43;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x04;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 24;
	read = 1;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
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
	pRequestHeader->PKCMD[0] = 0x2a;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x01;
	pRequestHeader->PKCMD[5] = 0x09;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	//pRequestHeader->PKCMD[8] = 0x1f;
	pRequestHeader->PKCMD[8] = (CHAR)x;
	pRequestHeader->PKCMD[9] = 0x00;		
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;
	
	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '1';
	//pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_W = '1';
	//additional = 31*2048;
	additional = x*2048;
	write = 1;

	pRequestHeader->Feature_Prev = 0;
	//pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->Feature_Curr= 0x01;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;	
	//pRequestHeader->LBAHigh_Curr = 0xf8;
	pRequestHeader->LBAHigh_Curr = (x*2048) >> 8;
*/
	pRequestHeader->PKCMD[0] = 0xa3;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x10;
	pRequestHeader->PKCMD[10] = 0xc1;
	pRequestHeader->PKCMD[11] = 0x00;

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
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x10;	
	pRequestHeader->LBAHigh_Curr = 0x00;
}
/**/
//#else
/* Read PIO*
else{
	pRequestHeader->PKCMD[0] = 0x5c;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x0c;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 12;
	read = 1;

	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x0c;	
	pRequestHeader->LBAHigh_Curr = 0x00;
}
/**/
//#endif




/* Read DMA*
	pRequestHeader->PKCMD[0] = 0x28;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0xaf;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x01;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;
	
	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '1';
	pRequestHeader->COM_TYPE_R = '1';
	additional = 2048;

	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x01;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;	
	pRequestHeader->LBAHigh_Curr = 0x80;
/**/

/* Read PIO*
	pRequestHeader->PKCMD[0] = 0x28;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0xaf;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x01;
	pRequestHeader->PKCMD[9] = 0x00;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;
	
	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';
	additional = 2048;

	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x00;	
	pRequestHeader->LBAHigh_Curr = 0x80;
/**/


// READ KEY
else if(2){

	pRequestHeader->PKCMD[0] = 0xa4;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x0c;
	pRequestHeader->PKCMD[10] = 0xc2;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 12;
	read = 1;
	
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x0c;	
	pRequestHeader->LBAHigh_Curr = 0x00;

}else if(3){
	pRequestHeader->PKCMD[0] = 0xa4;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x08;
	pRequestHeader->PKCMD[10] = 0x00;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 8;
	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
	pRequestHeader->LBALow_Curr = 0x00;
	pRequestHeader->LBAMid_Curr = 0x08;	
	pRequestHeader->LBAHigh_Curr = 0x00;
}else if(4){
	pRequestHeader->PKCMD[0] = 0xa4;
	pRequestHeader->PKCMD[1] = 0x00;
	pRequestHeader->PKCMD[2] = 0x00;
	pRequestHeader->PKCMD[3] = 0x00;
	pRequestHeader->PKCMD[4] = 0x00;
	pRequestHeader->PKCMD[5] = 0x00;
	pRequestHeader->PKCMD[6] = 0x00;
	pRequestHeader->PKCMD[7] = 0x00;
	pRequestHeader->PKCMD[8] = 0x00;
	pRequestHeader->PKCMD[9] = 0x08;
	pRequestHeader->PKCMD[10] = 0x05;
	pRequestHeader->PKCMD[11] = 0x00;

	pRequestHeader->COM_TYPE_P = '1';
	pRequestHeader->COM_TYPE_D_P = '0';
	pRequestHeader->COM_TYPE_R = '1';

	additional = 8;
	read = 1;
	pRequestHeader->Feature_Prev = 0;
	pRequestHeader->Feature_Curr= 0x00;
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
	pdu.pDataSeg = (CHAR *)pRequestHeader->PKCMD;
	
	xxx = clock();
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("IdeCommand: Send Request "));
		return -1;
	}

	if((additional > 0) && (write)){
		//CHAR pData[64*1024];

		iResult = SendIt(
			connsock,
			data2,
			additional
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), _T("IdeCommand: Send data for WRITE "));
			return -1;
		}
	}


	// READ additional data
	if((additional > 0) && (read)){
		int i;

		printf("XXXXXXX\n");
		iResult = RecvIt(connsock, pData, additional);
		if(iResult <= 0) {
			PrintError(WSAGetLastError(), _T("PacketCommand: Receive additional data"));
				return -1;
		}
		for(i = 0 ; i < additional ; i++){
			printf("%02x :" , (UCHAR)((CHAR*)pData)[i]);
			//printf("%c : " , (UCHAR)((CHAR*)pData)[i]);
			if(!((i+1) % 16)){
				printf("\n");
			}
			if(!((i+1) % 2)){
				//printf("%02x" , (UCHAR)((CHAR*)pData)[i-1]);
				//printf("\n");
			}
			else{
				//printf("%d : ", i/2);
				//printf("%02x" , (UCHAR)((CHAR*)pData)[i+1]);
			}
		}
		printf("\n");
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
	
	xxx = clock() - xxx;
	// Check Request Header.
	pReplyHeader = (PLANSCSI_PACKET_REPLY_PDU_HEADER)pdu.pR2HHeader;

	//printf("path command tag %0x\n", ntohl(pReplyHeader->PathCommandTag));

	if(pReplyHeader->Opcode != IDE_RESPONSE){
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header. OP Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
	fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header. OP 0x%x\n", pReplyHeader->Opcode);
		return -1;
	}
	else if(pReplyHeader->F == 0){
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header. F Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	/*
	else if(pReplyHeader->Command != iCommandReg) {
		
		fprintf(stderr, "[NdasCli]IDECommand: BAD Reply Header. Command Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Flags, iCommandReg, pReplyHeader->Command);
		return -1;
	}
	*/
	printf("time == %d \n", xxx);
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]IDECommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
			pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
			iCommandReg, pReplyHeader->Command
			);
		fprintf(stderr, "ErrReg 0x%02x\n", pReplyHeader->Feature_Curr);
		return -1;
	}
	
	return 0;
}


inline VOID
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
			UINT	TargetId,
			BOOL	Silent,
			BOOL	SetDefaultTransferMode
			)
{
	struct hd_driveid	info;
	int					iResult;
	CHAR				buffer[41];
	int dma_mode;
	int set_dma_feature_mode;

	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "Identify Failed...\n");
		return iResult;
	}

	printf("0 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[1]), (UCHAR)(((PCHAR)&info)[0]));
	printf("2 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[5]), (UCHAR)(((PCHAR)&info)[4]));
	printf("10 words  0x%c%c\n", (UCHAR)(((PCHAR)&info)[21]), (UCHAR)(((PCHAR)&info)[20]));
	printf("47 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[95]), (UCHAR)(((PCHAR)&info)[94]));
	printf("49 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[99]), (UCHAR)(((PCHAR)&info)[98]));
	printf("59 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[119]), (UCHAR)(((PCHAR)&info)[118]));

	//if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETMULT, 0, 0x08, 0, NULL)) != 0) {
	//		fprintf(stderr, "[NdasCli]GetDiskInfo: Set Feature Failed...\n");
	//		return iResult;
	//}
//	printf("47 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[95]), (UCHAR)(((PCHAR)&info)[94]));
//	printf("59 words  0x%02x%02x\n", (UCHAR)(((PCHAR)&info)[119]), (UCHAR)(((PCHAR)&info)[118]));

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_CHECKPOWERMODE1, 0, 0, 0, NULL)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo: Set Feature Failed...\n");
			return iResult;
	}
#endif

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_STANDBY, 0, 0, 0, NULL)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo: Set Feature Failed...\n");
			return iResult;
	}
#endif

#if 0
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_CHECKPOWERMODE1, 0, 0, 0, NULL)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo: Set Feature Failed...\n");
			return iResult;
	}
#endif
	if (!Silent)
		printf("Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
			TargetId, info.major_rev_num, info.minor_rev_num, info.capability);
	if (!Silent)
		printf("DMA 0x%x, U-DMA 0x%x\n", 
			info.dma_mword, 
			info.dma_ultra);

	if(SetDefaultTransferMode) {
#if 0 
		//
		// DMA Mode.
		//
		if(!(info.dma_ultra & 0x0020)) {
			fprintf(stderr, "Not Support UDMA mode 5...\n");
			return -1;
		}
#endif
		//fprintf(stderr, "eide_dma_min %d\n", info.eide_dma_min);


#if 1
		// set PIO mode 8 + 4 = 12
		if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0x0c, 0x03, 0, NULL, 0,0 )) != 0) {
				fprintf(stderr, "Set Feature Failed...\n");
				fprintf(stderr," Can't set to UDMA mode 2(33)\n");
				//return iResult;
		}
#endif

	//	if(ActiveHwVersion >= LANSCSIIDE_VERSION_2_5)
		if(ActiveHwVersion >= LANSCSIIDE_VERSION_2_0)
		{
			dma_mode = 0;
			// ultra dma
			// find fastest ultra dma mode
			if(info.dma_ultra & 0x0001)
				dma_mode = 0;
			if(info.dma_ultra & 0x0002)
				dma_mode = 1;
			if(info.dma_ultra & 0x0004)
				dma_mode = 2;
#ifdef __LSP_CHECK_CABLE80__
				if(info.hw_config & 0x2000)	// true : device detected CBLID - above V_ih
				{
					// try higher ultra dma mode (cable 80 needed)
#endif
					if(info.dma_ultra & 0x0008)
						dma_mode = 3;
					if(info.dma_ultra & 0x0010)
						dma_mode = 4;
					if(info.dma_ultra & 0x0020)
						dma_mode = 5;
					if(info.dma_ultra & 0x0040)
						dma_mode = 6;
					// level 7 is not supported level
	//				if(info.dma_ultra & 0x0080)
	//					dma_mode = 7;
#ifdef __LSP_CHECK_CABLE80__
				}
				else
				{
					// cable 80 not detected
				}
#endif
			if (!Silent)
				printf("Setting UDMA %d\n", dma_mode);

			set_dma_feature_mode = 0x40 /* ultra dma */ | dma_mode;			
			if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, (short)set_dma_feature_mode, 0x03, 0, NULL, 0, 0)) != 0) {
				fprintf(stderr, "Set Feature Failed...\n");
				fprintf(stderr," Can't set to UDMA mode\n");
				//return iResult;
			} 
			// identify.
			if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
				fprintf(stderr, "Identify Failed...\n");
				return iResult;
			}
		} else if(info.dma_mword & 0x00ff) {
			// find fastest dma mode

			// dma mode 2, 1 and(or) 0 is supported
			if(info.dma_mword & 0x0001)
			{
				/* multiword dma mode 0 is supported*/
				dma_mode = 0;
			}
			if(info.dma_mword & 0x0002)
			{
				/* multiword dma mode 1 is supported*/
				dma_mode = 1;
			}
			if(info.dma_mword & 0x0004)
			{
				/* multiword dma mode 2 is supported*/
				dma_mode = 2;
			}
			if (!Silent)
				printf("Setting DMA mode %d\n", dma_mode);

			// Always set DMA mode because NDAS chip and HDD may have different DMA setting.
			set_dma_feature_mode = 0x20 /* dma */ | dma_mode;
			if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, (short)set_dma_feature_mode, 0x03, 0, NULL, 0, 0)) != 0) {
				fprintf(stderr, "Set Feature Failed...\n");
				//return iResult;
			} 
			// identify.
			if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
				fprintf(stderr, "Identify Failed...\n");
				return iResult;
			}
		} else	{
			// PIO
		}
	}
	if (!Silent) {
	
		printf("Supported PIO mode 0x%x\n", 
				info.eide_pio_modes);
		
		printf("PIO W/O IORDY 0x%x, PIO W/ IORDY 0x%x\n", 
				info.eide_pio, info.eide_pio_iordy);

		printf("DMA 0x%x, U-DMA 0x%x\n", 
				info.dma_mword, info.dma_ultra);
	}
	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);

	if (!Silent) 
		printf("Serial No: %s\n", buffer);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	
	if (!Silent) 
		printf("Firmware rev: %s\n", buffer);
	
	memset(buffer, 0, 41);
	strncpy(buffer, (PCHAR)info.model, 40);
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);

	if (!Silent) 
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
	if(info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400) {	// Support LBA48bit
		PerTarget[TargetId].bLBA48 = TRUE;
		PerTarget[TargetId].SectorCount = info.lba_capacity_2;
		if (!Silent) 
			printf("Big LBA\n");
	} else {
		PerTarget[TargetId].bLBA48 = FALSE;
		
		if((info.capability & 0x02) && Lba_capacity_is_ok(&info)) {
			PerTarget[TargetId].SectorCount = info.lba_capacity;
		}
		
		PerTarget[TargetId].SectorCount = info.lba_capacity;	
	}

	
	PerTarget[TargetId].bSmartSupported =  info.command_set_1 & 0x0001;
	if (PerTarget[TargetId].bSmartSupported) {
		PerTarget[TargetId].bSmartEnabled = info.cfs_enable_1 & 0x01;
	} else {
		PerTarget[TargetId].bSmartEnabled = FALSE;
	}

	if (!Silent) {
		printf("SMART: %s and %s\n", 
			(PerTarget[TargetId].bSmartSupported)?"Supported":"Not supported",
			(PerTarget[TargetId].bSmartEnabled)?"Enabled":"Disabled"
			);
	}

	if (!Silent) {
		printf("CAP 2 %I64d, CAP %d\n",
			info.lba_capacity_2,
			info.lba_capacity
			);

		printf("LBA %d, LBA48 %d, Number of Sectors: %I64d\n", 
			PerTarget[TargetId].bLBA, 
			PerTarget[TargetId].bLBA48, 
			PerTarget[TargetId].SectorCount);
	}
//	PerTarget[TargetId].bLBA = TRUE;
//	PerTarget[TargetId].SectorCount = 1024*1024*1024;

	return 0;
}


// No transfer mode change.
int
GetDiskInfo2(
			SOCKET	connsock,
			UINT	TargetId
			)
{
	struct hd_driveid	info;
	int					iResult;
	CHAR				buffer[41];

	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: PIdentify Failed...\n");
		return iResult;
	}

	printf("[NdasCli]GetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, \n", 
		TargetId, info.major_rev_num, info.minor_rev_num);
	
	printf("[NdasCli]GetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
		info.dma_mword, 
		info.dma_ultra);

	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}

	printf("Current setting: Supported PIO 0x%x, DMA 0x%x, U-DMA 0x%x\n", 
			info.eide_pio_modes,
			info.dma_mword, 
			info.dma_ultra);
	printf("Current mode: ");
	if (info.dma_mword & 0x100) {
		printf("DMA mode 0 ");
	}
	if (info.dma_mword & 0x200) {
		printf("DMA mode 1 ");
	}
	if (info.dma_mword & 0x400) {
		printf("DMA mode 2 ");
	}
	if (info.dma_ultra & 0x100) {
		printf("UDMA mode 0 ");
	}
	if (info.dma_ultra & 0x200) {
		printf("UDMA mode 1 ");
	}
	if (info.dma_ultra & 0x400) {
		printf("UDMA mode 2 ");
	}
	if (info.dma_ultra & 0x800) {
		printf("UDMA mode 3 ");
	}
	if (info.dma_ultra & 0x1000) {
		printf("UDMA mode 4 ");
	}
	if (info.dma_ultra & 0x2000) {
		printf("UDMA mode 5 ");
	}
	if (info.dma_ultra & 0x4000) {
		printf("UDMA mode 6 ");
	}
	if (info.dma_ultra & 0x8000) {
		printf("UDMA mode 7 ");
	}
	printf("\n");

	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);
	printf("Serial No: %s\n", buffer);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	printf("Firmware rev: %s\n", buffer);
	
	memset(buffer, 0, 41);
	strncpy(buffer, (PCHAR)info.model, 40);
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);
	printf("Model No: %s\n", buffer);

	if(info.capability &= 0x02)
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
		PrintError(WSAGetLastError(), _T("GetInterfaceList: socket "));
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
		PrintError(WSAGetLastError(), _T("GetInterfaceList: WSAIoctl "));
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
		fprintf(stderr, "[NdasCli]MakeConnection: Error When Get NIC List!!!!!!!!!!\n");
		
		return FALSE;
	} else {
		if (socketAddressList->iAddressCount!=1)
			fprintf(stderr, "[NdasCli]MakeConnection: Number of NICs : %d\n", socketAddressList->iAddressCount);
	}
	
	//
	// Find NIC that is connected to LanDisk.
	//
	for(i = 0; i < socketAddressList->iAddressCount; i++) {
		
		socketLpx = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);

#if 0
		printf("[NdasCli]MakeConnection: NIC %02d: Address %02X:%02X:%02X:%02X:%02X:%02X\n",
			i,
			socketLpx.LpxAddress.Node[0],
			socketLpx.LpxAddress.Node[1],
			socketLpx.LpxAddress.Node[2],
			socketLpx.LpxAddress.Node[3],
			socketLpx.LpxAddress.Node[4],
			socketLpx.LpxAddress.Node[5]
			);
#endif

		sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		if(sock == INVALID_SOCKET) {
			PrintError(WSAGetLastError(), _T("MakeConnection: socket "));
			return FALSE;
		}
		
		socketLpx.LpxAddress.Port = 0; // unspecified
		
		// Bind NIC.
		iErrcode = bind(sock, (struct sockaddr *)&socketLpx, sizeof(socketLpx));
		if(iErrcode == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), _T("MakeConnection: bind "));
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
			PrintError(WSAGetLastError(), _T("MakeConnection: connect "));
			closesocket(sock);
			sock = INVALID_SOCKET;
			
			fprintf(stderr, "[NdasCli]MakeConnection: LanDisk is not connected with NIC Number %d\n", i);
			
			continue;
		} else {
			*pSocketData = sock;
			
			break;
		}
	}
	
	if(sock == INVALID_SOCKET) {
		fprintf(stderr, "[NdasCli]MakeConnection: No LanDisk!!!\n");
		
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

