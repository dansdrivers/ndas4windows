//============================================================================================
// frhed - free hex editor

#include "precomp.h"
#include "resource.h"
#include "hexwnd.h"
#include "toolbar.h"

HINSTANCE hMainInstance;
LRESULT CALLBACK MainWndProc (HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HexWndProc (HWND, UINT, WPARAM, LPARAM);

//CF_RTF defined in Richedit.h, but we don't include it cause that would be overkill
#ifndef CF_RTF
	#define CF_RTF TEXT("Rich Text Format")
#endif
const CLIPFORMAT CF_BINARYDATA = (CLIPFORMAT)RegisterClipboardFormat("BinaryData");
const CLIPFORMAT CF_RICH_TEXT_FORMAT = (CLIPFORMAT)RegisterClipboardFormat(CF_RTF);

//--------------------------------------------------------------------------------------------
// WinMain: the starting point.
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, char* szCmdLine, int iCmdShow)
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( iCmdShow );

//Pabs inserted
#ifdef _DEBUG
//	MessageBox(NULL,"This program has been built in development/debug mode.\nTo disable this message please obtain a release version or disable line 32 of \"main.cpp\"","frhed",MB_OK);
#endif
//end

	hMainInstance = hInstance;

	// Register window class and open window.
	HACCEL hAccel;

	MSG msg;
	WNDCLASSEX wndclass;

	Zero(wndclass);
	wndclass.cbSize = sizeof(wndclass);
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = HexWndProc;
	wndclass.hInstance = hInstance;
	wndclass.hCursor = NULL;
	wndclass.lpszClassName = szHexClass;

	RegisterClassEx (&wndclass);

//Register the main window class

	wndclass.cbSize = sizeof (wndclass);
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.lpfnWndProc = MainWndProc;
	wndclass.hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON1));
	wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
	wndclass.lpszMenuName = MAKEINTRESOURCE (IDR_MAINMENU);
	wndclass.lpszClassName = szMainClass;
//Pabs changed - cast required to compile in mscv++6-stricter compliance with ansi
	wndclass.hIconSm =(HICON) LoadImage (hInstance, MAKEINTRESOURCE (IDI_ICON1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
//end

	RegisterClassEx (&wndclass);

	OleInitialize(NULL);

	hwndMain = CreateWindow (szMainClass,
		"frhed window",
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		hexwnd.iWindowX,
		hexwnd.iWindowY,
		hexwnd.iWindowWidth,
		hexwnd.iWindowHeight,
		NULL,
		NULL,
		hInstance,
		NULL);

	hwndHex = CreateWindowEx(WS_EX_CLIENTEDGE, szHexClass,
		NULL,
		WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
		10,
		10,
		100,
		100,
		hwndMain,
		NULL,
		hInstance,
		NULL);

	/*The if prevents the window from being resized to 0x0
	 it becomes just a title bar*/
	if( hexwnd.iWindowX != CW_USEDEFAULT ){
		//Prevent window creep when Taskbar is at top or left of screen
		WINDOWPLACEMENT wp;
		wp.length = sizeof(wp);
		GetWindowPlacement( hwndMain, &wp );
		wp.showCmd = hexwnd.iWindowShowCmd;
		wp.rcNormalPosition.left = hexwnd.iWindowX;
		wp.rcNormalPosition.top = hexwnd.iWindowY;
		wp.rcNormalPosition.right = hexwnd.iWindowWidth + hexwnd.iWindowX;
		wp.rcNormalPosition.bottom = hexwnd.iWindowHeight + hexwnd.iWindowY;
		SetWindowPlacement( hwndMain, &wp );
	}

	ShowWindow( hwndMain, hexwnd.iWindowShowCmd );
	UpdateWindow (hwndMain);

	if( szCmdLine != NULL && strlen( szCmdLine ) != 0 )
	{
		// Command line not empty: open a file on startup.

		// BoW Patch: Remove any " by filtering the command line
		char sz[MAX_PATH];
		char* p = szCmdLine;
		strncpy( sz, szCmdLine, sizeof( sz ) );
		DWORD dwStart = 0, dwLength = 0, dwEnd = 0; // MF cmd line parms
		TCHAR *pPathStart = NULL; // MF cmd line parms
		for( int i = 0; i < MAX_PATH; ++i )
		{
			char c = sz[i];
			if( c == 0 )
			{
				*p = 0;
				break;
			}
			else if( c != '"' )
			{
			// MF cmd line parms start again
			if (c == '/') // switch coming up
			{
				switch (sz[i + 1])
				{
					case 'S': // Start offset
					case 's':
						dwStart = strtoul(sz + i + 2, &pPathStart, 0);
						if (pPathStart)
							i = pPathStart - sz;
					break;
					case 'L': // Length of selection
					case 'l':
						dwLength = strtoul(sz + i + 2, &pPathStart, 0);
						if (pPathStart)
							i = pPathStart - sz;
					break;
					case 'E': // End of selection
					case 'e':
						dwEnd = strtoul(sz + i + 2, &pPathStart, 0);
						if (pPathStart)
							i = pPathStart - sz;
					break;
				}
			}
			else // MF cmd line parms end
				*p++ = c;
			}
		}
		if (dwLength)
			dwEnd = dwStart + dwLength - 1;
		// end of patch

		char lpszPath[MAX_PATH];
		HRESULT hres = ResolveIt( hwndMain, szCmdLine, lpszPath );
		if( SUCCEEDED( hres ) )
		{
			// Trying to open a link file: decision by user required.
			int ret = MessageBox( hwndMain,
				"You are trying to open a link file.\n"
				"Click on Yes if you want to open the file linked to,\n"
				"or click on No if you want to open the link file itself.\n"
				"Choose Cancel if you want to abort opening.",
				"frhed", MB_YESNOCANCEL | MB_ICONQUESTION );
			switch( ret )
			{
			case IDYES:
				hexwnd.load_file( lpszPath );
				break;
			case IDNO:
				hexwnd.load_file( szCmdLine );
				break;
			case IDCANCEL:
				break;
			}
		}
		else
		{
			hexwnd.load_file( szCmdLine );
		}
		if (dwEnd) hexwnd.CMD_setselection(dwStart, dwEnd);
	}

	hAccel = LoadAccelerators (hInstance, MAKEINTRESOURCE (IDR_ACCELERATOR1));

	while (GetMessage (&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator (hwndMain, hAccel, &msg))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}
	}

	OleUninitialize();

	return msg.wParam;
}

