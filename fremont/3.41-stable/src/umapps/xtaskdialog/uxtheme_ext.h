//---------------------------------------------------------------------------
//
// uxtheme.h - theming API header file.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//---------------------------------------------------------------------------

#ifndef _UXTHEME_H_
#define _UXTHEME_H_

#include <commctrl.h>
#include <SpecStrings.h>

namespace uxtheme {

FORCEINLINE BOOL UxThemeIsAvailable();
__inline VOID UxThemeInitialize();
__inline VOID UxThemeUninitialize();

#ifndef NM_LDOWN
#define NM_LDOWN                (NM_FIRST-20)
#define NM_RDOWN                (NM_FIRST-21)
#define NM_THEMECHANGED         (NM_FIRST-22)
#endif

#ifndef CCM_SETWINDOWTHEME
#define CCM_SETWINDOWTHEME      (CCM_FIRST + 0xb)
#define CCM_DPISCALE            (CCM_FIRST + 0xc) // wParam == Awareness
#endif

#ifndef TB_SETWINDOWTHEME
#define TB_SETWINDOWTHEME       CCM_SETWINDOWTHEME
#endif

#ifndef RB_GETBANDMARGINS
#define RB_GETBANDMARGINS   (WM_USER + 40)
#define RB_SETWINDOWTHEME       CCM_SETWINDOWTHEME
#endif

#ifndef TTM_SETWINDOWTHEME
#define TTM_SETWINDOWTHEME      CCM_SETWINDOWTHEME
#endif

#define THEMEAPI(funcname) HRESULT (STDAPICALLTYPE * funcname)
#define THEMEAPI_(type, funcname) type (STDAPICALLTYPE * funcname)

typedef HANDLE HTHEME;          // handle to a section of theme data for class

#define MAX_THEMECOLOR  64
#define MAX_THEMESIZE   64

THEMEAPI_(HTHEME, OpenThemeData)(
    HWND hwnd,
    LPCWSTR pszClassList
    );

#define OTD_FORCE_RECT_SIZING   0x00000001          // make all parts size to rect
#define OTD_NONCLIENT           0x00000002          // set if hTheme to be used for nonclient area
#define OTD_VALIDBITS           (OTD_FORCE_RECT_SIZING | \
                                 OTD_NONCLIENT)

THEMEAPI_(HTHEME, OpenThemeDataEx)(
    HWND hwnd,
    LPCWSTR pszClassList,
    DWORD dwFlags
    );

THEMEAPI(CloseThemeData)(
    HTHEME hTheme
    );

THEMEAPI(DrawThemeBackground)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __in_opt LPCRECT pClipRect
    );

//------------------------------------------------------------------------
//---- bits used in dwFlags of DTBGOPTS ----
#define DTBG_CLIPRECT           0x00000001  // rcClip has been specified
#define DTBG_DRAWSOLID          0x00000002  // DEPRECATED: draw transparent/alpha images as solid
#define DTBG_OMITBORDER         0x00000004  // don't draw border of part
#define DTBG_OMITCONTENT        0x00000008  // don't draw content area of part
#define DTBG_COMPUTINGREGION    0x00000010  // TRUE if calling to compute region
#define DTBG_MIRRORDC           0x00000020  // assume the hdc is mirrorred and
                                            // flip images as appropriate (currently 
                                            // only supported for bgtype=imagefile)
#define DTBG_NOMIRROR           0x00000040  // don't mirror the output, overrides everything else 
#define DTBG_VALIDBITS          (DTBG_CLIPRECT | \
                                 DTBG_DRAWSOLID | \
                                 DTBG_OMITBORDER | \
                                 DTBG_OMITCONTENT | \
                                 DTBG_COMPUTINGREGION | \
                                 DTBG_MIRRORDC | \
                                 DTBG_NOMIRROR)

typedef struct _DTBGOPTS
{
    DWORD dwSize;           // size of the struct
    DWORD dwFlags;          // which options have been specified
    RECT rcClip;            // clipping rectangle
} DTBGOPTS, *PDTBGOPTS;

THEMEAPI(DrawThemeBackgroundEx)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __in_opt const DTBGOPTS *pOptions
    );

//---------------------------------------------------------------------------
//----- DrawThemeText() flags ----
#define DTT_GRAYED              0x00000001          // draw a grayed-out string (this is deprecated)
#define DTT_FLAGS2VALIDBITS     (DTT_GRAYED)

