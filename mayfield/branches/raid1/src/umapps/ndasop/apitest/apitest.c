#include <windows.h>
#include <tchar.h>
#include "ndastype.h"
#include "ndasuser.h"
#include "ndasop.h"

int __cdecl main()
{
	NDAS_UNITDEVICE_ID UnitDeviceID;
	BOOL ret;
	UINT32 nDiskCount = 1;

	UnitDeviceID.DeviceId.Node[0] = 0x00;
	UnitDeviceID.DeviceId.Node[1] = 0x0b;
	UnitDeviceID.DeviceId.Node[2] = 0xd0;
	UnitDeviceID.DeviceId.Node[3] = 0x00;
	UnitDeviceID.DeviceId.Node[4] = 0xa8;
	UnitDeviceID.DeviceId.Node[5] = 0x4d;
	UnitDeviceID.UnitNo = 0;

	ret = NdasOpBind(nDiskCount, &UnitDeviceID, NMT_SINGLE);
	if(ret != nDiskCount)
		wprintf(_T("Error %d (0x%08x)\n"), GetLastError(), GetLastError());
	else
		wprintf(_T("Success\n"));


		return 0;
}
