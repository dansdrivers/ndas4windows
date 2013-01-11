#ifndef _INSTALLTIPDLG_H_
#define _INSTALLTIPDLG_H_

#include <windows.h>

namespace itipdlg {

	extern HWND hWndDlg;

	typedef struct _THREADPARAM {
		HINSTANCE hInstance;
		DWORD wLang;
	} THREADPARAM, *PTHREADPARAM;

	DWORD WINAPI InstallTipThreadProc(LPVOID lvParam);

}

#endif // _INSTALLTIPDLG_H_