#pragma once
#include "propertylist.h"

namespace devprop {

class CHardwarePage :
	public CPropertyPageImpl<CHardwarePage>
{
protected:
	CPropertyListCtrl m_propList;

	ndas::Device* m_pDevice;

public:
	enum { IDD = IDD_DEVPROP_HW };

	BEGIN_MSG_MAP_EX(CHardwarePage)
		MSG_WM_INITDIALOG(OnInitDialog)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	CHardwarePage();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	void SetDevice(ndas::Device* pDevice);

};

class CAddWriteKeyDlg :
	public CDialogImpl<CAddWriteKeyDlg>
{
	CString m_strWriteKey;
	CString m_strDeviceName;
	CString m_strDeviceId;

	CButton m_butOK;

public:
	enum { IDD = IDD_ADD_WRITE_KEY };

	BEGIN_MSG_MAP_EX(CAddWriteKeyDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEVICE_WRITE_KEY,EN_CHANGE,OnWriteKeyChange)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
//		CHAIN_MSG_MAP(CDialogImpl<CAddWriteKeyDlg>)
	END_MSG_MAP()

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	void SetDeviceName(LPCTSTR szNAme);
	void SetDeviceId(LPCTSTR szDeviceId);
	LPCTSTR GetWriteKey();

	void OnWriteKeyChange(UINT, int, HWND);
	void OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
	void OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
};

class CRenameDialog :
	public CDialogImpl<CRenameDialog>
{
	static const MAX_NAME_LEN = MAX_NDAS_DEVICE_NAME_LEN;
	TCHAR m_szName[MAX_NAME_LEN + 1];
	TCHAR m_szOldName[MAX_NAME_LEN + 1];
	CEdit m_wndName;
	CButton m_wndOK;

public:
	enum { IDD = IDD_RENAME };

	BEGIN_MSG_MAP_EX(CRenameDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_HANDLER_EX(IDC_DEVICE_NAME, EN_CHANGE, OnDeviceNameChange)
	END_MSG_MAP()

	CRenameDialog();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	void SetName(LPCTSTR szName);
	LPCTSTR GetName();

	LRESULT OnClose(
		WORD /*wNotifyCode*/, 
		WORD wID, 
		HWND /*hWndCtl*/, 
		BOOL& /*bHandled*/);

	LRESULT OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl);
};

class CGeneralPage :
	public CPropertyPageImpl<CGeneralPage>,
	public CWinDataExchange<CGeneralPage>
{
	ndas::Device* m_pDevice;

	TCHAR m_chConcealed;

	CEdit m_edtDevName;
	CEdit m_edtDevId;
	CEdit m_edtDevStatus;
	CEdit m_edtDevWriteKey;
	CButton m_butAddRemoveDevWriteKey;

	CWindow m_edtUnitDevType;
	CEdit m_edtUnitDevStatus;
	CEdit m_edtUnitDevCapacity;
	CEdit m_edtUnitDevROHosts;
	CEdit m_edtUnitDevRWHosts;

	CStatic m_wndUnitDevIcon;

	HICON m_hUnitDevIcon;

	CTreeViewCtrlEx m_tvLogDev;

public:
	enum { IDD = IDD_DEVPROP_GENERAL };

	BEGIN_MSG_MAP_EX(CGeneralPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_RENAME,OnRename)
		COMMAND_ID_HANDLER_EX(IDC_ADD_WRITE_KEY, OnModifyWriteKey)
	END_MSG_MAP()

	CGeneralPage();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	void SetDevice(ndas::Device* pDevice);
	void OnRename(UINT, int, HWND);

	void OnModifyWriteKey(UINT, int, HWND);

	VOID UpdateUnitDeviceData(ndas::UnitDevice* pUnitDevice);
	VOID UpdateData();

	void GenerateLogDevTree(ndas::UnitDevice* pUnitDevice);

};

class CHostStatPage :
	public CPropertyPageImpl<CHostStatPage>
{
protected:

	ndas::UnitDevice* m_pUnitDevice;

	CListViewCtrl m_wndListView;

public:
	enum { IDD = IDD_DEVPROP_HOSTINFO };

	BEGIN_MSG_MAP_EX(CHostStatPage)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP()

	CHostStatPage();
	CHostStatPage(ndas::UnitDevice* pUnitDevice);
	virtual ~CHostStatPage();

	VOID SetUnitDevice(ndas::UnitDevice* pUnitDevice);
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

};

class CAdvancedPage :
	public CPropertyPageImpl<CAdvancedPage>
{
protected:

	ndas::Device* m_pDevice;

	CButton m_wndDeactivate;
	CButton m_wndReset;

public:
	enum { IDD = IDD_DEVPROP_ADVANCED };

	BEGIN_MSG_MAP_EX(CAdvancedPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_DEACTIVATE_DEVICE, OnDeactivateDevice)
		COMMAND_ID_HANDLER_EX(IDC_RESET_DEVICE, OnResetDevice)
	END_MSG_MAP()

	CAdvancedPage();
	CAdvancedPage(ndas::Device* pDevice);
	virtual ~CAdvancedPage();

	VOID SetDevice(ndas::Device* pDevice);
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	VOID OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
	VOID OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
};

class CDevicePropSheet :
	public CPropertySheetImpl<CDevicePropSheet>
{
	BOOL m_bCentered;
	CGeneralPage m_pspGeneral;
	CAdvancedPage m_pspAdvanced;
	CHardwarePage m_pspHardware;
	CHostStatPage* m_pspHostStats;

	ndas::Device* m_pDevice;

public:

	BEGIN_MSG_MAP_EX(CDevicePropSheet)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<CDevicePropSheet>)
	END_MSG_MAP()

	CDevicePropSheet(
		_U_STRINGorID title = (LPCTSTR) NULL,
		UINT uStartPage = 0,
		HWND hWndParent = NULL);
	virtual ~CDevicePropSheet();

	void SetDevice(ndas::Device* pDevice);
	void OnShowWindow(BOOL bShow, UINT nStatus);

};

} // namespace devprop
