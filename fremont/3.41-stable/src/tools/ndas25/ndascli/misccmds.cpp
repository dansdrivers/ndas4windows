#include "stdafx.h"
#include "..\inc\rijndael-api-fst.h"
#include <iostream>
#include <set>

//#define PSDK // Some functions are build only with Platform SDK

extern int ActiveHwVersion; // set at login time
extern UINT16	HeaderEncryptAlgo;
extern UINT16	DataEncryptAlgo;
extern int		iTargetID;
extern unsigned _int64	iPassword_v1;
extern unsigned _int64	iSuperPassword_v1;

int SetTransferMode(SOCKET connsock, int TargetId, char* Mode);

void ReverseBytes(PUCHAR Buf, int len)
{
	int i;
	UCHAR Temp;
	for(i=0;i<len/2;i++) {
		Temp = Buf[i];
		Buf[i] = Buf[len-i-1];
		Buf[len-i-1] = Temp;
	}
}

void PrintPassword(PUCHAR Password)
{
	int i;
	BOOL printable = TRUE;
	for (i=0;i<PASSWORD_LENGTH;i++) {
		printf("%02x", Password[i]);
	}
	for (i=0;i<PASSWORD_LENGTH;i++) {
		if (!isgraph(Password[i]) && Password[i]!=0) 
			printable = FALSE;
	}
	if (printable) {
		printf("(");
		for (i=0;i<PASSWORD_LENGTH;i++) {
			if (Password[i]==0) {
				break;
			} else {
				printf("%c", Password[i]);
			}
		}
		printf(")");
	}
}

int ConnectToNdas(SOCKET* connsock, char* target, UINT32 UserId, PUCHAR Password)
{
	int retval = 0;
	LPX_ADDRESS			address;
	BOOL	Ret;
	UCHAR LocalPassword[PASSWORD_LENGTH] = {0};

	Ret = lpx_addr(target, &address);
	if (!Ret) {
		printf("Invalid address\n");
		return -1;
	}

	if(MakeConnection(&address, connsock) == FALSE) {
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
		strncpy((char*)LocalPassword, (char*)Password, PASSWORD_LENGTH);
	}

	if((retval = Login(*connsock, LOGIN_TYPE_NORMAL, UserId, LocalPassword, FALSE)) != 0) {
		fprintf(stderr, "[NdasCli]main: Login Failed...\n");
		return -1;
	}

	return 0;
}

int DisconnectFromNdas(SOCKET connsock, UINT32 UserId)
{
	if (IS_SUPERVISOR(UserId)) {
		unsigned int param = 0;
		VendorCommand(connsock, VENDOR_OP_RESET, &param, &param,&param, NULL, 0, NULL);
//		fprintf(stderr, "Press reset button and press enter");
//		getchar();
//		fprintf(stderr, "Resetting\n");
	} else {
		if(Logout(connsock) != 0) {
			fprintf(stderr, "Logout failed\n");
			return -1;
		}
//		fprintf(stderr, "Logout..\n");
	}
	closesocket(connsock);
	return 0;
}

int 
EncryptAesFst128(
		   unsigned char	*pData,
		   unsigned _int32	uiDataLength,
		   unsigned char	*pKey,
		   unsigned char	*pPassword
) {
#ifdef PSDK
	int ret;
	cipherInstance ci;
	keyInstance ki;
	char keybinary[16];
	int i;
	// Stage1: Create cipher instance
	ret = cipherInit(&ci, MODE_ECB, NULL);
	if(ret != TRUE) {
		printf("Failed to init cipher\n");
		return 0;
	}

	for(i=0;i<16;i++) {
		keybinary[i] = pKey[i] ^ pPassword[i]; // ???
	}

	// Stage2: Create key instance
	ret = makeKeyEncDec((keyInstance *)&ki, 128, keybinary);
	if(ret != TRUE) {
		printf("Failed to create key\n");
		return 0;
	}

	// Stage3: Encypt
	ki.direction = DIR_ENCRYPT;
	ret = blockEncrypt(
					&ci,
					&ki,
					pData,				// Input buffer
					uiDataLength<<3,		// bits
					pData				// output buffer
				);
	return 0;
#else
	printf("PSDK is not defined.\n");
	return 0;
#endif
}

int CmdAesLibTest(char* target, char* arg[])
{
	int TestDataLength = 16;
	unsigned char* SrcData;
	unsigned char* Crypt128Result;
	unsigned char* AesFst128Result;
	unsigned int chap_c[4] = { 0x3ef33342, 0x1fbc1fbc, 0x2093ea93, 0x6411529c};
	unsigned int chap_c_be[4];
	unsigned char* password = def_password0; // 16 bytes
	unsigned int password_be[4];
	
	int i;

	SrcData = (unsigned char*)malloc(TestDataLength);
	Crypt128Result = (unsigned char*)malloc(TestDataLength);
	AesFst128Result = (unsigned char*)malloc(TestDataLength);
	memset(Crypt128Result, 0, TestDataLength);
	memset(AesFst128Result, 0, TestDataLength);	
	for(i=0;i<TestDataLength;i++) {
		SrcData[i] = (unsigned char)(i & 0x0ff);
	}

	// Encrypt128Result
	memcpy(Crypt128Result, SrcData, TestDataLength);
	Encrypt128(Crypt128Result, TestDataLength, (unsigned char*) chap_c, password);

	// EncryptAesFst128
	memcpy(AesFst128Result, SrcData, TestDataLength);
	for(i=0;i<4;i++) {
		chap_c_be[i] = HTONL(chap_c[i]);
		password_be[i] = HTONL(((unsigned int*)password)[i]);
	}
	EncryptAesFst128(AesFst128Result, TestDataLength, (unsigned char*) chap_c_be, (unsigned char*)password_be);

	if (memcmp(Crypt128Result, AesFst128Result, TestDataLength) !=0) {
		printf("Result mismatch\n");
		printf("HASH128 - \n");
		PrintHex(Crypt128Result, TestDataLength);
		printf("\nAES128 - \n");
		PrintHex(AesFst128Result, TestDataLength);
	}
	
	free(SrcData);
	free(Crypt128Result);
	free(AesFst128Result);
	return 0;
}

// Arg[0]: user id(High 16 bit) + permission(Low 16 bit).
// Arg[1]: Address. Should be a multiple of 16
// Arg[2]: Length.
// Arg[3]: File name to write. File format is binary.
int SetEep(char* target, char* arg[], BOOL UserMode)
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	BOOL	Ret;
	char FileBuf[1024 + 16] = {0};
	int UserId;
//	UCHAR VendorOp;
	UINT32 Param0, Param1, Param2;
	FILE *eeprom = NULL;
	int address;
	int length;
	UCHAR Operation;

	UserId = (int) _strtoi64(arg[0], NULL, 0);

	address = (int) _strtoi64(arg[1], NULL, 0);
	length = (int) _strtoi64(arg[2], NULL, 0);

	if (address %16 !=0) {
		printf("Address should be multiple of 16\n");
		return -1;
	}

	if (UserMode && address < 1024) {
		printf("WARNING address should be more than 1024 in usermode eep operation\n");
	}

	if (length %16 !=0) {
		printf("Length should be multiple of 16\n");
		return -1;
	}

	eeprom = fopen(arg[3], "rb");
	if (eeprom == 0) {
		printf("Failed to open ROM file %s\n", arg[3]);
		goto errout;
	}
	iResult = (int)fread(FileBuf, 1, length, eeprom);
	if (iResult != length) {
		printf("Failed to read file\n");
		goto errout;
	}

	Ret = ConnectToNdas(&connsock, target, UserId, NULL);

	if (Ret) {
		return -1;
	}

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		if (UserMode) {
			printf("SET_U_EEP:");
			Operation = VENDOR_OP_U_SET_EEP;
		} else {
			printf("SET_EEP:");
			Operation = VENDOR_OP_SET_EEP;
		}
		printf(" UserId %x, Address %08x, Length %08x\n", 
			UserId, address, length);

		Param0 = htonl(0); // not used
		Param1 = htonl(address);
		Param2 = htonl(length);
		iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, NULL, 0, FileBuf);
	} else {
		printf("Running 2.0G mode seteep\n");
		if (length ==16) {
			CHAR AhsBuf[12];
			Operation = VENDOR_OP_SET_EEP;
			printf(" UserId %x, Address %08x, Length %08x\n", 
				UserId, address, length);
			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			
			memcpy(&Param2, FileBuf, 4);
			memcpy(AhsBuf, &FileBuf[4], 12);
			iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, AhsBuf, 12, NULL);
		} else {
			printf("Length should be 16 in 2.0G SETEEP\n");
			iResult = -1;
		}
	}

	if (iResult < 0) {
		printf("Failed to run vendor command\n");
	} else {
		printf("Result %0x\n", iResult);
	}
	DisconnectFromNdas(connsock, UserId);

errout:
	if (eeprom)
		fclose(eeprom);
	return 0;
}

int CmdSetEep(char* target, char* arg[])
{
	return SetEep(target, arg, FALSE);
}

int CmdSetUEep(char* target, char* arg[])
{
	return SetEep(target, arg, TRUE);
}

// Arg[0]: user id(High 16 bit) + permission(Low 16 bit).
// Arg[1]: Address. Should be a multiple of 16
// Arg[2]: Length.
int GetEep(char* target, char* arg[], BOOL UserMode)
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	char FileBuf[1024+16] = {0};
	int UserId;
	UINT32 Param0, Param1, Param2;
	int address;
	int length;
	UCHAR Operation;
	UserId = (int) _strtoi64(arg[0], NULL, 0);

	address = (int) _strtoi64(arg[1], NULL, 0);
	length = (int) _strtoi64(arg[2], NULL, 0);
	if (address %16 !=0) {
		printf("Address should be multiple of 16\n");
		return -1;
	}

	if (UserMode && address <1024) {
		printf("WARNING address should be more than 1024 in usermode eep operation\n");
	}

	if (length % 16 !=0) {
		printf("Length should be multiple of 16\n");
		return -1;
	}

	printf(" UserId %x, Address %08x, Length %08x\n", 
		UserId, address, length);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		if (UserMode) {
			printf("GET_U_EEP:");
			Operation = VENDOR_OP_U_GET_EEP;
		} else {
			printf("GET_EEP:");
			Operation = VENDOR_OP_GET_EEP;
		}

		Param0 = htonl(0);	// not used
		Param1 = htonl(address);
		Param2 = htonl(length);
		iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, NULL, 0, FileBuf);
	} else {
		printf("Running 2.0G mode geteep\n");
		if (length ==16) {
			CHAR AhsBuf[12];
			Operation = VENDOR_OP_GET_EEP;
			printf(" UserId %x, Address %08x, Length %08x\n", 
				UserId, address, length);
			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, AhsBuf, 12, NULL);

			memcpy(FileBuf, &Param2, 4);
			memcpy(&FileBuf[4], AhsBuf, 12);
		} else {
			printf("Length should be 16 in 2.0G GETEEP\n");
			iResult = -1;
		}
	}

	if (iResult < 0) {
		printf("Failed to run vendor command\n");
	} else {
		printf("Result %0x\n", iResult);
		PrintHex((PUCHAR)FileBuf, length);
		printf("\n");
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

// No argument
int CmdDumpEep(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	char FileBuf[2048] = {0};
	int UserId;
	UINT32 Param0, Param1, Param2;
	int address;
	//int length;
	UCHAR Operation;
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		printf("Not implemented for NDAS 2.5\n");
#if 0
		if (UserMode) {
			printf("GET_U_EEP:");
			Operation = VENDOR_OP_U_GET_EEP;
		} else {
			printf("GET_EEP:");
			Operation = VENDOR_OP_GET_EEP;
		}

		Param0 = htonl(0);	// not used
		Param1 = htonl(address);
		Param2 = htonl(length);
		iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, NULL, 0, FileBuf);
#endif
	} else {
		printf("Running 2.0G mode geteep\n");
		for(address=0;address<2048;address+=16) {
			CHAR AhsBuf[12];
			CHAR buf[16];
			int i;
			Operation = VENDOR_OP_GET_EEP;
			printf(".");
			Param0 = htonl(0); // not used
			Param1 = htonl(address/16);
			Param0 = htonl(0); // not used.
			iResult = VendorCommand(connsock, Operation, &Param0, &Param1, &Param2, AhsBuf, 12, NULL);

			memcpy(buf, &Param2, 4);
			memcpy(&buf[4], AhsBuf, 12);
			// Swap and save to filebuf
			for(i=0;i<16;i++) {
				FileBuf[address+i] = buf[15-i];
			}
		}
		for(address=0;address<2048;address+=32) {
			int i;
			printf("\n%04x:", address);
			for(i=0;i<32;i++) {
				if (i%4 == 0) {
					printf(" ");
				}
				printf("%02x", (unsigned char)FileBuf[address+i]);
			}
		}
		printf("\n");
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

int CmdGetEep(char* target, char* arg[])
{
	return GetEep(target, arg, FALSE);
}
int CmdGetUEep(char* target, char* arg[])
{
	return GetEep(target, arg, TRUE);
}


// Arg[0]: user id(High 16 bit) + permission(Low 16 bit).
// Arg[1]: Vendor OP code
// Arg[2]: password. Use 0 for default
// Arg[3]: Param0(Bit 95-64)
// Arg[4]: Param1(Bit 63-32)
// Arg[5]: Param2(Bit 31-0)
int CmdRawVendorCommand(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UCHAR VendorOp;
	UINT32 Param0, Param1, Param2;
	
	UserId = (int) _strtoi64(arg[0], NULL, 0);

	VendorOp = (UCHAR) _strtoi64(arg[1], NULL, 0);
	if (VendorOp == 0) {
		printf("Invalid vendor op %s\n", arg[0]);
		return 0;
	}

	if (arg[2]!=NULL) {
		if (arg[2][0] == '0') {
			arg[2] = NULL;
		}
	}

	if (arg[3] != NULL)
		Param0 = (int) _strtoi64(arg[3], NULL, 0);
	else
		Param0 = 0;
	if (arg[4] !=NULL)
		Param1 = (int) _strtoi64(arg[4], NULL, 0);
	else
		Param1 = 0;
	if (arg[5] !=NULL)
		Param2 = (int) _strtoi64(arg[5], NULL, 0);
	else
		Param2 = 0;

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[2]) !=0)
		goto errout;


	printf("VendorCommand: UserId %x, Operation %x, Param %08x:%08x:%08x\n", 
		UserId, VendorOp, Param0, Param1, Param2);
	Param0 = htonl(Param0);
	Param1 = htonl(Param1);
	Param2 = htonl(Param2);
	iResult = VendorCommand(connsock, VendorOp, &Param0, &Param1, &Param2, NULL, 0, NULL);
	Param0 = ntohl(Param0);
	Param1 = ntohl(Param1);
	Param2 = ntohl(Param2);

	if (iResult == 0) {
		printf("Result %08x:%08x:%08x\n", Param0, Param1, Param2);
	} else if (iResult < 0) {
		printf("Failed to run vendor command\n");
	} else {
		printf("Result failed(%0x) - %08x:%08x:%08x\n", iResult, Param0, Param1, Param2);
	}

	DisconnectFromNdas(connsock, UserId);

errout:

	return 0;
}

//
// No parameter
//
int CmdTestVendorCommand0(char* target, char* arg[])
{
	printf("Not implemented\n");
	return 0;
}

//
// Arg[0]: Superuser password. Optional
//
int CmdShowAccount(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH + 16] = {0}; 
	int idperm;
	int i;
	int perm;
	
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) !=0)
		goto errout;
	for(i=0;i<8;i++) {
		idperm = MAKE_USER_ID(i,0);
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(idperm);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_USER_PERMISSION, &Param0, &Param1, &Param2, NULL, 0, NULL);
		idperm = ntohl(Param2);
		if (iResult == 0) {
			perm = USER_PERM_FROM_USER_ID(idperm);
			printf("#%d:[%s%s%s] ", i,
				(idperm & USER_PERMISSION_EW)?"E":" ",
				(idperm & USER_PERMISSION_SW)?"W":" ",
				(idperm & USER_PERMISSION_RO)?"R":" "
			);
		} else {
			printf("Failed to get permission\n");
			goto errout;
		}
		
		Param0 = htonl(0);
		Param1 = htonl(0x30 + 16 * i);
		Param2 = htonl(16);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_EEP, &Param0, &Param1, &Param2, NULL, 0, (PCHAR)Password);
		if (iResult == 0) {
			ReverseBytes(Password, PASSWORD_LENGTH);
			PrintPassword(Password);
			printf("\n");
		} else {
			printf("Failed to get permission\n");
			goto errout;
		}		
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

// Arg[0]: UserId+Permission:0,1,2,4
// Arg[1]: Superuser PW. Optional
int CmdSetPermission(char* target, char* arg[])
{

	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	int idperm;

	if (arg[0]) {
		idperm = (int) _strtoi64(arg[0], NULL, 0);
	} else {
		printf("User ID + Permission is not give\n");
		return 0;
	}

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, Password) !=0)
		goto errout;


	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(idperm);
	iResult = VendorCommand(connsock, VENDOR_OP_SET_USER_PERMISSION, &Param0, &Param1, &Param2, NULL, 0, NULL);

	
	DisconnectFromNdas(connsock, UserId);

errout:
	return 0;
}

