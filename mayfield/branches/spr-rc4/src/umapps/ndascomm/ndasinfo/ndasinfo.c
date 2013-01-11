/*
ndasinfo.cpp : Retrieves informations of the NDAS Device
Copyright(C) 2002-2005 XIMETA, Inc.

Gi youl Kim <kykim@ximeta.com>

This library is provided to support manufacturing processes and
for diagnostic purpose. Do not redistribute this library.
*/

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <ndas/ndascomm.h> /* include this header file to use NDASComm API */
#include <ndas/ndasmsg.h> /* included to process Last Error from NDASComm API */
#include "hdreg.h"

#define API_CALL(_stmt_) \
	do { \
	if (!(_stmt_)) { \
	ShowErrorMessage(); \
	return (int)GetLastError(); \
	} \
	} while(0)

#define API_CALL_JMP(_stmt_, jmp_loc) \
	do { \
	if (!(_stmt_)) { \
	ShowErrorMessage(); \
	goto jmp_loc; \
	} \
	} while(0)

static
void ShowErrorMessage();

void Usage()
{
	printf(
		"usage: ndasinfo (-i ID | -m Mac)\n"
		"\n"
		"ID : 20 chars of id and 5 chars of key of the NDAS Device ex(01234ABCDE56789FGHIJ13579)\n"
		"Mac : Mac address of the NDAS Device ex(00:01:10:00:12:34)\n"
		);
}

