#ifndef _REBOOTFLAG_H_
#define _REBOOTFLAG_H_

#include <windows.h>

//
// Set and clear reboot flag in the registry key.
//
#define NDSETUP_REBOOTFLAG_KEY		TEXT("Software\\XIMETA\\NetDiskSetup")
#define NDSETUP_REBOOTFLAG_VALUE	TEXT("RebootFlag")

BOOL ClearRebootFlag();
BOOL GetRebootFlag(PBOOL pbReboot);
BOOL SetRebootFlag(BOOL bReboot);

#endif // _REBOOTFLAGS_H_
