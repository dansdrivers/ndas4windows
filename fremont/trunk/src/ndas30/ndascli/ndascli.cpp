/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2008, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 12/11/2008
 -------------------------------------------------------------------------
*/

// ndascli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

LONG DbgLevelNdasCli = DBG_LEVEL_NDAS_CLI;

#define NdasEmuDbgCall(l,x,...) do {							\
    if (l <= DbgLevelNdasCli) {									\
		fprintf(stderr,"|%d|%s|%d|",l,__FUNCTION__, __LINE__);	\
		fprintf(stderr,x,__VA_ARGS__);							\
    } 															\
} while(0)


#define Decrypt32	Decrypt32_l
#define Encrypt32	Encrypt32_l
#define Hash32To128 Hash32To128_l

UINT64	iPassword_v1			= 0x1f4a50731530eabb;
UINT64	iPassword_v1_seagate	= 0x99a26ebc46274152;
UINT64	iSuperPassword_v1		= 0x3E2B321A4750131E;

INT		ActiveHwVersion = 0;

INT32	HPID = 0;
INT16	RPID = 0;
INT32	iTag = 0;

INT		iSessionPhase		= 0;

UINT16	HeaderEncryptAlgo	= 0;
UINT16	DataEncryptAlgo		= 0;
UINT16	HeaderDigestAlgo	= 0;
UINT16	DataDigestAlgo		= 0;

INT		ActiveHwRevision = 0;
UINT32	ActiveUserId	 = 0;
INT		iTargetID		 = 0;
UINT16	requestBlocks;

UINT	CHAP_C[4];

UCHAR cur_password[PASSWORD_LENGTH] = {0}; // Set by Login. To do: Move to per session data.

#ifdef BUILD_FOR_DIST

CHAR DefaultCommand[] = "";

#else

CHAR DefaultCommand[] = 

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

typedef struct _CLI_COMMAND {

	CHAR			*Cmd;
	CLI_CMD_FUNC	Func;
	CHAR			*Help;
	INT				NeedArg;

} CLI_COMMAND, *PCLI_COMMAND;

CLI_COMMAND CommandList[] = {

#ifndef BUILD_FOR_DIST
	
	{"AesTest",				CmdAesLibTest, "Compare result of SW AES lib and VHDL's AES lib. Not implemented", 0},
	{"BatchTest",			CmdBatchTest, "Batch Test", 0} ,
	{"BLDeadlockTest",		CmdBufferLockDeadlockTest, "Test write buffer deadlock timeout is working \n\tParameter: <Seconds>", 1},
	{"BlockVariedIo",		CmdBlockVariedIo, "Io with various block size. Set 0 for random sector count \n\tParameter: <Write Size(MB)> <Pos in MB> <Iteration> <Sector count>", 4},
	{"CheckPowerMode",		CmdCheckPowerMode, "Check power mode \n\tParameter: none", 0}, 
	{"DigestTest",			CmdDigestTest, "Try to IO with bad CRC \n\tParameter: none", 0},
	{"Discovery",			CmdDiscovery, "Login in discovery mode \n\tParameter: none", 0},
	{"DynamicOptionTest",	CmdDynamicOptionTest, "IO with various option \n\tParameter: none", 0},

	{"GetEEP",				CmdGetEep, "Get EEP contents  \n\tParameter: <UserId> <Address> <Length>", 3}, 
	{"SetEEP",				CmdSetEep, "Set EEP contents  \n\tParameter: <UserId> <Address> <Length> <FileName>", 4},
	{"GetUEEP",				CmdGetUEep, "Get User EEP contents  \n\tParameter: <UserId> <Address> <Length>", 3}, 
	{"SetUEEP",				CmdSetUEep, "Set User EEP contents  \n\tParameter: <UserId> <Address> <Length> <FileName>", 4},
	{"GetDumpEEP",			CmdGetDumpEep, "Get Dump contents of EEPROM \n\tParameter: none", 0}, 
	{"SetDumpEEP",			CmdSetDumpEep, "Set Dump contents of EEPROM  \n\tParameter: <UserId> <Address> <Length> <FileName>", 4},

	{"GenBc",				CmdGenBc, "Generate broadcast message. Added for testing MAC crash \n\tParameter: none", 0},
	{"GetMaxAddr",			CmdGetNativeMaxAddr, "Get native max address \n\tParameter: [Dev]", 0},
	{"HostList",			CmdHostList, "List hosts connected to NDAS device \n\tParameter: none", 0},
	{"LockedWrite",			CmdLockedWrite, 
	 "Write using HW buffer lock and read for correctness  \n\tParameter: <Write Size(MB)> <Iterations> <Pos in MB> <LockMode> [UserId] [Blocks] \n\tLockMode ex: 0 for no lock, 0x21 for yield lock and vendor cmd. 0x4 for mutex-0 as write-lock.",
	 4},

	{"LoginR",				CmdLoginR, "Login with given permission and password and try read \n\tParameter: <UserId+Permission> [PW]", 1},
	{"LoginRw",				CmdLoginRw, "Login with given permission and password and try IO \n\tParameter: <UserId+Permission> [PW]", 1},
	{"LoginWait",			CmdLoginWait, "Login with given permission and password and wait for seconds  \n\tParameter: <UserId+Permission> <Time to wait> [PW]", 2},

	{"LpxConnect",			CmdLpxConnect, "Create lpx connection and wait \n\tParameter: none", 0},

	{"MutexCli",			CmdMutexCli, "Connect and run mutex command from user input \n\tParameter: none", 0},
	{"MutexTest1",			CmdMutexTest1, "Try to take mutex, increase value and give it\n\tParameter: <LockId>", 1},

	{"Nop",					CmdNop, "Send NOP and receive reply \n\tParameter: none", 0},

	{"PnpListen",			CmdPnpListen, "Wait for PNP broadcast  \n\tParameter: <Host Network card's MAC address>", 1},
	{"PnpRequest",			CmdPnpRequest, "Send and receive PNP request \n\tParameter: <Host Network card's MAC address>", 1},

	{"RawVendor",			CmdRawVendorCommand, "Run RAW vendor command \n\tParameter: <UserId> <Code> <Password> <Param0> <Param1> <Param2>", 6},
	{"ResetAccount",		CmdResetAccount, "Reset account's permission and password to default  \n\tParameter: [Superuser PW]", 6},
	{"SetMac",				CmdSetMac, "Set MAC address \n\tParameter: <MAC>", 1},
	{"SetPacketDrop",		CmdSetPacketDrop, "Set number of packets to drop out of 1000 packets  \n\tparameter: <\"rx\" or \"tx\"> <Packets to drop>", 2},

	{"SetPermission",		CmdSetPermission, "Set user permission  \n\tParameter: <UserId+Permission> [Superuser PW]", 1},
	{"SetPw",				CmdSetPassword, "Set password in superuser mode  \n\tParameter: <UserNum> <New PW> [Superuser PW]", 2},
	{"SetUserPw",			CmdSetUserPassword, "Set user password  \n\tParameter: <UserNum> <New PW> [Current PW]", 2},

	{"ShowAccount",			CmdShowAccount, "Show user account Give superuser password if it is changed  \n\tParameter: [Superuser PW]"},

	{"Standby",				CmdStandby, "Enter standby mode \n\tParameter: none", 0},
	{"StandbyTest",			CmdStandbyTest, "Test NDAS standby bug \n\tParameter: none", 0},
	{"StandbyIo",			CmdStandbyIo, "Enter standby mode and try to IO after 5 seconds  \n\tParameter: <DevNum> <R or W>", 2},

	{"TestVendor0",			CmdTestVendorCommand0, "Test vendor commands \n\tParameter: none", 0},
	{"TransferModeIo",		CmdTransferModeIo, "IO with given transfer mode  \n\tParameter: <Write Size(MB)> <Pos in MB> <P/D/U><0~7> [Dev] [SectorSize]", 3},
	{"TTD",					CmdTextTargetData, "Get/Set Text target data  \n\tParameter: <Get|Set> [Set data]", 1},
	// {"TTL",				CmdTextTargetList, "Get Text target list  No parameter"}, // Use discovery
	
	{"ViewMeta",			CmdViewMeta, "Show NDAS meta data  \n\tParameter: [Dev]\n"},
	{"WriteCache",			CmdWriteCache, "Enable or disable write cache.\n\tParameter: [on|off] [Dev]"},

	{"FlushTest",			CmdFlushTest, "Write some data with flush command  \n\tParameter: <Write Size(MB)> <Iteration> <Pos in MB> ", 3},
	{"Identify",			CmdIdentify, "Show identify info  \n\tParameter: [Dev]\n"},
	{"Read",				CmdRead, "Read. Set 0 for random sector count  \n\tParameter: <Read Size(MB)> <Iterations> <Pos in MB> <Sector count>", 4},
	{"SetMode",				CmdSetMode, "Set given transfer mode  \n\tParameter: <P/D/U><0~7> [Dev]", 1},
	{"Smart",				CmdSmart, "Run smart \n\tParameter: none", 0},
	{"Write",				CmdWrite, "Write. Set 0 for random sector count  \n\tParameter: <Write Size(MB)> <Iterations> <Pos in MB> <Sector count>", 4},
	{"Verify",				CmdVerify, "Verify. Set 0 for random sector count  \n\tParameter: <Verify Size(MB)> <Iterations> <Pos in MB> <Sector count>", 4},

	{"DelayedIo",			CmdDelayedIo, "IO after setting inter-packet delay.\n\tparameter: <Delay>", 1},
	{"ExIo",				CmdExIo, "Io in exclusive write mode  \n\tParameter: <Write Size(MB)> <Pos in MB> <Iteration>", 3},
	{"InterleavedIo",		CmdInterleavedIo, "Read/write in interleaved manner  \n\tparameter: <Write Size(MB)> <Pos in MB>", 2},

	{"CheckFile",			CmdCheckFile, "Read from disk and check contents with given file  \n\tParameter: <File name> <Iteration> <Pos in sector> [Dev] [SectorCount] [Transfermode]\n", 3},
	{"ReadFile",			CmdReadFile, "Read from disk and write to file  \n\tParameter: <File name> <Pos in sector> [Dev=0] [SectorCount=1] [Transfermode]\n", 2},
	{"WriteFile",			CmdWriteFile, "Write file to disk  \n\tParameter: <File name> <Iteration> <Pos in sector> [Dev] [SectorCount] [Transfermode]\n", 3}, 

	{"ReadPattern",			CmdReadPattern, "Read and check pattern with random block size  <Pattern#> <Read Size(MB)> <Pos in MB> [Block Size] [UserId] [PW]", 3},
	{"WritePattern",		CmdWritePattern, "Write pattern with random block size  <Pattern#> <Write Size(MB)> <Pos in MB> [Block Size] [UserId] [PW]", 3},
	// {"Compare", CmdCompare, "Compare contents of two disk.\n"},

#endif

	{"GetOption",			CmdGetOption, "Get option  \n\tParameter: [Superuser PW]", 0},
	{"SetOption",			CmdSetOption, "Set option  Run GetOption for list of option  \n\tParameter: <Option> [Superuser PW]", 1},

	{"GetConfig",			CmdGetConfig, "Get misc  configurations \n\tParameter: none", 0},
	{"SetDefaultConfig",	CmdSetDefaultConfig, "Reset some configurations to default.\n\t(retransmission time=200ms, connection timeout=5s, standby time=30m, packet delay=8n)", 0},
	{"SetDefaultConfigAuto",CmdSetDefaultConfigAuto, "Set all device in the network to default config.\n\t(retransmission time=200ms, connection timeout=5s, standby time=30m, packet delay=8n)", 0},

	{"SetRetransmit",		CmdSetRetransmit, "Set retransmission timeout in msec \n\tParameter: none", 0},
	// {"SetStandby",		CmdSetStandby, "Set standby time in minutes.\n\tParameter: <Standby time>"},

};

