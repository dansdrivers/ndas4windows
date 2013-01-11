#include "stdafx.h"
#include "msilog.h"
#include "fstrbuf.h"

static 
LPCTSTR 
UPGRADE_SAFE_REMOVE_PARENT_REGKEY = _T("Software\\NDASSetup");

static 
LPCTSTR 
UPGRADE_SAFE_REMOVE_REGKEY = _T("Software\\NDASSetup\\UpgradeSafeRemove");

static 
LPCTSTR 
UPGRADE_SAFE_REMOVE_REGKEYNAME = _T("UpgradeSafeRemove");

static
LPCTSTR
UPGRADE_SAFE_REMOVE_PROPERTY = _T("NdasMsmUpgradeSafeRemove");

//
// Immediate action
//
NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiSetUpgradeSafeRemoveContext(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	HKEY hKey;

	//
	// This key is a volatile key, which will be delete on reboot
	//
	// This uses HKEY_CURRENT_USER!
	//
	LONG lResult = ::RegCreateKeyEx(
		HKEY_CURRENT_USER,
		UPGRADE_SAFE_REMOVE_REGKEY,
		0,
		NULL,
		REG_OPTION_VOLATILE,
		KEY_ALL_ACCESS,
		NULL,
		&hKey,
		NULL);

	if (ERROR_SUCCESS != lResult) 
	{
		MSILOGERR(FSB256(_T("Creating %s failed"), UPGRADE_SAFE_REMOVE_REGKEY));
		::SetLastError(lResult);
		return ERROR_INSTALL_FAILURE;
	}

	::RegCloseKey(hKey);

	MSILOGINFO(_T("Upgrade safe removal requested"));

	return ERROR_SUCCESS;

}

NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiClearUpgradeSafeRemoveContext(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	HKEY hKey;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_CURRENT_USER, 
		UPGRADE_SAFE_REMOVE_PARENT_REGKEY, 
		0,
		DELETE,
		&hKey);

	if (ERROR_SUCCESS != lResult)
	{
		// not an upgrade safe removal
		MSILOGINFO(_T("Context parent does not exist"));
		return ERROR_SUCCESS;
	}

	lResult = ::RegDeleteKey(hKey, UPGRADE_SAFE_REMOVE_REGKEYNAME);

	::RegCloseKey(hKey);

	if (ERROR_SUCCESS == lResult)
	{
		MSILOGINFO(_T("Context deleted successfully!"));
	}
	else if (ERROR_FILE_NOT_FOUND == lResult)
	{
		MSILOGINFO(_T("Context does not exist!"));
	}
	else
	{
		MSILOGERR_PROC2(FSB256(_T("RegDeleteKey(%s) failed"), UPGRADE_SAFE_REMOVE_REGKEYNAME), lResult);
		return ERROR_INSTALL_FAILURE;
	}

	return ERROR_SUCCESS;
}

// Immediate action
NC_DLLSPEC 
UINT 
NC_CALLSPEC 
NcaMsiGetUpgradeSafeRemoveProperty(MSIHANDLE hInstall)
{
	RUN_MODE_TRACE(hInstall);

	// This sets NDASMM_UPGRADE_SAFE_REMOVE 
	// if UPGRADE_SAFE_REMOVE_REGKEY exists
	HKEY hKey;
	LONG lResult = ::RegOpenKeyEx(
		HKEY_CURRENT_USER, 
		UPGRADE_SAFE_REMOVE_REGKEY, 
		0,
		KEY_READ,
		&hKey);
	if (ERROR_SUCCESS != lResult)
	{
		// not an upgrade safe removal
		MSILOGINFO(_T("Not an upgrade safe removal"));
		return ERROR_SUCCESS;
	}

	::RegCloseKey(hKey);

	MSILOGINFO(_T("Upgrade safe removal initiated"));

	UINT msiRet = ::MsiSetProperty(
		hInstall, 
		UPGRADE_SAFE_REMOVE_PROPERTY, 
		_T("1"));

	if (ERROR_SUCCESS != msiRet)
	{
		MSILOGERR_PROC2(FSB256(_T("MsiSetProperty(%s) failed"), UPGRADE_SAFE_REMOVE_PROPERTY), msiRet);
		return ERROR_INSTALL_FAILURE;
	}

	return ERROR_SUCCESS;
}

