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


#include "stdafx.h"

LONG DbgLevelCliCmd = DBG_LEVEL_CLI_CMD;

#define NdasEmuDbgCall(l,x,...) do {							\
    if (l <= DbgLevelCliCmd) {									\
		fprintf(stderr,"|%d|%s|%d|",l,__FUNCTION__, __LINE__);	\
		fprintf(stderr,x,__VA_ARGS__);							\
    } 															\
} while(0)

extern INT		iTargetID;
extern INT		ActiveHwVersion; // set at login time

extern UINT16	HeaderEncryptAlgo;
extern UINT16	DataEncryptAlgo;

INT 
RwPatternChecking (
	SOCKET	connsock, 
	INT		WriteSize, 
	INT		Ite, 
	UINT64	Pos, 
	INT		SectorCount, 
	INT		LockedWriteMode
	);

INT 
ReadPattern (
	SOCKET	connsock, 
	INT		Pattern, 
	UINT64	Pos, 
	INT		IoSize, 
	INT		Blocks
	);

INT
WritePattern (
	SOCKET	connsock, 
	INT		Pattern, 
	UINT64	Pos, 
	INT		IoSize, 
	INT		Blocks, 
	INT		LockedWriteMode
	);

INT SetTransferMode (
	SOCKET	connsock, 
	INT		TargetId, 
	PCHAR	Mode
	);

inline
VOID
ReverseBytes (
	PUCHAR	Buf,
	INT		len
	)
{
	INT i;
	UCHAR Temp;

	for (i=0;i<len/2;i++) {
	
		Temp		 = Buf[i];
		Buf[i]		 = Buf[len-i-1];
		Buf[len-i-1] = Temp;
	}
}

inline 
VOID
ConvertString (
	PCHAR	result,
	PCHAR	source,
	INT		size
	)
{
	for (INT i = 0; i < size / 2; i++) {

		result[i * 2]	  = source[i * 2 + 1];
		result[i * 2 + 1] = source[i * 2];
	}

	result[size] = '\0';
}

INT CmdDiscovery (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	SOCKET			connsock;
	LPX_ADDRESS		address;
	INT				retval;

	retval = lpx_addr(target, &address);

	if (!retval) {
	
		printf("Invalid address\n");
		return -1;
	}

	if (MakeConnection(&address, &connsock) == FALSE) {

		fprintf(stderr, "[NdasCli]main: Can't Make Connection to LanDisk!!!\n");
		return  -1;
	}

	Discovery(connsock);
	closesocket(connsock);

	return 0;
}

// Arg[0]: Write size in MB per iteration
// Arg[1]: Number of iteration
// Arg[2]: Position to write in MB
// Arg[3]: Lock mode
// Arg[4]: UserId (Optional)
// Arg[5]: Block size (Optional)

