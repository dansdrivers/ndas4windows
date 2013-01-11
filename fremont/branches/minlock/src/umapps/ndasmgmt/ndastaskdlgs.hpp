#pragma once

template <typename T>
class CMountTaskBase : public WTLEX::CTaskDialogExImpl<T>
{
public:

	HRESULT m_hr;
	HANDLE m_threadCompleted;

	enum { Initialized, Executing, Completed } m_state;

	CMountTaskBase() : m_hr(S_OK)
	{
		ATLVERIFY( m_threadCompleted = CreateEvent(NULL, TRUE, TRUE, NULL) );
	}

	~CMountTaskBase()
	{
		ATLVERIFY( CloseHandle(m_threadCompleted) );
	}

	HRESULT GetTaskResult()
	{
		return m_hr;
	}

	static DWORD WINAPI ThreadStart(LPVOID Parameter)
	{
		CMountTaskBase<T>* pBase = static_cast<T*>(Parameter);
		T* pThis = static_cast<T*>(Parameter);
		DWORD ret = pThis->ThreadStart();
		ATLVERIFY( SetEvent(pBase->m_threadCompleted) );
		return ret;
	}
};

class CMountTaskDialog : public CMountTaskBase<CMountTaskDialog>
{
	ndas::LogicalDevicePtr m_pLogicalUnit;
	BOOL m_accessMode;
	CString m_LogicalUnitName;

public:

	CMountTaskDialog(ndas::LogicalDevicePtr LogicalUnit, BOOL WriteMode);
	void OnCreated();
	void OnDestroyed();
	BOOL OnButtonClicked(int nButton);
	BOOL OnTimer(DWORD dwTickCount);

	DWORD ThreadStart();
};

class CDismountTaskDialog : public CMountTaskBase<CDismountTaskDialog >
{
	ndas::LogicalDevicePtr m_pLogicalUnit;
	CString m_LogicalUnitName;

	NDAS_LOGICALDEVICE_EJECT_PARAM m_EjectParam;

public:

	CDismountTaskDialog(ndas::LogicalDevicePtr LogicalUnit);

	void OnCreated();

	void OnDestroyed();
	BOOL OnButtonClicked(int nButton);
	BOOL OnTimer(DWORD TickCount);

	DWORD ThreadStart();
};