typedef struct _LPX_DGRAM_HEADER {
	UINT16 Signature; // 0xffd6. Big endian.
	UINT16 Reserved1;
	UCHAR MsgVer; // 0x01
	UCHAR Reserved2;
	UCHAR Flags;
	UCHAR Reserved3;
	UCHAR OpCode;
	UCHAR Reserved4;
	UINT16 Length;	// Size Including this 
	UINT32 Reserved5;
} LPX_DGRAM_HEADER;

typedef struct _HEARTBEAT_RAW_DATA_V2 {
	LPX_DGRAM_HEADER DgramHeader;
	struct {
		UCHAR DeviceType; // 0 for ASIC
		UCHAR Version; // HW version. 3 for NDAS 2.5
		UINT16 Revision; // Revision.
		UINT16 StreamListenPort; // For HW always 0x2710. For Emulator, emulator defined value.
		UINT16 Reserved;
	} PnpMessage;
} HEARTBEAT_RAW_DATA_V2, *PHEARTBEAT_RAW_DATA_V2;

#define PNP_REQ_SRC_PORT 0x7421

//
// Arg[0]: Host MAC address to send request.
//
int CmdPnpRequest(char* target, char* arg[])
{
	SOCKADDR_LPX slpx;
	int result;
	int broadcastPermission;
	LPX_DGRAM_HEADER PnpReq;
	HEARTBEAT_RAW_DATA_V2 PnPReply;
	int i = 0;
	SOCKET			sock;
	int addrlen;

	if (arg[0] == 0) {
		fprintf(stderr, "MAC address of Host is required.\n");
		return -1;
	}
	fprintf(stderr, "Sending PNP request(Ctrl-C when there is no response)\n");

	// Create Listen Socket.
	sock = socket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), _T("socket"));
		return -1;
	}

	broadcastPermission = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			(const char*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
		fprintf(stderr, "Can't setsockopt for broadcast: %d\n", errno);
		return -1;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family = AF_LPX;

	result = lpx_addr(arg[0], &slpx.LpxAddress);
	slpx.LpxAddress.Port = htons(PNP_REQ_SRC_PORT);

	result = bind(sock, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result < 0) {
		fprintf(stderr, "Error! when binding...: %d\n", WSAGetLastError());
		return -1;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family  = AF_LPX;
#if 1
    slpx.LpxAddress.Node[0] = 0xFF;
    slpx.LpxAddress.Node[1] = 0xFF;
    slpx.LpxAddress.Node[2] = 0xFF;
    slpx.LpxAddress.Node[3] = 0xFF;
    slpx.LpxAddress.Node[4] = 0xFF;
    slpx.LpxAddress.Node[5] = 0xFF;
#else
	{
		lpx_addr(target, &slpx.LpxAddress);
	}
#endif
	slpx.LpxAddress.Port = htons(10001);

	memset(&PnpReq, 0, sizeof(LPX_DGRAM_HEADER));

	PnpReq.Signature = htons(0xffd6);
	PnpReq.MsgVer = 0x01;
	PnpReq.OpCode = 0x02;
	PnpReq.Length = sizeof(LPX_DGRAM_HEADER);	// No payload

	result = sendto(sock, (const char*)&PnpReq, sizeof(PnpReq),
			0, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result < 0) {
		fprintf(stderr, "Can't send broadcast message: %d\n", WSAGetLastError());
		return -1;
	}

	// to do: Wait for reply
	addrlen = sizeof(slpx);
	result = recvfrom(sock, (char*)&PnPReply, sizeof(PnPReply),0,(struct sockaddr *)&slpx, &addrlen);
	if (result == sizeof(PnPReply)) {
		HEARTBEAT_RAW_DATA_V2 ExpectedReply = {0};
		printf("Received - ");
		PrintHex((UCHAR*)&PnPReply, sizeof(PnPReply));
		ExpectedReply.DgramHeader.Signature = htons(0xffd6);
		ExpectedReply.DgramHeader.MsgVer = 1;
		ExpectedReply.DgramHeader.OpCode = 0x82;
		ExpectedReply.DgramHeader.Length = htons(24);
		ExpectedReply.PnpMessage.DeviceType = 0;
		ExpectedReply.PnpMessage.Version = 3;
		ExpectedReply.PnpMessage.Revision = htons(0);
		ExpectedReply.PnpMessage.StreamListenPort = htons(10000);
		if (memcmp(&ExpectedReply, &PnPReply, sizeof(PnPReply)) != 0) {
			printf("\nExpected - ");
			PrintHex((UCHAR*)&ExpectedReply, sizeof(ExpectedReply));
			printf("\nUnexpected reply packet\n");
		} else {
			printf("\nExpected reply packet received\n");
		}
	} else {
		printf("Reply of invalid size or error %d\n", result);
	}

	closesocket(sock);
	return 0;
}

int ReadPattern(SOCKET connsock, int Pattern, UINT64 Pos, int IoSize, int Blocks)
{
	PUCHAR				data = NULL;
	INT64 j;
	int retval = 0;
	int TransferBlock;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	for (j = 0; j < IoSize * MB;) {
		if (Blocks == 0) {
			TransferBlock = (rand() % 128) + 1;
		} else {
			TransferBlock = Blocks;
		}
		if ((j + TransferBlock * 512) > IoSize * MB)
			TransferBlock = (int)(IoSize * MB - j)/512;

		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		if (retval == -2) { // CRC error. try one more time.
			fprintf(stderr, "CRC errored. Retrying\n");
			retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		}
		if(retval != 0) {
			fprintf(stderr, "[NdasCli]main: READ Failed... Sector %d\n", (Pos * MB + j) / 512);
			goto errout;
		}
		if (FALSE==CheckPattern(Pattern, (int)j, data, TransferBlock * 512)) {
			retval = -1;
			goto errout;
		}
		j+= TransferBlock * 512;
	}
errout:
	if (data)
		free(data);
	return retval;
}

//
// Pos and Size is in MB
// Blocks: number of blocks to write per one ide command
//
int WritePattern(SOCKET connsock, int Pattern, UINT64 Pos, int IoSize, int Blocks, int LockedWriteMode)
{
	PUCHAR				data = NULL;
	int j;
	int retry = 0;
	int retval = 0;
//	int MAX_REQ = Blocks * 512;
	BOOL Locked;
	UINT32 WriteLockCount;
	// Lock modes
	BOOL bLockedWrite = FALSE;
	BOOL bVendorUnlock = FALSE;
	BOOL bMutexLock = FALSE;
	BOOL bPduUnlock = FALSE;
	BOOL bPerWriteUnlock = FALSE;
	BOOL bYieldedUnlock = FALSE;
	int TransferBlock;
	int TransferSize;
	UINT64 PosByte;
	int IoSizeByte;
	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);
//	data[MAX_DATA_BUFFER_SIZE-1] = 0;

	if (LockedWriteMode) {
		bLockedWrite = TRUE;
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			if (LockedWriteMode & 0x1) {
				bVendorUnlock = TRUE;
			} else if (LockedWriteMode & 0x2) {
				bPduUnlock = TRUE;
			} else {
				fprintf(stderr, "Invalid unlock mode\n");
				goto errout;
			}
			if (LockedWriteMode & 0x10) {
				//
			} else if (LockedWriteMode & 0x20) {
				bYieldedUnlock = TRUE;
			} else {
				bPerWriteUnlock = TRUE;
			}
		} else if (ActiveHwVersion == LANSCSIIDE_VERSION_2_0 || ActiveHwVersion == LANSCSIIDE_VERSION_1_1){
			if (LockedWriteMode & 0x4) {
				bMutexLock = TRUE;
			} else {
				fprintf(stderr, "Unsupported lock mode\n");
				goto errout;
			}
		} else {
			fprintf(stderr, "Lock is not supported\n");
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
		if ((j + TransferBlock * 512) > IoSizeByte)
			TransferBlock = (int)(IoSizeByte - j)/512;
		TransferSize = TransferBlock * 512;

		FillPattern(Pattern, j, data, TransferSize);
		if (bLockedWrite == TRUE && Locked == FALSE) {
			if (bMutexLock) {
				// mutex lock mode. 

				retry = 0;
				do {
					UINT32 Param0 = 0;
					((PBYTE)&Param0)[3] = (BYTE)0;
					retval = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, &Param0, NULL, NULL, NULL, 0, NULL);
					if (retval ==0) 
						break;
					if (retval == LANSCSI_RESPONSE_T_SET_SEMA_FAIL) {
						fprintf(stderr, "Vendor command VENDOR_OP_SET_MUTEX failed %x: retry = %d\n", retval, retry);
						retry ++;
						Sleep(100);
						continue;
					}
					printf("Vendor command failed %x\n", retval);
					goto errout;
				} while(1);
			} else {
				retval = VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
				if (retval !=0) {
					fprintf(stderr, "Failed to get write lock\n");
					goto errout;
				}
			}
			fprintf(stderr, "LOCK ACQUIRED !!!!\n");

			Locked = TRUE;
		}

		if (bLockedWrite == TRUE && bPduUnlock == TRUE && bPerWriteUnlock ==TRUE && 
			(bPerWriteUnlock || (j + TransferSize == IoSizeByte))) {
			// Set free lock flag
			retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, IDECMD_OPT_UNLOCK_BUFFER_LOCK, &WriteLockCount);
			if (retval == -2) { // CRC error. try one more time.
				FillPattern(Pattern, (int)j, data, TransferSize);
				fprintf(stderr, "CRC errored. Retrying\n");
				retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, IDECMD_OPT_UNLOCK_BUFFER_LOCK, &WriteLockCount);
			}
			Locked = FALSE;
		} else {
			retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, &WriteLockCount);
			if (retval == -2) { // CRC error. try one more time.
				FillPattern(Pattern, (int)j, data, TransferSize);
				fprintf(stderr, "CRC errored. Retrying\n");
				retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)TransferBlock, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, &WriteLockCount);
			}
		}
		if (retval !=0) {
			fprintf(stderr, "[NdasCli]main: WRITE Failed... Sector %d\n", (PosByte + j) / 512);
			goto errout;
		}
		
		if (Locked == TRUE && bYieldedUnlock == TRUE && WriteLockCount >1) {
			retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to free write lock\n");
				goto errout;
			}
			Locked = FALSE;
		} else if (Locked == TRUE && bVendorUnlock == TRUE && bPerWriteUnlock == TRUE) {
			retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to free write lock\n");
				goto errout;
			}
			Locked = FALSE;
		} else if (Locked == TRUE && bMutexLock == TRUE && bPerWriteUnlock == TRUE) {
			// mutex lock mode. 
			UINT32 Param0 = 0;
			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, NULL, NULL, NULL, 0, NULL);
			if (retval !=0)  {
				printf("Free mutex failed\n");
				goto errout;
			}				
		}
		j+= TransferSize;
	}
	if (bLockedWrite == TRUE && Locked) {
		if (bMutexLock) {

			UINT32 Param0 = 0;
			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, NULL, NULL, NULL, 0, NULL);
			if (retval !=0) 
				printf("Free mutex failed\n");
		} else {
			retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to free write lock\n");
				goto errout;
			}
		}
		Locked = FALSE;
	}
	retval = 0;
errout:
	if (data)
		free(data);
	return retval;
}


// WriteSize, Pos is in MB
// SectorCount: If 0, random sector count
// LockedWriteMode: ORed value of following.
//		0 - no use
//		Bit 1,2,3: 0x1 - Use vendor cmd to unlock, 0x2 - Use PDU flag to unlock, 0x4 - Use mutex 0
//		Bit 4,5: 0x00 - Unlock after every write, 0x10 - Unlock after all write. 
//				 0x20 - unlock if another host is queued write-request.
int RwPatternChecking(SOCKET connsock, int WriteSize, int Ite, UINT64 Pos, int SectorCount, int LockedWriteMode)
{
	PUCHAR				data = NULL;
	int					iResult;
	int i;
	int retval = 0;
	int CurrentPattern;
	int Blocks = SectorCount;
	clock_t start_time, end_time;

	srand( (unsigned)GetTickCount() );

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	fprintf(stderr, "IO position=%dMB, IO Size=%dMB, Iteration=%d, ", (int)Pos, WriteSize, Ite);
	if (LockedWriteMode) {
		fprintf(stderr, "\n\tUse buffer-lock in ");
		if (LockedWriteMode & 0x1) {
			fprintf(stderr, "vendor-cmd unlock mode, ");
		} else if (LockedWriteMode & 0x2) {
			fprintf(stderr, "PDU-flag unlock mode, ");
		} else if (LockedWriteMode & 0x4) {
			fprintf(stderr, "Mutex-0 lock mode, ");
		} else {
			fprintf(stderr, "Invalid unlock mode\n");
			goto errout;
		}
		if (LockedWriteMode & 0x10) {
			fprintf(stderr, "one lock for all writing\n");
		} else if (LockedWriteMode & 0x20) {
			fprintf(stderr, "unlock if another writing is pending\n");
		} else {
			fprintf(stderr, "each lock for every writing\n");
		}
	} else {
		fprintf(stderr, "No buffer-lock\n");
	}
	for(i=0;i<Ite;i++) {
		CurrentPattern = rand() % GetNumberOfPattern();
		if (SectorCount ==0)
			Blocks = (rand() % 128) + 1;
		fprintf(stderr, "Iteration %d - BlockSize %d, Pattern#%d, \n", i+1, Blocks, CurrentPattern);
		fprintf(stderr, "  Writing %dMB:", WriteSize);
		start_time = clock();
		iResult = WritePattern(connsock, CurrentPattern, Pos, WriteSize, Blocks, LockedWriteMode);
		if (iResult !=0) {
			printf("Writing pattern failed.\n");
			goto errout;
		}
		end_time = clock();
		printf(" %.1f MB/sec\n", 1.*WriteSize*CLK_TCK/(end_time-start_time));

		fprintf(stderr, "  Reading %dMB:", WriteSize);
		start_time = clock();
		iResult = ReadPattern(connsock, CurrentPattern, Pos, WriteSize, Blocks);
		if (iResult !=0) {
			goto errout;
		}
		end_time = clock();
		printf(" %.1f MB/sec\n", 1.*WriteSize*CLK_TCK/(end_time-start_time));
	}

	printf("Success!!!!!!!!!!!!!\n");
errout:
	if (data)
		free(data);

	return retval;
}

// Arg[0]: Write size in MB per iteration
// Arg[1]: Number of iteration
// Arg[2]: Position to write in MB
// Arg[3]: Lock mode
// Arg[4]: UserId (Optional)
// Arg[5]: Block size (Optional)
int CmdLockedWrite(char* target, char* arg[])
{
	int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int LockMode;
	SOCKET connsock;
	int UserId;
	int Blocks;

	if (arg[3] == 0)  {
		printf("Not enough parameter\n");
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


	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	retval = RwPatternChecking(connsock, WriteSize, Ite, Pos, Blocks, LockMode);

	DisconnectFromNdas(connsock, UserId);
	return retval;
}


// Arg[0]: Time to wait in seconds.
int CmdBufferLockDeadlockTest(char* target, char* arg[])
{
	SOCKET				connsock;
	//UCHAR				data[512+16];
	unsigned			UserId;
	UINT32 WaitSec;
	int retval;
	UINT32 deadlocktime;
	UINT32 i;
	WaitSec = (UINT32) _strtoi64(arg[0], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	
	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

#if 0
	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, 0, FALSE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}
#endif
	retval = VendorCommand(connsock, VENDOR_OP_GET_DEAD_LOCK_TIME, 0, 0, &deadlocktime, NULL, 0, NULL);
	if (retval !=0) {
		fprintf(stderr, "Failed to get write lock\n");
		return retval;
	} else {
		deadlocktime = ntohl(deadlocktime) + 1;
		fprintf(stderr, "Deadlock time=%d\n", deadlocktime);
	}

	fprintf(stderr, "Trying to take buffer lock - ");
	retval = VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
	if (retval !=0) {
		fprintf(stderr, "Failed to get write lock\n");
		return retval;
	} else {
		fprintf(stderr, "Success\n");
	}
	fprintf(stderr, "Waiting %d seconds", WaitSec);

	for(i=0;i<WaitSec;i++) {
		Sleep(1000);
		fprintf(stderr, ".");
//		retval = NopCommand(connsock);
	}
	retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
	if (WaitSec < deadlocktime) {
		if (retval != 0) {
			fprintf(stderr, "\nConnection lost!!!!!!!!!!!!!\n");
			return -1;
		} else {
			fprintf(stderr, "\nSuccess!!!!!!!!!!!!!\n");
			return 0;
		}
	} else {
		if (retval ==0) {
			fprintf(stderr, "\nFreeing lock should be failed\n");
			Logout(connsock);
			return -1;
		} else {
			fprintf(stderr, "\nSuccess!!!!!!!!!!!!!\n");
			return 0;
		}
	}
}

int CmdNop(char* target, char* arg[])
{
	SOCKET				connsock;
	unsigned			UserId;
	int retval;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0) {
		return -1;
	}
	retval = NopCommand(connsock);
	if (retval == 0)
		printf("Success..\n");
	Logout(connsock);
	return 0;
}

int CmdTextTargetData(char* target, char* arg[])
{
	SOCKET				connsock;
	unsigned			UserId;
	int retval;
	UINT64 Param = 0;
	BOOL bSet = FALSE;

	if (arg[0]) {
		if (_stricmp(arg[0], "Set")==0) {
			bSet = TRUE;
			Param = _strtoi64(arg[1], NULL, 0);
		}
	}

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	if (bSet) {
		retval = TextTargetData(connsock, PARAMETER_OP_SET, iTargetID, &Param);
		if (retval==0) {
			fprintf(stderr, "TextTargetData set: %I64d\n", Param);
		}
	} else {
		retval = TextTargetData(connsock, PARAMETER_OP_GET, iTargetID, &Param);
		if (retval==0) {
			fprintf(stderr, "TextTargetData get: %I64d\n", Param);
		}
	}

	if (retval!=0) {
		fprintf(stderr, "TextTargetData failed %d.\n", retval);
	}

	DisconnectFromNdas(connsock, UserId);
	return 0;
}

