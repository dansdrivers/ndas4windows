// NdasCli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <time.h>
#include <stdio.h>
#include <tchar.h>

//
// Define BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX and for BUILD_FOR_DIST
//		to build ndascli version that fixes incorrect 1.1 NDAS EEPROM problem.
//
//#define BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX
//#define BUILD_FOR_DIST

//#define SAMPLE_MAC " 00:0b:d0:00:ff:d1 "
//#define SAMPLE_MAC " 00:0b:d0:fe:02:3c "
#define SAMPLE_MAC " 00:0c:29:21:26:ea " // vmware

#ifdef BUILD_FOR_DIST
char DefaultCommand[] = "";
#else
char DefaultCommand[] = 
//	"AesTest", "00:08:74:4F:F2:97", NULL, NULL, NULL, NULL, NULL, NULL
//	"GetEEP 00:f0:0f:00:ff:d1 0x00002 0 256"   // Get EEP using user id
//	"PnpRequest 00:f0:0f:00:ff:d1 00:0c:6e:4b:6e:66" // Send pnp request and wait reply
//	"SetEEP 00:f0:0f:00:ff:d1 0x0002 0x40 16 userpass.txt"  // set password
//	"LoginTest00", "00:f0:0f:00:ff:d1", NULL, NULL, NULL, NULL
//	"RawVendor 00:f0:0f:00:ff:d1 0x10002 0x03 \"\" 0 0 0" // RAW vendor command
//	"RawVendor 00:0b:d0:02:3c:bb 0x10001 0x14 \"\" 0 0x80000001 0" // RAW vendor command
//	"LockedWrite 00:f0:0f:00:ff:d1 256 16 100000 0x21"   // Write 256MB at 100G position
//	"LockedWrite " SAMPLE_MAC " 256 1 150000 0x21"   // Write 256MB at 100G position
//	"BLDeadlockTest 00:f0:0f:00:ff:d1 20"	// 
//	"Nop 00:f0:0f:00:ff:d1"	// Test NOP handling
//	"TTD 00:f0:0f:00:ff:d1 Set 0x12332"	// Test NOP handling
//	"loginr" SAMPLE_MAC "0x10001"
//	"DynamicOptionTest 00:f0:0f:00:ff:d1"
//	"ShowAccount 00:f0:0f:00:ff:d1"
//	"SetPermission 00:f0:0f:00:ff:d1 0x70007"
//	"LoginRw 00:f0:0f:00:ff:d1 0x10004"
//	"LoginR 00:f0:0f:00:ff:d1 0x10004 userpw1"
//	"LoginR 00:f0:0f:00:ff:d1 0x10001"
//	"LoginR" SAMPLE_MAC "0x10001"
//	"BlockVariedIo 00:f0:0f:00:ff:d1 16 10000 256 0"
//	"MutexCli 00:f0:0f:00:ff:d1"
//	"SetPw 00:f0:0f:00:ff:d1 1 pwpw"
//	"SetUserPw 00:f0:0f:00:ff:d1 1"
//	"SetEEP 00:f0:0f:00:ff:d1 0x2 512 384 C:\\projects\\pilots\\convert\\debug\\usb.bin"
//	"DigestTest 00:f0:0f:00:ff:d1"
//	"PnpWait 00:f0:0f:00:ff:d1 00:0c:6e:4b:6e:66"
//	"Read 00:f0:0f:00:ff:d1 1024 1 100000 32"
//	"BatchTest 00:f0:0f:00:ff:d1 00:0c:6e:4b:6e:66"
//	"WritePattern 00:f0:0f:00:ff:d1 3271 128 1000 0 0x10004 userpw1"
//	"WritePattern 00:f0:0f:00:ff:d1 3222 256 10000 0 0x10004"
//	"InterleavedIo 00:f0:0f:00:ff:d1 128 10000"
//	"MutexTest1 00:f0:0f:00:ff:d1 0"
//	"SetMAC 00:f0:0f:00:ff:d1 00:0b:d0:00:ff:d1"
//	"StandbyTest" SAMPLE_MAC	
//	"Discovery 00:0b:d0:fe:02:04"
//	"Smart 00:0b:d0:fe:02:04"
//	"ViewMeta 00:0b:d0:fe:02:04"
//	"Discovery 00:0b:d0:00:bb:80"
//	"seteep 00:0b:d0:00:ff:d1 0x0000 0x30 16 eep0x30.bin"
	"help"
;
#endif

#define Decrypt32 Decrypt32_l
#define Encrypt32 Encrypt32_l
#define Hash32To128 Hash32To128_l


// Global Variable.
_int32			HPID = 0;
_int16			RPID = 0;
_int32			iTag = 0;
int				NRTarget;
unsigned int	CHAP_C[4];
short		requestBlocks;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
UINT16  MaxPendingTasks = 0;
UINT16	HeaderEncryptAlgo = 0 ;
UINT16	DataEncryptAlgo = 0;
UINT16	HeaderDigestAlgo = 0;
UINT16	DataDigestAlgo = 0;
int				iSessionPhase = 0;
int				ActiveHwVersion = 0;
int				ActiveHwRevision = 0;
UINT32			ActiveUserId =0;
int				iTargetID = 0;

unsigned _int64	iPassword_v1 = 0x1f4a50731530eabb;
unsigned _int64	iPassword_v1_seagate = 0x99a26ebc46274152;
unsigned _int64	iSuperPassword_v1 = 0x3E2B321A4750131E;

unsigned char def_password0[] = {0x1f, 0x4a, 0x50, 0x73, 0x15, 0x30, 0xea, 0xbb,
	0x3e, 0x2b, 0x32, 0x1a, 0x47, 0x50, 0x13, 0x1e};

unsigned char def_supervisor_password[] = {
	0xA3, 0x07, 0xA9, 0xAA, 0x33, 0xC5, 0x18, 0xA8,
	0x64, 0x94, 0x2A, 0xD1, 0x15, 0x7A, 0x1B, 0x30
};

unsigned char cur_password[PASSWORD_LENGTH] = {0}; // Set by Login. To do: Move to per session data.


typedef struct _CLI_COMMAND {
	char* Cmd;
	CLI_CMD_FUNC Func;
	char* Help;
} CLI_COMMAND, *PCLI_COMMAND;


