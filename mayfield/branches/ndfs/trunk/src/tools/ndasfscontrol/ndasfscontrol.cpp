// NdNtfsControl.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>                
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>
#include <crtdbg.h>
#include <winioctl.h>

#define ND_FAT_SHUTDOWN         CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 17, METHOD_BUFFERED, FILE_READ_ACCESS)

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	HANDLE  deviceHandle;

	deviceHandle = CreateFile( TEXT("\\\\.\\NdFatControl"),
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

		returnValue = DeviceIoControl( deviceHandle, ND_FAT_SHUTDOWN, NULL, 0, NULL, 0, NULL, NULL );

		printf("ND_FAT_SHUTDOWN: Shutdown returnValue = %d", returnValue);
		CloseHandle(deviceHandle);		
	}

	return 0;
}