#pragma once
#include <atlcrack.h>
#include "pix.h"

template<class T, class TBase = CStatic, class TWinTraits = CControlWinTraits>
class ATL_NO_VTABLE CImageCtrlImpl : 
	public CWindowImpl<T, TBase, TWinTraits>
{
protected:
	UINT m_nImageID;
	CPix m_pix;
public:
	typedef CImageCtrlImpl<T, TBase, TWinTraits> thisClass;
	typedef CWindowImpl<T, TBase, TWinTraits> superClass;

	BEGIN_MSG_MAP_EX( thisClass )
		MSG_WM_PAINT(OnPaint)
	END_MSG_MAP()

	BOOL SetImage(UINT nImageID)
	{
		return m_pix.LoadFromResource(
			_Module.GetResourceInstance(), 
			nImageID, 
			_T("IMAGE"));
	}

	void OnPaint(HDC /*hDC*/)
	{
		CPaintDC dc(m_hWnd);
		CRect rectClient;
		GetClientRect( rectClient );

		CDCHandle dcHandle(dc);
		m_pix.Render( dcHandle, rectClient );
	}
};

class CImageCtrl : 
	public CImageCtrlImpl<CImageCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, GetWndClassName());
};

