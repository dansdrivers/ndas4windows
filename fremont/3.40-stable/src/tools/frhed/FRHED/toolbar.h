#ifndef toolbar_h
#define toolbar_h

LRESULT SetTooltipText(HINSTANCE hInst, HWND hWnd, LPARAM lParam);
void EnableDriveButtons(HWND hWndToolBar, BOOL bEnable);
HWND CreateTBar(HINSTANCE hInst, HWND hWnd, UINT idBitmap);

#endif // toolbar_h
