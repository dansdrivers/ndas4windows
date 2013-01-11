#include "stdafx.h"
#include <regstr.h>
#include "autores.h"
#define XDBG_FILENAME "svcinst.cpp"
#define XDBG_LIBRARY_MODULE_FLAG 0x00001000
#include "xdebug.h"
#include "svcinst.h"

#define RFP_NO_PATH_PREFIX 0x0001

static LPTSTR pResolveFullPath(
	IN LPCTSTR szPath, 
	OUT LPDWORD pcchFullPath = NULL,
	OUT LPTSTR* ppszFilePart = NULL,
	IN DWORD Flags = 0);

static BOOL pPathContainsSpace(IN LPCTSTR szPath);
static LPTSTR pQuotePath(IN LPCTSTR szPath);
static LPCTSTR pServiceStartTypeString(DWORD dwStartType);
static LPCTSTR pServiceTypeString(DWORD dwServiceType);
static LPCTSTR pServiceErrorControlString(DWORD dwErrorControl);
static BOOL pOpenDevice(IN LPCTSTR DriverName, HANDLE * lphDevice);

//////////////////////////////////////////////////////////////////////////
//
// NdasDiServiceExists
//
//////////////////////////////////////////////////////////////////////////

NDASDI_API 
BOOL 
WINAPI
NdasDiServiceExistsSCH(
	SC_HANDLE schSCManager,
	LPCTSTR ServiceName)
{
	AutoSCHandle schService = OpenService( schSCManager,
		ServiceName,
		SERVICE_QUERY_STATUS);

	if (schService == NULL) {
		return FALSE;
	}

	return TRUE ;
}