int CmdDiscovery(char* target, char* arg[])
{
	SOCKET				connsock;
	LPX_ADDRESS			address;
	int retval;

	retval = lpx_addr(target, &address);
	if (!retval) {
		printf("Invalid address\n");
		return -1;
	}

	if(MakeConnection(&address, &connsock) == FALSE) {
		fprintf(stderr, "[NdasCli]main: Can't Make Connection to LanDisk!!!\n");
		return  -1;
	}

	Discovery(connsock);

	closesocket(connsock);
	return 0;
}


int CmdDynamicOptionTest(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	int Pattern;
	clock_t start_time, end_time;
	UINT64 Pos = 20 * 1024; // in MB. 20G
	UINT32 IoSize = 256; // in MB.
	int Option;
	int Blocks = 128;
	int retval;
	srand( (unsigned)GetTickCount()  );

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	if((iResult = GetDiskInfo(connsock, iTargetID, FALSE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]main: Identify Failed... Master iTargetID %d\n", iTargetID);
		retval = -1;
		goto errout;
	}

	// Set starting mode: no CRC, no AES
	Param0 = Param1 = Param2 = 0;
	iResult = VendorCommand(connsock, VENDOR_OP_SET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);

	if (iResult == 0) {
		printf("Set no CRC, no AES\n");
	} else if (iResult < 0) {
		printf("Failed to run vendor command\n");
	} 

	Pattern = rand() % GetNumberOfPattern();
	Option = 0;
	// Write data
	fprintf(stderr, "Writing default data %dMB with no option\n", IoSize);
	retval = WritePattern(connsock, Pattern, Pos, IoSize, Blocks , 0x21);
	if (retval!=0) {
		fprintf(stderr, "Failed to write pattern\n");
		goto errout;
	}

	for (Option=0x0f;Option>=0;Option--) {
		// Set option
		fprintf(stderr, "Setting option: ");
		if (Option ==0)
			fprintf(stderr, "NONE");
		if (Option & 0x1) 
			fprintf(stderr, "Data Enc. ");
		if (Option & 0x2)
			fprintf(stderr, "Header Enc. ");
		if (Option & 0x4)
			fprintf(stderr, "Data Dig. ");
		if (Option & 0x8)
			fprintf(stderr, "Header Dig. ");
		fprintf(stderr, "\n");

		Param0 = Param1 = 0;
		Param2 = htonl(Option);
		retval = VendorCommand(connsock, VENDOR_OP_SET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
		if (retval!=0) {
			fprintf(stderr, "Failed to write pattern\n");
			goto errout;
		}
		
		fprintf(stderr, "Reading %dMB: ", IoSize);
		start_time = clock();
		retval = ReadPattern(connsock, Pattern, Pos, IoSize, Blocks);
		if (retval!=0) {
			fprintf(stderr, "Failed to read pattern\n");
			goto errout;
		}
		end_time = clock();
		printf(" %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));

		Pattern = rand() % GetNumberOfPattern();
		fprintf(stderr, "Writing %dMB: ", IoSize);
		start_time = clock();
		retval = WritePattern(connsock, Pattern, Pos, IoSize, Blocks , 0);
		if (retval!=0) {
			fprintf(stderr, "Failed to read pattern\n");
			goto errout;
		}
		end_time = clock();
		printf(" %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	return 0;
}

//
// Arg[0]: UserId+Permission
// Arg[1]: Password. Optional
//
int CmdLoginRw(char* target, char* arg[])
{
	UINT64 Pos = 10;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	static const short Blocks = 64; // Number of sectors to read 

	if (arg[0]==0 || arg[0][0] ==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
		fprintf(stderr, "UserId is not given. Using default ID %x\n", UserId);
	} else {
		UserId = (int) _strtoi64(arg[0], NULL, 0);
	}
	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[1]) !=0)
		goto errout;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, FALSE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}
	fprintf(stderr, "Reading ");
	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	if (retval == -2) { // CRC error. try one more time.
		fprintf(stderr, "CRC errored. Retrying\n");
		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	}
	if(retval != 0) {
		fprintf(stderr, "CmdLoginRw: READ Failed... Sector %d\n", (Pos * MB) / 512);
		goto errout;
	} else {
		fprintf(stderr, "succeeded\n");
	}
	
	fprintf(stderr, "Writing (with lock) ");

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
	} else {
		do {
			UINT32 Param0 = 0;
			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, &Param0, NULL, NULL, NULL, 0, NULL);
			if (retval ==0) 
				break;
			if (retval == LANSCSI_RESPONSE_T_SET_SEMA_FAIL) {
				Sleep(1);
				continue;
			}
			printf("Set mutex failed %x\n", retval);
			goto errout;
		} while(1);
	}

	retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	if (retval == -2) { // CRC error. try one more time.
		fprintf(stderr, "CRC errored. Retrying\n");
		retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	}
	if(retval != 0) {
		fprintf(stderr, "CmdLoginRw: WRITE Failed... Sector %d\n", (Pos * MB) / 512);
		goto errout;
	} else {
		if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
			VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
		} else {
			UINT32 Param0 = 0;
			((PBYTE)&Param0)[3] = (BYTE)0;
			retval = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, NULL, NULL, NULL, 0, NULL);
			if (retval != 0) 
				printf("Free mutex failed %x\n", retval);
		}
		fprintf(stderr, "succeeded\n");
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;
}

//
// Arg[0]: UserId+Permission
// Arg[1]: Time to wait in seconds
// Arg[2]: Password. Optional
//
int CmdLoginWait(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	unsigned			UserId;
	static const short Blocks = 128; // Number of sectors to read 
	int	WaitTime;
	int i;
	if (arg[0] == NULL || arg[1] == NULL) {
		printf("UserId and time should be supplied\n");
		return 0;
	}
	UserId = (int) _strtoi64(arg[0], NULL, 0);
	WaitTime = (int) _strtoi64(arg[1], NULL, 0);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[2]) !=0)
		goto errout;

	printf("Logged in and waiting for %d seconds", WaitTime);
	for(i=0;i<WaitTime;i++) {
		printf(".");
		Sleep(1000);
	}
	printf("\n");
	
	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);

	return retval;
}



//
// Arg[0]: UserNumber
// Arg[1]: New password 
// Arg[2]: Superuser password. Optional.
int CmdSetPassword(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	int					iResult;
	unsigned			UserId;
	UCHAR NewPassword[PASSWORD_LENGTH + 16] = {0};
	int UserNum;
//	UCHAR ResetPw[9];
	UINT32 Param0=0, Param1=0, Param2=0;

	UserNum = (int) _strtoi64(arg[0], NULL, 0);
	if (UserNum>7) {
		printf("Invalid user number\n");
		return -1;
	}
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

#if 0
	printf("Using password 0xffffffffff\n");
	memset(ResetPw, 0x0ff, 8);
	ResetPw[8] =0;
	arg[2] = (char*)ResetPw;
#endif
	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[2]) !=0)
		goto errout;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		// Assume password is plain text.
		strncpy((char*)NewPassword, arg[1], PASSWORD_LENGTH);

		Param0 = htonl(0);
		Param1 = htonl(0x30 + 16 * UserNum);
		Param2 = htonl(16);
		ReverseBytes(NewPassword, PASSWORD_LENGTH);
		iResult = VendorCommand(connsock, VENDOR_OP_SET_EEP, &Param0, &Param1, &Param2, NULL, 0, (PCHAR)NewPassword);
		if (iResult == 0) {
			ReverseBytes(NewPassword, PASSWORD_LENGTH);
			printf("Changed password to %s\n", arg[1]);
		} else {
			goto errout;
		}
	} else {
		// 1.1 or 2.0
		// Assume password is plain text.
		if (arg[1]==0 || arg[1][0]==0) {
			printf("Setting to default password\n");
			if (UserNum == 0) {
				UCHAR pw[8];
				memcpy(pw, (UCHAR*)&iSuperPassword_v1, 8);
				ReverseBytes(pw, 8);

				memcpy(&Param0, pw, 4);
				memcpy(&Param1, &pw[4], 4);
			} else {
				UCHAR pw[8];
				memcpy(pw, (UCHAR*)&iPassword_v1, 8);
				ReverseBytes(pw, 8);

				memcpy(&Param0, pw, 4);
				memcpy(&Param1, &pw[4], 4);
			}
		} else {
			strncpy((char*)NewPassword, arg[1], PASSWORD_LENGTH_V1);
			ReverseBytes(NewPassword, 8);	
			memcpy(&Param0, &NewPassword[0], 4);
			memcpy(&Param1, &NewPassword[4], 4);
		}
		if (UserNum ==0) {
			printf("Changing superuser password\n");
			iResult = VendorCommand(connsock, VENDOR_OP_SET_SUPERVISOR_PW, &Param0, &Param1, &Param2, NULL, 0, NULL);
		} else {
			printf("Changing normal user password.\n");
			iResult = VendorCommand(connsock, VENDOR_OP_SET_USER_PW, &Param0, &Param1, &Param2, NULL, 0, NULL);
		}
		
		if (iResult == 0) {
			printf("Changed password to %s\n", arg[1]);
		} else {
			goto errout;
		}
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

//
// Arg[0]: Superuser password. Optional
//
int CmdResetAccount(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH + 16] = {0};
	int idperm;
	int i;

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR) arg[0]) !=0)
		goto errout;

	for(i=0;i<8;i++) {
		if (i==0 || i==1) {
			idperm = MAKE_USER_ID(i,USER_PERMISSION_MASK); // Give all permission
		} else {
			idperm = MAKE_USER_ID(i,0);
		}
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(idperm);
		iResult = VendorCommand(connsock, VENDOR_OP_SET_USER_PERMISSION, &Param0, &Param1, &Param2, NULL, 0, NULL);
		if (iResult != 0) {
			printf("Failed to set permission\n");
			goto errout;
		}
		
		Param0 = htonl(0);
		Param1 = htonl(0x30 + 16 * i);
		Param2 = htonl(16);
		if (i==0) {
			memcpy(Password, def_supervisor_password, PASSWORD_LENGTH);
			ReverseBytes(Password, PASSWORD_LENGTH);
			iResult = VendorCommand(connsock, VENDOR_OP_SET_EEP, &Param0, &Param1, &Param2, NULL, 0, (PCHAR)Password);
		} else {
			memcpy(Password, def_password0, PASSWORD_LENGTH);
			ReverseBytes(Password, PASSWORD_LENGTH);
			iResult = VendorCommand(connsock, VENDOR_OP_SET_EEP, &Param0, &Param1, &Param2, NULL, 0, (PCHAR)Password);
		}

		if (iResult != 0) {
			printf("Failed to set password\n");
			goto errout;
		}		
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

// Arg[0]: User  PW. Optional
int CmdGetOption(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	int i;
#if 0
	UCHAR ResetPw[9];
	printf("Using password 0xffffffffff\n");
	memset(ResetPw, 0x0ff, 8);
	ResetPw[8] =0;
	arg[0] = (char*)ResetPw;
#endif

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) !=0)
		goto errout;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		// 2.5 version support get option
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		// Get d_opt will return static option except jumbo.
		iResult = VendorCommand(connsock, VENDOR_OP_GET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
		Param2 = htonl(Param2);
		printf("Dynamic Option value=%02x\n", Param2);
		for(i=5;i>=0;i--) {
			printf("Option bit %d:", i);
			switch(i) {
				case 5:		printf("No heart frame    ");	break;
				case 4:		printf("Reserved          ");	break;
				case 3:		printf("Header CRC        ");	break;
				case 2:		printf("Data CRC          ");	break;
				case 1:		printf("Header Encryption ");	break;
				case 0:		printf("Data Encryption   ");	break;
			}
			printf((Param2 &(1<<i))?"On    ":"Off   ");
			printf("\n");
		}
	} else {
		// Get from login information
		printf("Bit 1: Header Encryption :%s\n", HeaderEncryptAlgo?"On":"Off");
		printf("Bit 0: Data Encryption   :%s\n", DataEncryptAlgo?"On":"Off");
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

// Arg[0]: Option
// Arg[1]: Superuser PW. Optional
int CmdSetOption(char* target, char* arg[])
{

	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	int i;	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	int Option;
	int option_count;
	Option = (int) _strtoi64(arg[0], NULL, 0);

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);
	
	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[1]) !=0)
		goto errout;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		option_count = 5;
	} else {
		option_count = 2;
#ifndef BUILD_FOR_DIST
		printf("Warning: currently supervisor password change does not work with header/data encryption\n");
#endif
	}

	for(i=option_count-1;i>=0;i--) {
		printf("Option %d:", i);
		switch(i) {
			case 5:		printf("No heart frame    ");	break;
			case 4:		printf("Reserved          ");	break;
			case 3:		printf("Header CRC        ");	break;
			case 2:		printf("Data CRC          ");	break;
			case 1:		printf("Header Encryption ");	break;
			case 0:		printf("Data Encryption   ");	break;
		}
		printf((Option &(1<<i))?"    On":"    Off");
		printf("\n");
	}
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(Option);
	} else {
		Param0 = htonl(0);
		Param1 = htonl(Option);
		Param2 = 0;
	}
	iResult = VendorCommand(connsock, VENDOR_OP_SET_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (iResult !=0) {
		printf("Vendor command failed\n");
		goto errout;
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}


// Arg[0]: Write size in MB per iteration
// Arg[1]: Position to write in MB
// Arg[2]: number of Iteration
// Arg[3]: Sector count per operation. 0 for random value from 1~128
int CmdBlockVariedIo(char* target, char* arg[])
{
	int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	int LockMode = 0x21;
	SOCKET connsock;
	UINT32 UserId;

	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Pos = _strtoi64(arg[1], NULL, 0);
	Ite = (int)_strtoi64(arg[2], NULL, 0);
	SectorCount = (int) _strtoi64(arg[3], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		LockMode =0x21;
	} else {
		LockMode =0x0;
	}
		
	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	retval = RwPatternChecking(connsock, WriteSize, Ite, Pos, SectorCount, LockMode);

	DisconnectFromNdas(connsock, UserId);

	return retval;
}

// Arg[0]: Write size in MB per iteration
// Arg[1]: Position to write in MB
// Arg[2]: number of Iteration
int CmdExIo(char* target, char* arg[])
{
	int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount = 128;
	int LockMode = 0x0;
	SOCKET connsock;
	UINT32 UserId;

	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Pos = _strtoi64(arg[1], NULL, 0);
	Ite = (int)_strtoi64(arg[2], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, FALSE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	retval = RwPatternChecking(connsock, WriteSize, Ite, Pos, SectorCount, LockMode);

	DisconnectFromNdas(connsock, UserId);

	return retval;
}


#define MUTEX_INFO_LEN 64
#define NUMBER_OF_MUTEX 8
#define NUMBER_OF_MUTEX_20	4

int CmdMutexCli(char* target, char* arg[])
{
	SOCKET	connsock;
	int		iResult;
	int		retVal = 0;
	
	int		UserId;
	UINT32	Param0 = 0, Param1 = 0, Param2 = 0;
	BYTE*	Param0Byte = (BYTE*)&Param0;
	char	line[100];
	char	cmd[10];
	int		LockNumber;
	char	LockData[64+1] = {0};
	int port;
	int i;
	BOOL Locked;
	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_RO);
	
	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	printf("Help: <Take | Give | Setinfo | Info | Break | Owner | infoAll | Quit> [Lock number] [Lock Data]\n");
	while(1) {
		if( fgets(line, 100, stdin) == NULL) {
			printf( "fgets error\n" );
			break;
		}
		if (line[0]==0 || line[0] == '\n')
			continue;
		if (toupper(line[0]) == 'Q')
			break;

		memset(LockData, 0, 64);
		if (strlen(line)>0) {
			retVal = sscanf(line, "%s %d %s", cmd, &LockNumber, LockData);
			if (retVal<1) {
				printf("Invalid command\n");
				continue;
			}
			if (retVal==1)
				LockNumber = 0;
			if (retVal==2) {
				LockData[0] = 0;
			}
		} else {
			continue;
		}
		switch(toupper(cmd[0])) {
		case 'T':
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, &Param0, &Param1, &Param2, LockData, 0, NULL);
				if (iResult ==0) {
					printf("Lock result = %d\n", Param0Byte[0]);
					printf("Set lock succeeded: previous lock info = %s\n", (char*)LockData);
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Set lock failed: Already locked. Lock info = %s\n", (char*)LockData);
				} else {
					printf("Set lock failed: Unexpected error code=%x\n", iResult);
				}
			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, &Param0, &Param1, &Param2, NULL, 0, NULL);
				if (iResult ==0) {
					printf("Set lock succeeded: counter = %d\n", ntohl(Param1));
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Set lock failed: Already locked. Counter = %u\n", ntohl(Param1));
				} else {
					printf("Set lock failed: error code=%x. Counter = %u\n", iResult, ntohl(Param1));
				}
			}
			break;
		case 'G':
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Param0Byte[3] = (BYTE)LockNumber;
				if (LockData[0] ==0) {
					iResult = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, &Param1, &Param2, NULL, 0, NULL);
				} else {
					iResult = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, &Param1, &Param2, LockData, MUTEX_INFO_LEN, NULL);
				}
				if (iResult ==0) {
					printf("Free lock succeeded\n");
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Free lock failed: Not lock owner\n");
				} else {
					printf("Free lock failed. Unexpected error code=%x\n", iResult);
				}
			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, &Param1, &Param2, NULL, 0, NULL);
				if (iResult ==0) {
					printf("Free lock succeeded: counter = %u\n", ntohl(Param1));
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Free lock failed: Result=%d. Counter = %d\n", iResult, ntohl(Param1));
				} else {
					printf("Free lock failed: error code=%x\n", iResult);
				}
			}
			break;
		case 'S': // Set info
			if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {
				printf("Unsupported command in this chip\n");
				break;
			}

			Param0Byte[3] = (BYTE)LockNumber;
			iResult = VendorCommand(connsock, VENDOR_OP_SET_MUTEX_INFO, &Param0, &Param1, &Param2, LockData, MUTEX_INFO_LEN, NULL);
			if (iResult ==0) {
				printf("Set info succeeded\n");
			} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
				printf("Set info failed: Not a lock owner.\n");
			} else {
				printf("Set info failed: Unexpected error code=%x\n", iResult);
			}
			break;
		case 'I': // Get info
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_INFO, &Param0, &Param1, &Param2, LockData, 0, NULL);
				if (iResult == 0) {
					printf("Get lock info succeeded: lock info = %s\n", (char*)LockData);
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) { 
					printf("Get lock info failed: lock is not held. lock info =%s\n", (char*)LockData);
				} else {
					printf("Get lock info failed: Unexpected error=%x\n", iResult);
				}
			} if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_INFO, &Param0, &Param1, &Param2, NULL, 0, NULL);
				if (iResult ==0) {
					printf("Get lock info succeeded: counter = %u\n", ntohl(Param1));
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Get lock info failed: Counter = %u\n", ntohl(Param1));
				} else {
					printf("Get lock info : error code=%x\n", iResult);
				}
			} else {
				printf("Unsupported command in this chip\n");
				break;
			}
			break;
		case 'B': // Break
			if (ActiveHwVersion != LANSCSIIDE_VERSION_2_5) {
				printf("Unsupported command in this chip\n");
				break;
			}

			Param0Byte[3] = (BYTE)LockNumber;
			iResult = VendorCommand(connsock, VENDOR_OP_BREAK_MUTEX, &Param0, &Param1, &Param2, LockData, 0, NULL);
			if (iResult == 0) {
				printf("Lock result = %d\n", Param0Byte[0]);
			} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) { 
				printf("Break lock failed: lock is not held.");
			} else {
				printf("Break lock failed: Unexpected error=%x\n", iResult);
			}
			break;
		case 'O': // Get owner
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_OWNER, &Param0, &Param1, &Param2, 0, 0, NULL);
				if (iResult == 0) {
					port = htonl(Param1)>>16;
					printf("Lock result = %d\n", Param0Byte[0]);
					printf("Get owner succeeded: MAC %02x:%02x:%02x:%02x:%02x:%02x Port %x\n", 
						((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
						((BYTE*)&Param2)[0], ((BYTE*)&Param2)[1],
						((BYTE*)&Param2)[2], ((BYTE*)&Param2)[3],
						port);
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Get owner failed: lock is not held\n");
				} else {
					printf("Get owner failed: Error=%x\n", iResult);
				}
			} else if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				Param0Byte[3] = (BYTE)LockNumber;
				iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_OWNER, &Param0, &Param1, &Param2, 0, 0, NULL);
				if (iResult == 0) {
					port = htonl(Param1)>>16;
					printf("Lock result = %d\n", Param0Byte[0]);
					printf("Get owner succeeded: MAC %02x:%02x:%02x:%02x:%02x:%02x Port %x\n", 
						((BYTE*)&Param0)[2], ((BYTE*)&Param0)[3],
						((BYTE*)&Param1)[0], ((BYTE*)&Param1)[1],
						((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
						port);
				} else if (iResult == LANSCSI_RESPONSE_T_COMMAND_FAILED) {
					printf("Get owner failed: lock is not held\n");
				} else {
					printf("Get owner failed: Error=%x\n", iResult);
				}
			}
			break;
		case 'A': // info All
			if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
				for(i=0;i<NUMBER_OF_MUTEX;i++) {
					Param0Byte[3] = (BYTE)i;
					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_INFO, &Param0, &Param1, &Param2, LockData, 0, NULL);
					if (iResult == 0) {
						Locked = TRUE;
					} else {
						Locked = FALSE;
					}

					Param0Byte[3] = (BYTE)i;
					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_OWNER, &Param0, &Param1, &Param2, 0, 0, NULL);

					printf("#%d: %s ", i, Locked?"Held":"Free");
					if (Locked) {
						port = htonl(Param1)>>16;
						printf("%02x:%02x:%02x:%02x:%02x:%02x.%04x ",
							((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3],
							((BYTE*)&Param2)[0], ((BYTE*)&Param2)[1],
							((BYTE*)&Param2)[2], ((BYTE*)&Param2)[3],
							port);
					} else {
						printf("                       ");
					}
					printf("Info=%s\n", LockData);
				}
			} else if(ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
				unsigned int counter;

				for(i=0;i<NUMBER_OF_MUTEX_20;i++) {
					Param0Byte[3] = (BYTE)i;
					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_INFO, &Param0, &Param1, &Param2, NULL, 0, NULL);
					if (iResult != 0) {
						printf("#%d: Failed to get counter.\n", i);
						continue;
					}
					counter = ntohl(Param1);

					Param0Byte[3] = (BYTE)i;
					iResult = VendorCommand(connsock, VENDOR_OP_GET_MUTEX_OWNER, &Param0, &Param1, &Param2, 0, 0, NULL);
					if (iResult != 0) {
						printf("#%d: Failed to get owner.\n", i);
						continue;
					}

					printf("#%d: ", i);
					port = htonl(Param1)>>16;
					printf("%02x:%02x:%02x:%02x:%02x:%02x ",
						((BYTE*)&Param0)[2], ((BYTE*)&Param0)[3],
						((BYTE*)&Param1)[0], ((BYTE*)&Param1)[1],
						((BYTE*)&Param1)[2], ((BYTE*)&Param1)[3]);
					printf("Counter=%u\n", counter);
				}
			} else {
				printf("Unsupported command in this chip\n");
				break;
			}
			break;
		default:
			printf("Unknown command\n");
			break;
		}
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}