int __cdecl main(int argc, char *argv[])
{
	BOOL bResult;
	int i, j, x;

	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	BOOL bParameterOk;
	CHAR szBuffer[100];
	BYTE pbData[8];
	const DWORD cbData = sizeof(pbData);
	UINT32 iNRTargets;
	NDASCOMM_UNIT_DEVICE_STAT UnitStat;
	NDASCOMM_UNIT_DEVICE_INFO UnitInfo;
	NDASCOMM_IDE_REGISTER ide_register;
	struct hd_driveid info;

	API_CALL(NdasCommInitialize());

	ZeroMemory(&ci, sizeof(ci));

	ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A; /* ASCII char set */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.bWriteAccess = TRUE; /* Connect with read-write privilege */
	ci.protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.ui64OEMCode = 0; /* Use default password */
	ci.bSupervisor = FALSE; /* Log in as normal user */
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */

	bParameterOk = FALSE;
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-i") && ++i < argc)
		{
			ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A;
			if(IsBadReadPtr(argv[i], 20))
			{
				Usage();
				return 0;
			}
			strncpy(ci.DeviceIDA.szDeviceStringId, argv[i], 20);
			strncpy(ci.DeviceIDA.szDeviceStringKey, argv[i] +20, 5);

			bParameterOk = TRUE;
		}
		else if(!strcmp(argv[i], "-m") && ++i < argc)
		{
			ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
			if(IsBadReadPtr(argv[i], 17))
			{
				Usage();
				return 0;
			}
			sscanf(argv[i], "%02x:%02x:%02x:%02x:%02x:%02x",
			&ci.AddressLPX[0],
			&ci.AddressLPX[1],
			&ci.AddressLPX[2],
			&ci.AddressLPX[3],
			&ci.AddressLPX[4],
			&ci.AddressLPX[5]);

			bParameterOk = TRUE;
		}
	}

	if(!bParameterOk)
	{
		Usage();
		return 0;
	}


	API_CALL_JMP(hNDAS = NdasCommConnect(&ci, 0, NULL), out);

	/* NDAS device information */

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_type, pbData, cbData), out);
	printf("Hardware Type : %d\n", *(BYTE*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_version, pbData, cbData), out);
	printf("Hardware Version : %d\n", *(BYTE*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_proto_type, pbData, cbData), out);
	printf("Hardware Protocol Type : %d\n", *(BYTE*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_proto_version, pbData, cbData), out);
	printf("Hardware Protocol Version : %d\n", *(BYTE*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_num_slot, pbData, cbData), out);
	printf("Number of slot : %d\n", *(DWORD*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_blocks, pbData, cbData), out);
	printf("Maximum transfer blocks : %d\n", *(DWORD*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_targets, pbData, cbData), out);
	printf("Maximum targets : %d\n", *(DWORD*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_lus, pbData, cbData), out);
	printf("Maximum LUs : %d\n", *(DWORD*)pbData);

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_header_encrypt_algo, pbData, cbData), out);
	printf("Header Encryption : %s\n", *(WORD*)pbData ? "YES" : "NO");

	API_CALL_JMP(NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_data_encrypt_algo, pbData, cbData), out);
	printf("Data Encryption : %s\n", *(WORD*)pbData ? "YES" : "NO");


	printf("\n");

	API_CALL_JMP(NdasCommDisconnect(hNDAS), out);
	hNDAS = NULL;

	/* get unit device number */
	ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	API_CALL_JMP(NdasCommGetUnitDeviceStat(&ci, &UnitStat, 0, NULL), out);

	iNRTargets = UnitStat.iNRTargets;

	printf("Number of targets : %d\n", iNRTargets);

	for(i = 0; i < (int)iNRTargets; i++)
	{
		printf("Unit device information [%d/%d] :\n", i, iNRTargets);
		ci.UnitNo = i;
		ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		API_CALL_JMP(hNDAS = NdasCommConnect(&ci, 0, NULL), out);

		API_CALL_JMP(NdasCommGetUnitDeviceInfo(hNDAS, &UnitInfo), out);

		ide_register.use_dma = 0;
		ide_register.use_48 = 0;
		ide_register.device.lba_head_nr = 0;
		ide_register.device.dev = (0 == i) ? 0 : 1;
		ide_register.device.lba = 0;
		ide_register.command.command = WIN_IDENTIFY;

		API_CALL_JMP(NdasCommIdeCommand(hNDAS, &ide_register, NULL, 0, (PBYTE)&info, sizeof(info)), out);

		API_CALL_JMP(NdasCommDisconnect(hNDAS), out);
		hNDAS = NULL;

		printf("\tSector count : %I64d\n", UnitInfo.SectorCount);
		printf("\tSupports LBA : %s\n", (UnitInfo.bLBA) ? "YES" : "NO");
		printf("\tSupports LBA48 : %s\n", (UnitInfo.bLBA48) ? "YES" : "NO");
		printf("\tSupports PIO : %s\n", (UnitInfo.bPIO) ? "YES" : "NO");
		printf("\tSupports DMA : %s\n", (UnitInfo.bDma) ? "YES" : "NO");
		printf("\tSupports UDMA : %s\n", (UnitInfo.bUDma) ? "YES" : "NO");
		printf("\tSupports FLUSH CACHE : Supports - %s, Enabled - %s\n", (info.command_set_2 & 0x1000) ? "YES" : "NO", (info.cfs_enable_2 & 0x1000) ? "YES" : "NO");
		printf("\tSupports FLUSH CACHE EXT : Supports - %s, Enabled - %s\n", (info.command_set_2 & 0x2000) ? "YES" : "NO", (info.cfs_enable_2 & 0x2000) ? "YES" : "NO");
		memcpy(szBuffer, (const char *)UnitInfo.Model, sizeof(UnitInfo.Model)); szBuffer[sizeof(UnitInfo.Model)] = '\0';
		printf("\tModel : %s\n", szBuffer);
		strncpy(szBuffer, (const char *)UnitInfo.FwRev, sizeof(UnitInfo.FwRev)); szBuffer[sizeof(UnitInfo.FwRev)] = '\0';
		printf("\tFirmware Rev : %s\n", szBuffer);
		strncpy(szBuffer, (const char *)UnitInfo.SerialNo, sizeof(UnitInfo.SerialNo)); szBuffer[sizeof(UnitInfo.SerialNo)] = '\0';
		printf("\tSerial number : %s\n", szBuffer);
		printf("\tMedia type : %s\n",
			(UnitInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE) ? "Unknown device" :
			(UnitInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE) ? "Non-packet mass-storage device (HDD)" :
			(UnitInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_COMPACT_BLOCK_DEVICE) ? "Non-packet compact storage device (Flash card)" :
			(UnitInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE) ? "CD-ROM device (CD/DVD)" :
			(UnitInfo.MediaType == NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE) ? "Optical memory device (MO)" :
			"Unknown device"
			);
		printf("\n");

		ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
		API_CALL_JMP(NdasCommGetUnitDeviceStat(&ci, &UnitStat, 0, NULL), out);

		/* Check Ultra DMA mode */
		for(j = 7; j >= 0; j--)
		{
			if(info.dma_ultra & (0x01 << j))
			{
				printf("\tUltra DMA mode %d and below are supported\n", j);
				break;
			}
		}
		for(j = 7; j >= 0; j--)
		{
			if(info.dma_ultra & (0x01 << (j + 8)))
			{
				printf("\tUltra DMA mode %d selected\n", j);
				break;
			}
		}
		if(j < 0)
			printf("\tUltra DMA mode is not selected\n");

		/* Check DMA mode */
		for(j = 2; j >= 0; j--)
		{
			if(info.dma_mword & (0x01 << j))
			{
				printf("\tDMA mode %d and below are supported\n", j);
				break;
			}
		}
		for(j = 2; j >= 0; j--)
		{
			if(info.dma_mword & (0x01 << (j + 8)))
			{
				printf("\tDMA mode %d selected\n", j);
				break;
			}
		}
		if(j < 0)
			printf("\tDMA mode is not selected\n");
		printf("\n");



		printf("\tPresent : %s\n", (UnitStat.bPresent) ? "YES" : "NO");
		printf("\tNumber of hosts with RW priviliage : %d\n", UnitStat.NRRWHost);
		printf("\tNumber of hosts with RO priviliage : %d\n", UnitStat.NRROHost);

		printf("\tTarget data : ");
		for(j = 0; j < 8; j++)
		{
			x = (int)(((PBYTE)&UnitStat.TargetData)[j]);
			printf("%02X ", x);
		}
		printf("\n");
		printf("\n");
	}

out:
	if(hNDAS)
	{
		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;
	}

	API_CALL(NdasCommUninitialize());
	return GetLastError();
}

void
ShowErrorMessage()
{
	DWORD dwError = GetLastError();

	HMODULE hModule = LoadLibraryEx(
		_T("ndasmsg.dll"),
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	LPTSTR lpszErrorMessage = NULL;

	if (dwError & APPLICATION_ERROR_MASK) {
		if (NULL != hModule) {

			INT iChars = FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}
	}
	else
	{
		INT iChars = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwError,
			0,
			(LPTSTR) &lpszErrorMessage,
			0,
			NULL);
	}

	if (NULL != lpszErrorMessage) {
		_tprintf(_T("Error : %d(%08x) : %s\n"), dwError, dwError, lpszErrorMessage);
		LocalFree(lpszErrorMessage);
	}
	else
	{
		_tprintf(_T("Unknown error : %d(%08x)\n"), dwError, dwError);
	}

	/* refresh error */
	SetLastError(dwError);
}