INT 
CmdLockedWrite ( 
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	INT		WriteSize;
	INT		Ite;
	UINT64	Pos;
	INT		retval;
	INT		LockMode;
	SOCKET	connsock;
	INT		UserId;
	INT		Blocks;

	if (arg[3] == 0) {

		NdasEmuDbgCall( 4, "Not enough parameter\n" );
		return -1;
	}

	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Ite = (int) _strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	LockMode = (int) _strtoi64(arg[3], NULL, 0);
	
	if (arg[4] == 0) {
	
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	} else {

		UserId = (int) _strtoi64(arg[4], NULL, 0);
	}

	if (arg[5] == 0) {

		Blocks = 128;

	} else {

		Blocks = (int) _strtoi64(arg[5], NULL, 0);
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {
		
		return -1;
	}

	// Need to get disk info before IO
	
	if ((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {

		NdasEmuDbgCall( 4, "[NdasCli]GetDiskInfo Failed...\n" );
		return retval;
	}

	retval = RwPatternChecking( connsock, WriteSize, Ite, Pos, Blocks, LockMode );

	DisconnectFromNdas(connsock, UserId);
	return retval;
}

// arg[0]: Dev number. Optional. Default value is 0.

INT 
CmdGetNativeMaxAddr (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	INT		retval = 0;
	SOCKET	connsock;
	PUCHAR	data = NULL;
	UINT	UserId;
	INT		dev;

	if (arg[0] == NULL) {
	
		dev = 0;

	} else {

		dev = (INT) _strtoi64(arg[0], NULL, 0);

		if (dev!=0 && dev!=1) {

			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);
	} else {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		goto errout;
	}

	retval = IdeCommand( connsock, iTargetID, 0, WIN_READ_NATIVE_MAX_EXT, 0, 0, 0, 0, 0, 0, 0 );
	retval = IdeCommand( connsock, iTargetID, 0, WIN_READ_NATIVE_MAX,     0, 0, 0, 0, 0, 0, 0 );

	if (retval != 0) {

		fprintf(stderr, "IdeCommand failed\n");
		goto errout;
	}

	DisconnectFromNdas(connsock, UserId);

errout:

	closesocket(connsock);
	return retval;

}


#define MUTEX_INFO_LEN		64
#define NUMBER_OF_MUTEX		 8
#define NUMBER_OF_MUTEX_20	 4

INT CmdMutexCli (
	CHAR	*target, 
	CHAR	*arg[]
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	
	INT		UserId;
	
	union {

		struct {
		
			UINT32	Param0;
			UINT32	Param1;
			UINT32	Param2;
		};

		UCHAR	Param8[12];
	};

	BYTE*	Param0Byte = (BYTE*)&Param0;
	CHAR	line[100];
	CHAR	cmd[10];
	INT		LockNumber;
	CHAR	LockData[64+1] = {0};
	
	INT		port;
	INT		i;
	BOOL	Locked;

	RtlZeroMemory( Param8, 12 );
	Param0 = 0; Param1 = 0; Param2 = 0;

	UserId = MAKE_USER_ID( DEFAULT_USER_NUM, USER_PERMISSION_RO );
	
	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		goto errout;
	}

	NdasEmuDbgCall( 4, "Help: <Take | Give | Setinfo | Info | Break | Owner | infoAll | Quit> [Lock number] [Lock Data]\n" );

	while (1) {
	
		if( fgets(line, 100, stdin) == NULL) {
		
			NdasEmuDbgCall( 4, "fgets error\n" );
			break;
		}

		if (line[0]==0 || line[0] == '\n') {

			continue;
		}

		if (toupper(line[0]) == 'Q') {

			break;
		}

		memset( LockData, 0, 64 );
		
		if (strlen(line) > 0) {

			retVal = sscanf( line, "%s %d %s", cmd, &LockNumber, LockData );
			
			if (retVal<1) {

				NdasEmuDbgCall( 4, "Invalid command\n" );
				continue;
			}

			if (retVal==1) {
			
				LockNumber = 0;
			}

			if (retVal==2) {
			
				LockData[0] = 0;
			}

		} else {

			continue;
		}

		switch(toupper(cmd[0])) {

		case 'T':
		case 't':

			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

				Param0Byte[3] = (BYTE)LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_SET_MUTEX, NULL, &Param0, &Param1, &Param2, LockData, 0, NULL );
			
				if (iResult == 0) {

					NdasEmuDbgCall( 4, "Lock result = %d\n", Param0Byte[0] );
					NdasEmuDbgCall( 4, "Set lock succeeded: previous lock info = %s\n", (CHAR*)LockData );
		
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {

					NdasEmuDbgCall( 4, "Set lock failed: Already locked. Lock info = %s\n", (CHAR*)LockData );

				} else {

					NdasEmuDbgCall( 4, "Set lock failed: Unexpected error code=%x\n", iResult );
				}
			
			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {

				Param8[3] = (UCHAR) LockNumber;
				
				iResult = VendorCommand( connsock, VENDOR_OP_SET_MUTEX, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );
				
				if (iResult == 0) {
				
					NdasEmuDbgCall( 4, "Set lock succeeded: counter = %d\n", ntohl(Param1) );

				} else if (iResult == LANSCSI_RESPONSE_T_SET_SEMA_FAIL) {

					NdasEmuDbgCall( 4, "Set lock failed: Already locked. Counter = %u, Param0 = %d, Param2 = %d\n", ntohl(Param1), ntohl(Param0), ntohl(Param2) );

				} else {

					NdasEmuDbgCall( 4, "Set lock failed: error code=%x. Counter = %u\n", iResult, ntohl(Param1) );
				}
			}

			break;

		case 'G':
		case 'g':

			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

				Param0Byte[3] = (BYTE)LockNumber;
				
				if (LockData[0] == 0) {
				
					iResult = VendorCommand( connsock, VENDOR_OP_FREE_MUTEX, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

				} else {

					iResult = VendorCommand( connsock, VENDOR_OP_FREE_MUTEX, NULL, &Param0, &Param1, &Param2, LockData, MUTEX_INFO_LEN, NULL );
				}

				if (iResult == 0) {

					NdasEmuDbgCall( 4, "Free lock succeeded\n" );

				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {

					NdasEmuDbgCall( 4, "Free lock failed: Not lock owner\n" );

				} else {

					NdasEmuDbgCall( 4, "Free lock failed. Unexpected error code=%x\n", iResult );
				}
			
			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				
				Param8[3] = (UCHAR) LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_FREE_MUTEX, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );
				
				if (iResult == 0) {

					NdasEmuDbgCall( 4, "Free lock succeeded: counter = %u\n", ntohl(Param1) );
				
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
				
					NdasEmuDbgCall( 4, "Free lock failed: Result=%d. Counter = %d\n", iResult, ntohl(Param1) );

				} else {

					NdasEmuDbgCall( 4, "Free lock failed: error code=%x\n", iResult );
				}
			}

			break;

		case 'S': // Set info

			if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {

				NdasEmuDbgCall( 4, "Unsupported command in this chip\n" );
				break;
			}

			Param0Byte[3] = (BYTE)LockNumber;

			iResult = VendorCommand(connsock, VENDOR_OP_SET_MUTEX_INFO, NULL, &Param0, &Param1, &Param2, LockData, MUTEX_INFO_LEN, NULL);
			
			if (iResult ==0) {
			
				NdasEmuDbgCall( 4, "Set info succeeded\n" );

			} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {

				NdasEmuDbgCall( 4, "Set info failed: Not a lock owner.\n");

			} else {

				NdasEmuDbgCall( 4, "Set info failed: Unexpected error code=%x\n", iResult);
			}

			break;

		case 'I': // Get info
		case 'i': 
			
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			
				Param0Byte[3] = (BYTE)LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_INFO, NULL, &Param0, &Param1, &Param2, LockData, 0, NULL );

				if (iResult == 0) {

					NdasEmuDbgCall( 4, "Get lock info succeeded: lock info = %s\n", (CHAR*)LockData );

				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) { 

					NdasEmuDbgCall( 4, "Get lock info failed: lock is not held. lock info =%s\n", (CHAR*)LockData );

				} else {

					NdasEmuDbgCall( 4, "Get lock info failed: Unexpected error=%x\n", iResult );
				}

			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
		
				Param8[3] = (UCHAR) LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_INFO, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );

				if (iResult == 0) {

					NdasEmuDbgCall( 4, "Get lock info succeeded: counter = %u\n", ntohl(Param1) );

				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					
					NdasEmuDbgCall( 4, "Get lock info failed: Counter = %u\n", ntohl(Param1) );
				
				} else {

					NdasEmuDbgCall( 4, "Get lock info : error code=%x\n", iResult );
				}

			} else {
				
				NdasEmuDbgCall( 4, "Unsupported command in this chip\n" );
				break;
			}

			break;

		case 'B': // Break
		case 'b': 
			
			if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {
			
				NdasEmuDbgCall( 4, "Unsupported command in this chip\n");
				break;
			}

			Param0Byte[3] = (BYTE)LockNumber;

			iResult = VendorCommand( connsock, VENDOR_OP_BREAK_MUTEX, NULL, &Param0, &Param1, &Param2, LockData, 0, NULL) ;
			
			if (iResult == 0) {
			
				NdasEmuDbgCall( 4, "Lock result = %d\n", Param0Byte[0]);

			} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) { 

				NdasEmuDbgCall( 4, "Break lock failed: lock is not held." );

			} else {

				NdasEmuDbgCall( 4, "Break lock failed: Unexpected error=%x\n", iResult );
			}

			break;

		case 'O': // Get owner
		case 'o': 

			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

				Param0Byte[3] = (BYTE)LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_OWNER, NULL, &Param0, &Param1, &Param2, 0, 0, NULL );

				if (iResult == 0) {

					port = htonl(Param1) >> 16;

					NdasEmuDbgCall( 4, "Lock result = %d\n", Param0Byte[0]);
					NdasEmuDbgCall( 4, "Get owner succeeded: MAC %02x:%02x:%02x:%02x:%02x:%02x Port %x\n", 
						((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
						((BYTE*)&Param2)[0], ((BYTE*)&Param2)[1],
						((BYTE*)&Param2)[2], ((BYTE*)&Param2)[3],
						port);

				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {

					NdasEmuDbgCall( 4, "Get owner failed: lock is not held\n");

				} else {

					NdasEmuDbgCall( 4, "Get owner failed: Error=%x\n", iResult);
				}

			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {

				Param8[3] = (UCHAR) LockNumber;

				iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_OWNER, Param8, &Param0, &Param1, &Param2, 0, 0, NULL );
				
				if (iResult == 0) {
				
					port = htonl(Param0) >> 16;

					NdasEmuDbgCall( 4, "Lock result = %d\n", Param0Byte[0] );
					
					NdasEmuDbgCall( 4, "Get owner succeeded: MAC %02x:%02x:%02x:%02x:%02x:%02x Port %x\n", 
							((BYTE*)&Param0)[2], ((BYTE*)&Param0)[3],
							((BYTE*)&Param1)[0], ((BYTE*)&Param1)[1],
							((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
							port );

				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {

					NdasEmuDbgCall( 4, "Get owner failed: lock is not held\n");

				} else {

					NdasEmuDbgCall( 4, "Get owner failed: Error=%x\n", iResult);
				}
			}

			break;

		case 'A': // info All
		case 'a': 

			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

				for (i=0;i<NUMBER_OF_MUTEX;i++) {
	
					Param0Byte[3] = (BYTE)i;

					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_INFO, NULL, &Param0, &Param1, &Param2, LockData, 0, NULL);

					if (iResult == 0) {

						Locked = TRUE;

					} else {

						Locked = FALSE;
					}

					Param0Byte[3] = (BYTE)i;

					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_OWNER, NULL, &Param0, &Param1, &Param2, 0, 0, NULL);

					NdasEmuDbgCall( 4, "#%d: %s ", i, Locked ? "Held":"Free" );

					if (Locked) {

						port = htonl(Param1) >> 16;

						NdasEmuDbgCall( 4, "%02x:%02x:%02x:%02x:%02x:%02x.%04x ",
							((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
							((BYTE*)&Param2)[0], ((BYTE*)&Param2)[1],
							((BYTE*)&Param2)[2], ((BYTE*)&Param2)[3],
							port);

					} else {

						NdasEmuDbgCall( 4, "                       ");
					}

					NdasEmuDbgCall( 4, "Info=%s\n", LockData);
				}

			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				
				UINT counter;

				for (i=0; i<NUMBER_OF_MUTEX_20; i++) {
				
					RtlZeroMemory( Param8, 12 );
					Param8[3] = (UCHAR) i;

					iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_INFO, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );
					
					if (iResult != 0) {
					
						NdasEmuDbgCall( 4, "#%d: Failed to get counter.\n", i );
						continue;
					}

					counter = ntohl(Param1);

					RtlZeroMemory( Param8, 12 );
					Param8[3] = (UCHAR) i;

					iResult = VendorCommand( connsock, VENDOR_OP_GET_MUTEX_OWNER, Param8, &Param0, &Param1, &Param2, 0, 0, NULL );

					if (iResult != 0) {

						NdasEmuDbgCall( 4, "#%d: Failed to get owner.\n", i );
						continue;
					}

					NdasEmuDbgCall( 4, "#%d: ", i );

					port = htonl(Param0) >> 16;
					
					NdasEmuDbgCall( 4, "%02x:%02x:%02x:%02x:%02x:%02x ", 
							((BYTE*)&Param0)[2], ((BYTE*)&Param0)[3],
							((BYTE*)&Param1)[0], ((BYTE*)&Param1)[1],
							((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3] );
				
					NdasEmuDbgCall( 4, "Counter=%u\n", counter );
				}

			} else {
				
				NdasEmuDbgCall( 4, "Unsupported command in this chip\n" );
				break;
			}

			break;

		default:

			NdasEmuDbgCall( 4, "Unknown command\n" );
			break;
		}
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

// Arg[0]: UserNumber
// Arg[1]: New password 
// Arg[2]: Superuser password. Optional.

INT 
CmdSetPassword (
	PCHAR target, 
	PCHAR arg[]
	)
{
	INT		retval = 0;
	SOCKET	connsock;
	INT		iResult;
	UINT	UserId;
	UCHAR	NewPassword[PASSWORD_LENGTH + 16] = {0};
	INT		UserNum;

	union {

		struct {
		
			UINT32	Param0;
			UINT32	Param1;
			UINT32	Param2;
		};

		UCHAR	Param8[12];
	};

	RtlZeroMemory( Param8, 12 );
	Param0 = 0; Param1 = 0; Param2 = 0;

	UserNum = (INT) _strtoi64( arg[0], NULL, 0 );

	if (UserNum > 7) {

		NdasEmuDbgCall( 4, "Invalid user number\n" );
		return -1;
	}

	UserId = MAKE_USER_ID( SUPERVISOR_USER_NUM, USER_PERMISSION_EW );

#if 0
	NdasEmuDbgCall( 4, "Using password 0xffffffffff\n");
	memset(ResetPw, 0x0ff, 8);
	ResetPw[8] =0;
	arg[2] = (CHAR*)ResetPw;
#endif

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[2]) != 0) {

		goto errout;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
	
		// Assume password is plain text.

		strncpy( (CHAR *)NewPassword, arg[1], PASSWORD_LENGTH );

		Param0 = htonl(0);
		Param1 = htonl(0x30 + 16 * UserNum);
		Param2 = htonl(16);

		ReverseBytes(NewPassword, PASSWORD_LENGTH);
		
		iResult = VendorCommand(connsock, VENDOR_OP_SET_EEP, NULL, &Param0, &Param1, &Param2, NULL, 0, (PCHAR)NewPassword);
		
		if (iResult == 0) {
		
			ReverseBytes(NewPassword, PASSWORD_LENGTH);
			NdasEmuDbgCall( 4, "Changed password to %s\n", arg[1]);

		} else {
			
			goto errout;
		}

	} else {
		
		// 1.1 or 2.0
		// Assume password is plain text.
		
		if (arg[1] == NULL) {

			NdasEmuDbgCall( 4, "Setting to default password\n" );
			
			if (UserNum == 0) {
			
				// super user

				Param8[7] = 0x1E;
				Param8[6] = 0x13;
				Param8[5] = 0x50;
				Param8[4] = 0x47;
				Param8[3] = 0x1A;
				Param8[2] = 0x32;
				Param8[1] = 0x2B;
				Param8[0] = 0x3E;
			
			} else {
			
				// normal user

				Param8[7] = 0xBB;
				Param8[6] = 0xEA;
				Param8[5] = 0x30;
				Param8[4] = 0x15;
				Param8[3] = 0x73;
				Param8[2] = 0x50;
				Param8[1] = 0x4A;
				Param8[0] = 0x1F;
			}

		} else {

			PCHAR	pStart, pEnd;

			pStart = arg[1];

			for (UINT i=0; i<strlen(arg[1]); i++) {

				if (i == PASSWORD_LENGTH_V1) {

					break;
				}

				Param8[i] = (UCHAR)strtoul(pStart, &pEnd, 16);

				pStart += 3;
			}
		}

		if (UserNum == 0) {
		
			NdasEmuDbgCall( 4, "Changing superuser password\n" );
			
			iResult = VendorCommand( connsock, VENDOR_OP_SET_SUPERVISOR_PW, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );

		} else {

			NdasEmuDbgCall( 4, "Changing normal user password.\n" );

			iResult = VendorCommand( connsock, VENDOR_OP_SET_USER_PW, Param8, &Param0, &Param1, &Param2, NULL, 0, NULL );
		}
		
		if (iResult == 0) {

			NdasEmuDbgCall( 4, "Changed password to %s\n", arg[1] );

		} else {

			goto errout;
		}
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

// Arg[0]: user id(High 16 bit) + permission(Low 16 bit).
// Arg[1]: Address. Should be a multiple of 16
// Arg[2]: Length

INT GetEep (
	PCHAR	target, 
	PCHAR	arg[], 
	BOOL	UserMode
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	CHAR	FileBuf[1024+16] = {0};
	INT		UserId;
	UINT32	Param0, Param1, Param2;
	INT		address;
	INT		length;
	UCHAR	Operation;

	UserId = (INT) _strtoi64( arg[0], NULL, 0 );

	address = (INT) _strtoi64( arg[1], NULL, 0 );
	length = (INT) _strtoi64( arg[2], NULL, 0 );
	
	if (address %16 != 0) {
	
		NdasEmuDbgCall( 4, "Address should be multiple of 16\n" );
		return -1;
	}

	if (UserMode && address < 1024) {

		NdasEmuDbgCall( 4, "WARNING address should be more than 1024 in usermode eep operation\n" );
	}

	if (length % 16 != 0) {

		NdasEmuDbgCall( 4, "Length should be multiple of 16\n" );
		return -1;
	}

	NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, length );

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		goto errout;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		if (UserMode) {
		
			NdasEmuDbgCall( 4, "GET_U_EEP:");
			Operation = VENDOR_OP_U_GET_EEP;

		} else {

			NdasEmuDbgCall( 4, "GET_EEP:");
			Operation = VENDOR_OP_GET_EEP;
		}

		Param0 = htonl(0);	// not used
		Param1 = htonl(address);
		Param2 = htonl(length);

		iResult = VendorCommand(connsock, Operation, NULL, &Param0, &Param1, &Param2, NULL, 0, FileBuf );

	} else {

		NdasEmuDbgCall( 4, "Running 2.0G mode geteep\n" );

		if (length == 16) {

			CHAR AhsBuf[12];
			CHAR tempBuf[16];
			INT  i;

			Operation = VENDOR_OP_GET_EEP;
			
			NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, length );

			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);

			iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, AhsBuf, 12, NULL );

			memcpy( tempBuf, &Param2, 4 );
			memcpy( &tempBuf[4], AhsBuf, 12 );

			// swap buffer

			for (i=0; i< 16; i++) {

				FileBuf[i] = tempBuf[15-i];
			}

		} else {

			NdasEmuDbgCall( 4, "Length should be 16 in 2.0G GETEEP\n" );

			iResult = -1;
		}
	}

	if (iResult < 0) {

		NdasEmuDbgCall( 4, "Failed to run vendor command\n" );

	} else {

		NdasEmuDbgCall( 4, "Result %0x\n", iResult );

		PrintHex( (PUCHAR)FileBuf, length );
		NdasEmuDbgCall( 4, "\n");
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

// No argument

INT CmdGetDumpEep (
	PCHAR target, 
	PCHAR arg[]
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	CHAR	FileBuf[2048] = {0};
	INT		UserId;
	UINT32	Param0, Param1, Param2;
	INT		address;
	UCHAR	Operation;

	UserId = MAKE_USER_ID( SUPERVISOR_USER_NUM, USER_PERMISSION_EW );

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		goto errout;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
	
		NdasEmuDbgCall( 4, "Not implemented for NDAS 2.5\n");
#if 0
		if (UserMode) {

			NdasEmuDbgCall( 4, "GET_U_EEP:");
			Operation = VENDOR_OP_U_GET_EEP;

		} else {

			NdasEmuDbgCall( 4, "GET_EEP:");
			Operation = VENDOR_OP_GET_EEP;
		}

		Param0 = htonl(0);	// not used
		Param1 = htonl(address);
		Param2 = htonl(length);

		iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, NULL, 0, FileBuf);
#endif

	} else {

		NdasEmuDbgCall( 4, "Running 2.0G mode get eep\n" );

		for (address = 0; address < 2048;address += 16) {

			CHAR AhsBuf[12];
			CHAR buf[16];
			INT  i;

			Operation = VENDOR_OP_GET_EEP;
			
			NdasEmuDbgCall( 4, ".");
			
			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			Param0 = htonl(0); // not used.

			iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, AhsBuf, 12, NULL );

			memcpy( buf, &Param2, 4 );
			memcpy( &buf[4], AhsBuf, 12 );

			// Swap and save to filebuf
			
			for (i=0;i<16;i++) {
			
				FileBuf[address+i] = buf[15-i];
			}
		}

		for (address = 0; address < 2048; address += 32) {

			INT i;

			NdasEmuDbgCall( 4, "\n%04x:", address );
			
			for(i=0;i<32;i++) {
			
				if (i%4 == 0) {

					NdasEmuDbgCall( 4, " ");
				}

				NdasEmuDbgCall( 4, "%02x", (UCHAR)FileBuf[address+i] );
			}
		}

		NdasEmuDbgCall( 4, "\n");
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

