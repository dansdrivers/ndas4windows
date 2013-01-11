#ifndef hexwnd_h
#define hexwnd_h

#include "IDT.h"

// This is frhed vCURRENT_VERSION.SUB_RELEASE_NO
#include "version.h"

//--------------------------------------------------------------------------------------------
// Callback functions for dialogue boxes.
//Pabs changed - line insert
BOOL CALLBACK FillWithDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);//msg handler for fill with dialog
BOOL CALLBACK ShortcutsDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK ChangeInstProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK UpgradeDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l);
LRESULT CALLBACK DisplayProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK MoveCopyDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK ReverseDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK MultiDropDlgProc (HWND hw, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK DragDropDlgProc (HWND h, UINT m, WPARAM w, LPARAM l);
BOOL CALLBACK DragDropOptionsDlgProc (HWND h, UINT m, WPARAM w, LPARAM l);
//end
BOOL CALLBACK GoToDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK FindDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AboutDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CopyHexdumpDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK EnterDecimalValueDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK PasteDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CutDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CopyDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ViewSettingsDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AppendDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK BitManipDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK CharacterSetDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ChooseDiffDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK BinaryModeDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK SelectBlockDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AddBmkDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK RemoveBmkDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK OpenPartiallyDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK FastPasteDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK TmplDisplayDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ReplaceDlgProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam);

//--------------------------------------------------------------------------------------------
#define bitval(base,pos) ((base)[(pos)/8]&(1<<((pos)%8)))
#define ANSI_SET ANSI_FIXED_FONT
#define OEM_SET  OEM_FIXED_FONT
#define LITTLEENDIAN_MODE 0
#define BIGENDIAN_MODE 1
#define SCROLL_TIMER_ID 1
#define SCROLL_DELAY_TIMER_ID 2
#define MOUSE_OP_DELAY_TIMER_ID 3
#define MRUMAX 9
#define BMKMAX 9
#define BMKTEXTMAX 256
#define TPL_TYPE_MAXLEN 16
#define TPL_NAME_MAXLEN 128
#define FINDDLG_BUFLEN (64*1024)

typedef struct
{
	int one;
	int two;
} intpair;

typedef struct
{
	int offset;
	char* name;
} bookmark;

//--------------------------------------------------------------------------------------------
// Global variables.
#include "simparr.h"

HRESULT ResolveIt( HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath );
DWORD GetLongPathNameWin32(LPCSTR lpszShortPath,LPSTR lpszLongPath, DWORD cchBuffer);
//--------------------------------------------------------------------------------------------
enum ClickArea{AREA_NONE, AREA_OFFSETS, AREA_BYTES, AREA_CHARS};
enum SCROLL_TYPE{ SCROLL_NONE, SCROLL_BACK, SCROLL_FORWARD };

class HexEditorWindow
{
	friend interface CDropTarget;
public:
	int iGetCharMode();
	int iGetBinMode();
	int select_if_found_on_current_pos( SimpleString& finddata, int finddir, int do_repaint, char (*cmp) (char) );
	int transl_text_to_binary( SimpleString& in, SimpleArray<char>& out );
	int iGetStartOfSelection();
	int iGetEndOfSelection();
	int CMD_setselection(int iSelStart, int iSelEnd);// MF new function

	//GK20AUG2K
	void CMD_GotoDllExports();
	void CMD_GotoDllImports();
	void CMD_EncodeDecode();
	void CMD_OpenDrive();
	void CMD_OpenNDAS();
	void CMD_InfoNDAS();
	void CMD_DriveGotoFirstTrack();
	void CMD_DriveGotoNextTrack();
	void CMD_DriveGotoPrevTrack();
	void CMD_DriveGotoLastTrack();
	void CMD_DriveGotoTrackNumber();
	void CMD_DriveSaveTrack();
	void RefreshCurrentTrack();

