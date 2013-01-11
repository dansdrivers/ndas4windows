#pragma once

class CNdasDevicePropGeneralPage :
	public CPropertyPageImpl<CNdasDevicePropGeneralPage>,
	public CWinDataExchange<CNdasDevicePropGeneralPage>
{
public:

	enum { IDD = IDD_DEVPROP_GENERAL };

	enum { WM_THREADED_WORK_COMPLETED = WM_USER + 0x4001 };

	BEGIN_MSG_MAP_EX(CNdasDevicePropGeneralPage)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_SETCURSOR(OnSetCursor)
		MESSAGE_HANDLER_EX(WM_THREADED_WORK_COMPLETED, OnUpdateThreadCompleted)
		COMMAND_ID_HANDLER_EX(IDC_REFRESH_HOST, OnCmdRefresh)
		COMMAND_ID_HANDLER_EX(IDC_RENAME,OnRename)
		COMMAND_ID_HANDLER_EX(IDC_ADD_WRITE_KEY, OnModifyWriteKey)
		COMMAND_HANDLER_EX(IDC_UNITDEVICE_LIST,CBN_SELCHANGE,OnUnitDeviceSelChange)
		CHAIN_MSG_MAP(CPropertyPageImpl<CNdasDevicePropGeneralPage>)
	END_MSG_MAP()

	CNdasDevicePropGeneralPage();

	void SetDevice(ndas::DevicePtr pDevice);

	// Message Handlers
	LRESULT OnInitDialog(HWND hwndFocus, LPARAM lParam);
	LRESULT OnSetCursor(HWND hWnd, UINT hit, UINT msg);
	void OnCmdRefresh(UINT, int, HWND);
	void OnRename(UINT, int, HWND);
	void OnModifyWriteKey(UINT, int, HWND);
	void OnUnitDeviceSelChange(UINT, int, HWND);
	LRESULT OnUpdateThreadCompleted(UINT uMsg, WPARAM wParam, LPARAM lParam);

	BOOL OnTranslateAccelerator(LPMSG /*lpMsg*/);

	// PropertyPage Notifications
	void OnReset();

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
	HACCEL m_hAccel;

	CImageList m_imageList;

	std::vector<HWND> m_unitDeviceControls;
	std::vector<bool> m_unitDeviceUpdated;

	int m_curUnitDevice;

	XTL::AutoObjectHandle m_ThreadCompleted;
	LONG m_ThreadCount;

	void _ShowUnitDeviceControls(int nCmdShow);
	void _GrabUnitDeviceControls();

	void _InitData();
	void _UpdateDeviceData();
	void _UpdateUnitDeviceData(ndas::UnitDevicePtr pUnitDevice);
	void _UpdateLogDeviceData(ndas::UnitDevicePtr pUnitDevice);

	void _SetCurUnitDevice(int index);

	static BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
	{
		CNdasDevicePropGeneralPage* pThis = reinterpret_cast<CNdasDevicePropGeneralPage*>(lParam);
		return pThis->OnEnumChild(hWnd);
	}
	
	BOOL OnEnumChild(HWND hWnd);

	struct UpdateUnitDeviceData : std::unary_function<ndas::UnitDevicePtr, void> 
	{
		UpdateUnitDeviceData(CNdasDevicePropGeneralPage* pInstance) : m_pInstance(pInstance) {}
		void operator()(const ndas::UnitDevicePtr& pUnitDevice) 
		{
			m_pInstance->_UpdateUnitDeviceData(pUnitDevice);
		}
	private:
		CNdasDevicePropGeneralPage* m_pInstance;
	};

	struct UpdateThreadParam {
		CNdasDevicePropGeneralPage* Instance;
		DWORD UnitIndex;
	};

	static DWORD WINAPI spUpdateThreadStart(LPVOID Context);
	DWORD pUpdateThreadStart(DWORD UnitIndex);
	void pNotifyThreadCompleted();
};
