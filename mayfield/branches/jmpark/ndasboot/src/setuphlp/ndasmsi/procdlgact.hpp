#pragma once
#include "threadbase.hpp"

class CProcessDialogActivator :
	public CThreadBase<CProcessDialogActivator>
{
	HANDLE m_hStopEvent;
	DWORD m_dwInterval;
public:
	CProcessDialogActivator();
	~CProcessDialogActivator();
	DWORD ThreadMain();
	BOOL Start(DWORD dwInterval);
	BOOL Stop(BOOL fWait = TRUE);
	DWORD Wait(DWORD dwTimeout = INFINITE);
	void ActivateProcessWindow();
};