NDASDI_API 
BOOL 
WINAPI
NdasDiFindService(
	LPCTSTR ServiceName, 
	LPBOOL pbPendingDeletion)
{
	BOOL fSuccess = FALSE;

	DPInfo(_FT("Finding a service %s.\n"), ServiceName);

	AutoSCHandle schSCManager = OpenSCManager(
		NULL, 
		NULL, 
		GENERIC_EXECUTE);

	if (NULL == (SC_HANDLE) schSCManager) {
		DPErrorEx(_FT("Opening SC Manager failed: "));
		return FALSE;
	}

	fSuccess = NdasDiServiceExistsSCH(schSCManager, ServiceName);

	if (fSuccess) {

		DPInfo(_FT("Service %s exists.\n"), ServiceName);

		fSuccess = NdasDiIsServiceMarkedForDeletion(ServiceName);
		if (fSuccess) {
			*pbPendingDeletion = TRUE;
		} else {
			*pbPendingDeletion = FALSE;
		}

		return TRUE;

	} else {

		DPErrorEx(_FT("Failed:"));

		fSuccess = NdasDiIsServiceMarkedForDeletion(ServiceName);
		if (fSuccess) {
			*pbPendingDeletion = TRUE;
			return TRUE;
		} else {
			*pbPendingDeletion = FALSE;
			return FALSE;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// NdasDiInstallService
//
//////////////////////////////////////////////////////////////////////////
//
// start type   SERVICE_DEMAND_START, SERVICE_BOOT_START,
//
NDASDI_API 
BOOL 
WINAPI
NdasDiInstallServiceSCH(
	SC_HANDLE schSCManager,
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD DesiredAccess,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password)
{

	DPInfo(_FT("Creating a service %s(DisplayName: %s, Descr: %s) ")
		_T("Access %d, %s, %s, %s, ")
		_T("Path %s, LoadOrderGroup %s, Dep %s, Account %s, Pwd %s\n"),
	   ServiceName,
	   (NULL == DisplayName) ? ServiceName : DisplayName,
	   Description,
	   DesiredAccess, 
	   pServiceTypeString(ServiceType), 
	   pServiceStartTypeString(StartType), 
	   pServiceErrorControlString(ErrorControl),
	   BinaryPathName, 
	   (LoadOrderGroup == NULL) ? _T("(none)") : LoadOrderGroup,
	   (Dependencies == NULL) ? _T("(none)") : Dependencies, 
	   (AccountName == NULL) ? _T("LocalSystem") : AccountName,
	   (Password == NULL) ? _T("(none)") : _T("(set)"));
	
	//
	// NOTE: This creates an entry for a standalone driver. If this
	//       is modified for use with a driver that requires a Tag,
	//       Group, and/or Dependencies, it may be necessary to
	//       query the registry for existing driver information
	//       (in order to determine a unique Tag, etc.).
	//

	AutoSCHandle schService = CreateService( 
		schSCManager,  
		ServiceName,   
		(NULL == DisplayName) ? ServiceName : DisplayName,
		DesiredAccess, 
		ServiceType, 
		StartType,	
		ErrorControl,
		BinaryPathName,
		LoadOrderGroup,
		lpdwTagId,   
		Dependencies,
		AccountName,
		Password);

	if (NULL == (SC_HANDLE) schService) {
		DPErrorEx(_FT("Creating a service %s failed: "), ServiceName);
		return FALSE;
	}

	if (NULL != Description) {
		SERVICE_DESCRIPTION svcDesc;
		svcDesc.lpDescription = (LPTSTR) Description;
		BOOL fSuccess = ChangeServiceConfig2(
			schService,
			SERVICE_CONFIG_DESCRIPTION,
			&svcDesc);
		if (!fSuccess) {
			DPWarningEx(_FT("Setting a service description failed: "));
		}
	}

	DPInfo(_FT("Service %s created successfully.\n"), ServiceName);
	return TRUE;
}

NDASDI_API 
BOOL 
WINAPI
NdasDiInstallService(
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD DesiredAccess,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR BinaryPathName,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies,
	LPCTSTR AccountName,
	LPCTSTR Password)
{

	DPInfo(_FT("Creating a service %s(Display: %s, Descr: %s) ")
		_T("Access %d, %s, %s, %s, ")
		_T("Path %s, LoadOrderGroup %s, Dep %s, Account %s, Pwd %s\n"),
		ServiceName,
		DisplayName,
		Description,
		DesiredAccess, 
		pServiceTypeString(ServiceType), 
		pServiceStartTypeString(StartType), 
		pServiceErrorControlString(ErrorControl),
		BinaryPathName, 
		(LoadOrderGroup == NULL) ? _T("(none)") : LoadOrderGroup,
		(Dependencies == NULL) ? _T("(none)") : Dependencies, 
		(AccountName == NULL) ? _T("LocalSystem") : AccountName,
		(Password == NULL) ? _T("(none)") : _T("(set)"));

	AutoSCHandle schSCManager = OpenSCManager(
		NULL, 
		NULL, 
		SC_MANAGER_ALL_ACCESS);

	if (NULL == (SC_HANDLE) schSCManager) {
		DPErrorEx(_FT("Opening sc manager failed: "));
		return FALSE;
	}

	return NdasDiInstallServiceSCH(
		schSCManager,
		ServiceName,
		DisplayName,
		Description,
		DesiredAccess,
		ServiceType,
		StartType,
		ErrorControl,
		BinaryPathName,
		LoadOrderGroup,
		lpdwTagId,
		Dependencies,
		AccountName,
		Password);

}

//////////////////////////////////////////////////////////////////////////
//
// NdasDiInstallDriverService
//
//////////////////////////////////////////////////////////////////////////

NDASDI_API 
BOOL 
WINAPI
NdasDiInstallDriverService(
	LPCTSTR SourceFilePath,
	LPCTSTR ServiceName,
	LPCTSTR DisplayName,
	LPCTSTR Description,
	DWORD ServiceType,
	DWORD StartType,
	DWORD ErrorControl,
	LPCTSTR LoadOrderGroup,
	LPDWORD lpdwTagId,
	LPCTSTR Dependencies)
{
	BOOL fSuccess = FALSE;
	HRESULT hr = S_FALSE;
	const TCHAR DRIVERS_DIR[] = _T("\\DRIVERS\\");

	TCHAR szDriverFilePath[MAX_PATH] = {0};

	DPInfo(_FT("Installing a driver service")
		_T(" from %s as %s(%s), load order %s, depends %s.\n"),
		SourceFilePath,
		DisplayName, ServiceName,
		LoadOrderGroup, Dependencies);
	
	LPTSTR lpFileName = NULL;

	LPTSTR lpszFullSrcPath = pResolveFullPath(
		SourceFilePath, 
		NULL,
		&lpFileName,
		0);

	if (NULL == lpszFullSrcPath) {
		DPErrorEx(_FT("Resolving full path of %s failed: "), SourceFilePath);
		return FALSE;
	}

	// We should LocalFree(lpszFullSrcPath).
	// Instead of manual Freeing, attach it to autoresource
	AutoHLocal hLocal = (HLOCAL) lpszFullSrcPath;

	// gets c:\windows\system32
	UINT cch = GetSystemDirectory(szDriverFilePath, MAX_PATH);
	if (0 == cch) {
		DPErrorEx(_FT("Getting a system directory failed: "));
		return FALSE;
	}

	// append \drivers\ -> "c:\windows\system32\drivers\"
	hr = StringCchCat(szDriverFilePath, MAX_PATH, DRIVERS_DIR);

	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr)) {
		DPErrorEx(_FT("Concating %s, %s failed: "), szDriverFilePath, DRIVERS_DIR);
		return FALSE;
	}

	// append mydriver.sys -> "c:\windows\system32\drivers\mydrivers.sys"
	hr = StringCchCat(szDriverFilePath, MAX_PATH, lpFileName);

	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr)) {
		DPErrorEx(_FT("Concating %s, %s failed: "), szDriverFilePath, lpFileName);
		return FALSE;
	}

	// copy to the system driver path
	DPInfo(_FT("Copying %s to %s.\n"), lpszFullSrcPath, szDriverFilePath);

	fSuccess = CopyFile(lpszFullSrcPath, szDriverFilePath, FALSE);
	if (!fSuccess) {
		DPErrorEx(_FT("Copying %s to %s failed: "), lpszFullSrcPath, szDriverFilePath);
		return FALSE;
	}

	// call actual service installer
	return NdasDiInstallService(
		ServiceName, 
		DisplayName, 
		Description,
		SERVICE_ALL_ACCESS, 
		ServiceType,
		StartType,
		SERVICE_ERROR_NORMAL,
		szDriverFilePath,
		LoadOrderGroup,
		lpdwTagId,
		Dependencies,
		NULL,
		NULL);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasDiStartService
//
//////////////////////////////////////////////////////////////////////////

NDASDI_API 
BOOL 
WINAPI
NdasDiStartServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR ServiceName,
	IN DWORD argc,
	IN LPCTSTR* argv)
{
	BOOL fSuccess = FALSE;

	DPInfo(_FT("Starting a service %s.\n"), ServiceName);

	AutoSCHandle hService = OpenService(
		schSCManager, 
		ServiceName, 
		SERVICE_STOP);

	if (NULL == (SC_HANDLE) hService) {
		DPErrorEx(_FT("Opening a service %s failed: "));
		return FALSE;
	}

	fSuccess = StartService(hService, argc, argv);

	if (!fSuccess) {
		DPErrorEx(_FT("Starting a service %s failed: "));
		return FALSE;
	}

	DPInfo(_FT("Service %s started successfully.\n"));
	return TRUE;

}

NDASDI_API 
BOOL 
WINAPI
NdasDiStartService(
	IN LPCTSTR ServiceName,
	IN DWORD argc,
	IN LPCTSTR* argv)
{
	BOOL fSuccess = FALSE;

	AutoSCHandle schSCManager = OpenSCManager(
		NULL, 
		NULL, 
		GENERIC_EXECUTE);

	DPInfo(_FT("Starting a service %s.\n"), ServiceName);

	if (NULL == (SC_HANDLE) schSCManager) {
		DPErrorEx(_FT("Opening SC Manager failed: "));
		return FALSE;
	}

	return NdasDiStartServiceSCH(
		schSCManager, 
		ServiceName, 
		argc, 
		argv);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasDiStopService
//
//////////////////////////////////////////////////////////////////////////
NDASDI_API 
BOOL 
WINAPI
NdasDiStopServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR ServiceName)
{
	BOOL fSuccess = FALSE;

	DPInfo(_FT("Stopping a service %s.\n"), ServiceName);

	AutoSCHandle hService = OpenService(
		schSCManager, 
		ServiceName, 
		SERVICE_STOP);

	if (NULL == (SC_HANDLE) hService) {
		DPErrorEx(_FT("Opening a service %s failed: "));
		return FALSE;
	}

	SERVICE_STATUS Status = {0};
	fSuccess = ControlService(hService, SERVICE_CONTROL_STOP, &Status);

	if (!fSuccess) {
		DPErrorEx(_FT("Stopping a service %s failed: "));
		return FALSE;
	}

	DPInfo(_FT("Service %s stopped successfully.\n"));
	return TRUE;
}

