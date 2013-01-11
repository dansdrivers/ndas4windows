#pragma once

class CNdasDevicePropHostStatPage :
	public CPropertyPageImpl<CNdasDevicePropHostStatPage>
{
public:
	
	enum { IDD = IDD_DEVPROP_HOSTINFO };

	enum { WM_THREADED_WORK_COMPLETED = WM_USER + 0x4001 };

	BEGIN_MSG_MAP_EX(CNdasDevicePropHostStatPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SETCURSOR(OnSetCursor)
		MESSAGE_HANDLER_EX(WM_THREADED_WORK_COMPLETED, OnThreadCompleted)
		COMMAND_ID_HANDLER_EX(IDC_REFRESH_HOST, OnCmdRefresh)
		MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
		CHAIN_MSG_MAP(CPropertyPageImpl<CNdasDevicePropHostStatPage>)
	ALT_MSG_MAP(1)
	END_MSG_MAP()

	CNdasDevicePropHostStatPage();
	CNdasDevicePropHostStatPage(ndas::UnitDevicePtr pUnitDevice);

	void SetUnitDevice(ndas::UnitDevicePtr pUnitDevice);
	void BeginRefreshHosts();

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnSetCursor(HWND hWnd, UINT hit, UINT msg);
	LRESULT OnThreadCompleted(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnCmdRefresh(UINT, int, HWND);
	HBRUSH OnCtlColorStatic(HDC dcCtl, HWND wndCtl);

	// CPropertyPageImpl
	// Notifies a page that the property sheet is about to be destroyed.
	void OnReset();

	BOOL OnTranslateAccelerator(LPMSG /*lpMsg*/);

private:

	struct HostInfoData
	{
		ACCESS_MASK Access;
		NDAS_HOST_INFO HostInfo;
	};

	HACCEL m_hAccel;
	HCURSOR m_hCursor;
	ndas::UnitDevicePtr m_pUnitDevice;
	CSimpleArray<HostInfoData> m_hostInfoDataArray;
	CListViewCtrl m_wndListView;
	CAnimateCtrl m_findHostsAnimate;
	CBrush m_brushListViewBk;
	CHyperLink m_wndRefreshLink;
	CHandle m_ThreadCompletionEvent;

	static DWORD WINAPI pHostInfoUpdateThreadStart(LPVOID Context);
	DWORD pHostInfoUpdateThreadStart();
};
