/*
Module : XTaskDialog.cpp
Purpose: Defines the implementation for a set of classes which implements an emulation of the Vista Task Dialog API. 
         The code is designed to support our emulated version of Task Dialogs on Windows 2000, Windows XP, 
         Windows 2003 or later.
Created: PJN / 14-03-2007
History: PJN / 20-03-2007 1. Fixed a bug where the code unnecessarily set the progress bar range to 0-100 in 
                          CXTaskDialog::OnInitDialog. This was causing client calls to TDM_SET_PROGRESS_BAR_RANGE
                          in TDN_CREATED notifications to effectively be ignored. Thanks to Demetrios A. Thomakos for
                          reporting this issue.
                          2. For completeness, the DLL version of XTaskDialog now also emulates the TaskDialog
                          API call in addition to the existing TaskDialogIndirect API call. Thanks to Demetrios A. 
                          Thomakos for reporting this issue.
                          3. XTaskDialog now ships with its own version of the error, warning and info icons in addition
                          to the existing shield icon. This now allows XTaskDialog to work out of the box on 
                          Windows 98 and Windows ME in addition to 2000, XP and 2003 which were already supported. In
                          addition the icons are a closer match for the Vista icons. For example the TD_INFORMATION_ICON
                          icon is a I on a blue background. 
                          4. Fixed a typo and a minor code optimization in CXTaskDialogCommandLink::DrawItem.
                          5. CXTaskDialogCommandLink now provides a WM_ERASEBKGND handler to optimize it drawing
                          6. Minor code optimization in CXTaskDialog::CalculateCommandLinkMetrics.
                          7. CXTaskDialog::LoadStringResource method has been renamed to LoadStringResources.     
                          8. Fixed a minor vertical layout issue for TDF_EXPAND_FOOTER_AREA footer text in CXTaskDialog::Layout
         PJN / 22-03-2007 1. TaskDialog function exported from XTaskDlg dll now uses the correct definition for the TaskDialog
                          API as described at http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/commctls/taskdialogs/taskdialogreference/taskdialogfunctions/taskdialog.asp.
                          It seems that the description of the TaskDialog as provided with the latest Vista SDK (aka Windows SDK)
                          is incorrect and forget to mention the pszContent parameter. Thanks to Demetrios A. Thomakos for 
                          reporting this issue.
                          2. Removed an unused "m_bCancelButtonPresent" member variable from the CXTaskDialog class.
                          3. Fixed bug where cancel button would always be ignored when clicked when you did not provide the
                          TDF_ALLOW_DIALOG_CANCELLATION flag.
         PJN / 30-03-2007 1. Fixed a problem where the progress control was not created with the proper width if the end of the button row 
                          was to the left of the left of the control text. Thanks to Demetrios A. Thomakos for reporting this bug.
                          2. Fixed a bug where the main instruction text could sometimes appear in a standard sized font instead of 
                          the expected larger size. Thanks to Demetrios A. Thomakos for reporting this bug.
                          3. Fixed a bug where all static text controls could sometimes display clipped text. Thanks to Demetrios A. 
                          Thomakos for reporting this bug.
         PJN / 31-03-2007 1. Removed use of internal "VerificalTextSpacing" enum value.
         PJN / 05-04-2007 1. Fixed a bug where the code would not correctly set the initial selection state of the radio buttons in
                          CXTaskDialog::OnInitDialog. Thanks to Demetrios A. Thomakos for reporting this bug.

Copyright (c) 2007 by PJ Naughter (Web: www.naughter.com, Email: pjna@naughter.com)

All rights reserved.

Copyright / Usage Details:

You are allowed to include the source code in any product (commercial, shareware, freeware or otherwise) 
when your product is released in binary form. You are allowed to modify the source code in any way you want 
except you cannot modify the copyright details at the top of each module. If you want to distribute source 
code with your application, then you are only allowed to distribute versions released by the author. This is 
to maintain a single distribution point for the source code. 

*/


////////////////////////////////// Includes ///////////////////////////////////

#include "stdafx.h"
#include "XTaskDialog.h"
#include "resource.h"
#include <atlapp.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include "uxtheme_ext.h"

////////////////////////////////// Macros / Defines ///////////////////////////

//XTaskDialog require WINVER to be defined >= 0x500 to compile correctly
#if WINVER < 0x500
#pragma message("XTaskDialog requires the WINVER preprocessor macro to be 0x500 or greater to compile correctly.")
#endif

#ifndef PBM_SETMARQUEE
#define PBM_SETMARQUEE (WM_USER+10)
#endif

#ifndef MAX_LINKID_TEXT
#define MAX_LINKID_TEXT 48
#endif

#ifndef L_MAX_URL_LENGTH
#define L_MAX_URL_LENGTH (2048 + 32 + sizeof("://"))
#endif

#if (_WIN32_WINNT < 0x0501)

#ifndef LITEM
typedef struct tagLITEM
{
	UINT  mask;
	int   iLink;
	UINT  state;
	UINT  stateMask;
	WCHAR szID[MAX_LINKID_TEXT];
	WCHAR szUrl[L_MAX_URL_LENGTH];
} LITEM, *PLITEM;
#endif

#ifndef NMLINK
typedef struct tagNMLINK
{
	NMHDR hdr;
	LITEM item;
} NMLINK, *PNMLINK;
#endif

#endif

#ifndef PBS_MARQUEE
#define PBS_MARQUEE 0x08
#endif

#ifndef SPI_GETCLIENTAREAANIMATION
#define SPI_GETCLIENTAREAANIMATION 0x1042
#endif


////////////////////////////////// Implementation /////////////////////////////

CXTaskDialog::CXTaskDialog() : m_hMainIcon(NULL),
                               m_hFooterIcon(NULL),
                               m_hMainInstructionFont(NULL),
                               m_hFont(NULL),
                               m_nButton(0),
                               m_sDefaultExpandoCollapsedCaption(_T("See &details >>")),
                               m_sDefaultExpandoExpandedCaption(_T("Hide &details <<")),
                               m_nTimerID(0),
                               m_nExpandoTimerID(0),
                               m_pTaskConfig(NULL),
                               m_dwLastTickCount(0),
                               m_bVerificationFlagChecked(FALSE),
                               m_nIDDefaultButton(-1),
                               m_nYDivider1(-1),
                               m_nFinalYDivider1(-1),
                               m_nYDivider2(-1),
                               m_nFinalYDivider2(-1),
                               m_nMainInstructionID(-1),
                               m_nFooterID(-1),
                               m_nProgressID(-1),
                               m_nContentID(-1),
                               m_nVerificationCheckBoxID(-1),
                               m_nRadioButtonChecked(-1),
                               m_nMaxButtonID(-1),
                               m_nMainIconID(-1),
                               m_nFooterIconID(-1),
                               m_nExpandedTextID(-1),
                               m_nExpandoButtonID(-1),
                               m_bExpandedExpanded(FALSE),
                               m_colorTopBackground(RGB(255, 255, 255)),
                               m_colorDividerBackground(RGB(240, 240, 240)), //If you know a better way of getting the colors Vista's TaskDialog uses, then please let me know!
                               m_colorDivider(RGB(233, 233, 233)),
                               m_colorMainInstruction(RGB(0, 51, 153)),
                               m_brushTopBackground(NULL),
                               m_brushDividerBackground(NULL),
                               m_nFinalExpandoVerticalMovement(0),
                               m_nCurrentExpandoVerticalMovement(0),
                               m_nYDivider3(-1),
                               m_nFinalYDivider3(-1),
                               m_hCommandLink(NULL),
                               m_hHotCommandLink(NULL),
                               m_hBigWarning(NULL),
                               m_hSmallWarning(NULL),
                               m_hBigError(NULL),
                               m_hSmallError(NULL),
                               m_hBigInformation(NULL),
                               m_hSmallInformation(NULL),
                               m_hBigShield(NULL),
                               m_hSmallShield(NULL),
                               m_bMainIconIsInteral(FALSE),
                               m_bDoExpandoAnimation(TRUE)
{
	using namespace uxtheme;

	UxThemeInitialize();

	//Cache the colors we will be using
	m_colorStandardText = ::GetSysColor(COLOR_BTNTEXT);

	if (UxThemeIsAvailable() && IsAppThemed())
	{
		m_colorTopBackground = GetThemeSysColor(NULL, COLOR_3DFACE);
		m_colorDividerBackground = m_colorTopBackground;
		m_colorDivider = m_colorTopBackground;
	}
	else
	{
		m_colorTopBackground = ::GetSysColor(COLOR_3DFACE);
		m_colorDividerBackground = m_colorTopBackground;
		m_colorDivider = m_colorTopBackground;
	}

	struct {
		CString& StringObject;
		LPCTSTR FallbackText;
		UINT ResourceId;
	} StringDefs[] = {
		m_sDefaultExpandoCollapsedCaption, _T("See &details >>"), IDS_XTASKDIALOG_SHOW_DETAILS,
		m_sDefaultExpandoExpandedCaption, _T("Hide &details <<"), IDS_XTASKDIALOG_HIDE_DETAILS,
	};

	for (int i = 0; i < RTL_NUMBER_OF(StringDefs); ++i)
	{
		if (!StringDefs[i].StringObject.LoadString(StringDefs[i].ResourceId))
		{
			ATLTRACE("Resource %d is missing (%ls)\n", 
				StringDefs[i].ResourceId, StringDefs[i].FallbackText);
			StringDefs[i].StringObject = StringDefs[i].FallbackText;
		}
	}

	struct {
		UINT ResourceId;
		LPCTSTR FallbackText;
		int TaskDialogConfigId;
		int DialogId;
	} CommandButtonDefs[] = {
		IDS_XTASKDIALOG_BUTTON_CAPTION0, _T("OK"),     TDCBF_OK_BUTTON, IDOK,
		IDS_XTASKDIALOG_BUTTON_CAPTION1, _T("&Yes"),   TDCBF_YES_BUTTON, IDYES,
		IDS_XTASKDIALOG_BUTTON_CAPTION2, _T("&No"),    TDCBF_NO_BUTTON, IDNO,
		IDS_XTASKDIALOG_BUTTON_CAPTION3, _T("&Retry"), TDCBF_RETRY_BUTTON, IDRETRY,
		IDS_XTASKDIALOG_BUTTON_CAPTION4, _T("Cancel"), TDCBF_CANCEL_BUTTON, IDCANCEL,
		IDS_XTASKDIALOG_BUTTON_CAPTION5, _T("&Close"), TDCBF_CLOSE_BUTTON, IDCLOSE,
	};

	//Initialize the command buttons array
	for (int i = 0; i < RTL_NUMBER_OF(CommandButtonDefs); ++i)
	{
		if (!m_CommandButtons[i].m_sCaption.LoadString(CommandButtonDefs[i].ResourceId))
		{
			ATLTRACE("Resource %d is missing (%ls)\n", 
				CommandButtonDefs[i].ResourceId,
				CommandButtonDefs[i].FallbackText);
			m_CommandButtons[i].m_sCaption = CommandButtonDefs[i].FallbackText;
		}
		m_CommandButtons[i].m_nTaskDialogConfigID = CommandButtonDefs[i].TaskDialogConfigId;
		m_CommandButtons[i].m_nDialogID = CommandButtonDefs[i].DialogId;
	}
  
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (GetVersionEx(&osvi) && osvi.dwMajorVersion >= 6) 
	{
		//If we're running on Vista, then use the user preference for "SPI_GETCLIENTAREAANIMATION" by default
		SystemParametersInfo(SPI_GETCLIENTAREAANIMATION, 0, &m_bDoExpandoAnimation, 0);
	}
}

CXTaskDialog::~CXTaskDialog()
{
	if (m_brushTopBackground)
		DeleteObject(m_brushTopBackground);
	if (m_brushDividerBackground)
		DeleteObject(m_brushDividerBackground);
	if (m_hMainIcon)
		DestroyIcon(m_hMainIcon);
	if (m_hFooterIcon)
		DestroyIcon(m_hFooterIcon);
	if (m_hCommandLink)
		DestroyIcon(m_hCommandLink);
	if (m_hHotCommandLink)
		DestroyIcon(m_hHotCommandLink);
	if (m_hBigWarning)
		DestroyIcon(m_hBigWarning);
	if (m_hSmallWarning)
		DestroyIcon(m_hSmallWarning);
	if (m_hBigError)
		DestroyIcon(m_hBigError);
	if (m_hSmallError)
		DestroyIcon(m_hSmallError);
	if (m_hBigInformation)
		DestroyIcon(m_hBigInformation);
	if (m_hSmallInformation)
		DestroyIcon(m_hSmallInformation);
	if (m_hBigShield)
		DestroyIcon(m_hBigShield);
	if (m_hSmallShield)
		DestroyIcon(m_hSmallShield);
	if (m_hMainInstructionFont)
		DeleteObject(m_hMainInstructionFont);
	if (m_hFont)
		DeleteObject(m_hFont);
  
	int i;
	for (i=0; i<m_ctrlsRadio.GetSize(); i++)
		delete m_ctrlsRadio[i];

	for (i=0; i<m_ctrlsCommandLinks.GetSize(); i++)
		delete m_ctrlsCommandLinks[i];

	for (i=0; i<m_dlgItems.GetSize(); i++)
		delete m_dlgItems[i];

	uxtheme::UxThemeUninitialize();
}

void CXTaskDialog::CalculateButtonMetrics(HDC hDC, const CString& sCaption, int nMaxWidth, int& nButtonWidth, int& nButtonHeight)
{
	CRect rButton(0, 0, nMaxWidth, nMaxWidth);
	::DrawText(hDC, sCaption, -1, &rButton, DT_LEFT | DT_CALCRECT);
	int nWidth = rButton.Width() + HorizontalSpacingOnButton*2;
	if (nWidth > nButtonWidth)
		nButtonWidth = nWidth;
	int nHeight = rButton.Height() + VerticalSpacingOnButton*2;
	if (nHeight > nButtonHeight)
		nButtonHeight = nHeight;

#if 0
	HGDIOBJ oldFont = SelectObject(hDC, m_hFont);
	TEXTMETRIC tm;
	ATLVERIFY( GetTextMetrics(hDC, &tm) );
	SelectObject(hDC, oldFont);
	LONG nXDialogBaseUnit = tm.tmAveCharWidth;
	LONG nYDialogBaseUnit = tm.tmHeight;
	nButtonWidth = max(nButtonWidth, MulDiv(MinimumButtonWidth, nXDialogBaseUnit, 4));
	nButtonHeight = max(nButtonHeight, MulDiv(MinimumButtonHeight, nYDialogBaseUnit, 8));
#endif
	nButtonWidth = max(nButtonWidth, MinimumButtonWidth);
	nButtonHeight = max(nButtonHeight, MinimumButtonHeight);
}