	void CMD_CloseDrive();
	void OnContextMenu( int xPos, int yPos );

//Pabs changed - line insert
	bool dragging;
	char bSaveIni;//if 0 ini data is not saved on exit/whenever save_ini_data is called
	void CMD_fw();//fill selection with command
	void CMD_revert();
	void CMD_saveselas();
	void CMD_deletefile();
	void CMD_insertfile();
	void CMD_move_copy(char noUI = 0, bool redraw = 1);
	void CMD_reverse();
	int load_hexfile(void* hexin, BYTE cf);
	int CMD_open_hexdump ();
	void status_bar_click (bool left);
	void CMD_adopt_colours();
	
	//General OLEDD options
	int enable_drop;
	int enable_drag;
	int enable_scroll_delay_dd;
	int enable_scroll_delay_sel;
	int always_pick_move_copy;
	//Input OLEDD options
	int prefer_CF_HDROP;
	int prefer_CF_BINARYDATA;
	int prefer_CF_TEXT;
	//Output OLEDD options
	int output_CF_BINARYDATA;
	int output_CF_TEXT;
	int output_text_special;
	int output_text_hexdump_display;
	int output_CF_RTF;
	HGLOBAL RTF_hexdump(int start, int end, DWORD* plen = NULL);
	int CMD_OLEDD_options();
	CDropTarget* target;
	void start_mouse_operation();
	ClickArea get_pos(long x, long y);
	void set_drag_caret(long x, long y, bool Copying, bool Overwrite );
	void fix_scroll_timers(long x, long y);
	void kill_scroll_timers();
	bool dontdrop;
	int lbd_pos; ClickArea lbd_area;
	int nibblenum, bytenum, column, line, new_pos, old_pos, old_col, old_row;
	int bMouseOpDelayTimerSet;
	int bScrollDelayTimerSet;
	//int ScrollInset;
	int ScrollDelay;
	int ScrollInterval;
	int MouseOpDist;
	int MouseOpDelay;
	ClickArea area;
	SCROLL_TYPE prev_vert;
	SCROLL_TYPE prev_horz;

#ifdef USEMEMDC
	HDC mdc;// = 0; //Memory dc - allocated on create, killed on exit
	HBITMAP mbm,obm;//Memory & old bitmaps - allocated on create, killed on exit, killed & allocated on window resize
	char hbs;// = 0;//Has been resized
#endif //USEMEMDC
//end
	int replace_selected_data( SimpleString& replacedata, int do_repaint = TRUE );
	int find_and_select_data( SimpleString& finddata, int finddir, int do_repaint, char (*cmp) (char) );
	int transl_binary_to_text( SimpleString& dest, char* src, int len );
	void CMD_replace();
//Pabs removed CMD_explorersettings
	int OnWndMsg( HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam );
	int CMD_summon_text_edit();
	int CMD_findprev();
	int CMD_findnext();
	void CMD_colors_to_default();
	int CMD_goto();
	int read_tpl_token( char* pcTpl, int tpl_len, int& index, char* name );
	int ignore_non_code( char* pcTpl, int tpl_len, int& index );
	int apply_template_on_memory( char* pcTpl, int tpl_len, SimpleArray<char>& ResultArray );
	int apply_template( char* pcTemplate );
	int CMD_apply_template();
	int dropfiles( HANDLE hDrop );
	int CMD_fast_paste ();
	int CMD_open_partially ();
	int CMD_clear_all_bmk ();
	int CMD_remove_bkm ();
	int CMD_goto_bookmark (int cmd);
	int make_bookmark_list (HMENU menu);
	int CMD_add_bookmark ();
	int CMD_MRU_selected (int cmd);
	int make_MRU_list (HMENU menu);
	int update_MRU ();
	int CMD_select_block ();
	int timer (WPARAM w, LPARAM l);
	int CMD_binarymode ();
	inline char TranslateAnsiToOem (char c);
	int get_diffs (char* ps, int sl, char* pd, int dl, intpair* pdiff);
	int compare_arrays (char* ps, int sl, char* pd, int dl);
	int CMD_compare ();
	int CMD_properties ();
	int make_font ();
	int CMD_select_all ();
	int CMD_on_backspace ();
	int CMD_toggle_insertmode ();
	int CMD_on_deletekey ();
	int CMD_character_set ();
	//void destroy_backbuffer ();
	int CMD_manipulate_bits ();
	int CMD_edit_append ();
	int save_ini_data ();
//Pabs inserted "char* key = NULL"
	int read_ini_data (char* key = NULL);
//end
	int CMD_color_settings (COLORREF* pColor);
	int CMD_view_settings ();
	void adjust_view_for_selection ();
	int CMD_select_with_arrowkeys (int key);
	int CMD_open ();
	int CMD_save ();
	int CMD_save_as ();
	int CMD_new ();
	int CMD_edit_cut ();
	void update_for_new_datasize ();
	int byte_to_BC_destlen (char* src, int srclen);
	int translate_bytes_to_BC (char* pd, unsigned char* src, int srclen);
	int is_bytecode (char* src, int len);
	int calc_bctrans_destlen (char* src, int srclen);
	int translate_bytecode (char* dest, char* src, int srclen, int binmode=LITTLEENDIAN_MODE);
	int create_bc_translation (char* dest, char* src, int srclen, int charmode=ANSI_SET, int binmode=LITTLEENDIAN_MODE);
	int find_byte_pos (char* src, char c);
	int create_bc_translation (char** ppd, char* src, int srclen, int charmode=ANSI_SET, int binmode=LITTLEENDIAN_MODE);
	int CMD_edit_enterdecimalvalue ();
	int CMD_edit_paste ();
	int CMD_copy_hexdump (char* mem = NULL, int memlen = 0);
	int CMD_edit_copy ();
	int CMD_find ();
	int mousemove (int xPos, int yPos);
	int lbuttonup (int xPos, int yPos);
	int close ();
	int initmenupopup (WPARAM w, LPARAM l);
	void adjust_view_for_caret ();
	void print_line( HDC hdc, int line, char* linebuffer, HBRUSH hbr );
	void mark_char (HDC hdc);
	void adjust_hscrollbar ();
	void adjust_vscrollbar ();
	void clear_all ();
	int repaint (int line=-1);
	int repaint( int from, int to );//Pabs inserted
	HexEditorWindow();
	~HexEditorWindow();
	int load_file (char* fname);
	int file_is_loadable (char* fname);
	int at_window_create (HWND hw, HINSTANCE hI);
	int resize_window (int cx, int cy);
	int set_focus ();
	int kill_focus ();
	int lbuttondown ( int nFlags, int xPos, int yPos);
	int keydown (int key);
	int character (char ch);
	int vscroll (int cmd, int pos);
	int hscroll (int cmd, int pos);
	int paint ();
	int command (int cmd);
	int destroy_window ();
	void set_wnd_title ();
	void set_caret_pos ();

public:
	int iWindowShowCmd, iWindowX, iWindowY, iWindowWidth, iWindowHeight;

private:
//Pabs inserted
	int bMakeBackups;//Backup the file when saving
//end
	int bDontMarkCurrentPos;
	int bInsertingHex;
	SimpleString TexteditorName;
	SimpleString EncodeDlls;
	SimpleArray<char> Linebuffer;
	int iHexWidth;
	int bReadOnly, bOpenReadOnly;//Pabs inserted ", iPartialOpenLen, iPartialFileLen, bPartialStats"
	int iPartialOffset, bPartialOpen, iPartialOpenLen, iPartialFileLen, bPartialStats;
	int iBmkCount;
	bookmark pbmkList[BMKMAX];
	int iMRU_count;
	char strMRU[MRUMAX][_MAX_PATH+1];
	int bFilestatusChanged;
	int iUpdateLine, iUpdateToLine;//Pabs inserted iUpdateToLine
	int bScrollTimerSet;
	int iMouseX, iMouseY;
	int iBinaryMode;
	int bUnsignedView;
	int iFontSize;
	HFONT hFont;
	int iInsertMode;
	int iCharacterSet;
	int iTextColor, iBkColor;
	COLORREF iTextColorValue, iBkColorValue, iSepColorValue;
	COLORREF iSelBkColorValue, iSelBkColor, iSelTextColorValue, iSelTextColor;
	COLORREF iBmkColor;
	int iAutomaticBPL;
	int bFileNeverSaved;
	SimpleArray<unsigned char> DataArray;
	int bSelected;
//Pabs inserted "bSelecting, bMoving, bDroppedHere"
	int bLButtonDown, bSelecting, bMoving, bDroppedHere, iLBDownX, iLBDownY;
//end
	int iStartOfSelection, iEndOfSelection;
	int m_iEnteringMode, m_iFileChanged;
	int cxChar, cxCaps, cyChar, cxClient, cyClient, cxBuffer, cyBuffer, iNumlines,
		iVscrollMax, iVscrollPos, iVscrollInc,
		iHscrollMax, iHscrollPos, iHscrollInc,
		iCurLine, iCurByte, iCurNibble;
//Pabs replaced "iOffsetLen" with "iMinOffsetLen, iMaxOffsetLen, bAutoOffsetLen"
	int iMinOffsetLen, iMaxOffsetLen, bAutoOffsetLen, iByteSpace, iBytesPerLine, iCharSpace, iCharsPerLine;
	char *filename;
	HWND hwnd;
	HINSTANCE hInstance;
};

