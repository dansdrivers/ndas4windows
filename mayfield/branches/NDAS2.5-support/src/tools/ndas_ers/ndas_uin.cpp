// ndas_uin.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "ndas_uin.h"
#include "DebugPrint.h"
#include <winsvc.h>
#include <Psapi.h>
#include <shlwapi.h>
#include <setupapi.h>
#include "snetcfg.h"
#include "NDDevice.h"
#include "NDFilter.h"
#include "NDNetComp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// The one and only application object

CWinApp theApp;
TCHAR
	g_szProgramPath[MAX_PATH], 
	g_szSystemPath[MAX_PATH], 
	g_szWindowsPath[MAX_PATH],
	g_szStartupPath[MAX_PATH],
	g_szDesktopPath[MAX_PATH],
	g_szProgramsPath[MAX_PATH];

struct OEM_INFO{
	LPTSTR szFilePath;
	LPTSTR szShortcutPath;
	LPTSTR szAdminShortcutName;
	LPTSTR szSCSI_DiskName;
};

OEM_INFO g_aOEM[] = {
	{"XIMETA\\NetDisk", "NetDisk", "NetDisk Administrator.lnk", "Disk&Ven_NETDISK"},
	{"Gennetworks\\GenDisk", "GenDisk", "GenDisk Administrator.lnk", "Disk&Ven_GENDISK"},
	{"Iomega\\Network Hard Drive", "Iomega Network Hard Drive", NULL, "Disk&Ven_NETHD"},
	{"Logitec\\LHD-LU2", "LHD-LU2 Tools", "LHD-LU2 Administrator.lnk", "Disk&Ven_LHD-LU2"},
	{"XiMeta\\Eoseed", "Eoseed Tools", "Eoseed Administrator.lnk", "Disk&Ven_EOSEED"},
};

int g_nOEM = sizeof(g_aOEM) / sizeof(g_aOEM[0]);

using namespace std;

BOOL g_bOptionExitOnError = TRUE;
BOOL g_bVerbous = FALSE;

#define eprintf(TEXTS) \
	printf("ERROR : "); \
	printf TEXTS ; \
	if(FALSE != g_bOptionExitOnError) exit(1);

#define vprintf(TEXTS) \
	if(g_bVerbous) printf TEXTS ;

BOOL KillApplication(LPCTSTR lpszAppName)
{
    DWORD aProcesses[1024], cbNeeded, cProcesses;
	char szMessage[1024];
    unsigned int i;
	int iResult;
//    unsigned int j;
//	TCHAR szProgramName[MAX_PATH], szProgramNameShort[MAX_PATH], szProcessNameShort[MAX_PATH];

	vprintf(("\tSearching for running application %s\n", lpszAppName));

	// AING_TO_DO : change to FindWindowEx & Kill Process

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
	{
		eprintf(("EnumProcesses Failed\n"));
        exit(1);
	}
	
	cProcesses = cbNeeded / sizeof(DWORD);
	
    for ( i = 0; i < cProcesses; i++ )
	{
		char szProcessName[MAX_PATH] = "";
		
		// Get a handle to the process.
		
		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
									   PROCESS_VM_READ,
									   FALSE, aProcesses[i] );
		
		// Get the process name.
		
		if (NULL != hProcess )
		{
			HMODULE hMod;
			DWORD cbNeeded;
			
			if ( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &cbNeeded) )
			{
				GetModuleFileNameEx( hProcess, hMod, szProcessName, sizeof(szProcessName) );
				if(NULL != strrchr(szProcessName, '\\') &&
					0 == stricmp(lpszAppName, strrchr(szProcessName, '\\') +1))
				{
					// check if it is correct process
						sprintf(szMessage, "NDAS application %s is running now. It need to be closed.", szProcessName);
						iResult = MessageBox(NULL, szMessage,
							_T("Application detected"), MB_RETRYCANCEL);
						if(IDCANCEL == iResult)
						{
							CloseHandle(hProcess);
							exit(1); // force exit
						}
						else
						{
							return FALSE;
						}
				}
/*
				for(j = 0; j < g_nOEM; j++)
				{
					sprintf(szProgramName, "%s\\%s\\%s", g_szProgramPath, g_aszOEM[j], lpszAppName);
					GetShortPathName(szProgramName, szProgramNameShort, MAX_PATH);
					GetShortPathName(szProcessName, szProcessNameShort, MAX_PATH);
					printf(_T("Module %s : %s\n"), szProcessName, szProgramName);
					if(0 == stricmp(szProgramNameShort, szProcessNameShort))
					{
						eprintf(("NDAS application %s is running now. Close it first\n", lpszAppName));
						CloseHandle(hProcess);
						exit(1); // force exit
						//						printf("Killing %s\n", lpszAppName);
						//						TerminateProcess(hProcess, 0);
						return TRUE;
					}
				}
*/
			}
		}
		CloseHandle( hProcess );
	}

	return TRUE;
}

