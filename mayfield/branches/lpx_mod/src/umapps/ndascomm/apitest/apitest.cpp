#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "ndastype.h"
#include "ndasuser.h"
#include "ndascomm_api.h"
#include "socketlpx.h"

HMODULE _hModule = NULL;

int __cdecl main()
{
	DWORD dwError = 0;
	BOOL bRet;
	HNDAS hNdas;
	CHAR data[512 * 128];

	_hModule = LoadLibrary(L"ndascomm.dll");
	if (NULL == _hModule) {
		wprintf(L"Unable to load ndascomm.dll.\n");
		return 1;
	}

  NdasCommInitialize();

  NDAS_CONNECTION_INFO ci;
  ZeroMemory(&ci, sizeof(ci));
  ci.type = NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS;
  ci.UnitNo = 0;
  ci.bWriteAccess = FALSE;
  ci.protocol = IPPROTO_LPXTCP;
  ci.MacAddress[0] = 0x00;
  ci.MacAddress[1] = 0x0b;
  ci.MacAddress[2] = 0xd0;
  ci.MacAddress[3] = 0x00;
  ci.MacAddress[4] = 0xb8;
  ci.MacAddress[5] = 0xad;

  HNDAS hNDAS;
  hNDAS = NdasCommConnect(&ci);
  if(NULL == hNDAS)
  {
    printf("NULL == hNDAS %08X\n", ::GetLastError());
    return FALSE;
  }

  NdasCommDisconnect(hNDAS);
  hNDAS = NdasCommConnect(&ci);
  if(NULL == hNDAS)
  {
    printf("NULL == hNDAS %08X\n", ::GetLastError());
    return FALSE;
  }

  bRet = NdasCommBlockDeviceRead(hNDAS, 0, 128, data);

  NDAS_UNIT_DEVICE_DYN_INFO dynInfo;
  BOOL bResults;
  bResults = NdasCommGetUnitDeviceDynInfo(&ci, &dynInfo);
  if(FALSE == bResults)
  {
    printf("FALSE == bResults %08X\n", ::GetLastError());
    return FALSE;
  }

  printf("%d %d %d %d",
    dynInfo.iNRTargets,
    dynInfo.bPresent,
    dynInfo.NRRWHost,
    dynInfo.NRROHost);


//  hNdas = NdasRawConnectW(L"GNVXGA4MPQSVY6CSLJ7K", L"3CG70", 0x1F4A50731530EABB /* HASH_KEY_USER */, 214 /* IPPROTO_LPXTCP */);
//  ZeroMemory(data, 512*128);
//  data[0] = 0xff;
//  data[3] = 0xff;
//  NdasRawBlockDeviceWriteSafeBuffer(hNdas, 0, 1, data);
//  ZeroMemory(data, 512*128);
//  NdasRawBlockDeviceRead(hNdas, 0, 1, data);
//  NdasRawDisconnect(hNdas);



//	dwError = t1();
//	dwError = t12();
//	dwError = t2();
//	NdasRawConnectW proc = (NdasRawConnect)GetProcAddress(_hModule, "NdasRawConnectW");
//	if(!proc) {
//		wprintf(L"Unable to load function NdasRawConnectW.\n");
//		return 1;
//	}

//	dwError = proc(NULL, NULL, 0, 0);

	wprintf(_T("Error %d (0x%08x)\n"), GetLastError(), GetLastError());

	FreeLibrary(_hModule);
}
