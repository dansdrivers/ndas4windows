////////////////////////////////////////////////////////////////////////////
//
// Common interface class for working thread
//
////////////////////////////////////////////////////////////////////////////

#pragma once

class CWorkThread
{
private:
	BOOL m_bStopped;
	CRITICAL_SECTION	m_csSync;
	HANDLE				m_hThread;
protected:
	//
	// Run is called after the thread is created. 
	// Subclass should place its main function in this method.
	//
	virtual void Run() = 0;
	// Returns true if stop request has been sent to this working thread
	BOOL IsStopped();
	
	static DWORD WINAPI ThreadStart(LPVOID pParam);
public:
	CWorkThread(void);
	~CWorkThread(void);

	operator HANDLE() const;
	// 
	// Create thread and run
	//
	void Execute();
	//
	// Send stop message to the working thread
	//
	void Stop();

};
