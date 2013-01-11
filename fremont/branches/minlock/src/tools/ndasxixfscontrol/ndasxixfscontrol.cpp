// ndasxixfscontrol.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>                
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>
#include <crtdbg.h>
#include <winioctl.h>

#define NDAS_XIXFS_UNLOAD				CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 18, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

int __cdecl _tmain(int argc, _TCHAR* argv[]) {

	HANDLE  deviceHandle;
    DWORD lpBytesReturned;

	deviceHandle = CreateFile( TEXT("\\\\.\\XixfsControl"),
							   GENERIC_READ | GENERIC_WRITE,
							   0,
							   NULL,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   NULL );

	if(deviceHandle == INVALID_HANDLE_VALUE) {
	
		printf("CreateFile fail\n");
		getchar();

	} else {

		BOOL	returnValue;

		returnValue = DeviceIoControl( deviceHandle, NDAS_XIXFS_UNLOAD, NULL, 0, NULL, 0, &lpBytesReturned, NULL );

		printf("NDAS_FS_UNLOAD: Shutdown returnValue = %d", returnValue);
		CloseHandle(deviceHandle);		
	}

	return 0;
}