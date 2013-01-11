#pragma once
#include "propertylist.h"
#include <boost/shared_ptr.hpp>
#include <xtl/xtlautores.h>
#include <xtl/xtlthread.h>

template <typename T>
class CWorkerThreadBase
{
public:
	DWORD OnWorkerThreadStart()
	{
		// OnWorkerThreadStart Routine should be implemented 
		// by the inheritor
		C_ASSERT(FALSE); 
	}
	HANDLE CreateWorkerThread(
		LPSECURITY_ATTRIBUTES lpsa = NULL, 
		DWORD dwStackSize = 0,
		DWORD dwCreationFlags = 0,
		LPDWORD lpThreadId = NULL)
	{
		T* pThis = static_cast<T*>(this);
		return ::CreateThread(lpsa, dwStackSize, T::WorkerThreadStartRoutine, pThis, dwCreationFlags, lpThreadId);
	}

	static DWORD WINAPI WorkerThreadStartRoutine(LPVOID lpParameter)
	{
		T* pThis = static_cast<T*>(lpParameter);
		return pThis->OnWorkerThreadStart();
	}

	typedef struct _THREAD_PARAM {
		T* pThis;
		LPVOID lpParameter;
	} THREAD_PARAM, *PTHREAD_PARAM;

	HANDLE CreateWorkerThreadParam(
		LPVOID lpParameter,
		LPSECURITY_ATTRIBUTES lpsa = NULL,
		DWORD dwStackSize = 0,
		DWORD dwCreationFlags = 0,
		LPDWORD lpThreadId = NULL)
	{
		T* pThis = static_cast<T*>(this);
		PTHREAD_PARAM pThreadParam = reinterpret_cast<PTHREAD_PARAM>(
			::HeapAlloc(
				::GetProcessHeap(), 
				HEAP_ZERO_MEMORY, 
				sizeof(THREAD_PARAM)));
		if (NULL == pThreadParam)
		{
			return NULL;
		}
		pThreadParam->lpParameter = lpParameter;
		pThreadParam->pThis = pThis;
		return ::CreateThread(
			lpsa, dwStackSize, 
			T::WorkerThreadStartRoutineParam, 
			pThreadParam, 
			dwCreationFlags, lpThreadId);
	}
	static DWORD WINAPI WorkerThreadStartRoutineParam(LPVOID lpParameter)
	{
		PTHREAD_PARAM pThreadParam = static_cast<PTHREAD_PARAM>(lpParameter);
		LPVOID lpParam = pThreadParam->lpParameter;
		T* pThis = pThreadParam->pThis;
		BOOL fSuccess = ::HeapFree(::GetProcessHeap(), 0, lpParameter);
		ATLASSERT(fSuccess);
		return pThis->OnWorkerThreadStart(lpParam);
	}
};

//////////////////////////////////////////////////////////////////////////
//
// Add Write Key Dialog (in General Property Page)
//
//////////////////////////////////////////////////////////////////////////

class CDeviceAddWriteKeyDlg :
	public CDialogImpl<CDeviceAddWriteKeyDlg>
{
	CString m_strWriteKey;
	CString m_strDeviceName;
	CString m_strDeviceId;

	CButton m_butOK;

public:
	enum { IDD = IDD_ADD_WRITE_KEY };

	BEGIN_MSG_MAP_EX(CDeviceAddWriteKeyDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_DEVICE_WRITE_KEY,EN_CHANGE,OnWriteKeyChange)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
	END_MSG_MAP()

	void SetDeviceName(LPCTSTR szName);
	void SetDeviceId(LPCTSTR szDeviceId);
	LPCTSTR GetWriteKey();

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	void OnWriteKeyChange(UINT, int, HWND);
	void OnOK(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
	void OnCancel(UINT /*wNotifyCode*/, int /* wID */, HWND /*hWndCtl*/);
};

//////////////////////////////////////////////////////////////////////////
//
// Rename Dialog (in General Property Page)
//
//////////////////////////////////////////////////////////////////////////

class CDeviceRenameDialog :
	public CDialogImpl<CDeviceRenameDialog>
{

public:
	enum { IDD = IDD_RENAME };

	BEGIN_MSG_MAP_EX(CDeviceRenameDialog)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnClose)
		COMMAND_HANDLER_EX(IDC_DEVICE_NAME, EN_CHANGE, OnDeviceNameChange)
	END_MSG_MAP()

	CDeviceRenameDialog();

	void SetName(LPCTSTR szName);
	LPCTSTR GetName();

	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDeviceNameChange(UINT uCode, int nCtrlID, HWND hwndCtrl);

private:
	static const MAX_NAME_LEN = MAX_NDAS_DEVICE_NAME_LEN;
	TCHAR m_szName[MAX_NAME_LEN + 1];
	TCHAR m_szOldName[MAX_NAME_LEN + 1];
	CEdit m_wndName;
	CButton m_wndOK;
};