void CXTaskDialog::CalculateButtonMetrics(HDC hDC, int nMaxWidth, int& nButtonWidth, int& nButtonHeight, int& nMaxButtonID, int& nButtons, int& nExpandoButtonWidth)
{
	//Set the output parameters to initial default values
	nButtonWidth = -1;
	nButtonHeight = -1;
	nMaxButtonID = 0;
	nExpandoButtonWidth = -1;
	nButtons = 0;
  
	//Validate our parameters
	ATLASSERT(m_pTaskConfig);
  
	//Work thro all the command buttons
	for (int i=0; i<sizeof(m_CommandButtons)/sizeof(CXTaskDialogButtonDetails); i++)
	{
		if (m_pTaskConfig->dwCommonButtons & m_CommandButtons[i].m_nTaskDialogConfigID)
		{
			++nButtons;
			CalculateButtonMetrics(hDC, m_CommandButtons[i].m_sCaption, nMaxWidth, nButtonWidth, nButtonHeight);
      
			if (m_CommandButtons[i].m_nDialogID > nMaxButtonID)
				nMaxButtonID = m_CommandButtons[i].m_nDialogID;
		}
	}

	//And the custom buttons
	nButtons += m_pTaskConfig->cButtons;
  
	//If no command buttons or no custom buttons are specified then just display an OK button
	if (nButtons == 0)
	{
		ATLASSERT(m_CommandButtons[0].m_nTaskDialogConfigID == TDCBF_OK_BUTTON);
		CalculateButtonMetrics(hDC, m_CommandButtons[0].m_sCaption, nMaxWidth, nButtonWidth, nButtonHeight);
		if (IDOK > nMaxButtonID)
			nMaxButtonID = IDOK;
	}

	if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
	{
		//try all the custom command buttons for width calculations and max button id calculations
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			CString sCaption;
			LoadString(m_pTaskConfig->hInstance, sCaption, m_pTaskConfig->pButtons[i].pszButtonText);
			CalculateButtonMetrics(hDC, sCaption, nMaxWidth, nButtonWidth, nButtonHeight);
      
			if (m_pTaskConfig->pButtons[i].nButtonID > nMaxButtonID)
				nMaxButtonID = m_pTaskConfig->pButtons[i].nButtonID;
		}
	}
	else
	{
		//Just try all the custom command link buttons for max button id calculations only
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			if (m_pTaskConfig->pButtons[i].nButtonID > nMaxButtonID)
				nMaxButtonID = m_pTaskConfig->pButtons[i].nButtonID;
		}
	}
  
	//And the expando button
	CalculateButtonMetrics(hDC, m_sExpandoCollapsedCaption, nMaxWidth, nExpandoButtonWidth, nButtonHeight);
	CalculateButtonMetrics(hDC, m_sExpandoExpandedCaption, nMaxWidth, nExpandoButtonWidth, nButtonHeight);
  
	//Now try all the radio buttons for max button id calculations
	for (UINT i=0; i<m_pTaskConfig->cRadioButtons; i++)
	{
		if (m_pTaskConfig->pRadioButtons[i].nButtonID > nMaxButtonID)
			nMaxButtonID = m_pTaskConfig->pRadioButtons[i].nButtonID;
	}
}

void CXTaskDialog::CalculateCommandLinkMetrics(HDC hDC, HGDIOBJ hBigFont, HGDIOBJ hSmallFont, const CString& sCaption, int nTextWidth, int& nButtonHeight)
{
	int nEOLPos = sCaption.Find(_T("\n"));
	if (nEOLPos == -1) //There is not small text provided
	{
		HGDIOBJ hOldFont = SelectObject(hDC, hBigFont);

		CRect rText(0, 0, nTextWidth, nTextWidth);
		::DrawText(hDC, sCaption, -1, &rText, DT_LEFT | DT_CALCRECT | DT_SINGLELINE | DT_EXPANDTABS);

		nButtonHeight = rText.Height() + 4*SpacingSize;
    
		SelectObject(hDC, hOldFont);
	}
	else
	{
		CString sMainText(sCaption.Left(nEOLPos));
		CString sSmallText(sCaption.Right(sCaption.GetLength() - nEOLPos - 1));
    
		HGDIOBJ hOldFont = SelectObject(hDC, hBigFont);

		CRect rText(0, 0, nTextWidth, nTextWidth);
		::DrawText(hDC, sMainText, -1, &rText, DT_LEFT | DT_CALCRECT | DT_SINGLELINE | DT_EXPANDTABS);

		nButtonHeight = rText.Height() + 5*SpacingSize;
    
		SelectObject(hDC, hSmallFont);
    
		rText = CRect(0, 0, nTextWidth, nTextWidth);
		::DrawText(hDC, sSmallText, -1, &rText, DT_LEFT | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS | DT_WORDBREAK);

		nButtonHeight += rText.Height();
    
		SelectObject(hDC, hOldFont);
	}
}