INT CmdGetEep (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	return GetEep( target, arg, FALSE );
}

INT 
CmdGetUEep (
	PCHAR target, 
	PCHAR arg[]
	)
{
	return GetEep( target, arg, TRUE );
}

// Arg[0]: user id(High 16 bit) + permission(Low 16 bit).
// Arg[1]: Address. Should be a multiple of 16
// Arg[2]: Length.
// Arg[3]: File name to write. File format is binary.

INT 
SetEep (
	PCHAR	target, 
	PCHAR	arg[], 
	BOOL	UserMode
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	BOOL	Ret;
	CHAR	FileBuf[1024 + 16] = {0};
	INT		UserId;
	UINT32	Param0, Param1, Param2;
	FILE	*eeprom = NULL;
	INT		address;
	INT		length;
	UCHAR	Operation;

	UserId = (INT) _strtoi64( arg[0], NULL, 0 );

	address = (INT) _strtoi64( arg[1], NULL, 0 );
	length = (INT) _strtoi64( arg[2], NULL, 0 );

	if (address %16 != 0) {

		NdasEmuDbgCall( 4, "Address should be multiple of 16 %d\n", address );
		return -1;
	}

	if (UserMode && address < 1024) {

		NdasEmuDbgCall( 4, "WARNING address should be more than 1024 in usermode eep operation\n" );
	}

	if (length %16 != 0 ) {

		NdasEmuDbgCall( 4, "Length should be multiple of 16 length = %d\n", length);
		return -1;
	}

	eeprom = fopen( arg[3], "rb" );

	if (eeprom == 0) {
	
		NdasEmuDbgCall( 4, "Failed to open ROM file %s\n", arg[3] );
		goto errout;
	}

	iResult = (INT)fread( FileBuf, 1, length, eeprom );

	if (iResult != length) {
	
		NdasEmuDbgCall( 4, "Failed to read file\n" );
		goto errout;
	}

	Ret = ConnectToNdas( &connsock, target, UserId, NULL );

	if (Ret) {

		return -1;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
		if (UserMode) {
		
			NdasEmuDbgCall( 4, "SET_U_EEP:" );
			Operation = VENDOR_OP_U_SET_EEP;

		} else {

			NdasEmuDbgCall( 4, "SET_EEP:" );
			Operation = VENDOR_OP_SET_EEP;
		}

		NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, length );

		Param0 = htonl(0); // not used
		Param1 = htonl(address);
		Param2 = htonl(length);

		iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, NULL, 0, FileBuf );

	} else {

		NdasEmuDbgCall( 4, "Running 2.0G mode seteep\n");

		if (length == 16) {

			CHAR AhsBuf[12];
			CHAR tempBuf[16];
			INT  i;

			Operation = VENDOR_OP_SET_EEP;
			
			NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, length );

			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			
			// swap buffer

			for (i=0; i< 16; i++) {

				tempBuf[i] = FileBuf[15-i];
			}

			memcpy( &Param2, tempBuf, 4 );
			memcpy( AhsBuf, &tempBuf[4], 12 );

			iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, AhsBuf, 12, NULL );

		} else {

			NdasEmuDbgCall( 4, "Length should be 16 in 2.0G SETEEP\n" );
			iResult = -1;
		}
	}

	if (iResult < 0) {

		NdasEmuDbgCall( 4, "Failed to run vendor command\n");

	} else {

		NdasEmuDbgCall( 4, "Result %0x\n", iResult );
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	if (eeprom) {

		fclose(eeprom);
	}

	return 0;
}

