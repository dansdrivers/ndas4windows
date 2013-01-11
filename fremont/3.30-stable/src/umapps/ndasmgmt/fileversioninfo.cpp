#include "stdafx.h"
#include "fileversioninfo.h"

#define ALIGN_DOWN(length, type) \
	((ULONG)(length) & ~(sizeof(type) - 1))

#define ALIGN_UP(length, type) \
	(ALIGN_DOWN(((ULONG)(length) + sizeof(type) - 1), type))

HRESULT
FvReadFileVersionFixedFileInfoByHandle(
	__in_opt HMODULE ModuleHandle,
	__out VS_FIXEDFILEINFO* FixedFileInfo)
{
	HRESULT hr;

	// Does not support very long file name at this time
	TCHAR moduleFileName[MAX_PATH] = {0};
	DWORD n = GetModuleFileName(ModuleHandle, moduleFileName, MAX_PATH);
	_ASSERTE(0 != n && MAX_PATH != n);
	if (0 == n || n >= MAX_PATH) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}
	return FvReadFileVersionFixedFileInfo(moduleFileName, FixedFileInfo);
}

HRESULT
FvReadFileVersionFixedFileInfo(
	__in LPCTSTR ModuleFileName,
	__out VS_FIXEDFILEINFO* FixedFileInfo)
{
	CHeapPtr<void> fileVersionInfo;

	HRESULT hr = FvReadFileVersionInfo(ModuleFileName, &fileVersionInfo);
	if (FAILED(hr))
	{
		ATLTRACE("FvGetFileVersionInfo failed, hr=0x%X\n", hr);
		return hr;
	}

	const VS_FIXEDFILEINFO* info = FvGetFixedFileInfo(fileVersionInfo);

	if (NULL == info)
	{
		ATLTRACE("VS_FIXEDFILEINFO is not available.\n", hr);
		return E_FAIL;
	}

	*FixedFileInfo = *info;

	return S_OK;
}

HRESULT
FvGetFileVersionTranslations(
	__in LPCVOID FileVersionInfo,
	__out LPDWORD TranslationCount,
	__out const LANGCODEPAGE** TranslationArray)
{
	*TranslationCount = 0;
	*TranslationArray = NULL;

	UINT byteLength;
	LPCVOID data = NULL;
	BOOL success = VerQueryValue(
		FileVersionInfo, 
		_T("\\VarFileInfo\\Translation"), 
		(LPVOID*) &data, 
		&byteLength);
	if (!success)
	{
		return E_FAIL;
	}

	DWORD count = byteLength / sizeof(LANGCODEPAGE);

	ATLTRACE(" FileVersionTranslation: count=%d\n", count);

	*TranslationCount = count;
	*TranslationArray = reinterpret_cast<const LANGCODEPAGE*>(data);

#if _DEBUG
	for (DWORD i = 0; i < count; ++i)
	{
		ATLTRACE(" Translation: LangId=%u(0x%04X), CodePage=%u(0x%04X)\n",
			(*TranslationArray)[i].Language,
			(*TranslationArray)[i].Language,
			(*TranslationArray)[i].CodePage,
			(*TranslationArray)[i].CodePage);
	}
#endif

	return S_OK;
}

HRESULT
FvGetFileVersionInformationString(
	__in LPCVOID FileVersionInfo,
	__in LPCTSTR InformationName,
	__out LPCTSTR* Information,
	__out PUINT InformationLength)
{
	DWORD count;
	const LANGCODEPAGE* translations;
	HRESULT hr = FvGetFileVersionTranslations(
		FileVersionInfo, &count, &translations);
	if (FAILED(hr))
	{
		ATLTRACE("GetFileVersionTranslations failed, hr=0x%X\n", hr);
		return hr;
	}
	if (count < 1)
	{
		return E_FAIL;
	}

	TCHAR blockName[128];

	hr = StringCchPrintf(
		blockName, RTL_NUMBER_OF(blockName),
		_T("\\StringFileInfo\\%04x%04x\\%s"),
		translations[0].Language,
		translations[0].CodePage,
		InformationName);

	if (FAILED(hr))
	{
		ATLASSERT(FALSE && "BlockName buffer is too small");
		return hr;
	}

	BOOL success = VerQueryValue(
		FileVersionInfo, blockName, 
		(LPVOID*) Information, 
		InformationLength);

	if (!success)
	{
		// VerQueryValue may not set the last error
		ATLTRACE("VerQueryValue failed, error=0x%X\n", GetLastError());
		return E_FAIL;
	}

	return S_OK;
}

CONST VS_FIXEDFILEINFO* 
FvGetFixedFileInfo(
	__in LPCVOID FileVersionInfo)
{
	UINT length;

	VS_FIXEDFILEINFO* fixedFileInfo = NULL;

	BOOL success = VerQueryValue(
		FileVersionInfo, _T("\\"), (LPVOID*)&fixedFileInfo, &length);

	if (!success || sizeof(VS_FIXEDFILEINFO) != length) 
	{
		return NULL;
	}
	return fixedFileInfo;
}

HRESULT
FvReadFileVersionInfo(
	__in LPCTSTR FileName,
	__deref_out PVOID* FileVersionInfo)
{
	HRESULT hr;

	ATLASSERT(NULL != FileVersionInfo);
	*FileVersionInfo = NULL;

	DWORD fileVersionHandle = 0;
	DWORD fileVersionInfoSize = GetFileVersionInfoSize(FileName, &fileVersionHandle);
	if (0 == fileVersionInfoSize) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		ATLTRACE("GetFileVersionInfoSize failed, hr=0x%X\n", hr);
		return hr;
	}

	CHeapPtr<void> fileVersionInfo;

	if (!fileVersionInfo.AllocateBytes(fileVersionInfoSize))
	{
		return E_OUTOFMEMORY;
	}

	BOOL success = GetFileVersionInfo(
		FileName, 
		fileVersionHandle, 
		fileVersionInfoSize, 
		fileVersionInfo);

	if (!success) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		ATLTRACE("GetFileVersionInfo failed, hr=0x%X\n", hr);
		return hr;
	}

	*FileVersionInfo = fileVersionInfo.Detach();

	return S_OK;
}

HRESULT
FvReadFileVersionInfoByHandle(
	__in_opt HMODULE ModuleHandle,
	__deref_out PVOID* FileVersionInfo)
{
	HRESULT hr;

	// Does not support very long file name at this time
	TCHAR moduleFileName[MAX_PATH] = {0};
	DWORD n = GetModuleFileName(ModuleHandle, moduleFileName, MAX_PATH);
	_ASSERTE(0 != n && MAX_PATH != n);

	if (0 == n || n >= MAX_PATH) 
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	hr = FvReadFileVersionInfo(moduleFileName, FileVersionInfo);

	return hr;
}

