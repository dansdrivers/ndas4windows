////////////////////////////////////////////////////////////////////////////
//
// Interface of CRecoverDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBRECOVERCDLG_H_
#define _NBRECOVERCDLG_H_

#include "resource.h"
#include "nbtextprogbar.h"
#include "ndasobserver.h"
#include "ndasobject.h"
//////////////////////////////////////////////////////////////////////////////
//
// Recover Dialog class
//
//////////////////////////////////////////////////////////////////////////////
class CRecoverDlg :
	public CDialogImpl<CRecoverDlg>,
	public CWinDataExchange<CRecoverDlg>
{
public:
	//
	// Variables that store progress information
	//
	BOOL		m_bFinished;
	BOOL		m_bRunning;
	BOOL		m_bStopRequest;
	UINT		m_nCurrentPhase;
	UINT32		m_nBytesPerBit;
	BOOL		m_bForceStart;


	//
	// Disks used for sync, reestablishing, adding.
	//
	CUnitDiskObjectPtr	m_pDevice;

	// Variables to display information about the progress
	time_t		m_timeBegin;
	time_t		m_timePrev;
	UINT		m_nCurrentStep;
	double		m_fPrevMBPerSec;

	CTextProgressBarCtrl m_progBar;
	CWindow	m_btnOK;
	CWindow m_btnCancel;
	WTL::CString m_strBindType;
	WTL::CString m_strDevice;
	WTL::CString m_strPhase;

	UINT m_id_bind_type, m_id_caption;


	void Start();
public:
	//
	// constructor
	//
	int IDD;
	CRecoverDlg(BOOL bForceStart, UINT id_bind_type, UINT id_caption);
	

	BEGIN_DDX_MAP(CRecoverDlg)
		DDX_TEXT(IDC_TEXT_PHASE, m_strPhase)
		DDX_TEXT(IDC_TEXT_DEST, m_strDevice)
		DDX_TEXT(IDC_TEXT_BIND_TYPE, m_strBindType)
	END_DDX_MAP()

	BEGIN_MSG_MAP(CRecoverDlg)
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

	BOOL CallBackRecover(DWORD dwStatus, UINT32 Total, UINT32 Current);
	void SetPhaseText(UINT ID);

	void SetMemberDevice(CUnitDiskObjectPtr device);
	void RefreshProgBar(UINT32 nTotalSize, UINT32 nCurrentStep);
};



#endif // _NBRECOVERCDLG_H_
