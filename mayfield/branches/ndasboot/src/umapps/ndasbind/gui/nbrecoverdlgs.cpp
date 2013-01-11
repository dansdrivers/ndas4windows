////////////////////////////////////////////////////////////////////////////
//
// Implementation of CRecoverDlg class 
// 
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "nbrecoverdlgs.h"
#include "ndasexception.h"
#include "ndas/ndasop.h"
#include "apperrdlg.h"


///////////////////////////////////////////////////////////////////////////////
// CRecoverDlg
///////////////////////////////////////////////////////////////////////////////
CRecoverDlg::CRecoverDlg(BOOL bForceStart, UINT id_task, UINT id_caption)
: m_bRunning(FALSE), m_bFinished(FALSE), m_nCurrentPhase(0),
  m_fPrevMBPerSec(0), m_nBytesPerBit(128 * 512), m_bStopRequest(FALSE), 
  m_bForceStart(bForceStart), m_id_bind_type(id_task), m_id_caption(id_caption)
{
	IDD = IDD_RECOVER;
}

void CRecoverDlg::SetMemberDevice(CUnitDiskObjectPtr device)
{
	m_pDevice = device;
}

LRESULT CRecoverDlg::OnInitDialog(HWND /*hWndCtl*/, LPARAM /*lParam*/)
{
	ATLASSERT( m_pDevice.get() != NULL );
	CenterWindow();
	m_progBar.SubclassWindow( GetDlgItem(IDC_PROGBAR) );
	m_btnOK.Attach( GetDlgItem(IDOK) );
	m_btnCancel.Attach( GetDlgItem(IDCANCEL) );

	WTL :: CString strBtnFace;
	strBtnFace.LoadString( IDS_RECOVER_BTN_CLOSE );
	m_btnCancel.SetWindowText( strBtnFace );

	DoDataExchange(TRUE);
	m_strBindType.LoadString(m_id_bind_type);
	m_strDevice = m_pDevice->GetTitle();
	DoDataExchange(FALSE);

	WTL::CString strCaption;
	strCaption.LoadString(m_id_caption);
	SetWindowText(strCaption);

	SetPhaseText(IDS_RECOVERDLG_PHASE_READY);

	if(m_bForceStart)
		Start();

	return 0;
}

void CRecoverDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	if(m_bRunning)
		m_bStopRequest = TRUE;
	else
		EndDialog(IDOK);
}

void CRecoverDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	if ( m_bFinished )
	{
		EndDialog(IDOK);
	}
	else
	{
		if ( m_bRunning )
		{
//			Stop();
		}
		else
		{
			Start();
		}
	}
}

void CRecoverDlg::SetPhaseText(
	UINT ID)
{
	m_strPhase.LoadString(ID);
	DoDataExchange(FALSE);
}

BOOL CRecoverDlg::CallBackRecover(
	DWORD dwStatus,
	UINT32 Total,
	UINT32 Current)

{
	WTL::CString strBtnFace;

	switch(dwStatus)
	{
	case NDAS_RECOVER_STATUS_INITIALIZE:
		// activate stop button
		m_nBytesPerBit = Current;
		m_timeBegin = ::time( NULL );
		m_progBar.OnSetRange32(0, 0, Total);
		m_progBar.OnSetPos(0, 0, NULL);
		m_btnCancel.EnableWindow(TRUE);
		SetPhaseText(IDS_RECOVERDLG_PHASE_SYNC);
		break;
	case NDAS_RECOVER_STATUS_RUNNING:
		RefreshProgBar(Total, Current);
		// refresh time remaining, progress
		break;
	case NDAS_RECOVER_STATUS_FINALIZE:
		//		m_bFinished = TRUE;
		break;
	case NDAS_RECOVER_STATUS_FAILED:
		m_bRunning = FALSE;
		m_bFinished = FALSE;

		m_btnOK.EnableWindow(TRUE);
		strBtnFace.LoadString( IDS_RECOVER_BTN_CLOSE );
		m_btnCancel.SetWindowText(strBtnFace);
		m_btnCancel.EnableWindow(TRUE);
		SetPhaseText(IDS_RECOVERDLG_PHASE_READY);
		break;
	case NDAS_RECOVER_STATUS_COMPLETE:
		m_bRunning = FALSE;
		m_bFinished = TRUE;

		m_btnOK.EnableWindow(FALSE);
		strBtnFace.LoadString( IDS_RECOVER_BTN_CLOSE );
		m_btnCancel.SetWindowText(strBtnFace);

		SetPhaseText(IDS_RECOVERDLG_PHASE_DONE);
		break;
	}

	if(m_bStopRequest)
	{
		m_bStopRequest = FALSE;
		return FALSE;
	}

	return TRUE;
}

