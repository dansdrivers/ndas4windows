// LanScsiEmu.cpp : 
//	This emulator is only for 2.5 chipset. 
//   It may contain some 2.0/1.1/1.0 support code, but this emulator does not support them
//

#include "stdafx.h"
#include "ndasemu.h"
#include "ndasemupriv.h"

#define _LPX_

unsigned char HostMacAddr[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // BJR

#define	NDASDEV_LISTENPORT_NUMBER	10000
#define	BROADCAST_SOURCEPORT_NUMBER	10001
#define BROADCAST_DESTPORT_NUMBER	(BROADCAST_SOURCEPORT_NUMBER+1)

#define	NR_MAX_TARGET			1
#define MAX_DATA_BUFFER_SIZE	(64 * 1024 + 16)
//#define	DEFAULT_DISK_SIZE				1024 * 1024 * 256 // 256MB
#define	DEFAULT_DISK_SIZE				((INT64)1024 * 1024 * 1024 * 3) // 3Giga
#define	MAX_CONNECTION			64
#define DROP_RATE				0  // out of 1000 packets

#define HASH_KEY_READONLY HASH_KEY_USER
#define HASH_KEY_READWRITE HASH_KEY_USER

unsigned char password0_def[16] = {0x1f, 0x4a, 0x50, 0x73, 0x15, 0x30, 0xea, 0xbb,
	0x3e, 0x2b, 0x32, 0x1a, 0x47, 0x50, 0x13, 0x1e};

typedef	struct _TARGET_DATA {
	BOOL			bPresent;
	BOOL			bLBA;
	BOOL			bLBA48;

	union {
		struct {
			unsigned _int8	NRRWHost;
			unsigned _int8	NRROHost;
			unsigned _int16	Reserved1;
		} V1; 	// NDAS 1.0, 1.1, 2.0
		struct {
			unsigned _int8	NREWHost;
			unsigned _int8	NRSWHost;
			unsigned _int8	NRROHost;
			unsigned _int8	Reserved1;
		} V2;	// NDAS 2.5
	};
	
	unsigned _int64	TargetData;
	char *			ExportDev;
	int			Export;
	
	// IDE Info.
	_int64	Size;

	unsigned short	pio_mode;
	unsigned short	dma_mword;
	unsigned short	dma_ultra;
	
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
	int				UserNum;
	int				Permission;
	UCHAR			Password[16]; // Order is reversed from password in Prom 
	int				AccessCountIncreased;
	unsigned _int32	HPID;
	unsigned _int16	RPID;
	UINT64			SessionId;
	unsigned		CHAP_I;
	unsigned _int32	CHAP_C[4]; // MSB??

	UCHAR	HostMacAddress[6];

	union {
		UCHAR	Options;
		struct {
			UCHAR DataEncryption:1;  // Bit 0?
			UCHAR HeaderEncryption:1; 
			UCHAR DataCrc:1;
			UCHAR HeaderCrc:1;
			UCHAR JumboFrame:1;
			UCHAR NoHeartFrame:1;
			UCHAR ReservedOption:2;
		};
	};

	ENCRYPTION_INFO	EncryptInfo;
	NDASDIGEST_INFO	DigestInfo;

} SESSION_DATA, *PSESSION_DATA;


#include <pshpack1.h>
//
// This structure is LSB
// If EEPROM reset is asserted, 
//	0x10~0xaf(MaxConnectionTimeout~UserPasswords) is set to default value.
typedef struct _PROM_DATA {
	UCHAR	EthAddr[6]; 
	UCHAR	Signature[2];
	UCHAR	Reserved1[8];
	UINT16	MaxConnectionTimeout; 		// not valid in emu. depends on LPX
	UINT16	HeartBeatTimeout; 
	UINT32	MaxRetransmissionTimeout; 	// not valid in emu. depends on LPX 
	union {
		UCHAR	Options;
		struct {
			UCHAR DataEncryption:1;  // Bit 0?
			UCHAR HeaderEncryption:1; 
			UCHAR DataCrc:1;
			UCHAR HeaderCrc:1;
			UCHAR JumboFrame:1;
			UCHAR NoHeartFrame:1;
			UCHAR ReservedOption:2;
		};
	};
	UCHAR	DeadLockTimeout;
	UINT16	SataTimeout;
	UINT32	StandbyTimeout;
	UCHAR	WatchdogTimeout; 
	UCHAR   IdentifyTimeout;
	UCHAR  	HighSpeedUsbDataTimeout;
	UCHAR	FullSpeedUsbTxDataTimeout;
	UINT16	HighSpeedUsbRxAckTimeout;
	UINT16	FullSpeedUSBRxAckTimeout;
	UCHAR	UserPermissions[8]; // 8 user. one byte for each
	UCHAR	UserPasswords[8][16]; // 8 user. 128 bit for each.
	UCHAR	Reserved2[336];
	UCHAR	DeviceDescriptorHS[18];
	UCHAR	DeviceQualifierHS[10];
	UCHAR   ConfigurationDescriptorHS[9];
	UCHAR	Interface[9];
	UCHAR	BulkInEndpoint[7];
	UCHAR	BulkOutEndpoint[7];
	UCHAR	OtherSpeedConfigurationDescriptor[36];
	UCHAR	DeviceDescriptorFS[18];
	UCHAR	DeviceQualifierFS[10];
	UCHAR 	ConfigurationDescriptorFS[9];

	UCHAR	Reserved3[1024-645];
} PROM_DATA, *PPROM_DATA; // Protected PROM area

#include <poppack.h>
UCHAR USER_PROM[1024 * 3];	// Unprotected PROM Area

C_ASSERT_SIZEOF(PROM_DATA, 1024);

// Global Variable.
_int16			G_RPID = 0;
unsigned _int8	thisHWVersion = LANSCSIIDE_VERSION_2_0;

int				NRTarget;
TARGET_DATA		PerTarget[NR_MAX_TARGET];
SOCKET			listenSock;
SESSION_DATA	sessionData[MAX_CONNECTION];
PROM_DATA		Prom;
PROM_DATA_OLD	PromOld;
RAM_DATA_OLD	RamDataOld;

#define Decrypt32 Decrypt32_l
#define Encrypt32 Encrypt32_l
#define Hash32To128 Hash32To128_l

//
// Error injection option
//
BOOL EiReadHang = FALSE;	// rh
BOOL EiReadBadSector = FALSE;			// r
BOOL EiWriteBadSector = FALSE;			// w
BOOL EiVerifyBadSector = FALSE;			// v
BOOL EiHangFirstLogin = FALSE;			// f
UINT64 EiErrorLocation = 100000;
UINT32 EiErrorLength = 16;

BOOL IsInErrorRange(UINT64 Start, UINT32 Length)
{
	if (Start+Length-1 < EiErrorLocation)
		return FALSE;
	if (EiErrorLocation+EiErrorLength-1<Start)
		return FALSE;
	return TRUE;
}

UINT32 OptVerbose = 1;
// Verbose level 0: no messagge
//				1: Show vendor command/special command
//				2: Show RW op.
//				3: Show detailed RW info.

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

//
// Set default prom value. If ResetAll is TRUE, reset all value. If not, only 0x10~0xaf region will be reset.
//
void
SetDefaultPromValue(PPROM_DATA Prom, BOOL ResetAll)
{
	UCHAR	EthAddr[6] = {0xff, 0xff, 0xff, 0xd0, 0x0b, 0x00};  // LSB
	UCHAR	Signature[2] = {0x11, 0x01};
	UINT16	MaxConnectionTimeout = 0x4;
	UINT16	HeartBeatTimeout = 0x4;  
	UINT32	MaxRetransmissionTimeout= 0xc7; 
//	UCHAR	Options = 0;
	UCHAR DataEncryption	= 0;
	UCHAR HeaderEncryption	= 0;
	UCHAR DataCrc			= 0;
	UCHAR HeaderCrc			= 0;
	UCHAR JumboFrame = 0;
	UCHAR NoHeartFrame = 0;
	UCHAR	DeadLockTimeout = 0x09;
	UINT16	SataTimeout = 0x0f; 
	UINT32	StandbyTimeout = 0x8005;
	UCHAR	WatchdogTimeout = 0xb3; 
	UCHAR   IdentifyTimeout = 0x20;
	UCHAR  	HighSpeedUsbDataTimeout = 0x16;
	UCHAR	FullSpeedUsbTxDataTimeout = 0x4c;
	UINT16	HighSpeedUsbRxAckTimeout = 0x3c;
	UINT16	FullSpeedUSBRxAckTimeout = 0x01f4;
	UCHAR	UserPermissions[8] = {0x07, 0,0,0,0,0,0,0};
	UCHAR	UserPassword[16] = 
		{0x1E, 0x13,0x50,0x47, 0x1A, 0x32, 0x2B,0x3E,
		 0xBB,0xEA,0x30,0x15,0x73, 0x50,0x4A,0x1F};
	UCHAR	DeviceDescriptorHS[18] =
		{0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
		 0xb4, 0x04, 0x30, 0x68, 0x01, 0x00, 0x62, 0x78, 0x8e, 0x01};
	UCHAR	DeviceQualifierHS[10] = 
		{0x0a, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00};
	UCHAR   ConfigurationDescriptorHS[9] = 
		{0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x32};
	UCHAR	Interface[9] =
		{0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00};
	UCHAR	BulkInEndpoint[7] =
		{0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00};
	UCHAR	BulkOutEndpoint[7] = 
		{0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00};
	UCHAR	OtherSpeedConfigurationDescriptor[36] =
		{0x09, 0x07, 0x20, 0x00, 0x01, 0x01, 0x00, 0xc0, 
		 0x32, 0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 
		 0x50, 0x00, 0x07, 0x05, 0x81, 0x02, 0x40, 0x00, 
		 0x00, 0x07, 0x05, 0x02, 0x02, 0x40, 0x00, 0x00, 
		 0x00, 0x00, 0x00, 0x00};
	UCHAR	DeviceDescriptorFS[18] =
		{0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0xb4, 
		 0x04, 0x30, 0x68, 0x01, 0x00, 0x62, 0x78, 0x8e, 0x01};
	UCHAR	DeviceQualifierFS[10] = 
		{0x0a, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00};
	UCHAR 	ConfigurationDescriptorFS[9] =
		{0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0xc0, 0x32};

	int i;
	fprintf(stderr, "Setting default PROM value%s\n", ResetAll?"(Full)":"");

#define SET_PROM_BYTES_VAL(_member) memcpy(Prom->_member, _member, sizeof(_member))
#define SET_PROM_INT_VAL(_member) Prom->_member = _member

	if (ResetAll) {
		memset(Prom, 0, sizeof(PROM_DATA));
	}
	if (ResetAll) {
		SET_PROM_BYTES_VAL(EthAddr);
		SET_PROM_BYTES_VAL(Signature);
	}
	SET_PROM_INT_VAL(MaxConnectionTimeout);
	SET_PROM_INT_VAL(HeartBeatTimeout);
	SET_PROM_INT_VAL(MaxRetransmissionTimeout);
	SET_PROM_INT_VAL(DataEncryption);
	SET_PROM_INT_VAL(HeaderEncryption);	
	SET_PROM_INT_VAL(DataCrc);
	SET_PROM_INT_VAL(HeaderCrc);
	SET_PROM_INT_VAL(JumboFrame);
	SET_PROM_INT_VAL(NoHeartFrame);
	SET_PROM_INT_VAL(DeadLockTimeout);
	SET_PROM_INT_VAL(SataTimeout);
	SET_PROM_INT_VAL(StandbyTimeout);
	SET_PROM_INT_VAL(WatchdogTimeout);
	SET_PROM_INT_VAL(IdentifyTimeout);
	SET_PROM_INT_VAL(HighSpeedUsbDataTimeout);
	SET_PROM_INT_VAL(FullSpeedUsbTxDataTimeout);
	SET_PROM_INT_VAL(HighSpeedUsbRxAckTimeout);
	SET_PROM_INT_VAL(FullSpeedUSBRxAckTimeout);
	SET_PROM_BYTES_VAL(UserPermissions);
	for(i=0;i<8;i++) {
		memcpy(Prom->UserPasswords[i], UserPassword, sizeof(UserPassword));
	}
	if (ResetAll) {	
		SET_PROM_BYTES_VAL(DeviceDescriptorHS);
		SET_PROM_BYTES_VAL(DeviceQualifierHS);
		SET_PROM_BYTES_VAL(ConfigurationDescriptorHS);
		SET_PROM_BYTES_VAL(Interface);
		SET_PROM_BYTES_VAL(BulkInEndpoint);
		SET_PROM_BYTES_VAL(BulkOutEndpoint);
		SET_PROM_BYTES_VAL(OtherSpeedConfigurationDescriptor);
		SET_PROM_BYTES_VAL(DeviceDescriptorFS);
		SET_PROM_BYTES_VAL(DeviceQualifierFS);
		SET_PROM_BYTES_VAL(ConfigurationDescriptorFS);		
	}
}
void PrintHex(unsigned char* Buf, int len)
{
	int i;
	for(i=0;i<len;i++) {
		printf("%02x", Buf[i]);
		if ((i+1)%4==0)
			printf(" ");
	}
}

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

BOOL
mac_addr(
		 PCHAR			pStr,
		 PUCHAR			pAddr
		 )
{
	PCHAR	pStart, pEnd;

	if(pStr == NULL)
		return FALSE;

	pStart = pStr;

	for(int i = 0; i < 6; i++) {
		
		pAddr[i] = (UCHAR)strtoul(pStart, &pEnd, 16);
		
		pStart += 3;
	}

	return TRUE;
}

inline int 
RecvIt(
	   SOCKET	sock,
	   PUCHAR	buf, 
	   int		size
	   )
{
	int res;
	int len = size;
	
//	fprintf(stderr, "RecvIt %d ", size);

	while (len > 0) {
		if ((res = recv(sock, (char*)buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "RecvIt error");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "RecvIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}	
	
//	fprintf(stderr, " - done\n");
	return size;
}

inline int 
SendIt(
	   SOCKET	sock,
	   PUCHAR	buf, 
	   int		size
	   )
{
	int res;
	int len = size;

//	fprintf(stderr, "SendIt %d ", size);
	
	while (len > 0) {
		if ((res = send(sock, (char*)buf, len, 0)) == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "SendIt");
			return res;
		} else if(res == 0) {
			fprintf(stderr, "SendIt: Disconnected...\n");
			return res;
		}
		len -= res;
		buf += res;
	}
//	fprintf(stderr, " - done\n");
	return size;
}

int
ReadRequest(
			SOCKET			connSock,
			PUCHAR			pBuffer,
			PLANSCSI_PDU_POINTERS	pPdu,
			PSESSION_DATA	pSessionData
			)
{
	int		iResult, iTotalRecved = 0;
	PUCHAR	pPtr = pBuffer;

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
		&& pSessionData->HeaderEncryption != 0) {
		// Decrypt first 32 byte to get AHSLen
		if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
			Decrypt128(
				(unsigned char*)pPdu->pH2RHeader,
				32,
				(unsigned char *)&pSessionData->CHAP_C,
				pSessionData->Password
				);
		}
		else {
			Decrypt32(
				(unsigned char*)pPdu->pH2RHeader,
				32,
				(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
				(unsigned char*)&pSessionData->EncryptInfo.Password64
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
	
		pPdu->pAHS = (PCHAR)pPtr;

		pPtr += ntohs(pPdu->pH2RHeader->AHSLen);
	}

	if (pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE 
		&& pSessionData->HeaderCrc != 0) {
		// Read CRC
		iResult = RecvIt(
			connSock,
			pPtr,
			4
			);
		
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Recv AHS...\n");

			return iResult;
		} else if(iResult == 0) {
			fprintf(stderr, "ReadRequest: Disconnected...\n");

			return iResult;
		} else
			iTotalRecved += iResult;

		pPdu->pDataSeg = (PCHAR)pPtr;
		pPdu->pHeaderDig = (PCHAR)pPtr;

		pPtr += 4;
	} else {
		pPdu->pHeaderDig = NULL;
	}

//	fprintf(stderr, "iTotalRecved = %d, pSessionData->iSessionPhase = %d\n", iTotalRecved, pSessionData->iSessionPhase);

	// Read paddings
	if (thisHWVersion == LANSCSIIDE_VERSION_2_5 && iTotalRecved % 16 != 0) {
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

		pPdu->pDataSeg = (PCHAR)pPtr;

		pPtr += iResult;
	}

	// Decrypt remaing headers.
	if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pSessionData->HeaderEncryption != 0) {
		if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
			Decrypt128(
				((unsigned char*)pPdu->pH2RHeader) + 32,
				iTotalRecved - 32,
				(unsigned char *)&pSessionData->CHAP_C,
				pSessionData->Password
				);
		}
		else {
			Decrypt32(
				((unsigned char*)pPdu->pH2RHeader) + 32,
				iTotalRecved - 32,
				(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
				(unsigned char*)&pSessionData->EncryptInfo.Password64
				);
		}	
	}

	// Check header CRC
	if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& pSessionData->HeaderCrc != 0) {
		unsigned hcrc = ((unsigned *)pPdu->pHeaderDig)[0];
		CRC32(
			(unsigned char*)pBuffer,
			&(((unsigned char*)pBuffer)[sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen)]),
			sizeof(LANSCSI_H2R_PDU_HEADER) + ntohs(pPdu->pH2RHeader->AHSLen)
		);
		if(hcrc != ((unsigned *)pPdu->pHeaderDig)[0])
			fprintf(stderr, "Header Digest Error !!!!!!!!!!!!!!!...\n");
	}
#if 0
	if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
			Decrypt128(
				(unsigned char*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&pSessionData->CHAP_C,
				pSessionData->Password
				);
		}
		else {
			Decrypt32(
				(unsigned char*)pPdu->pDataSeg,
				ntohs(pPdu->pH2RHeader->AHSLen),
				(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
				(unsigned char*)&pSessionData->EncryptInfo.Password64
				);
		}		
		//fprintf(stderr, "ReadRequest: Decrypt Header 2 !!!!!!!!!!!!!!!...\n");
	}
#endif

	// Read Data segment.pPdu->pH2RHeader->DataSegLen must be 0 for 1.1~2.5

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
		
		pPdu->pDataSeg = (PCHAR)pPtr;
		
		pPtr += ntohl(pPdu->pH2RHeader->DataSegLen);

		
		if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
			&& pSessionData->DataEncryption) {
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
				Decrypt128(
					(unsigned char*)pPdu->pDataSeg,
					ntohl(pPdu->pH2RHeader->DataSegLen),
					(unsigned char *)&pSessionData->CHAP_C,
					pSessionData->Password
					);
			}
			else {
				Decrypt32(
					(unsigned char*)pPdu->pDataSeg,
					ntohl(pPdu->pH2RHeader->DataSegLen),
					(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
					(unsigned char*)&pSessionData->EncryptInfo.Password64
					);
			}			
		}
	}
	
	// Read Data Dig.
	pPdu->pDataDig = NULL;
	
	return iTotalRecved;
}

