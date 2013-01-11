#include <windows.h>
#include "resource.h"

extern HINSTANCE ghInst;

INT_PTR CALLBACK TipDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
/*
    switch (message) 
    { 
        case WM_INITDIALOG: 
            CheckDlgButton(hwndDlg, ID_ABSREL, fRelative); 
            return TRUE; 
 
        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            { 
                case IDOK: 
                    fRelative = IsDlgButtonChecked(hwndDlg, 
                        ID_ABSREL); 
                    iLine = GetDlgItemInt(hwndDlg, ID_LINE, 
                        &fError, fRelative); 
                    if (fError) 
                    { 
                        MessageBox(hwndDlg, SZINVALIDNUMBER, 
                            SZGOTOERR, MB_OK); 
                        SendDlgItemMessage(hwndDlg, ID_LINE, 
                            EM_SETSEL, 0, -1L); 
                    } 
                    else 

                    // Notify the owner window to carry out the task. 
 
                    return TRUE; 
 
                case IDCANCEL: 
                    DestroyWindow(hwndDlg); 
                    hwndGoto = NULL; 
                    return TRUE; 
            } 
    } 
*/
    return FALSE; 
} 

HWND TipDlg()
{
	HWND hWnd;
	
	hWnd = CreateDialog(ghInst, MAKEINTRESOURCE(IDD_WARNTIP), GetForegroundWindow(), TipDlgProc);
	ShowWindow(hWnd, SW_SHOW);
	return hWnd;
}