CLI_COMMAND CommandList[] = {
#ifndef BUILD_FOR_DIST
	{"AesTest", CmdAesLibTest, "Compare result of SW AES lib and VHDL's AES lib. Not implemented."},
	{"RawVendor", CmdRawVendorCommand, "Run RAW vendor command. \n\tParameter: <UserId> <Code> <Password> <Param0> <Param1> <Param2>"},
	{"DumpEEP", CmdDumpEep, "Dump contents of EEPROM."}, 
	{"GetEEP", CmdGetEep, "Get EEP contents. \n\tParameter: <UserId> <Address> <Length>"}, 
	{"SetEEP", CmdSetEep, "Set EEP contents. \n\tParameter: <UserId> <Address> <Length> <FileName>"},
	{"GetUEEP", CmdGetUEep, "Get User EEP contents. \n\tParameter: <UserId> <Address> <Length>"}, 
	{"SetUEEP", CmdSetUEep, "Set User EEP contents. \n\tParameter: <UserId> <Address> <Length> <FileName>"},
	{"TestVendor0", CmdTestVendorCommand0, "Test vendor commands"},
	{"BatchTest", CmdBatchTest, "Batch Test"} ,
	{"PnpRequest", CmdPnpRequest, "Send and receive PNP request. \n\tParameter: <Host Network card's MAC address>"},
	{"PnpListen", CmdPnpListen, "Wait for PNP broadcast. \n\tParameter: <Host Network card's MAC address>"},
	{"LockedWrite", CmdLockedWrite, "Write using HW buffer lock and read for correctness. \n\tParameter: <Write Size(MB)> <Iterations> <Pos in MB> <LockMode> [UserId] [Blocks]\
\n\tLockMode ex: 0 for no lock, 0x21 for yield lock and vendor cmd. 0x4 for mutex-0 as write-lock."},
	{"BLDeadlockTest", CmdBufferLockDeadlockTest, "Test write buffer deadlock timeout is working. \n\tParameter: <Seconds>"},
	{"Nop", CmdNop, "Send NOP and receive reply. No parameter"},
	{"TTD", CmdTextTargetData, "Get/Set Text target data. \n\tParameter: <Get|Set> [Set data]"},
//	{"TTL", CmdTextTargetList, "Get Text target list. No parameter"}, // Use discovery
	{"Discovery", CmdDiscovery, "Login in discovery mode. No parameter"},
	{"DynamicOptionTest", CmdDynamicOptionTest, "IO with various option. No parameter"},
	{"ShowAccount", CmdShowAccount, "Show user account. Give superuser password if it is changed. \n\tParameter: [Superuser PW]"},
	{"SetPermission", CmdSetPermission, "Set user permission. \n\tParameter: <UserId+Permission> [Superuser PW]"},
	{"SetPw", CmdSetPassword, "Set password in superuser mode. \n\tParameter: <UserNum> <New PW> [Superuser PW]"},
	{"SetUserPw", CmdSetUserPassword, "Set user password. \n\tParameter: <UserNum> <New PW> [Current PW]"},
	{"LoginR", CmdLoginR, "Login with given permission and password and try read. \n\tParameter: <UserId+Permission> [PW]"},
	{"LoginRw", CmdLoginRw, "Login with given permission and password and try IO. \n\tParameter: <UserId+Permission> [PW]"},
	{"LoginWait", CmdLoginWait, "Login with given permission and password and wait for seconds. \n\tParameter: <UserId+Permission> <Time to wait> [PW]"},
	{"ResetAccount", CmdResetAccount, "Reset account's permission and password to default. \n\tParameter: [Superuser PW]"},
	{"BlockVariedIo", CmdBlockVariedIo, "Io with various block size. Set 0 for random sector count \n\tParameter: <Write Size(MB)> <Pos in MB> <Iteration> <Sector count>"},
	{"Read", CmdRead, "Read. Set 0 for random sector count. \n\tParameter: <Read Size(MB)> <Iterations> <Pos in MB> <Sector count>"},
	{"Write", CmdWrite, "Write. Set 0 for random sector count. \n\tParameter: <Write Size(MB)> <Iterations> <Pos in MB> <Sector count>"},
	{"Verify", CmdVerify, "Verify. Set 0 for random sector count. \n\tParameter: <Verify Size(MB)> <Iterations> <Pos in MB> <Sector count>"},
	{"ExIo", CmdExIo, "Io in exclusive write mode. \n\tParameter: <Write Size(MB)> <Pos in MB> <Iteration>"},
	{"MutexCli", CmdMutexCli, "Connect and run mutex command from user input. No parameter"},
	{"LpxConnect", CmdLpxConnect, "Create lpx connection and wait."},
	{"SetPacketDrop", CmdSetPacketDrop, "Set number of packets to drop out of 1000 packets. \n\tparameter: \"rx\" or \"tx\". Parameter: <Packets to drop>"},
	{"HostList", CmdHostList, "List hosts connected to NDAS device. No parameter"},
	{"DigestTest", CmdDigestTest, "Try to IO with bad CRC. No parameter"},
	{"ReadPattern", CmdReadPattern, "Read and check pattern with random block size. <Pattern#> <Read Size(MB)> <Pos in MB> [Block Size] [UserId] [PW]"},
	{"WritePattern", CmdWritePattern, "Write pattern with random block size. <Pattern#> <Write Size(MB)> <Pos in MB> [Block Size] [UserId] [PW]"},
	{"InterleavedIo", CmdInterleavedIo, "Read/write in interleaved manner. \n\tparameter: <Write Size(MB)> <Pos in MB>"},
	{"DelayedIo", CmdDelayedIo, "IO after setting interpacket delay.\n\tparameter: <Delay>"},
	{"SetMac", CmdSetMac, "Set MAC address \n\tParameter: <MAC>"},
	{"MutexTest1", CmdMutexTest1, "Try to take mutex, increase value and give it\n\tParameter: <LockId>"},
	{"Standby", CmdStandby, "Enter standby mode. No parameter"},
	{"StandbyTest", CmdStandbyTest, "Test NDAS standby bug. No parameter"},
	{"StandbyIo", CmdStandbyIo, "Enter standby mode and try to IO after 5 seconds. \n\tParameter: <DevNum> <R or W>"},
	{"CheckPowerMode", CmdCheckPowerMode, "Check power mode. No parameter"}, 
	{"TransferModeIo", CmdTransferModeIo, "IO with given transfer mode. \n\tParameter: <Write Size(MB)> <Pos in MB> <P/D/U><0~7> [Dev] [SectorSize]"},
	{"SetMode", CmdSetMode, "Set given transfer mode. \n\tParameter: <P/D/U><0~7> [Dev]"},
	{"WriteCache", CmdWriteCache, "Enable or disable write cache.\n\tParameter: [on|off] [Dev]"},
	{"FlushTest", CmdFlushTest, "Write some data with flush command. \n\tParameter: <Write Size(MB)> <Iteration> <Pos in MB> "},
	{"Smart", CmdSmart, "Run smart. No parameter"},
	{"Identify", CmdIdentify, "Show identify info. \n\tParameter: [Dev]\n"},
	{"ViewMeta", CmdViewMeta, "Show NDAS meta data. \n\tParameter: [Dev]\n"},
	{"GenBc", CmdGenBc, "Generate broadcast message. Added for testing MAC crash\n"},
	{"GetMaxAddr", CmdGetNativeMaxAddr, "Get native max address.\n"},
	{"WriteFile", CmdWriteFile, "Write file to disk. \n\tParameter: <File name> <Iteration> <Pos in sector> [Dev] [SectorCount] [Transfermode]\n"}, 
	{"CheckFile", CmdCheckFile, "Read from disk and check contents with given file. \n\tParameter: <File name> <Iteration> <Pos in sector> [Dev] [SectorCount] [Transfermode]\n"},
	{"ReadFile", CmdReadFile, "Read from disk and write to file. \n\tParameter: <File name> <Pos in sector> [Dev=0] [SectorCount=1] [Transfermode]\n"},
//	{"Compare", CmdCompare, "Compare contents of two disk.\n"},
#endif

	{"GetOption", CmdGetOption, "Get option. \n\tParameter: [Superuser PW]"},
	{"SetOption", CmdSetOption, "Set option. Run GetOption for list of option. \n\tParameter: <Option> [Superuser PW]"},
	{"GetConfig", CmdGetConfig, "Get misc. configurations."},
	{"SetDefaultConfig", CmdSetDefaultConfig, "Reset some configurations to default.\n\t(retransmission time=200ms, connection timeout=5s, standby time=30m, packet delay=8n)"},
	{"SetDefaultConfigAuto", CmdSetDefaultConfigAuto, "Set all device in the network to default config.\n\t(retransmission time=200ms, connection timeout=5s, standby time=30m, packet delay=8n)"},
	{"SetRetransmit", CmdSetRetransmit, "Set retransmission timeout in msec."},
//	{"SetStandby", CmdSetStandby, "Set standby time in minutes.\n\tParameter: <Standby time>"},
};

