///////////////////////////////////////////////////////////////////////////////
//
// Interface of CWorkThread which is used for background job
//
///////////////////////////////////////////////////////////////////////////////

#ifndef _NDASMIRRORTHREAD_H_
#define _NDASMIRRORTHREAD_H_

#include "ndasobject.h"
#include "ndasobserver.h"

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
	HANDLE GetHandle() const;
	// 
	// Create thread and run
	//
	void Execute();
	//
	// Send stop message to the working thread
	//
	void Stop();

};

typedef enum _NBSYNC_PHASE
{
	NBSYNC_PHASE_CONNECT = 1,		// Connect to disks
	NBSYNC_PHASE_REBIND,			// Reestablish mirror
	NBSYNC_PHASE_BIND,				// Add mirror
	NBSYNC_PHASE_RETRIVE_BITMAP,	// Retrieve bitmap from source disk
	NBSYNC_PHASE_SYNCHRONIZE,		// Copying dirty blocks
	NBSYNC_PHASE_FINISHED,			
	NBSYNC_PHASE_FAILED
};
typedef enum _NBSYNC_ERRORCODE
{
	NBSYNC_ERRORCODE_STOPPED = -1000,
	NBSYNC_ERRORCODE_FAIL_TO_MARK_BITMAP,
	NBSYNC_ERRORCODE_FAIL_TO_ADDMIRROR,
	NBSYNC_ERRORCODE_FAIL_TO_CONNECT,
	NBSYNC_ERRORCODE_FAIL_TO_READ_BITMAP,
	NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP,
	NBSYNC_ERRORCODE_FAIL_TO_COPY,
	NBSYNC_ERRORCODE_FAIL_TO_CLEAR_DIRTYFLAG,
	NBSYNC_ERRORCODE_UNSPECIFIED
};

typedef enum _NBSYNC_WORKTYPE
{
	NBSYNC_TYPE_SYNC_ONLY = 1,		// Data-synchronization only
	NBSYNC_TYPE_REMIRROR,			// Rebind a new disk to a previously mirrored disk
	NBSYNC_TYPE_ADDMIRROR			// Add a new disk as a mirror disk to a single disk
};

struct NBSYNC_REPORT : public NDAS_SYNC_REPORT
{
	UINT	nPhase;
	int		nErrorCode;
	BOOL	bBound;				// Set to TRUE if binding information has been changed
	_int64	nTotalSize;
	_int64  nTotalDirtySize;
	_int64	nProcessedSize;
	_int64  nProcessedDirtySize;
};

class CMirrorWorkThread : 
	public CSubject,
	public CWorkThread
{
protected:
	BOOL	m_bRemirror;	// If true, the destination disk will be bound
							// to the source disk as a mirror disk and
							// be synchronized.
							// if false, it will just be synchronized.

	NBSYNC_REPORT		m_report;
	CUnitDiskObjectPtr	m_pSource, m_pDest;

	int		m_nWorkType;
	BOOL	m_bBound;		// Set to be TRUE if binding is done

	virtual void Run();

	// Rebind mirror
	// (update NDAS_DIB_V2 and mark all the bitmap dirty)
	// @return	TRUE if the process has finished successfully
	BOOL RebindMirror();

	// Add mirror
	// NDAS_DIB_V2 will be written 
	// and all the bitmaps on the source disk will be marked dirty
	// @return	TRUE if the process has finished successfully
	BOOL AddMirror();

	// Synchronize mirror
	// @return	TRUE if the process has finished successfully
	BOOL SyncMirror();

	// Merge bitmap
	// If both disks are dirty, we need to revert modified data in
	// the dest disk. Thus, the source disk's bitmap will be updated
	// here to include the dest disk's bitmap, and the merged bitmap
	// will be used for synchronization.
	BOOL MergeBitmap();

	void Notify(UINT nPhase, int nErrorCode = 0);
	void NotifyBound();
	// Notify total amount of work to observer
	void NotifyTotalSize(_int64 nTotalSize, _int64 nTotalDirtySize);
	// Notify progress to observer
	void NotifyProgress(_int64 nProcessedSize);
	void NotifyProgressDirty(_int64 nProcessedDirtySize);
public:
	CMirrorWorkThread(int nWorkType = NBSYNC_TYPE_SYNC_ONLY);
	// method for CSubject
	virtual const NDAS_SYNC_REPORT *GetReport();
	
	// methods for initialization
	//void SetSourceLocation(const UNIT_DISK_LOCATION *pSourceLocation);
	//void SetDestLocation(const UNIT_DISK_LOCATION *pDestLocation);
	void SetSource(CUnitDiskObjectPtr source);
	void SetDest(CUnitDiskObjectPtr dest);
};

#endif // _NDASMIRRORTHREAD_H_