THEMEAPI(DrawThemeText)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchText) LPCWSTR pszText,
    int cchText,
    DWORD dwTextFlags,
    DWORD dwTextFlags2,
    LPCRECT pRect
    );


//---------------------------------------------------------------------------
//
// DrawThemeTextEx
//

// Callback function used by DrawTextWithGlow instead of DrawTextW
typedef 
int
(WINAPI *DTT_CALLBACK_PROC)
(
    __in HDC hdc,
    __inout_ecount(cchText) LPWSTR pszText,
    __in int cchText,
    __inout LPRECT prc,
    __in UINT dwFlags,
    __in LPARAM lParam);

//---- bits used in dwFlags of DTTOPTS ----
#define DTT_TEXTCOLOR       (1UL << 0)      // crText has been specified
#define DTT_BORDERCOLOR     (1UL << 1)      // crBorder has been specified
#define DTT_SHADOWCOLOR     (1UL << 2)      // crShadow has been specified
#define DTT_SHADOWTYPE      (1UL << 3)      // iTextShadowType has been specified
#define DTT_SHADOWOFFSET    (1UL << 4)      // ptShadowOffset has been specified
#define DTT_BORDERSIZE      (1UL << 5)      // iBorderSize has been specified
#define DTT_FONTPROP        (1UL << 6)      // iFontPropId has been specified
#define DTT_COLORPROP       (1UL << 7)      // iColorPropId has been specified
#define DTT_STATEID         (1UL << 8)      // IStateId has been specified
#define DTT_CALCRECT        (1UL << 9)      // Use pRect as and in/out parameter
#define DTT_APPLYOVERLAY    (1UL << 10)     // fApplyOverlay has been specified
#define DTT_GLOWSIZE        (1UL << 11)     // iGlowSize has been specified
#define DTT_CALLBACK        (1UL << 12)     // pfnDrawTextCallback has been specified
#define DTT_COMPOSITED      (1UL << 13)     // Draws text with antialiased alpha (needs a DIB section)
#define DTT_VALIDBITS       (DTT_TEXTCOLOR | \
                             DTT_BORDERCOLOR | \
                             DTT_SHADOWCOLOR | \
                             DTT_SHADOWTYPE | \
                             DTT_SHADOWOFFSET | \
                             DTT_BORDERSIZE | \
                             DTT_FONTPROP | \
                             DTT_COLORPROP | \
                             DTT_STATEID | \
                             DTT_CALCRECT | \
                             DTT_APPLYOVERLAY | \
                             DTT_GLOWSIZE | \
                             DTT_COMPOSITED)

typedef struct _DTTOPTS
{
    DWORD             dwSize;              // size of the struct
    DWORD             dwFlags;             // which options have been specified
    COLORREF          crText;              // color to use for text fill
    COLORREF          crBorder;            // color to use for text outline
    COLORREF          crShadow;            // color to use for text shadow
    int               iTextShadowType;     // TST_SINGLE or TST_CONTINUOUS
    POINT             ptShadowOffset;      // where shadow is drawn (relative to text)
    int               iBorderSize;         // Border radius around text
    int               iFontPropId;         // Font property to use for the text instead of TMT_FONT
    int               iColorPropId;        // Color property to use for the text instead of TMT_TEXTCOLOR
    int               iStateId;            // Alternate state id
    BOOL              fApplyOverlay;       // Overlay text on top of any text effect?
    int               iGlowSize;           // Glow radious around text
    DTT_CALLBACK_PROC pfnDrawTextCallback; // Callback for DrawText
    LPARAM            lParam;              // Parameter for callback
} DTTOPTS, *PDTTOPTS; 

THEMEAPI
(DrawThemeTextEx)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchText) LPCWSTR pszText,
    int cchText,
    DWORD dwTextFlags,
    __inout LPRECT pRect,
    __in_opt const DTTOPTS *pOptions
    );

THEMEAPI
(GetThemeBackgroundContentRect)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pBoundingRect,
    __out LPRECT pContentRect
    );

THEMEAPI
(GetThemeBackgroundExtent)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pContentRect,
    __out LPRECT pExtentRect
    );

THEMEAPI
(GetThemeBackgroundRegion)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    __out HRGN *pRegion
    );

enum THEMESIZE
{
    TS_MIN,             // minimum size
    TS_TRUE,            // size without stretching
    TS_DRAW             // size that theme mgr will use to draw part
};