#define NR_CLI_CMD (sizeof(CommandList)/sizeof(CommandList[0]))

void PrintHex(unsigned char* Buf, int len)
{
	int i;
	for(i=0;i<len;i++) {
		printf("%02x", Buf[i]);
		if ((i+1)%4==0)
			printf(" ");
		if ((i+1)%32==0)
			printf("\n");
	}
}

void
PrintError(
		   int		ErrorCode,
		   PTCHAR	prefix
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
	_ftprintf(stderr, _T("%s: %s"), prefix, (LPCTSTR)lpMsgBuf);
	
	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

void
PrintErrorCode(
			   PTCHAR	prefix,
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
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			// Decrypt first 32 byte to get AHSLen
			Decrypt128(
				(unsigned char*)pPdu->pH2RHeader,
				//sizeof(LANSCSI_H2R_PDU_HEADER),
				32,
				(unsigned char *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "ReadRequest: Decrypt Header 1 !!!!!!!!!!!!!!!...\n");
		} else {
			// Decrypt first 32 byte to get AHSLen
			Decrypt32(
				(unsigned char*)pPdu->pH2RHeader,
				//sizeof(LANSCSI_H2R_PDU_HEADER),
				32,
				(unsigned char *)&CHAP_C,
				(unsigned char*)&cur_password
//				(unsigned char*)&iPassword_v1
				);
		}
	}

//	fprintf(stderr, "AHSLen = %d\n", ntohs(pPdu->pH2RHeader->AHSLen));

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

		pPdu->pDataSeg = pPtr; // ????

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
	} 

	if (HeaderDigestAlgo != 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			4
			);
		
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Recv CRC...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReadRequest: Disconnected...\n");

			return iResult;
		} else
			iTotalRecved += iResult;

		pPdu->pDataSeg = pPtr;
		pPdu->pHeaderDig = pPtr;

		pPtr += 4;
	}

	// Read paddings
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 && iTotalRecved % 16 != 0) {
		iResult = RecvIt(
			connSock,
			pPtr,
			16 - (iTotalRecved % 16)
		);

		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Recv AHS...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReadRequest: Disconnected...\n");

			return iResult;
		} else
			iTotalRecved += iResult;

		pPdu->pDataSeg = pPtr;

		pPtr += iResult;
	}

	// Decrypt remaing headers.
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			Decrypt128(
				((unsigned char*)pPdu->pH2RHeader) + 32,
				iTotalRecved - 32,
				(unsigned char *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "ReadRequest: Decrypt Header 1 !!!!!!!!!!!!!!!...\n");
		} else {
			// Decrypt first 32 byte to get AHSLen
			Decrypt32(
				((unsigned char*)pPdu->pH2RHeader) + 32,
				iTotalRecved - 32,
				(unsigned char *)&CHAP_C,
				(unsigned char*)&cur_password
//				(unsigned char*)&iPassword_v1
				);
		}
	}

	// Check header CRC
	if(HeaderDigestAlgo != 0) {
		unsigned hcrc = ((unsigned *)pPdu->pHeaderDig)[0];
		CRC32(
			(unsigned char*)pBuffer,
			&(((unsigned char*)pBuffer)[sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen)]),
			sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen)
		);
		if(hcrc != ((unsigned *)pPdu->pHeaderDig)[0]) {
			fprintf(stderr, "Header Digest Error !!!!!!!!!!!!!!!...\n");
			// Header information may invalid. We have no way but to disconnect..
			return SOCKET_ERROR;
		}
	}
#if 0			
	// What's this???
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			Decrypt128(
				(unsigned char*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "ReadRequest: Decrypt Header 2 !!!!!!!!!!!!!!!...\n");
		} else {
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&CHAP_C,
				(unsigned char*)&iPassword_v1
				);
		}
	}
#endif
	// Read Data segment. Not used by 1.1~2.5

//	fprintf(stderr, "DataSegLen = %d\n", ntohl(pPdu->pH2RHeader->DataSegLen));

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
			&& HeaderEncryptAlgo != 0) {
			//&& DataEncryptAlgo != 0) {	//by limbear
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {			
				Decrypt128(
					(unsigned char*)pPdu->pDataSeg,
					ntohl(pPdu->pH2RHeader->DataSegLen),
					(unsigned char *)&CHAP_C,
					cur_password
					);
			} else {
				Decrypt32(
					(unsigned char*)pPdu->pDataSeg,
					ntohl(pPdu->pH2RHeader->DataSegLen),
					(unsigned char *)&CHAP_C,
					(unsigned char*)&cur_password
//					(unsigned char*)&iPassword_v1
					);
			}
		}
			
	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
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
			(unsigned char*)pHeader,
			&(((unsigned char*)pHeader)[sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen]),
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
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "SendRequest: Encrypt Header 1 !!!!!!!!!!!!!!!...\n");
		} else {
			Encrypt32(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&CHAP_C,
				(unsigned char*)&cur_password
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
SendRequest(
			SOCKET			connSock,
			PLANSCSI_PDU_POINTERS	pPdu
			)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;
// changed by ILGU 2003_0819
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		iDataSegLen = ntohl(pHeader->DataSegLen);
	} else {
		iDataSegLen = ntohs(pHeader->AHSLen);
	}

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderDigestAlgo != 0) {
		CRC32(
			(unsigned char*)pHeader,
			&(((unsigned char*)pHeader)[sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen]),
			sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
			);
		iDataSegLen += 4;
	}

	//
	// Encrypt Header.
	//	

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			Encrypt128(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&CHAP_C,
				cur_password
				);
			//fprintf(stderr, "SendRequest: Encrypt Header 1 !!!!!!!!!!!!!!!...\n");
		} else {
			Encrypt32(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&CHAP_C,
				(unsigned char*)&cur_password
//				(unsigned char*)&iPassword_v1
				);
		}
	}	
	
	//
	// Encrypt Header.
	//
	/*if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		//&& DataEncryptAlgo != 0	by limbear
		&& HeaderEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt128(
			(unsigned char*)pPdu->pDataSeg,
			iDataSegLen,
			(unsigned char *)&CHAP_C,
			password0
			);
		//fprintf(stderr, "SendRequest: Encrypt Header 2 !!!!!!!!!!!!!!!...\n");
	}*/

	// Send Request.
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		iResult = SendIt(
			connSock,
			(PCHAR)pHeader,
			(sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + 15) & 0xfffffff0 // Align 16 byte.
			);
	} else {
		iResult = SendIt(
			connSock,
			(PCHAR)pHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen // No align
			);
	}
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), _T("SendRequest: Send Request "));
		return -1;
	}
	
	return 0;
}