INT CmdSetDumpEep (
	PCHAR target, 
	PCHAR arg[]
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	BOOL	Ret;
	CHAR	FileBuf[2048] = {0};
	INT		UserId;
	UINT32	Param0, Param1, Param2;
	FILE	*eeprom = NULL;
	INT		address;
	INT		length;
	UCHAR	Operation;

	if (arg[0] == NULL || arg[1] == NULL || arg[2] == NULL) {

		NdasEmuDbgCall( 4, "Not enough parameter\n" );
		return -1;
	}

	UserId = (INT) _strtoi64( arg[0], NULL, 0 );

	address = (INT) _strtoi64( arg[1], NULL, 0 );
	length = (INT) _strtoi64( arg[2], NULL, 0 );

	if (address %16 != 0) {

		NdasEmuDbgCall( 4, "Address should be multiple of 16 %d\n", address );
		return -1;
	}

	if (length %16 != 0 ) {

		NdasEmuDbgCall( 4, "Length should be multiple of 16 length = %d\n", length );
		return -1;
	}

	eeprom = fopen( arg[3], "rb" );

	if (eeprom == 0) {
	
		NdasEmuDbgCall( 4, "Failed to open ROM file %s\n", arg[3] );
		goto errout;
	}

	iResult = (INT)fread( FileBuf, 1, length, eeprom );

	if (iResult != length) {
	
		NdasEmuDbgCall( 4, "Failed to read file\n" );
		goto errout;
	}

	Ret = ConnectToNdas( &connsock, target, UserId, NULL );

	if (Ret) {

		return -1;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		
		NdasEmuDbgCall( 4, "SET_EEP:" );
		Operation = VENDOR_OP_SET_EEP;

		NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, length );

		Param0 = htonl(0); // not used
		Param1 = htonl(address);
		Param2 = htonl(length);

		iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, NULL, 0, FileBuf );

	} else {

		NdasEmuDbgCall( 4, "Running 2.0G mode seteep\n" );

		for (address = 0; address < 2048; address += 16) {

			CHAR AhsBuf[12];
			CHAR tempBuf[16];
			INT  i;

			Operation = VENDOR_OP_SET_EEP;
			
			NdasEmuDbgCall( 4, "UserId %x, Address %08x, Length %08x\n", UserId, address, 16 );

			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			
			// swap buffer

			for (i=0; i< 16; i++) {

				tempBuf[i] = FileBuf[address + 15 - i];
			}

			memcpy( &Param2, tempBuf, 4 );
			memcpy( AhsBuf, &tempBuf[4], 12 );

			iResult = VendorCommand( connsock, Operation, NULL, &Param0, &Param1, &Param2, AhsBuf, 12, NULL );
		}
	}

	if (iResult < 0) {

		NdasEmuDbgCall( 4, "Failed to run vendor command\n" );

	} else {

		NdasEmuDbgCall( 4, "Result %0x\n", iResult );
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	if (eeprom) {

		fclose(eeprom);
	}

	return 0;
}

INT 
CmdSetEep (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	return SetEep( target, arg, FALSE );
}

INT CmdSetUEep (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	return SetEep( target, arg, TRUE );
}

INT 
CmdTextTargetData (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	SOCKET	connsock;
	INT		UserId;
	INT		retval;
	UINT64	Param = 0;
	BOOL	bSet = FALSE;

	if (arg[0]) {

		if (strcmp(arg[0], "Set") == 0) {
		
			bSet = TRUE;

			if (arg[1] == NULL) {

				NdasEmuDbgCall( 4, "\nNot enough parameter\n" );
				return -1;
			}

			Param = _strtoi64(arg[1], NULL, 0);
		}
	}

	UserId = MAKE_USER_ID( DEFAULT_USER_NUM, USER_PERMISSION_SW );

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		return -1;
	}

	if (bSet) {
	
		retval = TextTargetData( connsock, PARAMETER_OP_SET, iTargetID, &Param );

		if (retval == 0) {

			fprintf(stderr, "TextTargetData set: %I64d\n", Param);
		}

	} else {

		retval = TextTargetData( connsock, PARAMETER_OP_GET, iTargetID, &Param );
		
		if (retval == 0) {

			fprintf( stderr, "TextTargetData get: %I64d\n", Param );
		}
	}

	if (retval != 0) {

		fprintf( stderr, "TextTargetData failed %d.\n", retval );
	}

	DisconnectFromNdas(connsock, UserId);
	return 0;
}

// arg[0]: Dev number. Optional. Default value is 0.

INT 
CmdIdentify (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	SOCKET	connsock;
	INT		UserId;
	INT		iResult;
	INT		dev;

	struct hd_driveid	info;
	CHAR				buffer[41];
	
	INT		bSmartSupported, bSmartEnabled;

	if (arg[0] == NULL) {

		dev = 0;

	} else {

		dev = (INT) _strtoi64(arg[0], NULL, 0);

		if (dev!=0 && dev!=1) {

			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);

	} else {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		return -1;
	}

	iResult = IdeCommand( connsock, iTargetID, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0 );

	if (iResult != 0) {
	
		fprintf(stderr, "Identify Failed...\n");
		goto errout;
	}

	printf( "Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
			iTargetID, info.major_rev_num, info.minor_rev_num, info.capability );

	printf( "dma_mword 0x%x, U-DMA 0x%x - Current mode=", 
			info.dma_mword, info.dma_ultra );

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

	printf( "Supported PIO mode 0x%x\n", info.eide_pio_modes );
		
	printf( "PIO W/O IORDY 0x%x, PIO W/ IORDY 0x%x\n", info.eide_pio, info.eide_pio_iordy );

	ConvertString( (PCHAR)buffer, (PCHAR)info.serial_no, 20 );

	printf( "Serial No: %s\n", buffer );
	
	ConvertString( (PCHAR)buffer, (PCHAR)info.fw_rev, 8 );
	
	printf( "Firmware rev: %s\n", buffer );
	
	memset( buffer, 0, 41 );

	strncpy(buffer, (PCHAR)info.model, 40);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);

	printf( "Model No: %s\n", buffer );

	printf( "Capability %x\n", info.capability );

	printf( "Capacity 2 %I64d, Capacity %d\n", info.lba_capacity_2, info.lba_capacity );

	printf( "LBA %d, LBA48 %d\n", (info.capability &= 0x02), (info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400) );
	
	bSmartSupported =  info.command_set_1 & 0x0001;

	if (bSmartSupported) {
	
		bSmartEnabled = info.cfs_enable_1 & 0x0001;

	} else {

		bSmartEnabled = FALSE;
	}

	printf( "SMART feature: %s and %s\n", 
			bSmartSupported ? "Supported" : "Not supported", bSmartEnabled?"Enabled":"Disabled" );
	
	printf( "Write cache feature: support=%d enabled=%d\n", 
			(info.command_set_1 & 0x20) != 0, (info.cfs_enable_1 & 0x20) != 0 );

	printf( "Config feature: support=%d enabled=%d\n",
			(info.command_set_2 & 0x0800) != 0, (info.cfs_enable_2 & 0x0800) != 0 );

	printf( "FUA feature: WRITEDMA_FUA_EXT, MULTWRITE_FUA_EXT: support=%d enabled=%d\n",
			(info.cfsse & 0x0040) == 0x0040, (info.csf_default & 0x0040) == 0x0040 );
	
	printf( "FUA feature: WRITEDMA_QUEUED_FUA_EXT: support=%d enabled=%d\n",
			(info.cfsse & 0x0080) == 0x0080, (info.csf_default & 0x0080) == 0x0080 );

	if (info.cfs_enable_2 & 0x0800) {

		struct hd_driveconf drvconf;

		iResult = IdeCommand( connsock, 
							  iTargetID, 
							  0, 
							  WIN_DEV_CONFIG, 
							  0, 
							  0, 
							  (_int8)DEVCONFIG_CONFIG_IDENTIFY,
							  sizeof(drvconf), 
							  (PCHAR)&drvconf,
							  0, 
							  0 );

		if (iResult != 0) {

			fprintf( stderr, "Device configuration identify Failed...\n" );
			goto errout;
		}

		printf( "DevConf: Revision=%04x\n", drvconf.revision );

		printf( "DevConf: Dma=%04x\n", drvconf.dma );
		printf( "DevConf: UDma=%04x\n", drvconf.udma);
		printf( "DevConf: Max LBA=%I64x\n",	
				((_int64)drvconf.maximum_lba[0]) + ((_int64)drvconf.maximum_lba[1] << 16) +
				((_int64)drvconf.maximum_lba[2] << 32) + ((_int64)drvconf.maximum_lba[3] << 48) );
	
		printf( "DevConf: Command set/features=%04x\n", drvconf.cmd_sfs );
		printf( "DevConf: Integrity=%04x\n", drvconf.integrity );
	}

