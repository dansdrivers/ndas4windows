#pragma once
#include <windows.h>
#include <tchar.h>
#include <msiquery.h>

LPTSTR
pMsiGetProperty(
    MSIHANDLE hInstall, 
    LPCTSTR szPropertyName,
    LPDWORD pcch);
    
LPTSTR
pMsiGetSourcePath(
    MSIHANDLE hInstall, 
    LPCTSTR szFolder,
    LPDWORD pcch);