int CmdLpxConnect(char* target, char* arg[])
{
	LPX_ADDRESS			address;
	BOOL	Ret;
	SOCKET connsock;

	Ret = lpx_addr(target, &address);
	if (!Ret) {
		printf("Invalid address\n");
		return -1;
	}

	if(MakeConnection(&address, &connsock) == FALSE) {
		fprintf(stderr, "[NdasCli]main: Can't Make Connection to LanDisk!!!\n");
		return -1;
	}
	fprintf(stderr, "Connected. Press enter to disconnect\n");
	fgetc(stdin);
	closesocket(connsock);
	return 0;
}

//
// Arg[0]: UserNumber
// Arg[1]: New password 
// Arg[2]: Previous password. Optional.
int CmdSetUserPassword(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	int					iResult;
	unsigned			UserId;
	UCHAR NewPassword[PASSWORD_LENGTH] = {0};
	int UserNum;
	UINT32 Param0, Param1, Param2;

	UserNum = (int) _strtoi64(arg[0], NULL, 0);
	if (UserNum>7) {
		printf("Invalid user number\n");
		return -1;
	}
	UserId = MAKE_USER_ID(UserNum, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[2]) !=0)
		goto errout;
	
	if (arg[1] == 0 || arg[1][0]==0) {
		printf("Setting default password\n");
		memcpy(NewPassword, def_password0, PASSWORD_LENGTH);
	} else {
		// Assume password is plain text.
		strncpy((char*)NewPassword, arg[1], PASSWORD_LENGTH);
	}
	Param0 = Param1 = Param2 = 0;
//	ReverseBytes(NewPassword, PASSWORD_LENGTH);
	iResult = VendorCommand(connsock, VENDOR_OP_SET_USER_PW, &Param0, &Param1, &Param2, (PCHAR)NewPassword, 16, (PCHAR)NewPassword);
	if (iResult == 0) {
//		ReverseBytes(NewPassword, PASSWORD_LENGTH);
		if (arg[1] == 0 || arg[1][0]==0) {
			printf("Changed password to default\n");
		} else {
			printf("Changed password to %s\n", NewPassword);
		}
	} else {
		printf("Failed to change password\n");
		goto errout;
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

//
// Arg[0]: UserId+Permission
// Arg[1]: Password. Optional
//
int CmdLoginR(char* target, char* arg[])
{
	UINT64 Pos = 10;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	static const short Blocks = 64; // Number of sectors to read 
	UserId = (int) _strtoi64(arg[0], NULL, 0);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[1]) !=0)
		goto errout;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}
	fprintf(stderr, "Reading..");

	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	if(retval != 0) {
		fprintf(stderr, "CmdLoginR: READ Failed... Sector %d\n", (Pos * MB) / 512);
		goto errout;
	}

	fprintf(stderr, "succeeded\n");
	
	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;
}


int CmdSetPacketDrop(char* target, char* arg[])
{
	HANDLE deviceHandle;
	ULONG Param;
	DWORD dwReturn;
	BOOL bRet;
	
	deviceHandle = CreateFile (
				TEXT("\\\\.\\SocketLpx"),
				GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,
				0
		 );
             
	if( INVALID_HANDLE_VALUE == deviceHandle ) {
		fprintf(stderr, "Failed to open lpx\n");
		return -1;
	}

	if (_stricmp(target, "tx")!=0 && _stricmp(target, "rx")!=0) {
		printf("Current setting:\n");

		bRet = DeviceIoControl(
				 deviceHandle,
				 IOCTL_LPX_GET_RX_DROP_RATE,
				 0,   
				 0, 
				 &Param,
				 sizeof(Param),
				 &dwReturn,	
				 NULL        // Overlapped
				 );	
		if (bRet == FALSE) {
			fprintf(stderr, "Failed to get drop rate\n");
		} else {
			printf("  Drops %d packets of 1000 rx packet\n", Param);
		}

		bRet = DeviceIoControl(
				 deviceHandle,
				 IOCTL_LPX_GET_TX_DROP_RATE,
				 0,   
				 0, 
				 &Param,
				 sizeof(Param),
				 &dwReturn,	
				 NULL        // Overlapped
				 );	
		if (bRet == FALSE) {
			fprintf(stderr, "Failed to get drop rate\n");
		} else {
			printf("  Drops %d packets of 1000 tx packets\n", Param);
		}
	} else {
		int rate = (int) _strtoi64(arg[0], NULL, 0);
		if (_stricmp(target, "tx")==0) {
			bRet = DeviceIoControl(
					 deviceHandle,
					 IOCTL_LPX_SET_TX_DROP_RATE,
					 &rate,
					 sizeof(rate),
					 0,  
					 0, 
					 &dwReturn,	
					 NULL        // Overlapped
					 );	
		} else {
			bRet = DeviceIoControl(
					 deviceHandle,
					 IOCTL_LPX_SET_RX_DROP_RATE,
					 &rate,
					 sizeof(rate),
					 0,  
					 0, 
					 &dwReturn,	
					 NULL        // Overlapped
					 );	
		}
		if (bRet == FALSE) {
			fprintf(stderr, "Failed to set drop rate\n");
		}
	}
	CloseHandle(deviceHandle);
	return 0;
}

int CmdHostList(char* target, char* arg[])
{
	SOCKET	connsock;
	int		iResult;
	int		retVal = 0;
	
	int		UserId;
	UINT32	Param0 = 0, Param1 = 0, Param2 = 0;
	BYTE	HostList[512] = {0};

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	
	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	
	iResult = VendorCommand(connsock, VENDOR_OP_GET_HOST_LIST, &Param0, &Param1, &Param2, (PCHAR)HostList, 0, NULL);
	if (iResult ==0) {
		UINT32 i;
		int Active = 0;
		int Pos;
		BYTE InvalidMac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
//		PrintHex(HostList, 512);
		for(i=0;i<ntohl(Param2);i++) {
			Pos = i*8;
			if (memcmp(InvalidMac, &HostList[Pos+2], 6) == 0) {
				continue;
			}
			Active++;
			printf("#%02d: Info %02x:%02x MAC %02x:%02x:%02x:%02x:%02x:%02x \n", 
					i, 
					HostList[Pos+0], HostList[Pos+1], HostList[Pos+2], 
					HostList[Pos+3], HostList[Pos+4], HostList[Pos+5],
					HostList[Pos+6], HostList[Pos+7]);
		}
		printf("Number of entries: %d\n", Active);
	} else {
		printf("Failed to get host list\n");
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}


int CmdDigestTest(char* target, char* arg[])
{
	UINT64 Pos = 10000;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	static const short Blocks = 128; // Number of sectors to read 
	UINT32 Param0, Param1, Param2;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);


	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

#if 1
	printf("Header with bad CRC test..\n"); 

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;
	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}

	printf("Turning on header CRC\n");
	Param0 = Param1 = 0;
	Param2 = htonl(0x8);
	retval = VendorCommand(connsock, VENDOR_OP_SET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (retval !=0) {
		printf("Failed to set header CRC\n");
		goto errout;
	}

	VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);

	printf("Writing - this should be FAILED.\n");

	retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, IDECMD_OPT_BAD_HEADER_CRC, 0);
	
	if (retval ==0) {
		printf("Error!! Connection should be closed for bad header CRC \n");
	} else {
		printf("Success!!\n");
	}

	DisconnectFromNdas(connsock, UserId);
#endif

#if 1
	printf("Data with bad CRC test..\n"); 

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;
	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}

	printf("Turning on data CRC\n");
	Param0 = Param1 = 0;
	Param2 = htonl(0x4);
	retval = VendorCommand(connsock, VENDOR_OP_SET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (retval !=0) {
		printf("Failed to set header CRC\n");
		goto errout;
	}

	VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);

	retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, IDECMD_OPT_BAD_DATA_CRC, 0);
	
	if(retval != -2) {
		fprintf(stderr, "Error!! CRC error should be occured\n");
		goto errout;
	} else {
		fprintf(stderr, "CRC error returned. Succeeded!!\n");
	}
	
	VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);	

	DisconnectFromNdas(connsock, UserId);
#endif
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;
}

//
// Arg[0]: Host MAC address of NIC to send request.
//
int CmdPnpListen(char* target, char* arg[])
{
	SOCKADDR_LPX slpx;
	int result;
	int broadcastPermission;
	HEARTBEAT_RAW_DATA_V2 PnPReply;
	int i = 0;
	SOCKET			sock;
	int addrlen;
	LPX_ADDRESS			address;
	BOOL	Ret;
	double start_time = (double)clock() / CLOCKS_PER_SEC;

	Ret = lpx_addr(target, &address);
	if (!Ret) {
		printf("Invalid address\n");
		return -1;
	}

	if (arg[0] == 0) {
		fprintf(stderr, "MAC address of Host is required.\n");
		return -1;
	}

	// Create Listen Socket.
	sock = socket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), _T("socket"));
		return -1;
	}

	broadcastPermission = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			(const char*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
		fprintf(stderr, "Can't setsockopt for broadcast: %d\n", errno);
		return -1;
	}


	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family = AF_LPX;

	result = lpx_addr(arg[0], &slpx.LpxAddress);
	slpx.LpxAddress.Port = htons(10002);

	result = bind(sock, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result !=0) {
		fprintf(stderr, "Failed to bind. Maybe another PNP listner is running or host MAC is not set correctly\n");
		return -1;
	}
	addrlen = sizeof(slpx);
	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family  = AF_LPX;
	slpx.LpxAddress.Port = htons(10001);
    slpx.LpxAddress.Node[0] = 0xFF;
    slpx.LpxAddress.Node[1] = 0xFF;
    slpx.LpxAddress.Node[2] = 0xFF;
    slpx.LpxAddress.Node[3] = 0xFF;
    slpx.LpxAddress.Node[4] = 0xFF;
    slpx.LpxAddress.Node[5] = 0xFF;

	while(1) {
		result = recvfrom(sock, (char*)&PnPReply, sizeof(PnPReply),0,(struct sockaddr *)&slpx, &addrlen);
		if (result == sizeof(PnPReply) && memcmp(slpx.LpxAddress.Node, address.Node, 6)==0) {
			HEARTBEAT_RAW_DATA_V2 ExpectedReply = {0};
			double rxt;

			rxt = (double)clock() / CLOCKS_PER_SEC;

			printf("%.1f: PNP received - ", 
					rxt,
					(BYTE)slpx.LpxAddress.Node[0], (BYTE)slpx.LpxAddress.Node[1], 
					(BYTE)slpx.LpxAddress.Node[2], (BYTE)slpx.LpxAddress.Node[3],
					(BYTE)slpx.LpxAddress.Node[4], (BYTE)slpx.LpxAddress.Node[5]
			);

			ExpectedReply.DgramHeader.Signature = htons(0xffd6);
			ExpectedReply.DgramHeader.MsgVer = 1;
			ExpectedReply.DgramHeader.OpCode = 0x81;
			ExpectedReply.DgramHeader.Length = htons(24);
			ExpectedReply.PnpMessage.DeviceType = 0;
			ExpectedReply.PnpMessage.Version = 3;
			ExpectedReply.PnpMessage.StreamListenPort = htons(10000);
			if (memcmp(&ExpectedReply, &PnPReply, sizeof(PnPReply)) != 0) {
				printf("Unexpected reply packet\n");
			} else {
				printf("Expected reply packet received\n");
			}
		} else if (result == 2) {
//			printf("Legacy pnp message\n");
		} else {
			break;
		}

		// Wait for 30 seconds. problem! - if no netdisk exists on the network, this will not work!
		if (start_time + 30 < (double)clock() / CLOCKS_PER_SEC) {
			break;
		}
	}
	closesocket(sock);
	return 0;
}


