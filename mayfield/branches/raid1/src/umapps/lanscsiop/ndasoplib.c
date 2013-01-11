#include "ndasop.h"
#include "lanscsiop.h"
#include "lsbusioctl.h"
#include <stdio.h>

//////////////////////////////////////////////////////
//
// Debugging...
//

#ifdef _DEBUG	
//
// It is more desirable not to use stdio in the library
// but for the debugging purpose.
// However, in some codes other than debug codes use 
// sprintf at this time. Hence we included stdio.h above
// and here also. The former will be removed eventually.

#include <stdio.h>

#if defined(OUTPUT_PRINTF)
#define _OutputDebugString printf
#elif defined(OUTPUT_TRACE)
#define _OutputDebugString TRACE
#else
#define _OutputDebugString OutputDebugString
#endif

// DbgPrint
#define DEBUG_BUFFER_LENGTH 256

static CHAR	DebugBuffer[DEBUG_BUFFER_LENGTH + 1];

static VOID
DbgPrint(
		 IN PCHAR	DebugMessage,
		 ...
		 )
{
	va_list ap;
	va_start(ap, DebugMessage);
	_vsnprintf(DebugBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	_OutputDebugString(DebugBuffer);
	va_end(ap);
}

static ULONG DebugPrintLevel = 2;

#define DebugPrint(_l_, _x_)			\
		do{								\
			if(_l_ < DebugPrintLevel)	\
				DbgPrint _x_;			\
		}	while(0)					\
		
static void PrintError(
					   DWORD	ErrorCode,
					   LPTSTR strPrefix
					   )
{
	LPTSTR lpMsgBuf;
	
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
	_OutputDebugString(strPrefix);

	_OutputDebugString(lpMsgBuf);
	
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

#else	/* _DEBUG */

#define DebugPrint(_l_, _x_)			\
		do{								\
		} while(0)
#define _OutputDebugString OutputDebugString
#define PrintError __noop

#endif	/* _DEBUG */

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define KEY_NAME_NETDISKS ("SOFTWARE\\XIMETA\\NetDisks")

BOOL NDAS_SetPassword(unsigned char *szAddress, unsigned _int64 *piPassword)
{
	if(!szAddress)
		return FALSE;

	// password
	// if it's sample's address, use its password
	if(	szAddress[0] == 0x00 &&
		szAddress[1] == 0xf0 &&
		szAddress[2] == 0x0f)
	{
		*piPassword = HASH_KEY_SAMPLE;
	}
#ifdef OEM_RUTTER
	else if(	szAddress[0] == 0x00 &&
				szAddress[1] == 0x0B &&
				szAddress[2] == 0xD0 &&
				szAddress[3] & 0xFE == 0x20
				)
	{
		*piPassword = HASH_KEY_RUTTER;
	}
#endif // OEM_RUTTER
	else if(	szAddress[0] == 0x00 &&
				szAddress[1] == 0x0B &&
				szAddress[2] == 0xD0)
	{
		*piPassword = HASH_KEY_USER;
	}
	else
	{
		//
		//	default to XIMETA
		//
		*piPassword = HASH_KEY_USER;
	}

	DebugPrint(1, ("[NDASOpLib] NDAS_SetPassword : %02X:%02X:%02X:%02X:%02X:%02X %I64x\n",
		(int)szAddress[0],
		(int)szAddress[1],
		(int)szAddress[2],
		(int)szAddress[3],
		(int)szAddress[4],
		(int)szAddress[5],
		*piPassword));
	return TRUE;
}

// Function NDAS_Unbind
// Disks

BOOL
NDAS_IdeIO(UNIT_DISK_LOCATION *pUnitDisk, UINT nCommand, IDE_COMMAND_IO *aCommands, LANSCSI_PATH *pPath)
{
	BOOL bReturn = FALSE;
	LANSCSI_PATH path;
	LPX_ADDRESS address;
	IDE_COMMAND_IO *pCommand = NULL;
	_int8 data[512];
	unsigned _int8 response;
	int iResult;
	unsigned int i;

	ZeroMemory(&path, sizeof(LANSCSI_PATH));
	CopyMemory(address.Node, pUnitDisk->MACAddr, 6);

	// Connect
	if(!MakeConnection(&address, &path) || (unsigned int)NULL == path.connsock)
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_IdeIO : MakeConnection Failed\n"));
		goto out;
	}

	// Login
	// if we don't write, login read only
	path.iUserID = (pUnitDisk->SlotNumber +1);
	for(i = 0; i < nCommand; i++)
	{
		if(WIN_WRITE == aCommands[i].command)
		{
			path.iUserID = (pUnitDisk->SlotNumber +1) | (pUnitDisk->SlotNumber +1) << 16;
			break;
		}
		
	}
	path.iCommandTag = 0;
	path.HPID = 0;
	path.iHeaderEncryptAlgo = 0;
	path.iDataEncryptAlgo = 0;
	NDAS_SetPassword(address.Node, &path.iPassword);
	path.iSessionPhase = LOGOUT_PHASE;
	if(Login(&path, LOGIN_TYPE_NORMAL))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_IdeIO : Login Failed\n"));
		goto out;
	}
	
	if(GetDiskInfo(&path, pUnitDisk->SlotNumber))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_IdeIO : GetDiskInfo Failed\n"));
		goto out;
	}

	if(NULL != pPath)
	{
		CopyMemory(pPath, &path, sizeof(LANSCSI_PATH));
	}

	for(i = 0; i < nCommand; i++)
	{
		pCommand = &aCommands[i];
		iResult = IdeCommand(
			&path, 
			pUnitDisk->SlotNumber, 
			0, 
			pCommand->command, 
			(pCommand->iSector < 0) ? path.PerTarget[pUnitDisk->SlotNumber].SectorCount + pCommand->iSector : pCommand->iSector,
			1,
			0,
			(WIN_WRITE == pCommand->command) ? CopyMemory(data, pCommand->data, sizeof(pCommand->data)), data: pCommand->data,
			&response);

		if(iResult || LANSCSI_RESPONSE_SUCCESS != response)
		{
			DebugPrint(1, ("[NDASOpLib] NDAS_IdeIO : IdeCommand cmd : %d, #%d\n", pCommand->command, i));
			goto out;
		}
	}
	
	bReturn = TRUE;
