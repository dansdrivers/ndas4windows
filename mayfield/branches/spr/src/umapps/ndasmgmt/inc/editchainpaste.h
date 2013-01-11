#pragma once

/////////////////////////////////////////////////////////////////////////////
// EditChainPaste - Implementing Chaining Paste for a Edit control
//
// Written by Chesong Lee <cslee@ximeta.com>
// Copyright (c) 2003 XIMETA, Inc.
//

#ifndef __cplusplus
#error WTL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLCTRLS_H__
#error EditChainPaste.h requires atlcrack.h to be included first
#endif

#define ECP_CHAINING_PASTE (WM_USER + 0x342)

class CEditChainPaste : public CMessageMap
{
public:
	CContainedWindowT<CEdit> m_ctrlEdit;
	CWindow m_wndOwner;
	CEdit m_wndNextEdit;

	BEGIN_MSG_MAP_EX(CEditChainPaste)
	ALT_MSG_MAP(1)
		MSG_WM_PASTE(OnPaste)
		MESSAGE_HANDLER_EX(ECP_CHAINING_PASTE, OnChainingPaste)
	END_MSG_MAP()

	//
	// WM_USER + 0x342 may conflict with other message
	// the following is a better implementation
	// However, non-const static member requires using cpp obj for initialization
	//
	// static UINT _uECP_Chaining_Paste = 0;
	// ...
	// if (0 == _uECP_Chaining_Paste) {
	//	_uECP_Chaning_Paste = RegisterWindowsMessage(TEXT("ECP_CHAINING_PASTE"))
	// }
	// 

	CEditChainPaste() :
		m_ctrlEdit(this, 1) 
	{}

	void Attach(HWND hwndOwner, HWND hwndEdit, HWND hwndNextEdit = NULL)
	{
		ATLASSERT(::IsWindow(hwndOwner));
		ATLASSERT(::IsWindow(hwndEdit));
		ATLASSERT(!m_ctrlEdit.IsWindow()); // Only attach once

		m_wndOwner = hwndOwner;
		m_ctrlEdit.SubclassWindow(hwndEdit);
		m_wndNextEdit = hwndNextEdit;
	}

	//
	// Message handlers
	//

	void OnPaste()
	{
		ATLTRACE(TEXT("OnPaste!\n"));

		// handle only when the current edit control has no text
		if (m_ctrlEdit.GetWindowTextLength() > 0) {
			SetMsgHandled(FALSE);
			return;
		}

		HGLOBAL hClip;
		LPCTSTR lpText = _GetClipboardText(hClip);
		if (lpText) {

			LPTSTR lpNext = _PasteToCtrl(lpText, m_ctrlEdit);

			if (lpNext && m_wndNextEdit.IsWindow()) {
				m_wndNextEdit.SendMessage(ECP_CHAINING_PASTE, (WPARAM)lpNext, 0);
			}

			::GlobalUnlock(hClip);
			::CloseClipboard();

			SetMsgHandled(TRUE);

		} else {

			SetMsgHandled(FALSE);

		}

	}

	LRESULT OnChainingPaste(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		ATLTRACE(TEXT("Chaining paste - wParam %d\n"), wParam);
		LPTSTR lpCur = (LPTSTR) wParam;
		LPTSTR lpNext = _PasteToCtrl(lpCur, m_ctrlEdit);

		if (lpNext && m_wndNextEdit.IsWindow()) {
			m_wndNextEdit.SendMessage(ECP_CHAINING_PASTE, (WPARAM)lpNext, 0);
		}
		return 0;
	}

	//
	// internal implementations
	//

	LPTSTR _PasteToCtrl(LPCTSTR lpStartChar, CEdit& editCtrl)
	{
		//
		// We only paste 5 characters to each control - m_wndStringIDs[i]
		//
		UINT uMaxText = editCtrl.GetLimitText();
		LPTSTR lpCur = const_cast<LPTSTR>(lpStartChar);
		DWORD dwCnt(0);
		for (; *lpCur != TEXT('\0') && dwCnt < uMaxText; ++dwCnt, ++lpCur) 
		{}

		TCHAR lpszToPaste[256]; // plus NULL
		// Get pasting token which is 5 chars or less 
		HRESULT hr = ::StringCchCopyN(lpszToPaste, 256, lpStartChar, uMaxText);
		ATLASSERT(SUCCEEDED(hr));

		editCtrl.SetFocus();
		editCtrl.SetWindowText(lpszToPaste);

		LPCTSTR delimiters = TEXT("- \t\n");
		// if we see the valid delimiter skip it!
		for (; *lpCur == delimiters[0] || *lpCur == delimiters[1] ||
			*lpCur == delimiters[2] || *lpCur == delimiters[3]; ++lpCur) 
		{}

		if (*lpCur == TEXT('\0'))
			return NULL;

		return lpCur;
	}

	//
	// be sure to call :GlobalUnlock(hData) and ::CloseClipboard()
	// after finished processing
	//

	LPCTSTR _GetClipboardText(HGLOBAL& hGlobal)
	{

#ifdef UNICODE
		UINT uClipFormat(CF_UNICODETEXT);
#else
		UINT uClipFormat(CF_TEXT);
#endif

		if (!::IsClipboardFormatAvailable(uClipFormat)) {
			return FALSE;
		}

		if (!::OpenClipboard(m_wndOwner)) {
			return FALSE;
		}

		hGlobal = ::GetClipboardData(uClipFormat);
		if (NULL == hGlobal) {
			::CloseClipboard();
			return FALSE;
		}

		LPTSTR lpszClipboardText = (LPTSTR) ::GlobalLock(hGlobal);
		if (NULL == lpszClipboardText) {
			::CloseClipboard();
		}

		return lpszClipboardText;
	}

};
