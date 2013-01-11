#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#include <ndas/ndastype.h>
#include <ndas/ndasuser.h>
#include <ndas/ndascomm.h>
#include <socketlpx.h>

void ShowErrorMessage(BOOL bExit = FALSE)
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

void Usage(int err)
{
	printf(
		"usage: ndasinfo (-i ID | -m Mac)\n"
		"\n"
		"ID : 20 chars of id of the NDAS Device ex(01234ABCDE56789FGHIJ)\n"
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

	NDAS_CONNECTION_INFO ci;
	ZeroMemory(&ci, sizeof(ci));
	ci.UnitNo = 0;
	ci.bWriteAccess = FALSE;

	ci.protocol = IPPROTO_LPXTCP;

	bParameterOk = FALSE;
	for(i = 1; i < argc; i++)
	{
		if(!strcmp(argv[i], "-i") && ++i < argc)
		{			
			ci.type = NDAS_CONNECTION_INFO_TYPE_IDA;
			if(::IsBadReadPtr(argv[i], 20))
				Usage(2);
			strncpy(ci.szDeviceStringId, argv[i], 20);
//				min(sizeof(ci.szDeviceStringId), strlen(argv[i])));

			bParameterOk = TRUE;
		}
		else if(!strcmp(argv[i], "-m") && ++i < argc)
		{
			ci.type = NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS;
			if(::IsBadReadPtr(argv[i], 17))
				Usage(4);
			sscanf(argv[i], "%02x:%02x:%02x:%02x:%02x:%02x",
			&ci.MacAddress[0],
			&ci.MacAddress[1],
			&ci.MacAddress[2],
			&ci.MacAddress[3],
			&ci.MacAddress[4],
			&ci.MacAddress[5]);

			bParameterOk = TRUE;
		}
	}

	if(!bParameterOk)
	{
		Usage(0);
	}


	HNDAS hNDAS;
	hNDAS = NdasCommConnect(&ci);
	if(NULL == hNDAS)
	{
		ShowErrorMessage(TRUE);
	}

	// NDAS device information
	NDAS_DEVICE_INFO Info;
	bResult = NdasCommGetDeviceInfo(hNDAS, &Info);
	if(!bResult)
	{
		ShowErrorMessage(TRUE);
	}

	printf("Hardware Type : %d\n", Info.HWType);
	printf("Hardware Version : %d\n", Info.HWVersion);
	printf("Hardware Protocol Type : %d\n", Info.HWProtoType);
	printf("Hardware Protocol Version : %d\n", Info.HWProtoVersion);
	printf("Number of slot : %d\n", Info.iNumberofSlot);
	printf("Maximum transfer blocks : %d\n", Info.iMaxBlocks);
	printf("Maximum targets : %d\n", Info.iMaxTargets);
	printf("Maximum LUs : %d\n", Info.iMaxLUs);
	printf("Header Encryption : %s\n", (Info.iHeaderEncryptAlgo) ? "YES" : "NO");
	printf("Data Encryption : %s\n", (Info.iDataEncryptAlgo) ? "YES" : "NO");
	printf("\n");

	NdasCommDisconnect(hNDAS);
	hNDAS = NULL;

	// get unit device number
	UINT32 iNRTargets;
	NDAS_UNIT_DEVICE_DYN_INFO UnitDynInfo;
	bResult = NdasCommGetUnitDeviceDynInfo(&ci, &UnitDynInfo);
	if(!bResult)
	{
		ShowErrorMessage(TRUE);
	}
	iNRTargets = UnitDynInfo.iNRTargets;

	printf("Number of targets : %d\n", iNRTargets);	
	
	NDAS_UNIT_DEVICE_INFO UnitInfo;
	for(i = 0; i < (int)iNRTargets; i++)
	{
		printf("Unit device information [%d/%d] :\n", i, iNRTargets);
		ci.UnitNo = i;
		hNDAS = NdasCommConnect(&ci);
		if(NULL == hNDAS)
		{
			ShowErrorMessage(TRUE);
		}

		bResult = NdasCommGetUnitDeviceInfo(hNDAS, &UnitInfo);
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

		bResult = NdasCommGetUnitDeviceDynInfo(&ci, &UnitDynInfo);
		if(!bResult)
		{
			ShowErrorMessage(TRUE);
		}

		printf("\tPresent : %s\n", (UnitDynInfo.bPresent) ? "YES" : "NO");
		printf("\tNumber of hosts with RW priviliage : %d\n", UnitDynInfo.NRRWHost);
		printf("\tNumber of hosts with RO priviliage : %d\n", UnitDynInfo.NRROHost);

		printf("\tTarget data : ");
		for(j = 0; j < 8; j++)
		{
			x = (int)(((PBYTE)&UnitDynInfo.TargetData)[j]);
			printf("%02X ", x);
		}
		printf("\n");
		printf("\n");
	}

	return 0;
}
