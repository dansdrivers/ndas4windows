#include "InstallTipDlg.h"

#include "MultilangRes.h"
#include "NDSetup.h"

#include "resource.h"

namespace itipdlg {

	HWND hWndDlg = NULL;
	HINSTANCE hInst = NULL;

	//++
	//
	// InstallTipDlgProc
	// InstallTipThreadProc
	//
	// A helper thread worker function for activating
	// a "Digital Signature Warning" dialg.
	//
	DWORD WINAPI InstallTipThreadProc(LPVOID lvParam);
	INT_PTR CALLBACK InstallTipDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL PlaceWindowLowerRight(HWND hwndChild, HWND hwndParent);
	BOOL ResizeWindowToBitmap(HWND hWnd, HBITMAP hBitmap);

	BOOL PlaceWindowLowerRight(HWND hwndChild, HWND hwndParent)
	{
	   RECT    rChild, rParent, rWorkArea;
	   int     wChild, hChild, wParent, hParent;
	   int     xNew, yNew;
	   BOOL  bResult;

	   // Get the Height and Width of the child window
	   GetWindowRect (hwndChild, &rChild);
	   wChild = rChild.right - rChild.left;
	   hChild = rChild.bottom - rChild.top;

	   // Get the Height and Width of the parent window
	   GetWindowRect(hwndParent, &rParent);
	   wParent = rParent.right - rParent.left;
	   hParent = rParent.bottom - rParent.top;

	   // Get the limits of the 'workarea'
	   bResult = SystemParametersInfo(
		  SPI_GETWORKAREA,  // system parameter to query or set
		  sizeof(RECT),
		  &rWorkArea,
		  0);
	   if (!bResult)
	   {
		  rWorkArea.left = rWorkArea.top = 0;
		  rWorkArea.right = GetSystemMetrics(SM_CXSCREEN);
		  rWorkArea.bottom = GetSystemMetrics(SM_CYSCREEN);
	   }

	   // Calculate new X position, then adjust for workarea
	   // xNew = rParent.left + ((wParent - wChild) /2);
	   xNew = rParent.left + (wParent - wChild);
	   if (xNew < rWorkArea.left)
	   {
		  xNew = rWorkArea.left;
	   }
	   else if ((xNew+wChild) > rWorkArea.right) 
	   {
		  xNew = rWorkArea.right - wChild;
	   }

	   // Calculate new Y position, then adjust for workarea
	   yNew = rParent.top  + (hParent - hChild);
	   if (yNew < rWorkArea.top) 
	   {
		  yNew = rWorkArea.top;
	   }
	   else if ((yNew+hChild) > rWorkArea.bottom) 
	   {
		  yNew = rWorkArea.bottom - hChild;
	   }

	   // Set it, and return
	   return SetWindowPos (hwndChild, NULL, xNew, yNew, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}

	BOOL ResizeWindowToBitmap(HWND hWnd, HBITMAP hBitmap)
	{
		BITMAP bitmap;
		int cx, cy;
		GetObject(hBitmap, sizeof(BITMAP), &bitmap);
		cx = bitmap.bmWidth;
		cy = bitmap.bmHeight;
		return MoveWindow(hWnd, 0, 0, cx, cy, TRUE);
	}

	INT_PTR CALLBACK InstallTipDlgProc(HWND hWndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
	{
		static HBITMAP hBitmap = NULL;

		UNREFERENCED_PARAMETER(hWndDlg);
		UNREFERENCED_PARAMETER(message);
		UNREFERENCED_PARAMETER(wParam);
		
		switch (message)
		{
		case WM_INITDIALOG:
			{
				LANGID langID;
				langID = *((LANGID*)lParam);

				int offset = GetLanguageResourceOffset(langID);
				TCHAR szText[1024];
				LoadString(hInst, IDS_TIP_TEXT + offset, szText, 1024);
				SetWindowText(GetDlgItem(hWndDlg, IDC_TIP_TEXT), szText);
				LoadString(hInst, IDS_TIP_TITLE + offset, szText, 1024);
				SetWindowText(hWndDlg, szText);
				//SetWindowText(GetDlgItem(
				//ResizeWindowToBitmap(hWndDlg, hBitmap);
				// DeleteObject(hBitmap);
				PlaceWindowLowerRight(hWndDlg, GetDesktopWindow());
	#if defined OEM_IOMEGA || OEM_LOGITEC
				// ShowWindow(hWndDlg, SW_HIDE);
				MoveWindow(hWndDlg, 0, 0, 0, 0, TRUE);
	#endif
				return TRUE;
			}
		case WM_CLOSE:
			{
				if (NULL != hBitmap)
					DeleteObject(hBitmap);
				DestroyWindow(hWndDlg);
				return TRUE;
			}
		}

		return FALSE;
	}

	DWORD WINAPI InstallTipThreadProc(LPVOID lvParam)
	{
		HWND	hWnd;
		MSG		msg;
		PTHREADPARAM pThParam;

		DebugPrintf(TEXT("Entering InstallTipThreadProc()...\n"));

		pThParam = (PTHREADPARAM) lvParam;
		hInst = pThParam->hInstance;

		hWnd = CreateDialogParam(
			pThParam->hInstance, 
			MAKEINTRESOURCE(IDD_WARNTIP), 
			GetForegroundWindow(), 
			InstallTipDlgProc,
			(LPARAM) &(pThParam->wLang)
			);

		ShowWindow(hWnd, SW_SHOW);
		
		hWndDlg = hWnd;

		while (GetMessage(&msg, hWnd, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		hWndDlg = NULL;

		DebugPrintf(TEXT("Leaving InstallTipThreadProc()...\n"));

		return ERROR_SUCCESS;
	}

}
