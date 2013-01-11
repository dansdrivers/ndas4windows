#include "precomp.hpp"
//
// We are implementing ndasnif APIs, so we should make NDASNIF_LINKAGE as
// NULL (not __declspec(dllexport)). For dll exports, we are using DEF
// files.
//
#define NDASNIF_LINKAGE
#include <ndas/ndasnif.h>
#include "nifxml.h"

BOOL
WINAPI 
DllMain(
  HINSTANCE hInstance,
  DWORD dwReason,
  LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(dwReason);
	UNREFERENCED_PARAMETER(lpReserved);
	return TRUE;
}

struct BufferedBSTR2STRW {
	BufferedBSTR2STRW(WCHAR* buffer, DWORD bufferSize) : 
		buffer(buffer), bufferSize(bufferSize), next(buffer) {}
	WCHAR* operator()(const BSTR& bstr){
		const WCHAR* pwsz = bstr;
		UINT len = ::lstrlenW(pwsz);
		ATLASSERT(next + len < buffer + bufferSize); 

		WCHAR* pwszStr = next;

		::CopyMemory(next, pwsz,  len * sizeof(WCHAR));
		next += len;
		ATLASSERT(next <= buffer + bufferSize);

		*next = 0;
		next += 1;
		ATLASSERT(next <= buffer + bufferSize);

		return pwszStr;
	}
private:
	WCHAR* const buffer;
	DWORD bufferSize;
	WCHAR* next;
};

HRESULT
NdasNifImportW(
	LPCWSTR FileName, 
	LPDWORD lpEntryCount,
	NDAS_NIF_V1_ENTRYW** ppEntry)
{
	ATLENSURE_RETURN_HR(!::IsBadStringPtrW(FileName, MAX_PATH), E_POINTER);
	ATLENSURE_RETURN_HR(!::IsBadWritePtr(lpEntryCount, sizeof(DWORD)), E_POINTER);
	ATLENSURE_RETURN_HR(!::IsBadWritePtr(ppEntry, sizeof(NDAS_NIF_V1_ENTRYW*)), E_POINTER);

	NifDeviceArray darray;
	NifXmlSerializer reader;
	CComBSTR bstrFileName = FileName;
	HRESULT hr = reader.Load(bstrFileName, darray);
	if (FAILED(hr))
	{
		return hr;
	}
	
	int count = darray.GetSize();
	int cb = sizeof(NDAS_NIF_V1_ENTRYW) * count;
	for (int i = 0; i < count; ++i)
	{
		const NifDeviceEntry& e = darray[i];
		cb += (e.Name.Length() + 1) * sizeof(WCHAR);
		cb += (e.DeviceId.Length() + 1) * sizeof(WCHAR);
		cb += (e.WriteKey.Length() + 1) * sizeof(WCHAR);
		cb += (e.Description.Length() + 1) * sizeof(WCHAR);
	}

	*ppEntry = static_cast<NDAS_NIF_V1_ENTRYW*>(::LocalAlloc(LPTR, cb));
	if (NULL == *ppEntry)
	{
		return E_OUTOFMEMORY;
	}

	LPWSTR lpStr = reinterpret_cast<LPWSTR>(
		reinterpret_cast<BYTE*>(*ppEntry) + 
		sizeof(NDAS_NIF_V1_ENTRYW) * count);

	BufferedBSTR2STRW conv(lpStr, cb - sizeof(NDAS_NIF_V1_ENTRYW) * count);

	for (int i = 0; i < count; ++i)
	{
		NDAS_NIF_V1_ENTRYW& entry = *ppEntry[i];
		const NifDeviceEntry& arrayEntry = darray[i];
		entry.Flags = 0;
		entry.Name = conv(arrayEntry.Name);
		entry.DeviceId = conv(arrayEntry.DeviceId);
		entry.WriteKey = conv(arrayEntry.WriteKey);
		entry.Description = conv(arrayEntry.Description);
	}

	*lpEntryCount = count;

	return S_OK;
}

HRESULT
WINAPI
NdasNifExportW(
	LPCWSTR FileName,
	DWORD EntryCount,
	const NDAS_NIF_V1_ENTRYW* pEntry)
{
	ATLENSURE_RETURN_HR(!::IsBadStringPtrW(FileName, UINT_PTR(-1)), E_POINTER);
	ATLENSURE_RETURN_HR(!::IsBadReadPtr(pEntry, sizeof(NDAS_NIF_V1_ENTRYW) * EntryCount), E_POINTER);
	
	NifDeviceArray darray;
	for (DWORD i = 0; i < EntryCount; ++i)
	{
		NifDeviceEntry arrEntry;

		CComBSTR bstrName(pEntry[i].Name);
		HRESULT hr = bstrName.CopyTo(&arrEntry.Name);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		CComBSTR bstrDeviceId(pEntry[i].DeviceId);
		hr = bstrDeviceId.CopyTo(&arrEntry.DeviceId);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		CComBSTR bstrWriteKey(pEntry[i].WriteKey);
		hr = bstrWriteKey.CopyTo(&arrEntry.WriteKey);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		CComBSTR bstrDescription(pEntry[i].Description);
		hr = bstrDescription.CopyTo(&arrEntry.Description);
		ATLENSURE_RETURN_HR(SUCCEEDED(hr), hr);

		darray.Add(arrEntry);
	}

	CComBSTR bstrFileName = FileName;
	NifXmlSerializer writer;
	HRESULT hr = writer.Save(bstrFileName, darray);
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}

