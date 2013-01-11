#pragma once

class CInterAppMessenger
{
	UINT m_AppMsgId;
public:
	CInterAppMessenger() : m_AppMsgId(0) {}
	BOOL Initialize(LPCTSTR ApplicationId);
	BOOL PostMessage(WPARAM wParam, LPARAM lParam);
};

class CSingletonApp
{
	HANDLE m_hMutex;
	BOOL m_fOtherInstance;
public:
	CSingletonApp() : m_hMutex(NULL) {}
	~CSingletonApp();
	BOOL Initialize(LPCTSTR SingletonId);
	BOOL IsFirstInstance();
	BOOL WaitOtherInstances(DWORD Timeout = INFINITE);
};

template <typename T>
class ATL_NO_VTABLE CInterAppMsgImpl : public CMessageMap
{
public:

	const UINT m_uAppMsgId;

	BEGIN_MSG_MAP_EX(CInterAppMsgImpl<T>)
		MESSAGE_HANDLER_EX(m_uAppMsgId, OnInterAppMsg)
	END_MSG_MAP()

	CInterAppMsgImpl(LPCTSTR ApplicationId) : 
		m_uAppMsgId(::RegisterWindowMessage(ApplicationId)) {}

	CInterAppMsgImpl(UINT MsgId) : m_uAppMsgId(MsgId) {}

	LRESULT OnInterAppMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		T* pThis = static_cast<T*>(this);
		pThis->OnInterAppMsg(wParam, lParam);
		return 0;
	}

	void OnInterAppMsg(WPARAM wParam, LPARAM lParam)
	{
		ATLTRACE("InterAppMessage: wParam=%d, lParam=%d\n", wParam, lParam);
	}
};