#define NR_CLI_CMD (sizeof(CommandList)/sizeof(CommandList[0]))

VOID
Usage (
	INT	index
	)
{
	INT i;
	
	NdasEmuDbgCall( 4, "\nUsage: ndascli [Command] [Target MAC] [Command Arg0] [Command Arg1] ...\n" );

	// NdasEmuDbgCall( 4, "If no command line argument is given, default command in source is used.\n" );

	NdasEmuDbgCall( 4, "\tUserID should be given in 2.5 style\n" );
	NdasEmuDbgCall( 4, "\tUserId will be converted into legacy for NDAS ~2.0\n\n" );

	if (index < NR_CLI_CMD) {

		NdasEmuDbgCall( 4, "%s\n\t%s\n", CommandList[index].Cmd, CommandList[index].Help );
		NdasEmuDbgCall( 4, "\n" );

		return;
	}

	for (i=0; i<NR_CLI_CMD; i++) {
	
		NdasEmuDbgCall( 4, "%s\n\t%s\n", CommandList[i].Cmd, CommandList[i].Help );
		NdasEmuDbgCall( 4, "\n" );
	}

	return;
}

VOID
PrintError (
	INT		ErrorCode,
	PCHAR	prefix
	)
{
	LPVOID lpMsgBuf;
	
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				   FORMAT_MESSAGE_FROM_SYSTEM | 
				   FORMAT_MESSAGE_IGNORE_INSERTS,
				   NULL,
				   ErrorCode,
				   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				   (LPTSTR) &lpMsgBuf,
				   0,
				   NULL );

	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string

	_ftprintf(stderr, _T("%s: %s"), prefix, (LPCTSTR)lpMsgBuf);
	
	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	
	LocalFree( lpMsgBuf );
}

VOID
PrintErrorCode (
	PCHAR	prefix,
	INT		ErrorCode
	)
{
	PrintError( ErrorCode, prefix );
}

inline 
INT
RecvIt (
	SOCKET	sock,
	PCHAR	buf, 
	INT		size
	)
{
	INT				iErrcode;
	INT				len = size, iReceived;
	WSAOVERLAPPED	overlapped;
	WSABUF			buffer[1];
	DWORD			dwFlag;
	DWORD			dwRecvDataLen;
	WSAEVENT		hEvent;
	BOOL			bResult;


	NdasEmuDbgCall( 5, "RecvIt %d ", size );

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

//	NdasEmuDbgCall( 4, "-- done \n");
  
	return dwRecvDataLen;
}


inline 
INT 
SendIt (
	SOCKET	sock,
	PCHAR	buf, 
	INT		size
	)
{
	INT res;
	INT len = size;

//	NdasEmuDbgCall( 4, "SendIt %d ", size);	
	
	while (len > 0) {
		
		if ((res = send(sock, buf, len, 0)) <= 0) {
			PrintError(WSAGetLastError(), _T("SendIt "));
			return res;
		}
		len -= res;
		buf += res;
	}

//	NdasEmuDbgCall( 4, "-- done \n");
	
	return size;
}