BOOL SetAllAccessRight(HKEY hParentKey, LPCTSTR lpszRegistry)
{
	HKEY hKey = NULL;
	LONG lResult;
	BOOL bResult;
	BOOL bReturn = FALSE;
	int i;

	PSECURITY_DESCRIPTOR psdRegistry = NULL;
	SECURITY_INFORMATION siRegistry = DACL_SECURITY_INFORMATION;
	PACL aclKey = NULL;
	DWORD cbSecurityDescriptor = 0;

	HANDLE hCurrentProcess = NULL;
	HANDLE hProcessToken = NULL;
	PTOKEN_USER pTokenUser = NULL;
	DWORD dwTokenLength;
	DWORD cbAcl;

	SECURITY_DESCRIPTOR absSD;

	// open to modify DAC
	lResult = RegOpenKeyEx(
		hParentKey,
		lpszRegistry,
		0,
		READ_CONTROL | WRITE_DAC, // need to read & modify DAC
		&hKey);

	if(ERROR_SUCCESS != lResult) // don't care if not exist
	{
		if(ERROR_FILE_NOT_FOUND == lResult) // don't care
		{
			bReturn = TRUE;
			goto out;
		}

		eprintf(("RegOpenKeyEx failed : %d\n", lResult));
		goto out;
	}

	// get process token
	hCurrentProcess = GetCurrentProcess();
	lResult = DuplicateHandle(
		GetCurrentProcess(),
		GetCurrentProcess(),
		GetCurrentProcess(),
		&hCurrentProcess,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS);

	if(0 == lResult)
	{
		eprintf(("DuplicateHandle failed : %d\n", GetLastError()));
		goto out;
	}
	
	bResult = OpenProcessToken(
		hCurrentProcess,
		TOKEN_ALL_ACCESS,
		&hProcessToken);

	if(!lResult)
	{
		eprintf(("OpenProcessToken failed : %d\n", GetLastError()));
		goto out;
	}
	

	// get user token information of current process
	bResult = GetTokenInformation(
		hProcessToken,
		TokenUser,
		NULL, // retrieve size
		0,
		&dwTokenLength);

	if(!bResult && ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		eprintf(("GetTokenInformation failed for sizing : %d\n", GetLastError()));
		goto out;
	}

	pTokenUser = (PTOKEN_USER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwTokenLength);

	bResult = GetTokenInformation(
		hProcessToken,
		TokenUser,
		pTokenUser,
		dwTokenLength,
		&dwTokenLength);

	if(!bResult)
	{
		eprintf(("GetTokenInformation failed : %d\n", GetLastError()));
		goto out;
	}

	
	// calcurate size of aclKey
#define NUM_OF_ACES 1 // my(user) key only
	cbAcl = sizeof(ACL) + 
    ((sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) * NUM_OF_ACES);

	for (i = 0; i < NUM_OF_ACES; i++)
	{
		cbAcl += GetLengthSid(pTokenUser->User.Sid);
	}

	aclKey = (PACL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbAcl );

	ZeroMemory(aclKey, cbAcl);
	
	bResult = InitializeAcl(
		aclKey,
		cbAcl ,
		ACL_REVISION);
	
	if(!bResult)
	{
		eprintf(("InitializeAcl failed : %d\n", GetLastError()));
		goto out;
	}

	bResult = AddAccessAllowedAce(
		aclKey,
		ACL_REVISION,
		GENERIC_ALL,
		pTokenUser->User.Sid);
	
	if(!bResult)
	{
		eprintf(("AddAccessAllowedAce failed : %d\n", GetLastError()));
		goto out;
	}

	// initialize absolute SD
	bResult = InitializeSecurityDescriptor(&absSD, SECURITY_DESCRIPTOR_REVISION);

	if(!bResult)
	{
		eprintf(("InitializeSecurityDescriptor failed : %d\n", GetLastError()));
		goto out;
	}

	// set absSD with aclKey
	bResult = SetSecurityDescriptorDacl(
		&absSD,
		TRUE,
		aclKey,
		FALSE);

	if(!bResult)
	{
		eprintf(("SetSecurityDescriptorDacl failed : %d\n", GetLastError()));
		goto out;
	}

	// validate absSD
	bResult = IsValidSecurityDescriptor(&absSD);

	if(!bResult)
	{
		eprintf(("IsValidSecurityDescriptor failed : %d\n", GetLastError()));
		goto out;
	}

	// finally, set absSD to hKey
	lResult = RegSetKeySecurity(
		hKey,
		DACL_SECURITY_INFORMATION,
		&absSD
		);

	if(ERROR_SUCCESS != lResult)
	{
		eprintf(("RegSetKeySecurity failed : %d\n", lResult));
		goto out;
	}

	bReturn = TRUE;
out:
	if(hKey)
		RegCloseKey(hKey);

	if(hProcessToken)
		CloseHandle(hProcessToken);	

	if(aclKey)
		HeapFree(GetProcessHeap(), 0, aclKey);

	if(pTokenUser)
		HeapFree(GetProcessHeap(), 0, pTokenUser);

	return bReturn;
}