HRESULT CXTaskDialog::Layout()
{
	//get dc for drawing
	HDC hdc = CreateDC(_T("DISPLAY"), NULL, NULL, NULL);
	ATLASSERT(hdc);
	if (hdc == NULL)
	{
		DWORD dwLastError = GetLastError();
		ATLTRACE(_T("CXTaskDialog::Layout, Failed to create display DC, Error:%X\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}

	//Create the metrics for the message box font
	NONCLIENTMETRICS ncm;
	memset(&ncm, 0, sizeof(ncm));
	ncm.cbSize = sizeof(ncm);
	if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0))
	{
		DWORD dwLastError = GetLastError();
		::DeleteDC(hdc);
		ATLTRACE(_T("CXTaskDialog::Layout, Failed in call to SystemParametersInfo, Error:%u\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}
  
	//create the font we will be using for all the other text statics
	ATLASSERT(m_hFont == NULL);
	m_hFont = CreateFontIndirect(&ncm.lfMessageFont);
	ATLASSERT(m_hFont);
	if (m_hFont == NULL)
	{
		DWORD dwLastError = GetLastError();
		::DeleteDC(hdc);
		ATLTRACE(_T("CXTaskDialog::Layout, Failed to create message font, Error:%u\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}
  
	//Also create the font we will using for the main instruction static
	ATLASSERT(m_hMainInstructionFont == NULL);
	// LONG nNewHeight = static_cast<LONG>(ncm.lfMessageFont.lfHeight * 1.5);
	// ncm.lfMessageFont.lfHeight = nNewHeight;
	ncm.lfMessageFont.lfWeight = FW_BOLD; 
	m_hMainInstructionFont = CreateFontIndirect(&ncm.lfMessageFont);
	ATLASSERT(m_hMainInstructionFont);
	if (m_hMainInstructionFont == NULL)
	{
		DWORD dwLastError = GetLastError();
		::DeleteDC(hdc);
		ATLTRACE(_T("CXTaskDialog::Layout, Failed to create main instruction fontDC, Error:%u\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}

	//select the standard font into the DC ready for the button metric calculations
	HFONT hOldFont = static_cast<HFONT>(::SelectObject(hdc, m_hFont));
  
	//Work out how many standard button items we are going to have on the Task Dialog
	int nStandardButtons = 0;
	ATLASSERT(m_pTaskConfig);
	for (int i=0; i<sizeof(m_CommandButtons)/sizeof(CXTaskDialogButtonDetails); i++)
	{
		if (m_pTaskConfig->dwCommonButtons & m_CommandButtons[i].m_nTaskDialogConfigID)
			++nStandardButtons;
	}
	if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
	{
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)  
			++nStandardButtons;
	}
  
	//If no command link buttons and no command buttons are specified then just display an OK button
	BOOL bAddSingleOkButton = FALSE;
	if ((nStandardButtons == 0) && ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) && (m_pTaskConfig->cButtons == 0)))
	{
		nStandardButtons = 1;
		bAddSingleOkButton = TRUE;
	}
  
	//Work out the maximum width of the text based on the width on the monitor we are going to show on
	HMONITOR hMonitor = MonitorFromWindow(m_pTaskConfig->hwndParent, MONITOR_DEFAULTTONEAREST);
    
	//Get details about the monitor and do the max width calculation
	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	int nMaxDialogWidthPel;
	if (GetMonitorInfo(hMonitor, &mi))
		nMaxDialogWidthPel = (mi.rcMonitor.right - mi.rcMonitor.left) / 3 + 80;
		// nMaxDialogWidthPel = (mi.rcMonitor.right - mi.rcMonitor.left) / 3 + 80;
	else
		nMaxDialogWidthPel = (::GetSystemMetrics(SM_CXSCREEN) / 3) + 80; //Fall back to using the primary monitor

	nMaxDialogWidthPel = min(nMaxDialogWidthPel, 500);

	//Should we make the window a specific width   
	LONG nDialogBaseUnits = GetDialogBaseUnits();
	short nXDialogBaseUnit = LOWORD(nDialogBaseUnits);
	short nYDialogBaseUnit = HIWORD(nDialogBaseUnits);
	int nMaxDialogWidth = m_pTaskConfig->cxWidth ? (m_pTaskConfig->cxWidth * nXDialogBaseUnit) / 8 : nMaxDialogWidthPel;
  
	//Calculate the width and height of the buttons
	int nMaxWidthOfText = nMaxDialogWidth - 4*SpacingSize;
	if (m_hMainIcon)
		nMaxWidthOfText -= 32;
	int nButtonWidth;
	int nButtonHeight;
	int nButtons;
	m_nMaxButtonID = -1;
	int nExpandoButtonWidth;
	CalculateButtonMetrics(hdc, nMaxWidthOfText, nButtonWidth, nButtonHeight, m_nMaxButtonID, nButtons, nExpandoButtonWidth);
	int nAllButtonsWidth = nButtonWidth * nStandardButtons + (ButtonSpacing * (nStandardButtons - 1));
	int nNextControlID = m_nMaxButtonID + 1;

	//Are we doing verification processing
	BOOL bDoingVerification = (m_sVerificationText.GetLength() != 0);
  
	BOOL multiLineMainInstruction = FALSE;

	//Work out the widest text which we have
	int nCalculatedTextWidth = -1;
	if (nAllButtonsWidth > nMaxWidthOfText) 
		nMaxWidthOfText  = nAllButtonsWidth + (4*SpacingSize);
	CRect rText(0, 0, nMaxWidthOfText, nMaxWidthOfText);
	CRect rTextSingle = rText;
	::SelectObject(hdc, m_hMainInstructionFont); //Use the correct font
	DWORD dtFormat = DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS;
	::DrawText(hdc, m_sMainInstruction, -1, &rText, dtFormat);
	::DrawText(hdc, m_sMainInstruction, -1, &rTextSingle, dtFormat | DT_SINGLELINE);

	multiLineMainInstruction = (rText.Height() > rTextSingle.Height());

	nCalculatedTextWidth = rText.Width();
	rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);
	::SelectObject(hdc, m_hFont); //Use the correct font
	::DrawText(hdc, m_sDisplayableContent, -1, &rText, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	if (rText.Width() > nCalculatedTextWidth)
		nCalculatedTextWidth = rText.Width();
	rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);      
	::DrawText(hdc, m_sDisplayableExpanded, -1, &rText, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	if (rText.Width() > nCalculatedTextWidth)
		nCalculatedTextWidth = rText.Width();
	rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);      
	::DrawText(hdc, m_sVerificationText, -1, &rText, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	if (rText.Width() > nCalculatedTextWidth)
		nCalculatedTextWidth = rText.Width();
	rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);      
	::DrawText(hdc, m_sDisplayableFooter, -1, &rText, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	if (rText.Width() > nCalculatedTextWidth)
		nCalculatedTextWidth = rText.Width();
	++nCalculatedTextWidth; //Always include one extra horizontal pixel to ensure no clipping of text occurs in static controls

	nCalculatedTextWidth = max(nCalculatedTextWidth, 300);

	if (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS)
	{
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			CString sCaption;
			LoadString(m_pTaskConfig->hInstance, sCaption, m_pTaskConfig->pButtons[i].pszButtonText);
			int nEOLPos = sCaption.Find(_T("\n"));
			if (nEOLPos == -1) //There is not small text provided
			{
				//Next calculate the width of the main text
				rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);
				::SelectObject(hdc, m_hMainInstructionFont); //Use the correct font
				::DrawText(hdc, sCaption, -1, &rText, DT_LEFT | DT_CALCRECT | DT_SINGLELINE | DT_EXPANDTABS);

				int nCommandLinkWidth = rText.Width() + 4*SpacingSize;
				if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON) == 0)
					nCommandLinkWidth += (2*SpacingSize + 16);

				if (nCommandLinkWidth > nCalculatedTextWidth)
					nCalculatedTextWidth = nCommandLinkWidth;
			}
			else
			{
				CString sMainText(sCaption.Left(nEOLPos));
				CString sSmallText(sCaption.Right(sCaption.GetLength() - nEOLPos - 1));

				//Next calculate the width of the main text
				rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);
				::SelectObject(hdc, m_hMainInstructionFont); //Use the correct font
				::DrawText(hdc, sMainText, -1, &rText, DT_LEFT | DT_CALCRECT | DT_SINGLELINE | DT_EXPANDTABS);

				int nCommandLinkWidth = rText.Width() + 4*SpacingSize;
				if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON) == 0)
					nCommandLinkWidth += (2*SpacingSize + 16);

				if (nCommandLinkWidth > nCalculatedTextWidth)
					nCalculatedTextWidth = nCommandLinkWidth;
        
				//Next calculate the width of the small text
				rText = CRect(0, 0, nMaxWidthOfText, nMaxWidthOfText);
				::SelectObject(hdc, m_hFont); //Use the correct font
				::DrawText(hdc, sSmallText, -1, &rText, DT_LEFT | DT_CALCRECT | DT_NOPREFIX | DT_WORDBREAK | DT_EXPANDTABS);

				nCommandLinkWidth = rText.Width() + 4*SpacingSize;
				if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON) == 0)
					nCommandLinkWidth += (2*SpacingSize + 16);

				if (nCommandLinkWidth > nCalculatedTextWidth)
					nCalculatedTextWidth = nCommandLinkWidth;
			}
		}      
	}
    

	//Now calculate the main instruction height
	::SelectObject(hdc, m_hMainInstructionFont);
	CRect rTempMainInstruction(0, 0, nCalculatedTextWidth, nCalculatedTextWidth);
	::DrawText(hdc, m_sMainInstruction, -1, &rTempMainInstruction, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	++rTempMainInstruction.bottom; //Always include one extra vertical pixel to ensure no clipping of text occurs in static controls
  
	if (rTempMainInstruction.Height() < 33)
	{
		rTempMainInstruction.bottom = 33;
	}

	//Now calculate the content text height
	::SelectObject(hdc, m_hFont);
	CRect rTempContent(0, 0, nCalculatedTextWidth, nCalculatedTextWidth);
	::DrawText(hdc, m_sDisplayableContent, -1, &rTempContent, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	++rTempContent.bottom; //Always include one extra vertical pixel to ensure no clipping of text occurs in static controls

	//Now calculate the expanded text height
	CRect rTempExpanded(0, 0, nCalculatedTextWidth, nCalculatedTextWidth);
	::DrawText(hdc, m_sDisplayableExpanded, -1, &rTempExpanded, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	++rTempExpanded.bottom; //Always include one extra vertical pixel to ensure no clipping of text occurs in static controls

	//Now calculate the height of the verification check box text
	CRect rTempVerification(0, 0, nMaxWidthOfText, nMaxWidthOfText);
	::DrawText(hdc, m_sVerificationText, -1, &rTempVerification, DT_LEFT | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	++rTempVerification.bottom; //Always include one extra vertical pixel to ensure no clipping of text occurs in static controls

	//Now calculate the height of the footer text
	CRect rTempFooter(0, 0, nMaxWidthOfText, nMaxWidthOfText);
	::DrawText(hdc, m_sDisplayableFooter, -1, &rTempFooter, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT | DT_EXPANDTABS);
	++rTempFooter.bottom; //Always include one extra vertical pixel to ensure no clipping of text occurs in static controls
  
	//Start to work out the size of the client area for the Task Dialog
	CRect rDialog(0, 0, 0, SpacingSize);

	//initialize the DLGTEMPLATE structure
	m_dlgTempl.x = 0;
	m_dlgTempl.y = 0;
	m_dlgTempl.cdit = 0;
	m_dlgTempl.style = WS_CAPTION | WS_VISIBLE | WS_POPUP | DS_MODALFRAME | DS_SETFONT;
	if ((m_pTaskConfig->dwFlags & TDF_POSITION_RELATIVE_TO_WINDOW))
		m_dlgTempl.style |= DS_CENTER; //Use the DS_CENTER flag if we have been asked to center relative to a window
	if (m_pTaskConfig->dwFlags & TDF_CAN_BE_MINIMIZED)
		m_dlgTempl.style |= WS_MINIMIZEBOX;
	if ((m_pTaskConfig->dwFlags & TDF_ALLOW_DIALOG_CANCELLATION) || (m_pTaskConfig->dwCommonButtons & TDCBF_CANCEL_BUTTON))
		m_dlgTempl.style |= WS_SYSMENU;
	if (m_pTaskConfig->dwFlags & TDF_RTL_LAYOUT)
		m_dlgTempl.dwExtendedStyle = WS_EX_RIGHT | WS_EX_RTLREADING;
	else 
		m_dlgTempl.dwExtendedStyle = 0;

	//Initialize the dialog item array
	for (int i=0; i<m_dlgItems.GetSize(); i++)
		delete m_dlgItems[i];
	m_dlgItems.RemoveAll();  

	//Reset the various ids
	m_nIDDefaultButton = -1;
	m_nMainInstructionID = -1;
	m_nVerificationCheckBoxID = -1;
	m_nFooterID = -1;
	m_nYDivider1 = -1;
	m_nYDivider2 = -1;
	m_nYDivider3 = -1;
	m_nContentID = -1;
	m_nProgressID = -1;
	m_nMainIconID = -1;
	m_nFooterIconID = -1;
	m_nExpandedTextID = -1;
	m_nExpandoButtonID = -1;
	m_bExpandedExpanded = ((m_pTaskConfig->dwFlags & TDF_EXPANDED_BY_DEFAULT) != 0);
  
	//Add the main icon
	int nLeftText = 2*SpacingSize;
	CRect rMainIcon(0, 0, 0, 0);
	if (m_hMainIcon)
	{
		enum { IconWidth = 36, IconHeight = 36 };
		//Grow the message box rect also
		rDialog.right += (IconWidth + SpacingSize);

		//Add the main icon to the template 
		rMainIcon = CRect(nLeftText, MainIconVerticalOffset, nLeftText + IconHeight, MainIconVerticalOffset + IconHeight);
		m_nMainIconID = nNextControlID++;
		HRESULT hr = AddItem(CXTaskDialogItem::STATIC, m_nMainIconID, &rMainIcon, _T(""), nXDialogBaseUnit, nYDialogBaseUnit, SS_CENTER | SS_CENTERIMAGE);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add a main icon control to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Move in the text to the right
		nLeftText += (2*SpacingSize + IconWidth);
	}
	int nRightText = nLeftText + nCalculatedTextWidth;
	rDialog.right = nRightText + 2*SpacingSize;
  
	//Allow space for the button row
	int bLeft = 2*SpacingSize;
	int bRight = bLeft + nAllButtonsWidth;
	if (rDialog.right <= (bRight + (2*SpacingSize)))
		rDialog.right = bRight + (2*SpacingSize);

	//add message text
	if (m_sMainInstruction.GetLength())
	{
		//Add the main instruction text to the template
		rDialog.bottom += SpacingSize;
		m_nMainInstructionID = nNextControlID++;
		CRect rMainInstruction(nLeftText, rDialog.bottom, nRightText, rDialog.bottom + rTempMainInstruction.Height());
		DWORD astyle = multiLineMainInstruction ? 0 : SS_CENTERIMAGE;
		HRESULT hr = AddItem(CXTaskDialogItem::STATIC, m_nMainInstructionID, &rMainInstruction, m_sMainInstruction, nXDialogBaseUnit, nYDialogBaseUnit, astyle);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add main instruction text, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the main instruction text
		if (rMainInstruction.bottom >= rDialog.bottom)
			rDialog.bottom = rMainInstruction.bottom + SpacingSize;
	}

	//Add the content text if necessary 
	if (m_sContent.GetLength())
	{
		//Add the main content text to the template 
		rDialog.bottom += SpacingSize;
		CRect rContent(nLeftText, rDialog.bottom, nRightText, rDialog.bottom + rTempContent.Height());
		m_nContentID = nNextControlID++;
		HRESULT hr = AddItem(m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS ? CXTaskDialogItem::HYPERLINK : CXTaskDialogItem::STATIC, m_nContentID, &rContent, m_sContent, nXDialogBaseUnit, nYDialogBaseUnit);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add main content text to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the content text
		if (rContent.bottom >= rDialog.bottom)
			rDialog.bottom = rContent.bottom + SpacingSize;
	}
  
	//Add the expanded text if necessary
	if (m_sExpanded.GetLength() && ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0))
	{
		//Add the main content text to the template 
		int nTopExpanded = rDialog.bottom + SpacingSize;
		CRect rExpanded(nLeftText, nTopExpanded, nRightText, nTopExpanded + rTempExpanded.Height());
		m_nExpandedTextID = nNextControlID++;
		HRESULT hr = AddItem(m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS ? CXTaskDialogItem::HYPERLINK : CXTaskDialogItem::STATIC, m_nExpandedTextID, &rExpanded, m_sExpanded, nXDialogBaseUnit, nYDialogBaseUnit, 0, m_bExpandedExpanded ? 0 : WS_VISIBLE);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add expanded text to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		if (m_bExpandedExpanded)
		{
			//Ensure the task dialog extends below the content text
			if (rExpanded.bottom >= rDialog.bottom)
				rDialog.bottom = rExpanded.bottom + SpacingSize;
		}
	}

	//Add the progress control if necessary
	if ((m_pTaskConfig->dwFlags & TDF_SHOW_PROGRESS_BAR) || (m_pTaskConfig->dwFlags & TDF_SHOW_MARQUEE_PROGRESS_BAR))
	{
		//Add the progress control to the template  
		rDialog.bottom += SpacingSize;
		CRect rProgress(nLeftText, rDialog.bottom, rDialog.right - 2*SpacingSize, rDialog.bottom + ProgressControlHeight);
		m_nProgressID = nNextControlID++;
		HRESULT hr = AddItem(CXTaskDialogItem::PROGRESS, m_nProgressID, &rProgress, _T(""), nXDialogBaseUnit, nYDialogBaseUnit, (m_pTaskConfig->dwFlags & TDF_SHOW_MARQUEE_PROGRESS_BAR) ? PBS_MARQUEE : 0);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add progress control to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the progress control
		if (rProgress.bottom >= rDialog.bottom)
			rDialog.bottom = rProgress.bottom + SpacingSize;
	}
  
	//Add the radio buttons if necessary
	for (UINT i=0; i<m_pTaskConfig->cRadioButtons; i++)
	{
		if (i == 0)
			rDialog.bottom += SpacingSize;
  
		//Work out the height of this radio button
		CRect rTempRadio(0, 0, nMaxWidthOfText, nMaxWidthOfText);
		CString sCaption;
		LoadString(m_pTaskConfig->hInstance, sCaption, m_pTaskConfig->pRadioButtons[i].pszButtonText);
		::DrawText(hdc, sCaption, -1, &rTempRadio, DT_LEFT | DT_CALCRECT | DT_EXPANDTABS);
  
		//Add this radio button to the template 
		CRect rRadio(nLeftText + 2*SpacingSize, rDialog.bottom, nRightText, rDialog.bottom + rTempRadio.Height());
		HRESULT hr = AddItem(CXTaskDialogItem::RADIO, m_pTaskConfig->pRadioButtons[i].nButtonID, &rRadio, sCaption, nXDialogBaseUnit, nYDialogBaseUnit, (i==0) ? WS_GROUP : 0);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add a radio button to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the last radio button just added
		if (rRadio.bottom >= rDialog.bottom)
			rDialog.bottom = rRadio.bottom + RadioButtonVerticalSpacingSize;
	}
  
	//Add the command link buttons if necessary
	if (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS)
	{
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			if (i == 0)
				rDialog.bottom += SpacingSize;
      
			CString sCaption;
			LoadString(m_pTaskConfig->hInstance, sCaption, m_pTaskConfig->pButtons[i].pszButtonText);
			int nCommandLinkHeight = 0;
			CalculateCommandLinkMetrics(hdc, m_hMainInstructionFont, m_hFont, sCaption, CXTaskDialogCommandLink::CalculateCommandLinkTextWidth(nRightText - nLeftText, ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON) == 0)), nCommandLinkHeight);
			CRect rCommandLink(nLeftText, rDialog.bottom, nRightText, rDialog.bottom + nCommandLinkHeight);
			HRESULT hr = AddItem(CXTaskDialogItem::BUTTON, m_pTaskConfig->pButtons[i].nButtonID, &rCommandLink, sCaption, nXDialogBaseUnit, nYDialogBaseUnit, BS_LEFT | BS_OWNERDRAW | BS_ICON);
			if (FAILED(hr))
			{
				::SelectObject(hdc, hOldFont);
				::DeleteDC(hdc);
				ATLTRACE(_T("CXTaskDialog::Layout, Failed to add command link button to the dialog, Error:%X\n"), hr);
				return hr;
			}   
      
			//Ensure the task dialog extends below the progress control
			if (rCommandLink.bottom >= rDialog.bottom)
				rDialog.bottom = rCommandLink.bottom;
		}
    
		rDialog.bottom += SpacingSize;
	}
  
	//Ensure the task dialog extends below the main icon
	if (rMainIcon.bottom >= rDialog.bottom)
		rDialog.bottom = rMainIcon.bottom + SpacingSize;

	//Remember the Y offset where we will be drawing the first divider
	rDialog.bottom += SpacingSize;
	m_nYDivider1 = rDialog.bottom;
	rDialog.bottom += 2*SpacingSize;

	//Start laying out the button
	CRect rButtonRow(0, rDialog.bottom, nAllButtonsWidth, rDialog.bottom + nButtonHeight);
  
	//Prepare the "X" and "Y" values which we will use as the origin point for all the buttons we add
	int y = rButtonRow.top;
	int x = rDialog.right - SpacingSize*2 - nAllButtonsWidth;

	//First add the custom buttons
	int nButtonsAdded = 0;
	if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
	{  
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			CRect rect(x, y, x + nButtonWidth, y + nButtonHeight);
			CString sCaption;
			LoadString(m_pTaskConfig->hInstance, sCaption, m_pTaskConfig->pButtons[i].pszButtonText);
			HRESULT hr = AddItem(CXTaskDialogItem::BUTTON, m_pTaskConfig->pButtons[i].nButtonID, &rect, sCaption, nXDialogBaseUnit, nYDialogBaseUnit);
			if (FAILED(hr))
			{
				::SelectObject(hdc, hOldFont);
				::DeleteDC(hdc);
				ATLTRACE(_T("CXTaskDialog::Layout, Failed to add button to the dialog, Error:%X\n"), hr);
				return hr;
			}
			++nButtonsAdded;
      
			x += nButtonWidth + ButtonSpacing;
		}
	}

	//Then add the command buttons
	for (int i=0; i<sizeof(m_CommandButtons)/sizeof(CXTaskDialogButtonDetails); i++)
	{
		if ((m_pTaskConfig->dwCommonButtons & m_CommandButtons[i].m_nTaskDialogConfigID) ||
			((m_CommandButtons[i].m_nTaskDialogConfigID == TDCBF_OK_BUTTON) && (bAddSingleOkButton)))
		{
			CRect rect(x, y, x + nButtonWidth, y + nButtonHeight);
			HRESULT hr = AddItem(CXTaskDialogItem::BUTTON, m_CommandButtons[i].m_nDialogID, &rect, m_CommandButtons[i].m_sCaption, nXDialogBaseUnit, nYDialogBaseUnit, WS_GROUP);
			if (FAILED(hr))
			{
				::SelectObject(hdc, hOldFont);
				::DeleteDC(hdc);
				ATLTRACE(_T("CXTaskDialog::Layout, Failed to add a button to the dialog, Error:%X\n"), hr);
				return hr;
			}
			++nButtonsAdded;
      
			x += nButtonWidth + ButtonSpacing;
		}
	}

	//Ensure the task dialog extends below the button row
	if (nButtonsAdded && (rButtonRow.bottom >= rDialog.bottom))
		rDialog.bottom = rButtonRow.bottom + SpacingSize;

	//Add the expanded button if required
	if (m_sExpanded.GetLength())
	{
		//Create the rect for the expando button
		CRect rExpando(2*SpacingSize, rDialog.bottom, 2*SpacingSize + nExpandoButtonWidth, rDialog.bottom + nButtonHeight);

		//Add the expando button to the template
		m_nExpandoButtonID = nNextControlID++;
		HRESULT hr = AddItem(CXTaskDialogItem::BUTTON, m_nExpandoButtonID, &rExpando, m_bExpandedExpanded ? m_sExpandoExpandedCaption : m_sExpandoCollapsedCaption, nXDialogBaseUnit, nYDialogBaseUnit);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to expando button to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the expando button
		if (rExpando.bottom >= rDialog.bottom)
			rDialog.bottom = rExpando.bottom + SpacingSize;
	}

	//add the verification check box if necessary
	if (bDoingVerification)
	{
		CRect rVerification(2*SpacingSize, rDialog.bottom, nRightText, rDialog.bottom + rTempVerification.Height());
    
		//Add the check box to the template
		m_nVerificationCheckBoxID = nNextControlID++;
		HRESULT hr = AddItem(CXTaskDialogItem::CHECKBOX, m_nVerificationCheckBoxID, &rVerification, m_sVerificationText, nXDialogBaseUnit, nYDialogBaseUnit);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add verification check box to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the button row
		if (rVerification.bottom >= rDialog.bottom)
			rDialog.bottom = rVerification.bottom + SpacingSize;
	}
  
	//Do we need to do Footer processing?
	BOOL bValidFooter = (m_hFooterIcon || m_sFooter.GetLength());
	if (bValidFooter)
	{
		rDialog.bottom += SpacingSize;
		m_nYDivider2 = rDialog.bottom;
	}

	//Add the footer icon and text
	if (bValidFooter)
	{
		rDialog.bottom += 2*SpacingSize;
  
		//Calculate the position for the footer icon and add it if necessary
		CRect rFooterIcon(0, 0, 0, 0);
		int nXFooterPosition = 2*SpacingSize;
		int nYFooterPosition = rDialog.bottom;
		int nFooterHeight = rTempFooter.Height();
		if (m_hFooterIcon)
		{
			int nIconOffsetY = 0;
			if (nFooterHeight > 16)
				nIconOffsetY = (nFooterHeight - 16)/2;
			int nBottomFooterIcon = rDialog.bottom + 16 + nIconOffsetY;
			rFooterIcon = CRect(nXFooterPosition, rDialog.bottom + nIconOffsetY, 2*SpacingSize + 16, nBottomFooterIcon);
      
			//Add the footer icon
			m_nFooterIconID = nNextControlID++;
			HRESULT hr = AddItem(CXTaskDialogItem::STATIC, m_nFooterIconID, &rFooterIcon, _T(""), nXDialogBaseUnit, nYDialogBaseUnit);
			if (FAILED(hr))
			{
				::SelectObject(hdc, hOldFont);
				::DeleteDC(hdc);
				ATLTRACE(_T("CXTaskDialog::Layout, Failed to add footer icon to the dialog, Error:%X\n"), hr);
				return hr;
			}
      
			//Ensure the task dialog extends below the footer icon
			if (nBottomFooterIcon >= rDialog.bottom)
				rDialog.bottom = nBottomFooterIcon + SpacingSize;
        
			//Indent the footer text an appropriate amount
			nXFooterPosition += (16 + SpacingSize);
		}
    
		//Add the footer text
		CRect rFooter(nXFooterPosition, nYFooterPosition, nRightText, nYFooterPosition + nFooterHeight);
		m_nFooterID = nNextControlID++;
		HRESULT hr = AddItem(m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS ? CXTaskDialogItem::HYPERLINK : CXTaskDialogItem::STATIC, m_nFooterID, &rFooter, m_sFooter, nXDialogBaseUnit, nYDialogBaseUnit);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add footer text to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		//Ensure the task dialog extends below the footer text
		if (rFooter.bottom >= rDialog.bottom)
			rDialog.bottom = rFooter.bottom + SpacingSize;
	}
  
	//Add the expanded text if necessary
	if (m_sExpanded.GetLength() && (m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA))
	{
		//Add the main content text to the template 
		int nTopExpanded = rDialog.bottom + 3*SpacingSize;
		CRect rExpanded(2*SpacingSize, nTopExpanded, nRightText, nTopExpanded + rTempExpanded.Height());
		m_nExpandedTextID = nNextControlID++;
		HRESULT hr = AddItem(m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS ? CXTaskDialogItem::HYPERLINK : CXTaskDialogItem::STATIC, m_nExpandedTextID, &rExpanded, m_sExpanded, nXDialogBaseUnit, nYDialogBaseUnit, 0, m_bExpandedExpanded ? 0 : WS_VISIBLE);
		if (FAILED(hr))
		{
			::SelectObject(hdc, hOldFont);
			::DeleteDC(hdc);
			ATLTRACE(_T("CXTaskDialog::Layout, Failed to add expanded text to the dialog, Error:%X\n"), hr);
			return hr;
		}
    
		if (m_bExpandedExpanded)
		{
			if (m_nYDivider2 == -1)
				m_nYDivider2 = nTopExpanded - 2*SpacingSize;
			else
				m_nYDivider3 = nTopExpanded - 2*SpacingSize;
    
			//Ensure the task dialog extends below the expanded text
			if (rExpanded.bottom >= rDialog.bottom)
				rDialog.bottom = rExpanded.bottom + SpacingSize;
		}
	}
  
	//Finally do the additional spacing for the bottom of the dialog
	rDialog.bottom += SpacingSize;

	//Finally fill in the values for the size and position in the dialog template
	m_dlgTempl.x = static_cast<short>((rDialog.left * 4) / nXDialogBaseUnit);
	m_dlgTempl.y = static_cast<short>((rDialog.top * 8) / nYDialogBaseUnit);
	m_dlgTempl.cx = static_cast<short>((rDialog.Width() * 4) / nXDialogBaseUnit);
	m_dlgTempl.cy = static_cast<short>((rDialog.Height() * 8) / nYDialogBaseUnit);

	::SelectObject(hdc, hOldFont);
	::DeleteDC(hdc);
  
	return S_OK;
}