int _tmain(int argc, _TCHAR* argv[])
{
	WORD	wVersionRequested;
	WSADATA	wsaData;
	INT		err;
	
	PUCHAR	data = NULL;
	PUCHAR	data2 = NULL;
	INT		i;
	INT		retval = 0;
	CHAR*	TargetNode;
	CHAR*	Cmd;
	CHAR*	Arg[6] = {0};

	//int keyin;

	// Common initailization

	wVersionRequested = MAKEWORD( 2, 2 );
	
	err = WSAStartup( wVersionRequested, &wsaData );

	if(err != 0) {

		PrintError( WSAGetLastError(), _T("main: WSAStartup ") );
		retval = -1;
		goto cleanup;
	}

#ifdef BUILD_FOR_DIGILAND_1_1_INCORRECT_EEPROM_AUTO_FIX

	NdasEmuDbgCall( 4, "This software will change all NDAS 1.1 on the network to default config. Will you continue?(y/n)");

	keyin = getCHAR();

	if (keyin == 'y' || keyin == 'Y') {

		CmdSetDefaultConfigAuto(NULL,NULL);
	}

#else

	if (argc == 1 && !(DefaultCommand == NULL || DefaultCommand[0] == 0)) {

		CHAR* pos;
		const CHAR* delim= " \n";

		NdasEmuDbgCall( 4, "Using default command: %s\n", DefaultCommand );

		Cmd = strtok(DefaultCommand, delim);

		TargetNode = strtok(NULL, delim);
		pos = TargetNode;
		i=0;
		
		while(pos) {
		
			pos = Arg[i] = strtok(NULL, delim);
			i++;

			if (i>=6) {

				break;
			}
		}

	} else if (argc<2 || argc>9) {
		
		Usage( NR_CLI_CMD );
		goto cleanup;

	} else {

		Cmd = argv[1];
		TargetNode = argv[2];

		for (i=3;i<argc;i++) {

			Arg[i-3] = argv[i];
		}
	}

	for (i=0;i<NR_CLI_CMD;i++) {
	
		if (_stricmp(CommandList[i].Cmd, Cmd) == 0) {

			if (argc < CommandList[i].NeedArg + 3) {

				NdasEmuDbgCall( 4, "\nNot enough parameter\n" );
				Usage(i);
				goto cleanup;
			}

			retval = CommandList[i].Func( TargetNode, Arg );
			goto cleanup;
		}
	}

	NdasEmuDbgCall( 4, "Unknown command %s\n", Cmd );

	Usage( NR_CLI_CMD );
	
	retval = -1;

#endif	

cleanup:
	
	err = WSACleanup();
	
	if (err != 0) {
	
		PrintError( WSAGetLastError(), _T("main: WSACleanup ") );
	}

	return retval;
}

INT
SendRequest (
	SOCKET					connSock,
	PLANSCSI_PDU_POINTERS	pPdu
	)
{
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
	
		iDataSegLen = ntohl(pHeader->DataSegLen);

	} else {

		iDataSegLen = ntohs(pHeader->AHSLen);
	}

	NdasEmuDbgCall( 5, "iSessionPhase = %d, HeaderEncryptAlgo = %d, HeaderDigestAlgo = %d, iDataSegLen = %d, cur_password[0] = %x, cur_password[1] = %x\n", 
						iSessionPhase, HeaderEncryptAlgo, HeaderDigestAlgo, iDataSegLen, cur_password[0],  cur_password[1] ); 

	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE && HeaderDigestAlgo != 0) {

		CRC32( (UCHAR*)pHeader,
			   &(((UCHAR*)pHeader)[sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen]),
			   sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen );

		iDataSegLen += 4;
	}

	// Encrypt Header.

	if (iSessionPhase == FLAG_FULL_FEATURE_PHASE && HeaderEncryptAlgo != 0) {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
			Encrypt128( (UCHAR*)pHeader,
						sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
						(UCHAR *)&CHAP_C,
						cur_password );

			//fprintf(stderr, "SendRequest: Encrypt Header 1 !!!!!!!!!!!!!!!...\n");

		} else {

			Encrypt32( (UCHAR*)pHeader,
					   sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
					   (UCHAR *)CHAP_C,
					   (UCHAR*)cur_password );
		}
	}	
	
	// Encrypt Header.
	
	/*if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		//&& DataEncryptAlgo != 0	by limbear
		&& HeaderEncryptAlgo != 0
		&& iDataSegLen > 0) {
		
		Encrypt128(
			(UCHAR*)pPdu->pDataSeg,
			iDataSegLen,
			(UCHAR *)&CHAP_C,
			password0
			);
		//fprintf(stderr, "SendRequest: Encrypt Header 2 !!!!!!!!!!!!!!!...\n");
	}*/

	// Send Request

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
	
		iResult = SendIt( connSock,
						  (PCHAR)pHeader,
						  (sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + 15) & 0xfffffff0 );// Align 16 byte.

	} else {

		iResult = SendIt( connSock,
						  (PCHAR)pHeader,
						  sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen ); // No align
	}

	if (iResult == SOCKET_ERROR) {
	
		PrintError( WSAGetLastError(), _T("SendRequest: Send Request ") );
		return -1;
	}
	
	return 0;
}

INT
ReadReply (
	SOCKET					connSock,
	PCHAR					pBuffer,
	PLANSCSI_PDU_POINTERS	pPdu
	)
{
	INT		iResult, iTotalRecved = 0;
	PCHAR	pPtr = pBuffer;

	// Read Header
	
	iResult = RecvIt( connSock, pPtr, sizeof(LANSCSI_H2R_PDU_HEADER) );

	if (iResult == SOCKET_ERROR) {
	
		NdasEmuDbgCall( 4, "ReadRequest: Can't Recv Header...\n");
		return iResult;

	} else if (iResult == 0) {

		NdasEmuDbgCall( 4, "ReadRequest: Disconnected...\n");
		return iResult;

	} else {

		iTotalRecved += iResult;
	}

	pPdu->pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

	pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

	NdasEmuDbgCall( 5, "iSessionPhase = %d, HeaderEncryptAlgo = %d\n", iSessionPhase, HeaderEncryptAlgo ); 

	if (iSessionPhase == FLAG_FULL_FEATURE_PHASE && HeaderEncryptAlgo != 0) {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
			// Decrypt first 32 byte to get AHSLen

			Decrypt128( (UCHAR*)pPdu->pH2RHeader,
						//sizeof(LANSCSI_H2R_PDU_HEADER),
						32,
						(UCHAR *)&CHAP_C,
						cur_password );

			//NdasEmuDbgCall( 4, "ReadRequest: Decrypt Header 1 !!!!!!!!!!!!!!!...\n");

		} else {

			// Decrypt first 32 byte to get AHSLen

			Decrypt32( (UCHAR*)pPdu->pH2RHeader,
						//sizeof(LANSCSI_H2R_PDU_HEADER),
						32,
						(UCHAR *)&CHAP_C,
						(UCHAR*)&cur_password );
		}
	}

	NdasEmuDbgCall( 5, "AHSLen = %d\n", ntohs(pPdu->pH2RHeader->AHSLen) );

	// Read AHS

	if (ntohs(pPdu->pH2RHeader->AHSLen) > 0) {
	
		iResult = RecvIt( connSock, pPtr, ntohs(pPdu->pH2RHeader->AHSLen) );

		if (iResult == SOCKET_ERROR) {
		
			NdasEmuDbgCall( 4, "ReadRequest: Can't Recv AHS...\n");
			return iResult;

		} else if (iResult == 0) {

			NdasEmuDbgCall( 4, "ReadRequest: Disconnected...\n");

			return iResult;

		} else {

			iTotalRecved += iResult;
		}

		pPdu->pDataSeg = pPtr; // ????

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
	} 

	if (HeaderDigestAlgo != 0) {

		iResult = RecvIt( connSock, pPtr, 4 );
		
		if (iResult == SOCKET_ERROR) {

			NdasEmuDbgCall( 4, "ReadRequest: Can't Recv CRC...\n");
			return iResult;

		} else if(iResult == 0) {

			NdasEmuDbgCall( 4, "ReadRequest: Disconnected...\n");
			return iResult;

		} else {

			iTotalRecved += iResult;
		}

		pPdu->pDataSeg = pPtr;
		pPdu->pHeaderDig = pPtr;

		pPtr += 4;
	}

	// Read paddings

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 && iTotalRecved % 16 != 0) {

		iResult = RecvIt( connSock, pPtr, 16 - (iTotalRecved % 16) );

		if (iResult == SOCKET_ERROR) {

			NdasEmuDbgCall( 4, "ReadRequest: Can't Recv AHS...\n");
			return iResult;

		} else if (iResult == 0) {

			NdasEmuDbgCall( 4, "ReadRequest: Disconnected...\n");
			return iResult;

		} else {

			iTotalRecved += iResult;
		}

		pPdu->pDataSeg = pPtr;

		pPtr += iResult;
	}

	// Decrypt remaing headers.

	if (iSessionPhase == FLAG_FULL_FEATURE_PHASE && HeaderEncryptAlgo != 0) {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
			Decrypt128( ((UCHAR*)pPdu->pH2RHeader) + 32,
						iTotalRecved - 32,
						(UCHAR *)&CHAP_C,
						cur_password );

			//NdasEmuDbgCall( 4, "ReadRequest: Decrypt Header 1 !!!!!!!!!!!!!!!...\n");

		} else {

			// Decrypt first 32 byte to get AHSLen

			Decrypt32( ((UCHAR*)pPdu->pH2RHeader) + 32,
					   iTotalRecved - 32,
					   (UCHAR *)&CHAP_C,
					   (UCHAR*)&cur_password );
		}
	}

	// Check header CRC

	if (HeaderDigestAlgo != 0) {

		INT32 hcrc = ((INT32 *)pPdu->pHeaderDig)[0];

		CRC32( (UCHAR*)pBuffer,
			   &(((UCHAR*)pBuffer)[sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen)]),
			   sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen) );

		if (hcrc != ((INT32 *)pPdu->pHeaderDig)[0]) {

			NdasEmuDbgCall( 4, "Header Digest Error !!!!!!!!!!!!!!!...\n");
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
				(UCHAR*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(UCHAR *)&CHAP_C,
				cur_password
				);
			//NdasEmuDbgCall( 4, "ReadRequest: Decrypt Header 2 !!!!!!!!!!!!!!!...\n");
		} else {
			Decrypt32(
				(UCHAR*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(UCHAR *)&CHAP_C,
				(UCHAR*)&iPassword_v1
				);
		}
	}
