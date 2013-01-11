#include <windows.h>
#include <crtdbg.h>
#include "lock.hxx"

// InitSync is  used  as  a  synchronization  mechanism  to prevent
// multiple  threads  from  overlapping  execution  of  the  WSAStartup and
// WSACleanup procedures.

namespace // private namespace
{

enum DllInitSyncOperation
{
	SyncInitialize,
	SyncLock,
	SyncUnlock
};

void
DllInitSync(DllInitSyncOperation op)
{
	//
	// This function is a lock singleton holder
	//
	static CCritSecLock InitLock;
	static BOOL fInit = InitLock.Initialize();

	switch (op)
	{
	case SyncInitialize: 
		// static initializer has been called already, nothing further
		return; 
	case SyncLock: 
		InitLock.Lock(); 
		return;
	case SyncUnlock: 
		InitLock.Unlock(); 
		return;
	}
}

} // end of private namespace

void
DllCreateInitSync()
{
	DllInitSync(SyncInitialize);
}

void
DllDestroyInitSync()
{
}

void
DllEnterInitSync()
{
	DllInitSync(SyncLock);
}

void
DllLeaveInitSync()
{
	DllInitSync(SyncUnlock);
}

