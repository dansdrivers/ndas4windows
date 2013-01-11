#ifndef NDSETUP_H
#define NDSETUP_H

#define	LPX_NETCOMPID		_T("NKC_LPX")
#define	LPX_INF             _T("netlpx.inf")
#define	LPX_SERVICE			_T("lpx")
#define	LANSCSIBUS_INF		_T("lanscsibus.inf")
#define LANSCSIBUS_HWID		_T("Root\\LANSCSIBus")
#define	LANSCSIMINIPORT_INF _T("lanscsiminiport.inf")
#define LANSCSIMINIPORT_HWID	_T("LanscsiBus\\NetDisk_V0")

#define NDS_SUCCESS (0)
#define NDS_REBOOT_REQUIRED (1)
#define NDS_FAIL (-1)
#define NDS_INVALID_ARGUMENTS (-2)
#define NDS_PREBOOT_REQUIRED (-3)

#include "NDLog.h"

#endif