THEMEAPI
(GetThemePartSize)(
    HTHEME hTheme,
    __in_opt HDC hdc,
    int iPartId,
    int iStateId,
    __in_opt LPCRECT prc,
    enum THEMESIZE eSize,
    __out SIZE *psz
    );

THEMEAPI
(GetThemeTextExtent)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __in_ecount(cchCharCount) LPCWSTR pszText,
    int cchCharCount,
    DWORD dwTextFlags,
    __in_opt LPCRECT pBoundingRect,
    __out LPRECT pExtentRect
    );

THEMEAPI
(GetThemeTextMetrics)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    __out TEXTMETRICW *ptm
    );

//-------------------------------------------------------------------------
//----- HitTestThemeBackground, HitTestThemeBackgroundRegion flags ----

//  Theme background segment hit test flag (default). possible return values are:
//  HTCLIENT: hit test succeeded in the middle background segment
//  HTTOP, HTLEFT, HTTOPLEFT, etc:  // hit test succeeded in the the respective theme background segment.
#define HTTB_BACKGROUNDSEG          0x00000000
//  Fixed border hit test option.  possible return values are:
//  HTCLIENT: hit test succeeded in the middle background segment
//  HTBORDER: hit test succeeded in any other background segment
#define HTTB_FIXEDBORDER            0x00000002      // Return code may be either HTCLIENT or HTBORDER. 
//  Caption hit test option.  Possible return values are:
//  HTCAPTION: hit test succeeded in the top, top left, or top right background segments
//  HTNOWHERE or another return code, depending on absence or presence of accompanying flags, resp.
#define HTTB_CAPTION                0x00000004
//  Resizing border hit test flags.  Possible return values are:
//  HTCLIENT: hit test succeeded in middle background segment
//  HTTOP, HTTOPLEFT, HTLEFT, HTRIGHT, etc:    hit test succeeded in the respective system resizing zone
//  HTBORDER: hit test failed in middle segment and resizing zones, but succeeded in a background border segment
#define HTTB_RESIZINGBORDER_LEFT    0x00000010      // Hit test left resizing border, 
#define HTTB_RESIZINGBORDER_TOP     0x00000020      // Hit test top resizing border
#define HTTB_RESIZINGBORDER_RIGHT   0x00000040      // Hit test right resizing border
#define HTTB_RESIZINGBORDER_BOTTOM  0x00000080      // Hit test bottom resizing border
#define HTTB_RESIZINGBORDER         (HTTB_RESIZINGBORDER_LEFT | \
                                     HTTB_RESIZINGBORDER_TOP | \
                                     HTTB_RESIZINGBORDER_RIGHT | \
                                     HTTB_RESIZINGBORDER_BOTTOM)
// Resizing border is specified as a template, not just window edges.
// This option is mutually exclusive with HTTB_SYSTEMSIZINGWIDTH; HTTB_SIZINGTEMPLATE takes precedence  
#define HTTB_SIZINGTEMPLATE         0x00000100
// Use system resizing border width rather than theme content margins.   
// This option is mutually exclusive with HTTB_SIZINGTEMPLATE, which takes precedence.
#define HTTB_SYSTEMSIZINGMARGINS    0x00000200

THEMEAPI
(HitTestThemeBackground)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    DWORD dwOptions,
    LPCRECT pRect,
    HRGN hrgn,
    POINT ptTest,
    __out WORD *pwHitTestCode
    );

THEMEAPI
(DrawThemeEdge)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pDestRect,
    UINT uEdge,
    UINT uFlags,
    __out_opt LPRECT pContentRect
    );

THEMEAPI
(DrawThemeIcon)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    LPCRECT pRect,
    HIMAGELIST himl,
    int iImageIndex
    );

THEMEAPI_(BOOL, IsThemePartDefined)(
    HTHEME hTheme,
    int iPartId,
    int iStateId
    );

THEMEAPI_(BOOL, IsThemeBackgroundPartiallyTransparent)(
    HTHEME hTheme,
    int iPartId,
    int iStateId
    );

THEMEAPI
(GetThemeColor)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out COLORREF *pColor
    );

THEMEAPI
(GetThemeMetric)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
(GetThemeString)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out_ecount(cchMaxBuffChars) LPWSTR pszBuff,
    int cchMaxBuffChars
    );