NDASDI_API 
BOOL 
WINAPI
NdasDiStopService(
	IN LPCTSTR ServiceName)
{
	BOOL fSuccess = FALSE;

	DPInfo(_FT("Stopping a service %s.\n"), ServiceName);

	AutoSCHandle schSCManager = OpenSCManager(
		NULL, 
		NULL, 
		GENERIC_EXECUTE);

	if (NULL == (SC_HANDLE) schSCManager) {
		DPErrorEx(_FT("Opening SC Manager failed: "));
		return FALSE;
	}

	return NdasDiStopServiceSCH(
		schSCManager, 
		ServiceName);
}

//////////////////////////////////////////////////////////////////////////
//
// NdasDiDeleteService
//
//////////////////////////////////////////////////////////////////////////

NDASDI_API 
BOOL 
WINAPI 
NdasDiDeleteServiceSCH(
	IN SC_HANDLE schSCManager,
	IN LPCTSTR	ServiceName)
{
	BOOL fSuccess = FALSE;

	DPInfo(_FT("Deleting Service %s.\n"), ServiceName);

	AutoSCLock scLock = LockServiceDatabase(schSCManager);
	if (NULL == (SC_LOCK) scLock) {
		DPErrorEx(_FT("Locking service database failed: "));
		return FALSE;
	}

	AutoSCHandle hService = OpenService(
		schSCManager, 
		ServiceName, 
		DELETE);

	if (NULL == (SC_HANDLE) hService) {
		DPErrorEx(_FT("Opening a service %s failed: "), ServiceName);
		return FALSE;
	}

	fSuccess = DeleteService(hService);

	if (!fSuccess) {
		DPErrorEx(_FT("Deleting a service %s failed: "), ServiceName);
		return FALSE;
	}

	DPInfo(_FT("Service %s deleted successfully.\n"), ServiceName);
	return TRUE;
}

NDASDI_API 
BOOL 
WINAPI 
NdasDiDeleteService(
	IN LPCTSTR	ServiceName)
{
	AutoSCHandle schSCManager = OpenSCManager(
		NULL, 
		NULL, 
		SC_MANAGER_ALL_ACCESS );

	DPInfo(_FT("Deleting Service %s.\n"), ServiceName);

	if (NULL == (SC_HANDLE) schSCManager) {
		DPErrorEx(_FT("Opening SC Manager failed: "));
		return FALSE;
	}

	return NdasDiDeleteServiceSCH(schSCManager, ServiceName);
}

NDASDI_API 
BOOL 
WINAPI
NdasDiIsServiceMarkedForDeletion(
	IN LPCTSTR ServiceName)
{
	TCHAR szKey[MAX_PATH] = {0};

	HRESULT hr = StringCchPrintf(
		szKey, 
		MAX_PATH, 
		REGSTR_PATH_SERVICES _T("\\%s"), 
		ServiceName);

	DWORD dwFlag;
	DWORD dwFlagSize = sizeof(dwFlag);
	HKEY hKey;

	LONG lResult = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		szKey, 
		0, 
		KEY_QUERY_VALUE, 
		&hKey);

	if (ERROR_SUCCESS != lResult) {
		return FALSE;
	}

	AutoHKey autoHKey = hKey;

	lResult = RegQueryValueEx(
		hKey,
		_T("DeleteFlag"),
		0, 
		NULL, 
		(LPBYTE) &dwFlag, 
		&dwFlagSize);

	if (ERROR_SUCCESS == lResult && 1 == dwFlag) {
		DPInfo(_FT("Service %s is marked for deletion.\n"), ServiceName);
		return TRUE;
	}

	return FALSE;			
}

//////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
//////////////////////////////////////////////////////////////////////////

