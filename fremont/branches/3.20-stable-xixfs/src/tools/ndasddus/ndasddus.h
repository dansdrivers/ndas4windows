#pragma once
#include "svchelp.h"
#include "pnpevent.h"

#define NDASDEVU_DISPLAY_NAME _T("NDAS Device Driver Update Service")
#define NDASDEVU_SERVICE_NAME _T("ndasddus")

class CNdasDDUServiceInstaller :
	public CServiceInstallerT<CNdasDDUServiceInstaller>
{
public:
	static BOOL _PostInstall(SC_HANDLE hSCService);
};

class CNdasDDUService : 
	public CService,
	public CDeviceEventHandlerT<CNdasDDUService>
{
	HDEVNOTIFY m_hDevNotify;
	HANDLE m_hTaskThread;
	HANDLE m_hStopTask;

	static unsigned int __stdcall _ServiceThreadProc(void* pArg);
	VOID StartTask();

	DWORD OnTaskStart();

public:

	CNdasDDUService();
	~CNdasDDUService();

	virtual VOID ServiceMain(DWORD dwArgc, LPTSTR* lpArgs);
	virtual VOID ServiceDebug(DWORD dwArgc, LPTSTR* lpArgs);

	virtual DWORD OnServiceShutdown();
	virtual DWORD OnServiceStop();

	virtual DWORD OnServiceDeviceEvent(DWORD dwEventType, LPVOID lpEventData);

	void OnDevNodesChanged();
};
