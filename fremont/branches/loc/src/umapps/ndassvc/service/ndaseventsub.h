#pragma once

class CNdasEventPreSubscriber
{
public:

	CNdasEventPreSubscriber(LPCTSTR PipeName, DWORD MaxInstances);

	bool Initialize();
	HANDLE GetWaitHandle();
	HANDLE GetPipeHandle();
	bool Disconnect();
	bool BeginWaitForConnection();
	bool EndWaitForConnection();
	bool ResetPipeHandle();
	HANDLE DetachPipeHandle();

protected:

	bool ResetOverlapped();

	const LPCTSTR PipeName;
	const DWORD MaxInstances;

	XTL::AutoFileHandle m_hPipe;
	XTL::AutoObjectHandle m_hEvent;
	OVERLAPPED m_overlapped;
};

class CNdasEventSubscriber
{
public:
	CNdasEventSubscriber(HANDLE hConnectedPipe); 
	bool Initialize();
	HANDLE GetWaitHandle();
	HANDLE GetPipeHandle();
	bool Disconnect();
	bool BeginWriteMessage(const NDAS_EVENT_MESSAGE& msg);
	bool EndWriteMessage();
protected:
	bool ResetOverlapped();
	XTL::AutoFileHandle m_hPipe;
	XTL::AutoObjectHandle m_hEvent;
	OVERLAPPED m_overlapped;
};