static LPCTSTR pServiceStartTypeString(DWORD dwStartType)
{
	switch (dwStartType) {
	case SERVICE_AUTO_START: return _T("SERVICE_AUTO_START");
	case SERVICE_BOOT_START: return _T("SERVICE_BOOT_START");
	case SERVICE_DEMAND_START: return _T("SERVICE_DEMAND_START");
	case SERVICE_DISABLED: return _T("SERVICE_DISABLED");
	case SERVICE_SYSTEM_START: return _T("SERVICE_SYSTEM_START");
	default:
		return _T("INVALID_SERVICE_START_TYPE");
	}
}

static LPCTSTR pServiceTypeString(DWORD dwServiceType)
{
	if (SERVICE_FILE_SYSTEM_DRIVER == dwServiceType) {
		return _T("SERVICE_FILE_SYSTEM_DRIVER");
	} else if (SERVICE_KERNEL_DRIVER == dwServiceType) {
		return _T("SERVICE_KERNEL_DRIVER");
	} else if (SERVICE_WIN32_OWN_PROCESS & dwServiceType) {
		if (SERVICE_INTERACTIVE_PROCESS & dwServiceType) {
			return _T("SERVICE_WIN32_OWN_PROCESS (INTERACTIVE)");
		} else {
			return _T("SERVICE_WIN32_OWN_PROCESS");
		}
	} else if (SERVICE_WIN32_SHARE_PROCESS & dwServiceType) {
		if (SERVICE_INTERACTIVE_PROCESS & dwServiceType) {
			return _T("SERVICE_WIN32_SHARE_PROCESS (INTERACTIVE)");
		} else {
			return _T("SERVICE_WIN32_SHARE_PROCESS");
		}
	}
	return _T("INVALID_SERVICE_TYPE");
}

static LPCTSTR pServiceErrorControlString(DWORD dwErrorControl)
{
	switch (dwErrorControl) {
	case SERVICE_ERROR_IGNORE: return _T("SERVICE_ERROR_IGNORE");
	case SERVICE_ERROR_NORMAL: return _T("SERVICE_ERROR_NORMAL");
	case SERVICE_ERROR_SEVERE: return _T("SERVICE_ERROR_SEVERE");
	case SERVICE_ERROR_CRITICAL: return _T("SERVICE_ERROR_CRITICAL");
	default: return _T("INVALID_SERVICE_ERROR_CONTROL_TYPE");
	}
}

//////////////////////////////////////////////////////////////////////////
//
// Resolve Full Path
//
//////////////////////////////////////////////////////////////////////////

static LPTSTR pResolveFullPath(
	IN LPCTSTR szPath, 
	OUT LPDWORD pcchFullPath, 
	OUT LPTSTR* ppszFilePart,
	IN DWORD Flags)
{

#ifdef UNICODE
	static const TCHAR PATH_PREFIX[] = L"\\\\?\\";
	static const DWORD PATH_PREFIX_LEN = RTL_NUMBER_OF(PATH_PREFIX);
#else
	static const TCHAR PATH_PREFIX[] = "";
	static const DWORD PATH_PREFIX_LEN = RTL_NUMBER_OF(PATH_PREFIX);
#endif

	//
	// szPathBuffer = \\?\
	// lpszPathBuffer ptr ^
	//
	LPTSTR lpszLongPathBuffer = NULL;
	BOOL fSuccess = FALSE;

	// Inf must be a full pathname
	DWORD cch = GetFullPathName(
		szPath,
		0,
		NULL,
		NULL);

	if (0 == cch) {
		return NULL;
	}

	// cch contains required buffer size
	lpszLongPathBuffer = (LPTSTR) LocalAlloc(
		LPTR,
		(PATH_PREFIX_LEN - 1 + cch) * sizeof(TCHAR));

	if (NULL == lpszLongPathBuffer) {
		// out of memory
		return NULL;
	}


	// lpsz is a path without path prefix
	LPTSTR lpsz = lpszLongPathBuffer + (PATH_PREFIX_LEN - 1);

	cch = GetFullPathName(
		szPath,
		cch,
		lpsz,
		ppszFilePart);

	if (0 == cch) {
		LocalFree(lpszLongPathBuffer);
		return NULL;
	}

	if (NULL != pcchFullPath) {
		*pcchFullPath = cch;
	}

	if (!(Flags & RFP_NO_PATH_PREFIX)) {
		if (_T('\\') != lpsz[0] || _T('\\') != lpsz[1]) {
			// UNC path does not support path prefix "\\?\" 
			// Also path with \\?\ does not need path prefix
			::CopyMemory(
				lpszLongPathBuffer, 
				PATH_PREFIX, 
				(PATH_PREFIX_LEN - 1) * sizeof(TCHAR));
			lpsz = lpszLongPathBuffer;
			if (NULL != pcchFullPath) {
				*pcchFullPath += PATH_PREFIX_LEN;
			}
		}
	}

	return lpsz;
}

