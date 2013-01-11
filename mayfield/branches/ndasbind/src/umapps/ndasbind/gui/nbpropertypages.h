////////////////////////////////////////////////////////////////////////////
//
// Interface of CDiskPropertySheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBPROPERTYPAGES_H_
#define _NBPROPERTYPAGES_H_

#include <vector>
#include "resource.h"
#include "ndasobject.h"

class CDiskPropertySheet;
class CDiskPropertyPage
{
protected:
	CDiskPropertySheet *m_pSheet;
	CDiskPropertySheet *GetParentSheet() { return m_pSheet; }
public:
	void SetParentSheet(CDiskPropertySheet *pSheet){ m_pSheet = pSheet; }
};

//////////////////////////////////////////////////////////////////////////
// Page 1
//	- Displays general information about the disk
//////////////////////////////////////////////////////////////////////////
class CDiskPropertyPage1
	: public CPropertyPageImpl<CDiskPropertyPage1>,
	  public CWinDataExchange<CDiskPropertyPage1>,
	  public CDiskPropertyPage
{
protected:

public:
	enum { IDD = IDD_PROPERTY_PAGE1 };
	CDiskPropertyPage1()
		: CPropertyPageImpl<CDiskPropertyPage1>(IDS_DISKPROPERTYPAGE_GENERALTAB)
	{
		// TODO : String resource
	}
	BEGIN_MSG_MAP_EX(CDiskPropertyPage1)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_BTN_MIGRATE, OnMigrate)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/);
	void OnMigrate(UINT wNotifyCode, int wID, HWND hwndCtl);

};

//////////////////////////////////////////////////////////////////////////
// Page 2
//	- Displays hardware information
//////////////////////////////////////////////////////////////////////////
class CToolTipListCtrl : 
	public CWindowImpl<CToolTipListCtrl, CListViewCtrl>
{
protected:
	std::vector<WTL::CString> m_vtToolTip;
public:
	BEGIN_MSG_MAP_EX(CToolTipListCtrl)
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_GETINFOTIP, OnGetInfoTip)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	int InsertItem(LPCTSTR szCol1, LPCTSTR szCol2, LPCTSTR szToolTip);
	BOOL SubclassWindow(HWND hWnd);
	LRESULT OnGetInfoTip(LPNMHDR lParam);
};

class CDiskPropertyPage2
	: public CPropertyPageImpl<CDiskPropertyPage2>,
	  public CWinDataExchange<CDiskPropertyPage2>,
	  public CDiskPropertyPage
{
protected:
	CToolTipListCtrl m_listProperty;
public:
	enum { IDD = IDD_PROPERTY_PAGE2 };
	CDiskPropertyPage2()
		: CPropertyPageImpl<CDiskPropertyPage2>(IDS_DISKPROPERTYPAGE_HARDWARETAB)
	{
		
	}
	BEGIN_MSG_MAP_EX(CDiskPropertyPage2)
		MSG_WM_INITDIALOG(OnInitDialog)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()
	LRESULT OnInitDialog(HWND /*hWndFocus*/, LPARAM /*lParam*/);
};

//////////////////////////////////////////////////////////////////////////
// Page 3
//	- Used for disks whose disk information block cannot be recognized
//////////////////////////////////////////////////////////////////////////
class CDiskPropertyPage3
	: public CPropertyPageImpl<CDiskPropertyPage3>
{
protected:
public:
	enum { IDD = IDD_PROPERTY_PAGE3 };
	CDiskPropertyPage3()
		: CPropertyPageImpl<CDiskPropertyPage3>(IDS_DISKPROPERTYPAGE_GENERALTAB)
	{
	}
	BEGIN_MSG_MAP_EX(CDiskPropertyPage3)
	END_MSG_MAP()
};

#endif // _NBPROPERTYPAGES_H_