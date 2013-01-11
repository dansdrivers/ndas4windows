///////////////////////////////////////////////////////////////////////////////
//
// Implemenation of CWorkThread which is used for background job
//
///////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "ndasmirrorthread.h"
#include "ndassector.h"
#include "ndassession.h"
#include "ndasexception.h"

CWorkThread::CWorkThread(void)
: m_hThread(INVALID_HANDLE_VALUE)
{
	::InitializeCriticalSection( &m_csSync );
}

CWorkThread::~CWorkThread(void)
{
	::DeleteCriticalSection( &m_csSync );
}

DWORD WINAPI CWorkThread::ThreadStart(LPVOID pParam)
{
	reinterpret_cast<CWorkThread*>(pParam)->Run();
	return 0;
}

void CWorkThread::Execute()
{
	m_bStopped = FALSE;
	m_hThread = ::CreateThread( 
					NULL, 
					0, 
					CWorkThread::ThreadStart,
					reinterpret_cast<LPVOID>(this),
					0, 
					NULL
					);
}

CWorkThread::operator HANDLE() const
{
	return m_hThread;
}

HANDLE CWorkThread::GetHandle() const
{
	return m_hThread;
}

void CWorkThread::Stop()
{
	::EnterCriticalSection( &m_csSync );
	m_bStopped = TRUE;
	::LeaveCriticalSection( &m_csSync );
}

BOOL CWorkThread::IsStopped()
{
	BOOL bStopped;
	::EnterCriticalSection( &m_csSync );
	bStopped = m_bStopped;
	::LeaveCriticalSection( &m_csSync );
	return bStopped;
}
///////////////////////////////////////////////////////////////////////////////
// CMirrorWorkThread
///////////////////////////////////////////////////////////////////////////////
CMirrorWorkThread::CMirrorWorkThread(int nWorkType)
: m_nWorkType(nWorkType), m_bBound(FALSE)
{
}

void CMirrorWorkThread::Run()
{
	::ZeroMemory( &m_report, sizeof(NBSYNC_REPORT) );
	m_report.nSize = sizeof(NBSYNC_REPORT);

	Notify( NBSYNC_PHASE_CONNECT );
	try {
		m_pSource->Open( TRUE );
		m_pDest->Open( TRUE );
	}
	catch( CNDASException &e )
	{
		m_pSource->Close();
		m_pDest->Close();
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_CONNECT );
		return;
	}

	if ( m_nWorkType == NBSYNC_TYPE_REMIRROR && !m_bBound )
	{
		if ( !RebindMirror() )
			goto out;
	}

	if ( m_nWorkType == NBSYNC_TYPE_ADDMIRROR && !m_bBound )
	{
		Notify( NBSYNC_PHASE_BIND );
		if ( !AddMirror() )
			goto out;
	}

	if ( m_pDest->GetInfoHandler()->IsPeerDirty() )
	{
		// If both disks are dirty, merge bitmap data.
		if ( !MergeBitmap() )
			goto out;
	}

	SyncMirror();
out:
	m_pSource->Close();
	m_pDest->Close();
}

BOOL CMirrorWorkThread::MergeBitmap()
{
	CBitmapSector sourceBitmap, destBitmap;
	
	try {
		sourceBitmap.ReadAccept( m_pSource->GetSession() );
		destBitmap.ReadAccept( m_pDest->GetSession() );
	}
	catch ( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_READ_BITMAP );
		return FALSE;
	}

	sourceBitmap.Merge( &destBitmap );

	try {
		sourceBitmap.WriteAccept( m_pSource->GetSession() );
		//
		// Clear dest disk's bitmap
		//
		m_pDest->MarkAllBitmap( FALSE );
		m_pDest->SetDirty( FALSE );
		m_pDest->CommitDiskInfo();
	}
	catch ( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP );
		return FALSE;
	}

	return TRUE;
}