#define STRING_HKEY_ROOT(HKEY_ROOT) \
	(\
		(HKEY_CLASSES_ROOT == HKEY_ROOT) ? "HKEY_CLASSES_ROOT" :\
		(HKEY_CURRENT_USER == HKEY_ROOT) ? "HKEY_CURRENT_USER" :\
		(HKEY_LOCAL_MACHINE == HKEY_ROOT) ? "HKEY_LOCAL_MACHINE" :\
		(HKEY_USERS == HKEY_ROOT) ? "HKEY_USERS" :\
		(HKEY_CURRENT_CONFIG == HKEY_ROOT) ? "HKEY_CURRENT_CONFIG" : ""\
	)

BOOL RemoveRegistry(HKEY hParentKey, LPCTSTR lpszRegistry, BOOL bSetAccessRight = FALSE)
{
	HKEY hKey = NULL;
	DWORD dwIndex;
	LONG lResult;
	BOOL bReturn = FALSE;
	TCHAR szSubKeyPath[255], szSubKeyName[255];
	unsigned long sizeKey;
	FILETIME ftLastWriteTime;

	vprintf(("\tSearching registry %s\\%s\n", STRING_HKEY_ROOT(hParentKey), lpszRegistry));
	if(bSetAccessRight)
	{
//		printf("\tACLs changed to admin ownership and delete control for key %s\\%s\n", STRING_HKEY_ROOT(hParentKey), lpszRegistry);
		if(!SetAllAccessRight(hParentKey, lpszRegistry))
			return FALSE;
	}

	lResult = RegOpenKeyEx(
		hParentKey,
		lpszRegistry,
		0,
		KEY_ENUMERATE_SUB_KEYS,
		&hKey);

	if(ERROR_SUCCESS != lResult)
	{
		if(ERROR_FILE_NOT_FOUND == lResult) // don't care
			goto out;

		eprintf(("RegOpenKeyEx failed : %d\n", lResult));
		goto out;
	}

	// remove all subkeys
	dwIndex = 0;
	while(1)
	{
		sizeKey = 255;
		lResult = RegEnumKeyEx(
			hKey,
			dwIndex,
			szSubKeyName,
			&sizeKey,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);

		if(ERROR_NO_MORE_ITEMS == lResult)
			break;
		if(ERROR_SUCCESS != lResult)
		{
			eprintf(("RegEnumKey failed : %d\n", lResult));
			goto out;
		}

		sprintf(szSubKeyPath, "%s\\%s", lpszRegistry, szSubKeyName);

		RemoveRegistry(hParentKey, szSubKeyPath, bSetAccessRight);
//		dwIndex++; // do not increase becuase dwIndex removed
	}	

	lResult = RegDeleteKey(
		hParentKey, 
		lpszRegistry);

	if(ERROR_SUCCESS == lResult)
		printf("\t\tRemoved %s\\%s\n", STRING_HKEY_ROOT(hParentKey), lpszRegistry);
	else
	{
		eprintf(("Failed to delete Registry %s\\%s\n", STRING_HKEY_ROOT(hParentKey), lpszRegistry));
		return FALSE;
	}
	
	bReturn = TRUE;
out:
	if(hKey)
		RegCloseKey(hKey);

	return bReturn;
}