#endif

	// Read Data segment. Not used by 1.1~2.5

	NdasEmuDbgCall( 5, "DataSegLen = %d\n", ntohl(pPdu->pH2RHeader->DataSegLen) );

	if (ntohl(pPdu->pH2RHeader->DataSegLen) > 0) {

		iResult = RecvIt( connSock, pPtr, ntohl(pPdu->pH2RHeader->DataSegLen) );

		if (iResult == SOCKET_ERROR) {
		
			NdasEmuDbgCall( 4, "ReadRequest: Can't Recv Data segment...\n");
			return iResult;

		} else if(iResult == 0) {

			NdasEmuDbgCall( 4, "ReadRequest: Disconnected...\n");
			return iResult;

		} else {

			iTotalRecved += iResult;
		}

		pPdu->pDataSeg = pPtr;
		
		pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);
		
		if (iSessionPhase == FLAG_FULL_FEATURE_PHASE && HeaderEncryptAlgo != 0) {

			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

				Decrypt128( (UCHAR*)pPdu->pDataSeg,
							ntohl(pPdu->pH2RHeader->DataSegLen),
							(UCHAR *)&CHAP_C,
							cur_password );

			} else {

				Decrypt32( (UCHAR*)pPdu->pDataSeg,
						   ntohl(pPdu->pH2RHeader->DataSegLen),
						   (UCHAR *)&CHAP_C,
						   (UCHAR*)&cur_password );
			}
		}
	}
	
	// Read Data Dig

	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
}

