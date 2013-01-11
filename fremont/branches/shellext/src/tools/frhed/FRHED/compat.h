#ifndef compat_h
#define compat_h

//For cygwin and those with old SDKs
#ifndef DD_DEFSCROLLINSET
#define DD_DEFSCROLLINSET ( 11 )
#endif
#ifndef DD_DEFSCROLLDELAY
#define DD_DEFSCROLLDELAY ( 50 )
#endif
#ifndef DD_DEFSCROLLINTERVAL
#define DD_DEFSCROLLINTERVAL ( 50 )
#endif
#ifndef DD_DEFDRAGDELAY
#define DD_DEFDRAGDELAY ( 200 )
#endif
#ifndef DD_DEFDRAGMINDIST
#define DD_DEFDRAGMINDIST ( 2 )
#endif

#ifndef CF_DIBV5
	#define CF_DIBV5 17
	#ifdef CF_MAX
		#if( CF_MAX == CF_DIBV5 )
			#undef CF_MAX
			#define CF_MAX 18
		#endif
	#endif
#endif

#ifndef _O_SHORT_LIVED
#define _O_SHORT_LIVED 0x1000
#endif

#ifndef GetEnhMetaFileBits
WINGDIAPI UINT WINAPI GetEnhMetaFileBits(HENHMETAFILE, UINT, LPBYTE);
#endif

#if !defined(_wopen) && !defined(_WIO_DEFINED)
int _wopen(const wchar_t*, int, ...);
#endif

/*
#ifndef _wremove
int _wremove (const wchar_t*);
#endif
*/

#ifndef TBN_FIRST
#define TBN_FIRST (0U-700U)
#endif

#ifndef TBN_GETINFOTIPA
#define TBN_GETINFOTIPA (TBN_FIRST - 18)
#endif

#ifndef TBN_GETINFOTIPW
#define TBN_GETINFOTIPW (TBN_FIRST - 19)
#endif

#ifndef LVS_EX_INFOTIP
#define LVS_EX_INFOTIP 0x00000400
#endif

typedef LV_KEYDOWN NMLVKEYDOWN;

typedef NM_LISTVIEW NMLISTVIEW;

#ifndef ListView_GetCheckState
#define ListView_GetCheckState(hwndLV, i) \
((SendMessage(hwndLV, LVM_GETITEMSTATE, i, LVIS_STATEIMAGEMASK) >> 12) - 1)
#endif

#ifndef ListView_SetCheckState
#define ListView_SetCheckState(hwndLV, i, fCheck) \
ListView_SetItemState(hwndLV, i, INDEXTOSTATEIMAGEMASK((fCheck)+1), LVIS_STATEIMAGEMASK)
#endif

#ifdef __CYGWIN__
typedef struct tagNMTBGETINFOTIPA
{
	NMHDR hdr;
	LPSTR pszText;
	int cchTextMax;
	int iItem;
	LPARAM lParam;
} NMTBGETINFOTIPA, *LPNMTBGETINFOTIPA;

typedef struct tagNMTBGETINFOTIPW
{
	NMHDR hdr;
	LPWSTR pszText;
	int cchTextMax;
	int iItem;
	LPARAM lParam;
} NMTBGETINFOTIPW, *LPNMTBGETINFOTIPW;

#ifdef UNICODE
	#define TBN_GETINFOTIP TBN_GETINFOTIPW
	#define NMTBGETINFOTIP NMTBGETINFOTIPW
	#define LPNMTBGETINFOTIP LPNMTBGETINFOTIPW
#else
	#define TBN_GETINFOTIP TBN_GETINFOTIPA
	#define NMTBGETINFOTIP NMTBGETINFOTIPA
	#define LPNMTBGETINFOTIP LPNMTBGETINFOTIPA
#endif

typedef struct _OSVERSIONINFOEXA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	CHAR szCSDVersion[128];
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wReserved[2];
} OSVERSIONINFOEXA, *POSVERSIONINFOEXA, *LPOSVERSIONINFOEXA;
typedef struct _OSVERSIONINFOEXW {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wReserved[2];
} OSVERSIONINFOEXW, *POSVERSIONINFOEXW, *LPOSVERSIONINFOEXW;
#ifdef UNICODE
	typedef OSVERSIONINFOEXW OSVERSIONINFOEX;
	typedef POSVERSIONINFOEXW POSVERSIONINFOEX;
	typedef LPOSVERSIONINFOEXW LPOSVERSIONINFOEX;
#else
	typedef OSVERSIONINFOEXA OSVERSIONINFOEX;
	typedef POSVERSIONINFOEXA POSVERSIONINFOEX;
	typedef LPOSVERSIONINFOEXA LPOSVERSIONINFOEX;
#endif // UNICODE

#endif //__CYGWIN__

#endif //compat_h