errout:

	DisconnectFromNdas( connsock, UserId );
	
	return 0;

}

// Arg[0]: File to compare
// Arg[1]: number of Iteration
// Arg[2]: Position to write in sector
// Arg[3]: Device number 
// Arg[4]: Sector count per operation
// Arg[5]: (optional) Transfer mode

INT CmdCheckFile (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	FILE	*wfile = NULL;
	PCHAR	filebuf;
	PCHAR	diskbuf;
	INT		Ite;
	UINT64	Pos;
	INT		retval;
	INT		SectorCount;
	SOCKET	connsock;
	UINT32	UserId;
	INT		i;
	INT		dev;

	Ite =  (INT)_strtoi64(arg[1], NULL, 0);

	Pos = _strtoi64(arg[2], NULL, 0);

	if (arg[4] && arg[4][0]) {

		SectorCount = (INT) _strtoi64(arg[4], NULL, 0);

	} else {

		SectorCount = 128;
	}

	filebuf = (CHAR*)malloc(SectorCount * 512);
	diskbuf = (CHAR*)malloc(SectorCount * 512);

	wfile = fopen(arg[0], "rb");

	if (wfile == 0) {

		printf("Failed to open file %s\n", arg[0]);
		goto errout;
	}

	retval = (INT)fread(filebuf, 1, 512, wfile);

	if (retval != 512) {

		printf( "Failed to read file\n" );
		goto errout;
	}

	for (i=1; i<SectorCount; i++) {

		memcpy( filebuf+i*512, filebuf, 512 );
	}

	if (arg[3] == NULL) {

		dev = 0;

	} else {

		dev = (INT) _strtoi64(arg[3], NULL, 0);

		if (dev!=0 && dev!=1) {

			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev == 0) {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		return -1;
	}

	if (arg[5] && arg[5][0]) {
	
		SetTransferMode( connsock, iTargetID, arg[5] );

	} else {

		if ((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {

			fprintf( stderr, "[NdasCli]GetDiskInfo Failed...\n" );
			return retval;
		}
	}

	printf( "Iteration: %3d Pos: %I64d, Sectors: %d\n", Ite, Pos, SectorCount );

	for (i=0;i<Ite; i++) {

		retval = IdeCommand( connsock, 
							 (short)iTargetID, 
							 0, 
							 WIN_READ, 
							 (Pos * 512 + i*512*SectorCount) / 512, 
							 (short)SectorCount, 
							 0, 
							 SectorCount * 512, 
							 (PCHAR)diskbuf, 
							 0, 
							 0 );

		if (retval != 0) {
		
			fprintf( stderr, "\n[NdasCli]main: WRITE Failed... Sector %d\n", (Pos * 512 + i*512*SectorCount) / 512 );
			goto errout;
		}

		if (memcmp(diskbuf, filebuf, SectorCount * 512)!=0) {

			fprintf( stderr, "Data mismatch at sector %d\n", (UINT32)((Pos * 512 + i*512*SectorCount) / 512) );
		}
	}

	DisconnectFromNdas(connsock, UserId);

errout:

	if (filebuf) {

		free(filebuf);
	}

	if (diskbuf) {

		free(diskbuf);
	}

	return retval;
}


// Arg[0]: File.
// Arg[1]: number of sectors to read
// Arg[2]: Position to read in sector
// Arg[3]: (optional) Device number 
// Arg[4]: (optional) Sector count per operation. 0 for random value from 1~128. Default 1
// Arg[5]: (optional) Transfer mode

INT 
CmdReadFile (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	FILE	*wfile = NULL;
	PCHAR	filebuf;
	INT		TotalSector;
	UINT64	Pos;
	INT		retval;
	INT		SectorCount;
	SOCKET	connsock;
	UINT32	UserId;
	INT		i;
	INT		dev;

	TotalSector =  (INT)_strtoi64(arg[1], NULL, 0);

	if (TotalSector ==0) {

		printf("Sectors to read should not be 0\n");	
		goto errout;
	}

	Pos = _strtoi64( arg[2], NULL, 0 );

	if (arg[4] && arg[4][0]) {

		SectorCount = (INT) _strtoi64(arg[4], NULL, 0);

	} else {

		SectorCount = 1;
	}

	filebuf = (CHAR*)malloc(SectorCount * 512);
	wfile = fopen(arg[0], "wb");

	if (wfile == 0) {
	
		printf("Failed to open file %s\n", arg[0]);
		goto errout;
	}

	if (arg[3] == NULL) {

		dev = 0;

	} else {

		dev = (INT) _strtoi64(arg[3], NULL, 0);

		if (dev!=0 && dev!=1) {

			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev == 0) {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	} else {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {

		return -1;
	}

	if (arg[5] && arg[5][0]) {
	
		SetTransferMode(connsock, iTargetID, arg[5]);

	} else {

		if ((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {
		
			fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
			return retval;
		}
	}

	printf( "Sector to read: %3d Pos: %I64d, Sectors per operation: %d\n", TotalSector, Pos, SectorCount );

	for (i=0; i<TotalSector; i+=SectorCount) {

		retval = IdeCommand( connsock, (short)iTargetID, 0, WIN_READ, Pos + i, (short)SectorCount, 0, SectorCount * 512, (PCHAR)filebuf, 0, 0 );

		if (retval != 0) {

			fprintf(stderr, "\n[NdasCli]main: READ Failed... Sector %I64d\n", (Pos + i));
			goto errout;
		}

		fwrite( filebuf, SectorCount, 512, wfile );
	}

	DisconnectFromNdas(connsock, UserId);

errout:

	if (filebuf) {

		free(filebuf);
	}

	if (wfile) {

		fclose(wfile);
	}

	return retval;
}

// Arg[0]: File.
// Arg[1]: number of Iteration
// Arg[2]: Position to write in sector
// Arg[3]: (optional) Device number 
// Arg[4]: (optional) Sector count per operation. 0 for random value from 1~128
// Arg[5]: (optional) Transfer mode

INT 
CmdWriteFile (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	FILE	*wfile = NULL;
	PCHAR	filebuf;
	PCHAR	writebuf;
	INT		Ite;
	UINT64	Pos;
	INT		retval;
	INT		SectorCount;
	SOCKET	connsock;
	UINT32	UserId;
	INT		i;
	INT		dev;

	printf( "This command only reads first 512 bytes and repeat it!!\n" );

	Ite =  (INT)_strtoi64(arg[1], NULL, 0);

	Pos = _strtoi64(arg[2], NULL, 0);
	
	if (arg[4] && arg[4][0]) {
	
		SectorCount = (INT) _strtoi64( arg[4], NULL, 0 );

	} else {

		SectorCount = 128;
	}

	filebuf = (CHAR*)malloc(SectorCount * 512);
	writebuf = (CHAR*)malloc(SectorCount * 512);

	wfile = fopen(arg[0], "rb");

	if (wfile == 0) {

		printf( "Failed to open file %s\n", arg[0] );
		goto errout;
	}

	retval = (INT)fread( filebuf, 1, 512, wfile );

	if (retval != 512) {

		printf("Failed to read file\n");
		goto errout;
	}

	for (i=1; i<SectorCount; i++) {

		memcpy( filebuf+i*512, filebuf, 512 );
	}

	if (arg[3] == NULL) {

		dev = 0;

	} else {

		dev = (INT) _strtoi64(arg[3], NULL, 0);

		if (dev!=0 && dev!=1) {

			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev == 0) {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	} else {

		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) != 0) {
	
		return -1;
	}

	if (arg[5] && arg[5][0]) {

		SetTransferMode(connsock, iTargetID, arg[5]);

	} else {

		if ((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {

			fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
			return retval;
		}
	}

	printf( "Iteration: %3d Pos: %I64d, Sectors per operation: %d\n", Ite, Pos, SectorCount );

	for (i=0; i<Ite; i++) {

		memcpy( writebuf, filebuf, 512*SectorCount ); // buffer contents is changed after IdeCommand.

		retval = IdeCommand( connsock, 
							 (short)iTargetID, 
							 0, 
							 WIN_WRITE, 
							 (Pos * MB + i*512*SectorCount) / 512, 
							 (short)SectorCount, 
							 0, 
							 SectorCount * 512, 
							 (PCHAR)writebuf, 
							 0, 
							 0 );

		if (retval != 0) {
	
			fprintf( stderr, "\n[NdasCli]main: WRITE Failed... Sector %d\n", (Pos * MB + i*512*SectorCount) / 512 );
			goto errout;
		}
	}

	DisconnectFromNdas(connsock, UserId);

errout:

	if (filebuf) {

		free(filebuf);
	}

	return retval;
}

// arg[0]: User1 password. Optional

INT
CmdGetConfig (
	PCHAR target, 
	PCHAR arg[]
	)
{
	SOCKET		connsock;
	INT			iResult;
	INT			retVal = 0;
	
	INT			UserId;
	UINT32		Param0, Param1, Param2;

	UCHAR		Password[PASSWORD_LENGTH] = {0};
	UINT32		Val;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) != 0) {

		return -1;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
	
		NdasEmuDbgCall( 4, "Getting config is not supported in NDAS 1.0\n" );
		DisconnectFromNdas(connsock, UserId);

		return 0;
	}

	// Get Max ret time

	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);

	iResult = VendorCommand( connsock, VENDOR_OP_GET_MAX_RET_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Val = htonl(Param2);

	} else {

		Val = htonl(Param1);
	}

	NdasEmuDbgCall( 4, "Max retransmission time = %d msec\n", Val + 1 );

	// Get Max con time

	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);

	iResult = VendorCommand(connsock, VENDOR_OP_GET_MAX_CONN_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL);

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Val = htonl(Param2);

	} else {

		Val = htonl(Param1);
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {

		NdasEmuDbgCall( 4, "Connection timeout = %d sec\n", Val + 1);

	} else {

		NdasEmuDbgCall( 4, "Connection timeout = %d msec\n", Val + 1);
	}

	// Get heart-beat time

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);

		iResult = VendorCommand(connsock, VENDOR_OP_GET_HEART_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL);

		Val = htonl(Param2);

		NdasEmuDbgCall( 4, "Heartbeat time = %d sec\n", Val + 1);
	}

	// Get standby time

	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);

	iResult = VendorCommand(connsock, VENDOR_OP_GET_STANBY_TIMER, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL);

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Val = htonl(Param2);

	} else {

		Val = htonl(Param1);
	}

	if (Val & (0x1<<31)) {

		NdasEmuDbgCall( 4, " Standby time = %d min\n", (Val & 0x7fffffff)+1 );

	} else {

		NdasEmuDbgCall( 4, "Standby time = DISABLED\n");
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {

		// Get delay time

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		
		iResult = VendorCommand(connsock, VENDOR_OP_GET_DELAY, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL);
		
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		
			Val = htonl(Param2);
		
		} else {
		
			Val = htonl(Param1);
		}

		NdasEmuDbgCall( 4, "Inter-packet Delay time = %d nsec\n", (Val + 1) * 8);
	}

	// Get dynamic max ret time

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);

		iResult = VendorCommand(connsock, VENDOR_OP_GET_DYNAMIC_MAX_RET_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL);

		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

			Val = htonl(Param2);

		} else {

			Val = htonl(Param1);
		}

		NdasEmuDbgCall( 4, "Dynamic max-retransmission time = %d msec\n", Val + 1 );
	}

	// Get option

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);

		iResult = VendorCommand( connsock, VENDOR_OP_GET_D_OPT, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

		Val = htonl(Param2);

		NdasEmuDbgCall( 4, "Dynamic Option value=0x%02x\n", Val );
	}

	// Get deadlock time

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		
		iResult = VendorCommand( connsock, VENDOR_OP_GET_DEAD_LOCK_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );
		
		Val = htonl(Param2);
		
		NdasEmuDbgCall( 4, "Deadlock time= %d sec\n", Val+1 );
	}
	
	// Get watchdog time

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		
		iResult = VendorCommand( connsock, VENDOR_OP_GET_WATCHDOG_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );
		
		Val = htonl(Param2);
		
		if (Val ==0) {
		
			NdasEmuDbgCall( 4, "Watchdog time= DISABLED\n" );

		} else {

			NdasEmuDbgCall( 4, "Watchdog time= %d sec\n", Val+1 );
		}
	}

	DisconnectFromNdas(connsock, UserId);

	return 0;
}