int CmdBatchTest(char* target, char* arg[])
{
	printf("Not implemented\n");
	return 0;
}

int DeviceSpinUp(SOCKET connsock, int TargetId)
{
	struct hd_driveid	info;
	int					iResult;
	
	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]SetWriteCache: Identify Failed...\n");
		return iResult;
	}

	printf("Current setting: Power-Up In Standby support=%d enabled=%d\n", 
			(info.command_set_2 & 0x20) != 0,
			(info.cfs_enable_2 & 0x20) != 0);
	printf("Current setting: Required SetFeature to spin-up support=%d enabled=%d\n", 
			(info.command_set_2 & 0x40) != 0,
			(info.cfs_enable_2 & 0x40) != 0);
	if((info.command_set_2 & 0x20) == 0) {
		printf("Power-Up In Standby feature not supported.\n");
		return -1;
	}
	if((info.cfs_enable_2 & 0x20) == 0) {
		printf("Power-Up In Standby feature not enabled.\n");
		return -1;
	}
	if((info.command_set_2 & 0x40) == 0) {
		printf("Required SetFeature to spin-up feature not supported.\n");
		return -1;
	}
	if((info.cfs_enable_2 & 0x40) == 0) {
		printf("Required SetFeature to spin-up not enabled.\n");
		return -1;
	}

	iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0, (_int8)SETFEATURES_SPINUP, 0, NULL, 0,0 );
	if (iResult !=0) {
		printf("Failed to set feature\n");
		return -1;
	}

	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}
	
	printf("Device is spinning.\n");
	return 0;	
}


#define NDASCLI_MAX_TASKS 16



// Arg[0]: Size in MB per iteration
// Arg[1]: number of Iteration
// Arg[2]: Position to read in MB
// Arg[3]: Sector count per operation. 0 for random value from 1~128
int CmdRead(char* target, char* arg[])
{
	UINT64 ReadSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	UCHAR* data;
	int i;
	UINT64 j;
	int Blocks;
	int MAX_REQ;
	clock_t start_time, end_time, first_start, end_last;
	NDASCLI_TASK	tasks[NDASCLI_MAX_TASKS];
	int task_pending_sn, task_next_sn, pending_tasks;

	ReadSize = (UINT64) _strtoi64(arg[0], NULL, 0);
	Ite = (int)_strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	SectorCount = (int) _strtoi64(arg[3], NULL, 0);
	task_pending_sn = 0;
	task_next_sn = 0;
	pending_tasks = 0;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

#if 1
#if 0
	DeviceSpinUp(connsock, iTargetID);
#endif
	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

#else

	PerTarget[iTargetID].bLBA = TRUE;
	PerTarget[iTargetID].bLBA48 = TRUE;
	PerTarget[iTargetID].SectorCount = 0x100000000;
	PerTarget[iTargetID].bSmartSupported = FALSE;
	PerTarget[iTargetID].bSmartEnabled = FALSE;
	PerTarget[iTargetID].bPIO = FALSE;
		
#endif
	
	data = (PUCHAR) malloc(SectorCount * 512);
	memset(data, 0xaa, SectorCount * 512);

	for(i = 0; i < NDASCLI_MAX_TASKS; i++) {
		tasks[i].TargetId = iTargetID;
		tasks[i].LUN = 0;
		tasks[i].BufferLength = SectorCount * 512;
		tasks[i].Buffer = (PCHAR)data;
		tasks[i].Option = 0;
		tasks[i].TaskTag = 0;
		tasks[i].IdeCommand = WIN_READ;
		tasks[i].SentIdeCommand = 0;
		tasks[i].Location = 0;
		tasks[i].SectorCount = 0;
		tasks[i].Feature = 0;
		tasks[i].Info = 0;
	}


	printf("Size: %I64dMB, Pos: %I64d, Sectors: %d, Iteration: %3d - \n", 
		ReadSize, Pos, SectorCount, Ite);
	first_start = clock();
	for (i=0;i<Ite;i++) {
		start_time = clock();

		if (SectorCount==0)
			Blocks = (rand() % 128) + 1;
		else 
			Blocks = SectorCount;
		MAX_REQ = Blocks * 512;

		for (j = 0; j < ReadSize * MB; j+=MAX_REQ) {
			
			tasks[task_next_sn % NDASCLI_MAX_TASKS].Location = (Pos * MB + j) / 512;
			tasks[task_next_sn % NDASCLI_MAX_TASKS].SectorCount = (short)Blocks;

			retval = SendIdeCommandRequestAndData(connsock, &tasks[task_next_sn % NDASCLI_MAX_TASKS]);
			if(retval != 0) {
				fprintf(stderr, "\n[NdasCli]main: SendIdeCommandRequestAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
				goto errout;
			}
			if (j!=0 && j%(1024*1024*1024) == 0) {
				fprintf(stderr, "\n%d/%dGB", (int)(j/1024/1024/1024), (int)(ReadSize /1024));
			}

			// Check the pending task
			pending_tasks ++;
			task_next_sn ++;
			if(pending_tasks == MaxPendingTasks) {
				retval = ReceiveIdeCommandReplyAndData(connsock, &tasks[task_pending_sn % NDASCLI_MAX_TASKS]);
				if(retval != 0) {
					fprintf(stderr, "\n[NdasCli]main: ReceiveIdeCommandReplyAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
					goto errout;
				}
				task_pending_sn ++;
				pending_tasks --;
			}
		}
		// Receive the replies of pending tasks.
		for( ; pending_tasks > 0; pending_tasks--) {
				retval = ReceiveIdeCommandReplyAndData(connsock, &tasks[task_pending_sn % NDASCLI_MAX_TASKS]);
				if(retval != 0) {
					fprintf(stderr, "\n[NdasCli]main: ReceiveIdeCommandReplyAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
					goto errout;
				}
				task_pending_sn ++;
		}

		end_time = clock();
		printf("Iteration %d: %.1f MB/sec\n", i+1, 1.*ReadSize*CLK_TCK/(end_time-start_time));
	}
	end_last = clock();
	printf("Average: %.1f MB/sec\n", 1.*ReadSize*CLK_TCK*Ite/(end_last-first_start));
	DisconnectFromNdas(connsock, UserId);
errout:
	if (data)
		free(data);
	return retval;
}

// Arg[0]: Size in MB per iteration
// Arg[1]: number of Iteration
// Arg[2]: Position to read in MB
// Arg[3]: Sector count per operation. 0 for random value from 1~128
int CmdVerify(char* target, char* arg[])
{
	UINT64 ReadSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	int i;
	UINT64 j;
	int Blocks;
	int MAX_REQ;
	clock_t start_time, end_time;
	ReadSize = (UINT64) _strtoi64(arg[0], NULL, 0);
	Ite = (int)_strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	SectorCount = (int) _strtoi64(arg[3], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	printf("Size: %dMB, Pos: %I64d, Sectors: %d, Iteration: %3d\n", 
		ReadSize, Pos, SectorCount, Ite);
	start_time = clock();
	for (i=0;i<Ite;i++) {
		if (SectorCount==0)
			Blocks = (rand() % 128) + 1;
		else 
			Blocks = SectorCount;
		MAX_REQ = Blocks * 512;

		for (j = 0; j < ReadSize * MB; j+=MAX_REQ) {
			retval = IdeCommand(connsock, 0, 0, WIN_VERIFY, (Pos * MB + j) / 512, (short)Blocks, 0, 0, NULL, 0, 0);
			if(retval != 0) {
				fprintf(stderr, "\n[NdasCli]main: Verify Failed... Sector %d\n", (Pos * MB + j) / 512);
				goto errout;
			}
			if (j!=0 && j%(1024*1024*1024) == 0) {
				fprintf(stderr, "\n%d/%dGB", (int)(j/1024/1024/1024), (int)(ReadSize /1024));
			}
		}
	}
	end_time = clock();
	printf(" %.1f MB/sec\n", 1.*ReadSize*Ite*CLK_TCK/(end_time-start_time));

	DisconnectFromNdas(connsock, UserId);
errout:

	return retval;
}

// Arg[0]: File.
// Arg[1]: number of Iteration
// Arg[2]: Position to write in sector
// Arg[3]: (optional) Device number 
// Arg[4]: (optional) Sector count per operation. 0 for random value from 1~128
// Arg[5]: (optional) Transfer mode
int CmdWriteFile(char* target, char* arg[])
{
	FILE *wfile = NULL;
	char* filebuf;
	char* writebuf;
	//int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	int i; //, j;
	int dev;

	printf("This command only reads first 512 bytes and repeat it!!\n");

	Ite =  (int)_strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	if (arg[4] && arg[4][0]) {
		SectorCount = (int) _strtoi64(arg[4], NULL, 0);
	} else {
		SectorCount = 128;
	}
	filebuf = (char*)malloc(SectorCount * 512);
	writebuf = (char*)malloc(SectorCount * 512);
	wfile = fopen(arg[0], "rb");
	if (wfile == 0) {
		printf("Failed to open file %s\n", arg[0]);
		goto errout;
	}
	retval = (int)fread(filebuf, 1, 512, wfile);
	if (retval != 512) {
		printf("Failed to read file\n");
		goto errout;
	}

	for(i=1;i<SectorCount;i++) {
		memcpy(filebuf+i*512, filebuf, 512);
	}

	if (arg[3] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[3], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	if (arg[5] && arg[5][0]) {
		SetTransferMode(connsock, iTargetID, arg[5]);
	} else {
		if((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
			return retval;
		}
	}

	printf("Iteration: %3d Pos: %I64d, Sectors per operation: %d\n", 
		Ite, Pos, SectorCount);

	for (i=0;i<Ite;i++) {
		memcpy(writebuf, filebuf, 512*SectorCount); // buffer contents is changed after IdeCommand.
		retval = IdeCommand(connsock, (short)iTargetID, 0, WIN_WRITE, (Pos * MB + i*512*SectorCount) / 512, (short)SectorCount, 0, SectorCount * 512, (PCHAR)writebuf, 0, 0);
		if(retval != 0) {
			fprintf(stderr, "\n[NdasCli]main: WRITE Failed... Sector %d\n", (Pos * MB + i*512*SectorCount) / 512);
			goto errout;
		}
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	if (filebuf)
		free(filebuf);
	return retval;
}

// Arg[0]: File to compare
// Arg[1]: number of Iteration
// Arg[2]: Position to write in sector
// Arg[3]: Device number 
// Arg[4]: Sector count per operation
// Arg[5]: (optional) Transfer mode
int CmdCheckFile(char* target, char* arg[])
{
	FILE *wfile = NULL;
	char* filebuf;
	char* diskbuf;
	//int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	int i; // , j;
	int dev;

	Ite =  (int)_strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	if (arg[4] && arg[4][0]) {
		SectorCount = (int) _strtoi64(arg[4], NULL, 0);
	} else {
		SectorCount = 128;
	}
	filebuf = (char*)malloc(SectorCount * 512);
	diskbuf = (char*)malloc(SectorCount * 512);

	wfile = fopen(arg[0], "rb");
	if (wfile == 0) {
		printf("Failed to open file %s\n", arg[0]);
		goto errout;
	}
	retval = (int)fread(filebuf, 1, 512, wfile);
	if (retval != 512) {
		printf("Failed to read file\n");
		goto errout;
	}

	for(i=1;i<SectorCount;i++) {
		memcpy(filebuf+i*512, filebuf, 512);
	}

	if (arg[3] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[3], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	if (arg[5] && arg[5][0]) {
		SetTransferMode(connsock, iTargetID, arg[5]);
	} else {
		if((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
			return retval;
		}
	}

	printf("Iteration: %3d Pos: %I64d, Sectors: %d\n", 
		Ite, Pos, SectorCount);

	for (i=0;i<Ite;i++) {
		retval = IdeCommand(connsock, (short)iTargetID, 0, WIN_READ, (Pos * 512 + i*512*SectorCount) / 512, (short)SectorCount, 0, SectorCount * 512, (PCHAR)diskbuf, 0, 0);
		if(retval != 0) {
			fprintf(stderr, "\n[NdasCli]main: WRITE Failed... Sector %d\n", (Pos * 512 + i*512*SectorCount) / 512);
			goto errout;
		}
		if (memcmp(diskbuf, filebuf, SectorCount * 512)!=0) {
			fprintf(stderr, "Data mismatch at sector %d\n", (UINT32)((Pos * 512 + i*512*SectorCount) / 512));
		}
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	if (filebuf)
		free(filebuf);
	if (diskbuf)
		free(diskbuf);
	return retval;
}


// Arg[0]: File.
// Arg[1]: number of sectors to read
// Arg[2]: Position to read in sector
// Arg[3]: (optional) Device number 
// Arg[4]: (optional) Sector count per operation. 0 for random value from 1~128. Default 1
// Arg[5]: (optional) Transfer mode
int CmdReadFile(char* target, char* arg[])
{
	FILE *wfile = NULL;
	char* filebuf;
	//char* writebuf;
	//int WriteSize;
	int TotalSector;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	int i; //, j;
	int dev;

	TotalSector =  (int)_strtoi64(arg[1], NULL, 0);
	if (TotalSector ==0) {
		printf("Sectors to read should not be 0\n");	
		goto errout;
	}
	Pos = _strtoi64(arg[2], NULL, 0);
	if (arg[4] && arg[4][0]) {
		SectorCount = (int) _strtoi64(arg[4], NULL, 0);
	} else {
		SectorCount = 1;
	}
	filebuf = (char*)malloc(SectorCount * 512);
	wfile = fopen(arg[0], "wb");
	if (wfile == 0) {
		printf("Failed to open file %s\n", arg[0]);
		goto errout;
	}

	if (arg[3] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[3], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	if (arg[5] && arg[5][0]) {
		SetTransferMode(connsock, iTargetID, arg[5]);
	} else {
		if((retval = GetDiskInfo2(connsock, iTargetID)) != 0) {
			fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
			return retval;
		}
	}

	printf("Sector to read: %3d Pos: %I64d, Sectors per operation: %d\n", 
		TotalSector, Pos, SectorCount);

	for (i=0;i<TotalSector;i+=SectorCount) {
		retval = IdeCommand(connsock, (short)iTargetID, 0, WIN_READ, Pos + i, (short)SectorCount, 0, SectorCount * 512, (PCHAR)filebuf, 0, 0);
		if(retval != 0) {
			fprintf(stderr, "\n[NdasCli]main: READ Failed... Sector %I64d\n", (Pos + i));
			goto errout;
		}
		fwrite(filebuf, SectorCount, 512, wfile);
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	if (filebuf)
		free(filebuf);
	if (wfile)
		fclose(wfile);
	return retval;
}


// Arg[0]: Size in MB per iteration
// Arg[1]: number of Iteration
// Arg[2]: Position to read in MB
// Arg[3]: Sector count per operation. 0 for random value from 1~128
int CmdWrite(char* target, char* arg[])
{
	int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int SectorCount;
	SOCKET connsock;
	UINT32 UserId;
	UCHAR* data;
	int i, j;
	int Blocks;
	int MAX_REQ;
	clock_t start_time, end_time;
	NDASCLI_TASK	tasks[NDASCLI_MAX_TASKS];
	int task_pending_sn, task_next_sn, pending_tasks;

	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Ite = (int)_strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	SectorCount = (int) _strtoi64(arg[3], NULL, 0);
	task_pending_sn = 0;
	task_next_sn = 0;
	pending_tasks = 0;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

#if 1
	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

#else

	PerTarget[iTargetID].bLBA = TRUE;
	PerTarget[iTargetID].bLBA48 = TRUE;
	PerTarget[iTargetID].SectorCount = 0x100000000;
	PerTarget[iTargetID].bSmartSupported = FALSE;
	PerTarget[iTargetID].bSmartEnabled = FALSE;
	PerTarget[iTargetID].bPIO = FALSE;

#endif

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);
	memset(data, 0xaa, MAX_DATA_BUFFER_SIZE);

	for(i = 0; i < NDASCLI_MAX_TASKS; i++) {
		tasks[i].TargetId = iTargetID;
		tasks[i].LUN = 0;
		tasks[i].BufferLength = SectorCount * 512;
		tasks[i].Buffer = (PCHAR)data;
		tasks[i].Option = 0;
		tasks[i].TaskTag = 0;
		tasks[i].IdeCommand = WIN_WRITE;
		tasks[i].SentIdeCommand = 0;
		tasks[i].Location = 0;
		tasks[i].SectorCount = 0;
		tasks[i].Feature = 0;
		tasks[i].Info = 0;
	}

	printf("Size: %dMB, Pos: %I64d, Sectors: %d, Iteration: %3d - ", 
		WriteSize, Pos, SectorCount, Ite);
	start_time = clock();

	for (i=0;i<Ite;i++) {
		if (SectorCount==0)
			Blocks = (rand() % 128) + 1;
		else 
			Blocks = SectorCount;
		MAX_REQ = Blocks * 512;

		for (j = 0; j < WriteSize * MB; j+=MAX_REQ) {
			tasks[task_next_sn % NDASCLI_MAX_TASKS].Location = (Pos * MB + j) / 512;
			tasks[task_next_sn % NDASCLI_MAX_TASKS].SectorCount = (short)Blocks;

			retval = SendIdeCommandRequestAndData(connsock, &tasks[task_next_sn % NDASCLI_MAX_TASKS]);
			if(retval != 0) {
				fprintf(stderr, "\n[NdasCli]main: SendIdeCommandRequestAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
				goto errout;
			}
			if (j!=0 && j%(1024*1024*1024) == 0) {
				fprintf(stderr, "\n%d/%dGB", (int)(j/1024/1024/1024), (int)(WriteSize /1024));
			}

			// Check the pending task
			pending_tasks ++;
			task_next_sn ++;
			if(pending_tasks == MaxPendingTasks) {
				retval = ReceiveIdeCommandReplyAndData(connsock, &tasks[task_pending_sn % NDASCLI_MAX_TASKS]);
				if(retval != 0) {
					fprintf(stderr, "\n[NdasCli]main: ReceiveIdeCommandReplyAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
					goto errout;
				}
				task_pending_sn ++;
				pending_tasks --;
			}
		}
		// Receive the replies of pending tasks.
		for( ; pending_tasks > 0; pending_tasks--) {
				retval = ReceiveIdeCommandReplyAndData(connsock, &tasks[task_pending_sn % NDASCLI_MAX_TASKS]);
				if(retval != 0) {
					fprintf(stderr, "\n[NdasCli]main: ReceiveIdeCommandReplyAndData() Failed... Sector %d\n", (Pos * MB + j) / 512);
					goto errout;
				}
				task_pending_sn ++;
		}
	}
	end_time = clock();
	printf(" %.1f MB/sec\n", 1.*WriteSize*Ite*CLK_TCK/(end_time-start_time));

	DisconnectFromNdas(connsock, UserId);
errout:
	if (data)
		free(data);
	return retval;
}

// Arg[0]: Pattern#
// Arg[1]: Read size
// Arg[2]: Pos
// Arg[3]: Blocksize. 0 for random. Optional
// Arg[4]: userid. Optional
// Arg[5]: pw. Optional
int CmdReadPattern(char* target, char* arg[])
{
	clock_t start_time, end_time;
	int Pattern;
	int IoSize;
	INT64 Pos;
	int retval = 0;
	int UserId;
	SOCKET connsock;
	int Blocks;

	srand( (unsigned)GetTickCount() );

	Pattern = (int) _strtoi64(arg[0], NULL, 0);
	IoSize = (int) _strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);

	if (arg[3]) {
		Blocks = (int) _strtoi64(arg[3], NULL, 0);
	} else {
		Blocks = 0;
	}
	if (arg[4]) {
		UserId = (int) _strtoi64(arg[4], NULL, 0);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	}


	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[5]) !=0)
		goto errout;


	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	fprintf(stderr, "Pattern #%d, IO position=%I64dMB, IO Size=%dMB, Random block size\n", Pattern, Pos, IoSize);

	start_time = clock();

	retval = ReadPattern(connsock, Pattern, Pos, IoSize, Blocks);
	if (retval !=0) {
		printf("Read failed\n");
		goto errout;
	}
	end_time = clock();
	printf(" %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));
	DisconnectFromNdas(connsock, UserId);

errout:
	return retval;
}


// Arg[0]: Pattern#
// Arg[1]: Read size
// Arg[2]: Pos
// Arg[3]: Blocksize. 0 for random. Optional
// Arg[4]: userid. Optional
// Arg[5]: pw. Optional
int CmdWritePattern(char* target, char* arg[])
{
	clock_t start_time, end_time;
	int Pattern;
	int IoSize;
	INT64 Pos;
	int retval = 0;
	int UserId;
	SOCKET connsock;
	int Blocks;
	srand( (unsigned)GetTickCount() );
	int LockMode;
	Pattern = (int) _strtoi64(arg[0], NULL, 0);
	IoSize = (int) _strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);

	if (arg[3]) {
		Blocks = (int) _strtoi64(arg[3], NULL, 0);
	} else {
		Blocks = 0;
	}
	if (arg[4]) {
		UserId = (int) _strtoi64(arg[4], NULL, 0);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	}

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[5]) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	fprintf(stderr, "Pattern #%d, IO position=%I64dMB, IO Size=%dMB, ", Pattern, Pos, IoSize);
	if (Blocks ==0) { 
		fprintf(stderr, "Random block size\n");
	} else {
		fprintf(stderr, "Block size=%d\n", Blocks);
	}
	start_time = clock();
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		LockMode = 0x21;
	} else {
		LockMode = 0x14;
	}

	retval = WritePattern(connsock, Pattern, Pos, IoSize, Blocks, LockMode);
	if (retval !=0) {
		printf("Write failed\n");
		goto errout;
	}
	end_time = clock();
	printf(" %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));
	DisconnectFromNdas(connsock, UserId);

errout:
	return retval;
}



// Arg[0]: Io size
// Arg[1]: Pos
int CmdInterleavedIo(char* target, char* arg[])
{
	clock_t start_time, end_time;
	int IoSize;
	INT64 Pos;
	int retval = 0;
	int UserId;
	SOCKET connsock;
	int Blocks = 128;
	int Pattern;
	int Pattern2;
	srand( (unsigned)GetTickCount() );
	PBYTE data;
	BOOL Locked;
	UINT64 PosByte;
	UINT32 IoSizeByte;
	UINT32 TransferSize;
	UINT32 j;
	UINT32 WriteLockCount;
	
	IoSize = (int) _strtoi64(arg[0], NULL, 0);
	Pos = _strtoi64(arg[1], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	fprintf(stderr, "Write/Read Interleaving IO position=%I64dMB, IO Size=%dMB, Block Size=%d\n", Pos, IoSize, Blocks);

	Pattern = rand() % GetNumberOfPattern();
	start_time = clock();
	
	Locked =FALSE;
	IoSizeByte = IoSize * MB;
	PosByte = Pos * MB;
	TransferSize = Blocks * 512;

	for (j = 0; j < IoSizeByte;j+=TransferSize) {
		FillPattern(Pattern, j, data, TransferSize);
		if (Locked == FALSE) {
			retval = VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to get write lock\n");
				goto errout;
			}
			Locked = TRUE;
		}
		// Write
		retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, &WriteLockCount);
		if (retval !=0) {
			fprintf(stderr, "Failed to write\n");
			goto errout;
		}

		if (Locked == TRUE && WriteLockCount >1) {
			retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to free write lock\n");
				goto errout;
			}
			Locked = FALSE;
		} 
		// Read
	
		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		if (retval == -2) { // CRC error. try one more time.
			fprintf(stderr, "CRC errored. Retrying\n");
			retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		}
		if(retval != 0) {
			fprintf(stderr, "[NdasCli]main: READ Failed... Sector %d\n", (Pos * MB + j) / 512);
			goto errout;
		}
		if (FALSE==CheckPattern(Pattern, (int)j, data, Blocks * 512)) {
			retval = -1;
			goto errout;
		}
	}
	if (Locked) {
		retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
		if (retval !=0) {
			fprintf(stderr, "Failed to free write lock\n");
			goto errout;
		}
		Locked = FALSE;
	}
	end_time = clock();
	printf("Write and read %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));


	fprintf(stderr, "Read/write Interleaving IO position=%I64dMB, IO Size=%dMB, Block Size=%d\n", Pos, IoSize, Blocks);

	start_time = clock();
	
	Locked =FALSE;
	IoSizeByte = IoSize * MB;
	PosByte = Pos * MB;
	TransferSize = Blocks * 512;
	Pattern2 = rand() % GetNumberOfPattern();

	for (j = 0; j < IoSizeByte;j+=TransferSize) {
		// Read
		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		if (retval == -2) { // CRC error. try one more time.
			fprintf(stderr, "CRC errored. Retrying\n");
			retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		}
		if(retval != 0) {
			fprintf(stderr, "[NdasCli]main: READ Failed... Sector %d\n", (Pos * MB + j) / 512);
			goto errout;
		}
		if (FALSE==CheckPattern(Pattern, (int)j, data, Blocks * 512)) {
			retval = -1;
			goto errout;
		}

		FillPattern(Pattern2, j, data, TransferSize);
		if (Locked == FALSE) {
			retval = VendorCommand(connsock, VENDOR_OP_GET_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to get write lock\n");
				goto errout;
			}
			Locked = TRUE;
		}
		// Write
		retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (PosByte + j) / 512, (short)Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, &WriteLockCount);
		if (retval !=0) {
			fprintf(stderr, "Failed to write\n");
			goto errout;
		}

		if (Locked == TRUE && WriteLockCount >1) {
			retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
			if (retval !=0) {
				fprintf(stderr, "Failed to free write lock\n");
				goto errout;
			}
			Locked = FALSE;
		} 
	}
	if (Locked) {
		retval = VendorCommand(connsock, VENDOR_OP_FREE_WRITE_LOCK, 0, 0, 0, NULL, 0, NULL);
		if (retval !=0) {
			fprintf(stderr, "Failed to free write lock\n");
			goto errout;
		}
		Locked = FALSE;
	}
	end_time = clock();
	printf("Read and write %.1f MB/sec\n", 1.*IoSize*CLK_TCK/(end_time-start_time));

	DisconnectFromNdas(connsock, UserId);

errout:
	if (data)
		free(data);
	return retval;
}



// arg[0]: User1 password. Optional
int CmdGetConfig(char* target, char* arg[])
{

	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	UINT32 Val;
	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) !=0)
		return -1;

	if (ActiveHwVersion == LANSCSIIDE_VERSION_1_0) {
		printf("Getting config is not supported in NDAS 1.0\n");
		DisconnectFromNdas(connsock, UserId);
		return 0;
	}

	// Get Max ret time
	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);
	iResult = VendorCommand(connsock, VENDOR_OP_GET_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		Val = htonl(Param2);
	} else {
		Val = htonl(Param1);
	}
	printf("Max retransmission time = %d msec\n", Val + 1);

	// Get Max con time
	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);
	iResult = VendorCommand(connsock, VENDOR_OP_GET_MAX_CONN_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		Val = htonl(Param2);
	} else {
		Val = htonl(Param1);
	}
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {
		printf("Connection timeout = %d sec\n", Val + 1);
	} else {
		printf("Connection timeout = %d msec\n", Val + 1);
	}
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		// Get heart-beat time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_HEART_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
		Val = htonl(Param2);
		printf("Heartbeat time = %d sec\n", Val + 1);
	}

	// Get standby time
	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);
	iResult = VendorCommand(connsock, VENDOR_OP_GET_STANBY_TIMER, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		Val = htonl(Param2);
	} else {
		Val = htonl(Param1);
	}
	if (Val & (0x1<<31)) {
		printf("Standby time = %d min\n", (Val & 0x7fffffff)+1);
	} else {
		printf("Standby time = DISABLED\n");
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {
		// Get delay time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_DELAY, &Param0, &Param1, &Param2, NULL, 0, NULL);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Val = htonl(Param2);
		} else {
			Val = htonl(Param1);
		}
		printf("Inter-packet Delay time = %d nsec\n", (Val + 1) * 8);
	}

	// Get dynamic max ret time
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_DYNAMIC_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Val = htonl(Param2);
		} else {
			Val = htonl(Param1);
		}

		printf("Dynamic max-retransmission time = %d msec\n", Val + 1);
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		// Get option
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_D_OPT, &Param0, &Param1, &Param2, NULL, 0, NULL);
		Val = htonl(Param2);
		printf("Dynamic Option value=0x%02x\n", Val);
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		// Get deadlock time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_DEAD_LOCK_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
		Val = htonl(Param2);
		printf("Deadlock time= %d sec\n", Val+1);
	}
	
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		// Get watchdog time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_WATCHDOG_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
		Val = htonl(Param2);
		if (Val ==0) {
			printf("Watchdog time= DISABLED\n");
		} else {
			printf("Watchdog time= %d sec\n", Val+1);
		}
	}
	DisconnectFromNdas(connsock, UserId);
	return 0;
}


