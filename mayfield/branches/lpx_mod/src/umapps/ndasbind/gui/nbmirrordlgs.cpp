////////////////////////////////////////////////////////////////////////////
//
// Implementation of CMirrorDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "nbmirrordlgs.h"
#include "ndasexception.h"

///////////////////////////////////////////////////////////////////////////////
// CMirrorDlg
///////////////////////////////////////////////////////////////////////////////
CMirrorDlg::CMirrorDlg(int nWorkType)
: m_bRunning(FALSE), m_bFinished(FALSE), m_nCurrentPhase(0),
  m_bBound(FALSE),
  m_fPrevMBPerSec(0), m_nWorkType(nWorkType), 
  m_workThread( CMirrorWorkThread(nWorkType) )
{
	switch( nWorkType )
	{
	case NBSYNC_TYPE_SYNC_ONLY:
		IDD = IDD_SYNC;
		break;
	case NBSYNC_TYPE_REMIRROR:
		IDD = IDD_REMIRROR_SYNC;
		break;
	case NBSYNC_TYPE_ADDMIRROR:
		IDD = IDD_ADDMIRROR_SYNC;
		break;
	default:
		IDD = IDD_SYNC;
		break;
	}
}

void CMirrorDlg::SetSyncDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest)
{
	m_pSource = source;
	m_pDest = dest;
}

void CMirrorDlg::SetReMirDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest)
{
	m_pSource = source;
	m_pDest = dest;
}

void CMirrorDlg::SetAddMirDisks(CUnitDiskObjectPtr source, CUnitDiskObjectPtr dest)
{
	m_pSource = source;
	m_pDest = dest;
}

LRESULT CMirrorDlg::OnInitDialog(HWND /*hWndCtl*/, LPARAM /*lParam*/)
{
	ATLASSERT( m_pSource.get() != NULL );
	ATLASSERT( m_pDest.get() != NULL );
	CenterWindow();
	m_progBar.SubclassWindow( GetDlgItem(IDC_PROGBAR) );
	m_btnOK.Attach( GetDlgItem(IDOK) );
	m_btnCancel.Attach( GetDlgItem(IDCANCEL) );

	DoDataExchange(TRUE);
	m_strSource = m_pSource->GetTitle();
	m_strDest = m_pDest->GetTitle();
	DoDataExchange(FALSE);

	return 0;
}

void CMirrorDlg::OnCancel(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	EndDialog(IDCANCEL);
}

void CMirrorDlg::OnOK(UINT /*wNotifyCode*/, int /*wID*/, HWND /*hwndCtl*/)
{
	if ( m_bFinished )
	{
		EndDialog(IDOK);
	}
	else
	{
		if ( m_bRunning )
		{
			Stop();
		}
		else
		{
			Start();
		}
	}
}

void CMirrorDlg::PeekMessageLoop()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
void CMirrorDlg::MsgWaitForNotification()
{
	DWORD dwRet;
	HANDLE hObject[2];
	hObject[0] = (HANDLE)m_workThread;
	hObject[1] = m_hSyncEvent;
	do
	{
		dwRet = ::MsgWaitForMultipleObjects(2, hObject, FALSE, 
			INFINITE, QS_ALLINPUT);
		if ( dwRet == WAIT_OBJECT_0 || dwRet == WAIT_FAILED )
		{
			DispathNotification();
			break;
		}
		else if ( dwRet == WAIT_OBJECT_0 + 1)
		{
			// Get notification
			DispathNotification();
		}
		else if ( dwRet )
		{
			PeekMessageLoop();
		}
	} while (TRUE);
}