THEMEAPI
(GetThemeBool)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out BOOL *pfVal
    );

THEMEAPI
(GetThemeInt)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
(GetThemeEnumValue)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out int *piVal
    );

THEMEAPI
(GetThemePosition)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out POINT *pPoint
    );

THEMEAPI
(GetThemeFont)(
    HTHEME hTheme,
    HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __out LOGFONTW *pFont
    );

THEMEAPI
(GetThemeRect)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out LPRECT pRect
    );

typedef struct _MARGINS
{
    int cxLeftWidth;      // width of left border that retains its size
    int cxRightWidth;     // width of right border that retains its size
    int cyTopHeight;      // height of top border that retains its size
    int cyBottomHeight;   // height of bottom border that retains its size
} MARGINS, *PMARGINS;

THEMEAPI
(GetThemeMargins)(
    HTHEME hTheme,
    __in_opt HDC hdc,
    int iPartId,
    int iStateId,
    int iPropId,
    __in_opt LPCRECT prc,
    __out MARGINS *pMargins
    );

#if WINVER >= 0x0600
#define MAX_INTLIST_COUNT 402
#else
#define MAX_INTLIST_COUNT 10
#endif

typedef struct _INTLIST
{
    int iValueCount;      // number of values in iValues
    int iValues[MAX_INTLIST_COUNT];
} INTLIST, *PINTLIST;

THEMEAPI
(GetThemeIntList)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out INTLIST *pIntList
    );

enum PROPERTYORIGIN
{
    PO_STATE,           // property was found in the state section
    PO_PART,            // property was found in the part section
    PO_CLASS,           // property was found in the class section
    PO_GLOBAL,          // property was found in [globals] section
    PO_NOTFOUND         // property was not found
};

THEMEAPI
(GetThemePropertyOrigin)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out enum PROPERTYORIGIN *pOrigin
    );

THEMEAPI
(SetWindowTheme)(
    HWND hwnd,
    LPCWSTR pszSubAppName,
    LPCWSTR pszSubIdList
    );

enum WINDOWTHEMEATTRIBUTETYPE
{
    WTA_NONCLIENT = 1
};

typedef struct _WTA_OPTIONS
{
    DWORD dwFlags;          // values for each style option specified in the bitmask
    DWORD dwMask;           // bitmask for flags that are changing
                            // valid options are: WTNCA_NODRAWCAPTION, WTNCA_NODRAWICON, WTNCA_NOSYSMENU
} WTA_OPTIONS, *PWTA_OPTIONS;

#define WTNCA_NODRAWCAPTION       0x00000001    // don't draw the window caption
#define WTNCA_NODRAWICON          0x00000002    // don't draw the system icon
#define WTNCA_NOSYSMENU           0x00000004    // don't expose the system menu icon functionality
#define WTNCA_NOMIRRORHELP        0x00000008    // don't mirror the question mark, even in RTL layout
#define WTNCA_VALIDBITS           (WTNCA_NODRAWCAPTION | \
                                   WTNCA_NODRAWICON | \
                                   WTNCA_NOSYSMENU | \
                                   WTNCA_NOMIRRORHELP)

THEMEAPI
(SetWindowThemeAttribute)(
    HWND hwnd,
    enum WINDOWTHEMEATTRIBUTETYPE eAttribute,
    __in_bcount(cbAttribute) PVOID pvAttribute,
    DWORD cbAttribute
    );

__inline HRESULT SetWindowThemeNonClientAttributes(HWND hwnd, DWORD dwMask, DWORD dwAttributes)    
{
    WTA_OPTIONS wta;
    wta.dwFlags = dwAttributes;
    wta.dwMask = dwMask;
    return SetWindowThemeAttribute(hwnd, WTA_NONCLIENT, (void*)&(wta), sizeof(wta));
}


THEMEAPI
(GetThemeFilename)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out_ecount(cchMaxBuffChars) LPWSTR pszThemeFileName,
    int cchMaxBuffChars
    );

THEMEAPI_(COLORREF, GetThemeSysColor)(
    HTHEME hTheme,
    int iColorId
    );

THEMEAPI_(HBRUSH, GetThemeSysColorBrush)(
    HTHEME hTheme,
    int iColorId
    );

THEMEAPI_(BOOL, GetThemeSysBool)(
    HTHEME hTheme,
    int iBoolId
    );