BOOL CMirrorWorkThread::SyncMirror()
{
	CBitmapSector bitmapSector;

	Notify( NBSYNC_PHASE_RETRIVE_BITMAP );
	//
	// Get bitmap from the disk
	//
	try{
		bitmapSector.ReadAccept( m_pSource->GetSession() );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_READ_BITMAP );
		return FALSE;
	}

	Notify( NBSYNC_PHASE_SYNCHRONIZE );

	//
	// Copying blocks
	//
	CHDDDiskInfoHandler *pHandler;

	pHandler = dynamic_cast<CHDDDiskInfoHandler*>(m_pSource->GetInfoHandler().get());
	ATLASSERT( pHandler != NULL );

	int			i, j, k;
	int 		nBitCount, nBitIdx;
	int			nBitmapSecCount;
	int			nSectorPerBit, nSectorPerByte;
	int			nRemSector;	// The remainder when the number of total user sectors is divided by nSectorPerBit
	int			nRemBit;	// The remainder when the number of bits is divided by the number of bits in a sector
	_int8		bitFlag;
	_int8		*pbBitmap;
	_int64		nTotalDirtySize;
	_int64		nProcessedDirtySize;
	CDataSector dataSector, bitmapUpdateSector;

	pbBitmap = bitmapSector.GetData();
	nSectorPerBit = pHandler->GetSectorsPerBit();
	nSectorPerByte = nSectorPerBit*8;

	dataSector.Resize(nSectorPerBit);
	::ZeroMemory( 
		bitmapUpdateSector.GetData(), 
		bitmapUpdateSector.GetCount() * NDAS_BLOCK_SIZE 
		);

	// Calculate total amount of sectors to copy
	nTotalDirtySize = 0;
	nProcessedDirtySize = 0;
	
	// TODO : DISK_INFORMATINO_BLOCK validation should be done somewhere...
	nBitCount = static_cast<int>(pHandler->GetUserSectorCount() / nSectorPerBit);
	nRemSector = static_cast<int>(pHandler->GetUserSectorCount() % nSectorPerBit);
	for ( nBitIdx=0; nBitIdx < nBitCount; nBitIdx++ )
	{
		bitFlag = pbBitmap[ nBitIdx/8 ];
		if ( (bitFlag & (0x01 << (nBitIdx%8))) != 0 )
		{
			nTotalDirtySize += nSectorPerBit;
		}
	}
	if ( nRemSector > 0 )
	{
		bitFlag = pbBitmap[ nBitCount/8 ];
		if ( (bitFlag & (0x01 << (nBitCount%8))) != 0 )
		{
			nTotalDirtySize += nRemSector;
		}
	}

	NotifyTotalSize( 
		pHandler->GetUserSectorCount(),
		nTotalDirtySize );

	nBitmapSecCount = nBitCount / 8 / NDAS_BLOCK_SIZE;
	nRemBit = nBitCount - (nBitmapSecCount * 8 * NDAS_BLOCK_SIZE);
	for ( i=0; i < nBitmapSecCount; i++ )
	{
		for ( j=0; j < NDAS_BLOCK_SIZE; j++ )
		{
			bitFlag = pbBitmap[i*NDAS_BLOCK_SIZE + j];
			if ( bitFlag == 0x00 )	// flag is clean(since most flags can be clean, this can improve efficiency)
			{
				continue;
			}

			for ( k=0; k < 8; k++ )
			{
				if ( IsStopped() )
				{
					Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_STOPPED );
					return FALSE;
				}
				// Sectors are mapped from LSB to MSB
				if ( (bitFlag & ( 0x01 << k )) != 0 )
				{
					dataSector.SetLocation(
						static_cast<_int64>((i*NDAS_BLOCK_SIZE + j)*8 + k)*nSectorPerBit
						);
					try {
						dataSector.ReadAccept( m_pSource->GetSession() );
						dataSector.WriteAccept( m_pDest->GetSession() );
					}
					catch( CNDASException &e )
					{
						e.PrintStackTrace();
						Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
						return FALSE;
					}
					nProcessedDirtySize += nSectorPerBit;
				}
				NotifyProgressDirty( nProcessedDirtySize );
			} // for ( k=0; ...
			NotifyProgress( static_cast<_int64>(i*NDAS_BLOCK_SIZE + j) * 8 * nSectorPerBit );
		}	// for ( j=0; ...
		// Clean up bitmap
		if ( IsStopped() )
		{
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_STOPPED );
			return FALSE;
		}
		bitmapUpdateSector.SetLocation( 
			bitmapSector.GetLocation() + static_cast<_int64>(i) 
			);
		try{
			bitmapUpdateSector.WriteAccept( m_pSource->GetSession() );
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP );
			return FALSE;
		}
		NotifyProgress( static_cast<_int64>(i*NDAS_BLOCK_SIZE + j) * 8 * nSectorPerBit );
	}	// for ( i=0; ...

	// Copy remaining blocks
	if ( nRemBit > 0 || nRemSector > 0 )
	{
		for ( nBitIdx = nBitmapSecCount*8*NDAS_BLOCK_SIZE; nBitIdx < nBitCount; nBitIdx++ )
		{
			if ( IsStopped() )
			{
				Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_STOPPED );
				return FALSE;
			}
			bitFlag = pbBitmap[ nBitIdx/8 ];
			if ( (bitFlag & (0x01 << (nBitIdx%8))) != 0 )
			{
				dataSector.SetLocation( 
					static_cast<_int64>(nBitIdx) * nSectorPerBit
					);
				try {
					dataSector.ReadAccept( m_pSource->GetSession() );
					dataSector.WriteAccept( m_pDest->GetSession() );
				}
				catch( CNDASException &e )
				{
					e.PrintStackTrace();
					Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
					return FALSE;
				}
				nProcessedDirtySize += nSectorPerBit;
				NotifyProgressDirty( nProcessedDirtySize );
			}
			NotifyProgress( nBitIdx * nSectorPerBit );
		}

		if ( nRemSector > 0 )
		{
			bitFlag = pbBitmap[ nBitCount/8 ];
			if ( (bitFlag & (0x01 << nBitCount%8)) != 0 )
			{
				dataSector.Resize(nRemSector);
				dataSector.SetLocation( 
					static_cast<_int64>(nBitCount) * nSectorPerBit
					);
				try {
					dataSector.ReadAccept( m_pSource->GetSession() );
					dataSector.WriteAccept( m_pDest->GetSession() );
				}
				catch( CNDASException &e )
				{
					e.PrintStackTrace();
					Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
					return FALSE;
				}
				nProcessedDirtySize += nRemSector;
				NotifyProgressDirty( nProcessedDirtySize );
			}
		}
		bitmapUpdateSector.SetLocation( 
			bitmapSector.GetLocation() + nBitmapSecCount
			);
		try{
			bitmapUpdateSector.WriteAccept( m_pSource->GetSession() );
		}
		catch( CNDASException &e )
		{
			e.PrintStackTrace();
			Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_UPDATE_BITMAP );
			return FALSE;
		}
		NotifyProgress( pHandler->GetUserSectorCount() );
	}

	// clear all Bitmaps
	try {
		dataSector.Resize(NDAS_BLOCK_SIZE_BITMAP);
		dataSector.SetLocation(NDAS_BLOCK_LOCATION_BITMAP);
		ZeroMemory(dataSector.GetData(), NDAS_BLOCK_SIZE_BITMAP * 512);
		dataSector.WriteAccept( m_pSource->GetSession() );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
		return FALSE;
	}

	// read NDAS_BLOCK_LOCATION_WRITE_LOG and write 
	LAST_WRITTEN_SECTOR LWS;
	try {
		// read LWS of source disk
		dataSector.Resize(1);
		dataSector.SetLocation(NDAS_BLOCK_LOCATION_WRITE_LOG);
		dataSector.ReadAccept( m_pSource->GetSession() );
		CopyMemory(&LWS, dataSector.GetData(), sizeof(LAST_WRITTEN_SECTOR));

		// AING_TO_DO : need correct check
		if(LWS.transferBlocks > 0 && LWS.transferBlocks <= 128)
		{
			// copy area of LWS from source to dest
			dataSector.Resize(LWS.transferBlocks);
			dataSector.SetLocation(LWS.logicalBlockAddress);
			dataSector.ReadAccept( m_pSource->GetSession() );
			dataSector.WriteAccept( m_pDest->GetSession() );
		}		

			// clear LWSs
			dataSector.Resize(1);
			dataSector.SetLocation(NDAS_BLOCK_LOCATION_WRITE_LOG);
			ZeroMemory(dataSector.GetData(), 512);
			dataSector.WriteAccept( m_pSource->GetSession() );
			dataSector.WriteAccept( m_pDest->GetSession() );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_COPY );
		return FALSE;
	}

	//
	// Clear dirty flag
	//
	try{
		m_pSource->SetDirty(FALSE);
		m_pSource->CommitDiskInfo( TRUE );
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_CLEAR_DIRTYFLAG );
		return FALSE;
	}

	Notify( NBSYNC_PHASE_FINISHED );
	return TRUE;
}

