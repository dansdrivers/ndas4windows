#include "precomp.h"

//============================================================================================

#define NumBx(var) NumBox( #var , var, "NumBox" )
#define NBx(capt, var) NumBox( #var , var, capt )
void NumBox (char* varname, int x, char* caption)
{
	char buf[100];
	sprintf (buf, "%s = %d = 0x%x", varname, x, x);
	MessageBox (NULL, buf, caption, MB_OK);
}

#define TxtBx(var) TxtBox( #var , var)
void TxtBox (char* varname, char* s)
{
	char buf[100];
	sprintf (buf, "%s = %s", varname, s);
	MessageBox (NULL, buf, "TxtBox", MB_OK);
}

#define ChrBx(var) CharBox( #var , var)
void CharBox (char* varname, char c)
{
	char buf[100];
	sprintf (buf, "%s = %c", varname, c);
	MessageBox (NULL, buf, "CharBox", MB_OK);
}

//Pabs changed - mutated TextToClipboard into two functions
void TextToClipboard( char* pcText, int len, HWND hwnd );
void TextToClipboard( char* pcText, HWND hwnd )
{
	int len = 1 + strlen( pcText );
	TextToClipboard(pcText,len,hwnd);
}

void TextToClipboard( char* pcText, int len, HWND hwnd )
{
	// Changed for pabs's patch to compare files command.
	HGLOBAL hGlobal = GlobalAlloc( GHND, len );
	if( hGlobal != NULL )
	{
		SetCursor (LoadCursor (NULL, IDC_WAIT));//tell user to wait
		char* pd = (char*) GlobalLock (hGlobal);// get pointer to clip data
		if( pd )
		{
			//succesfuly got pointer
			strcpy( pd, pcText );//copy Text into global mem
			GlobalUnlock (hGlobal);//unlock global mem
			if(OpenClipboard(hwnd))
			{
				// open clip
				EmptyClipboard(); //empty clip
				SetClipboardData (CF_TEXT, hGlobal);//copy to clip
				CloseClipboard (); //close clip
			}
			else //failed to open clip
				MessageBox (hwnd,"Cannot get access to clipboard.","Copy",MB_OK | MB_ICONERROR);
		}
		else
		{//failed to get pointer to global mem
			GlobalFree(hGlobal);
			MessageBox (hwnd,"Cannot lock clipboard.","Copy",MB_OK | MB_ICONERROR);
		}
		SetCursor (LoadCursor (NULL, IDC_ARROW));//user can stop waiting
	}
	else// failed to allocate global mem
		MessageBox (hwnd, "Not enough memory for copying.", "Copy", MB_OK | MB_ICONERROR);
}

// #define PRINT_NUMBER( buf, var ) sprintf( buf, #var " = %d = 0x%x", x, x )

