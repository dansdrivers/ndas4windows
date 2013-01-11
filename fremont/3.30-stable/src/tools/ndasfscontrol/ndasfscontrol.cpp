// NdasNtfsControl.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>                
#include <stdlib.h>
#include <stdio.h>
#include <winioctl.h>
#include <string.h>
#include <crtdbg.h>
#include <winioctl.h>

#include <NdasFs.h>


int __cdecl _tmain(int argc, _TCHAR* argv[]) {

	HANDLE  deviceHandle;
    DWORD lpBytesReturned;

	deviceHandle = CreateFile( TEXT("\\\\.\\NdasFatControl"),
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

		returnValue = DeviceIoControl( deviceHandle, FSCTL_NDAS_FS_UNLOAD, NULL, 0, NULL, 0, &lpBytesReturned, NULL );

		printf("NDAS_FS_UNLOAD: Shutdown returnValue = %d", returnValue);
		CloseHandle(deviceHandle);		
	}

	return 0;
}