out:
	if(path.connsock)
	{
		if(LOGOUT_PHASE != path.iSessionPhase)
		{
			Logout(&path);
		}
		closesocket(path.connsock);
		path.connsock = (UINT)NULL;
	}
	return bReturn;
}

BOOL
NDAS_SetBitmap(UNIT_DISK_LOCATION *pUnitDisk, UINT nCommand, IDE_COMMAND_IO *aCommands, LANSCSI_PATH *pPath)
{
	BOOL bReturn = FALSE;
	LANSCSI_PATH path;
	LPX_ADDRESS address;
	IDE_COMMAND_IO *pCommand = NULL;
	unsigned _int8 response;
	int iResult;
	unsigned int i;

	ZeroMemory(&path, sizeof(LANSCSI_PATH));
	CopyMemory(address.Node, pUnitDisk->MACAddr, 6);

	// Connect
	if(!MakeConnection(&address, &path) || (unsigned int)NULL == path.connsock)
	{
		goto out;
	}

	// Login
	path.iUserID = (pUnitDisk->SlotNumber +1) | (pUnitDisk->SlotNumber +1) << 16;
	path.iCommandTag = 0;
	path.HPID = 0;
	path.iHeaderEncryptAlgo = 0;
	path.iDataEncryptAlgo = 0;
	NDAS_SetPassword(address.Node, &path.iPassword);
	path.iSessionPhase = LOGOUT_PHASE;
	if(Login(&path, LOGIN_TYPE_NORMAL))
		goto out;
	
	if(GetDiskInfo(&path, pUnitDisk->SlotNumber))
		goto out;

	if(NULL != pPath)
	{
		CopyMemory(pPath, &path, sizeof(LANSCSI_PATH));
	}

	for(i = 0; i < nCommand; i++)
	{
		pCommand = &aCommands[i];
		iResult = IdeCommand(
			&path, 
			pUnitDisk->SlotNumber, 
			0, 
			pCommand->command, 
			(pCommand->iSector < 0) ? path.PerTarget[pUnitDisk->SlotNumber].SectorCount + pCommand->iSector : pCommand->iSector,
			1,
			0,
			pCommand->data,
			&response);

		if(iResult || LANSCSI_RESPONSE_SUCCESS != response)
			goto out;
	}
	
	bReturn = TRUE;
out:
	if(path.connsock)
	{
		if(LOGOUT_PHASE != path.iSessionPhase)
		{
			Logout(&path);
		}
		closesocket(path.connsock);
		path.connsock = (UINT)NULL;
	}
	return bReturn;
}

BOOL NDAS_DisabledByUser(UNIT_DISK_LOCATION *pUnitDisk, int iDisabled)
{
	LONG lResult;
	CHAR szAddress [100];
	CHAR szAddress2 [100];
	DWORD dwSize, dwType;
	HKEY hKeyNetDisks, hKeyUnitDisk;
	HKEY hKey;
	UINT i;
    CHAR     achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string 
    FILETIME ftLastWriteTime;      // last write time 
	CHAR	szUnitDisk[200];

	DebugPrint(1, ("[NDASOpLib] NDAS_DisabledByUser\n"));
	
	lResult = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		KEY_NAME_NETDISKS,
		0,
		KEY_READ,
		&hKeyNetDisks
		);

	DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : Key Open\n"));
	if(ERROR_SUCCESS != lResult)
		goto out;