BOOL CMirrorWorkThread::RebindMirror()
{
	Notify( NBSYNC_PHASE_REBIND );
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
	try {
		m_pSource->MarkAllBitmap();
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_MARK_BITMAP );
		return FALSE;
	}

	try {
		CUnitDiskInfoHandlerPtr pHandler = m_pSource->GetInfoHandler();
		aggregationRoot->Rebind( 
							m_pDest, 
							pHandler->GetPosInBind() ^ 0x01
							);
		m_pSource->SetDirty(TRUE);
		aggregationRoot->CommitDiskInfo(TRUE);


		// Write binding information to the destination disk
		m_pDest->Mirror(m_pSource);
		m_pDest->CommitDiskInfo(TRUE);
	}
	catch ( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_ADDMIRROR);
		return FALSE;
	}
	m_bBound = TRUE;

	return TRUE;
}

BOOL CMirrorWorkThread::AddMirror()
{
	Notify( NBSYNC_PHASE_BIND );
	CUnitDiskObjectVector vtDisks;

	vtDisks.push_back( m_pSource );
	vtDisks.push_back( m_pDest );
	
	try {
		// Bind the two disks
		m_pSource->Bind( vtDisks, 0, NMT_RAID1, FALSE );
		m_pDest->Bind( vtDisks, 1, NMT_RAID1, FALSE );
		// Mark bitmap dirty
		m_pSource->SetDirty(TRUE);
		m_pSource->MarkAllBitmap();

		m_pSource->CommitDiskInfo();
		m_pDest->CommitDiskInfo();
	}
	catch( CNDASException &e )
	{
		e.PrintStackTrace();
		Notify( NBSYNC_PHASE_FAILED, NBSYNC_ERRORCODE_FAIL_TO_ADDMIRROR );
		return FALSE;
	}
	m_bBound = TRUE;
	return TRUE;
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
void CMirrorWorkThread::NotifyBound()
{
	m_report.bBound = TRUE;
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


