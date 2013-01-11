////////////////////////////////////////////////////////////////////////////
//
// Implementation of CMirrorDlg class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "nbmirrordlgs.h"
#include "nbdefine.h"
#include "ndasexception.h"
///////////////////////////////////////////////////////////////////////////////
// CMirrorWorkThread
///////////////////////////////////////////////////////////////////////////////
CMirrorWorkThread::CMirrorWorkThread(int nWorkType)
: m_nWorkType(nWorkType), m_bRebound(FALSE), m_bAdded(FALSE)
{
}

void CMirrorWorkThread::Run()
{
	::ZeroMemory( &m_report, sizeof(NBSYNC_REPORT) );
	m_report.nSize = sizeof(NBSYNC_REPORT);
	if ( m_nWorkType == NBSYNC_TYPE_REMIRROR && !m_bRebound )
	{
		try{
			Notify( NBSYNC_PHASE_REBIND );
			RebindMirror();
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			// TODO : We need more detail... the reason.
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_MARK_BITMAP );
			return;
		}
	}

	if ( m_nWorkType == NBSYNC_TYPE_ADDMIRROR && !m_bAdded )
	{
		try {
			Notify( NBSYNC_PHASE_BIND );
			AddMirror();
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			// TODO : We need more detail... the reason.
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_ADDMIRROR );
			return;
		}
	}

	CSession sSource, sDest;
	CBitmapSector bitmapSector;
	const UNIT_DISK_LOCATION *pSourceLocation, *pDestLocation;

	pSourceLocation = m_pSource->GetLocation()->GetUnitDiskLocation();
	pDestLocation	= m_pDest->GetLocation()->GetUnitDiskLocation();

	Notify( NBSYNC_PHASE_CONNECT );
	try{
		sSource.Connect( pSourceLocation->MACAddr );
		sDest.Connect( pDestLocation->MACAddr );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_CONNECT );
		return;
	}

	try{
		sSource.Login( pSourceLocation->UnitNumber, TRUE );
		sDest.Login( pSourceLocation->UnitNumber, TRUE );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_CONNECT );
		return;
	}

	Notify( NBSYNC_PHASE_RETRIVE_BITMAP );
	//
	// Get bitmap from the disk
	//
	try{
		bitmapSector.ReadAccept( &sSource );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_READ_BITMAP );
		return;
	}

	Notify( NBSYNC_PHASE_SYNCHRONIZE );

	//
	// Copying blocks
	//
	CHDDDiskInfoHandler *pHandler;

	pHandler = dynamic_cast<CHDDDiskInfoHandler*>(m_pSource->GetInfoHandler().get());
	ATLASSERT( pHandler != NULL );

	int			i, j, k;
	_int8		bitFlag;
	_int16		nBitmapSecCount;
	_int32		nSectorPerBit, nSectorPerByte;
	_int8		*pbBitmap;
	_int64		nTotalDirtySize;
	_int64		nProcessedDirtySize;
	CDataSector dataSector, bitmapUpdateSector;

	pbBitmap = bitmapSector.GetData();
	nSectorPerBit = pHandler->GetSectorsPerBit();
	nSectorPerByte = nSectorPerBit*8;
	nBitmapSecCount = 
		static_cast<_int16>(
			pHandler->GetUserSectorCount() / (nSectorPerBit*BLOCK_SIZE*8)
			);
	ATLASSERT( nBitmapSecCount <= bitmapSector.GetCount() );
	dataSector.Resize(nSectorPerBit);
	::ZeroMemory( 
		bitmapUpdateSector.GetData(), 
		bitmapUpdateSector.GetCount() * BLOCK_SIZE 
		);
	// Calculate total amount of sectors to copy
	nTotalDirtySize = 0;
	nProcessedDirtySize = 0;
	for ( i=0; i < nBitmapSecCount; i++ )
	{
		for ( j=0; j < BLOCK_SIZE; j++ )
		{
			bitFlag = pbBitmap[i*BLOCK_SIZE + j];
			if ( bitFlag == 0x00 )	// flag is clean(since most flags can be clean, this can improve efficiency)
			{
				continue;
			}
			for ( k=0; k < sizeof(_int8)* 8; k++ )
			{
				if ( (bitFlag & ( 0x01 << k )) != 0 )
					nTotalDirtySize += nSectorPerBit;
			}
		}
	}
	NotifyTotalSize( 
		pHandler->GetUserSectorCount(),
		nTotalDirtySize );
	for ( i=0; i < nBitmapSecCount; i++ )
	{
		for ( j=0; j < BLOCK_SIZE; j++ )
		{
			bitFlag = pbBitmap[i*BLOCK_SIZE + j];
			if ( bitFlag == 0x00 )	// flag is clean(since most flags can be clean, this can improve efficiency)
			{
				continue;
			}

			for ( k=0; k < sizeof(_int8)* 8; k++ )
			{
				if ( IsStopped() )
				{
					goto out;
				}
				// Sectors are mapped from LSB to MSB
				if ( (bitFlag & ( 0x01 << k )) != 0 )
				{
					dataSector.SetLocation(
						(static_cast<_int64>(i*BLOCK_SIZE + j)*sizeof(_int8) + k)*nSectorPerBit
						);
					try {
						dataSector.ReadAccept( &sSource );
						dataSector.WriteAccept( &sDest );
					}
					catch( CNDASException &e )
					{
						e.PrintStackTrace();
						Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
						return;
					}
					nProcessedDirtySize += nSectorPerBit;
				}
				NotifyProgressDirty( nProcessedDirtySize );
			} // for ( k=0; ...
		}	// for ( j=0; ...
		// Clean up bitmap
		if ( IsStopped() )
		{
			goto out;
		}
		bitmapUpdateSector.SetLocation( 
			bitmapSector.GetLocation() + static_cast<_int64>(i) 
			);
		try{
			bitmapUpdateSector.WriteAccept( &sSource );
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP );
			return;
		}
		NotifyProgress( static_cast<_int64>(i*BLOCK_SIZE + j) * 8 * nSectorPerBit );
	}	// for ( i=0; ...

	// In case the bitmap does not fit into sectors exactly.
	_int64 nProcessedSize = 
		static_cast<_int64>(nBitmapSecCount) * BLOCK_SIZE * 8 * nSectorPerBit;
	if ( nProcessedSize < pHandler->GetUserSectorCount() )
	{
		int nLeftSize = 
			static_cast<int>(pHandler->GetUserSectorCount()-nProcessedSize);
		for ( i=0; i < nLeftSize; i++ )
		{
			bitFlag = pbBitmap[nBitmapSecCount*BLOCK_SIZE + i/nSectorPerByte];
			if ( IsStopped() )
			{
				goto out;
			}
			if ( (bitFlag & (0x01 << (i%8))) != 0 )
			{
				dataSector.SetLocation(
					static_cast<_int64>(nBitmapSecCount*BLOCK_SIZE + i)*nSectorPerBit
					);
				try {
					dataSector.ReadAccept( &sSource );
					dataSector.WriteAccept( &sDest );
				}
				catch( CNDASException &e )
				{
					e.PrintStackTrace();
					Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
					return;
				}
				nProcessedDirtySize += nSectorPerBit;
				NotifyProgressDirty( nProcessedDirtySize );
			}
		}
		bitmapUpdateSector.SetLocation( 
			bitmapSector.GetLocation() + nBitmapSecCount
			);
		try{
			bitmapUpdateSector.WriteAccept( &sSource );
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP );
			return;
		}
		NotifyProgress( pHandler->GetUserSectorCount() );
	}

	sSource.Logout();
	sDest.Logout();
	sSource.Disconnect();
	sDest.Disconnect();

	try{
		m_pSource->OpenExclusive();
		m_pSource->SetDirty(FALSE);
		m_pSource->CommitDiskInfo( TRUE );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_CLEAR_DIRTYFLAG );
		return;
	}

	Notify( NBSYNC_PHASE_FINISHED );
	return;

