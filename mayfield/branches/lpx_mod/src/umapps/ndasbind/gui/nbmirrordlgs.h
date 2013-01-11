////////////////////////////////////////////////////////////////////////////
//
// Interface of CMirrorDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBSYNCDLG_H_
#define _NBSYNCDLG_H_

#include "resource.h"
#include "nbtextprogbar.h"
#include "ndasobserver.h"
#include "ndasobject.h"
#include "ndasmirrorthread.h"
//////////////////////////////////////////////////////////////////////////////
// Mirror Dialog class
//
//	- This class is used for the following three different operations.
//	  The desired operation can be selected by passing a parameter 
//	  to the constructor.
//		1) Synchronization : copy source disk's data to dest disk
//		2) Reestablishing mirror : add a mirror disk to a once mirrored disk.
//								   and synchronize the two disks.
//		3) Add mirror : Add a mirror disk to a disk and copy the disk's data
//						to the mirror disk.
//////////////////////////////////////////////////////////////////////////////
class CMirrorDlg :
	public CDialogImpl<CMirrorDlg>,
	public CWinDataExchange<CMirrorDlg>,
	public CMultithreadedObserver
{
protected:
	int			m_nWorkType;

	//
	// Variables that store progress information
	//
	BOOL		m_bBound;			
	BOOL		m_bFinished;
	BOOL		m_bRunning;
	UINT		m_nCurrentPhase;

	CMirrorWorkThread m_workThread;

	//
	// Disks used for sync, reestablishing, adding.
	//
	CUnitDiskObjectPtr	m_pSource, m_pDest;

	// Variables to display information about the progress
	time_t		m_timeBegin;
	time_t		m_timePrev;
	UINT		m_nCurrentStep;
	double		m_fPrevMBPerSec;

	CTextProgressBarCtrl m_progBar;
	CWindow	m_btnOK;
	CWindow m_btnCancel;
	WTL::CString m_strSource;
	WTL::CString m_strDest;
	WTL::CString m_strPhase;


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
	int IDD;
	CMirrorDlg(int nWorkType = NBSYNC_TYPE_SYNC_ONLY);
	

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
	void SetSyncDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest);
	//
	// Set mirror disks used for reestablishing mirror and synchronization
	//
	void SetReMirDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr newDisk);
	//
	// Set mirror disks used for adding new mirror and synchronization
	//
	void SetAddMirDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest);

	//
	// Return true if binding status has been changed.
	//
	BOOL IsBindingStatusChanged();
};



#endif // _NBSYNCDLG_H_