int
SendReply(
	SOCKET			connSock,
	PLANSCSI_PDU_POINTERS	pPdu,
	PSESSION_DATA	pSessionData
	) {
	PLANSCSI_H2R_PDU_HEADER pHeader;
	int						iDataSegLen, iResult;

	pHeader = pPdu->pH2RHeader;
	iDataSegLen = ntohs(pHeader->AHSLen);

	if((pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE 
		|| pSessionData->iSessionPhase == LOGOUT_PHASE)
		&& pSessionData->HeaderCrc != 0) {
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
	if((pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE 
		|| pSessionData->iSessionPhase == LOGOUT_PHASE)
		&& pSessionData->HeaderEncryption != 0) {
		if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
			Encrypt128(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&pSessionData->CHAP_C,
				pSessionData->Password
				);
		}
		else {
			Encrypt128(
				(unsigned char*)pHeader,
				sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen,
				(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
				(unsigned char*)&pSessionData->EncryptInfo.Password64
				);
		}		
	}
	
	// Send Request.
	if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		iResult = SendIt(
			connSock,
			(PUCHAR)pHeader,
			(sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen + 15) & 0xfffffff0 // Align 16 byte.
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "SendRequest: Send Request ");
			return -1;
		}
	}
	else {	
		iResult = SendIt(
			connSock,
			(PUCHAR)pHeader,
			(sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen ) 
			);
		if(iResult == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "SendRequest: Send Request ");
			return -1;
		}
	}
	return 0;
}


