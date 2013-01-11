#ifndef _ACTIVATEWARNDLG_H_
#define _ACTIVATEWARNDLG_H_

#include <windows.h>

namespace awdlgproc {
	
	extern volatile LONG lThreadCount;

	DWORD WINAPI ActivateWarnDlgProc(LPVOID lvParam);
}

#endif // _ACTIVATEWARNDLG_H_