#ifdef _DEBUG
	sprintf(szAddress2, "%02x:%02x:%02x:%02x:%02x:%02x",
		pUnitDisk->MACAddr[0],
		pUnitDisk->MACAddr[1],
		pUnitDisk->MACAddr[2],
		pUnitDisk->MACAddr[3],
		pUnitDisk->MACAddr[4],
		pUnitDisk->MACAddr[5]);

	DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : szAddress2 %s\n", szAddress2));
#endif
	
	i = 0;

	while(1)
	{
		cbName = MAX_KEY_LENGTH;
		lResult = RegEnumKeyEx(
			hKeyNetDisks,
			i++,
			achKey,
			&cbName,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);

		if(ERROR_SUCCESS != lResult)
			goto out;

		lResult = RegOpenKeyEx(
			hKeyNetDisks,
			achKey,
			0,
			KEY_READ,
			&hKey
			);

		if(ERROR_SUCCESS != lResult)
			goto out;

		dwSize = MAX_VALUE_NAME;
		
		lResult = RegQueryValueEx(
			hKey,
			"Address",
			0,
			&dwType,
			szAddress,
			&dwSize);

		if(ERROR_SUCCESS != lResult)
			continue;

		if(stricmp(szAddress, szAddress2))
			continue;

		DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : Found NDAS\n"));

		dwSize = MAX_VALUE_NAME;

#ifdef _DEBUG
		sprintf(szUnitDisk, "UnitDisk_%d", (int)pUnitDisk->SlotNumber);
		DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : szUnitDisk %s\n", szUnitDisk));
#endif
		
		lResult = RegOpenKeyEx(
			hKey,
			szUnitDisk,
			0,
			KEY_ALL_ACCESS,
			&hKeyUnitDisk
			);

		if(ERROR_SUCCESS != lResult)
			break;

		DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : SetValueEx %d\n", iDisabled));

		lResult = RegSetValueEx(
			hKeyUnitDisk,
			"DisabledByUser",
			0,
			REG_DWORD,
			(LPBYTE)&iDisabled,
			sizeof(DWORD));

		DebugPrint(4, ("[NDASOpLib] NDAS_DisabledByUser : SetValueEx %d\n", lResult));
		if(ERROR_SUCCESS != lResult)
			break;

		RegCloseKey(hKeyUnitDisk);

		break;
	}

out:
	return TRUE;
}

BOOL
NDAS_ClearInfo(UNIT_DISK_LOCATION *pUnitDisk)
{
	BOOL bReturn;
	IDE_COMMAND_IO aCommands[3];

	// clear MBR
	aCommands[0].command = WIN_WRITE;
	aCommands[0].iSector = 0;
	ZeroMemory(aCommands[0].data, 512);

	// clear S0
	aCommands[1].command = WIN_WRITE;
	aCommands[1].iSector = -1;
	ZeroMemory(aCommands[1].data, 512);

	// clear S1
	aCommands[2].command = WIN_WRITE;
	aCommands[2].iSector = -2;
	ZeroMemory(aCommands[2].data, 512);

	bReturn = NDAS_IdeIO(pUnitDisk, 3, aCommands, NULL);

	if(bReturn)
	{
//		NDAS_DisabledByUser(pUnitDisk, 1);
	}

	return bReturn;
}