INT 
CmdSetDefaultConfig (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	SOCKET		connsock;
	INT			iResult;
	INT			retVal = 0;
	
	INT			UserId;
	UCHAR		Password[PASSWORD_LENGTH] = {0};
	UINT32		Val;

	union {

		struct {
		
			UINT32	Param0;
			UINT32	Param1;
			UINT32	Param2;
		};

		UCHAR	Param8[12];
	};

	RtlZeroMemory( Param8, 12 );
	Param0 = 0; Param1 = 0; Param2 = 0;

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) != 0) {

		return -1;
	}

	// Set Max ret time

	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);
	
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
	
		Param2 = htonl(199);

	} else {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

			Param1 = htonl(199999); // In micro-second unit

		} else {

			Param1 = htonl(199);
		}
	}

	iResult = VendorCommand( connsock, VENDOR_OP_SET_MAX_RET_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {

		Val = htonl(Param2);

	} else {

		Val = htonl(Param1);
	}

	// Set Max con time

	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
	
		Param2 = htonl(4);

	} else {

		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_0) {

			Param1 = htonl(4);

		} else {

			if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {

				Param1 = htonl(4999999); // In micro-second unit

			} else {

				Param1 = htonl(4999);
			}
		}
	}

	iResult = VendorCommand( connsock, VENDOR_OP_SET_MAX_CONN_TIME, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

	// Set standby time

	if (ActiveHwVersion > LANSCSIIDE_VERSION_1_0) {
		
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		
			Param2 = htonl(30 | (1<<31));

		} else {

			Param1 = htonl( 30 | 1<<31 );
		}

		iResult = VendorCommand( connsock, VENDOR_OP_SET_STANBY_TIMER, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );
	}

	// Set delay time

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {

		
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		
			Param2 = htonl(7);

		} else {

			Param1 = htonl(7);
		}

		iResult = VendorCommand( connsock, VENDOR_OP_SET_DELAY, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );
	}

	DisconnectFromNdas( connsock, UserId );

	return 0;
}

// Arg[0]: User  PW. Optional

INT CmdGetOption (
	PCHAR	target, 
	PCHAR	arg[]
	)
{
	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	
	INT		UserId;
	UINT32	Param0, Param1, Param2;
	UCHAR	Password[PASSWORD_LENGTH] = {0};
	INT		i;


	UserId = MAKE_USER_ID( DEFAULT_USER_NUM, USER_PERMISSION_RO );

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) !=0) {

		goto errout;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
	
		// 2.5 version support get option

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);

		// Get d_opt will return static option except jumbo.
		
		iResult = VendorCommand( connsock, VENDOR_OP_GET_D_OPT, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );
		
		Param2 = htonl(Param2);
		NdasEmuDbgCall( 4, "Dynamic Option value=%02x\n", Param2 );
		
		for(i=5; i>=0; i--) {
		
			NdasEmuDbgCall( 4, "Option bit %d:", i);

			switch(i) {

				case 5:		NdasEmuDbgCall( 4, "No heart frame    ");	break;
				case 4:		NdasEmuDbgCall( 4, "Reserved          ");	break;
				case 3:		NdasEmuDbgCall( 4, "Header CRC        ");	break;
				case 2:		NdasEmuDbgCall( 4, "Data CRC          ");	break;
				case 1:		NdasEmuDbgCall( 4, "Header Encryption ");	break;
				case 0:		NdasEmuDbgCall( 4, "Data Encryption   ");	break;
			}

			NdasEmuDbgCall( 4, (Param2 &(1<<i))?"On    ":"Off   " );
			NdasEmuDbgCall( 4, "\n");
		}

	} else {

		// Get from login information

		NdasEmuDbgCall( 4, "Bit 1: Header Encryption :%s\n", HeaderEncryptAlgo ? "On":"Off" );
		NdasEmuDbgCall( 4, "Bit 0: Data Encryption   :%s\n", DataEncryptAlgo ? "On":"Off" );
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

// Arg[0]: Option
// Arg[1]: Superuser PW. Optional

INT 
CmdSetOption (
	PCHAR	target, 
	PCHAR	arg[]
	)
{

	SOCKET	connsock;
	INT		iResult;
	INT		retVal = 0;
	INT		i;	
	INT		UserId;
	UCHAR	Password[PASSWORD_LENGTH] = {0};
	INT		Option;
	INT		option_count;

	union {

		struct {

			UINT32	Param0;
			UINT32	Param1;
			UINT32	Param2;
		};

		UCHAR	Param8[12];
	};

	RtlZeroMemory( Param8, 12 );
	Param0 = 0; Param1 = 0; Param2 = 0;

	Option = (INT) _strtoi64(arg[0], NULL, 0);

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);
	
	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[1]) != 0) {

		goto errout;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		option_count = 5;

	} else {

		option_count = 2;

#ifndef BUILD_FOR_DIST
		NdasEmuDbgCall( 4, "Warning: currently supervisor password change does not work with header/data encryption\n" );
#endif
	}

	for (i = option_count-1 ; i>=0 ;i--) {

		NdasEmuDbgCall( 4, "Option %d:", i );
		
		switch (i) {
		
			case 5:		NdasEmuDbgCall( 4, "No heart frame    " );	break;
			case 4:		NdasEmuDbgCall( 4, "Reserved          " );	break;
			case 3:		NdasEmuDbgCall( 4, "Header CRC        " );	break;
			case 2:		NdasEmuDbgCall( 4, "Data CRC          " );	break;
			case 1:		NdasEmuDbgCall( 4, "Header Encryption " );	break;
			case 0:		NdasEmuDbgCall( 4, "Data Encryption   " );	break;
		}

		NdasEmuDbgCall( 4, (Option & (1<<i)) ? "    On":"    Off" );
		NdasEmuDbgCall( 4, "\n" );
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(Option);

	} else {

		Param0 = htonl(0);
		//Param1 = htonl(Option);
		Param2 = 0;

		Param8[7] = Option;
	}

	iResult = VendorCommand( connsock, VENDOR_OP_SET_OPT, NULL, &Param0, &Param1, &Param2, NULL, 0, NULL );

	if (iResult != 0) {

		NdasEmuDbgCall( 4, "Vendor command failed\n" );
		goto errout;
	}

	DisconnectFromNdas( connsock, UserId );

