//============================================================================================
// Frhed main definition file.
#include "precomp.h"
#include <shlwapi.h>
#include <ctype.h>
#include "toolbar.h"
#include "resource.h"
#include "hexwnd.h"
#include "gktools.h"
#include "PhysicalDrive.h"
#include "PMemoryBlock.h"
#include "simparr.cpp"
#include "BinTrans.cpp"
#include <scrc32.h>

/*In the following headers:
	ULONG m_cRefCount; //The reference count that all objects based on IUnknown must have
	//The following have to do with the automatic destruction IUnknown::Release is supposed to do
	bool deleteself; //Should we delete ourself on zero reference count
	C<SomeObject>** pthis; //A pointer to a pointer that we should set to NULL on destruction*/
#include "idt.h"
#include "ids.h"
#include "ido.h"

BOOL ShowHtmlHelp( UINT uCommand, DWORD dwData, HWND hParentWindow );

//Pabs inserted
//When in use HexEditorWindow::paint will use a memory dc to prevent flicker
//is a little buggy - when paint is called with iUpdateLine != -1 the display fucks up a bit
//This is because the memdcs are a pain in the arse as they are not initialised with the data in the screendc
//Ways to fix
// 1 = bitblt from screen to memdc before painting - this is way too sssllllooooowwwww - if U press an arrow key the cursor moves half a second later
// 2 = always repaint entire screen - not nearly as slow but is inefficient
// 3 = (impossible) select the bitmap out of a screen dc, into a memdc, paint & select the bitmap back into the screen dc
// - this won't work because SelectObject will not select bitmaps into a screen dc
//#define USEMEMDC
#ifdef USEMEMDC
	//#define BITBLT_TOMDC //#1
	#ifndef BITBLT_TOMDC
		#define UPDATE_ALL_LINES //#2
	#endif
#endif
//end

IPhysicalDrive* Drive = NULL;
IPhysicalDrive* PhysicalDrive = NULL;
IPhysicalDrive* NDASDrive = NULL;
PMemoryBlock Track;
INT64 CurrentSectorNumber = 0;

//--------------------------------------------------------------------------------------------
// Functions to find out which instance of frhed is being started right now.
int g_iInstCount;
//Pabs inserted
//used for several things
//1. The range for the spinners in ChangeInstProc
//2. Which instance to read/write to/from
int iLoadInst = 0;//1.high 2.read
int iSaveInst = 0;//1.low 2.write
//Note g_iInstCount is the initial value for both these spinners
//end

// String containing data to replace.
SimpleString strToReplaceData;
// String containing data to replace with.
SimpleString strReplaceWithData;

SimpleString TxtEditName;
char *pcGotoDlgBuffer;
int iGotoDlgBufLen;
int bOpenReadOnlySetting;//Pabs inserted ", bShowFileStatsPL"
unsigned int iStartPL, iNumBytesPl, iPLFileLen, bShowFileStatsPL;
bookmark *pbmkRemove;
int iRemBmk;
int iBmkOffset;

char pcBmkTxt[BMKTEXTMAX];
int iFindDlgBufLen, iFindDlgMatchCase, iFindDlgDirection, iFindDlgLastLen;

//GK16AUG2K: additional options for the find dialog
int iFindDlgUnicode = 0;
char *pcFindDlgBuffer;
//Pabs changed "iCopyHexdumpType = IDC_EXPORTDISPLAY" inserted
int iCopyHexdumpDlgStart, iCopyHexdumpDlgEnd, iCopyHexdumpMode = BST_CHECKED, iCopyHexdumpType = IDC_EXPORTDISPLAY;
//end
int iDecValDlgOffset, iDecValDlgValue, iDecValDlgSize, iDecValDlgTimes;
//Pabs changed " = 1, iPasteSkip" inserted
int iPasteMode, iPasteMaxTxtLen, iPasteTimes = 1, iPasteSkip;
//end
char *pcPasteText;
int iCutOffset, iCutNumberOfBytes, iCutMode = BST_CHECKED;
int iCopyStartOffset, iCopyEndOffset;//Pabs inserted ", bAutoOLSetting"
int iAutomaticXAdjust = BST_CHECKED, iBPLSetting, iOffsetLenSetting, bAutoOLSetting;
int iAppendbytes;
int iManipPos;
unsigned char cBitValue;
int iCharacterSetting, iFontSizeSetting;
int bUnsignedViewSetting;
char szFileName[_MAX_PATH];
int iDestFileLen, iSrcFileLen;
intpair* pdiffChoice;
int iDiffNum;
int iBinaryModeSetting;
int iStartOfSelSetting, iEndOfSelSetting;
int iPasteAsText;
char* pcTmplText;
SimpleString BrowserName;

HWND hwndMain, hwndHex, hwndStatusBar, hwndToolBar;
int find_bytes (char* ps, int ls, char* pb, int lb, int mode, char (*cmp) (char));
HRESULT ResolveIt( HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath );

//Pabs changed - line insert
#define FW_MAX 1024//max bytes to fill with
char pcFWText[FW_MAX];//hex representation of bytes to fill with
char buf[FW_MAX];//bytes to fill with
int buflen;//number of bytes to fill with
char szFWFileName[_MAX_PATH];//fill with file name
int FWFile,FWFilelen;//fill with file and len
LONG oldproc;//old hex box proc
LONG cmdoldproc;//old command box proc
HFONT hfon;//needed so possible to display infinity char in fill with dlg box
HFONT hfdef;//store default text font for text boxes here
char curtyp;//filling with input-0 or file-1
char asstyp;
//Temporary stuff for CMD_move_copy
int iMoveDataUpperBound, iMove1stEnd,iMove2ndEndorLen, iMovePos;
OPTYP iMoveOpTyp;
//end
char szHexClass[] = "frhed hexclass";
char szMainClass[] = "frhed wndclass";
//inserted to allow 32-bit scrolling
SCROLLINFO SI = {sizeof(SI),0,0,0,0,0,0};
//end
//Pabs inserted
char bPasteBinary, bPasteUnicode;
//end

// RK: function by pabs.
void MessageCopyBox(HWND hWnd, LPTSTR lpText, LPCTSTR lpCaption, UINT uType, HWND hwnd);
char contextpresent();
char unknownpresent();
char oldpresent();
char linkspresent();
char frhedpresent();
/*Recursively delete key for WinNT
Don't use this under Win9x
Don't use this to delete keys you know will have no subkeys or should not have subkeys
This recursively deletes subkeys of the key and then
returns the return value of RegDeleteKey(basekey,keynam)*/
LONG RegDeleteWinNTKey(HKEY basekey, char keynam[]);


void TextToClipboard( char* pcText, HWND hwnd );

BOOL CALLBACK EnumWindowsProc( HWND hwnd, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );
	char buf[64];
	if( GetClassName( hwnd, buf, 64 ) != 0 )
	{
		if( strcmp( buf, szMainClass ) == 0 )
		{
			g_iInstCount++;
		}
	}
	return TRUE;
}

void count_instances()
{
	g_iInstCount = 0;
	EnumWindows( (WNDENUMPROC) EnumWindowsProc, 0 );
}

class WaitCursor
{
	private:
		HCURSOR cur;
	public:
		WaitCursor()
		{
			cur = SetCursor( LoadCursor( NULL, IDC_WAIT ) );
		}

		~WaitCursor()
		{
			SetCursor( cur == NULL ? LoadCursor( NULL, IDC_NO ) : cur );
		}
};

//--------------------------------------------------------------------------------------------
// Required for the find function.
inline char equal (char c)
{
	return c;
}

inline char lower_case (char c)
{
	if (c >= 'A' && c <= 'Z')
		return (char)('a' + c - 'A');
	else
		return c;
}

//Pabs changed - line insert
//used to swap tmpstart and tmpend if start>end
template<class T>void swap(T& x, T& y){
	T temp = x;
	x = y;
	y = temp;
}
/*template<class T>void swapxor(T x, T y){
	x ^= y;
	y ^= x;
	x ^= y;
}*/
//end

//--------------------------------------------------------------------------------------------
HexEditorWindow::HexEditorWindow ()
{
	PhysicalDrive = CreatePhysicalDriveInstance();
	NDASDrive = CreateNDASDriveInstance();

	Drive = PhysicalDrive;
//Pabs inserted
	bSaveIni = 1;
	bMakeBackups = 0;
#ifdef USEMEMDC
	mdc = 0 ;
	mbm = obm = 0;
	hbs = 0;
#endif //USEMEMDC

	MouseOpDist = GetProfileInt( "Windows", "DragMinDist", DD_DEFDRAGMINDIST );
	MouseOpDelay = GetProfileInt( "Windows", "DragDelay", DD_DEFDRAGDELAY );
	//We use the size of the font instead
	//ScrollInset = GetProfileInt( "Windows", "DragScrollInset", DD_DEFSCROLLINSET );
	ScrollDelay = GetProfileInt( "Windows", "DragScrollDelay", DD_DEFSCROLLDELAY );
	ScrollInterval = GetProfileInt( "Windows", "DragScrollInterval", DD_DEFSCROLLINTERVAL );

	enable_drop = TRUE;
	enable_drag = TRUE;
	enable_scroll_delay_dd = TRUE;
	enable_scroll_delay_sel = FALSE;
	always_pick_move_copy = FALSE;
	prefer_CF_HDROP = TRUE; // pabs: make the default 2 accept files FALSE;
	prefer_CF_BINARYDATA = TRUE;
	prefer_CF_TEXT = FALSE;
	output_CF_BINARYDATA = TRUE;
	output_CF_TEXT = TRUE;
	output_text_special = FALSE;
	output_text_hexdump_display = TRUE;
	output_CF_RTF = FALSE;
	dontdrop = false;
//end
	bDontMarkCurrentPos = FALSE;

	bInsertingHex = FALSE;

	BrowserName = "iexplore";
	TexteditorName.SetToString( "notepad.exe" );

	iWindowX = CW_USEDEFAULT;
	iWindowY = CW_USEDEFAULT;
	iWindowWidth = CW_USEDEFAULT;
	iWindowHeight = CW_USEDEFAULT;
	iWindowShowCmd = SW_SHOW;

	iHexWidth = 3;

	// iClipboardEncode = TRUE;
	iBmkColor = RGB( 255, 0, 0 );
	iSelBkColorValue = RGB( 255, 255, 0 );
	iSelTextColorValue = RGB( 0, 0, 0 );

	pcGotoDlgBuffer = NULL;
	iGotoDlgBufLen = 0;
	bOpenReadOnly = bReadOnly = FALSE;
	iPartialOffset=0;
	bPartialStats = 0;
	bPartialOpen=FALSE;
	iBmkCount=0;
	int i;
	for (i=1; i<=MRUMAX; i++)
		sprintf (&(strMRU[i-1][0]), "dummy%d", i);
	iMRU_count = 0;
	bFilestatusChanged = TRUE;
	iBinaryMode = LITTLEENDIAN_MODE;
	szFileName[0] = '\0';
	bUnsignedView = TRUE;
	iFontSize = 10;
	iInsertMode = FALSE;
	iTextColorValue = RGB (0,0,0);
	iBkColorValue = RGB (255,255,255);
	iSepColorValue = RGB (192,192,192);
	iAutomaticBPL = BST_CHECKED;
	bSelected = FALSE;
//Pabs replaced bLButtonIsDown with bSelecting & inserted
	bLButtonDown = FALSE;
	bMoving = FALSE;
	bSelecting = FALSE;
	bDroppedHere = FALSE;
//end
	iStartOfSelection = 0;
	iEndOfSelection = 0;
	hwnd = 0;
//Pabs changed "iOffsetLen" replaced with "iMinOffsetLen = iMaxOffsetLen"
	iMinOffsetLen = iMaxOffsetLen = 6;//max is same as min because there is no data
//end
	iByteSpace = 2;
	iBytesPerLine = 16;
	iCharSpace = 1;
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	iCharsPerLine = iMaxOffsetLen + iByteSpace + iBytesPerLine*3 + iCharSpace + iBytesPerLine;
//end
	filename = new char[_MAX_PATH];
	filename[0] = '\0';
	m_iEnteringMode = BYTES;
	m_iFileChanged = FALSE;
	bFileNeverSaved = TRUE;

	iFindDlgBufLen = 64 * 1024 - 1;
	pcFindDlgBuffer = new char[ iFindDlgBufLen ];
	if( pcFindDlgBuffer == NULL )
		MessageBox( NULL, "Could not allocate Find buffer!", "frhed", MB_OK );
	pcFindDlgBuffer[0] = '\0';

	iCopyHexdumpDlgStart = 0;
	iCopyHexdumpDlgEnd = 0;
	iCharacterSet = ANSI_FIXED_FONT;

	// Read in the last saved preferences.
	count_instances();
	read_ini_data ();

	bFileNeverSaved = TRUE;
	bSelected = FALSE;
	m_iFileChanged = FALSE;
	iVscrollMax = 0;
	iVscrollPos = 0;
	iVscrollInc = 0;
	iHscrollMax = 0;
	iHscrollPos = 0;
	iHscrollInc = 0;
	iCurLine = 0;
	iCurByte = 0;
	DataArray.ClearAll ();
	DataArray.SetGrowBy (100);
	sprintf (filename, "Untitled");
}

//--------------------------------------------------------------------------------------------
HexEditorWindow::~HexEditorWindow ()
{
	if (hFont != NULL)
		DeleteObject (hFont);
	if (filename != NULL)
		delete [] filename;
	if (pcFindDlgBuffer != NULL)
		delete [] pcFindDlgBuffer;
	if( pcGotoDlgBuffer != NULL )
		delete [] pcGotoDlgBuffer;
//Pabs inserted
#ifdef USEMEMDC
	if( mdc ){
		mbm = (HBITMAP) SelectObject(mdc,obm);
		DeleteObject(mbm);
		DeleteDC(mdc);
	}
#endif //USEMEMDC
	delete PhysicalDrive;
	delete NDASDrive;
	Drive = NULL;
//end
}

//--------------------------------------------------------------------------------------------
int HexEditorWindow::load_file (char* fname)
{
	if (file_is_loadable (fname))
	{
		int filehandle;
		if ((filehandle = _open (fname,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			int filelen = _filelength (filehandle);
			DataArray.ClearAll ();
			// Try to allocate memory for the file.
			if (DataArray.SetSize (filelen) == TRUE)
			{

				// If read-only mode on opening is enabled or the file is read only:
				if( bOpenReadOnly || -1== _access(fname,02))//Pabs added call to _access
					bReadOnly = TRUE;
				else
					bReadOnly = FALSE;

				if( filelen == 0)
				{
					// This is an empty file. Don't need to read anything.
					_close( filehandle );
//Pabs replaced strcpy w GetLongPathNameWin32
					GetLongPathNameWin32( fname, filename, _MAX_PATH );
//end
					bFileNeverSaved = FALSE;
					bPartialStats = 0;
					bPartialOpen=FALSE;
					// Update MRU list.
					update_MRU ();
					bFilestatusChanged = TRUE;
					update_for_new_datasize();
					return TRUE;
				}
				else
				{
					// Load the file.
					SetCursor (LoadCursor (NULL, IDC_WAIT));
					DataArray.SetUpperBound (filelen-1);
					if (_read (filehandle, DataArray, DataArray.GetLength ()) != -1)
					{
						_close (filehandle);
//Pabs replaced strcpy w GetLongPathNameWin32
						GetLongPathNameWin32( fname, filename, _MAX_PATH );
//end
						bFileNeverSaved = FALSE;
						bPartialStats = 0;
						bPartialOpen=FALSE;
						// Update MRU list.
						update_MRU ();
						bFilestatusChanged = TRUE;
						update_for_new_datasize();
						SetCursor (LoadCursor (NULL, IDC_ARROW));
						return TRUE;
					}
					else
					{//Pabs replaced NULL w hwnd
						_close (filehandle);
						SetCursor (LoadCursor (NULL, IDC_ARROW));
						MessageBox (hwnd, "Error while reading from file.", "Load error", MB_OK | MB_ICONERROR);
						return FALSE;
					}
				}
			}
			else
			{
				MessageBox (hwnd, "Not enough memory to load file.", "Load error", MB_OK | MB_ICONERROR);
				return FALSE;
			}
		}
		else
		{
			char buf[500];
			sprintf (buf, "Error code 0x%x occured while opening file %s.", errno, fname);
			MessageBox (hwnd, buf, "Load error", MB_OK | MB_ICONERROR);
			return FALSE;//end
		}
	}
	else
		return FALSE;
}

//--------------------------------------------------------------------------------------------
int HexEditorWindow::file_is_loadable (char* fname)
{
	int filehandle;
	if ((filehandle = _open (fname,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
	{
		_close (filehandle);
		return TRUE;
	}
	else
		return FALSE;
}

//--------------------------------------------------------------------------------------------
int HexEditorWindow::at_window_create (HWND hw, HINSTANCE hI)
{
	hwnd = hwndHex = hw;
	hInstance = hI;

	iVscrollMax = 0;
	iVscrollPos = 0;
	iVscrollInc = 0;
	iHscrollMax = 0;
	iHscrollPos = 0;
	iHscrollInc = 0;

	iCurLine = 0;
	iCurByte = 0;
	iCurNibble = 0;

	target = new CDropTarget(true,&target);
	if( target )
	{
		CoLockObjectExternal( target, TRUE, FALSE );
		if( enable_drop )
			RegisterDragDrop( hwnd, target );
	}
	if( !target || !enable_drop || ( enable_drop && prefer_CF_HDROP ) )
		DragAcceptFiles( hwnd, TRUE ); // Accept files dragged into window.
	return TRUE;
}

//--------------------------------------------------------------------------------------------
// Window was resized to new width of cx and new height of cy.
int HexEditorWindow::resize_window (int cx, int cy)
{
//Pabs inserted
#ifdef USEMEMDC
	hbs = 1;//Next time HexEditorWindow::paint is called it will resize the memory bitmap
#endif
//end
	// Get font data.
	HDC hdc = GetDC (hwnd);
	make_font ();
	HFONT of = (HFONT) SelectObject( hdc, hFont );
	TEXTMETRIC tm;
	GetTextMetrics (hdc, &tm);
	cxChar = tm.tmAveCharWidth;
	cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * cxChar / 2;
	cyChar = tm.tmHeight + tm.tmExternalLeading;
	SelectObject (hdc, of);
	ReleaseDC (hwnd, hdc);

	//--------------------------------------------
	cxClient = cx;
	cyClient = cy;

	//--------------------------------------------

//Pabs inserted
	int x, y;
	x = DataArray.GetLength();//value of the last offset
	if( bPartialStats ) x += iPartialOffset;
	if(x)for(iMaxOffsetLen = y = 0;y<x;iMaxOffsetLen++,y = y<<4 | 0x0f);//length of last offset
	else iMaxOffsetLen = 1;
	if( iMinOffsetLen > iMaxOffsetLen) iMaxOffsetLen = iMinOffsetLen;
//end

	cxBuffer = max (1, cxClient / cxChar);
	cyBuffer = max (1, cyClient / cyChar);
	// Adjust bytes per line to width of window.
	// cxBuffer = maximal width of client-area in chars.
	if( iAutomaticBPL )
	{
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		int bytemax = cxBuffer-iMaxOffsetLen-iByteSpace-iCharSpace;
//end
		iBytesPerLine = bytemax / 4;
		if (iBytesPerLine < 1)
			iBytesPerLine = 1;
	}

//Pabs inserted
	x = DataArray.GetLength()/iBytesPerLine*iBytesPerLine;//value of the last offset
	if( bPartialStats ) x += iPartialOffset;
	if(x)for(iMaxOffsetLen = y = 0;y<x;iMaxOffsetLen++,y = y<<4 | 0x0f);//length of last offset
	else iMaxOffsetLen = 1;
	if( bAutoOffsetLen ) iMinOffsetLen = iMaxOffsetLen;
	else if( iMinOffsetLen > iMaxOffsetLen) iMaxOffsetLen = iMinOffsetLen;
//end

	// Caret or end of selection will be vertically centered if line not visible.
	if( bSelected )
	{
		if( iEndOfSelection / iBytesPerLine < iCurLine || iEndOfSelection / iBytesPerLine > iCurLine + cyBuffer )
			iCurLine = max( 0, iEndOfSelection / iBytesPerLine - cyBuffer / 2 );
	}
	else
	{
		if( iCurByte/iBytesPerLine < iCurLine || iCurByte/iBytesPerLine > iCurLine + cyBuffer )
			iCurLine = max( 0, iCurByte/iBytesPerLine-cyBuffer/2 );
	}

//Pabs removed - scrollbar fix

//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	iCharsPerLine = iMaxOffsetLen + iByteSpace + iBytesPerLine*3 + iCharSpace + iBytesPerLine;
//end

	// Get number of lines to display.
	if ((DataArray.GetLength()+1) % iBytesPerLine == 0)
		iNumlines = (DataArray.GetLength()+1) / iBytesPerLine;
	else
		iNumlines = (DataArray.GetLength()+1) / iBytesPerLine + 1;

//Pabs inserted "ffff" after each 0xffff - 32bit scrolling
	if (iNumlines <= 0xffffffff)
		iVscrollMax = iNumlines-1;
	else
		iVscrollMax = 0xffffffff;
//end
//Pabs reorganised this bit to fix the scrollbars
	SI.fMask  = SIF_RANGE | SIF_POS | SIF_DISABLENOSCROLL | SIF_PAGE;
	if(iVscrollPos>iVscrollMax-cyBuffer+1)iVscrollPos = iVscrollMax-cyBuffer+1;
	if(iVscrollPos<0)iVscrollPos=0;
	SI.nPage = cyBuffer;
	SI.nPos = iCurLine = iVscrollPos;
	SI.nMin = 0;
	SI.nMax = iVscrollMax;
	SetScrollInfo(hwnd, SB_VERT, &SI, TRUE);

	iHscrollMax = iCharsPerLine - 1;
	if(iHscrollPos>iHscrollMax-cxBuffer+1)iHscrollPos = iHscrollMax-cxBuffer+1;
	if(iHscrollPos<0)iHscrollPos=0;
	SI.nPage = cxBuffer;
	SI.nPos = iHscrollPos;
	SI.nMax = iHscrollMax;
	SetScrollInfo(hwnd, SB_HORZ, &SI, TRUE);

	set_wnd_title ();
	if (hwnd == GetFocus ())
		set_caret_pos ();
	repaint ();
	return TRUE;
}

//--------------------------------------------------------------------------------------------
// When focus is gained.
int HexEditorWindow::set_focus ()
{
	if (cxChar == 0 || cyChar == 0)
	{
		make_font ();
		HDC hdc = GetDC (hwnd);
		HFONT of = (HFONT) SelectObject (hdc, hFont);
		TEXTMETRIC tm;
		GetTextMetrics (hdc, &tm);
		cxChar = tm.tmAveCharWidth;
		cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * cxChar / 2;
		cyChar = tm.tmHeight + tm.tmExternalLeading;
		SelectObject (hdc, of);
		ReleaseDC (hwnd, hdc);
	}
	update_for_new_datasize ();
	CreateCaret (hwnd, NULL, cxChar, cyChar);
	set_caret_pos ();
	ShowCaret (hwnd);
	return 0;
}

//--------------------------------------------------------------------------------------------
// What to do when focus is lost.
int HexEditorWindow::kill_focus ()
{
	HideCaret (hwnd);
	DestroyCaret ();
	return TRUE;
}

//Pabs inserted
//Kludge until HW::character can call MoveLeft(character=1,extend=false)
bool dontcheckkeystate = false;
//end
//--------------------------------------------------------------------------------------------
// Handler for non-character keys (arrow keys, page up/down etc.)
int HexEditorWindow::keydown (int key)
{
	if (filename[0] == '\0' || iCurByte<0)
		return 0;

	//Pabs rewrote the rest of this function
	//Only handle these
	switch (key){
		case VK_ESCAPE:case VK_LEFT:case VK_RIGHT:case VK_UP:case VK_DOWN:
		case VK_PRIOR:case VK_NEXT:case VK_HOME:case VK_END:break;
		default:return 0;
	}

	int shift = 0;
	int ctrl = 0;

	if (!dontcheckkeystate) {
		shift = !!(GetKeyState( VK_SHIFT ) & 0x8000);
		ctrl = !!(GetKeyState( VK_CONTROL ) & 0x8000);
	}

	int* a;//Data to update
	int  b;//How much to update it by
	int  c;//The original value
	int sel = bSelected;

	if(shift && !ctrl && key!=VK_ESCAPE)
	{
		a=&iEndOfSelection;
		if( !bSelected ){
			iStartOfSelection = iEndOfSelection = c = iCurByte;
			bSelected = TRUE;
		}
		else c=iEndOfSelection;
	}
	else if( !bSelecting )
	{
		c=iCurByte;
		if( bSelected )
		{
			iStartOfSelSetting = iStartOfSelection;
			iEndOfSelSetting = iEndOfSelection;
			if(iStartOfSelSetting>iEndOfSelSetting)swap(iStartOfSelSetting,iEndOfSelSetting);

			switch (key){
				case VK_ESCAPE: if(dragging)return 0; ShowCaret( hwnd );
				case VK_LEFT:case VK_UP:case VK_PRIOR:case VK_HOME:
					iCurByte = iStartOfSelSetting;
					c = iEndOfSelSetting;
					iCurNibble = 0;
				break;
				case VK_RIGHT:case VK_DOWN:case VK_NEXT:case VK_END:
					iCurByte = iEndOfSelSetting;
					c = iStartOfSelSetting;
					iCurNibble = 1;
				break;
			}
		}
		if(ctrl && shift){a=&iEndOfSelection;c=iEndOfSelection;bSelected=TRUE;}
		else{bSelected = FALSE; a=&iCurByte;}
	}
	else /*if( bSelected && bSelecting )*/
	{
		a=&iStartOfSelection;c=iStartOfSelection;
		if (key==VK_ESCAPE){
			iStartOfSelection = iEndOfSelection= new_pos;
		}
	}

	int icn = iCurNibble;

	int lastbyte = LASTBYTE;
	if(!bSelected)lastbyte++;

	switch(key)
	{
		case VK_LEFT: b = ( m_iEnteringMode == CHARS || bSelected ? 1 : iCurNibble = !iCurNibble ); break;
		case VK_RIGHT: b = ( m_iEnteringMode == CHARS || bSelected ? 1 : iCurNibble ); if( m_iEnteringMode == BYTES ) iCurNibble = !iCurNibble; break;
		case VK_UP:   case VK_DOWN:  b = iBytesPerLine; break;
		case VK_PRIOR:case VK_NEXT:  b = cyBuffer * iBytesPerLine; break;
	}

	switch(key)
	{
		case VK_LEFT: case VK_UP:  case VK_PRIOR: *a-=b; break;
		case VK_RIGHT:case VK_DOWN:case VK_NEXT:  *a+=b; break;
		case VK_HOME:
			if(ctrl) *a = 0;
			else *a=*a/iBytesPerLine*iBytesPerLine;
			iCurNibble = 0;
		break;
		case VK_END:
			if(ctrl) *a = lastbyte;
			else *a=(*a/iBytesPerLine+1)*iBytesPerLine-1;
			iCurNibble = 1;
			break;
	}

	if(bSelected){
		if(lastbyte<0){bSelected=iCurNibble=iCurByte=0;}
		else{
			if(iStartOfSelection>lastbyte){iStartOfSelection=lastbyte;iCurNibble=1;}
			if(iStartOfSelection<0){iStartOfSelection=0;iCurNibble=0;}
			if(iEndOfSelection>lastbyte){iEndOfSelection=lastbyte;iCurNibble=1;}
			if(iEndOfSelection<0){iEndOfSelection=0;iCurNibble=0;}
		}
	} else {
		if(iCurByte>lastbyte){iCurByte=lastbyte;iCurNibble=1;}
		if(iCurByte<0){iCurByte=0;iCurNibble=0;}
	}
	//repaint from line c to line a
	if(c!=*a||icn!=iCurNibble||sel!=bSelected){
		int repall = 0;
		int adjusth = 0, adjustv = 0;
		int column,line;

		if(bSelected){iCurByte = *a;iCurNibble = 0;}
		if ( bSelecting ? area == AREA_BYTES : m_iEnteringMode == BYTES )
			column = BYTES_LOGICAL_COLUMN;
		else
			column = CHARS_LOGICAL_COLUMN;

		if (column >= iHscrollPos+cxBuffer){
			iHscrollPos = column-(cxBuffer-1);
			adjusth = 1;
		}
		else if (column < iHscrollPos){
			iHscrollPos = column;
			adjusth = 1;
		}

		if(adjusth){
			adjust_hscrollbar ();
			repall = 1;
		}

		line = BYTELINE;
		if( line < iCurLine ){
			iCurLine = line;
			adjustv = 1;
		}
		else if( line > iCurLine+cyBuffer - 1 )
		{
			iCurLine = line - cyBuffer + 1;
			if( iCurLine < 0 )
				iCurLine = 0;
			adjustv = 1;
		}

		if(adjustv){
			adjust_vscrollbar ();
			repall = 1;
		}

		if(repall) repaint();
		else repaint(*a/iBytesPerLine,c/iBytesPerLine);
	}

	return 0;
}

//--------------------------------------------------------------------------------------------
// Handler for character keys.
int HexEditorWindow::character (char ch)
{
	if(bSelecting)
		return 0;

//Pabs inserted
	//This will be handled in HexEditorWindow::keydown
	if(ch == 27) return 0;
//end

	// If we are in read-only mode, give a warning and return,
	// except if TAB was pressed.
	if( bReadOnly && ch != '\t' )
	{
		MessageBox( hwnd, "Can't change file because read-only mode is engaged!", "Keyboard editing", MB_OK | MB_ICONERROR );
		return 0;
	}

	char x, c = (char)tolower (ch);
	if (ch == '\t') // TAB => change EnteringMode.
	{
		if (m_iEnteringMode == BYTES)
			m_iEnteringMode = CHARS;
		else
			m_iEnteringMode = BYTES;

		if(bSelected)
			adjust_view_for_selection();
		else
			adjust_view_for_caret();

		repaint ();
		return 0;
	}

	// If read-only mode, return.
	if( bReadOnly )
		return 1;

	// If there is selection replace.
	if ( bSelected )
//Pabs inserted & moved to allow replacement of the selection with a character
	{
		int ss = iStartOfSelection;
		int es = iEndOfSelection;
		if (ss > es) swap(es,ss);
		int len = es - ss ;//+1 gives length +1-1 give what we want so it evens out
		DataArray.RemoveAt(ss+1,len);//Remove extraneous data
		DataArray[ss] = ch;//Write the character
		bSelected = FALSE;//Deselect
		iCurByte = ss;//Set caret
		m_iEnteringMode = CHARS;
		m_iFileChanged = TRUE;//File has changed
		bFilestatusChanged = TRUE;
		update_for_new_datasize();//Redraw stuff
		return 0;
	}

	// If in bytes and char is not a hex digit, return.
	if (m_iEnteringMode==BYTES && !((c>='a'&&c<='f')||(c>='0'&&c<='9')))
		return 1;

	// If caret at EOF.
	if (iCurByte == DataArray.GetLength())
	{
		if (DataArray.InsertAtGrow(iCurByte, 0, 1) == TRUE)
		{
			iCurNibble = 0;
			iInsertMode = FALSE;
			character (ch);
			update_for_new_datasize ();
			return 1;
		}
		else
		{//Pabs replaced NULL w hwnd
			MessageBox (hwnd, "Not enough memory for inserting character.", "Insert mode error", MB_OK | MB_ICONERROR);
		}//end
		return 0;
	}

	if( iInsertMode )
	{
		// INSERT
		if( m_iEnteringMode == BYTES )
		{
			if( ( c >= 'a' && c <= 'f' ) || ( c >= '0' && c <= '9' ) )
			{
				if( bInsertingHex )
				{
					// Expecting the lower nibble of the recently inserted byte now.
					// The bInsertingHex mode must be turned off if anything other is done
					// except entering a valid hex digit. This is checked for in the
					// HexEditorWindow::OnWndMsg() method.
					bInsertingHex = FALSE;
					if (c >= 'a' && c <= 'f')
						x = (char)(c - 0x61 + 0x0a);
					else
						x = (char)(c - 0x30);
					DataArray[iCurByte] = (BYTE)((DataArray[iCurByte] & 0xf0) | x);
					m_iFileChanged = TRUE;
					bFilestatusChanged = TRUE;
					iCurByte++;
					iCurNibble = 0;
					update_for_new_datasize();
				}
				else
				{
					// Insert a new byte with the high nibble set to value just typed.
					if( DataArray.InsertAtGrow( iCurByte, 0, 1 ) == TRUE )
					{
						bInsertingHex = TRUE;
						if (c >= 'a' && c <= 'f')
							x = (char)(c - 0x61 + 0x0a);
						else
							x = (char)(c - 0x30);
						DataArray[iCurByte] = (BYTE)((DataArray[iCurByte] & 0x0f) | (x << 4));
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
						iCurNibble = 1;
						update_for_new_datasize();
					}
					else
					{//Pabs replaced NULL w hwnd
						MessageBox (hwnd, "Not enough memory for inserting.", "Insert mode error", MB_OK | MB_ICONERROR);
						return 0;
					}//end
				}
			}
			return 1;
		}
		else if (m_iEnteringMode == CHARS)
		{
			if (DataArray.InsertAtGrow(iCurByte, 0, 1) == TRUE)
			{
				iCurNibble = 0;
				iInsertMode = FALSE;
				character (ch);
				iInsertMode = TRUE;
				iCurNibble = 0;
				update_for_new_datasize ();
			}
			else
			{//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Not enough memory for inserting.", "Insert mode error", MB_OK | MB_ICONERROR);
				return 0;
			}//end
		}
		return 1;
	}
	else
	{
//Pabs inserted
		dontcheckkeystate = true;
//end
		// OVERWRITE
		// TAB => change mode.
		// Byte-mode: only a-f, 0-9 allowed.
		if ((m_iEnteringMode == BYTES) && ((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9')))
		{
			if (c >= 'a' && c <= 'f')
				x = (char)(c - 0x61 + 0x0a);
			else
				x = (char)(c - 0x30);
			if (iCurNibble == 0)
				DataArray[iCurByte] = (BYTE)((DataArray[iCurByte] & 0x0f) | (x << 4));
			else
				DataArray[iCurByte] = (BYTE)((DataArray[iCurByte] & 0xf0) | x);
			m_iFileChanged = TRUE;
			bFilestatusChanged = TRUE;
			keydown (VK_RIGHT);
		}
		// Char-mode.
		else if (m_iEnteringMode == CHARS)
		{
			switch (iCharacterSet)
			{
			case ANSI_FIXED_FONT:
				DataArray[iCurByte] = ch;
				break;

			case OEM_FIXED_FONT:
				{
					char src[2], dst[2];
					src[0] = ch;
					src[1] = 0;
					CharToOem (src, dst);
					DataArray[iCurByte] = dst[0];
				}
				break;
			}
			m_iFileChanged = TRUE;
			bFilestatusChanged = TRUE;
			keydown (VK_RIGHT);
		}
//Pabs inserted
		dontcheckkeystate = false;
//end
	}
	return 0;
}

//--------------------------------------------------------------------------------------------
// Handler for vertical scrollbar.
int HexEditorWindow::vscroll (int cmd, int pos)
{
	if (NO_FILE || DataArray.GetLength()==0)
		return 0;

	iVscrollInc = 0;
	switch (cmd)
	{
	case SB_TOP:
		iCurLine = 0;
		break;
	case SB_BOTTOM:
		iCurLine = iNumlines-1;
		break;
	case SB_LINEUP:
		if (iCurLine > 0)
			iCurLine -= 1;
		break;
	case SB_LINEDOWN:
		if (iCurLine < iNumlines-1)
			iCurLine += 1;
		break;
	case SB_PAGEUP:
		if (iCurLine >= cyBuffer)
			iCurLine -= cyBuffer;
		else
			iCurLine = 0;
		break;
	case SB_PAGEDOWN:
		if (iCurLine <= iNumlines-1-cyBuffer)
			iCurLine += cyBuffer;
		else
			iCurLine = iNumlines-1;
		break;
	case SB_THUMBTRACK:
		iCurLine = (int) (pos * ((float)(iNumlines-1)/(float)iVscrollMax));
		SI.fMask  = SIF_POS | SIF_DISABLENOSCROLL;
		SI.nPos   = iVscrollPos = iCurLine = pos;
		SetScrollInfo(hwnd, SB_VERT, &SI, true);

		if (iCurLine > iNumlines-1)
			iCurLine = iNumlines-1;
		// Make sure the number of empty lines is as small as possible.
		if( iNumlines - iCurLine < cyBuffer )
		{
			iCurLine = ( ( DataArray.GetUpperBound() + 1 ) / iBytesPerLine ) - ( cyBuffer - 1 );
			if( iCurLine < 0 )
				iCurLine = 0;
		}
		repaint();
		return 0;
	default:
		return 0;//break;//Pabs put return here - don't repaint if don't need to
	}
//Pabs inserted
	if(iCurLine>iVscrollMax-cyBuffer+1)iCurLine = iVscrollMax-cyBuffer+1;
	if( iCurLine < 0 ) iCurLine = 0;
//end
	iVscrollPos = (int) ((float)iCurLine * ((float)iVscrollMax)/(float)(iNumlines-1));

	SI.fMask  = SIF_POS | SIF_DISABLENOSCROLL;
	SI.nPos   = iVscrollPos;
	SetScrollInfo(hwnd, SB_VERT, &SI, TRUE);

	if (iCurLine > iNumlines-1)
		iCurLine = iNumlines-1;
	repaint ();
	return 0;
}

//--------------------------------------------------------------------------------------------
// Handler for horizontal scrollbar.
int HexEditorWindow::hscroll (int cmd, int pos)
{
	if (NO_FILE || DataArray.GetLength()==0)
		return 0;

	iHscrollInc = 0;
	switch (cmd)
	{
	case SB_TOP:
		iHscrollInc = -iHscrollPos;
		break;
	case SB_BOTTOM:
		iHscrollInc = iHscrollMax - iHscrollPos;
		break;
	case SB_LINEUP:
		if (iHscrollPos > 0)
			iHscrollInc = -1;
		break;
	case SB_LINEDOWN:
		if (iHscrollPos < iHscrollMax)
			iHscrollInc = 1;
		break;
	case SB_PAGEUP:
		if (iHscrollPos >= cxBuffer)
			iHscrollInc = -cxBuffer;
		else
			iHscrollInc = -iHscrollPos;
		break;
	case SB_PAGEDOWN:
		if (iHscrollPos <= iHscrollMax-cxBuffer)
			iHscrollInc = cxBuffer;
		else
			iHscrollInc = iHscrollMax - iHscrollPos;
		break;
	case SB_THUMBTRACK:
		iHscrollInc = pos - iHscrollPos;
		break;
	default:
		return 0;//break;//Pabs put return here - don't repaint if don't need to
	}
	iHscrollPos += iHscrollInc;
//Pabs inserted
	if(iHscrollPos>iHscrollMax-cxBuffer+1)iHscrollPos = iHscrollMax-cxBuffer+1;
	if( iHscrollPos < 0 ) iHscrollPos = 0;
//end
	SI.fMask  = SIF_POS | SIF_DISABLENOSCROLL;
	SI.nPos   = iHscrollPos;
	SetScrollInfo(hwnd, SB_HORZ, &SI, TRUE);
	InvalidateRect (hwnd, NULL, FALSE);
	UpdateWindow (hwnd);
	return 0;
}

//--------------------------------------------------------------------------------------------
// WM_PAINT handler.
int HexEditorWindow::paint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint (hwnd, &ps);
//Pabs inserted
	//-------------------------------------------------------
#ifdef USEMEMDC
	HDC sdc = hdc;//ps.hdc;//Save the screen dc (hdc becomes mdc)
	if(!mdc)mdc = CreateCompatibleDC(hdc);
	hdc = mdc;
	RECT rc ; GetClientRect(hwnd,&rc);
	SIZE obs = {rc.right-rc.left+1,rc.bottom-rc.top+1};//bitmap size & old bitmap size
	if(hbs){
		hbs = 0;
		mbm = (HBITMAP) SelectObject(hdc, obm);
		DeleteObject(mbm);
		mbm = 0;
	}
	if(!mbm){
		mbm = CreateCompatibleBitmap(sdc,obs.cx,obs.cy);
		obm = (HBITMAP) SelectObject(hdc, mbm);
	}

//Fixes for iUpdateLine == -1 display bug
#ifdef UPDATE_ALL_LINES
	iUpdateLine = iUpdateToLine = -1;
#endif //UPDATE_ALL_LINES
#ifdef BITBLT_TOMDC
	//This BitBlt prevents the display from fucking up but is incredibly slow
	BitBlt(hdc,rc.left,rc.top,obs.cx,obs.cy,sdc,rc.left,rc.top,SRCCOPY);
#endif //BITBLT_TOMDC
#endif //USEMEMDC
//end
	//-------------------------------------------------------
	HideCaret (hwnd);
	// Delete remains of last position.
	int a, b;
	b = min (iCurLine + cyBuffer, iNumlines-1);
	iBkColor = PALETTERGB (GetRValue(iBkColorValue),GetGValue(iBkColorValue),GetBValue(iBkColorValue));
	iTextColor = PALETTERGB (GetRValue(iTextColorValue),GetGValue(iTextColorValue),GetBValue(iTextColorValue));
	SetTextColor (hdc, iTextColor);
	SetBkColor (hdc, iBkColor);
	HPEN pen1 = CreatePen (PS_SOLID, 1, iBkColor);
	HPEN oldpen = (HPEN) SelectObject (hdc, pen1);
	HBRUSH brush1 = CreateSolidBrush (iBkColor);
	HBRUSH oldbrush = (HBRUSH) SelectObject (hdc, brush1);
	// Delete lower border if there are empty lines on screen.
	if ((b-iCurLine+1)*cyChar < cyClient)
		Rectangle (hdc, 0, (b-iCurLine+1)*cyChar, cxClient, cyClient);
	// Delete right border.
	Rectangle (hdc, ((iHscrollMax+1)-iHscrollPos)*cxChar, 0, cxClient, cyClient);
	SelectObject (hdc, oldpen);
	SelectObject (hdc, oldbrush);
	DeleteObject (pen1);
	DeleteObject (brush1);

	// Get font.
	HFONT oldfont = (HFONT) SelectObject (hdc, hFont);
	HPEN sep_pen = CreatePen (PS_SOLID, 1, iSepColorValue);
	oldpen = (HPEN) SelectObject (hdc, sep_pen);

//Pabs altered so that less code could be used (2 if else blocks becomes 1 if else block)
	int resized = 0;
	if( Linebuffer.GetSize() >= iCharsPerLine || (resized = Linebuffer.SetSize( iCharsPerLine )))
	{
		// Linebuffer large enough.
		// Linebuffer successfully resized.
		if(resized)Linebuffer.ExpandToSize();

		HBRUSH hbr = CreateSolidBrush( iBmkColor );
		if (iUpdateLine < 0) a = iCurLine;
		else a = iUpdateLine;
		if (iUpdateToLine >= 0) b = iUpdateToLine;
		else b = iCurLine + cyBuffer;

		if(a>b)swap(a,b);
		if(a<iCurLine)a=iCurLine;
		if(b>iCurLine + cyBuffer)b=iCurLine + cyBuffer;

		for (int i = a; i <= b; i++)
			print_line( hdc, i, Linebuffer, hbr );

		DeleteObject( hbr );
		SelectObject (hdc, oldpen);
		DeleteObject (sep_pen);
		SelectObject (hdc, oldfont);
		// Mark character.
		if( BYTELINE>=a && BYTELINE<=b ) mark_char (hdc);
//Pabs inserted
#ifdef USEMEMDC
		BitBlt(sdc,rc.left,rc.top,obs.cx,obs.cy,hdc,rc.left,rc.top,SRCCOPY);
#endif //USEMEMDC
//end
		ShowCaret (hwnd);
		EndPaint (hwnd, &ps);
		set_caret_pos ();
		set_wnd_title ();
		iUpdateLine = iUpdateToLine = -1;
		return 0;
	}
	else
	{
		// Linebuffer too small.
		// Could not allocate line buffer.
		Rectangle( hdc, 0, 0, cxClient, cyClient );
		char buf[] = "Error: could not allocate line buffer.\nPlease save your changes and restart frhed.";
		RECT r;
		r.top = 0;
		r.left = 0;
		r.right = cxClient;
		r.bottom = cyClient;
		DrawText( hdc, buf, -1, &r, DT_LEFT );
//Pabs inserted
#ifdef USEMEMDC
		BitBlt(sdc,rc.left,rc.top,obs.cx,obs.cy,hdc,rc.left,rc.top,SRCCOPY);
#endif //USEMEMDC
		EndPaint (hwnd, &ps);
//end
	}

	return 0;
}


//--------------------------------------------------------------------------------------------
// Receives WM_COMMAND messages and passes either them to their handler functions or
// processes them here.
int HexEditorWindow::command (int cmd)
{
	HMENU hMenu = GetMenu (hwndMain);
	switch( cmd )
	{
//Pabs changed - line insert
//File Menu
	case IDM_REVERT:
		CMD_revert();
		break;
	case IDM_SAVESELAS:
		CMD_saveselas();
		break;
	case IDM_DELETEFILE:
		CMD_deletefile();
		break;
	case IDM_INSERTFILE:
		CMD_insertfile();
		break;
	case IDM_OPEN_HEXDUMP:
		CMD_open_hexdump();
		break;
//Edit Menu
	case IDM_FILL_WITH:
		CMD_fw();
		break;
	case IDM_EDIT_MOVE_COPY:
		CMD_move_copy();
		break;
	case IDM_EDIT_REVERSE:
		CMD_reverse();
		break;
//Options menu
	case IDM_MAKE_BACKUPS:
		bMakeBackups=!bMakeBackups;
		break;
	case IDM_ADOPT_COLOURS:
		CMD_adopt_colours();
		break;
	case IDM_OLE_DRAG_DROP:
		DialogBox (hInstance, MAKEINTRESOURCE (IDD_DRAG_DROP_OPTIONS), hwnd, (DLGPROC) DragDropOptionsDlgProc);
		break;
//Registry menu
	case IDM_SHORTCUTS:
		DialogBox (hInstance, MAKEINTRESOURCE (IDD_SHORTCUTS), hwnd, (DLGPROC) ShortcutsDlgProc);
		break;
	case IDM_UPGRADE:
		WNDCLASS wc;
		wc.style = CS_SAVEBITS;
		wc.lpfnWndProc = DisplayProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON1));
		wc.hCursor = LoadCursor( NULL, IDC_ARROW );
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = "frhed display";
		RegisterClass (&wc);

		DialogBox (hInstance, MAKEINTRESOURCE (IDD_UPGRADE), hwnd, (DLGPROC) UpgradeDlgProc);

		UnregisterClass(wc.lpszClassName,hInstance);

		repaint();
		break;
	case IDM_REMOVE:
		int res,r,r0;r=r0=0;//r&r0 used to determine if the user has removed all frhed data
		res = MessageBox(hwnd,"Are you sure you want to remove frhed ?","Remove frhed",MB_YESNO);
		if(res!=IDYES) return 0;
		//Can assume registry data exists
		res = linkspresent();
		if(res){
			r0++;
			res = MessageBox(hwnd,"Remove known links ?","Remove frhed",MB_YESNO);
			if(res==IDYES) {
				r++;
				//Remove known links & registry entries of those links
				HKEY hk;
				char valnam[_MAX_PATH+1]="";
				DWORD valnamsize = _MAX_PATH+1,typ;
				char valbuf[_MAX_PATH+1]="";
				DWORD valbufsize = _MAX_PATH+1,ret;
				if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",0,KEY_ALL_ACCESS,&hk)){
					for(DWORD i = 0;;i++){
						typ=0;
						valnamsize = valbufsize = _MAX_PATH+1;
						valbuf[0]=valnam[0]=0;
						ret = RegEnumValue(hk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
						if(typ==REG_SZ && valbuf[0]!=0 && PathIsDirectory(valbuf)){
							PathAddBackslash(valbuf);
							strcat(valbuf,"frhed.lnk");
							remove(valbuf);
						}
						if(ERROR_NO_MORE_ITEMS==ret)break;
					}
					RegCloseKey(hk);
				}
				RegDeleteKey(HKEY_CURRENT_USER,"Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links");
			}
		}
		res = contextpresent()||unknownpresent();
		if(res){
			r0++;
			res = MessageBox(hwnd,"Remove 'Open in frhed' command(s) ?","Remove frhed",MB_YESNO);
			if(res==IDYES) {
				r++;
				//Remove 'Open in frhed' command registry entries
				RegDeleteKey( HKEY_CLASSES_ROOT, "*\\shell\\Open in frhed\\command" ); //WinNT requires the key to have no subkeys
				RegDeleteKey( HKEY_CLASSES_ROOT, "*\\shell\\Open in frhed" );
				RegDeleteKey( HKEY_CLASSES_ROOT, "Unknown\\shell\\Open in frhed\\command" ); //WinNT requires the key to have no subkeys
				RegDeleteKey( HKEY_CLASSES_ROOT, "Unknown\\shell\\Open in frhed" );
				char stringval[ _MAX_PATH ]="";
				long len = _MAX_PATH;
				RegQueryValue(HKEY_CLASSES_ROOT,"Unknown\\shell",stringval,&len);
				if(!strcmp(stringval, "Open in frhed")){
					HKEY hk;
					if(ERROR_SUCCESS==RegOpenKey(HKEY_CLASSES_ROOT, "Unknown\\shell",&hk)){
						RegDeleteValue(hk,NULL);
						RegCloseKey(hk);
					}
				}
			}
		}
		HKEY tmp;
		res = RegOpenKey(HKEY_CURRENT_USER,"Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO ,&tmp);
		if(res==ERROR_SUCCESS){
			RegCloseKey(tmp);
			r0++;
			res = MessageBox(hwnd,"Remove registry entries ?","Remove frhed",MB_YESNO);
			if(res==IDYES) {
				r++;
				bSaveIni = 0;//Don't save ini data when the user quits (and hope other instances are not running now (they will write new data)
				OSVERSIONINFO ver;
				ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
				GetVersionEx(&ver);
				if(ver.dwPlatformId==VER_PLATFORM_WIN32_NT)
					RegDeleteWinNTKey(HKEY_CURRENT_USER,"Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO );
				else
					RegDeleteKey(HKEY_CURRENT_USER,"Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO );
				res = oldpresent();
				if(res){
					res = MessageBox(hwnd,"Registry entries from previous versions of frhed were found\n"
						"Should they all be removed ?","Remove frhed",MB_YESNO);
					if(res==IDYES){
						if(ver.dwPlatformId==VER_PLATFORM_WIN32_NT)
							RegDeleteWinNTKey(HKEY_CURRENT_USER,"Software\\frhed");
						else
							RegDeleteKey(HKEY_CURRENT_USER,"Software\\frhed");
					}
				}
			}
		}
#if 0
		for(;;i++){
			RegEnumKey (HKEY_USERS,i,buf,MAX_PATH+1);
			if(res==ERROR_NO_MORE_ITEMS)break;
			if(!stricmp(buf,".Default")){
				if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_USERS,buf,0,KEY_ALL_ACCESS,&hk)){
					removefrhedfromuser(hk);
					RegCloseKey(hk);
				}
			}
		}
#endif // 0
		if(r==r0)MessageBox(hwnd,
			"Now all that remains to remove frhed from this computer is to:\n"
			"1. Quit all other instances of frhed(after turning off \"Save ini...\" in each one)\n"
			"2. Quit this instance of frhed\n"
			"3. Check that the registry data was removed (just in case)\n"
			"4. Delete the directory where frhed currently resides\n"
			"5. If you have an email account please take the time to\n"
			"    email the author/s and give the reason/s why you have\n"
			"    removed frhed from your computer","Remove frhed",MB_OK);
		break;
	case IDM_SAVEINI:
		bSaveIni = !bSaveIni;//!(MF_CHECKED==GetMenuState(hMenu,IDM_SAVEINI,0));
		break;
	case IDM_CHANGEINST:{//switch instance
		//for both the spinners
		//iLoadInst is the max
		//iSaveInst is the min
		//g_iInstCount is the start pos
		//Get & set the ranges
		char num[64]="";int i;
		HKEY hk;
		for(i = 0;;i++){
			sprintf( num, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\%d", i );
			if(ERROR_SUCCESS!=RegOpenKeyEx(HKEY_CURRENT_USER,num,0,KEY_EXECUTE,&hk))//On Error
				break;
			else RegCloseKey(hk);//Close the key - just testing if it exists
		}
		iLoadInst = --i;
		iSaveInst = 0;
		if (iLoadInst==-1){
			MessageBox(hwnd,"No instance data present","Change Instance",MB_OK);
			break;
		}
		//Get the instance to save to when quitting & the instance to load from
		if(DialogBox (hInstance, MAKEINTRESOURCE (IDD_CHANGEINST), hwnd, (DLGPROC) ChangeInstProc)){
			//Set the instance to load
			g_iInstCount = iLoadInst;
			//Read the data
			read_ini_data ();
			//Set the instance to save to when quitting
			g_iInstCount = iSaveInst;
			//Repaint the screen
			repaint ();
		}
		break;
	}
	case IDM_CONTEXT:
		if(MF_CHECKED==GetMenuState(hMenu,IDM_CONTEXT,0)){
			RegDeleteKey( HKEY_CLASSES_ROOT, "*\\shell\\Open in frhed\\command" ); //WinNT requires the key to have no subkeys
			RegDeleteKey( HKEY_CLASSES_ROOT, "*\\shell\\Open in frhed" );
		}
		else{
			HKEY key1;
			LONG res = RegCreateKey( HKEY_CLASSES_ROOT,
				"*\\shell\\Open in frhed\\command",
				&key1 );
			if( res == ERROR_SUCCESS ){
				char cmd[_MAX_PATH];
				strcpy(cmd, _pgmptr );
				strcat(cmd," %1");
				RegSetValue( key1, NULL, REG_SZ, cmd, strlen(cmd));
			}
		}
		break;
	case IDM_UNKNOWN:
		if(MF_CHECKED==GetMenuState(hMenu,IDM_UNKNOWN,0)){
			HKEY hk;
			RegDeleteKey( HKEY_CLASSES_ROOT, "Unknown\\shell\\Open in frhed\\command" ); //WinNT requires the key to have no subkeys
			RegDeleteKey( HKEY_CLASSES_ROOT, "Unknown\\shell\\Open in frhed" );
			if(ERROR_SUCCESS==RegOpenKey(HKEY_CLASSES_ROOT,"Unknown\\shell",&hk)){
				RegDeleteValue(hk,NULL);
				RegCloseKey(hk);
			}
		}
		else{
			HKEY key1;
			LONG res = RegCreateKey( HKEY_CLASSES_ROOT,
				"Unknown\\shell\\Open in frhed\\command",
				&key1 );
			if( res == ERROR_SUCCESS ){
				char cmd[_MAX_PATH];
				strcpy(cmd, _pgmptr );
				strcat(cmd," %1");
				RegSetValue( key1, NULL, REG_SZ, cmd, strlen(cmd));
			}
		}
		break;
	case IDM_DEFAULT:
		if(MF_CHECKED==GetMenuState(hMenu,IDM_DEFAULT,0)){
			HKEY hk;
			if(ERROR_SUCCESS==RegOpenKey(HKEY_CLASSES_ROOT,"Unknown\\shell",&hk)){
				RegDeleteValue(hk,NULL);
				RegCloseKey(hk);
			}
		}
		else
			RegSetValue( HKEY_CLASSES_ROOT, "Unknown\\shell", REG_SZ, "Open in frhed", 13);
		break;
//end
	case IDM_REPLACE:
		CMD_replace();
		break;

//Pabs removed IDM_EXPLORERSETTINGS

	case IDM_OPEN_TEXT:
		CMD_summon_text_edit();
		break;

	case IDM_FINDPREV:
		CMD_findprev();
		break;

	case IDM_FINDNEXT:
		CMD_findnext();
		break;

	case IDM_BMK_COLOR:
		CMD_color_settings( &iBmkColor );
		break;

	case IDM_RESET_COLORS:
		CMD_colors_to_default();
		break;

	case IDM_EDIT_READONLYMODE:
		if( bReadOnly == FALSE )
			bReadOnly = TRUE;
		else
			bReadOnly = FALSE;
		update_for_new_datasize();
		break;

	case IDM_APPLYTEMPLATE:
		CMD_apply_template();
		break;

	case IDM_PARTIAL_OPEN:
		CMD_open_partially ();
		break;

	case IDM_CLEARALL_BMK:
		CMD_clear_all_bmk ();
		break;

	case IDM_REMOVE_BKM:
		CMD_remove_bkm ();
		break;

	case IDM_BOOKMARK1: case IDM_BOOKMARK2: case IDM_BOOKMARK3:
		case IDM_BOOKMARK4: case IDM_BOOKMARK5: case IDM_BOOKMARK6:
		case IDM_BOOKMARK7: case IDM_BOOKMARK8: case IDM_BOOKMARK9:
			CMD_goto_bookmark (cmd);
		break;

	case IDM_ADDBOOKMARK:
		CMD_add_bookmark ();
		break;

	case IDM_MRU1: case IDM_MRU2: case IDM_MRU3: case IDM_MRU4: case IDM_MRU5:
		case IDM_MRU6: case IDM_MRU7: case IDM_MRU8: case IDM_MRU9:
		CMD_MRU_selected (cmd);
		break;

	case IDM_SELECT_BLOCK:
		CMD_select_block ();
		break;

	case IDM_BINARYMODE:
		CMD_binarymode ();
		break;

	case IDM_COMPARE:
		CMD_compare ();
		break;

	case IDM_READFLOAT:
		{
//Pabs optimized - since sprintf returns the string len we can use a pointer, save 46 bytes & not call strcat so many times
			char buf[500],* buf2 = buf;
			buf[0] = 0;
			float floatval;//Pabs replaced 4 with sizeof(floatval)
			if (DataArray.GetLength()-iCurByte >= sizeof(floatval))
			{
				// Space enough for float.
				if (iBinaryMode == LITTLEENDIAN_MODE)
				{
					floatval = *((float*)&(DataArray[iCurByte]));
				}
				else // BIGENDIAN_MODE
				{
					char* pf = (char*) &floatval;
					int i;//Pabs replaced 4 with sizeof(floatval)
					for (i=0; i<sizeof(floatval); i++)//Pabs replaced 3 with sizeof(floatval)-1
						pf[i] = DataArray[iCurByte+(int)sizeof(floatval)-1-i];
				}
				buf2 += sprintf (buf2, "float size value:\n%f\n", floatval);
			}
			else
				buf2 += sprintf (buf2, "Not enough space for float size value.\n");
			double dval;//Pabs replaced 8 with sizeof(dval) - portability
			if (DataArray.GetLength()-iCurByte >= sizeof(dval))
			{
				// Space enough for double.
				if (iBinaryMode == LITTLEENDIAN_MODE)
				{
					dval = *((double*)&(DataArray[iCurByte]));
				}
				else // BIGENDIAN_MODE
				{
					char* pd = (char*) &dval;
					int i;//Pabs replaced 8 with sizeof(dval) - portability
					for (i=0; i<sizeof(dval); i++)//Pabs replaced 7 with (int)sizeof(dval)-1 - portability
						pd[i] = DataArray[iCurByte+(int)sizeof(dval)-1-i];
				}
				buf2 += sprintf (buf2, "\ndouble size value:\n%g\n", dval);
			}
			else
			{
				buf2 += sprintf (buf2, "\nNot enough space for double size value.\n");
//end
			}//Pabs replaced NULL w hwnd
			MessageCopyBox (hwnd, buf, "Floating point values", MB_ICONINFORMATION, hwnd);
			break;//end
		}

	case IDM_PROPERTIES:
		CMD_properties ();
		break;

	case IDM_SELECT_ALL:
		CMD_select_all ();
		break;

	case IDA_BACKSPACE:
		CMD_on_backspace ();
		break;

	case IDA_INSERTMODETOGGLE:
		CMD_toggle_insertmode ();
		break;

	case IDA_DELETEKEY:
		CMD_on_deletekey ();
		break;

	case IDM_CHARACTER_SET:
		CMD_character_set ();
		break;

	case IDM_EDIT_MANIPULATEBITS:
		CMD_manipulate_bits ();
		break;

	case IDM_EDIT_APPEND:
		CMD_edit_append ();
		break;

	case IDM_SELTEXT_COLOR:
		CMD_color_settings( &iSelTextColorValue );
		break;

	case IDM_SELBACK_COLOR:
		CMD_color_settings( &iSelBkColorValue );
		break;

	case IDM_SEP_COLOR:
		CMD_color_settings (&iSepColorValue);
		break;

	case IDM_TEXT_COLOR:
		CMD_color_settings (&iTextColorValue);
		break;

	case IDM_BK_COLOR:
		CMD_color_settings (&iBkColorValue);
		break;

	case IDM_VIEW_SETTINGS:
		CMD_view_settings ();
		break;

	case IDM_INTERNALSTATUS:
		{
			// Remove break for internal information on F2.
//Pabs inserted
#ifndef _DEBUG
			break;
#endif
//end
//Pabs optimized - since sprintf returns the string len we can use a pointer, save 296 bytes & not call strcat so many times
			/*Used to do this
			char buf[4000], buf2[300];
			for each bit of status info{
				sprintf (buf2, "<status string>\n", <status data>);
				strcat(buf,buf2);
			}*/
			char buf[4000],* buf2 = buf;
			buf2 += sprintf (buf2, "Data length: %d\n", DataArray.GetLength ());
			buf2 += sprintf (buf2, "Upper Bound: %d\n", DataArray.GetUpperBound ());
			buf2 += sprintf (buf2, "Data size: %d\n", DataArray.GetSize ());
			buf2 += sprintf (buf2, "cxChar: %d\n", cxChar );
			buf2 += sprintf (buf2, "cyChar: %d\n", cyChar );
			buf2 += sprintf (buf2, "iNumlines: %d\n", iNumlines);
			buf2 += sprintf (buf2, "iCurLine: %d\n", iCurLine);
			buf2 += sprintf (buf2, "iCurByte: %d\n", iCurByte);
			buf2 += sprintf (buf2, "cyBuffer: %d\n", cyBuffer );
//Pabs inserted
			buf2 += sprintf (buf2, "cxBuffer: %d\n", cxBuffer );
//end
			buf2 += sprintf (buf2, "iMRU_count: %d\n", iMRU_count);
			int i;
			for (i=0; i<MRUMAX; i++)
			{
				buf2 += sprintf (buf2, "MRU %d=%s\n", i, &(strMRU[i][0]));
//end
			}//Pabs replaced NULL w hwnd
			MessageBox (hwnd, buf, "Internal status", MB_OK);
			break;//end
		}

	case IDM_EDIT_CUT:
		iCutMode = BST_CHECKED;
		CMD_edit_cut ();
		break;

	case IDM_HELP_TOPICS:
		ShowHtmlHelp ( HELP_CONTENTS, 0, hwnd);
		break;

	case IDM_EDIT_ENTERDECIMALVALUE:
		CMD_edit_enterdecimalvalue ();
		break;

	case IDM_COPY_HEXDUMP:
		CMD_copy_hexdump ();
		break;

	case IDM_EDIT_COPY:
		CMD_edit_copy ();
		break;

	case IDM_PASTE_WITH_DLG:
		CMD_edit_paste ();
		break;

	case IDM_EDIT_PASTE:
		CMD_fast_paste ();
		break;

	case IDM_ABOUT:
		DialogBox (hInstance, MAKEINTRESOURCE (IDD_ABOUTDIALOG), hwnd, (DLGPROC) AboutDlgProc);
		break;

	case IDM_FIND:
		CMD_find ();
		break;

	case IDM_GO_TO:
		CMD_goto();
		break;

	case IDM_CHANGE_MODE:
		character ('\t');
		break;

	case IDM_SAVE_AS:
		CMD_save_as ();
		break;

	case IDM_SAVE:
		CMD_save ();
		break;

	case IDM_EXIT:
		SendMessage (hwnd, WM_CLOSE, 0, 0);
		break;

	case IDM_SCROLL_PRIOR:{
		if(Drive->IsOpen())
			CMD_DriveGotoPrevTrack();
		else
		{
			int icl = iCurLine;
			iCurLine-=cyBuffer;
			if (iCurLine < 0)
				iCurLine = 0;
			if (icl != iCurLine){
				adjust_vscrollbar ();
				repaint ();
			}
		}
	}
	break;

	case IDM_SCROLL_NEXT:{
		if(Drive->IsOpen())
			CMD_DriveGotoNextTrack();
		else
		{
			int icl = iCurLine;
			iCurLine+=cyBuffer;
			if (iCurLine > iNumlines-cyBuffer)
				iCurLine=iNumlines-cyBuffer;
			if (iCurLine < 0)
				iCurLine=0;
			if (icl != iCurLine){
				adjust_vscrollbar();
				repaint();
			}
		}
	}
	break;

	case IDM_SCROLL_UP:
		if (iCurLine > 0)
		{
			iCurLine--;
			adjust_vscrollbar ();
			repaint ();
		}
		break;

	case IDM_SCROLL_DOWN:
		if (iCurLine < iNumlines-cyBuffer)
		{
			iCurLine++;
			adjust_vscrollbar ();
			repaint ();
		}
		break;

	case IDM_SCROLL_LEFT:
		if (iHscrollPos > 0)
		{
			iHscrollPos--;
			adjust_hscrollbar ();
			repaint ();
		}
		break;

	case IDM_SCROLL_RIGHT:
		if (iHscrollPos < iCharsPerLine-cxBuffer)
		{
			iHscrollPos++;
			adjust_hscrollbar ();
			repaint();
		}
		break;

	case IDM_OPEN:
		CMD_open ();
		break;

	case IDM_NEW:
		CMD_new ();
		break;

	//GK20AUG2K
	case ID_MISC_GOTO_DLL_EXPORTS:
		CMD_GotoDllExports();
		break;
	case ID_MISC_GOTO_DLL_IMPORTS:
		CMD_GotoDllImports();
		break;
	case ID_MISC_ENCODEDECODE:
		CMD_EncodeDecode();
		break;
	case ID_DISK_OPEN_DRIVE:
		CMD_OpenDrive();
		break;
	case ID_DISK_OPEN_NDAS:
		CMD_OpenNDAS();
		break;
	case ID_DISK_NDAS_INFO:
		CMD_InfoNDAS();
		break;
	case ID_SET_NDAS_CRC32:
		CMD_NDAS_CRC32();
		break;
	case ID_DISK_CLOSEDRIVE:
		CMD_CloseDrive();
		break;
	case ID_DISK_GOTOFIRSTTRACK:
		CMD_DriveGotoFirstTrack();
		break;
	case ID_DISK_GOTONEXTTRACK:
		CMD_DriveGotoNextTrack();
		break;
	case ID_DISK_GOTOPREVIOUSTRACK:
		CMD_DriveGotoPrevTrack();
		break;
	case ID_DISK_GOTOLASTTRACK:
		CMD_DriveGotoLastTrack();
		break;
	case ID_DISK_GOTOTRACK:
		CMD_DriveGotoTrackNumber();
		break;
	case ID_DISK_SAVETRACK:
		CMD_DriveSaveTrack();
		break;

	default:
		{//Pabs replaced NULL w hwnd
			char buf[500];
			sprintf (buf, "Unknown COMMAND-ID %d.", cmd);
			MessageBox (hwnd, buf, "frhed ERROR", MB_OK);
		}//end
		break;
	}
	return 0;
}


//--------------------------------------------------------------------------------------------
int HexEditorWindow::destroy_window ()
{
	DragAcceptFiles(hwnd,FALSE);
	RevokeDragDrop(hwnd);
	if (target) CoLockObjectExternal(target, FALSE, TRUE);
	return 0;
}

//--------------------------------------------------------------------------------------------
// Set the window title and the statusbar text.
//Pabs hugely revamped for less code
void HexEditorWindow::set_wnd_title()
{
	char buf[512];

	if (strlen (filename) != 0)
	{
		// Change window title.
		if (bFilestatusChanged)
		{
			sprintf (buf, "[%s", filename);
			if (m_iFileChanged == TRUE)
				strcat (buf, " *");
			strcat (buf, "]");
			if (bPartialOpen==TRUE)
				strcat (buf, " - P");
			strcat (buf, " - frhed");
			SetWindowText (hwndMain, buf);
			bFilestatusChanged = FALSE;
		}
//Pabs changed - buf2 declaration moved so that selection bugfix works properly
		char buf2[80];
//end
		// Selection going on.
		if (bSelected == TRUE)
		{
			if (iEndOfSelection >= iStartOfSelection)
			{
				sprintf (buf, "Selected: Offset %d=0x%x to %d=0x%x (%d byte(s))", iStartOfSelection, iStartOfSelection,
					iEndOfSelection, iEndOfSelection, iEndOfSelection-iStartOfSelection+1);
			}
			else
			{
				sprintf (buf, "Selected: Offset %d=0x%x to %d=0x%x (%d byte(s))", iEndOfSelection, iEndOfSelection,
					iStartOfSelection, iStartOfSelection, iStartOfSelection-iEndOfSelection+1);
			}
			SendMessage (hwndStatusBar, SB_SETTEXT, 0, (LPARAM) buf);
		}
		else // Normal display.
		{
//Pabs changed - \t removed from end of string literal ("Offset %d=0x%x\t" -> "Offset %d=0x%x")
			sprintf (buf, "Offset %d=0x%x", iCurByte, iCurByte);
//end
			int wordval, longval;
//Pabs changed - buf2 declaration moved so that selection bugfix works properly
			//char buf2[80];
//end
//Pabs changed - line insert
			if (DataArray.GetLength()-iCurByte > 0){//if we are not looking at the End byte
				// R. Kibria: changed the output slightly (used to be "Bits = 0b").
				strcat (buf, "   Bits=");//append stuff to status text
				unsigned char zzz=DataArray[iCurByte];//quicker to have a tmp var than to call operator[] 8 times
				int i;
				for(i=0;i<8;i++)buf2[i]=((zzz>>i)&0x1?'1':'0');//Extract bits
				for(i=0;i<4;i++)swap(buf2[i],buf2[7-i]);//flip order-for some reason it doesn't display correctly going i-- or i++ in for loop
				buf2[8]='\0';//terminate string
				strcat (buf, buf2);//append to status text
			}
			strcat (buf, "\t");//add that \t back on to the status text
//end
			if (bUnsignedView) // Values signed/unsigned?
			{
				// UNSIGNED
				if (iBinaryMode == LITTLEENDIAN_MODE)
				{
					// UNSIGNED LITTLEENDIAN_MODE
					// Decimal value of byte.
					if (DataArray.GetLength ()-iCurByte >= 1)
					{
						sprintf (buf2, "\tUnsigned: B:%u", (unsigned int) DataArray[iCurByte]);
						strcat (buf, buf2);
					}
					else
					{
						sprintf (buf2, "\tEND");
						strcat (buf, buf2);
					}
					// Space enough for a word?
					if (DataArray.GetLength ()-iCurByte >= 2)
					{
						// Space enough for a word.
						wordval = (DataArray[iCurByte+1] << 8) | DataArray[iCurByte];
						sprintf (buf2, ",W:%u", (unsigned int) wordval);
						strcat (buf, buf2);
					}
					if (DataArray.GetLength ()-iCurByte >= 4)
					{
						// Space enough for a longword.
						longval = wordval | (((DataArray[iCurByte + 3] << 8) | DataArray[iCurByte + 2]) << 16);
						sprintf (buf2, ",L:%u", (unsigned int) longval);
						strcat (buf, buf2);
					}
				}
				else
				{
					// UNSIGNED BIGENDIAN_MODE
					// Decimal value of byte.
					if (DataArray.GetLength ()-iCurByte >= 1)
					{
						sprintf (buf2, "\tUnsigned: B:%u", (unsigned int) DataArray[iCurByte]);
						strcat (buf, buf2);
					}
					else
					{
						sprintf (buf2, "\tEND");
						strcat (buf, buf2);
					}
					// Space enough for a word?
					if (DataArray.GetLength ()-iCurByte >= 2)
					{
						// Space enough for a word.
						wordval = (DataArray[iCurByte] << 8) | DataArray[iCurByte+1];
						sprintf (buf2, ",W:%u", (unsigned int) wordval);
						strcat (buf, buf2);
					}
					if (DataArray.GetLength ()-iCurByte >= 4)
					{
						// Space enough for a longword.
						longval = (wordval<<16) | (DataArray[iCurByte+2]<<8) | (DataArray[iCurByte+3]);
						sprintf (buf2, ",L:%u", (unsigned int) longval);
						strcat (buf, buf2);
					}
				}
			}
			else // SIGNED
			{
				if (iBinaryMode == LITTLEENDIAN_MODE)
				{
					// SIGNED LITTLEENDIAN_MODE
					// Decimal value of byte.
					if (DataArray.GetLength ()-iCurByte >= 1)
					{
						sprintf (buf2, "\tSigned: B:%d", (int) (signed char) DataArray[iCurByte]);
						strcat (buf, buf2);
					}
					else
					{
						sprintf (buf2, "\tEND");
						strcat (buf, buf2);
					}
					// Space enough for a word?
					if (DataArray.GetLength ()-iCurByte >= 2)
					{
						// Space enough for a word.
						wordval = (DataArray[iCurByte + 1] << 8) | DataArray[iCurByte];
						sprintf (buf2, ",W:%d", (int) (signed short) wordval);
						strcat (buf, buf2);
					}
					if (DataArray.GetLength ()-iCurByte >= 4)
					{
						// Space enough for a longword.
						longval = wordval | (((DataArray[iCurByte + 3] << 8) | DataArray[iCurByte + 2]) << 16);
						sprintf (buf2, ",L:%d", (signed int) longval);
						strcat (buf, buf2);
					}
				}
				else
				{
					// SIGNED BIGENDIAN_MODE
					// Decimal value of byte.
					if (DataArray.GetLength ()-iCurByte >= 1)
					{
						sprintf (buf2, "\tSigned: B:%d", (signed char) DataArray[iCurByte]);
						strcat (buf, buf2);
					}
					else
					{
						sprintf (buf2, "\tEND");
						strcat (buf, buf2);
					}
					// Space enough for a word.
					if (DataArray.GetLength ()-iCurByte >= 2)
					{
						// Space enough for a longword.
						wordval = (DataArray[iCurByte] << 8) | DataArray[iCurByte+1];
						sprintf (buf2, ",W:%d", (int) (signed short) wordval);
						strcat (buf, buf2);
					}
					if (DataArray.GetLength ()-iCurByte >= 4)
					{
						// Space enough for a longword.
						longval = (wordval<<16) | (DataArray[iCurByte+2]<<8) | (DataArray[iCurByte+3]);
						sprintf (buf2, ",L:%d", (signed int) longval);
						strcat (buf, buf2);
					}
				}
			}
			SendMessage (hwndStatusBar, SB_SETTEXT, 0, (LPARAM) buf);
//Pabs inserted - bugfix - before set_wnd_title wouldn't set the text for the last two part if selection is going on
		}
//end - some lines
			// Character set, input mode or read-only, binary mode.
			switch (iCharacterSet)
			{
			case ANSI_FIXED_FONT:
				sprintf (buf, "\tANSI");
				break;

			case OEM_FIXED_FONT:
				sprintf (buf, "\tOEM");
				break;
			}

			if( bReadOnly )
			{
				sprintf (buf2, " / READ");
				strcat (buf, buf2);
			}
			else if( iInsertMode )
			{
				sprintf (buf2, " / INS");
				strcat (buf, buf2);
			}
			else
			{
				sprintf (buf2, " / OVR");
				strcat (buf, buf2);
			}
			if (iBinaryMode == LITTLEENDIAN_MODE)
			{
				sprintf (buf2, " / L"); // Intel
				strcat (buf, buf2);
			}
			else if (iBinaryMode == BIGENDIAN_MODE)
			{
				sprintf (buf2, " / B"); // Motorola
				strcat (buf, buf2);
			}
			SendMessage (hwndStatusBar, SB_SETTEXT, 1, (LPARAM) buf);

			// File size.
			sprintf (buf, "\tSize: %u", DataArray.GetLength ());
			SendMessage (hwndStatusBar, SB_SETTEXT, 2, (LPARAM) buf);
//Pabs removed '}'
	}
	else
	{
		SetWindowText (hwndMain, "frhed");
		SendMessage (hwndStatusBar, WM_SETTEXT, 0, (LPARAM) "No file loaded");
	}
}

//--------------------------------------------------------------------------------------------
// Set Caret position.
void HexEditorWindow::set_caret_pos ()
{
	if (bSelected)
	{
		SetCaretPos (-cxChar, -cyChar );
		return;
	}

	int iCaretLine = iCurByte / iBytesPerLine,
		iBottomLine = iCurLine + cyBuffer - 1;

	switch (m_iEnteringMode)
	{
	case CHARS:
		if (iCaretLine >= iCurLine && iCaretLine <= iBottomLine && filename[0] != '\0')
		{
			int y = iCaretLine - iCurLine,
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
				x = iMaxOffsetLen+iByteSpace+iBytesPerLine*3+iCharSpace
//end
				- iHscrollPos + (iCurByte%iBytesPerLine);
			SetCaretPos ( x*cxChar, y*cyChar );
		}
		else
			SetCaretPos ( -cxChar, -cyChar );
		break;
	case BYTES:
		// If caret in vertical visible area...
		if (iCaretLine >= iCurLine && iCaretLine <= iBottomLine && filename[0] != '\0')
		{
			int y = iCaretLine - iCurLine,
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
				x = iMaxOffsetLen+iByteSpace + (iCurByte%iBytesPerLine)*3 - iHscrollPos + iCurNibble;
//end
			SetCaretPos ( x*cxChar, y*cyChar );
		}
		else
			SetCaretPos ( -cxChar, -cyChar );
	}
}

//--------------------------------------------------------------------------------------------
// Repaints the whole window.
int HexEditorWindow::repaint( int from, int to )
{
	HideCaret( hwnd );
	iUpdateLine = from; iUpdateToLine = to;
	InvalidateRect( hwnd, NULL, FALSE );
	UpdateWindow( hwnd );
	ShowCaret( hwnd );
	return 0;
}
int HexEditorWindow::repaint( int line )
{
	return repaint(line, line);
}

//--------------------------------------------------------------------------------------------
// Clear everything up.
void HexEditorWindow::clear_all ()
{
//Pabs changed "iOffsetLen" replaced with "iMinOffsetLen = iMaxOffsetLen" and '8' with '6'
	iMaxOffsetLen = iMinOffsetLen = 6;
//end
	iByteSpace = 2;
	iBytesPerLine = 16;
	iCharSpace = 1;
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	iCharsPerLine = iMaxOffsetLen + iByteSpace + iBytesPerLine*3 + iCharSpace + iBytesPerLine;
//end
	DataArray.ClearAll ();
	filename[0] = '\0';
	iVscrollMax = 0;
	iVscrollPos = 0;
	iVscrollInc = 0;
	iHscrollMax = 0;
	iHscrollPos = 0;
	iHscrollInc = 0;
	iCurLine = 0;
	iCurByte = 0;
	iCurNibble = 0;
}

//--------------------------------------------------------------------------------------------
// Set the vertical scrollbar position.
void HexEditorWindow::adjust_vscrollbar ()
{
	iVscrollPos = (int) ((float)iCurLine * ((float)iVscrollMax)/(float)(iNumlines-1));
	SI.fMask  = SIF_POS | SIF_DISABLENOSCROLL;
	SI.nPos   = iVscrollPos;
	SetScrollInfo(hwnd, SB_VERT, &SI, true);
}

//--------------------------------------------------------------------------------------------
// Set the horizontal scrollbar position.
void HexEditorWindow::adjust_hscrollbar ()
{
	SI.fMask  = SIF_POS | SIF_DISABLENOSCROLL;
	SI.nPos   = iHscrollPos;
	SetScrollInfo(hwnd, SB_HORZ, &SI, true);
}

//--------------------------------------------------------------------------------------------
// Highlight (invert) the character/byte at the current offset.
void HexEditorWindow::mark_char (HDC hdc)
{
	if( bDontMarkCurrentPos )
		return;

	if (bSelected)
	{
		SetCaretPos (-cxChar, -cyChar);
		return;
	}

	int DC_was_allocated = FALSE;
	if (hdc == 0)
	{
		hdc = GetDC (hwnd);
		DC_was_allocated = TRUE;
	}

	int chpos;
	RECT r;
	switch (m_iEnteringMode)
	{
	case CHARS:
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		chpos = iMaxOffsetLen + iByteSpace + (iCurByte%iBytesPerLine)*3 - iHscrollPos;
//end
		r.left = chpos * cxChar;
		r.top = (iCurByte/iBytesPerLine-iCurLine)*cyChar;
		r.right = r.left + 2*cxChar;
		r.bottom = (iCurByte/iBytesPerLine-iCurLine+1)*cyChar;
		InvertRect (hdc, &r);
		break;
	case BYTES:
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		chpos = iMaxOffsetLen + iByteSpace + iBytesPerLine*3 + iCharSpace
//end
			+ (iCurByte % iBytesPerLine) - iHscrollPos;
		r.left = chpos * cxChar;
		r.top = (iCurByte/iBytesPerLine-iCurLine)*cyChar;
		r.right = (chpos+1)*cxChar;
		r.bottom = (iCurByte/iBytesPerLine-iCurLine+1)*cyChar;
		InvertRect (hdc, &r);
	}

	if (DC_was_allocated)
		ReleaseDC (hwnd, hdc);
}

//--------------------------------------------------------------------------------------------
// Repaint one line in the window.
void HexEditorWindow::print_line (HDC hdc, int line, char* linbuf, HBRUSH hbr )
{
	// line = absolute line number.
	// if line not visible:
	if (line < iCurLine || line > iCurLine + cyBuffer)
		return;
	int startpos = line * iBytesPerLine, endpos, i = 0, m;
	char buf[80], c;

	// Return if this line does not even contain the end-of-file double
	// underscore (at index upperbound+1).
	if( startpos > DataArray.GetUpperBound() + 1 )
	{
		return;
	}

	// Write offset.

//Pabs revamped to fix the display glitch
//can now have any offsetlen up to INT_MAX & the bug where offsets are drawn over data is fixed

	sprintf (buf, "%%%d.%dx", iMinOffsetLen, iMinOffsetLen);

	i = sprintf (linbuf, buf, bPartialStats ? startpos + iPartialOffset : startpos );

	iMaxOffsetLen += iByteSpace;

	memset(linbuf+i,' ',iMaxOffsetLen-i);

	iMaxOffsetLen -= iByteSpace;
//end

	// Last line reached? Then only write rest of bytes.
	// startpos+iBytesPerLine-1 = Last byte in current line.
	if (startpos+iBytesPerLine > DataArray.GetLength ())
	{
		// If the first byte of the next line would not be valid, then
		// only print the bytes up to and including the last byte of the file.
		endpos = DataArray.GetUpperBound()+1;
	}
	else
	{
		// Print the bytes up to the end of this line, they are all valid.
		endpos = startpos+iBytesPerLine-1;
	}
	// Could happen on arrow down, so that last line is on bottom of window:
	if( endpos < startpos )
	{
		endpos = startpos;
	}
	// Write bytes.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	m = iMaxOffsetLen+iByteSpace; // Index of first byte in line.
//end
	for (i=startpos; i<=endpos; i++)
	{
		if (i == DataArray.GetLength())
		{
			linbuf[m++] = '_';
			linbuf[m++] = '_';
			linbuf[m++] = ' ';
		}
		else
		{
			c = (char)(DataArray[i] >> 4);
			if( c < 10 )
				c += '0';
			else
				c = (char)(c - 10 + 'a');
			linbuf[m++] = c;
			c = (char)(DataArray[i] & 0x0f);
			if( c < 10 )
				c += '0';
			else
				c = (char)(c - 10 + 'a');
			linbuf[m++] = c;
			linbuf[m++] = ' ';
		}
	}
	// Write spaces for non-existant bytes.
	if (endpos-startpos < iBytesPerLine-1)
	{
		for (i=0; i<iBytesPerLine-1-(endpos-startpos); i++)
		{
			linbuf[m++] = ' ';
			linbuf[m++] = ' ';
			linbuf[m++] = ' ';
		}
	}

	// Write offset to chars.
	for (i=0; i<iCharSpace; i++)
		linbuf[m++] = ' ';

	// Write ASCIIs.
	for (i=startpos; i<=endpos; i++)
	{
		if (i == DataArray.GetLength())
		{
			linbuf[m++] = ' ';
		}
		else if (iCharacterSet == OEM_FIXED_FONT && DataArray[i]!=0)
		{
			linbuf[m++] = DataArray[i];
		}
		else if ((DataArray[i]>=32 && DataArray[i]<=126) || (DataArray[i]>=160 && DataArray[i]<=255) || (DataArray[i]>=145 && DataArray[i]<=146))
		{
			linbuf[m++] = DataArray[i];
		}
		else
		{
			linbuf[m++] = '.';
		}
	}

	// Write spaces for nonexisting chars.
	if (endpos-startpos < iBytesPerLine-1)
		for (i=0; i<iBytesPerLine-1-(endpos-startpos); i++)
			linbuf[m++] = ' ';


	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//int line_len = m; // Length of the line in chars.

	// Set normal text colors.
	iBkColor = PALETTERGB (GetRValue(iBkColorValue),GetGValue(iBkColorValue),GetBValue(iBkColorValue));
	iTextColor = PALETTERGB (GetRValue(iTextColorValue),GetGValue(iTextColorValue),GetBValue(iTextColorValue));
	SetTextColor (hdc, iTextColor);
	SetBkColor (hdc, iBkColor);

	// How much of offset and byte-space is visible? Print it in normal text colors.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	if( iHscrollPos < iMaxOffsetLen + iByteSpace )
//end
	{
		// A part of offset+byte-space is visible.
		// Write offset to screen.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		TextOut( hdc, 0, ( line - iCurLine ) * cyChar, linbuf + iHscrollPos, iMaxOffsetLen + iByteSpace - iHscrollPos );
//end
	}

//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	int iHexStart = iMaxOffsetLen + iByteSpace;
//end
	//int iHexXStart = iHexStart * cxChar;

	// Write char-space, if it is visible.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	if( iHscrollPos < iMaxOffsetLen + iByteSpace + iBytesPerLine * iHexWidth )
//end
	{
		// Char-space is visible.
		TextOut( hdc,
			( iHexStart + iBytesPerLine * iHexWidth - iHscrollPos ) * cxChar,
			( line - iCurLine ) * cyChar,
			linbuf + iHexStart + iBytesPerLine * iHexWidth,
			iCharSpace );
	}

	iSelBkColor = PALETTERGB (GetRValue(iSelBkColorValue),GetGValue(iSelBkColorValue),GetBValue(iSelBkColorValue));
	iSelTextColor = PALETTERGB (GetRValue(iSelTextColorValue),GetGValue(iSelTextColorValue),GetBValue(iSelTextColorValue));
	BOOL last_normtext = TRUE;

	int p, el = startpos + iBytesPerLine - 1 - 1, s, e;
	for( p = startpos; p <= el; p++ )
	{
		// Write hex, if it is visible.
		// s = Position in string of last character of current hex.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		s = iMaxOffsetLen + iByteSpace + ( p - startpos + 1 ) * iHexWidth;
//end
		e = s - iHexWidth;
		// Print only if at least a part of the hex is visible.
		if( iHscrollPos < s && iHscrollPos + cxBuffer >= e )
		{
			// Selected bytes must be printed in selection colors.
			if( bSelected && IN_BOUNDS( p, iStartOfSelection, iEndOfSelection ) )
			{
				if( last_normtext )
				{
					// Set selection colors.
					SetTextColor (hdc, iSelTextColor);
					SetBkColor (hdc, iSelBkColor);
					last_normtext = FALSE;
				}
			}
			else
			{
				if( !last_normtext )
				{
					// Set normal text colors.
					SetTextColor (hdc, iTextColor);
					SetBkColor (hdc, iBkColor);
					last_normtext = TRUE;
				}
			}
			// Hex is visible.
			TextOut( hdc,
				( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar,
				( line - iCurLine ) * cyChar,
				linbuf + iHexStart + ( p - startpos ) * iHexWidth,
				iHexWidth );
		}

//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		s = iMaxOffsetLen + iByteSpace + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos + 1);
//end
		// Write char, if it is visible.
		if( iHscrollPos < s && iHscrollPos + cxBuffer >= s - 1 )
		{
			// Selected bytes must be printed in selection colors.
			if( bSelected && IN_BOUNDS( p, iStartOfSelection, iEndOfSelection ) )
			{
				if( last_normtext )
				{
					// Set selection colors.
					SetTextColor (hdc, iSelTextColor);
					SetBkColor (hdc, iSelBkColor);
					last_normtext = FALSE;
				}
			}
			else
			{
				if( !last_normtext )
				{
					// Set normal text colors.
					SetTextColor (hdc, iTextColor);
					SetBkColor (hdc, iBkColor);
					last_normtext = TRUE;
				}
			}
			// Char is visible.
			TextOut( hdc,
				( iHexStart + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos ) - iHscrollPos ) * cxChar,
				( line - iCurLine ) * cyChar,
				linbuf + iHexStart + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos ),
				1 );
		}
	}

	// The last hex in the line is not completely in selection colors. It's
	// succeding space must be printed in normal text colors (visually more
	// appealing).
	// Write hex, if it is visible.
	// s = Position in string of last character of current hex.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	s = iMaxOffsetLen + iByteSpace + ( p - startpos + 1 ) * iHexWidth;
//end
	e = s - iHexWidth;
	// Print only if at least a part of the hex is visible.
	if( iHscrollPos < s && iHscrollPos + cxBuffer >= e )
	{
		// Selected bytes must be printed in selection colors.
		if( bSelected && IN_BOUNDS( p, iStartOfSelection, iEndOfSelection ) )
		{
			if( last_normtext )
			{
				// Output the last space first.
				TextOut( hdc,
					( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar + 2 * cxChar,
					( line - iCurLine ) * cyChar,
					" ",
					1 );
				// Set selection colors.
				SetTextColor (hdc, iSelTextColor);
				SetBkColor (hdc, iSelBkColor);
				last_normtext = FALSE;
				last_normtext = TRUE;
				// Write hex.
				TextOut( hdc,
					( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar,
					( line - iCurLine ) * cyChar,
					linbuf + iHexStart + ( p - startpos ) * iHexWidth,
					iHexWidth - 1 );
			}
			else
			{
				// Write hex.
				TextOut( hdc,
					( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar,
					( line - iCurLine ) * cyChar,
					linbuf + iHexStart + ( p - startpos ) * iHexWidth,
					iHexWidth - 1 );
				// Set normal text colors.
				SetTextColor (hdc, iTextColor);
				SetBkColor (hdc, iBkColor);
				last_normtext = TRUE;
				// Output the last space.
				TextOut( hdc,
					( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar + 2 * cxChar,
					( line - iCurLine ) * cyChar,
					" ",
					1 );
			}
		}
		else
		{
			// Non-selected hex.
			if( !last_normtext )
			{
				// Set normal text colors.
				SetTextColor (hdc, iTextColor);
				SetBkColor (hdc, iBkColor);
				last_normtext = TRUE;
			}
			// Hex is visible.
			TextOut( hdc,
				( iHexStart + ( p - startpos ) * iHexWidth - iHscrollPos ) * cxChar,
				( line - iCurLine ) * cyChar,
				linbuf + iHexStart + ( p - startpos ) * iHexWidth,
				iHexWidth );
		}
	}

//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
	s = iMaxOffsetLen + iByteSpace + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos + 1);
//end
	// Write char, if it is visible.
	if( iHscrollPos < s && iHscrollPos + cxBuffer >= s - 1 )
	{
		// Selected bytes must be printed in selection colors.
		if( bSelected && IN_BOUNDS( p, iStartOfSelection, iEndOfSelection ) )
		{
			if( last_normtext )
			{
				// Set selection colors.
				SetTextColor (hdc, iSelTextColor);
				SetBkColor (hdc, iSelBkColor);
			}
		}
		else
		{
			if( !last_normtext )
			{
				// Set normal text colors.
				SetTextColor (hdc, iTextColor);
				SetBkColor (hdc, iBkColor);
			}
		}
		// Char is visible.
		TextOut( hdc,
			( iHexStart + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos ) - iHscrollPos ) * cxChar,
			( line - iCurLine ) * cyChar,
			linbuf + iHexStart + iBytesPerLine * iHexWidth + iCharSpace + ( p - startpos ),
			1 );
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Separators.
	for (i = 0; i < (iBytesPerLine / 4) + 1; i++)
	{
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
		m = (iMaxOffsetLen+iByteSpace)*cxChar - cxChar/2 + 3*cxChar*4*i - cxChar*iHscrollPos;
//end
		MoveToEx (hdc, m, (line-iCurLine)*cyChar, NULL);
		LineTo (hdc, m, (line-iCurLine+1)*cyChar);
	}
	// Separator for chars.
	m = CHARSTART*cxChar - cxChar*iHscrollPos - 2;
	MoveToEx (hdc, m, (line-iCurLine)*cyChar, NULL);
	LineTo (hdc, m, (line-iCurLine+1)*cyChar);
	// Second separator.
	MoveToEx (hdc, m+2, (line-iCurLine)*cyChar, NULL);
	LineTo (hdc, m+2, (line-iCurLine+1)*cyChar);

	// Print bookmark indicators.
	// Are there bookmarks in this line?
	el = startpos + iBytesPerLine - 1;
	int chpos;
	RECT r;
	// Brush for bookmark borders.
	for( i = 0; i < iBmkCount; i++ )
	{
		// Print the bookmark if it is within the file.
		if( IN_BOUNDS( pbmkList[i].offset, startpos, el ) && pbmkList[i].offset <= DataArray.GetUpperBound() )
		{
			// Found a bookmark in this line.
			// Mark hex.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
			chpos = iMaxOffsetLen + iByteSpace + ( pbmkList[i].offset % iBytesPerLine )*3 - iHscrollPos;
//end
			r.left = chpos * cxChar;
			r.top = ( pbmkList[i].offset / iBytesPerLine - iCurLine ) * cyChar;
			r.right = r.left + 2*cxChar;
			r.bottom = ( pbmkList[i].offset / iBytesPerLine - iCurLine + 1 ) * cyChar;
			FrameRect( hdc, &r, hbr );

			// Mark char.
//Pabs replaced "iOffsetLen" with "iMaxOffsetLen"
			chpos = iMaxOffsetLen + iByteSpace + iBytesPerLine*3 + iCharSpace
//end
				+ ( pbmkList[i].offset % iBytesPerLine ) - iHscrollPos;
			r.left = chpos * cxChar;
			r.top = ( pbmkList[i].offset / iBytesPerLine - iCurLine) * cyChar;
			r.right = ( chpos + 1 ) * cxChar;
			r.bottom = ( pbmkList[i].offset / iBytesPerLine - iCurLine + 1 ) * cyChar;
			FrameRect( hdc, &r, hbr );
		}
	}
	return;
}

//--------------------------------------------------------------------------------------------
// Dialogue procedure for Go-to dialogue.
BOOL CALLBACK GoToDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		if( iGotoDlgBufLen == 0 )
		{
			// Maximal length of text in Edit-Control:
			iGotoDlgBufLen = SendMessage( GetDlgItem( hDlg, IDC_EDIT1 ), EM_GETLIMITTEXT, 0, 0 );
			// Memory for returning text to caller.
			pcGotoDlgBuffer = new char[iGotoDlgBufLen];
			if( pcGotoDlgBuffer == NULL )//Pabs replaced NULL w hDlg
				MessageBox( hDlg, "Could not allocate Goto buffer.", "Go to error", MB_OK | MB_ICONERROR );
			memset( pcGotoDlgBuffer, 0, iGotoDlgBufLen );//end
		}
		if( pcGotoDlgBuffer != NULL )
			SetWindowText( GetDlgItem (hDlg, IDC_EDIT1), pcGotoDlgBuffer );
		SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			// Copy text in Edit-Control.
			EndDialog( hDlg, GetDlgItemText( hDlg, IDC_EDIT1, pcGotoDlgBuffer, iGotoDlgBufLen ) );
			return TRUE;

		case IDCANCEL:
			EndDialog( hDlg, 0 );
			return TRUE;
		}
		break;
	}
	return FALSE;
}


//--------------------------------------------------------------------------------------------
// Dialogue procedure for Find dialogue.
BOOL CALLBACK FindDlgProc( HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );

	switch( iMsg )
	{
	case WM_INITDIALOG:
		// Is the buffer containing the data to find from the last find command empty?
		// if( iFindDlgBufLen == 0 )
		{
			// Maximal Length of text in Edit-Control:
			// iFindDlgBufLen = SendMessage( GetDlgItem( hDlg, IDC_EDIT1 ), EM_GETLIMITTEXT, 0, 0 );
			// SendMessage( GetDlgItem( hDlg, IDC_EDIT1 ), EM_GETLIMITTEXT, 0, 0 );

			// Memory for returning text to caller.
			// pcFindDlgBuffer = new char[iFindDlgBufLen];
		}
		SetFocus( GetDlgItem( hDlg, IDC_EDIT1 ) );
		// if( iFindDlgLastLen != 0 )
		{
			int res = SendMessage( GetDlgItem( hDlg, IDC_EDIT1 ), EM_SETLIMITTEXT, iFindDlgBufLen, 0 );
			res = SetWindowText( GetDlgItem( hDlg, IDC_EDIT1 ), pcFindDlgBuffer );
		}

		//GK16AUG2K
		if (iFindDlgDirection == -1 )
			CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
		else
			CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);

		if( iFindDlgMatchCase )
			CheckDlgButton( hDlg, IDC_CHECK1, BST_CHECKED );

		if( iFindDlgUnicode )
			CheckDlgButton( hDlg, IDC_CHECK4, BST_CHECKED );

		return FALSE;

	case WM_COMMAND:
		//GK16AUG2K
		if( HIWORD(wParam) == BN_CLICKED )
		{
			iFindDlgMatchCase = IsDlgButtonChecked (hDlg, IDC_CHECK1);

			//GK16AUG2K: UNICODE search
			iFindDlgUnicode = (IsDlgButtonChecked (hDlg, IDC_CHECK4)== BST_CHECKED);

			if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
				iFindDlgDirection = -1;
			else
				iFindDlgDirection = 1;

			switch (LOWORD (wParam))
			{
			case IDOK:
				// Copy text in Edit-Control. Return the number of characters
				// in the Edit-control minus the zero byte at the end.
				EndDialog (hDlg, GetDlgItemText (hDlg, IDC_EDIT1, pcFindDlgBuffer, iFindDlgBufLen));
				return TRUE;
			case IDCANCEL:
				EndDialog (hDlg, 0);
				return TRUE;
			}
		}
		break;
	}
	return FALSE;
}

//--------------------------------------------------------------------------------------------
// Set horizontal scroll position so that caret is visible.
void HexEditorWindow::adjust_view_for_caret ()
{
	int log_column;
	if (m_iEnteringMode == BYTES)
		log_column = BYTES_LOGICAL_COLUMN;
	else
		log_column = CHARS_LOGICAL_COLUMN;

//Pabs changed to put cursor in center of screen
	if ( log_column >= iHscrollPos + cxBuffer
	  || log_column < iHscrollPos )
		iHscrollPos = log_column - cxBuffer/2;
	if( iHscrollPos > iHscrollMax - cxBuffer + 1 )
		iHscrollPos = iHscrollMax - cxBuffer + 1;
	if( iHscrollPos < 0 ) iHscrollPos = 0;
	adjust_hscrollbar ();
	if( BYTELINE < iVscrollPos
	 || BYTELINE > iVscrollPos + cyBuffer - 1 )
		iCurLine = BYTELINE - cyBuffer/2;
	if( iCurLine > iVscrollMax - cyBuffer + 1 )
		iCurLine = iVscrollMax - cyBuffer + 1;
	if( iCurLine < 0 ) iCurLine = 0;
	adjust_vscrollbar ();
//end
}

//--------------------------------------------------------------------------------------------
// Initialize main menu items.
int HexEditorWindow::initmenupopup( WPARAM w, LPARAM l )
{
	HMENU h = (HMENU) w;
//Pabs changed this function because it is now possible
//to change the buffer size in partial open mode
//Pabs inserted - easier to add new menus
	int ii = 0;
	// Submenu "File":
//Pabs made it easier to add new menus - all nums replaced with ii++
	if( l == ii++ )
	{
		//GK16AUG2K: for WindowsBlinds WM_INITMENUPOPUP
		// The GetMenuItemID function retrieves the menu item identifier of a menu item
		// located at the specified position in a menu.
		if( GetMenuItemID(h,0) != IDM_NEW )
			return 0;
//Pabs changed - line insert
		//Revert is allowed when the file has been changed
		EnableMenuItem( h, IDM_REVERT, m_iFileChanged ? MF_ENABLED : MF_GRAYED );
		//Save selection as is always allowed
		EnableMenuItem( h, IDM_SAVESELAS, MF_ENABLED );
		//Delete file is allowed when the file has been saved and read only mode is off
		EnableMenuItem( h, IDM_DELETEFILE, ( !bFileNeverSaved && !bReadOnly ) ? MF_ENABLED : MF_GRAYED );
		//Insert file is allowed when read only mode is off
		// RK: ...and there is no selection going on. - Pabs removed this to allow replacement of selection with the incoming file
		EnableMenuItem( h, IDM_INSERTFILE, ( !bReadOnly ) ? MF_ENABLED : MF_GRAYED );
		// "Import Hexdump" is always allowed.
		EnableMenuItem( h, IDM_OPEN_HEXDUMP, MF_ENABLED );
//end
		// "New file" is always possible.
		EnableMenuItem( h, IDM_NEW, MF_ENABLED );

		// "Open" and "Open Partially" are always allowed.
		EnableMenuItem( h, IDM_OPEN, MF_ENABLED );
		EnableMenuItem( h, IDM_PARTIAL_OPEN, MF_ENABLED );

		// "Save" is always possible.
		EnableMenuItem( h, IDM_SAVE, MF_ENABLED );

		// "Save as" is always possible.
		EnableMenuItem( h, IDM_SAVE_AS, MF_ENABLED );

		// Hexdump is only possible if the file isn't empty.
		if( DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_COPY_HEXDUMP, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_COPY_HEXDUMP, MF_GRAYED );

		// "Exit" is always allowed.
		EnableMenuItem( h, IDM_EXIT, MF_ENABLED );

		// Create the MRU list.
		make_MRU_list( h );
	}
	// Submenu "Disk":
	else if( l == ii++ )
	{
		UINT uEnable = Drive->IsOpen() ? MF_ENABLED : MF_GRAYED;
		UINT uEnableNDAS = (Drive->IsOpen() && NDASDrive->IsOpen()) ? MF_ENABLED : MF_GRAYED;

		EnableMenuItem( h, ID_DISK_CLOSEDRIVE, uEnable );
		EnableMenuItem( h, ID_DISK_READMFT, uEnable );
		EnableMenuItem( h, ID_DISK_GOTOFIRSTTRACK, uEnable );
		EnableMenuItem( h, ID_DISK_GOTONEXTTRACK, uEnable );
		EnableMenuItem( h, ID_DISK_GOTOPREVIOUSTRACK, uEnable );
		EnableMenuItem( h, ID_DISK_GOTOLASTTRACK, uEnable );
		EnableMenuItem( h, ID_DISK_GOTOTRACK, uEnable );
		EnableMenuItem( h, ID_DISK_SAVETRACK, uEnable );
		EnableMenuItem( h, ID_DISK_NDAS_INFO, uEnableNDAS );
	}
	// Submenu "Edit":
	else if( l == ii++ )
	{
//Pabs changed - line insert
		//"Fill with" is allowed if read-only is disabled.
		//If there is no selection the whole file is filled
		EnableMenuItem( h, IDM_FILL_WITH, (!bReadOnly?MF_ENABLED:MF_GRAYED) );

		//"Move/Copy bytes" is allowed if read-only is disabled.
		EnableMenuItem( h, IDM_EDIT_MOVE_COPY, (!bReadOnly?MF_ENABLED:MF_GRAYED) );

		//"Reverse bytes" is allowed if read-only is disabled.
		EnableMenuItem( h, IDM_EDIT_REVERSE, (!bReadOnly?MF_ENABLED:MF_GRAYED) );
//end
		// "Cut" is allowed if there is a selection or the caret is on a byte.
		// It is not allowed in read-only mode.
		if( ( bSelected || iCurByte <= DataArray.GetUpperBound() ) && !bReadOnly )
			EnableMenuItem( h, IDM_EDIT_CUT, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_EDIT_CUT, MF_GRAYED );

		// "Copy" is allowed if there is a selection or the caret is on a byte.
		if( bSelected || iCurByte <= DataArray.GetUpperBound() )
			EnableMenuItem( h, IDM_EDIT_COPY, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_EDIT_COPY, MF_GRAYED );

		// "Paste" is allowed if the clipboard contains text,
		// there is no selection going on and read-only is disabled.
		if( !bReadOnly && !bSelected )
		{
			if( OpenClipboard( NULL ) )
			{
//Pabs changed to allow any data on clip to enable paste when there is no error
				EnableMenuItem( h, IDM_EDIT_PASTE, EnumClipboardFormats(0) ? MF_ENABLED : MF_GRAYED );
//end
				CloseClipboard ();
			}
			else
			{
				// Clipboard could not be opened => can't paste.
				EnableMenuItem( h, IDM_EDIT_PASTE, MF_GRAYED );
			}
		}
		else
			EnableMenuItem( h, IDM_EDIT_PASTE, MF_GRAYED );

		// "Paste with dialogue" is allowed if read-only is disabled and.
		// there is no selection going on.
		if( !bReadOnly && !bSelected )
			EnableMenuItem( h, IDM_PASTE_WITH_DLG, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_PASTE_WITH_DLG, MF_GRAYED );

		// "Append" is allowed if read-only is disabled
		// and there is no selection going on.
		if( !bReadOnly && !bSelected )
			EnableMenuItem( h, IDM_EDIT_APPEND, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_EDIT_APPEND, MF_GRAYED );

		// "Delete" is allowed if there is a selection or the caret is on a byte.
		// It is not allowed in read-only mode.
		if( ( bSelected || iCurByte <= DataArray.GetUpperBound() ) && !bReadOnly )
			EnableMenuItem( h, IDA_DELETEKEY, MF_ENABLED );
		else
			EnableMenuItem( h, IDA_DELETEKEY, MF_GRAYED );

		// "Select All" is allowed if file is not empty.
		if( DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_SELECT_ALL, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_SELECT_ALL, MF_GRAYED );

		// "Select block" is allowed if file is not empty.
		if( DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_SELECT_BLOCK, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_SELECT_BLOCK, MF_GRAYED );

		// "Change editing mode" is always allowed.
		EnableMenuItem( h, IDM_CHANGE_MODE, MF_ENABLED );

		// "Toggle entering mode" is allowed if read-only is disabled.
		if( !bReadOnly )
			EnableMenuItem( h, IDA_INSERTMODETOGGLE, MF_ENABLED );
		else
			EnableMenuItem( h, IDA_INSERTMODETOGGLE, MF_GRAYED );

		// "Read-only mode" is always allowed.
		EnableMenuItem( h, IDM_EDIT_READONLYMODE, MF_ENABLED );
		// Check or uncheck this item.
		if( bReadOnly )
			CheckMenuItem( h, IDM_EDIT_READONLYMODE, MF_CHECKED );
		else
			CheckMenuItem( h, IDM_EDIT_READONLYMODE, MF_UNCHECKED );

		// "Find" is allowed if the file is not empty.
		if( DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_FIND, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_FIND, MF_GRAYED );

		// "Replace" is allowed if the file is not empty and read-only is disabled.
		if( DataArray.GetLength() > 0 && !bReadOnly )
			EnableMenuItem( h, IDM_REPLACE, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_REPLACE, MF_GRAYED );

		// "Find next" is allowed if the file is not empty,
		// and there is a findstring OR there is a selection
		// (which will be searched for).
		if( DataArray.GetLength() > 0 && ( pcFindDlgBuffer != NULL || bSelected ) )
			EnableMenuItem( h, IDM_FINDNEXT, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_FINDNEXT, MF_GRAYED );

		// "Find previous" is allowed if the file is not empty,
		// and there is a findstring OR there is a selection
		// (which will be searched for).
		if( DataArray.GetLength() > 0 && ( pcFindDlgBuffer != NULL || bSelected ) )
			EnableMenuItem( h, IDM_FINDPREV, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_FINDPREV, MF_GRAYED );

		// "Go to" is allowed if the file isn't empty.
		if( DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_GO_TO, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_GO_TO, MF_GRAYED );

		// "Enter decimal value" is allowed if read-only is disabled, the file is not empty,
		// the caret is on a byte and there is no selection going on.
		if( !bReadOnly && DataArray.GetLength() > 0 && iCurByte <= DataArray.GetUpperBound() && !bSelected )
			EnableMenuItem( h, IDM_EDIT_ENTERDECIMALVALUE, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_EDIT_ENTERDECIMALVALUE, MF_GRAYED );

		// "Manipulate bits" is allowed if the caret is on a byte, read-only is disabled
		// and there is no selection going on.
		if( !bReadOnly && iCurByte <= DataArray.GetUpperBound() && !bSelected )
			EnableMenuItem( h, IDM_EDIT_MANIPULATEBITS, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_EDIT_MANIPULATEBITS, MF_GRAYED );

		// "Compare from current offset" is allowed if the caret is on a byte
		// and there is no selection going on.
		if( iCurByte <= DataArray.GetUpperBound() && !bSelected )
			EnableMenuItem( h, IDM_COMPARE, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_COMPARE, MF_GRAYED );

		// "Get floating point value" is allowed if the caret is on a byte
		// and there is no selection going on.
		if( iCurByte <= DataArray.GetUpperBound() && !bSelected )
			EnableMenuItem( h, IDM_READFLOAT, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_READFLOAT, MF_GRAYED );

		// "File properties" is always allowed.
		EnableMenuItem( h, IDM_PROPERTIES, MF_ENABLED );

		// "Apply template" is allowed if the caret is on a byte
		// and there is no selection going on.
		if( iCurByte <= DataArray.GetUpperBound() && !bSelected )
			EnableMenuItem( h, IDM_APPLYTEMPLATE, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_APPLYTEMPLATE, MF_GRAYED );

		// "Open in text editor" is allowed if file has been saved before.
		if( !bFileNeverSaved )
			EnableMenuItem( h, IDM_OPEN_TEXT, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_OPEN_TEXT, MF_GRAYED );

		UINT uEnableNDAS = (Drive->IsOpen() && NDASDrive->IsOpen()) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem( h, ID_SET_NDAS_CRC32, uEnableNDAS );
	}
	// Submenu "View":
	else if( l == ii++ )
	{
		// These items are always enabled.
		EnableMenuItem( h, IDM_SCROLL_LEFT, MF_ENABLED );
		EnableMenuItem( h, IDM_SCROLL_RIGHT, MF_ENABLED );
		EnableMenuItem( h, IDM_SCROLL_UP, MF_ENABLED );
		EnableMenuItem( h, IDM_SCROLL_DOWN, MF_ENABLED );
		EnableMenuItem( h, IDM_SCROLL_PRIOR, MF_ENABLED );
		EnableMenuItem( h, IDM_SCROLL_NEXT, MF_ENABLED );
	}
	// Submenu "Options":
	else if( l == ii++ )
	{
		// These items are always enabled.
		EnableMenuItem( h, IDM_VIEW_SETTINGS, MF_ENABLED );
		EnableMenuItem( h, IDM_TEXT_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_BK_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_SEP_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_SELTEXT_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_SELBACK_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_BMK_COLOR, MF_ENABLED );
		EnableMenuItem( h, IDM_RESET_COLORS, MF_ENABLED );
		EnableMenuItem( h, IDM_CHARACTER_SET, MF_ENABLED );
		EnableMenuItem( h, IDM_BINARYMODE, MF_ENABLED );

//Pabs inserted
		EnableMenuItem( h, IDM_ADOPT_COLOURS, MF_ENABLED );
		EnableMenuItem( h, IDM_MAKE_BACKUPS, MF_ENABLED );
		CheckMenuItem( h, IDM_MAKE_BACKUPS, bMakeBackups ? MF_CHECKED : MF_UNCHECKED );
//end

	}
//Pabs changed - insert new menu
	//Submenu "Registry":
	else if( l == ii++ )
	{
		//these always enabled
		EnableMenuItem( h, IDM_CONTEXT, MF_ENABLED );
		EnableMenuItem( h, IDM_UNKNOWN, MF_ENABLED );
		EnableMenuItem( h, IDM_SAVEINI, MF_ENABLED );
		EnableMenuItem( h, IDM_SHORTCUTS, MF_ENABLED );
		EnableMenuItem( h, IDM_CHANGEINST, MF_ENABLED );//Always enabled to allow reloading of current instance

		//these enabled if certain things are present
		int tmp = unknownpresent();
		int tmp2 = contextpresent();
		EnableMenuItem( h, IDM_DEFAULT, tmp ? MF_ENABLED : MF_GRAYED );
		EnableMenuItem( h, IDM_UPGRADE, oldpresent() ? MF_ENABLED : MF_GRAYED );
		EnableMenuItem( h, IDM_REMOVE, ( frhedpresent() || tmp || tmp2 ) ? MF_ENABLED : MF_GRAYED );

		//these checked if certain things are present
		CheckMenuItem( h, IDM_CONTEXT, tmp2 ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( h, IDM_UNKNOWN, tmp ? MF_CHECKED : MF_UNCHECKED );
		CheckMenuItem( h, IDM_SAVEINI, bSaveIni ? MF_CHECKED : MF_UNCHECKED );
		tmp = 0;
		char stringval[ _MAX_PATH ]="";
		long len = _MAX_PATH;
		RegQueryValue(HKEY_CLASSES_ROOT,"Unknown\\shell",stringval,&len);
		if(!strcmp(stringval, "Open in frhed"))tmp = 1;
		CheckMenuItem( h, IDM_DEFAULT, tmp ? MF_CHECKED : MF_UNCHECKED );

	}
//end
	// Submenu "Bookmarks":
	else if( l == ii++ )
	{
		// "Add bookmark" is allowed if the file is not
		// empty and there is no selection going on.
		if( !bSelected && DataArray.GetLength() > 0 )
			EnableMenuItem( h, IDM_ADDBOOKMARK, MF_ENABLED );
		else
			EnableMenuItem( h, IDM_ADDBOOKMARK, MF_GRAYED );

		// "Remove bookmark" and "Clear all bookmarks" are allowed if there are bookmarks set.
		if( iBmkCount > 0 )
		{
			EnableMenuItem( h, IDM_REMOVE_BKM, MF_ENABLED );
			EnableMenuItem( h, IDM_CLEARALL_BMK, MF_ENABLED );
		}
		else
		{
			EnableMenuItem( h, IDM_REMOVE_BKM, MF_GRAYED );
			EnableMenuItem( h, IDM_CLEARALL_BMK, MF_GRAYED );
		}
		// Create the bookmark list.
		make_bookmark_list( h );
	}
	// Submenu "Help":
	else if( l == ii++ )
	{
		// These items are always enabled.
		EnableMenuItem( h, IDM_HELP_TOPICS, MF_ENABLED );
		EnableMenuItem( h, IDM_ABOUT, MF_ENABLED );
	}
//end - nums replaced with ii++
	return 0;
}

//--------------------------------------------------------------------------------------------
// Handler on window closing.
int HexEditorWindow::close ()
{
//Pabs changed - restructured so that help file is always closed on exit & the user can save&exit, exit | not exit if file changed
	if( m_iFileChanged == TRUE )
	{
		int res = MessageBox (hwnd, "Do you want to save your changes?", "Exit", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to quit or User wants to save and the save was unsuccessful
			return 0;//Don't exit
//		if( MessageBox (hwnd, "File was changed! Exit anyway?", "Exit", MB_YESNO | MB_ICONQUESTION) == IDNO )
//			return 0;
	}
//	else
//	{
	// If help was open close it.
//	ShowHtmlHelp(HELP_QUIT, 0, hwnd);
//	}
//end

	// Store window position for next startup.
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof( WINDOWPLACEMENT );
	GetWindowPlacement( hwndMain, &wndpl );
	iWindowShowCmd = wndpl.showCmd;
	iWindowX = wndpl.rcNormalPosition.left;
	iWindowY = wndpl.rcNormalPosition.top;
	iWindowWidth = wndpl.rcNormalPosition.right - iWindowX;
	iWindowHeight = wndpl.rcNormalPosition.bottom - iWindowY;
	save_ini_data();

	// Destroy window.
	DestroyWindow( hwnd );
	return 0;
}

//--------------------------------------------------------------------------------------------
BOOL CALLBACK AboutDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch( iMsg )
	{
	case WM_INITDIALOG:
		{
			// Set the version information.
			SetWindowText( GetDlgItem( hDlg, IDC_STATIC1 ),
				"frhed - free hex editor for 32-bit Windows\nVersion "CURRENT_VERSION"."
				SUB_RELEASE_NO "\n(c) Raihan Kibria 2000"
				"\nFill with by Pabs Dec 1999"
				"\nDisk-Access, Code/Decode Extension and some other bits by Gerson Kurz.");
			// Set the email-addresses.
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT1 ),
				"rkibria@hrz1.hrz.tu-darmstadt.de"
				"\r\nPabs: pabs3@zip.to");
			// Set the homepage URL.
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT2 ), "http://www.kibria.de" );
			return TRUE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;

		case IDC_BUTTON1:
			{
				HINSTANCE hi = ShellExecute( hwndMain, "open", BrowserName, "http://www.kibria.de", NULL, SW_SHOWNORMAL );
				if( (int) hi <= 32 )
					MessageBox( hwndMain, "Could not call browser.", "Go to homepage", MB_OK | MB_ICONERROR );
			}
		}
		break;
	}
	return FALSE;
}

//--------------------------------------------------------------------------------------------
// FIND_BYTES
// Arguments:
// ps   = Start position.
// ls   = Length of src array.
// pb   = Start of searchstring.
// lb   = Length searchstring.
// mode = -1 : backwards search.
//         1 : forward search.
// cmp  = pointer to function that is applied to data before comparing.
//
// Return:
// Position of found string or -1 if not there.

int find_bytes (char* ps, int ls, char* pb, int lb, int mode, char (*cmp) (char))
{
	int start, inc, end, i;
	if (mode == 1)
	{
		start = 0;
		inc = 1;
		end = ls - lb + 1;
	}
	else
	{
		start = ls - lb;
		inc = -1;
		end = 1;
	}

	for (; mode * start < end; start += inc)
	{
		for (i = start; i < start + lb; i++)
		{
			if (cmp (ps[i]) != cmp (pb[i - start]))
				break;
		}
		if (i == start + lb)
			return start;
	}

	return -1;
}


//Pabs rewrote lbuttonup, mousemove, lbuttondown for drag-drop editing
//--------------------------------------------------------------------------------------------
// WM_LBUTTONUP handler.
int HexEditorWindow::lbuttonup (int xPos, int yPos)
{
	bLButtonDown = FALSE;
	// Kill timer.
	kill_scroll_timers();
	KillTimer (hwnd, MOUSE_OP_DELAY_TIMER_ID);
	bMouseOpDelayTimerSet = FALSE;
	// Release mouse.
	ReleaseCapture ();

	if ( !bSelecting && !dragging )
	{
		switch(get_pos(xPos,yPos)){
			case AREA_BYTES:
			case AREA_OFFSETS:
				iCurNibble = nibblenum;
				if(iCurNibble==2)return 0;
				m_iEnteringMode = BYTES;
				break;
			default:
				m_iEnteringMode = CHARS;
		}

		SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
		if(!bSelected){
			int a = iCurByte/iBytesPerLine;
			int b = new_pos/iBytesPerLine;
			iCurByte = new_pos;
			//if(a!=b)repaint(b);
			repaint(a,b);
		}
		else{
			int a = iStartOfSelection/iBytesPerLine;
			int b = iEndOfSelection/iBytesPerLine;
			if(a>b)swap(a,b);
			bSelected = FALSE;
			iCurByte = new_pos;
			int c = iCurByte/iBytesPerLine;
			if(c<a||c>b)repaint(c);
			repaint(a,b);
		}
	}

	dragging = false;

	bSelecting = FALSE;

	return 0;
}

//--------------------------------------------------------------------------------------------
// WM_MOUSEMOVE handler.
int HexEditorWindow::mousemove (int xPos, int yPos)
{

	iMouseX = xPos;
	iMouseY = yPos;

	if( !bLButtonDown || ( bLButtonDown && bSelecting ) )
		get_pos(xPos, yPos);

	if( bLButtonDown ){

		if ( bSelecting ){
			// Capture mouse.
			if( !GetCapture() )
			{
				SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
				SetCapture (hwnd);
			}

			fix_scroll_timers(xPos,yPos);

			int lastbyte = LASTBYTE;
			if( new_pos > lastbyte ) new_pos = lastbyte;
			if( iEndOfSelection != new_pos )
			{
				bSelected = TRUE;
				int oeos = iEndOfSelection;
				iEndOfSelection = new_pos;
				repaint(oeos/iBytesPerLine,new_pos/iBytesPerLine);
			}
		/*
		} else if( bPullScrolling ){
			if( !GetCapture() ){
				SetCapture (hwnd);
				SetCursor( LoadCursor( NULL, IDC_SIZEALL ) );
			}
			scroll_to_point( xPos, yPos );
		}
		*/
		} else if( bMouseOpDelayTimerSet &&
			!( (xPos >= iLBDownX - MouseOpDist) && (xPos <= iLBDownX + MouseOpDist)
			&& (yPos >= iLBDownY - MouseOpDist) && (yPos <= iLBDownX + MouseOpDist)
			) ){
			start_mouse_operation();
		}
	}
	else
	{
		SetCursor( LoadCursor( NULL, ( enable_drag && bSelected && IN_BOUNDS( new_pos, iStartOfSelection, iEndOfSelection ) ) ? IDC_ARROW : IDC_IBEAM ) );
	}

	old_pos = new_pos;

	return 0;
}

//--------------------------------------------------------------------------------------------
// WM_LBUTTONDOWN handler.
int HexEditorWindow::lbuttondown ( int nFlags, int xPos, int yPos )
{
	bLButtonDown = TRUE;

	iLBDownX = xPos;
	iLBDownY = yPos;

	if (NO_FILE)
		return 0;

//Pabs inserted - after reading the fuzz report
	//Someone sent us invalid data
	if ( ( nFlags & ( ~ (MK_CONTROL|MK_LBUTTON|MK_MBUTTON|MK_RBUTTON|MK_SHIFT) ) ) ||
		( xPos < 0 || xPos > cxClient ) ||
		( yPos < 0 || yPos > cyClient ) ){
		return 0;
	}
//end

	switch(lbd_area = get_pos(xPos,yPos)){
		case AREA_OFFSETS:
			if(!bAutoOffsetLen && iMinOffsetLen>1)
			{
				iMinOffsetLen--;
				save_ini_data ();
				update_for_new_datasize ();
				return 0;
			}
		default:
			lbd_pos = old_pos = new_pos;
	}

	//User wants to extend selection
	if( nFlags & MK_SHIFT )
	{
		if( nibblenum!=2 ){
			int lastbyte = LASTBYTE;
			if( new_pos == lastbyte + 1 )
				new_pos--;
			if( new_pos <= lastbyte )
			{
				bSelecting = TRUE;
				if(iEndOfSelection != new_pos)
				{
					int oeos = iEndOfSelection;
					iEndOfSelection = new_pos;
					if( !bSelected )
					{
						bSelected = TRUE;
						iStartOfSelection = iCurByte;
					}
					repaint(oeos/iBytesPerLine,new_pos/iBytesPerLine);
				}
			}
		}
	}
	else
	{
		/*Set the timer and wait until we know that the user wants some kind of
		mouse operation. We know this because they will hold the mouse down for
		some time or they will move the mouse around.*/
		SetTimer( hwnd, MOUSE_OP_DELAY_TIMER_ID, MouseOpDelay, NULL );
		bMouseOpDelayTimerSet = TRUE;
		SetCapture( hwnd );
	}

	return 0;
}
//end

//Pabs inserted
ClickArea HexEditorWindow::get_pos(long x, long y)
{
	area = AREA_NONE;

	int scr_column = x / cxChar; // Column on the screen.
	int scr_row = y / cyChar; // Line on the screen.
	column = scr_column + iHscrollPos; // Absolute column number.
	line = iCurLine + scr_row; // Absolute line number.

	int iStartofChars = iCharsPerLine - iBytesPerLine;
	int iStartofBytes = iMaxOffsetLen + iByteSpace;
	new_pos = bytenum = nibblenum = -1;

	//Click on offsets
	if (column >= 0 && column < iStartofBytes)
	{
		nibblenum = 0;
		bytenum = 0;
		new_pos = line * iBytesPerLine;
		area = AREA_OFFSETS;
	}
	// Click on bytes.
	else if ( column >= iStartofBytes && column < iStartofChars - iCharSpace - 1)
	{
		int relpos = column - iStartofBytes;
		nibblenum = relpos%3;
		bytenum = relpos/3;
		area = AREA_BYTES;
		if( nibblenum==2 ){
			if( x%cxChar > cxChar/2 ){
				bytenum++;
				nibblenum = 0;
			}
			else nibblenum = 1;
		}
	}
	// Click between bytes and chars
	else if ( column >= iStartofChars - iCharSpace - 1 && column < iStartofChars )
	{
		if( x + iHscrollPos*cxChar <= cxChar*(iStartofChars-1)-cxChar*iCharSpace/2 ){
			area = AREA_BYTES;
			nibblenum = 1;
			bytenum = iBytesPerLine - (dragging ? 0 : 1);
		} else {
			area = AREA_CHARS;
			bytenum = 0;
		}
	}
	// Click on chars.
	else if (column >= iStartofChars && column < iCharsPerLine)
	{
		bytenum = column - iStartofChars;
		area = AREA_CHARS;
	}
	//Click after chars
	else
	{
		bytenum = iBytesPerLine;
		new_pos = (line+1) * iBytesPerLine - 1;
	}

	if( area >= AREA_BYTES ){
		new_pos = line * iBytesPerLine + bytenum;
	}

	int lastbyte = LASTBYTE;
	if( new_pos > lastbyte+1 )
	{
		nibblenum = 1;
		new_pos = lastbyte+1;
		bytenum = new_pos%iBytesPerLine;
		line = new_pos/iBytesPerLine;
	}
	else if( new_pos < 0 )
		line = bytenum = nibblenum = new_pos = 0;


	return area;
}

void HexEditorWindow::set_drag_caret(long x, long y, bool Copying, bool Overwrite)
{
	get_pos(x, y);

	int iStartofBytes = iMaxOffsetLen + iByteSpace;

	if( area == AREA_OFFSETS ){
		area = AREA_BYTES;
	}
	else if( column >= iCharsPerLine ){
		new_pos++;
		area = AREA_CHARS;
	}

	int update = 0;
	int lastbyte = LASTBYTE;
	if( bSelected /*&& !bAllowDropInSel*/ ){
		iStartOfSelSetting = iStartOfSelection;
		iEndOfSelSetting = iEndOfSelection;
		if(iStartOfSelSetting>iEndOfSelSetting)swap(iStartOfSelSetting,iEndOfSelSetting);
		if( Copying || !bMoving || Overwrite )
		{
			if ( new_pos >= iStartOfSelSetting+1 && new_pos <= iEndOfSelSetting )
			{
				if( new_pos < (iStartOfSelSetting+iEndOfSelSetting) / 2 )
					new_pos = iStartOfSelSetting;
				else
					new_pos = iEndOfSelSetting+1;
				update = 1;
			}
		}
		else //if ( Moving )
		{
			if ( new_pos >= iStartOfSelSetting && new_pos <= iEndOfSelSetting+1 )
			{
				if( !iStartOfSelSetting && iEndOfSelSetting==lastbyte)
					new_pos = ( new_pos >= (iStartOfSelSetting+iEndOfSelSetting)/2) ? lastbyte+1 : 0;
				else if( new_pos <= (iStartOfSelSetting+iEndOfSelSetting) / 2 ){
					if(iStartOfSelSetting)new_pos = iStartOfSelSetting-1;
					else new_pos = iEndOfSelSetting+2;
				}
				else{
					if(iEndOfSelSetting==lastbyte)new_pos = iStartOfSelSetting-1;
					else new_pos = iEndOfSelSetting+2;
				}

				update = 1;
			}
		}
	}

	if( new_pos > lastbyte+1 )
	{
		new_pos = lastbyte+1;
		update = 1;
	}
	else if( new_pos < 0 )
	{
		new_pos = 0;
		update = 1;
	}


	if( update ){
		bytenum = new_pos % iBytesPerLine;
		line = new_pos / iBytesPerLine;
	}

	y = line;
	x = bytenum;

	if( area == AREA_BYTES ){
		x = x*3 + iStartofBytes - 1;
	}
	else if( area == AREA_CHARS ){
		x = iCharsPerLine - iBytesPerLine + x;
	}

	x -= iHscrollPos;
	y -= iCurLine;

	if ( x != old_col || y != old_row ){
		old_col = x; old_row = y;

		x = x*cxChar;
		y = y*cyChar;
		if( area == AREA_BYTES ){
			x += cxChar / 2;
		}

		//Set caret position & show it
		SetCaretPos(x, y);
		ShowCaret( hwnd );
	}
}

void HexEditorWindow::fix_scroll_timers(long x, long y){
	SCROLL_TYPE vert = ( x < cxChar ? SCROLL_BACK : ( x <= (cxClient/cxChar-1)*cxChar ? SCROLL_NONE : SCROLL_FORWARD ) );
	SCROLL_TYPE horz = ( y < cyChar ? SCROLL_BACK : ( y <= (cyClient/cyChar-1)*cyChar ? SCROLL_NONE : SCROLL_FORWARD ) );
	if( ( prev_vert == SCROLL_NONE && vert != SCROLL_NONE ) || ( prev_horz == SCROLL_NONE && horz != SCROLL_NONE ) ){
		if( bSelecting ){
			if( enable_scroll_delay_sel ){
				bScrollDelayTimerSet = TRUE;
				SetTimer( hwnd, SCROLL_DELAY_TIMER_ID, ScrollDelay, NULL );
			} else {
				bScrollTimerSet = TRUE;
				SetTimer( hwnd, SCROLL_TIMER_ID, ScrollInterval, NULL );
			}
		} else if( dragging ){
			if( enable_scroll_delay_dd ){
				bScrollDelayTimerSet = TRUE;
				SetTimer( hwnd, SCROLL_DELAY_TIMER_ID, ScrollDelay, NULL );
			} else {
				bScrollTimerSet = TRUE;
				SetTimer( hwnd, SCROLL_TIMER_ID, ScrollInterval, NULL );
			}
		}
	} else if( ( prev_vert != SCROLL_NONE && vert == SCROLL_NONE ) || ( prev_horz != SCROLL_NONE && horz == SCROLL_NONE ) ){
		kill_scroll_timers();
	}
	prev_vert = vert;
	prev_horz = horz;
}

void HexEditorWindow::kill_scroll_timers(){
	KillTimer( hwnd, SCROLL_DELAY_TIMER_ID );
	bScrollDelayTimerSet = FALSE;
	KillTimer( hwnd, SCROLL_TIMER_ID );
	bScrollTimerSet = FALSE;
	prev_vert = prev_horz = SCROLL_NONE;
}

//end

//-------------------------------------------------------------------
// On find command.
int HexEditorWindow::CMD_find ()
{
	if (filename[0] == '\0')
		return 0;

	// If there is selected data then make it the data to find.
	if( bSelected )
	{
		// Get start offset and length (is at least =1) of selection.
		int sel_start, select_len;
		if( iEndOfSelection < iStartOfSelection )
		{
			sel_start = iEndOfSelection;
			select_len = iStartOfSelection - iEndOfSelection + 1;
		}
		else
		{
			sel_start = iStartOfSelection;
			select_len = iEndOfSelection - iStartOfSelection + 1;
		}

		// Get the length of the bytecode representation of the selection (including zero-byte at end).
		/*int findlen = */byte_to_BC_destlen( (char*) &DataArray[sel_start], select_len );

		// New buffer length is at least FINDDLG_BUFLEN = 64K, bigger if findstring is bigger than 64K.
		// iFindDlgBufLen = max( FINDDLG_BUFLEN, findlen );

		// Signal dialogue function to display the text in the edit box.
		// iFindDlgLastLen = findlen;

		// Delete old buffer.
		// if( pcFindDlgBuffer != NULL )
		//	delete [] pcFindDlgBuffer;

		// Allocate new buffer.
		// pcFindDlgBuffer = new char[ iFindDlgBufLen ];
		// if( pcFindDlgBuffer == NULL )
		//	MessageBox( hwnd, "Could not allocate findstring buffer!", "Find ERROR", MB_OK | MB_ICONERROR );

		// Translate the selection into bytecode and write it into the edit box buffer.
		translate_bytes_to_BC( pcFindDlgBuffer, &DataArray[sel_start], select_len );
	}

	int srclen = DialogBox( hInstance, MAKEINTRESOURCE( IDD_FINDDIALOG ), hwnd, (DLGPROC) FindDlgProc );
	// NumBx(iFindDlgLastLen);
	char* pcFindstring;
	if (srclen != 0)
	{
		//GK16AUG2K
		int destlen;
		WCHAR* wSearchString = 0;

		if( iFindDlgUnicode )
		{
			long lSizeMax = strlen( pcFindDlgBuffer );
			wSearchString = new WCHAR[lSizeMax+4];
			destlen = MultiByteToWideChar(CP_ACP, 0, pcFindDlgBuffer, -1, wSearchString, lSizeMax+1) * 2 - 2;
			pcFindstring = (char*) wSearchString;
		}
		else
		{
			// Create findstring.
			destlen = create_bc_translation (&pcFindstring, pcFindDlgBuffer, srclen, iCharacterSet, iBinaryMode);
		}
		if (destlen > 0)
		{
			int i;
			char (*cmp) (char);

			if (iFindDlgMatchCase == BST_CHECKED)
				cmp = equal;
			else
				cmp = lower_case;

			SetCursor (LoadCursor (NULL, IDC_WAIT));
			// Find forward.
			if (iFindDlgDirection == 1)
			{
				i = find_bytes ((char*) &(DataArray[iCurByte + 1]), DataArray.GetLength() - iCurByte - 1, pcFindstring, destlen, 1, cmp);
				if (i != -1)
					iCurByte += i + 1;
			}
			// Find backward.
			else
			{
				i = find_bytes( (char*) &(DataArray[0]),
					min( iCurByte + (destlen - 1), DataArray.GetLength() ),
					pcFindstring, destlen, -1, cmp );
				if (i != -1)
					iCurByte = i;
			}
			SetCursor (LoadCursor (NULL, IDC_ARROW));

			if (i != -1)
			{
				// Caret will be vertically centered if line of found string is not visible.
				/* OLD: ONLY SET CURSOR POSITION
				if( iCurByte/iBytesPerLine < iCurLine || iCurByte/iBytesPerLine > iCurLine + cyBuffer )
					iCurLine = max( 0, iCurByte/iBytesPerLine-cyBuffer/2 );
				adjust_vscrollbar();
				*/

				// NEW: Select found interval.
				bSelected = TRUE;
				iStartOfSelection = iCurByte;
				iEndOfSelection = iCurByte + destlen - 1;
				adjust_view_for_selection();

				repaint();
			}
			else
				MessageBox (hwnd, "Could not find data.", "Find", MB_OK | MB_ICONERROR);
			//GK16AUG2K
			if( iFindDlgUnicode )
				delete [] wSearchString;
			else
				delete [] pcFindstring;
		}
		else
			MessageBox (hwnd, "Findstring is zero bytes long.", "Find", MB_OK | MB_ICONERROR);
	}
	return 1;
}

//-------------------------------------------------------------------
// On copy command.
int HexEditorWindow::CMD_edit_copy ()
{
	if (!bSelected)
	{
		// No selection: copy current byte.
		iCopyStartOffset = iCurByte;
		iCopyEndOffset = iCurByte;
	}
	else
	{
		// Copy selection.
		if (iEndOfSelection >= iStartOfSelection)
		{
			iCopyStartOffset = iStartOfSelection;
			iCopyEndOffset = iEndOfSelection;
		}
		else
		{
			iCopyStartOffset = iEndOfSelection;
			iCopyEndOffset = iStartOfSelection;
		}
	}

	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_COPYDIALOG), hwnd, (DLGPROC) CopyDlgProc) != FALSE)
	{
		// Get dialogue values.
		iStartOfSelSetting = iCopyStartOffset;
		iEndOfSelSetting = iCopyEndOffset;
		if (iEndOfSelSetting<iStartOfSelSetting)swap(iEndOfSelSetting,iStartOfSelSetting);
		int destlen = byte_to_BC_destlen ((char*) &(DataArray[iStartOfSelSetting]), iEndOfSelSetting-iStartOfSelSetting+1);
		HGLOBAL hGlobal = GlobalAlloc (GHND, destlen);
		if (hGlobal != NULL)
		{
			SetCursor (LoadCursor (NULL, IDC_WAIT));
			char* pd = (char*) GlobalLock (hGlobal);
			translate_bytes_to_BC (pd, &(DataArray[iStartOfSelSetting]), iEndOfSelSetting-iStartOfSelSetting+1);
			GlobalUnlock (hGlobal);
			OpenClipboard (hwnd);
			EmptyClipboard ();
			SetClipboardData (CF_TEXT, hGlobal);
			CloseClipboard ();
			SetCursor (LoadCursor (NULL, IDC_ARROW));
		}
		else
			MessageBox (hwnd, "Not enough memory for copying.", "Copy", MB_OK | MB_ICONERROR);
	}
	return 1;
}

//-------------------------------------------------------------------
// On hexdump to file/clipboard command.
int HexEditorWindow::CMD_copy_hexdump (char* mem, int memlen)
{
	if (DataArray.GetLength() <= 0)
	{
		MessageBox (hwnd, "Can't hexdump empty file.", "Export hexdump", MB_OK | MB_ICONERROR);
		return 0;
	}
	if( !bSelected )
	{
		// Assume whole file is to be hexdumped. (except the last line (if incomplete))
		iCopyHexdumpDlgStart = 0;
		iCopyHexdumpDlgEnd = ((DataArray.GetUpperBound())/iBytesPerLine)*iBytesPerLine;
	}
	else
	{
		// Assume selected area is to be hexdumped.
		// Make sure end of selection is greater than start of selection.
//Pabs changed - line remove & insert - works better now for both exporting types
		iCopyHexdumpDlgStart = iStartOfSelection ;
		iCopyHexdumpDlgEnd = iEndOfSelection ;
		if( iCopyHexdumpDlgEnd < iCopyHexdumpDlgStart ) swap(iCopyHexdumpDlgStart,iCopyHexdumpDlgEnd);
//end
	}

	if ( mem || ( DialogBox (hInstance, MAKEINTRESOURCE (IDD_HEXDUMPDIALOG), hwnd, (DLGPROC) CopyHexdumpDlgProc) != FALSE ) )
	{
//Pabs changed - bugfix insert
		if( iCopyHexdumpDlgEnd < iCopyHexdumpDlgStart ) swap(iCopyHexdumpDlgStart,iCopyHexdumpDlgEnd);
//end
		// Show wait cursor.
		if(!mem)
			SetCursor (LoadCursor (NULL, IDC_WAIT));

//Pabs removed - see further down
//Done so that you can select partial lines for non-display output
//If both on the same line in display output just that line is output

//Pabs changed - line insert & following lines indented
		char* pMem = mem; int buflen;
		if(iCopyHexdumpType == IDC_EXPORTDISPLAY){
			iCopyHexdumpDlgStart = iCopyHexdumpDlgStart / iBytesPerLine * iBytesPerLine;//cut back to the line start
			iCopyHexdumpDlgEnd = iCopyHexdumpDlgEnd / iBytesPerLine * iBytesPerLine;//cut back to the line start
//end
			// Number of lines to copy:
			int linecount = (iCopyHexdumpDlgEnd - iCopyHexdumpDlgStart) / iBytesPerLine + 1;
			// Req'd mem for lines:
			// (Every line ended with CR+LF ('\r'+'\n'))
//Pabs changed - "int" removed - see further up
			buflen = linecount * (iCharsPerLine+2) + 1;
			if( mem && buflen > memlen ) return 0;
//end
			// Create hexdump.
			int a,b,k,m,n,j,l;
			char buf1[128], buf2[128];
//Pabs changed - "char*" removed - see further up
			if(!mem){
				pMem = new char[buflen];
				if(!pMem) return 0;
			}
//end
			for (n=0; n < buflen; n++)
				pMem[n] = ' ';
			// Write hexdump.
			// a = first byte of first line of hexdump.
			// b = first byte of last line of hexdump.
			b = iCopyHexdumpDlgEnd;
			// a = Offset of current line.
			// k = Offset in text array.
			for (k = 0, a = iCopyHexdumpDlgStart; a <= b; a += iBytesPerLine, k += iCharsPerLine + 2)
			{
				// Write offset.
//Pabs changed to fix output glitch

				sprintf (buf1, "%%%d.%dx", iMinOffsetLen, iMinOffsetLen);

				sprintf (buf2, buf1, bPartialStats ? a + iPartialOffset : a );

				m = strlen(buf2);

				iMaxOffsetLen += iByteSpace;

				memset(buf2+m,' ',iMaxOffsetLen-m);

				buf2[iMaxOffsetLen] = '\0';

				iMaxOffsetLen -= iByteSpace;
//end

				l = 0; // l = Offset in line, relative to k.
				n = 0;
				while (buf2[n] != '\0')
					pMem[k + (l++)] = buf2[n++]; // Copy Offset. l = next empty place after spaces.
				// Write bytes and chars.
				for (j = 0; j < iBytesPerLine; j++)
				{
					if (a+j > DataArray.GetUpperBound ())
					{
						// Nonexistant byte.
						pMem[k + l + j*3    ] = ' ';
						pMem[k + l + j*3 + 1] = ' ';
						pMem[k + l + j*3 + 2] = ' ';
						// Nonexistant char.
						pMem[k + l + iBytesPerLine*3 + iCharSpace + j] = ' ';
					}
					else
					{
						// Write byte.
						sprintf (buf1, "%2.2x ", DataArray[a + j]);
						pMem[k + l + j*3    ] = buf1[0];
						pMem[k + l + j*3 + 1] = buf1[1];
						pMem[k + l + j*3 + 2] = buf1[2];
						// Write char.
						if( iCharacterSet == OEM_FIXED_FONT && DataArray[a + j] != 0 )
							pMem[k + l + iBytesPerLine*3 + iCharSpace + j] = DataArray[a + j];
						else if( (DataArray[a + j] >= 32 && DataArray[a + j] <= 126) || (DataArray[a + j]>=160 && DataArray[a + j] <= 255) || (DataArray[a + j] >= 145 && DataArray[a + j] <= 146) )
							pMem[k + l + iBytesPerLine*3 + iCharSpace + j] = DataArray[a + j];
						else
							pMem[k + l + iBytesPerLine*3 + iCharSpace + j] = '.';
					}
				}
				pMem[k + iCharsPerLine    ] = '\r';
				pMem[k + iCharsPerLine + 1] = '\n';
			}
			pMem[buflen-1] = '\0';
//Pabs changed - line insert
		} else if(iCopyHexdumpType == IDC_EXPORTDIGITS) {

			// Req'd mem for lines:
			int numchar = iCopyHexdumpDlgEnd - iCopyHexdumpDlgStart + 1;
			buflen = numchar * 2 + 1;
			if( mem && buflen > memlen ) return 0;
			// Create hexdump.
			if(!mem){
				pMem = new char[buflen];
				if(!pMem) return 0;
			}
			for (int i = iCopyHexdumpDlgStart; i <= iCopyHexdumpDlgEnd; i++){
				sprintf(&pMem[i*2],"%2.2x",DataArray[i]);
			}
			pMem[buflen - 1] = '\0';
		} else if(iCopyHexdumpType == IDC_EXPORTRTF) {
			if( mem ) return 0;
			//Bit of a kludge here
			pMem = (char*)RTF_hexdump(iCopyHexdumpDlgStart, iCopyHexdumpDlgEnd, (DWORD*)&buflen);
			if( !pMem ) return 0;
		} else return 0;
		if(mem) return 1;
//end
		// Remove wait cursor.
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		if (iCopyHexdumpMode == BST_CHECKED && iCopyHexdumpType != IDC_EXPORTRTF)
		{
			// To clipboard.
			HGLOBAL hGlobal = GlobalAlloc (GHND, buflen);
			if (hGlobal != NULL)
			{
				char* pDest = (char*) GlobalLock (hGlobal);
				memcpy (pDest, pMem, buflen);
				GlobalUnlock (hGlobal);
				OpenClipboard (hwnd);
				EmptyClipboard ();
				SetClipboardData (CF_TEXT, hGlobal);
				CloseClipboard ();
			}
			else
				MessageBox (hwnd, "Not enough memory for hexdump to clipboard.", "Export hexdump", MB_OK | MB_ICONERROR);
		}
		else if (iCopyHexdumpMode == BST_CHECKED && iCopyHexdumpType == IDC_EXPORTRTF)
		{
			// To clipboard.
			if( OpenClipboard (hwnd) ){
				EmptyClipboard();
				SetClipboardData(CF_RICH_TEXT_FORMAT, (HGLOBAL)pMem);
				CloseClipboard();
			}
			else
				MessageBox (hwnd, "Could not hexdump to clipboard.", "Export hexdump", MB_OK | MB_ICONERROR);
			pMem = NULL;
		}
		else
		{
			// to file.
			char szFileName[_MAX_PATH];
			char szTitleName[_MAX_FNAME + _MAX_EXT];
			strcpy (szFileName, "hexdump.txt");
			HGLOBAL hg = NULL;
			if( iCopyHexdumpType == IDC_EXPORTRTF ){
				hg = (HGLOBAL)pMem;
				pMem = (char*)GlobalLock(hg);
				if( !pMem ){
					GlobalFree( hg );
					return 0;
				}
				strcpy(&szFileName[8],"rtf");
			}

			// to file.
			OPENFILENAME ofn;
			ofn.lStructSize = sizeof (OPENFILENAME);
			ofn.hwndOwner = hwnd;
			ofn.hInstance = NULL;
			ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
			ofn.lpstrCustomFilter = NULL;
			ofn.nMaxCustFilter = 0;
			ofn.nFilterIndex = 0;
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = _MAX_PATH;
			ofn.lpstrFileTitle = szTitleName;
			ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
			ofn.lpstrInitialDir = NULL;
			ofn.lpstrTitle = NULL;
			ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
			ofn.nFileOffset = 0;
			ofn.nFileExtension = 0;
			ofn.lpstrDefExt = NULL;
			ofn.lCustData = 0L;
			ofn.lpfnHook = NULL;
			ofn.lpTemplateName = NULL;
			if (GetSaveFileName (&ofn))
			{
				int filehandle;
				if ((filehandle = _open (szFileName, _O_RDWR|_O_CREAT|_O_TRUNC|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
				{
					// Write file.
					if ((_write (filehandle, pMem, buflen-1)) != -1)
					{//Pabs replaced NULL w hwnd
						MessageBox (hwnd, "Hexdump saved.", "Export hexdump", MB_OK | MB_ICONINFORMATION);
					}
					else
						MessageBox (hwnd, "Could not save Hexdump.", "Export hexdump", MB_OK | MB_ICONERROR);
					_close (filehandle);
				}
				else
					MessageBox (hwnd, "Could not save Hexdump.", "Export hexdump", MB_OK | MB_ICONERROR);
			}//end
			if( iCopyHexdumpType == IDC_EXPORTRTF ){
				GlobalUnlock( hg );
				GlobalFree( hg );
				hg = pMem = NULL;
			}
		}
		if(pMem) delete [] pMem;
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK CopyHexdumpDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[16];
			sprintf (buf, "%x", iCopyHexdumpDlgStart);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "%x", iCopyHexdumpDlgEnd);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			if (iCopyHexdumpMode == BST_CHECKED)
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
			else
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
//Pabs changed - line insert
			CheckDlgButton (hDlg, iCopyHexdumpType, BST_CHECKED);
//end
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf1[16], buf2[16];
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf1, 16)!=0 && GetDlgItemText (hDlg, IDC_EDIT2, buf2, 16))
				{
					sscanf (buf1, "%x", &iCopyHexdumpDlgStart);
					sscanf (buf2, "%x", &iCopyHexdumpDlgEnd);
					iCopyHexdumpMode = IsDlgButtonChecked (hDlg, IDC_RADIO2);
//Pabs changed - line insert
					if( IsDlgButtonChecked (hDlg, IDC_EXPORTDISPLAY) )
						iCopyHexdumpType = IDC_EXPORTDISPLAY;
					else if( IsDlgButtonChecked (hDlg, IDC_EXPORTDIGITS) )
						iCopyHexdumpType = IDC_EXPORTDIGITS;
					else if( IsDlgButtonChecked (hDlg, IDC_EXPORTRTF) )
						iCopyHexdumpType = IDC_EXPORTRTF;
//end
					EndDialog (hDlg, TRUE);
				}
				else
					EndDialog (hDlg, FALSE);
				return TRUE;
			}
		case IDCANCEL:
			EndDialog (hDlg, FALSE);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
// On paste command.
int HexEditorWindow::CMD_edit_paste ()
{
//Pabs removed reinitialisations
	if( iInsertMode )
		iPasteMode = 2;
	else
		iPasteMode = 1;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_PASTEDIALOG), hwnd, (DLGPROC) PasteDlgProc))
	{
		switch( iPasteMode )
		{
		case 1: // Overwrite.
			{
				char* pcPastestring;
				// Create findstring.
				int destlen;
				if( iPasteAsText == TRUE )
				{
					destlen = strlen( pcPasteText );
					pcPastestring = new char[ destlen ];
					memcpy( pcPastestring, pcPasteText, destlen );
				}
				else
				{
					destlen = create_bc_translation (&pcPastestring, pcPasteText, strlen (pcPasteText), iCharacterSet, iBinaryMode);
				}
				if (destlen > 0)
				{
					// Enough space for writing?
					// DataArray.GetLength()-iCurByte = number of bytes from including curbyte to end.
//Pabs changed - "(iPasteSkip+destlen)" used to be "destlen"
					if (DataArray.GetLength()-iCurByte >= (iPasteSkip+destlen)*iPasteTimes)
//end
					{
						// Overwrite data.
						SetCursor (LoadCursor (NULL, IDC_WAIT));
						int i,k;
						for (k=0; k<iPasteTimes; k++)
						{
							for (i=0; i<destlen; i++)
							{
//Pabs changed - "(iPasteSkip+destlen)" used to be "destlen"
								DataArray[(iCurByte+k*(iPasteSkip+destlen))+i] = pcPastestring[i];
//end
							}
						}
						SetCursor (LoadCursor (NULL, IDC_ARROW));
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
					}
					else
					{
						MessageBox (hwnd, "Not enough space for overwriting.", "Paste", MB_OK | MB_ICONERROR);
					}
					delete [] pcPastestring;
				}
				delete [] pcPasteText;
				repaint ();
				break;
			}

		case 2: // Insert.
			{
				char* pcPastestring;
				int destlen;
				if( iPasteAsText == TRUE )
				{
					destlen = strlen( pcPasteText );
					pcPastestring = new char[ destlen ];
					memcpy( pcPastestring, pcPasteText, destlen );
				}
				else
				{
					destlen = create_bc_translation( &pcPastestring, pcPasteText, strlen( pcPasteText ), iCharacterSet, iBinaryMode );
				}
				if( destlen > 0 )
				{
					// Insert at iCurByte. Bytes there will be pushed up.
					SetCursor( LoadCursor( NULL, IDC_WAIT ) );
//Pabs changed - line insert
					int i, k;
					for( k = 0,i=iCurByte; k < iPasteTimes; k++ ){
						if(!DataArray.InsertAtGrow(iCurByte,(unsigned char*)pcPastestring,0,destlen)){
							delete [] pcPastestring;
							delete [] pcPasteText;
							SetCursor (LoadCursor (NULL, IDC_ARROW));
							MessageBox (hwnd, "Not enough memory for inserting.", "Paste", MB_OK | MB_ICONERROR);
							update_for_new_datasize ();
							return FALSE;
						}
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
						iCurByte+=destlen+iPasteSkip;
					}
					iCurByte=i;
//end
					// RK: removed old code that pabs commented out.
					m_iFileChanged = TRUE;
					bFilestatusChanged = TRUE;
					update_for_new_datasize ();
					delete [] pcPastestring;
					SetCursor (LoadCursor (NULL, IDC_ARROW));
				}
				else
				{
					delete [] pcPasteText;
					MessageBox( hwnd, "Tried to insert zero-length array.", "Paste", MB_OK | MB_ICONERROR);
					update_for_new_datasize ();
					return FALSE;
				}
				delete [] pcPasteText;
				repaint ();
				break;
			}

		default:
			break;
		}
	}
	return 0;
}

//-------------------------------------------------------------------
// On "enter decimal value" command.
int HexEditorWindow::CMD_edit_enterdecimalvalue ()
{
	iDecValDlgOffset = iCurByte;
	if (iCurByte <= LASTBYTE && iCurByte >= 0)
//Pabs inserted
	{
		int t = LASTBYTE - iCurByte + 1;
		//Set the size down a bit if someone called this func with a size thats too large
		for(;t<iDecValDlgSize;iDecValDlgSize/=2);
		//Get the right value
		if(iDecValDlgSize==2)
			iDecValDlgValue = *( (WORD*) &(DataArray[iCurByte]) );
		else if(iDecValDlgSize==4)
			iDecValDlgValue = *( (DWORD*) &(DataArray[iCurByte]) );
		else
//end
			iDecValDlgValue = (int) DataArray[iCurByte];
//Pabs inserted
	}
//end
	else
		iDecValDlgValue = 0;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_DECIMALDIALOG), hwnd, (DLGPROC) EnterDecimalValueDlgProc))
	{
		if (iDecValDlgOffset<0 || iDecValDlgOffset>LASTBYTE)
		{
			MessageBox (hwnd, "Invalid start offset.", "Enter decimal value", MB_OK | MB_ICONERROR);
			return 0;
		}//Pabs inserted bugfix "-1"
		if (iDecValDlgOffset+iDecValDlgSize*iDecValDlgTimes-1 > LASTBYTE)
		{
			MessageBox (hwnd, "Not enough space for writing decimal values.", "Enter decimal value", MB_OK | MB_ICONERROR);
			return 0;
		}
		SetCursor (LoadCursor (NULL, IDC_WAIT));
		int i, k = 0;
		for (i = 0; i < iDecValDlgTimes; i++)
		{
			if (iDecValDlgOffset + k > DataArray.GetUpperBound ())
			{
				MessageBox (hwnd, "Reached end of file prematurely.", "Enter decimal value", MB_OK | MB_ICONERROR);
				break;
			}

			if (iBinaryMode == LITTLEENDIAN_MODE)
			{
				switch (iDecValDlgSize)
				{
				case 1:
					DataArray[iDecValDlgOffset + k] = (BYTE)iDecValDlgValue;
					break;

				case 2:
					DataArray[iDecValDlgOffset + k] = (BYTE) (iDecValDlgValue & 0xff);
					DataArray[iDecValDlgOffset + k+1] = (BYTE) ((iDecValDlgValue & 0xff00) >> 8);
					break;

				case 4:
					DataArray[iDecValDlgOffset + k  ] = (BYTE) (iDecValDlgValue & 0xff);
					DataArray[iDecValDlgOffset + k+1] = (BYTE) ((iDecValDlgValue & 0xff00) >> 8);
					DataArray[iDecValDlgOffset + k+2] = (BYTE) ((iDecValDlgValue & 0xff0000) >> 16);
					DataArray[iDecValDlgOffset + k+3] = (BYTE) ((iDecValDlgValue & 0xff000000) >> 24);
					break;
				}
			}
			else
			{
				switch (iDecValDlgSize)
				{
				case 1:
					DataArray[iDecValDlgOffset + k] = (BYTE)iDecValDlgValue;
					break;

				case 2:
					DataArray[iDecValDlgOffset + k+1] = (BYTE) (iDecValDlgValue & 0xff);
					DataArray[iDecValDlgOffset + k] = (BYTE) ((iDecValDlgValue & 0xff00) >> 8);
					break;

				case 4:
					DataArray[iDecValDlgOffset + k+3] = (BYTE) (iDecValDlgValue & 0xff);
					DataArray[iDecValDlgOffset + k+2] = (BYTE) ((iDecValDlgValue & 0xff00) >> 8);
					DataArray[iDecValDlgOffset + k+1] = (BYTE) ((iDecValDlgValue & 0xff0000) >> 16);
					DataArray[iDecValDlgOffset + k+0] = (BYTE) ((iDecValDlgValue & 0xff000000) >> 24);
					break;
				}
			}
			k += iDecValDlgSize;
		}
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		repaint ();
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK EnterDecimalValueDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[16];
			sprintf (buf, "%d", iDecValDlgValue);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "x%x", iDecValDlgOffset);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			sprintf (buf, "1");
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), buf);
//Pabs inserted
			if(iDecValDlgSize==2)
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
			else if(iDecValDlgSize==4)
				CheckDlgButton (hDlg, IDC_RADIO3, BST_CHECKED);
			else
//end - next line indented
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
			iDecValDlgSize = 1;
		else if (IsDlgButtonChecked (hDlg, IDC_RADIO2) == BST_CHECKED)
			iDecValDlgSize = 2;
		else if (IsDlgButtonChecked (hDlg, IDC_RADIO3) == BST_CHECKED)
			iDecValDlgSize = 4;
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[16];
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 16) != 0)
					sscanf (buf, "%d", &iDecValDlgValue);
				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 16) != 0)
					sscanf (buf, "%x", &iDecValDlgOffset);
				int i;
				if (sscanf (buf, "x%x", &i) == 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{
						i = -1;
					}
				}
				iDecValDlgOffset = i;
				if (GetDlgItemText (hDlg, IDC_EDIT3, buf, 16) != 0)
					sscanf (buf, "%d", &iDecValDlgTimes);
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
BOOL CALLBACK PasteDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char *buf;
			if (OpenClipboard (NULL))
			{
				HGLOBAL hClipMemory = GetClipboardData (CF_TEXT);
				if (hClipMemory != NULL)
				{
					int gsize = GlobalSize (hClipMemory);
					if (gsize > 0)
					{
						char* pClipMemory = (char*) GlobalLock (hClipMemory);
						buf = new char[gsize];
						memcpy (buf, pClipMemory, gsize);
						SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
						delete [] buf;
					}
					GlobalUnlock (hClipMemory);
				}
				CloseClipboard ();
			}
			char buf2[16];
			sprintf (buf2, "%d", iPasteTimes);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf2);
//Pabs changed - line insert
			sprintf (buf2, "%d", iPasteSkip);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), buf2);
//end
			if( iPasteMode == 1 )
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			else
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
			if( iPasteAsText == TRUE )
				CheckDlgButton (hDlg, IDC_CHECK1, BST_CHECKED);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				if (IsDlgButtonChecked (hDlg, IDC_CHECK1) == BST_CHECKED)
					iPasteAsText = TRUE;
				else
					iPasteAsText = FALSE;
				if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
					iPasteMode = 1;
				else if( IsDlgButtonChecked( hDlg, IDC_RADIO2 ) == BST_CHECKED )
					iPasteMode = 2;
				char buf[64];
				int i;
				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 64) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{//Pabs replaced NULL w hDlg
						MessageBox (hDlg, "Number of times to paste not recognized.", "Paste", MB_OK | MB_ICONERROR);
						i = -1;
					}
				}
				if (i==-1 || i==0)
				{
					MessageBox (hDlg, "Number of times to paste must be at least 1.", "Paste", MB_OK | MB_ICONERROR);
					EndDialog (hDlg, 0);//end
					return 0;
				}
				iPasteTimes = i;
//Pabs changed - line insert
				if (GetDlgItemText (hDlg, IDC_EDIT3, buf, 64) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{
						MessageBox (hDlg, "Number of bytes to skip not recognized.", "Paste", MB_OK | MB_ICONERROR);
						EndDialog (hDlg, 0);
						return 0;
					}
				}
				iPasteSkip = i;
//end
				iPasteMaxTxtLen = SendMessage (GetDlgItem (hDlg, IDC_EDIT1), EM_GETLIMITTEXT, 0, 0) + 1;
				pcPasteText = new char[iPasteMaxTxtLen];
				GetDlgItemText (hDlg, IDC_EDIT1, pcPasteText, iPasteMaxTxtLen);
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
// Find char c from pointer to array src on, return it's position.
int HexEditorWindow::find_byte_pos (char* src, char c)
{
	int i=0;
	while (src[i] != c)
	{
		i++;
	}
	return i;
}

//-------------------------------------------------------------------
// dest must be set to right length before calling.
int HexEditorWindow::create_bc_translation (char* dest, char* src, int srclen, int charmode, int binmode)
{
	int i, di=0, bclen;
	for (i=0; i<srclen; i++)
	{
		if ((bclen = is_bytecode (&(src[i]), srclen-i)) > 0) // Get length of byte-code.
		{
			// Bytecode found.
			translate_bytecode (&(dest[di]), &(src[i]), srclen-i, binmode);
			di += bclen;
			i += find_byte_pos (&(src[i]), '>');
		}
		else // Normal character.
		{
			if (src[i] == '\\') // Special char "\<" or "\\"?
			{
				if (i+1 < srclen)
				{
					if (src[i+1] == '<')
					{
						dest[di++] = '<'; // Special char recognized.
						i++;
					}
					else if( src[i+1] == '\\' )
					{
						dest[di++] = '\\'; // Special char recognized.
						i++;
					}
					else
						dest[di++] = src[i]; // Unknown special char.
				}
				else
					dest[di++] = src[i]; // Not enough space for special char.
			}
			else
			{
				// No special char.
				switch (charmode)
				{
				case ANSI_SET:
					dest[di++] = src[i];
					break;

				case OEM_SET:
					dest[di++] = TranslateAnsiToOem (src[i]);
					break;
				}
			}
		}
	}
	return di;
}

//-------------------------------------------------------------------
// Get value of one code.
// Return: value of code.
// bytecode must be checked before.
int HexEditorWindow::translate_bytecode (char* dest, char* src, int srclen, int binmode)
{
	int i, k=0;
	char buf[50];
	for (i=4; i<srclen; i++)
	{
		if (src[i]=='>')
			break;
		else
		{
			buf[k++] = src[i];
		}
	}
	buf[k] = 0;
	int value;
	float fvalue;
	double dvalue;
	switch (src[2]) // Get value from text.
	{
	case 'd':
		sscanf (buf, "%d", &value);
		break;

	case 'h':
		sscanf (buf, "%x", &value);
		break;

	case 'l':
		sscanf (buf, "%f", &fvalue);
		break;

	case 'o':
		sscanf (buf, "%lf", &dvalue);
		break;
	}

	if (binmode == LITTLEENDIAN_MODE)
	{
		switch (src[1])
		{
		case 'b':
			dest[0] = (char) value;
			break;

		case 'w':
			dest[0] = (char)(value & 0xff);
			dest[1] = (char)((value & 0xff00)>>8);
			break;

		case 'l':
			dest[0] = (char)(value & 0xff);
			dest[1] = (char)((value & 0xff00)>>8);
			dest[2] = (char)((value & 0xff0000)>>16);
			dest[3] = (char)((value & 0xff000000)>>24);
			break;

		case 'f':
			*((float*)dest) = fvalue;
			break;

		case 'd':
			*((double*)dest) = dvalue;
			break;
		}
	}
	else // BIGENDIAN_MODE
	{
		switch (src[1])
		{
		case 'b':
			dest[0] = (char) value;
			break;

		case 'w':
			dest[0] = HIBYTE (LOWORD (value));
			dest[1] = LOBYTE (LOWORD (value));
			break;

		case 'l':
			dest[0] = HIBYTE (HIWORD (value));
			dest[1] = LOBYTE (HIWORD (value));
			dest[2] = HIBYTE (LOWORD (value));
			dest[3] = LOBYTE (LOWORD (value));
			break;

		case 'f':
			{
				char* p = (char*) &fvalue;
				int i;
				for (i=0; i<4; i++)
				{
					dest[i] = p[3-i];
				}
			}
			break;

		case 'd':
			{
				char* p = (char*) &dvalue;
				int i;
				for (i=0; i<8; i++)
				{
					dest[i] = p[7-i];
				}
			}
			break;
		}
	}
	return value;
}

//-------------------------------------------------------------------
// Get length of code.
int HexEditorWindow::calc_bctrans_destlen (char* src, int srclen)
{
	int i, destlen = 0, l, k;
	for (i=0; i<srclen; i++)
	{
		if ((l = is_bytecode (&(src[i]), srclen-i)) == 0)
		{
			if (src[i] == '\\')
			{
				if (i+1 < srclen)
				{
					if (src[i+1] == '<')
					{
						// Code for "<" alone without decoding.
						destlen++;
						i++;
					}
					else if( src[i+1] == '\\' )
					{
						// Code for "\\".
						destlen++;
						i++;
					}
					else
					{
						destlen++;
					}
				}
				else
				{
					destlen++;
				}
			}
			else
			{
				destlen++;
			}
		}
		else
		{
			destlen += l;
			for (k=i; i<srclen; k++)
			{
				if (src[k]=='>')
					break;
			}
			i = k;
		}
	}
	return destlen;
}

//-------------------------------------------------------------------
// Bytecode?
// Return = 0 if no bytecode
//        = Length 1/2/4 if bytecode
int HexEditorWindow::is_bytecode (char* src, int len)
{
	int i=0;

	if (src[i] == '<')
	{
		if (i+1 < len)
		{
			switch (src[i+1])
			{
			case 'b': case 'w': case 'l': case 'f': case 'd':
				if (i+2 < len)
				{
					switch (src[i+2])
					{
						case 'd': case 'h': case 'l': case 'o':
							if (i+3 < len)
							{
								if (src[i+3] == ':')
								{
									int j,k;
									for (j=4; j < len; j++)
									{
										if (src[i+j] == '>')
											break;
									}
									if (j==4 || j==len)
										return FALSE;
									for (k=4; k<j; k++)
									{
										switch (src[i+2])
										{
										case 'd':
											if ((src[i+k]>='0' && src[i+k]<='9') || src[i+k]=='-')
												continue;
											else
												return FALSE; // Non-digit found.
											break;

										case 'h':
											if ((src[i+k]>='0' && src[i+k]<='9') ||
												(src[i+k]>='a' && src[i+k]<='f'))
												continue;
											else
												return FALSE; // Non-hex-digit.
											break;

										case 'o': case 'l': // float or double.
											if ((src[i+k]>='0' && src[i+k]<='9') || src[i+k]=='-' || src[i+k]=='.' || src[i+k]=='e' || src[i+k]=='E')
												continue;
											else
												return FALSE;
											break;
										}
									}
									switch (src[i+1])
									{
									default:
									case 'b': return 1;
									case 'w': return 2;
									case 'l': return 4;
									case 'f': return 4;
									case 'd': return 8;
									}
								}
								else
									return FALSE; // No ':'.
							}
							else
								return FALSE; // No space for ':'.
							break;

						default:
							return FALSE; // Wrong second option.
					}
				}
				else
					return FALSE; // No space for option 2.
				break;

			default:
				return FALSE; // Wrong first option.
				break;
			}
		}
		else
			return FALSE; // No space for option 1;
	}
	else
		return FALSE; // No '<'.
}

//-------------------------------------------------------------------
// Create translation of bytecode-string.
// Return: Length of resulting string.
// ppd = pointer to pointer to result, must be delete[]-ed later.
// If the input string was empty, no translated array is created and zero is returned.
int HexEditorWindow::create_bc_translation (char** ppd, char* src, int srclen, int charmode, int binmode)
{
	int destlen = calc_bctrans_destlen (src, srclen);
	if (destlen > 0)
	{
		*ppd = new char[destlen];
		create_bc_translation (*ppd, src, srclen, charmode, binmode);
		return destlen;
	}
	else
	{
		// Empty input string => don't allocate anything and return 0.
		*ppd = NULL;
		return 0;
	}
}

//-------------------------------------------------------------------
// Translate an array of bytes to a text string using special syntax.
// Return: Length of string including zero-byte.
int HexEditorWindow::translate_bytes_to_BC (char* pd, unsigned char* src, int srclen)
{
	int i, k = 0;
	char buf[16];
	for (i=0; i<srclen; i++)
	{
		if (src[i] == '<')
		{
			pd[k++] = '\\';
			pd[k++] = '<';
		}
		else if( src[i] == '\\' )
		{
			pd[k++] = '\\';
			pd[k++] = '\\';
		}
		else if (src[i] >= 32 && src[i] < 127)
		{
			pd[k++] = src[i];
		}
		else if( src[i]==10 || src[i]==13 )
		{
			pd[k++] = src[i];
		}
		else
		{
			pd[k++] = '<';
			pd[k++] = 'b';
			pd[k++] = 'h';
			pd[k++] = ':';
			sprintf (buf, "%2.2x", src[i]);
			pd[k++] = buf[0];
			pd[k++] = buf[1];
			pd[k++] = '>';
		}
	}
	pd[k] = '\0';
	return k+1;
}

//-------------------------------------------------------------------
// Used with translate_bytes_to_BC.
// Return: Length of bytecode-string including zero-byte.
int HexEditorWindow::byte_to_BC_destlen (char* src, int srclen)
{
	int i, destlen = 1;
	for (i=0; i<srclen; i++)
	{
		if (src[i] == '<')
			destlen+=2; // Escapecode needed.
		else if( src[i] == '\\' )
			destlen+=2; // Escapecode needed.
		else if (src[i] >= 32 && src[i] < 127)
			destlen++; // Normal char.
		else if( src[i]==10 || src[i]==13 )
			destlen++; // LF/CR.
		else
			destlen+=7; // Escapecode needed.
	}
	return destlen;
}

//-------------------------------------------------------------------
// If filesize changes, scrollbars etc. must be adjusted.
void HexEditorWindow::update_for_new_datasize ()
{
	RECT r;
	GetClientRect (hwnd, &r);
	resize_window (r.right, r.bottom);
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_edit_cut ()
{
	if (bSelected) // If selecting...
	{
		if (iEndOfSelection >= iStartOfSelection)
		{
			iCutOffset = iStartOfSelection;
			iCutNumberOfBytes = iEndOfSelection-iStartOfSelection+1;
		}
		else
		{
			iCutOffset = iEndOfSelection;
			iCutNumberOfBytes = iStartOfSelection-iEndOfSelection+1;
		}
	}
	else // No selection: cut current byte.
	{
		iCutOffset = iCurByte;
		iCutNumberOfBytes = 1;
	}
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_CUTDIALOG), hwnd, (DLGPROC) CutDlgProc))
	{
		// Can requested number be cut?
		// DataArray.GetLength ()-iCutOffset = number of bytes from current pos. to end.
		if( DataArray.GetLength() - iCutOffset >= iCutNumberOfBytes )
		{
			// OK
			//int newlen = DataArray.GetLength () - iCutNumberOfBytes;
			// Cut to clipboard?
			switch (iCutMode)
			{
			case BST_CHECKED:
				{
					// Transfer to cipboard.
					int destlen = byte_to_BC_destlen ((char*) &(DataArray[iCutOffset]), iCutNumberOfBytes);
					HGLOBAL hGlobal = GlobalAlloc (GHND, destlen);
					if (hGlobal != NULL)
					{
						SetCursor (LoadCursor (NULL, IDC_WAIT));
						char* pd = (char*) GlobalLock (hGlobal);
						translate_bytes_to_BC (pd, &(DataArray[iCutOffset]), iCutNumberOfBytes);
						GlobalUnlock (hGlobal);
						OpenClipboard (hwnd);
						EmptyClipboard ();
						SetClipboardData (CF_TEXT, hGlobal);
						CloseClipboard ();
						SetCursor (LoadCursor (NULL, IDC_ARROW));
					}
					else
					{//Pabs replaced NULL w hwnd
						// Not enough memory for clipboard.
						MessageBox (hwnd, "Not enough memory for cutting to clipboard.", "Cut", MB_OK | MB_ICONERROR);
						return 0;
					}//end
					break;
				}

			default:
				break;
			}
			// Cut data.
			if (DataArray.RemoveAt (iCutOffset, iCutNumberOfBytes) == FALSE)
			{//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Could not cut data.", "Cut", MB_OK | MB_ICONERROR);
				return FALSE;
			}//end
			iCurByte = iCutOffset;
			if (iCurByte > LASTBYTE)
				iCurByte = LASTBYTE;
			if (iCurByte<0)
				iCurByte=0;
			m_iFileChanged = TRUE;
			bFilestatusChanged = TRUE;
			bSelected = FALSE;
			update_for_new_datasize ();
			repaint ();
		}
		else
		{//Pabs replaced NULL w hwnd
			// Too many bytes to cut.
			MessageBox (hwnd, "Can't cut more bytes than are present.", "Cut", MB_OK | MB_ICONERROR);
		}//end
	}
	return 0;
}

//-------------------------------------------------------------------
BOOL CALLBACK CutDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[32];
			sprintf (buf, "x%x", iCutOffset);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "x%x", iCutOffset+iCutNumberOfBytes-1);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			sprintf (buf, "%d", iCutNumberOfBytes);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), buf);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			CheckDlgButton (hDlg, IDC_CHECK1, iCutMode);
			CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[64];
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 64) != 0)
				{
					int i;
					if (sscanf (buf, "x%x", &i) == 0)
					{
						if (sscanf (buf, "%d", &i) == 0)
						{//Pabs replaced NULL w hDlg
							MessageBox (hDlg, "Start offset not recognized.", "Cut", MB_OK | MB_ICONERROR);
							i = -1;
						}//end
					}
					if (i==-1)
					{
						EndDialog (hDlg, 0);
						return 0;
					}
					iCutOffset = i;
				}

				switch (IsDlgButtonChecked (hDlg, IDC_RADIO1))
				{
				case BST_CHECKED: // Get end offset.
					{
						if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 64) != 0)
						{
							int i;
							if (sscanf (buf, "x%x", &i) == 0)
							{
								if (sscanf (buf, "%d", &i) == 0)
								{//Pabs replaced NULL w hDlg
									MessageBox (hDlg, "End offset not recognized.", "Cut", MB_OK | MB_ICONERROR);
									i = -1;
								}//end
							}
							if (i==-1)
							{
								EndDialog (hDlg, 0);
								return 0;
							}
							iCutNumberOfBytes = i-iCutOffset+1;
						}
					}
					break;

				default: // Get number of bytes.
					{
						if (GetDlgItemText (hDlg, IDC_EDIT3, buf, 64) != 0)
						{
							int i;
							if (sscanf (buf, "%d", &i) == 0)
							{//Pabs replaced NULL w hDlg
								MessageBox (hDlg, "Number of bytes not recognized.", "Cut", MB_OK | MB_ICONERROR);
								i = -1;
							}//end
							if (i==-1)
							{
								EndDialog (hDlg, 0);
								return 0;
							}
							iCutNumberOfBytes = i;
						}
					}
				}

				iCutMode = IsDlgButtonChecked (hDlg, IDC_CHECK1);
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
BOOL CALLBACK CopyDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[32];
			sprintf (buf, "x%x", iCopyStartOffset);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "x%x", iCopyEndOffset);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			sprintf (buf, "%d", iCopyEndOffset-iCopyStartOffset+1);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), buf);
			CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[64];
				// Read start offset.
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 64) != 0)
				{
					int i;
					if (sscanf (buf, "x%x", &i) == 0)
					{
						if (sscanf (buf, "%d", &i) == 0)
						{//Pabs replaced NULL w hDlg
							MessageBox (hDlg, "Start offset not recognized.", "Copy", MB_OK | MB_ICONERROR);
							i = -1;
						}//end
					}
					if (i==-1)
					{
						EndDialog (hDlg, 0);
						return 0;
					}
					iCopyStartOffset = i;
				}

				switch (IsDlgButtonChecked (hDlg, IDC_RADIO1))
				{
				case BST_CHECKED: // Get end offset.
					{
						if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 64) != 0)
						{
							int i;
							if (sscanf (buf, "x%x", &i) == 0)
							{
								if (sscanf (buf, "%d", &i) == 0)
								{//Pabs replaced NULL w hDlg
									MessageBox (hDlg, "End offset not recognized.", "Copy", MB_OK | MB_ICONERROR);
									i = -1;
								}//end
							}
							if (i==-1)
							{
								EndDialog (hDlg, 0);
								return 0;
							}
							iCopyEndOffset = i;
						}
						break;
					}

				default: // Get number of bytes.
					{
						if (GetDlgItemText (hDlg, IDC_EDIT3, buf, 64) != 0)
						{
							int i;
							if (sscanf (buf, "%d", &i) == 0)
							{//Pabs replaced NULL w hDlg
								MessageBox (hDlg, "Number of bytes not recognized.", "Copy", MB_OK | MB_ICONERROR);
								i = -1;
							}//end
							if (i==-1)
							{
								EndDialog (hDlg, 0);
								return 0;
							}
							iCopyEndOffset = iCopyStartOffset + i;
						}
						break;
					}
				}

				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_new ()
{
	EnableDriveButtons(hwndToolBar, FALSE);
	if( m_iFileChanged == TRUE )
	{
//Pabs changed - restructured so that the user can save&new, new | not new if file changed
		int res = MessageBox (hwnd, "Do you want to save your changes?", "New", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to create new or User wants to save and the save was unsuccessful
			return 0;//Don't empty file
//		if (MessageBox (hwnd, "File was changed! New anyway?", "New", MB_YESNO | MB_ICONQUESTION) == IDNO)
//			return 0;
//end
	}

    if( Drive->IsOpen() )
        Drive->Close();

	bFileNeverSaved = TRUE;
	bSelected = FALSE;
//Pabs replaced bLButtonIsDown with bSelecting & inserted
	bLButtonDown = FALSE;
	bMoving = FALSE;
	bSelecting = FALSE;
	bDroppedHere = FALSE;
//end
	m_iFileChanged = FALSE;
	bFilestatusChanged = TRUE;
	iVscrollMax = 0;
	iVscrollPos = 0;
	iVscrollInc = 0;
	iHscrollMax = 0;
	iHscrollPos = 0;
	iHscrollInc = 0;
	iCurLine = 0;
	iCurByte = 0;
	iCurNibble = 0;
	bPartialStats = 0;
	bPartialOpen=FALSE;
	// Delete old data.
	DataArray.ClearAll ();
	sprintf (filename, "Untitled");
	update_for_new_datasize ();
	repaint ();
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_save_as ()
//Pabs changed this function so it would return 0 when unsuccessful
{
	char szFileName[_MAX_PATH] = "";
	char szTitleName[_MAX_FNAME + _MAX_EXT] = "";
	if(!Drive->IsOpen()) strcpy (szFileName, filename);
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetSaveFileName (&ofn))
	{
//Pabs inserted
		WaitCursor w1;
//end
		int filehandle;

		// Check if file already exists first.
		if ((filehandle = _open (szFileName, _O_RDWR|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			// File already exists: ask what is to be done.
			_close (filehandle);
//Pabs replaced NULL w hwnd
			if( MessageBox (hwnd, "A file of that name already exists!\n"
				"Do you want to overwrite it?",
				"Save as",//end
				MB_YESNO | MB_ICONWARNING ) == IDNO )
			{
				// Don't overwrite.
//Pabs changed 0 used to be 1 - didn't save file
				return 0;
//end
			}
		}

		if ((filehandle = _open (szFileName, _O_RDWR|_O_CREAT|_O_TRUNC|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			//Pabs removed setcursor call
			if ((_write (filehandle, DataArray, DataArray.GetLength ())) != -1)
			{
				// File was saved.
//Pabs replaced strcpy w GetLongPathNameWin32
				GetLongPathNameWin32( szFileName, filename, _MAX_PATH );
//end
				m_iFileChanged = FALSE;
				bFilestatusChanged = TRUE;
				bFileNeverSaved = FALSE;
				bPartialStats = 0;
				bPartialOpen=FALSE;
				update_MRU();
			}
//Pabs changed to return 0 if failed
			else{//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Could not save file.", "Save as", MB_OK | MB_ICONERROR);
				_close (filehandle);
				return 0;
			}
//end
			//Pabs removed setcursor call
			_close (filehandle);
		}
//Pabs changed to return 0 if failed
		else{
			MessageBox (hwnd, "Could not save file.", "Save as", MB_OK | MB_ICONERROR);
			return 0;//end
		}
//end
	}
//Pabs inserted - didn't save
	else return 0;
//end
	repaint ();
	return 1;
}
//-------------------------------------------------------------------
//Pabs changed this function so it would return 0 when unsuccessful
int HexEditorWindow::CMD_save ()
{
//Pabs inserted
	WaitCursor w1;
//end

	// File was not saved before => name must be chosen.
	if( bFileNeverSaved )
	{//Pabs replaced message w return CMD_save_as();
		return CMD_save_as();
	}//end

//Pabs inserted
	if( bMakeBackups ){//Assume the filename is valid & has a length
		int len = strlen(filename);
		char* newname = new char[len+5];//".bak" appended
		if(newname){
			strcpy(newname,filename);
			strcat(newname,".bak");
			remove(newname);
			//Must use Win32 here as the CRT has no copy function only rename
			//& we need a copy of the file to be present for saving a partially opened file
			if(!CopyFile(filename,newname,TRUE))
				MessageBox(hwnd,
				"Could not backup file\n"
				"Backup aborted, Save continuing","Backup",MB_OK);
			delete[]newname;
		}
		else MessageBox(hwnd,
			"Could not allocate backup name buffer\n"
			"Backup aborted, Save continuing","Backup",MB_OK);
	}
//end

	// File is partially loaded => must be saved partially or saved as.
	if (bPartialOpen)
	{
		int filehandle;
		if ((filehandle = _open (filename,_O_RDWR|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
//Pabs removed WaitCursor declaration
//Pabs inserted to allow resizing of the DataArray
			int nbl, i, n, r, e;//Length of the DataArray, loop var & start of loop, loop increment, relative movement len, end of loop
			BYTE tmp;//Temporary byte for moving data
			nbl = DataArray.GetLength();

			if(nbl != iPartialOpenLen){
				i = iPartialOffset + iPartialOpenLen;
				e = iPartialFileLen - 1;
				if(nbl > iPartialOpenLen){//Bigger .'. we need to start at the end
					n = -1;
					swap(e,i);
				}
				else if(nbl < iPartialOpenLen){//Smaller .'. we need to start at the start
					n = 1;
				}
				r = nbl - iPartialOpenLen;

				//move the data
				e+=n;
				do{
					_lseek(filehandle,i,SEEK_SET);
					_read(filehandle,&tmp,1);
					_lseek(filehandle,i+r,SEEK_SET);
					if( -1 == _write(filehandle,&tmp,1) ){
						MessageBox( hwnd, "Could not move data in the file.", "Save", MB_OK | MB_ICONERROR );
						_close( filehandle );
						return 0;
					}
					i+=n;
				}while(i!=e);
				if(nbl < iPartialOpenLen){//If the new file is bigger than the first _write will resize the file properly otherwise we need to specifically resize the file
					if( -1 == _chsize(filehandle, iPartialFileLen + r)){
						MessageBox( hwnd, "Could not resize the file.", "Save", MB_OK | MB_ICONERROR );
						_close( filehandle );
						return 0;
					}
				}
			}
//end
			if( _lseek( filehandle, iPartialOffset, 0 ) == -1 )
			{
				MessageBox( hwnd, "Could not seek in file.", "Save", MB_OK | MB_ICONERROR );
				_close( filehandle );
				return 0;
			}//Pabs replaced "DataArray.GetLength()" w "nbl"
			if( _write( filehandle, DataArray, nbl ) == -1 )
			{//end
				MessageBox( hwnd, "Could not write data to file.", "Save", MB_OK | MB_ICONERROR );
//Pabs inserted
				_close( filehandle );
				return 0;
//end
			}
			_close (filehandle);
			m_iFileChanged = FALSE;
			bFilestatusChanged = TRUE;
		}
		else
		{//Pabs replaced NULL w hwnd
			MessageBox (hwnd, "Could not save partially opened file.", "Save", MB_OK | MB_ICONERROR );
		//end
//Pabs inserted
			return 0;
//end
		}
		repaint ();
		return 1;
	}

	int filehandle;
	if ((filehandle = _open (filename,_O_RDWR|_O_CREAT|_O_TRUNC|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
	{
//Pabs changed to allow for error handling & use of the WaitCursor class
		//SetCursor (LoadCursor (NULL, IDC_WAIT));
		if( -1 == _write (filehandle, DataArray, DataArray.GetLength ()) ){
	//Pabs replaced NULL w hwnd
			MessageBox (hwnd, "Could not write data to file.", "Save", MB_OK | MB_ICONERROR);
			_close (filehandle);
			return 0;
		}
		_close (filehandle);
		//SetCursor (LoadCursor (NULL, IDC_ARROW));
//end
		m_iFileChanged = FALSE;
		bFilestatusChanged = TRUE;
		bPartialStats = 0;
		bPartialOpen = FALSE;
	}
	else
	{
		MessageBox (hwnd, "Could not save file.", "Save", MB_OK | MB_ICONERROR);
//end
//Pabs inserted
		return 0;
//end
	}
	repaint ();
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_open ()
{
	if (m_iFileChanged == TRUE)
	{
//Pabs changed - restructured so that the user can save&open, open | not open if file changed
		int res = MessageBox (hwnd, "Do you want to save your changes?", "Open", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to open or User wants to save and the save was unsuccessful
			return 0;//Don't open
//		if (MessageBox (hwnd, "File was changed! Open anyway?", "Open", MB_YESNO | MB_ICONQUESTION) == IDNO)
//			return 0;
	}
	char szFileName[_MAX_PATH];
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	szFileName[0] = '\0';
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetOpenFileName (&ofn))
	{
		if (load_file (szFileName))
		{
			iVscrollMax = 0;
			iVscrollPos = 0;
			iVscrollInc = 0;
			iHscrollMax = 0;
			iHscrollPos = 0;
			iHscrollInc = 0;
			iCurLine = 0;
			iCurByte = 0;
			iCurNibble = 0;
			bSelected = FALSE;
			m_iFileChanged = FALSE;
			bFilestatusChanged = TRUE;
			bFileNeverSaved = FALSE;
			RECT r;
			GetClientRect (hwnd, &r);
			SendMessage (hwnd, WM_SIZE, 0, (r.bottom << 16) | r.right);
			InvalidateRect (hwnd, NULL, FALSE);
			UpdateWindow (hwnd);
		}
	}
	return 1;
}

//-------------------------------------------------------------------
void HexEditorWindow::adjust_view_for_selection ()
{
	if( bSelected )
	{
//Pabs changed to put selection in center of screen
		int sosline,soscol,eosline,eoscol,maxcols,lines,cols;

		iStartOfSelSetting = iStartOfSelection;
		iEndOfSelSetting = iEndOfSelection;
		if(iStartOfSelSetting>iEndOfSelSetting)
			swap(iStartOfSelSetting,iEndOfSelSetting);

		sosline = iStartOfSelSetting / iBytesPerLine;
		eosline = iEndOfSelSetting / iBytesPerLine;
		if (m_iEnteringMode == BYTES){
			soscol = iMaxOffsetLen + iByteSpace + ( iStartOfSelSetting % iBytesPerLine) * 3;
			eoscol = iMaxOffsetLen + iByteSpace + ( iEndOfSelSetting % iBytesPerLine) * 3 + 2;
			maxcols = iBytesPerLine * 3;
		}
		else{
			soscol = CHARSTART + ( iStartOfSelSetting % iBytesPerLine );
			eoscol = CHARSTART + ( iEndOfSelSetting % iBytesPerLine );
			maxcols = iBytesPerLine;
		}

		lines = eosline - sosline + 1;
		cols = ((eosline == sosline) ? eoscol - soscol + 1 : maxcols);

		if( lines > cyBuffer ){
			if( iVscrollPos <= (sosline+eosline-cyBuffer+1)/2 )
				iCurLine = sosline;
			else iCurLine = eosline-cyBuffer+1;
		}
		else iCurLine = sosline - (cyBuffer - (eosline-sosline) )/2;

		int mincol, maxcol;
		if(soscol>=eoscol){maxcol=soscol;mincol=eoscol;}
		else{maxcol=eoscol;mincol=soscol;}

		if( cols > cxBuffer && maxcol-mincol+1 > cxBuffer ){
			if( abs(iHscrollPos-mincol) < abs(iHscrollPos+cxBuffer-1-maxcol) )
				iHscrollPos = mincol-1;
			else iHscrollPos = maxcol-cxBuffer+1;
		}
		else{
			if( maxcol-mincol+1 < cxBuffer && lines > 1){
				if (m_iEnteringMode == BYTES){
					mincol = iMaxOffsetLen + iByteSpace;
					maxcol = iMaxOffsetLen + iByteSpace + ( iBytesPerLine - 1 ) * 3 + 2;
				}
				else{
					mincol = CHARSTART;
					maxcol = CHARSTART + iBytesPerLine;
				}
			}
			iHscrollPos = mincol - (cxBuffer - (maxcol-mincol) )/2;
		}

		if( iHscrollPos > iHscrollMax - cxBuffer + 1 )
			iHscrollPos = iHscrollMax - cxBuffer + 1;
		if( iHscrollPos < 0 ) iHscrollPos = 0;
		if( iCurLine > iVscrollMax - cyBuffer + 1 )
			iCurLine = iVscrollMax - cyBuffer + 1;
		if( iCurLine < 0 ) iCurLine = 0;
		adjust_hscrollbar();
		adjust_vscrollbar();
//end
	}
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_view_settings()
{
	TxtEditName = TexteditorName;
	iBPLSetting = iBytesPerLine;
	iAutomaticXAdjust = iAutomaticBPL;
	iOffsetLenSetting = iMinOffsetLen;//Pabs replaced "iOffsetLen" with "iMinOffsetLen"
	bUnsignedViewSetting = bUnsignedView;
	bOpenReadOnlySetting = bOpenReadOnly;
//Pabs inserted
	bAutoOLSetting = bAutoOffsetLen;
//end
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_VIEWSETTINGSDIALOG), hwnd, (DLGPROC) ViewSettingsDlgProc))
	{
		TexteditorName = TxtEditName;

//Pabs changed INT_MAX and 2147483647 both used to be 9 and iMinOffsetLen used to be iOffsetLen
		if( iOffsetLenSetting > 0 && iOffsetLenSetting <= INT_MAX )
			iMinOffsetLen = iOffsetLenSetting;
		else
			MessageBox( hwnd, "Offset length must be a value between 1 and 2147483647 (INT_MAX).", "View settings",
				MB_OK | MB_ICONERROR );
//end
//Pabs inserted
		bAutoOffsetLen = bAutoOLSetting;
//end
		iBytesPerLine = iBPLSetting;
		iAutomaticBPL = iAutomaticXAdjust;
		bUnsignedView = bUnsignedViewSetting;
		bOpenReadOnly = bOpenReadOnlySetting;
		save_ini_data ();
		update_for_new_datasize ();
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK ViewSettingsDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[32];
			sprintf (buf, "%d", iBPLSetting);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "%d", iOffsetLenSetting);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			CheckDlgButton (hDlg, IDC_CHECK1, iAutomaticXAdjust);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			if( bUnsignedViewSetting )
				CheckDlgButton( hDlg, IDC_RADIO1, BST_CHECKED );
			else
				CheckDlgButton( hDlg, IDC_RADIO2, BST_CHECKED );

			if( bOpenReadOnlySetting )
				CheckDlgButton( hDlg, IDC_CHECK5, BST_CHECKED );
			else
				CheckDlgButton( hDlg, IDC_CHECK5, BST_UNCHECKED );
//Pabs inserted
			CheckDlgButton( hDlg, IDC_CHECK2, bAutoOLSetting ? BST_CHECKED : BST_UNCHECKED );
//end
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT3 ), TxtEditName );
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT4 ), BrowserName );
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[512];
				int i=-1;
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 512) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{
//Pabs changed all message boxes to be owned by the dialog
						MessageBox (hDlg, "Number of bytes not recognized.", "View Settings", MB_OK | MB_ICONERROR);
						i = -1;
					}
				}
				if (i==-1)
				{
					EndDialog (hDlg, 0);
					return 0;
				}
				iBPLSetting = i;
				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 512) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{
						MessageBox (hDlg, "Length of offset not recognized.", "View Settings", MB_OK | MB_ICONERROR);
						i = -1;
					}
				}
				if( i == -1 )
				{
					EndDialog( hDlg, 0 );
					return 0;
				}
				iOffsetLenSetting = i;
				// Get the text editor path and name.
				if( GetDlgItemText( hDlg, IDC_EDIT3, buf, 512 ) != 0 )
				{
					TxtEditName.SetToString( buf );
				}
				else
				{
					MessageBox( hDlg, "Field for text editor name was empty: name not changed.", "View settings", MB_OK | MB_ICONERROR );
				}
				// Get the Browser path and name.
				if( GetDlgItemText( hDlg, IDC_EDIT4, buf, 512 ) != 0 )
				{
					BrowserName = buf;
				}
				else
				{
					MessageBox( hDlg, "Field for Internet Browser was empty: name not changed.", "View settings", MB_OK | MB_ICONERROR );
				}
//end
				if (iBPLSetting < 1)
					iBPLSetting = 1;
				iAutomaticXAdjust = IsDlgButtonChecked (hDlg, IDC_CHECK1);
//Pabs inserted
				bAutoOLSetting = IsDlgButtonChecked (hDlg, IDC_CHECK2);
//end
				if (IsDlgButtonChecked (hDlg, IDC_RADIO1))
					bUnsignedViewSetting = TRUE;
				else
					bUnsignedViewSetting = FALSE;

				if( IsDlgButtonChecked( hDlg, IDC_CHECK5 ) )
					bOpenReadOnlySetting = TRUE;
				else
					bOpenReadOnlySetting = FALSE;
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
// Change the color indicated by pColor.
int HexEditorWindow::CMD_color_settings (COLORREF* pColor)
{
	CHOOSECOLOR cc;
	COLORREF crCustColors[16];
	cc.lStructSize = sizeof (CHOOSECOLOR);
	cc.hwndOwner = hwnd;
	cc.hInstance = NULL;
	cc.rgbResult = *pColor;
	cc.lpCustColors = crCustColors;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	cc.lCustData = 0L;
	cc.lpfnHook;
	cc.lpTemplateName = NULL;
	if (ChooseColor (&cc))
	{
		*pColor = cc.rgbResult;
		save_ini_data ();
		update_for_new_datasize ();
		repaint ();
	}
	return 0;
}

//-------------------------------------------------------------------
//Pabs changed - char key added
int HexEditorWindow::read_ini_data (char* key)
//end
{
	// Is there any data for frhed in the registry?
	HKEY key1;

//Pabs changed - lines rewritten to allow opening other versions of frhed for upgrade purposes
	char keyname[64] = "Software\\frhed\\";
	LONG res;
	if(!key){
		sprintf( &keyname[15], "v"CURRENT_VERSION"." SUB_RELEASE_NO "\\%d", g_iInstCount );
	}
	else{
		strcat( keyname, key );//add the version information
		sprintf( &keyname[strlen(keyname)], "\\%d", g_iInstCount );//strlen returns the index of the \0
	}

	res = RegOpenKeyEx( HKEY_CURRENT_USER,
		keyname,
		0,
		KEY_ALL_ACCESS,
		&key1 );
//end
	if( res == ERROR_SUCCESS )
	{
		// There is data: read it.
		DWORD datasize = sizeof( int );
		LONG res;
		res = RegQueryValueEx( key1, "iTextColorValue", NULL, NULL, (BYTE*) &iTextColorValue, &datasize );
		res = RegQueryValueEx( key1, "iBkColorValue", NULL, NULL, (BYTE*) &iBkColorValue, &datasize );
		res = RegQueryValueEx( key1, "iSepColorValue", NULL, NULL, (BYTE*) &iSepColorValue, &datasize );
		res = RegQueryValueEx( key1, "iSelTextColorValue", NULL, NULL, (BYTE*) &iSelTextColorValue, &datasize );
		res = RegQueryValueEx( key1, "iSelBkColorValue", NULL, NULL, (BYTE*) &iSelBkColorValue, &datasize );
		res = RegQueryValueEx( key1, "iBmkColor", NULL, NULL, (BYTE*) &iBmkColor, &datasize );

		res = RegQueryValueEx( key1, "iAutomaticBPL", NULL, NULL, (BYTE*) &iAutomaticBPL, &datasize );
		res = RegQueryValueEx( key1, "iBytesPerLine", NULL, NULL, (BYTE*) &iBytesPerLine, &datasize );
		res = RegQueryValueEx( key1, "iOffsetLen", NULL, NULL, (BYTE*) &iMinOffsetLen, &datasize );//Pabs replaced "iOffsetLen" with "iMinOffsetLen"
		res = RegQueryValueEx( key1, "iCharacterSet", NULL, NULL, (BYTE*) &iCharacterSet, &datasize );
		res = RegQueryValueEx( key1, "iFontSize", NULL, NULL, (BYTE*) &iFontSize, &datasize );
		res = RegQueryValueEx( key1, "bOpenReadOnly", NULL, NULL, (BYTE*) &bOpenReadOnly, &datasize );

//Pabs inserted
		res = RegQueryValueEx( key1, "bMakeBackups", NULL, NULL, (BYTE*) &bMakeBackups, &datasize );
		res = RegQueryValueEx( key1, "bAutoOffsetLen", NULL, NULL, (BYTE*) &bAutoOffsetLen, &datasize );
		res = RegQueryValueEx( key1, "enable_drop", NULL, NULL, (BYTE*) &enable_drop, &datasize );
		res = RegQueryValueEx( key1, "enable_drag", NULL, NULL, (BYTE*) &enable_drag, &datasize );
		res = RegQueryValueEx( key1, "enable_scroll_delay_dd", NULL, NULL, (BYTE*) &enable_scroll_delay_dd, &datasize );
		res = RegQueryValueEx( key1, "enable_scroll_delay_sel", NULL, NULL, (BYTE*) &enable_scroll_delay_sel, &datasize );
		res = RegQueryValueEx( key1, "always_pick_move_copy", NULL, NULL, (BYTE*) &always_pick_move_copy, &datasize );
		res = RegQueryValueEx( key1, "prefer_CF_HDROP", NULL, NULL, (BYTE*) &prefer_CF_HDROP, &datasize );
		res = RegQueryValueEx( key1, "prefer_CF_BINARYDATA", NULL, NULL, (BYTE*) &prefer_CF_BINARYDATA, &datasize );
		res = RegQueryValueEx( key1, "prefer_CF_TEXT", NULL, NULL, (BYTE*) &prefer_CF_TEXT, &datasize );
		res = RegQueryValueEx( key1, "output_CF_BINARYDATA", NULL, NULL, (BYTE*) &output_CF_BINARYDATA, &datasize );
		res = RegQueryValueEx( key1, "output_CF_TEXT", NULL, NULL, (BYTE*) &output_CF_TEXT, &datasize );
		res = RegQueryValueEx( key1, "output_text_special", NULL, NULL, (BYTE*) &output_text_special, &datasize );
		res = RegQueryValueEx( key1, "output_text_hexdump_display", NULL, NULL, (BYTE*) &output_text_hexdump_display, &datasize );
		res = RegQueryValueEx( key1, "output_CF_RTF", NULL, NULL, (BYTE*) &output_CF_RTF, &datasize );
//end

		char szPath[ _MAX_PATH + 1 ];
		datasize = _MAX_PATH + 1;
		res = RegQueryValueEx( key1, "TexteditorName", NULL, NULL, (BYTE*) &szPath, &datasize );
		TexteditorName = szPath;

		datasize = _MAX_PATH + 1;
		strcpy(szPath,"FRHEXDES.DLL;FRHEDX.DLL"); // default
		res = RegQueryValueEx( key1, "EncodeDlls", NULL, NULL, (BYTE*) &szPath, &datasize );
		EncodeDlls = szPath;

		datasize = _MAX_PATH + 1;
		res = RegQueryValueEx( key1, "BrowserName", NULL, NULL, (BYTE*) &szPath, &datasize );
		BrowserName = szPath;

		res = RegQueryValueEx( key1, "iWindowShowCmd", NULL, NULL, (BYTE*) &iWindowShowCmd, &datasize );
		res = RegQueryValueEx( key1, "iWindowX", NULL, NULL, (BYTE*) &iWindowX, &datasize );
		res = RegQueryValueEx( key1, "iWindowY", NULL, NULL, (BYTE*) &iWindowY, &datasize );
		res = RegQueryValueEx( key1, "iWindowWidth", NULL, NULL, (BYTE*) &iWindowWidth, &datasize );
		res = RegQueryValueEx( key1, "iWindowHeight", NULL, NULL, (BYTE*) &iWindowHeight, &datasize );

		res = RegQueryValueEx( key1, "iMRU_count", NULL, NULL, (BYTE*) &iMRU_count, &datasize );
		int i;
		char fname[64];
		for( i = 1; i <= MRUMAX; i++ )
		{
			sprintf( fname, "MRU_File%d", i );
			datasize = _MAX_PATH + 1;
			res = RegQueryValueEx( key1, fname, NULL, NULL, (BYTE*) &szPath, &datasize );
			strcpy( &( strMRU[i-1][0] ), szPath );
		}

		// if( res != ERROR_SUCCESS )//Pabs replaced NULL w hwnd
		//	MessageBox( hwnd, "Could not read value", "frhed", MB_OK );

		// Close registry.
		RegCloseKey( key1 );
	}
	else
	{
		// There is no data. Write with default values.
		MessageBox( hwnd, "Frhed is being started for the first time\n"
			"and will be attempting to write to the registry.", "Initialize registry", MB_OK );
		save_ini_data();//end
	}
	return 0;
}

//-------------------------------------------------------------------
int HexEditorWindow::save_ini_data ()
{
//Pabs inserted
	if(!bSaveIni)return 0;//just return if we are not to save ini data
//end

	HKEY key1;

	char keyname[64];
	sprintf( keyname, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\%d", g_iInstCount );

	LONG res = RegCreateKey( HKEY_CURRENT_USER, keyname, &key1 );

	if( res == ERROR_SUCCESS )
	{
		RegSetValueEx( key1, "iTextColorValue", 0, REG_DWORD, (CONST BYTE*) &iTextColorValue, sizeof( int ) );
		RegSetValueEx( key1, "iBkColorValue", 0, REG_DWORD, (CONST BYTE*) &iBkColorValue, sizeof( int ) );
		RegSetValueEx( key1, "iSepColorValue", 0, REG_DWORD, (CONST BYTE*) &iSepColorValue, sizeof( int ) );
		RegSetValueEx( key1, "iSelTextColorValue", 0, REG_DWORD, (CONST BYTE*) &iSelTextColorValue, sizeof( int ) );
		RegSetValueEx( key1, "iSelBkColorValue", 0, REG_DWORD, (CONST BYTE*) &iSelBkColorValue, sizeof( int ) );
		RegSetValueEx( key1, "iBmkColor", 0, REG_DWORD, (CONST BYTE*) &iBmkColor, sizeof( int ) );

		RegSetValueEx( key1, "iAutomaticBPL", 0, REG_DWORD, (CONST BYTE*) &iAutomaticBPL, sizeof( int ) );
		RegSetValueEx( key1, "iBytesPerLine", 0, REG_DWORD, (CONST BYTE*) &iBytesPerLine, sizeof( int ) );
		RegSetValueEx( key1, "iOffsetLen", 0, REG_DWORD, (CONST BYTE*) &iMinOffsetLen, sizeof( int ) );//Pabs replaced "iOffsetLen" with "iMinOffsetLen"
		RegSetValueEx( key1, "iCharacterSet", 0, REG_DWORD, (CONST BYTE*) &iCharacterSet, sizeof( int ) );
		RegSetValueEx( key1, "iFontSize", 0, REG_DWORD, (CONST BYTE*) &iFontSize, sizeof( int ) );
		RegSetValueEx( key1, "bOpenReadOnly", 0, REG_DWORD, (CONST BYTE*) &bOpenReadOnly, sizeof( int ) );

//Pabs inserted
		RegSetValueEx( key1, "bMakeBackups", 0, REG_DWORD, (CONST BYTE*) &bMakeBackups, sizeof( int ) );
		RegSetValueEx( key1, "bAutoOffsetLen", 0, REG_DWORD, (CONST BYTE*) &bAutoOffsetLen, sizeof( int ) );
		RegSetValueEx( key1, "enable_drop", 0, REG_DWORD, (CONST BYTE*) &enable_drop, sizeof( int ) );
		RegSetValueEx( key1, "enable_drag", 0, REG_DWORD, (CONST BYTE*) &enable_drag, sizeof( int ) );
		RegSetValueEx( key1, "enable_scroll_delay_dd", 0, REG_DWORD, (CONST BYTE*) &enable_scroll_delay_dd, sizeof( int ) );
		RegSetValueEx( key1, "enable_scroll_delay_sel", 0, REG_DWORD, (CONST BYTE*) &enable_scroll_delay_sel, sizeof( int ) );
		RegSetValueEx( key1, "always_pick_move_copy", 0, REG_DWORD, (CONST BYTE*) &always_pick_move_copy, sizeof( int ) );
		RegSetValueEx( key1, "prefer_CF_HDROP", 0, REG_DWORD, (CONST BYTE*) &prefer_CF_HDROP, sizeof( int ) );
		RegSetValueEx( key1, "prefer_CF_BINARYDATA", 0, REG_DWORD, (CONST BYTE*) &prefer_CF_BINARYDATA, sizeof( int ) );
		RegSetValueEx( key1, "prefer_CF_TEXT", 0, REG_DWORD, (CONST BYTE*) &prefer_CF_TEXT, sizeof( int ) );
		RegSetValueEx( key1, "output_CF_BINARYDATA", 0, REG_DWORD, (CONST BYTE*) &output_CF_BINARYDATA, sizeof( int ) );
		RegSetValueEx( key1, "output_CF_TEXT", 0, REG_DWORD, (CONST BYTE*) &output_CF_TEXT, sizeof( int ) );
		RegSetValueEx( key1, "output_text_special", 0, REG_DWORD, (CONST BYTE*) &output_text_special, sizeof( int ) );
		RegSetValueEx( key1, "output_text_hexdump_display", 0, REG_DWORD, (CONST BYTE*) &output_text_hexdump_display, sizeof( int ) );
		RegSetValueEx( key1, "output_CF_RTF", 0, REG_DWORD, (CONST BYTE*) &output_CF_RTF, sizeof( int ) );
//end

		RegSetValueEx( key1, "TexteditorName", 0, REG_SZ, (CONST BYTE*) (char*) TexteditorName, TexteditorName.StrLen() + 1 );
		RegSetValueEx( key1, "BrowserName", 0, REG_SZ, (CONST BYTE*) (char*) BrowserName, BrowserName.StrLen() + 1 );

		RegSetValueEx( key1, "iWindowShowCmd", 0, REG_DWORD, (CONST BYTE*) &iWindowShowCmd, sizeof( int ) );
		RegSetValueEx( key1, "iWindowX", 0, REG_DWORD, (CONST BYTE*) &iWindowX, sizeof( int ) );
		RegSetValueEx( key1, "iWindowY", 0, REG_DWORD, (CONST BYTE*) &iWindowY, sizeof( int ) );
		RegSetValueEx( key1, "iWindowWidth", 0, REG_DWORD, (CONST BYTE*) &iWindowWidth, sizeof( int ) );
		RegSetValueEx( key1, "iWindowHeight", 0, REG_DWORD, (CONST BYTE*) &iWindowHeight, sizeof( int ) );

		RegSetValueEx( key1, "iMRU_count", 0, REG_DWORD, (CONST BYTE*) &iMRU_count, sizeof( int ) );
		int i;
		char fname[ 64 ];
		for( i = 1; i <= MRUMAX; i++ )
		{
			sprintf( fname, "MRU_File%d", i );
			RegSetValueEx( key1, fname, 0, REG_SZ, (CONST BYTE*) &(strMRU[i-1][0]), strlen( &(strMRU[i-1][0]) ) + 1 );
		}

		// Close registry.
		RegCloseKey( key1 );
	}
	else
	{//Pabs replaced NULL w hwnd
		MessageBox( hwnd, "Could not save preferences to registry.", "Frhed", MB_OK | MB_ICONERROR );
	}//end
	return 0;
}


//-------------------------------------------------------------------
BOOL CALLBACK AppendDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[32];
			sprintf (buf, "%d", iAppendbytes);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[64];
				int i=-1;
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 64) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{//Pabs replaced NULL w hDlg
						MessageBox (hDlg, "Number of bytes to append not recognized.", "Append", MB_OK | MB_ICONERROR);
						i = -1;
					}//end
				}
				if (i==-1)
				{
					EndDialog (hDlg, 0);
					return 0;
				}
				iAppendbytes = i;
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_edit_append ()
{
	iAppendbytes = 1;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_APPENDDIALOG), hwnd, (DLGPROC) AppendDlgProc))
	{
		int i, oldupbound = DataArray.GetLength();
		SetCursor (LoadCursor (NULL, IDC_WAIT));
		if (DataArray.SetSize (DataArray.GetSize()+iAppendbytes) == FALSE)
		{//Pabs replaced NULL w hwnd
			MessageBox (hwnd, "Not enough memory for appending.", "Append", MB_OK | MB_ICONERROR);
			return FALSE;
		}//end
		DataArray.SetUpperBound(DataArray.GetUpperBound()+iAppendbytes);
		for (i=0; i<iAppendbytes; i++)
			DataArray[oldupbound+i] = 0;
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		update_for_new_datasize ();
		repaint ();
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK BitManipDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[64];
			sprintf( buf, "Manipulate bits at offset 0x%x=%d", iManipPos, iManipPos );
			SetWindowText( GetDlgItem( hDlg, IDC_STATIC1 ), buf );

			sprintf( buf, "Value: 0x%x , %d signed, %u unsigned.", (unsigned char) cBitValue, (signed char) cBitValue, (unsigned char) cBitValue );
			SetWindowText( GetDlgItem( hDlg, IDC_STATIC2 ), buf );

			SetFocus (GetDlgItem (hDlg, IDC_CHECK8));
			if (bitval (&cBitValue,0) != 0)
				CheckDlgButton (hDlg, IDC_CHECK1, BST_CHECKED);
			if (bitval (&cBitValue,1) != 0)
				CheckDlgButton (hDlg, IDC_CHECK2, BST_CHECKED);
			if (bitval (&cBitValue,2) != 0)
				CheckDlgButton (hDlg, IDC_CHECK3, BST_CHECKED);
			if (bitval (&cBitValue,3) != 0)
				CheckDlgButton (hDlg, IDC_CHECK4, BST_CHECKED);
			if (bitval (&cBitValue,4) != 0)
				CheckDlgButton (hDlg, IDC_CHECK5, BST_CHECKED);
			if (bitval (&cBitValue,5) != 0)
				CheckDlgButton (hDlg, IDC_CHECK6, BST_CHECKED);
			if (bitval (&cBitValue,6) != 0)
				CheckDlgButton (hDlg, IDC_CHECK7, BST_CHECKED);
			if (bitval (&cBitValue,7) != 0)
				CheckDlgButton (hDlg, IDC_CHECK8, BST_CHECKED);
			return FALSE;
		}

	case WM_COMMAND:
		{
			cBitValue = 0;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK8) == BST_CHECKED)
				cBitValue += 128;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK7) == BST_CHECKED)
				cBitValue += 64;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK6) == BST_CHECKED)
				cBitValue += 32;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK5) == BST_CHECKED)
				cBitValue += 16;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK4) == BST_CHECKED)
				cBitValue += 8;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK3) == BST_CHECKED)
				cBitValue += 4;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK2) == BST_CHECKED)
				cBitValue += 2;
			if (IsDlgButtonChecked (hDlg, IDC_CHECK1) == BST_CHECKED)
				cBitValue += 1;
			switch (LOWORD (wParam))
			{
			case IDC_CHECK1: case IDC_CHECK2: case IDC_CHECK3: case IDC_CHECK4:
			case IDC_CHECK5: case IDC_CHECK6: case IDC_CHECK7: case IDC_CHECK8:
				{
					char buf[64];
					sprintf( buf, "Value: 0x%x , %d signed, %u unsigned.", (unsigned char) cBitValue, (signed char) cBitValue, (unsigned char) cBitValue );
					SetWindowText( GetDlgItem( hDlg, IDC_STATIC2 ), buf );
				}
				break;

			case IDOK:
				{
					EndDialog (hDlg, 1);
					return TRUE;
				}

			case IDCANCEL:
				EndDialog (hDlg, 0);
				return TRUE;
			}
			break;
		}
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_manipulate_bits ()
{
	if (DataArray.GetLength() == 0)
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "File is empty.", "Manipulate Bits", MB_OK | MB_ICONERROR);
		return 0;
	}
	if (iCurByte<0 || iCurByte>LASTBYTE)
	{
		MessageBox (hwnd, "Must choose byte in the file.", "Manipulate Bits", MB_OK | MB_ICONERROR);
		return 0;
	}//end
	cBitValue = DataArray[iCurByte];
	iManipPos = iCurByte;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_MANIPBITSDIALOG), hwnd, (DLGPROC) BitManipDlgProc))
	{
		SetCursor (LoadCursor (NULL, IDC_WAIT));
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		DataArray[iCurByte] = cBitValue;
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		repaint ();
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_character_set ()
{
	iCharacterSetting = iCharacterSet;
	iFontSizeSetting = iFontSize;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_CHARACTERSETDIALOG), hwnd, (DLGPROC) CharacterSetDlgProc))
	{
		iCharacterSet = iCharacterSetting;
		iFontSize = iFontSizeSetting;
		save_ini_data ();
		update_for_new_datasize ();
		kill_focus ();
		set_focus ();
	}
	return 0;
}

//-------------------------------------------------------------------
BOOL CALLBACK CharacterSetDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[16];
			sprintf (buf, "%d", iFontSizeSetting);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			switch (iCharacterSetting)
			{
			case ANSI_FIXED_FONT:
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
				break;

			case OEM_FIXED_FONT:
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
				break;
			}
			SetFocus (GetDlgItem (hDlg, IDC_RADIO1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				int i;
				char buf[16];
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 16) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{//Pabs replaced NULL w hDlg
						MessageBox (hDlg, "Font size not recognized.", "Character set", MB_OK | MB_ICONERROR);
						i = -1;
					}//end
				}
				if (i != -1)
					iFontSizeSetting = i;
				if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
					iCharacterSetting = ANSI_FIXED_FONT;
				else
					iCharacterSetting = OEM_FIXED_FONT;
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_on_deletekey ()
{
	iCutMode = BST_UNCHECKED;
	return CMD_edit_cut ();
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_toggle_insertmode ()
{
	iInsertMode = (iInsertMode) ? FALSE : TRUE;
	set_wnd_title();
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_on_backspace ()
{
	if (iInsertMode)
	{
		// INSERT-mode: If one exists delete previous byte.
		if (iCurByte > 0)
		{
			if (DataArray.RemoveAt (iCurByte-1, 1) == TRUE)
			{
				iCurByte--;
				update_for_new_datasize ();
			}
			else//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Could not remove byte.", "Backspace key", MB_OK | MB_ICONERROR);
		}//end
	}
	else
	{
		// Only step back.
		if (iCurByte>0)
			iCurByte--;
		repaint ();
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_select_all ()
{
	if (DataArray.GetLength() <= 0)
		return 0;
	bSelected = TRUE;
	iStartOfSelection = 0;
	iEndOfSelection = DataArray.GetUpperBound ();
	adjust_view_for_selection ();
	repaint ();
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::make_font ()
{
	if (hFont != NULL)
		DeleteObject (hFont);
	HDC hdc = GetDC (hwnd);
	int nHeight = -MulDiv(iFontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseDC (hwnd, hdc);
	int cset;
	if (iCharacterSet==ANSI_FIXED_FONT)
		cset = ANSI_CHARSET;
	else
		cset = OEM_CHARSET;
	hFont = CreateFont (nHeight,0,0,0,0,0,0,0,cset,OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH | FF_DONTCARE,0);
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_properties ()
{
	char buf[1000], buf2[500], *pc;
	sprintf (buf, "File name and path: ");
	GetFullPathName (filename, 500, buf2, &pc);
	strcat (buf, buf2);
	if( bPartialOpen )
	{
		sprintf (buf2, "\nPartially opened at offset 0x%x = %d.\n"
			"Number of bytes read: %d = %d kilobytes.\n",
			iPartialOffset, iPartialOffset, DataArray.GetLength(), DataArray.GetLength()/1024);
		strcat (buf, buf2);
	}
	else
	{
		sprintf (buf2, "\nFile size: %d bytes = %d kilobytes.\n", DataArray.GetLength(), DataArray.GetLength()/1024);
		strcat (buf, buf2);
	}
	sprintf (buf2, "\nNumber of hexdump lines: %d.\n", iNumlines);
	strcat (buf, buf2);//Pabs replaced NULL w hwnd
	MessageCopyBox (hwnd, buf, "File properties", MB_ICONINFORMATION, hwnd);
	return 1;//end
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_compare ()
{
	if (DataArray.GetLength() <= 0)
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "Current file is empty.", "Compare", MB_OK | MB_ICONERROR);
		return 0;
	}//end
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = "Choose file to compare with";
	ofn.Flags = OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetOpenFileName (&ofn))
	{
		int filehandle;
		if ((filehandle = _open (szFileName,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			int filelen = _filelength (filehandle);
			iDestFileLen = filelen;
			iSrcFileLen = DataArray.GetLength()-iCurByte;
			char* cmpdata = new char[filelen];
			// Read data.
			if (_read (filehandle, cmpdata, filelen) != -1)
			{
				int diff;
				if ((diff = compare_arrays ((char*) &(DataArray[iCurByte]), DataArray.GetLength()-iCurByte, cmpdata, filelen)) == 0)
				{//Pabs replaced NULL w hwnd
					// No difference.
					MessageBox (hwnd, "Data matches exactly.", "Compare", MB_OK | MB_ICONINFORMATION);
				}//end
				else
				{
					// Differences exist.
					intpair* pdiff = new intpair[diff];
					get_diffs ((char*) &(DataArray[iCurByte]), DataArray.GetLength()-iCurByte, cmpdata, filelen, pdiff);
					pdiffChoice = pdiff;
					iDiffNum = diff;
					if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_CHOOSEDIFFDIALOG), hwnd, (DLGPROC) ChooseDiffDlgProc))
					{
						iStartOfSelection = iCurByte+pdiff[iDiffNum].one;
						iEndOfSelection = iCurByte+pdiff[iDiffNum].two;
						bSelected = TRUE;
						iCurByte = iCurByte+pdiff[iDiffNum].one;
						adjust_view_for_selection ();
						repaint ();
					}
					delete [] pdiff;
				}
				_close (filehandle);
				delete [] cmpdata;
				return TRUE;
			}
			else
			{
				delete [] cmpdata;
				_close (filehandle);//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Error while reading from file.", "Compare", MB_OK | MB_ICONERROR);
				return FALSE;//end
			}
		}
	}
	return 1;
}

//-------------------------------------------------------------------
// Return = number of differing bytes.
int HexEditorWindow::compare_arrays (char* ps, int sl, char* pd, int dl)
{
	int i, diff=0, type=1;
	// type=0 means differences, type=1 means equality at last char.
	for (i=0; i<sl && i<dl; i++)
	{
		switch (type)
		{
		case 0:
			if (ps[i]==pd[i])
			{
				diff++;
				type = 1;
			}
			break;

		case 1:
			if (ps[i]!=pd[i])
			{
				type = 0;
			}
			break;
		}
	}
	if (type == 0)
		diff++;
	return diff;
}

//-------------------------------------------------------------------
// Transfer offsets of differences to pdiff.
int HexEditorWindow::get_diffs (char* ps, int sl, char* pd, int dl, intpair* pdiff)
{
	int i, diff=0, type=1;
	// type=0 means differences, type=1 means equality at last char.
	for (i=0; i<sl && i<dl; i++)
	{
		switch (type)
		{
		case 0:
			// Working on area of difference at the moment.
			if (ps[i]==pd[i])
			{
				// Chars equal again.
				pdiff[diff].two = i-1; // Save end of area of difference.
				diff++;
				type = 1;
			}
			// else: chars still different.
			break;

		case 1:
			// Working on area of equality at the moment.
			if (ps[i]!=pd[i])
			{
				// Start of area of difference found.
				pdiff[diff].one = i; // Save start of area of difference.
				type = 0;
			}
			// else: chars still equal.
			break;
		}
	}
	if (type == 0) // If area of difference was at end of file.
	{
		pdiff[diff].two = i-1;
		diff++;
	}
	return diff;
}

//-------------------------------------------------------------------
BOOL CALLBACK ChooseDiffDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[100];
			int i;
			sprintf (buf, "%d areas of difference found.", iDiffNum);
			SetWindowText (GetDlgItem (hDlg, IDC_STATIC1), buf);
			sprintf (buf, "Remaining loaded data size: %d, size of file on disk: %d.", iSrcFileLen, iDestFileLen);
			SetWindowText (GetDlgItem (hDlg, IDC_STATIC2), buf);
			HWND hwndList = GetDlgItem (hDlg, IDC_LIST1);
			for (i=0; i<iDiffNum; i++)
			{
				sprintf (buf, "%d) 0x%x=%d to 0x%x=%d (%d bytes)", i+1, pdiffChoice[i].one, pdiffChoice[i].one,
					pdiffChoice[i].two, pdiffChoice[i].two, pdiffChoice[i].two-pdiffChoice[i].one+1);
				SendMessage (hwndList, LB_ADDSTRING, 0, (LPARAM) buf);
			}
			SendMessage (hwndList, LB_SETCURSEL, 0, 0);
			SetFocus (GetDlgItem (hDlg, IDC_LIST1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
			// By pabs.
			case IDCOPY:
				{//copy button was pressed
					int sumlen=1;//length of buffer initially is 1 for the '\0'
					int len=0;//length of current string
					char*buf=(char*)malloc(1);//buffer = '\0'
					buf[0]=0;//init buffer with '\0'
					char*bt=NULL;//temporary pointer - used so that if realloc returns NULL buf does not lose its value
					HWND hwndList = GetDlgItem (hDlg, IDC_LIST1);//get the list
					int num = SendMessage(hwndList,LB_GETCOUNT,0,0);//get the # items in the list
					for(int i=0;i<num;i++)
					{	//loop num times
						len=SendMessage(hwndList,LB_GETTEXTLEN,i,0)+2;//get sise of next line +2 is for '\r\n' at the end of each line
						sumlen+=len;//add len to the buffer sise
						bt = (char*)realloc(buf,sumlen);//resise buffer
						if(bt!=NULL)
							buf=bt;//realloc succesful overwrite buffer address
						else// if realloc returns NULL(not enough mem to re-alloc buffer)
						break;//exit loop without changing buffer address
						// the -1 is to counteract the initialisation of sumlen
						SendMessage(hwndList,LB_GETTEXT,i,(LPARAM)&buf[sumlen-len-1]);//get the string & add it to the end of the buffer
						strcat(buf,"\r\n");//add '\r\n' to the end of the line - this is '\r\n' rather than '\n' so that it can be pasted into notepad & dos programs
					}//end of the loop
					TextToClipboard(buf,hwndHex);//copy the stuff to the clip ( this function needs work to clean it up )(len=1+strlen)
					free(buf);//free the buffer mem
					return TRUE;//yes we did process the message
				}
				break;


		case IDOK:
			{
				HWND hwndList = GetDlgItem (hDlg, IDC_LIST1);
				iDiffNum = SendMessage (hwndList, LB_GETCURSEL, 0, 0);
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
char HexEditorWindow::TranslateAnsiToOem (char c)
{
	char sbuf[2], dbuf[2];
	sbuf[0]=c;
	sbuf[1]=0;
	CharToOemBuff (sbuf, dbuf, 1);
	return dbuf[0];
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_binarymode ()
{
	iBinaryModeSetting = iBinaryMode;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_BINARYMODEDIALOG), hwnd, (DLGPROC) BinaryModeDlgProc))
	{
		iBinaryMode = iBinaryModeSetting;
		repaint ();
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK BinaryModeDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			if (iBinaryModeSetting == LITTLEENDIAN_MODE)
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			else
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
			SetFocus (GetDlgItem (hDlg, IDC_RADIO1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
					iBinaryModeSetting = LITTLEENDIAN_MODE;
				else
					iBinaryModeSetting = BIGENDIAN_MODE;
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
//Pabs added stuff for OLE drag-drop
int HexEditorWindow::timer (WPARAM w, LPARAM l)
{
	UNREFERENCED_PARAMETER( l );

	switch(w){
		case MOUSE_OP_DELAY_TIMER_ID:
			start_mouse_operation();
		break;
		case SCROLL_DELAY_TIMER_ID:
			KillTimer( hwnd, SCROLL_DELAY_TIMER_ID );
			bScrollDelayTimerSet = FALSE;
			SetTimer( hwnd, SCROLL_TIMER_ID, ScrollInterval, NULL );
			bScrollTimerSet = TRUE;
		break;
		case SCROLL_TIMER_ID:
		{
	if (!bScrollTimerSet)
		return 0;

//Pabs changed for better scrolling
	int adjusth = 0, adjustv = 0;
	if (iMouseY >= cyBuffer*cyChar)
	{
		// Lower border reached.
		if (iCurLine < LASTLINE-cyBuffer+1){
			iCurLine++; adjustv = 1;
		}
	}
	else if (iMouseY < cyChar)
	{
		// Upper border reached.
		if (iCurLine > 0){
			iCurLine--; adjustv = 1;
		}
	}

	if (iMouseX >= cxBuffer*cxChar)
	{
		// Right border reached.
		if (iHscrollPos < iCharsPerLine-cxBuffer){
			iHscrollPos++; adjusth = 1;
		}
	}
	else if (iMouseX < cxChar)
	{
		// Left border reached.
		if (iHscrollPos > 0){
			iHscrollPos--; adjusth = 1;
		}
	}

	if(adjusth)
		adjust_hscrollbar ();
	if(adjustv)
		adjust_vscrollbar ();
	if(adjusth||adjustv)
		repaint ();
//end
		}
		break;
	}

	return 1;
}

//Pabs inserted
void HexEditorWindow::start_mouse_operation()
{
	KillTimer( hwnd, MOUSE_OP_DELAY_TIMER_ID );
	bMouseOpDelayTimerSet = FALSE;

	iStartOfSelSetting = iStartOfSelection;
	iEndOfSelSetting = iEndOfSelection;
	if( iStartOfSelSetting > iEndOfSelSetting ) swap(iStartOfSelSetting,iEndOfSelSetting);//make sure start<=end

	//In the selection
	if( enable_drag && bSelected && ( lbd_pos >= iStartOfSelSetting && lbd_pos <= iEndOfSelSetting ) )
	{
		DWORD dwEffect;
		CDataObject dataobj; IDataObject* lpDataObj;
		FORMATETC fe; STGMEDIUM sm;
		bool madedata = false;
		//Stick the bit[s] of data in the data object
		if( output_CF_BINARYDATA ){
			sm.hGlobal =  GlobalAlloc(GHND|GMEM_DDESHARE, sizeof(UINT) + iEndOfSelSetting - iStartOfSelSetting+1);
			if(sm.hGlobal){
				BYTE* p = (BYTE*)GlobalLock(sm.hGlobal);
				if(p){
					*(UINT*)p = iEndOfSelSetting - iStartOfSelSetting+1;
					memcpy((BYTE*)p+sizeof(UINT),&DataArray[iStartOfSelSetting],iEndOfSelSetting - iStartOfSelSetting+1);
					madedata = true;
				}
			}
			if(madedata){
				sm.tymed = TYMED_HGLOBAL;
				sm.pUnkForRelease = NULL;
				fe.cfFormat = CF_BINARYDATA;
				fe.dwAspect = DVASPECT_CONTENT;
				fe.lindex = -1;
				fe.ptd = NULL;
				fe.tymed = TYMED_HGLOBAL;
				dataobj.SetData( &fe, &sm, TRUE );
				madedata = false;
			}
		}
		if( output_CF_TEXT ){
			int destlen;
			if( output_text_special ){
				//The special syntax
				destlen = byte_to_BC_destlen ((char*) &(DataArray[iStartOfSelSetting]), iEndOfSelSetting-iStartOfSelSetting+1);
				sm.hGlobal = GlobalAlloc (GHND|GMEM_DDESHARE, destlen);
				if(sm.hGlobal)
				{
					char* p = (char*)GlobalLock(sm.hGlobal);
					if(p){
						translate_bytes_to_BC (p, &(DataArray[iStartOfSelSetting]), iEndOfSelSetting-iStartOfSelSetting+1);
						madedata = true;
					}
					GlobalUnlock(sm.hGlobal);
				}
			} else {
				//One of the two hexdump types
				int temp[3] = {iCopyHexdumpType,iCopyHexdumpDlgStart,iCopyHexdumpDlgEnd};
				if( /*Output like display*/ output_text_hexdump_display ){
					iCopyHexdumpType = IDC_EXPORTDISPLAY;
					iCopyHexdumpDlgStart = iStartOfSelSetting / iBytesPerLine * iBytesPerLine;//cut back to the line start
					iCopyHexdumpDlgEnd = iEndOfSelSetting / iBytesPerLine * iBytesPerLine;//cut back to the line start
					destlen = ((iCopyHexdumpDlgEnd - iCopyHexdumpDlgStart) / iBytesPerLine + 1) * (iCharsPerLine+2) + 1;
				} else /*Just output hex digits*/ {
					iCopyHexdumpType = IDC_EXPORTDIGITS;
					destlen = (iEndOfSelSetting - iStartOfSelSetting) * 2 + 3;
				}
				sm.hGlobal = GlobalAlloc (GHND|GMEM_DDESHARE, destlen);
				if(sm.hGlobal)
				{
					char* p = (char*)GlobalLock(sm.hGlobal);
					if(p){
						if(CMD_copy_hexdump (p, destlen))
							madedata = true;
						GlobalUnlock(sm.hGlobal);
					}
				}
				iCopyHexdumpType = temp[0];
				iCopyHexdumpDlgStart = temp[1];
				iCopyHexdumpDlgEnd = temp[2];
			}
			if(madedata){
				sm.tymed = TYMED_HGLOBAL;
				sm.pUnkForRelease = NULL;
				fe.cfFormat = CF_TEXT;
				fe.dwAspect = DVASPECT_CONTENT;
				fe.lindex = -1;
				fe.ptd = NULL;
				fe.tymed = TYMED_HGLOBAL;
				dataobj.SetData( &fe, &sm, TRUE );
				madedata = false;
			}
		}
		if( output_CF_RTF ){
				if(sm.hGlobal = RTF_hexdump(iStartOfSelSetting, iEndOfSelSetting)){
					sm.tymed = TYMED_HGLOBAL;
					sm.pUnkForRelease = NULL;
					fe.cfFormat = CF_RICH_TEXT_FORMAT;
					fe.dwAspect = DVASPECT_CONTENT;
					fe.lindex = -1;
					fe.ptd = NULL;
					fe.tymed = TYMED_HGLOBAL;
					dataobj.SetData( &fe, &sm, TRUE );
					madedata = false;
				}
		}
		dataobj.DisableSetData();
		CDropSource source; IDropSource* lpDropSrc;
		if( S_OK != dataobj.QueryInterface(IID_IDataObject, (void**)&lpDataObj) || !lpDataObj) goto SELECTING;
		if( S_OK != source.QueryInterface(IID_IDropSource, (void**)&lpDropSrc) || !lpDropSrc) goto SELECTING;
		old_col = old_row = -1;
		bMoving = TRUE;
		bDroppedHere = FALSE;
		HRESULT r = DoDragDrop( lpDataObj, lpDropSrc, bReadOnly ? DROPEFFECT_COPY : DROPEFFECT_COPY|DROPEFFECT_MOVE, &dwEffect);
		bMoving = FALSE;
		if( r == DRAGDROP_S_DROP && dwEffect & DROPEFFECT_MOVE && !bDroppedHere ) {
			DataArray.RemoveAt( iStartOfSelSetting, iEndOfSelSetting - iStartOfSelSetting + 1 );
			bSelected = FALSE; m_iFileChanged = bFilestatusChanged = TRUE;
			iCurByte = iStartOfSelSetting;
			repaint(iStartOfSelSetting,iEndOfSelSetting);
		}
		lpDataObj->Release();
		lpDropSrc->Release();
	} else {
SELECTING:
		int lastbyte = LASTBYTE;
		if( lastbyte == -1 ) return;
		SetCursor( LoadCursor( NULL, IDC_IBEAM ) );
		if( old_pos >= lastbyte+1 )
			old_pos = lastbyte;
		else if( old_pos < 0 )
			old_pos = 0;
		if( lbd_pos >= lastbyte+1 )
			lbd_pos = lastbyte;
		else if( lbd_pos < 0 )
			lbd_pos = 0;
		iStartOfSelection = lbd_pos;
		iEndOfSelection = old_pos;
		bSelected = bSelecting = TRUE;
		repaint();
	}
	/*else
	{
		bPullScrolling = TRUE;
	}*/
}
//end
//TODO: update such that when the IDataObject changes the list box is re-created
BOOL CALLBACK DragDropDlgProc (HWND h, UINT m, WPARAM w, LPARAM l){
	static DROPPARAMS* p = NULL;
	switch (m){
		case WM_INITDIALOG:{
			p = (DROPPARAMS*)l;
			CheckDlgButton( h, p->effect ? IDC_COPY : IDC_MOVE, TRUE );
			if( !(p->allowable_effects & DROPEFFECT_MOVE) )
				EnableWindow( GetDlgItem( h, IDC_MOVE ), FALSE );
			if( !(p->allowable_effects & DROPEFFECT_COPY) )
				EnableWindow( GetDlgItem( h, IDC_COPY ), FALSE );
			HWND list = GetDlgItem(h, IDC_LIST);
			if( p->numformatetcs && p->formatetcs ){
				//ListView_DeleteAllItems(list);
				{
					LVCOLUMN col;
					Zero(col);
					ListView_InsertColumn(list,0,&col);
				}
				char szFormatName[100], SetSel = 0;
				LVITEM lvi;
				Zero(lvi);
				lvi.mask = LVIF_TEXT | LVIF_PARAM;

				UINT i;
				for( i = 0; i < p->numformatetcs; i++ ){
					CLIPFORMAT temp = p->formatetcs[i].cfFormat;
					lvi.lParam = lvi.iItem = i;
					lvi.pszText = NULL;

					// For registered formats, get the registered name.
					if (GetClipboardFormatName(temp, szFormatName, sizeof(szFormatName)))
						lvi.pszText = szFormatName;
					else{
						//Get the name of the standard clipboard format.
						switch(temp){
							#define CASE(a,b) case a: lvi.pszText = #a; SetSel = b; break;
								CASE(CF_TEXT,1)
							#undef CASE
							#define CASE(a) case a: lvi.pszText = #a; break;
								CASE(CF_BITMAP) CASE(CF_METAFILEPICT) CASE(CF_SYLK)
								CASE(CF_DIF) CASE(CF_TIFF) CASE(CF_OEMTEXT)
								CASE(CF_DIB) CASE(CF_PALETTE) CASE(CF_PENDATA)
								CASE(CF_RIFF) CASE(CF_WAVE) CASE(CF_UNICODETEXT)
								CASE(CF_ENHMETAFILE) CASE(CF_HDROP) CASE(CF_LOCALE)
								CASE(CF_MAX) CASE(CF_OWNERDISPLAY) CASE(CF_DSPTEXT)
								CASE(CF_DSPBITMAP) CASE(CF_DSPMETAFILEPICT)
								CASE(CF_DSPENHMETAFILE) CASE(CF_PRIVATEFIRST)
								CASE(CF_PRIVATELAST) CASE(CF_GDIOBJFIRST)
								CASE(CF_GDIOBJLAST) CASE(CF_DIBV5)
							#undef CASE
							default:
								if(i!=i);
								#define CASE(a) else if(temp>a##FIRST&&temp<a##LAST) sprintf(szFormatName,#a "%d",temp-a##FIRST);
									CASE(CF_PRIVATE) CASE(CF_GDIOBJ)
								#undef CASE
								//Format ideas for future: hex number, system/msdn constant, registered format, WM_ASKFORMATNAME, tickbox for delay rendered or not*/
								/*else if(temp>0xC000&&temp<0xFFFF)
									sprintf(szFormatName,"CF_REGISTERED%d",temp-0xC000);*/
								else break;
								lvi.pszText = szFormatName;
							break;
						}
					}

					if (lvi.pszText == NULL){
						sprintf(szFormatName,"0x%.8x",temp);
						lvi.pszText = szFormatName;
					}

					//Insert into the list
					if(lvi.pszText){
						ListView_InsertItem(list,&lvi);
						if(SetSel == 1){SetSel = 2; ListView_SetItemState(list, i, LVIS_SELECTED, LVIS_SELECTED);}
					}
				}
				ListView_SetColumnWidth(list, 0, LVSCW_AUTOSIZE_USEHEADER);
				if(!SetSel) ListView_SetItemState(list, i, LVIS_SELECTED, LVIS_SELECTED);
			}
			SetFocus( list );
		}
		return FALSE;
		case WM_COMMAND:
			switch (LOWORD (w)){
				case IDOK:{
					p->effect = IsDlgButtonChecked( h, IDC_MOVE ) ? false : true;
					HWND list = GetDlgItem(h, IDC_LIST);
					p->numformats = ListView_GetSelectedCount(list);
					if( p->numformats ){
						for(;;){
							p->formats = (UINT*)malloc( p->numformats*sizeof(*p->formats) );
							if(p->formats) break;
							p->numformats--; //p->numformats/=2; //Or this
						}
						if( p->formats ){
							LVITEM temp;
							Zero(temp);
							temp.mask = LVIF_PARAM;
							temp.iItem = -1;
							for( UINT i = 0; i < p->numformats; i++ ){
								temp.iItem = ListView_GetNextItem(list, temp.iItem, LVNI_SELECTED);
								ListView_GetItem(list,&temp);
								p->formats[i] = temp.lParam;
							}
						}
					}
					EndDialog (h, 1);
				} return TRUE;
				case IDCANCEL:
					EndDialog (h, -1);
				return TRUE;
				case IDC_UP:
				case IDC_DOWN:{
					HWND list = GetDlgItem(h, IDC_LIST);
					LVITEM item[2];
					Zero(item[0]);
					Zero(item[1]);
					//If anyone knows a better way to swap two items please send a patch
					item[0].iItem = ListView_GetNextItem(list, (UINT)-1, LVNI_SELECTED);
					if(item[0].iItem==-1) item[0].iItem = ListView_GetNextItem(list, (UINT)-1, LVNI_FOCUSED);
					if(item[0].iItem==-1){ MessageBox( h, "Select an item to move.", "Drag-drop", MB_OK ); goto END; }
					item[0].mask = LVIF_TEXT|LVIF_PARAM|LVIF_STATE;
					item[0].stateMask = (UINT)-1;
					char text[2][100];
					item[0].pszText = text[0];
					item[0].cchTextMax = sizeof(text[0]);
					item[1] = item[0];
					item[1].pszText = text[1];
					if(LOWORD(w)==IDC_UP){
						if(item[1].iItem==0) item[1].iItem=p->numformatetcs-1;
						else item[1].iItem--;
					} else {
						if( (UINT)item[1].iItem==p->numformatetcs-1 ) item[1].iItem = 0;
						else item[1].iItem++;
					}
					ListView_GetItem(list, &item[0]);
					ListView_GetItem(list, &item[1]);
					swap(item[0].iItem,item[1].iItem);
					item[0].state |= LVIS_FOCUSED|LVIS_SELECTED;
					item[1].state &= ~(LVIS_FOCUSED|LVIS_SELECTED);
					ListView_SetItem(list, &item[0]);
					ListView_SetItem(list, &item[1]);
					END:
						SetFocus(list);
				} return TRUE;
			}
		return FALSE;
	}
	return FALSE;
}

BOOL CALLBACK DragDropOptionsDlgProc (HWND h, UINT m, WPARAM w, LPARAM l){
	switch (m){
		case WM_INITDIALOG:{
			if(hexwnd.enable_drag) CheckDlgButton( h, IDC_ENABLE_DRAG, TRUE );
			if(hexwnd.enable_drop) CheckDlgButton( h, IDC_ENABLE_DROP, TRUE );
			if(hexwnd.enable_scroll_delay_dd) CheckDlgButton( h, IDC_EN_SD_DD, TRUE );
			if(hexwnd.enable_scroll_delay_sel) CheckDlgButton( h, IDC_EN_SD_SEL, TRUE );
			if(hexwnd.always_pick_move_copy) CheckDlgButton( h, IDC_ALWAYS_CHOOSE, TRUE );
			if(hexwnd.prefer_CF_HDROP) CheckDlgButton( h, IDC_DROP_CF_HDROP, TRUE );
			if(hexwnd.prefer_CF_BINARYDATA) CheckDlgButton( h, IDC_DROP_BIN_DATA, TRUE );
			if(hexwnd.prefer_CF_TEXT) CheckDlgButton( h, IDC_DROP_CF_TEXT, TRUE );
			if(hexwnd.output_CF_BINARYDATA) CheckDlgButton( h, IDC_DRAG_BIN_DATA, TRUE );
			if(hexwnd.output_CF_TEXT) CheckDlgButton( h, IDC_DRAG_CF_TEXT, TRUE );
			CheckDlgButton( h, hexwnd.output_text_special?IDC_TEXT_SPECIAL:IDC_TEXT_HEXDUMP, TRUE );
			if(hexwnd.output_text_hexdump_display) CheckDlgButton( h, IDC_TEXT_DISPLAY, TRUE );
			if(hexwnd.output_CF_RTF) CheckDlgButton( h, IDC_DRAG_RTF, TRUE );
		}
		return FALSE;
		case WM_COMMAND:
			switch (LOWORD (w)){
				case IDOK:{
					hexwnd.enable_drag = BST_CHECKED == IsDlgButtonChecked( h, IDC_ENABLE_DRAG );
					hexwnd.enable_drop = BST_CHECKED == IsDlgButtonChecked( h, IDC_ENABLE_DROP );
					hexwnd.enable_scroll_delay_dd = BST_CHECKED == IsDlgButtonChecked( h, IDC_EN_SD_DD );
					hexwnd.enable_scroll_delay_sel = BST_CHECKED == IsDlgButtonChecked( h, IDC_EN_SD_SEL );
					hexwnd.always_pick_move_copy = BST_CHECKED == IsDlgButtonChecked( h, IDC_ALWAYS_CHOOSE );
					hexwnd.prefer_CF_HDROP = BST_CHECKED == IsDlgButtonChecked( h, IDC_DROP_CF_HDROP );
					hexwnd.prefer_CF_BINARYDATA = BST_CHECKED == IsDlgButtonChecked( h, IDC_DROP_BIN_DATA );
					hexwnd.prefer_CF_TEXT = BST_CHECKED == IsDlgButtonChecked( h, IDC_DROP_CF_TEXT );
					hexwnd.output_CF_BINARYDATA = BST_CHECKED == IsDlgButtonChecked( h, IDC_DRAG_BIN_DATA );
					hexwnd.output_CF_TEXT = BST_CHECKED == IsDlgButtonChecked( h, IDC_DRAG_CF_TEXT );
					hexwnd.output_text_special = BST_CHECKED == IsDlgButtonChecked( h, IDC_TEXT_SPECIAL );
					hexwnd.output_text_hexdump_display = BST_CHECKED == IsDlgButtonChecked( h, IDC_TEXT_DISPLAY );
					hexwnd.output_CF_RTF = BST_CHECKED == IsDlgButtonChecked( h, IDC_DRAG_RTF );

					if( !hexwnd.target || !hexwnd.enable_drop || ( hexwnd.enable_drop && hexwnd.prefer_CF_HDROP ) )
						DragAcceptFiles( hwndHex, TRUE );
					else DragAcceptFiles( hwndHex, FALSE );

					if( hexwnd.target && hexwnd.enable_drop )
						RegisterDragDrop( hwndHex, hexwnd.target );
					else RevokeDragDrop( hwndHex );

					hexwnd.save_ini_data();

					EndDialog (h, 1);
				} return TRUE;
				case IDCANCEL:
					EndDialog (h, -1);
				return TRUE;
			}
		return FALSE;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_select_block ()
{
//Pabs inserted
	if(bSelected){
		iStartOfSelSetting = iStartOfSelection;
		iEndOfSelSetting = iEndOfSelection;
	}
	else
//end - next line indented
		iEndOfSelSetting = iStartOfSelSetting = iCurByte;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_SELECT_BLOCK_DIALOG), hwnd, (DLGPROC) SelectBlockDlgProc))
	{
//Pabs changed to make the selection valid if it is not
		iStartOfSelection = LASTBYTE;
		if (iStartOfSelSetting<0)iStartOfSelSetting=0;
		if (iStartOfSelSetting>iStartOfSelection)iStartOfSelSetting=iStartOfSelection;
		if (iEndOfSelSetting<0)iEndOfSelSetting=0;
		if (iEndOfSelSetting>iStartOfSelection)iEndOfSelSetting=iStartOfSelection;
//end
		iStartOfSelection = iStartOfSelSetting;
		iEndOfSelection = iEndOfSelSetting;
		bSelected = TRUE;
		adjust_view_for_selection ();
		repaint ();
	}
	return 0;
}

//-------------------------------------------------------------------
BOOL CALLBACK SelectBlockDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[128];
			sprintf (buf, "x%x", iStartOfSelSetting);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "x%x", iEndOfSelSetting);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[128];
				int i=0;
				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 128) != 0)
				{
					if (sscanf (buf, "x%x", &iStartOfSelSetting) == 0)
					{
						if (sscanf (buf, "%d", &iStartOfSelSetting) == 0)
						{//Pabs replaced NULL w hDlg
							MessageBox (hDlg, "Start offset not recognized.", "Select block", MB_OK | MB_ICONERROR);
							i = -1;
						}
					}
				}
				if (i==-1)
				{
					EndDialog (hDlg, 0);
					return 0;
				}
				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 128) != 0)
				{
					if (sscanf (buf, "x%x", &iEndOfSelSetting) == 0)
						if (sscanf (buf, "%d", &iEndOfSelSetting) == 0)
						{
							MessageBox (hDlg, "End offset not recognized.", "Select block", MB_OK | MB_ICONERROR);
							i = -1;
						}//end
				}
				if (i==-1)
				{
					EndDialog (hDlg, 0);
					return 0;
				}
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::update_MRU ()
{
	int i;
	for (i=0; i<iMRU_count; i++)
	{
		if (strcmp (&(strMRU[i][0]), filename)==0) // Name already in list.
		{
			// Name already in list => Put name at beginning of list.
			char temp[_MAX_PATH+1];
			strcpy (temp, &(strMRU[0][0])); // Save Pos.1.
			// Put No.1 name at pos. of chosen name.
			int j;
			for (j=i; j>0; j--)
				strcpy (&(strMRU[j][0]), &(strMRU[j-1][0]));
			strcpy (&(strMRU[0][0]), filename); // Put chosen name at top.
			break;
		}
	}
	if (i==iMRU_count) // Name not yet in list.
	{
		if (iMRU_count<MRUMAX)
		{
			// Space available in list.
			// Put chosen name at top pos., push rest down.
			int j;
			iMRU_count++;
			for (j=iMRU_count-1; j>0; j--)
				strcpy (&(strMRU[j][0]), &(strMRU[j-1][0]));
			strcpy (&(strMRU[0][0]), filename);
		}
		else
		{
			// No more space in list.
			// Push all down, last pos. will be lost.
			int j;
			for (j=MRUMAX-1; j>0; j--)
				strcpy (&(strMRU[j][0]), &(strMRU[j-1][0]));
			strcpy (&(strMRU[0][0]), filename);
		}
	}
	save_ini_data ();
	return 1;
}

//-------------------------------------------------------------------
// Creates the MRU list and inserts it into the File menu.
int HexEditorWindow::make_MRU_list (HMENU menu)
{
	int i;
//Pabs changed - 15 used to be 9
	while (RemoveMenu (menu, 15, MF_BYPOSITION)==TRUE)
		;

	if (iMRU_count>0)
	{
		AppendMenu (menu, MF_SEPARATOR, 0, 0);
		char buf[_MAX_PATH+1+30];
		for (i=0; i<iMRU_count; i++)
		{
			sprintf (buf, "&%d %s", i+1, &(strMRU[i][0]));
			AppendMenu (menu, MF_ENABLED | MF_STRING, IDM_MRU1+i, buf);
		}
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_MRU_selected (int cmd)
{
	if (cmd-IDM_MRU1+1>iMRU_count)
		return 0;

	if (file_is_loadable (&(strMRU[cmd-IDM_MRU1][0])))
	{
		if (m_iFileChanged == TRUE)
		{
//Pabs changed - restructured so that the user can save&open, open | not open if file changed
			int res = MessageBox (hwnd, "Do you want to save your changes?", "Open", MB_YESNOCANCEL | MB_ICONQUESTION);
			if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to open or User wants to save and the save was unsuccessful
				return 0;//Don't open
//			if (MessageBox (hwnd, "File was changed! Open anyway?", "Open", MB_YESNO | MB_ICONQUESTION) == IDNO)
//				return 0;
		}
		if (load_file (&(strMRU[cmd-IDM_MRU1][0])))
		{
			iVscrollMax = 0;
			iVscrollPos = 0;
			iVscrollInc = 0;
			iHscrollMax = 0;
			iHscrollPos = 0;
			iHscrollInc = 0;
			iCurLine = 0;
			iCurByte = 0;
			iCurNibble = 0;
			m_iFileChanged = FALSE;
			bFilestatusChanged = TRUE;
			bFileNeverSaved = FALSE;
//Pabs changed -line insert
			bSelected = FALSE;
//end
			update_MRU ();
			update_for_new_datasize ();
		}
	}
	else
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "This file could not be accessed and\n"
			"will be removed from the MRU list.", "MRU list", MB_OK | MB_ICONERROR);
		int j;//end
		for (j=cmd-IDM_MRU1; j<iMRU_count-1; j++)
			strcpy (&(strMRU[j][0]), &(strMRU[j+1][0]));
		iMRU_count--;
		save_ini_data ();
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_add_bookmark ()
{
	if (DataArray.GetLength()<=0)
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "Can not set bookmark in empty file.", "Add bookmark", MB_OK | MB_ICONERROR);
		return 0;
	}//end
	if (iBmkCount<BMKMAX)
	{
		iBmkOffset = iCurByte;
		pcBmkTxt[0]=0;
		if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_ADDBMK_DIALOG), hwnd, (DLGPROC) AddBmkDlgProc))
		{
			if (iBmkOffset>=0 && iBmkOffset<=DataArray.GetLength())
			{
				// Is there already a bookmark on this offset?
				int i;
				for( i = 0; i < iBmkCount; i++ )
				{
					if( pbmkList[ i ].offset == iBmkOffset )
					{//Pabs replaced NULL w hwnd
						MessageBox (hwnd, "There already is a bookmark on that position.", "Add bookmark", MB_OK | MB_ICONERROR);
						return 0;
					}//end
				}
				// No bookmark on that position yet.
				pbmkList[iBmkCount].offset = iBmkOffset;
				if (strlen(pcBmkTxt)>0)
				{
					pbmkList[iBmkCount].name = new char[strlen(pcBmkTxt)+1];
					strcpy (pbmkList[iBmkCount].name, pcBmkTxt);
				}
				else
					pbmkList[iBmkCount].name = NULL;
				iBmkCount++;
				repaint();
			}
			else
			{//Pabs replaced NULL w hwnd
				MessageBox (hwnd, "Can not set bookmark at that position.", "Add bookmark", MB_OK | MB_ICONERROR);
				return 0;
			}//end
		}
	}
	else
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "Can not set any more bookmarks.", "Add bookmark", MB_OK | MB_ICONERROR);
	}//end
	return 0;
}

//-------------------------------------------------------------------
// Insert the bookmark list into the menu.
int HexEditorWindow::make_bookmark_list (HMENU menu)
{
	int i;
	while (RemoveMenu (menu, 3, MF_BYPOSITION)==TRUE)
		;

	if (iBmkCount>0)
	{
		AppendMenu (menu, MF_SEPARATOR, 0, 0);
		char buf[128];
		for (i=0; i<iBmkCount; i++)
		{
			if (pbmkList[i].name == NULL)
				sprintf (buf, "&%d 0x%x", i+1, pbmkList[i].offset);
			else
				sprintf (buf, "&%d 0x%x:%s", i+1, pbmkList[i].offset, pbmkList[i].name);
			if (pbmkList[i].offset <= DataArray.GetLength())
				AppendMenu (menu, MF_ENABLED | MF_STRING, IDM_BOOKMARK1+i, buf);
			else
				AppendMenu (menu, MF_GRAYED | MF_STRING, IDM_BOOKMARK1+i, buf);
		}
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_goto_bookmark( int cmd )
{
	if( pbmkList[ cmd - IDM_BOOKMARK1 ].offset >= 0 && pbmkList[ cmd - IDM_BOOKMARK1 ].offset <= DataArray.GetLength() )
	{
		iCurByte = pbmkList[ cmd - IDM_BOOKMARK1 ].offset;
//Pabs inserted
		iCurNibble = 0;
		bSelected = FALSE;
//end
		update_for_new_datasize();
		adjust_vscrollbar();
		repaint();
	}
	else
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "Bookmark points to invalid position.", "Go to bookmark", MB_OK | MB_ICONERROR);
	}//end
	return 0;
}

//-------------------------------------------------------------------
BOOL CALLBACK AddBmkDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	int i = -1;
	switch (iMsg)
	{
	case WM_INITDIALOG:
		char buf[32];
		sprintf (buf, "x%x", iBmkOffset);
		SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
		SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			char buf[16];
			if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 16) != 0)
			{
				if (sscanf (buf, "x%x", &i) == 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{//Pabs replaced NULL w hDlg
						MessageBox (hDlg, "Start offset not recognized.", "Add bookmark", MB_OK | MB_ICONERROR);
						i = -1;
					}//end
				}
				if (i==-1)
				{
					EndDialog (hDlg, 0);
					return 0;
				}
				iBmkOffset = i;
			}
			GetDlgItemText (hDlg, IDC_EDIT2, pcBmkTxt, BMKTEXTMAX);
			EndDialog (hDlg, 1);
			return TRUE;

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_remove_bkm ()
{
	if (iBmkCount==0)
	{//Pabs replaced NULL w hwnd
		MessageBox (hwnd, "No bookmarks to remove.", "Remove bookmark", MB_OK | MB_ICONERROR);
		return 0;
	}//end
	pbmkRemove = pbmkList;
	iRemBmk = iBmkCount;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_REMOVEBMK_DIALOG), hwnd, (DLGPROC) RemoveBmkDlgProc))
	{
		if (pbmkList[iRemBmk].name != NULL)
			delete [] (pbmkList[iRemBmk].name);
		int i;
		for (i=iRemBmk; i<iBmkCount-1; i++)
			pbmkList[i] = pbmkList[i+1];
		iBmkCount--;
		repaint();
	}
	return 1;
}

//-------------------------------------------------------------------
BOOL CALLBACK RemoveBmkDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[128];
			int i;
			HWND hwndList = GetDlgItem (hDlg, IDC_LIST1);
			for (i=0; i<iRemBmk; i++)
			{
				if (pbmkRemove[i].name == NULL)
					sprintf (buf, "%d) 0x%x", i+1, pbmkRemove[i].offset);
				else
					sprintf (buf, "%d) 0x%x:%s", i+1, pbmkRemove[i].offset, pbmkRemove[i].name);
				SendMessage (hwndList, LB_ADDSTRING, 0, (LPARAM) buf);
			}
			SendMessage (hwndList, LB_SETCURSEL, 0, 0);
			SetFocus (GetDlgItem (hDlg, IDC_LIST1));
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				HWND hwndList = GetDlgItem (hDlg, IDC_LIST1);
				iRemBmk = SendMessage (hwndList, LB_GETCURSEL, 0, 0);
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_clear_all_bmk ()
{
	if( MessageBox( hwnd, "Really clear all bookmarks?", "Clear all bookmarks", MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		int i;
		for (i=0; i<iBmkCount; i++)
			if (pbmkList[i].name != NULL)
				delete [] (pbmkList[i].name);
		iBmkCount = 0;
		return 1;
	}
	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_open_partially ()
{
	if (m_iFileChanged == TRUE)
	{
//Pabs changed - restructured so that the user can save&open, open | not open if file changed
		int res = MessageBox (hwnd, "Do you want to save your changes?", "Open partially", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to open partially or User wants to save and the save was unsuccessful
			return 0;//Don't open
//		if (MessageBox (hwnd, "File was changed! Open partially anyway?", "Open", MB_YESNO | MB_ICONQUESTION) == IDNO)
//			return 0;
//end
	}
	char szFileName[_MAX_PATH];
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	szFileName[0] = '\0';
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetOpenFileName (&ofn))
	{
		unsigned int filehandle, filelen;
		if ((filehandle = _open (szFileName,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			filelen = _filelength (filehandle);
			_close (filehandle);
		}
		iStartPL = 0;
		iNumBytesPl = filelen;
		iPLFileLen = filelen;
		if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_OPEN_PARTIAL_DIALOG), hwnd, (DLGPROC) OpenPartiallyDlgProc))
		{
			if (iStartPL+iNumBytesPl<=filelen)
			{
				if ((filehandle = _open (szFileName,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
				{
					DataArray.ClearAll ();
					if (DataArray.SetSize (iNumBytesPl) == TRUE)
					{
						DataArray.SetUpperBound (iNumBytesPl-1);
						_lseek (filehandle, iStartPL, 0);
						iPartialOffset = iStartPL;
//Pabs inserted
						iPartialOpenLen = iNumBytesPl;
						iPartialFileLen = filelen;
						bPartialStats = bShowFileStatsPL;
//end
						if (_read (filehandle, DataArray, iNumBytesPl) != -1)
						{
							_close (filehandle);
							// If read-only mode on opening is enabled or the file is read only:
							if( bOpenReadOnly || -1== _access(szFileName,02))//Pabs added call to _access
								bReadOnly = TRUE;
							else
								bReadOnly = FALSE;
							strcpy (filename, szFileName);
							bFileNeverSaved = FALSE;
							iVscrollMax = 0;
							iVscrollPos = 0;
							iVscrollInc = 0;
							iHscrollMax = 0;
							iHscrollPos = 0;
							iHscrollInc = 0;
							iCurLine = 0;
							iCurByte = 0;
							iCurNibble = 0;
							m_iFileChanged = FALSE;
							bFilestatusChanged = TRUE;
							bFileNeverSaved = FALSE;
							bPartialOpen=TRUE;
							bSelected=FALSE;//Pabs inserted
							RECT r;
							GetClientRect (hwnd, &r);
							SendMessage (hwnd, WM_SIZE, 0, (r.bottom << 16) | r.right);
							InvalidateRect (hwnd, NULL, FALSE);
							UpdateWindow (hwnd);
							return TRUE;
						}
						else
						{
							_close (filehandle);//Pabs replaced NULL w hwnd
							MessageBox (hwnd, "Error while reading from file.", "Open partially", MB_OK | MB_ICONERROR);
							return FALSE;
						}
					}
					else
					{
						MessageBox (hwnd, "Not enough memory to load file.", "Open partially", MB_OK | MB_ICONERROR);
						return FALSE;
					}
				}
				else
				{
					char buf[500];
					sprintf (buf, "Error code 0x%x occured while opening file %s.", errno, szFileName);
					MessageBox (hwnd, buf, "Open partially", MB_OK | MB_ICONERROR);
					return FALSE;
				}
			}
			else
			{
				MessageBox (hwnd, "Too many bytes to load.", "Open partially", MB_OK | MB_ICONERROR);
				return 0;
			}//end
		}
	}
	return 1;
}


//-------------------------------------------------------------------
BOOL CALLBACK OpenPartiallyDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf[128];
			sprintf (buf, "x%x", iStartPL);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT1), buf);
			sprintf (buf, "Size of file: %u. Load how many bytes:", iPLFileLen);
			SetWindowText (GetDlgItem (hDlg, IDC_STATIC2), buf);
			sprintf (buf, "%u", iNumBytesPl);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf);
			SetFocus (GetDlgItem (hDlg, IDC_EDIT1));
			CheckDlgButton( hDlg, IDC_RADIO1, BST_CHECKED );
//Pabs inserted
			CheckDlgButton( hDlg, IDC_CHECK1, bShowFileStatsPL ? BST_CHECKED : BST_UNCHECKED );
//end
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				char buf[128];
				int i=0;

				if (GetDlgItemText (hDlg, IDC_EDIT1, buf, 128) != 0)
				{
					if (sscanf (buf, "x%x", &iStartPL) == 0)
					{
						if (sscanf (buf, "%u", &iStartPL) == 0)
						{
							i = -1;
						}
					}
				}

				// Only complain about wrong offset in start offset editbox if loading from start.
				if( i==-1 && IsDlgButtonChecked( hDlg, IDC_RADIO1 ) == BST_CHECKED )
				{//Pabs replaced NULL w hDlg
					MessageBox (hDlg, "Start offset not recognized.", "Open partially", MB_OK | MB_ICONERROR);
					EndDialog (hDlg, 0);
					return 0;
				}

				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 128) != 0)
				{
					if (sscanf (buf, "%u", &iNumBytesPl) == 0)
					{
						i = -1;
					}
				}

				if (i==-1)
				{
					MessageBox (hDlg, "Number of bytes not recognized.", "Open partially", MB_OK | MB_ICONERROR);
					EndDialog (hDlg, 0);
					return 0;
				}

				if( IsDlgButtonChecked( hDlg, IDC_RADIO2 ) == BST_CHECKED )
				{
					// Load from end of file: arguments must be adapted.
					if( iPLFileLen < iNumBytesPl )
					{
						MessageBox (hDlg, "Specified number of bytes to load\ngreater than file size.", "Open partially", MB_OK | MB_ICONERROR);
						EndDialog (hDlg, 0);
						return 0;//end
					}
					iStartPL = iPLFileLen - iNumBytesPl;
				}
//Pabs inserted
				bShowFileStatsPL = ( IsDlgButtonChecked( hDlg, IDC_CHECK1 ) == BST_CHECKED );
//end
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_fast_paste ()
{
//Pabs removed reinitialisations
	// If iInsertMode is TRUE, then insert, don't overwrite.
	if( iInsertMode )
		iPasteMode = 2;
	else
		iPasteMode = 1;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_FASTPASTE_DIALOG), hwnd, (DLGPROC) FastPasteDlgProc))
	{
		switch( iPasteMode )
		{
		case 1: // Overwrite.
			{
				char* pcPastestring;
				// Create findstring.
				int destlen;
//Pabs inserted
				if( bPasteBinary ){
					destlen = iPasteMaxTxtLen;
					pcPastestring = pcPasteText;
					pcPasteText = NULL;
				}
				else
//end
				if( iPasteAsText == TRUE )
				{//Pabs changed to allow unicode text
					destlen = bPasteUnicode ? 2*wcslen( (WCHAR*) pcPasteText ) : strlen( pcPasteText );
					pcPastestring = new char[ destlen ];
					memcpy( pcPastestring, pcPasteText, destlen );
				}
				else
				{
					destlen = create_bc_translation (&pcPastestring, pcPasteText, strlen (pcPasteText), iCharacterSet, iBinaryMode);
				}
				if (destlen > 0)
				{
					// Enough space for writing?
					// DataArray.GetLength()-iCurByte = number of bytes from including curbyte to end.
//Pabs changed - "(iPasteSkip+destlen)" used to be "destlen"
					if (DataArray.GetLength()-iCurByte >= (iPasteSkip+destlen)*iPasteTimes)
//end
					{
						// Overwrite data.
						SetCursor (LoadCursor (NULL, IDC_WAIT));
						int i,k;
						for (k=0; k<iPasteTimes; k++)
						{
							for (i=0; i<destlen; i++)
							{
//Pabs changed - "(iPasteSkip+destlen)" used to be "destlen"
								DataArray[(iCurByte+k*(iPasteSkip+destlen))+i] = pcPastestring[i];
//end
							}
						}
						SetCursor (LoadCursor (NULL, IDC_ARROW));
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
					}
					else
					{
						MessageBox (hwnd, "Not enough space for overwriting.", "Paste", MB_OK | MB_ICONERROR);
					}
					delete [] pcPastestring;
				}
				delete [] pcPasteText;
				repaint ();
				break;
			}

		case 2: // Insert.
			{
				char* pcPastestring;
				int destlen;
//Pabs inserted
				if( bPasteBinary ){
					destlen = iPasteMaxTxtLen;
					pcPastestring = pcPasteText;
					pcPasteText = NULL;
				}
				else
//end
				if( iPasteAsText == TRUE )
				{//Pabs changed to allow unicode text
					destlen = bPasteUnicode ? 2*wcslen( (WCHAR*) pcPasteText ) : strlen( pcPasteText );
					pcPastestring = new char[ destlen ];
					memcpy( pcPastestring, pcPasteText, destlen );
				}
				else
				{
					destlen = create_bc_translation (&pcPastestring, pcPasteText, strlen (pcPasteText), iCharacterSet, iBinaryMode);
				}
				if (destlen > 0)
				{
					// Insert at iCurByte. Byte there will be pushed up.
					SetCursor (LoadCursor (NULL, IDC_WAIT));
//Pabs changed - line insert
					int i, k;
					for( k = 0,i=iCurByte; k < iPasteTimes; k++ ){
						if(!DataArray.InsertAtGrow(iCurByte,(unsigned char*)pcPastestring,0,destlen)){
							delete [] pcPastestring;
							delete [] pcPasteText;
							SetCursor (LoadCursor (NULL, IDC_ARROW));
							MessageBox (hwnd, "Not enough memory for inserting.", "Paste", MB_OK | MB_ICONERROR);
							update_for_new_datasize ();
							return FALSE;
						}
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
						iCurByte+=destlen+iPasteSkip;
					}
					iCurByte=i;
//end
					// RK: removed old code that pabs commented out.
					m_iFileChanged = TRUE;
					bFilestatusChanged = TRUE;
					update_for_new_datasize ();
					delete [] pcPastestring;
					SetCursor (LoadCursor (NULL, IDC_ARROW));
				}
				delete [] pcPasteText;
				repaint ();
				break;
			}

		default:
			break;
		}
	}
	return 0;
}

//-------------------------------------------------------------------
BOOL CALLBACK FastPasteDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hwndNextViewer = NULL;
	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			char buf2[16];
			sprintf (buf2, "%d", iPasteTimes);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT2), buf2);
//Pabs changed - line insert
			sprintf (buf2, "%d", iPasteSkip);
			SetWindowText (GetDlgItem (hDlg, IDC_EDIT3), buf2);

			SendMessage(hDlg,WM_COMMAND,MAKELONG(IDC_REFRESH,BN_CLICKED),(LPARAM)GetDlgItem(hDlg,IDC_RELOAD));

			hwndNextViewer = SetClipboardViewer(hDlg);
//end
			// Depending on INS or OVR mode, set the radio button.
			if( iPasteMode == 1 )
				CheckDlgButton (hDlg, IDC_RADIO1, BST_CHECKED);
			else
				CheckDlgButton (hDlg, IDC_RADIO2, BST_CHECKED);
			if( iPasteAsText == TRUE )
				CheckDlgButton (hDlg, IDC_CHECK1, BST_CHECKED);
			SetFocus (GetDlgItem (hDlg, IDC_RADIO1));
			return FALSE;
		}
	case WM_CHANGECBCHAIN:
		// If the next window is closing, repair the chain.
		if ((HWND) wParam == hwndNextViewer)
			hwndNextViewer = (HWND) lParam;
		// Otherwise, pass the message to the next link.
		else if (hwndNextViewer != NULL)
			SendMessage(hwndNextViewer, iMsg, wParam, lParam);
	break;
	case WM_DRAWCLIPBOARD:  // clipboard contents changed.
		// Update the window by using Auto clipboard format.
		SendMessage(hDlg,WM_COMMAND,MAKELONG(IDC_REFRESH,BN_CLICKED),(LPARAM)GetDlgItem(hDlg,IDC_RELOAD));
		// Pass the message to the next window in clipboard viewer chain.
		SendMessage(hwndNextViewer, iMsg, wParam, lParam);
	break;
	case WM_DESTROY:
		ChangeClipboardChain(hDlg, hwndNextViewer);
		hwndNextViewer = NULL;
	break;
	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				if (IsDlgButtonChecked (hDlg, IDC_CHECK1) == BST_CHECKED)
					iPasteAsText = TRUE;
				else
					iPasteAsText = FALSE;
				if (IsDlgButtonChecked (hDlg, IDC_RADIO1) == BST_CHECKED)
					iPasteMode = 1;
				else
					iPasteMode = 2;
				char buf[64];
				int i;
				if (GetDlgItemText (hDlg, IDC_EDIT2, buf, 64) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{//Pabs replaced NULL w hDlg
						MessageBox (hDlg, "Number of times to paste not recognized.", "Paste", MB_OK | MB_ICONERROR);
						i = -1;
					}
				}
				if (i<=0)
				{
					MessageBox (hDlg, "Number of times to paste must be at least 1.", "Paste", MB_OK | MB_ICONERROR);
					EndDialog (hDlg, 0);
					return 0;
				}
				iPasteTimes = i;
//Pabs changed - line insert
				if (GetDlgItemText (hDlg, IDC_EDIT3, buf, 64) != 0)
				{
					if (sscanf (buf, "%d", &i) == 0)
					{
						MessageBox (hDlg, "Number of bytes to skip not recognized.", "Paste", MB_OK | MB_ICONERROR);
						return 0;//end
					}
				}
				iPasteSkip = i;
				HWND list = GetDlgItem(hDlg, IDC_LIST);
				i = SendMessage (list, LB_GETCURSEL, 0, 0);
				if( i == LB_ERR ){
					MessageBox (hDlg, "You need to select a clipboard format to use.", "Paste", MB_OK | MB_ICONERROR);
					return 0;//end
				}
				UINT uFormat = SendMessage (list, LB_GETITEMDATA, i, 0);
//end
				if (OpenClipboard (NULL))
				{//Pabs replaced CF_TEXT with uFormat
					HGLOBAL hClipMemory = GetClipboardData (uFormat);
					if (hClipMemory != NULL)
					{
						int gsize = GlobalSize (hClipMemory);
						if (gsize > 0)
						{
							char* pClipMemory = (char*) GlobalLock (hClipMemory);
							pcPasteText = new char[gsize];
							memcpy (pcPasteText, pClipMemory, gsize);
						}
						GlobalUnlock (hClipMemory);
//Pabs changed - line insert
						bPasteUnicode = 0;
						switch(uFormat){
							case CF_UNICODETEXT:
								bPasteUnicode = 1;
							case CF_TEXT:
							case CF_DSPTEXT:
							case CF_OEMTEXT:
								bPasteBinary = 0;
								break;
							default:
								iPasteMaxTxtLen = gsize;//CRAP f**king M$ Windoze
								bPasteBinary = 1;
								break;
						}
//end
					}
					CloseClipboard ();
				}
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
//Pabs inserted
		case IDC_REFRESH:{
			//Get all the Clipboard formats
			HWND list = GetDlgItem(hDlg, IDC_LIST);
			SendMessage( list, LB_RESETCONTENT, 0, 0 );
			if(CountClipboardFormats() && OpenClipboard(NULL)){
				UINT uFormat;
				int i;
				char szFormatName[100], SetSel = 0;
				LPCSTR lpFormatName;

				uFormat = EnumClipboardFormats(0);
				while (uFormat){
					lpFormatName = NULL;

					// For registered formats, get the registered name.
					if (GetClipboardFormatName(uFormat, szFormatName, sizeof(szFormatName)))
						lpFormatName = szFormatName;
					else{
						//Get the name of the standard clipboard format.
						switch(uFormat){
							#define CASE(a,b) case a: lpFormatName = #a; SetSel = b; break;
								CASE(CF_TEXT,1)
							#undef CASE
							#define CASE(a) case a: lpFormatName = #a; break;
								CASE(CF_BITMAP) CASE(CF_METAFILEPICT) CASE(CF_SYLK)
								CASE(CF_DIF) CASE(CF_TIFF) CASE(CF_OEMTEXT)
								CASE(CF_DIB) CASE(CF_PALETTE) CASE(CF_PENDATA)
								CASE(CF_RIFF) CASE(CF_WAVE) CASE(CF_UNICODETEXT)
								CASE(CF_ENHMETAFILE) CASE(CF_HDROP) CASE(CF_LOCALE)
								CASE(CF_MAX) CASE(CF_OWNERDISPLAY) CASE(CF_DSPTEXT)
								CASE(CF_DSPBITMAP) CASE(CF_DSPMETAFILEPICT)
								CASE(CF_DSPENHMETAFILE) CASE(CF_PRIVATEFIRST)
								CASE(CF_PRIVATELAST) CASE(CF_GDIOBJFIRST)
								CASE(CF_GDIOBJLAST) CASE(CF_DIBV5)
							#undef CASE
							default:
								if(i!=i);
								#define CASE(a) else if(uFormat>a##FIRST&&uFormat<a##LAST) sprintf(szFormatName,#a "%d",uFormat-a##FIRST);
									CASE(CF_PRIVATE) CASE(CF_GDIOBJ)
								#undef CASE
								/*Format ideas for future: hex number, system/msdn constant, registered format, WM_ASKFORMATNAME, tickbox for delay rendered or not*/
								/*else if(uFormat>0xC000&&uFormat<0xFFFF){
									sprintf(szFormatName,"CF_REGISTERED%d",uFormat-0xC000);
								}*/
								else break;
								lpFormatName = szFormatName;
							break;
						}
					}

					if (lpFormatName == NULL){
						sprintf(szFormatName,"0x%.8x",uFormat);
						lpFormatName = szFormatName;
					}

					//Insert into the list
					if(lpFormatName){
						i = SendMessage(list, LB_ADDSTRING, 0, (LPARAM) lpFormatName);
						if(SetSel == 1){SetSel = 2; SendMessage(list, LB_SETCURSEL, i, 0);}
						SendMessage(list, LB_SETITEMDATA, i, uFormat);
					}

					uFormat = EnumClipboardFormats(uFormat);
				}
				CloseClipboard();
				if(!SetSel) SendMessage(list, LB_SETCURSEL, 0, 0);
			}
		}
		break;
		case IDC_LIST:
			if(HIWORD(wParam) == LBN_SELCHANGE){
				HWND list = GetDlgItem(hDlg, IDC_LIST);
				int i = SendMessage(list, LB_GETCURSEL, 0, 0);
				UINT f = SendMessage(list, LB_GETITEMDATA, i, 0);
				if( f == CF_UNICODETEXT ) CheckDlgButton(hDlg,IDC_CHECK1, BST_CHECKED);
			}
			break;
		}
		break;
//end
	}
	return FALSE;
}

//-------------------------------------------------------------------
//Pabs inserted
BOOL CALLBACK MultiDropDlgProc (HWND h, UINT m, WPARAM w, LPARAM l)
{
	switch (m)
	{
		case WM_INITDIALOG:
		{
			SetWindowText(h,"Open");
			SetDlgItemText(h,0xFFFF,"Choose a file to open:");
			SetDlgItemText(h,IDOK,"Open");
			HWND hwndList = GetDlgItem (h, IDC_LIST1);
			SetWindowLong(hwndList,GWL_STYLE,(GetWindowLong(hwndList,GWL_STYLE)&(~LBS_SORT))|WS_HSCROLL);

			char file[_MAX_PATH+1];
			UINT n = DragQueryFile( (HDROP) l, 0xFFFFFFFF, NULL, 0 );
			SendMessage (hwndList, LB_INITSTORAGE, n, _MAX_PATH+1);
			for (UINT i=0; i<n; i++)
			{
				DragQueryFile( (HDROP) l, i, file, _MAX_PATH+1 );
				SendMessage (hwndList, LB_INSERTSTRING, i, (LPARAM) file);
			}
			SendMessage (hwndList, LB_SETCURSEL, 0, 0);
			SetFocus (hwndList);
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (w))
		{
			case IDOK:
				EndDialog (h, SendDlgItemMessage(h, IDC_LIST1, LB_GETCURSEL, 0, 0));
			return TRUE;
			case IDCANCEL:
				EndDialog (h, SendDlgItemMessage(h, IDC_LIST1, LB_GETCOUNT, 0, 0));
			return TRUE;
		}
		break;
	}
	return FALSE;
}
//end

// Handler for WM_DROPFILES
int HexEditorWindow::dropfiles( HANDLE hDrop )
{
	//Kludge to prevent drops when user cancels menu/dialog in CDropTarget::Drop
	if( dontdrop ){	dontdrop = false; return 0; }
	char lpszFile[ _MAX_PATH+1 ];
//Pabs inserted
	UINT numfiles;
//end
//Pabs inserted around DragQueryFile after reading the fuzz report
	try{//Make sure that hDrop is valid
		numfiles = DragQueryFile( (HDROP) hDrop, 0xFFFFFFFF, NULL, 0 );
	} catch(...){ return 0; }
//end
//Pabs inserted
	UINT i = 0;
	if( numfiles > 1 )
	{
		i = DialogBoxParam( hInstance, MAKEINTRESOURCE (IDD_REMOVEBMK_DIALOG), hwnd, (DLGPROC) MultiDropDlgProc, (LPARAM) hDrop );
		if( i >= numfiles ){
			DragFinish( (HDROP) hDrop );
			return 0;
		}
	}
	DragQueryFile( (HDROP) hDrop, i, lpszFile, _MAX_PATH+1 );
//end

	DragFinish( (HDROP) hDrop ); // handle to memory to free
	SetForegroundWindow( hwnd );
	if( m_iFileChanged == TRUE )
	{
//Pabs changed - restructured so that the user can save&open, open | not open if file changed
		int res = MessageBox (hwnd, "Do you want to save your changes?", "Open", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to open or User wants to save and the save was unsuccessful
			return 0;//Don't open
//		if( MessageBox (hwnd, "File was changed! Open anyway?", "frhed", MB_YESNO | MB_ICONQUESTION) == IDNO )
//			return 0;
	}

	char lpszTarget[ MAX_PATH ];
	// Is this a link file?
	HRESULT hres = ResolveIt( hwnd, lpszFile, lpszTarget );
//Pabs restructured to reduce amount of code
	char* lpszFileToOpen = lpszFile;
	if( SUCCEEDED( hres ) )
	{
		// Trying to open a link file: decision by user required.
		int ret = MessageBox( hwnd,
			"You are trying to open a link file.\n"
			"Click on Yes if you want to open the file linked to,\n"
			"or click on No if you want to open the link file itself.\n"
			"Choose Cancel if you want to abort opening.",
			"frhed", MB_YESNOCANCEL | MB_ICONQUESTION );
		switch( ret )
		{
			case IDYES:
				lpszFileToOpen = lpszTarget;
			break;
			case IDCANCEL:
				return 0;
		}
	}

	if( (lpszFileToOpen == lpszTarget && load_file( lpszFileToOpen ) ) || load_file( lpszFile ) )
	{
		iVscrollMax = 0;
		iVscrollPos = 0;
		iVscrollInc = 0;
		iHscrollMax = 0;
		iHscrollPos = 0;
		iHscrollInc = 0;
		iCurLine = 0;
		iCurByte = 0;
		iCurNibble = 0;
		m_iFileChanged = FALSE;
		bFilestatusChanged = TRUE;
		update_for_new_datasize();
	}
//end

	return 1;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_apply_template()
{
	if( DataArray.GetLength() == 0 )
	{//Pabs replaced NULL w hwnd
		MessageBox( hwnd, "File is empty.", "Template error", MB_OK | MB_ICONERROR );
		return FALSE;
	}//end

	// Get name of template file.
	char szTemplateName[_MAX_PATH];
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	szTemplateName[0] = '\0';
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Template files (*.tpl)\0*.tpl\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szTemplateName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = "Choose template file";
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if( GetOpenFileName( &ofn ) )
	{
		// szTemplateName contains name of chosen tpl file.
		apply_template( szTemplateName );
	}
	return TRUE;
}

//-------------------------------------------------------------------
int HexEditorWindow::apply_template( char* pcTemplate )
{
	// Load template file.
	int filehandle;
	if( ( filehandle = _open( pcTemplate, _O_RDONLY|_O_BINARY, _S_IREAD|_S_IWRITE ) ) != -1 )
	{
		int tpl_filelen = _filelength( filehandle );
		if( tpl_filelen > 0 )
		{
			char* pcTpl = new char[ tpl_filelen ];
			if( pcTpl != NULL && _read( filehandle, pcTpl, tpl_filelen ) != -1 )
			{
				// Template file is loaded into pcTpl.
				SimpleArray<char> TplResult;
				TplResult.SetSize( 1, 100 );
				// Print filename and current offset to output.
				TplResult.AppendArray( "File: ", 6 );
				TplResult.AppendArray( filename, strlen( filename ) );
				TplResult.AppendArray( "\xd\xa", 2 );
				TplResult.AppendArray( "Template file: ", 15 );
				TplResult.AppendArray( pcTemplate, strlen( pcTemplate ) );
				TplResult.AppendArray( "\xd\xa", 2 );
				TplResult.AppendArray( "Applied at offset: ", 19 );
				char buf[16];
				sprintf( buf, "%d\xd\xa\xd\xa", iCurByte );
				TplResult.AppendArray( buf, strlen( buf ) );
				// Apply template on file in memory.
				apply_template_on_memory( pcTpl, tpl_filelen, TplResult );
				TplResult.Append( '\0' );
				// Display template data.
				pcTmplText = TplResult;
				DialogBox( hInstance, MAKEINTRESOURCE( IDD_TMPL_RESULT_DIALOG ), hwnd, (DLGPROC) TmplDisplayDlgProc );
			}
			else//Pabs replaced NULL w hwnd
				MessageBox( hwnd, "Template file could not be loaded.", "Template error", MB_OK | MB_ICONERROR );
			// Delete template data.
			if( pcTpl != NULL )
				delete [] pcTpl;
		}
		else
			MessageBox( hwnd, "Template file is empty.", "Template error", MB_OK | MB_ICONERROR );
		_close( filehandle );
	}
	else
	{
		char buf[500];
		sprintf( buf, "Error code 0x%x occured while\nopening template file %s.", errno, pcTemplate );
		MessageBox( hwnd, buf, "Template error", MB_OK | MB_ICONERROR );
		return FALSE;//end
	}
	return TRUE;
}

//-------------------------------------------------------------------
// Applies the template code in pcTpl of length tpl_len on the current file
// from the current offset and outputs the result to the ResultArray.
int HexEditorWindow::apply_template_on_memory( char* pcTpl, int tpl_len, SimpleArray<char>& ResultArray )
{
	// Use source code in pcTpl to decipher data in file.
	int index = 0, fpos = iCurByte;
	// While there is still code left...
	while( index < tpl_len )
	{
		// Read in the var type.
		if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
		{
			// index now points to first code character.
			// Get var type.
			char cmd[ TPL_TYPE_MAXLEN ]; // This holds the variable type, like byte or word.
			if( read_tpl_token( pcTpl, tpl_len, index, cmd ) == TRUE )
			{
				// cmd holds 0-terminated var type, index set to position of first space-
				// character after the type. Now test if valid type was given.
				//---- type BYTE ---------------------------------
				if( strcmp( cmd, "BYTE" ) == 0 || strcmp( cmd, "char" ) == 0 )
				{
					// This is a byte/char.
					if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
					{
						// Enough space for a byte?
						if( DataArray.GetLength() - fpos >= 1 )
						{
							// Read var name.
							char name[ TPL_NAME_MAXLEN ];
							// index is set to a non-space character by last call to ignore_non_code.
							// Therefore the variable name can be read into buffer name.
							read_tpl_token( pcTpl, tpl_len, index, name );
							// Write variable type and name to output.
							ResultArray.AppendArray( cmd, strlen(cmd) );
							ResultArray.Append( ' ' );
							ResultArray.AppendArray( name, strlen(name) );
							// Write value to output.
							char buf[ TPL_NAME_MAXLEN + 200];
							if( DataArray[fpos] != 0 )
								sprintf( buf, " = %d (signed) = %u (unsigned) = 0x%x = \'%c\'\xd\xa", (int) (signed char) DataArray[fpos], DataArray[fpos], DataArray[fpos], DataArray[fpos] );
							else
								sprintf( buf, " = %d (signed) = %u (unsigned) = 0x%x\xd\xa", (int) (signed char) DataArray[fpos], DataArray[fpos], DataArray[fpos] );
							ResultArray.AppendArray( buf, strlen(buf) );
							// Increase pointer for next variable.
							fpos += 1;
						}
						else
						{
							ResultArray.AppendArray( "ERROR: not enough space for byte-size datum.", 45 );
							return FALSE;
						}
					}
					else
					{
						// No non-spaces after variable type up to end of array, so
						// no space for variable name.
						ResultArray.AppendArray( "ERROR: missing variable name.", 29 );
						return FALSE;
					}
				}
				else if( strcmp( cmd, "WORD" ) == 0 || strcmp( cmd, "short" ) == 0 )
				{
					// This is a word.
					if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
					{
						// Enough space for a word?
						if( DataArray.GetLength() - fpos >= 2 )
						{
							// Read var name.
							char name[ TPL_NAME_MAXLEN ];
							read_tpl_token( pcTpl, tpl_len, index, name );
							// Write variable type to output.
							ResultArray.AppendArray( cmd, strlen(cmd) );
							ResultArray.Append( ' ' );
							// Write variable name to output.
							ResultArray.AppendArray( name, strlen(name) );
							WORD wd;
							// Get value depending on binary mode.
							if( iBinaryMode == LITTLEENDIAN_MODE )
							{
								wd = *( (WORD*)( &DataArray[ fpos ] ) );
							}
							else // BIGENDIAN_MODE
							{
								int i;
								for( i=0; i<2; i++ )
									((char*)&wd)[ i ] = DataArray[ fpos + 1 - i ];
							}
							char buf[ TPL_NAME_MAXLEN + 200 ];
							sprintf( buf, " = %d (signed) = %u (unsigned) = 0x%x\xd\xa", (int) (signed short) wd, wd, wd );
							ResultArray.AppendArray( buf, strlen(buf) );
							fpos += 2;
						}
						else
						{
							ResultArray.AppendArray( "ERROR: not enough space for WORD.", 34 );
							return FALSE;
						}
					}
					else
					{
						ResultArray.AppendArray( "ERROR: missing variable name.", 29 );
						return FALSE; // No more code: missing name.
					}
				}
				else if( strcmp( cmd, "DWORD" ) == 0 || strcmp( cmd, "int" ) == 0 ||
					strcmp( cmd, "long" ) == 0 || strcmp( cmd, "LONG" ) == 0 )
				{
					// This is a longword.
					if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
					{
						// Enough space for a longword?
						if( DataArray.GetLength() - fpos >= 4 )
						{
							// Read var name.
							char name[ TPL_NAME_MAXLEN ];
							read_tpl_token( pcTpl, tpl_len, index, name );
							// Write variable type to output.
							ResultArray.AppendArray( cmd, strlen(cmd) );
							ResultArray.Append( ' ' );
							// Write variable name to output.
							ResultArray.AppendArray( name, strlen(name) );
							DWORD dw;
							// Get value depending on binary mode.
							if( iBinaryMode == LITTLEENDIAN_MODE )
							{
								dw = *( (DWORD*)( &DataArray[ fpos ] ) );
							}
							else // BIGENDIAN_MODE
							{
								int i;
								for( i=0; i<4; i++ )
									((char*)&dw)[ i ] = DataArray[ fpos + 3 - i ];
							}
							char buf[ TPL_NAME_MAXLEN + 200 ];
							sprintf( buf, " = %d (signed) = %u (unsigned) = 0x%x\xd\xa", (signed long) dw, (unsigned long) dw, dw );
							ResultArray.AppendArray( buf, strlen(buf) );
							fpos += 4;
						}
						else
						{
							ResultArray.AppendArray( "ERROR: not enough space for DWORD.", 34 );
							return FALSE;
						}
					}
					else
					{
						ResultArray.AppendArray( "ERROR: missing variable name.", 29 );
						return FALSE; // No more code: missing name.
					}
				}
				else if( strcmp( cmd, "float" ) == 0 )
				{
					// This is a float.
					if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
					{
						// Enough space for a float?
						if( DataArray.GetLength() - fpos >= 4 )
						{
							// Read var name.
							char name[ TPL_NAME_MAXLEN ];
							read_tpl_token( pcTpl, tpl_len, index, name );
							// Write variable type to output.
							ResultArray.AppendArray( cmd, strlen(cmd) );
							ResultArray.Append( ' ' );
							// Write variable name to output.
							ResultArray.AppendArray( name, strlen(name) );
							float f;
							// Get value depending on binary mode.
							if( iBinaryMode == LITTLEENDIAN_MODE )
							{
								f = *( (float*)( &DataArray[ fpos ] ) );
							}
							else // BIGENDIAN_MODE
							{
								int i;
								for( i=0; i<4; i++ )
									((char*)&f)[ i ] = DataArray[ fpos + 3 - i ];
							}
							char buf[ TPL_NAME_MAXLEN + 200 ];
							sprintf( buf, " = %f = 0x%x\xd\xa", f, (unsigned long) *((int*) &f) );
							ResultArray.AppendArray( buf, strlen(buf) );
							fpos += 4;
						}
						else
						{
							ResultArray.AppendArray( "ERROR: not enough space for float.", 34 );
							return FALSE;
						}
					}
					else
					{
						ResultArray.AppendArray( "ERROR: missing variable name.", 29 );
						return FALSE; // No more code: missing name.
					}
				}
				else if( strcmp( cmd, "double" ) == 0 )
				{
					// This is a double.
					if( ignore_non_code( pcTpl, tpl_len, index ) == TRUE )
					{
						// Enough space for a double?
						if( DataArray.GetLength() - fpos >= 8 )
						{
							// Read var name.
							char name[ TPL_NAME_MAXLEN ];
							read_tpl_token( pcTpl, tpl_len, index, name );
							// Write variable type to output.
							ResultArray.AppendArray( cmd, strlen(cmd) );
							ResultArray.Append( ' ' );
							// Write variable name to output.
							ResultArray.AppendArray( name, strlen(name) );
							double d;
							// Get value depending on binary mode.
							if( iBinaryMode == LITTLEENDIAN_MODE )
							{
								d = *( (double*)( &DataArray[ fpos ] ) );
							}
							else // BIGENDIAN_MODE
							{
								int i;
								for( i=0; i<8; i++ )
									((char*)&d)[ i ] = DataArray[ fpos + 7 - i ];
							}
							char buf[ TPL_NAME_MAXLEN + 200 ];
							sprintf( buf, " = %g\xd\xa", d );
							ResultArray.AppendArray( buf, strlen(buf) );
							fpos += 8;
						}
						else
						{
							ResultArray.AppendArray( "ERROR: not enough space for double.", 35 );
							return FALSE;
						}
					}
					else
					{
						ResultArray.AppendArray( "ERROR: missing variable name.", 29 );
						return FALSE; // No more code: missing name.
					}
				}
				else
				{
					ResultArray.AppendArray( "ERROR: Unknown variable type \"", 30 );
					ResultArray.AppendArray( cmd, strlen( cmd ) );
					ResultArray.Append( '\"' );
					return FALSE;
				}
			}
			else
			{
				// After the type there is only the array end. Therefore
				// no space for a variable name.
				ResultArray.AppendArray( "ERROR: Missing variable name.", 29 );
				return FALSE;
			}
		}
		else
		{
			// No non-spaces up to the end of the array.
			break;
		}
	}
	// No more code left in pcTpl.
	char buf[128];
	sprintf( buf, "\xd\xa-> Length of template = %d bytes.\xd\xa", fpos - iCurByte );
	ResultArray.AppendArray( buf, strlen( buf ) );
	return TRUE;
}

//-------------------------------------------------------------------
// This will set index to the position of the next non-space-character.
// Return is FALSE if there are no non-spaces left up to the end of the array.
int HexEditorWindow::ignore_non_code( char* pcTpl, int tpl_len, int& index )
{
	while( index < tpl_len )
	{
		// If code found, return.
		switch( pcTpl[ index ] )
		{
		case ' ': case '\t': case 0x0d: case 0x0a:
			break;

		default:
			return TRUE;
		}
		index++;
	}
	return FALSE;
}

//-------------------------------------------------------------------
// Writes all non-space characters from index to dest and closes dest
// with a zero-byte. index is set to position of the first space-
// character. Return is false ? there is only the array end after the
// keyword. In that case index is set to tpl_len.
int HexEditorWindow::read_tpl_token( char* pcTpl, int tpl_len, int& index, char* dest )
{
	int i = 0;
	while( index + i < tpl_len )
	{
		switch( pcTpl[ index + i ] )
		{
		case ' ': case '\t': case 0x0d: case 0x0a:
			dest[i] = '\0';
			index += i;
			return TRUE;

		default:
			dest[i] = pcTpl[ index + i ];
		}
		i++;
	}
	dest[i] = '\0';
	index += i;
	return FALSE;
}

//-------------------------------------------------------------------
// TmplDisplayDlgProc
BOOL CALLBACK TmplDisplayDlgProc( HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam )
{
	UNREFERENCED_PARAMETER( lParam );
	switch( iMsg )
	{
	case WM_INITDIALOG:
		{
			SetWindowText( GetDlgItem( hDlg, IDC_EDIT1 ), pcTmplText );
			SetFocus( GetDlgItem( hDlg, IDC_EDIT1 ) );
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			{
				EndDialog (hDlg, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::CMD_goto()
{
	if( filename[0] == '\0' )
		return 0;
	if( DialogBox( hInstance, MAKEINTRESOURCE(IDD_GOTODIALOG), hwnd, (DLGPROC) GoToDlgProc ) )
	{
		int offset, i = 0, r = 0;
		if( pcGotoDlgBuffer[0] == '+' || pcGotoDlgBuffer[0] == '-' )
		{
			// Relative jump. Read offset from 2nd character on.
			r = 1;
		}

		if( sscanf( &pcGotoDlgBuffer[r], "x%x", &offset ) == 0 )
		{
			// No fields assigned.
			if( sscanf ( &pcGotoDlgBuffer[r], "%d", &offset ) == 0 )
			{
				// No fields assigned: invalid offset.
				i = -1;
			}
		}

		if( i==-1 )
		{//Pabs replaced NULL w hwnd
			MessageBox (hwnd, "Offset not recognized.", "Go to", MB_OK | MB_ICONERROR);
			return 0;
		}//end

		if( r == 1 )
		{
			// Relative jump.
			if ( pcGotoDlgBuffer[0] == '-' )
				r = -1;

			if( iCurByte + r * offset >= 0 && iCurByte + r * offset <= LASTBYTE )
			{
				iCurByte = iCurByte + r * offset;
				iCurLine = BYTELINE;
//Pabs inserted
				if( iCurLine > iNumlines - cyBuffer + 1 )
					iCurLine = iNumlines - cyBuffer + 1;
				if( iCurLine < 0 ) iCurLine = 0;
//end
				adjust_vscrollbar();
				repaint();
			}
			else
				MessageBox( hwnd, "Invalid offset.", "Go to", MB_OK | MB_ICONERROR );
		}
		else
		{
			// Absolute jump.
			if( offset >= 0 && offset <= LASTBYTE )
			{
				iCurByte = offset;
				iCurLine = BYTELINE;
//Pabs inserted
				if( iCurLine > iNumlines - cyBuffer + 1 )
					iCurLine = iNumlines - cyBuffer + 1;
				if( iCurLine < 0 ) iCurLine = 0;
//end
				adjust_vscrollbar();
				repaint();
			}
			else
				MessageBox( hwnd, "Invalid offset.", "Go to", MB_OK | MB_ICONERROR );
		}
	}
	return 1;
}

//-------------------------------------------------------------------
// Resolve link files for opening from command line.
// Copied from compiler documentation.
HRESULT ResolveIt( HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath )
{
	HRESULT hres;
	IShellLink* psl;
	char szGotPath[MAX_PATH];
	char szDescription[MAX_PATH];
	WIN32_FIND_DATA wfd;

	*lpszPath = 0; // assume failure

	// Get a pointer to the IShellLink interface.
	CoInitialize( NULL );
	hres = CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl );
	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf;

		// Get a pointer to the IPersistFile interface.
		hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
		if (SUCCEEDED(hres))
		{
			WCHAR wsz[MAX_PATH];

			// Ensure that the string is Unicode.
			MultiByteToWideChar(CP_ACP, 0, lpszLinkFile, -1, wsz, MAX_PATH);

			// Load the shortcut.
			hres = ppf->Load(wsz, STGM_READ);
			if (SUCCEEDED(hres))
			{
				// Resolve the link.
				hres = psl->Resolve(hwnd, SLR_ANY_MATCH);
				if (SUCCEEDED(hres))
				{
					// Get the path to the link target.
					hres = psl->GetPath(szGotPath, MAX_PATH, (WIN32_FIND_DATA *)&wfd, SLGP_SHORTPATH );
					if (!SUCCEEDED(hres) )
					{
						// application-defined function
					}

					// Get the description of the target.
					hres = psl->GetDescription(szDescription, MAX_PATH);
					if (!SUCCEEDED(hres))
					{
						// HandleErr(hres);
					}
					lstrcpy(lpszPath, szGotPath);
				}
			}
		// Release the pointer to the IPersistFile interface.
		ppf->Release();
		}
	// Release the pointer to the IShellLink interface.
	psl->Release();
	}
	else
	{
	}
	CoUninitialize();
	return hres;
}

/*Pabs inserted func
Create link files for registry menu
Copied from compiler documentation. - had to fix up some stuff - totally wrong parameters used - all freaky compile time errors
CreateLink - uses the shell's IShellLink and IPersistFile interfaces to create and store a shortcut to the specified object.
Returns the result of calling the member functions of the interfaces.
lpszPathObj - address of a buffer containing the path of the object.
lpszPathLink - address of a buffer containing the path where the shell link is to be stored.*/
HRESULT CreateLink(LPCSTR lpszPathObj, LPCSTR lpszPathLink){
	HRESULT hres;
	IShellLink* psl;

	// Get a pointer to the IShellLink interface.

	CoInitialize( NULL );
	hres = CoCreateInstance(CLSID_ShellLink, NULL,
		CLSCTX_INPROC_SERVER, IID_IShellLink, (void **)&psl);
	if (SUCCEEDED(hres)) {
		IPersistFile* ppf;

		// Set the path to the shortcut target
		psl->SetPath(lpszPathObj);

		// Query IShellLink for the IPersistFile interface for saving the
		// shortcut in persistent storage.
		hres = psl->QueryInterface(IID_IPersistFile,(void **)&ppf);

		if (SUCCEEDED(hres)) {
			WCHAR wsz[MAX_PATH];

			//Bugfix - create the dir before saving the file because IPersistFile::Save won't
			hres = 1;
			char* tmp;
			*(tmp = PathFindFileName(lpszPathLink)-1) = 0;//Remove filename
			if(!PathIsDirectory(lpszPathLink))hres = CreateDirectory(lpszPathLink,NULL);
			*tmp = '\\';

			if (hres) {

				// Ensure that the string is ANSI.
				MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1,wsz, MAX_PATH);

				// Save the link by calling IPersistFile::Save.
				hres = ppf->Save(wsz, TRUE);

			}

			ppf->Release();
		}
		psl->Release();
	}
	CoUninitialize();
	return hres;
}

//Pabs inserted
//Parts copied from compiler docs - search for ITEMIDLIST in title in msdn
//Adapted from Q132750:"HOWTO: Convert a File Path to an ITEMIDLIST" in the Knowledge Base
char PathsEqual(const char* p0, const char* p1){
	LPITEMIDLIST pidl[2];
	LPSHELLFOLDER pFolder;
	OLECHAR olePath[2][MAX_PATH];
	ULONG chEaten;//only needed by parse dispname

	CoInitialize( NULL );
	if (SUCCEEDED(SHGetDesktopFolder(&pFolder)))
	{
		// IShellFolder::ParseDisplayName requires the file name be in Unicode.
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, p0, -1,olePath[0], MAX_PATH);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, p1, -1,olePath[1], MAX_PATH);

		// Convert the paths to ITEMIDLISTs.
		HRESULT hr = pFolder->ParseDisplayName(NULL,NULL,olePath[0],&chEaten,&pidl[0],NULL);
		if (FAILED(hr)){
			pFolder->Release();
			CoUninitialize();
			return -1;
		}
		hr = pFolder->ParseDisplayName(NULL,NULL,olePath[1],&chEaten,&pidl[1],NULL);
		if (FAILED(hr)){
			pFolder->Release();
			CoUninitialize();
			return -1;
		}
		hr = pFolder->CompareIDs(0,pidl[0],pidl[1]);

		//free ITEMIDLISTs
		IMalloc* pm;
		SHGetMalloc(&pm);
		pm->Free(pidl[0]);
		pm->Free(pidl[1]);
		pm->Release();

		pFolder->Release();
		CoUninitialize();
		return !hr;
	}
	CoUninitialize();
	return -1;
}

//-------------------------------------------------------------------
void HexEditorWindow::CMD_colors_to_default()
{//Pabs replaced NULL w hwnd
	if( MessageBox( hwnd, "Really reset colors to default values?", "frhed", MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{//end
		iBmkColor = RGB( 255, 0, 0 );
		iSelBkColorValue = RGB( 255, 255, 0 );
		iSelTextColorValue = RGB( 0, 0, 0 );
		iTextColorValue = RGB( 0, 0, 0 );
		iBkColorValue = RGB( 255, 255, 255 );
		iSepColorValue = RGB( 192, 192, 192 );
		save_ini_data();
		repaint();
	}
}


void HexEditorWindow::CMD_GotoDllExports()
{
	ULONG ulOffset, ulSize;
	if( GetDllExportNames( filename, &ulOffset, &ulSize ) )
	{
		bSelected = TRUE;
		iStartOfSelection = (int)ulOffset;
		iEndOfSelection = (int)(ulOffset + ulSize - 1);
		adjust_view_for_selection();
		repaint();
	}
}

void HexEditorWindow::CMD_GotoDllImports()
{
	ULONG ulOffset, ulSize;
	if( GetDllImportNames( filename, &ulOffset, &ulSize ) )
	{
		bSelected = TRUE;
		iStartOfSelection = (int)ulOffset;
		iEndOfSelection = (int)(ulOffset + ulSize - 1);
		adjust_view_for_selection();
		repaint();
	}
}

void HexEditorWindow::OnContextMenu( int xPos, int yPos )
{

	POINT p = {xPos,yPos};
	HWND w = WindowFromPoint(p);

	if (NO_FILE || hwnd != w)
		return;

	ScreenToClient(hwnd,&p);

	xPos = p.x;
	int log_column = xPos / cxChar + iHscrollPos;

	//Click on offsets
	if (log_column < iMaxOffsetLen + iByteSpace){
		if(!bAutoOffsetLen && iMinOffsetLen<=INT_MAX){
			iMinOffsetLen++;
			save_ini_data ();
			update_for_new_datasize ();
		}
	}
	else{
		HMENU hMenu = LoadMenu( hInstance, MAKEINTRESOURCE(IDR_CONTEXTMENU) );
		if( hMenu )
		{
			// You could use other menu indices based on context... if you like
			hMenu = GetSubMenu( hMenu, Drive->IsOpen() ? 1 : 0 );

			if( hMenu )
             {
            		    //"Move/Copy bytes" is allowed if read-only is disabled.
		    EnableMenuItem( hMenu, IDM_EDIT_MOVE_COPY, (!bReadOnly?MF_ENABLED:MF_GRAYED) );

		    //"Reverse bytes" is allowed if read-only is disabled.
		    EnableMenuItem( hMenu, IDM_EDIT_REVERSE, (!bReadOnly?MF_ENABLED:MF_GRAYED) );
    //end
		    // "Cut" is allowed if there is a selection or the caret is on a byte.
		    // It is not allowed in read-only mode.
		    if( ( bSelected || iCurByte <= DataArray.GetUpperBound() ) && !bReadOnly )
			    EnableMenuItem( hMenu, IDM_EDIT_CUT, MF_ENABLED );
		    else
			    EnableMenuItem( hMenu, IDM_EDIT_CUT, MF_GRAYED );

		    // "Copy" is allowed if there is a selection or the caret is on a byte.
		    if( bSelected || iCurByte <= DataArray.GetUpperBound() )
			    EnableMenuItem( hMenu, IDM_EDIT_COPY, MF_ENABLED );
		    else
			    EnableMenuItem( hMenu, IDM_EDIT_COPY, MF_GRAYED );

        		// "Paste" is allowed if the clipboard contains text,
		    // there is no selection going on and read-only is disabled.
		    if( !bReadOnly && !bSelected )
		    {
			    if( OpenClipboard( NULL ) )
			    {
    //Pabs changed to allow any data on clip to enable paste when there is no error
				    EnableMenuItem( hMenu, IDM_EDIT_PASTE, EnumClipboardFormats(0) ? MF_ENABLED : MF_GRAYED );
    //end
				    CloseClipboard ();
			    }
			    else
			    {
				    // Clipboard could not be opened => can't paste.
				    EnableMenuItem( hMenu, IDM_EDIT_PASTE, MF_GRAYED );
			    }
		    }
		    else
			    EnableMenuItem( hMenu, IDM_EDIT_PASTE, MF_GRAYED );


                TrackPopupMenu( hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, xPos, yPos, 0, hwnd, NULL );
            }

		}
	}
}

void HexEditorWindow::CMD_CloseDrive()
{
	Drive->Close();
	CMD_new();
}

void HexEditorWindow::CMD_DriveGotoFirstTrack()
{
	CurrentSectorNumber = 0;
	RefreshCurrentTrack();
}

void HexEditorWindow::CMD_DriveGotoNextTrack()
{
	if( CurrentSectorNumber < SelectedPartitionInfo->m_NumberOfSectors-1 )
		CurrentSectorNumber++;
	RefreshCurrentTrack();
}


void HexEditorWindow::CMD_DriveGotoPrevTrack()
{
	if( CurrentSectorNumber )
		CurrentSectorNumber--;
	RefreshCurrentTrack();
}

void HexEditorWindow::CMD_DriveGotoTrackNumber()
{
	if( GotoTrackDialog( hInstance, hwnd ) )
		RefreshCurrentTrack();
}

void HexEditorWindow::CMD_DriveSaveTrack()
{
	LPBYTE pTarget = Track.GetObjectMemory();
	unsigned char *pSource = (DataArray);
	CopyMemory(pTarget, pSource, Track.GetObjectSize());
	if( Drive->WriteAbsolute(Track.GetObjectMemory(), Track.GetObjectSize(), CurrentSectorNumber + SelectedPartitionInfo->m_StartingSector) )
	{
		m_iFileChanged = FALSE;
		bFilestatusChanged = TRUE;
		bFileNeverSaved = FALSE;
		bPartialOpen=TRUE;
		InvalidateRect (hwnd, NULL, FALSE);
		UpdateWindow(hwnd);
	}	
}

void HexEditorWindow::CMD_DriveGotoLastTrack()
{
	CurrentSectorNumber = SelectedPartitionInfo->m_NumberOfSectors-1;
	RefreshCurrentTrack();
}

void HexEditorWindow::RefreshCurrentTrack()
{
	if( Drive->ReadAbsolute(Track.GetObjectMemory(), Track.GetObjectSize(), CurrentSectorNumber + SelectedPartitionInfo->m_StartingSector) )
	{
		ULONG BytesPerSector = Track.GetObjectSize();
		DataArray.ClearAll();
		if (DataArray.SetSize (BytesPerSector) == TRUE)
		{
			DataArray.SetUpperBound(BytesPerSector-1);
			CopyMemory( DataArray, Track.GetObjectMemory(), BytesPerSector );
			bReadOnly = TRUE;
			sprintf(filename, "%s:Sector %I64d(0x%I64X)", (LPCSTR) SelectedPartitionInfo->GetNameAsString(), CurrentSectorNumber, CurrentSectorNumber);
			bFileNeverSaved = FALSE;
			iVscrollMax = 0;
			iVscrollPos = 0;
			iVscrollInc = 0;
			iHscrollMax = 0;
			iHscrollPos = 0;
			iHscrollInc = 0;
			iCurLine = 0;
			iCurByte = 0;
			iCurNibble = 0;
			m_iFileChanged = FALSE;
			bFilestatusChanged = TRUE;
			bFileNeverSaved = FALSE;
			bPartialOpen=TRUE;
			RECT r;
			GetClientRect (hwnd, &r);
			SendMessage (hwnd, WM_SIZE, 0, (r.bottom << 16) | r.right);
			InvalidateRect (hwnd, NULL, FALSE);
			UpdateWindow (hwnd);
		}
	}
}

void HexEditorWindow::CMD_OpenDrive()
{
	if( !GetDriveNameDialog( hInstance, hwnd ) )
		return;

	Drive->Close();
	Drive = PhysicalDrive;
	EnableDriveButtons(hwndToolBar, FALSE);

	if( !Drive->Open( SelectedPartitionInfo->m_dwDrive ) )
	{
		MessageBox( hwnd, "Unable to open drive", "Open Drive", MB_OK | MB_ICONERROR );
	}
	else
	{
		EnableDriveButtons(hwndToolBar, TRUE);
		if( Track.GetObjectSize() != SelectedPartitionInfo->m_dwBytesPerSector )
			Track.Create(SelectedPartitionInfo->m_dwBytesPerSector);

		CurrentSectorNumber = 0;
		RefreshCurrentTrack();
	}
	
}

void HexEditorWindow::CMD_NDAS_CRC32()
{
	if(512 != Track.GetObjectSize())
		return;

	*(ULONG *)&(DataArray[248]) = crc32_calc((unsigned char *)DataArray, 248);
	*(ULONG *)&(DataArray[252]) = crc32_calc((unsigned char *)DataArray + 256, 256);

	InvalidateRect (hwnd, NULL, FALSE);
	UpdateWindow (hwnd);
}

void HexEditorWindow::CMD_InfoNDAS()
{
	Drive = NDASDrive;
	if( !Drive->IsOpen())
		return;

	CHAR szBuf[5000];
	BOOL bResults = Drive->GetDriveLayout((UCHAR *)szBuf, 5000);

	if(!bResults)
		return;

	MessageBox(hwnd, szBuf, "NDAS Information", MB_ICONINFORMATION);
}

void HexEditorWindow::CMD_OpenNDAS()
{
	if( !GetAddressDialog( hInstance, hwnd ) )
		return;


	char *OpenParameter[10];
	OpenParameter[0] = (char *)&SelectedPartitionInfo;
	OpenParameter[1] = (char *)&NdasConnectionInfo;

	Drive->Close();
	Drive = NDASDrive;
	EnableDriveButtons(hwndToolBar, FALSE);

	if( !Drive->Open((int)OpenParameter))
	{
		MessageBox( hwnd, "Unable to open NDAS", "Open Drive", MB_OK | MB_ICONERROR );
	}
	else
	{
		EnableDriveButtons(hwndToolBar, TRUE);
		if( Track.GetObjectSize() != SelectedPartitionInfo->m_dwBytesPerSector )
			Track.Create(SelectedPartitionInfo->m_dwBytesPerSector);

		CurrentSectorNumber = 0;
		RefreshCurrentTrack();
	}
}

void HexEditorWindow::CMD_EncodeDecode()
{
	MEMORY_CODING mc;

	if( GetMemoryCoding( hInstance, hwnd, &mc, EncodeDlls ) )
	{
		if( bSelected )
		{
			mc.lpbMemory = &(DataArray[iStartOfSelection]);
			mc.dwSize = iEndOfSelection-iStartOfSelection;
		}
		else
		{
			mc.lpbMemory = DataArray;
			mc.dwSize = DataArray.GetLength();
		}
		mc.fpEncodeFunc(&mc);
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		repaint();
	}
}

//-------------------------------------------------------------------
// Find next occurance of the current findstring.
int HexEditorWindow::CMD_findnext()
{
	// If there is selected data then make it the data to find.
	if( bSelected )
	{
		// Get start/end offset and length of selection.
		int sel_start, select_len;
		if( iEndOfSelection < iStartOfSelection )
		{
			sel_start = iEndOfSelection;
			select_len = iStartOfSelection - iEndOfSelection + 1;
		}
		else
		{
			sel_start = iStartOfSelection;
			select_len = iEndOfSelection - iStartOfSelection + 1;
		}

		// iFindDlgLastLen = iFindDlgBufLen; // Remember old buffer length.
		// iFindDlgBufLen = byte_to_BC_destlen( (char*) &DataArray[sel_start], select_len ); // Get the length of the bytecode representation of the selection.
		// if( pcFindDlgBuffer != NULL ) // Delete old buffer, if necessary.
		//	delete [] pcFindDlgBuffer;
		// pcFindDlgBuffer = new char[iFindDlgBufLen]; // Allocate new buffer.
		translate_bytes_to_BC( pcFindDlgBuffer, &DataArray[sel_start], select_len ); // Translate the selection into bytecode and write it into the edit box buffer.
	}

	// Is there a findstring? (Initmenupopup actually filters this already).
	if( pcFindDlgBuffer != NULL )
	{
		// There is a findstring. Create its translation.
		char* pcFindstring;
		int srclen = strlen( pcFindDlgBuffer );
		int destlen = create_bc_translation( &pcFindstring, pcFindDlgBuffer, srclen, iCharacterSet, iBinaryMode );
		if( destlen > 0 )
		{
			int i;
			char (*cmp) (char);

			if( iFindDlgMatchCase == BST_CHECKED )
				cmp = equal;
			else
				cmp = lower_case;

			SetCursor( LoadCursor( NULL, IDC_WAIT ) );

			i = find_bytes( (char*) &(DataArray[iCurByte + 1]), DataArray.GetLength () - iCurByte - 1, pcFindstring, destlen, 1, cmp );
			if( i != -1 )
				iCurByte += i + 1;

			SetCursor( LoadCursor( NULL, IDC_ARROW ) );
			if( i != -1 )
			{
				/* OLD
				// Caret will be vertically centered if line of found string is not visible.
				if( iCurByte/iBytesPerLine < iCurLine || iCurByte/iBytesPerLine > iCurLine + cyBuffer )
					iCurLine = max( 0, iCurByte/iBytesPerLine-cyBuffer/2 );
				adjust_vscrollbar();
				*/

				bSelected = TRUE;
				iStartOfSelection = iCurByte;
				iEndOfSelection = iCurByte + destlen - 1;
				adjust_view_for_selection();
				repaint();
			}
			else
				MessageBox( hwnd, "Could not find any more occurances.", "Find next", MB_OK | MB_ICONERROR );

			delete [] pcFindstring;
		}
		else
//Pabs replaced message with CMD_find
		{
			iFindDlgDirection = 1;
			CMD_find();
		}
//end
	}
	else
	{
		//Can't call CMD_find cause it won't alloc a new buffer
		// There is no findstring.
		MessageBox( hwnd, "String to find not specified.", "Find next", MB_OK | MB_ICONERROR );
	}
	return 0;
}

//-------------------------------------------------------------------
// Find previous occurance of the current findstring.
int HexEditorWindow::CMD_findprev()
{
	// If there is selected data then make it the data to find.
	if( bSelected )
	{
		// Get start/end offset and length of selection.
		int sel_start, select_len;
		if( iEndOfSelection < iStartOfSelection )
		{
			sel_start = iEndOfSelection;
			select_len = iStartOfSelection - iEndOfSelection + 1;
		}
		else
		{
			sel_start = iStartOfSelection;
			select_len = iEndOfSelection - iStartOfSelection + 1;
		}

		// Remember old buffer length.
		// iFindDlgLastLen = iFindDlgBufLen;

		// Get the length of the bytecode representation of the selection.
		// iFindDlgBufLen = byte_to_BC_destlen( (char*) &DataArray[sel_start], select_len );

		// Delete old buffer, if necessary.
		// if( pcFindDlgBuffer != NULL )
		//	delete [] pcFindDlgBuffer;

		// Allocate new buffer.
		// pcFindDlgBuffer = new char[iFindDlgBufLen];

		// Translate the selection into bytecode and write it into the edit box buffer.
		translate_bytes_to_BC( pcFindDlgBuffer, &DataArray[sel_start], select_len );
		//GK16AUG2K
		iFindDlgUnicode = 0;
	}

	// Is there a findstring? (Initmenupopup actually filters this already).
	if( pcFindDlgBuffer != NULL )
	{
		// There is a findstring. Create its translation.
		char* pcFindstring;
		int srclen = strlen( pcFindDlgBuffer );
		int destlen = create_bc_translation( &pcFindstring, pcFindDlgBuffer, srclen, iCharacterSet, iBinaryMode );
		if( destlen > 0 )
		{
			int i;
			char (*cmp) (char);

			if( iFindDlgMatchCase == BST_CHECKED )
				cmp = equal;
			else
				cmp = lower_case;

			SetCursor( LoadCursor( NULL, IDC_WAIT ) );
			{
				// Search the array starting at index 0 to the current byte,
				// plus the findstring-length minus 1. If
				// you are somewhere in the middle of the findstring with the caret
				// and you choose "find previous" you usually want to find the beginning
				// of the findstring in the file.
				i = find_bytes( (char*) &(DataArray[0]),
					min( iCurByte + (destlen - 1), DataArray.GetLength() ),
					pcFindstring, destlen, -1, cmp );
				if (i != -1)
					iCurByte = i;
			}
			SetCursor( LoadCursor( NULL, IDC_ARROW ) );
			if( i != -1 )
			{
				/* OLD
				// Caret will be vertically centered if line of found string is not visible.
				if( iCurByte/iBytesPerLine < iCurLine || iCurByte/iBytesPerLine > iCurLine + cyBuffer )
					iCurLine = max( 0, iCurByte/iBytesPerLine-cyBuffer/2 );
				adjust_vscrollbar();
				*/

				bSelected = TRUE;
				iStartOfSelection = iCurByte;
				iEndOfSelection = iCurByte + destlen - 1;
				adjust_view_for_selection();
				repaint();
			}
			else
				MessageBox( hwnd, "Could not find any more occurances.", "Find previous", MB_OK | MB_ICONERROR );
			delete [] pcFindstring;
		}
		else
//Pabs replaced message with CMD_find
		{
			iFindDlgDirection = -1;
			CMD_find();
		}
//end
	}
	else
	{
		// There is no findstring.
		MessageBox( hwnd, "String to find not specified.", "Find previous", MB_OK | MB_ICONERROR );
	}
	return 0;
}

//-------------------------------------------------------------------
// Handler for the "Open in text editor" command.
int HexEditorWindow::CMD_summon_text_edit()
{
	if( filename != NULL )
	{
//Pabs inserted
		if(m_iFileChanged){
			if(IDYES == MessageBox (hwnd, "Do you want to save your changes?", "Open in text editor", MB_YESNO | MB_ICONQUESTION)){
				CMD_save();
			}
		}
//end
		HINSTANCE hi = ShellExecute( hwnd, "open", TexteditorName, filename, NULL, SW_SHOWNORMAL );
		if( (int) hi <= 32 )
			MessageBox( hwnd, "An error occured when calling the text editor.", "Open in text editor", MB_OK | MB_ICONERROR );
	}
	else
	{
		MessageBox( hwnd, "Filename is NULL.", "Open in text editor", MB_OK | MB_ICONERROR );
	}
	return 0;
}

//-------------------------------------------------------------------
// Process and route all window messages.
int HexEditorWindow::OnWndMsg( HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam )
{

	// First decide if bInsertingHex mode has to be turned off.
	// It might have been set to true in HexEditorWindow::character().
	if( bInsertingHex )
	{
		switch( iMsg )
		{
			case WM_SIZE: case WM_KILLFOCUS: case WM_LBUTTONDOWN:
			case WM_LBUTTONUP: case WM_VSCROLL:
			case WM_HSCROLL: case WM_COMMAND: case WM_DROPFILES:
			case WM_CLOSE: case WM_DESTROY:
				bInsertingHex = FALSE;
				break;

			case WM_KEYDOWN:
				switch( wParam )
				{
				case VK_END:
				case VK_HOME:
				case VK_LEFT:
				case VK_RIGHT:
				case VK_UP:
				case VK_DOWN:
				case VK_PRIOR:
				case VK_NEXT:
					bInsertingHex = FALSE;
					break;

				default:
					break;
				}

			default:
				break;
		}
	}

	switch( iMsg )
	{
	case WM_CREATE:
		at_window_create( hwnd, hMainInstance );
		return 0;

	case WM_SIZE:
		resize_window( LOWORD( lParam ), HIWORD( lParam ) );
		return 0;

	case WM_SETFOCUS:
		set_focus();
		return 0;

	case WM_KILLFOCUS:
		kill_focus();
		return 0;

	case WM_LBUTTONDOWN:
		lbuttondown( wParam, LOWORD( lParam ), HIWORD( lParam ) );
		return 0;

	case WM_LBUTTONUP:
		lbuttonup( LOWORD( lParam ), HIWORD( lParam ) );
		return 0;

	// GK20AUG2K
	case WM_CONTEXTMENU:
		OnContextMenu( LOWORD(lParam), HIWORD(lParam) );
		return 0L;

	case WM_MOUSEMOVE:
		mousemove( (int) (short) LOWORD( lParam ), (int) (short) HIWORD( lParam ) );
		return 0;

	case WM_KEYDOWN:
		keydown( wParam );
		return 0;

	case WM_CHAR:
		character( (char) wParam );
		return 0;

	case WM_VSCROLL:
//Pabs inserted to allow 32-bit scrolling
		SI.fMask = SIF_TRACKPOS;
		GetScrollInfo(hwnd,SB_VERT,&SI);
//end
		vscroll( LOWORD( wParam ), SI.nTrackPos );//Pabs replaced "HIWORD( wParam )" with si.nTrackPos
		return 0;

	case WM_HSCROLL:
//Pabs inserted to allow 32-bit scrolling
		SI.fMask = SIF_TRACKPOS;
		GetScrollInfo(hwnd,SB_HORZ,&SI);
//end
		hscroll( LOWORD( wParam ), SI.nTrackPos );//Pabs replaced "HIWORD( wParam )" with si.nTrackPos
		return 0;

	case WM_PAINT:
		paint();
		return 0;

	case WM_COMMAND:
		command( LOWORD( wParam ) );
		return 0;

	case WM_TIMER:
		timer( wParam, lParam );
		return 0;

	case WM_DROPFILES:
		dropfiles( (HANDLE) wParam ); // handle of internal drop structure.
		return 0;

	case WM_CLOSE:
		close();
		return 0;

	case WM_DESTROY:
		destroy_window();
		PostQuitMessage( 0 );
		return 0;
	}
	return DefWindowProc (hwnd, iMsg, wParam, lParam);
}

//Pabs changed - removed CMD_explorersettings

//-------------------------------------------------------------------
// Create a text representation of an array of bytes and save it in
// a SimpleString object.
int	HexEditorWindow::transl_binary_to_text( SimpleString& dest, char* src, int len )
{
	UNREFERENCED_PARAMETER( dest );
	// How long will the text representation of array of bytes be?
	int destlen = byte_to_BC_destlen( src, len );
	strToReplaceData.SetSize( destlen );
	strToReplaceData.ExpandToSize();
	if( (char*) strToReplaceData != NULL )
	{
		translate_bytes_to_BC( (char*) strToReplaceData, (unsigned char*) src, len );
		return TRUE;
	}
	else
		return FALSE;
}

//-------------------------------------------------------------------
void HexEditorWindow::CMD_replace()
{
	// If there is selected data then make it the data to find.
	if( bSelected )
	{
		int sel_start, select_len;
		if( iEndOfSelection < iStartOfSelection )
		{
			sel_start = iEndOfSelection;
			select_len = iStartOfSelection - iEndOfSelection + 1;
		}
		else
		{
			sel_start = iStartOfSelection;
			select_len = iEndOfSelection - iStartOfSelection + 1;
		}

		if( transl_binary_to_text( strToReplaceData, (char*) &DataArray[sel_start], select_len ) )
		{
		}
		else
		{//Pabs replaced NULL w hwnd
			MessageBox( hwnd, "Could not use selection as replace target.", "Replace", MB_OK );
			return;
		}
	}

	// Open the dialogue box.
	// While the dialogue box is opened don't mark the current position.
	bDontMarkCurrentPos = TRUE;
	if( DialogBox( hInstance, MAKEINTRESOURCE(IDD_REPLACEDIALOG), hwnd, (DLGPROC) ReplaceDlgProc ) )
	{
	}
	bDontMarkCurrentPos = FALSE;
}

//-------------------------------------------------------------------
BOOL CALLBACK ReplaceDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	switch (iMsg)
	{
	case WM_INITDIALOG:
		if( iPasteAsText )
			CheckDlgButton (hDlg, IDC_USETRANSLATION_CHECK, BST_UNCHECKED);
		else
			CheckDlgButton (hDlg, IDC_USETRANSLATION_CHECK, BST_CHECKED);
		if( strToReplaceData.StrLen() != 0 )
			SetWindowText( GetDlgItem ( hDlg, IDC_TO_REPLACE_EDIT ), strToReplaceData );
		SetFocus( GetDlgItem( hDlg, IDC_TO_REPLACE_EDIT ) );
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD (wParam))
		{
		case IDOK:
			EndDialog (hDlg, 0);
			return TRUE;

		case IDCANCEL:
			{
				if( IsDlgButtonChecked( hDlg, IDC_USETRANSLATION_CHECK ) == BST_CHECKED )
					iPasteAsText = FALSE;
				else
					iPasteAsText = TRUE;
				EndDialog (hDlg, 0);
				return TRUE;
			}

		case IDC_FINDPREVIOUS_BUTTON:
			{
				char (*cmp) (char);
				if( IsDlgButtonChecked( hDlg, IDC_MATCHCASE_CHECK ) == BST_CHECKED )
					cmp = equal;
				else
					cmp = lower_case;

				int buflen = SendMessage( GetDlgItem (hDlg, IDC_TO_REPLACE_EDIT), EM_GETLIMITTEXT, 0, 0 );
				char* buf = new char[ buflen ];
				GetDlgItemText( hDlg, IDC_TO_REPLACE_EDIT, buf, buflen );
				strToReplaceData = buf;
				if( ! hexwnd.find_and_select_data( strToReplaceData, -1, TRUE, cmp ) )
					MessageBox( hDlg, "Could not find data.", "Replace/Find backward", MB_OK | MB_ICONERROR );
				delete [] buf;
			}
			break;

		case IDC_FINDNEXT_BUTTON:
			{
				char (*cmp) (char);
				if( IsDlgButtonChecked( hDlg, IDC_MATCHCASE_CHECK ) == BST_CHECKED )
					cmp = equal;
				else
					cmp = lower_case;

				int buflen = SendMessage( GetDlgItem (hDlg, IDC_TO_REPLACE_EDIT), EM_GETLIMITTEXT, 0, 0 );
				char* buf = new char[ buflen ];
				GetDlgItemText( hDlg, IDC_TO_REPLACE_EDIT, buf, buflen );
				strToReplaceData = buf;
				if( ! hexwnd.find_and_select_data( strToReplaceData, 1, TRUE, cmp ) )
				{
					MessageBox( hDlg, "Could not find data.", "Replace/Find forward", MB_OK | MB_ICONERROR );
					break;
				}
				delete [] buf;
			}
			break;

		// Replace all following occurances of the findstring.
		case IDC_FOLLOCC_BUTTON:
			{
				char (*cmp) (char);
				if( IsDlgButtonChecked( hDlg, IDC_MATCHCASE_CHECK ) == BST_CHECKED )
					cmp = equal;
				else
					cmp = lower_case;

				// Replace all following occurances of the findstring.
				int occ_num = 0;

				int buflen = SendMessage( GetDlgItem (hDlg, IDC_TO_REPLACE_EDIT), EM_GETLIMITTEXT, 0, 0 );
				char* buf = new char[ buflen ];
				if( buf != NULL )
					GetDlgItemText( hDlg, IDC_TO_REPLACE_EDIT, buf, buflen );
				else
				{
					MessageBox( hDlg, "Could not store data to replace.", "Replace/Replace all following", MB_OK | MB_ICONERROR );
					break;
				}
				strToReplaceData = buf;
				delete [] buf;

				buflen = SendMessage( GetDlgItem( hDlg, IDC_REPLACEWITH_EDIT ), EM_GETLIMITTEXT, 0, 0 );
				buf = new char[ buflen ];
				if( buf != NULL )
					GetDlgItemText( hDlg, IDC_REPLACEWITH_EDIT, buf, buflen );
				else
				{
					MessageBox( hDlg, "Could not store data to replace with.", "Replace/Replace all following", MB_OK | MB_ICONERROR );
					break;
				}
				strReplaceWithData = buf;
				delete [] buf;

				if( IsDlgButtonChecked( hDlg, IDC_USETRANSLATION_CHECK ) == BST_CHECKED )
					iPasteAsText = FALSE;
				else
					iPasteAsText = TRUE;

				//------------------

				// Don't do anything if to-replace and replace-with data are same.
				Text2BinTranslator tr_find( strToReplaceData ), tr_replace( strReplaceWithData );
				if( tr_find.bCompareBin( tr_replace, hexwnd.iGetCharMode(), hexwnd.iGetBinMode() ) )
				{
					MessageBox( hDlg, "To-replace and replace-with data are same.", "Replace all following occurances", MB_OK | MB_ICONERROR );
					break;
				}

				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				for(;;){
					// Replace
					while( hexwnd.select_if_found_on_current_pos( strToReplaceData, 1, FALSE, cmp ) )
					{
						// Found find-data on current caret position.
						hexwnd.replace_selected_data( strReplaceWithData, FALSE );
						occ_num++;
					}

					if( ! hexwnd.find_and_select_data( strToReplaceData, 1, FALSE, cmp ) )
					{
						break;
					}
					occ_num++;
					hexwnd.replace_selected_data( strReplaceWithData, FALSE );
				};
				SetCursor( LoadCursor( NULL, IDC_ARROW ) );

				char tbuf[32];
				sprintf( tbuf, "%d occurances replaced.", occ_num );
				MessageBox( hDlg, tbuf, "Replace/Replace all following", MB_OK | MB_ICONERROR );

				hexwnd.set_wnd_title();
				hexwnd.adjust_view_for_selection();
				hexwnd.update_for_new_datasize();
				hexwnd.repaint();
			}
			break;

		case IDC_PREVOCC_BUTTON:
			{
				// Replace all previous occurances of the findstring.
				char (*cmp) (char);
				if( IsDlgButtonChecked( hDlg, IDC_MATCHCASE_CHECK ) == BST_CHECKED )
					cmp = equal;
				else
					cmp = lower_case;

				int occ_num = 0;

				int buflen = SendMessage( GetDlgItem (hDlg, IDC_TO_REPLACE_EDIT), EM_GETLIMITTEXT, 0, 0 );
				char* buf = new char[ buflen ];
				if( buf != NULL )
					GetDlgItemText( hDlg, IDC_TO_REPLACE_EDIT, buf, buflen );
				else
				{
					MessageBox( hDlg, "Could not store data to replace.", "Replace/Replace all following", MB_OK | MB_ICONERROR );
					break;
				}
				strToReplaceData = buf;
				delete [] buf;

				buflen = SendMessage( GetDlgItem( hDlg, IDC_REPLACEWITH_EDIT ), EM_GETLIMITTEXT, 0, 0 );
				buf = new char[ buflen ];
				if( buf != NULL )
					GetDlgItemText( hDlg, IDC_REPLACEWITH_EDIT, buf, buflen );
				else
				{
					MessageBox( hDlg, "Could not store data to replace with.", "Replace/Replace all following", MB_OK | MB_ICONERROR );
					break;
				}
				strReplaceWithData = buf;
				delete [] buf;

				if( IsDlgButtonChecked( hDlg, IDC_USETRANSLATION_CHECK ) == BST_CHECKED )
					iPasteAsText = FALSE;
				else
					iPasteAsText = TRUE;

				// Don't do anything if to-replace and replace-with data are same.
				Text2BinTranslator tr_find( strToReplaceData ), tr_replace( strReplaceWithData );
				if( tr_find.bCompareBin( tr_replace, hexwnd.iGetCharMode(), hexwnd.iGetBinMode() ) )
				{
					MessageBox( hDlg, "To-replace and replace-with data are same.", "Replace all following occurances", MB_OK | MB_ICONERROR );
					break;
				}

				SetCursor( LoadCursor( NULL, IDC_WAIT ) );
				while( hexwnd.find_and_select_data( strToReplaceData, -1, FALSE, cmp ) ){
					occ_num++;
					hexwnd.replace_selected_data( strReplaceWithData, FALSE );
				};
				SetCursor( LoadCursor( NULL, IDC_ARROW ) );

				char tbuf[32];
				sprintf( tbuf, "%d occurances replaced.", occ_num );
				MessageBox( hDlg, tbuf, "Replace/Replace all following", MB_OK | MB_ICONERROR );

				hexwnd.set_wnd_title();
				hexwnd.adjust_view_for_selection();
				hexwnd.update_for_new_datasize();
				hexwnd.repaint();
			}
			break;

		case IDC_REPLACE_BUTTON:
			{
				if( IsDlgButtonChecked( hDlg, IDC_USETRANSLATION_CHECK ) == BST_CHECKED )
					iPasteAsText = FALSE;
				else
					iPasteAsText = TRUE;
				int buflen = SendMessage( GetDlgItem (hDlg, IDC_TO_REPLACE_EDIT), EM_GETLIMITTEXT, 0, 0 );
				char* buf = new char[ buflen ];
				if( buf != NULL )
				{
					if( GetDlgItemText( hDlg, IDC_TO_REPLACE_EDIT, buf, buflen ) != 0)
					{
						strToReplaceData = buf;
						delete [] buf;
						if( (char*) strToReplaceData != NULL )
						{
							buflen = SendMessage( GetDlgItem( hDlg, IDC_REPLACEWITH_EDIT ), EM_GETLIMITTEXT, 0, 0 );
							buf = new char[ buflen ];
							GetDlgItemText( hDlg, IDC_REPLACEWITH_EDIT, buf, buflen );
							strReplaceWithData = buf;
							delete [] buf;
							hexwnd.replace_selected_data( strReplaceWithData );
						}
						else
						{
							MessageBox( hDlg, "Could not store data to replace.", "Replace", MB_OK | MB_ICONERROR );
						}
						break;
					}
					else
					{
						delete [] buf;
						MessageBox( hDlg, "Could not read out data to replace.", "Replace", MB_OK | MB_ICONERROR );
					}
				}
				else
				{
					MessageBox( hDlg, "Could not allocate memory for data to replace.", "Replace", MB_OK | MB_ICONERROR );
				}
			}
			break;
		}
		break;
	}
	return FALSE;
}

//-------------------------------------------------------------------
int HexEditorWindow::find_and_select_data( SimpleString& finddata, int finddir, int do_repaint, char (*cmp) (char) )
{
	char* tofind;
	int destlen = create_bc_translation( &tofind,
		(char*) finddata,
		finddata.StrLen(),
		iCharacterSet,
		iBinaryMode
		);

	int i;
	if( finddir == 1 )
	{
		i = find_bytes( (char*) &(DataArray[iCurByte + 1]),
				DataArray.GetLength() - iCurByte - 1,
				tofind,	destlen, 1, cmp );
		if( i != -1 )
			iCurByte += i + 1;
	}
	else
	{
		i = find_bytes( (char*) &(DataArray[0]),
					min( iCurByte + (destlen - 1), DataArray.GetLength() ),
					tofind, destlen, -1, cmp );
		if( i != -1 )
			iCurByte = i;
	}

	if( i != -1 )
	{
		// NEW: Select found interval.
		bSelected = TRUE;
		iStartOfSelection = iCurByte;
		iEndOfSelection = iCurByte + destlen - 1;
		if( do_repaint )
		{
			adjust_view_for_selection();
			repaint();
		}
	}
	else
	{
		if( tofind != NULL )
			delete [] tofind;
		return FALSE;
	}

	if( tofind != NULL )
		delete [] tofind;
	return TRUE;
}

//-------------------------------------------------------------------
// SimpleString replacedata contains data to replace with.
int HexEditorWindow::replace_selected_data( SimpleString& replacedata, int do_repaint )
{
	if( bSelected )
	{
		if( replacedata.IsEmpty() )
		{
			// Selected data is to be deleted, since replace-with data is empty string.
			if( DataArray.Replace( iGetStartOfSelection(),
				iGetEndOfSelection() - iGetStartOfSelection() + 1,
				NULL, 0	) == TRUE )
			{
				bSelected = FALSE;
				bFilestatusChanged = TRUE;
				m_iFileChanged = TRUE;
				iCurByte = iStartOfSelection;
				if( do_repaint )
				{
					update_for_new_datasize();
					repaint();
				}
				return TRUE;
			}
			else
			{//Pabs replaced NULL w hwnd
				MessageBox( hwnd, "Could not delete selected data.", "Replace", MB_OK | MB_ICONERROR );
				return FALSE;
			}//end
		}

		// Replace with non-zero-length data.
		if( iPasteAsText )
		{
			int a = iGetStartOfSelection(), b = iGetEndOfSelection();
			if( DataArray.Replace( a, b - a + 1,
				(unsigned char*) (char*) replacedata, replacedata.StrLen()
				) == TRUE )
			{
				iEndOfSelection = iStartOfSelection + replacedata.StrLen() - 1;
				if( do_repaint )
				{
					update_for_new_datasize();
					repaint();
				}
				bFilestatusChanged = TRUE;
				m_iFileChanged = TRUE;
				return TRUE;
			}
			else
			{//Pabs replaced NULL w hwnd
				MessageBox( hwnd, "Replacing failed.", "Replace", MB_OK | MB_ICONERROR );
				return FALSE;
			}//end
		}
		else
		{
			// Input string contains special-syntax-coded binary data.
			SimpleArray<char> out;
			if( transl_text_to_binary( replacedata, out ) == TRUE )
			{
				int a = iGetStartOfSelection(), b = iGetEndOfSelection();
				if( DataArray.Replace( a, b - a + 1,
					(unsigned char*) (char*) out, out.GetLength()
					) == TRUE )
				{
					bFilestatusChanged = TRUE;
					m_iFileChanged = TRUE;
					iEndOfSelection = iStartOfSelection + out.GetLength() - 1;
					if( do_repaint )
					{
						update_for_new_datasize();
						repaint();
					}
					return TRUE;
				}
				else
				{//Pabs replaced NULL w hwnd
					MessageBox( hwnd, "Replacing failed.", "Replace", MB_OK | MB_ICONERROR );
					return FALSE;
				}
			}
			else
			{
				MessageBox( hwnd, "Could not translate text to binary.", "Replace", MB_OK | MB_ICONERROR );
				return FALSE;
			}
		}
	}
	else
	{
		MessageBox( hwnd, "Data to replace must be selected.", "Replace", MB_OK | MB_ICONERROR );
	}//end
	return FALSE;
}

//-------------------------------------------------------------------
// Translate the text in the string to binary data and store in the array.
int HexEditorWindow::transl_text_to_binary( SimpleString& in, SimpleArray<char>& out )
{
	char* pcOut;
	int destlen = create_bc_translation( &pcOut,
		(char*) in,
		in.StrLen(),
		iCharacterSet,
		iBinaryMode
		);

	if( destlen != 0 )
	{
		out.Adopt( pcOut, destlen - 1, destlen );
		return TRUE;
	}
	else
	{
		// The string was empty.

	}
	return FALSE;
}

//-------------------------------------------------------------------
// Following code by Pabs.

//see CMD_fw below
static unsigned char input(const int& index){
	return buf[index];
}

//see CMD_fw below
unsigned char file(const int& index){
	unsigned char x;
	_lseek(FWFile, index, SEEK_SET);
	_read(FWFile,&x,1);
	return x;
}
char aa=0;
//convert a string of hex digits to a string of chars
void hexstring2charstring(){
	// RK: removed definition of variable "a".
	char* pcTemp;//needed so sscanf starts in the right place
	aa=0;
	int i,ii=strlen(pcFWText);
	if(ii%2){//if number of hex digits is odd
		//concatenate them
		for(i=0;i<ii;i++)pcFWText[ii+i]=pcFWText[i];
		pcFWText[ii*2]=0;aa=1;
	}
	for(i=ii=0;pcFWText[i]!='\0';i+=2){
		pcTemp=&pcFWText[i];//update start pos

		// RK: next two lines changed, would crash when compiled with VC++ 4.0.
		/*
		sscanf(pcTemp,"%2x",&a);//get byte from the hexstring
		buf[ii]=a;//store it
		*/
		// Replaced with this line:
		sscanf(pcTemp,"%2x",&buf[ii]);//get byte from the hexstring

		ii++;
	}//for
	buflen=ii;//store length
/*	pcFWText[(aa?ii:ii*2)]=*/buf[ii]='\0';//terminate strings so they are good for use
//	^
//	|
//	access violation so i do it in the dlgproc
}//func

//used to delete non-hex chars after the user pastes into the hexbox
void deletenonhex(HWND hEd){
	GetWindowText(hEd,pcFWText,FW_MAX);
	int ii=0;
	for(int i =0;pcFWText[i]!='\0';i++){
		if(isxdigit(pcFWText[i])){pcFWText[ii]=pcFWText[i];ii++;}
	}
	pcFWText[ii]='\0';
	SetWindowText(hEd,pcFWText);
}

void HexEditorWindow::CMD_revert(){

	if( bFileNeverSaved ){
		m_iFileChanged = FALSE;
		CMD_new();
		return;
	}

	char tmp = 0;

	if (bPartialOpen){
		int filehandle;
		if ((filehandle = _open (filename,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1){
			_lseek (filehandle, iPartialOffset, 0);
			if ( DataArray.SetSize( iPartialOpenLen ) ){
				if (_read (filehandle, &DataArray[0], iPartialOpenLen ) != -1)
					tmp = 1;
			}
			_close (filehandle);
		}
	}

	if ( bPartialOpen ? tmp : load_file(filename) ){
		// If the file is read only:
		if( -1== _access(filename,02) )
			bReadOnly = TRUE;
		else
			bReadOnly = FALSE;

		iVscrollMax = iVscrollPos = iVscrollInc = iHscrollMax = iHscrollPos =
		iHscrollInc = iCurLine = iCurByte = iCurNibble = 0;
		m_iFileChanged = FALSE;
		bFilestatusChanged = TRUE;
		bFileNeverSaved = FALSE;
		bSelected = FALSE;
		RECT r;
		GetClientRect (hwnd, &r);
		SendMessage (hwnd, WM_SIZE, 0, (r.bottom << 16) | r.right);
		InvalidateRect (hwnd, NULL, FALSE);
		UpdateWindow (hwnd);
	}
}

//fill with command
void HexEditorWindow::CMD_fw(){
	if(bSelected){ iStartOfSelSetting=iStartOfSelection; iEndOfSelSetting=iEndOfSelection; }
	else{ iStartOfSelSetting=0; iEndOfSelSetting=DataArray.GetUpperBound(); }
	//dlgproc opens file or fills buffer from user input
	if (DialogBox(hInstance, MAKEINTRESOURCE (IDD_FILL_WITH), hwnd, (DLGPROC) FillWithDlgProc)){
		SetCursor(LoadCursor(NULL,IDC_WAIT));
		unsigned char (*fnc)(const int&);
		long i,ii,iimax;
		if (curtyp){//1-file
			fnc=file;
			iimax = FWFilelen;
		}//if
		else {//0-input
			fnc=input;
			iimax=buflen;
		}//else
		if (iStartOfSelSetting>iEndOfSelSetting)swap(iStartOfSelSetting,iEndOfSelSetting);//make sure start<=end
		switch(asstyp){// use switch instead of pointers to funcs that just call an operator as its faster
			case 0:{
				for(ii=0,i=iStartOfSelSetting;i<=iEndOfSelSetting;i++){
					DataArray[(int)i]=fnc(ii);
					ii++;
					ii%=iimax;
				}//for
			} break;
			case 1:{
				for(ii=0,i=iStartOfSelSetting;i<=iEndOfSelSetting;i++){
					DataArray[(int)i]|=fnc(ii);
					ii++;
					ii%=iimax;
				}//for
			} break;
			case 2:{
				for(ii=0,i=iStartOfSelSetting;i<=iEndOfSelSetting;i++){
					DataArray[(int)i]&=fnc(ii);
					ii++;
					ii%=iimax;
				}//for
			} break;
			case 3:{
				for(ii=0,i=iStartOfSelSetting;i<=iEndOfSelSetting;i++){
					DataArray[(int)i]^=fnc(ii);
					ii++;
					ii%=iimax;
				}//for
			} break;
		}
		if (curtyp) _close(FWFile);//close file
		m_iFileChanged = TRUE;//mark as changed
		bFilestatusChanged = TRUE;
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		repaint ();//you tell me
	}//if dlgbox
}

//hex box msg handler
LRESULT CALLBACK HexProc(HWND hEdit, UINT iMsg, WPARAM wParam, LPARAM lParam){
	char a;
	LONG i=0;
	if(iMsg==WM_CHAR){
		a=(TCHAR) wParam;//only enter chars if they are hex digits or backspace
		// RK: Changed parameter 1, was "(WNDPROC)oldproc", in next 3 CallWindowProc calls.
		if(isxdigit(a)||(a==0x8))return CallWindowProc( (WNDPROC) oldproc, hEdit, iMsg, wParam, lParam);
		else {MessageBeep(MB_ICONEXCLAMATION);return 0L;}
	}
	else if (iMsg==WM_PASTE){
		i= CallWindowProc((WNDPROC)oldproc, hEdit, iMsg, wParam, lParam);//paste as usual
		deletenonhex(hEdit);//but delete non-hex chars
	}
	else return CallWindowProc((WNDPROC)oldproc, hEdit, iMsg, wParam, lParam);//use default proc otherwise
	return i;
}

//init stuff
void inittxt(HWND hDlg){
	if (curtyp){//1-file
		SetDlgItemText(hDlg,IDC_SI, "???");
		SetDlgItemText(hDlg,IDC_IFS, "???");
		SetDlgItemText(hDlg,IDC_R, "???");
	}
	else{//0-input
		char bufff[250];
		sprintf(bufff,"%d=0x%x",buflen,buflen);
		SetDlgItemText(hDlg,IDC_SI, bufff);
		if(buflen){
			int tteemmpp=(1+abs(iStartOfSelSetting-iEndOfSelSetting))/buflen;
			sprintf(bufff,"%d=0x%x",tteemmpp,tteemmpp);SetDlgItemText(hDlg,IDC_IFS, bufff);
			SendDlgItemMessage(hDlg,IDC_IFS,WM_SETFONT,(WPARAM) hfdef,MAKELPARAM(TRUE, 0));
			tteemmpp=(1+abs(iStartOfSelSetting-iEndOfSelSetting))%buflen;
			sprintf(bufff,"%d=0x%x",tteemmpp,tteemmpp);SetDlgItemText(hDlg,IDC_R, bufff);
		}
		else{
			SetDlgItemText(hDlg,IDC_IFS, "\xa5");//set to infinity symbol
			SendDlgItemMessage(hDlg,IDC_IFS,WM_SETFONT,(WPARAM) hfon,MAKELPARAM(TRUE, 0));
			SetDlgItemText(hDlg,IDC_R, "0=0x0");
		}
	}
}
//fillwithdlgbox msg handler
BOOL CALLBACK FillWithDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam){
	UNREFERENCED_PARAMETER( lParam );
	switch (iMsg){
		case WM_INITDIALOG:{
			HWND hEditt=GetDlgItem(hDlg, IDC_HEX);//get the handle to the hex edit box
			SendMessage(hEditt,EM_SETLIMITTEXT, (WPARAM) FW_MAX,0);//limit the amount of text the user can enter
			SetWindowText(hEditt, pcFWText);//init hex text
			SetFocus(hEditt);//give the hex box focus
			EnableWindow(hEditt,!curtyp);
			oldproc = GetWindowLong (hEditt, GWL_WNDPROC);//save the old proc for use in the new proc
			SetWindowLong(hEditt,GWL_WNDPROC,(LONG)HexProc);//override the old proc to be HexProc
			EnableWindow(GetDlgItem(hDlg, IDC_HEXSTAT),!curtyp);

			HWND typ = GetDlgItem(hDlg, IDC_TYPE);
			SendMessage(typ,CB_ADDSTRING ,0,(LPARAM) (LPCTSTR) "Input");
			SendMessage(typ,CB_ADDSTRING ,0,(LPARAM) (LPCTSTR) "File");
			SendMessage(typ,CB_SETCURSEL,(WPARAM)curtyp,0);//set cursel to previous

			//en/disable filename box and browse button
			HWND fn=GetDlgItem(hDlg, IDC_FN);
			SetWindowText(fn,szFWFileName);
			EnableWindow(fn,curtyp);
			EnableWindow(GetDlgItem(hDlg, IDC_BROWSE),curtyp);
			EnableWindow(GetDlgItem(hDlg, IDC_FILESTAT),curtyp);

			//init all the readonly boxes down below
			char bufff[250];
			int tteemmpp=1+abs(iStartOfSelSetting-iEndOfSelSetting);
			sprintf(bufff,"%d=0x%x",iStartOfSelSetting,iStartOfSelSetting);
			SetDlgItemText(hDlg, IDC_STS,bufff);
			sprintf(bufff,"%d=0x%x",iEndOfSelSetting,iEndOfSelSetting);
			SetDlgItemText(hDlg, IDC_ES,bufff);
			sprintf(bufff,"%d=0x%x",tteemmpp,tteemmpp);
			SetDlgItemText(hDlg, IDC_SS,bufff);
			hfdef = (HFONT) SendDlgItemMessage(hDlg,IDC_R,WM_GETFONT,0,0);
			if((hfon = CreateFont(16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH|FF_DONTCARE,"Symbol"))==NULL)PostQuitMessage(1);
			inittxt(hDlg);
			switch(asstyp){
				case 0:CheckDlgButton (hDlg, IDC_EQ, BST_CHECKED);break;
				case 1:CheckDlgButton (hDlg, IDC_OR, BST_CHECKED);break;
				case 2:CheckDlgButton (hDlg, IDC_AND, BST_CHECKED);break;
				case 3:CheckDlgButton (hDlg, IDC_XOR, BST_CHECKED);break;
			}
			return 0;//stop the system from setting focus to the control handle in (HWND) wParam because we already set focus above
		}
		break;
		case WM_COMMAND:{
			switch (LOWORD (wParam)){
				case IDOK:{//ok pressed
					int i;
					if(curtyp){//1-file
						GetDlgItemText(hDlg,IDC_FN,szFWFileName,_MAX_PATH);//get file name
						if((FWFile=_open(szFWFileName,_O_RDONLY|_O_BINARY))==-1){//if there is error opening
							MessageBox(hDlg,"Error opening file","Error", MB_OK | MB_ICONERROR);//tell user but don't close dlgbox
							return 1;//didn't process this message
						}//if
						if((FWFilelen=_filelength(FWFile))==0){//if filelen is zero
							MessageBox (hDlg, "Can't fill a selection with a file of zero size.", "Error", MB_OK | MB_ICONERROR);//tell user but don't close dlgbox
							_close(FWFile);//close file
							return 1;//didn't process this message
						}//if
						else if (FWFilelen==-1){//error returned by _filelength
							MessageBox(hDlg,"Error opening file","Error", MB_OK | MB_ICONERROR);//tell user but don't close dlgbox
							_close(FWFile);//close file
							return 1;//didn't process this message
						}//elseif
					}
					else{//0-input
						if (!buflen){//no hex input
							MessageBox (hDlg, "Can't fill a selection with a string of zero size.", "Error", MB_OK | MB_ICONERROR);//tell user but don't close dlgbox
							return 1;//didn't process this message
						}//if
						if ((i=(GetDlgItemText (hDlg, IDC_HEX, pcFWText, FW_MAX) == 0))||(i==FW_MAX-1)){//error
							MessageBox (hDlg, "Too great a number of bytes to fill with or some other error.", "Error", MB_OK | MB_ICONERROR);//tell user but don't close dlgbox
							return 1;//didn't process this message
						}//if
						hexstring2charstring();//just in case
						pcFWText[(aa?buflen:buflen*2)]='\0';//access violation if i do it in the above function

					}//else
					if(BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_EQ))asstyp=0;
					else if(BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_OR))asstyp=1;
					else if(BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_AND))asstyp=2;
					else if(BST_CHECKED == IsDlgButtonChecked(hDlg,IDC_XOR))asstyp=3;
					DeleteObject(hfon);// won't need till next time
					DeleteObject(hfdef);
					EndDialog (hDlg, 1);//tell CMD_fw to carry out the fill with operation
					return 0;//did process this message
				}//ok
				break;
				case IDCANCEL:{//cancel pressed
					DeleteObject(hfon);// won't need till next time
					EndDialog (hDlg, 0);//tell CMD_fw not to carry out the fill with operation
					return 0;//did process this message
				}//cancel
				break;
				case IDC_TYPE:{
					if(HIWORD(wParam)==CBN_SELCHANGE){//thing to fill selection with changes
						curtyp = (char)SendMessage(GetDlgItem(hDlg, IDC_TYPE),CB_GETCURSEL,0,0);//get cursel
						EnableWindow(GetDlgItem(hDlg, IDC_FN),curtyp);//en/disable fnamebox and browse button
						EnableWindow(GetDlgItem(hDlg, IDC_BROWSE),curtyp);
						EnableWindow(GetDlgItem(hDlg, IDC_FILESTAT),curtyp);
						curtyp=!curtyp;//flip it for the others
						EnableWindow(GetDlgItem(hDlg, IDC_HEX),curtyp);//en/disable hexboxand relateds
						EnableWindow(GetDlgItem(hDlg, IDC_HEXSTAT),curtyp);
						curtyp=!curtyp;//restore original value -not for below -accurate value needed elsewhere
						//set text in boxes down below
						inittxt(hDlg);
					}
				}
				break;
				case IDC_BROWSE:{
					//prepare OPENFILENAME for the file open common dlg box
					szFWFileName[0] = '\0';
					OPENFILENAME ofn;
					ofn.lStructSize = sizeof (OPENFILENAME);
					ofn.hwndOwner = hDlg;
					ofn.hInstance = NULL;
					ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
					ofn.lpstrCustomFilter = NULL;
					ofn.nMaxCustFilter = 0;
					ofn.nFilterIndex = 0;
					ofn.lpstrFile = szFWFileName;
					ofn.nMaxFile = _MAX_PATH;
					ofn.lpstrFileTitle = NULL;
					ofn.lpstrInitialDir = NULL;
					ofn.lpstrTitle = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
					ofn.lpstrDefExt = NULL;
					ofn.lCustData = 0L;
					ofn.lpfnHook = NULL;
					ofn.lpTemplateName = NULL;
					//show open dlgbox and if file good save name & path in edit box
					if (GetOpenFileName(&ofn))SetDlgItemText(hDlg,IDC_FN,ofn.lpstrFile);
					return 0;//did process this message
				}//browse
				break;
				case IDC_HEX:{
					if(HIWORD(wParam)==EN_UPDATE){//hexedit updated
						GetWindowText(GetDlgItem(hDlg, IDC_HEX), pcFWText, FW_MAX);//gettext
						hexstring2charstring();//convert to char string
						//set text in boxes down below
						inittxt(hDlg);
						return 0;//did process this message
					}//if
					return 1;//didn't process this message
				}//edit1
				break;
				default:return 1;//didn't process this message
			}//switch
		}//wm_command
		break;
	}//switch
	return FALSE;
}//fwdlgproc

void HexEditorWindow::CMD_deletefile(){
	if(IDYES==MessageBox(hwnd,"Are you sure you want to delete this file?","frhed",MB_ICONERROR|MB_YESNO)){
		if(IDYES==MessageBox(hwnd,"Are you really really sure you want to delete this file?","frhed",MB_ICONERROR|MB_YESNO)){
			if(remove(filename))
				MessageBox (hwnd, "Could not delete file.", "Delete file", MB_OK | MB_ICONERROR);
			else {
				//Remove from MRU
				int i;
				for (i=0; i<iMRU_count; i++)
					if(!strcmp (&(strMRU[i][0]), filename))break;
				for (; i<iMRU_count-1; i++)
					strcpy (&(strMRU[i][0]), &(strMRU[i+1][0]));
				iMRU_count--;
				m_iFileChanged = FALSE;CMD_new();//tricky-tricky
			}
		}
	}
}

void HexEditorWindow::CMD_insertfile(){
	char szFileName[_MAX_PATH];
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	szFileName[0] = '\0';
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetOpenFileName (&ofn))
	{
		// RK: don't allow inserting same file we're editing right now.
		//Pabs removed - the bug appears to have disappeared as a result of my improvements

		int fhandle;
		if((fhandle=_open(szFileName,_O_RDONLY|_O_BINARY))!=-1){
			int inslen;
			if((inslen=_filelength (fhandle))!= -1){
				bool bigger, smaller, rssuc = 1, rdsuc = 0;//New size bigger?, resize succesful, read successful
				int rs,re,rl;//Remove start, end, len
				int nl,ol;//New & old lens
				BYTE *dst,*src;
				int count;

				if( bSelected ){
					rs = iStartOfSelection; re = iEndOfSelection;
					if(re<rs)swap(re,rs);
					rl = re + 1 - rs;
				}
				else { rs = iCurByte; rl = 0; }

				ol = DataArray.GetSize();
				nl = ol + inslen - rl;

				bigger = ( inslen > rl );
				smaller = ( inslen < rl );

				if( smaller ){
					src = &DataArray[rs+rl];
					dst = &DataArray[rs+inslen];
					count = ol - (rs+rl);
				}

				if( bigger ){
					rssuc = !!DataArray.SetSize(nl);
					if( rssuc ){
						src = &DataArray[rs+rl];
						dst = &DataArray[rs+inslen];
						count = ol - (rs+rl);
						memmove(dst,src,count);
						DataArray.ExpandToSize();
					}
				}


				if( rssuc ) {
					rdsuc = ( -1 != _read(fhandle,&DataArray[rs],inslen) );
				}

				//In the following two if blocks DataArray.SetUpperBound(somelen-1);
				//is used instead of DataArray.SetSize(somelen);
				//to prevent the possiblity that shortening the
				//buffer might fail (way too hard to handle) because SimpleArray currently
				//uses new & delete instead of malloc & free, which,
				//together with realloc could get around this problem easily
				if( smaller && rdsuc ){
					memmove(dst,src,count);
					DataArray.SetUpperBound(nl-1);
					//DataArray.SetSize(nl);
				}

				if( bigger && rssuc && !rdsuc ){
					memmove(src,dst,count);
					DataArray.SetUpperBound(ol-1);
					//DataArray.SetSize(ol);
				}

				if( rssuc && rdsuc ) {
					// RK: Added a call to update_for_new_datasize().
					if(inslen){
						iStartOfSelection = rs;
						iEndOfSelection = rs+inslen-1;
					}
					else if( bSelected ){
						iCurByte = rs;
						iCurNibble = 0;
					}

					if(inslen || bSelected){
						m_iFileChanged = TRUE;
						bFilestatusChanged = TRUE;
						bSelected = ( inslen != 0 );
						update_for_new_datasize();
					}
				}
				else{
					MessageBox(hwnd,"Could not insert data","Insert file",MB_OK | MB_ICONERROR);
				}
			}
			// RK: Spelling of "size"!
			else MessageBox(hwnd,"Error checking file size","Insert file",MB_OK | MB_ICONERROR);
			_close (fhandle);
		}
		else MessageBox(hwnd,"Error opening file","Insert file",MB_OK | MB_ICONERROR);
	}
}

void HexEditorWindow::CMD_saveselas(){
	char szFileName[_MAX_PATH];
	char szTitleName[_MAX_FNAME + _MAX_EXT];
	OPENFILENAME ofn;
	szTitleName[0] =szFileName[0] = '\0';
	ofn.lStructSize = sizeof (OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "All Files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrFileTitle = szTitleName;
	ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0L;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;
	if (GetSaveFileName (&ofn))
	{
		int filehandle;
		if ((filehandle = _open (szFileName, _O_RDWR|_O_CREAT|_O_TRUNC|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1)
		{
			SetCursor (LoadCursor (NULL, IDC_WAIT));
			if(bSelected){
				iStartOfSelSetting = iStartOfSelection;
				iEndOfSelSetting = iEndOfSelection;
			} else {
				iStartOfSelSetting = 0;
				iEndOfSelSetting = LASTBYTE;
			}
			if(iStartOfSelSetting>iEndOfSelSetting)swap(iStartOfSelSetting,iEndOfSelSetting);
			if (!((_write (filehandle, &DataArray[iStartOfSelSetting], iEndOfSelSetting-iStartOfSelSetting+1)) != -1))
				MessageBox (hwnd, "Could not save file.", "Save as", MB_OK | MB_ICONERROR);
			SetCursor (LoadCursor (NULL, IDC_ARROW));
			_close (filehandle);
		}
		else
			MessageBox (hwnd, "Could not save file.", "Save as", MB_OK | MB_ICONERROR);
	}
	repaint ();
}

void MessageCopyBox(HWND hWnd, LPTSTR lpText, LPCTSTR lpCaption, UINT uType, HWND hwnd)
{
	int len=strlen(lpText);//get the # bytes needed to store the string (not counting '\0')
	//& get where we have to put a '\0' character later
	// RK: Added "?" to end of string.
	strcat(lpText,"\nDo you want the above output to be copied to the clipboard?\n");
	if(IDYES==MessageBox (hWnd, lpText, lpCaption, MB_YESNO | uType))
	{
		//user wants to copy output
		lpText[len]=0;//Remove the line added above
		//Pabs removed & replaced with TextToClipboard
		TextToClipboard( lpText, hwnd );
	}
//user doesn't want to copy output
}

void eatwhite(FILE*f){
	//why the hell is this function not implemented for c FILE[s]
	//but implemented for c++ iostream[s]
	int c;
	for(;;){
		c = fgetc(f);
		if(!isspace(c)){
			ungetc(c,f);
			break;
		}
	}
}

//-------------------------------------------------------------------
// Following code by R. Kibria.

//-------------------------------------------------------------------
// If the caret is at a position where the findstring starts,
// select the data there.
int HexEditorWindow::select_if_found_on_current_pos( SimpleString& finddata, int finddir, int do_repaint, char (*cmp) (char) )
{
	char* tofind;
	// Create a translation from bytecode to char array of finddata.
	int destlen = create_bc_translation( &tofind,
		(char*) finddata,
		finddata.StrLen(),
		iCharacterSet,
		iBinaryMode
		);

	int i;
	if( finddir == 1 )
	{
		// Find forward.
		i = find_bytes( (char*) &(DataArray[iCurByte]),
				DataArray.GetLength() - iCurByte - 1,
				tofind,	destlen, 1, cmp );
		if( i != -1 )
			iCurByte += i;
	}
	else
	{
		// Find backward.
		i = find_bytes( (char*) &(DataArray[0]),
					min( iCurByte + (destlen - 1), DataArray.GetLength() ),
					tofind, destlen, -1, cmp );
		if( i != -1 )
			iCurByte = i;
	}

	if( i != -1 )
	{
		// NEW: Select found interval.
		bSelected = TRUE;
		iStartOfSelection = iCurByte;
		iEndOfSelection = iCurByte + destlen - 1;
		if( do_repaint )
		{
			adjust_view_for_selection();
			repaint();
		}
	}
	else
	{
		if( tofind != NULL )
			delete [] tofind;
		return FALSE;
	}

	if( tofind != NULL )
		delete [] tofind;

	return TRUE;
}

//-------------------------------------------------------------------
int HexEditorWindow::iGetBinMode()
{
	return iBinaryMode;
}

//-------------------------------------------------------------------
int HexEditorWindow::iGetCharMode()
{
	return iCharacterSet;
}

//-------------------------------------------------------------------
int HexEditorWindow::iGetStartOfSelection()
{
	if( iStartOfSelection < iEndOfSelection )
		return iStartOfSelection;
	else
		return iEndOfSelection;
}

//-------------------------------------------------------------------
int HexEditorWindow::iGetEndOfSelection()
{
	if( iStartOfSelection < iEndOfSelection )
		return iEndOfSelection;
	else
		return iStartOfSelection;
}

//-------------------------------------------------------------------
//Following code by Pabs

//Required for the next two functions
long lhpos,lhlen;
int (*lhgetc)(void*);
int flhgetc(void*i){return fgetc((FILE*)i);}
int clhgetc(void*i){return lhpos<lhlen?*((BYTE*)i+lhpos++):EOF;}

int (*lhungetc)(void*,int);
int flhungetc(void*i, int c){return ungetc(c,(FILE*)i);}
int clhungetc(void*i,int c){return lhpos<lhlen?*((BYTE*)i+ --lhpos) = (BYTE) c:EOF;}

long (*lhtell)(void*);
long flhtell(void*i){return ftell((FILE*)i);}
long clhtell(void*){return lhpos;}

int (*lhseek)(void*,long);
int flhseek(void*i,long p){return fseek((FILE*)i,p,SEEK_SET);}
int clhseek(void*,long p){return 0,lhpos = p;}

int lheatwhite(void*i){
	int c;
	for(;;){
		c = lhgetc(i);
		if(!isspace(c)){
			lhungetc(i,c);
			break;
		}
	}
	return c;
}

int HexEditorWindow::load_hexfile(void* hexin, BYTE cf){
	//Variables used below & their functions
	BYTE typ = 0;//type of file (0=just hex digits)
	BYTE flnd = 1, dim = 1, diio = 1;
	//typ = 1: first line not done, don't ignore mismatches, don't ignore invalid offsets
	//typ = 0: first nibble being done,,don't ignore illegal characters
	int i,ii=0,ls,ol,bpl,tmp,fo = 0,fol;
	//typ = 1: general index var, amount of data read in, line start(, offset start), offset length, bytes per line, tmp var for msgs & errors, first offset, first offset length
	//typ = 0: general index var, amount of bytes read,,,, tmp var for msgs & errors
	unsigned char ct, c[4] = {0,0,0,0};
	//typ = 1: tmp char, 3 tmp chars for the reading of the hex data (1 '\0')
	//typ = 1: tmp char storing nibble chars
	int temp[4] = {0,0,0,0};
	//Used for detecting EOF

	//Check the type of file - if only whitespace & hex then just hex else can be used to set line len etc
	//There is probably a better way to do this
	for(i = 0; i < lhlen; i++){
		ct = (BYTE)lhgetc(hexin);
		if(!(isspace(ct) || isxdigit(ct))){
			typ++; break;
		}
	}

	lhseek(hexin,0);

	char msg[150] =
		"Does this data have the same format as the frhed display?"
		"\nThis data contains ";
	strcat(msg,typ?"characters other than":"only" );
	strcat(msg," whitespace and hexdigits. (");
	if(!typ)strcat(msg,"un");
	strcat(msg,"like frhed display)");

	tmp = MessageBox(hwnd,msg,"Import Hexdump",MB_YESNOCANCEL);
	if(tmp==IDYES)typ=1;
	else if(tmp==IDNO)typ=0;
	else return FALSE;

	if(typ){//Display output

		BYTE alter = (IDYES == MessageBox(hwnd,"Would you like display settings found in the data to replace current ones?","Import Hexdump",MB_YESNO));

		bAutoOLSetting = 1;
		for(;;){
			//get the offset
			if(diio)ls = lhtell(hexin);

			for(ol = 0;;ol++){
				temp[0]=lhgetc(hexin);
				if(temp[0]==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
				c[0] = (BYTE)temp[0];
				if(isspace(c[0]))break;
				else if(!isxdigit(c[0]) && diio){
					tmp = MessageBox(hwnd, "Illegal character in offset.\nIgnore further invalid offsets?","Import Hexdump",MB_YESNOCANCEL | MB_ICONERROR);
					if(tmp==IDYES)diio = 0;
					else if(tmp==IDCANCEL)
						return FALSE;//bad file
				}
			}

			if(alter && flnd)
				iOffsetLenSetting = fol = ol;

			if( bAutoOLSetting && fol != ol ) bAutoOLSetting = 0;

			i = lhtell(hexin);

			if(diio){
				if(cf) sscanf((char*)hexin+ls,"%x",&tmp);
				else{lhseek(hexin,ls); fscanf((FILE*)hexin,"%x",&tmp);}
				if(flnd && tmp){
					char msg[150];
					sprintf(msg,"The first offset found was 0x%x, which is greater than zero."
						"\nDo you want to insert %d null bytes at the start of the data?",tmp,tmp);
					if(IDYES == MessageBox(hwnd, msg,"Import Hexdump",MB_YESNO | MB_ICONERROR)){
						if (DataArray.SetSize (ii=tmp) == FALSE)
						{
							return IDYES == MessageBox (hwnd, "Not enough memory to import data.\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
						}
						DataArray.ExpandToSize();
						memset(&DataArray[0],0,tmp);
					}
					else{
						fo = tmp; if(alter){ bPartialStats = TRUE; iPartialOffset = tmp; }
					}
				}
				else if(ii+fo!=tmp){
					tmp = MessageBox(hwnd, "Invalid offset found.\nIgnore further invalid offsets?","Import Hexdump",MB_YESNOCANCEL | MB_ICONERROR);
					if(tmp==IDYES)diio = 0;
					else if(tmp==IDCANCEL)
						return FALSE;//bad file
				}
			}

			lhseek(hexin,i);

			if(lheatwhite(hexin)==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);

			ls=ii;//remember the start of the line in the DataArray

			//get data bytes
			for(bpl = 0;;bpl++){
				//get the three chars
				for(i=0;i<3;i++){
					temp[i]=lhgetc(hexin);
					if(temp[i]==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
					c[i] = (BYTE)temp[i];
				}
				if(!(isxdigit(c[0]) && isxdigit(c[1]) && isspace(c[2])))
					goto IllegalCharacter;
				//yes we are fine
				else {
					//store the value no matter what
					if (DataArray.SetSize (ii+1) == FALSE)
					{
						return IDYES == MessageBox (hwnd, "Not enough memory to import data.\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
					}
					DataArray.ExpandToSize();
					//do this so that we don't overwrite memory outside the DataArray
					// - because sscanf requires an int for storage
					sscanf((char*)c,"%x",&tmp);//save it to tmp
					DataArray[ii] = (BYTE)tmp;
					ii++;//next byte

					for(i=0;i<3;i++){
						temp[i]=lhgetc(hexin);
						if(temp[i]==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
						c[i] = (BYTE)temp[i];
					}
					lhungetc(hexin,c[2]);
					lhungetc(hexin,c[1]);
					lhungetc(hexin,c[0]);
					if(c[0] == ' ' || c[0] =='_'){
						if(c[1]==c[0] && c[2]==' '){
							//get those back
							for(i=0;i<3;i++)if(lhgetc(hexin)==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
							bpl++;
							for(;;bpl++){
								for(i=0;i<3;i++){
									temp[i]=lhgetc(hexin);
									if(temp[i]==EOF){return TRUE;}//Assume the file is good
									c[i] = (BYTE)temp[i];
								}
								if( c[0]=='\r' && c[1]=='\n' ){//We have missed the chars because of all the spaces
									lhungetc(hexin,c[2]);
									lhungetc(hexin,c[1]);
									lhungetc(hexin,c[0]);
									goto NextLine;
								}
								if( c[0]==' ' && c[1] != ' ' ){//We have found the start of the chars
									lhungetc(hexin,c[2]);
									lhungetc(hexin,c[1]);
									lhungetc(hexin,c[0]);
									break;
								}
							}
						}
						else if (c[0]=='_') goto IllegalCharacter;

						if(alter && flnd){
							iBPLSetting = bpl + 1;
							iAutomaticXAdjust = BST_UNCHECKED;
						}
						break;

					}
					else if(!isxdigit(c[0])){
IllegalCharacter:
						//someone has been buggering with the file & the syntax is screwed up
						//the next digit is not hex ' ' or '_'
						return IDYES == MessageBox (hwnd, "Illegal character in hex data.\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);//bad file
					}
				}//the first 3 chars read in properly
			}//got the data bytes

			//weak point - assumes iCharSpace is 1
			//trash the extra space
			if(lhgetc(hexin)==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);

			//Verify that the data read by the above loop is correct and equal to that read by this loop
			for (;ls<ii;ls++){

				temp[0] = lhgetc(hexin);
				if(temp[0]==EOF)return IDYES == MessageBox (hwnd, "Unexpected end of data found\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
				c[0] = (BYTE)temp[0];
				ct = DataArray[ls];
				//Get translated character - '.' for non-printables c[0] else

				c[1] = ((ct>=32 && ct<=126) || (ct>=160 && ct<=255) || (ct>=145 && ct<=146))?ct:'.';
				//check if the data is the same
				if( !ct || c[1] == ct ){//0 (both OEM & ANSI translate) or one of those ranges where neither cset translates the character
					if(c[0]!=c[1]){
BadData:
						if(dim){
							tmp = MessageBox (hwnd, "Character data does not agree with hex data.\nIgnore further mismatched data?\nNB: Hex data will be used when ignoring.", "Import Hexdump", MB_YESNOCANCEL | MB_ICONERROR);
							if(tmp==IDYES)dim = 0;
							else if(tmp==IDCANCEL)
								return FALSE;//bad file
						}
					}
				}
				else{
					if(c[0]==ct){
						if(alter) iCharacterSetting = OEM_FIXED_FONT;
					}
					else{
						if(c[0]!=c[1]) goto BadData;
						else if(alter) iCharacterSetting = ANSI_FIXED_FONT;
					}
				}
			}//get rest of line

NextLine:
			if(lheatwhite(hexin) == EOF) return TRUE;//yes we have finished successfully

			if(flnd)flnd = 0;
		}//parsing loop
	}//display format

//----------------------------------------------------------------------------------

//Use only if U know c is a hex digit & not something else like' ', 'z' etc
#define hex2nibble(c)       ( isdigit((c)) ? (c)-'0' : (c)-( islower((c)) ? 'a' : 'A')+10 )

	else {//just hex & space
		flnd=0;//Start with the first nibble
		for(i = 0; i < lhlen; i++){
			if((temp[0]=lhgetc(hexin))==EOF){
				return IDYES == MessageBox (hwnd, "Error while reading data."
					"\nSomething wierd happened - "
					"\nThe function tried to read at end of the data,"
					"\nwhen it should not get to the EOF character"
					"\nPlease report this to Pabs\nCannot continue!"
					"\nDo you want to keep what has been found so far?"
					, "Import Hexdump", MB_YESNO | MB_ICONERROR);
			}

			if(isxdigit(temp[0])){
				if(!flnd){
					if (DataArray.SetSize(ii+1)==FALSE){
						return IDYES == MessageBox (hwnd, "Not enough memory to import data.\nCannot continue!\nDo you want to keep what has been found so far?", "Import Hexdump", MB_YESNO | MB_ICONERROR);
					}
					DataArray.ExpandToSize();
					DataArray[ii] = 0;
				}
				DataArray[ii] |= hex2nibble((BYTE)temp[0]) ;
				if(flnd)
					ii++;
				else
					DataArray[ii] <<= 4;
				flnd=!flnd;
			}
			else if(!isspace(temp[0]) && diio){
				tmp = MessageBox (hwnd, "Illegal character found.\nIgnore further illegal characters?", "Import Hexdump", MB_YESNOCANCEL | MB_ICONERROR);
				if(tmp==IDYES)diio = 0;
				else if(tmp==IDCANCEL)
					return FALSE;
			}
		}

		return TRUE;
	}//hex & space
}

int HexEditorWindow::CMD_open_hexdump ()
{
	if (m_iFileChanged == TRUE)
	{
		int res = MessageBox (hwnd, "Do you want to save your changes?", "Import Hexdump", MB_YESNOCANCEL | MB_ICONQUESTION);
		if( res == IDCANCEL || ( res == IDYES && !( bFileNeverSaved ? CMD_save_as() : CMD_save() ) ) )//User doesn't want to import hexdump or User wants to save and the save was unsuccessful
			return 0;//Don't open
	}

	BYTE bUseClip = 0;
	HGLOBAL hClipMemory;
	int tmp;
	void* hexin;

	//Check if clipboard may be used
	if( OpenClipboard( NULL ) )
	{
		hClipMemory = GetClipboardData( CF_TEXT );
		if( hClipMemory != NULL ) bUseClip = 1;
		CloseClipboard ();
	}

	//Check if user wants to use clipboard
	if(bUseClip){
		tmp = MessageBox(hwnd,"There is text on the clipboard.\nDo you want to import from\nthe clipboard instead of a file?","Import Hexdump",MB_YESNOCANCEL|MB_ICONQUESTION);
		if(tmp==IDYES)bUseClip=1;
		else if(tmp==IDNO)bUseClip=0;
		else return FALSE;
	}

	tmp = 1;

	char szFileName[_MAX_PATH];
	//Import from clipboard
	if(bUseClip){
		//Set up variables for the function
		//Open the clipboard
		if( OpenClipboard( NULL ) ){
			//Get the data
			hClipMemory = GetClipboardData( CF_TEXT );
			if( hClipMemory != NULL ){
				lhlen = GlobalSize(hClipMemory);
				if (lhlen){
					BYTE* pClipMemory = (BYTE*) GlobalLock (hClipMemory);
					if (pClipMemory){
						hexin = new char[lhlen];
						if(hexin){
							tmp = 0;
							memcpy (hexin, pClipMemory, lhlen);
							lhpos = 0;
							lhgetc = clhgetc;
							lhungetc = clhungetc;
							lhtell = clhtell;
							lhseek = clhseek;
						}
					}
				}
				GlobalUnlock (hClipMemory);
			}
			CloseClipboard ();

			//Because windoze sux so much & GlobalSize returns a size > the requested size (filling the rest with zeros only sometimes)
			int i = strlen((char*)hexin);
			if( i < lhlen ) //In case strlen doesn't find a '\0' at the end of the string
				lhlen = i;
		}

		//An error occurred - either could not open clip or the handle was NULL
		if(tmp){
			MessageBox(hwnd,"Could not get text from the clipboard.\nCannot continue!","Import Hexdump",MB_OK|MB_ICONERROR);
			return FALSE;
		}
	}
	//Import from file
	else{
		//Initialize the struct
		char szTitleName[_MAX_FNAME + _MAX_EXT];
		szFileName[0] = '\0';
		OPENFILENAME ofn;
		ofn.lStructSize = sizeof (OPENFILENAME);
		ofn.hwndOwner = hwnd;
		ofn.hInstance = NULL;
		ofn.lpstrFilter = "Hex Dump files(*.txt,*.hex)\0*.txt;*.hex\0All Files (*.*)\0*.*\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter = 0;
		ofn.nFilterIndex = 0;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = _MAX_PATH;
		ofn.lpstrFileTitle = szTitleName;
		ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
		ofn.lpstrInitialDir = NULL;
		ofn.lpstrTitle = NULL;
		ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT;
		ofn.nFileOffset = 0;
		ofn.nFileExtension = 0;
		ofn.lpstrDefExt = NULL;
		ofn.lCustData = 0L;
		ofn.lpfnHook = NULL;
		ofn.lpTemplateName = NULL;
		if (GetOpenFileName (&ofn)){
			int filehandle;
			if ((filehandle = _open (szFileName,_O_RDONLY|_O_BINARY,_S_IREAD|_S_IWRITE)) != -1){
 				lhlen = _filelength (filehandle);

				if( lhlen == 0 ){
					_close(filehandle);
					MessageBox(hwnd,"The file is empty\nCannot continue!","Import Hexdump",MB_OK|MB_ICONERROR);
					return FALSE;
				}

				//Set up variables for the function
				hexin = _fdopen(filehandle,"rb");
				if(hexin){
					tmp = 0;
					lhgetc = flhgetc;
					lhungetc = flhungetc;
					lhtell = flhtell;
					lhseek = flhseek;
				}
			}
		}
		else return FALSE; //User pressed cancel or error occurred

		if(tmp){
			MessageBox(hwnd,"Could not get text from the file.\nCannot continue!","Import Hexdump",MB_OK|MB_ICONERROR);
			return FALSE;
		}
	}

	//Save data in case import fails
	BYTE* data;
	tmp = DataArray.GetSize();
	if(tmp){
		data = new BYTE[tmp];
		if(!data){
			if(bUseClip)
				delete [] (char*)hexin;
			else
				fclose((FILE*) hexin);

			MessageBox (hwnd, "Not enough memory to import data.\nCannot continue!", "Import Hexdump", MB_OK | MB_ICONERROR);
			return FALSE;
		}
		BYTE* start = &DataArray[0];
		memcpy(data,start,tmp);
	}

	SetCursor (LoadCursor (NULL, IDC_WAIT));

	iOffsetLenSetting = iMinOffsetLen;
	bAutoOLSetting = bAutoOffsetLen;
	iBPLSetting = iBytesPerLine;
	iAutomaticXAdjust = iAutomaticBPL;
	iCharacterSetting = iCharacterSet;

	if(load_hexfile(hexin,bUseClip)){
		//Successful
		strcpy( filename, "Untitled");
		iCurLine = iCurByte = iCurNibble =
			bPartialOpen = bPartialStats = m_iFileChanged =
			bSelected =
			iVscrollMax = iVscrollPos =
			iVscrollInc = iHscrollMax = iHscrollPos =
			iHscrollInc = iCurLine = iCurByte = iCurNibble = FALSE;
		bFileNeverSaved = bFilestatusChanged = TRUE;
		// If read-only mode on opening is enabled or the file is read only:
		if( bOpenReadOnly || (!bUseClip && -1== _access(szFileName,02)))
			bReadOnly = TRUE;
		else
			bReadOnly = FALSE;

		iMinOffsetLen = iOffsetLenSetting;
		if(!bAutoOLSetting) bAutoOffsetLen = bAutoOLSetting;//You cannot ever tell that bAutoOffsetLen is on because not all of the data may have been hexdumped
		iBytesPerLine = iBPLSetting;
		iAutomaticBPL = iAutomaticXAdjust;
		iCharacterSet = iCharacterSetting;

		save_ini_data ();
	}
	else{
		//Restore data
		if (tmp)//This way we don't have to do much copying etc.
			DataArray.Adopt(data,tmp-1,tmp);
		else DataArray.ClearAll();
	}

	SetCursor (LoadCursor (NULL, IDC_ARROW));

	update_for_new_datasize();

	//Deinitialize
	if(bUseClip)
		delete [] (BYTE*)hexin;
	else
		fclose((FILE*) hexin);

	return TRUE;
}

char contextpresent(){
	HKEY key1;
	LONG res;
	res = RegOpenKeyEx( HKEY_CLASSES_ROOT, "*\\shell\\Open in frhed\\command", 0, KEY_ALL_ACCESS, &key1 );
	if( res == ERROR_SUCCESS ){//succeeded check if has the required keys & data
		char stringval[ _MAX_PATH ];
		char exepath[ _MAX_PATH ];
		long len = 0;//dummy
		strcpy( exepath, _pgmptr );
		strcat(exepath," %1");
		RegQueryValue(key1,NULL,stringval,&len);
		RegCloseKey(key1);
		if(strcmp(stringval, exepath))
			return 1;
	}
	return 0;
}

char unknownpresent(){
	HKEY key1;
	LONG res;
	res = RegOpenKeyEx( HKEY_CLASSES_ROOT, "Unknown\\shell\\Open in frhed\\command", 0, KEY_ALL_ACCESS, &key1 );
	if( res == ERROR_SUCCESS ){//succeeded check if has the required keys & data
		char stringval[ _MAX_PATH ];
		char exepath[ _MAX_PATH ];
		long len = 0;//dummy
		strcpy( exepath, _pgmptr );
		strcat(exepath," %1");
		RegQueryValue(key1,NULL,stringval,&len);
		RegCloseKey(key1);
		if(strcmp(stringval, exepath))
			return 1;
	}
	return 0;
}

char oldpresent(){
	HKEY hk;
	char keyname[] = "Software\\frhed";
	char subkeynam[MAX_PATH + 1] = "";
	LONG res = RegOpenKeyEx( HKEY_CURRENT_USER,keyname,0,KEY_ALL_ACCESS,&hk );
	if( res == ERROR_SUCCESS ){
		for(DWORD i = 0;; i++ ){
			res = RegEnumKey(hk,i,subkeynam,MAX_PATH + 1);
			if(res==ERROR_NO_MORE_ITEMS)break;
			else if(0!=strcmp(subkeynam,"v"CURRENT_VERSION"."SUB_RELEASE_NO)){
				RegCloseKey(hk);
				return TRUE;
			}
		}
		RegCloseKey(hk);
	}
	return FALSE;
}

char frhedpresent(){
	//Check if frhed\subreleaseno exists
	HKEY hk;
	if(ERROR_SUCCESS==RegOpenKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO ,&hk)){
		RegCloseKey(hk);
		return TRUE;
	}

	return FALSE;
}

char linkspresent(){
	//Check if frhed\subreleaseno\links exists
	HKEY hk;
	if(ERROR_SUCCESS==RegOpenKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",&hk)){
		RegCloseKey(hk);
		return TRUE;
	}

	return FALSE;
}

int CALLBACK SearchCallbackProc(HWND hw, UINT m, LPARAM l, LPARAM d){
	UNREFERENCED_PARAMETER( d );
	if(m==BFFM_SELCHANGED){
		char szDir[MAX_PATH];
		BOOL ret = SHGetPathFromIDList((LPITEMIDLIST) l ,szDir) && PathIsDirectory(szDir);
		SendMessage(hw,BFFM_ENABLEOK,0,ret);//Enable/Disable
		SendMessage(hw,BFFM_SETSTATUSTEXT,0,(LPARAM)(ret?"frhed can start searching here":"frhed cannot start the search from here"));
	}
	return 0;
}

HWND hwlist;
char rn[MAX_PATH];
char vn[50];
LVITEM li;
LVFINDINFO fi;
char*fnam;
char cr,si;
int indx;

//Thanks to Raihan for the code this was based on - see his web page
void TraverseFolders(){
	_finddata_t F;
	int S;

	//First find all the links
	if ((S = _findfirst ("*.lnk", &F)) != -1){
		do
			if ( !( F.attrib & _A_SUBDIR ) && !ResolveIt(NULL,F.name,rn) ){
				if(cr){//findnfix
					PathStripPath(rn);//strip to file name
					si = !_stricmp(rn,"frhed.exe");
				}
				else si = PathsEqual(_pgmptr,rn);//update
				if(si){
					_fullpath( rn, F.name, MAX_PATH);
					remove(rn);//get rid of the file if we are fixing (in case of 2 links to frhed in same dir & links with the wrong name)
					*( (fnam = PathFindFileName(rn)) - 1 ) = 0; //strip the file name (& '\\') off
					if(-1==ListView_FindItem(hwlist,(UINT)-1,&fi)){//Not present yet
						//Insert the item
						ListView_InsertItem(hwlist,&li);
						//Add to the Registry
						sprintf(vn,"%d",li.iItem);
						SHSetValue(HKEY_CURRENT_USER,"Software\\frhed\\v"CURRENT_VERSION"."SUB_RELEASE_NO"\\links",vn,REG_SZ,rn,fnam-rn);
						li.iItem++;
					}
					strcat(rn,"\\frhed.lnk");//put the name backon
					CreateLink(_pgmptr,rn);//create the new link
				}
			}
		while (!_findnext (S, &F));
		_findclose (S);
	}

	//Then find all the subdirs
	if ((S = _findfirst ("*", &F)) != -1)
	{
		do //except "." && ".."
			if(F.attrib & _A_SUBDIR && strcmp (".", F.name) && strcmp ("..", F.name) )
			{
				_chdir(F.name);
				TraverseFolders ();
				_chdir("..");
			}
		while (!_findnext (S, &F));
		_findclose (S);
	}

}

int CALLBACK BrowseCallbackProc(HWND hw, UINT m, LPARAM l, LPARAM d){
	UNREFERENCED_PARAMETER( d );
	char szDir[MAX_PATH];

		//If the folder exists & doesn't contain a link then enabled
		// Set the status window to the currently selected path.
	if(m==BFFM_SELCHANGED){
		if (SHGetPathFromIDList((LPITEMIDLIST) l ,szDir) && PathIsDirectory(szDir)){
			PathAddBackslash(szDir);
			strcat(szDir,"frhed.lnk");
			if(PathFileExists(szDir)){
				SendMessage(hw,BFFM_ENABLEOK,0,0);//Disable
				SendMessage(hw,BFFM_SETSTATUSTEXT,0,(LPARAM)"This folder already contains a file called \"frhed.lnk\"");
			}
			else{
				//If there is any other (faster/easier) way to test whether the file-system is writeable or not Please let me know - Pabs
				int fh = _creat(szDir,_S_IWRITE);
				if(fh!=-1){
					_close(fh);
					remove(szDir);
					SendMessage(hw,BFFM_ENABLEOK,0,1);//Enable
					SendMessage(hw,BFFM_SETSTATUSTEXT,0,(LPARAM)"\"frhed.lnk\" can be added to this folder");
				}
				else{
					SendMessage(hw,BFFM_ENABLEOK,0,0);//Disable
					SendMessage(hw,BFFM_SETSTATUSTEXT,0,(LPARAM)"\"frhed.lnk\" cannot be added to this folder");
				}
			}
		}
		else{
			SendMessage(hw,BFFM_ENABLEOK,0,0);//Disable
			SendMessage(hw,BFFM_SETSTATUSTEXT,0,(LPARAM)"\"frhed.lnk\" cannot be added to this folder");
		}
	}
	return 0;
}

BOOL CALLBACK ShortcutsDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l){
	switch(m){
		case WM_INITDIALOG:{
			//Add a column
			LVCOLUMN col;
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH ;
			col.fmt = LVCFMT_LEFT;
			col.pszText = "link names are \"frhed.lnk\"";
			col.cx = 153;
			ListView_InsertColumn(GetDlgItem(hw,IDC_LIST),0,&col);

			//Load links from the registry
			//Tricky-tricky
			SendMessage(hw,WM_COMMAND,MAKELONG(IDC_RELOAD,BN_CLICKED),(LPARAM)GetDlgItem(hw,IDC_RELOAD));
			//Determine (from registry) if sendto/etc present
			return FALSE;
		}
		case WM_COMMAND:{
			switch (LOWORD (w)){
				case IDCANCEL:
				case IDOK:{
					//Delete all values
					RegDeleteKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links");
					//Save links (from the list box) to the registry & file system
					HKEY hk;
					if(ERROR_SUCCESS==RegCreateKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",&hk)){
						char valnam[50] = "";//value name
						char buf[_MAX_PATH+1]="";//location of the link (all links named frhed.lnk)
						HWND list = GetDlgItem (hw, IDC_LIST);//get the list
						int num = ListView_GetItemCount(list);//get the # items in the list
						int len;
						for(int i=0;i<num;i++){//loop num times
							sprintf(valnam,"%d",i);//write the valname
							ListView_GetItemText(list, i, 0, buf, _MAX_PATH+1);//get the string
							len = strlen(buf)+1;//string len +1
							RegSetValueEx(hk,valnam,0,REG_SZ,(BYTE*)buf,len);
							//Just in case
							PathAddBackslash(buf);
							strcat(buf,"frhed.lnk");
							CreateLink(_pgmptr,buf);
						}//end of the loop
						RegCloseKey(hk);
					}
					else MessageBox(hw,"Could not Save shortcut entries", "Shortcuts",MB_OK);

					//If the key is empty after this kill it (to prevent re-enabling of "Remove frhed")
					SHDeleteEmptyKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links");
					SHDeleteEmptyKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO );

					EndDialog (hw, 0);
					return TRUE;
				}

				case IDC_MOVE:
				case IDC_ADD:{
					//Call the filesystem dialog box to browse to a folder
					//& add it

					BROWSEINFO bi;
					CHAR szDir[MAX_PATH];
					LPITEMIDLIST pidl;
					LPMALLOC pMalloc;
					HWND list = GetDlgItem(hw,IDC_LIST);
					int di = -1;
					HKEY hk;

					if(LOWORD(w)==IDC_MOVE){
						di = ListView_GetSelectedCount(list);
						if(di>1){
							MessageBox(hw,"Can't move more than 1 link at a time","Move link",MB_OK);
							return TRUE;
						}
						else if(di!=1){
							MessageBox(hw,"No link selected to move","Move link",MB_OK);
							return TRUE;
						}
						di = ListView_GetNextItem(list, (UINT)-1, LVNI_SELECTED);
						if(di==-1){
							MessageBox(hw,"Couldn't find the selected item","Move link",MB_OK);
							return TRUE;
						}
					}
					if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
						ZeroMemory(&bi,sizeof(bi));
						bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT ;
						bi.hwndOwner = hw;
						bi.lpfn = BrowseCallbackProc;
						if(LOWORD(w)==IDC_ADD)
							bi.lpszTitle = "Place a link to frhed in...";
						else if(LOWORD(w)==IDC_MOVE)
							bi.lpszTitle = "Move the link to frhed to...";

						pidl = SHBrowseForFolder(&bi);
						if (pidl){
							if(SHGetPathFromIDList(pidl,szDir)) {
								//Check if the item is already in the list
								int num = ListView_GetItemCount(list);//get the # items in the list
								int done = 0;
								char path[MAX_PATH]="";
								char buf[_MAX_PATH+1]="";

								strcpy(path,szDir);
								_strupr(path);
								for(int i=0;i<num;i++){//loop num times
									ListView_GetItemText(list, i, 0, buf,_MAX_PATH+1);//get the string
									_strupr(buf);
									if(!strcmp(buf,path)){
										done = 1;
										break;
									}
								}//end of the loop
								char valnam[_MAX_PATH+1];
								if(done){
									MessageBox(hw,"There is already a link in that folder","Add/Move",MB_OK);
									//Just in case
									PathAddBackslash(szDir);
									strcat(szDir,"frhed.lnk");
									CreateLink(_pgmptr,szDir);
								}
								else{
									if(LOWORD(w)==IDC_ADD){
										//Add to the list
										LVITEM item;
										ZeroMemory(&item,sizeof(item));
										item.mask=LVIF_TEXT;
										item.pszText = szDir;
										item.iItem = num;
										ListView_InsertItem(list, &item);
										//Add to the registry (find a string name that doesn't exist first)
										if(ERROR_SUCCESS==RegCreateKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",&hk)){
											for(DWORD i = 0;;i++){
												sprintf(valnam,"%d",i);
												if(ERROR_FILE_NOT_FOUND==RegQueryValueEx(hk,valnam,0,NULL,NULL,NULL)){
													RegSetValueEx(hk,valnam,0,REG_SZ,(BYTE*)szDir,strlen(szDir)+1);
													break;
												}
											}
											RegCloseKey(hk);
											//Add to the filesystem
											PathAddBackslash(szDir);
											strcat(szDir,"frhed.lnk");
											CreateLink(_pgmptr,szDir);
										}
									}
									else if(LOWORD(w)==IDC_MOVE){
										//Move the old one to the new loc
										DWORD valnamsize,typ;
										char valbuf[_MAX_PATH+1];
										DWORD valbufsize,ret;
										char cursel[_MAX_PATH+1]="";
										//Get the old path
										ListView_GetItemText(list,di,0,cursel,_MAX_PATH+1);
										_strupr(cursel);
										//Set the new path in the registry
										if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",0,KEY_ALL_ACCESS,&hk)){
											for(DWORD i = 0;;i++){
												typ=0;
												valnamsize = valbufsize = _MAX_PATH+1;
												valbuf[0]=valnam[0]=0;
												ret = RegEnumValue(hk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
												_strupr(valbuf);
												if(typ==REG_SZ && valbuf[0]!=0 && !strcmp(valbuf,cursel)){
													RegSetValueEx(hk,valnam,0,REG_SZ,(BYTE*)szDir,strlen(szDir)+1);
													break;
												}
												if(ERROR_NO_MORE_ITEMS==ret)break;
											}
											RegCloseKey(hk);
											//Set the new path
											ListView_SetItemText(list,di,0,szDir);
											//Move the actual file
											PathAddBackslash(szDir);
											strcat(szDir,"frhed.lnk");
											PathAddBackslash(cursel);
											strcat(cursel,"frhed.lnk");
											CreateLink(_pgmptr,cursel);//Just in case
											rename(cursel,szDir);
										}
									}
								}
							}
							pMalloc->Free(pidl);
						}
						pMalloc->Release();
					}
					return TRUE;
				}
				case IDC_FIND_AND_FIX:
					//Go through the file system searching for links to frhed.exe and fix them so they point to this exe
				case IDC_UPDATE:{
					//Go through the file system searching for links to this exe
					//-Thanks to Raihan for the traversing code this was based on.
					if(LOWORD (w) == IDC_FIND_AND_FIX && IDNO == MessageBox(hw,"Existing links to old versions of frhed will be updated to this version\nAre you sure you want to continue","Find & fix",MB_YESNO))return TRUE;
					//Find a spot to start from
					LPMALLOC pMalloc;

					if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
						BROWSEINFO bi;
						LPITEMIDLIST pidl;
						ZeroMemory(&bi,sizeof(bi));
						bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT ;
						bi.hwndOwner = hw;
						bi.lpfn = SearchCallbackProc;
						bi.lpszTitle = "Pick a folder to start searching in.";

						pidl = SHBrowseForFolder(&bi);
						if (pidl){
							CHAR szDir[MAX_PATH];
							if(SHGetPathFromIDList(pidl,szDir)) {
								WaitCursor wc;//Wait until finished
								_chdir(szDir);//Set the dir to start searching in
								hwlist = GetDlgItem(hw,IDC_LIST);//Set the list hwnd
								ZeroMemory(&li,sizeof(li));
								li.mask = LVIF_TEXT;//Using the text only
								li.iItem = ListView_GetItemCount(hwlist);//Initial insert pos
								ZeroMemory(&fi,sizeof(fi));
								fi.flags = LVFI_STRING;//will need to check for duplicates
								fi.psz = li.pszText = rn;//Positions don't change beween files (Absolute path is entered into rn)
								cr = ( LOWORD (w) == IDC_FIND_AND_FIX );//any frhed.exe if 1 else _pgmptr
								TraverseFolders();//Search
							}
							pMalloc->Free(pidl);
						}
						pMalloc->Release();
					}

					return TRUE;
				}
				case IDC_DELETE:{
					HWND list = GetDlgItem (hw, IDC_LIST);//get the list
					//Delete the selected links from the registry & the filesystem
					int di = ListView_GetSelectedCount(list);
					if(di==0){
						MessageBox(hw,"No links selected to delete","Delete links",MB_OK);
						return TRUE;
					}
					for(;;){
						di = ListView_GetNextItem(list, (UINT)-1, LVNI_SELECTED);
						if(di==-1)break;
						char valnam[_MAX_PATH+1];
						DWORD valnamsize,typ;
						char valbuf[_MAX_PATH+1];
						DWORD valbufsize,ret;
						char delbuf[_MAX_PATH+1] = "";
						ListView_GetItemText(list,di,0,delbuf,_MAX_PATH+1);
						_strupr(delbuf);
						ListView_DeleteItem(list,di);
						HKEY hk;
						if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",0,KEY_ALL_ACCESS,&hk)){
							for(DWORD i = 0;;i++){
								typ=0;
								valnamsize = valbufsize = _MAX_PATH+1;
								valbuf[0]=valnam[0]=0;
								ret = RegEnumValue(hk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
								_strupr(valbuf);
								if(typ==REG_SZ && valbuf[0]!=0 && !strcmp(valbuf,delbuf)){
									RegDeleteValue(hk,valnam);
									break;
								}
								if(ERROR_NO_MORE_ITEMS==ret)break;
							}
							RegCloseKey(hk);
							PathAddBackslash(delbuf);
							strcat(delbuf,"frhed.lnk");
							remove(delbuf);
						}
						SHDeleteEmptyKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links");
					}
					return TRUE;
				}
				case IDC_RELOAD:{
					//Reload links from the registry frhed\subreleaseno\links\ all values loaded & tested
					HKEY hk;
					char valnam[_MAX_PATH+1]="";
					DWORD valnamsize = _MAX_PATH+1,typ;
					char valbuf[_MAX_PATH+1]="";
					DWORD valbufsize = _MAX_PATH+1,ret;
					HWND list = GetDlgItem(hw,IDC_LIST);
					//Delete list
					ListView_DeleteAllItems(list);
					LVITEM item;
					ZeroMemory(&item,sizeof(item));
					item.mask=LVIF_TEXT;
					item.pszText = valbuf;
					if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",0,KEY_ALL_ACCESS,&hk)){
						//Load all the string values
						for(DWORD i = 0;;i++){
							typ=0;
							valnamsize = valbufsize = _MAX_PATH+1;
							valbuf[0]=valnam[0]=0;
							ret = RegEnumValue(hk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
							if(typ==REG_SZ && valbuf[0]!=0 && PathIsDirectory(valbuf)){//Valid dir
								//Add the string
								item.iItem = i;
								ListView_InsertItem(list, &item);
								PathAddBackslash(valbuf);
								strcat(valbuf,"frhed.lnk");
								CreateLink(_pgmptr,valbuf);
							}
							if(ERROR_NO_MORE_ITEMS==ret)break;
						}
						RegCloseKey(hk);
					}
					return TRUE;
				}
				case IDC_START:
				case IDC_PROGRAMS:
				case IDC_SENDTO:
				case IDC_DESKTOP:{
					//Create links in
					//HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\Start Menu = C:\WINDOWS\Start Menu on my computer
					//HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\Programs = C:\WINDOWS\Start Menu\Programs on my computer
					//HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\SendTo = C:\WINDOWS\SendTo on my computer
					//HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders\Desktop = C:\WINDOWS\Desktop on my computer
					HKEY hk;
					if(ERROR_SUCCESS==RegOpenKey(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",&hk)){
						char szDir[_MAX_PATH+1]="";
						DWORD len = _MAX_PATH+1;
						//Get the path from the registry
						switch(LOWORD(w)){
							case IDC_START:
								RegQueryValueEx(hk,"Start Menu",0,NULL,(BYTE*)szDir,&len);
							break;
							case IDC_PROGRAMS:
								RegQueryValueEx(hk,"Programs",0,NULL,(BYTE*)szDir,&len);
							break;
							case IDC_SENDTO:
								RegQueryValueEx(hk,"SendTo",0,NULL,(BYTE*)szDir,&len);
							break;
							case IDC_DESKTOP:
								RegQueryValueEx(hk,"Desktop",0,NULL,(BYTE*)szDir,&len);
							break;
						}
						RegCloseKey(hk);

						HWND list = GetDlgItem(hw,IDC_LIST);
						int num = ListView_GetItemCount(list);//get the # items in the list
						int done = 0;
						char path[_MAX_PATH+1]="";
						char buf[_MAX_PATH+1]="";
						strcpy(path,szDir);
						_strupr(path);
						for(int i=0;i<num;i++){//loop num times
							ListView_GetItemText(list,i,0,buf,_MAX_PATH+1);//get the string
							_strupr(buf);//convert to upper since strcmp is case sensitive & Win32 is not
							if(!strcmp(buf,path)){
								done = 1;
								break;
							}
						}//end of the loop
						if(done){
							MessageBox(hw,"There is already a link in that folder","Add",MB_OK);
							//Just in case
							PathAddBackslash(szDir);
							strcat(szDir,"frhed.lnk");
							CreateLink(_pgmptr,szDir);
						}
						else{
							LVITEM item;
							ZeroMemory(&item,sizeof(item));
							item.mask=LVIF_TEXT;
							item.pszText = szDir;
							item.iItem = num;
							ListView_InsertItem(list, &item);
							char valnam[_MAX_PATH+1];
							if(ERROR_SUCCESS==RegCreateKey(HKEY_CURRENT_USER, "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\links",&hk)){
								//Find a value name that does not exist
								for(DWORD i = 0;;i++){
									sprintf(valnam,"%d",i);
									if(ERROR_FILE_NOT_FOUND==RegQueryValueEx(hk,valnam,0,NULL,NULL,NULL)){
										RegSetValueEx(hk,valnam,0,REG_SZ,(BYTE*)szDir,strlen(szDir)+1);//Add the value to the registry
										break;
									}
								}
								RegCloseKey(hk);
								PathAddBackslash(szDir);
								strcat(szDir,"frhed.lnk");
								CreateLink(_pgmptr,szDir);
							}
						}
					}
				}
			}
			break;
		}//WM_COMMAND
		case WM_NOTIFY:{
			NMLVKEYDOWN& nmh = *((NMLVKEYDOWN*)l);
			if( nmh.hdr.idFrom == IDC_LIST && nmh.hdr.code==LVN_KEYDOWN && nmh.wVKey==VK_DELETE)
				SendMessage(hw,WM_COMMAND,MAKELONG(IDC_DELETE,BN_CLICKED),(LPARAM)GetDlgItem(hw,IDC_DELETE));
			break;
		}
	}//switch m
	return FALSE;
}

BOOL CALLBACK ChangeInstProc (HWND hw, UINT m, WPARAM w, LPARAM l){
	UNREFERENCED_PARAMETER( l );
	//God damn spinners make life easy
	switch(m){
		case WM_INITDIALOG:{
			//for both the spinners
			//iLoadInst is the max
			//iSaveInst is the min
			//g_iInstCount is the start pos
			LONG range = MAKELONG(iLoadInst,iSaveInst), pos = MAKELONG( g_iInstCount, 0);
			HWND hWndUpDown = GetDlgItem(hw, IDC_SINST);
			SendMessage( hWndUpDown, UDM_SETRANGE, 0L, range);
			SendMessage( hWndUpDown, UDM_SETPOS, 0L, pos);
			hWndUpDown = GetDlgItem(hw, IDC_LINST);
			SendMessage( hWndUpDown, UDM_SETRANGE, 0L, range);
			SendMessage( hWndUpDown, UDM_SETPOS, 0L, pos);
		}
		case WM_COMMAND:{
			switch (LOWORD (w)){
				case IDOK:{
					iLoadInst = SendDlgItemMessage(hw,IDC_LINST,UDM_GETPOS,0,0);
					iSaveInst = SendDlgItemMessage(hw,IDC_SINST,UDM_GETPOS,0,0);
					EndDialog(hw, 1);
					return TRUE;
				}
				case IDCANCEL:{
					EndDialog(hw, 0);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/*Recursively delete key for WinNT
Don't use this under Win9x
Don't use this to delete keys you know will/should have no subkeys
This recursively deletes subkeys of the key and then
returns the return value of RegDeleteKey(basekey,keynam)*/
LONG RegDeleteWinNTKey(HKEY basekey, char keynam[]){
	HKEY tmp;
	LONG res;
	res = RegOpenKeyEx(basekey, keynam, 0, KEY_READ, &tmp);
	if(res==ERROR_SUCCESS){
		char subkeynam[_MAX_PATH+1];
		DWORD subkeylen = _MAX_PATH+1;
		for(DWORD i = 0;; i++ ){//Delete subkeys for WinNT
			subkeynam[0] = 0;
			res = RegEnumKey(tmp,i,subkeynam,subkeylen);
			if(res==ERROR_NO_MORE_ITEMS)break;
			else{
				DWORD numsub;
				res = RegQueryInfoKey(tmp,NULL,NULL,NULL,&numsub,NULL,NULL,NULL,NULL,NULL,NULL,NULL);
				if(res==ERROR_SUCCESS && numsub>0)
					RegDeleteWinNTKey(tmp,subkeynam);//Recursively delete
				RegDeleteKey(tmp,subkeynam);
			}
		}
		RegCloseKey(tmp);
	}
	return RegDeleteKey(basekey,keynam);
}

LONG RegCopyValues(HKEY src,char*skey,HKEY dst,char* dkey){
	HKEY sk,dk;
	LONG res;

	res = RegCreateKey(dst,dkey,&dk);
	if(res==ERROR_SUCCESS)res = RegOpenKeyEx(src,skey,0,KEY_READ,&sk);
	else RegCloseKey(dk);

	if(res==ERROR_SUCCESS){
		char valnam[_MAX_PATH+1];
		DWORD valnamsize,typ;
		char valbuf[_MAX_PATH+1];
		DWORD valbufsize;
		for(DWORD i = 0;;i++){
			valnamsize = valbufsize = _MAX_PATH+1;
			valbuf[0]=valnam[0]=0;
			res = RegEnumValue(sk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
			if(ERROR_NO_MORE_ITEMS==res)break;
			if(res==ERROR_SUCCESS)
				RegSetValueEx(dk,valnam,0,typ,(BYTE*)valbuf,valbufsize);
		}
		RegCloseKey(dk);
		RegCloseKey(sk);
	}
	return res;
}

void ChangeSelVer(HWND hw, char* text);
void ChangeSelInst(HWND hw, char* text);
char DataRead = 0;

BOOL CALLBACK UpgradeDlgProc(HWND hw,UINT m,WPARAM w,LPARAM l){

	switch(m){
		case WM_INITDIALOG:{
			int i;
			HKEY hk;
			LONG res;
			char subkeynam[_MAX_PATH+1];
			LVITEM item;
			ZeroMemory(&item,sizeof(LVITEM));
			item.mask = LVIF_TEXT ;
			item.pszText = subkeynam;

			DWORD exstyle = LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT|LVS_EX_INFOTIP;
			HWND list = GetDlgItem(hw,IDC_VERS);
			ListView_DeleteColumn(list,0);
			ListView_DeleteAllItems(list);
			ListView_SetExtendedListViewStyle(list,exstyle);

			//Add a column
			LVCOLUMN col;
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH ;
			col.fmt = LVCFMT_LEFT;
			col.pszText = "HKCU\\Software\\frhed";
			col.cx = 165;
			ListView_InsertColumn(list,0,&col);

			//Fill the vers list with the various versions
			if(0==RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\frhed",0,KEY_ALL_ACCESS,&hk)){
				for(i=0;;i++){
					subkeynam[0]=0;
					res = RegEnumKey(hk,i,subkeynam,_MAX_PATH+1);
					if(res==ERROR_NO_MORE_ITEMS)break;
					else{
						item.iItem = i;
						ListView_InsertItem(list, &item);
					}
				}
				RegCloseKey(hk);
			}
			list = GetDlgItem(hw,IDC_INSTS);
			ListView_DeleteColumn(list,0);
			ListView_DeleteAllItems(list);
			ListView_SetExtendedListViewStyle(list,exstyle);

			list = GetDlgItem(hw,IDC_INSTDATA);
			ListView_DeleteColumn(list,1);
			ListView_DeleteColumn(list,0);
			ListView_DeleteAllItems(list);
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH;
			col.fmt = LVCFMT_LEFT;
			col.cx = 105;
			col.pszText = "Option";
			ListView_InsertColumn(list,0,&col);
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH;
			col.fmt = LVCFMT_LEFT;
			col.cx = 105;
			col.pszText = "Value";
			ListView_InsertColumn(list,1,&col);

			list = GetDlgItem(hw,IDC_LINKS);
			ListView_DeleteColumn(list,0);
			ListView_DeleteAllItems(list);
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH;
			col.fmt = LVCFMT_LEFT;
			col.cx = 155;
			col.pszText = "Links";
			ListView_InsertColumn(list,0,&col);

			list = GetDlgItem(hw,IDC_MRU);
			ListView_DeleteColumn(list,0);
			ListView_DeleteAllItems(list);
			ZeroMemory(&col,sizeof(col));
			col.mask = LVCF_TEXT|LVCF_WIDTH;
			col.fmt = LVCFMT_LEFT;
			col.cx = 185;
			col.pszText = "MRU Files";
			ListView_InsertColumn(list,0,&col);

			DataRead = 0;


		}//WM_INITDIALOG
		break;
		case WM_COMMAND:{
			HWND insts = GetDlgItem(hw,IDC_INSTS);
			char keynam[_MAX_PATH+1]="Software\\frhed\\";
			LVCOLUMN col;
			switch(LOWORD(w)){
				case IDCANCEL:{
					EndDialog(hw,0);
					return 0;
				}
				case IDC_COPY:{
					HKEY lk;
					HWND vers = GetDlgItem(hw,IDC_VERS);
					int i = ListView_GetNextItem(vers, (UINT)-1, LVNI_SELECTED);
					ListView_GetItemText(vers,i,0,&keynam[15],_MAX_PATH+1-15);
					if(!strcmp(&keynam[15],"v"CURRENT_VERSION"."SUB_RELEASE_NO)){
						//If that key was this version don't copy
						MessageBox(hw,
							"You can't copy the registry data of the selected version\n"
							"to the current one because it is the current one!", "Copy",MB_OK);
						return 0;
					}

					//Open the reg key to load from
					if(ERROR_SUCCESS==RegOpenKey(HKEY_CURRENT_USER,keynam,&lk)){
						char cver[_MAX_PATH+1]="Software\\frhed\\v"CURRENT_VERSION"."SUB_RELEASE_NO"\\";
						int i,numi=ListView_GetItemCount(insts),len,lenc=strlen(cver);
						strcat(keynam,"\\");
						for(i=0;i<numi;i++){
							if(ListView_GetCheckState(insts,i)){
								len = strlen(keynam);
								ListView_GetItemText(insts,i,0,&keynam[len],_MAX_PATH+1-len);//get the inst
								strcpy(&cver[lenc],&keynam[len]);
								RegCopyValues(HKEY_CURRENT_USER,keynam,HKEY_CURRENT_USER,cver);//copy the key
								keynam[len]=cver[lenc]=0;
							}//if cur inst checked
						}//loop insts
						RegCloseKey(lk);
						SendMessage(hw,WM_INITDIALOG,0,0);//Readd all the instances
					}
					else MessageBox(hw,"Could not open the selected version key","Copy",MB_OK);
				}
				return 0;
				case IDC_READ:{
					//Get the instance
					int i = ListView_GetNextItem(insts, (UINT)-1, LVNI_SELECTED);
					char text[_MAX_PATH+1];
					ListView_GetItemText(insts,i,0,text,_MAX_PATH+1);
					//Get the version
					ZeroMemory(&col,sizeof(col));
					col.mask = LVCF_TEXT;
					col.pszText = keynam;
					col.cchTextMax = _MAX_PATH+1;
					ListView_GetColumn(insts,0,&col);
					//Save the current instance
					int tmp = g_iInstCount;
					//Set the instance to read from
					g_iInstCount = atoi(text);
					//Read the data
					hexwnd.read_ini_data(keynam);
					//Reset the instance
					g_iInstCount = tmp;
				}
				break;
				case IDC_DELETE:{
					HWND vers = GetDlgItem(hw,IDC_VERS);
					int v,i,numv=ListView_GetItemCount(vers),numi=ListView_GetItemCount(insts),len;
					for(v=0;v<numv;v++){
						if(ListView_GetCheckState(vers,v)){
							for(i=0;i<numi;i++){
								if(ListView_GetCheckState(insts,i)){
									ListView_GetItemText(vers,v,0,&keynam[15],_MAX_PATH+1-15);//get the ver
									strcat(keynam,"\\");
									len = strlen(keynam);
									ListView_GetItemText(insts,i,0,&keynam[len],_MAX_PATH+1-len);//get the inst
									RegDeleteKey(HKEY_CURRENT_USER,keynam);//delete the key
									keynam[len-1]=0;//cut off the "\\<inst>"
									SHDeleteEmptyKey(HKEY_CURRENT_USER,keynam);//Delete an empty key
									if(!strcmp(&keynam[15],"v"CURRENT_VERSION"."SUB_RELEASE_NO))hexwnd.bSaveIni = 0;//If that key was this version don't save
								}//if cur inst checked
							}//loop insts
						}//if cur ver checked
					}//loop vers
					SendMessage(hw,WM_INITDIALOG,0,0);//Readd all the instances
				}
				break;
			}//switch ctrl id
		}//WM_COMMAND
		return 1;
		case WM_NOTIFY:{//WM_NOTIFYFORMAT
			NMHDR& nmh = *((LPNMHDR) l);
			if(nmh.code==LVN_DELETEALLITEMS)
				return TRUE;//Tell the system not to waste time
			if( nmh.code==LVN_ITEMCHANGED ){
				if(nmh.idFrom!=IDC_INSTS && nmh.idFrom!=IDC_VERS)
					return 0;//Bugger off if we don't have to update anything
				if(0==ListView_GetSelectedCount(nmh.hwndFrom))
					return 0;//Bugger off if no selection

				NMLISTVIEW& nml = *((NMLISTVIEW*)l);
				if(nml.uChanged == LVIF_STATE && (nml.uNewState&LVIS_SELECTED)==(nml.uOldState&LVIS_SELECTED))
					return 0;//Bugger off if the state hasn't changed and the item hasn't been de/selected

				char text[_MAX_PATH+1];
				int i = ListView_GetNextItem(nmh.hwndFrom, (UINT)-1, LVNI_SELECTED);
				ListView_GetItemText(nmh.hwndFrom,i,0,text,_MAX_PATH+1);

				switch(nmh.idFrom){
					case IDC_VERS:
						ChangeSelVer(hw, text);
					return 0;
					case IDC_INSTS:
						ChangeSelInst(hw, text);
					return 0;
				}//switch ctrl id

			}//if LVN_ITEMACTIVATE
			return 0;
		}//WM_NOTIFY
	}//switch m
	return FALSE;
}

struct DispDataStruct{
	int iTextColorValue,iBkColorValue,iSepColorValue,iSelTextColorValue,iSelBkColorValue,iBmkColor,//Color values
	iBytesPerLine,iOffsetLen,iFontSize,//signed integers
	iAutomaticBPL,bAutoOffsetLen,bOpenReadOnly,bMakeBackups,//Bool
	iWindowShowCmd,iCharacterSet;//Multiple different values
}DispData;

//Delete all items from all lists
//init the insts list
void ChangeSelVer(HWND hw, char* text){
	HWND insts = GetDlgItem(hw,IDC_INSTS);
	HWND instdata = GetDlgItem(hw,IDC_INSTDATA);
	HWND links = GetDlgItem(hw,IDC_LINKS);
	HWND mru = GetDlgItem(hw,IDC_MRU);

	ListView_DeleteAllItems(insts);
	ListView_DeleteColumn(insts,0);
	ListView_DeleteAllItems(instdata);
	ListView_DeleteAllItems(links);
	ListView_DeleteAllItems(mru);

	//Init the version number on the insts list header
	LVCOLUMN col;
	ZeroMemory(&col,sizeof(col));
	col.mask = LVCF_TEXT|LVCF_WIDTH;
	col.fmt = LVCFMT_LEFT;
	col.pszText = text;
	col.cx = 120;
	ListView_InsertColumn(insts,0,&col);

	char keyname[100];
	char subkeynam[_MAX_PATH+1];
	strcpy(keyname,"Software\\frhed\\");
	strcat(keyname,text);

	LVITEM item;
	ZeroMemory(&item,sizeof(LVITEM));
	item.mask = LVIF_TEXT ;
	item.pszText = subkeynam;
	HKEY hk;
	LONG res;

	//Fill the instance list with the various instances of the current selected version
	if(0==RegOpenKeyEx(HKEY_CURRENT_USER,keyname,0,KEY_ALL_ACCESS,&hk)){
		for(int i=0;;i++){
			res = RegEnumKey(hk,i,subkeynam,_MAX_PATH+1);
			if(res==ERROR_NO_MORE_ITEMS)break;
			else{
				int instno=0;
				if(StrToIntEx(subkeynam,STIF_DEFAULT,&instno)){
					item.iItem = i;
					ListView_InsertItem(insts, &item);
				}
			}
		}
		RegCloseKey(hk);
	}

	//Add all the links
	strcat(keyname,"\\links");
	char* valnam = subkeynam;
	char valbuf[_MAX_PATH+1];
	DWORD valnamsize,valbufsize,typ;
	item.pszText = valbuf;
	if(ERROR_SUCCESS==RegOpenKeyEx(HKEY_CURRENT_USER, keyname,0,KEY_ALL_ACCESS,&hk)){
		//Load all the string values
		for(DWORD i = 0;;i++){
			typ=0;
			valnamsize = valbufsize = _MAX_PATH+1;
			valbuf[0]=valnam[0]=0;
			res = RegEnumValue(hk,i,valnam,&valnamsize,0,&typ,(BYTE*) valbuf,&valbufsize);
			if(typ==REG_SZ && valbuf[0]!=0 ){
				//Add the string
				item.iItem = i;
				ListView_InsertItem(links, &item);
			}
			if(ERROR_NO_MORE_ITEMS==res)break;
		}
		RegCloseKey(hk);
	}

	DataRead = 0;
	HWND disp = GetDlgItem(hw,IDC_DISPLAY);
	InvalidateRect (disp, NULL, TRUE);
	UpdateWindow (disp);

}

void ChangeSelInst(HWND hw, char* text){
	HWND insts = GetDlgItem(hw,IDC_INSTS);
	HWND instdata = GetDlgItem(hw,IDC_INSTDATA);
	HWND mru = GetDlgItem(hw,IDC_MRU);

	ListView_DeleteAllItems(instdata);
	ListView_DeleteAllItems(mru);

	//Assemble the keyname
	char keynam[_MAX_PATH+1]="Software\\frhed\\";
	LVCOLUMN col;
	ZeroMemory(&col,sizeof(col));
	col.mask = LVCF_TEXT;
	col.pszText = &keynam[15];
	col.cchTextMax = _MAX_PATH+1-15;
	ListView_GetColumn(insts,0,&col);
	strcat(keynam,"\\");
	strcat(keynam,text);

	HKEY hk;
	if(0==RegOpenKeyEx(HKEY_CURRENT_USER,keynam,0,KEY_ALL_ACCESS,&hk)){
		//Add all the data
		char databuf[_MAX_PATH+1] = "";
		char szText[_MAX_PATH+1] = "";
		DWORD data[3];
		DWORD datasize;
		DWORD typ;

		int mrucount = 0;

		LVITEM item;
		ZeroMemory(&item,sizeof(item));
		item.mask = LVIF_TEXT ;
		item.iSubItem = 0;

		int i;

		char valnams[2][22][20]=
			{
				{//registry value names
					"iTextColorValue","iBkColorValue","iSepColorValue","iSelTextColorValue","iSelBkColorValue","iBmkColor",//Color values
					"iBytesPerLine","iOffsetLen","iFontSize","iWindowX","iWindowY","iWindowWidth","iWindowHeight","iMRU_count",//signed integers
					"iAutomaticBPL","bAutoOffsetLen","bOpenReadOnly","bMakeBackups",//Bool
					"iWindowShowCmd","iCharacterSet",//Multiple different values
					"TexteditorName","BrowserName"//Strings
				},
				{//names to go in the list box
					"Text Color","Back Color","Separator Color","Selected Text Color","Selected Back Color","Bookmark Color",//Color values
					"Bytes Per Line","Offset Len","Font Size","Window XPos","Window YPos","Window Width","Window Height","# MRU items",//signed integers
					"Automatic BPL","Auto Offset Len","Open Read Only","Make backups",//Bool
					"Window Show Cmd","Character Set",//Multiple different values
					"Text Editor Name","Browser Name"//Strings
				}
		};
		int* assignloc[22]={
			&DispData.iTextColorValue,&DispData.iBkColorValue,&DispData.iSepColorValue,&DispData.iSelTextColorValue,&DispData.iSelBkColorValue,&DispData.iBmkColor,//Color values
			&DispData.iBytesPerLine,&DispData.iOffsetLen,&DispData.iFontSize,NULL,NULL,NULL,NULL,NULL,//signed integers
			&DispData.iAutomaticBPL,&DispData.bAutoOffsetLen,&DispData.bOpenReadOnly,&DispData.bMakeBackups,//Bool
			&DispData.iWindowShowCmd,&DispData.iCharacterSet,//Multiple different values
			NULL,NULL//Strings
		};

		for(i=0;i<22;i++){
			datasize = _MAX_PATH+1;
			item.pszText = valnams[1][item.iItem];
			ListView_InsertItem(instdata, &item);
			item.pszText = valnams[0][item.iItem];
			RegQueryValueEx( hk, item.pszText, NULL,&typ, (BYTE*) &databuf[0], &datasize );
			if(assignloc[i])*assignloc[i]=*((int*)databuf);
			if(i==13)
				memcpy(&mrucount,databuf, sizeof( int));
			if(i<6){
				data[0]=databuf[0];data[0]&=0xff;//Or data[0] = (BYTE) data[0];
				data[1]=databuf[1];data[1]&=0xff;
				data[2]=databuf[2];data[2]&=0xff;
				sprintf(szText,"RGB - %u,%u,%u",data[0],data[1],data[2]);
			}

			else if(i<6+8){
				sprintf(szText,"%u",*((int*)databuf));
			}
			else if(i<6+8+4){
				if((int)databuf[0])strcpy(szText,"True");
				else strcpy(szText,"False");
			}
			else if(i<6+8+4+1){
				memcpy(&data[0],&databuf[0], sizeof( int));
				switch(data[0]){
					case SW_HIDE: strcpy(szText,"Hide");break;
					case SW_MINIMIZE: strcpy(szText,"Minimize");break;
					case SW_RESTORE: strcpy(szText,"Restore");break;
					case SW_SHOW: strcpy(szText,"Show");break;
					case SW_SHOWMAXIMIZED: strcpy(szText,"Show Maximized");break;
					case SW_SHOWMINIMIZED: strcpy(szText,"Show Minimized");break;
					case SW_SHOWMINNOACTIVE: strcpy(szText,"Show MinNoactive");break;
					case SW_SHOWNA: strcpy(szText,"Show NA");break;
					case SW_SHOWNOACTIVATE: strcpy(szText,"Show Noactivate");break;
					case SW_SHOWNORMAL: strcpy(szText,"Show Normal");break;
				}
			}
			else if(i<6+8+4+1+1){
				memcpy(&data[0],&databuf[0], sizeof (int));
				switch (data[0]){
					case ANSI_FIXED_FONT:strcpy (szText, "ANSI");break;
					case OEM_FIXED_FONT:strcpy (szText, "OEM");break;
				}
			}
			else if(i<6+8+4+1+1+2)
				strcpy(szText,databuf);
			ListView_SetItemText(instdata, item.iItem, 1, szText);
			item.iItem++;
		}

		ZeroMemory(&item,sizeof(LVITEM));
		item.mask = LVIF_TEXT ;
		item.pszText = databuf;

		//Add all the MRUs
		for( i = 1; i <= mrucount; i++ ){
			sprintf( szText, "MRU_File%d", i );
			datasize = _MAX_PATH + 1;
			RegQueryValueEx( hk, szText, NULL, NULL, (BYTE*) &databuf, &datasize );
			item.iItem = i-1;
			ListView_InsertItem(mru, &item);
		}

		//Paint the display box
		DataRead = 1;
		HWND disp = GetDlgItem(hw,IDC_DISPLAY);
		InvalidateRect (disp, NULL, TRUE);
		UpdateWindow (disp);

		RegCloseKey(hk);

	}
}
#define nibble2hex(c) ( (c) < 10 ? (c) + '0': (c) - 10 + 'a' )
#define NIBBLE2HEX(c) ( (c) < 10 ? (c) + '0': (c) - 10 + 'A' )

LRESULT CALLBACK DisplayProc( HWND hw, UINT m, WPARAM w, LPARAM l ){
	switch(m){
		case WM_PAINT :{
			PAINTSTRUCT ps;
			HDC dc = BeginPaint(hw,&ps);
			if(dc==NULL)return 0;
			if(!DataRead){EndPaint(hw,&ps);return 0;}

//-------------Draw the caption bar----------------------------------------
			RECT rt;
			GetClientRect(hw,&rt);
			rt.bottom=rt.top+18;
			DrawCaption(hw,dc,&rt,DC_ACTIVE|DC_ICON|DC_TEXT);
			rt.left = 150;rt.right = rt.left+16;rt.bottom--;rt.top++;
			UINT type;
			switch(DispData.iWindowShowCmd){
				case SW_HIDE:
				case SW_MINIMIZE:
				case SW_SHOWMINIMIZED:
				case SW_SHOWMINNOACTIVE:
					type = DFCS_CAPTIONMIN;
				break;
				case SW_RESTORE:
				case SW_SHOWNORMAL:
				case SW_SHOW:
				case SW_SHOWNA:
				case SW_SHOWNOACTIVATE:
					type = DFCS_CAPTIONRESTORE;
				break;
				case SW_SHOWMAXIMIZED:
					type = DFCS_CAPTIONMAX;
				break;
			}
			DrawFrameControl(dc,&rt,DFC_CAPTION,type);

//-------------Draw the status bar-----------------------------------------
			GetClientRect(hw,&rt);
			rt.top = rt.bottom-18;
			DrawEdge (dc, &rt, BDR_SUNKENOUTER, BF_RECT);
			HFONT fon = (HFONT) SendMessage(GetParent(hw),WM_GETFONT,0,0);
			HFONT ofon = (HFONT) SelectObject(dc,fon);
			char statusbuf[]="ANSI / READ";int i=0,len=11;
			if(DispData.iCharacterSet!=ANSI_FIXED_FONT){
				statusbuf[1]='O';
				statusbuf[2]='E';
				statusbuf[3]='M';
				i++;len--;
			}
			if(!DispData.bOpenReadOnly){
				statusbuf[7]=0;
				strcat(statusbuf,"OVR");
				len--;
			}
			SIZE s;
			GetTextExtentPoint32(dc,&statusbuf[i],len,&s);
			int mode = SetBkMode(dc,TRANSPARENT);
			UINT align = SetTextAlign(dc,TA_CENTER);
			TextOut(dc,(rt.left+rt.right)/2,(rt.top+rt.bottom-s.cy)/2,&statusbuf[i],len);
			SetTextAlign(dc,align);
			SetBkMode(dc,mode);
			SelectObject(dc,ofon);

//-------------Draw the border---------------------------------------------
			GetClientRect(hw,&rt);
			rt.top+=19;
			rt.bottom-=20;
			DrawEdge (dc, &rt, EDGE_SUNKEN, BF_RECT);

//-------------Draw hex contents-------------------------------------------
			//Print 1 row unselected, 1 row selected, 2 bookmarks & separators
			rt.left+=2;rt.top+=2;
			rt.right-=2;rt.bottom-=2;
			//Create the font & stick in the DC
			int nHeight = -MulDiv(DispData.iFontSize, GetDeviceCaps(dc, LOGPIXELSY), 72);
			int cset = ( DispData.iCharacterSet==ANSI_FIXED_FONT ? ANSI_CHARSET : OEM_CHARSET);
			fon = CreateFont (nHeight,0,0,0,0,0,0,0,cset,OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH | FF_DONTCARE,0);
			ofon = (HFONT) SelectObject (dc, fon);

			//Set the text & back colours for non-selected
			SetBkColor(dc,DispData.iBkColorValue);
			SetTextColor(dc,DispData.iTextColorValue);

			//Create the text
			int p;
			int tmp,mol;
			TEXTMETRIC tm;
			GetTextMetrics (dc, &tm);
			if(DispData.iAutomaticBPL){
				//Get the number of chars that will fit into the box
				tmp = len = (rt.right-rt.left)/tm.tmAveCharWidth;len++;//'\0'
				//Get the number of chars to print
				tmp -= DispData.iOffsetLen + 3;//3 spaces
				tmp /=4;//"ff " & "ÿ" = 4 chars so numchars = leftovers / 4
				if(tmp)for(mol = i = 0;i<tmp;mol++,i = i<<4 | 0x0f);//length of last offset
				else mol = 1;
				if( DispData.iOffsetLen > mol) mol = DispData.iOffsetLen;
				len = mol + 4 + 4 * tmp;
			}
			else{
				tmp = DispData.iBytesPerLine;
				if(tmp)for(mol = i = 0;i<tmp;mol++,i = i<<4 | 0x0f);//length of last offset
				else mol = 1;
				if( DispData.iOffsetLen > mol) mol = DispData.iOffsetLen;
				len = mol + 4 + 4 * tmp;
				//Offset len, 3 spaces & '\0', numchars * 4 (2 hex 1 space & character) - \0 not printed
			}
			char* linebuf = new char[len];
			if(!linebuf)return 0;
			//Offset & 2 spaces
			char sbuf[100];
			int tol = DispData.bAutoOffsetLen ? mol : DispData.iOffsetLen;
			sprintf (sbuf, "%%%d.%dx", tol, tol);
			sprintf (linebuf, sbuf, 0);
			p = strlen(linebuf);
			mol += 2;
			memset(linebuf+p,' ',mol-p);
			linebuf[mol]=0;
			p=strlen(linebuf);
			//numchars
			int ii;
			for(i=0;i<tmp;i++){
				ii = (i>>4)&0x0f;
				linebuf[p++] = (char)nibble2hex(ii);
				ii = i&0x0f;
				linebuf[p++] = (char)nibble2hex(ii);
				linebuf[p++] = ' ';
			}
			linebuf[p++] = ' ';
			for(i=0;i<tmp;i++){
				if	(
						(DispData.iCharacterSet == OEM_FIXED_FONT && i!=0) ||
						((i>=32 && i<=126) || (i>=160 && i<=255) || (i>=145 && i<=146))
					)
					linebuf[p++] = (char)i;
				else
					linebuf[p++] = '.';
			}
			linebuf[p] = 0;

			SetBkMode(dc,OPAQUE);

			//Draw the non-selected text
			ExtTextOut(dc,rt.left,rt.top,ETO_CLIPPED|ETO_OPAQUE,&rt, linebuf, p,NULL);

			rt.top-=nHeight;

			//Set the text & back colours for selected text
			SetBkColor(dc,DispData.iSelBkColorValue);
			SetTextColor(dc,DispData.iSelTextColorValue);

			//Create the text
			sprintf (linebuf, sbuf, tmp);
			p = strlen(linebuf);
			memset(linebuf+p,' ',mol-p);
			linebuf[mol]=0;
			mol -= 2;
			p=strlen(linebuf);
			//numchars
			for(i=0;i<tmp-1;i++){
				ii = ((tmp+i)>>4)&0x0f;
				linebuf[p++] = (char)nibble2hex(ii);
				ii = (tmp+i)&0x0f;
				linebuf[p++] = (char)nibble2hex(ii);
				linebuf[p++] = ' ';
			}
			linebuf[p++] = '_';
			linebuf[p++] = '_';
			linebuf[p++] = ' ';
			linebuf[p++] = ' ';
			for(i=0;i<tmp-1;i++){
				ii = tmp+i;
				if	(
						(DispData.iCharacterSet == OEM_FIXED_FONT && ii!=0) ||
						((ii>=32 && ii<=126) || (ii>=160 && ii<=255) || (ii>=145 && ii<=146))
					)
					linebuf[p++] = (char)ii;
				else
					linebuf[p++] = '.';
			}
			linebuf[p++] = ' ';
			linebuf[p] = 0;

			//Draw the selected text
			ExtTextOut(dc,rt.left,rt.top,ETO_CLIPPED,&rt, linebuf, p,NULL);
			delete[]linebuf;

			//Kill the font
			SelectObject (dc, ofon);
			DeleteObject(fon);

			rt.top+=nHeight;

			//Create the separator pen
			HPEN sp = CreatePen (PS_SOLID, 1, DispData.iSepColorValue);
			HPEN op = (HPEN) SelectObject (dc, sp);

			//Draw the separators
			int m;
			for (i = 0; i < (tmp / 4) + 1; i++){
				m = (mol+2)*tm.tmAveCharWidth - tm.tmAveCharWidth/2 + 3*tm.tmAveCharWidth*4*i;
				MoveToEx (dc, m, rt.top, NULL);
				LineTo (dc, m, rt.top-nHeight*2);
			}
			// Separator for chars.
			m = tm.tmAveCharWidth*(mol + 3 + tmp * 3) - 2;
			MoveToEx (dc, m, rt.top, NULL);
			LineTo (dc, m, rt.top-nHeight*2);
			// Second separator.
			MoveToEx (dc, m+2, rt.top, NULL);
			LineTo (dc, m+2, rt.top-nHeight*2);

			//Kill the separator pen
			SelectObject (dc, op);
			DeleteObject (sp);

			rt.bottom=rt.top-nHeight;

			//Create a brush for bookmarks
			HBRUSH bb = CreateSolidBrush( DispData.iBmkColor );
			// Mark hex.
			rt.left += tm.tmAveCharWidth*(mol+2);
			rt.right = rt.left + 2 * tm.tmAveCharWidth + 1;
			FrameRect( dc, &rt, bb );
			// Mark char.
			rt.left += tm.tmAveCharWidth*(tmp*3+1);
			rt.right = rt.left + tm.tmAveCharWidth + 1;
			FrameRect( dc, &rt, bb );

			//Kill the brush
			DeleteObject(bb);

			EndPaint(hw,&ps);
		}//WM_PAINT
		return 0;
	}//Switch
	return DefWindowProc(hw,m,w,l);
}

//--------------------------------------------------------------------------------------------
// Receives WM_NOTIFY messages and passes either them to their handler functions or
// processes them here.
void HexEditorWindow::status_bar_click (bool left){
	//User clicked the status bar

	//Find where the user clicked
	DWORD pos = GetMessagePos();
	POINTS ps = MAKEPOINTS(pos);
	POINT p = { ps.x, ps.y };

	//Find which status rect the user clicked in
	RECT rt;int np, i, n = -1;
	ScreenToClient(hwndStatusBar,&p);
	np = SendMessage(hwndStatusBar,SB_GETPARTS,0,0);
	for(i = 0; i < np; i++){
		SendMessage(hwndStatusBar,SB_GETRECT,i,(LPARAM)&rt);
		if(PtInRect(&rt,p)){
			n = i;
			break;//Can't be in > 1 rect at a time
		}
	}

	//Som variables used below
	int r, len, cn;
	char* text;
	SIZE s;
	HFONT fon[2];
	HDC dc;

	//Which rect are we in?
	if ( n == 0 || n == 1 ){
		//In one of the rects that requires the text & fonts
		//Initialize
		len = LOWORD(SendMessage(hwndStatusBar,SB_GETTEXTLENGTH,n,0));
		text = new char[len+1];
		if( !text ) return;//Leave if we can't get the text
		SendMessage(hwndStatusBar,SB_GETTEXT,n,(LPARAM)text);
		//This font stuff plagued me for ages but know that you
		//need to put the right font in the dc as it won't have the right one
		//after a GetDC - the status bar must put it in during a paint
		dc = GetDC(hwndStatusBar);
		fon[0] = (HFONT) SendMessage(hwndStatusBar, WM_GETFONT, 0, 0);
		fon[1] = (HFONT) SelectObject(dc, fon[0]);
		GetTextExtentPoint32(dc,text,len,&s);
	}

	//Do the requested function
	switch(n){

		case -1:{
			//The user clicked the status bar outside the rects
			//May want to add something useful here later
			//An easter egg will do for now
			int tmp = (IDYES==MessageBox(hwnd, "BOO","Scared ya, di'n' I, huh, huh?",MB_ICONQUESTION|MB_YESNO));
			MessageBox(hwnd, tmp?"Wwwwooooohhhhoooo!!! :-)":"Aww damn. ;-("," ",MB_ICONEXCLAMATION|MB_OK);
			break;
		}


		case 0:{
			//The user clicked in the 1st part - offset/bits/byte/word/dword

			//If there is a selection
			if(bSelected){
				//Maybe the user wants to change the selection/deselect
				p.y = s.cx;
				GetTextExtentPoint32(dc, text, 9, &s);//len "Selected:" = 9
				if( ( p.x > rt.left + p.y ) || ( p.x < rt.left + s.cx ) ){
					//In the space / on the "Selected:" bit - deselect
					if( left ){
						iCurNibble = 0;
						iCurByte = iGetStartOfSelection();
					} else {
						iCurNibble = 1;
						iCurByte = iGetEndOfSelection();
					}
					bSelected = FALSE;
					repaint();
				}
				else{ //In offsets - reselect
					CMD_select_block ();//This fixes up the status text by itself
				}
				break;//Skip to the deinitialisation code
			}

			//If the caret is on the END byte
			int flen = DataArray.GetLength() ;
			if( flen - iCurByte < 1){
				//The caret is on the END (__ ) byte
				//Just change offset/go to prev byte
				if(flen){
					if( left ) CMD_goto();
					else{ iCurByte--; repaint(); }
				}
				//or append (if file is zero size)
				else if ( !bReadOnly && !bSelected ) CMD_edit_append();
				break;//Skip to the deinitialisation code
			}

			//Get which bit the click is in
			//offset, bits, space, un/signed, values

			//Find the start of the un/signed bit
			char* st = text;
			for(i = 0; i < len; i++){
				if(text[i]=='\t' && text[i+1]=='\t'){
					text[i] = '\0';//Clip the string
					st = &text[i+2];//Remember the second part
					break;
				}
			}

			//There was no '\t\t' in the middle of the string - something went wrong
			if( text == st ) break;

			//Get the start of the un/signed bit in client coords
			len = strlen(st);
			GetTextExtentPoint32(dc, st, len, &s);
			r = rt.right - s.cx;

			//Set up stuff for GetTextExtentExPoint
			if(r < p.x){
				r = p.x - r;
				//Already know where the string starts
				//and how long it is
			}
			else{
				r = p.x - rt.left;
				st = text;
				len = strlen(st);
			}

			//Find which character the used clicked in.
			GetTextExtentExPoint(dc, st, len, r, &cn, NULL, &s);

			//In the right or left aligned area?
			if( st != text ){
				//We're in the right aligned area

				//Find the end of the Un/signed section
				for(i = 0; i < len && st[i] != ':'; i++);

				//Where are we?
				if( cn <= i ){
					//We're in the un/signed area
					//Switch from Unsigned <--> Signed
					bUnsignedView = !bUnsignedView;
					//Fix up the status text
					set_wnd_title();
				}
				else if( !bReadOnly && !bSelected ){
					//We're in the values

					//Determine the part in which you clicked - BYTE/WORD/DWORD
					//Lots of assumptions here
					iDecValDlgSize = 0x01;
					//Start from where the above loop stopped
					for( ; i < len; i++){
						//If clicked this character stop
						if( cn == i ) break;
						//Double the size enterable
						if( st[i] == ',' ) iDecValDlgSize <<= 1;
					}

					//Enter decimal value
					CMD_edit_enterdecimalvalue();
				}//In the values
			}//In right aligned stuff
			else{
				//We're not in the right aligned stuff

				//Are we in the left aligned stuff?
				if(p.x > rt.left+s.cx+2){
					//We're in the space between left & right sides
					//Select block, goto or internal status
					if ( left ) CMD_select_block();
					else
#ifdef _DEBUG
					/*In debug mode do the internal status thing
					if ctrl/alt/shift keys are down*/
					if( GetKeyState( VK_SHIFT ) & 0x8000 ||
						GetKeyState( VK_CONTROL ) & 0x8000 ||
						GetKeyState( VK_MENU ) & 0x8000 ) {
						command(IDM_INTERNALSTATUS);
					}
					else
#endif
						CMD_goto();
				}
				else{
					//We're in the left aligned stuff

					//Find the end of the offset
					for( i = 0; i < len && !( st[i]==' ' && st[i+1]==' ' && st[i+2]==' ' ) ; i++);

					//Find if we're in the offset
					if(cn <= i + 1){
						//In the Offset section
						//Goto or View settings or something else
						if ( left ) CMD_goto();
						else CMD_view_settings();
					}
					else if( !bReadOnly && !bSelected ){
						//In bits section

						//Find the end of " Bits="
						for( ; i < len && st[i]!='='; i++);

						//Are we in the actual bits or " Bits="
						if( cn <= i ){
							//In " Bits="
							CMD_manipulate_bits();
						}
						else{
							//In actual bits

							//This bit uses the old method because it is more accurate

							//Init for the below loops
							st = &text[i+1];
							char tmp = st[0];
							st[0] = '\0';
							len = strlen(text);
							GetTextExtentPoint32(dc,text,len,&s);
							r = rt.left+s.cx;
							st[0] = tmp;

							//Flip the equivalent of a "bit"
							tmp = 1;
							for(i = 0; i < 8; i++){
								GetTextExtentPoint32(dc,st,i+1,&s);
								if(p.x <= r+s.cx+1){//Better with <=
									if ( st[i] == '0' ) st[i] = '1';
									else if( st[i] == '1' ) st[i] = '0';
									tmp = 0;
									break;
								}
							}
							if(tmp){
								i = 7;
								if ( st[i] == '0' ) st[i] = '1';
								else if( st[i] == '1' ) st[i] = '0';
							}

							//Assemble all the "bits"
							DataArray[iCurByte] = 0x00;
							for( i = 0; i < 8; i++){
								if( st[i] == '1' ) DataArray[iCurByte] |= 0x01;
								if( i < 7 ) DataArray[iCurByte] <<= 1;
							}

							//Redraw the data & status bar etc
							bFilestatusChanged = m_iFileChanged = TRUE;
							repaint(BYTELINE);

						}//In actual bits
					}//In bits section
				}//In the left aligned stuff
			}//Not in the right aligned stuff
			break;
		}//In the first rect


		case 1:{
			//The user clicked in the 2nd part
			char area = 0;//Part - ANSI/OEM, READ/INS/OVR or L/B

			//Start of the string is at r - centered
			r = (rt.left+rt.right-s.cx)/2;

			//Find the one we clicked in using text functions
			if(p.x > r){
				//Find which character the user clicked in.
				r = p.x - r;
				GetTextExtentExPoint(dc, text, len, r, &cn, NULL, &s);
				//Find the part that the user clicked in.
				for(i = 0; i < len; i++){
					//If pos is in this character stop
					if ( cn == i ) break;
					//Increment the part
					if( text[i] == '/' ) area++;
				}
			}

			//Do what the user asked for
			switch(area){
				case 0:{//ANSI <--> OEM
					if(iCharacterSet==ANSI_FIXED_FONT)iCharacterSet=OEM_FIXED_FONT;
					else if(iCharacterSet==OEM_FIXED_FONT)iCharacterSet=ANSI_FIXED_FONT;
					save_ini_data ();
					//Repaint - this affects text drawing
					kill_focus ();
					set_focus ();
					break;
				}
				case 1:{//READ/INS/OVR
					if ( left ){//READ -> INS -> OVR -> READ...
						if(bReadOnly){ bReadOnly = 0; iInsertMode= 1;}
						else if(iInsertMode) iInsertMode = 0;
						else bReadOnly = 1;
					}
					else{//READ <- INS <- OVR <- READ...
						if(bReadOnly){ bReadOnly = 0; iInsertMode= 0;}
						else if(iInsertMode) bReadOnly = 1;
						else iInsertMode = 1;
					}
					break;
				}
				case 2:{// L <--> B
					//This is not a typo - these preprocessor directives are required to make optimum code
					#if ((!LITTLEENDIAN_MODE) == BIGENDIAN_MODE) && (LITTLEENDIAN_MODE == (!BIGENDIAN_MODE))
						//If this does not work replace with the below
						iBinaryMode = !iBinaryMode;
					#else
						if(iBinaryMode==LITTLEENDIAN_MODE)iBinaryMode=BIGENDIAN_MODE;
						else if(iBinaryMode==BIGENDIAN_MODE)iBinaryMode=LITTLEENDIAN_MODE;
					#endif
					break;
				}
			}//switch ANSI/READ/L

			//Fix up the status text
			//Since set_focus does this too don't call unless have too
			if( area != 0 ) set_wnd_title();

			break;
		}//2nd rect


		case 2:{
			//The user clicked in the third part - file size part - what should we do here?
			//Pabs thinking append, file properties (call shell/frhed) or something else?
			if ( left && !bReadOnly && !bSelected ) CMD_edit_append();
			else CMD_properties();
			/*or use OleCreatePropertyFrame to show the property sheet for that file
			or use the undocumented function below
			SHObjectProperties uFlags
			#define OPF_PRINTERNAME 0x01
			#define OPF_PATHNAME 0x02
			WINSHELLAPI BOOL WINAPI SHObjectProperties(HWND hwndOwner,UINT uFlags,LPCSTR lpstrName,LPCSTR lpstrParameters);*/
			break;

		}//File size part
	}//switch( which part )

	if( n == 0 || n == 1 ){
		//Deinitialize
		SelectObject(dc,fon[1]);
		ReleaseDC(hwndStatusBar,dc);
		delete [] text;
	}
}

void HexEditorWindow::CMD_adopt_colours()
{
	if( MessageBox( hwnd, "Really adopt the operating system colour scheme?", "frhed", MB_YESNO | MB_ICONQUESTION ) == IDYES )
	{
		iTextColorValue = GetSysColor(COLOR_WINDOWTEXT);
		iBkColorValue = GetSysColor(COLOR_WINDOW);
		iSelTextColorValue = GetSysColor(COLOR_HIGHLIGHTTEXT);
		iSelBkColorValue = GetSysColor(COLOR_HIGHLIGHT);
//		What should these be?
//		iSepColorValue = RGB( 192, 192, 192 );
//		iBmkColor = RGB( 255, 0, 0 );
		save_ini_data();
		repaint();
	}
}

BOOL CALLBACK MoveCopyDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l){
	UNREFERENCED_PARAMETER( l );
	switch(m){
		case WM_INITDIALOG:{
			char buf[30];
			sprintf (buf, "x%x", iMove1stEnd);
			SetDlgItemText (hw, IDC_1STOFFSET, buf);
			sprintf (buf, "x%x", iMove2ndEndorLen);
			SetDlgItemText (hw, IDC_2NDDELIM, buf);
			CheckDlgButton (hw, IDC_OTHEREND, BST_CHECKED);
			sprintf (buf, "x%x", iMovePos);
			SetDlgItemText (hw, IDC_MOVEMENT, buf);
			CheckDlgButton (hw, IDC_FPOS, BST_CHECKED);
			if(iMoveOpTyp==OPTYP_MOVE) CheckDlgButton (hw, IDC_MOVE, BST_CHECKED);
			else if(iMoveOpTyp==OPTYP_COPY) CheckDlgButton (hw, IDC_COPY, BST_CHECKED);
			SetFocus (GetDlgItem (hw, IDC_1STOFFSET));
			return FALSE;
		}//WM_INITDIALOG
		case WM_COMMAND:{
			switch(LOWORD(w)){
				case IDCANCEL:{
					EndDialog(hw,0);
					return TRUE;
				}
				case IDOK:{
					HWND cntrl;
					int i, r = 0, len;
					char* buf = NULL;

					const int dlgitems[3]={IDC_1STOFFSET,IDC_2NDDELIM,IDC_MOVEMENT};
					const int check[3]={0,IDC_LEN,IDC_FORWARD};
					int vals[3];

					for(int n = 0; n<3; n++){
						cntrl = GetDlgItem(hw,dlgitems[n]);
						len = GetWindowTextLength(cntrl)+10;
						buf = (char*)realloc(buf,len);
						if(!buf){
							MessageBox (hw, "Not enough memory", "Move/Copy", MB_OK | MB_ICONERROR);
							return 0;
						}
						buf[0]=0;
						GetWindowText(cntrl,buf,len);

						i=r=0;
						if ( buf[i] == '-' ){
							if( n>0 && IsDlgButtonChecked(hw,check[n]) ){
								// Relative jump. Read offset from next character on.
								i++; r=1;
							}
							else if(n){
								MessageBox (hw, "You have chosen an offset but it is negative, which is invalid.", "Move/Copy", MB_OK | MB_ICONERROR);
								return 0;
							}
						}

						if( sscanf( &buf[i], "x%x", &vals[n] ) == 0 ){
							// No fields assigned.
							if( sscanf ( &buf[i], "%d", &vals[n] ) == 0 ){
								// No fields assigned: badly formed number.
								char msg[]="The value in box number n cannot be recognized.";
								msg[24] = (char)('1'+n);
								MessageBox (hw, msg, "Move/Copy", MB_OK | MB_ICONERROR);
								return 0;
							}
						}
						if(r) vals[n] *=-1; //Negate
					}
					free(buf);
					buf=0;

					iMove1stEnd = vals[0];
					if(IsDlgButtonChecked(hw,IDC_OTHEREND)){
						iMove2ndEndorLen = *((unsigned int*)&vals[1]);
					}
					else{
						if(!vals[1]){
							MessageBox (hw, "Cannot move/copy a block of zero length", "Move/Copy", MB_OK | MB_ICONERROR);
							return 0;
						}

						if(vals[1]>0)vals[1]--;
						else vals[1]++;
						iMove2ndEndorLen = iMove1stEnd+vals[1];

					}

					if(iMove1stEnd<0 || iMove1stEnd>iMoveDataUpperBound || iMove2ndEndorLen<0 || iMove2ndEndorLen>iMoveDataUpperBound){
						MessageBox (hw, "The chosen block extends into non-existant data.", "Move/Copy", MB_OK | MB_ICONERROR);
						return 0;
					}

					if(iMove1stEnd>iMove2ndEndorLen)swap(iMove1stEnd,iMove2ndEndorLen);

					if(IsDlgButtonChecked(hw,IDC_FPOS)){
						iMovePos = vals[2];
					}
					else if(IsDlgButtonChecked(hw,IDC_LPOS)){
						iMovePos = iMove1stEnd + (vals[2]-iMove2ndEndorLen);
					}
					else{
						iMovePos = iMove1stEnd + vals[2];
					}

					iMoveOpTyp = IsDlgButtonChecked(hw,IDC_MOVE)? OPTYP_MOVE : OPTYP_COPY;

					if(iMovePos==iMove1stEnd && iMoveOpTyp==OPTYP_MOVE){
						MessageBox (hw, "The block was not moved!", "Move/Copy", MB_OK | MB_ICONEXCLAMATION);
						EndDialog(hw,0);
						return 0;
					}

					if( iMovePos<0 || ( iMoveOpTyp==OPTYP_MOVE ? iMovePos+iMove2ndEndorLen-iMove1stEnd>iMoveDataUpperBound : iMovePos > iMoveDataUpperBound+1 ) ){
						MessageBox (hw, "Cannot move/copy the block outside the data", "Move/Copy", MB_OK | MB_ICONERROR);
						return 0;
					}

					EndDialog(hw,1);
					return TRUE;
				}
			}//switch ctrl id
			return FALSE;
		}//WM_COMMAND
	}//switch m
	return FALSE;
}

//Reverse the bytes between (and including) a & b
void reverse_bytes(BYTE*a,BYTE*b){
	if(a==b)return;
	BYTE t;
	DWORD bb=b-a;
	DWORD c=bb;
	c=c/2-(c+1)%2;
	for(DWORD i=0;i<=c;i++){
		//swap the bytes
		t=a[i];
		a[i]=a[bb-i];
		a[bb-i]=t;
	}
}

void HexEditorWindow::CMD_move_copy(char noUI, bool redraw){
	if(bReadOnly) return;

	if(!noUI){
		if(bSelected){
			iMove1stEnd = iStartOfSelection;
			iMove2ndEndorLen = iEndOfSelection;
			if(iMove1stEnd>iMove2ndEndorLen)swap(iMove1stEnd,iMove2ndEndorLen);
		}
		else{ iMove1stEnd = iMove2ndEndorLen = iCurByte; }
	}
	iMoveDataUpperBound = LASTBYTE;

	if( noUI || DialogBox(hInstance, MAKEINTRESOURCE (IDD_MOVE_COPY), hwnd, (DLGPROC) MoveCopyDlgProc) ){
		/*Call like so
		iMove1stEnd = position of start of block to move;
		iMove2ndEndorLen = position of end of block to move;
		iMoveDistorPos = position of start of block after move;
		CMD_move(1);
		*/
		int dist;
		int len;
		//Make sure all the parameters are correct
		if( iMove1stEnd<0 || iMove1stEnd>iMoveDataUpperBound ||
			iMove2ndEndorLen<0 || iMove2ndEndorLen>iMoveDataUpperBound ||
			iMovePos<0
		)
			return;
		if(iMove1stEnd>iMove2ndEndorLen)swap(iMove1stEnd,iMove2ndEndorLen);
		dist = iMovePos-iMove1stEnd;
		if( iMoveOpTyp == OPTYP_COPY )
		{
			if(iMovePos > iMoveDataUpperBound+1)return;
			len = iMove2ndEndorLen-iMove1stEnd+1;
		}
		else if( iMoveOpTyp == OPTYP_MOVE )
		{
			if(!dist)return;
			if(iMove2ndEndorLen+dist>iMoveDataUpperBound)return;
		}

		SetCursor (LoadCursor (NULL, IDC_WAIT));

		if( iMoveOpTyp == OPTYP_COPY )
		{
			int clen = iMoveDataUpperBound+1;
			if(!DataArray.SetSize (clen+len))
			{
				SetCursor (LoadCursor (NULL, IDC_ARROW));
				MessageBox(hwnd,"Not enough memory", "Move/Copy",MB_OK|MB_ICONERROR);
				return;
			}
			else{
				DataArray.ExpandToSize();
				memmove(&DataArray[iMovePos+len],&DataArray[iMovePos],clen-iMovePos);
				if(iMovePos>iMove1stEnd && iMovePos<=iMove2ndEndorLen)
				{
					memcpy(&DataArray[iMovePos],&DataArray[iMove1stEnd],iMovePos-iMove1stEnd);
					memcpy(&DataArray[iMovePos+(iMovePos-iMove1stEnd)],&DataArray[iMovePos+len],iMove1stEnd+len-iMovePos);
				}
				else{
					int tmp;
					if(iMovePos<=iMove1stEnd)tmp = len;
					else if(iMovePos>iMove2ndEndorLen)tmp = 0;
					memcpy(&DataArray[iMovePos],&DataArray[iMove1stEnd+tmp],len);
				}
			}
		}
		else if( iMoveOpTyp ==  OPTYP_MOVE )
		{
			//Before we just made a copy of the block & inserted it after moving the rest into place
			//Now (thanx to a whole bunch of dice) I found out that 3 calls to reverse_bytes will do
			//it doesn't require extra memory - yay, but may take a long time if moving long distances

			//It doesn't matter what order these three calls are in, it works
			//in all cases but the values will need to be calculated differently
			int ms,me,//Start and end of the block we are moving
				os,oe,//  "    "   "   "  "  other block that is affected
				ts,te;//  "    "   "   "  "  total block
			ms=iMove1stEnd;me=iMove2ndEndorLen;
			if(iMovePos>iMove1stEnd){
				os=iMove2ndEndorLen+1;oe=iMove2ndEndorLen+dist;
			}
			else /*if(iMovePosorDist>iMove1stEnd)*/{//Commented out because iMovePosorDist==iMove1stEnd has been checked for above
				os=iMovePos;oe=iMove1stEnd-1;
			}
			ts=min(ms,os);te=max(me,oe);
			if(ms!=me)reverse_bytes(&DataArray[ms],&DataArray[me]);
			if(os!=oe)reverse_bytes(&DataArray[os],&DataArray[oe]);
			if(ts!=te)reverse_bytes(&DataArray[ts],&DataArray[te]);
		}

		if( bSelected ){
			//If the selection was inside the bit that was moved move it too
			if( iMove1stEnd<=iStartOfSelection && iMove1stEnd<=iEndOfSelection && iMove2ndEndorLen>=iStartOfSelection && iMove2ndEndorLen>=iEndOfSelection ){
				iStartOfSelection+=dist;iEndOfSelection+=dist;
			}
			else{ bSelected = FALSE; iCurByte=iStartOfSelection+dist;}//If the above is not true deselect - this may change when multiple selections are allowed
		}
		//Or if the current byte was in the move/copied bit move it too
		else if( iCurByte>=iMove1stEnd && iCurByte<=iMove2ndEndorLen ){
			iCurByte+=dist;
		}

		SetCursor (LoadCursor (NULL, IDC_ARROW));
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		if( redraw ){
			if( iMoveOpTyp == OPTYP_COPY ) update_for_new_datasize();
			else if( iMoveOpTyp == OPTYP_MOVE ) repaint();
		}
	}
}

BOOL CALLBACK ReverseDlgProc (HWND h, UINT m, WPARAM w, LPARAM l)
{
	UNREFERENCED_PARAMETER( l );
	switch (m)
	{
	case WM_INITDIALOG:
		{
			char buf[128];
			sprintf (buf, "x%x", iStartOfSelSetting);
			SetDlgItemText (h, IDC_EDIT1, buf);
			sprintf (buf, "x%x", iEndOfSelSetting);
			SetDlgItemText (h, IDC_EDIT2, buf);
			SetFocus (GetDlgItem (h, IDC_EDIT1));
			//Because we are using the Select block dialog template some things need to be changed
			SetWindowText(h,"Reverse bytes");
			#define IDC_STATIC      0xFFFF //Stupid bloody windows
			SetDlgItemText(h,IDC_STATIC,"Reverse bytes between and including");
			SetDlgItemText(h,IDC_STATIC2,"these two offsets (prefix x for hex)");
			return FALSE;
		}

	case WM_COMMAND:
		switch (LOWORD (w))
		{
		case IDOK:
			{
				char buf[128];
				if (GetDlgItemText (h, IDC_EDIT1, buf, 128) != 0)
				{
					if (sscanf (buf, "x%x", &iStartOfSelSetting) == 0)
					{
						if (sscanf (buf, "%d", &iStartOfSelSetting) == 0)
						{
							MessageBox (h, "Start offset not recognized.", "Reverse bytes", MB_OK | MB_ICONERROR);
							return 0;
						}
					}
				}
				if (GetDlgItemText (h, IDC_EDIT2, buf, 128) != 0)
				{
					if (sscanf (buf, "x%x", &iEndOfSelSetting) == 0)
						if (sscanf (buf, "%d", &iEndOfSelSetting) == 0)
						{
							MessageBox (h, "End offset not recognized.", "Reverse bytes", MB_OK | MB_ICONERROR);
							return 0;
						}
				}
				if(iEndOfSelSetting==iStartOfSelSetting){
					MessageBox (h, "Cannot reverse the order of one byte.", "Reverse bytes", MB_OK | MB_ICONERROR);
					return 0;
				}

				if(iStartOfSelSetting<0 || iStartOfSelSetting>iMoveDataUpperBound || iEndOfSelSetting<0 || iEndOfSelSetting>iMoveDataUpperBound){
					MessageBox (h, "The chosen block extends into non-existant data.\nThe offsets will be shifted to correct positions.", "Reverse bytes", MB_OK | MB_ICONERROR);
				}

				EndDialog (h, 1);
				return TRUE;
			}

		case IDCANCEL:
			EndDialog (h, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void HexEditorWindow::CMD_reverse()
{
	if(bSelected){
		iStartOfSelSetting = iStartOfSelection;
		iEndOfSelSetting = iEndOfSelection;
	}
	else iEndOfSelSetting = iStartOfSelSetting = iCurByte;
	iMoveDataUpperBound = LASTBYTE;
	if (DialogBox (hInstance, MAKEINTRESOURCE (IDD_SELECT_BLOCK_DIALOG), hwnd, (DLGPROC) ReverseDlgProc))
	{
		SetCursor (LoadCursor (NULL, IDC_WAIT));
		int maxb = LASTBYTE;
		if (iStartOfSelSetting<0)iStartOfSelSetting=0;
		if (iStartOfSelSetting>maxb)iStartOfSelSetting=maxb;
		if (iEndOfSelSetting<0)iEndOfSelSetting=0;
		if (iEndOfSelSetting>maxb)iEndOfSelSetting=maxb;
		if (iEndOfSelSetting<iStartOfSelSetting)swap(iEndOfSelSetting,iStartOfSelSetting);
		if(iStartOfSelSetting!=iEndOfSelSetting)reverse_bytes(&DataArray[iStartOfSelSetting],&DataArray[iEndOfSelSetting]);
		if( bSelected ){
			//If the selection was inside the bit that was reversed, then reverse it too
			if( iStartOfSelSetting<=iStartOfSelection && iStartOfSelSetting<=iEndOfSelection && iEndOfSelSetting>=iStartOfSelection && iEndOfSelSetting>=iEndOfSelection ){
				iStartOfSelection=iEndOfSelSetting-iStartOfSelection+iStartOfSelSetting;
				iEndOfSelection=iEndOfSelSetting-iEndOfSelection+iStartOfSelSetting;
			}
			else{ bSelected = FALSE; }//If the above is not true deselect - this may change when multiple selections are allowed
		}
		//Or if the current byte was in the reversed bytes reverse it too
		else if( iCurByte>=iStartOfSelSetting && iCurByte<=iEndOfSelSetting ){
			iCurByte=iEndOfSelSetting-iCurByte+iStartOfSelSetting;
			iCurNibble = !iCurNibble;
		}
		m_iFileChanged = TRUE;
		bFilestatusChanged = TRUE;
		SetCursor (LoadCursor (NULL, IDC_ARROW));
		repaint();
	}
}

//This gets a fully qualified long absolute filename from any other type of file name ON ANY Win32 PLATFORM damn stupid Micro$uck$ & bloody GetLongPathName
//It was copied and enhanced from an article by Jeffrey Richter available in the MSDN under "Periodicals\Periodicals 1997\Microsoft Systems Journal\May\Win32 Q & A" (search for GetLongPathName & choose the last entry)
DWORD GetLongPathNameWin32(LPCSTR lpszShortPath,LPSTR lpszLongPath, DWORD cchBuffer){

	/*Alternative methods to consider adding here
	GetLongPathName on Win98/NT5
	Recursive FindFirstFile ... FindClose calls
	Interrupt 21h Function 7160h Minor Code 2h
	- does exactly what we want
		- to the damn letter
		- even resolves SUBST's (or not if you only want the drive letter)
			- remember those from DOS?
	- but the MSDN doesn't say anything about whether or not it is supported
	  by WinNT4 (It is only listed in Win95 docs)*/

	LPSHELLFOLDER psfDesktop = NULL;
	ULONG chEaten = 0;
	LPITEMIDLIST pidlShellItem = NULL;
	char szLongPath[MAX_PATH] = "";
	char pszShortPathNameA[MAX_PATH] = "";

	//Make sure it is an absolute path
	_fullpath(pszShortPathNameA,lpszShortPath,MAX_PATH);

	//Allocate memory for the WCHAR version of the short path name
	DWORD len;len = strlen(pszShortPathNameA)+1;
	LPWSTR pszShortPathNameW; pszShortPathNameW = new WCHAR[len];
	if(!pszShortPathNameW) goto FAILED;

	//Convert to a WCHAR string
	if(!MultiByteToWideChar(CP_ACP,0,pszShortPathNameA,-1,pszShortPathNameW,len)) goto FAILED;

	// Get the Desktop's shell folder interface
	HRESULT hr;hr = SHGetDesktopFolder(&psfDesktop);
	if(hr!=NOERROR) goto FAILED;

	// Request an ID list (relative to the desktop) for the short pathname
	hr = psfDesktop->ParseDisplayName(NULL, NULL, pszShortPathNameW, &chEaten, &pidlShellItem, NULL);
	psfDesktop->Release(); // Release the desktop's IShellFolder
	delete [] pszShortPathNameW; pszShortPathNameW = NULL;//Since we don't need it anymore
	if(hr!=NOERROR) goto FAILED;

	// We did get an ID list, convert it to a long pathname
	int r;r = SHGetPathFromIDListA(pidlShellItem, szLongPath);

	// Free the ID list allocated by ParseDisplayName
	LPMALLOC pMalloc;pMalloc = NULL;
	SHGetMalloc(&pMalloc);
	pMalloc->Free(pidlShellItem);
	pMalloc->Release();

	//We got the ID list, but not the path
	if(r!=TRUE) goto FAILED;

	//Get the length of the string
	len = strlen(szLongPath)+1;

	//Copy the string over
	if(len<=cchBuffer){
		strcpy(lpszLongPath,szLongPath);
	}

	return len;

FAILED:
	if(lpszLongPath!=lpszShortPath)strncpy(lpszLongPath,lpszShortPath,cchBuffer);
	return 0;
}


int HexEditorWindow::CMD_setselection(int iSelStart, int iSelEnd) // Mike Funduc new function
{
	if ((iSelStart >= 0) && (iSelEnd >= iSelStart))
	{
		iStartOfSelection = iSelStart;
		iEndOfSelection = iSelEnd;
		bSelected = TRUE;
		adjust_view_for_selection();
			repaint ();
		return 1;
	}
	return 0;
}

/*
Returns a handle to a global memory pointer
Caller is responsible for passing the buck
or GlobalFree'ing the return value
#include <strstrea.h>
#include <iomanip.h>
*/
class HGlobalStream
{
	public:

		HGlobalStream(DWORD blcksz = 1024)//This block size was picked at random
		{
			_hex = _nbsp = _escfilt = 0;
			precision = 0;
			m_hGlobal = NULL;
			m_dwLen = m_dwSize = 0;
			m_dwBlockSize = blcksz;
		}

		~HGlobalStream()
		{
			if( m_hGlobal ) GlobalFree( m_hGlobal );
		}

		HGlobalStream& operator <<( const char* pszSource )
		{
			if( !_nbsp && !_escfilt )
				Realloc( strlen(pszSource), (void*)pszSource );
			else
				filter( pszSource );
			_hex = _nbsp = _escfilt = 0;
			return *this;
		}

		HGlobalStream& operator <<( const BYTE c )
		{
			if(_hex){
				BYTE nib[2] = { c>>4&0xf, c&0xf };
				nib[0] += nib[0]>=0xa ? 'a'-0xa:'0';
				nib[1] += nib[1]>=0xa ? 'a'-0xa:'0';
				Realloc( 2, (void*)&nib[0] );
			} else if(_escfilt||_nbsp){
				BYTE e[3]={'\\',c,1};
				BYTE* address = &e[1];
				switch(c){
					case ' ': if(_nbsp){e[1] = '~';e[2] = 2;address--;} break;
					case '\\': case '{': case '}': if(_escfilt){e[2] = 2;address--;} break;
				}
				Realloc( e[2], (void*)address );
			} else {
				Realloc( 1, (void*)&c );
			}
			_hex = _nbsp = _escfilt = 0;
			return *this;
		}

		HGlobalStream& operator <<( const DWORD i )
		{
			//Maximum size of an integer in hex is 8
			//Maximum size for an unsigned int is the length of 4294967295 (10)
			//+1 for the \0
#if( UINT_MAX>0xffffffff )
#error The buffer below needs increasing
#endif
			char integer[11];
			if(precision) sprintf( integer, _hex?"%*.*x":"%*.*u", precision, precision, i);
			else sprintf( integer, _hex?"%x":"%u", i);
			Realloc( strlen(integer), (void*)integer );
			_hex = _nbsp = _escfilt = 0;
			precision = 0;
			return *this;
		}

		HGlobalStream& operator <<( const int i )
		{
			//Maximum size of an integer in hex is 8
			//Maximum size for an int is the length of -2147483647 (11)
			//+1 for the \0
#if( UINT_MAX>0xffffffff )
#error The buffer below needs increasing
#endif
			char integer[12];
			sprintf( integer, _hex?"%x":"%d", i);
			Realloc( strlen(integer), (void*)integer );
			_hex = _nbsp = _escfilt = 0;
			return *this;
		}

		HGlobalStream& HGlobalStream::operator<<(HGlobalStream&(*_f)(HGlobalStream&)){
			(*_f)(*this);
			return *this;
		}

		void Realloc( DWORD len, void* src ){
			DWORD newlen = m_dwLen + (lastlen = len);
			DWORD newsize = (newlen / m_dwBlockSize + 1)*m_dwBlockSize;
			HGLOBAL hgTemp;
			if( newsize > m_dwSize ){
				if(m_hGlobal) hgTemp = GlobalReAlloc( m_hGlobal, newsize, GHND|GMEM_DDESHARE );
				else hgTemp = GlobalAlloc(GHND|GMEM_DDESHARE, newsize);
			} else hgTemp = m_hGlobal;
			if( hgTemp ){
				m_hGlobal = hgTemp;
				char* pTemp = (char*)GlobalLock( m_hGlobal );
				if( pTemp ){
					if( src ) memcpy( &pTemp[m_dwLen], src, lastlen );
					m_dwLen = newlen;
					GlobalUnlock( m_hGlobal );
				}
				m_dwSize = newsize;
			}
		}

		void filter( const char* src ){
			if( !src ) return;
			DWORD i = lastlen = 0;
			//Find out the length
			if( _nbsp && _escfilt ){
				for(; src[i]!='\0'; i++){
					switch(src[i]){ case '\\': case '{': case '}': case ' ': lastlen++; break; }
					lastlen++;
				}
			} else if( _escfilt ){
				for(; src[i]!='\0'; i++){
					switch(src[i]){ case '\\': case '{': case '}': lastlen++; break; }
					lastlen++;
				}
			} else if( _nbsp ){
				for(; src[i]!='\0'; i++){
					switch(src[i]){ case ' ': lastlen++; break; }
					lastlen++;
				}
			} else return;

			DWORD newlen = m_dwLen + lastlen;
			DWORD newsize = (newlen / m_dwBlockSize + 1)*m_dwBlockSize;
			HGLOBAL hgTemp;
			if( newsize > m_dwSize ){
				if(m_hGlobal) hgTemp = GlobalReAlloc( m_hGlobal, newsize, GHND|GMEM_DDESHARE );
				else hgTemp = GlobalAlloc(GHND|GMEM_DDESHARE, newsize);
			} else hgTemp = m_hGlobal;
			if( hgTemp ){
				m_hGlobal = hgTemp;
				char* pTemp = (char*)GlobalLock( m_hGlobal );
				if( pTemp ){
					pTemp += m_dwLen;
					DWORD ii = i = 0;
					//Filter the data
					if( _nbsp && _escfilt ){
						char c;
						for(; src[i]!='\0'; i++){
							switch(src[i]){
								case '\\': case '{': case '}': case ' ':
								pTemp[ii++] = '\\';
							}
							c = src[i];
							if( src[i] == ' ') c = '~';
							pTemp[ii++] = c;
						}
					} else if( _escfilt ){
						for(; src[i]!='\0'; i++){
							switch(src[i]){
								case '\\': case '{': case '}':
								pTemp[ii++] = '\\';
							}
							pTemp[ii++] = src[i];
						}
					} else if( _nbsp ){
						for(; src[i]!='\0'; i++){
							if( src[i] == ' '){
								pTemp[ii++] = '\\';
								pTemp[ii++] = '~';
							} else pTemp[ii++] = src[i];
						}
					}
					m_dwLen = newlen;
					GlobalUnlock( m_hGlobal );
				}
				m_dwSize = newsize;
			}
			_hex = _nbsp = _escfilt = 0;
		}

		HGLOBAL Relinquish()
		{
			HGLOBAL ret = m_hGlobal;
			m_hGlobal = NULL;
			m_dwLen = m_dwSize = 0;
			return ret;
		}

		void Reset()
		{
			if( m_hGlobal ){
				GlobalFree( m_hGlobal );
				m_hGlobal = NULL;
				m_dwLen = m_dwSize = 0;
			}
		}

		void setprecision( DWORD precis )
		{
			precision = precis;
		}

		unsigned _hex:1;
		unsigned _nbsp:1;
		unsigned _escfilt:1;
		DWORD precision;
		DWORD lastlen;
		DWORD nextlen;
		DWORD m_dwLen;
		DWORD m_dwSize;
		DWORD m_dwBlockSize;
		HGLOBAL m_hGlobal;
};

//Manipulators
HGlobalStream& hex( HGlobalStream& s ) {
	s._hex = 1;
	return s;
}

HGlobalStream& nbsp( HGlobalStream& s ) {
	s._nbsp = 1;
	return s;
}

HGlobalStream& escapefilter( HGlobalStream& s ) {
	s._escfilt = 1;
	return s;
}

HGLOBAL HexEditorWindow::RTF_hexdump(int start, int end, DWORD* plen){
	/*
	Similar to ostrstream, but homegrown & uses GlobalRealloc & GlobalFree
	can accept strings and integers, repeat strings and filter strings for
	characters to be converted to rtf escape sequences - like \ -> \\, { -> \{,  } -> \}...
	it will relinquish all responsibility for its data if you ask it to
	it does its data allocation in chunks to improve speed & frees excess on relinquish
	*/
	HGlobalStream s;//(1024);//That number is the chunk size to use when reallocating
	s <<
	//The whole of the RTF output code is formatted according to the structure of RTF
	"{\\rtf1\n"
		"\\ansi\n"
		"\\deff0\n"
		"{\\fonttbl\n"
			"{\\f0 ";
				//Get the charactersitics of the display font
				BYTE PitchAndFamily, CharSet;
				char* FaceName = NULL;

				HDC hdc = GetDC(hwnd);
				HFONT hFontOld = (HFONT)SelectObject(hdc, hFont);
				UINT cbData = GetOutlineTextMetrics (hdc, 0, NULL);
				OUTLINETEXTMETRIC* otm = NULL;
				if(cbData){
					otm = (OUTLINETEXTMETRIC*)LocalAlloc(LPTR, cbData);
					if(otm) GetOutlineTextMetrics(hdc, cbData, otm);
				}
				SelectObject(hdc, hFontOld);
				ReleaseDC(hwnd,hdc);

				if(otm){
					FaceName = (char*)otm+(UINT)otm->otmpFaceName;
					PitchAndFamily = otm->otmTextMetrics.tmPitchAndFamily;
					CharSet = otm->otmTextMetrics.tmCharSet;
				} else {
					LOGFONT lf;
					GetObject(hFont,sizeof(lf), &lf);
					PitchAndFamily = lf.lfPitchAndFamily;
					CharSet = lf.lfCharSet;
					cbData = GetTextFace(hdc, 0, NULL);
					if(cbData){
						FaceName = (char*)malloc( cbData );
						if(FaceName) GetTextFace(hdc, cbData, FaceName);
					}
				}

				//Output the font family,
				switch( PitchAndFamily & /* bits 4-7<<4 */ 0xf0 ){
					case FF_DECORATIVE: s << "\\fdecor "; break;
					case FF_DONTCARE:
					case FF_MODERN: s << "\\fmodern "; break;
					case FF_ROMAN: s << "\\froman "; break;
					case FF_SCRIPT: s << "\\fscript "; break;
					case FF_SWISS: s << "\\fswiss "; break;
					default: s << "\\fnil "; break;
					/*These have no equivalents in Win32 (or maybe it is my ancient M$DN library)
					case FF_TECH: s << "\\ftech "; break;
					case FF_BIDI: s << "\\fbidi "; break;
					*/
				}
				/*The following have no equivalents in Win32 (or maybe it is my ancient M$DN library)
				#define RTF_FCHARSET_INVALID 3
				#define RTF_FCHARSET_VIETNAMESE 163
				#define RTF_FCHARSET_ARABIC_TRADITIONAL 179
				#define RTF_FCHARSET_ARABIC_USER 180
				#define RTF_FCHARSET_HEBREW_USER 181
				#define RTF_FCHARSET_PC_437 254
				*/
				s <<
				//the character set,
				"\\fcharset" << (DWORD)CharSet << " " <<
				//the pitch type,
				"\\fprq" << (DWORD)(PitchAndFamily&0x3) << " ";
				if(otm){ s <<
					"{\\*\\panose " <<
						hex << (otm->otmPanoseNumber.bFamilyType) <<
						hex << (otm->otmPanoseNumber.bSerifStyle) <<
						hex << (otm->otmPanoseNumber.bWeight) <<
						hex << (otm->otmPanoseNumber.bProportion) <<
						hex << (otm->otmPanoseNumber.bContrast) <<
						hex << (otm->otmPanoseNumber.bStrokeVariation) <<
						hex << (otm->otmPanoseNumber.bArmStyle) <<
						hex << (otm->otmPanoseNumber.bLetterform) <<
						hex << (otm->otmPanoseNumber.bMidline) <<
						hex << (otm->otmPanoseNumber.bXHeight) <<
					"}";
				}
				//and the name of the font
				if(FaceName && FaceName[0]) s << escapefilter << FaceName;
				if(otm) LocalFree(LocalHandle(otm));
				else if(FaceName) free(FaceName);
				s <<

				/*Possible future stuff
				Embedded font data
				but why bother?
				since RTF sux anyway*/

			"}\n" // \f0

			//Font for information paragraphs
			"{\\f1 "
				"\\froman \\fcharset0 \\fprq2 Times New Roman"
			"}\n" // \f1

		"}\n" // \fonttbl

		"{\\colortbl\n" //The colour table is referenced by the document to change colours for various things
			/*Back colour*/"\\red" << (DWORD)GetRValue(iBkColorValue) << " \\green" << (DWORD)GetGValue(iBkColorValue) << " \\blue" << (DWORD)GetBValue(iBkColorValue) << ";\n"
			/*Text colour*/"\\red" << (DWORD)GetRValue(iTextColorValue) << " \\green" << (DWORD)GetGValue(iTextColorValue) << " \\blue" << (DWORD)GetBValue(iTextColorValue) << ";\n";
			if( bSelected ){ s << //Regular selection colours
				/*Sel bck col*/"\\red" << (DWORD)GetRValue(iSelBkColorValue) << " \\green" << (DWORD)GetGValue(iSelBkColorValue) << " \\blue" << (DWORD)GetBValue(iSelBkColorValue) << ";\n"
				/*Sel txt col*/"\\red" << (DWORD)GetRValue(iSelTextColorValue) << " \\green" << (DWORD)GetGValue(iSelTextColorValue) << " \\blue" << (DWORD)GetBValue(iSelTextColorValue) << ";\n";
			} else { //Caret is the text colour inverted
				//Wish I could do iBkColorValueTmp = InvertColour(iBkColorValue)
				iBkColorValue=~iBkColorValue;iTextColorValue=~iTextColorValue; s <<
				/*Car bck col*/"\\red" << (DWORD)GetRValue(iBkColorValue) << " \\green" << (DWORD)GetGValue(iBkColorValue) << " \\blue" << (DWORD)GetBValue(iBkColorValue) << ";\n"
				/*Car txt col*/"\\red" << (DWORD)GetRValue(iTextColorValue) << " \\green" << (DWORD)GetGValue(iTextColorValue) << " \\blue" << (DWORD)GetBValue(iTextColorValue) << ";\n";
				iBkColorValue=~iBkColorValue;iTextColorValue=~iTextColorValue;
			} s <<
			/*Bookmarks  */"\\red" << (DWORD)GetRValue(iBmkColor) << " \\green" << (DWORD)GetGValue(iBmkColor) << " \\blue" << (DWORD)GetBValue(iBmkColor) << ";\n"
			//Separators */iSepColorValue is not needed because drawing objects specify their own colours (stupid eh?)
		"}\n" // \colortbl

		//This is new for RTF 1.7, but it should be ignored by older readers so who cares (older than M$ Word XP = Word 2002??)
		"{\\*\\generator frhed v"CURRENT_VERSION"."SUB_RELEASE_NO";}\n"

		//Metadata here too?
		"{\\info\n"
			//Put the filename in the title
			"{\\title " << escapefilter << filename << "}\n"
			//...
		"}\n"; // \info

		//Document formatting properties
			//Sot sure if this will have any effect
			if( bMakeBackups ) s << "\\makebackup ";
			s <<
			//Turn off spelling & grammar checking in our hexdump
			"\\deflang1024 \\deflangfe1024 \\noproof "
			//Indicate that this document was created from text
			"\\fromtext "
			/*We use the 'Web/Online Layout View' at 100% cause it is good
			for drawing our vertical lines & offers minimal visual fluff*/
			"\\viewkind5 \\viewscale100"
			//...
		"\n" // DFPs

		//Section formatting properties
			//...
		//"\n"  SFPs - none yet - uncomment "\n" when we get some

		"{\n" //Paragraph specifying title
			//Times New Roman 20 pt
			"\\f1 \\fs40\n" <<
			//Print the file name
			escapefilter << filename <<
		"\\par\n}\n" // title para

		/*Nothing to put here yet
		"{\n" //Paragraph specifying other properties
			//Metadata
		"\\par\n}\n" // metadata para
		*/

		//Paragraph formatting properties
			//...
		//"\n"  PFPs - none yet - uncomment "\n" when we get some

		//Character formatting properties
			//Font 0 (hFont), font size, back colour 0 (iBkColorValue), iBkColorValue(Word2000 sux) text colour 1 (iTextColorValue)
			"\\f0 \\fs" << (DWORD)iFontSize*2 << "\\cb0 \\chcbpat0 \\cf1"
			//...
		"\n"; // CFPs


		/*Warning M$ Word 2000 (& probably all of them (versions of M$ Word)) will:
			For arbitrary byte values:
				convert them into underscores (generally the control characters)
				convert them to \'hh where hh is the hex value of the byte (above 0x80)
				convert them to \tab \lquote \rquote \bullet \endash \emdash \~
				insert a unicode character (eg \u129) before the actual character
			For bookmarks:
				replace non-alphanumerics with underscores
				place "BM" in front of bookmarks that begin with a number
				hide bookmarks beginning with underscores
					can still view them with "Hidden bookmarks" checked
			For the lines:
				convert them into the newer shp format
		*/


		//The actual data, complete with highlighted selection & bookmarks (highlight & real ones)
		if( bSelected ){
			iStartOfSelSetting = iStartOfSelection;
			iEndOfSelSetting = iEndOfSelection;
			if( iEndOfSelSetting < iStartOfSelSetting ) swap( iEndOfSelSetting, iStartOfSelSetting );
		} else { iStartOfSelSetting = iEndOfSelSetting = iCurByte; }
		int endoffile = LASTBYTE+1;
		if(start>endoffile)start=endoffile;
		if(end>endoffile)end=endoffile;
		start = start / iBytesPerLine * iBytesPerLine;//cut back to the line start
		end = end / iBytesPerLine * iBytesPerLine;//cut back to the line start
		register int i = 0;
		register int bi = 0;
		BYTE c;
		//This bit needs to be optimized
		bool highlights_in_this_line;
		for(register int l=start; l<=end; l+=iBytesPerLine){
			s.setprecision( iMinOffsetLen );
			s <<
			//Offsets
			hex << (DWORD)l;
			
			//Bytespace
			bi = s.lastlen;
			for(i=0;i<iMaxOffsetLen+iByteSpace-bi;i++)
				s << "\\~";

			highlights_in_this_line = false;

			/*Wish I could do this in C++ - the alias would expire like a local variable
			alias sos iStartOfSelSetting, eos iEndOfSelSetting, bpl iBytesPerLine;*/

			//If the current line has any part selected
			if( (iStartOfSelSetting <= l && iEndOfSelSetting >= l) || (iStartOfSelSetting >= l && iEndOfSelSetting < l+iBytesPerLine) || (iStartOfSelSetting < l+iBytesPerLine && iEndOfSelSetting >= l+iBytesPerLine) )
				highlights_in_this_line = true;
			//If the current line contains a bookmark
			else for( bi = 0; bi < iBmkCount; bi++ )
				if( IN_BOUNDS( pbmkList[i].offset, l, l+iBytesPerLine-1 ) ){ highlights_in_this_line = true; break; }

			//With highlights
			if( highlights_in_this_line ){
				int sosl = max(iStartOfSelSetting, l);
				int eosl = min(iEndOfSelSetting, l+iBytesPerLine-1);
				//Bytes
				for(i=l;i<l+iBytesPerLine;i++){
					if(i==sosl) s <<
					"{\\cb2 \\chcbpat2 \\cf3 "; // iSelBkColorValue, iSelBkColorValue (Word2000 sux), iSelTextColorValue
						for( bi = 0; bi < iBmkCount; bi++ )
							if(pbmkList[bi].offset==i) break;
						if( bi < iBmkCount ){
							if(m_iEnteringMode == BYTES){ s <<
								"{\\*\\bkmkstart ";
									if( pbmkList[bi].name ) s << escapefilter << pbmkList[bi].name;
									else s << (DWORD)i;
									s <<
								"}";
							} s <<
							"{\\chbrdr \\brdrs \\brdrcf4 "; // iBmkColor
						}
						if( i==endoffile ) s << "__\\~";
						else if( i>endoffile ) s << "\\~\\~\\~";
						else s << hex << DataArray[i];
						if( bi < iBmkCount ){ s <<
							"}";
							if(m_iEnteringMode == BYTES){ s <<
								"{\\*\\bkmkend ";
									if(pbmkList[bi].name) s << escapefilter << pbmkList[bi].name;
									else s << (DWORD)i;
									s <<
								"}";
							}
						}
						if(i==eosl) s <<
					"}"; s << //Selected colours
					"\\~";
				} //Bytes
				//Charspace
				for(i=0;i<iCharSpace;i++)
					s << "\\~";
				//Chars
				for(i=l;i<l+iBytesPerLine;i++){
					if(i==sosl) s <<
					"{\\cb2 \\chcbpat2 \\cf3 "; // iSelBkColorValue, iSelBkColorValue (Word2000 sux), iSelTextColorValue
						for( bi = 0; bi < iBmkCount; bi++ )
							if(pbmkList[bi].offset==i) break;
						if( bi < iBmkCount ){
							if(m_iEnteringMode == CHARS){ s <<
								"{\\*\\bkmkstart ";
									if( pbmkList[bi].name ) s << escapefilter << pbmkList[bi].name;
									else s << (DWORD)i;
									s <<
								"}";
							} s <<
							"{\\chbrdr \\brdrs \\brdrcf4 "; // iBmkColor
						}
						if( i>=endoffile ) s << "\\~";
						else {
							c = DataArray[i];
							if(!( ( iCharacterSet == OEM_FIXED_FONT && c != 0 ) || ( iCharacterSet == ANSI_FIXED_FONT && ( ( c >= 32 && c <= 126) || (c>=160 && c<=255) || (c>=145 && c<=146) ) ) ))
								c = '.';
							s << nbsp << escapefilter << c;
						}
						if( bi < iBmkCount ){ s <<
							"}";
							if(m_iEnteringMode == CHARS){ s <<
								"{\\*\\bkmkend ";
									if(pbmkList[bi].name) s << escapefilter << pbmkList[bi].name;
									else s << (DWORD)i;
									s <<
								"}";
							}
						}
						if(i==eosl) s <<
					"}"; //Selected colours
				} //Chars
				s <<
				//End of line
				"\\line\n";
			} else /*No highlights - how boring*/ {
				//Bytes
				for(i=l;i<l+iBytesPerLine;i++){
					if( i==endoffile ) s << "__\\~";
					else if( i>endoffile ) s << "\\~\\~\\~";
					else s << hex << DataArray[i] << "\\~";
				}
				//Charspace
				for(i=0;i<iCharSpace;i++)
					s << "\\~";
				//Chars
				for(i=l;i<l+iBytesPerLine;i++){
					if( i>=endoffile ) s << "\\~";
						else {
							c = DataArray[i];
							if(!( ( iCharacterSet == OEM_FIXED_FONT && c != 0 ) || ( iCharacterSet == ANSI_FIXED_FONT && ( ( c >= 32 && c <= 126) || (c>=160 && c<=255) || (c>=145 && c<=146) ) ) ))
								c = '.';
							s << nbsp << escapefilter << c;
						}
				}
				//End of line
				s <<
				"\\line\n";
			} // No highlights
		} //for each line

		s <<
		//The vertical lines
		//8192 is a majick number used to bring the lines in front of the text
		//Count is +4 because +2 for the 2 extra lines at charstart +1 for the extra one at the bytes end +1 cause \dpcount needs to be 1 more than the no of lines
		"{\\*\\do \\dobxcolumn \\dobypara \\dodhgt8192 \\dpgroup \\dpcount" << (DWORD)(iBytesPerLine/4+4) << "\n";
			register int x;
			register int y = ((end-start)/iBytesPerLine+1)*cyChar*15;
			//They should have just used the colour table
			//but no they is Micro$oft with absolutely no clue.
			register DWORD r = GetRValue(iSepColorValue);
			register DWORD g = GetGValue(iSepColorValue);
			register DWORD b = GetBValue(iSepColorValue);
			//The lines in the bytespace
			for (i = 0; i < iBytesPerLine / 4 +1; i++){
				x = (i*9+iMaxOffsetLen+iByteSpace-2)*cxChar*20+cxChar*10-20;
				s << //There are 20 twips per point
				//A line
				"\\dpline"
				//Positions of the ends
				" \\dpptx" << x << " \\dppty0 \\dpptx" << x << " \\dppty" << y <<
				//Repeat for the benefit of Word - fuck M$ sux
				" \\dpx" << x << " \\dpy0 \\dpxsize0 \\dpysize" << y <<
				//Solid lines
				" \\dplinesolid"
				//Colour of the lines - fuck M$ sux
				" \\dplinecor" << r << " \\dplinecog" << g << " \\dplinecob" << b << "\n";
			}
			x = (iMaxOffsetLen+iByteSpace+iBytesPerLine*9/4+iCharSpace-1)*cxChar*20-10*cxChar;//There are 20 twips per point
			//The two lines at the start of the charspace
			s << 
			//A line
			"\\dpline"
			//Positions of the ends
			" \\dpptx" << x-30 << " \\dppty0 \\dpptx" << x-30 << " \\dppty" << y <<
			//Repeat for the benefit of Word - fuck M$ sux
			" \\dpx" << x-30 << " \\dpy0 \\dpxsize0 \\dpysize" << y <<
			//Solid lines
			" \\dplinesolid"
			//Colour of the lines - fuck M$ sux
			" \\dplinecor" << r << " \\dplinecog" << g << " \\dplinecob" << b << "\n"

			//A line
			"\\dpline"
			//Positions of the ends
			" \\dpptx" << x << " \\dppty0 \\dpptx" << x << " \\dppty" << y <<
			//Repeat for the benefit of Word - fuck M$ sux
			" \\dpx" << x << " \\dpy0 \\dpxsize0 \\dpysize" << y <<
			//Solid lines
			" \\dplinesolid"
			//Colour of the lines - fuck M$ sux
			" \\dplinecor" << r << " \\dplinecog" << g << " \\dplinecob" << b << "\n"

			//End of group
			"\\dpendgroup\n"
		"}\n" // \do
	"}" << (BYTE)'\0'; // \rtf1
	if( plen ) *plen = s.m_dwLen;
	return s.Relinquish();
}


HexEditorWindow hexwnd;