BOOL NDAS_GetStatus(UNIT_DISK_LOCATION *pUnitDisk, PNDAS_STATUS pStatus)
{
	BOOL bReturn = FALSE;
	IDE_COMMAND_IO cmd[2];
	LONG lResult;

	HKEY hKeyNetDisks;
	HKEY hKey;
	UINT i;
    CHAR     achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string 
    FILETIME ftLastWriteTime;      // last write time 
	CHAR szAddress[MAX_VALUE_NAME], szAddress2[18];
	UCHAR szSerial[MAX_VALUE_NAME];
	DWORD dwType;
	DWORD dwSize;
	
	LPX_ADDRESS address;
	LANSCSI_PATH path;

	DISK_INFORMATION_BLOCK *pDiskInfoV1;
	DISK_INFORMATION_BLOCK_V2 *pDiskInfoV2;
	
	DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : %02X:%02X:%02X:%02X:%02X:%02X\n",
		(int)pUnitDisk->MACAddr[0],
		(int)pUnitDisk->MACAddr[1],
		(int)pUnitDisk->MACAddr[2],
		(int)pUnitDisk->MACAddr[3],
		(int)pUnitDisk->MACAddr[4],
		(int)pUnitDisk->MACAddr[5]
		));

	ZeroMemory(&path, sizeof(LANSCSI_PATH));
	CopyMemory(address.Node, pUnitDisk->MACAddr, 6);

	if(NULL == pStatus)
		goto out;

	ZeroMemory(pStatus, sizeof(NDAS_STATUS));

	// Registry check start.
	// AING_TO_DO : use NDAS registry functions
	lResult = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE,
		KEY_NAME_NETDISKS,
		0,
		KEY_READ,
		&hKeyNetDisks
		);

	if(ERROR_SUCCESS != lResult)
		goto out;

	sprintf(szAddress2, "%02x:%02x:%02x:%02x:%02x:%02x",
		pUnitDisk->MACAddr[0],
		pUnitDisk->MACAddr[1],
		pUnitDisk->MACAddr[2],
		pUnitDisk->MACAddr[3],
		pUnitDisk->MACAddr[4],
		pUnitDisk->MACAddr[5]);

	i = 0;

	while(1)
	{
		cbName = MAX_KEY_LENGTH;
		lResult = RegEnumKeyEx(
			hKeyNetDisks,
			i++,
			achKey,
			&cbName,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);

		if(ERROR_SUCCESS != lResult)
			goto out;

		lResult = RegOpenKeyEx(
			hKeyNetDisks,
			achKey,
			0,
			KEY_READ,
			&hKey
			);

		if(ERROR_SUCCESS != lResult)
			goto out;

		dwSize = MAX_VALUE_NAME;
		
		lResult = RegQueryValueEx(
			hKey,
			"Address",
			0,
			&dwType,
			szAddress,
			&dwSize);

		if(ERROR_SUCCESS != lResult)
			continue;

		if(stricmp(szAddress, szAddress2))
			continue;

		pStatus->IsRegistered = 1;

		dwSize = MAX_VALUE_NAME;
		
		lResult = RegQueryValueEx(
			hKey,
			"SerialKey",
			0,
			&dwType,
			szSerial,
			&dwSize);

		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : lResult  = %d szSerial = %x %x %x %x %x %x %x %x %x, %d\n", 
			lResult,
			(int)szSerial[0],
			(int)szSerial[1],
			(int)szSerial[2],
			(int)szSerial[3],
			(int)szSerial[4],
			(int)szSerial[5],
			(int)szSerial[6],
			(int)szSerial[7],
			(int)szSerial[8],
			(szSerial[8] == 0xff)
			));
		
		if(ERROR_SUCCESS == lResult && szSerial[8] == 0xff)
		{
			pStatus->IsRegisteredWritable = 1;
		}

		break;
	}

	// Registry check end.

	// Connect
	if(!MakeConnection(&address, &path) || (unsigned int)NULL == path.connsock)
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : MakeConnection Failed\n"));
		goto out;
	}

	pStatus->IsAlive = 1;

	// Login
	// if we don't write, login read only
	path.iUserID = (pUnitDisk->SlotNumber +1);

	path.iCommandTag = 0;
	path.HPID = 0;
	path.iHeaderEncryptAlgo = 0;
	path.iDataEncryptAlgo = 0;
	NDAS_SetPassword(address.Node, &path.iPassword);
	path.iSessionPhase = LOGOUT_PHASE;

	if(Login(&path, LOGIN_TYPE_NORMAL))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : Login Failed\n"));
		goto out;
	}
	
	if(GetDiskInfo(&path, pUnitDisk->SlotNumber))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : GetDiskInfo Failed\n"));
		goto out;
	}

	if(Logout(&path))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : Logout Failed\n"));
		goto out;
	}

	if(0 != Discovery(&path))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : Discovery Failed\n"));
		goto out;
	}

	closesocket(path.connsock);

	pStatus->IsDiscovered = 1;
	pStatus->HWVersion = path.HWVersion;
	pStatus->HWProtoVersion = path.HWProtoVersion;
	pStatus->NrUserReadWrite = path.PerTarget[pUnitDisk->SlotNumber].NRRWHost;
	pStatus->NrUserReadOnly = path.PerTarget[pUnitDisk->SlotNumber].NRROHost;

	if(MEDIA_TYPE_BLOCK_DEVICE != path.PerTarget[pUnitDisk->SlotNumber].MediaType)
	{
		switch(path.PerTarget[pUnitDisk->SlotNumber].MediaType)
		{
		case MEDIA_TYPE_CDROM_DEVICE:
			pStatus->DiskType = DISK_TYPE_DVD;
			pStatus->IsSupported = 1;
			break;

		case MEDIA_TYPE_OPMEM_DEVICE:
			pStatus->DiskType = DISK_TYPE_MO;
			pStatus->IsSupported = 1;
			break;
		default:
			pStatus->IsSupported = 0;
		}

		DebugPrint(1, ("[NDASOpLib] NDAS_GetStatus : Packet type %d\n", path.PerTarget[pUnitDisk->SlotNumber].MediaType));
		bReturn = TRUE;
		goto out;
	}

	cmd[0].command = WIN_READ;
	cmd[0].iSector = -1;
	pDiskInfoV1 = (PDISK_INFORMATION_BLOCK)cmd[0].data;
	cmd[1].command = WIN_READ;
	cmd[1].iSector = -2;
	pDiskInfoV2 = (PDISK_INFORMATION_BLOCK_V2)cmd[1].data;

	NDAS_IdeIO(pUnitDisk, 2, cmd, NULL);

	if(DISK_INFORMATION_SIGNATURE_V2 == pDiskInfoV2->Signature)
	{
		pStatus->MajorVersion = pDiskInfoV2->MajorVersion;
		pStatus->MinorVersion = pDiskInfoV2->MinorVersion;

		if(IS_HIGHER_VERSION_V2(*pDiskInfoV2))
			goto out;

		pStatus->IsSupported = 1;

		pStatus->DiskType = 
			(1 == pDiskInfoV2->nDiskCount) ? DISK_TYPE_NORMAL :
			(NMT_RAID1 == pDiskInfoV2->iMediaType) ? DISK_TYPE_BIND_RAID1 :
			(NMT_VDVD == pDiskInfoV2->iMediaType) ? DISK_TYPE_VDVD :
			DISK_TYPE_AGGREGATION;
	}
	else if(DISK_INFORMATION_SIGNATURE == pDiskInfoV1->Signature)
	{
		pStatus->MajorVersion = pDiskInfoV1->MajorVersion;
		pStatus->MinorVersion = pDiskInfoV1->MinorVersion;
		
		if(IS_WRONG_VERSION(*pDiskInfoV1))
			goto out;
	
		pStatus->IsSupported = 1;

		pStatus->DiskType = 
			(UNITDISK_TYPE_SINGLE == pDiskInfoV1->DiskType) ? DISK_TYPE_NORMAL :
			(UNITDISK_TYPE_AGGREGATION_FIRST == pDiskInfoV1->DiskType) ? DISK_TYPE_AGGREGATION :
			(UNITDISK_TYPE_AGGREGATION_SECOND == pDiskInfoV1->DiskType) ? DISK_TYPE_AGGREGATION :
			(UNITDISK_TYPE_AGGREGATION_THIRD == pDiskInfoV1->DiskType) ? DISK_TYPE_AGGREGATION :
			(UNITDISK_TYPE_AGGREGATION_FOURTH == pDiskInfoV1->DiskType) ? DISK_TYPE_AGGREGATION :
			(UNITDISK_TYPE_MIRROR_MASTER == pDiskInfoV1->DiskType) ? DISK_TYPE_BIND_RAID1 :
			(UNITDISK_TYPE_MIRROR_SLAVE == pDiskInfoV1->DiskType) ? DISK_TYPE_BIND_RAID1 :
			(UNITDISK_TYPE_VDVD == pDiskInfoV1->DiskType) ? DISK_TYPE_VDVD : 0;
	}
	else
	{
		pStatus->MajorVersion = 0;
		pStatus->MinorVersion = 0;
		pStatus->DiskType = DISK_TYPE_NORMAL;
		pStatus->IsSupported = 1;
	}

	bReturn = TRUE;