//
// Arg[0]: Interpacket delay( x 8ns)
//
int CmdDelayedIo(char* target, char* arg[])
{
	UINT64 Pos = 10000;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	static const short Blocks = 128; // Number of sectors to read 
	int Delay;
	UINT32 Param0, Param1, Param2;

	Delay = (int) _strtoi64(arg[0], NULL, 0);

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	
	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	fprintf(stderr, "Setting interpacket delay to %d micro-seconds\n", (Delay+1) * 8 / 1000);
	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(Delay);
	iResult = VendorCommand(connsock, VENDOR_OP_SET_DELAY, &Param0, &Param1, &Param2, NULL, 0, NULL);

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}
	fprintf(stderr, "Reading ");
	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	if (retval == -2) { // CRC error. try one more time.
		fprintf(stderr, "CRC errored. Retrying\n");
		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	}
	if(retval != 0) {
		fprintf(stderr, "CmdDelayedIo: READ Failed... Sector %d\n", (Pos * MB) / 512);
		goto errout;
	} else {
		fprintf(stderr, "succeeded\n");
	}
	
	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;
}

// Arg[0]: New MAC address
int CmdSetMac(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	BOOL	Ret;
	char EepromBuf[32] = {0};
	int UserId;
	UINT32 Param0, Param1, Param2;

	LPX_ADDRESS			NewAddress;

	Ret = lpx_addr(arg[0], &NewAddress);
	if (Ret == 0) {
		fprintf(stderr, "Invalid MAC\n");
		return 0;
	}
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;
	if (ActiveHwVersion == LANSCSIIDE_VERSION_2_5) {
		// Get current contents
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(16);
		iResult = VendorCommand(connsock, VENDOR_OP_GET_EEP, &Param0, &Param1, &Param2, NULL, 0, EepromBuf);

		if (iResult < 0) {
			printf("Failed to get EEP\n");
			goto errout;
		} 
		memcpy(EepromBuf, NewAddress.Node, 6);
		ReverseBytes((PUCHAR)EepromBuf, 6);
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(16);
		iResult = VendorCommand(connsock, VENDOR_OP_SET_EEP, &Param0, &Param1, &Param2, NULL, 0, EepromBuf);

		if (iResult < 0) {
			printf("Failed to set EEP\n");
			goto errout;
		} 

		fprintf(stderr, "Changed MAC address to %02x:%02x:%02x:%02x:%02x:%02x\n", 
			NewAddress.Node[0], NewAddress.Node[1], NewAddress.Node[2],
			NewAddress.Node[3], NewAddress.Node[4], NewAddress.Node[5]
		);
	} else if (ActiveHwVersion == LANSCSIIDE_VERSION_2_0) {
		memcpy(((char*)&Param0)+2, &NewAddress.Node[0], 2);
		memcpy((char*)&Param1, &NewAddress.Node[2], 4);

		iResult = VendorCommand(connsock, VENDOR_OP_SET_MAC, &Param0, &Param1, &Param2, NULL, 0, NULL);
		if (iResult < 0) {
			printf("Failed to set MAC\n");
			goto errout;
		} 

		fprintf(stderr, "Changed MAC address to %02x:%02x:%02x:%02x:%02x:%02x\n", 
			NewAddress.Node[0], NewAddress.Node[1], NewAddress.Node[2],
			NewAddress.Node[3], NewAddress.Node[4], NewAddress.Node[5]
		);
	} else {
		fprintf(stderr, "SetMAC is not supported for this version\n");
	}
	DisconnectFromNdas(connsock, UserId);

errout:
	return 0;
}

// arg[0]: Lock Id
int CmdMutexTest1(char* target, char* arg[])
{
	SOCKET	connsock;
	int		iResult;
	int		retVal = 0;
	
	int		UserId;
	UINT32	Param0 = 0, Param1 = 0, Param2 = 0;
	BYTE*	Param0Byte = (BYTE*)&Param0;
	int		LockNumber;
	CHAR	LockData[64+1] = {0};
//	int i;
	int iteration = 10000;
	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);

	if (arg[0] == 0) {
		printf("Lock id is required\n");
		return 0;
	}
	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	LockNumber = (int) _strtoi64(arg[0], NULL, 0);

	while (iteration > 0) {
		Param0Byte[3] = (BYTE)LockNumber;
		iResult = VendorCommand(connsock, VENDOR_OP_SET_MUTEX, &Param0, &Param1, &Param2, LockData, 0, NULL);
		if (iResult ==0) {
			UINT32 Num = 0;
			LockData[64] = 0;
			if (LockData[0] != 0) {
				sscanf(LockData, "%d", &Num);
			}
			Num++;
			sprintf(LockData, "%8d", Num);

			Sleep(1);
			Param0Byte[3] = (BYTE)LockNumber;
			iResult = VendorCommand(connsock, VENDOR_OP_FREE_MUTEX, &Param0, &Param1, &Param2, LockData, 64, NULL);
			if (iResult != 0) {
				printf("Failed to free mutex\n");
				goto errout;
			}
			iteration--;
			if (iteration%1000)
				printf(".");
		}
	}
	DisconnectFromNdas(connsock, UserId);
errout:
	return 0;
}

