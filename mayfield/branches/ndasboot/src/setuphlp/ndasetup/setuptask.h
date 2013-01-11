#pragma once
#include "msiproc.h"
#include "ndas/ndupdate.h"

struct ISetupUI;

class CSetupTask
{
	static DWORD WINAPI spThreadProc(LPVOID lpContext);

protected:

	ISetupUI* m_pSetupUI;
	DWORD	m_dwThreadID;
	HANDLE	m_hThread;
	BOOL	m_fCanceled;

	virtual DWORD OnTaskStart() = 0;
	BOOL HasCanceled();

public:

	CSetupTask(ISetupUI* pSetupUI);
	virtual ~CSetupTask();

	BOOL Start();
	BOOL Cancel();
	DWORD WaitForStop(DWORD dwWaitTimeout = INFINITE);

};

class CSetupInitalize :
	public CSetupTask
{
	PMSIAPI m_pMsiApi;
public:
	CSetupInitalize(ISetupUI* pSetupUI) : 
	  CSetupTask(pSetupUI) 
	{}

	DWORD OnTaskStart();
};

class CSetupCheckUpdate :
	public CSetupTask
{
	NDUPDATE_SYSTEM_INFO m_SysInfo;
	TCHAR m_szUpdateURL[MAX_PATH];

public:
	CSetupCheckUpdate(
		ISetupUI* pSetupUI,
		LPCTSTR szUpdateURL,
		CONST NDUPDATE_SYSTEM_INFO& SysInfo);

	DWORD OnTaskStart();
};

class CSetupUpgradeMsi :
	public CSetupTask
{
public:
	CSetupUpgradeMsi(ISetupUI* pSetupUI) : CSetupTask(pSetupUI) {}
	DWORD OnTaskStart();
};