//--------------------------------------------------------------------------------------------
// MAKROS
//Pabs replaced iOffsetLen w iMaxOffsetLen
#define CHARSTART (iMaxOffsetLen + iByteSpace + iBytesPerLine * 3 + iCharSpace)
#define BYTEPOS (iCurByte % iBytesPerLine)
#define BYTELINE (iCurByte / iBytesPerLine)
#define BYTES_LOGICAL_COLUMN (iMaxOffsetLen + iByteSpace + BYTEPOS * 3 + iCurNibble)
//end
#define CHARS_LOGICAL_COLUMN (CHARSTART + BYTEPOS)
#define LAST_LOG_COLUMN (iHscrollPos + cxBuffer - 1) // Last visible logical column.
#define CURSOR_TOO_HIGH (iCurLine > BYTELINE)
#define CURSOR_TOO_LOW (iCurLine+cyBuffer-1 < BYTELINE)
#define LAST_VISIBLE_LINE (iCurLine+cyBuffer-1)
#define LASTLINE (iNumlines-1)
#define LASTBYTE (DataArray.GetUpperBound ())
#define STARTSELECTION_LINE (iStartOfSelection / iBytesPerLine)
#define ENDSELECTION_LINE (iEndOfSelection / iBytesPerLine)
#define IN_BOUNDS( i, a, b ) ( ( i >= a && i <= b ) || ( i >= b && i <= a ) )
#define NO_FILE (filename[0] == '\0')
#define BYTES 0 // for EnteringMode
#define CHARS 1
#define WM_F1DOWN (WM_USER+1)

//============================================================================================
// The main window object.
extern HexEditorWindow hexwnd;

extern HINSTANCE hMainInstance;
extern HWND hwndMain, hwndHex, hwndStatusBar, hwndToolBar;
extern char szHexClass[];
extern char szMainClass[];
extern int iMoveDataUpperBound, iMove1stEnd,iMove2ndEndorLen, iMovePos;
enum OPTYP{OPTYP_MOVE, OPTYP_COPY};
extern OPTYP iMoveOpTyp;
extern const CLIPFORMAT CF_BINARYDATA;
extern const CLIPFORMAT CF_RICH_TEXT_FORMAT;
#endif // hexwnd_h