INT
Login (
	SOCKET	connsock,
	UCHAR	cLoginType,
	INT32	iUserID, // This user ID is 2.5 style. Requires converting for 1.0~2.0
	PUCHAR	iPassword,
	BOOL	Silent
	)
{
	INT8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestPdu;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
	PBIN_PARAM_SECURITY					pParamSecu;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	PAUTH_PARAMETER_CHAP				pParamChap;
	LANSCSI_PDU_POINTERS				pdu;
	INT									iSubSequence;
	INT									iResult;
	INT32								CHAP_I;
	BOOLEAN								SeagateTrial = FALSE;

	memcpy( cur_password, iPassword, PASSWORD_LENGTH );

	ActiveHwVersion = LANSCSIIDE_VERSION_1_0;

	// Encryption and digest is turned off during login process. And turned on or off after negotiation.
	
	HeaderEncryptAlgo	= 0;
	DataEncryptAlgo		= 0;
	HeaderDigestAlgo	= 0;
	DataDigestAlgo		= 0;

retry_with_new_ver:

	// Init

	iSubSequence	= 0;
	iSessionPhase	= FLAG_SECURITY_PHASE;

	// First Packet

	memset( PduBuffer, 0, MAX_REQUEST_SIZE );
	
	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opcode	= LOGIN_REQUEST;
	pLoginRequestPdu->HPID		= htonl(HPID);

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
		pLoginRequestPdu->AHSLen	 = 0;

	} else {

		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen	 = htons(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
	}

	pLoginRequestPdu->CSubPacketSeq		= htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag	= htonl(iTag);
	pLoginRequestPdu->ParameterType		= 1;
	pLoginRequestPdu->ParameterVer		= 0;

	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion;
	pLoginRequestPdu->VerMin = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType = cLoginType;

	pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
	
	// Send Request

	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (CHAR *)pParamSecu;

	if (!Silent) {

		_ftprintf( stderr, _T("[NdasCli]login: First.\n") );	
	}

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError(WSAGetLastError(), _T("Login: Send First Request "));
		return -1;
	}

	// Read Request

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	
	if (iResult == SOCKET_ERROR) {

		_ftprintf( stderr, _T("[NdasCli]login: First Can't Read Reply.\n") );
		return -1;
	}
	
	// Check Request Header

	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;
	
	if ((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)				|| 
		(pLoginReplyHeader->T != 0)									|| 
		(pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)				|| 
		(pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)				|| 
		(pLoginReplyHeader->VerActive > HW_VER )					|| 
		(pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)	|| 
		(pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		_ftprintf( stderr, _T("[NdasCli]login: BAD First Reply Header.\n") );
		return -1;
	}

	if (pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
	
		if (ActiveHwVersion != pLoginReplyHeader->VerActive)  {

			ActiveHwVersion = pLoginReplyHeader->VerActive;

			if (!Silent) {

				_ftprintf( stderr, _T("[NdasCli]login: Retry with version %d.\n"), ActiveHwVersion );
			}

			goto retry_with_new_ver;

		} else {

			if (!Silent) {

				_ftprintf( stderr, _T("[NdasCli]login: First Failed.\n") );
			}
		}
		
		return -1;

	} else {

		// Ver 1.0 seems to response with SUCCESS when version mismatched.
		
		if (ActiveHwVersion != pLoginReplyHeader->VerActive)  {
		
			ActiveHwVersion = pLoginReplyHeader->VerActive;

			if (!Silent) {

				_ftprintf(stderr, _T("[NdasCli]login: Retrying with version %d.\n"), ActiveHwVersion);
			}

			goto retry_with_new_ver;
		}
	}

	// Store Data

	RPID = ntohs(pLoginReplyHeader->RPID);
	
	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;

	ActiveHwVersion	= pLoginReplyHeader->VerActive;
	
	ActiveHwRevision = ntohs(pLoginReplyHeader->Revision);

	if (!Silent) {

		printf( "[NdasCli]login: Version %d Revision %x Auth %d\n", 
				ActiveHwVersion, ActiveHwRevision, ntohs(pParamSecu->AuthMethod) );
	}
	
	fprintf( stderr, "iUserID = %08X\n", iUserID );

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

		if (!Silent) {

			printf( "Not NDAS 2.5. Setting User ID to %x\n", ActiveUserId );
		}
#endif

	} else {

		ActiveUserId = iUserID;
	}

	fprintf( stderr, "ActiveUserId = %08X\n", ActiveUserId );

	// Second Packet.

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	iSubSequence = 1;

	pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	
	pLoginRequestPdu->Opcode			= LOGIN_REQUEST;
	pLoginRequestPdu->HPID				= htonl(HPID);
	pLoginRequestPdu->RPID				= htons(RPID);
	pLoginRequestPdu->DataSegLen		= 0;
	pLoginRequestPdu->AHSLen			= htons(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);

	pLoginRequestPdu->CSubPacketSeq		=  htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag	= htonl(iTag);
	pLoginRequestPdu->ParameterType		= PARAMETER_TYPE_BINARY;
	pLoginRequestPdu->ParameterVer		= 0;

	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion;
	pLoginRequestPdu->VerMin = 0;
	
	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType	= BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType	= cLoginType;
	pParamSecu->AuthMethod	= htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_AES128);	

	} else {

		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);	
	}

	// Send Request.
	
	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg = (CHAR *)pParamSecu;
	
	if (!Silent) {

		fprintf(stderr, "[NdasCli]login: Second.\n");
	}

	if (SendRequest(connsock, &pdu) != 0) {
	
		PrintError(WSAGetLastError(), _T("[NdasCli]Login: Send Second Request "));
		return -1;
	}

	// Read Request.

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);

	if (iResult == SOCKET_ERROR) {
	
		fprintf(stderr, "[NdasCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.

	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)				|| 
		(pLoginReplyHeader->T != 0)									|| 
		(pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)				|| 
		(pLoginReplyHeader->NSG != FLAG_SECURITY_PHASE)				|| 
		(pLoginReplyHeader->VerActive > HW_VER)						|| 
		(pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)	|| 
		(pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Second Reply Header.\n");
		return -1;
	}
	
	if (pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
	
		fprintf(stderr, "[NdasCli]login: Second Failed.\n");
		return -1;
	}
	
	// Check Data segment.

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		if ((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) ||	// Minus AuthParamter[1]
			(pdu.pDataSeg == NULL)) {

			fprintf(stderr, "[NdasCli]login: BAD Second Reply Data.\n");
			return -1;
		}

	} else {
		
		if ((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) || (pdu.pDataSeg == NULL)) {	

			fprintf(stderr, "[NdasCli]login: BAD Second Reply Data.\n");
			return -1;
		}	
	}

	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	
	if (pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY	|| 
		pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)	|| 
		pParamSecu->LoginType != cLoginType) {	
		
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

	printf( "[NdasCli]login: Hash %d, Challenge %X %X %X %X\n", 
			ntohl(pParamChap->CHAP_A), 
			CHAP_C[0], CHAP_C[1], CHAP_C[2], CHAP_C[3] );

	//retry_with_seagate_password:
	 
	// Third Packet.
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu			= (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode	= LOGIN_REQUEST;
	pLoginRequestPdu->T			= 1;
	pLoginRequestPdu->CSG		= FLAG_SECURITY_PHASE;
	pLoginRequestPdu->NSG		= FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->HPID		= htonl(HPID);
	pLoginRequestPdu->RPID		= htons(RPID);

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
		pLoginRequestPdu->AHSLen = 0;

	} else {

		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
	}

	iSubSequence = 2;

	pLoginRequestPdu->CSubPacketSeq		= htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag	= htonl(iTag);
	pLoginRequestPdu->ParameterType		= 1;
	pLoginRequestPdu->ParameterVer		= 0;

	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion; // temp
	pLoginRequestPdu->VerMin = 0;

	pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamSecu->ParamType	= BIN_PARAM_TYPE_SECURITY;
	pParamSecu->LoginType	= cLoginType;
	pParamSecu->AuthMethod	= htons(AUTH_METHOD_CHAP);
	
	pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->AuthParamter;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_AES128);	
	
	} else {

		pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);
	}

	pParamChap->CHAP_I = htonl(CHAP_I);

	if (ActiveHwVersion <= LANSCSIIDE_VERSION_2_0 && LOGIN_TYPE_NORMAL != cLoginType) {
	
		// Change user ID to 0 for discovery login.
		pParamChap->CHAP_N = htonl(0);

	} else {

		pParamChap->CHAP_N = htonl(ActiveUserId);
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		AES_cipher((UCHAR*)CHAP_C, (UCHAR*)pParamChap->V2.CHAP_CR, (UCHAR*)iPassword);

		printf( "CHAP_C: %08x %08x %08x %08x\n", CHAP_C[0], CHAP_C[1], CHAP_C[2], CHAP_C[3]);
		
		printf( "CHAP_R: %08x %08x %08x %08x\n", 
				pParamChap->V2.CHAP_CR[0], pParamChap->V2.CHAP_CR[1], 
				pParamChap->V2.CHAP_CR[2], pParamChap->V2.CHAP_CR[3] );

		printf("Password: ");
		PrintHex(iPassword, 16);
		printf("\n");

	} else {

		if (ActiveUserId != NDAS_SUPERVISOR) {
		
			if (memcmp(iPassword, def_password0, sizeof(def_password0)) ==0) {

				if (SeagateTrial) {

					printf("Using default seagate password\n");
					Hash32To128((UCHAR*)&CHAP_C, (UCHAR*)pParamChap->V1.CHAP_R, (PUCHAR)&iPassword_v1_seagate);
					memcpy(cur_password, (PUCHAR)&iPassword_v1_seagate, PASSWORD_LENGTH_V1);

				} else {

					// printf("Using default password\n");

					Hash32To128((UCHAR*)&CHAP_C, (UCHAR*)pParamChap->V1.CHAP_R, (PUCHAR)&iPassword_v1);
					memcpy(cur_password, (PUCHAR)&iPassword_v1, PASSWORD_LENGTH_V1);
				}

			} else {

				printf("Password=%s\n", iPassword);
				Hash32To128((UCHAR*)&CHAP_C, (UCHAR*)pParamChap->V1.CHAP_R, iPassword);
				memcpy(cur_password, iPassword, PASSWORD_LENGTH_V1);
			}

		} else {

			printf( "iPassword[0] = %x, def_supervisor_password[0] = %x\n", iPassword[0], def_supervisor_password[0] );
			
			if (memcmp(iPassword, def_supervisor_password, sizeof(def_supervisor_password)) == 0) {
			
				printf("Using default password\n");
				Hash32To128((UCHAR*)&CHAP_C, (UCHAR*)pParamChap->V1.CHAP_R, (PUCHAR)&iSuperPassword_v1);
				memcpy(cur_password, (PUCHAR)&iSuperPassword_v1, PASSWORD_LENGTH_V1);

			} else {

				Hash32To128((UCHAR*)&CHAP_C, (UCHAR*)pParamChap->V1.CHAP_R, iPassword);
				printf( "change password, iPassword[0] = %x, iPassword[1] = %x\n", iPassword[0], iPassword[1] );
				printf( "change password, iPassword[2] = %x, iPassword[3] = %x\n", iPassword[2], iPassword[3] );
				printf( "change password, iPassword[4] = %x, iPassword[5] = %x\n", iPassword[4], iPassword[5] );
				printf( "change password, iPassword[6] = %x, iPassword[7] = %x\n", iPassword[6], iPassword[7] );
				memcpy(cur_password, iPassword, PASSWORD_LENGTH_V1);
			}
		}
	}

	// Send Request

	pdu.pH2RHeader	= (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg	= (CHAR *)pParamSecu;

	if (SendRequest(connsock, &pdu) != 0) {
	
		PrintError(WSAGetLastError(), _T("Login: Send Third Request "));
		return -1;
	}
	
	// Read Request

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);
	
	if (iResult == SOCKET_ERROR) {
	
		fprintf(stderr, "[NdasCli]login: Second Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header

	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)				|| 
		(pLoginReplyHeader->T == 0)									|| 
		(pLoginReplyHeader->CSG != FLAG_SECURITY_PHASE)				|| 
		(pLoginReplyHeader->NSG != FLAG_LOGIN_OPERATION_PHASE)		|| 
		(pLoginReplyHeader->VerActive > HW_VER)						|| 
		(pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)	|| 
		(pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Third Reply Header.\n");
		return -1;
	}
	
	if (pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

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
					// goto retry_with_seagate_password; 
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

		if ((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) || (pdu.pDataSeg == NULL)) {

			fprintf(stderr, "[NdasCli]login: BAD Third Reply Data.\n");
			return -1;
		}

	} else {
		
		if ((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) || (pdu.pDataSeg == NULL)) {

			fprintf(stderr, "[NdasCli]login: BAD Third Reply Data.\n");
			return -1;
		}	
	}

	pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
	
	if (pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY || pParamSecu->LoginType != cLoginType) {

		fprintf(stderr, "[NdasCli]login: BAD Third Reply Parameters.\n");
		return -1;
	}
	
	iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;

	// Fourth Packet.

	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pLoginRequestPdu			= (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
	pLoginRequestPdu->Opcode	= LOGIN_REQUEST;
	pLoginRequestPdu->T			= 1;
	pLoginRequestPdu->CSG		= FLAG_LOGIN_OPERATION_PHASE;
	pLoginRequestPdu->NSG		= FLAG_FULL_FEATURE_PHASE;
	pLoginRequestPdu->HPID		= htonl(HPID);
	pLoginRequestPdu->RPID		= htons(RPID);

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
		pLoginRequestPdu->AHSLen = 0;

	} else {

		pLoginRequestPdu->DataSegLen = 0;
		pLoginRequestPdu->AHSLen = htons(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
	}

	iSubSequence = 3;

	pLoginRequestPdu->CSubPacketSeq		= htons((UINT16)iSubSequence);
	pLoginRequestPdu->PathCommandTag	= htonl(iTag);
	pLoginRequestPdu->ParameterType		= 1;
	pLoginRequestPdu->ParameterVer		= 0;

	pLoginRequestPdu->VerMax = (BYTE)ActiveHwVersion; // temp
	pLoginRequestPdu->VerMin = 0;

	pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
	
	pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
	
	// Send Request
	
	pdu.pH2RHeader	= (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
	pdu.pDataSeg	= (CHAR *)pParamNego;

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError(WSAGetLastError(), _T("Login: Send Fourth Request "));
		return -1;
	}
	
	// Read Request.

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);

	if (iResult == SOCKET_ERROR) {

		fprintf(stderr, "[NdasCli]login: Fourth Can't Read Reply.\n");
		return -1;
	}
	
	// Check Request Header.

	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)											|| 
		(pLoginReplyHeader->T == 0)																|| 
		((pLoginReplyHeader->Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))	|| 
		((pLoginReplyHeader->Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)			|| 
		(pLoginReplyHeader->VerActive > HW_VER)													|| 
		(pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)								|| 
		(pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Header.\n");
		return -1;
	}
	
	if (pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		fprintf(stderr, "[NdasCli]login: Fourth Failed.\n");
		return -1;
	}
	
	// Check Data segment.

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		if ((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) || (pdu.pDataSeg == NULL)) {

			fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Data.\n");
			return -1;
		}

	} else {

		if ((ntohs(pLoginReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) || (pdu.pDataSeg == NULL)) {
			
			fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Data.\n");
			return -1;
		}	
	}

	pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
	
	if (pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {

		fprintf(stderr, "[NdasCli]login: BAD Fourth Reply Parameters.\n");
		return -1;
	}

	if (!Silent) {

		printf( "[NdasCli]login: Hw Type %d, Hw Version %d, NRSlots %d, MaxBlocks %d, MaxTarget %d MLUN %d\n", 
				 pParamNego->HWType, pParamNego->HWVersion, ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
				 ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID) );

		printf( "[NdasCli]login: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
				 pParamNego->HeaderEncryptAlgo,
				 pParamNego->HeaderDigestAlgo,
				 pParamNego->DataEncryptAlgo,
				 pParamNego->DataDigestAlgo );
	}

	requestBlocks		= (short)(ntohl(pParamNego->MaxBlocks));
	MaxPendingTasks		= (short)ntohl(pParamNego->NRSlot);
	HeaderEncryptAlgo	= pParamNego->HeaderEncryptAlgo;
	DataEncryptAlgo		= pParamNego->DataEncryptAlgo;
	HeaderDigestAlgo	= pParamNego->HeaderDigestAlgo;
	DataDigestAlgo		= pParamNego->DataDigestAlgo;
	iSessionPhase		= FLAG_FULL_FEATURE_PHASE;

	if (ActiveUserId == NDAS_SUPERVISOR) {

		// actually this should be user password...
		
		memcpy( cur_password, (VOID*)&iPassword_v1, PASSWORD_LENGTH );
	}

	return 0;
}