out:
	if(path.connsock)
		closesocket(path.connsock);

	return bReturn;
}


/*
	iMirrorLevel
	0 : do not mirror
	1 : mirror, full dirty
	2 : mirror, clean
*/
BOOL
NDAS_Bind(UINT nDiskCount, UNIT_DISK_LOCATION *aUnitDisks, int iMirrorLevel)
{
	IDE_COMMAND_IO *cmd = NULL;
	unsigned int i, j;
	_int64 nDiskSize;
	LANSCSI_PATH path;
	DISK_INFORMATION_BLOCK *pDiskInfoV1;
	DISK_INFORMATION_BLOCK_V2 *pDiskInfoV2;
	unsigned _int64 nBitmapSize, sizeUserSpace, sizeXArea;
	BOOL bReturn = FALSE;
	unsigned int nDIB;

	if(nDiskCount <= 1)
		goto out;

	if(iMirrorLevel && 0 != nDiskCount %2)
		goto out;

	// mirror : 0 - 1 group
	// 128 * n sectors devide
	// absolute 2 mega
	// 0x80 for V1 disk type
	// all single !
	for(i = 0; i < nDiskCount; i++)
	{
//		if(!NDAS_IsBindable(&aUnitDisks[i], &bResult) || FALSE == bResult)
//			return FALSE;		
	}

	nDIB = 1 /* DISK_INFORMATION_BLOCK */ + 1 /* DISK_INFORMATION_BLOCK_V2 */ + GET_TRAIL_SECTOR_COUNT_V2(nDiskCount);
	cmd = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDE_COMMAND_IO) * nDIB);

	// build cmd
	// V1
	cmd[0].command = WIN_WRITE;
	cmd[0].iSector = -1;
	pDiskInfoV1 = (PDISK_INFORMATION_BLOCK)cmd[0].data;
	pDiskInfoV1->Signature = DISK_INFORMATION_SIGNATURE;
	pDiskInfoV1->MajorVersion = 1;
	pDiskInfoV1->MinorVersion = 0;
	pDiskInfoV1->DiskType = UNITDISK_TYPE_INVALID; // intentional wrong value

	// V2
	for(i = 1; i < nDIB; i++)
	{
		cmd[i].command = WIN_WRITE;
		cmd[i].iSector = -2 - ((signed _int64)i - 1);
	}
	pDiskInfoV2 = (PDISK_INFORMATION_BLOCK_V2)cmd[1].data;
	pDiskInfoV2->Signature = DISK_INFORMATION_SIGNATURE_V2;
	pDiskInfoV2->MajorVersion = CURRENT_MAJOR_VERSION_V2;
	pDiskInfoV2->MinorVersion = CURRENT_MINOR_VERSION_V2;
	pDiskInfoV2->iSectorsPerBit = 128;
	pDiskInfoV2->iMediaType = (iMirrorLevel) ? NMT_RAID1 : NMT_AGGREGATE;
	pDiskInfoV2->nDiskCount = nDiskCount;
	
	// if nDiskCount > NDAS_MAX_UNITS_IN_V2, aUnitDisks will be copied into cmd[2], cmd[3]...
	CopyMemory(pDiskInfoV2->UnitDisks, aUnitDisks, min(nDiskCount, NDAS_MAX_UNITS_IN_V2) * sizeof(UNIT_DISK_LOCATION));

	if(nDiskCount > NDAS_MAX_UNITS_IN_V2) // fill trailing sectors
	{
		int nCopyPoint;
		for(i = 2; i < nDIB; i++)
		{
			nCopyPoint = NDAS_MAX_UNITS_IN_SECTOR * (i - 2) + NDAS_MAX_UNITS_IN_V2;
			CopyMemory(cmd[i].data, &aUnitDisks[nCopyPoint], min(nDiskCount - nCopyPoint, NDAS_MAX_UNITS_IN_SECTOR) * sizeof(UNIT_DISK_LOCATION));
		}
			
	}

	// size checker

	for(i = 0; i < nDiskCount; i++)
	{

#ifdef _DEBUG
		char szTemp[100];
		sprintf(szTemp, "[NDASOpLib]NDAS_Bind: %d\n", i);
		OutputDebugString(szTemp);
#endif
		
		// V1
		CopyMemory(pDiskInfoV1->EtherAddress, aUnitDisks[i].MACAddr, 6);
		pDiskInfoV1->UnitNumber = aUnitDisks[i].SlotNumber;

		// V2
		NDAS_IdeIO(&aUnitDisks[i], 0, NULL, &path);
		nDiskSize = path.PerTarget[aUnitDisks[i].SlotNumber].SectorCount;

		// start to calc sizeUserSpace, sizeXArea
		sizeUserSpace = nDiskSize;
		
		if(iMirrorLevel)
		{
			// choose smaller size
			NDAS_IdeIO(&aUnitDisks[i ^1], 0, NULL, &path);			
			sizeUserSpace = min(sizeUserSpace, path.PerTarget[aUnitDisks[i ^1].SlotNumber].SectorCount);
		}

		nBitmapSize = (sizeUserSpace / pDiskInfoV2->iSectorsPerBit /* bits needed */) / 8 /* bytes needed */ / 512 /* sectors needed */ +1 /* last sector */; // backup bitmap size

		sizeXArea = nBitmapSize;
		sizeXArea *= 2; // double bitmap
		sizeXArea += 1 + 1 + 10; // S0, S1 and trails
		sizeXArea = max(2 * 1024 * 2, sizeXArea); // at least 2mega

		// reduce X Area size
		sizeUserSpace -= sizeXArea;
		sizeUserSpace -= sizeUserSpace % 128; // multiples of 128

		// AING_TO_DO : reduce sizeUserSpace so that bound disk size won't be over 2TB limit
//		sizeUserSpace = 160 * 128; // 10MB


		pDiskInfoV2->sizeXArea = sizeXArea;
		pDiskInfoV2->sizeUserSpace = sizeUserSpace;
		// end to calc sizeUserSpace, sizeXArea

		pDiskInfoV2->iSequence = i;

		// dirt or clean
		if(1 == iMirrorLevel)
		{
			pDiskInfoV2->FlagDirty = TRUE;
		}
		else
		{
			pDiskInfoV2->FlagDirty = FALSE;
		}

		// write S0, S1
		NDAS_IdeIO(&aUnitDisks[i], nDIB, cmd, NULL);

		// dirt or clean bitmap
		if(iMirrorLevel)
		{
			IDE_COMMAND_IO *aCmds;
			aCmds = HeapAlloc(GetProcessHeap(), 0, sizeof(IDE_COMMAND_IO) * (unsigned _int32)nBitmapSize);
			for(j = 0; j < nBitmapSize; j++)
			{
				aCmds[j].command = WIN_WRITE;
				aCmds[j].iSector = (-1 * sizeXArea) + (signed _int64)j;
				FillMemory(aCmds[j].data, 512, (iMirrorLevel == 1) ? 0xff : 0x00);
			}
			
			NDAS_IdeIO(&aUnitDisks[i], (unsigned _int32)nBitmapSize, aCmds, NULL);
			HeapFree(GetProcessHeap(), 0, aCmds);
		}
	}

	bReturn = TRUE;