out:
	sSource.Logout();
	sDest.Logout();
	sSource.Disconnect();
	sDest.Disconnect();
	
	if ( IsStopped() )
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_STOPPED );
	else
		Notify( NBSYNC_PHASE_FINISHED );
}

void CMirrorWorkThread::RebindMirror()
{
	// Get list of disks involved in the previous mirroring.
	// NOTE : Because disks aggregated can also be mirrored,
	//	there can be more than two disks involved in the mirroring.
	CDiskObjectPtr aggregationRoot;
	aggregationRoot = m_pSource->GetParent();
	while ( !aggregationRoot->GetParent()->IsRoot() )
	{
		aggregationRoot = aggregationRoot->GetParent();
	}

	// Mark all the bitmaps dirty.
	m_pSource->OpenExclusive();
	m_pDest->OpenExclusive();
	m_pSource->MarkAllBitmap();

	CUnitDiskInfoHandlerPtr pHandler = m_pSource->GetInfoHandler();
	aggregationRoot->Rebind( 
						m_pDest, 
						pHandler->GetPosInBind() ^ 0x01
						);
	aggregationRoot->CommitDiskInfo(TRUE);


	// Write binding information to the destination disk
	m_pDest->Mirror(m_pSource);
	m_pDest->CommitDiskInfo(TRUE);
	m_bRebound = TRUE;

	m_pSource->Close();
	m_pDest->Close();
}