INT
Logout (
	SOCKET	connsock
	)
{
	INT8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	INT									iResult;
	
	memset( PduBuffer, 0, MAX_REQUEST_SIZE );
	
	pRequestHeader				= (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode		= LOGOUT_REQUEST;
	pRequestHeader->F			= 1;
	pRequestHeader->HPID		= htonl(HPID);
	pRequestHeader->RPID		= htons(RPID);
	pRequestHeader->CPSlot		= 0;
	pRequestHeader->DataSegLen	= 0;
	pRequestHeader->AHSLen		= 0;

	pRequestHeader->CSubPacketSeq	= 0;
	pRequestHeader->PathCommandTag	= htonl(++iTag);

	// Send Request

	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError( WSAGetLastError(), _T("[NdasCli]Logout: Send Request ") );
		return -1;
	}
	
	// Read Request.

	iResult = ReadReply( connsock, (PCHAR)PduBuffer, &pdu );

	if (iResult == SOCKET_ERROR) {
	
		NdasEmuDbgCall( 4, "[NdasCli]Logout: Can't Read Reply.\n" );
		return -1;
	}
	
	// Check Request Header

	pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pReplyHeader->Opcode != LOGOUT_RESPONSE) || (pReplyHeader->F == 0)) {
		
		NdasEmuDbgCall( 4, "[NdasCli]Logout: BAD Reply Header.\n" );
		return -1;
	}
	
	if (pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		NdasEmuDbgCall( 4, "[NdasCli]Logout: Failed.\n" );
		return -1;
	}
	
	iSessionPhase = FLAG_SECURITY_PHASE;

	return 0;
}

INT
TextTargetList (
	SOCKET	connsock
	)
{
	INT8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_LIST				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	INT									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader			= (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode	= TEXT_REQUEST;
	pRequestHeader->F		= 1;
	pRequestHeader->HPID	= htonl(HPID);
	pRequestHeader->RPID	= htons(RPID);
	pRequestHeader->CPSlot	= 0;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		pRequestHeader->DataSegLen	= htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
		pRequestHeader->AHSLen		= 0;	

	} else {

		pRequestHeader->DataSegLen	= 0;
		pRequestHeader->AHSLen		= htons(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
	}

	pRequestHeader->CSubPacketSeq	= 0;
	pRequestHeader->PathCommandTag	= htonl(++iTag);
	pRequestHeader->ParameterType	= PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer	= PARAMETER_CURRENT_VERSION;
	
	// Make Parameter

	pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	
	pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
	
	// Send Request
	
	pdu.pH2RHeader	= (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg	= (CHAR *)pParam;

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError(WSAGetLastError(), _T("TextTargetList: Send First Request "));
		return -1;
	}
	
	// Read Request.
	// fprintf(stderr, "[NdasCli]TextTargetList: step 3.\n");

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);

	fprintf(stderr, "[NdasCli]TextTargetList: step 2.\n");
	
	if (iResult == SOCKET_ERROR) {

		fprintf(stderr, "[NdasCli]TextTargetList: Can't Read Reply.\n");
		return -1;
	}

	fprintf(stderr, "[NdasCli]TextTargetList: step 1.\n");
	
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	// Check Request Header.

	if ((pReplyHeader->Opcode != TEXT_RESPONSE)					|| 
		(pReplyHeader->F == 0)									|| 
		(pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)	|| 
		(pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]TextTargetList: BAD Reply Header.\n");
		return -1;
	}
	
	if (pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		fprintf(stderr, "[NdasCli]TextTargetList: Failed.\n");
		return -1;
	}
	
	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		if (ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
		
			fprintf(stderr, "[NdasCli]TextTargetList: No Data Segment.\n");
			return -1;		
		}

	} else {
		
		if (ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {
		
			fprintf(stderr, "[NdasCli]TextTargetList: No Data Segment.\n");
			return -1;		
		}
	}

	pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;

	if (pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
	
		fprintf(stderr, "TEXT: Bad Parameter Type.: %d\n",pParam->ParamType);
		return -1;			
	}

	printf("[NdasCli]TextTargetList: NR Targets : %d\n", pParam->NRTarget);
	NRTarget = pParam->NRTarget;
	
	for (int i = 0; i < pParam->NRTarget; i++) {

		PBIN_PARAM_TARGET_LIST_ELEMENT	pTarget;
		INT								iTargetId;
		
		pTarget = &pParam->PerTarget[i];
		iTargetId = ntohl(pTarget->TargetID);
		
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
			printf( "[NdasCli]TextTargetList: Target ID: %d, NR_EW: %d, NR_SW: %d, NR_RO: %d, Data: %I64d \n",  
					ntohl(pTarget->TargetID), 
					pTarget->V2.NREWHost,
					pTarget->V2.NRSWHost,
					pTarget->V2.NRROHost,
					pTarget->TargetData );

			PerTarget[iTargetId].V2.NREWHost = pTarget->V2.NREWHost;
			PerTarget[iTargetId].V2.NRSWHost = pTarget->V2.NRSWHost;
			PerTarget[iTargetId].V2.NRROHost = pTarget->V2.NRROHost;

		} else {

			printf( "[NdasCli]TextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: %I64d \n",  
					ntohl(pTarget->TargetID), 
					pTarget->V1.NRRWHost,
					pTarget->V1.NRROHost,
					pTarget->TargetData );

			PerTarget[iTargetId].V1.NRRWHost = pTarget->V1.NRRWHost;
			PerTarget[iTargetId].V1.NRROHost = pTarget->V1.NRROHost;
		}

		PerTarget[iTargetId].bPresent = TRUE;
		PerTarget[iTargetId].TargetData = pTarget->TargetData;		
	}
	
	return 0;
}

