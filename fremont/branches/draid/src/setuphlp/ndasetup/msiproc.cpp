#include "stdafx.h"
#include "msiproc.h"


UINT MsiApiLoad(PMSIAPI pMsiApi)
{
	_ASSERTE(!IsBadWritePtr(pMsiApi,sizeof(MSIAPI)));

	HMODULE hMsi = ::LoadLibrary(MSI_DLL);
	if (NULL == hMsi) {
		return ::GetLastError();
	}

	if (hMsi)
	{
		pMsiApi->SetInternalUI = (PFnMsiSetInternalUI)GetProcAddress(hMsi, MSIAPI_MsiSetInternalUI);
		pMsiApi->InstallProduct = (PFnMsiInstallProduct)GetProcAddress(hMsi, MSIAPI_MsiInstallProduct);
		pMsiApi->ApplyPatch = (PFnMsiApplyPatch)GetProcAddress(hMsi, MSIAPI_MsiApplyPatch);
		pMsiApi->ReinstallProduct = (PFnMsiReinstallProduct)GetProcAddress(hMsi, MSIAPI_MsiReinstallProduct);
		pMsiApi->QueryProductState = (PFnMsiQueryProductState)GetProcAddress(hMsi, MSIAPI_MsiQueryProductState);
		pMsiApi->OpenDatabase = (PFnMsiOpenDatabase)GetProcAddress(hMsi, MSIAPI_MsiOpenDatabase);
		pMsiApi->DatabaseOpenView = (PFnMsiDatabaseOpenView)GetProcAddress(hMsi, MSIAPI_MsiDatabaseOpenView);
		pMsiApi->ViewExecute = (PFnMsiViewExecute)GetProcAddress(hMsi, MSIAPI_MsiViewExecute);
		pMsiApi->ViewFetch = (PFnMsiViewFetch)GetProcAddress(hMsi, MSIAPI_MsiViewFetch);
		pMsiApi->RecordGetString = (PFnMsiRecordGetString)GetProcAddress(hMsi, MSIAPI_MsiRecordGetString);
		pMsiApi->CloseHandle = (PFnMsiCloseHandle)GetProcAddress(hMsi, MSIAPI_MsiCloseHandle);
		pMsiApi->EnableLog = (PFnMsiEnableLog)GetProcAddress(hMsi, MSIAPI_MsiEnableLog);
		pMsiApi->SummmaryInfoGetProperty = (PFnMsiSummaryInfoGetProperty)GetProcAddress(hMsi,MSIAPI_MsiSummaryInfoGetProperty);
		pMsiApi->GetSummaryInformation = (PFnMsiGetSummaryInformation)GetProcAddress(hMsi,MSIAPI_MsiGetSummaryInformation);
	}

	if (!pMsiApi->SetInternalUI ||
		!pMsiApi->InstallProduct ||
		!pMsiApi->ApplyPatch ||
		!pMsiApi->ReinstallProduct ||
		!pMsiApi->QueryProductState ||
		!pMsiApi->OpenDatabase ||
		!pMsiApi->DatabaseOpenView ||
		!pMsiApi->ViewExecute ||
		!pMsiApi->ViewFetch ||
		!pMsiApi->RecordGetString ||
		!pMsiApi->CloseHandle ||
		!pMsiApi->EnableLog ||
		!pMsiApi->GetSummaryInformation ||
		!pMsiApi->SummmaryInfoGetProperty)
	{
		::FreeLibrary(hMsi);
		return ERROR_LOAD_MSI_API_FAILED;
	}

	pMsiApi->hModule = hMsi;
	return ERROR_SUCCESS;
}

UINT MsiApiUnload(PMSIAPI pMsiApi)
{
	_ASSERTE(!IsBadReadPtr(pMsiApi, sizeof(MSIAPI)));
	_ASSERTE(NULL != pMsiApi->hModule);
	BOOL fSuccess = ::FreeLibrary(pMsiApi->hModule);
	if (!fSuccess) {
		return ::GetLastError();
	}
	return ERROR_SUCCESS;
}