void CMirrorDlg::Start()
{
	//
	// Before start to mirroring, check both source and dest is accessable
	//
	if ( !m_pSource->CanAccessExclusive(TRUE) )
	{
		WTL::CString strMsg;
		strMsg.FormatMessage( 
			IDS_MIRRORDLG_FAIL_TO_ACCESS_EXCLUSIVELY,
			m_pSource->GetTitle()
			);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	if ( !m_pDest->CanAccessExclusive(TRUE) )
	{
		WTL::CString strMsg;
		strMsg.FormatMessage(
			IDS_MIRRORDLG_FAIL_TO_ACCESS_EXCLUSIVELY,
			m_pSource->GetTitle()
			);
		WTL::CString strTitle;
		strTitle.LoadString(IDS_APPLICATION);
		MessageBox( 
			strMsg,
			strTitle,
			MB_OK | MB_ICONWARNING
			);
		return;
	}

	//
	// Launch working thread
	//
	WTL::CString strBtnFace;
	strBtnFace.LoadString( IDS_MIRRORDLG_BTN_STOP );
	m_btnOK.SetWindowText( strBtnFace );
	m_btnCancel.EnableWindow( FALSE );

	m_bRunning = TRUE;
	m_workThread.Attach( this );
	m_workThread.SetSource( m_pSource );
	m_workThread.SetDest( m_pDest );

	m_timeBegin = ::time( NULL );
	m_workThread.Execute();
	MsgWaitForNotification();
	
	m_bRunning = FALSE;
	if ( m_bFinished )
	{
		strBtnFace.LoadString( IDS_MIRRORDLG_BTN_OK );
		m_btnOK.SetWindowText( strBtnFace );
	}
	else
	{
		strBtnFace.LoadString( IDS_MIRRORDLG_BTN_RESUME );
		m_btnOK.SetWindowText( strBtnFace );
	}
	
	m_btnCancel.EnableWindow( TRUE );
	m_btnOK.EnableWindow( TRUE );
}

void CMirrorDlg::Stop()
{
	m_btnOK.EnableWindow( FALSE );
	m_strPhase.LoadString( IDS_MIRRORDLG_STOPPING );
	DoDataExchange( FALSE );
	m_workThread.Stop();
}

void CMirrorDlg::DispathNotification()
{
	NBSYNC_REPORT report;
	report.nSize = sizeof(NBSYNC_REPORT);
	CMultithreadedObserver::GerReport( static_cast<NDAS_SYNC_REPORT*>(&report) );

	if ( m_nCurrentPhase != report.nPhase )
	{
		switch( report.nPhase )
		{
		case NBSYNC_PHASE_CONNECT:
			m_strPhase.LoadString( IDS_MIRRORDLG_CONNECT );
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_REBIND:
			m_strPhase.LoadString( IDS_MIRRORDLG_REBIND );
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_BIND:
			m_strPhase.LoadString( IDS_MIRRORDLG_BIND );
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_RETRIVE_BITMAP:
			m_strPhase.LoadString( IDS_MIRRORDLG_RETRIEVE_BITMAP );
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_SYNCHRONIZE:
			m_strPhase.LoadString( IDS_MIRRORDLG_SYNCHRONIZE );
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_FINISHED:
			m_strPhase.LoadString( IDS_MIRRORDLG_FINISHED );
			DoDataExchange(FALSE);
			m_bFinished = TRUE;
			break;
		case NBSYNC_PHASE_FAILED:
		default:
			if ( report.nErrorCode == NBSYNC_ERRORCODE_STOPPED )
			{
				m_strPhase.LoadString( IDS_MIRRORDLG_STOPPED );
				DoDataExchange(FALSE);
			}
			else
			{
				switch( report.nErrorCode )
				{
				case NBSYNC_ERRORCODE_FAIL_TO_MARK_BITMAP:
					m_strPhase.FormatMessage( 
						IDS_MIRRORDLG_FAIL_TO_MARK_BITMAP,
						m_pSource->GetTitle() 
						);
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_ADDMIRROR:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_ADDMIRROR );
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_CONNECT:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_CONNECT );
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_READ_BITMAP:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_READ_BITMAP );
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_UPDATE_BITMAP );
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_COPY:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_COPY );
					break;
				case NBSYNC_ERRORCODE_FAIL_TO_CLEAR_DIRTYFLAG:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAIL_TO_CLEAR_DIRTYFLAG );
					break;
				default:
					m_strPhase.LoadString( IDS_MIRRORDLG_FAILED );
					break;
				}
				DoDataExchange(FALSE);
			}
			break;
		}

		if ( report.bBound && !m_bBound )
		{
			m_bBound = TRUE;
		}
		m_nCurrentPhase = report.nPhase;
	}

	// Display process
	if ( (m_nCurrentPhase == NBSYNC_PHASE_SYNCHRONIZE 
		  || m_nCurrentPhase == NBSYNC_PHASE_FINISHED
		  || m_nCurrentPhase == NBSYNC_PHASE_FAILED )
		&& report.nTotalSize != 0 )
	{
		// Step in progress bar
		UINT nNewStep = 
			static_cast<UINT>(
			   (report.nProcessedSize*m_progBar.GetRangeLimit(FALSE)) 
			 / report.nTotalSize
			);
		while ( m_progBar.GetPos() < nNewStep )
		{
			m_progBar.StepIt();
		}

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
			double fMBPerSecond = 
				(report.nProcessedDirtySize) / 2.0 / 1024
				/ timeElapsed;
			

			WTL::CString strMBPerSecond;
			strMBPerSecond.Format( _T("%.01f"),fMBPerSecond);
			::SetWindowText( GetDlgItem(IDC_TEXT_RATE), strMBPerSecond );

			WTL::CString strTimeLeft;
			timeLeft = 
				static_cast<time_t>(
					(report.nTotalDirtySize-report.nProcessedDirtySize) 
					/ ( fMBPerSecond * 2 * 1024 )
				);
			int nHour, nMin, nSec;
			nSec = timeLeft % 60;
			nMin = (timeLeft / 60) % 60;
			nHour = timeLeft / 60 / 60;
			strTimeLeft.Format( _T("%02d:%02d:%02d"), nHour, nMin, nSec );
			::SetWindowText( GetDlgItem(IDC_TEXT_LEFTTIME), strTimeLeft );
		}
	}
	return;
}

BOOL CMirrorDlg::IsBindingStatusChanged()
{
	return m_bBound;
}