HRESULT CXTaskDialog::AddItem(CXTaskDialogItem::ControlType cType, UINT nID, const CRect* pRect, LPCTSTR pszCaption, short nXDialogBaseUnit, short nYDialogBaseUnit, DWORD dwAdditionalStyles, DWORD dwStylesToRemove)
{
	//Validate our parameters
	ATLASSERT(pRect);

	//Create the instance which encapsulates our representation of a child control and fill in the type, location, size, extended style and id of the control
	CXTaskDialogItem* pDlgItem = NULL;
	ATLTRY(pDlgItem = new CXTaskDialogItem(cType));
	if (pDlgItem == NULL)
	{
		ATLTRACE(_T("CXTaskDialog::AddItem, Failed to allocate required memory for a dialog item\n"));
		return E_OUTOFMEMORY;
	}
  
	pDlgItem->m_ControlType = cType;
	pDlgItem->m_dlgItemTemplate.x = static_cast<short>((pRect->left * 4) / nXDialogBaseUnit);
	pDlgItem->m_dlgItemTemplate.y = static_cast<short>((pRect->top * 8) / nYDialogBaseUnit);
	pDlgItem->m_dlgItemTemplate.cx = static_cast<short>((pRect->Width() * 4) / nXDialogBaseUnit);
	pDlgItem->m_dlgItemTemplate.cy = static_cast<short>((pRect->Height() * 8) / nYDialogBaseUnit);
	pDlgItem->m_dlgItemTemplate.dwExtendedStyle = 0;
	pDlgItem->m_dlgItemTemplate.id = static_cast<WORD>(nID);
	pDlgItem->m_sCaption = pszCaption;

	switch (cType)
	{
    case CXTaskDialogItem::ICON:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_CHILD | SS_ICON | WS_VISIBLE | dwAdditionalStyles;
		break;
    }  
    case CXTaskDialogItem::BUTTON:
    {
		GetButtonCount()++;
		pDlgItem->m_dlgItemTemplate.style = WS_VISIBLE | WS_CHILD | WS_TABSTOP | dwAdditionalStyles;
      
		//If the default button value is set as 0, then make the first button added the default button
		BOOL bThisIsFirstButton = TRUE;
		if (m_pTaskConfig->nDefaultButton == 0)
		{
			for (int i=0; i<m_dlgItems.GetSize() && bThisIsFirstButton; i++)
				bThisIsFirstButton = (m_dlgItems[i]->m_ControlType != CXTaskDialogItem::BUTTON);
		}
		if ((m_pTaskConfig->nDefaultButton == 0 && bThisIsFirstButton) || (static_cast<int>(nID) == m_pTaskConfig->nDefaultButton))
		{
			m_nIDDefaultButton = nID;
			pDlgItem->m_dlgItemTemplate.style |= BS_DEFPUSHBUTTON;
		}
		else
			pDlgItem->m_dlgItemTemplate.style |= BS_PUSHBUTTON;
		break;
    }
    case CXTaskDialogItem::CHECKBOX:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | dwAdditionalStyles;
		break;
    }
    case CXTaskDialogItem::RADIO:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | dwAdditionalStyles;
		break;
    }
    case CXTaskDialogItem::EDITCONTROL:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_CHILD | WS_VISIBLE | dwAdditionalStyles;
		break;
    }
    case CXTaskDialogItem::STATIC:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_CHILD | WS_VISIBLE | SS_LEFT | dwAdditionalStyles;
		break;
    }
    case CXTaskDialogItem::PROGRESS:
    {
		pDlgItem->m_dlgItemTemplate.style = PBS_SMOOTH | WS_CHILD | WS_VISIBLE | SS_LEFT | dwAdditionalStyles;
		break;
    }
    case CXTaskDialogItem::HYPERLINK:
    {
		pDlgItem->m_dlgItemTemplate.style = WS_CHILD | WS_VISIBLE | SS_LEFT | dwAdditionalStyles;
		break;
    }
    default:
    {
		ATLASSERT(FALSE); // should never get here
    }
	}

	//Remove any specified styles from the control before we add it to our member array, and update the dialog template counter
	pDlgItem->m_dlgItemTemplate.style &= ~dwStylesToRemove;
	m_dlgItems.Add(pDlgItem);
	m_dlgTempl.cdit++;
  
	return S_OK;
}

CString CXTaskDialog::CreateDisplayableText(const CString& sText)
{
	//What will be the return value from this function
	CString sModifiedText(sText);
  
	sModifiedText.Replace(_T("</A>"), _T("")); //First nuke the end anchor tags
	BOOL bFoundHyperlink = TRUE;
	do
	{
		//Try to find the next hyperlink tag
		int nHREFBegin = sModifiedText.Find(_T("<A HREF="));
		if (nHREFBegin != -1)
		{
			CString sTempText(sModifiedText.Right(sModifiedText.GetLength() - 8 - nHREFBegin));
			int nHREFEnd = sTempText.Find(_T(">"));
			if (nHREFEnd != -1)
				sModifiedText = sModifiedText.Left(nHREFBegin) + sTempText.Right(sTempText.GetLength() - nHREFEnd - 1);
			else
				bFoundHyperlink = FALSE;
		}
		else
			bFoundHyperlink = FALSE;
	}
	while (bFoundHyperlink);
  
	return sModifiedText;
}


HRESULT CXTaskDialog::LoadString(HINSTANCE hInstance, CString& sString, PCWSTR pszID)
{
	if (pszID)
	{  
		if (HIWORD(pszID) == NULL)
		{
			if (!sString.LoadString(hInstance, LOWORD(pszID)))
			{
				DWORD dwLastError = GetLastError();
				ATLTRACE(_T("CXTaskDialog::LoadString, Failed to load string resource, Error:%u\n"), dwLastError);
				return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
			}
		}
		else
			sString = pszID;
	}
	else
		sString.Empty();      

	return S_OK;
}