int
Login(
	  SOCKET			connsock,
	  UCHAR				cLoginType,
	  _int32			iUserID, // This user ID is 2.5 style. Requires converting for 1.0~2.0
	  unsigned char*	iPassword,
	  BOOL				Silent
	  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestPdu;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
	PBIN_PARAM_SECURITY					pParamSecu;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	PAUTH_PARAMETER_CHAP				pParamChap;
	LANSCSI_PDU_POINTERS				pdu;
	int									iSubSequence;
	int									iResult;
	unsigned							CHAP_I;
	//_int32								ActiveUserID;
	BOOLEAN SeagateTrial = FALSE;

	memcpy(cur_password, iPassword, PASSWORD_LENGTH);

	ActiveHwVersion = LANSCSIIDE_VERSION_1_0; 
	// Encryption and digest is turned off during login process. And turned on or off after negotiation.
	HeaderEncryptAlgo = 0 ;
	DataEncryptAlgo = 0;
	HeaderDigestAlgo = 0;
	DataDigestAlgo = 0;

retry_with_new_ver:
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
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
		pLoginRequestPdu->AHSLen = 0;
	} else {
		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	}
	pLoginRequestPdu->CSubPacketSeq = htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;

	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion;
	pLoginRequestPdu->VerMin = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;

	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;

	if (!Silent)
		_ftprintf(stderr, _T("[NdasCli]login: First.\n"));	
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("Login: Send First Request "));
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		_ftprintf(stderr, _T("[NdasCli]login: First Can't Read Reply.\n"));
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
		|| (pLoginReplyHeader->VerActive > HW_VER )
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		_ftprintf(stderr, _T("[NdasCli]login: BAD First Reply Header.\n"));
		return -1;
	}


	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		if (ActiveHwVersion != pLoginReplyHeader->VerActive)  {
			ActiveHwVersion = pLoginReplyHeader->VerActive;
			if (!Silent)
				_ftprintf(stderr, _T("[NdasCli]login: Retry with version %d.\n"), ActiveHwVersion);
			goto retry_with_new_ver;
		} else {
			if (!Silent)
				_ftprintf(stderr, _T("[NdasCli]login: First Failed.\n"));
		}
		
		return -1;
	} else {
		// Ver 1.0 seems to response with SUCCESS when version mismatched.
		if (ActiveHwVersion != pLoginReplyHeader->VerActive)  {
			ActiveHwVersion = pLoginReplyHeader->VerActive;
			if (!Silent)
				_ftprintf(stderr, _T("[NdasCli]login: Retrying with version %d.\n"), ActiveHwVersion);
			goto retry_with_new_ver;
		}
	}

	// Store Data.
	RPID = ntohs(pLoginReplyHeader->RPID);
	
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;

	ActiveHwVersion	= pLoginReplyHeader->VerActive;
	ActiveHwRevision = ntohs(pLoginReplyHeader->Revision);

	if (!Silent)
		printf("[NdasCli]login: Version %d Revision %x Auth %d\n", 
			ActiveHwVersion, 
			ActiveHwRevision,
			ntohs(pParamSecu->AuthMethod)
			);
	
	fprintf(stderr, "iUserID = %08X\n", iUserID);

	if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {
		// Convert to 1.0~2.0 User Id
		if (IS_SUPERVISOR(iUserID)) {
			ActiveUserId = NDAS_SUPERVISOR;
		} else {
			if (iUserID & USER_PERMISSION_EW) {
				if (iUserID & 0x0ff00) {
					// Target 1
					ActiveUserId = SECOND_TARGET_RW_USER;
					iTargetID = 1;
				} else {
					ActiveUserId = FIRST_TARGET_RW_USER;
					iTargetID = 0;
				}
			} else {
				if (iUserID & 0x0ff00) {
					// Target 1
					ActiveUserId = SECOND_TARGET_RO_USER;
					iTargetID = 1;
				} else {
					ActiveUserId = FIRST_TARGET_RO_USER;
					iTargetID = 0;
				}
			}
		}
#ifndef BUILD_FOR_DIST
		if (!Silent)
			printf("Not NDAS 2.5. Setting User ID to %x\n", ActiveUserId);
#endif
	} else {
		ActiveUserId = iUserID;
	}

	fprintf(stderr, "ActiveUserId = %08X\n", ActiveUserId);

	// 
	// Second Packet.
	//
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opcode = LOGIN_REQUEST;
	pLoginRequestPdu->HPID = htonl(HPID);
	pLoginRequestPdu->RPID = htons(RPID);
	pLoginRequestPdu->DataSegLen = 0;
	pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
	iSubSequence = 1;
	pLoginRequestPdu->CSubPacketSeq =  htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = PARAMETER_TYPE_BINARY;
	pLoginRequestPdu->ParameterVer = 0;

//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion;
	pLoginRequestPdu->VerMin = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5)
		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_AES128);	
	else
		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);	

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;
	
	if (!Silent)
		fprintf(stderr, "[NdasCli]login: Second.\n");
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("[NdasCli]Login: Send Second Request "));
		return -1;
	}

	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T != 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->VerActive > HW_VER)		
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Second Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]login: Second Failed.\n");
		return -1;
	}
	
	// Check Data segment.
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	// Minus AuthParamter[1]
			|| (pdu.pDataSeg == NULL)) {
			fprintf(stderr, "[NdasCli]login: BAD Second Reply Data.\n");
			return -1;
		}
	} else {
		if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
			|| (pdu.pDataSeg == NULL)) {	
			fprintf(stderr, "[NdasCli]login: BAD Second Reply Data.\n");
			return -1;
		}	
	}
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)
		//|| pParamSecu->AuthMethod != htons(0)
		|| pParamSecu->LoginType != cLoginType) {	
		
		fprintf(stderr, "[NdasCli]login: BAD Second Reply Parameters.\n");
		return -1;
	}
	
	// Store Challenge.	
	pParamChap = &pParamSecu->ChapParam;
	CHAP_I = ntohl(pParamChap->CHAP_I);
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		CHAP_C[0] = pParamChap->V2.CHAP_CR[0]; // endian conversion??
		CHAP_C[1] = pParamChap->V2.CHAP_CR[1];
		CHAP_C[2] = pParamChap->V2.CHAP_CR[2];
		CHAP_C[3] = pParamChap->V2.CHAP_CR[3];
	} else {
		CHAP_C[0] = ntohl(pParamChap->V1.CHAP_C[0]);
	}
