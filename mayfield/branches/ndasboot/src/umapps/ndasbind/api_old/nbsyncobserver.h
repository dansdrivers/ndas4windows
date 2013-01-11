////////////////////////////////////////////////////////////////////////////
//
// Interface of CSyncObserver class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////


#ifndef _NBSYNCOBSERVER_H_
#define _NBSYNCOBSERVER_H_

#include "ndasobserver.h"
#include "ndasmirrorthread.h"
#include "ndasbind.h"

class CSyncObserver : public CMultithreadedObserver
{
public:
	//
	// Wait for subject thread to end
	//
	// @param pThread
	//   [in] the thread to wait for.
	// @param lpProgressRoutine
	//   [in] Address of a callback function of type LPSYNC_PROGRESS_ROUTINE that is called each time
	//        another portion of the data has been copied. The amount of data copied between each call can vary each time.
	//        This parameter can be NULL.
	// @param lpData
	//   [in] Argument to be passed to the callback function. This parameter can be NULL.
	// @param pbCancel
	//   [in] If this flag is set to TRUE during the synchronization, the operation will be canceled. 
	//        Otherwise, the copy operation will continue to completion. 
	// @return TRUE if the thread terminated successfully
	//
	BOOL WaitForNotification(CWorkThread* pThread, LPSYNC_PROGRESS_ROUTINE lpRoutine, LPVOID lpData, LPBOOL pbCancel);
};

#endif // _NBSYNCOBSERVER_H_