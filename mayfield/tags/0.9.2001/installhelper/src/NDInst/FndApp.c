// fndapp.cpp : Defines the entry point for the DLL application.
//
#include <windows.h>
#include "FndApp.h"

BOOL APIENTRY FindInstanceA(LPCSTR lpszUID)
{
	HANDLE hMutex  = OpenMutexA( READ_CONTROL, FALSE, lpszUID );
			
	if( hMutex != NULL )
	{
		// instance of the application is running:
		CloseHandle(hMutex);
		return TRUE;
	}

	return FALSE;
}

BOOL APIENTRY FindInstanceW(LPCWSTR lpszUID)
{
	HANDLE hMutex  = OpenMutexW( READ_CONTROL, FALSE, lpszUID );
			
	if( hMutex != NULL )
	{
		CloseHandle(hMutex);
		return TRUE;
	}

	return FALSE;
}