#if 0
	printf("[NdasCli]login: Hash %d, Challenge %x %x %x %x\n", 
		ntohl(pParamChap->CHAP_A), 
		CHAP_C[0], CHAP_C[1], CHAP_C[2], CHAP_C[3]
		);
#endif

//retry_with_seagate_password:
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

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
		pLoginRequestPdu->AHSLen = 0;
	} else {
		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	}
	iSubSequence = 2;
	pLoginRequestPdu->CSubPacketSeq = htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;

//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion; // temp
	pLoginRequestPdu->VerMin = 0;
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;
	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5)
		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_AES128);	
	else
		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);

	pParamChap->CHAP_I = htonl(CHAP_I);
	if (ActiveHwVersion <= LANSCSIIDE_VERSION_2_0 && LOGIN_TYPE_NORMAL != cLoginType) {
		// Change user ID to 0 for discovery login.
		pParamChap->CHAP_N = htonl(0);
	} else {
		pParamChap->CHAP_N = htonl(ActiveUserId);
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		AES_cipher((unsigned char*)CHAP_C, (unsigned char*)pParamChap->V2.CHAP_CR, (unsigned char*)iPassword);
//		AES_cipher_dummy((unsigned char*)CHAP_C, (unsigned char*)pParamChap->CHAP_R, (unsigned char*)iPassword);
#if 0
		printf("CHAP_C: %08x %08x %08x %08x\n", CHAP_C[0], CHAP_C[1], CHAP_C[2], CHAP_C[3]);
		printf("CHAP_R: %08x %08x %08x %08x\n", 
			pParamChap->V2.CHAP_CR[0], pParamChap->V2.CHAP_CR[1], 
			pParamChap->V2.CHAP_CR[2], pParamChap->V2.CHAP_CR[3]
		);
		printf("Password: ");
		PrintHex(iPassword, 16);
		printf("\n");
#endif
	} else {
		if (ActiveUserId != NDAS_SUPERVISOR) {
			if (memcmp(iPassword, def_password0, sizeof(def_password0)) ==0) {
				if (SeagateTrial) {
					printf("Using default seagate password\n");
					Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->V1.CHAP_R, (PUCHAR)&iPassword_v1_seagate);
					memcpy(cur_password, (PUCHAR)&iPassword_v1_seagate, PASSWORD_LENGTH_V1);
				} else {
//					printf("Using default password\n");

					Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->V1.CHAP_R, (PUCHAR)&iPassword_v1);
					memcpy(cur_password, (PUCHAR)&iPassword_v1, PASSWORD_LENGTH_V1);
				}
			} else {
				printf("Password=%s\n", iPassword);
				Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->V1.CHAP_R, iPassword);
				memcpy(cur_password, iPassword, PASSWORD_LENGTH_V1);
			}
		} else {		
			if (memcmp(iPassword, def_supervisor_password, sizeof(def_supervisor_password)) ==0) {
//				printf("Using default password\n");
				Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->V1.CHAP_R, (PUCHAR)&iSuperPassword_v1);
				memcpy(cur_password, (PUCHAR)&iSuperPassword_v1, PASSWORD_LENGTH_V1);
			} else {
				Hash32To128((unsigned char*)&CHAP_C, (unsigned char*)pParamChap->V1.CHAP_R, iPassword);
				memcpy(cur_password, iPassword, PASSWORD_LENGTH_V1);
			}
		}
	}

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamSecu;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("Login: Send Third Request "));
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| (pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)
		|| (pLoginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)
		|| (pLoginReplyHeader->VerActive > HW_VER)	
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Third Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]login: Third Failed: %x ", pLoginReplyHeader->Response);
		if (pLoginReplyHeader->Response == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				fprintf(stderr, "Password mismatch\n");
			} else {
				if (memcmp(cur_password, (PUCHAR)&iPassword_v1, PASSWORD_LENGTH_V1) == 0) {
					SeagateTrial = TRUE;
					fprintf(stderr, " - Retrying with seagate password\n");
					goto retry_with_new_ver;
					// Retrying step 3 only does not work.
//					goto retry_with_seagate_password; 
				}
				fprintf(stderr, "Password mismatch or write-user exists\n");
			}
		} else if (pLoginReplyHeader->Response == LANSCSI_RESPONSE_T_NO_PERMISSION) {
			fprintf(stderr, "No permission\n");
		} else if (pLoginReplyHeader->Response == LANSCSI_RESPONSE_T_CONFLICT_PERMISSION) {
			fprintf(stderr, "Another user has logged in\n");
		} else if (pLoginReplyHeader->Response == LANSCSI_RESPONSE_RI_AUTH_FAILED) {
			fprintf(stderr, "Password mismatch(only some emulators return this error)\n");
		} else {
			fprintf(stderr, "Unexpected error code\n");
		}
		return -1;
	}
	
	// Check Data segment.
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) 
			|| (pdu.pDataSeg == NULL)) {
			fprintf(stderr, "[NdasCli]login: BAD Third Reply Data.\n");
			return -1;
		}
	} else {
		if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
			|| (pdu.pDataSeg == NULL)) {			
			fprintf(stderr, "[NdasCli]login: BAD Third Reply Data.\n");
			return -1;
		}	
	}
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
		//|| pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)
		//|| pParamSecu->AuthMethod != htons(0)
		|| pParamSecu->LoginType != cLoginType){		
		fprintf(stderr, "[NdasCli]login: BAD Third Reply Parameters.\n");
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

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
		pLoginRequestPdu->AHSLen = 0;
	} else {
		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	}
	iSubSequence = 3;
	pLoginRequestPdu->CSubPacketSeq = htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag = htonl(iTag);
	pLoginRequestPdu->ParameterType = 1;
	pLoginRequestPdu->ParameterVer = 0;