void CMirrorWorkThread::AddMirror()
{
	// Mark all the bitmaps dirty
	m_pSource->OpenExclusive();
	m_pDest->OpenExclusive();
	m_pSource->MarkAllBitmap();

	// Bind the two disks
	CDiskObjectVector vtDisks;

	vtDisks.push_back( m_pSource );
	vtDisks.push_back( m_pDest );
	
	m_pSource->Bind( vtDisks, 0, TRUE );
	m_pDest->Bind( vtDisks, 1, TRUE );

	m_pSource->CommitDiskInfo();
	m_pDest->CommitDiskInfo();
	m_bAdded = TRUE;
	m_pSource->Close();
	m_pDest->Close();
}
void CMirrorWorkThread::Notify(UINT nPhase, int nErrorCode)
{
	m_report.nPhase = nPhase;
	m_report.nErrorCode = nErrorCode;
	CSubject::Notify();
}
void CMirrorWorkThread::NotifyTotalSize(_int64  nTotalSize, _int64 nTotalDirtySize)
{
	m_report.nTotalSize = nTotalSize;
	m_report.nTotalDirtySize = nTotalDirtySize;
	CSubject::Notify();
}
void CMirrorWorkThread::NotifyRebound()
{
	m_report.bRebound = TRUE;
	CSubject::Notify();
}
void CMirrorWorkThread::NotifyProgress(_int64 nProcessedSize)
{
	m_report.nProcessedSize = nProcessedSize;
	CSubject::Notify();
}

void CMirrorWorkThread::NotifyProgressDirty(_int64 nProcessedDirtySize)
{
	m_report.nProcessedDirtySize = nProcessedDirtySize;
	CSubject::Notify();
}

const NDAS_SYNC_REPORT *CMirrorWorkThread::GetReport()
{
	return static_cast<NDAS_SYNC_REPORT*>( &m_report );
}


void CMirrorWorkThread::SetSource(CUnitDiskObjectPtr source)
{
	m_pSource = source;
}
void CMirrorWorkThread::SetDest(CUnitDiskObjectPtr dest)
{
	m_pDest = dest;
}


///////////////////////////////////////////////////////////////////////////////
// CMirrorDlg
///////////////////////////////////////////////////////////////////////////////
CMirrorDlg::CMirrorDlg(int nWorkType)
: m_bRunning(FALSE), m_bFinished(FALSE), m_nCurrentPhase(0),
  m_bRebound(FALSE),
  m_fPrevMBPerSec(0), m_nWorkType(nWorkType), 
  m_workThread( CMirrorWorkThread(nWorkType) )
{
}

void CMirrorDlg::SetSyncDisks(CMirDiskObjectPtr mirDisks)
{
	// TODO : How do we select source disk?
	UINT nDirtyCount = mirDisks->GetDirtyDiskCount();
	ATLASSERT ( nDirtyCount > 0 );

	if ( nDirtyCount > 1 )
	{
		// TODO : Not implemented(We need to ask user to select one)
	}

	UINT nDirtyDiskIdx = mirDisks->GetDirtyDiskIndex();
	if ( nDirtyDiskIdx == 0 )
	{
		m_pSource = boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->front());
		m_pDest = boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->back());
	}
	else
	{
		m_pSource = boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->back());
		m_pDest = boost::dynamic_pointer_cast<CUnitDiskObject>(mirDisks->front());
	}

	m_pMirDisks = mirDisks;
}