THEMEAPI_(int, GetThemeSysSize)(
    HTHEME hTheme,
    int iSizeId
    );

THEMEAPI
(GetThemeSysFont)(
    HTHEME hTheme,
    int iFontId,
    __out LOGFONTW *plf
    );

THEMEAPI
(GetThemeSysString)(
    HTHEME hTheme,
    int iStringId,
    __out_ecount(cchMaxStringChars) LPWSTR pszStringBuff,
    int cchMaxStringChars
    );

THEMEAPI
(GetThemeSysInt)(
    HTHEME hTheme,
    int iIntId,
    __out int *piValue
    );

THEMEAPI_(BOOL, IsThemeActive)(
    VOID
    );

THEMEAPI_(BOOL, IsAppThemed)(
    VOID
    );

THEMEAPI_(HTHEME, GetWindowTheme)(
    HWND hwnd
    );

#define ETDT_DISABLE                    0x00000001
#define ETDT_ENABLE                     0x00000002
#define ETDT_USETABTEXTURE              0x00000004
#define ETDT_USEAEROWIZARDTABTEXTURE    0x00000008

#define ETDT_ENABLETAB              (ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE)
        
#define ETDT_ENABLEAEROWIZARDTAB    (ETDT_ENABLE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)
                             
#define ETDT_VALIDBITS              (ETDT_DISABLE | \
                                     ETDT_ENABLE | \
                                     ETDT_USETABTEXTURE | \
                                     ETDT_USEAEROWIZARDTABTEXTURE)

THEMEAPI
(EnableThemeDialogTexture)(
    __in HWND hwnd,
    __in DWORD dwFlags
    );

THEMEAPI_(BOOL, IsThemeDialogTextureEnabled)(
    __in HWND hwnd
    );

//---------------------------------------------------------------------------
//---- flags to control theming within an app ----

#define STAP_ALLOW_NONCLIENT    (1UL << 0)
#define STAP_ALLOW_CONTROLS     (1UL << 1)
#define STAP_ALLOW_WEBCONTENT   (1UL << 2)
#define STAP_VALIDBITS          (STAP_ALLOW_NONCLIENT | \
                                 STAP_ALLOW_CONTROLS | \
                                 STAP_ALLOW_WEBCONTENT)

THEMEAPI_(DWORD, GetThemeAppProperties)(
    VOID
    );

THEMEAPI_(void, SetThemeAppProperties)(
    DWORD dwFlags
    );

THEMEAPI (GetCurrentThemeName)(
    __out_ecount(cchMaxNameChars) LPWSTR pszThemeFileName,
    int cchMaxNameChars,
    __out_ecount_opt(cchMaxColorChars) LPWSTR pszColorBuff,
    int cchMaxColorChars,
    __out_ecount_opt(cchMaxSizeChars) LPWSTR pszSizeBuff,
    int cchMaxSizeChars
    );

#define SZ_THDOCPROP_DISPLAYNAME    L"DisplayName"
#define SZ_THDOCPROP_CANONICALNAME  L"ThemeName"
#define SZ_THDOCPROP_TOOLTIP        L"ToolTip"
#define SZ_THDOCPROP_AUTHOR         L"author"

THEMEAPI
(GetThemeDocumentationProperty)(
    LPCWSTR pszThemeName,
    LPCWSTR pszPropertyName,
    __out_ecount(cchMaxValChars) LPWSTR pszValueBuff,
    int cchMaxValChars
    );

THEMEAPI
(DrawThemeParentBackground)(
    HWND hwnd,
    HDC hdc,
    __in_opt const RECT* prc
    );


#define DTPB_WINDOWDC           0x00000001
#define DTPB_USECTLCOLORSTATIC  0x00000002
#define DTPB_USEERASEBKGND      0x00000004

THEMEAPI
(DrawThemeParentBackgroundEx)(
    HWND hwnd,
    HDC hdc,
    DWORD dwFlags,
    __in_opt const RECT* prc
    );

THEMEAPI
(EnableTheming)(
    BOOL fEnable
    );

#define GBF_DIRECT      0x00000001      // direct dereferencing.
#define GBF_COPY        0x00000002      // create a copy of the bitmap
#define GBF_VALIDBITS   (GBF_DIRECT | \
                         GBF_COPY)

// #if (_WIN32_WINNT >= 0x0600)