out:

	if(NULL != cmd)
	{
		HeapFree(GetProcessHeap(), 0, cmd);
		cmd = NULL;
	}

	return bReturn;
}

BOOL
NDAS_Unbind(UNIT_DISK_LOCATION *pUnitDisk)
{
	// AING_TO_DO : complex check routine

	BOOL bReturn = FALSE;
	IDE_COMMAND_IO cmd, *cmd_chk = NULL;
	DISK_INFORMATION_BLOCK	*pDiskInfoV1;
	DISK_INFORMATION_BLOCK_V2 *pDiskInfoV2;
	UINT i, j;
	UINT nDIB;

	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : Start\n"));

	// Check V2
	cmd.command = WIN_READ;
	cmd.iSector = -2;

	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : Reading V2\n"));
	if(!NDAS_IdeIO(pUnitDisk, 1, &cmd, NULL))
		goto out;
	
	pDiskInfoV2 = (PDISK_INFORMATION_BLOCK_V2)cmd.data;

	if(DISK_INFORMATION_SIGNATURE_V2 != pDiskInfoV2->Signature) // V2 information exists
		goto chk_v1;
	
	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : V2\n"));

	if(IS_HIGHER_VERSION_V2(*pDiskInfoV2))
		goto out;

	// AING_TO_DO :version check routine needed here

	if(pDiskInfoV2->nDiskCount <= 1)
	{
		// nothing to do
		bReturn = TRUE;
		goto out;
	}

	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : Disk count >= 2\n"));

	if(NMT_VDVD == pDiskInfoV2->iMediaType)
		goto out;

	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : not VDVD\n"));
	// AING_TO_DO : if nr of disks is more than NDAS_MAX_UNITS_IN_V2, read more sectors...
	nDIB = 1 + /* DISK_INFORMATION_BLOCK */ + 1 /* DISK_INFORMATION_BLOCK_V2 */ + GET_TRAIL_SECTOR_COUNT_V2(pDiskInfoV2->nDiskCount);
	cmd_chk = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDE_COMMAND_IO) * nDIB);

	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : nDIB = %d\n", nDIB));
	for(i = 0; i < nDIB; i++)
	{
		cmd_chk[i].command = WIN_READ;
		cmd_chk[i].iSector = -1 * ((signed _int64)i);
	}
	
	DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : nDiskCount = %d\n", pDiskInfoV2->nDiskCount));
	for(i = 0; i < pDiskInfoV2->nDiskCount; i++) // for each netdisks
	{
		DISK_INFORMATION_BLOCK	*pDIV1;
		DISK_INFORMATION_BLOCK_V2 *pDIV2;

		// verify that the disk is bound correctly
		if(!NDAS_IdeIO(&pUnitDisk[i], nDIB, cmd_chk, NULL))
			continue;

		pDIV1 = (PDISK_INFORMATION_BLOCK)cmd_chk[0].data;
		pDIV2 = (PDISK_INFORMATION_BLOCK_V2)cmd_chk[1].data;

		// check V1 data
		if(DISK_INFORMATION_SIGNATURE != pDIV1->Signature ||
			IS_WRONG_VERSION(*pDIV1))
			continue;

		// check V2 data
		if(DISK_INFORMATION_SIGNATURE_V2 != pDIV2->Signature)
			continue;

		// higher version check code
		if(IS_HIGHER_VERSION_V2(*pDIV2))
			continue;

		if(pDIV2->nDiskCount != pDiskInfoV2->nDiskCount ||
			pDIV2->iSequence != i ||
			pDIV2->iMediaType != pDiskInfoV2->iMediaType)
			continue;

		// compare unit disk informations
		if(memcmp(pDIV2->UnitDisks, pDiskInfoV2->UnitDisks, min(pDiskInfoV2->nDiskCount, NDAS_MAX_UNITS_IN_V2) * sizeof(UNIT_DISK_LOCATION)))
			continue;

		if(pDiskInfoV2->nDiskCount > NDAS_MAX_UNITS_IN_V2)
		{
			for(j = NDAS_MAX_UNITS_IN_V2; j < pDiskInfoV2->nDiskCount; j++)
			{
				if(memcmp(&cmd_chk[2 + (j - NDAS_MAX_UNITS_IN_V2) / NDAS_MAX_UNITS_IN_SECTOR].data[(j - NDAS_MAX_UNITS_IN_V2) % NDAS_MAX_UNITS_IN_SECTOR], &pUnitDisk[j], sizeof(UNIT_DISK_LOCATION)))
					continue;
			}
		}

		DebugPrint(1, ("[NDASOpLib] NDAS_Unbind : Clear %d\n", i));

		// ok it is safe to clear
		if(!NDAS_ClearInfo(&(pDiskInfoV2->UnitDisks[i])))
			continue;
	}