BOOL RemoveRegistryMatch(HKEY hParentKey, LPCTSTR lpszRegistry, LPCTSTR szName, LPCTSTR szData, BOOL bSetAccessRight = FALSE)
{
	TCHAR szSubKeyPath[255], szSubKeyName[255];
	char szKeyData[256];
	unsigned long sizeKeyData = 256;
	DWORD dwIndex;
	HKEY hKey = NULL, hSubKey = NULL;
	LONG lResult;
	BOOL bReturn = FALSE;
	unsigned long sizeKey;
	FILETIME ftLastWriteTime;

	vprintf(("\tSearching registry %s\\%s with %s is %s\n", STRING_HKEY_ROOT(hParentKey), lpszRegistry, szName, szData));

	lResult = RegOpenKeyEx(
		hParentKey,
		lpszRegistry,
		0,
		KEY_ENUMERATE_SUB_KEYS,
		&hKey);

	if(ERROR_SUCCESS != lResult)
	{
		if(ERROR_FILE_NOT_FOUND == lResult) // don't care
			goto out;

		eprintf(("RegOpenKeyEx failed : %d\n", lResult));
		goto out;
	}

	dwIndex = 0;
	while(1)
	{
		sizeKey = 255;
		lResult = RegEnumKeyEx(
			hKey,
			dwIndex,
			szSubKeyName,
			&sizeKey,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);

		if(ERROR_NO_MORE_ITEMS == lResult)
			break;
		if(ERROR_SUCCESS != lResult)
		{
			eprintf(("RegEnumKey failed : %d\n", lResult));
			goto out;
		}

		sprintf(szSubKeyPath, "%s\\%s", lpszRegistry, szSubKeyName);
		
		lResult = RegOpenKeyEx(
			hParentKey,
			szSubKeyPath,
			0,
			KEY_QUERY_VALUE,
			&hSubKey);

		if(ERROR_SUCCESS != lResult)
		{
			dwIndex++;
			if(ERROR_ACCESS_DENIED == lResult) // special case
				continue;

			eprintf(("RegOpenKeyEx failed : %d %s\n", lResult, szSubKeyPath));
			continue;
		}
		
		lResult = RegQueryValueEx(
			hSubKey, 
			szName,
			NULL,
			NULL,
			(unsigned char *)szKeyData,
			&sizeKeyData);

		if(ERROR_SUCCESS == lResult && NULL != strstr(szKeyData, szData))
		{
			RemoveRegistry(hParentKey, szSubKeyPath, bSetAccessRight);
		}
		else
			dwIndex++;
		
		RegCloseKey(hSubKey);
	}	

	bReturn = TRUE;
out:

	if(hKey)
		RegCloseKey(hKey);
	return bReturn;
}

BOOL DeleteRecursive(LPCTSTR lpszPath)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	TCHAR szFindPath[MAX_PATH];
	TCHAR szLocalPath[MAX_PATH];
	DWORD dwFileAttributes;

	sprintf(szFindPath, "%s\\*", lpszPath);
	
	hFind = FindFirstFile(szFindPath, &FindFileData);

	if(INVALID_HANDLE_VALUE == hFind)
		return FALSE;

	while(1)
	{
		if(0 == strcmp(FindFileData.cFileName, ".") || 0 == strcmp(FindFileData.cFileName, ".."))
			goto next;
		sprintf(szLocalPath, "%s\\%s", lpszPath, FindFileData.cFileName);
		dwFileAttributes = GetFileAttributes(szLocalPath);
		if(FILE_ATTRIBUTE_DIRECTORY & dwFileAttributes)
		{
			DeleteRecursive(szLocalPath);
//			continue;
			RemoveDirectory(szLocalPath);
			goto next;
		}
		else if(0 == SetFileAttributes(szLocalPath, FILE_ATTRIBUTE_NORMAL))
		{
			printf("SetFileAttributes failed for %s\n", szLocalPath);
			exit(10);
		}
		
		if(0 == DeleteFile(szLocalPath))
		{
			printf("DeleteFile failed for %s\n", szLocalPath);
			exit(10);
		}
		
		printf("\t\tRemoved %s\n", szLocalPath);

next:
		if(FALSE == FindNextFile(hFind, &FindFileData))
			if(ERROR_NO_MORE_FILES == GetLastError())
				goto out;
			else
			{
				printf("error while searching Files\n");
				exit(10);
			}
	}
out:

	FindClose(hFind);

	return TRUE;
}

BOOL DeleteNetDiskFiles()
{
	int i;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	TCHAR szPath[MAX_PATH];

	vprintf(("Searching NDAS applications\n"));

	for(i = 0; i < g_nOEM; i++)
	{
		sprintf(szPath, "%s\\%s", g_szProgramPath, g_aOEM[i].szFilePath);
		hFind = FindFirstFile(szPath, &FindFileData);
		if(INVALID_HANDLE_VALUE == hFind)
			continue;
		
		FindClose(hFind);

		DeleteRecursive(szPath);
		printf("\tRemoved %s\b", szPath);
		RemoveDirectory(szPath);
	}
	
	return TRUE;
}