DWORD HandleLoginRequest(PSESSION_DATA pSessionData, PLANSCSI_PDU_POINTERS pdu, PUCHAR PduBuffer)
{
	PLANSCSI_LOGIN_REQUEST_PDU_HEADER	pLoginRequestHeader;
	PLANSCSI_LOGIN_REPLY_PDU_HEADER		pLoginReplyHeader;
	PBIN_PARAM_SECURITY					pSecurityParam;
	PAUTH_PARAMETER_CHAP				pAuthChapParam;
	PBIN_PARAM_NEGOTIATION				pParamNego;
	int i;
	fprintf(stderr, "LOGIN_REQUEST ");
	pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)PduBuffer;
	pLoginRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pdu->pH2RHeader;

	if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
		// Bad Command...
		fprintf(stderr, "Session2: Bad Command. Invalid login phase\n");
		pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
		goto MakeLoginReply;
	} 
	
	// Check Header.
	if((pLoginRequestHeader->VerMin > LANSCSIIDE_CURRENT_VERSION)
		|| (pLoginRequestHeader->ParameterType != PARAMETER_TYPE_BINARY)
		|| (pLoginRequestHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
		// Bad Parameter...
		fprintf(stderr, "Session2: Bad Parameter.\n");
		
		pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_VERSION_MISMATCH;
		goto MakeLoginReply;
	}
	
#if 0 // this cause problem if peer try to login with ver1.0 protocol first
	// Check Sub Packet Sequence.
	if(ntohs(pLoginRequestHeader->CSubPacketSeq) != pSessionData->CSubPacketSeq) {
		// Bad Sub Sequence...
		fprintf(stderr, "Session2: Bad Sub Packet Sequence. H %d R %d\n",
			pSessionData->CSubPacketSeq,
			ntohs(pLoginRequestHeader->CSubPacketSeq));
		
		pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		goto MakeLoginReply;
	}
#endif
	
	// Check Port...
	if(pLoginRequestHeader->CSubPacketSeq > 0) {
		if((pSessionData->HPID != (unsigned)ntohl(pLoginRequestHeader->HPID))
			|| (pSessionData->RPID != ntohs(pLoginRequestHeader->RPID))
			|| (pSessionData->CPSlot != ntohs(pLoginRequestHeader->CPSlot))
			|| (pSessionData->PathCommandTag != (unsigned)ntohl(pLoginRequestHeader->PathCommandTag))) {
			
			fprintf(stderr, "Session2: Bad Port parameter.\n");
			
			pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
			goto MakeLoginReply;
		}
	}
	
	switch(ntohs(pLoginRequestHeader->CSubPacketSeq)) {
	case 0:
		{
			fprintf(stderr, "*** First ***\n");
			// Check Flag.
			if((pLoginRequestHeader->T != 0)
				|| (pLoginRequestHeader->CSG != FLAG_SECURITY_PHASE)
				|| (pLoginRequestHeader->NSG != FLAG_SECURITY_PHASE)) {
				fprintf(stderr, "Session: BAD First Flag.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Check Parameter.

			// to support version 1.1, 2.0
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
				|| (pdu->pDataSeg == NULL)) {							
				fprintf(stderr, "Session: BAD First Request Data.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}	
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pAHS == NULL)) {
					fprintf(stderr, "Session: BAD First Request Data.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {

			pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
				thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
				thisHWVersion == LANSCSIIDE_VERSION_2_5) {

				pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
			}
			// end of supporting version

			if(pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) {
				fprintf(stderr, "Session: BAD First Request Parameter.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Login Type.
			if((pSecurityParam->LoginType != LOGIN_TYPE_NORMAL) 
				&& (pSecurityParam->LoginType != LOGIN_TYPE_DISCOVERY)) {
				fprintf(stderr, "Session: BAD First Login Type.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
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
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
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
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Check Parameter.
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pDataSeg == NULL)) {
				
					fprintf(stderr, "Session: BAD Second Request Data.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pAHS == NULL)) {
				
					fprintf(stderr, "Session: BAD Second Request Data.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
			}
			if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
				|| (pSecurityParam->LoginType != pSessionData->iLoginType)
				|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
				
				fprintf(stderr, "Session: BAD Second Request Parameter.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Hash Algorithm.
			pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
			if( (thisHWVersion == LANSCSIIDE_VERSION_2_5) &&
				!(ntohl(pAuthChapParam->CHAP_A) & HASH_ALGORITHM_AES128)) {
				fprintf(stderr, "Session: Not Supported HASH Algorithm.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Store Data.
			pSessionData->CHAP_I = ntohl(pAuthChapParam->CHAP_I);
			
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pSessionData->EncryptInfo.CHAP_C = (rand() << 16) + rand();
			}
			// Create Challenge
			pSessionData->CHAP_C[0] = (rand() << 16) + rand();
			pSessionData->CHAP_C[1] = (rand() << 16) + rand();
			pSessionData->CHAP_C[2] = (rand() << 16) + rand();
			pSessionData->CHAP_C[3] = (rand() << 16) + rand();
			
			// Make Header
			pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
			pLoginReplyHeader->T = 0;
			pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
			pLoginReplyHeader->NSG = FLAG_SECURITY_PHASE;

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
			} else if (thisHWVersion == LANSCSIIDE_VERSION_1_1 || 
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
			}
			
			pSecurityParam = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
			pAuthChapParam = &pSecurityParam->ChapParam;
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pSecurityParam->ChapParam.CHAP_A = htonl(HASH_ALGORITHM_AES128);
				pSecurityParam->ChapParam.V2.CHAP_CR[0] = pSessionData->CHAP_C[0]; // Endian conversion??
				pSecurityParam->ChapParam.V2.CHAP_CR[1] = pSessionData->CHAP_C[1];
				pSecurityParam->ChapParam.V2.CHAP_CR[2] = pSessionData->CHAP_C[2];
				pSecurityParam->ChapParam.V2.CHAP_CR[3] = pSessionData->CHAP_C[3];							
			}
			else {
				pSecurityParam->ChapParam.CHAP_A = htonl(HASH_ALGORITHM_MD5);
				pSecurityParam->ChapParam.V1.CHAP_C[0] = htonl(pSessionData->EncryptInfo.CHAP_C);
			}
			
			printf("CHAP_C %08x %08x %08x %08x\n", 
				pSessionData->CHAP_C[0], pSessionData->CHAP_C[1], pSessionData->CHAP_C[2], pSessionData->CHAP_C[3]);
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
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			
			// Check Parameter.

			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pDataSeg == NULL)) {
				
					fprintf(stderr, "Session: BAD Third Request Data.\n"); 
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pAHS == NULL)) {
				
					fprintf(stderr, "Session: BAD Third Request Data.\n"); 
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
			}
			if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
				|| (pSecurityParam->LoginType != pSessionData->iLoginType)
				|| (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
				
				fprintf(stderr, "Session: BAD Third Request Parameter.\n");
				pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
				goto MakeLoginReply;
			}
			pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->AuthParamter;
			if( (thisHWVersion == LANSCSIIDE_VERSION_2_5) && 
				!(ntohl(pAuthChapParam->CHAP_A) == HASH_ALGORITHM_AES128)) {
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
			
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {

				switch(pSessionData->iLoginType) {
				case LOGIN_TYPE_NORMAL:
					{
						BOOL	bRW = FALSE;
						int UserNum = USER_NUM_FROM_USER_ID(pSessionData->iUser);
						int Permission = USER_PERM_FROM_USER_ID(pSessionData->iUser) & USER_PERMISSION_MASK;

						// Check user number and permision
						if (UserNum == SUPERVISOR_USER_NUM) {
							// Don't need to check permission
							fprintf(stderr, "Session: Superuser logined\n");
							fprintf(stderr, "Warning! Disconnecting other connections is not implemented\n");
						} else if (UserNum < SUPERVISOR_USER_NUM) {
							if (Permission == 0) {
								fprintf(stderr, "Session: Invalid permission request\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeLoginReply;
							}
							int GrantedPermision = Prom.UserPermissions[UserNum] & USER_PERMISSION_MASK;
							
							if (PerTarget[0].V2.NREWHost >0 && 
								(Permission == USER_PERMISSION_SW || Permission == USER_PERMISSION_EW)) {
								fprintf(stderr, "Session: Exclusive write user exists\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeLoginReply;										
							}

							if (!(Permission & GrantedPermision)) {
								fprintf(stderr, "Session: Not enough excess right\n");
								pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
								goto MakeLoginReply;										
							}
						} else {
							fprintf(stderr, "Session: Invalid user number %d\n", UserNum);
							pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
							goto MakeLoginReply;									
						}

						pSessionData->UserNum = UserNum;
						if (UserNum == SUPERVISOR_USER_NUM) {
							pSessionData->Permission = Permission = USER_PERMISSION_EW;
						} else {
							pSessionData->Permission = Permission;
						}
						for(i=0;i<16;i++) {
							pSessionData->Password[i] = Prom.UserPasswords[UserNum][15-i];
						}
						
						// Increase Login User Count.
						switch(Permission) {
							case USER_PERMISSION_EW:	PerTarget[0].V2.NREWHost++; break;
							case USER_PERMISSION_SW:	PerTarget[0].V2.NRSWHost++; break;
							case USER_PERMISSION_RO:	PerTarget[0].V2.NRROHost++; break;
						}
						pSessionData->AccessCountIncreased = 1;
						
					}
					break;
				case LOGIN_TYPE_DISCOVERY:
					{
						pSessionData->Permission = 0;
						pSessionData->UserNum = USER_NUM_FROM_USER_ID(pSessionData->iUser);
						for(i=0;i<16;i++) {
							pSessionData->Password[i] = Prom.UserPasswords[pSessionData->UserNum][15-i];
						}
					}
					break;
				default:
					break;
				}
				
				//
				// Check CHAP_R
				//
				{
					unsigned int	result[4] = { 0 };

					AES_cipher((unsigned char*)pSessionData->CHAP_C, (unsigned char*) result, pSessionData->Password);
					printf("Password: ");
					PrintHex(pSessionData->Password, 16);
					printf("\n");
					printf("CHAP_RESULT= %08x %08x %08x %08x\n", result[0], result[1], result[2], result[3]);
					if(memcmp(result, pAuthChapParam->V2.CHAP_CR, 16) != 0) {
						printf("CHAP_Received= %08x %08x %08x %08x\n", pAuthChapParam->V2.CHAP_CR[0], pAuthChapParam->V2.CHAP_CR[1], pAuthChapParam->V2.CHAP_CR[2], pAuthChapParam->V2.CHAP_CR[3]);
						fprintf(stderr, "Auth Failed.\n");
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_AUTH_FAILED;
						goto MakeLoginReply;							
					}
				}
			} else { // NDAS 1.0, 1.1, 2.0
				switch(pSessionData->iLoginType) {
				case LOGIN_TYPE_NORMAL:
					{
						BOOL	bRW = FALSE;
						
						if(pSessionData->iUser == 0xffffffff) {	// Supervisor Login
							fprintf(stderr, "Session: Supervisor Login.\n");
							pSessionData->EncryptInfo.Password64 = HASH_KEY_SUPER;
							break;
						}

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
								if(PerTarget[0].V1.NRRWHost > 0) {
#if 1
									fprintf(stderr, "Session: Already RW. Logined\n");
									pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeLoginReply;
#else
									fprintf(stderr, "Session: Already RW. But allowing new login\n");
#endif
								}
								PerTarget[0].V1.NRRWHost++;								
							} else {
								PerTarget[0].V1.NRROHost++;
							}
						}
						if(pSessionData->iUser & 0x00000002) {	// Use Target0
							if(pSessionData->iUser &0x00020000) {
								if(PerTarget[1].V1.NRRWHost > 0) {
#if 0
									fprintf(stderr, "Session: Already RW. Logined\n");
									pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
									goto MakeLoginReply;
#else
									fprintf(stderr, "Session: Already RW. But allowing new login\n");
#endif
								}
								PerTarget[1].V1.NRRWHost++;
							} else {
								PerTarget[1].V1.NRROHost++;
							}
						}

						fprintf(stderr, "Target 0: NRRWHost = %d, NRROHost = %d\n", PerTarget[0].V1.NRRWHost, PerTarget[0].V1.NRROHost);
						fprintf(stderr, "Target 1: NRRWHost = %d, NRROHost = %d\n", PerTarget[1].V1.NRRWHost, PerTarget[1].V1.NRROHost);

						pSessionData->AccessCountIncreased = 1;
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

					if(memcmp(result, pAuthChapParam->V1.CHAP_R, 16) != 0) {
						fprintf(stderr, "Auth Failed.\n");
						pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
						goto MakeLoginReply;							
					}
				}
			}
			
			// Make Reply.
			pLoginReplyHeader->T = 1;
			pLoginReplyHeader->CSG = FLAG_SECURITY_PHASE;
			pLoginReplyHeader->NSG = FLAG_LOGIN_OPERATION_PHASE;
			pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
			
			// Set Phase.
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
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
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pDataSeg == NULL)) {
				
					fprintf(stderr, "Session: BAD Fourth Request Data.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)	// Minus AuthParameter[1]
					|| (pdu->pAHS == NULL)) {
				
					fprintf(stderr, "Session: BAD Fourth Request Data.\n");
					pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
					goto MakeLoginReply;
				}	
			}

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pParamNego = (PBIN_PARAM_NEGOTIATION)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pParamNego = (PBIN_PARAM_NEGOTIATION)pdu->pAHS;
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

			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
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
			pParamNego->MaxTargetID = htonl(1);
			pParamNego->MaxLUNID = htonl(1);

			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				if (Prom.HeaderEncryption) {
					pParamNego->HeaderEncryptAlgo = ENCRYPT_ALGO_AES128;
				} else {
					pParamNego->HeaderEncryptAlgo = 0;
				}
				
				if (Prom.HeaderCrc) {
					pParamNego->HeaderDigestAlgo = DIGEST_ALGO_CRC32;
				} else {
					pParamNego->HeaderDigestAlgo = 0;
				}
				
				if (Prom.DataEncryption) {
					pParamNego->DataEncryptAlgo = ENCRYPT_ALGO_AES128;
				} else {
					pParamNego->DataEncryptAlgo = 0;
				}
				
				if (Prom.DataCrc) {
					pParamNego->DataDigestAlgo = DIGEST_ALGO_CRC32;
				} else {
					pParamNego->DataDigestAlgo = 0;
				}
			}
			else {				
				pParamNego->HeaderEncryptAlgo = PromOld.HeaderEncryptAlgo;
				pParamNego->HeaderDigestAlgo = 0;
				
				pParamNego->DataEncryptAlgo = PromOld.DataEncryptAlgo;				
				pParamNego->DataDigestAlgo = 0;
			}
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
	
	pLoginReplyHeader->Opcode = LOGIN_RESPONSE;
	pLoginReplyHeader->VerMax = LANSCSIIDE_CURRENT_VERSION;
	pLoginReplyHeader->VerActive = LANSCSIIDE_CURRENT_VERSION;
	pLoginReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
	pLoginReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
	return 0;
}

DWORD HandleLogoutRequest(PSESSION_DATA pSessionData, PLANSCSI_PDU_POINTERS pdu, PUCHAR PduBuffer)
{
	PLANSCSI_LOGOUT_REQUEST_PDU_HEADER	pLogoutRequestHeader;
	PLANSCSI_LOGOUT_REPLY_PDU_HEADER	pLogoutReplyHeader;
	fprintf(stderr, "LOGOUT_REQUEST\n");
	pLogoutReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)PduBuffer;
		
	if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
		// Bad Command...
		fprintf(stderr, "Session2: LOGOUT Bad Command.\n");
		pLogoutReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
		goto MakeLogoutReply;
	} 
		
	// Check Header.
	pLogoutRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)pdu->pH2RHeader;
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
//	pLogoutReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //0;
	pLogoutReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);

MakeLogoutReply:
	pLogoutReplyHeader->Opcode = LOGOUT_RESPONSE;
	return 0;
}