//	if(!NDAS_Unbind(pAddress, iTargetID);

	bReturn = TRUE;
	goto out;

chk_v1:
	// Check V1
	cmd.command = WIN_READ;
	cmd.iSector = -1;

	if(!NDAS_IdeIO(pUnitDisk, 1, &cmd, NULL))
		goto out;
	
	pDiskInfoV1 = (PDISK_INFORMATION_BLOCK)cmd.data;

	if(DISK_INFORMATION_SIGNATURE != pDiskInfoV1->Signature)
	{
		// already single
		bReturn = TRUE;
		goto out;
	}

	// only 2 disks bound
	if(
		UNITDISK_TYPE_DVD == pDiskInfoV1->DiskType ||
		UNITDISK_TYPE_VDVD == pDiskInfoV1->DiskType ||
		UNITDISK_TYPE_MO == pDiskInfoV1->DiskType
		)
	{
		goto out;
	}

	if(UNITDISK_TYPE_SINGLE == pDiskInfoV1->DiskType)
	{
		if(!NDAS_ClearInfo(pUnitDisk))
			goto out;
	}
	else
	{
		UNIT_DISK_LOCATION UnitPeer;
		CopyMemory(UnitPeer.MACAddr, pDiskInfoV1->PeerAddress, 6);
		UnitPeer.SlotNumber = pDiskInfoV1->PeerUnitNumber;

		if(!NDAS_ClearInfo(pUnitDisk))
			goto out;

		cmd_chk = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDE_COMMAND_IO));
		cmd_chk->command = WIN_READ;
		cmd_chk->iSector = -1;

		if(
			NDAS_IdeIO(&UnitPeer, 1, cmd_chk, NULL) &&
			0 == memcmp(UnitPeer.MACAddr, ((PDISK_INFORMATION_BLOCK)cmd_chk->data)->PeerAddress, 6) &&
			UnitPeer.SlotNumber == ((PDISK_INFORMATION_BLOCK)cmd_chk->data)->PeerUnitNumber
			)
		{

			if(!NDAS_ClearInfo(&UnitPeer))
				goto out;
		}
	}

	bReturn = TRUE;