BOOL DeleteNetDiskShortcuts()
{
	int i;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	BOOL bResult;
	TCHAR szPath[MAX_PATH];

	vprintf(("Searching NDAS shortcuts\n"));

	for(i = 0; i < g_nOEM; i++)
	{
		sprintf(szPath, "%s\\%s", g_szProgramsPath, g_aOEM[i].szShortcutPath);
		hFind = FindFirstFile(szPath, &FindFileData);
		if(INVALID_HANDLE_VALUE == hFind)
			continue;
		
		FindClose(hFind);

		DeleteRecursive(szPath);
		bResult = RemoveDirectory(szPath);
		if(bResult)
			printf("\tRemoved %s\b", szPath);

		if(NULL != g_aOEM[i].szAdminShortcutName)
		{
			sprintf(szPath, "%s\\%s", g_szDesktopPath, g_aOEM[i].szAdminShortcutName);
			if(S_OK == DeleteFile(szPath))
				printf("\tRemoved %s\b", szPath);

			sprintf(szPath, "%s\\%s", g_szStartupPath, g_aOEM[i].szAdminShortcutName);
			if(S_OK == DeleteFile(szPath))
				printf("\tRemoved %s\b", szPath);
		}
	}
	
	return TRUE;
}

BOOL StopService(LPCTSTR lpszServiceName, BOOL bDeleteService = TRUE)
{
	SC_HANDLE hSCManager;
	SC_HANDLE hService;
	SERVICE_STATUS service_status;
	BOOL bReturn = FALSE;
	BOOL bResult;
	int i;

	// open service manager

	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if(!hSCManager)
	{
		eprintf(("Error in Service Manager\n"));
		goto out;
	}

	vprintf(("\tSearching for the service %s\n", lpszServiceName));

	// open service

	hService = OpenService(hSCManager, lpszServiceName, SC_MANAGER_ALL_ACCESS);
	if(!hService) // not found
	{
		goto out;
	}

	// stop service

	bResult = QueryServiceStatus(hService, &service_status);
	if(0 == bResult)
	{
		eprintf(("Error while query service status %s\n", lpszServiceName));
		goto out;
	}

	if(SERVICE_STOPPED == service_status.dwCurrentState) // already stop
	{
//		goto out;
	}
	else // need to stop
	{
		bResult = ControlService(hService, SERVICE_CONTROL_STOP, &service_status);
		if(0 == bResult)
		{
			eprintf(("Error while stopping service %s\n", lpszServiceName));
			goto out;
		}
		i = 0;
		while(1)
		{
			Sleep(1000); // wait 1 sec for each turn
			bResult = QueryServiceStatus(hService, &service_status);
			
			if(0 == bResult)
			{
				eprintf(("Error while stopping service %s\n", lpszServiceName));
				MessageBox(NULL, _T("You may need to run this program again after reboot."), _T("Warning"), MB_OK | MB_ICONEXCLAMATION);
				goto out;
			}

			if(SERVICE_STOPPED == service_status.dwCurrentState)
			{
				break;
			}
			
			if(10 == i++) // 10 times
			{
				eprintf(("Failed to stop service %s\n", lpszServiceName));
				MessageBox(NULL, _T("You may need to run this program again after reboot."), _T("Warning"), MB_OK | MB_ICONEXCLAMATION);
				goto out;
			}
		}	
	}

	// delete service
	if(bDeleteService)
	{
		bResult = DeleteService(hService);
		if(FALSE == bResult) // not error
		{
			vprintf(("\t\tNot removed %s\n", lpszServiceName));
		}
		else
			printf("\t\tRemoved %s\n", lpszServiceName);
	}
	
	bReturn = TRUE;

out:
	if(hService)
		bResult = CloseServiceHandle(hService);
	if(hSCManager)
		bResult = CloseServiceHandle(hSCManager);

	return bReturn;
}

BOOL DeleteSystemFile(LPCTSTR lpszDriverFileName)
{
	TCHAR szPath[MAX_PATH];

	DebugPrint(1, (_T("DeleteSystemFile %s\n"), lpszDriverFileName));

	sprintf(szPath, "%s\\%s", g_szSystemPath, lpszDriverFileName);
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	hFind = FindFirstFile(szPath, &FindFileData);
	if(INVALID_HANDLE_VALUE == hFind)
		return FALSE;

	FindClose(hFind);

	DeleteFile(szPath);

	printf("\t\tRemoved %s\n", szPath);

	return TRUE;
}

