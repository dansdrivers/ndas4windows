/*++

  Copyright (c) 2003 XIMETA Technology, Inc.

  This code contains a part of a sample of DDK

  Module Name:

    drvinstall.c

  Abstract:

    Console app for the installation of Device Drivers in Windows 2000.

  Environment:

    user mode only


  Notes:


  Original Disclaimer:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1999-2000 Microsoft Corporation.  All Rights Reserved.

  Revision History:

  9/22/99: Created Keith S. Garner, with input from Eliyas and others.

--*/

//#include <stdio.h> 
//#include <tchar.h> // Make all functions UNICODE safe.
//#include <windows.h>  
//#include <strsafe.h>

#include "NDDevice.h"
#include <newdev.h> // for the API UpdateDriverForPlugAndPlayDevices().
#include <setupapi.h> // for SetupDiXxx functions.

#define MAX_CLASS_NAME_LEN 32 // Stolen from <cfgmgr32.h>

//++
//
// wmain, library tester
//
#ifdef LIBTEST
#define USAGE TEXT( \
	"usage: NDDevice command [command-args]\n \
	\n \
	 install inf hwid\n \
	 update  inf hwid\n \
	 remove  hwid\n \
	 infcopy inf\n \
	\n" )

int __cdecl wmain(int argc, LPTSTR *argv, LPTSTR *env)
{
	struct { LPTSTR cmd; int argc; BOOL op; } opts[] = 
	{ { L"install", 2, FALSE}, 
	{ L"update", 2, FALSE},
	{ L"remove", 1, FALSE}, 
	{ L"infcopy", 1, FALSE}};
	const int nopts = 4;
	
	int		ret = 0, i, op;
	BOOL	bArg = FALSE;

	_tprintf(TEXT("NetDisk Device Setup\n"));

	bArg = FALSE;
	if (argc > 2)
		for (i = 0; i < nopts; i++)
			if ((lstrcmpi(opts[i].cmd, argv[1]) == 0) && 
				(argc == opts[i].argc + 2))
			{
				bArg = TRUE;
				op = i;
			}

	if (!bArg)
	{
		_tprintf(USAGE);
		return -1;
	}

	LogStart();

	switch (op)
	{
	case 0:
		ret = NDInstallDevice(argv[2], argv[3]);
		break;
	case 1:
		ret = NDUpdateDriver(argv[2], argv[3]);
		break;
	case 2:
		ret = NDRemoveDevice(argv[2]);
		break;
	case 3:
		TCHAR szOemInf[MAX_PATH + 1];
		ret = NDCopyInf(argv[2], szOemInf);
		_tprintf(TEXT("Copied to %s"), szOemInf);
		break;
	}

	if (ret == NDS_REBOOT_REQUIRED)
		_tprintf(TEXT("Reboot required.\n"));
	else if (ret == NDS_FAIL)
		_tprintf(TEXT("Failed\n"));

	LogEnd();

	return ret;
}
#endif

/*++

Routine Description:
    CopyDriverInf
    CopyDriver files to the system

Arguments:

Return Value:

    EXIT_xxxx

--*/

/*++

Routine Description:

    InstallDevice
    install a device manually

Arguments:

  szInfPath    - relative or absolute path to INF file
  szHardwareId - hardware ID to install device

Return Value:

    EXIT_xxxx

--*/
int __stdcall NDInstallDevice(LPCTSTR szInfPath, LPCTSTR szHardwareId)
{
    HDEVINFO DeviceInfoSet = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA DeviceInfoData;
    GUID ClassGUID;
    TCHAR ClassName[MAX_CLASS_NAME_LEN];
    TCHAR hwIdList[LINE_LEN+4];
    TCHAR szInfFullPath[MAX_PATH];
    DWORD err;
    DWORD failcode = NDS_FAIL;
    BOOL reboot = FALSE;
    DWORD flags = 0;
    DWORD len;

	LogPrintf(TEXT("Entering NDInstallDevice()..."));
	LogPrintf(TEXT("InstallDevice, INF file:%s, HardwareID:%s\n"), szInfPath, szHardwareId);

    // Inf must be a full pathname
    if(GetFullPathName(szInfPath,MAX_PATH,szInfFullPath,NULL) >= MAX_PATH) {
        // inf pathname too long
		LogPrintf(TEXT("InstallDevice, Path Too Long:'%s'\n"), szInfFullPath);
		LogPrintf(TEXT("Leaving NDInstallDevice()..."));
        return NDS_FAIL;
    }

    //
    // List of hardware ID's must be double zero-terminated
    //
    ZeroMemory(hwIdList,sizeof(hwIdList));
    lstrcpyn(hwIdList,szHardwareId,LINE_LEN);

    //
    // Use the INF File to extract the Class GUID.
    //
    if (!SetupDiGetINFClass(szInfFullPath,&ClassGUID,ClassName,sizeof(ClassName)/sizeof(ClassName[0]),0))
    {
		LogPrintf(TEXT("InstallDevice, Failed to get Class GUID from INF file\n"));
        goto final;
    }

    //
    // Create the container for the to-be-created Device Information Element.
    //
    DeviceInfoSet = SetupDiCreateDeviceInfoList(&ClassGUID,0);
    if(DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
		LogPrintf(TEXT("InstallDevice, Failed SetupDiCreateDeviceInfoList()\n"));
        goto final;
    }

    //
    // Now create the element.
    // Use the Class GUID and Name from the INF file.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo(DeviceInfoSet,
        ClassName,
        &ClassGUID,
        NULL,
        0,
        DICD_GENERATE_ID,
        &DeviceInfoData))
    {
		LogPrintf(TEXT("InstallDevice, failed SetupDiCreateDeviceInfo()\n"));
        goto final;
    }

    //
    // Add the HardwareID to the Device's HardwareID property.
    //
    if(!SetupDiSetDeviceRegistryProperty(DeviceInfoSet,
        &DeviceInfoData,
        SPDRP_HARDWAREID,
        (LPBYTE)hwIdList,
        (lstrlen(hwIdList)+1+1)*sizeof(TCHAR)))
    {
		LogPrintf(TEXT("InstallDevice, failed SetupDiSetDeviceRegistryProperty()\n"));
        goto final;
    }

    //
    // Transform the registry element into an actual devnode
    // in the PnP HW tree.
    //
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE,
        DeviceInfoSet,
        &DeviceInfoData))
    {
		LogPrintf(TEXT("InstallDevice, failed SetupDiCallClassInstaller()\n"));
        goto final;
    }

	//
    // update the driver for the device we just created
    //
    failcode = NDUpdateDriver(szInfPath, szHardwareId);

