//+---------------------------------------------------------------------------
//
//  Microsoft Windows
//  Copyright (C) Microsoft Corporation, 1997.
//
//  File:       M A I N . C P P
//
//  Contents:   Code to provide a simple cmdline interface to
//              the sample code functions
//
//  Notes:      The code in this file is not required to access any
//              netcfg functionality. It merely provides a simple cmdline
//              interface to the sample code functions provided in
//              file snetcfg.cpp.
//
//  Author:     kumarp    28-September-98
//
//----------------------------------------------------------------------------

//#include "pch.h"
//#pragma hdrstop

#include "NDSetup.h"
#include "NDNetComp.h"
#include "snetcfg.h"

void LogNetCfgError(HRESULT hr);

#ifdef DBG
#ifdef LIBTEST
int __cdecl wmain(int argc, LPWSTR *argv)
{
	 NDInstallNetComp(L"TEST", 0, L"0");
	 NDUninstallNetComp(L"TEST");
	return 0;
}
#endif
#endif

//++
//
//	try to install LPX protocol up to INSTALL_RETRY times
//
//	install LPX protocol
//
//	return value
//
//	NDS_SUCCESS
//  NDS_REBOOT_REQUIRED
/// NDS_PREBOOT_REQUIRED
//  NDS_FAIL
//

int __stdcall NDInstallNetComp(PCWSTR szNetComp, UINT nc, PCWSTR szInfFullPath)
{
    HRESULT hr ;
	int iRet;

	LogPrintf(TEXT("Entering NDInstallNetProtocol(%s, %d, %s)..."), szNetComp, nc, szInfFullPath);

	hr = HrInstallNetComponent(szNetComp, (NetClass) nc, szInfFullPath) ;

	LogNetCfgError(hr);

	if(SUCCEEDED(hr))
	{
		if (hr == NETCFG_S_REBOOT)
			iRet = NDS_REBOOT_REQUIRED;
		else
			iRet = NDS_SUCCESS;
	}
	else if(hr == NETCFG_E_NEED_REBOOT)
	{
		LogPrintf(TEXT("LpxInstall(), A system reboot is required before the component can be installed."));
		iRet = NDS_PREBOOT_REQUIRED;
		SetLastError(HRESULT_CODE(hr));
	}
	else
	{
		SetLastError(HRESULT_CODE(hr));
		iRet = NDS_FAIL;
	}

	LogPrintf(TEXT("Leaving NDInstallNetProtocol()... %d"), iRet);
	return iRet;
}

//++
//
//	uninstall LPX
//
//	return value
//
//	NDS_SUCCESS
//	NDS_REBOOT_REQUIRED
//	NDS_FAIL
//

int __stdcall NDUninstallNetComp(PCWSTR szNetComp)
{
    HRESULT hr ;
	int	iRet;

	LogPrintf(TEXT("Entering NDUninstallNetProtocol(%s)..."), szNetComp);

	hr = HrUninstallNetComponent(szNetComp) ;

	LogNetCfgError(hr);

	iRet = NDS_FAIL;
	if(SUCCEEDED(hr))
	{
		if(hr == NETCFG_S_REBOOT)
			iRet = NDS_REBOOT_REQUIRED;
		else
			iRet = NDS_SUCCESS;
	}

	LogPrintf(TEXT("Leaving NDUninstallNetProtocol()... %d"), iRet);
	return iRet;
}

//++
//
// log HRESULTs of NetCfgx to the string
// 
// returns none
//

void LogNetCfgError(HRESULT hr)
{
	switch (hr)
	{
	case S_OK:
		LogPrintf(TEXT("S_OK"));
		break;
	case NETCFG_E_ALREADY_INITIALIZED:
		LogPrintf(TEXT("NETCFG_E_ALREADY_INITIALIZED"));
		break;
	case NETCFG_E_NOT_INITIALIZED:
		LogPrintf(TEXT("NETCFG_E_NOT_INITIALIZED"));
		break;
	case NETCFG_E_IN_USE:
		LogPrintf(TEXT("NETCFG_E_IN_USE"));
		break;
	case NETCFG_E_NO_WRITE_LOCK:
		LogPrintf(TEXT("NETCFG_E_NO_WRITE_LOCK"));
		break;
	case NETCFG_E_NEED_REBOOT:
		LogPrintf(TEXT("NETCFG_E_NEED_REBOOT"));
		break;
	case NETCFG_E_ACTIVE_RAS_CONNECTIONS:
		LogPrintf(TEXT("NETCFG_E_ACTIVE_RAS_CONNECTIONS"));
		break;
	case NETCFG_E_ADAPTER_NOT_FOUND:
		LogPrintf(TEXT("NETCFG_E_ADAPTER_NOT_FOUND"));
		break;
	case NETCFG_E_COMPONENT_REMOVED_PENDING_REBOOT:
		LogPrintf(TEXT("NETCFG_E_COMPONENT_REMOVED_PENDING_REBOOT"));
		break;
	case NETCFG_S_REBOOT:
		LogPrintf(TEXT("NETCFG_S_REBOOT"));
		break;
	case NETCFG_S_DISABLE_QUERY:
		LogPrintf(TEXT("NETCFG_S_DISABLE_QUERY"));
		break;
	case NETCFG_S_STILL_REFERENCED:
		LogPrintf(TEXT("NETCFG_S_STILL_REFERENCED"));
		break;
	case NETCFG_S_CAUSED_SETUP_CHANGE:
		LogPrintf(TEXT("NETCFG_S_CAUSED_SETUP_CHANGE"));
		break;
	default:
		LogPrintfErr(TEXT("NetCfg UNKNOWN_ERROR"));
		break;
	}
	return;
}