BOOL WINAPI CallBackRecover(
	DWORD dwStatus,
	UINT32 Total,
	UINT32 Current,
	LPVOID lpParameter)
{
	CRecoverDlg *pDlgRecover = (CRecoverDlg *)lpParameter;

	if(!pDlgRecover)
		return FALSE;

	return pDlgRecover->CallBackRecover(dwStatus, Total, Current);
}

DWORD WINAPI ThreadRecover( LPVOID lpParam ) 
{ 
	CRecoverDlg *pDlgRecover = (CRecoverDlg *)lpParam;
	BOOL bResults;

	NDAS_CONNECTION_INFO ConnectionInfo;
	ConnectionInfo.bWriteAccess = TRUE;
	ConnectionInfo.type = NDAS_CONNECTION_INFO_TYPE_MAC_ADDRESS;
	ConnectionInfo.UnitNo = pDlgRecover->m_pDevice->GetLocation()->GetUnitDiskLocation()->UnitNumber;;
	ConnectionInfo.protocol = IPPROTO_LPXTCP;
	ConnectionInfo.ui64OEMCode = 0;
	CopyMemory(ConnectionInfo.MacAddress, 
		pDlgRecover->m_pDevice->GetLocation()->GetUnitDiskLocation()->MACAddr, sizeof(ConnectionInfo.MacAddress));
	
	bResults = NdasOpRecover(&ConnectionInfo, CallBackRecover, lpParam);

	if(!bResults)
	{
		ShowErrorMessageBox(IDS_RECOVERDLG_FAIL);
	}

	ExitThread(0);
	return 0; 
} 

void CRecoverDlg::Start()
{
	WTL::CString strBtnFace;

	//
	// Launch working thread
	//
	m_btnOK.EnableWindow(FALSE);

	strBtnFace.LoadString( IDS_RECOVER_BTN_CANCEL );
	m_btnCancel.SetWindowText( strBtnFace );
	m_btnCancel.EnableWindow(FALSE);

	m_bRunning = TRUE;

	HANDLE hThread;
	hThread = CreateThread(
		NULL,
		0,
		ThreadRecover,
		this,
		NULL, // run immediately
		NULL);

	if(!hThread)
	{
		WTL::CString strMsg = _T("");
		ShowErrorMessageBox(strMsg);

		strBtnFace.LoadString( IDS_RECOVER_BTN_CLOSE );
		m_btnCancel.SetWindowText( strBtnFace );
		m_bRunning = FALSE;
		m_bFinished = TRUE;
	}
}

void CRecoverDlg::RefreshProgBar(UINT32 nTotalSize, UINT32 nCurrentStep)
{
	// Step in progress bar
	UINT nNewStep = nCurrentStep;

	m_progBar.OnSetPos(0, nCurrentStep, NULL);

	nCurrentStep = (!nCurrentStep) ? 1 : nCurrentStep;

	// Display time left
	time_t timeNow = ::time( NULL );
	time_t timeElapsed = timeNow - m_timeBegin;
	time_t timeLeft;
	if ( timeElapsed != 0 
		&& m_timePrev != timeNow	// To prevent too frequent update
		)
	{
		m_timePrev = timeNow;
		// Calculate transfer rate
		double fMBPerSecond = ((double)(nCurrentStep * m_nBytesPerBit)) / (1024 * 1024) / timeElapsed;


		WTL::CString strMBPerSecond;
		strMBPerSecond.Format( _T("%.01f"),fMBPerSecond);
		::SetWindowText( GetDlgItem(IDC_TEXT_RATE), strMBPerSecond );

		WTL::CString strTimeLeft;
		timeLeft = (nTotalSize - nCurrentStep) * (timeElapsed) / (nCurrentStep);

		int nHour, nMin, nSec;
		nSec = timeLeft % 60;
		nMin = (timeLeft / 60) % 60;
		nHour = timeLeft / 60 / 60;
		strTimeLeft.Format( _T("%02d:%02d:%02d"), nHour, nMin, nSec );
		::SetWindowText( GetDlgItem(IDC_TEXT_LEFTTIME), strTimeLeft );
	}
}