final:

    if (DeviceInfoSet != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    }

	LogPrintf(TEXT("Leaving NDInstallDevice()..."));
    return failcode;
}

/*++

Routine Description:
    UPDATE
    update driver for existing device(s)

Arguments:

Return Value:

    EXIT_xxxx

--*/
int __stdcall NDUpdateDriver(IN LPCTSTR szInfPath, IN LPCTSTR szHardwareId)
{
    DWORD failcode = NDS_FAIL;
    BOOL reboot = FALSE;
    DWORD flags = 0;
    DWORD res;
    TCHAR szInfFullPath[MAX_PATH];

    flags |= INSTALLFLAG_FORCE;

	LogPrintf(TEXT("Entering NDUpdateDriver()..."));
	LogPrintf(TEXT("INF file:%s, HardwareID:%s Flags:%x"), szInfPath, szHardwareId, flags);

	if (szInfPath)
	{
		// Inf must be a full pathname
		res = GetFullPathName(szInfPath,MAX_PATH,szInfFullPath,NULL);
		if((res >= MAX_PATH) || (res == 0)) {
			// inf pathname too long
			LogPrintf(TEXT("Leaving NDUpdateDriver()... %d"), NDS_FAIL);
			return NDS_FAIL;
		}
		if(GetFileAttributes(szInfFullPath)==(DWORD)(-1)) {
			// inf doesn't exist
			LogPrintf(TEXT("INF file doesn't exist"));
			LogPrintf(TEXT("Leaving NDUpdateDriver()... %d"), NDS_FAIL);
			return NDS_FAIL;
		}
	}
	else
	{
		LogPrintf(TEXT("INF file is not specified"));
		return NDS_FAIL;
	}

	if (szInfFullPath)
		LogPrintf(TEXT("Updating drivers %s from %s..."), szHardwareId, szInfFullPath);
	else
		LogPrintf(TEXT("Updating drivers for %s..."), szHardwareId);

    if(!UpdateDriverForPlugAndPlayDevices(GetDesktopWindow(),szHardwareId,szInfFullPath,flags,&reboot)) {
		if (GetLastError() == ERROR_NO_SUCH_DEVINST)
		{
			LogPrintf(TEXT("No device instance found"));
			return NDS_SUCCESS;
		}
		LogPrintfErr(TEXT("Error from UpdateDriverForPlugAndPlayDevices()"));
		LogPrintf(TEXT("=> INF file:%s, HardwareID:%s Flags:%x Reboot:%d"), szInfFullPath, szHardwareId, flags, reboot);
        goto final;
    }

	LogPrintf(TEXT("Update completed."));

    failcode = reboot ? NDS_REBOOT_REQUIRED : NDS_SUCCESS;

final:

	LogPrintf(TEXT("Leaving NDUpdateDriver()... %d"), failcode);
    return failcode;
}


