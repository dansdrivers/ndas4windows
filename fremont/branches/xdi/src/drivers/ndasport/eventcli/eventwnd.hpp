#pragma once
#include <ndas/ndasportioctl.h>
#include <ndas/ndasdiskioctl.h>

#ifdef MSG_WM_DEVICECHANGE
#undef MSG_WM_DEVICECHANGE
#endif
// atlcrack.h cast lParam to DWORD which truncates a pointer in AMD64
#define MSG_WM_DEVICECHANGE(func) \
	if (uMsg == WM_DEVICECHANGE) \
	{ \
		SetMsgHandled(TRUE); \
		lResult = (LRESULT)func((UINT)wParam, (LPVOID)lParam); \
		if(IsMsgHandled()) \
			return TRUE; \
	}

typedef CWinTraits<
	WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
	WS_EX_TOOLWINDOW> CMainFrameWinTraits;

class CPnpEventConsumerWindow : 
	public CFrameWindowImpl<CPnpEventConsumerWindow>
{
public:
	BEGIN_MSG_MAP_EX(CPnpEventConsumerWindow)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_DEVICECHANGE(OnDeviceChange)
	END_MSG_MAP()

	LRESULT OnCreate(LPCREATESTRUCT lpcs);
	void OnDestroy();
	BOOL OnDeviceChange(UINT EventType, LPVOID EventData);
	BOOL OnDeviceChange(UINT EventType, PDEV_BROADCAST_DEVICEINTERFACE Dbcc);
	BOOL OnDeviceChange(UINT EventType, PDEV_BROADCAST_HANDLE Dbch);
	BOOL OnDeviceChange(UINT EventType, PDEV_BROADCAST_OEM Dbco);
	BOOL OnDeviceChange(UINT EventType, PDEV_BROADCAST_PORT Dbcp);
	BOOL OnDeviceChange(UINT EventType, PDEV_BROADCAST_VOLUME Dbcv);
	BOOL OnDeviceChange(UINT EventType, _DEV_BROADCAST_USERDEFINED* Dbcu);

	void ReportDeviceChange(PDEV_BROADCAST_HDR DevBroadcast);

	void OnNdasPortEvent(PNDASPORT_PNP_NOTIFICATION NdasPortNotification);
	void OnNdasAtaLinkEvent(PNDAS_ATA_LINK_EVENT NdasAtaLinkEvent);

	HANDLE m_ndasPortHandle;
	CSimpleArray<HDEVNOTIFY> m_DevNotifyHandles;
};
