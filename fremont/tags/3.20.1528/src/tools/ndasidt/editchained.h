#pragma once

template <class T>
class CChainedEditT : public CWindowImpl<CChainedEditT<T>, CEdit>
{
public:
	BEGIN_MSG_MAP(CChainedEditT<T>)
		MESSAGE_HANDLER(WM_KEYDOWN, OnKeyDown)
	END_MSG_MAP()

	LRESULT OnKeyDown(UINT uMsg, WPARAM wParam,LPARAM lParam, BOOL& bHandled)
	{
		switch (wParam)
		{
		case VK_BACK:
		case VK_LEFT:
			{
				int startPos = 0, endPos = 0;
				GetSel(startPos, endPos);
				if (0 == startPos && 0 == endPos && NULL != m_hWndPrevCtl)
				{
					ATLTRACE("Go Prev\n");
					CWindow wndCtl = m_hWndPrevCtl;
					wndCtl.SetFocus();
					return 0;
				}
				break;
			}
		case VK_RIGHT:
			{
				int startPos = 0, endPos = 0;
				GetSel(startPos, endPos);
				int len = GetWindowTextLength();
				if (len == startPos && len == endPos)
				{
					CWindow wndNext = ::GetNextDlgTabItem(GetParent(), m_hWnd, FALSE);
					ATLTRACE("Go Next %p\n", wndNext.m_hWnd);
					wndNext.SetFocus();
					//CWindow wndCtl = m_hWndNextCtl;
					//wndCtl.SetFocus();
					return 0;
				}
				break;
			}
		}
		bHandled = FALSE;
		return 0;
	}

	CChainedEditT() : m_hWndPrevCtl(NULL) {}

	void SetPrevCtrl(HWND hWnd)
	{
		m_hWndPrevCtl = hWnd;
	}

	void SetNextCtrl(HWND hWnd)
	{
		m_hWndNextCtl = hWnd;
	}

	HWND m_hWndPrevCtl;
	HWND m_hWndNextCtl;
};

typedef CChainedEditT<CWindow> CChainedEdit;
