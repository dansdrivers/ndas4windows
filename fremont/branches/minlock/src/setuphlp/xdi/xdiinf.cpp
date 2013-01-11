#include "precomp.hpp"
#include "xdi.h"

//
// Returns S_OK when found, otherwise returns S_FALSE
//
HRESULT
xDipHardwareIdInList(
	__in LPCWSTR HardwareId, 
	__in LPCWSTR TargetHardwareIdList)
{
	LPCWSTR p = TargetHardwareIdList;

	while (TRUE) 
	{
		if (L'\0' == *p) 
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "Hardware ID %ls not found.\n", TargetHardwareIdList);
			return S_FALSE;
		}

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "Comparing Hardware ID %ls against %ls\n", HardwareId, p);

		if (0 == lstrcmpiW(HardwareId, p)) 
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "Found %ls\n", p);
			return S_OK;
		}

		// next hardware ID
		_ASSERTE(!IsBadStringPtr(p, -1));
		while (L'\0' != *p) 
		{
			++p;
		}
		_ASSERTE(!IsBadStringPtr(p, -1));
		++p;
	}
}

HRESULT
xDipInfHandleModelContainsHardwareId(
	HINF InfFileHandle, 
	LPCWSTR ModelSection,
	LPCWSTR Platform,
	LPCWSTR HardwareIdList)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, "[%ls.%ls]\n", 
		ModelSection, Platform ? Platform : L"none");

	WCHAR actualSection[255] = {0};

	HRESULT hr = StringCchCopy(
		actualSection, 
		RTL_NUMBER_OF(actualSection),
		ModelSection);

	if (FAILED(hr))
	{
		return hr;
	}

	//
	// If szPlatform is given, append to the section
	//
	if (Platform && Platform[0] != 0)
	{
		hr = StringCchCatW(actualSection, RTL_NUMBER_OF(actualSection), L".");
		if (FAILED(hr)) return hr;

		hr = StringCchCatW(actualSection, RTL_NUMBER_OF(actualSection), Platform);
		if (FAILED(hr)) return hr;
	}

	INFCONTEXT infContext = {0};

	BOOL success = SetupFindFirstLine(
		InfFileHandle, 
		actualSection,
		NULL,
		&infContext);

	if (!success) 
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());
		return hr;
	}

	do 
	{
		WCHAR infHardwareId[MAX_PATH] = {0};
		
		success = SetupGetStringField(
			&infContext, 2, infHardwareId, MAX_PATH, NULL);
		
		if (!success)
		{
			hr = HRESULT_FROM_SETUPAPI(GetLastError());
			return hr;
		}

		hr = xDipHardwareIdInList(infHardwareId, HardwareIdList);

		if (S_OK == hr)
		{
			return hr;
		}

		success = SetupFindNextLine(&infContext, &infContext);

	} while (success);

	return S_FALSE;
}

HRESULT
xDipInfHandleContainsHardwareId(
	HINF InfFileHandle,
	LPCTSTR HardwareIdList)
{
	HRESULT hr;
	INFCONTEXT infContext = {0};
	WCHAR model[MAX_PATH] = {0};

	BOOL success = SetupFindFirstLineW(
		InfFileHandle,
		L"Manufacturer",
		NULL,
		&infContext);

	if (!success) 
	{
		XTLTRACE1(TRACE_LEVEL_WARNING, "No Manufacturer section found.\n");
		return S_FALSE;
	}

	do 
	{
		//
		// Model Section Fields
		//
		success = SetupGetStringFieldW(
			&infContext, 1, model, MAX_PATH, NULL);
		if (!success) 
		{
			XTLTRACE1(TRACE_LEVEL_ERROR, "No Model section found.\n");
			return S_FALSE;
		}
		
		XTLTRACE1(TRACE_LEVEL_INFORMATION, "Model: %ls\n", model);

		DWORD fieldCount = SetupGetFieldCount(&infContext);

		XTLTRACE1(TRACE_LEVEL_INFORMATION, "Decorations: %d\n", fieldCount);

		if (fieldCount < 2)
		{
			XTLTRACE1(TRACE_LEVEL_INFORMATION, "TargetOS: (none)\n");

			hr = xDipInfHandleModelContainsHardwareId(
				InfFileHandle, model, NULL, HardwareIdList);

			if (S_OK == hr)
			{
				return hr;
			}
		}
		else
		{
			// String Field is 1-based index
			for (DWORD i = 2; i <= fieldCount; ++i)
			{
				// Decoration: e.g. NTamd64
				WCHAR decoration[50] = {0};
				success = SetupGetStringFieldW(
					&infContext, i, 
					decoration, RTL_NUMBER_OF(decoration), 
					NULL);
				// Ignore if there is no decoration
				if (!success)
				{
					continue;
				}

				XTLTRACE1(TRACE_LEVEL_INFORMATION, "TargetOS: %ls\n", decoration);
				
				hr = xDipInfHandleModelContainsHardwareId(
					InfFileHandle, model, decoration, HardwareIdList);
				
				if (S_OK == hr)
				{
					return hr;
				}
			}
		}

		success = SetupFindNextLine(&infContext, &infContext);

	} while (success);

	return S_FALSE;
}