//	inserted by ilgu 2003_0819
	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion; // temp
	pLoginRequestPdu->VerMin = 0;	
	pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
	
	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (char *)pParamNego;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("Login: Send Fourth Request "));
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]login: Fourth Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
		|| (pLoginReplyHeader->T == 0)
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
		|| ((pLoginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
		|| (pLoginReplyHeader->VerActive > HW_VER)	
		|| (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Header.\n");
		return -1;
	}
	
	if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]login: Fourth Failed.\n");
		return -1;
	}
	
	// Check Data segment.
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)	
			|| (pdu.pDataSeg == NULL)) {
			fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Data.\n");
			return -1;
		}
	} else {
		if((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY)
			|| (pdu.pDataSeg == NULL)) {
			
			fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Data.\n");
			return -1;
		}	
	}
	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
		fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Parameters.\n");
		return -1;
	}
	if (!Silent) {
		printf("[NdasCli]login: Hw Type %d, Hw Version %d, NRSlots %d, MaxBlocks %d, MaxTarget %d MLUN %d\n", 
			pParamNego->HWType, pParamNego->HWVersion,
			ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
			ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID)
			);
		printf("[NdasCli]login: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
			pParamNego->HeaderEncryptAlgo,
			pParamNego->HeaderDigestAlgo,
			pParamNego->DataEncryptAlgo,
			pParamNego->DataDigestAlgo
			);
	}
	requestBlocks = (short)(ntohl(pParamNego->MaxBlocks));
	MaxPendingTasks = (short)ntohl(pParamNego->NRSlot);
	HeaderEncryptAlgo = pParamNego->HeaderEncryptAlgo;
	DataEncryptAlgo = pParamNego->DataEncryptAlgo;
	HeaderDigestAlgo = pParamNego->HeaderDigestAlgo;
	DataDigestAlgo = pParamNego->DataDigestAlgo;
	iSessionPhase = FLAG_FULL_FEATURE_PHASE;

	if (ActiveUserId == NDAS_SUPERVISOR) {
		// actually this should be user password...
		memcpy(cur_password, (void*)&iPassword_v1, PASSWORD_LENGTH);
	}

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

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
		pRequestHeader->AHSLen = 0;	
	} else {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	}
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
		PrintError(WSAGetLastError(), _T("TextTargetList: Send First Request "));
		return -1;
	}
	
	// Read Request.
//	fprintf(stderr, "[NdasCli]TextTargetList: step 3.\n");
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
//	fprintf(stderr, "[NdasCli]TextTargetList: step 2.\n");
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]TextTargetList: Can't Read Reply.\n");
		return -1;
	}
//	fprintf(stderr, "[NdasCli]TextTargetList: step 1.\n");
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;


	// Check Request Header.
	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]TextTargetList: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]TextTargetList: Failed.\n");
		return -1;
	}
//	changed by ILGU 2003_0819
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		if(ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
			fprintf(stderr, "[NdasCli]TextTargetList: No Data Segment.\n");
			return -1;		
		}
	} else {
		if(ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
			fprintf(stderr, "[NdasCli]TextTargetList: No Data Segment.\n");
			return -1;		
		}
	}
	pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
		fprintf(stderr, "TEXT: Bad Parameter Type.: %d\n",pParam->ParamType);
		return -1;			
	}
	printf("[NdasCli]TextTargetList: NR Targets : %d\n", pParam->NRTarget);
	NRTarget = pParam->NRTarget;
	
	for(int i = 0; i < pParam->NRTarget; i++) {
		PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
		int								iTargetId;
		
		pTarget = &pParam->PerTarget[i];
		iTargetId = ntohl(pTarget->TargetID);
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			printf("[NdasCli]TextTargetList: Target ID: %d, NR_EW: %d, NR_SW: %d, NR_RO: %d, Data: %I64d \n",  
				ntohl(pTarget->TargetID), 
				pTarget->V2.NREWHost,
				pTarget->V2.NRSWHost,
				pTarget->V2.NRROHost,
				pTarget->TargetData
				);
			PerTarget[iTargetId].V2.NREWHost = pTarget->V2.NREWHost;
			PerTarget[iTargetId].V2.NRSWHost = pTarget->V2.NRSWHost;
			PerTarget[iTargetId].V2.NRROHost = pTarget->V2.NRROHost;
		} else {
			printf("[NdasCli]TextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: %I64d \n",  
				ntohl(pTarget->TargetID), 
				pTarget->V1.NRRWHost,
				pTarget->V1.NRROHost,
				pTarget->TargetData
				);
			PerTarget[iTargetId].V1.NRRWHost = pTarget->V1.NRRWHost;
			PerTarget[iTargetId].V1.NRROHost = pTarget->V1.NRROHost;
		}
		PerTarget[iTargetId].bPresent = TRUE;
		PerTarget[iTargetId].TargetData = pTarget->TargetData;		
	}
	
	return 0;
}

int
TextTargetData(
			   SOCKET	connsock,
			   UCHAR	cGetorSet,
			   UINT		TargetID,
			   UINT64*	TargetData
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

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
		pRequestHeader->AHSLen = 0;
	} else {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	
	// Make Parameter.
	pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_DATA;
	pParam->GetOrSet = cGetorSet;
	pParam->TargetData = *TargetData;
	pParam->TargetID = htonl(TargetID);
	
//	printf("TargetID %d, %I64d\n", ntohl(pParam->TargetID), pParam->TargetData);

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg = (char *)pParam;

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("TextTargetData: Send First Request "));
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]TextTargetData: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opcode != TEXT_RESPONSE)
		|| (pReplyHeader->F == 0)
		|| (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]TextTargetData: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]TextTargetData: Failed.\n");
		return -1;
	}
//	changed by ILGU 2003_0819
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
			fprintf(stderr, "[NdasCli]TextTargetData: No Data Segment.\n");
			return -1;			
		}
	} else {
		if(ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
			fprintf(stderr, "[NdasCli]TextTargetData: No Data Segment.\n");
			return -1;		
		}
	}
	pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;

	if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
		fprintf(stderr, "TextTargetData: Bad Parameter Type. %d\n", pParam->ParamType);
	//	return -1;			
	}

	*TargetData = pParam->TargetData;

//	printf("[NdasCli]TextTargetList: TargetID : %d, GetorSet %d, Target Data %d\n", 
//		ntohl(pParam->TargetID), pParam->GetOrSet, *TargetData);
	
	return 0;
}

//
// If AhsLen == 0, AHS is used for getting data.
int
VendorCommand(
			  SOCKET			connsock,
			  UCHAR				cOperation,
			  unsigned _int32	*pParameter0,
			  unsigned _int32	*pParameter1,
			  unsigned _int32	*pParameter2,
			  CHAR				*AHS,
			  int				AhsLen,
			  CHAR				*OptData
			  )
{
	_int8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS							pdu;
	int									iResult;
	UINT32 LocalParam0, LocalParam1, LocalParam2;

	if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {
		switch(cOperation) {
		case VENDOR_OP_GET_WRITE_LOCK:
		case VENDOR_OP_FREE_WRITE_LOCK:
		case VENDOR_OP_GET_HOST_LIST:
		// add more..
			fprintf(stderr, "\nUnsupported vendor operation for this HW: %x\n", cOperation);
			return 0;
		}
	}

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	LocalParam0 = LocalParam1 = LocalParam2 = 0;

	pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode = VENDOR_SPECIFIC_COMMAND;
	pRequestHeader->F = 1;
	pRequestHeader->HPID = htonl(HPID);
	pRequestHeader->RPID = htons(RPID);
	pRequestHeader->CPSlot = 0;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		pRequestHeader->DataSegLen = htonl((long)AhsLen);
		pRequestHeader->AHSLen = 0;
	} else {
		pRequestHeader->DataSegLen = 0;
		pRequestHeader->AHSLen = htons((short)AhsLen);
	}
	pRequestHeader->CSubPacketSeq = 0;
	pRequestHeader->PathCommandTag = htonl(++iTag);
	pRequestHeader->VendorID = ntohs(NKC_VENDOR_ID);
	pRequestHeader->VendorOpVersion = VENDOR_OP_CURRENT_VERSION;
	pRequestHeader->VendorOp = cOperation;
	if (pParameter0) 
		LocalParam0 = pRequestHeader->VendorParameter0 = *pParameter0;
	if (pParameter1)
		LocalParam1 = pRequestHeader->VendorParameter1 = *pParameter1;
	if (pParameter2)
		LocalParam2 = pRequestHeader->VendorParameter2 = *pParameter2;