//--------------------------------------------------------------------------------------------
// The main window procedure.
LRESULT CALLBACK MainWndProc( HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	switch (iMsg) {
		case WM_CREATE:
			hwndMain = hwnd;
			InitCommonControls ();
			hwndStatusBar = CreateStatusWindow (
				CCS_BOTTOM | WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
				"Ready", hwnd, 2);
			DragAcceptFiles( hwndMain, TRUE ); // Accept files dragged into main window.
			hwndToolBar = CreateTBar(hMainInstance,hwnd,IDB_TOOLBAR);
		return 0;
		case WM_COMMAND: return SendMessage(hwndHex, iMsg, wParam, lParam);
		case WM_SETFOCUS: SetFocus(hwndHex); break;
		case WM_CLOSE: return SendMessage(hwndHex, iMsg, wParam, lParam);
		case WM_INITMENUPOPUP: hexwnd.initmenupopup( wParam, lParam ); return 0;
		case WM_DROPFILES: return SendMessage(hwndHex, iMsg, wParam, lParam);

		case WM_SIZE:{
			SendMessage(hwndStatusBar, WM_SIZE, 0 , 0); //Moves status bar back to the bottom
			SendMessage(hwndToolBar, WM_SIZE, 0 , 0); //Moves tool bar back to the top

			//--------------------------------------------
			// Set statusbar divisions.
			int statbarw;
			statbarw = LOWORD(lParam);
			// Allocate an array for holding the right edge coordinates.
			HLOCAL hloc = LocalAlloc (LHND, sizeof(int) * 3);
			int* lpParts = (int*) LocalLock(hloc);

			// Calculate the right edge coordinate for each part, and
			// copy the coordinates to the array.
			lpParts[0] = statbarw*4/6;
			lpParts[1] = statbarw*5/6;
			lpParts[2] = statbarw;

			// Tell the status window to create the window parts.

			SendMessage (hwndStatusBar, SB_SETPARTS, (WPARAM) 3, (LPARAM) lpParts);

			// Free the array, and return.
			LocalUnlock(hloc);
			LocalFree(hloc);

			RECT rect;
			GetClientRect(hwndToolBar, &rect);
			int iToolbarHeight = rect.bottom - rect.top;
			GetClientRect(hwndStatusBar, &rect);

			MoveWindow(hwndHex, 0, iToolbarHeight, LOWORD(lParam), HIWORD(lParam)-rect.bottom-iToolbarHeight, TRUE);
			break;
		}

		case WM_NOTIFY:{
			//See if someone sent us invalid data
			HWND h;
			UINT code;
			try{
				//Attempt to dereference
				NMHDR& pn = *(NMHDR*)lParam;
				h = pn.hwndFrom;
				code = pn.code;
			}
			catch(...){ return 0; }

			if( h == hwndStatusBar )
			{
				if( (code == NM_CLICK) || (code == NM_RCLICK) )
					hexwnd.status_bar_click( code == NM_CLICK );
			}
			else if( h == hwndToolBar )
			{
				if( (code == TBN_GETINFOTIPA) || (code == TBN_GETINFOTIPW) )
				{
					try{
						if(code == TBN_GETINFOTIPA){
							NMTBGETINFOTIPA& pi = *(NMTBGETINFOTIPA*) lParam;
							LoadStringA(hMainInstance,pi.iItem,pi.pszText,pi.cchTextMax);
						} else {
							NMTBGETINFOTIPW& pi = *(NMTBGETINFOTIPW*) lParam;
							LoadStringW(hMainInstance,pi.iItem,pi.pszText,pi.cchTextMax);
						}
					}
					catch(...){}
				}
			}
		}
		return 0;

		case WM_DESTROY:
			DragAcceptFiles( hwndMain, FALSE );
			PostQuitMessage(0);
		return 0;

	}
	return DefWindowProc(hwnd, iMsg, wParam, lParam );
}

// The hex window procedure.
LRESULT CALLBACK HexWndProc( HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	return hexwnd.OnWndMsg( hwnd, iMsg, wParam, lParam );
}
//============================================================================================