DWORD HandleTextRequest(PSESSION_DATA pSessionData, PLANSCSI_PDU_POINTERS pdu, PUCHAR PduBuffer)
{

	PLANSCSI_TEXT_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_TEXT_REPLY_PDU_HEADER		pReplyHeader;
	UCHAR				ucParamType;
	
	pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)PduBuffer;
	
	fprintf(stderr, "TEXT_REQUEST \n");

	if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
		// Bad Command...
		fprintf(stderr, "Session2: TEXT_REQUEST Bad Command.\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
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
	if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
		if(ntohl(pRequestHeader->DataSegLen) < 4) {	// Minimum size.
			fprintf(stderr, "Session: TEXT No Data seg.\n");
			
			pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
			goto MakeTextReply;
		}
	}
	if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		if(ntohs(pRequestHeader->AHSLen) < 4) {	// Minimum size.
			fprintf(stderr, "Session: TEXT No Data seg.\n");
		
			pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
			goto MakeTextReply;
		}
	}
	// end of supporting version

	// to support version 1.1, 2.0 
	if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
		ucParamType = ((PBIN_PARAM)pdu->pDataSeg)->ParamType;
	} else if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		ucParamType = ((PBIN_PARAM)pdu->pAHS)->ParamType;
	} else {
		fprintf(stderr, "Unsupported version\n");
		exit(-1);
	}
	// end of supporting version

