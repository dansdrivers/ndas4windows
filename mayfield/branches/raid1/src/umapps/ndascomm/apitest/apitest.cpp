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

  NdasCommInitialize();

  NDAS_CONNECTION_INFO ci;
  ZeroMemory(&ci, sizeof(ci));
  ci.type = NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS;
  ci.UnitNo = 0;
  ci.bWriteAccess = TRUE;
  ci.protocol = IPPROTO_LPXTCP;
  ci.MacAddress[0] = 0x00;
  ci.MacAddress[1] = 0x0b;
  ci.MacAddress[2] = 0xd0;
  ci.MacAddress[3] = 0x01;
  ci.MacAddress[4] = 0x72;
  ci.MacAddress[5] = 0xfa;

  HNDAS hNDAS;
  hNDAS = NdasCommConnect(&ci);
  if(NULL == hNDAS)
  {
    printf("NULL == hNDAS %08X\n", ::GetLastError());
    return FALSE;
  }


  UINT64 logicalAddress = 0;
  while(1)
  {
	  bRet = NdasCommBlockDeviceRead(hNDAS, logicalAddress, 128, data);
	  if(!bRet)
		  break;

	  logicalAddress += 128;
  }

  bRet = NdasCommDisconnect(hNDAS);

  return bRet;
}
