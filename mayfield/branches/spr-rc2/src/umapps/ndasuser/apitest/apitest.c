#include <windows.h>
#include <tchar.h>
#include "ndastype.h"
#include "ndasuser.h"

HMODULE _hModule = NULL;

#define NDASUSER_API __declspec(dllimport)

typedef BOOL (*NdasValidateStringIdKeyWProc)(LPCWSTR, LPCWSTR);
typedef BOOL (*ConvertStringIdToDeviceIdProc)(NDAS_DEVICE_STRING_IDW, PNDAS_DEVICE_ID);
//typedef BOOL (CALLBACK* NDASDEVICEENUMPROC)(PNDAS_DEVICE_ENTRY lpEnumEntry, LPARAM lParam);
typedef BOOL (*NdasRegisterDeviceWProc)(LPCWSTR,LPCWSTR,LPCWSTR);
typedef BOOL (*NdasEnumDevicesWProcProc)(NDASDEVICEENUMPROC,LPARAM);

CALLBACK DeviceEnumProc(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, LPVOID lpContext)
{
	static int i = 0;
	wprintf(L"Found %d\n", i++);
	wprintf(L"DeviceId: %%s\n", lpEnumEntry->szDeviceStringId);
	wprintf(L"Name: %s\n",	lpEnumEntry->szDeviceName);
	return TRUE;
}

typedef struct _ID_TEST_DATA {
	LPCWSTR lpszId;
	LPCWSTR lpszKey;
	BOOL bExpectedResult;
} ID_TEST_DATA;

DWORD t4()
{
	ID_TEST_DATA data[] = {
		{ L"LUR6XCK8LSRTFHMR6KC",L"H0GA2",FALSE},
		{ L"LUR6XCK8LSRTFHMR6KCK",L"H0GA2",TRUE},
		{ L"LUR6XCK8LSRTFHMR6KCK",L"H0GA1",FALSE},
		{ L"LUR6XCK8LSRTFHMR6KCK",NULL,TRUE},
		{ L"LUR6XCK8LSRTFHMR6KC2",L"H0GA2",FALSE},
		{ L"LUR6XCK8LSRTFHMR6KCK",NULL,TRUE},
		{ L"LUR6XCK8LSRTFHMR6KC",NULL,FALSE},
		{ NULL,NULL,FALSE},
		{ L"2C1PXFVKM2N79PGNHFMG",NULL,TRUE},
		{ L"2C1PXFVKM2N79PGNHFMG",L"FV8JK",TRUE},
		{ NULL,L"FV8JK",FALSE},
		{ L"2C1PXFVKM2N79PGNHFMG",L"#$%$#",FALSE},
		{ L"2CL04L5ALSB7M18YKQ2X",L"2XNMX",TRUE},
		{ L"    2C1PXFVKM2N79PG",L"FV8JKFF",FALSE}
	};

	ID_TEST_DATA* pDatum;
	DWORD nData;
	DWORD i;
	BOOL bResult;
	NdasValidateStringIdKeyWProc proc;

	nData = sizeof(data) / sizeof(data[0]);

	proc = (NdasValidateStringIdKeyWProc) GetProcAddress(_hModule, "NdasValidateStringIdKeyW");

	if (proc) {
		for (pDatum = data, i = 0; i < nData; ++i, ++pDatum) {
			bResult = proc(pDatum->lpszId,pDatum->lpszKey);
			wprintf(L"Testing %s - %s: ", pDatum->lpszId, pDatum->lpszKey);
			if (bResult != pDatum->bExpectedResult) {
				wprintf(L"returned %d, expected %d, error!", bResult, pDatum->bExpectedResult);
				DebugBreak();
			} else {
				wprintf(L"returned %d, expected %d, success!", bResult, pDatum->bExpectedResult);
			}
			wprintf(L"\n");
		}
		
	}

	return 0;
}
/* NDAS_DEVICE_ID is never exposed to the public APIs */
/*
DWORD t3()
{
	NDAS_DEVICE_ID deviceId;
	BOOL f;
	ConvertStringIdToDeviceIdProc fn;
	LPWSTR ids = L"LUR6XCK8LSRTFHMR6KCK";

	fn = (ConvertStringIdToDeviceIdProc) GetProcAddress(_hModule, "ConvertStringIdToDeviceIdW");
	if (fn) {
		f = fn(ids, &deviceId);
		if (f) {
			wprintf(L"DeviceId %02X:%02X:%02X-%02X:%02X:%02X\n", 
				deviceId.Node[0], deviceId.Node[1], deviceId.Node[2],
				deviceId.Node[3], deviceId.Node[4], deviceId.Node[5]);
		} else {
			wprintf(L"Invalid Device String Id!\n");
		}
	}
	return 0;
}
*/
DWORD t2()
{
	NdasEnumDevicesWProcProc fn = (NdasEnumDevicesWProcProc)GetProcAddress(_hModule,"NdasEnumDevicesW");
	if (fn) {
		if (fn(DeviceEnumProc,(LPARAM)NULL)) {
			return 0;
		}
	}
	return GetLastError();
	
}

DWORD t1()
{
	NdasRegisterDeviceWProc fn = (NdasRegisterDeviceWProc)GetProcAddress(_hModule, "NdasRegisterDeviceW");
	if (fn) {
		// NdasDeviceRegisterW(L"A", L"B");
		BOOL fSuccess = fn(L"LUR6XCK8LSRTFHMR6KCK", NULL, L"MYNAME");
		wprintf(TEXT("Register Device returned %d\n"), fSuccess);
	} else {
		MessageBox(NULL, TEXT("No NdasRegisterDeviceW function"), TEXT(""), MB_OK);
	}
	return 0;
}

DWORD t12()
{
	NdasRegisterDeviceWProc fn = (NdasRegisterDeviceWProc)GetProcAddress(_hModule, "NdasRegisterDeviceW");
	if (fn) {
		// NdasDeviceRegisterW(L"A", L"B");
		fn(L"2C1PXFVKM2N79PGNHFMG", NULL, L"MY  NAME");
	} else {
		MessageBox(NULL, TEXT("No NdasRegisterDeviceW function"), TEXT(""), MB_OK);
	}
	return 0;
}

int __cdecl main()
{
	DWORD dwError = 0;

	_hModule = LoadLibrary(L"ndasuser.dll");
	if (NULL == _hModule) {
		wprintf(L"Unable to load ndasuser.dll.\n");
		return 1;
	}

//	dwError = t1();
//	dwError = t12();
	dwError = t2();
	wprintf(_T("Error %d (0x%08x)\n"), GetLastError(), GetLastError());

	FreeLibrary(_hModule);
}