THEMEAPI
(GetThemeBitmap)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    ULONG dwFlags,
    __out HBITMAP* phBitmap
    );

THEMEAPI
(GetThemeStream)(
    HTHEME hTheme,
    int iPartId,
    int iStateId,
    int iPropId,
    __out VOID **ppvStream,
    __out_opt DWORD *pcbStream,
    __in_opt HINSTANCE hInst
    );

// #endif // #if (_WIN32_WINNT >= 0x0600)

THEMEAPI
(BufferedPaintInit)(
    VOID
    );

THEMEAPI
(BufferedPaintUnInit)(
    VOID
    );


// HPAINTBUFFER
typedef HANDLE HPAINTBUFFER;  // handle to a buffered paint context


// BP_BUFFERFORMAT
typedef enum _BP_BUFFERFORMAT
{
    BPBF_COMPATIBLEBITMAP,    // Compatible bitmap
    BPBF_DIB,                 // Device-independent bitmap
    BPBF_TOPDOWNDIB,          // Top-down device-independent bitmap
    BPBF_TOPDOWNMONODIB       // Top-down monochrome device-independent bitmap
} BP_BUFFERFORMAT;

#define BPBF_COMPOSITED BPBF_TOPDOWNDIB


// BP_ANIMATIONSTYLE
typedef enum _BP_ANIMATIONSTYLE
{
    BPAS_NONE,                // No animation
    BPAS_LINEAR,              // Linear fade animation
    BPAS_CUBIC,               // Cubic fade animation
    BPAS_SINE                 // Sinusoid fade animation
} BP_ANIMATIONSTYLE;


// BP_ANIMATIONPARAMS
typedef struct _BP_ANIMATIONPARAMS
{
    DWORD               cbSize;
    DWORD               dwFlags; // BPAF_ flags
    BP_ANIMATIONSTYLE   style;
    DWORD               dwDuration;
} BP_ANIMATIONPARAMS, *PBP_ANIMATIONPARAMS;

#define BPPF_ERASE               0x0001 // Empty the buffer during BeginBufferedPaint()
#define BPPF_NOCLIP              0x0002 // Don't apply the target DC's clip region to the double buffer
#define BPPF_NONCLIENT           0x0004 // Using a non-client DC

                                        
// BP_PAINTPARAMS
typedef struct _BP_PAINTPARAMS
{
    DWORD                       cbSize;
    DWORD                       dwFlags; // BPPF_ flags
    const RECT *                prcExclude;
    const BLENDFUNCTION *       pBlendFunction;
} BP_PAINTPARAMS, *PBP_PAINTPARAMS;

THEMEAPI_(HPAINTBUFFER, BeginBufferedPaint)(
    HDC hdcTarget,
    const RECT* prcTarget,
    BP_BUFFERFORMAT dwFormat,
    __in_opt BP_PAINTPARAMS *pPaintParams,
    __out HDC *phdc
    );


THEMEAPI
(EndBufferedPaint)(
    HPAINTBUFFER hBufferedPaint,
    BOOL fUpdateTarget
    );

THEMEAPI
(GetBufferedPaintTargetRect)(
    HPAINTBUFFER hBufferedPaint,
    __out RECT *prc
    );

THEMEAPI_(HDC, GetBufferedPaintTargetDC)(
    HPAINTBUFFER hBufferedPaint
    );

THEMEAPI_(HDC, GetBufferedPaintDC)(
    HPAINTBUFFER hBufferedPaint
    );

THEMEAPI
(GetBufferedPaintBits)(
    HPAINTBUFFER hBufferedPaint,
    __out RGBQUAD **ppbBuffer,
    __out int *pcxRow
    );

THEMEAPI
(BufferedPaintClear)(
    HPAINTBUFFER hBufferedPaint,
    __in_opt const RECT *prc
    );

THEMEAPI
(BufferedPaintSetAlpha)(
    HPAINTBUFFER hBufferedPaint,
    __in_opt const RECT *prc,
    BYTE alpha
    );

// Macro for setting the buffer to opaque (alpha = 255)
#define BufferedPaintMakeOpaque(hBufferedPaint, prc) BufferedPaintSetAlpha(hBufferedPaint, prc, 255)

THEMEAPI
(BufferedPaintStopAllAnimations)(
    HWND hwnd
    );

typedef HANDLE HANIMATIONBUFFER;  // handle to a buffered paint animation

