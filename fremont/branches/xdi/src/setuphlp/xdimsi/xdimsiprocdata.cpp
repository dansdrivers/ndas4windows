#include "precomp.hpp"
#include "xdimsiprocdata.h"
#include "ascii85ex.h"

#ifndef ALIGN_DOWN 
#define ALIGN_DOWN(length, type) \
	((ULONG)(length) & ~(sizeof(type) - 1))
#endif

#ifndef ALIGN_UP
#define ALIGN_UP(length, type) \
	(ALIGN_DOWN(((ULONG)(length) + sizeof(type) - 1), type))
#endif

void
inline
pxDiMsiProcessDataFillIn(
	__in PXDIMSI_PROCESS_DATA ProcessData,
	__inout LPDWORD NextOffset,
	__inout LPDWORD LengthField,
	__inout LPDWORD OffsetField,
	__in LPCWSTR Data,
	__in DWORD DataLength)
{
	*LengthField = DataLength;
	*OffsetField = *NextOffset;
	*NextOffset += ALIGN_UP(sizeof(WCHAR) * (DataLength + 1), LONGLONG);
	CopyMemory(
		pxOffsetOf(ProcessData, *OffsetField),
		Data,
		DataLength * sizeof(WCHAR));
}

HRESULT
pxDiMsiProcessDataCreate(
	__inout PXDIMSI_PROCESS_DATA* ProcessData,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	*ProcessData = NULL;

	DWORD hardwareIdLength = lstrlenW(ProcessRecord->HardwareId);
	DWORD infPathLength = lstrlenW(ProcessRecord->InfPath);
	DWORD regKeyLength = lstrlenW(ProcessRecord->RegKey);
	DWORD regNameLength = lstrlenW(ProcessRecord->RegName);
	DWORD actionLength = lstrlenW(ProcessRecord->ActionName);

	DWORD msize = FIELD_OFFSET(XDIMSI_PROCESS_DATA, AdditionalData) +
		ALIGN_UP(sizeof(WCHAR) * (hardwareIdLength + 1), LONGLONG) +
		ALIGN_UP(sizeof(WCHAR) * (infPathLength + 1), LONGLONG) +
		ALIGN_UP(sizeof(WCHAR) * (regKeyLength + 1), LONGLONG) +
		ALIGN_UP(sizeof(WCHAR) * (regNameLength + 1), LONGLONG) +
		ALIGN_UP(sizeof(WCHAR) * (actionLength + 1), LONGLONG);

	PXDIMSI_PROCESS_DATA px = (PXDIMSI_PROCESS_DATA) calloc(msize, sizeof(BYTE));
	if (NULL == px)
	{
		return E_OUTOFMEMORY;
	}

	px->Version = sizeof(XDIMSI_PROCESS_DATA);
	px->Size = msize;
	px->ProcessType = ProcessRecord->ProcessType;
	px->ErrorNumber = ProcessRecord->ErrorNumber;
	px->Flags = ProcessRecord->Flags;
	px->ProgressTicks = ProcessRecord->ProgressTicks;

	ULONG nextOffset = FIELD_OFFSET(XDIMSI_PROCESS_DATA, AdditionalData);

	pxDiMsiProcessDataFillIn(
		px, 
		&nextOffset, 
		&px->HardwareIdLength,
		&px->HardwareIdOffset,
		ProcessRecord->HardwareId,
		hardwareIdLength);

	pxDiMsiProcessDataFillIn(
		px, 
		&nextOffset, 
		&px->InfPathLength,
		&px->InfPathOffset,
		ProcessRecord->InfPath,
		infPathLength);

	px->RegRoot = ProcessRecord->RegRoot;

	pxDiMsiProcessDataFillIn(
		px, 
		&nextOffset, 
		&px->RegKeyLength,
		&px->RegKeyOffset,
		ProcessRecord->RegKey,
		regKeyLength);

	pxDiMsiProcessDataFillIn(
		px, 
		&nextOffset, 
		&px->RegNameLength,
		&px->RegNameOffset,
		ProcessRecord->RegName,
		regNameLength);

	pxDiMsiProcessDataFillIn(
		px, 
		&nextOffset, 
		&px->ActionLength,
		&px->ActionOffset,
		ProcessRecord->ActionName,
		actionLength);

	*ProcessData = px;

	return S_OK;
}

HRESULT
pxDiMsiProcessDataFree(
	__in PXDIMSI_PROCESS_DATA ProcessData)
{
	free(ProcessData);
	return S_OK;
}

HRESULT
pxDiMsiProcessDataEncode(
	__in PXDIMSI_PROCESS_DATA ProcessData,
	__out LPWSTR* StringData,
	__out_opt LPDWORD StringDataLength)
{
	return Ascii85EncodeW(
		ProcessData,
		ProcessData->Size,
		StringData,
		StringDataLength);
}

HRESULT
pxDiMsiProcessDataDecode(
	__in LPCWSTR StringData,
	__out PXDIMSI_PROCESS_DATA* ProcessData,
	__out_opt LPDWORD OutputSize)
{
	return Ascii85DecodeW(
		StringData,
		(PVOID*) ProcessData,
		OutputSize);
}