HRESULT CXTaskDialog::Display(int* pnButton)
{
	USES_CONVERSION;

	//We need a Unicode version of the title for the in memory dialog template
	LPCWSTR wszWindowTitle = T2W(const_cast<LPTSTR>(m_sWindowTitle.operator LPCTSTR()));
	size_t nTitleLen = wcslen(wszWindowTitle);

	LPCWSTR wszFont = L"MS Shell Dlg";
	size_t nFontLen = wcslen(wszFont);

	//Work out the initial buffer size of the DLGTEMPLATE struct up to the Null terminated caption
	size_t nBufferSize = sizeof(DLGTEMPLATE) +  (2 * sizeof(WORD)) + 
		((nTitleLen + 1) * sizeof(WCHAR)) +
		(nFontLen + 1) * sizeof(WCHAR);

	//adjust size to make first control DWORD aligned
	nBufferSize = (nBufferSize + 3) & ~3; 

	int i = 0;
	for (i=0; i<m_dlgTempl.cdit; i++)
	{
		//Calculate the size buffer we need for each dialog item
		size_t nItemLength = sizeof(DLGITEMTEMPLATE) + 2*sizeof(WORD) + (m_dlgItems[i]->m_sCaption.GetLength() + 1)*sizeof(WCHAR) + 
			(m_dlgItems[i]->m_sSystemClass.GetLength() + 1)*sizeof(WCHAR);
    
		//Ensure that each following dialog item is DWORD alligned in memory
		if (i != m_dlgTempl.cdit - 1) // the last control does not need extra bytes
			nItemLength = (nItemLength + 3) & ~3; // take into account gap

		nBufferSize += nItemLength;
	}

	//Allocate some memory to store the dialog template in
	HLOCAL hLocal = LocalAlloc(LHND, nBufferSize);
	ATLASSERT(hLocal);
	if (hLocal == NULL)
	{
		ATLTRACE(_T("CXTaskDialog::Display, Failed to allocate memory to contain dialog template, Error:%u\n"), GetLastError());
		return E_OUTOFMEMORY;
	}
	BYTE* pBuffer = static_cast<BYTE*>(LocalLock(hLocal));
	ATLASSERT(pBuffer);
	if (pBuffer == NULL)
	{
		DWORD dwLastError = ::GetLastError();
		LocalFree(hLocal);
		ATLTRACE(_T("CXTaskDialog::Display, Failed to lock dialog template memory, Error:%d\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}

	//transfer DLGTEMPLATE structure to the buffer we have just allocated
	BYTE* pDest = pBuffer;
	memcpy(pDest, &m_dlgTempl, sizeof(DLGTEMPLATE));
	pDest += sizeof(DLGTEMPLATE);
	*(reinterpret_cast<WORD*>(pDest)) = 0;  //We do not require a menu for out Task Dialog
	pDest += sizeof(WORD);
	*(reinterpret_cast<WORD*>(pDest)) = 0;  //Use the default window class for the Task Dialog
	pDest += sizeof(WORD);
	//transfer window title
	memcpy(pDest, wszWindowTitle, nTitleLen * sizeof(WCHAR));
	pDest += nTitleLen * sizeof(WCHAR);
	*(reinterpret_cast<WCHAR*>(pDest)) = L'\0';
	pDest += sizeof(WCHAR);

	// type face
	CopyMemory(pDest, wszFont, (nFontLen + 1) * sizeof(WCHAR));
	pDest += (nFontLen + 1) * sizeof(WCHAR);

	//Now transfer the information for each one of the item templates
	for (i=0; i<m_dlgTempl.cdit; i++)
	{
		pDest = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD_PTR>(pDest + 3) & ~3); // make the pointer DWORD aligned
		memcpy(pDest, &m_dlgItems[i]->m_dlgItemTemplate, sizeof(DLGITEMTEMPLATE));
		pDest += sizeof(DLGITEMTEMPLATE);
    
		//transfer system class name
		int nSystemClassLength = m_dlgItems[i]->m_sSystemClass.GetLength();
		memcpy(pDest, m_dlgItems[i]->m_sSystemClass.operator LPCWSTR(), nSystemClassLength * sizeof(WCHAR));
		pDest += nSystemClassLength * sizeof(WCHAR);
		*(reinterpret_cast<WCHAR*>(pDest)) = L'\0';
		pDest += sizeof(WCHAR);

		//transfer item caption
		int nCaptionLength = m_dlgItems[i]->m_sCaption.GetLength();
		memcpy(pDest, m_dlgItems[i]->m_sCaption.operator LPCWSTR(), nCaptionLength * sizeof(WCHAR));
		pDest += nCaptionLength * sizeof(WCHAR);
		*(reinterpret_cast<WCHAR*>(pDest)) = L'\0';
		pDest += sizeof(WCHAR);

		*(reinterpret_cast<WORD*>(pDest)) = 0; //How many bytes in data for control
		pDest += sizeof(WORD);
	}

	//just make sure we did not overrun the heap
	ATLASSERT(static_cast<size_t>(pDest - pBuffer) <= nBufferSize);
  
	ATLASSERT(m_pTaskConfig);
	ATLASSERT(m_hWnd == NULL);
  
	//Allocate the thunk structure to bootstrap ATL Windowing support
	if (!m_thunk.Init(NULL, NULL))
	{
		DWORD dwLastError = GetLastError();
		LocalFree(hLocal);
		LocalUnlock(hLocal);
		ATLTRACE(_T("CXTaskDialog::Display, Failed to setup ATL Window thunk, Error:%d\n"), dwLastError);
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
	}
	_AtlWinModule.AddCreateWndData(&m_thunk.cd, this);
  
#ifdef _DEBUG
	m_bModal = true;
#endif //_DEBUG
  
	int nButton = static_cast<int>(::DialogBoxIndirectParam(_AtlBaseModule.GetResourceInstance(), reinterpret_cast<LPCDLGTEMPLATE>(pBuffer), m_pTaskConfig->hwndParent, StartDialogProc, NULL));
	if (pnButton)
		*pnButton = nButton;

	//Free up the memory we have used for the dialog template
	LocalUnlock(hLocal);
	LocalFree(hLocal);

	return S_OK;
}

HRESULT CXTaskDialog::LoadMainIcon()
{
	//A value we use in OnInitDialog to ensure we draw the dialog 16*16 icon correctly
	m_bMainIconIsInteral = FALSE;

	//Destroy any existing icon
	if (m_hMainIcon)
	{
		DestroyIcon(m_hMainIcon);
		m_hMainIcon = NULL;
	}
  
	//What will be the return value from this function  
	HRESULT hr = S_OK;

	if (m_pTaskConfig->dwFlags & TDF_USE_HICON_MAIN)
	{
		ATLASSERT(m_pTaskConfig->hMainIcon);
		m_hMainIcon = DuplicateIcon(NULL, m_pTaskConfig->hMainIcon);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (User specified), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->pszMainIcon == TD_ERROR_ICON)
	{
		m_hMainIcon = DuplicateIcon(NULL, m_hBigError);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (Error), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
		m_bMainIconIsInteral = TRUE;
	}
	else if (m_pTaskConfig->pszMainIcon == TD_WARNING_ICON)
	{
		m_hMainIcon = DuplicateIcon(NULL, m_hBigWarning);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (Warning), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
		m_bMainIconIsInteral = TRUE;
	}
	else if (m_pTaskConfig->pszMainIcon == TD_INFORMATION_ICON)
	{
		m_hMainIcon = DuplicateIcon(NULL, m_hBigInformation);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (Information), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
		m_bMainIconIsInteral = TRUE;
	}
	else if (m_pTaskConfig->pszMainIcon == TD_SHIELD_ICON)
	{
		m_hMainIcon = DuplicateIcon(NULL, m_hBigShield);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (Shield), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
		m_bMainIconIsInteral = TRUE;
	}
	else if (m_pTaskConfig->hInstance && m_pTaskConfig->pszMainIcon && HIWORD(m_pTaskConfig->pszMainIcon) == NULL)
	{
		LPCTSTR pszIcon = MAKEINTRESOURCE(LOWORD(m_pTaskConfig->pszMainIcon));
		m_hMainIcon = reinterpret_cast<HICON>(LoadIcon(m_pTaskConfig->hInstance, pszIcon));
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load main icon (User specified), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
  
	return hr;
}

HRESULT CXTaskDialog::LoadFooterIcon()
{
	//Destroy any existing icon
	if (m_hFooterIcon)
	{
		DestroyIcon(m_hFooterIcon);
		m_hFooterIcon = NULL;
	}
  
	//What will be the return value from this function  
	HRESULT hr = S_OK;

	//Load up the footer icon
	if (m_pTaskConfig->dwFlags & TDF_USE_HICON_FOOTER)
	{
		ATLASSERT(m_pTaskConfig->hFooterIcon);
		m_hFooterIcon = DuplicateIcon(NULL, m_pTaskConfig->hFooterIcon);
		if (m_hFooterIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadFooterIcon, Failed to load footer icon, Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->pszFooterIcon == TD_ERROR_ICON)
	{
		m_hFooterIcon = DuplicateIcon(NULL, m_hSmallError);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load footer icon (Error), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->pszFooterIcon == TD_WARNING_ICON)
	{
		m_hFooterIcon = DuplicateIcon(NULL, m_hSmallWarning);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load footer icon (Warning), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->pszFooterIcon == TD_INFORMATION_ICON)
	{
		m_hFooterIcon = DuplicateIcon(NULL, m_hSmallInformation);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load footer icon (Information), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->pszFooterIcon == TD_SHIELD_ICON)
	{
		m_hFooterIcon = DuplicateIcon(NULL, m_hSmallShield);
		if (m_hMainIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadMainIcon, Failed to load footer icon (Shield), Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
	else if (m_pTaskConfig->hInstance && m_pTaskConfig->pszFooterIcon && HIWORD(m_pTaskConfig->pszFooterIcon) == NULL)
	{
		LPCTSTR pszIcon = MAKEINTRESOURCE(LOWORD(m_pTaskConfig->pszFooterIcon));
		m_hFooterIcon = reinterpret_cast<HICON>(LoadImage(m_pTaskConfig->hInstance, pszIcon, IMAGE_ICON, 16, 16, 0));
		if (m_hFooterIcon == NULL)
		{
			DWORD dwLastError = GetLastError();
			ATLTRACE(_T("CXTaskDialog::LoadFooterIcon, Failed to load footer icon, Error:%d\n"), dwLastError);
			hr = MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, dwLastError);
		}
	}
  
	return S_OK;
}

void CXTaskDialog::PlaySound()
{
	ATLASSERT(m_pTaskConfig);
  
	if (m_pTaskConfig->pszMainIcon == TD_ERROR_ICON)
		MessageBeep(MB_ICONHAND);
	else if (m_pTaskConfig->pszMainIcon == TD_WARNING_ICON)
		MessageBeep(MB_ICONEXCLAMATION);
	else if (m_pTaskConfig->pszMainIcon == TD_INFORMATION_ICON)
		MessageBeep(MB_ICONASTERISK);
}

HRESULT CXTaskDialog::LoadStringResources()
{
	//Load up the button captions from resources
	for (int i=0; i<sizeof(m_CommandButtons)/sizeof(CXTaskDialogButtonDetails); i++)
	{
		CString sCaption;
		if (sCaption.LoadString(_AtlBaseModule.GetResourceInstance(), IDS_XTASKDIALOG_BUTTON_CAPTION0 + i))
			m_CommandButtons[i].m_sCaption = sCaption;
	}
  
	return S_OK;
}

HRESULT CXTaskDialog::CreateGDIResources()
{
	//Release any resources we currently have
	if (m_brushTopBackground)
		DeleteObject(m_brushTopBackground);
	if (m_brushDividerBackground)
		DeleteObject(m_brushDividerBackground);
	if (m_hCommandLink)
		DestroyIcon(m_hCommandLink);
	if (m_hHotCommandLink)
		DestroyIcon(m_hHotCommandLink);
	if (m_hBigWarning)
		DestroyIcon(m_hBigWarning);
	if (m_hSmallWarning)
		DestroyIcon(m_hSmallWarning);
	if (m_hBigError)
		DestroyIcon(m_hBigError);
	if (m_hSmallError)
		DestroyIcon(m_hSmallError);
	if (m_hBigInformation)
		DestroyIcon(m_hBigInformation);
	if (m_hSmallInformation)
		DestroyIcon(m_hSmallInformation);
	if (m_hBigShield)
		DestroyIcon(m_hBigShield);
	if (m_hSmallShield)
		DestroyIcon(m_hSmallShield);
    
	//Create the various resources we require
	m_brushTopBackground = CreateSolidBrush(m_colorTopBackground);
	if (m_brushTopBackground == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_brushDividerBackground = CreateSolidBrush(m_colorDividerBackground);
	if (m_brushDividerBackground == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hCommandLink = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_COMMANDLINK), IMAGE_ICON, 16, 16, 0));    
	if (m_hCommandLink == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hHotCommandLink = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_HOT_COMMANDLINK), IMAGE_ICON, 16, 16, 0));    
	if (m_hHotCommandLink == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hSmallWarning = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_WARNING), IMAGE_ICON, 16, 16, 0));    
	if (m_hSmallWarning == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hBigWarning = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_WARNING), IMAGE_ICON, 32, 32, 0));    
	if (m_hBigWarning == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hSmallError = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_ERROR), IMAGE_ICON, 16, 16, 0));    
	if (m_hSmallError == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hBigError = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_ERROR), IMAGE_ICON, 32, 32, 0));    
	if (m_hBigError == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hSmallInformation = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_INFORMATION), IMAGE_ICON, 16, 16, 0));    
	if (m_hSmallInformation == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hBigInformation = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_INFORMATION), IMAGE_ICON, 32, 32, 0));    
	if (m_hBigInformation == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hSmallShield = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_SHIELD), IMAGE_ICON, 16, 16, 0));    
	if (m_hSmallShield == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());
	m_hBigShield = reinterpret_cast<HICON>(LoadImage(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_XTASKDIALOG_SHIELD), IMAGE_ICON, 32, 32, 0));    
	if (m_hBigShield == NULL)
		return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, ::GetLastError());

	return S_OK;
}

HRESULT CXTaskDialog::TaskDialogIndirect(const TASKDIALOGCONFIG* pTaskConfig, int* pnButton, int* pnRadioButton, BOOL* pfVerificationFlagChecked)
{
	//Validate our parameters
	ATLASSERT(pTaskConfig);
	m_pTaskConfig = pTaskConfig;
  
	//Load up the various string resources we require  
	HRESULT hr = LoadStringResources();
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to create necessary string resources, hr=0x%X\n"), hr);
		return hr;
	}

	//Create the various GDI objects we require
	hr = CreateGDIResources();
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to create necessary GDI resources, hr=0x%X\n"), hr);
		return hr;
	}

	//Load up the Window title
	hr = LoadString(pTaskConfig->hInstance, m_sWindowTitle, pTaskConfig->pszWindowTitle);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up main window title, hr=0x%X\n"), hr);
		return hr;
	}
	//If a window title is still empty, then default to the filename of the executable program
	if (m_sWindowTitle.GetLength() == 0)
	{
		TCHAR szModuleName[_MAX_PATH];
		GetModuleFileName(NULL, szModuleName, _MAX_PATH);
		TCHAR szFileName[_MAX_PATH];
		TCHAR szExtension[_MAX_EXT];
		_tsplitpath_s(szModuleName, NULL, 0, NULL, 0, szFileName, sizeof(szFileName)/sizeof(TCHAR), szExtension, sizeof(szExtension)/sizeof(TCHAR));
		_tmakepath_s(szModuleName, sizeof(szModuleName)/sizeof(TCHAR), NULL, NULL, szFileName, szExtension);
		m_sWindowTitle = szModuleName;
	}
  
	//Load up the Main Instruction text
	hr = LoadString(pTaskConfig->hInstance, m_sMainInstruction, pTaskConfig->pszMainInstruction);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up main instruction text\n"));
		return hr;
	}

	//Load up the Content text
	hr = LoadString(pTaskConfig->hInstance, m_sContent, pTaskConfig->pszContent);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up content title\n"));
		return hr;
	}
	if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		m_sDisplayableContent = CreateDisplayableText(m_sContent);   
	else
		m_sDisplayableContent = m_sContent; 

	//Load up the Expanded text
	hr = LoadString(pTaskConfig->hInstance, m_sExpanded, pTaskConfig->pszExpandedInformation);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up expanded title\n"));
		return hr;
	}
	if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)   
		m_sDisplayableExpanded = CreateDisplayableText(m_sExpanded);
	else
		m_sDisplayableExpanded = m_sExpanded;
    
	//Load up the Expanded control text
	hr = LoadString(pTaskConfig->hInstance, m_sExpandoExpandedCaption, pTaskConfig->pszExpandedControlText);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up expanded control text\n"));
		return hr;
	}
	if (m_sExpandoExpandedCaption.IsEmpty())
		m_sExpandoExpandedCaption = m_sDefaultExpandoExpandedCaption;

	//Load up the Expanded control text
	hr = LoadString(pTaskConfig->hInstance, m_sExpandoCollapsedCaption, pTaskConfig->pszCollapsedControlText);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up collapsed control text\n"));
		return hr;
	}
	if (m_sExpandoCollapsedCaption.IsEmpty())
		m_sExpandoCollapsedCaption = m_sDefaultExpandoCollapsedCaption;

	//Load up the Verification rext
	hr = LoadString(pTaskConfig->hInstance, m_sVerificationText, pTaskConfig->pszVerificationText);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up verification text\n"));
		return hr;
	}

	//Load up the Footer rext
	hr = LoadString(pTaskConfig->hInstance, m_sFooter, pTaskConfig->pszFooter);
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up footer text\n"));
		return hr;
	}
	if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		m_sDisplayableFooter = CreateDisplayableText(m_sFooter);
	else
		m_sDisplayableFooter = m_sFooter;


	//Load up the main icon
	hr = LoadMainIcon();
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up main icon\n"));
		return hr;
	}
  
	//Load up the footer icon
	hr = LoadFooterIcon();
	if (FAILED(hr))
	{
		ATLTRACE(_T("CXTaskDialog::TaskDialogIndirect, Failed to load up footer icon\n"));
		return hr;
	}
    
	//Layout and display the task dialog
	hr = Layout();
	if (FAILED(hr))
		return hr;
	hr = Display(pnButton);
  
	//Update the 2 output parameters before we return
	if (pfVerificationFlagChecked)
		*pfVerificationFlagChecked = m_bVerificationFlagChecked;
	if (pnRadioButton)
		*pnRadioButton = m_nRadioButtonChecked;
    
	return hr;
}

void CXTaskDialog::SetChildControlFont(int nID, BOOL bLargeFont)
{
	HWND hwndChild = ::GetDlgItem(m_hWnd, nID);
	if (hwndChild && ::IsWindow(hwndChild))
	{
		CWindow ctrlChild(hwndChild);
		if (bLargeFont)
			ctrlChild.SetFont(m_hMainInstructionFont);
		else
			ctrlChild.SetFont(m_hFont);
	}
}

