////////////////////////////////////////////////////////////////////////////
//
// Interface of CBindSheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBBINDPAGES_H_
#define _NBBINDPAGES_H_

#include "resource.h"
#include "nblistviewctrl.h"
#include "nbdisklistctrl.h"
#include "nbarrowbutton.h"

class CBindSheet;
class CBindPage 
{
protected:
	CBindSheet *m_pSheet;
	CBindSheet *GetParentSheet() { return m_pSheet; }
public:
	void SetParentSheet(CBindSheet *pSheet){ m_pSheet = pSheet; }
};
//////////////////////////////////////////////////////////////////////////
// Page 1
//	- Displays general description and let users select type of binding
//////////////////////////////////////////////////////////////////////////
typedef enum _BIND_TYPE 
{
	BIND_TYPE_AGGR = 0,
	BIND_TYPE_RAID0,
	BIND_TYPE_RAID1,
	BIND_TYPE_RAID4
};

class CBindPage1
	: public CPropertyPageImpl<CBindPage1>,
	  public CBindPage,
	  public CWinDataExchange<CBindPage1>
{
protected:
	int  m_nType;
	UINT m_nDiskCount, m_nDiskCount2;
public:
	enum { IDD = IDD_BIND_PAGE1 };
	CBindPage1( _U_STRINGorID title = _T("") )
		: CPropertyPageImpl<CBindPage1>(title), 
		  m_nType(BIND_TYPE_AGGR), m_nDiskCount(2)
	{
	}

	BEGIN_MSG_MAP_EX(CBindPage1)
		MSG_WM_INITDIALOG(OnInitDialog)
		CHAIN_MSG_MAP(CPropertyPageImpl<CBindPage1>)
		MSG_WM_COMMAND(OnCommand)
	END_MSG_MAP()

	BEGIN_DDX_MAP(CBindPage1)
		DDX_RADIO(IDC_BIND_TYPE_AGGR, m_nType)
		DDX_UINT(IDC_COMBO_DISKCOUNT, m_nDiskCount)
	END_DDX_MAP()

	LRESULT OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/);
	
	VOID RefreshDiskCountComboBox(_BIND_TYPE nType);

	BOOL OnKillActive();
	BOOL OnSetActive();
	LRESULT OnCommand(UINT msg, int nID, HWND hWnd);
};

//////////////////////////////////////////////////////////////////////////
// Page 2
//	- Let users input number of disks to bind
//////////////////////////////////////////////////////////////////////////
class CBindPage2
	: public CPropertyPageImpl<CBindPage2>,
	public CBindPage,
	public CWinDataExchange<CBindPage2>
{
protected:
	CNBListViewCtrl m_wndListSingle;
	CNBBindListViewCtrl m_wndListBind;
	CDiskListCtrl m_wndDiskList;
	CArrowButton<ARROW_RIGHT>	m_btnAdd;
	CArrowButton<ARROW_LEFT>	m_btnRemove;
//	CArrowButton<ARROW_BOTTOM>	m_btnDown;
//	CArrowButton<ARROW_TOP>		m_btnUp;
	UINT			m_nDiskCount;
	int				m_nType;
	
	// Variables for drag & drop
	CImageList	m_imgDrag;
	BOOL		m_bDragging;
	BOOL		m_bDragFromBindList;
	BOOL		m_bPtInBindList;
	CDiskObjectList m_listDrag;
	int			m_nItemHit;			// index of the item under the mouse
public:
	enum { IDD = IDD_BIND_PAGE2 };
	CBindPage2( _U_STRINGorID title = _T("") )
		: CPropertyPageImpl<CBindPage2>(title), m_nDiskCount(0), m_bDragging(FALSE),
		m_nItemHit(-1)
	{
	}
	BEGIN_MSG_MAP_EX(CBindPage2)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_MOUSEMOVE(OnMouseMove)
		MSG_WM_LBUTTONUP(OnLButtonUp)
		MESSAGE_HANDLER_EX(WM_USER_NB_VIEW_LDBLCLK, OnDblClkListSingle)
		MESSAGE_HANDLER_EX(WM_USER_NB_BIND_VIEW_LDBLCLK, OnDblClkListBind)	
		CHAIN_MSG_MAP(CPropertyPageImpl<CBindPage2>)
		COMMAND_ID_HANDLER_EX(IDC_BTN_ADD, OnClickAddRemove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_REMOVE, OnClickAddRemove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_UP, OnClickMove)
		COMMAND_ID_HANDLER_EX(IDC_BTN_DOWN, OnClickMove)
		NOTIFY_CODE_HANDLER_EX(LVN_ITEMCHANGED, OnListSelChanged)
		NOTIFY_CODE_HANDLER_EX(LVN_BEGINDRAG, OnListBeginDrag)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	//
	// Returns true if disks in the list can be added to bind list
	//
	BOOL CanAddDisksToBindList(CDiskObjectList listDisks);
	void UpdateControls();
	//
	// Move items between two list controls
	//
	// @param bFromSingle [in] If true, move items from the list of single disks
	//						   to the list of disks to bind.
	void MoveBetweenLists(BOOL bFromSingle);

	LRESULT OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/);
	BOOL OnSetActive();
	BOOL OnKillActive();
	BOOL OnWizardFinish();
	//
	// Handler method when the user clicks add button or remove button
	//
	void OnClickAddRemove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/);
	//
	// Handler method when the user clicks move buttons
	//
	void OnClickMove(UINT /*wNotifyCode*/, int wID, HWND /*hwndCtl*/);

	//
	// Handler methods for drag & drop
	//
	void OnMouseMove(UINT /*nFlags*/, CPoint point);
	void OnLButtonUp(UINT /*nFlags*/, CPoint point);
	LRESULT OnDblClkListSingle(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDblClkListBind(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnListBeginDrag(LPNMHDR lpNMHDR);

	LRESULT OnListSelChanged(LPNMHDR lpNMHDR);
};


#endif // _NBBINDPAGES_H_