//////////////////////////////////////////////////////////////////////////
//
// pPathContainsSpace
//
//////////////////////////////////////////////////////////////////////////

static BOOL pPathContainsSpace(IN LPCTSTR szPath)
{
#ifdef UNICODE
	static const DWORD PATH_LIMIT = 32767;
#else
	static const DWORD PATH_LIMIT = MAX_PATH;
#endif
	LPCTSTR lp = szPath;
	for (DWORD i = 0; lp && i < PATH_LIMIT; ++i, ++lp) {
		if (_T(' ') == *lp) {
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
//
// Quote Path
//
//////////////////////////////////////////////////////////////////////////

static LPTSTR pQuotePath(IN LPCTSTR szPath)
{
#ifdef UNICODE
	static const DWORD PATH_LIMIT = 32767;
#else
	static const DWORD PATH_LIMIT = MAX_PATH;
#endif
	size_t cch = 0;
	HRESULT hr = StringCchLength(szPath, PATH_LIMIT, &cch);

	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr)) {
		DPErrorEx(_FT("Get string length for %s failed (hr = %08X): "), hr);
		return NULL;
	}

	// do not quote for already quoted path
	LPTSTR szQuoted = (LPTSTR) LocalAlloc(LPTR, (cch + 2) * sizeof(TCHAR));
	hr = StringCchPrintf(szQuoted, cch + 2, _T("\"%s\""), szPath);

	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr)) {
		DPErrorEx(_FT("Quoting a string %s failed (hr = 0x%08X): "), szPath, hr);
		LocalFree((HLOCAL)szQuoted);
		return NULL;
	}

	return szQuoted;
}

//////////////////////////////////////////////////////////////////////////
//
// pOpenDevice
//
//////////////////////////////////////////////////////////////////////////

static BOOL pOpenDevice(IN LPCTSTR DriverName, HANDLE * lphDevice )
{
	TCHAR    completeDeviceName[MAX_PATH];
	HANDLE   hDevice;

	HRESULT hr = S_FALSE;

	//
	// Create a \\.\XXX device name that CreateFile can use
	//
	// NOTE: We're making an assumption here that the driver
	//       has created a symbolic link using it's own name
	//       (i.e. if the driver has the name "XXX" we assume
	//       that it used IoCreateSymbolicLink to create a
	//       symbolic link "\DosDevices\XXX". Usually, there
	//       is this understanding between related apps/drivers.
	//
	//       An application might also peruse the DEVICEMAP
	//       section of the registry, or use the QueryDosDevice
	//       API to enumerate the existing symbolic links in the
	//       system.
	//

	if( (GetVersion() & 0xFF) >= 5 ) {

		//
		// We reference the global name so that the application can
		// be executed in Terminal Services sessions on Win2K
		//
		hr = StringCchPrintf(completeDeviceName, RTL_NUMBER_OF(completeDeviceName),
			TEXT("\\\\.\\Global\\%s"), DriverName);

		_ASSERTE(SUCCEEDED(hr));

	} else {

		hr = StringCchPrintf(completeDeviceName, RTL_NUMBER_OF(completeDeviceName),
			TEXT("\\\\.\\%s"), DriverName );

		_ASSERTE(SUCCEEDED(hr));

	}

	hDevice = CreateFile(
		completeDeviceName,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if ( INVALID_HANDLE_VALUE == hDevice) {
		return FALSE;
	}

	// If user wants handle, give it to them.  Otherwise, just close it.
	if ( lphDevice ) {
		*lphDevice = hDevice;
	} else {
		DWORD err = GetLastError();
		CloseHandle(hDevice);
		SetLastError(err);
	}

	return TRUE;
}