out:	

	if(NULL != cmd_chk)
	{
		HeapFree(GetProcessHeap(), 0, cmd_chk);
		cmd_chk = NULL;
	}
	return bReturn;
}

/*
BOOL NDAS_IsDirty(UNIT_DISK_LOCATION *pUnitDisk, unsigned _int32 *pFlagDirty)
{
	IDE_COMMAND_IO cmd_chk;
	DISK_INFORMATION_BLOCK_V2* pDiskInfoV2;

	DebugPrint(1, ("[NDASOpLib] NDAS_IsDirty : %02X:%02X:%02X:%02X:%02X:%02X\n",
		(int)pUnitDisk->MACAddr[0],
		(int)pUnitDisk->MACAddr[1],
		(int)pUnitDisk->MACAddr[2],
		(int)pUnitDisk->MACAddr[3],
		(int)pUnitDisk->MACAddr[4],
		(int)pUnitDisk->MACAddr[5]
		));

	cmd_chk.command = WIN_READ;
	cmd_chk.iSector = -2;

	if(!NDAS_IdeIO(pUnitDisk, 1, &cmd_chk, NULL))
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_IsDirty : NDAS_IdeIO Failed\n"));
		return FALSE;
	}

	pDiskInfoV2 = (PDISK_INFORMATION_BLOCK_V2)cmd_chk.data;

	if(DISK_INFORMATION_SIGNATURE_V2 != pDiskInfoV2->Signature)
	{
		DebugPrint(1, ("[NDASOpLib] NDAS_IsDirty : Signature Failed\n"));
		return FALSE;
	}

	if(NDAS_DIRTY_MIRROR_DIRTY & pDiskInfoV2->FlagDirty)
		*pFlagDirty |= NDAS_DIRTY_MIRROR_DIRTY;
	else
		*pFlagDirty &= ~NDAS_DIRTY_MIRROR_DIRTY;

	return TRUE;
}
*/