//				switch(((PBIN_PARAM)pdu.pDataSeg)->ParamType) {
	switch(ucParamType) {
	case BIN_PARAM_TYPE_TARGET_LIST:
		{
			PBIN_PARAM_TARGET_LIST	pParam;
			
			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pParam = (PBIN_PARAM_TARGET_LIST)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
	    		    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
	    		    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pParam = (PBIN_PARAM_TARGET_LIST)pdu->pAHS;
			}						
			pParam->NRTarget = (UCHAR)NRTarget;	

			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				for(int i = 0; i < pParam->NRTarget; i++) {
					pParam->PerTarget[i].TargetID = htonl(i);
					pParam->PerTarget[i].V2.NREWHost = PerTarget[i].V2.NREWHost;
					pParam->PerTarget[i].V2.NRSWHost = PerTarget[i].V2.NRSWHost;
					pParam->PerTarget[i].V2.NRROHost = PerTarget[i].V2.NRROHost;
					pParam->PerTarget[i].V2.Reserved1 = 0;
					pParam->PerTarget[i].TargetData = PerTarget[i].TargetData;
				}
			}
			else {
				for(int i = 0; i < pParam->NRTarget; i++) {
					pParam->PerTarget[i].TargetID = htonl(i);
					pParam->PerTarget[i].V1.NRRWHost = PerTarget[i].V1.NRRWHost;
					pParam->PerTarget[i].V1.NRROHost = PerTarget[i].V1.NRROHost;
					pParam->PerTarget[i].V1.Reserved1 = 0;
					pParam->PerTarget[i].TargetData = PerTarget[i].TargetData;				
				}				
			}
			
			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
			pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
		}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
			}
			// end of supporting version

		}
		break;
	case BIN_PARAM_TYPE_TARGET_DATA:
		{
			PBIN_PARAM_TARGET_DATA pParam;
			
			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pParam = (PBIN_PARAM_TARGET_DATA)pdu->pDataSeg;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pParam = (PBIN_PARAM_TARGET_DATA)pdu->pAHS;
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
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
    		    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
			}
			// end of supporting version
		}
		break;
	default:
		break;
	}
	

	// to support version 1.1, 2.0 
	if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
		pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
	}
	if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
	    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
	}
	// end of supporting version
MakeTextReply:
	pReplyHeader->Opcode = TEXT_RESPONSE;
	return 0;
}