BOOL DeleteOEMFile(LPCTSTR lpszAppName, LPCTSTR lpszKeyName, LPCTSTR lpszValue)
{
	TCHAR szPath[MAX_PATH], szOEMFile[MAX_PATH], szOEMFilePNF[MAX_PATH];
	TCHAR szValue[MAX_PATH];
	HANDLE hFind;
	WIN32_FIND_DATA FindFileData;

	DebugPrint(1, (_T("DeleteOEMFile [%s] %s=%s\n"), lpszAppName, lpszKeyName, lpszValue));

	sprintf(szPath, "%s\\inf\\oem*.inf", g_szWindowsPath);

	hFind = FindFirstFile(szPath, &FindFileData);

	if (INVALID_HANDLE_VALUE == hFind) 
		return FALSE;

	while(1)
	{
		sprintf(szOEMFile, "%s\\inf\\%s", g_szWindowsPath, FindFileData.cFileName);
		GetPrivateProfileString(lpszAppName, lpszKeyName, _T(""), szValue, MAX_PATH, szOEMFile);
		if(0 == stricmp(lpszValue, szValue))
		{
			DeleteFile(szOEMFile);
			strcpy(szOEMFilePNF, szOEMFile);
			szOEMFilePNF[strlen(szOEMFilePNF) -3] = 'P';
			DeleteFile(szOEMFilePNF);
			printf("\t\tRemoved %s(%s)\n", FindFileData.cFileName, lpszValue);
		}

		if(FALSE == FindNextFile(hFind, &FindFileData))
			if(ERROR_NO_MORE_FILES == GetLastError())
				break;
			else
			{
				printf("error while searching OEM Files\n");
				break;
			}
	}

	FindClose(hFind);

	return TRUE;
}

BOOL DeleteInstallShield(LPCTSTR lpszGUID)
{
	WIN32_FIND_DATA FindFileData;
	TCHAR szPath[MAX_PATH];
	HANDLE hFind;

	sprintf(szPath, "%s\\Installer\\%s", g_szWindowsPath, lpszGUID);

	vprintf(("\tSearching for folder %s\n", szPath));

	hFind = FindFirstFile(szPath, &FindFileData);

	if (INVALID_HANDLE_VALUE == hFind) 
		return FALSE;

	FindClose(hFind);

	DeleteRecursive(szPath);
	RemoveDirectory(szPath);

	printf("\t\tRemoved %s\n", szPath);

	return TRUE;

}

// {4d36e975-e325-11ce-bfc1-08002be10318}
const GUID CLSID_netLpx = { 0x4d36e975, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
//#define LPX_NETCOMPID ("NKC_LPX")

BOOL NDUninstallNetComp (PCSTR szNetComp)
{
    HRESULT hr ;

	vprintf(("Uninstalling %s\n", szNetComp));

	WCHAR szNetCompW[100];
	MultiByteToWideChar(
		CP_ACP,
		NULL,
		szNetComp,
		strlen(szNetComp),
		szNetCompW,
		100);

	hr = HrUninstallNetComponent(szNetCompW) ;

	if(FAILED(hr))
	{
		printf("Failed to uninstall %s\n", szNetComp);
		exit(1);
	}

	printf("Uninstalled %s\n", szNetComp);

	return TRUE;
}

#define LANSCSIBUS_SERVICE _T("lanscsibus")
#define LANSCSIMINIPORT_SERVICE _T("lanscsiminiport")

#define SAFE_CLOSESERVICEHANDLE(HANDLE_SERVICE) if(NULL != (HANDLE_SERVICE)) {CloseServiceHandle(HANDLE_SERVICE); (HANDLE_SERVICE) = NULL;}

int DeleteDeviceService(LPCTSTR szServiceName)
{
	BOOL bRet;
	SC_HANDLE hSCM = NULL, hService = NULL;
	int iReturn = NDS_FAIL;

	vprintf(("\tSearching for device service %s\n", szServiceName));

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (NULL == hSCM)
	{
		eprintf(("Error opening Service Control Manager\n"));
		goto out;
	}

	hService = OpenService(hSCM, szServiceName, DELETE);
	
	if (NULL == hService) // not found, skip
	{
		goto out;
	}

	bRet = DeleteService(hService);
	if (NULL == bRet) // failed to delete
	{
		eprintf((("Error deleting service %s\n"), szServiceName));
		goto out;
	}
	printf("\t\tRemoved %s\n", szServiceName);

	iReturn = NDS_SUCCESS;
out:
	SAFE_CLOSESERVICEHANDLE(hService);
	SAFE_CLOSESERVICEHANDLE(hSCM);

	return iReturn;
}

BOOL DeleteInstallerInformations()
{
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{97B2DE6D-0255-4A16-B3EF-28BD898BB7F4}");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Products\\D6ED2B79552061A43BFE82DB98B87B4F");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UpgradeCodes\\D6ED2B79552061A43BFE82DB98B87B4F");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\Installer\\UpgradeCodes\\D6ED2B79552061A43BFE82DB98B87B4F");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\Installer\\Products\\D6ED2B79552061A43BFE82DB98B87B4F");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Classes\\Installer\\Features\\D6ED2B79552061A43BFE82DB98B87B4F");

	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\3550ED84DB9FC4E4CBEB15F9F7902AC4");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\95AEB707BA80318479428B3C9D4D02F6");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\9A470394EA7F5D3418D2E44BEEA088CA");
	RemoveRegistry(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\D730970B43C42A8459737C31E9A57E6B");

	DeleteInstallShield(_T("{97B2DE6D-0255-4A16-B3EF-28BD898BB7F4}")); // retail
	DeleteInstallShield(_T("{E86E86BB-051E-4F19-B111-0E5D15E7CDE4}")); // retail - upgrade

	return TRUE;
}

