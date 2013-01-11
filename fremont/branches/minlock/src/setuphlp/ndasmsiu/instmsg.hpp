#pragma once
#include <windows.h>
#include <tchar.h>

namespace local
{

    UINT
    GetInstanceMessageId(
        LPCTSTR szUID);

    BOOL
    EnableTokenPrivilege(
        LPCTSTR lpszSystemName, 
        BOOL bEnable = TRUE );

    BOOL
    PostInstanceMessage(
        UINT nMsg,
        WPARAM wParam, 
        LPARAM lParam);
}
