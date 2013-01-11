#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <ndas/ndasuser.h>
#include <ndas/ndascomm.h>
#include <hdreg.h>

static
void ShowErrorMessage(BOOL bExit = FALSE);


void Usage(int err)
{
	printf(
		"usage: ndasinfo (-i ID | -m Mac)\n"
		"\n"
		"ID : 20 chars of id and 5 chars of key of the NDAS Device ex(01234ABCDE56789FGHIJ13579)\n"
		"Mac : Mac address of the NDAS Device ex(00:01:10:00:12:34)\n"
		);
	exit(err);
}

int __cdecl main(int argc, char *argv[])
{
	BOOL bResult;
	HNDAS hNdas;
	int i, j;
	int x;
	BOOL bParameterOk;
	CHAR buffer[100];

	bResult = NdasCommInitialize();

	if(!bResult)
	{
		ShowErrorMessage(TRUE);
	}

	NDASCOMM_CONNECTION_INFO ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.UnitNo = 0;
	ci.bWriteAccess = TRUE;
	ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;

	ci.protocol = NDASCOMM_TRANSPORT_LPX;

	bParameterOk = FALSE;
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-i") && ++i < argc)
		{			
			ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A;
			if(::IsBadReadPtr(argv[i], 20))
				Usage(2);
			strncpy(ci.DeviceIDA.szDeviceStringId, argv[i], 20);
			strncpy(ci.DeviceIDA.szDeviceStringKey, argv[i] +20, 5);
//				min(sizeof(ci.DeviceIDA.szDeviceStringId), strlen(argv[i])));

			bParameterOk = TRUE;
		}
		else if(!strcmp(argv[i], "-m") && ++i < argc)
		{
			ci.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
			if(::IsBadReadPtr(argv[i], 17))
				Usage(4);
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
		Usage(0);
	}


	HNDAS hNDAS;
	hNDAS = NdasCommConnect(&ci, 0, NULL);
	if(NULL == hNDAS)
	{
		ShowErrorMessage(TRUE);
	}

	// NDAS device information
	BYTE pbData[8];
	const DWORD cbData = sizeof(pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_type, pbData, cbData);
		printf("Hardware Type : %d\n", *(BYTE*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_version, pbData, cbData);
	printf("Hardware Version : %d\n", *(BYTE*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_proto_type, pbData, cbData);
	printf("Hardware Protocol Type : %d\n", *(BYTE*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_proto_version, pbData, cbData);
	printf("Hardware Protocol Version : %d\n", *(BYTE*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_num_slot, pbData, cbData);
	printf("Number of slot : %d\n", *(DWORD*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_blocks, pbData, cbData);
	printf("Maximum transfer blocks : %d\n", *(DWORD*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_targets, pbData, cbData);
	printf("Maximum targets : %d\n", *(DWORD*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_max_lus, pbData, cbData);
	printf("Maximum LUs : %d\n", *(DWORD*)pbData);

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_header_encrypt_algo, pbData, cbData);
	printf("Header Encryption : %s\n", *(WORD*)pbData ? "YES" : "NO");

	NdasCommGetDeviceInfo(hNDAS,ndascomm_handle_info_hw_data_encrypt_algo, pbData, cbData);
	printf("Data Encryption : %s\n", *(WORD*)pbData ? "YES" : "NO");


	printf("\n");

	NdasCommDisconnect(hNDAS);
	hNDAS = NULL;

	// get unit device number
	UINT32 iNRTargets;
	NDASCOMM_UNIT_DEVICE_STAT UnitStat;
	ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	bResult = NdasCommGetUnitDeviceStat(&ci, &UnitStat, 0, NULL);
	if(!bResult)
	{
		ShowErrorMessage(TRUE);
	}
	iNRTargets = UnitStat.iNRTargets;

	printf("Number of targets : %d\n", iNRTargets);	
	
	NDASCOMM_UNIT_DEVICE_INFO UnitInfo;
	NDASCOMM_IDE_REGISTER ide_register;
	hd_driveid info;	
	for(i = 0; i < (int)iNRTargets; i++)
	{
		printf("Unit device information [%d/%d] :\n", i, iNRTargets);
		ci.UnitNo = i;
		ci.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
		hNDAS = NdasCommConnect(&ci, 0, NULL);
		if(NULL == hNDAS)
		{
			ShowErrorMessage(TRUE);
		}

		bResult = NdasCommGetUnitDeviceInfo(hNDAS, &UnitInfo);
		if(!bResult)
		{
			ShowErrorMessage(TRUE);
		}

		ide_register.use_dma = 0;
		ide_register.use_48 = 0;
		ide_register.device.lba_head_nr = 0;
		ide_register.device.dev = (0 == i) ? 0 : 1;
		ide_register.device.lba = 0;
		ide_register.command.command = WIN_IDENTIFY;

		bResult = NdasCommIdeCommand(hNDAS, &ide_register, NULL, 0, (PBYTE)&info, sizeof(info));
		if(!bResult)
		{
			ShowErrorMessage(TRUE);
		}

		NdasCommDisconnect(hNDAS);
		hNDAS = NULL;

#define MEDIA_TYPE_UNKNOWN_DEVICE		0		// Unknown(not supported)
#define MEDIA_TYPE_BLOCK_DEVICE			1		// Non-packet mass-storage device (HDD)
#define MEDIA_TYPE_COMPACT_BLOCK_DEVICE 2		// Non-packet compact storage device (Flash card)
#define MEDIA_TYPE_CDROM_DEVICE			3		// CD-ROM device (CD/DVD)
#define MEDIA_TYPE_OPMEM_DEVICE			4		// Optical memory device (MO)

		printf("\tSector count : %I64d\n", UnitInfo.SectorCount);
		printf("\tSupports LBA : %s\n", (UnitInfo.bLBA) ? "YES" : "NO");
		printf("\tSupports LBA48 : %s\n", (UnitInfo.bLBA48) ? "YES" : "NO");
		printf("\tSupports PIO : %s\n", (UnitInfo.bPIO) ? "YES" : "NO");
		printf("\tSupports DMA : %s\n", (UnitInfo.bDma) ? "YES" : "NO");
		printf("\tSupports UDMA : %s\n", (UnitInfo.bUDma) ? "YES" : "NO");
		memcpy(buffer, (const char *)UnitInfo.Model, sizeof(UnitInfo.Model)); buffer[sizeof(UnitInfo.Model)] = '\0';
		printf("\tModel : %s\n", buffer);
		strncpy(buffer, (const char *)UnitInfo.FwRev, sizeof(UnitInfo.FwRev)); buffer[sizeof(UnitInfo.FwRev)] = '\0';
		printf("\tFirmware Rev : %s\n", buffer);
		strncpy(buffer, (const char *)UnitInfo.SerialNo, sizeof(UnitInfo.SerialNo)); buffer[sizeof(UnitInfo.SerialNo)] = '\0';
		printf("\tSerial number : %s\n", buffer);
		printf("\tMedia type : %s\n", 
			(UnitInfo.MediaType == MEDIA_TYPE_UNKNOWN_DEVICE) ? "Unknown device" :
			(UnitInfo.MediaType == MEDIA_TYPE_BLOCK_DEVICE) ? "Non-packet mass-storage device (HDD)" :
			(UnitInfo.MediaType == MEDIA_TYPE_COMPACT_BLOCK_DEVICE) ? "Non-packet compact storage device (Flash card)" :
			(UnitInfo.MediaType == MEDIA_TYPE_CDROM_DEVICE) ? "CD-ROM device (CD/DVD)" :
			(UnitInfo.MediaType == MEDIA_TYPE_OPMEM_DEVICE) ? "Optical memory device (MO)" :
			"Unknown device"
			);
		printf("\n");

		ci.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
		bResult = NdasCommGetUnitDeviceStat(&ci, &UnitStat, 0, NULL);
		if(!bResult)
		{
			ShowErrorMessage(TRUE);
		}

		// Check Ultra DMA mode
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

		// Check DMA mode
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

	return 0;
}

void 
ShowErrorMessage(BOOL bExit)
{
	DWORD dwError = ::GetLastError();

	HMODULE hModule = ::LoadLibraryEx(
		_T("ndasmsg.dll"), 
		NULL, 
		LOAD_LIBRARY_AS_DATAFILE);

	LPTSTR lpszErrorMessage = NULL;

	if (dwError & APPLICATION_ERROR_MASK) {
		if (NULL != hModule) {

			INT iChars = ::FormatMessage(
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
		INT iChars = ::FormatMessage(
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
		::LocalFree(lpszErrorMessage);
	}
	else
	{
		_tprintf(_T("Unknown error : %d(%08x)\n"), dwError, dwError);
	}

	if(bExit)
	{
		::SetLastError(dwError);
		exit(dwError);
	}
}