errout:

	return 0;
}

// WriteSize, Pos is in MB
// SectorCount: If 0, random sector count
// LockedWriteMode: ORed value of following.
//		0 - no use
//		Bit 1,2,3: 0x1 - Use vendor cmd to unlock, 0x2 - Use PDU flag to unlock, 0x4 - Use mutex 0
//		Bit 4,5: 0x00 - Unlock after every write, 0x10 - Unlock after all write. 
//				 0x20 - unlock if another host is queued write-request.

INT 
RwPatternChecking (
	SOCKET	connsock, 
	INT		WriteSize, 
	INT		Ite, 
	UINT64	Pos, 
	INT		SectorCount, 
	INT		LockedWriteMode
	)
{
	PUCHAR	data = NULL;
	INT		iResult;
	INT		i;
	INT		retval = 0;
	INT		CurrentPattern;
	INT		Blocks = SectorCount;
	clock_t start_time, end_time;

	srand( (unsigned)GetTickCount() );

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	NdasEmuDbgCall( 4, "IO position=%dMB, IO Size=%dMB, Iteration=%d, ", (INT)Pos, WriteSize, Ite );

	if (LockedWriteMode) {
	
		NdasEmuDbgCall( 4, "\n\tUse buffer-lock in ");

		if (LockedWriteMode & 0x1) {

			NdasEmuDbgCall( 4, "vendor-cmd unlock mode, ");

		} else if (LockedWriteMode & 0x2) {

			NdasEmuDbgCall( 4, "PDU-flag unlock mode, ");

		} else if (LockedWriteMode & 0x4) {

			NdasEmuDbgCall( 4, "Mutex-0 lock mode, ");

		} else {

			NdasEmuDbgCall( 4, "Invalid unlock mode\n");
			goto errout;
		}

		if (LockedWriteMode & 0x10) {

			NdasEmuDbgCall( 4, "one lock for all writing\n" );

		} else if (LockedWriteMode & 0x20) {

			NdasEmuDbgCall( 4, "unlock if another writing is pending\n" );

		} else {

			NdasEmuDbgCall( 4, "each lock for every writing\n" );
		}

	} else {

		NdasEmuDbgCall( 4, "No buffer-lock\n" );
	}

	for (i=0;i<Ite;i++) {

		CurrentPattern = rand() % GetNumberOfPattern();

		if (SectorCount == 0) {

			Blocks = (rand() % 128) + 1;
		}

		NdasEmuDbgCall( 4, "Iteration %d - BlockSize %d, Pattern#%d, \n", i+1, Blocks, CurrentPattern );
		NdasEmuDbgCall( 4, "  Writing %dMB:", WriteSize );

		start_time = clock();

		iResult = WritePattern( connsock, CurrentPattern, Pos, WriteSize, Blocks, LockedWriteMode );
		
		if (iResult !=0) {
		
			NdasEmuDbgCall( 4, "Writing pattern failed.\n" );
			goto errout;
		}

		end_time = clock();
		NdasEmuDbgCall( 4, "%.1f MB/sec\n", 1.*WriteSize*CLK_TCK/(end_time-start_time) );

		NdasEmuDbgCall( 4, "  Reading %dMB:", WriteSize );

		start_time = clock();

		iResult = ReadPattern( connsock, CurrentPattern, Pos, WriteSize, Blocks );
		
		if (iResult !=0) {

			goto errout;
		}

		end_time = clock();
		
		NdasEmuDbgCall( 4, "%.1f MB/sec\n", 1.*WriteSize*CLK_TCK/(end_time-start_time) );
	}

	NdasEmuDbgCall( 4, "Success!!!!!!!!!!!!!\n" );

errout:

	if (data) {

		free(data);
	}

	return retval;
}

INT 
ReadPattern (
	SOCKET	connsock, 
	INT		Pattern, 
	UINT64	Pos, 
	INT		IoSize, 
	INT		Blocks
	)
{
	PUCHAR	data = NULL;
	INT64	j;
	INT		retval = 0;
	INT		TransferBlock;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	for (j = 0; j < IoSize * MB;) {

		if (Blocks == 0) {
		
			TransferBlock = (rand() % 128) + 1;

		} else {

			TransferBlock = Blocks;
		}

		if ((j + TransferBlock * 512) > IoSize * MB) {

			TransferBlock = (INT)(IoSize * MB - j)/512;
		}

		retval = IdeCommand( connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0 );
		
		if (retval == -2) { // CRC error. try one more time.
		
			NdasEmuDbgCall( 4, "CRC errored. Retrying\n" );

			retval = IdeCommand( connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0 );
		}

		if (retval != 0) {
		
			NdasEmuDbgCall( 4, "[NdasCli]main: READ Failed... Sector %d\n", (Pos * MB + j) / 512 );
			goto errout;
		}

		if (CheckPattern(Pattern, (INT)j, data, TransferBlock * 512) == FALSE) {
		
			retval = -1;
			goto errout;
		}

		j+= TransferBlock * 512;
	}

errout:

	if (data) {

		free(data);
	}

	return retval;
}

// Pos and Size is in MB
// Blocks: number of blocks to write per one ide command