//	printf("VendorCommand: Operation %d\n", cOperation);

	// Send Request.
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if (AHS && AhsLen) {
		pdu.pAHS = &PduBuffer[sizeof(LANSCSI_VENDOR_REQUEST_PDU_HEADER)];
		memcpy(pdu.pAHS, AHS, AhsLen);
	}

	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("VendorCommand: Send First Request "));
		return -1;
	}

	if (cOperation == VENDOR_OP_RESET) 
		return 0;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 &&
		(cOperation == VENDOR_OP_GET_EEP || cOperation == VENDOR_OP_U_GET_EEP)) {
		int len = htonl(*pParameter2);
		if (DataDigestAlgo != 0) 
			len+=16;
		iResult = RecvIt(
			connsock, 
			OptData, 
			len
		);
		if(iResult <= 0) {
			PrintError(WSAGetLastError(), _T("VendorCommand: Receive Data for READ "));
			printf("RR\n");
			return -1;
		}
		if(DataEncryptAlgo != 0) {
			Decrypt128(
				(unsigned char*)OptData,
				len,
				(unsigned char*)&CHAP_C,
				cur_password
			);
		} 
		if(DataDigestAlgo != 0) {
			unsigned int crc;
			crc = ((unsigned *)OptData)[htonl(*pParameter2)];

			CRC32(
				(unsigned char *)OptData,
				&(((unsigned char *)OptData)[htonl(*pParameter2)]),
				htonl(*pParameter2)
			);

			if(crc != ((unsigned *)OptData)[htonl(*pParameter2)]) {
				fprintf(stderr, "Read data Digest Error !!!!!!!!!!!!!!!...\n");
			}
		}
	}
	
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 && 
		(cOperation == VENDOR_OP_SET_EEP || cOperation == VENDOR_OP_U_SET_EEP)) {
		unsigned int DataLength = htonl(*pParameter2);

		if(DataDigestAlgo != 0) {
			CRC32(
				(unsigned char *)OptData,
				&(((unsigned char *)OptData)[DataLength]),
				DataLength
			);
			DataLength += 16; //CRC + Padding for 16 byte align.
		}
		if(DataEncryptAlgo != 0) {
			Encrypt128(
				(unsigned char*)OptData,
				DataLength,
				(unsigned char *)&CHAP_C,
				cur_password
			);
		}

		iResult = SendIt(
			connsock,
			OptData,
			DataLength
		);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "VendorCommand: Failed to send data for WRITE\n");
			return -1;
		}
	}


	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]VendorCommand: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.pR2HHeader;


	if((pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		fprintf(stderr, "[NdasCli]VendorCommand: BAD Reply Header. Opcode=0x%x 0x%x\n", pReplyHeader->Opcode, pReplyHeader->F);
		return -1;
	}

	if (ntohs(pReplyHeader->AHSLen) !=0 && AHS!=NULL) {
		memcpy(AHS, &PduBuffer[sizeof(LANSCSI_VENDOR_REPLY_PDU_HEADER)], ntohs(pReplyHeader->AHSLen));
	}

	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
//		fprintf(stderr, "[NdasCli]VendorCommand: Failed.\n");
		// In some case, reply packet has valid values when command is failed.
		if (pParameter0)
			*pParameter0 = pReplyHeader->VendorParameter0;
		if (pParameter1)
			*pParameter1 = pReplyHeader->VendorParameter1;
		if (pParameter2)
			*pParameter2 = pReplyHeader->VendorParameter2;
		//exit(0);
		return pReplyHeader->Response;
	}
	if (pParameter0)
		*pParameter0 = pReplyHeader->VendorParameter0;
	if (pParameter1)
		*pParameter1 = pReplyHeader->VendorParameter1;
	if (pParameter2)
		*pParameter2 = pReplyHeader->VendorParameter2;

	// Change operation mode for some operation
	if (cOperation == VENDOR_OP_SET_D_OPT) {
		UINT32 Option = ntohl(LocalParam2);		
		DataEncryptAlgo = (Option & 0x1)?ENCRYPT_ALGO_AES128:ENCRYPT_ALGO_NONE;
		HeaderEncryptAlgo = (Option & 0x2)?ENCRYPT_ALGO_AES128:ENCRYPT_ALGO_NONE;
		DataDigestAlgo = (Option & 0x4)?DIGEST_ALGO_CRC32:DIGEST_ALGO_NONE;
		HeaderDigestAlgo = (Option & 0x8)?DIGEST_ALGO_CRC32:DIGEST_ALGO_NONE;
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

		PrintError(WSAGetLastError(), _T("[NdasCli]Logout: Send Request "));
		return -1;
	}
	
	// Read Request.
	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	if(iResult == SOCKET_ERROR) {
		fprintf(stderr, "[NdasCli]Logout: Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.
	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if((pReplyHeader->Opcode != LOGOUT_RESPONSE)
		|| (pReplyHeader->F == 0)) {
		
		fprintf(stderr, "[NdasCli]Logout: BAD Reply Header.\n");
		return -1;
	}
	
	if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
		fprintf(stderr, "[NdasCli]Logout: Failed.\n");
		return -1;
	}
	
	iSessionPhase = FLAG_SECURITY_PHASE;

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
				(unsigned char *)wbuf,
				&(((unsigned char *)wbuf)[DataLength]), 
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
					(unsigned char*)wbuf,
					DataLength,
					(unsigned char *)&CHAP_C,
					cur_password
					);
			} else {
				Encrypt32(
					(unsigned char*)wbuf,
					DataLength,
					(unsigned char *)&CHAP_C,
					(unsigned char*)&cur_password
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
					(unsigned char*)wbuf,
					DataLength,
					(unsigned char *)&CHAP_C,
					cur_password
					);
				if (DataDigestAlgo !=0) {
					Encrypt128(
						(unsigned char*)CrcBuf,
						16,
						(unsigned char *)&CHAP_C,
						cur_password
						);			
				}
			} else {
				Encrypt32(
					(unsigned char*)wbuf,
					DataLength,
					(unsigned char *)&CHAP_C,
					(unsigned char*)&iPassword_v1
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
						(unsigned char*)Buf,
						DataLength,
						(unsigned char*)&CHAP_C,
						cur_password
						);
				} else {
					Decrypt32(
						(unsigned char*)Buf,
						DataLength,
						(unsigned char*)&CHAP_C,
						(unsigned char*)&cur_password
						);
				}
			//fprintf(stderr, "IdeCommand: WIN_READ Encrypt data 1 !!!!!!!!!!!!!!!...\n");
			}

			if(DataDigestAlgo != 0) {
				crc = ((unsigned *)Buf)[Task->SectorCount * 128];

				CRC32(
					(unsigned char *)Buf,
					&(((unsigned char *)Buf)[Task->SectorCount * 512]),
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
						(unsigned char*)Buf,
						DataLength,
						(unsigned char*)&CHAP_C,
						cur_password
						);
				} else {
					Decrypt32(
						(unsigned char*)Buf,
						DataLength,
						(unsigned char*)&CHAP_C,
						(unsigned char*)&cur_password
						);
				}
				//fprintf(stderr, "IdeCommand: WIN_IDENTIFY Encrypt data 1 !!!!!!!!!!!!!!!...\n");
			}

			if(DataDigestAlgo != 0) {
				crc = ((unsigned *)Buf)[128];

				CRC32(
					(unsigned char*)Buf,
					&(((unsigned char*)Buf)[512]),
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
		printf("Check Power mode = 0x%02x", (unsigned char)(pReplyHeader->SectorCount_Curr));
		switch((unsigned char)(pReplyHeader->SectorCount_Curr)) {
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
	char							data2[1024];
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
	pRequestHeader->PKCMD[8] = (char)x;
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
	pdu.pDataSeg = (char *)pRequestHeader->PKCMD;
	
	xxx = clock();
	if(SendRequest(connsock, &pdu) != 0) {
		PrintError(WSAGetLastError(), _T("IdeCommand: Send Request "));
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
			UINT	TargetId,
			BOOL	Silent,
			BOOL	SetDefaultTransferMode
			)
{
	struct hd_driveid	info;
	int					iResult;
	char				buffer[41];
	int dma_mode;
	int set_dma_feature_mode;

	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "Identify Failed...\n");
		return iResult;
	}

	printf("0 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[1]), (unsigned char)(((PCHAR)&info)[0]));
	printf("2 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[5]), (unsigned char)(((PCHAR)&info)[4]));
	printf("10 words  0x%c%c\n", (unsigned char)(((PCHAR)&info)[21]), (unsigned char)(((PCHAR)&info)[20]));
	printf("47 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[95]), (unsigned char)(((PCHAR)&info)[94]));
	printf("49 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[99]), (unsigned char)(((PCHAR)&info)[98]));
	printf("59 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[119]), (unsigned char)(((PCHAR)&info)[118]));

	//if((iResult = IdeCommand(connsock, TargetId, 0, WIN_SETMULT, 0, 0x08, 0, NULL)) != 0) {
	//		fprintf(stderr, "[NdasCli]GetDiskInfo: Set Feature Failed...\n");
	//		return iResult;
	//}
//	printf("47 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[95]), (unsigned char)(((PCHAR)&info)[94]));
//	printf("59 words  0x%02x%02x\n", (unsigned char)(((PCHAR)&info)[119]), (unsigned char)(((PCHAR)&info)[118]));

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
	char				buffer[41];

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


//
// Discovery
//
int
Discovery(
		  SOCKET	connsock
		  )
{
	int	iResult;
	UCHAR Password[PASSWORD_LENGTH];
	UINT32 UserID;

	memcpy(Password, def_password0, PASSWORD_LENGTH);
	UserID = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);

	//////////////////////////////////////////////////////////
	//
	// Login Phase...
	//
//	fprintf(stderr, "[NdasCli]Discovery: Before Login \n");
	if((iResult = Login(connsock, LOGIN_TYPE_DISCOVERY, UserID, Password, FALSE)) != 0) {
		fprintf(stderr, "[NdasCli]Discovery: Login Failed...\n");
		return iResult;
	}
	
//	fprintf(stderr, "[NdasCli]Discovery: After Login \n");
	if((iResult = TextTargetList(connsock)) != 0) {
		fprintf(stderr, "[NdasCli]Discovery: Text Failed\n");
		return iResult;
	}
//	fprintf(stderr, "[NdasCli]Discovery: After Text \n");
	///////////////////////////////////////////////////////////////
	//
	// Logout Packet.
	//
	if((iResult = Logout(connsock)) != 0) {
		fprintf(stderr, "[NdasCli]Discovery: Logout Failed...\n");
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



void
usage(void)
{
	int i;
	printf("Usage: ndascli [Command] [Target MAC] [Command Arg0] [Command Arg1] ...\n");
//	printf("	If no command line argument is given, default command in source is used.\n");
	printf("	UserID should be given in 2.5 style. UserId will be converted into legacy for NDAS ~2.0\n");
	for(i=0;i<NR_CLI_CMD;i++) {
		printf(" %s\n\t%s\n", CommandList[i].Cmd, CommandList[i].Help);
	}
}

int
__cdecl
main(int argc, char* argv[])
{
	WORD				wVersionRequested;
	WSADATA				wsaData;
	int					err;
	
	PUCHAR				data = NULL;
	PUCHAR				data2 = NULL;
	unsigned _int64		i;
	int retval = 0;
	char* TargetNode;
	char* Cmd;
	char* Arg[6] = {0};
	//int keyin;

	// Common initailization
	wVersionRequested = MAKEWORD( 2, 2 );
	
	err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0) {
		PrintError(WSAGetLastError(), _T("main: WSAStartup "));
		retval = -1;
		goto cleanup;
	}

#ifdef BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX
	fprintf(stderr, "This software will change all NDAS 1.1 on the network to default config. Will you continue?(y/n)");
	keyin = getchar();
	if (keyin == 'y' || keyin == 'Y') {
		CmdSetDefaultConfigAuto(NULL,NULL);
	}
#else
	if (argc==1 && !(DefaultCommand==NULL || DefaultCommand[0]==0)) {
		char* pos;
		const char* delim= " \n";
		fprintf(stderr, "Using default command: %s\n", DefaultCommand);

		Cmd = strtok(DefaultCommand, delim);
		TargetNode = strtok(NULL, delim);
		pos = TargetNode;
		i=0;
		while(pos) {
			pos = Arg[i] = strtok(NULL, delim);
			i++;
			if (i>=6)
				break;
		}
	} else if (argc<2 || argc>9) {
		usage();
		goto cleanup;
	} else {
		Cmd = argv[1];
		TargetNode = argv[2];
		for(i=3;i<argc;i++) {
			Arg[i-3] = argv[i];
		}
	}
	for(i=0;i<NR_CLI_CMD;i++) {
		if (_stricmp(CommandList[i].Cmd, Cmd)==0) {
			retval = CommandList[i].Func(TargetNode, Arg);
			goto cleanup;
		}
	}
	printf("Unknown command %s\n", Cmd);
	usage();
	retval = -1;
#endif	

cleanup:
	err = WSACleanup();
	if(err != 0) {
		PrintError(WSAGetLastError(), _T("main: WSACleanup "));
	}
	return retval;
}