void CMirrorDlg::SetReMirDisks(CMirDiskObjectPtr source, CUnitDiskObjectPtr dest)
{
	m_pMirDisks = source;
	if ( m_pMirDisks->front()->IsUsable() )
	{
		m_pSource = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(m_pMirDisks->front());
	}
	else
	{
		m_pSource =
			boost::dynamic_pointer_cast<CUnitDiskObject>(m_pMirDisks->back());
	}
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
	m_btnOK.SetWindowText( _T("Stop") );
	m_btnCancel.EnableWindow( FALSE );

	m_bRunning = TRUE;
	m_workThread.Attach( this );
	m_workThread.SetSource( m_pSource );
	m_workThread.SetDest( m_pDest );

	m_timeBegin = CTime::GetCurrentTime();
	m_workThread.Execute();
	MsgWaitForNotification();
	
	m_bRunning = FALSE;
	if ( m_bFinished )
		m_btnOK.SetWindowText( _T("OK") );
	else
		m_btnOK.SetWindowText( _T("Resume") );
	
	m_btnCancel.EnableWindow( TRUE );
	m_btnOK.EnableWindow( TRUE );
}

void CMirrorDlg::Stop()
{
	m_btnOK.EnableWindow( FALSE );
	m_strPhase = _T("Stopping synchronization..");
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
		m_nCurrentPhase = report.nPhase;
		switch( report.nPhase )
		{
		case NBSYNC_PHASE_CONNECT:
			m_strPhase = _T("Connecting to disks..."); // TODO : String resource
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_REBIND:
			m_strPhase = _T("Updating disk information...");
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_BIND:
			m_strPhase = _T("Writing disk information required for mirroring...");
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_RETRIVE_BITMAP:
			m_strPhase = _T("Retrieving synchronization information..."); // TODO : String resource
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_SYNCHRONIZE:
			m_strPhase = _T("Synchronizing..."); // TODO : String resource
			DoDataExchange(FALSE);
			break;
		case NBSYNC_PHASE_FINISHED:
			m_strPhase = _T("Synchronization has finished."); // TODO : String resource
			DoDataExchange(FALSE);
			m_bFinished = TRUE;
			break;
		case NBSYNC_PHASE_FAILED:
		default:
			if ( report.nErrorCode == NBSYNC_ERRORCODE_STOPPED )
			{
				m_strPhase = _T("Synchronization has been stopped.");
				DoDataExchange(FALSE);
			}
			else
			{
				m_strPhase = _T("Synchronization has failed.");
				DoDataExchange(FALSE);
			}
			break;
		}

		if ( report.bRebound && !m_bRebound )
		{
			boost::dynamic_pointer_cast<CDiskObjectComposite>
				(m_pDest->GetParent())->DeleteChild(m_pDest);
			if ( m_pMirDisks->front()->IsUsable() )
			{
				m_pMirDisks->DeleteChild( m_pMirDisks->back() );
			}
			else
			{
				m_pMirDisks->DeleteChild( m_pMirDisks->front() );
			}
			m_pMirDisks->AddChild(m_pMirDisks, m_pDest);
			m_bRebound = TRUE;
		}
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

		// Display progressbar message
		/*
		WTL::CString strProgBarMsg;

		strProgBarMsg.Format(
			"%d / %d KBs", 
			static_cast<int>(report.nProcessedSize/2),
			static_cast<int>(report.nTotalSize/2));
		m_progBar.SetWindowText( strProgBarMsg );
		*/

		// Display time left
		CTime timeNow = CTime::GetCurrentTime();
		CTimeSpan timeElapsed = timeNow - m_timeBegin;
		CTimeSpan timeLeft;
		if ( timeElapsed.GetTotalSeconds() != 0 
			&& m_timePrev != timeNow	// To prevent too frequent update
			)
		{
			m_timePrev = timeNow;
			// Calculate transfer rate
			double fMBPerSecond = 
				(report.nProcessedDirtySize) / 2.0 / 1024
				/ timeElapsed.GetTotalSeconds();
			

			WTL::CString strMBPerSecond;
			strMBPerSecond.Format( _T("%.01f"),fMBPerSecond);
			::SetWindowText( GetDlgItem(IDC_TEXT_RATE), strMBPerSecond );

			WTL::CString strTimeLeft;
			timeLeft = 
				static_cast<UINT>(
					(report.nTotalDirtySize-report.nProcessedDirtySize) 
					/ ( fMBPerSecond * 2 * 1024 )
				);
			int nHour, nMin, nSec;
			nSec = timeLeft.GetSeconds();
			nMin = timeLeft.GetMinutes();
			nHour = static_cast<int>(timeLeft.GetTotalHours());
			strTimeLeft.Format( _T("%02d:%02d:%02d"), nHour, nMin, nSec );
			//strTimeLeft = timeLeft.Format(TIME_FORMAT);
			::SetWindowText( GetDlgItem(IDC_TEXT_LEFTTIME), strTimeLeft );
		}
	}
	return;
}