DWORD HandleIdeCommand(PSESSION_DATA pSessionData, PLANSCSI_PDU_POINTERS pdu, PUCHAR PduBuffer)
{
	PLANSCSI_IDE_REQUEST_PDU_HEADER_V1	pRequestHeader;
	PLANSCSI_IDE_REPLY_PDU_HEADER_V1	pReplyHeader;
	PUCHAR	data;
	_int64	Location;
	unsigned SectorCount;
	int	iUnitDisk;
	int						iResult;

//	if (thisHWVersion !=LANSCSIIDE_VERSION_2_5){
//		fprintf(stderr, "Unsupported version %d\n", LANSCSIIDE_VERSION_2_5);
//	}
	
				
	pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)PduBuffer;
	pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;					

	data = (PUCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_DATA_BUFFER_SIZE);
	if(data == NULL) {
		// Insufficient resource
		fprintf(stderr, "IDE_COMMAND: Insufficient resource.\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		goto MakeIDEReply1;
	}

	//
	// Convert Location and Sector Count.
	//
	Location = 0;
	SectorCount = 0;

	iUnitDisk = PerTarget[ntohl(pRequestHeader->TargetID)].Export;
	
	if(PerTarget[ntohl(pRequestHeader->TargetID)].bLBA48 == TRUE) {
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
		pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
		goto MakeIDEReply1;
	}
	
	if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
		// Bad Command...
		fprintf(stderr, "Session2: IDE_COMMAND Bad Command.\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
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
		|| (pRequestHeader->Command == WIN_WRITEDMA_EXT)
		|| (pRequestHeader->Command == WIN_READ_EXT)
		|| (pRequestHeader->Command == WIN_WRITE_EXT))) {
		fprintf(stderr, "Session2: Bad Command. LBA48 command to non-LBA48 device\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
		goto MakeIDEReply1;
	}
	

	switch(pRequestHeader->Command) {	
	case WIN_READ_EXT:
	case WIN_READDMA:
	case WIN_READDMA_EXT:
		{
						fprintf(stderr, "R");
						fprintf(stderr, "READ: Location %I64d, Sector Count %d...\n", Location, SectorCount);
			
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
			_read(iUnitDisk, data, SectorCount * 512);

			if (EiReadHang && IsInErrorRange(Location, SectorCount)) {
				fprintf(stderr, "Error Injection: Hanging when accessing sector %d, length %d\n", Location, SectorCount);
				Sleep(60 * 1000);
				break;
			}

			if (EiReadBadSector && IsInErrorRange(Location, SectorCount)) {
				fprintf(stderr, "Error Injection: Returning bad sector when accessing sector %d, length %d\n", Location, SectorCount);
				pReplyHeader->Command = ERR_STAT;
				pReplyHeader->Feature_Curr = ECC_ERR;
				break;
			}
		//	fprintf(stderr, "READ: Location %I64d, Sector Count %d... Success\n", Location, SectorCount);
		}
		break;
	case WIN_WRITE_EXT:
	case WIN_WRITEDMA:
	case WIN_WRITEDMA_EXT:
		{
			int DataLength;
						fprintf(stderr, "W");
						fprintf(stderr, "WRITE: Location %I64d, Sector Count %d...\n", Location, SectorCount);
			
			//
			// Check access right.
			//
			if(pSessionData->Permission == USER_PERMISSION_RO) {
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
			DataLength = SectorCount * 512;
			if (pSessionData->DataCrc != 0) {
				DataLength +=16;
			}
			// Receive Data.
			iResult = RecvIt(
				pSessionData->connSock,
				data,
				DataLength
				);
			if(iResult == SOCKET_ERROR) {
				fprintf(stderr, "ReadRequest: Can't Recv Data...\n");
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				HeapFree(GetProcessHeap(), 0, data);
				return 0;
			}

			//
			// Decrypt Data.
			//
			if(pSessionData->DataEncryption != 0) {
				if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
					Decrypt128(
						(unsigned char*)data,
						DataLength,
						(unsigned char *)&pSessionData->CHAP_C,
						pSessionData->Password
						);
				}
				else {
					Decrypt32(
						(unsigned char*)data,
						DataLength,
						(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
						(unsigned char*)&pSessionData->EncryptInfo.Password64
						);
				}
			}
			if(pSessionData->DataCrc != 0) {
				unsigned crc;
				crc = ((unsigned *)data)[SectorCount * 128];

				CRC32(
					(unsigned char *)data,
					&(((unsigned char *)data)[SectorCount * 512]),
					SectorCount * 512
				);

				if(crc != ((unsigned *)data)[SectorCount * 128]) {
					fprintf(stderr, "Data Digest Error !!!!!!!!!!!!!!!...\n");
				}
			}

			if (EiWriteBadSector && IsInErrorRange(Location, SectorCount)) {
				fprintf(stderr, "Error Injection: Returning bad sector when writing sector %I64d, length %d\n", Location, SectorCount);
				pReplyHeader->Command = ERR_STAT;
				pReplyHeader->Feature_Curr = ECC_ERR;
				break;
			}

			_lseeki64(iUnitDisk, Location * 512, SEEK_SET);
			_write(iUnitDisk, data, SectorCount * 512);
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

			if (EiVerifyBadSector && IsInErrorRange(Location, SectorCount)) {
				fprintf(stderr, "Error Injection: Returning bad sector for verifying sector %I64d, length %d\n", Location, SectorCount);
				pReplyHeader->Command = ERR_STAT;
				pReplyHeader->Feature_Curr = ECC_ERR;
				// Set location of error
				if (pReplyHeader->Command == WIN_VERIFY) {
					pReplyHeader->LBALow_Curr = (_int8)(EiErrorLocation);
					pReplyHeader->LBAMid_Curr = (_int8)(EiErrorLocation >> 8);
					pReplyHeader->LBAHigh_Curr = (_int8)(EiErrorLocation >> 16);
					pReplyHeader->LBAHeadNR = (_int8)(EiErrorLocation >> 24);
					pReplyHeader->SectorCount_Curr = (_int8)0; // reserved
				} else if (pReplyHeader->Command == WIN_VERIFY_EXT){
					pReplyHeader->LBALow_Curr = (_int8)(EiErrorLocation);
					pReplyHeader->LBAMid_Curr = (_int8)(EiErrorLocation >> 8);
					pReplyHeader->LBAHigh_Curr = (_int8)(EiErrorLocation >> 16);
					pReplyHeader->LBALow_Prev = (_int8)(EiErrorLocation >> 24);
					pReplyHeader->LBAMid_Prev = (_int8)(EiErrorLocation >> 32);
					pReplyHeader->LBAHigh_Prev = (_int8)(EiErrorLocation >> 40);

					pReplyHeader->SectorCount_Curr = (_int8)0; // reserved
					pReplyHeader->SectorCount_Prev = (_int8)0; // reserved
				}
				break;
			}
		}
		break;
	case WIN_SETFEATURES:
		{
			struct	hd_driveid	*pInfo;
			int Feature, Mode;
			int targetId;

			fprintf(stderr, "set features ");
			pInfo = (struct hd_driveid *)data;
//						Feature = pRequestHeader->Feature;
			// fixed to support version 1.1, 2.0
			Feature = pRequestHeader->Feature_Curr;
			Mode = pRequestHeader->SectorCount_Curr;
			targetId = ntohl(pRequestHeader->TargetID);
			if(targetId >= 2) {
				fprintf(stderr, "Invalid target ID\n");
				pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
				goto MakeIDEReply1;
			}

			switch(Feature) {
				case SETFEATURES_XFER: 
					fprintf(stderr, "SETFEATURES_XFER %x\n", Mode);
					if((Mode & 0xf0) == 0x00) {			// PIO

						PerTarget[targetId].pio_mode &= 0x00ff;
						PerTarget[targetId].pio_mode |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x20) {	// Muti-word DMA

						PerTarget[targetId].dma_mword &= 0x00ff;
						PerTarget[targetId].dma_mword |= (1 << ((Mode & 0xf) + 8));

					} else if((Mode & 0xf0) == 0x40) {	// Ultra DMA

						PerTarget[targetId].dma_ultra &= 0x00ff;
						PerTarget[targetId].dma_ultra |= (1 << ((Mode & 0xf) + 8));

					} else {
						fprintf(stderr, "XFER unknown mode %x\n", Mode);
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
			char	firmware_rev[8] = {'.', '2', 0, '0', 0, };
			char	model[40] = { 'D', 'N', 'S', 'A', 'm', 'E', 'l', 'u', 't', 'a', 0, };
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
			pInfo->dma_ultra = PerTarget[iUnitDisk].dma_ultra;
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
		int DataLength = 512;
		if (pSessionData->DataCrc != 0) {
			CRC32(
				(unsigned char*)data,
				&(((unsigned char*)data)[DataLength]), 
				DataLength
			);
			DataLength+=16;
		}

		//
		// Encryption.
		//
		if(pSessionData->DataEncryption != 0) {
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
				Decrypt128(
					(unsigned char*)data,
					DataLength,
					(unsigned char *)&pSessionData->CHAP_C,
					pSessionData->Password
					);
			}
			else {
				Decrypt32(
					(unsigned char*)data,
					DataLength,
					(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
					(unsigned char*)&pSessionData->EncryptInfo.Password64
					);
			}
		}

		// Send Data.
		iResult = SendIt(
			pSessionData->connSock,
			data,
			DataLength
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Send Identify Data...\n");
			pSessionData->iSessionPhase = LOGOUT_PHASE;

			HeapFree(GetProcessHeap(), 0, data);
			return 0;
		}
		
	}
	
	if((pRequestHeader->Command == WIN_READ_EXT)
		|| (pRequestHeader->Command == WIN_READDMA)
		|| (pRequestHeader->Command == WIN_READDMA_EXT)) {	

		int DataLength = SectorCount * 512;
		if (pSessionData->DataCrc != 0) {
			CRC32(
				(unsigned char*)data,
				&(((unsigned char*)data)[DataLength]),
				DataLength
			);
			DataLength+=16;
		}			
		
		//
		// Encrption.
		//
		if(pSessionData->DataEncryption != 0) {
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) { 
				Decrypt128(
					(unsigned char*)data,
					DataLength,
					(unsigned char *)&pSessionData->CHAP_C,
					pSessionData->Password
					);
			}
			else {
				Decrypt32(
					(unsigned char*)data,
					DataLength,
					(unsigned char *)&pSessionData->EncryptInfo.CHAP_C,
					(unsigned char*)&pSessionData->EncryptInfo.Password64
					);
			}
		}		

		// Send Data.
		iResult = SendIt(
			pSessionData->connSock,
			data,
			DataLength
			);
		if(iResult == SOCKET_ERROR) {
			fprintf(stderr, "ReadRequest: Can't Send READ Data...\n");
			pSessionData->iSessionPhase = LOGOUT_PHASE;

			HeapFree(GetProcessHeap(), 0, data);
			return 0;
		}
		
	}
	pReplyHeader->Opcode = IDE_RESPONSE;
	HeapFree(GetProcessHeap(), 0, data);
	return 0;
}

DWORD HandleVendorCommand(PSESSION_DATA pSessionData, PLANSCSI_PDU_POINTERS pdu, PUCHAR PduBuffer)
{
	PLANSCSI_VENDOR_REQUEST_PDU_HEADER	pRequestHeader;
	PLANSCSI_VENDOR_REPLY_PDU_HEADER	pReplyHeader;
	fprintf(stderr, "VENDOR_SPECIFIC_COMMAND\n");
	pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)PduBuffer;
	pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;				
	if((pRequestHeader->F == 0)
		|| (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
		|| (pSessionData->RPID != ntohs(pRequestHeader->RPID))
		|| (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
		|| (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
		
		fprintf(stderr, "Session2: Vender Bad Port parameter.\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
		goto MakeVendorReply;
	}

	if( (pRequestHeader->VendorID != htons(NKC_VENDOR_ID)) ||
	    (pRequestHeader->VendorOpVersion != VENDOR_OP_CURRENT_VERSION) 
	) {
		fprintf(stderr, "Session2: Vender Version don't match.\n");
		pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
		goto MakeVendorReply;

	}


	if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		switch(pRequestHeader->VendorOp) {
			case VENDOR_OP_SET_MAX_RET_TIME:
				Prom.MaxRetransmissionTimeout = NTOHL(pRequestHeader->VendorParameter2);
				break;
			case VENDOR_OP_SET_MAX_CONN_TIME:
				Prom.MaxConnectionTimeout = NTOHL(pRequestHeader->VendorParameter2);
				break;
			case VENDOR_OP_GET_MAX_RET_TIME:
				pRequestHeader->VendorParameter0 = 0;
				pRequestHeader->VendorParameter1 = 0;
				pRequestHeader->VendorParameter2 = HTONL(Prom.MaxRetransmissionTimeout);
				break;
			case VENDOR_OP_GET_MAX_CONN_TIME:
				pRequestHeader->VendorParameter0 = 0;
				pRequestHeader->VendorParameter1 = 0;
				pRequestHeader->VendorParameter2 = HTONL(Prom.MaxConnectionTimeout);					
				break;
			case VENDOR_OP_RESET:
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				break;
			case VENDOR_OP_SET_SUPERVISOR_PW:
			case VENDOR_OP_GET_MUTEX_OWNER:
			case VENDOR_OP_SET_DYNAMIC_MAX_CONN_TIME:
				fprintf(stderr, "Obsoleted vendor op 0x%x\n",pRequestHeader->VendorOp);
				break;

			case VENDOR_OP_SET_MUTEX:
			case VENDOR_OP_FREE_MUTEX:
			case VENDOR_OP_GET_MUTEX_INFO:
			case VENDOR_OP_SET_HEART_TIME:
			case VENDOR_OP_GET_HEART_TIME:
			case VENDOR_OP_SET_USER_PERMISSION:
			case VENDOR_OP_SET_USER_PW:
			case VENDOR_OP_SET_OPT:
			case VENDOR_OP_SET_STANBY_TIMER:
			case VENDOR_OP_GET_STANBY_TIMER:
			case VENDOR_OP_SET_DELAY:
			case VENDOR_OP_GET_DELAY:
			case VENDOR_OP_SET_DYNAMIC_MAX_RET_TIME:
			case VENDOR_OP_GET_DYNAMIC_MAX_RET_TIME:
			case VENDOR_OP_SET_D_OPT:
			case VENDOR_OP_GET_D_OPT:
			case VENDOR_OP_GET_WRITE_LOCK:
			case VENDOR_OP_FREE_WRITE_LOCK:
			case VENDOR_OP_SET_DEAD_LOCK_TIME:
			case VENDOR_OP_GET_DEAD_LOCK_TIME:
			case VENDOR_OP_SET_EEP:
			case VENDOR_OP_GET_EEP:
			case VENDOR_OP_U_SET_EEP:
			case VENDOR_OP_U_GET_EEP:
			case VENDOR_OP_SET_WATCHDOG_TIME:
			case VENDOR_OP_GET_WATCHDOG_TIME:
			case VENDOR_OP_SET_MAC:						
			default:
				fprintf(stderr, "Unknown or not-implemented vendor op 0x%x\n",pRequestHeader->VendorOp);
				break;
		}
	}
	else {
		switch(pRequestHeader->VendorOp) {
			case VENDOR_OP_SET_MAX_RET_TIME:
				fprintf(stderr, "Vendor: SET_MAX_RET_TIME\n");
				PromOld.MaxRetTime = NTOHL(pRequestHeader->VendorParameter1);
				break;
			case VENDOR_OP_SET_MAX_CONN_TIME:
				fprintf(stderr, "Vendor: SET_MAX_CONN_TIME\n");
				PromOld.MaxConnTime = NTOHL(pRequestHeader->VendorParameter1);
				break;
			case VENDOR_OP_GET_MAX_RET_TIME:
				fprintf(stderr, "Vendor: SET_MAX_RET_TIME\n");
				pRequestHeader->VendorParameter0 = 0;
				pRequestHeader->VendorParameter1 = HTONL(PromOld.MaxRetTime);
				break;
			case VENDOR_OP_GET_MAX_CONN_TIME:
				fprintf(stderr, "Vendor: GET_MAX_CONN_TIME\n");
				pRequestHeader->VendorParameter0 = 0;
				pRequestHeader->VendorParameter1 = HTONL(PromOld.MaxConnTime);
				break;
			case VENDOR_OP_SET_MUTEX: {
				BOOL	bret;
				if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
					thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

					bret = VendorSetLock11(
									&RamDataOld,
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
			case VENDOR_OP_FREE_MUTEX: {
				BOOL	bret;
				if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
					thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

						bret = VendorFreeLock11(
							&RamDataOld,
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
			case VENDOR_OP_GET_MUTEX_INFO: {
				BOOL	bret;

				fprintf(stderr, "Vendor: GET_SEMA\n");
				if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
					thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

						bret = VendorGetLock11(
							&RamDataOld,
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
			case VENDOR_OP_GET_MUTEX_OWNER: {
				BOOL	bret;

				fprintf(stderr, "Vendor: OWNER_SEMA\n");
				if( thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
					thisHWVersion == LANSCSIIDE_VERSION_2_0 ) {

						bret = VendorGetLockOwner11(
							&RamDataOld,
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
				PromOld.SuperPasswd = NTOHLL(pRequestHeader->VendorParameter);
				break;
			case VENDOR_OP_SET_USER_PW:
				fprintf(stderr, "Vendor: SET_USER_PW\n");
				PromOld.UserPasswd = NTOHLL(pRequestHeader->VendorParameter);
				break;
			case VENDOR_OP_RESET:
				fprintf(stderr, "Vendor: RESET\n");
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				break;
			default:
				break;
		}
	}

MakeVendorReply:
	pReplyHeader->Opcode = VENDOR_SPECIFIC_RESPONSE;

	return 0;
}

DWORD WINAPI 
SessionThreadProc(
				  LPVOID lpParameter   // thread data
				  )
{
	PSESSION_DATA			pSessionData = (PSESSION_DATA)lpParameter;
	int						iResult;
	UCHAR					PduBuffer[MAX_REQUEST_SIZE];
	LANSCSI_PDU_POINTERS	pdu;
	PLANSCSI_H2R_PDU_HEADER	pRequestHeader;
	PLANSCSI_R2H_PDU_HEADER	pReplyHeader;
//	int 				count, i;
	// to support version 1.1, 2.0
//	UCHAR				ucParamType;
	
	//
	// Init variables...
	//
	pSessionData->CSubPacketSeq = 0;	
	pSessionData->iSessionPhase = FLAG_SECURITY_PHASE;
	pSessionData->Options = Prom.Options;	// Set DataEncryption, HeaderEncryption, DataCrc, HeaderCrc, JumboFrame, NoHeartFrame
	pSessionData->AccessCountIncreased = 0;

	if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
	}
	else {
		pSessionData->EncryptInfo.HeaderEncryptAlgo = PromOld.HeaderEncryptAlgo;
		pSessionData->EncryptInfo.BodyEncryptAlgo = PromOld.DataEncryptAlgo;
	}

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
			HandleLoginRequest(pSessionData, &pdu, PduBuffer);
			break;
		case LOGOUT_REQUEST:
			HandleLogoutRequest(pSessionData, &pdu, PduBuffer);
			break;
		case TEXT_REQUEST:
			HandleTextRequest(pSessionData, &pdu, PduBuffer);
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
			HandleIdeCommand(pSessionData, &pdu, PduBuffer);
			break;

		case VENDOR_SPECIFIC_COMMAND:
			HandleVendorCommand(pSessionData, &pdu, PduBuffer);
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
//			unsigned _int32	iTemp;

			// Send Reply.
			pReplyHeader = (PLANSCSI_R2H_PDU_HEADER)PduBuffer;
			
			pReplyHeader->HPID = htonl(pSessionData->HPID);
			pReplyHeader->RPID = htons(pSessionData->RPID);
			pReplyHeader->CPSlot = htons(pSessionData->CPSlot);

			// to support version 1.1, 2.0 
			if (thisHWVersion == LANSCSIIDE_VERSION_1_0) {
				pReplyHeader->AHSLen = 0;
			}
			if (thisHWVersion == LANSCSIIDE_VERSION_1_1 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_0 ||
			    thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				pReplyHeader->DataSegLen = 0;
			}
			// end of supporting version

			pReplyHeader->CSubPacketSeq = htons(pSessionData->CSubPacketSeq);
			pReplyHeader->PathCommandTag = htonl(pSessionData->PathCommandTag);
			

			// end of supporting version
//			pdu.pR2HHeader = pReplyHeader;
//			pdu.pDataSeg = (char *)pParamSecu;
			// PDU is set already.??

			iResult = SendReply(pSessionData->connSock, &pdu, pSessionData);
			if (iResult) {
				// Error
				fprintf(stderr, "ReadRequest: Can't Send First Reply...\n");
				pSessionData->iSessionPhase = LOGOUT_PHASE;
				continue;
			}
		}
		
		if((pReplyHeader->Opcode == LOGIN_RESPONSE)
			&& (pSessionData->CSubPacketSeq == 4)) {
			pSessionData->CSubPacketSeq = 0;
			pSessionData->iSessionPhase = FLAG_FULL_FEATURE_PHASE;
		}
	}
	
//EndSession:

	fprintf(stderr, "Session2: Logout Phase.\n");
	
	switch(pSessionData->iLoginType) {
	case LOGIN_TYPE_NORMAL:
		if (pSessionData->AccessCountIncreased) {
			if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
				switch(pSessionData->Permission) {
					case USER_PERMISSION_EW:	PerTarget[0].V2.NREWHost--; break;
					case USER_PERMISSION_SW:	PerTarget[0].V2.NRSWHost--; break;
					case USER_PERMISSION_RO:	PerTarget[0].V2.NRROHost--; break;
				}
			}
			else {
				// Decrease Login User Count.
				// TODO: use atomic operation

				if(pSessionData->iUser == 0xffffffff) {	// Supervisor Logout
					fprintf(stderr, "Session: Supervisor Logout.\n");					
					break;
				}

				if(pSessionData->iUser & 0x00000001) {	// Use Target0
					if(pSessionData->iUser &0x00010000) {
						PerTarget[0].V1.NRRWHost--;
					} else {
						PerTarget[0].V1.NRROHost--;
					}
				}
				if(pSessionData->iUser & 0x00000002) {	// Use Target0
					if(pSessionData->iUser &0x00020000) {
						PerTarget[1].V1.NRRWHost--;
					} else {
						PerTarget[1].V1.NRROHost--;
					}
				}
			}
		}
		break;
	case LOGIN_TYPE_DISCOVERY:
	default:
		break;
	}

	CleanupLock11(&RamDataOld,pSessionData->SessionId);
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
		fprintf(stderr, "Can't setsockopt for broadcast: %d\n", errno);
		return ;
	}

	memset(&slpx, 0, sizeof(slpx));
	slpx.sin_family = AF_LPX;
	slpx.LpxAddress.Port = htons(BROADCAST_SOURCEPORT_NUMBER);

	memcpy(slpx.LpxAddress.Node, HostMacAddr, 6);
#if 0
	slpx.LpxAddress.Node[0] = 0xff;
	slpx.LpxAddress.Node[1] = 0xff;
	slpx.LpxAddress.Node[2] = 0xff;
	slpx.LpxAddress.Node[3] = 0xff;
	slpx.LpxAddress.Node[4] = 0xff;
	slpx.LpxAddress.Node[5] = 0xff;
#endif

	memset(&message, 0, sizeof(message));
	message.ucType = 0;
	message.ucVersion = thisHWVersion;

	result = bind(sock, (struct sockaddr *)&slpx, sizeof(slpx));
	if (result < 0) {
		fprintf(stderr, "Error! when binding...: %d\n", WSAGetLastError());
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
			fprintf(stderr, "Can't send broadcast message: %d\n", WSAGetLastError());
			// return;
		}

		//
		//	Delay 
		//

		Sleep((Prom.HeartBeatTimeout + 1) * 1000);
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

void
SetLpxDropRate(int Droprate)
{
	if (Droprate) {
		fprintf(stderr, "Setting droprate is disalbed\n");
	}
#if 0
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
#endif
}

void PrintUsage(void)
{
	fprintf(stderr, "rh: Hang when read specific sector\n");
	fprintf(stderr, "rb: Set bad sector mark when reading specific sector\n");
	fprintf(stderr, "wb: Set bad sector mark when writing specific sector\n");
	fprintf(stderr, "vb: Set bad sector mark when verifying specific sector\n");
	fprintf(stderr, "hf: Hang at first login step\n");
	fprintf(stderr, "v0 = NDAS 1.0, v1 = NDAS 1.1, v2 = NDAS 2.0, v3 = NDAS 2.5(3.0) : Set NDAS HW version\n");
	fprintf(stderr, "2: Two disk mode\n");
	fprintf(stderr, "sXXXX: Set disk size. ex- s1G, s256M, s2T\n");
}

int
__cdecl
main(int argc, char* argv[])
{
	WORD				wVersionRequested;
	WSADATA				wsaData;
	int					err;
	int					i;
	BOOLEAN TwoDiskMode = FALSE;
	INT64 DiskSize = DEFAULT_DISK_SIZE;
#ifdef _LPX_
	SOCKADDR_LPX		address;
#else
	struct sockaddr_in	servaddr;
#endif
	HANDLE				hBThread;

	if (argc>1) {
		int i;
		for(i=1;i<argc;i++) {
			if (strcmp(argv[i], "rh")==0) {
				fprintf(stderr, "EI: Turn on read hang\n");
				EiReadHang  = TRUE;				
			} else if (strcmp(argv[i], "rb") == 0) {
				fprintf(stderr, "EI: Read bad sector\n");
				EiReadBadSector = TRUE;
			} else if (strcmp(argv[i], "wb") == 0) {
				fprintf(stderr, "EI: Write bad sector\n");
				EiWriteBadSector = TRUE;
			} else if (strcmp(argv[i], "vb") == 0) {
				fprintf(stderr, "EI: Verify bad sector\n");
				EiVerifyBadSector = TRUE;
			} else if (strcmp(argv[i], "hf") == 0) {
				fprintf(stderr, "EI: Hang at first login\n");
				EiHangFirstLogin = TRUE;
			} else if (argv[i][0] == 'v' || argv[i][0] == 'V') {
				thisHWVersion = argv[i][1] - '0';
				fprintf(stderr, "HWVersion %d\n", thisHWVersion);
			} else if (argv[i][0] == 'h') {
				mac_addr(&argv[i][1], HostMacAddr);
				fprintf(stderr, "Host Address %s\n", &argv[i][1]);
			} else if (argv[i][0] == '2') {
				TwoDiskMode = TRUE;
			} else if (strcmp(argv[i], "help")==0) {
				PrintUsage();
				return 0;
			} else if (argv[i][0] == 's' || argv[i][0] == 'S') {
				DWORD Size;
				char Unit;
				if (argv[i][1] == 0)  {
					fprintf(stderr, "Size is not given\n");
					return 0;
				}
				sscanf(&argv[i][1], "%i%c", &Size, &Unit);
				Unit = (char)toupper(Unit);
				fprintf(stderr, "Using disk size:");
				if (Unit == 'M') {
					fprintf(stderr, "%dMB\n", Size);
					DiskSize = 1024LL * 1024 * Size;
				} else if (Unit == 'G') {
					fprintf(stderr, "%dGB\n", Size);
					DiskSize = 1024LL * 1024 * 1024 * Size;
				} else if (Unit == 'T') {
					fprintf(stderr, "%dTB\n", Size);
					DiskSize = 1024LL * 1024 * 1024 * 1024 * Size;
				} else {
					fprintf(stderr, "Unknown size unit\n");
					return 0;
				}
			}
		}
	}

	if (EiReadHang || EiReadBadSector || EiWriteBadSector|| EiVerifyBadSector) {
		fprintf(stderr, "Error injection location: %I64x, length: %x\n", EiErrorLocation, EiErrorLength);
	}

	printf("NDAS Emulator: Version = %d\n", thisHWVersion);

	if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
		SetDefaultPromValue(&Prom, TRUE);
	}
	else {
		//
		// EEPROM
		//

		PromOld.MaxConnTime = 4999; // 5 sec
		PromOld.MaxRetTime = 63; // 63 ms
		PromOld.UserPasswd = HASH_KEY_USER;
		PromOld.SuperPasswd = HASH_KEY_SUPER;
		PromOld.MaxConnTime = 4999; // 5 sec
		PromOld.MaxRetTime = 63; // 63 ms
		PromOld.UserPasswd = HASH_KEY_USER;
		PromOld.SuperPasswd = HASH_KEY_SUPER;
	}

	if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
	}
	else {
		RamDataOld.LockMutex = CreateMutex(NULL, FALSE, NULL);
		if(RamDataOld.LockMutex == NULL) {
			PrintError(GetLastError(), "CreateMutex");
			return -1;
		}
	}

	SetLpxDropRate(0);


	if (NR_MAX_TARGET !=1) {
		fprintf(stderr, "This emulator support only one target\n");
		return 1;
	}
	// Open UnitDisk.
	PerTarget[0].Export = _open("UnitDisk0", _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
	if(PerTarget[0].Export < 0) {
		// File does not exist.
		char	buffer[512];
		_int64	loc;

		PerTarget[0].Export = _open("UnitDisk0", _O_RDWR | _O_CREAT | _O_BINARY, _S_IREAD | _S_IWRITE);
		if(PerTarget[0].Export < 0) {
			printf("Can not open ND\n");
			return 1;
		}
		
		//
		//	Write init DIB
		//

		memset(buffer, 0, 512);
		loc = _lseeki64(PerTarget[0].Export, DiskSize - 512, SEEK_END);

		printf("Loc : %I64d\n", loc);
		if(_write(PerTarget[0].Export, buffer, 512) == -1) {
			perror( "Can not write ND" );
			return 1;
		}
	} 
	
	_lseeki64(PerTarget[0].Export, 0, SEEK_SET);

	// Init.
	for(i=0; i<NR_MAX_TARGET;i++) {
		PerTarget[i].bLBA = TRUE;
		PerTarget[i].bLBA48 = TRUE;
		PerTarget[i].bPresent = TRUE;

		if (thisHWVersion == LANSCSIIDE_VERSION_2_5) {
			PerTarget[i].V2.NRSWHost = 0;
			PerTarget[i].V2.NREWHost = 0;		
			PerTarget[i].V2.NRROHost = 0;
		}
		else {
			PerTarget[i].V1.NRRWHost = 0;
			PerTarget[i].V1.NRROHost = 0;					
		}
		PerTarget[i].TargetData = 0;
		PerTarget[i].Size = DiskSize;
		PerTarget[i].pio_mode = 0x0;
		PerTarget[i].dma_mword = 0x407; // Support up to DMA2, current mode is DMA 2
		PerTarget[i].dma_ultra = 0;
	}
	NRTarget = NR_MAX_TARGET;

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
	// No address is given. First interface will be used.
	address.sin_family = AF_LPX;
	address.LpxAddress.Port = htons(NDASDEV_LISTENPORT_NUMBER);
	memcpy(address.LpxAddress.Node, HostMacAddr, 6);
	printf("Bind to interface %02x:%02x:%02x:%02x:%02x:%02x\n", 
		HostMacAddr[0], HostMacAddr[1],HostMacAddr[2],
		HostMacAddr[3],HostMacAddr[4],HostMacAddr[5]);

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

	CloseHandle(RamDataOld.LockMutex);
	
	return 0;
}