/*++
Routine Discription:

Arguments:
    
    szHardwareId - PnP HardwareID of devices to remove.

Return Value:
    
    Standard Console ERRORLEVEL values:

    0 - Remove Successfull
    2 - Remove Failure.
    
--*/
int __stdcall NDRemoveDevice(IN LPCTSTR szHardwareId)
{
    HDEVINFO DeviceInfoSet;
    SP_DEVINFO_DATA DeviceInfoData;
    DWORD i,err;
	int iRet;

	LogPrintf(TEXT("Entering NDRemoveDevice(%s)..."), szHardwareId);

    //
    // Create a Device Information Set with all present devices.
    //
    DeviceInfoSet = SetupDiGetClassDevs(NULL, // All Classes
        0,
        0, 
        DIGCF_ALLCLASSES | DIGCF_PRESENT ); // All devices present on system
    if (DeviceInfoSet == INVALID_HANDLE_VALUE)
    {
        LogPrintfErr(TEXT("GetClassDevs(All Present Devices)"));
		LogPrintf(TEXT("Leaving NDRemoveDevice()... %d"), NDS_FAIL);
        return NDS_FAIL;
    }
    
    //
    //  Enumerate through all Devices.
    //
    DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    for (i=0;SetupDiEnumDeviceInfo(DeviceInfoSet,i,&DeviceInfoData);i++)
    {
        DWORD DataT;
        LPTSTR p,buffer = NULL;
        DWORD buffersize = 0;
        
        //
        // We won't know the size of the HardwareID buffer until we call
        // this function. So call it with a null to begin with, and then 
        // use the required buffer size to Alloc the nessicary space.
        // Keep calling we have success or an unknown failure.
        //
        while (!SetupDiGetDeviceRegistryProperty(
            DeviceInfoSet,
            &DeviceInfoData,
            SPDRP_HARDWAREID,
            &DataT,
            (PBYTE)buffer,
            buffersize,
            &buffersize))
        {
            if (GetLastError() == ERROR_INVALID_DATA)
            {
                //
                // May be a Legacy Device with no HardwareID. Continue.
                //
                break;
            }
            else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                //
                // We need to change the buffer size.
                //
                if (buffer) 
                    LocalFree(buffer);
                buffer = (LPTSTR) LocalAlloc(LPTR,buffersize);
            }
            else
            {
                //
                // Unknown Failure.
                //
                LogPrintfErr(TEXT("GetDeviceRegistryProperty"));
                goto cleanup_DeviceInfo;
            }            
        }

        if (GetLastError() == ERROR_INVALID_DATA) 
            continue;
        
        //
        // Compare each entry in the buffer multi-sz list with our HardwareID.
        //
        for (p=buffer;*p&&(p<&buffer[buffersize]);p+=lstrlen(p)+sizeof(TCHAR))
        {
            // LogPrintf(TEXT("Compare device ID: [%s]"),p);

            if (!_tcscmp(szHardwareId,p))
            {
                LogPrintf(TEXT("Found! [%s]"),p);

                //
                // Worker function to remove device.
                //
                if (!SetupDiCallClassInstaller(DIF_REMOVE, DeviceInfoSet, &DeviceInfoData))
					LogPrintfErr(TEXT("CallClassInstaller(REMOVE)"));
				/*
				if (!SetupDiDeleteDevRegKey(DeviceInfoSet, &DeviceInfoData, 
					DICS_FLAG_GLOBAL, 0xFFFFFFFF, DIREG_BOTH))
					LogPrintfErr(TEXT("DiDeleteDevRegKey"));
				*/
				
                break;
            }
        }

        if (buffer) LocalFree(buffer);
    }

    if ((GetLastError()!=NO_ERROR)&&(GetLastError()!=ERROR_NO_MORE_ITEMS))
        LogPrintfErr(TEXT("EnumDeviceInfo"));
    
    //
    //  Cleanup.
    //    
cleanup_DeviceInfo:
    err = GetLastError();
    SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    
	iRet = (err == NO_ERROR) ? NDS_SUCCESS : NDS_FAIL;
	LogPrintf(TEXT("Leaving NDRemoveDevice()... %d"), iRet);
    return iRet; 
}

int __stdcall NDCopyInf(IN TCHAR *szInfFullPath, OUT LPTSTR szOEMInfPath)
{
	BOOL	bRet = FALSE;
	size_t	cchPathLen;

	LogPrintf(TEXT("Entering NDCopyInf(%s, %s)..."), szInfFullPath, szOEMInfPath);

	StringCchLength(szInfFullPath, MAX_PATH, &cchPathLen);
	if (szInfFullPath && cchPathLen)
    {
        bRet = SetupCopyOEMInf(
                    szInfFullPath,
                    NULL,               // other files are in the
                                        // same dir. as primary INF
                    SPOST_PATH,         // first param. contains path to INF
                    0,                  // default copy style
                    szOEMInfPath,		// receives the name of the INF
                                        // after it is copied to %windir%\inf
                    MAX_PATH,           // max buf. size for the above
                    NULL,               // receives required size if non-null
                    NULL);				// optionally retrieves filename
	                                    // component of szInfNameAfterCopy
		if (!bRet)
			LogLastError();
	}
	LogPrintf(TEXT("Leaving NDCopyInf()... %d"), ((bRet) ? NDS_SUCCESS : NDS_FAIL));
	return ((bRet) ? NDS_SUCCESS : NDS_FAIL);
}