THEMEAPI_(HANIMATIONBUFFER, BeginBufferedAnimation)(
    HWND hwnd,
    HDC hdcTarget,
    const RECT* prcTarget,
    BP_BUFFERFORMAT dwFormat,
    __in_opt BP_PAINTPARAMS *pPaintParams,
    __in BP_ANIMATIONPARAMS *pAnimationParams,
    __out HDC *phdcFrom,
    __out HDC *phdcTo
    );

THEMEAPI
(EndBufferedAnimation)(
    HANIMATIONBUFFER hbpAnimation,
    BOOL fUpdateTarget
    );

THEMEAPI_(BOOL, BufferedPaintRenderAnimation)(
    HWND hwnd,
    HDC hdcTarget
    );
    
THEMEAPI_(BOOL, IsCompositionActive)();

THEMEAPI
(GetThemeTransitionDuration)(
    HTHEME hTheme,
    int iPartId,
    int iStateIdFrom,
    int iStateIdTo,
    int iPropId,
    __out DWORD *pdwDuration
    );  

typedef struct _UXTHEME_FUNCTION_ENTRY {
	LPSTR FunctionName;
	PVOID* FunctionPointer;
} UXTHEME_FUNCTION_ENTRY, PUXTHEME_FUNCTION_ENTRY;

DECLSPEC_SELECTANY 
UXTHEME_FUNCTION_ENTRY UxThemeFunctions[] = {
	"OpenThemeData", (PVOID*)&OpenThemeData,
	"OpenThemeDataEx", (PVOID*)&OpenThemeDataEx,
	"CloseThemeData", (PVOID*)&CloseThemeData,
	"DrawThemeBackground", (PVOID*)&DrawThemeBackground,
	"DrawThemeBackgroundEx", (PVOID*)&DrawThemeBackgroundEx,
	"DrawThemeText", (PVOID*)&DrawThemeText,
	"DrawThemeTextEx", (PVOID*)&DrawThemeTextEx,
	"GetThemeBackgroundContentRect", (PVOID*)&GetThemeBackgroundContentRect,
	"GetThemeBackgroundExtent", (PVOID*)&GetThemeBackgroundExtent,
	"GetThemeBackgroundRegion", (PVOID*)&GetThemeBackgroundRegion,
	"GetThemePartSize", (PVOID*)&GetThemePartSize,
	"GetThemeTextExtent", (PVOID*)&GetThemeTextExtent,
	"GetThemeTextMetrics", (PVOID*)&GetThemeTextMetrics,
	"HitTestThemeBackground", (PVOID*)&HitTestThemeBackground,
	"DrawThemeEdge", (PVOID*)&DrawThemeEdge,
	"DrawThemeIcon", (PVOID*)&DrawThemeIcon,
	"IsThemePartDefined", (PVOID*)&IsThemePartDefined,
	"IsThemeBackgroundPartiallyTransparent", (PVOID*)&IsThemeBackgroundPartiallyTransparent,
	"GetThemeColor", (PVOID*)&GetThemeColor,
	"GetThemeMetric", (PVOID*)&GetThemeMetric,
	"GetThemeString", (PVOID*)&GetThemeString,
	"GetThemeBool", (PVOID*)&GetThemeBool,
	"GetThemeInt", (PVOID*)&GetThemeInt,
	"GetThemeEnumValue", (PVOID*)&GetThemeEnumValue,
	"GetThemePosition", (PVOID*)&GetThemePosition,
	"GetThemeFont", (PVOID*)&GetThemeFont,
	"GetThemeRect", (PVOID*)&GetThemeRect,
	"GetThemeMargins", (PVOID*)&GetThemeMargins,
	"GetThemeIntList", (PVOID*)&GetThemeIntList,
	"GetThemePropertyOrigin", (PVOID*)&GetThemePropertyOrigin,
	"SetWindowTheme", (PVOID*)&SetWindowTheme,
	"SetWindowThemeAttribute", (PVOID*)&SetWindowThemeAttribute,
	"GetThemeFilename", (PVOID*)&GetThemeFilename,
	"GetThemeSysColor", (PVOID*)&GetThemeSysColor,
	"GetThemeSysColorBrush", (PVOID*)&GetThemeSysColorBrush,
	"GetThemeSysBool", (PVOID*)&GetThemeSysBool,
	"GetThemeSysSize", (PVOID*)&GetThemeSysSize,
	"GetThemeSysFont", (PVOID*)&GetThemeSysFont,
	"GetThemeSysString", (PVOID*)&GetThemeSysString,
	"GetThemeSysInt", (PVOID*)&GetThemeSysInt,
	"IsThemeActive", (PVOID*)&IsThemeActive,
	"IsAppThemed", (PVOID*)&IsAppThemed,
	"GetWindowTheme", (PVOID*)&GetWindowTheme,
	"EnableThemeDialogTexture", (PVOID*)&EnableThemeDialogTexture,
	"IsThemeDialogTextureEnabled", (PVOID*)&IsThemeDialogTextureEnabled,
	"GetThemeAppProperties", (PVOID*)&GetThemeAppProperties,
	"SetThemeAppProperties", (PVOID*)&SetThemeAppProperties,
	"GetCurrentThemeName", (PVOID*)&GetCurrentThemeName,
	"GetThemeDocumentationProperty", (PVOID*)&GetThemeDocumentationProperty,
	"DrawThemeParentBackground", (PVOID*)&DrawThemeParentBackground,
	"DrawThemeParentBackgroundEx", (PVOID*)&DrawThemeParentBackgroundEx,
	"EnableTheming", (PVOID*)&EnableTheming,

	"GetThemeBitmap", (PVOID*)&GetThemeBitmap,
	"GetThemeStream", (PVOID*)&GetThemeStream,

	"BufferedPaintInit", (PVOID*)&BufferedPaintInit,
	"BufferedPaintUnInit", (PVOID*)&BufferedPaintUnInit,
	"BeginBufferedPaint", (PVOID*)&BeginBufferedPaint,
	"EndBufferedPaint", (PVOID*)&EndBufferedPaint,
	"GetBufferedPaintTargetRect", (PVOID*)&GetBufferedPaintTargetRect,
	"GetBufferedPaintTargetDC", (PVOID*)&GetBufferedPaintTargetDC,
	"GetBufferedPaintDC", (PVOID*)&GetBufferedPaintDC,
	"GetBufferedPaintBits", (PVOID*)&GetBufferedPaintBits,
	"BufferedPaintClear", (PVOID*)&BufferedPaintClear,
	"BufferedPaintSetAlpha", (PVOID*)&BufferedPaintSetAlpha,
	"BufferedPaintStopAllAnimations", (PVOID*)&BufferedPaintStopAllAnimations,
	"BeginBufferedAnimation", (PVOID*)&BeginBufferedAnimation,
	"EndBufferedAnimation", (PVOID*)&EndBufferedAnimation,
	"BufferedPaintRenderAnimation", (PVOID*)&BufferedPaintRenderAnimation,
	"IsCompositionActive", (PVOID*)&IsCompositionActive,
	"GetThemeTransitionDuration", (PVOID*)&GetThemeTransitionDuration,
};

