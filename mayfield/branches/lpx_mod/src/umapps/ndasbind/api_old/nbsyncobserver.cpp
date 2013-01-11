////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSyncObserver class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "nbsyncobserver.h"

BOOL
CSyncObserver::WaitForNotification(CWorkThread* pThread, 
								   LPSYNC_PROGRESS_ROUTINE lpRoutine, 
								   LPVOID lpData, 
								   LPBOOL pbCancel)
{
	BOOL bResult = FALSE;
	BOOL bStopped = FALSE;
	DWORD dwRet;
	HANDLE hObjects[2];
	hObjects[0] = pThread->GetHandle();
	hObjects[1] = m_hSyncEvent;

	NBSYNC_REPORT report;
	do {
		dwRet = ::WaitForMultipleObjects(2, hObjects, FALSE, INFINITE);
		if ( dwRet == WAIT_FAILED )
		{
			break;
		}
		else if ( dwRet == WAIT_OBJECT_0 )
		{
			// Thread terminated
			report.nSize = sizeof(NBSYNC_REPORT);
			CMultithreadedObserver::GerReport( static_cast<NDAS_SYNC_REPORT*>(&report) );
			switch ( report.nPhase )
			{
			case NBSYNC_PHASE_FINISHED:
				bResult = TRUE;
				break;
			case NBSYNC_PHASE_FAILED:
			default:
				// ERROR : Thread has finished with error
				bResult = FALSE;
				break;
			}
			break; // Exit do-while loop
		}
		else if ( dwRet == WAIT_OBJECT_0 + 1)
		{
			if ( bStopped )
				continue;	// Ignore any event after stop.
			// Event notification is received
			report.nSize = sizeof(NBSYNC_REPORT);
			CMultithreadedObserver::GerReport( static_cast<NDAS_SYNC_REPORT*>(&report) );
			switch ( report.nPhase )
			{
			case NBSYNC_PHASE_SYNCHRONIZE:
				{
					BOOL bCallBackResult;
					LARGE_INTEGER TotalSize, ProcessedSize, TotalDirtySize, ProcessedDirtySize;
					TotalSize.QuadPart = report.nTotalSize;
					ProcessedSize.QuadPart = report.nProcessedSize;
					TotalDirtySize.QuadPart = report.nTotalDirtySize;
					ProcessedDirtySize.QuadPart = report.nProcessedDirtySize;
					bCallBackResult = lpRoutine( 
										TotalSize, ProcessedSize, 
										TotalDirtySize, ProcessedDirtySize,
										lpData
										);
					if ( !bCallBackResult )
					{
						pThread->Stop();
						bStopped = TRUE;
					}
				}
			default:
				break;
			}
		}
		// Terminate thread if cancel flag is set.
		if ( pbCancel != NULL && *pbCancel == TRUE )
		{
			// Operation cancelled
			pThread->Stop();
			bStopped = TRUE;
		}
	} while ( TRUE );

	return bResult;
}