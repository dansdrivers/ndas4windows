#pragma once
#include <windows.h>
#include <tchar.h>
#include <msiquery.h>

//
// Ad-hoc log message function
//
UINT
pMsiLogMessage(
	MSIHANDLE hInstall,
	LPCTSTR Format,
	...);

//
// Helper function to get MSI Property
//
// This function allocate the heap to *Value to
// hold enough value by calling HeapAlloc(GetProcessHeap(),...)
// Caller should free the memory using HeapFree(GetProcessHeap(),...)
// on success.
//
UINT
pMsiGetProperty(
    __in MSIHANDLE hInstall, 
    __in LPCTSTR PropertyName,
	__out LPTSTR* Value,
    __out_opt LPDWORD ValueLength);

//
// Helper function to get Source Path
//    
//
// This function allocate the heap to
// hold enough value by calling HeapAlloc(GetProcessHeap(),...)
// Caller should free the memory using HeapFree(GetProcessHeap(),...)
// for non-zero (non-NULL) returned values
//
LPTSTR
pMsiGetSourcePath(
    __in MSIHANDLE hInstall, 
    __in LPCTSTR szFolder,
    __out_opt LPDWORD pcch);
