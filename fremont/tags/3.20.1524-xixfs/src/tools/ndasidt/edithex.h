#pragma once

template < class T>
class CEditHexOnlyT : public CWindowImpl< CEditHexOnlyT< T > ,CEdit>
{
	typedef CWindowImpl< CEditHexOnlyT< T > ,CEdit> BaseClass;
public:
	BEGIN_MSG_MAP(CEditHexOnlyT<T>)
		MESSAGE_HANDLER(WM_CHAR, OnChar)
	END_MSG_MAP()

	LRESULT OnChar(UINT uMsg, WPARAM wParam,LPARAM lParam, BOOL& bHandled)
	{
		TCHAR nChar = static_cast<char>(wParam);
		if ((nChar >= '0' && nChar <= '9') ||
			(nChar >= 'a' && nChar <= 'f') ||
			(nChar >= 'A' && nChar <= 'F'))
		{
			bHandled = FALSE;
			return 0;
		}
		if (nChar == VK_BACK)
		{
			bHandled = FALSE;
			return 0;
		}
		bHandled = TRUE;
		return 0;
	}
};

typedef CEditHexOnlyT<CWindow> CEditHexOnly;