DECLSPEC_SELECTANY LONG UxThemeInitLock = 0;
DECLSPEC_SELECTANY LONG UxThemeRefCount = 0;
DECLSPEC_SELECTANY HMODULE UxThemeHandle = NULL;

FORCEINLINE
BOOL
UxThemeIsAvailable()
{
	return NULL != UxThemeHandle;
}

__inline
VOID
UxThemeInitialize()
{
	while (1 == InterlockedCompareExchange(&UxThemeInitLock, 1, 0))
	{
		::Sleep(0);
	}

	if (1 == ++UxThemeRefCount)
	{
		UxThemeHandle = LoadLibraryW(L"uxtheme.dll");
		if (NULL != UxThemeHandle)
		{
			for (int i = 0; i < sizeof(UxThemeFunctions) / sizeof(UxThemeFunctions[0]); ++i)
			{
				*UxThemeFunctions[i].FunctionPointer = 
					GetProcAddress(UxThemeHandle, UxThemeFunctions[i].FunctionName);
			}
		}
	}

	InterlockedExchange(&UxThemeInitLock, 0);
}

__inline
VOID
UxThemeUninitialize()
{
	while (1 == InterlockedCompareExchange(&UxThemeInitLock, 1, 0))
	{
		::Sleep(0);
	}

	if (0 == --UxThemeRefCount)
	{
		FreeLibrary(UxThemeHandle);
		UxThemeHandle = NULL;
	}

	InterlockedExchange(&UxThemeInitLock, 0);
}

}

#endif /* _UXTHEME_H_ */