INT
TextTargetData (
	SOCKET	connsock,
	UCHAR	cGetorSet,
	UINT	TargetID,
	PUINT64	TargetData
	)
{
	INT8								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	PBIN_PARAM_TARGET_DATA				pParam;
	LANSCSI_PDU_POINTERS				pdu;
	INT									iResult;
	
	memset(PduBuffer, 0, MAX_REQUEST_SIZE);
	
	pRequestHeader			= (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode	= TEXT_REQUEST;
	pRequestHeader->F		= 1;
	pRequestHeader->HPID	= htonl(HPID);
	pRequestHeader->RPID	= htons(RPID);
	pRequestHeader->CPSlot	= 0;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
	
		pRequestHeader->DataSegLen	= htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
		pRequestHeader->AHSLen		= 0;

	} else {

		pRequestHeader->DataSegLen	= 0;
		pRequestHeader->AHSLen		= htons(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
	}

	pRequestHeader->CSubPacketSeq	= 0;
	pRequestHeader->PathCommandTag	= htonl(++iTag);
	pRequestHeader->ParameterType	= PARAMETER_TYPE_BINARY;
	pRequestHeader->ParameterVer	= PARAMETER_CURRENT_VERSION;
	
	// Make Parameter

	pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
	
	pParam->ParamType	= BIN_PARAM_TYPE_TARGET_DATA;
	pParam->GetOrSet	= cGetorSet;
	pParam->TargetData	= *TargetData;

	pParam->TargetID = htonl(TargetID);
	
	printf( "TargetID %d, %I64d\n", ntohl(pParam->TargetID), pParam->TargetData );

	// Send Request

	pdu.pH2RHeader	= (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	pdu.pDataSeg	= (CHAR *)pParam;

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError(WSAGetLastError(), _T("TextTargetData: Send First Request "));
		return -1;
	}
	
	// Read Request.

	iResult = ReadReply(connsock, (PCHAR)PduBuffer, &pdu);

	if (iResult == SOCKET_ERROR) {

		fprintf( stderr, "[NdasCli]TextTargetData: Can't Read Reply.\n" );
		return -1;
	}
	
	// Check Request Header

	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pReplyHeader->Opcode != TEXT_RESPONSE)					|| 
		(pReplyHeader->F == 0)									|| 
		(pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)	|| 
		(pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		
		fprintf(stderr, "[NdasCli]TextTargetData: BAD Reply Header.\n");
		return -1;
	}
	
	if (pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		fprintf(stderr, "[NdasCli]TextTargetData: Failed.\n");
		return -1;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

		if (pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
		
			fprintf(stderr, "[NdasCli]TextTargetData: No Data Segment.\n");
			return -1;			
		}

	} else {

		if (ntohs(pReplyHeader->AHSLen) < BIN_PARAM_SIZE_REPLY) {

			fprintf(stderr, "[NdasCli]TextTargetData: No Data Segment.\n");
			return -1;		
		}
	}

	pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;

	if (pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {

		fprintf(stderr, "TextTargetData: Bad Parameter Type. %d\n", pParam->ParamType);
	//	return -1;			
	}

	*TargetData = pParam->TargetData;

	printf( "[NdasCli]TextTargetList: TargetID : %d, GetorSet %d, Target Data %d\n", 
			ntohl(pParam->TargetID), pParam->GetOrSet, *TargetData );
	
	return 0;
}


// Discovery

INT
Discovery (
	SOCKET	connsock
	)
{
	INT		iResult;
	UCHAR	Password[PASSWORD_LENGTH];
	UINT32	UserID;

	memcpy( Password, def_password0, PASSWORD_LENGTH );

	UserID = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);

	//////////////////////////////////////////////////////////
	//
	// Login Phase...
	//

	fprintf(stderr, "[NdasCli]Discovery: Before Login \n");

	if ((iResult = Login(connsock, LOGIN_TYPE_DISCOVERY, UserID, Password, FALSE)) != 0) {

		fprintf(stderr, "[NdasCli]Discovery: Login Failed...\n");
		return iResult;
	}
	
	fprintf(stderr, "[NdasCli]Discovery: After Login \n");

	if ((iResult = TextTargetList(connsock)) != 0) {
	
		fprintf(stderr, "[NdasCli]Discovery: Text Failed\n");
		return iResult;
	}

	fprintf(stderr, "[NdasCli]Discovery: After Text \n");
	
	///////////////////////////////////////////////////////////////
	//
	// Logout Packet.
	//
	
	if ((iResult = Logout(connsock)) != 0) {

		fprintf(stderr, "[NdasCli]Discovery: Logout Failed...\n");
		return iResult;
	}
	
	return 0;
}

INT ConnectToNdas (
	SOCKET	*connsock, 
	PCHAR	target, 
	UINT32	UserId, 
	PUCHAR Password
	)
{
	INT				retval = 0;
	LPX_ADDRESS		address;
	BOOL			Ret;
	UCHAR			LocalPassword[PASSWORD_LENGTH] = {0};

	Ret = lpx_addr(target, &address);

	if (!Ret) {
	
		printf("Invalid address\n");
		return -1;
	}

	if (MakeConnection(&address, connsock) == FALSE) {

		fprintf(stderr, "[NdasCli]main: Can't Make Connection to LanDisk!!!\n");
		return -1;
	}

	if (Password == NULL || Password[0] == 0) {

		if (IS_SUPERVISOR(UserId)) {

			memcpy(LocalPassword, def_supervisor_password, PASSWORD_LENGTH);

		} else {

			memcpy(LocalPassword, def_password0, PASSWORD_LENGTH);
		}

	} else {

		// Assume password is plain text

		strncpy((PCHAR)LocalPassword, (PCHAR)Password, PASSWORD_LENGTH);
	}

	if ((retval = Login(*connsock, LOGIN_TYPE_NORMAL, UserId, LocalPassword, FALSE)) != 0) {

		fprintf(stderr, "[NdasCli]main: Login Failed...\n");
		return -1;
	}

	return 0;
}

INT 
DisconnectFromNdas (
	SOCKET connsock, 
	UINT32 UserId
	)
{
	if (IS_SUPERVISOR(UserId)) {

		UINT param = 0;
		
		VendorCommand( connsock, VENDOR_OP_RESET, NULL, &param, &param, &param, NULL, 0, NULL );

		// NdasEmuDbgCall( 4, "Press reset button and press enter");
		// getchar();
		
		NdasEmuDbgCall( 4, "Resetting\n" );

	} else {

		if (Logout(connsock) != 0) {
		
			NdasEmuDbgCall( 4, "Logout failed\n" );
			return -1;
		}

		NdasEmuDbgCall( 4, "Logout..\n" );
	}
	
	closesocket(connsock);

	return 0;
}