// Login -> IO -> wait for minutes -> try vendor command -> try IO -> logout
int CmdStandbyTest(char* target, char* arg[])
{

	UINT64 Pos = 10000;
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	int					iResult;
	unsigned			UserId;
	static const short Blocks = 128; // Number of sectors to read 
	int wait_time = 60 * 8;
	int i;
	UINT32 Param0, Param1, Param2;

	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);

	// Need to get disk info before IO
	if((iResult = GetDiskInfo(connsock, iTargetID, FALSE, TRUE)) != 0) {
		fprintf(stderr, "GetDiskInfo Failed...\n");
		retval = iResult;
		goto errout;
	}
	fprintf(stderr, "Reading ");
	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
	if(retval != 0) {
		fprintf(stderr, "StandbyTest: READ Failed... Sector %d\n", (Pos * MB) / 512);
		goto errout;
	} else {
		fprintf(stderr, "succeeded\n");
	}
	fprintf(stderr, "Waiting %d seconds", wait_time);
	for(i=0;i<wait_time/10;i++) {
		Sleep(10 * 1000);
		fprintf(stderr, ".");
	}

	fprintf(stderr, "Try vendor command ");
	
	retval = VendorCommand(connsock, VENDOR_OP_GET_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
	if (retval !=0)	{
		fprintf(stderr, " Failed");
	} else {
		fprintf(stderr, "succeeded\n");
	}

	fprintf(stderr, "Reading ");

	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, ((Pos+100) * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);

	if (retval !=0)	{
		fprintf(stderr, " Failed");
	} else {
		fprintf(stderr, "succeeded\n");
	}

	fprintf(stderr, "Writing ");

	retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, ((Pos+100) * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);

	if (retval !=0)	{
		fprintf(stderr, " Failed");
	} else {
		fprintf(stderr, "succeeded\n");
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	if (data)
		free(data);
	return retval;

}

//
// arg[0]: Dev number. Optional. Default value is 0.
//
int CmdStandby(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	unsigned			UserId;
	int dev;
	if (arg[0] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[0], NULL, 0);
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

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	retval = IdeCommand(connsock, iTargetID, 0, WIN_STANDBYNOW1, 0, 0, 0, 0, 0, 0, 0);
//	retval = IdeCommand(connsock, iTargetID, 0, WIN_STANDBY, 0, 0, 0, 0, 0, 0);
	if(retval != 0) {
		fprintf(stderr, "Standby failed\n");
		goto errout;
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	return retval;

}

//
// arg[0]: Target disk
// arg[1]: 'r' or 'w'
//

int CmdStandbyIo(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	unsigned			UserId;
	int dev;
	UINT64 Pos = 10000;
	static const short Blocks = 64; // Number of sectors to read 
	clock_t start_time;

	if (arg[0] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[0], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid devices number\n");
			return 0;
		}
	}

	if (dev == 0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_EW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_EW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	retval = IdeCommand(connsock, iTargetID, 0, WIN_STANDBYNOW1, 0, 0, 0, 0, 0, 0, 0);
	if(retval != 0) {
		fprintf(stderr, "Standby failed\n");
		goto errout;
	}
	fprintf(stderr, "Wait 10 sec\n");
	Sleep(10 * 1000);

	start_time = clock();
	data = (PUCHAR) malloc(MAX_DATA_BUFFER_SIZE);
	memset(data, 0, MAX_DATA_BUFFER_SIZE);
	if (arg[1]==0 || arg[1][0]=='r' || arg[1][0]=='R') {
		fprintf(stderr, "Reading %d blocks - ", Blocks);
		retval = IdeCommand(connsock, iTargetID, 0, WIN_READ, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		if(retval != 0) {
			fprintf(stderr, "READ Failed... Sector %d\n", (Pos * MB) / 512);
			goto errout;
		} else {
			fprintf(stderr, "succeeded after %d sec\n", (clock() - start_time)/CLK_TCK);
		}
	} else {
		fprintf(stderr, "Writing %d blocks - ", Blocks);
		retval = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, (Pos * MB) / 512, Blocks, 0, MAX_DATA_BUFFER_SIZE, (PCHAR)data, 0, 0);
		if(retval != 0) {
			fprintf(stderr, "WRITE Failed... Sector %d\n", (Pos * MB) / 512);
			goto errout;
		} else {
			fprintf(stderr, "succeeded after %d sec\n", (clock() - start_time)/CLK_TCK);
		}
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	return retval;

}



//
// arg[0]: Dev number. Optional. Default value is 0.
//
int CmdCheckPowerMode(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	unsigned			UserId;
	int dev;
	if (arg[0] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[0], NULL, 0);
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

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

	retval = IdeCommand(connsock, iTargetID, 0, WIN_CHECKPOWERMODE1, 0, 0, 0, 0, 0, 0, 0);

	if(retval != 0) {
		fprintf(stderr, "Standby failed\n");
		goto errout;
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	return retval;

}

int SetWriteCache(SOCKET connsock, int TargetId, BOOL Enable)
{
	struct hd_driveid	info;
	int					iResult;
	
	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]SetWriteCache: Identify Failed...\n");
		return iResult;
	}

	printf("Current setting: Write cache support=%d enabled=%d\n", 
			(info.command_set_1 & 0x20) != 0,
			(info.cfs_enable_1 & 0x20) != 0);
	if((info.command_set_1 & 0x20) == 0) {
		printf("Write cache feature not supported.\n");
		return -1;
	}

	if(Enable)
		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0, (_int8)SETFEATURES_EN_WCACHE, 0, NULL, 0,0 );
	else
		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, 0, (_int8)SETFEATURES_DIS_WCACHE, 0, NULL, 0,0 );

	if (iResult !=0) {
		printf("Failed to set feature\n");
		return -1;
	}

	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}
	printf("Current setting: Write cache support=%d enabled=%d\n", 
		(info.command_set_1 & 0x20) != 0,
		(info.cfs_enable_1 & 0x20) != 0);
	
	return 0;	
}

int SetTransferMode(SOCKET connsock, int TargetId, char* Mode)
{
	struct hd_driveid	info;
	int					iResult;
//	char				buffer[41];
	int mode;
	
	// identify.
	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}

	if(info.field_valid & 0x0002) {
		printf("EDIDE field is valid.\n");
	} else {
		printf("EDIDE field is not valid.\n");
	}
	printf("Current setting: Supported PIO 0x%x, DMA 0x%x, U-DMA 0x%x\n", 
			info.eide_pio_modes,
			info.dma_mword, 
			info.dma_ultra);
	printf("Changing mode from ");
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

		if (mode>4) {
			printf("Invalid mode\n");
			return -1;
		}
		if(mode >= 3) {
			if ((info.eide_pio_modes & (0x1<<(mode-3))) == 0) {
				printf("Unsupported mode\n");
				return -1;
			}
		}
		iResult = IdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, mode | 0x8, SETFEATURES_XFER, 0, NULL, 0,0 );
		PerTarget[TargetId].bPIO = TRUE;
	} else if (Mode[0] == 'd' || Mode[0] == 'D') {
		if (mode>2) {
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
		if (mode>7) {
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
		printf("Unknown mode\n");
		return -1;
	}
	if (iResult !=0) {
		printf("Failed to set feature\n");
		return -1;
	}

	if((iResult = IdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info, 0, 0)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo: Identify Failed...\n");
		return iResult;
	}


	printf("PIO W/O IORDY 0x%x, PIO W/ IORDY 0x%x\n", 
			info.eide_pio, info.eide_pio_iordy);

	printf("DMA 0x%x, U-DMA 0x%x\n", 
			info.dma_mword, info.dma_ultra);
	
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
				
		PerTarget[TargetId].SectorCount = info.lba_capacity;	
	}

	return 0;	
}

// Param 0: Write Size(MB)
// Param 1: Pos in MB
// Param 2: <P/D/U><0~7> . ex) P0, ..., P4, D0, D1, D2, U0, ..., U7
// Param 3: Dev.(Optional)
// Param 4: Sector count(Optional)
int CmdTransferModeIo(char* target, char* arg[])
{
	int WriteSize;
	int Ite = 1;
	UINT64 Pos;
	int retval;
	int dev;
	SOCKET connsock;
	int UserId;
	//int Blocks;
	int LockMode = 0;
	int SectorCount;
	if (arg[2] == 0)  {
		printf("Not enough parameter\n");
		return -1;
	}
	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Pos = _strtoi64(arg[1], NULL, 0);

	if (arg[3] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[3], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (arg[4] == NULL) {
		SectorCount = 128;
	} else {
		SectorCount = (int) _strtoi64(arg[4], NULL, 0);
	}

	LockMode = 0x0;

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = SetTransferMode(connsock, iTargetID, arg[2])) != 0) {
		fprintf(stderr, "SetTransferMode Failed...\n");
		return retval;
	}

	retval = RwPatternChecking(connsock, WriteSize, Ite, Pos, SectorCount, LockMode);

	DisconnectFromNdas(connsock, UserId);
	return retval;
}

// Param 0: <P/D/U><0~7> . ex) P0, ..., P4, D0, D1, D2, U0, ..., U7
// Param 1: Dev.(Optional)
int CmdSetMode(char* target, char* arg[])
{
	int retval;
	int dev;
	SOCKET connsock;
	int UserId;

	if (arg[0] == 0)  {
		printf("Not enough parameter\n");
		return -1;
	}

	if (arg[1] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[1], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return 0;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = SetTransferMode(connsock, iTargetID, arg[0])) != 0) {
		fprintf(stderr, "SetTransferMode Failed...\n");
		return retval;
	}

	DisconnectFromNdas(connsock, UserId);
	return retval;
}

// Param 0: <P/D/U><0~7> . ex) P0, ..., P4, D0, D1, D2, U0, ..., U7
// Param 1: Dev.(Optional)
int CmdWriteCache(char* target, char* arg[])
{
	int retval;
	int dev;
	SOCKET connsock;
	int UserId;
	BOOL enable;

	if (arg[0] == 0)  {
		printf("Not enough parameter\n");
		return -1;
	}

	if (arg[1] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[1], NULL, 0);
		if (dev!=0 && dev!=1) {
			fprintf(stderr, "Invalid device number\n");
			return -1;
		}
	}

	if (dev==0) {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	} else {
		UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW) | 0x100;
	}

	if(_strnicmp(arg[0], "on", 2) == 0) {
		enable = TRUE;
	} else if(_strnicmp(arg[0], "off", 3) == 0) {
		enable = FALSE;
	} else {
		printf("Invalid parameter. Specify 'on' or 'off' parameter.\n");
		return -1;
	}

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = SetWriteCache(connsock, iTargetID, enable)) != 0) {
		fprintf(stderr, "SetWriteCache Failed...\n");
		return retval;
	}

	DisconnectFromNdas(connsock, UserId);
	return retval;
}

// Param 0: Write Size per iteration
// Param 1: Number of iteration
// Param 2: Position to write
int CmdFlushTest(char* target, char* arg[])
{
	int WriteSize;
	int Ite;
	UINT64 Pos;
	int retval;
	int LockMode;
	SOCKET connsock;
	int UserId;
	int Blocks;
	clock_t start_time, end_time;
	int i, iResult;
	int j;
	char Buf[512];
	if (arg[2] == 0)  {
		printf("Not enough parameter\n");
		return -1;
	}
	WriteSize = (int) _strtoi64(arg[0], NULL, 0);
	Ite = (int) _strtoi64(arg[1], NULL, 0);
	Pos = _strtoi64(arg[2], NULL, 0);
	LockMode = 0x0;
	UserId = MAKE_USER_ID(DEFAULT_USER_NUM, USER_PERMISSION_SW);
	Blocks = 128;

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Need to get disk info before IO
	if((retval = GetDiskInfo(connsock, iTargetID, TRUE, TRUE)) != 0) {
		fprintf(stderr, "[NdasCli]GetDiskInfo Failed...\n");
		return retval;
	}

	start_time = clock();
	fprintf(stderr, "Flush after %d writing\n", WriteSize/Ite);

	for(i=0;i<Ite;i++) {
//		fprintf(stderr, "  Writing %dMB:", WriteSize);
#if 0	// Sequential write
		iResult = WritePattern(connsock, 0 /* pattern */, Pos + i * WriteSize, WriteSize, Blocks, LockMode);
#else	// Scatterred write
		for(j=0;j<WriteSize/Ite;j++) {
			iResult = IdeCommand(connsock, iTargetID, 0, WIN_WRITE, Pos + 1000 * i, 1, 0, 512, (PCHAR)Buf, 0, NULL);
		}
#endif
		if (iResult !=0) {
			printf("Writing pattern failed.\n");
			goto errout;
		}
		retval = IdeCommand(connsock, iTargetID, 0, WIN_FLUSH_CACHE, 0, 0, 0, 0, 0, 0, 0);		
		if(retval != 0) {
			fprintf(stderr, "FLUSH_CACHE failed\n");
			goto errout;
		}
	}
	end_time = clock();
//	printf(" %.1f MB/sec\n", 1.*WriteSize * Ite *CLK_TCK/(end_time-start_time));
	printf(" %.4f sec\n", ((double)(end_time-start_time))/CLK_TCK);

	DisconnectFromNdas(connsock, UserId);
errout:	
	return 0;
}

// Param[0]: Retransmit timeout in msec.
int CmdSetRetransmit(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UINT32 Val;

	Val = (int) _strtoi64(arg[0], NULL, 0);

	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	// Set Max ret time
	Param0 = htonl(0);
	Param1 = htonl(0);
	Param2 = htonl(0);
	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
		Param2 = htonl(Val);
	} else {
		Param1 = htonl(Val);
	}

	iResult = VendorCommand(connsock, VENDOR_OP_SET_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);

	DisconnectFromNdas(connsock, UserId);
	return 0;
}

HRESULT
GetHostAddressList(
	LPSOCKET_ADDRESS_LIST* SocketAddressList,
	SOCKET sock)
{
	HRESULT hr;

	*SocketAddressList = NULL;

	//
	// Query Buffer length should not affect last error
	//
	DWORD socketAddressListLength = 0;
	
	LPSOCKET_ADDRESS_LIST socketAddressList = 
		static_cast<LPSOCKET_ADDRESS_LIST>(malloc(socketAddressListLength));

	if (NULL == socketAddressList)
	{
		return E_OUTOFMEMORY;
	}

	while (TRUE)
	{
		DWORD savedError = GetLastError();

		int sockret;

		sockret = WSAIoctl(
			sock, 
			SIO_ADDRESS_LIST_QUERY, 
			0, 0, 
			socketAddressList,
			socketAddressListLength, 
			&socketAddressListLength, 
			NULL, NULL);

		if (sockret != SOCKET_ERROR)
		{
			SetLastError(savedError);
			*SocketAddressList = socketAddressList;
			return S_OK;
		}

		if (WSAEFAULT != WSAGetLastError())
		{
			hr = HRESULT_FROM_WIN32(WSAGetLastError());
			free(socketAddressList);
			return hr;
		}

		SetLastError(savedError);

		PVOID p = realloc(socketAddressList, socketAddressListLength);

		if (NULL == p)
		{
			free(socketAddressList);
			return E_OUTOFMEMORY;
		}

		socketAddressList = (LPSOCKET_ADDRESS_LIST) p;
	}
}



typedef struct _RECV_CONTEXT {
	SOCKADDR_LPX NicAddr;
	SOCKET sock;
	WSABUF WsBuf;
	UCHAR PacketBuffer[1024];
	DWORD ReceivedBytes;
	DWORD Flags;
	SOCKADDR_LPX FromAddr;
	INT	FromLen;
	WSAOVERLAPPED Overlapped;
	BOOLEAN ReceivePending;
} RECV_CONTEXT, *PRECV_CONTEXT;

BOOL SetDefaultConfig(UCHAR* NicAddr, UCHAR* TargetAddr)
{
	SOCKET				sock;
	int					iResult;
	int retVal = 0;
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	UINT32 Val;
	SOCKADDR_LPX NicLpxAddr, TargetLpxAddr;

	sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);

	if(sock == INVALID_SOCKET) {
		PrintError(WSAGetLastError(), _T("MakeConnection: socket "));
		return FALSE;
	}
	
	memset(&NicLpxAddr, 0, sizeof(NicLpxAddr));
	NicLpxAddr.sin_family = AF_LPX;
	memcpy(NicLpxAddr.LpxAddress.Node, NicAddr, 6);
	NicLpxAddr.LpxAddress.Port = 0; // unspecified
		
	// Bind NIC.
	iResult = bind(sock, (struct sockaddr *)&NicLpxAddr, sizeof(NicLpxAddr));
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), _T("MakeConnection: bind "));
		closesocket(sock);
		return FALSE;
	}
		
	memset(&TargetLpxAddr, 0, sizeof(TargetLpxAddr));
	TargetLpxAddr.sin_family = AF_LPX;
	memcpy(TargetLpxAddr.LpxAddress.Node, TargetAddr, 6);
	TargetLpxAddr.LpxAddress.Port = htons(LPX_PORT_NUMBER);
		
	iResult = connect(sock, (struct sockaddr *)&TargetLpxAddr, sizeof(TargetLpxAddr));
	if(iResult == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), _T("MakeConnection: connect "));
		closesocket(sock);
		return FALSE;
	}

	/* Now connected to target */
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	memcpy(Password, def_supervisor_password, PASSWORD_LENGTH);

	iResult = Login(sock, LOGIN_TYPE_NORMAL, UserId, Password, TRUE);
	
	if (iResult != 0) {
		fprintf(stderr, "Failed to login\n");
		return FALSE;
	}

	fprintf(stderr, "Updating config");
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
//			Param1 = htonl(199);
			Param1 = htonl(199);
		}
	}

	fprintf(stderr, ".");
	iResult = VendorCommand(sock, VENDOR_OP_SET_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
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
	fprintf(stderr, ".");
	iResult = VendorCommand(sock, VENDOR_OP_SET_MAX_CONN_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_1) {
		// Set standby time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Param2 = htonl(30 | (1<<31));
		} else {
			Param1 = htonl(30 | (1<<31));
		}
		fprintf(stderr, ".");
		iResult = VendorCommand(sock, VENDOR_OP_SET_STANBY_TIMER, &Param0, &Param1, &Param2, NULL, 0, NULL);
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {
		// Set delay time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Param2 = htonl(7);
		} else {
			Param1 = htonl(7);
		}
		fprintf(stderr, ".");
		iResult = VendorCommand(sock, VENDOR_OP_GET_DELAY, &Param0, &Param1, &Param2, NULL, 0, NULL);
	}

	DisconnectFromNdas(sock, UserId);
	fprintf(stderr, "Done\n");
	return TRUE;
}

