////////////////////////////////////////////////////////////////////////////
//
// Interface of CMirrorDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NBSYNCDLG_H_
#define _NBSYNCDLG_H_

#include "resource.h"
#include "nbtextprogbar.h"
#include "ndasobserver.h"
#include "ndasobject.h"
#include "nbthread.h"

#define TIME_FORMAT "%H:%M:%S"

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
	BOOL	bRebound;			// Set to TRUE if binding information has been changed
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
	// Variables used for adding a mirror disk
	BOOL	m_bAdded;		// Set to be TRUE if modifying DISK_INFORMATION_BLOCK 
							// for the disks are done.
	// Variables used for reestablishing mirror
	BOOL	m_bRebound;		// Set to be TRUE if rebounding(see ReboundMirror)
							// is done

	virtual void Run();

	// Rebind mirror
	// (update DISK_INFORMATION_BLOCK_V2 and mark all the bitmap dirty)
	void RebindMirror();

	// Add mirror
	// DISK_INFORMATION_BLOCK_V2 will be written 
	// and all the bitmaps on the source disk will be marked dirty
	void AddMirror();

	void Notify(UINT nPhase, int nErrorCode = 0);
	void NotifyRebound();
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

//
// 
//
class CMirrorDlg :
	public CDialogImpl<CMirrorDlg>,
	public CWinDataExchange<CMirrorDlg>,
	public CMultithreadedObserver
{
protected:
	int			m_nWorkType;

	BOOL		m_bRebound;			
	BOOL		m_bFinished;
	BOOL		m_bRunning;
	UINT		m_nCurrentPhase;

	CTime		m_timeBegin;
	CTime		m_timePrev;
	UINT		m_nCurrentStep;
	double		m_fPrevMBPerSec;

	CTextProgressBarCtrl m_progBar;
	CWindow	m_btnOK;
	CWindow m_btnCancel;
	WTL::CString m_strSource;
	WTL::CString m_strDest;
	WTL::CString m_strPhase;

	CUnitDiskObjectPtr	m_pSource, m_pDest;
	CMirDiskObjectPtr	m_pMirDisks;

	CMirrorWorkThread m_workThread;

	// Start to synchronize
	void Start();
	// Stop synchronizing
	void Stop();
public:
	//
	// constructor
	//
	// @param nWorkType	[in] A type that specifies the work that the dialog will do.
	//
	CMirrorDlg(int nWorkType = NBSYNC_TYPE_SYNC_ONLY);
	enum { IDD = IDD_SYNC };

	BEGIN_DDX_MAP(CMirrorDlg)
		DDX_TEXT(IDC_TEXT_PHASE, m_strPhase)
		DDX_TEXT(IDC_TEXT_SOURCE, m_strSource)
		DDX_TEXT(IDC_TEXT_DEST, m_strDest)
	END_DDX_MAP()

	BEGIN_MSG_MAP(CMirrorDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER_EX(IDOK, OnOK)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
	void OnCancel(UINT wNotifyCode, int wID, HWND hwndCtl);
	void OnOK(UINT wNotifyCode, int wID, HWND hwndCtl);

	void PeekMessageLoop();
	void MsgWaitForNotification();
	void DispathNotification();

	//
	// Set mirror disks used for synchronization
	//
	void SetSyncDisks(CMirDiskObjectPtr mirDisks);
	//
	// Set mirror disks used for reestablishing mirror and synchronization
	//
	void SetReMirDisks(CMirDiskObjectPtr mirDisk, CUnitDiskObjectPtr newDisk);
	//
	// Set mirror disks used for adding new mirror and synchronization
	//
	void SetAddMirDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest);

};



#endif // _NBSYNCDLG_H_