INT
WritePattern (
	SOCKET	connsock, 
	INT		Pattern, 
	UINT64	Pos, 
	INT		IoSize, 
	INT		Blocks, 
	INT		LockedWriteMode
	)
{
	PUCHAR	data = NULL;
	INT		j;
	INT		retry = 0;
	INT		retval = 0;
	//INT	MAX_REQ = Blocks * 512;
	BOOL	Locked;
	UINT32	WriteLockCount;

	// Lock modes

	BOOL	bLockedWrite = FALSE;
	BOOL	bVendorUnlock = FALSE;
	BOOL	bMutexLock = FALSE;
	BOOL	bPduUnlock = FALSE;
	BOOL	bPerWriteUnlock = FALSE;
	BOOL	bYieldedUnlock = FALSE;

	INT		TransferBlock;
	INT		TransferSize;
	UINT64	PosByte;
	INT		IoSizeByte;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	// data[MAX_DATA_BUFFER_SIZE-1] = 0;

	if (LockedWriteMode) {
	
		bLockedWrite = TRUE;

		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {

			if (LockedWriteMode & 0x1) {

				bVendorUnlock = TRUE;

			} else if (LockedWriteMode & 0x2) {

				bPduUnlock = TRUE;

			} else {

				NdasEmuDbgCall( 4, "Invalid unlock mode\n" );
				goto errout;
			}

			if (LockedWriteMode & 0x10) {

				//

			} else if (LockedWriteMode & 0x20) {

				bYieldedUnlock = TRUE;

			} else {

				bPerWriteUnlock = TRUE;
			}

		} else if (ActiveHwVersion == LANSCSIIDE_VERSION_2_0 || ActiveHwVersion == LANSCSIIDE_VERSION_1_1) {

			if (LockedWriteMode & 0x4) {

				bMutexLock = TRUE;
	
			} else {

				NdasEmuDbgCall( 4, "Unsupported lock mode\n" );
				goto errout;
			}

		} else {

			NdasEmuDbgCall( 4, "Lock is not supported\n" );
			goto errout;
		}
	}

	Locked =FALSE;
	IoSizeByte = IoSize * MB;
	PosByte = Pos * MB;

	for (j = 0; j < IoSizeByte;) {

		if (Blocks == 0) {

			TransferBlock = (rand() % 128) + 1;

		} else {

			TransferBlock = Blocks;
		}

		if ((j + TransferBlock * 512) > IoSizeByte) {

			TransferBlock = (INT)(IoSizeByte - j)/512;
		}

		TransferSize = TransferBlock * 512;

		FillPattern(Pattern, j, data, TransferSize);
		
		if (bLockedWrite == TRUE && Locked == FALSE) {
		
			if (bMutexLock) {

				// mutex lock mode. 

				retry = 0;

				do {

					UINT32 Param0 = 0;
					((PBYTE)&Param0)[3] = (BYTE)0;
					retval = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, NULL, &Param0, NULL, NULL, NULL, 0, NULL);

					if (retval ==0) {

						break;
					}

					if (retval == LANSCSI_RESPONSE_T_SET_SEMA_FAIL) {
					
						NdasEmuDbgCall( 4, "Vendor command VENDOR_OP_SET_MUTEX failed %x: retry = %d\n", retval, retry );
						retry ++;
						Sleep(100);
						continue;
					}

					NdasEmuDbgCall( 4, "Vendor command failed %x\n", retval );
					goto errout;

				} while(1);

			} else {

				retval = VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, NULL, 0, 0, 0, NULL, 0, NULL);

				if (retval !=0) {

					NdasEmuDbgCall( 4, "Failed to get write lock\n" );
					goto errout;
				}
			}

			NdasEmuDbgCall( 4, "LOCK ACQUIRED !!!!\n" );

			Locked = TRUE;
		}

		if (bLockedWrite == TRUE && bPduUnlock == TRUE && bPerWriteUnlock ==TRUE && 
			(bPerWriteUnlock || (j + TransferSize == IoSizeByte))) {

			// Set free lock flag

			retval = IdeCommand( connsock, 
								 iTargetID, 
								 0, 
								 WIN_WRITE, 
								 (PosByte + j) / 512, 
								 (short)TransferBlock, 
								 0, 
								 MAX_DATA_BUFFER_SIZE, 
								 (PCHAR)data, 
								 IDECMD_OPT_UNLOCK_BUFFER_LOCK, 
								 &WriteLockCount );
			
			if (retval == -2) { // CRC error. try one more time.
			
				FillPattern( Pattern, (INT)j, data, TransferSize );

				NdasEmuDbgCall( 4, "CRC errored. Retrying\n" );
				
				retval = IdeCommand( connsock, 
									 iTargetID, 
									 0, 
									 WIN_WRITE, 
									 (PosByte + j) / 512, 
									 (short)TransferBlock, 
									 0, 
									 MAX_DATA_BUFFER_SIZE, 
									 (PCHAR)data, 
									 IDECMD_OPT_UNLOCK_BUFFER_LOCK, 
									 &WriteLockCount );
			}

			Locked = FALSE;

		} else {

			retval = IdeCommand( connsock, 
								 iTargetID, 
								 0, 
								 WIN_WRITE, 
								 (PosByte + j) / 512, 
								 (short)TransferBlock, 
								 0,
								 MAX_DATA_BUFFER_SIZE, 
								 (PCHAR)data, 
								 0, 
								 &WriteLockCount );

			if (retval == -2) { // CRC error. try one more time.

				FillPattern( Pattern, (INT)j, data, TransferSize );

				NdasEmuDbgCall( 4, "CRC errored. Retrying\n" );
				
				retval = IdeCommand( connsock, 
									 iTargetID, 
									 0, 
									 WIN_WRITE, 
									 (PosByte + j) / 512, 
									 (short)TransferBlock, 
									 0, 
									 MAX_DATA_BUFFER_SIZE, 
									 (PCHAR)data, 
									 0, 
									 &WriteLockCount );
			}
		}

		if (retval !=0) {

			NdasEmuDbgCall( 4, "[NdasCli]main: WRITE Failed... Sector %d\n", (PosByte + j) / 512 );
			goto errout;
		}
		
		if (Locked == TRUE && bYieldedUnlock == TRUE && WriteLockCount >1) {

			retval = VendorCommand( connsock, VENDOR_OP_FREE_WRITE_LOCK, NULL, 0, 0, 0, NULL, 0, NULL );

			if (retval !=0) {

				NdasEmuDbgCall( 4, "Failed to free write lock\n" );
				goto errout;
			}

			Locked = FALSE;

		} else if (Locked == TRUE && bVendorUnlock == TRUE && bPerWriteUnlock == TRUE) {

			retval = VendorCommand( connsock, VENDOR_OP_FREE_WRITE_LOCK, NULL, 0, 0, 0, NULL, 0, NULL );

			if (retval !=0) {

				NdasEmuDbgCall( 4, "Failed to free write lock\n" );
				goto errout;
			}

			Locked = FALSE;

		} else if (Locked == TRUE && bMutexLock == TRUE && bPerWriteUnlock == TRUE) {

			// mutex lock mode

			UINT32 Param0 = 0;

			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand( connsock, VENDOR_OP_FREE_MUTEX, NULL, &Param0, NULL, NULL, NULL, 0, NULL );

			if (retval !=0)  {

				NdasEmuDbgCall( 4, "Free mutex failed\n" );
				goto errout;
			}				
		}

		j+= TransferSize;
	}

	if (bLockedWrite == TRUE && Locked) {

		if (bMutexLock) {

			UINT32 Param0 = 0;
			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand( connsock, VENDOR_OP_FREE_MUTEX, NULL, &Param0, NULL, NULL, NULL, 0, NULL );

			if (retval !=0) {

				NdasEmuDbgCall( 4, "Free mutex failed\n" );
			}

		} else {
			
			retval = VendorCommand( connsock, VENDOR_OP_FREE_WRITE_LOCK, NULL, 0, 0, 0, NULL, 0, NULL );
			
			if (retval !=0) {
			
				NdasEmuDbgCall( 4, "Failed to free write lock\n" );
				goto errout;
			}
		}

		Locked = FALSE;
	}

	retval = 0;

errout:

	if (data) {

		free(data);
	}

	return retval;
}

INT SetTransferMode (
	SOCKET	connsock, 
	INT		TargetId, 
	PCHAR	Mode
	)
{
	struct hd_driveid	info;
	INT					iResult;
	INT					mode;
	
	// identify

	if ((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
	
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}

	if (info.field_valid & 0x0002) {

		printf("EDIDE field is valid.\n");

	} else {

		printf("EDIDE field is not valid.\n");
	}

	printf( "Current setting: Supported PIO 0x%x, DMA 0x%x, U-DMA 0x%x\n", 
			 info.eide_pio_modes, info.dma_mword, info.dma_ultra );

	printf( "Changing mode from " );

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

	printf("to %s\n", Mode);

	mode = (int) _strtoi64(&Mode[1], NULL, 0);

	if (Mode[0] == 'p' || Mode[0] == 'P') {

		if (mode > 4) {

			printf("Invalid mode\n");
			return -1;
		}

		if (mode >= 3) {
		
			if ((info.eide_pio_modes & (0x1<<(mode-3))) == 0) {

				printf("Unsupported mode\n");
				return -1;
			}
		}

		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, mode | 0x8, SETFEATURES_XFER, 0, NULL, 0,0 );

		PerTarget[TargetId].bPIO = TRUE;

	} else if (Mode[0] == 'd' || Mode[0] == 'D') {

		if (mode > 2) {

			printf("Invalid mode\n");
			return -1;
		}

		if ((info.dma_mword & (1<<mode)) == 0) {

			printf("Unsupported mode\n");
			return -1;
		}

		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0x20 | mode , SETFEATURES_XFER, 0, NULL, 0,0 );
		PerTarget[TargetId].bPIO = FALSE;

	} else if (Mode[0] == 'u' || Mode[0] == 'U') {

		if (mode > 7) {

			printf("Invalid mode\n");
			return -1;
		}

		if ((info.dma_ultra & (1<<mode)) == 0) {
		
			printf("Unsupported mode\n");
			return -1;
		}

		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0x40 | mode , SETFEATURES_XFER, 0, NULL, 0,0 );

		PerTarget[TargetId].bPIO = FALSE;

	} else {

		printf( "Unknown mode\n" );
		return -1;
	}

	if (iResult !=0 ) {
	
		printf("Failed to set feature\n");
		return -1;
	}

	if ((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {

		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}

	printf( "PIO W/O IORDY 0x%x, PIO W/ IORDY 0x%x\n", info.eide_pio, info.eide_pio_iordy );

	printf( "DMA 0x%x, U-DMA 0x%x\n", info.dma_mword, info.dma_ultra );
	
	if (info.capability &= 0x02) {

		PerTarget[TargetId].bLBA = TRUE;
	
	} else {

		PerTarget[TargetId].bLBA = FALSE;
	}

	// Calc Capacity.
 
	if (info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400) {	// Support LBA48bit
	
		PerTarget[TargetId].bLBA48 = TRUE;
		PerTarget[TargetId].SectorCount = info.lba_capacity_2;

	} else {

		PerTarget[TargetId].bLBA48 = FALSE;
		PerTarget[TargetId].SectorCount = info.lba_capacity;	
	}

	return 0;	
}