#define WM_USER_DONE (WM_USER + 0x4001)

//////////////////////////////////////////////////////////////////////////
//
// General Property Page
//
//////////////////////////////////////////////////////////////////////////

class CDeviceGeneralPage :
	public CPropertyPageImpl<CDeviceGeneralPage>,
	public CWinDataExchange<CDeviceGeneralPage>,
	public CWorkerThreadBase<CDeviceGeneralPage>
{
public:

	enum { IDD = IDD_DEVPROP_GENERAL };

	BEGIN_MSG_MAP_EX(CDeviceGeneralPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SETCURSOR(OnSetCursor)
		MESSAGE_HANDLER_EX(WM_USER_DONE, OnWorkDone)
		COMMAND_ID_HANDLER_EX(IDC_RENAME,OnRename)
		COMMAND_ID_HANDLER_EX(IDC_ADD_WRITE_KEY, OnModifyWriteKey)
		COMMAND_HANDLER_EX(IDC_UNITDEVICE_LIST,CBN_SELCHANGE,OnUnitDeviceSelChange)
		CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceGeneralPage>)
	END_MSG_MAP()

	CDeviceGeneralPage();

	void SetDevice(ndas::DevicePtr pDevice);

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnSetCursor(HWND hWnd, UINT hit, UINT msg);
	void OnRename(UINT, int, HWND);
	void OnModifyWriteKey(UINT, int, HWND);
	void OnUnitDeviceSelChange(UINT, int, HWND);
	LRESULT OnWorkDone(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// PropertyPage Notifications
	void OnReset();

	// WorkerThreadBase
	DWORD OnWorkerThreadStart(LPVOID lpParam);

private:

	ndas::DevicePtr m_pDevice;
	ndas::UnitDeviceVector m_pUnitDevices;

	TCHAR m_chConcealed;

	CEdit m_wndDeviceName;
	CEdit m_wndDeviceId;
	CEdit m_wndDeviceStatus;
	CEdit m_wndDeviceWriteKey;
	CEdit m_wndUnitDeviceStatus;
	CEdit m_wndUnitDeviceCapacity;
	CEdit m_wndUnitDeviceROHosts;
	CEdit m_wndUnitDeviceRWHosts;
	CButton m_wndAddRemoveWriteKey;
	CButton m_wndUnitDeviceGroup;
	CStatic m_wndUnitDeviceIcon;
	CStatic m_wndUnitDeviceType;
	CStatic m_wndNA;
	CButton m_wndLogDeviceNA;
	CTreeViewCtrlEx m_wndLogDeviceTree;
	CComboBox m_wndUnitDeviceList;

	HCURSOR m_hCursor;

	CImageList m_imageList;

	std::vector<HWND> m_unitDeviceControls;
	std::vector<bool> m_unitDeviceUpdated;

	int m_curUnitDevice;

	XTL::AutoObjectHandle m_hWorkerThread[2];

	void _ShowUnitDeviceControls(int nCmdShow);
	void _GrabUnitDeviceControls();

	void _InitData();
	void _UpdateDeviceData();
	void _UpdateUnitDeviceData(ndas::UnitDevicePtr pUnitDevice);
	void _UpdateLogDeviceData(ndas::UnitDevicePtr pUnitDevice);

	void _SetCurUnitDevice(int index);

	static BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
	{
		CDeviceGeneralPage* pThis = reinterpret_cast<CDeviceGeneralPage*>(lParam);
		return pThis->OnEnumChild(hWnd);
	}
	
	BOOL OnEnumChild(HWND hWnd);

	struct UpdateUnitDeviceData : std::unary_function<ndas::UnitDevicePtr, void> {
		UpdateUnitDeviceData(CDeviceGeneralPage* pInstance) : m_pInstance(pInstance) {}
		void operator()(const ndas::UnitDevicePtr& pUnitDevice) {
			m_pInstance->_UpdateUnitDeviceData(pUnitDevice);
		}
	private:
		CDeviceGeneralPage* m_pInstance;
	};

};

//////////////////////////////////////////////////////////////////////////
//
// Hardware Property Page
//
//////////////////////////////////////////////////////////////////////////

class CDeviceHardwarePage :
	public CPropertyPageImpl<CDeviceHardwarePage>
{
public:

	enum { IDD = IDD_DEVPROP_HW };

	BEGIN_MSG_MAP_EX(CDeviceHardwarePage)
		MSG_WM_INITDIALOG(OnInitDialog)
		REFLECT_NOTIFICATIONS()
	END_MSG_MAP()

	CDeviceHardwarePage();

	void SetDevice(ndas::DevicePtr pDevice);

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

private:

	CPropertyListCtrl m_propList;
	ndas::DevicePtr m_pDevice;

};

//////////////////////////////////////////////////////////////////////////
//
// HostStat Property Page
//
//////////////////////////////////////////////////////////////////////////

class CDeviceHostStatPage :
	public CPropertyPageImpl<CDeviceHostStatPage>,
	public CWorkerThreadBase<CDeviceHostStatPage>
{
public:
	
	enum { IDD = IDD_DEVPROP_HOSTINFO };

	BEGIN_MSG_MAP_EX(CDeviceHostStatPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SETCURSOR(OnSetCursor)
//		MSG_WM_TIMER(OnTimer)
		MESSAGE_HANDLER_EX(WM_USER_DONE, OnWorkDone)
		COMMAND_ID_HANDLER_EX(IDC_REFRESH_HOST, OnCmdRefresh)
		MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
		CHAIN_MSG_MAP(CPropertyPageImpl<CDeviceHostStatPage>)
	ALT_MSG_MAP(1)
	END_MSG_MAP()

	CDeviceHostStatPage();
	CDeviceHostStatPage(ndas::UnitDevicePtr pUnitDevice);

	void SetUnitDevice(ndas::UnitDevicePtr pUnitDevice);
	void BeginRefreshHosts();

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnSetCursor(HWND hWnd, UINT hit, UINT msg);
	LRESULT OnWorkDone(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnCmdRefresh(UINT, int, HWND);
	HBRUSH OnCtlColorStatic(HDC dcCtl, HWND wndCtl);

	// CPropertyPageImpl
	// Notifies a page that the property sheet is about to be destroyed.
	void OnReset();
	BOOL OnTranslateAccelerator(LPMSG /*lpMsg*/);

	// CWorkerThreadBase
	DWORD OnWorkerThreadStart();

private:

	struct HostInfoData
	{
		ACCESS_MASK Access;
		NDAS_HOST_INFO HostInfo;
	};

	HACCEL m_hAccel;
	HANDLE m_hWorkerThread;
	HCURSOR m_hCursor;
	ndas::UnitDevicePtr m_pUnitDevice;
	CSimpleArray<HostInfoData> m_hostInfoDataArray;
	CListViewCtrl m_wndListView;
	CAnimateCtrl m_findHostsAnimate;
	CBrush m_brushListViewBk;
	CHyperLink m_wndRefreshLink;
};

//////////////////////////////////////////////////////////////////////////
//
// Advanced Property Page
//
//////////////////////////////////////////////////////////////////////////

class CDeviceAdvancedPage :
	public CPropertyPageImpl<CDeviceAdvancedPage>
{
public:
	enum { IDD = IDD_DEVPROP_ADVANCED };

	BEGIN_MSG_MAP_EX(CDeviceAdvancedPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDC_DEACTIVATE_DEVICE, OnDeactivateDevice)
		COMMAND_ID_HANDLER_EX(IDC_RESET_DEVICE, OnResetDevice)
		COMMAND_ID_HANDLER_EX(IDC_EXPORT, OnCmdExport)
	END_MSG_MAP()

	CDeviceAdvancedPage();
	CDeviceAdvancedPage(ndas::DevicePtr pDevice);

	void SetDevice(ndas::DevicePtr pDevice);
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);

	void OnDeactivateDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
	void OnResetDevice(UINT uCode, int nCtrlID, HWND hwndCtrl);
	void OnCmdExport(UINT uCode, int nCtrlID, HWND hwndCtrl);
private:

	ndas::DevicePtr m_pDevice;

	CButton m_wndDeactivate;
	CButton m_wndReset;
};

//////////////////////////////////////////////////////////////////////////
//
// NDAS Device Property Sheet
//
//////////////////////////////////////////////////////////////////////////

typedef boost::shared_ptr<CDeviceHostStatPage> HostStatPagePtr;
typedef std::vector<HostStatPagePtr> HostStatPageVector;

class CDevicePropSheet :
	public CPropertySheetImpl<CDevicePropSheet>
{
public:

	BEGIN_MSG_MAP_EX(CDevicePropSheet)
		MSG_WM_SHOWWINDOW(OnShowWindow)
		CHAIN_MSG_MAP(CPropertySheetImpl<CDevicePropSheet>)
	END_MSG_MAP()

	CDevicePropSheet(
		_U_STRINGorID title = (LPCTSTR) NULL,
		UINT uStartPage = 0,
		HWND hWndParent = NULL);
	
	void SetDevice(ndas::DevicePtr pDevice);
	void OnShowWindow(BOOL bShow, UINT nStatus);

private:

	BOOL m_bCentered;
	CDeviceGeneralPage m_pspGeneral;
	CDeviceAdvancedPage m_pspAdvanced;
	CDeviceHardwarePage m_pspHardware;
	HostStatPageVector m_pspHostStats;
	ndas::DevicePtr m_pDevice;

};