BOOL MySystemShutdown()
{
	HANDLE hToken; 
	TOKEN_PRIVILEGES tkp; 
	
	// Get a token for this process. 
	
	if (!OpenProcessToken(GetCurrentProcess(), 
        TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
		return( FALSE ); 
	
	// Get the LUID for the shutdown privilege. 
	
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
        &tkp.Privileges[0].Luid); 
	
	tkp.PrivilegeCount = 1;  // one privilege to set    
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	
	// Get the shutdown privilege for this process. 
	
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
        (PTOKEN_PRIVILEGES)NULL, 0); 
	
	if (GetLastError() != ERROR_SUCCESS) 
		return FALSE; 
	
	// Shut down the system and force all applications to close. 
	
	if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0)) 
		return FALSE; 
	
	return TRUE;
}

BOOL RemoveRegistrySCSI()
{
	TCHAR szSubKeyPath[255], szSubKeyName[255];
	unsigned long sizeKeyData = 256;
	DWORD dwIndex;
	HKEY hKey = NULL, hSubKey = NULL;
	LONG lResult;
	BOOL bReturn = FALSE;
	unsigned long sizeKey;
	FILETIME ftLastWriteTime;
	int i;

	vprintf(("\tStart RemoveRegistrySCSI\n"));

	HKEY hParentKey = HKEY_LOCAL_MACHINE;
	LPCTSTR lpszRegistry = _T("SYSTEM\\CurrentControlSet\\Enum\\SCSI");

	lResult = RegOpenKeyEx(
		hParentKey,
		lpszRegistry,
		0,
		KEY_ENUMERATE_SUB_KEYS,
		&hKey);

	if(ERROR_SUCCESS != lResult)
	{
		if(ERROR_FILE_NOT_FOUND == lResult) // don't care
			goto out;

		eprintf(("RegOpenKeyEx failed : %d\n", lResult));
		goto out;
	}

	dwIndex = 0;
	while(1)
	{
		sizeKey = 255;
		lResult = RegEnumKeyEx(
			hKey,
			dwIndex,
			szSubKeyName,
			&sizeKey,
			NULL,
			NULL,
			NULL,
			&ftLastWriteTime);

		if(ERROR_NO_MORE_ITEMS == lResult)
			break;
		if(ERROR_SUCCESS != lResult)
		{
			eprintf(("RegEnumKey failed : %d\n", lResult));
			goto out;
		}

		sprintf(szSubKeyPath, "%s\\%s", lpszRegistry, szSubKeyName);

		for(i = 0; i < g_nOEM; i++)
		{
			if(0 == _tcsncmp(g_aOEM[i].szSCSI_DiskName, szSubKeyName, _tcslen(g_aOEM[i].szSCSI_DiskName)))
			{
				RemoveRegistry(hParentKey, szSubKeyPath, TRUE);
				dwIndex= 0;
			}
		}

		dwIndex++;
	}	

	bReturn = TRUE;
out:

	if(hKey)
		RegCloseKey(hKey);
	return bReturn;
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;
	int iResult;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		cerr << _T("Fatal Error: MFC initialization failed") << endl;
		nRetCode = 1;
	}

	printf("NDAS_ERS Info : Performing erasing operations for NDAS software\n");
	printf("Ver 1.1\n");

	for(int i = 1; i < argc; i++)
	{
		if(0 == _tcsicmp(argv[i], _T("/V")))
		{
			printf("Verbose mode\n");
			g_bVerbous = TRUE;
		}
	}

	// AING_TEST
