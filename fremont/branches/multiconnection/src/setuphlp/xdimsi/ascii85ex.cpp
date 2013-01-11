#include "precomp.hpp"
#include "ascii85.h"

typedef struct _ASCII85_ENCODE_CONTEXT {
	LPWSTR Buffer;
	DWORD CurrentOffset;
	DWORD BufferLength;
	HRESULT Error;
} ASCII85_ENCODE_CONTEXT, *PASCII85_ENCODE_CONTEXT;

typedef struct _ASCII85_DECODE_CONTEXT {
	LPBYTE Buffer;
	DWORD CurrentOffset;
	DWORD BufferLength;
	HRESULT Error;
} ASCII85_DECODE_CONTEXT, *PASCII85_DECODE_CONTEXT;

void 
Ascii85EncodeCallback(
	const char* buffer,
	size_t len,
	void* context)
{
	PASCII85_ENCODE_CONTEXT c = (PASCII85_ENCODE_CONTEXT) context;
	if (FAILED(c->Error)) return;
	if (c->CurrentOffset + len > c->BufferLength)
	{
		size_t s = (c->CurrentOffset + len + 1) * sizeof(WCHAR);
		PVOID p = realloc(c->Buffer, s);
		if (NULL == p)
		{
			c->Error = E_OUTOFMEMORY;
			return;
		}
		c->Buffer = (LPWSTR) p;
		c->BufferLength = c->CurrentOffset + len;
	}
	for (size_t i = 0; i < len; ++i)
	{
		c->Buffer[c->CurrentOffset++] = (WCHAR)(*(buffer+i));
	}
	c->Buffer[c->CurrentOffset] = L'\0';
}

void
Ascii85DecodeCallback(
	const unsigned char* buffer,
	size_t len,
	void* context)
{
	PASCII85_DECODE_CONTEXT c = (PASCII85_DECODE_CONTEXT) context;
	if (FAILED(c->Error)) return;
	if (c->CurrentOffset + len > c->BufferLength)
	{
		size_t s = c->CurrentOffset + len;
		PVOID p = realloc(c->Buffer, s);
		if (NULL == p)
		{
			c->Error = E_OUTOFMEMORY;
			return;
		}
		c->Buffer = (LPBYTE) p;
		c->BufferLength = c->CurrentOffset + len;
	}
	CopyMemory(
		&c->Buffer[c->CurrentOffset],
		buffer,
		len);
	c->CurrentOffset += len;
}

HRESULT
Ascii85EncodeW(
	__in PVOID Data,
	__in DWORD DataLength,
	__out LPWSTR* StringData,
	__out_opt LPDWORD StringDataLength)
{
	*StringData = NULL;
	if (StringDataLength) *StringDataLength = 0;

	ASCII85_ENCODE_CONTEXT context;
	context.Buffer = (LPWSTR) calloc(255 + 1, sizeof(WCHAR));

	if (NULL == context.Buffer)
	{
		return E_OUTOFMEMORY;
	}

	context.CurrentOffset = 0;
	context.BufferLength = 255;
	context.Error = S_OK;

	encode85(
		(unsigned char*)Data, 
		DataLength, 
		Ascii85EncodeCallback,
		&context);

	if (FAILED(context.Error))
	{
		free(context.Buffer);
	}
	else
	{
		*StringData = context.Buffer;
		if (StringDataLength) *StringDataLength = context.CurrentOffset;
	}

	return context.Error;
}

HRESULT
Ascii85DecodeW(
	__in LPCWSTR StringData,
	__out PVOID* BinaryData,
	__out_opt LPDWORD BinaryDataLength)
{
	*BinaryData = NULL;
	if (BinaryDataLength) *BinaryDataLength = 0;

	ASCII85_DECODE_CONTEXT context;
	context.Buffer = (LPBYTE) calloc(255 + 1, sizeof(WCHAR));
	if (NULL == context.Buffer)
	{
		return E_OUTOFMEMORY;
	}

	context.CurrentOffset = 0;
	context.BufferLength = 255;
	context.Error = S_OK;

	DWORD requiredBytes = WideCharToMultiByte(
		CP_ACP, 0,
		StringData, -1,
		NULL, 0,
		NULL, NULL);

	if (0 == requiredBytes)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	LPSTR str = (LPSTR) calloc(requiredBytes, sizeof(BYTE));

	if (NULL == str)
	{
		return E_OUTOFMEMORY;
	}

	requiredBytes = WideCharToMultiByte(
		CP_ACP, 0, 
		StringData, -1, 
		str, requiredBytes, 
		NULL, NULL);

	if (0 == requiredBytes)
	{
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	decode85(
		str,
		requiredBytes - 1, /* excludes the null character */ 
		Ascii85DecodeCallback,
		&context);

	if (FAILED(context.Error))
	{
		free(context.Buffer);
	}
	else
	{
		*BinaryData = context.Buffer;
		if (BinaryDataLength) *BinaryDataLength = context.CurrentOffset;
	}

	free(str);

	return context.Error;
}