HRESULT
xDipInfFileContainsHardwareId(
	LPCWSTR InfFile, 
	LPCWSTR HardwareIdList)
{
	HRESULT hr;

	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"Inspecting INF %ls for %ls...\n", InfFile, HardwareIdList);

	HINF infHandle = SetupOpenInfFileW(
		InfFile,
		NULL,
		INF_STYLE_WIN4,
		0);

	if (INVALID_HANDLE_VALUE == infHandle) 
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		XTLTRACE1(TRACE_LEVEL_ERROR, 
			"SetupOpenInfFile failed, hr=0x%X\n", hr);

		return hr;
	}

	hr = xDipInfHandleContainsHardwareId(infHandle, HardwareIdList);

	SetupCloseInfFile(infHandle);

	return hr;
}

HRESULT 
WINAPI
xDiUninstallHardwareOEMInf(
	__in_opt LPCWSTR FileFindSpec, /* if null, oem*.inf */
	__in LPCWSTR HardwareIdList, /* should be double _T('\0') terminated */
	__in DWORD Flags,
	__in_opt XDI_REMOVE_INF_CALLBACK Callback,
	__in_opt LPVOID CallbackContext)
{
	XTLTRACE1(TRACE_LEVEL_INFORMATION, 
		"FileFindSpec=%ls, HardwareIdList=%ls, Flags=%08X\n",
		FileFindSpec, HardwareIdList, Flags);

	HRESULT hr;
	_ASSERTE(NULL == Callback || !IsBadCodePtr((FARPROC)Callback));

	BOOL success = FALSE;

	WCHAR infDir[MAX_PATH];
	WCHAR infFindSpec[MAX_PATH];

	UINT n = GetSystemWindowsDirectoryW(infDir, MAX_PATH);
	_ASSERTE(0 != n);
	if (0 == n)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	success = PathAppendW(infDir, _T("INF"));
	if (!success)
	{
		return E_FAIL;
	}

	hr = StringCchCopyW(infFindSpec, MAX_PATH, infDir);
	_ASSERTE(SUCCEEDED(hr));
	if (FAILED(hr))
	{
		return hr;
	}

	success = PathAppendW(infFindSpec, FileFindSpec ? FileFindSpec : L"oem*.inf");
	_ASSERTE(success);
	if (!success)
	{
		return E_FAIL;
	}

	WIN32_FIND_DATAW findFileData = {0};

	// prevent file open error dialog from the system
	UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);

	HANDLE findFileHandle = FindFirstFileW(infFindSpec, &findFileData);
	
	if (INVALID_HANDLE_VALUE == findFileHandle) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		SetErrorMode(oldErrorMode);
		return hr;
	}

	do 
	{
		XTLTRACE1(TRACE_LEVEL_INFORMATION, 
			"OEMINF=%ls\n", findFileData.cFileName);

		hr = xDipInfFileContainsHardwareId(findFileData.cFileName, HardwareIdList);
		if (S_OK == hr)
		{
			BOOL success2 = Setupapi_SetupUninstallOEMInfW(
				findFileData.cFileName, 
				Flags, 
				NULL);

			if (Callback) 
			{
				BOOL cont = (*Callback)(
					success2 ? 0 : GetLastError(),
					infDir,
					findFileData.cFileName,
					CallbackContext);
				if (!cont) 
				{
					break;
				}
			}
		}

		success = FindNextFileW(findFileHandle, &findFileData);

	} while (success);

	FindClose(findFileHandle);
	SetErrorMode(oldErrorMode);

	return S_OK;
}
