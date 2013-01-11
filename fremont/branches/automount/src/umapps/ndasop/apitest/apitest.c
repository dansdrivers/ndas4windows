#include <windows.h>
#include <tchar.h>
#include <ndas/ndastypeex.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasop.h>

int __cdecl main()
{
	BOOL ret;
	NDASCOMM_CONNECTION_INFO ci;
	UINT32 nDiskCount = 1;

	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID;
	ci.Address.DeviceId.Node[0] = 0x00;
	ci.Address.DeviceId.Node[1] = 0x0b;
	ci.Address.DeviceId.Node[2] = 0xd0;
	ci.Address.DeviceId.Node[3] = 0x00;
	ci.Address.DeviceId.Node[4] = 0xa8;
	ci.Address.DeviceId.Node[5] = 0x4d;
	ci.UnitNo = 0;

	ret = NdasCommInitialize();
	if (!ret)
	{
		return GetLastError();
	}

	ret = NdasOpBind(nDiskCount, &ci, NMT_SINGLE, 0);

	if(ret != nDiskCount)
	{
		DWORD error = GetLastError();
		_tprintf(_T("Error %d (0x%08x)\n"), error, error);

		(void) NdasCommUninitialize();

		return error;
	}

	_tprintf(_T("Success\n"));

	(void) NdasCommUninitialize();

	return 0;
}