// If AhsLen == 0, AHS is used for getting data

INT
VendorCommand (
	SOCKET	connsock,
	UCHAR	cOperation,
	PUCHAR	Param8,
	PUINT32	Parameter0,
	PUINT32	Parameter1,
	PUINT32	Parameter2,
	PCHAR	AhsData,
	INT		AhsLen,
	PCHAR	OptData
	)
{
	CHAR								PduBuffer[MAX_REQUEST_SIZE];
	PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;
	LANSCSI_PDU_POINTERS				pdu;
	INT									iResult;
	UINT32								LocalParam0, LocalParam1, LocalParam2;

	if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {

		switch(cOperation) {
		
		case VENDOR_OP_GET_WRITE_LOCK:
		case VENDOR_OP_FREE_WRITE_LOCK:
		case VENDOR_OP_GET_HOST_LIST:

			// add more..
	
			NdasEmuDbgCall( 4, "\nUnsupported vendor operation for this HW: %x\n", cOperation );
			return 0;
		}
	}

	memset( PduBuffer, 0, MAX_REQUEST_SIZE );

	LocalParam0 = LocalParam1 = LocalParam2 = 0;

	pRequestHeader			= (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;
	pRequestHeader->Opcode	= VENDOR_SPECIFIC_COMMAND;
	pRequestHeader->F		= 1;
	pRequestHeader->HPID	= htonl(HPID);
	pRequestHeader->RPID	= htons(RPID);
	pRequestHeader->CPSlot	= 0;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
	
		pRequestHeader->DataSegLen	= htonl((long)AhsLen);
		pRequestHeader->AHSLen		= 0;

	} else {

		pRequestHeader->DataSegLen	= 0;
		pRequestHeader->AHSLen		= htons((short)AhsLen);
	}

	pRequestHeader->CSubPacketSeq	= 0;
	pRequestHeader->PathCommandTag	= htonl(++iTag);
	pRequestHeader->VendorID		= ntohs(NKC_VENDOR_ID);
	pRequestHeader->VendorOpVersion = VENDOR_OP_CURRENT_VERSION;
	pRequestHeader->VendorOp		= cOperation;

	if (Param8) {

		RtlCopyMemory( pRequestHeader->Parameter8, Param8, 12 );
	
	} else {

		if (Parameter0) {

			LocalParam0 = pRequestHeader->VendorParameter0 = *Parameter0;
		}

		if (Parameter1) {

			LocalParam1 = pRequestHeader->VendorParameter1 = *Parameter1;
		}

		if (Parameter2) {

			LocalParam2 = pRequestHeader->VendorParameter2 = *Parameter2;
		}
	}

	NdasEmuDbgCall( 5, "VendorCommand: Operation %d\n", cOperation );

	// Send Request

	pdu.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
	
	if (AhsData && AhsLen) {
	
		pdu.pAHS = &PduBuffer[sizeof(LANSCSI_VENDOR_REQUEST_PDU_HEADER)];
		memcpy( pdu.pAHS, AhsData, AhsLen );
	}

	if (SendRequest(connsock, &pdu) != 0) {

		PrintError( WSAGetLastError(), _T("VendorCommand: Send First Request ") );
		return -1;
	}

	if (cOperation == VENDOR_OP_RESET) {

		return 0;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 &&
		(cOperation == VENDOR_OP_GET_EEP || cOperation == VENDOR_OP_U_GET_EEP)) {

		INT len = htonl(pRequestHeader->VendorParameter2);

		if (DataDigestAlgo != 0) {

			len += 16;
		}

		iResult = RecvIt( connsock, OptData, len );

		if (iResult <= 0) {
	
			PrintError( WSAGetLastError(), _T("VendorCommand: Receive Data for READ ") );

			printf("RR\n");
			
			return -1;
		}

		if (DataEncryptAlgo != 0) {
	
			Decrypt128( (PUCHAR)OptData, len, (PUCHAR)&CHAP_C, cur_password );
		}

		if (DataDigestAlgo != 0) {

			UINT crc;
		
			crc = ((UINT *)OptData)[htonl(pRequestHeader->VendorParameter2)];

			CRC32( (PUCHAR)OptData,
				   &(((PUCHAR)OptData)[htonl(pRequestHeader->VendorParameter2)]),
				   htonl(pRequestHeader->VendorParameter2) );

			if (crc != ((UINT *)OptData)[htonl(pRequestHeader->VendorParameter2)]) {

				NdasEmuDbgCall( 4, "Read data Digest Error !!!!!!!!!!!!!!!...\n" );
			}
		}
	}
	
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5 && 
		(cOperation == VENDOR_OP_SET_EEP || cOperation == VENDOR_OP_U_SET_EEP)) {

		UINT DataLength = htonl(pRequestHeader->VendorParameter2);

		if (DataDigestAlgo != 0) {

			CRC32( (UCHAR *)OptData, &(((UCHAR *)OptData)[DataLength]), DataLength );

			DataLength += 16; //CRC + Padding for 16 byte align.
		}

		if (DataEncryptAlgo != 0) {
		
			Encrypt128( (PUCHAR)OptData, DataLength, (PUCHAR)&CHAP_C, cur_password );
		}

		iResult = SendIt( connsock, OptData, DataLength );

		if (iResult == SOCKET_ERROR) {
	
			NdasEmuDbgCall( 4, "VendorCommand: Failed to send data for WRITE\n" );
			return -1;
		}
	}


	// Read Request

	iResult = ReadReply( connsock, (PCHAR)PduBuffer, &pdu );

	if (iResult == SOCKET_ERROR) {
	
		NdasEmuDbgCall( 4, "[NdasCli]VendorCommand: Can't Read Reply.\n" );
		return -1;
	}
	
	// Check Request Header.

	pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.pR2HHeader;

	if ((pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE) || (pReplyHeader->F == 0)) {

		NdasEmuDbgCall( 4, "[NdasCli]VendorCommand: BAD Reply Header. Opcode=0x%x 0x%x\n", pReplyHeader->Opcode, pReplyHeader->F );
		return -1;
	}

	if (ntohs(pReplyHeader->AHSLen) != 0 && AhsData != NULL) {

		memcpy( AhsData, &PduBuffer[sizeof(LANSCSI_VENDOR_REPLY_PDU_HEADER)], ntohs(pReplyHeader->AHSLen) );
	}

	if (Param8) {

		RtlCopyMemory( Param8, pReplyHeader->Parameter8, 12 );

	} else {

		if (Parameter0) {

			*Parameter0 = pReplyHeader->VendorParameter0;
		}

		if (Parameter1) {

			*Parameter1 = pReplyHeader->VendorParameter1;
		}

		if (Parameter2) {

			*Parameter2 = pReplyHeader->VendorParameter2;
		}
	}

	if (pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {

		// NdasEmuDbgCall( 4, "[NdasCli]VendorCommand: Failed.\n");
		// In some case, reply packet has valid values when command is failed

		return pReplyHeader->Response;
	}

	// Change operation mode for some operation

	if (cOperation == VENDOR_OP_SET_D_OPT) {
	
		UINT32 Option = ntohl(LocalParam2);		

		DataEncryptAlgo		= (Option & 0x1) ? ENCRYPT_ALGO_AES128:ENCRYPT_ALGO_NONE;
		HeaderEncryptAlgo	= (Option & 0x2) ? ENCRYPT_ALGO_AES128:ENCRYPT_ALGO_NONE;
		DataDigestAlgo		= (Option & 0x4) ? DIGEST_ALGO_CRC32:DIGEST_ALGO_NONE;
		HeaderDigestAlgo	= (Option & 0x8) ? DIGEST_ALGO_CRC32:DIGEST_ALGO_NONE;
	}

	return 0;
}