template <>
struct std::less<SOCKADDR_LPX> {
	bool operator() (const SOCKADDR_LPX& lhs, const SOCKADDR_LPX& rhs) const {
		return (memcmp(lhs.LpxAddress.Node, rhs.LpxAddress.Node, 6)<0)?true:false;
	}
};

//struct LpxAddrComp : public std::binary_function<SOCKADDR_LPX, SOCKADDR_LPX, bool> {
//	bool operator() (const SOCKADDR_LPX& lhs, const SOCKADDR_LPX& rhs) const {
//		return (memcmp(lhs.LpxAddress.Node, rhs.LpxAddress.Node, 6)<0)?true:false;
//	}
//};

int CmdSetDefaultConfigAuto(char* target, char* arg[])
{
	int result;
	int broadcastPermission;
	int i;
	SOCKET			sock;
	LPSOCKET_ADDRESS_LIST SocketAddressList = NULL;
	HRESULT hr;
	PRECV_CONTEXT recvContexts = NULL;
	WSAEVENT* EventArray = NULL;
	DWORD WaitResult;
	std::set<SOCKADDR_LPX> ProcessedList;

	// Create Listen Socket.
	sock = socket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), _T("socket"));
		return -1;
	}
	
	hr = GetHostAddressList(
		&SocketAddressList,sock);
	if (FAILED(hr)) {
		PrintError(WSAGetLastError(), _T("get address list"));
		goto errout;
	}
	
	recvContexts = (PRECV_CONTEXT)malloc(sizeof(RECV_CONTEXT) * SocketAddressList->iAddressCount);
	memset(recvContexts, 0, sizeof(RECV_CONTEXT) * SocketAddressList->iAddressCount);
	EventArray = (WSAEVENT*) malloc(sizeof(WSAEVENT) * SocketAddressList->iAddressCount);
	for (i = 0; i < SocketAddressList->iAddressCount; ++i)
	{
		const SOCKET_ADDRESS* srcSocketAddr = 
			&SocketAddressList->Address[i];
		PSOCKADDR_LPX lpxAddr = (PSOCKADDR_LPX) srcSocketAddr->lpSockaddr;
		fprintf(stderr, "NIC address: %02x:%02x:%02x:%02x:%02x:%02x\n", 
			lpxAddr->LpxAddress.Node[0],
			lpxAddr->LpxAddress.Node[1],
			lpxAddr->LpxAddress.Node[2],
			lpxAddr->LpxAddress.Node[3],
			lpxAddr->LpxAddress.Node[4],
			lpxAddr->LpxAddress.Node[5]);
		recvContexts[i].sock = WSASocket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == sock) {
			PrintError(WSAGetLastError(), _T("socket"));
			goto errout;
		}

		broadcastPermission = 1;
		if (setsockopt(recvContexts[i].sock, SOL_SOCKET, SO_BROADCAST,
				(const char*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
			fprintf(stderr, "Can't setsockopt for broadcast: %d\n", errno);
			goto errout;
		}

		memcpy(&recvContexts[i].NicAddr, lpxAddr, sizeof(SOCKADDR_LPX));
		recvContexts[i].NicAddr.sin_family = AF_LPX;
		recvContexts[i].NicAddr.LpxAddress.Port = htons(10002);


		result = bind(
			recvContexts[i].sock, 
			(struct sockaddr *)&recvContexts[i].NicAddr, 
			sizeof(recvContexts[i].NicAddr));
		if (result < 0) {
			fprintf(stderr, "Error! when binding...: %d. Please execute \"net stop ndassvc\"\n", WSAGetLastError());
			goto errout;
		}

		EventArray[i] = WSACreateEvent();
		recvContexts[i].Overlapped.hEvent = EventArray[i];
		recvContexts[i].ReceivePending = FALSE;
		recvContexts[i].Flags = 0;
		recvContexts[i].ReceivedBytes = 0;
		recvContexts[i].WsBuf.len = sizeof(recvContexts[i].PacketBuffer);
		recvContexts[i].WsBuf.buf = (char*)recvContexts[i].PacketBuffer;
	}

	while (1) {
		for (i = 0; i < SocketAddressList->iAddressCount; ++i) {
			while(!recvContexts[i].ReceivePending) {
				recvContexts[i].FromLen = sizeof(recvContexts[i].FromAddr);
				ZeroMemory(&recvContexts[i].Overlapped, sizeof(WSAOVERLAPPED));
				recvContexts[i].Overlapped.hEvent = EventArray[i];
				WSAResetEvent(recvContexts[i].Overlapped.hEvent);

				result = WSARecvFrom(
						recvContexts[i].sock, 
						&recvContexts[i].WsBuf, 1, 
						&recvContexts[i].ReceivedBytes,
						&recvContexts[i].Flags, 
						(struct sockaddr*) &recvContexts[i].FromAddr,
						&recvContexts[i].FromLen, 
						&recvContexts[i].Overlapped, 
						NULL);
				if (result == 0) {

				} else if (WSAGetLastError() == WSA_IO_PENDING) {
					recvContexts[i].ReceivePending = TRUE;
				} else {
					printf("Recv error: %d, %d\n", result, WSAGetLastError());
					goto errout;
				}
			}
		}

		WaitResult = WSAWaitForMultipleEvents(
						SocketAddressList->iAddressCount,
						EventArray,
						FALSE,
						WSA_INFINITE,
						FALSE);
		if (WSA_WAIT_FAILED != WaitResult) {
			int index = WaitResult - WSA_WAIT_EVENT_0;
			DWORD received;
			DWORD Flags;
			BOOL success;
			success = WSAGetOverlappedResult(recvContexts[index].sock, 
				&recvContexts[index].Overlapped, &received,
				TRUE, &Flags);
			if (success) {
				if (ProcessedList.find(recvContexts[index].FromAddr) == ProcessedList.end()) {
					fprintf(stderr, "Heartbeat %02x:%02x from %02x:%02x:%02x:%02x:%02x:%02x - ",
						recvContexts[index].PacketBuffer[0], 
						recvContexts[index].PacketBuffer[1],
						recvContexts[index].FromAddr.LpxAddress.Node[0], 
						recvContexts[index].FromAddr.LpxAddress.Node[1], 
						recvContexts[index].FromAddr.LpxAddress.Node[2], 
						recvContexts[index].FromAddr.LpxAddress.Node[3], 
						recvContexts[index].FromAddr.LpxAddress.Node[4], 
						recvContexts[index].FromAddr.LpxAddress.Node[5]);
					if (recvContexts[index].PacketBuffer[1] == 0x01) {
						SetDefaultConfig(recvContexts[index].NicAddr.LpxAddress.Node, recvContexts[index].FromAddr.LpxAddress.Node);
					} else {
						fprintf(stderr, "Not NDAS 1.1. Ignoring.\n");
					}
					ProcessedList.insert(recvContexts[index].FromAddr);
				}
				recvContexts[index].ReceivePending = FALSE;
			} else {
				printf("WSAGetOverlappedResult failed.\n");
			}
		} else {
			printf("Wait failed: 0x%x\n", WaitResult);
			goto errout;
		}
	}
	
errout:
	if (EventArray)
		free(EventArray);
	if (recvContexts)
		free(recvContexts);
	if (SocketAddressList)
		free(SocketAddressList);
	closesocket(sock);
	return 0;
}

int CmdSetDefaultConfig(char* target, char* arg[])
{
	SOCKET				connsock;
	int					iResult;
	int retVal = 0;
	
	int UserId;
	UINT32 Param0, Param1, Param2;
	UCHAR Password[PASSWORD_LENGTH] = {0};
	UINT32 Val;
	UserId = MAKE_USER_ID(SUPERVISOR_USER_NUM, USER_PERMISSION_EW);

	if (ConnectToNdas(&connsock, target, UserId, (PUCHAR)arg[0]) !=0)
		return -1;

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

	iResult = VendorCommand(connsock, VENDOR_OP_SET_MAX_RET_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);
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

	iResult = VendorCommand(connsock, VENDOR_OP_SET_MAX_CONN_TIME, &Param0, &Param1, &Param2, NULL, 0, NULL);

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_1_0) {
		// Set standby time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Param2 = htonl(30 | (1<<31));
		} else {
			Param1 = htonl(30 | (1<<31));
		}

		iResult = VendorCommand(connsock, VENDOR_OP_SET_STANBY_TIMER, &Param0, &Param1, &Param2, NULL, 0, NULL);
	}

	if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_0) {
		// Set delay time
		Param0 = htonl(0);
		Param1 = htonl(0);
		Param2 = htonl(0);
		if (ActiveHwVersion >= LANSCSIIDE_VERSION_2_5) {
			Param2 = htonl(7);
		} else {
			Param1 = htonl(7);
		}
		iResult = VendorCommand(connsock, VENDOR_OP_GET_DELAY, &Param0, &Param1, &Param2, NULL, 0, NULL);
	}

	DisconnectFromNdas(connsock, UserId);
	return 0;
}

extern void
ConvertString(
			  PCHAR	result,
			  PCHAR	source,
			  int	size
			  );

//
// arg[0]: Dev number. Optional. Default value is 0.
//
int CmdIdentify(char* target, char* arg[])
{
	SOCKET connsock;
	int UserId;
	int iResult;
	int dev;
	struct hd_driveid	info;
	char				buffer[41];
	int bSmartSupported, bSmartEnabled;

	if (arg[0] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[0], NULL, 0);
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

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		return -1;

	iResult = IdeCommand(connsock, iTargetID, 0, WIN_IDENTIFY, 0, 0, 0, sizeof(info), (PCHAR)&info,0, 0);

	if (iResult !=0) {
		fprintf(stderr, "Identify Failed...\n");
		goto errout;
	}

	printf("Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
			iTargetID, info.major_rev_num, info.minor_rev_num, info.capability);
	printf("dma_mword 0x%x, U-DMA 0x%x - Current mode=", 
			info.dma_mword, info.dma_ultra);

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

	printf("Supported PIO mode 0x%x\n", 
				info.eide_pio_modes);
		
	printf("PIO W/O IORDY 0x%x, PIO W/ IORDY 0x%x\n", 
			info.eide_pio, info.eide_pio_iordy);

	ConvertString((PCHAR)buffer, (PCHAR)info.serial_no, 20);

	printf("Serial No: %s\n", buffer);
	
	ConvertString((PCHAR)buffer, (PCHAR)info.fw_rev, 8);
	
	printf("Firmware rev: %s\n", buffer);
	
	memset(buffer, 0, 41);
	strncpy(buffer, (PCHAR)info.model, 40);
	ConvertString((PCHAR)buffer, (PCHAR)info.model, 40);

	printf("Model No: %s\n", buffer);

	printf("Capability %x\n", info.capability);

	printf("Capacity 2 %I64d, Capacity %d\n",
		info.lba_capacity_2,
		info.lba_capacity
		);

	printf("LBA %d, LBA48 %d\n", 
		 (info.capability &= 0x02), 
		(info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400));
	

	bSmartSupported =  info.command_set_1 & 0x0001;
	if (bSmartSupported) {
		bSmartEnabled = info.cfs_enable_1 & 0x0001;
	} else {
		bSmartEnabled = FALSE;
	}
	printf("SMART feature: %s and %s\n", 
		bSmartSupported?"Supported":"Not supported",
		bSmartEnabled?"Enabled":"Disabled"
		);
	printf("Write cache feature: support=%d enabled=%d\n", 
			(info.command_set_1 & 0x20) != 0,
			(info.cfs_enable_1 & 0x20) != 0);
	printf("Config feature: support=%d enabled=%d\n",
		(info.command_set_2 & 0x0800) != 0,
		(info.cfs_enable_2 & 0x0800) != 0);

	printf("FUA feature: WRITEDMA_FUA_EXT, MULTWRITE_FUA_EXT: support=%d enabled=%d\n",
		(info.cfsse & 0x0040) == 0x0040, (info.csf_default & 0x0040) == 0x0040);
	printf("FUA feature: WRITEDMA_QUEUED_FUA_EXT: support=%d enabled=%d\n",
		(info.cfsse & 0x0080) == 0x0080, (info.csf_default & 0x0080) == 0x0080);

	if(info.cfs_enable_2 & 0x0800) {
		struct hd_driveconf drvconf;

		iResult = IdeCommand(connsock, iTargetID, 0, WIN_DEV_CONFIG, 0, 0,
			(_int8)DEVCONFIG_CONFIG_IDENTIFY, sizeof(drvconf), (PCHAR)&drvconf,0, 0);
		if (iResult !=0) {
			fprintf(stderr, "Device configuration identify Failed...\n");
			goto errout;
		}
		printf("DevConf: Revision=%04x\n", drvconf.revision);
		printf("DevConf: Dma=%04x\n", drvconf.dma);
		printf("DevConf: UDma=%04x\n", drvconf.udma);
		printf("DevConf: Max LBA=%I64x\n",	((_int64)drvconf.maximum_lba[0]) +
											((_int64)drvconf.maximum_lba[1] << 16) +
											((_int64)drvconf.maximum_lba[2] << 32) +
											((_int64)drvconf.maximum_lba[3] << 48));
		printf("DevConf: Command set/features=%04x\n", drvconf.cmd_sfs);
		printf("DevConf: Integrity=%04x\n", drvconf.integrity);
	}

	DisconnectFromNdas(connsock, UserId);
errout:	
	return 0;

}



int CmdGenBc(char* target, char* arg[])
{
	SOCKADDR_LPX slpx = {0};
	int result;
	int broadcastPermission;
	int i = 0;
	SOCKET			sock;
//	int addrlen;

		// LSHP broadcast
	char BcPacket[] = {
		0x4C, 0x53
		, 0x48, 0x50, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x03, 0x00, 0x10, 0x00, 0x00, 0x00, 0x24, 0x02
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x13, 0x72, 0x73, 0x64, (char)0xA3, 0x01, 0x00, 0x00, 0x0B, (char)0xD0, 0x02
		, 0x0C, (char)0x3C, (char)0x10, (char)0x27, 0x00, 0x00, 0x03, (char)0x14, (char)0xD1, 0x07, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		, 0x00, 0x00
	};

	// Create Socket.
	sock = socket(AF_LPX, SOCK_DGRAM, IPPROTO_LPXUDP);
	if(INVALID_SOCKET == sock) {
		PrintError(WSAGetLastError(), _T("socket"));
		return -1;
	}

	broadcastPermission = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
			(const char*)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
		fprintf(stderr, "Can't setsockopt for broadcast: %d\n", errno);
		return -1;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family = AF_LPX;

#if 0 // bind is not required for send
	result = lpx_addr(arg[0], &slpx.LpxAddress);
	slpx.LpxAddress.Port = htons(PNP_REQ_SRC_PORT);

	result = bind(sock, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result < 0) {
		fprintf(stderr, "Error! when binding...: %d\n", WSAGetLastError());
		return -1;
	}
#endif

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family  = AF_LPX;
    slpx.LpxAddress.Node[0] = 0xFF;
    slpx.LpxAddress.Node[1] = 0xFF;
    slpx.LpxAddress.Node[2] = 0xFF;
    slpx.LpxAddress.Node[3] = 0xFF;
    slpx.LpxAddress.Node[4] = 0xFF;
    slpx.LpxAddress.Node[5] = 0xFF;

	slpx.LpxAddress.Port = htons(17);
	for(i=0;i<10000;i++) {
		result = sendto(sock, (const char*)&BcPacket, sizeof(BcPacket),
				0, (struct sockaddr *)&slpx, sizeof(slpx));
		if (result < 0) {
			fprintf(stderr, "Can't send broadcast message: %d\n", WSAGetLastError());
			return -1;
		}
		Sleep(1);
	}
	closesocket(sock);
	return 0;
}


//
// arg[0]: Dev number. Optional. Default value is 0.
//
int CmdGetNativeMaxAddr(char* target, char* arg[])
{
	int retval = 0;
	SOCKET				connsock;
	PUCHAR				data = NULL;
	unsigned			UserId;
	int dev;
	if (arg[0] == NULL) {
		dev = 0;
	} else {
		dev = (int) _strtoi64(arg[0], NULL, 0);
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

	if (ConnectToNdas(&connsock, target, UserId, NULL) !=0)
		goto errout;

//	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ_NATIVE_MAX_EXT, 0, 0, 0, 0, 0, 0);
	retval = IdeCommand(connsock, iTargetID, 0, WIN_READ_NATIVE_MAX, 0, 0, 0, 0, 0, 0, 0);

	if(retval != 0) {
		fprintf(stderr, "IdeCommand failed\n");
		goto errout;
	}

	DisconnectFromNdas(connsock, UserId);
errout:
	closesocket(connsock);
	return retval;

}