//	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\DeviceClasses\\{2accfe60-c130-11d2-b082-00a0c91efb8b}"), _T("DeviceInstance"), _T("LanscsiBus"), TRUE);
//	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\NDAS"), _T("InfSection"), _T("lanscsiminiport"));	

	// Check Applications running
	vprintf(("Searching for running NDAS application. . .\n"));
	while(FALSE == KillApplication(_T("Admin.exe")));
	while(FALSE == KillApplication(_T("AggrMirUI.exe")));

	// Initializing folder paths
	vprintf(("Initializing folder paths. . .\n"));
	SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_DEFAULT, g_szProgramPath);
	SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, SHGFP_TYPE_DEFAULT, g_szSystemPath);
	SHGetFolderPath(NULL, CSIDL_WINDOWS, NULL, SHGFP_TYPE_DEFAULT, g_szWindowsPath);

	SHGetFolderPath(NULL, CSIDL_COMMON_STARTUP, NULL, SHGFP_TYPE_DEFAULT, g_szStartupPath);
	SHGetFolderPath(NULL, CSIDL_COMMON_PROGRAMS, NULL, SHGFP_TYPE_DEFAULT, g_szProgramsPath);
	SHGetFolderPath(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_DEFAULT, g_szDesktopPath);

	// Stop & delete all Services
	vprintf(("Stop & delete all Services. . .\n"));
	StopService(_T("NetDisk_Service"));
	StopService(_T("LanscsiHelper"));

	// Deleting device & drivers
	vprintf(("Unloading device & drivers. . .\n"));
	vprintf(("\tUnloading ROFilter. . .\n"));
	UnloadROFilter();
	vprintf(("\tRemoving LanscsiMiniport. . .\n"));
	NDRemoveDevice(LANSCSIMINIPORT_HWID);
	vprintf(("\tDeleteing LanscsiMiniport. . .\n"));
	DeleteDeviceService(LANSCSIMINIPORT_SERVICE);
	vprintf(("\tRemoving LanscsiBus. . .\n"));
	NDRemoveDevice(LANSCSIBUS_HWID);
	vprintf(("\tDeleting LanscsiBus. . .\n"));
	DeleteDeviceService(LANSCSIBUS_SERVICE);
	vprintf(("\tUninstalling LPX. . .\n"));
//	StopService(_T("lpx"), FALSE);
	NDUninstallNetComp(LPX_NETCOMPID);
//	StopService(_T("lpx"));
	
	// Delete installer informations
	vprintf(("Delete installer informations. . .\n"));
	DeleteInstallerInformations();

	// Delete Driver Registries
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\lanscsibus"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\ROFilt"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\LfsFilt"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\lanscsiminiport"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\lanscsihelper"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\NetDisk_Service"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\lpx"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\Winsock\\Setup Migration\\Providers\\lpx"));

	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\Eventlog\\System\\lanscsibus"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Services\\Eventlog\\System\\lanscsiminiport"));

	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\CriticalDeviceDatabase\\lanscsibus#netdisk_v0"));
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\CriticalDeviceDatabase\\root#lanscsibus"));

	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E975-E325-11CE-BFC1-08002BE10318}"), _T("ComponentId"), _T("NKC_LPX"));

	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\LanscsiBus"), TRUE);

	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\LEGACY_LANSCSIHELPER"), TRUE);
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\LEGACY_NETDISK_SERVICE"), TRUE); // not sure
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\LEGACY_LPX"), TRUE);
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\LEGACY_ROFILT"), TRUE);
	RemoveRegistry(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\LEGACY_LFSFILT"), TRUE);

	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Class\\{4D36E97B-E325-11CE-BFC1-08002BE10318}"), _T("InfSection"), _T("lanscsiminiport"), TRUE);
	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Class\\{4D36E97D-E325-11CE-BFC1-08002BE10318}"), _T("InfSection"), _T("lanscsibus"), TRUE);

	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\SYSTEM"), _T("HardwareID"), _T("Root\\LANSCSIBus"), TRUE);

	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\DeviceClasses\\{2accfe60-c130-11d2-b082-00a0c91efb8b}"), _T("DeviceInstance"), _T("LanscsiBus"), TRUE);
	RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Enum\\Root\\SYSTEM"), _T("Service"), _T("lanscsibus"), TRUE);

	RemoveRegistrySCSI();

	// delete driver files
	DeleteSystemFile(_T("drivers\\lpx.sys"));
	DeleteSystemFile(_T("drivers\\rofilt.sys"));
	DeleteSystemFile(_T("drivers\\lfsfilt.sys"));
	DeleteSystemFile(_T("drivers\\lanscsibus.sys"));
	DeleteSystemFile(_T("drivers\\lanscsiminiport.sys"));
	DeleteSystemFile(_T("wshlpx.dll"));

	// delete oem files
	DeleteOEMFile(_T("Version"), _T("CatalogFile"), _T("netlpx.cat")); // lpx
	DeleteOEMFile(_T("Version"), _T("CatalogFile"), _T("lanscsibus.cat")); // lanscsi bus
	DeleteOEMFile(_T("Version"), _T("CatalogFile"), _T("lanscsiminiport.cat")); // lanscsi miniport

	// delete netdisk files
	DeleteNetDiskFiles();

	// delete icons
	DeleteNetDiskShortcuts();

	// reboot
	iResult = MessageBox(NULL, _T("It is strongly recommended you to reboot system. Do you want to reboot now?"), _T("Job complete"), MB_YESNO);
	if(IDYES == iResult)
	{
		printf("\n\n*** Rebooting System ***\n");
		MySystemShutdown();
	}	

	return 0;
}