LRESULT CXTaskDialog::OnInitDialog(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//Do the TDN_DIALOG_CONSTRUCTED notification
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_DIALOG_CONSTRUCTED, 0, 0, m_pTaskConfig->lpCallbackData);

	//Handle the case if we have been asked to center relative to a monitor i.e. (TDF_POSITION_RELATIVE_TO_WINDOW not set)
	if ((m_pTaskConfig->dwFlags & TDF_POSITION_RELATIVE_TO_WINDOW) == 0)
	{
		//Get the monitor nearest the parent rect
		HMONITOR hMonitor = MonitorFromWindow(m_pTaskConfig->hwndParent, MONITOR_DEFAULTTONEAREST);
    
		//Get details about the monitor
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
    
		//Move the dialog 
		if (GetMonitorInfo(hMonitor, &mi))
		{
			CRect rDialog;
			GetWindowRect(&rDialog);
			int nDeltaX = (mi.rcWork.right - mi.rcWork.left)/2 - (rDialog.right + rDialog.left)/2;
			rDialog.left += nDeltaX;
      
			int nDeltaY = (mi.rcWork.bottom - mi.rcWork.top)/2 - (rDialog.bottom + rDialog.top)/2;
			rDialog.top += nDeltaY;
      
			SetWindowPos(NULL, rDialog.left, rDialog.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		}
	}

	//Setup the fonts on all the child controls, Note the ordering of these calls is important because 
	//it is possible that some of the calls to SetChildControlFont which take a ID* value may not exist
	//and may in fact belong to one of our created controls. This only really matters if one of these
	//values class with the m_nMainInstructionID value which requires the larger font
	SetChildControlFont(IDOK, FALSE);
	SetChildControlFont(IDYES, FALSE);
	SetChildControlFont(IDNO, FALSE);
	SetChildControlFont(IDCANCEL, FALSE);
	SetChildControlFont(IDRETRY, FALSE);
	SetChildControlFont(IDCLOSE, FALSE);
	SetChildControlFont(m_nMainInstructionID, TRUE);
	SetChildControlFont(m_nContentID, FALSE);
	SetChildControlFont(m_nExpandedTextID, FALSE);
	SetChildControlFont(m_nExpandoButtonID, FALSE);
	SetChildControlFont(m_nProgressID, FALSE);
	SetChildControlFont(m_nVerificationCheckBoxID, FALSE);
	SetChildControlFont(m_nFooterID, FALSE);
	UINT i;
	for (i=0; i<m_pTaskConfig->cButtons; i++)
		SetChildControlFont(m_pTaskConfig->pButtons[i].nButtonID, (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) ? TRUE : FALSE);
	for (i=0; i<m_pTaskConfig->cRadioButtons; i++)
		SetChildControlFont(m_pTaskConfig->pRadioButtons[i].nButtonID, FALSE);
  
	//Check / Uncheck the verification check box if necessary 
	if (m_nVerificationCheckBoxID != -1)
	{
		if (m_pTaskConfig->dwFlags & TDF_VERIFICATION_FLAG_CHECKED)
			CheckDlgButton(m_nVerificationCheckBoxID, BST_CHECKED);
		else
			CheckDlgButton(m_nVerificationCheckBoxID, 0);
	}
  
	//Turn on marquee mode for the progress control if required
	if (m_nProgressID != -1)
	{
		HWND hWndChild = GetDlgItem(m_nProgressID);
		CWindow ctrlChild(hWndChild);
		if (m_pTaskConfig->dwFlags & TDF_SHOW_MARQUEE_PROGRESS_BAR)
			ctrlChild.SendMessage(PBM_SETMARQUEE, TRUE, InitialMarqueeUpdates);
	}

	//Select the default button if required
	if (m_nIDDefaultButton != -1)
	{
		HWND hWndChild = ::GetDlgItem(m_hWnd, m_nIDDefaultButton);
		CWindow ctrlChild(hWndChild);
		ctrlChild.SetFocus();
	}
  
	//Set up the initial state of the radio buttons 
	ATLASSERT(m_pTaskConfig);
	if (m_pTaskConfig->cRadioButtons)
	{
		//Check/Set the first radio button if requirred
		if ((m_pTaskConfig->dwFlags & TDF_NO_DEFAULT_RADIO_BUTTON) == 0)
		{
			if (m_pTaskConfig->nDefaultRadioButton != 0) //Check the specified radio button
			{
				HWND hWndChild = ::GetDlgItem(m_hWnd, m_pTaskConfig->nDefaultRadioButton);
				CWindow ctrlChild(hWndChild);
				ctrlChild.SendMessage(BM_SETCHECK, BST_CHECKED, 0);
			}
			else //just select the first radio button
			{
				ATLASSERT(m_pTaskConfig->pRadioButtons);
				HWND hWndChild = ::GetDlgItem(m_hWnd, m_pTaskConfig->pRadioButtons[0].nButtonID);
				CWindow ctrlChild(hWndChild);
				ctrlChild.SendMessage(BM_SETCHECK, BST_CHECKED, 0);
			}
		}
	}

	//disable close button if required
	if (((m_pTaskConfig->dwFlags & TDF_ALLOW_DIALOG_CANCELLATION) == 0) && ((m_pTaskConfig->dwCommonButtons & TDCBF_CANCEL_BUTTON) == 0))
		EnableMenuItem(GetSystemMenu(FALSE), SC_CLOSE, MF_GRAYED);

	//Update the dialogs icons if necessary
	if (m_pTaskConfig->dwFlags & TDF_CAN_BE_MINIMIZED)
	{
		m_bMainIconIsInteral = FALSE;
		if (m_bMainIconIsInteral)
		{
			if (m_pTaskConfig->pszMainIcon == TD_ERROR_ICON)
				SetIcon(m_hSmallError, FALSE);
			else if (m_pTaskConfig->pszMainIcon == TD_WARNING_ICON)
				SetIcon(m_hSmallWarning, FALSE);
			else if (m_pTaskConfig->pszMainIcon == TD_INFORMATION_ICON)
				SetIcon(m_hSmallInformation, FALSE);
			else if (m_pTaskConfig->pszMainIcon == TD_SHIELD_ICON)
				SetIcon(m_hSmallShield, FALSE);
			else
			{
				ATLASSERT(FALSE);
			}    
		}
		else
			SetIcon(m_hMainIcon, FALSE);
		SetIcon(m_hMainIcon, TRUE);
	}

	//Hook up the window proc for the main icon so that it renders correctly
	if (m_nMainIconID != -1)
	{
		m_ctrlMainIcon.Init(&m_hMainIcon, TRUE);
		m_ctrlMainIcon.SubclassWindow(GetDlgItem(m_nMainIconID));
	}
   
	//Hook up the window proc for the footer icon so that it renders correctly
	if (m_nFooterIconID != -1)
	{
		m_ctrlFooterIcon.Init(&m_hFooterIcon, FALSE);
		m_ctrlFooterIcon.SubclassWindow(GetDlgItem(m_nFooterIconID));
	}
  
	//Subclass the main instruction static
	if (m_nMainInstructionID != -1)
	{
		BOOL useLink = FALSE;
		if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		{
			ATL::CString s;
			ATLVERIFY(GetDlgItemText(m_nMainInstructionID, s));
			for (int i = 0; i < s.GetLength(); ++i)
			{
				if (0 == _tcsnicmp(static_cast<LPCTSTR>(s)+i, _T("<A"), 2))
				{
					useLink = TRUE;
					break;
				}
			}
		}
		if (useLink)
		{
			m_ctrlMainInstructionTextLink.SetHyperLinkExtendedStyle(
				HLINK_USETAGS | HLINK_NOTIFYBUTTON, 
				HLINK_USETAGS | HLINK_NOTIFYBUTTON);
			m_ctrlMainInstructionTextLink.PreInit(m_hFont);
			m_ctrlMainInstructionTextLink.SubclassWindow(GetDlgItem(m_nMainInstructionID));
			m_ctrlMainInstructionTextLink.Init(m_colorMainInstruction, m_colorTopBackground, &m_brushTopBackground);
		}
		else
		{
			m_ctrlMainInstructionText.SubclassWindow(GetDlgItem(m_nMainInstructionID));
			m_ctrlMainInstructionText.Init(m_colorMainInstruction, m_colorTopBackground, &m_brushTopBackground);
		}
	}
  
	//Subclass the content static if necessary
	if (m_nContentID != -1)
	{
		BOOL useLink = FALSE;
		if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		{
			ATL::CString s;
			ATLVERIFY(GetDlgItemText(m_nContentID, s));
			for (int i = 0; i < s.GetLength(); ++i)
			{
				if (0 == _tcsnicmp(static_cast<LPCTSTR>(s)+i, _T("<A"), 2))
				{
					useLink = TRUE;
					break;
				}
			}
		}
		if (useLink)
		{
			m_ctrlContentTextLink.SetHyperLinkExtendedStyle(
				HLINK_USETAGS | HLINK_NOTIFYBUTTON, 
				HLINK_USETAGS | HLINK_NOTIFYBUTTON);
			m_ctrlContentTextLink.PreInit(m_hFont);
			m_ctrlContentTextLink.SubclassWindow(GetDlgItem(m_nContentID));
			m_ctrlContentTextLink.Init(m_colorStandardText, m_colorTopBackground, &m_brushTopBackground);
		}
		else
		{
			m_ctrlContentText.SubclassWindow(GetDlgItem(m_nContentID));
			m_ctrlContentText.Init(m_colorStandardText, m_colorTopBackground, &m_brushTopBackground);
		}
	}

	//Subclass the expanded static if necessary
	if (m_nExpandedTextID != -1)
	{
		BOOL useLink = FALSE;
		if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		{
			ATL::CString s;
			ATLVERIFY(GetDlgItemText(m_nExpandedTextID, s));
			for (int i = 0; i < s.GetLength(); ++i)
			{
				if (0 == _tcsnicmp(static_cast<LPCTSTR>(s)+i, _T("<A"), 2))
				{
					useLink = TRUE;
					break;
				}
			}
		}

		if (useLink)
		{
			m_ctrlExpandedTextLink.SetHyperLinkExtendedStyle(
				HLINK_USETAGS | HLINK_NOTIFYBUTTON, 
				HLINK_USETAGS | HLINK_NOTIFYBUTTON);
			m_ctrlExpandedTextLink.PreInit(m_hFont);
			m_ctrlExpandedTextLink.SubclassWindow(GetDlgItem(m_nExpandedTextID));
			m_ctrlExpandedTextLink.Init(m_colorStandardText, ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0) ? m_colorTopBackground : m_colorDividerBackground, 
										((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0) ? &m_brushTopBackground : &m_brushDividerBackground);
			m_ctrlExpandedTextEx.Attach(m_ctrlExpandedTextLink);
		}
		else
		{
			m_ctrlExpandedText.SubclassWindow(GetDlgItem(m_nExpandedTextID));
			m_ctrlExpandedText.Init(m_colorStandardText, ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0) ? m_colorTopBackground : m_colorDividerBackground, 
									((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0) ? &m_brushTopBackground : &m_brushDividerBackground);
			m_ctrlExpandedTextEx.Attach(m_ctrlExpandedText);
		}
	}
    
	//Subclass all the radio buttons if necessary   
	for (int i=0; i<m_ctrlsRadio.GetSize(); i++)
		delete m_ctrlsRadio[i];
	m_ctrlsRadio.RemoveAll();
	for (UINT i=0; i<m_pTaskConfig->cRadioButtons; i++)
	{
		CXTaskDialogButton* pCtrlRadio = NULL;
		ATLTRY(pCtrlRadio = new CXTaskDialogButton);
		if (pCtrlRadio == NULL)
		{
			ATLTRACE(_T("CXTaskDialog::OnInitDialog, Failed to allocate required memory for a radio button\n"));
			PostMessage(WM_CLOSE);
		}
		else
		{
			pCtrlRadio->SubclassWindow(GetDlgItem(m_pTaskConfig->pRadioButtons[i].nButtonID));
			pCtrlRadio->Init(m_colorTopBackground, &m_brushTopBackground);
			m_ctrlsRadio.Add(pCtrlRadio);
		}
	}   

	//Subclass all the command link buttons if necessary    
	if (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS)
	{
		for (int i=0; i<m_ctrlsCommandLinks.GetSize(); i++)
			delete m_ctrlsCommandLinks[i];
		m_ctrlsCommandLinks.RemoveAll();
		for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
		{
			CXTaskDialogCommandLink* pCtrlCommandLink = NULL;
			ATLTRY(pCtrlCommandLink = new CXTaskDialogCommandLink);
			if (pCtrlCommandLink == NULL)
			{
				ATLTRACE(_T("CXTaskDialog::OnInitDialog, Failed to allocate required memory for a command link button\n"));
				PostMessage(WM_CLOSE);
			}
			else
			{
				pCtrlCommandLink->SubclassWindow(GetDlgItem(m_pTaskConfig->pButtons[i].nButtonID));
				if (!pCtrlCommandLink->Init(m_hMainInstructionFont, m_hFont, m_hSmallShield, m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON ? NULL : m_hCommandLink, m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON ? NULL : m_hHotCommandLink))
				{
					ATLTRACE(_T("CXTaskDialog::OnInitDialog, Failed to initialize command link button, ID:%d\n"), m_pTaskConfig->pButtons[i].nButtonID);
					PostMessage(WM_CLOSE);
				}
				//Associated the array icon with each command link if required
				if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON) == 0)
					pCtrlCommandLink->SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(m_hCommandLink));       
				m_ctrlsCommandLinks.Add(pCtrlCommandLink);
			}
		}   
	}
  
	//Subclass the footer static if necessary
	if (m_nFooterID != -1)
	{
		BOOL useLink = FALSE;
		if (m_pTaskConfig->dwFlags & TDF_ENABLE_HYPERLINKS)
		{
			ATL::CString s;
			ATLVERIFY(GetDlgItemText(m_nFooterID, s));
			for (int i = 0; i < s.GetLength(); ++i)
			{
				if (0 == _tcsnicmp(static_cast<LPCTSTR>(s)+i, _T("<A"), 2))
				{
					useLink = TRUE;
					break;
				}
			}
		}

		if (useLink)
		{
			m_ctrlFooterTextLink.SetHyperLinkExtendedStyle(
				HLINK_USETAGS | HLINK_NOTIFYBUTTON, 
				HLINK_USETAGS | HLINK_NOTIFYBUTTON);
			m_ctrlFooterTextLink.PreInit(m_hFont);
			m_ctrlFooterTextLink.SubclassWindow(GetDlgItem(m_nFooterID));
			m_ctrlFooterTextLink.Init(m_colorStandardText, m_colorDividerBackground, &m_brushDividerBackground);
		}
		else
		{
			m_ctrlFooterText.SubclassWindow(GetDlgItem(m_nFooterID));
			m_ctrlFooterText.Init(m_colorStandardText, m_colorDividerBackground, &m_brushDividerBackground);
		}
	}
  
	//Subclass the check box if necessary
	if (m_nVerificationCheckBoxID != -1)
	{
		m_ctrlVerificationCheckBox.SubclassWindow(GetDlgItem(m_nVerificationCheckBoxID));
		m_ctrlVerificationCheckBox.Init(m_colorTopBackground, &m_brushDividerBackground);
	}
  
	//Set up the callback timer if required
	ATLASSERT(m_pTaskConfig);
	if (m_pTaskConfig->dwFlags & TDF_CALLBACK_TIMER)
	{
		ATLASSERT(m_nTimerID == 0);
		m_nTimerID = SetTimer(1, 200, NULL);
	}
  
	//Set the tick count for TDN_TIMER notifications  
	m_dwLastTickCount = GetTickCount();

	//Finally play any sound which is required now that dialog has initialized  
	PlaySound();
  
	//Do the TDN_CREATED notification
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_CREATED, 0, 0, m_pTaskConfig->lpCallbackData);

	return 0;
}

LRESULT CXTaskDialog::OnHelp(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//Do the TDN_HELP notification
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_HELP, 0, 0, m_pTaskConfig->lpCallbackData);

	return 0;
}

void CXTaskDialog::MoveChildControl(int nID, int nVerticalMovement)
{
	HWND hwndChild = ::GetDlgItem(m_hWnd, nID);
	if (hwndChild && ::IsWindow(hwndChild))
	{
		CWindow ctrlItem(hwndChild);
		CRect rChild;
		ctrlItem.GetWindowRect(&rChild);
		ScreenToClient(&rChild);
		rChild.top += nVerticalMovement;
		rChild.bottom += nVerticalMovement;
		ctrlItem.MoveWindow(&rChild);
	}
}

void CXTaskDialog::HideChildControl(int nID)
{
	HWND hwndChild = ::GetDlgItem(m_hWnd, nID);
	if (hwndChild && ::IsWindow(hwndChild))
	{
		CWindow ctrlItem(hwndChild);
		ctrlItem.ShowWindow(SW_HIDE);
	}
}

void CXTaskDialog::ShowChildControl(int nID)
{
	HWND hwndChild = ::GetDlgItem(m_hWnd, nID);
	if (hwndChild && ::IsWindow(hwndChild))
	{
		CWindow ctrlItem(hwndChild);
		ctrlItem.ShowWindow(SW_SHOW);
	}
}

void CXTaskDialog::HandleExpando()
{
	//Finally toggle the state of the boolean
	m_bExpandedExpanded = !m_bExpandedExpanded;
  
	//And update the Expando button text
	HWND hwndChild = GetDlgItem(m_nExpandoButtonID);
	CWindow ctrlItem(hwndChild);
	ctrlItem.SetWindowText(m_bExpandedExpanded ? m_sExpandoExpandedCaption : m_sExpandoCollapsedCaption);

	//Hide all the relavent controls where we will be expanding or collapsing
	if (m_bDoExpandoAnimation)
		HideChildControl(m_nExpandedTextID);
	if (((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0) && m_bDoExpandoAnimation)
	{
		HideChildControl(m_nExpandoButtonID);
		HideChildControl(m_nVerificationCheckBoxID);
		HideChildControl(m_nFooterIconID);
		HideChildControl(m_nFooterID);
		HideChildControl(IDOK);
		HideChildControl(IDYES);
		HideChildControl(IDNO);
		HideChildControl(IDCANCEL);
		HideChildControl(IDRETRY);
		HideChildControl(IDCLOSE);
		UINT i;
		if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
		{
			for (i=0; i<m_pTaskConfig->cButtons; i++)
				HideChildControl(m_pTaskConfig->pButtons[i].nButtonID);
		}
	}
  
	//Work out the final height of our client area
	if ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0)
	{
		CRect rExpandedText;
		m_ctrlExpandedTextEx.GetWindowRect(&rExpandedText);
		m_nFinalExpandoVerticalMovement = rExpandedText.Height() + 2*SpacingSize;
		int nVerticalMovement = m_bExpandedExpanded ? m_nFinalExpandoVerticalMovement : -m_nFinalExpandoVerticalMovement;
    
		if (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS)      
		{
			//move the command links buttons straight away (Similiar to what Vista's TaskDialogIndirect does)
			for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
				MoveChildControl(m_pTaskConfig->pButtons[i].nButtonID, nVerticalMovement);
		}  

		//move the radio buttons straight away (Similiar to what Vista's TaskDialogIndirect does)
		for (UINT i=0; i<m_pTaskConfig->cRadioButtons; i++)
			MoveChildControl(m_pTaskConfig->pRadioButtons[i].nButtonID, nVerticalMovement);

		//move the progreess control straight away (Similiar to what Vista's TaskDialogIndirect does)    
		MoveChildControl(m_nProgressID, nVerticalMovement);
    
		if (m_bDoExpandoAnimation)
		{
			m_nCurrentExpandoVerticalMovement = 0;
			m_nFinalYDivider1 = m_nYDivider1 + nVerticalMovement;
    
			m_nFinalYDivider3 = m_nYDivider3;
			if (m_nYDivider2 != -1)
				m_nFinalYDivider2 = m_nYDivider2 + nVerticalMovement;
			else
				m_nFinalYDivider2 = -1;
        
			m_nYDivider1 = -1;
			m_nYDivider2 = -1;
			m_nYDivider3 = -1;
		}
		else
		{
			MoveChildControl(m_nExpandoButtonID, nVerticalMovement);
			MoveChildControl(m_nVerificationCheckBoxID, nVerticalMovement);
			MoveChildControl(m_nFooterIconID, nVerticalMovement);
			MoveChildControl(m_nFooterID, nVerticalMovement);
			MoveChildControl(IDOK, nVerticalMovement);
			MoveChildControl(IDYES, nVerticalMovement);
			MoveChildControl(IDNO, nVerticalMovement);
			MoveChildControl(IDCANCEL, nVerticalMovement);
			MoveChildControl(IDRETRY, nVerticalMovement);
			MoveChildControl(IDCLOSE, nVerticalMovement);
      
			if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
			{
				for (UINT i=0; i<m_pTaskConfig->cButtons; i++)
					MoveChildControl(m_pTaskConfig->pButtons[i].nButtonID, nVerticalMovement);
			}
     
			if (m_bExpandedExpanded)
				ShowChildControl(m_nExpandedTextID);
			else
				HideChildControl(m_nExpandedTextID);
        
			m_nYDivider1 += nVerticalMovement;
			if (m_nYDivider2 != -1)
				m_nYDivider2 += nVerticalMovement;
        
			CRect rWindow;
			GetWindowRect(&rWindow);
			rWindow.bottom += m_bExpandedExpanded ? m_nFinalExpandoVerticalMovement : -m_nFinalExpandoVerticalMovement;
			SetWindowPos(NULL, &rWindow, SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
		}
	}
	else
	{
		CRect rExpandedText;
		m_ctrlExpandedTextEx.GetWindowRect(&rExpandedText);
		CRect rWindow;
		GetWindowRect(&rWindow);
		ScreenToClient(&rWindow);
		CRect rClient;
		GetClientRect(&rClient);
		m_nFinalExpandoVerticalMovement = rExpandedText.Height() + 4*SpacingSize - (rWindow.bottom - rClient.bottom);
    
		if (m_bDoExpandoAnimation)
		{
			m_nCurrentExpandoVerticalMovement = 0;
			m_nFinalYDivider1 = m_nYDivider1;
			m_nFinalYDivider2 = m_nYDivider2;
			if (m_bExpandedExpanded)
				m_nFinalYDivider3 = rClient.bottom;
			else
				m_nFinalYDivider3 = -1;
			m_nYDivider3 = -1;
		}
		else
		{
			CRect rWindow;
			GetWindowRect(&rWindow);
			rWindow.bottom += m_bExpandedExpanded ? m_nFinalExpandoVerticalMovement : -m_nFinalExpandoVerticalMovement;
			SetWindowPos(NULL, &rWindow, SWP_NOACTIVATE | SWP_NOZORDER);
		}
	}
    
	//Force a redraw before we start the animation
	if (m_bDoExpandoAnimation)
	{
		//Set the focus to the expando button before we start the animation
		GetDlgItem(m_nExpandedTextID).SetFocus();
  
		Invalidate();
		UpdateWindow();

		//Create the timer to do the expando expansion / collapse animation
		ATLASSERT(m_nExpandoTimerID == 0);
		m_nExpandoTimerID = SetTimer(2, 25, NULL);
	}
  
	//Do the TDN_EXPANDO_BUTTON_CLICKED notification
	ATLASSERT(m_pTaskConfig);
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_EXPANDO_BUTTON_CLICKED, m_bExpandedExpanded, 0, m_pTaskConfig->lpCallbackData);
}

LRESULT CXTaskDialog::OnDestroy(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//Kill the timers if required
	if (m_nTimerID)
	{
		KillTimer(m_nTimerID);
		m_nTimerID = 0;
	}
	if (m_nExpandoTimerID)
	{
		KillTimer(m_nExpandoTimerID);
		m_nExpandoTimerID = 0;
	}
  
	//Do the TDN_DESTROYED notification
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_DESTROYED, 0, 0, m_pTaskConfig->lpCallbackData);
  
	return 0;
}

LRESULT CXTaskDialog::OnCommand(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	//Validate our parameters
	ATLASSERT(m_pTaskConfig);

	WORD nID = LOWORD(wParam);

	//Work out if it was a radio button which was clicked  
	BOOL bRadioClicked = FALSE;
	for (UINT i=0; i<m_pTaskConfig->cRadioButtons && !bRadioClicked; i++)
	{
		ATLASSERT(m_pTaskConfig->pRadioButtons);
		bRadioClicked = (nID == m_pTaskConfig->pRadioButtons[i].nButtonID);
	}
  
	if (bRadioClicked)
	{
		//Do the TDN_RADIO_BUTTON_CLICKED notification
		if (m_pTaskConfig->pfCallback)
			m_pTaskConfig->pfCallback(m_hWnd, TDN_RADIO_BUTTON_CLICKED, wParam, 0, m_pTaskConfig->lpCallbackData);
	}
	else if (nID == m_nExpandoButtonID)
	{
		//Call the function which handles the updates for the expando
		HandleExpando();
	}
	else if (nID == m_nVerificationCheckBoxID)
	{
		//Do the TDN_VERIFICATION_CLICKED notification
		if (m_pTaskConfig->pfCallback)
		{
			HWND hWndChild = ::GetDlgItem(m_hWnd, m_nVerificationCheckBoxID);
			CWindow ctrlChild(hWndChild);
			BOOL bChecked = ctrlChild.SendMessage(BM_GETCHECK, 0, 0) == BST_CHECKED;
			m_pTaskConfig->pfCallback(m_hWnd, TDN_VERIFICATION_CLICKED, bChecked, 0, m_pTaskConfig->lpCallbackData);
		}
	}
	else
	{
		//Handle the TDF_ALLOW_DIALOG_CANCELLATION flag
		if (((m_pTaskConfig->dwFlags & TDF_ALLOW_DIALOG_CANCELLATION) == 0) && (nID == IDCANCEL) && ((m_pTaskConfig->dwCommonButtons & TDCBF_CANCEL_BUTTON) == 0))
			return 0;
  
		//Do the TDN_BUTTON_CLICKED notification
		BOOL bDisallowClose = FALSE;
		if (m_pTaskConfig->pfCallback)
			bDisallowClose = (m_pTaskConfig->pfCallback(m_hWnd, TDN_BUTTON_CLICKED, nID, 0, m_pTaskConfig->lpCallbackData) == TRUE);
    
		if (!bDisallowClose)
		{
			//Save the state of the verification checkbox
			HWND hWndChild = ::GetDlgItem(m_hWnd, m_nVerificationCheckBoxID);
			if (hWndChild && ::IsWindow(hWndChild))
				m_bVerificationFlagChecked = (::SendMessage(hWndChild, BM_GETCHECK, 0, 0) == BST_CHECKED);
        
			//Save which radio button is set
			m_nRadioButtonChecked = -1;
			for (UINT i=0; i<m_pTaskConfig->cRadioButtons && (m_nRadioButtonChecked == -1); i++)
			{
				ATLASSERT(m_pTaskConfig->pRadioButtons);
				hWndChild = ::GetDlgItem(m_hWnd, m_pTaskConfig->pRadioButtons[i].nButtonID);
				CWindow ctrlChild(hWndChild);
				if (ctrlChild.SendMessage(BM_GETCHECK, 0, 0L) != 0)
					m_nRadioButtonChecked = m_pTaskConfig->pRadioButtons[i].nButtonID;
			}

			//Finally end the dialog
			EndDialog(nID);
		}
	}
	return 0;
}

LRESULT CXTaskDialog::OnTimer(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (wParam == m_nExpandoTimerID)
	{
		//Also expand or contract the task dialog client window 
		CRect rWindow;
		GetWindowRect(&rWindow);
		int nDelta = max(1, m_nFinalExpandoVerticalMovement/10);
		m_nCurrentExpandoVerticalMovement += nDelta;
		rWindow.bottom += m_bExpandedExpanded ? nDelta : -nDelta;
		SetWindowPos(NULL, &rWindow, SWP_NOACTIVATE | SWP_NOZORDER);
    
		//Should we finish the animation
		if (m_nCurrentExpandoVerticalMovement >= m_nFinalExpandoVerticalMovement)
		{
			//Kill the timer
			KillTimer(m_nExpandoTimerID);
			m_nExpandoTimerID = 0;
      
			//Move the controls into place
			int nVerticalMovement = m_bExpandedExpanded ? m_nFinalExpandoVerticalMovement : -m_nFinalExpandoVerticalMovement;
			if ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0)
			{
				MoveChildControl(m_nExpandoButtonID, nVerticalMovement);
				MoveChildControl(m_nVerificationCheckBoxID, nVerticalMovement);
				MoveChildControl(m_nFooterIconID, nVerticalMovement);
				MoveChildControl(m_nFooterID, nVerticalMovement);
				MoveChildControl(IDOK, nVerticalMovement);
				MoveChildControl(IDYES, nVerticalMovement);
				MoveChildControl(IDNO, nVerticalMovement);
				MoveChildControl(IDCANCEL, nVerticalMovement);
				MoveChildControl(IDRETRY, nVerticalMovement);
				MoveChildControl(IDCLOSE, nVerticalMovement);
				UINT i;
				if ((m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS) == 0)
				{
					for (i=0; i<m_pTaskConfig->cButtons; i++)
						MoveChildControl(m_pTaskConfig->pButtons[i].nButtonID, nVerticalMovement);
				}
			}

			//Also update the divider positions
			m_nYDivider1 = m_nFinalYDivider1;
			m_nYDivider2 = m_nFinalYDivider2;
			m_nYDivider3 = m_nFinalYDivider3;
      
			//Reshow all the child controls
			if (m_bExpandedExpanded)
				ShowChildControl(m_nExpandedTextID);
			if ((m_pTaskConfig->dwFlags & TDF_EXPAND_FOOTER_AREA) == 0)
			{
				ShowChildControl(m_nExpandoButtonID);
				ShowChildControl(m_nProgressID);
				ShowChildControl(m_nVerificationCheckBoxID);
				ShowChildControl(m_nFooterIconID);
				ShowChildControl(m_nFooterID);
				ShowChildControl(IDOK);
				ShowChildControl(IDYES);
				ShowChildControl(IDNO);
				ShowChildControl(IDCANCEL);
				ShowChildControl(IDRETRY);
				ShowChildControl(IDCLOSE);
				UINT i;
				for (i=0; i<m_pTaskConfig->cButtons; i++)
					ShowChildControl(m_pTaskConfig->pButtons[i].nButtonID);
				for (i=0; i<m_pTaskConfig->cRadioButtons; i++)
					ShowChildControl(m_pTaskConfig->pRadioButtons[i].nButtonID);
			}
      
			//Set the focus back to the expando button
			GetDlgItem(m_nExpandoButtonID).SetFocus();
        
			//Force a redraw of everything  
			Invalidate();
		}    
	}
	else if (wParam == m_nTimerID) //It was the callback timer
	{
		//Do the TDN_TIMER notification
		ATLASSERT(m_pTaskConfig);
		if (m_pTaskConfig->pfCallback)
		{
			DWORD dwCurrentTickCount = GetTickCount();
			if (m_pTaskConfig->pfCallback(m_hWnd, TDN_TIMER, dwCurrentTickCount - m_dwLastTickCount, 0, m_pTaskConfig->lpCallbackData) == TRUE)
				m_dwLastTickCount = dwCurrentTickCount;
		}
	}
  
	return 0;
}

LRESULT CXTaskDialog::OnEraseBackground(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDC hDC = reinterpret_cast<HDC>(wParam);

	CRect rClient;
	GetClientRect(&rClient);

	//Draw out the white section of the message box  
	if (m_nYDivider1 != -1)
	{
		//Draw the top of the task dialog in white
		//::SetBkColor(hDC, RGB(255, 255, 255));
		::SetBkColor(hDC, ::GetSysColor(COLOR_3DFACE));
		CRect rDraw(rClient.left, rClient.top, rClient.right, m_nYDivider1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
    
		//Draw the first dividing line
		::SetBkColor(hDC, m_colorDivider);
		rDraw = CRect(rClient.left, m_nYDivider1, rClient.right, m_nYDivider1+1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);

		//Draw the part of the task dialog up to the second dividing line or rest of the task dialog
		::SetBkColor(hDC, m_colorDividerBackground);
		if (m_nYDivider2 == -1)
			rDraw = CRect(rClient.left, m_nYDivider1+1, rClient.right, rClient.bottom);
		else
			rDraw = CRect(rClient.left, m_nYDivider1+1, rClient.right, m_nYDivider2-1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
	}
	else
	{
		//Draw the whole task dialog in white
		//::SetBkColor(hDC, RGB(255, 255, 255));
		::SetBkColor(hDC, ::GetSysColor(COLOR_3DFACE));
		CRect rDraw(rClient.left, rClient.top, rClient.right, rClient.bottom);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
	}
  
	if (m_nYDivider2 != -1)
	{
		//Draw the second dividing line
		::SetBkColor(hDC, m_colorDivider);
		CRect rDraw(rClient.left, m_nYDivider2-1, rClient.right, m_nYDivider2);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
		// ::SetBkColor(hDC, RGB(255, 255, 255));
		::SetBkColor(hDC, ::GetSysColor(COLOR_3DFACE));
		rDraw = CRect(rClient.left, m_nYDivider2, rClient.right, m_nYDivider2+1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);

		//Draw the part of the task dialog up to the second dividing line or rest of the task dialog
		::SetBkColor(hDC, m_colorDividerBackground);
		if (m_nYDivider3 == -1)
			rDraw = CRect(rClient.left, m_nYDivider2+1, rClient.right, rClient.bottom);
		else
			rDraw = CRect(rClient.left, m_nYDivider2+1, rClient.right, m_nYDivider3-1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
	} 
  
	if (m_nYDivider3 != -1)
	{
		//Draw the third dividing line
		::SetBkColor(hDC, m_colorDivider);
		CRect rDraw(rClient.left, m_nYDivider3-1, rClient.right, m_nYDivider3);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
		//::SetBkColor(hDC, RGB(255, 255, 255));
		::SetBkColor(hDC, ::GetSysColor(COLOR_3DFACE));
		rDraw = CRect(rClient.left, m_nYDivider3, rClient.right, m_nYDivider3+1);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);

		//Draw the rest of the task dialog
		::SetBkColor(hDC, m_colorDividerBackground);
		rDraw = CRect(rClient.left, m_nYDivider3+1, rClient.right, rClient.bottom);
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rDraw, NULL, 0, NULL);
	} 
  
	return 0;
}

LRESULT CXTaskDialog::OnUpdateIcon(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	//A flag we use in this function to correctly update the dialogs icon
	HICON hOurUsedIcon = NULL;

	if (wParam == TDIE_ICON_MAIN)
	{
		if (lParam == NULL)
		{
			//Destroy the current icon
			if (m_hMainIcon)
			{
				DestroyIcon(m_hMainIcon);
				m_hMainIcon = NULL;
			}
		}
		else
		{
			//Destroy the current icon
			if (m_hMainIcon)
			{
				DestroyIcon(m_hMainIcon);
				m_hMainIcon = NULL;
			}
    
			//Load up the new icon
			ATLASSERT(m_pTaskConfig);
			if (m_pTaskConfig->dwFlags & TDF_USE_HICON_MAIN)
			{
				HICON hNewIcon = reinterpret_cast<HICON>(lParam);
				m_hMainIcon = DuplicateIcon(NULL, hNewIcon);
				if (m_hMainIcon == NULL)
					ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (User specified), Error:%d\n"), GetLastError());
			}
			else 
			{
				LPCWSTR wszIcon = reinterpret_cast<LPCWSTR>(lParam);
				if (wszIcon == TD_ERROR_ICON)
				{
					hOurUsedIcon = m_hBigError;
					m_hMainIcon = DuplicateIcon(NULL, m_hBigError);
					if (m_hMainIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (Error), Error:%d\n"), GetLastError());
				}
				else if (wszIcon == TD_WARNING_ICON)
				{
					hOurUsedIcon = m_hBigWarning;
					m_hMainIcon = DuplicateIcon(NULL, m_hBigWarning);
					if (m_hMainIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (Warning), Error:%d\n"), GetLastError());
				}          
				else if (wszIcon == TD_INFORMATION_ICON)
				{
					hOurUsedIcon = m_hBigInformation;
					m_hMainIcon = DuplicateIcon(NULL, m_hBigInformation);
					if (m_hMainIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (Information), Error:%d\n"), GetLastError());
				}          
				else if (wszIcon == TD_SHIELD_ICON)
				{
					hOurUsedIcon = m_hBigShield;
					m_hMainIcon = DuplicateIcon(NULL, m_hBigShield);
					if (m_hMainIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (Shield), Error:%d\n"), GetLastError());
				}
				else if (m_pTaskConfig->hInstance && wszIcon && HIWORD(wszIcon) == NULL)
				{
					LPCTSTR pszIcon = MAKEINTRESOURCE(LOWORD(wszIcon));
					m_hMainIcon = LoadIcon(m_pTaskConfig->hInstance, pszIcon);
					if (m_hMainIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load main icon (User specified), Error:%d\n"), GetLastError());
				}
				else
				{
					ATLASSERT(FALSE);
				}
			}
		}
    
		//And force a redraw of the icon
		if (m_nMainIconID != -1)
		{
			CWindow ctrlMainIcon(GetDlgItem(m_nMainIconID));
			CRect rChild;
			ctrlMainIcon.GetWindowRect(&rChild);
			ScreenToClient(&rChild);
			InvalidateRect(rChild);
		}
    
		//Update the window icon also if required
		if (m_pTaskConfig->dwFlags & TDF_CAN_BE_MINIMIZED)
		{
			if (hOurUsedIcon)
				SetIcon(hOurUsedIcon, FALSE);
			else
				SetIcon(m_hMainIcon, FALSE);
			SetIcon(m_hMainIcon, TRUE);
      
			if (m_hMainIcon == NULL)
				Invalidate(); 
		}
	}
	else
	{
		if (lParam == NULL)
		{
			//Destroy the current icon
			if (m_hFooterIcon)
			{
				DestroyIcon(m_hFooterIcon);
				m_hFooterIcon = NULL;
			}
		}
		else
		{
			//Destroy the current icon
			if (m_hFooterIcon)
			{
				DestroyIcon(m_hFooterIcon);
				m_hFooterIcon = NULL;
			}
    
			//Load up the new icon
			ATLASSERT(m_pTaskConfig);
			if (m_pTaskConfig->dwFlags & TDF_USE_HICON_FOOTER)
			{
				HICON hNewIcon = reinterpret_cast<HICON>(lParam);
				m_hFooterIcon = DuplicateIcon(NULL, hNewIcon);
				if (m_hFooterIcon == NULL)
					ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon, Error:%d\n"), GetLastError());
			}
			else 
			{
				LPCWSTR wszIcon = reinterpret_cast<LPCWSTR>(lParam);
				if (wszIcon == TD_ERROR_ICON)
				{
					m_hFooterIcon = DuplicateIcon(NULL, m_hSmallError);
					if (m_hFooterIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon (Error), Error:%d\n"), GetLastError());
				}
				else if (wszIcon == TD_WARNING_ICON)
				{
					m_hFooterIcon = DuplicateIcon(NULL, m_hSmallWarning);
					if (m_hFooterIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon (Warning), Error:%d\n"), GetLastError());
				}
				else if (wszIcon == TD_INFORMATION_ICON)
				{
					m_hFooterIcon = DuplicateIcon(NULL, m_hSmallInformation);
					if (m_hFooterIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon (Information), Error:%d\n"), GetLastError());
				}
				else if (wszIcon == TD_SHIELD_ICON)
				{
					m_hFooterIcon = DuplicateIcon(NULL, m_hSmallShield);
					if (m_hFooterIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon (Shield), Error:%d\n"), GetLastError());
				}
				else if (m_pTaskConfig->hInstance && wszIcon && HIWORD(wszIcon) == NULL)
				{
					LPCTSTR pszIcon = MAKEINTRESOURCE(LOWORD(wszIcon));          
					m_hFooterIcon = reinterpret_cast<HICON>(LoadImage(m_pTaskConfig->hInstance, pszIcon, IMAGE_ICON, 16, 16, 0));
					if (m_hFooterIcon == NULL)
						ATLTRACE(_T("CXTaskDialog::OnUpdateIcon, Failed to load footer icon, Error:%d\n"), GetLastError());
				}
				else
				{
					ATLASSERT(FALSE);
				}
			}
		}
    
		//And force a redraw of the icon
		if (m_nFooterIconID != -1)
		{
			CWindow ctrlFooterIcon(GetDlgItem(m_nFooterIconID));
			CRect rChild;
			ctrlFooterIcon.GetWindowRect(&rChild);
			ScreenToClient(&rChild);
			InvalidateRect(rChild);
		}
	}
  
	return 0;
}

LRESULT CXTaskDialog::OnEnableButton(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	int nBtnID = static_cast<int>(wParam);
	GetDlgItem(nBtnID).EnableWindow(lParam != 0);
	return 0;
}

LRESULT CXTaskDialog::OnClickButton(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	//First set the button as having focus
	int nBtnID = static_cast<int>(wParam);
	HWND hWndChild = GetDlgItem(nBtnID);
	if (NULL != hWndChild)
	{
		CWindow ctrlChild(hWndChild);
		ctrlChild.SetFocus();
	}
  
	//Then post the WM_COMMAND message to ourselves to simulate the click
	return PostMessage(WM_COMMAND, wParam, lParam);
}

LRESULT CXTaskDialog::OnClickRadioButton(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	UINT nRadioButtonID = static_cast<UINT>(wParam);

	ATLASSERT(m_pTaskConfig);
	for (UINT i=0; i<m_pTaskConfig->cRadioButtons; i++)
	{
		ATLASSERT(m_pTaskConfig->pRadioButtons);
		HWND hWndChild = ::GetDlgItem(m_hWnd, m_pTaskConfig->pRadioButtons[i].nButtonID);
		CWindow ctrlChild(hWndChild);
    
		//Check / Uncheck all the relavent radio buttons
		if (nRadioButtonID == static_cast<UINT>(m_pTaskConfig->pRadioButtons[i].nButtonID))
		{
			ctrlChild.SendMessage(BM_SETCHECK, BST_CHECKED, 0);
			ctrlChild.SetFocus();
		}
		else
			ctrlChild.SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
	}

	return 0;
}

LRESULT CXTaskDialog::OnClickVerification(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	CWindow ctrlCheckBox(GetDlgItem(m_nVerificationCheckBoxID));
	ctrlCheckBox.SendMessage(BM_SETCHECK, wParam ? BST_CHECKED : BST_UNCHECKED, 0);
	if (lParam)
		ctrlCheckBox.SetFocus();

	return 0;
}

LRESULT CXTaskDialog::OnSetProgressBarRange(UINT /*nMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	ATLASSERT(m_nProgressID != -1);
	HWND hWndChild = GetDlgItem(m_nProgressID);
	CWindow ctrlChild(hWndChild);

	return ctrlChild.SendMessage(PBM_SETRANGE, 0, lParam);
}

//Note that since PBM_SETMARQUEE is only supported on Common Control v6, you will only be able to use
//CXTaskDialog's support for TDM_SET_MARQUEE_PROGRESS_BAR / TDF_SHOW_MARQUEE_PROGRESS_BAR on an app 
//which is linked with a Common Control manifest and is running on Windows XP or later.
LRESULT CXTaskDialog::OnSetMarqueeProgressBar(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	ATLASSERT(m_nProgressID != -1);
	HWND hwndChild = GetDlgItem(m_nProgressID);
	CWindow ctrlProgress(hwndChild);
	BOOL bMarquee = static_cast<BOOL>(wParam);
	if (bMarquee)
	{
		ctrlProgress.ModifyStyle(0, PBS_MARQUEE); 
		ctrlProgress.SendMessage(PBM_SETMARQUEE, TRUE, lParam);
	}
	else
		ctrlProgress.ModifyStyle(PBS_MARQUEE, 0); 
	return bMarquee;
}

LRESULT CXTaskDialog::OnSetProgressBarPos(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ATLASSERT(m_nProgressID != -1);
	HWND hWndChild = GetDlgItem(m_nProgressID);
	CWindow ctrlChild(hWndChild);
	return ctrlChild.SendMessage(PBM_SETPOS, wParam, 0);
}

//Note that since PBM_SETSTATE is only support on Vista's Common Control, you will only be able to use
//CXTaskDialog's support for TDM_SET_PROGRESS_BAR_STATE state on Vista
LRESULT CXTaskDialog::OnSetProgressBarState(UINT /*nMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ATLASSERT(m_nProgressID != -1);
	HWND hWndChild = GetDlgItem(m_nProgressID);
	CWindow ctrlChild(hWndChild);
	ctrlChild.SendMessage(PBM_SETSTATE, wParam, 0);
	return 1;
}

LRESULT CXTaskDialog::OnSetElementText(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	TASKDIALOG_ELEMENTS element = static_cast<TASKDIALOG_ELEMENTS>(wParam);
	HWND hWndChild = NULL;
	switch (element)
	{
    case TDE_CONTENT:
    {
		hWndChild = GetDlgItem(m_nContentID);
		break;
    }
    case TDE_EXPANDED_INFORMATION:
    {
		hWndChild = GetDlgItem(m_nExpandedTextID);
		break;
    }
    case TDE_FOOTER:
    {
		hWndChild = GetDlgItem(m_nFooterID);
		break;
    }
    case TDE_MAIN_INSTRUCTION:
    {
		hWndChild = GetDlgItem(m_nMainInstructionID);
		break;
    }
    default:
    {
		ATLASSERT(FALSE);
		break;
    }
	}

	//What will be the return value from this function (assume the worst)
	BOOL bSuccess = FALSE;

	//Set the caption on the specified item if we can
	if (hWndChild && ::IsWindow(hWndChild))
	{
		//Update the text
		CWindow ctrlItem(hWndChild);
		ctrlItem.SetWindowText(CW2T(reinterpret_cast<LPCWSTR>(lParam)));

		bSuccess = TRUE;
	}  
  
	return bSuccess;
}

LRESULT CXTaskDialog::OnUpdateElementText(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	TASKDIALOG_ELEMENTS element = static_cast<TASKDIALOG_ELEMENTS>(wParam);
	HWND hWndChild = NULL;
	switch (element)
	{
    case TDE_CONTENT:
    {
		hWndChild = GetDlgItem(m_nContentID);
		break;
    }
    case TDE_EXPANDED_INFORMATION:
    {
		hWndChild = GetDlgItem(m_nExpandedTextID);
		break;
    }
    case TDE_FOOTER:
    {
		hWndChild = GetDlgItem(m_nFooterID);
		break;
    }
    case TDE_MAIN_INSTRUCTION:
    {
		hWndChild = GetDlgItem(m_nMainInstructionID);
		break;
    }
    default:
    {
		ATLASSERT(FALSE);
		break;
    }
	}
  
	//Set the caption on the specified item if we can
	if (hWndChild && ::IsWindow(hWndChild))
	{
		//Update the text
		CWindow ctrlItem(hWndChild);
		ctrlItem.SetWindowText(CW2T(reinterpret_cast<LPCWSTR>(lParam)));
	}
    
	return TRUE;
}

LRESULT CXTaskDialog::OnEnableRadioButton(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	int nBtnID = static_cast<int>(wParam);
  
	HWND hWndChild = GetDlgItem(nBtnID);
	if (hWndChild && ::IsWindow(hWndChild))
	{
		CWindow ctrlItem(hWndChild);
		ctrlItem.EnableWindow(lParam != 0);
	}
	return 0;
}

//Note that currently CXTaskDialog only supports showing the shield icon on command link buttons and not the 
//standard buttons!
LRESULT CXTaskDialog::OnSetButtonElevationRequiredState(UINT /*nMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	int nBtnID = static_cast<int>(wParam);
	if (lParam)
		GetDlgItem(nBtnID).SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(m_hSmallShield));
	else
	{
		if (m_pTaskConfig->dwFlags & TDF_USE_COMMAND_LINKS_NO_ICON)
			GetDlgItem(nBtnID).SendMessage(BM_SETIMAGE, IMAGE_ICON, NULL);
		else
			GetDlgItem(nBtnID).SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(m_hCommandLink));
	}
	return 0;
}

//Note that since SysLink (Aka Hyperlink) controls are only supported on Common Control v6, you will only be able to use
//CXTaskDialog's support for TDF_ENABLE_HYPERLINKS on an app which is linked with a Common Control manifest and is 
//running on Windows XP or later.
LRESULT CXTaskDialog::OnClick(int /*nID*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	ATLASSERT(pnmh);
	NMLINK* pnmLink = reinterpret_cast<NMLINK*>(pnmh);

	//Do the TDN_HYPERLINK_CLICKED notification
	if (m_pTaskConfig->pfCallback)
		m_pTaskConfig->pfCallback(m_hWnd, TDN_HYPERLINK_CLICKED, 0, reinterpret_cast<LPARAM>(pnmLink->item.szUrl), m_pTaskConfig->lpCallbackData);

	return 0;
}
