#ifndef MSIPROGRESSBAR_H
#define MSIPROGRESSBAR_H

#include <msiquery.h>
#pragma comment(lib, "msi.lib")

//
// Helper class for manipulating Windows Installer progress bar
// used in custom action dlls
//
// cslee@ximeta.com
//
// $id$
//
// $log$
//

class MsiProgressBar
{
	MSIHANDLE m_hInstall;
	PMSIHANDLE m_hActionRec;
	PMSIHANDLE m_hProgressRec;
	int m_iTotalTicks;
	int m_iTickIncrement;
	int m_iCurrent;

public:

	MsiProgressBar(MSIHANDLE hInstall, int iTotalTicks, int iTickIncrement) :
		m_hInstall(hInstall),
		m_iTotalTicks(iTotalTicks),
		m_iTickIncrement(iTickIncrement),
		m_iCurrent(0)
	{
		UINT iResult;

		// Tell the installer to use explicit progress messages.
		PMSIHANDLE hProgressRec = MsiCreateRecord(3);
		// Provides information related to progress messages 
		// to be sent by the current action.
		MsiRecordSetInteger(hProgressRec, 1, 1);
		// Number of ticks the progress bar moves for each ActionData message. 
		// This field is ignored if Field 3 is 0.
		MsiRecordSetInteger(hProgressRec, 2, 1);
		// The current action will send explicit ProgressReport messages.
		MsiRecordSetInteger(hProgressRec, 3, 0);
		iResult = MsiProcessMessage(m_hInstall, INSTALLMESSAGE_PROGRESS, hProgressRec);
	}

	virtual ~MsiProgressBar() {}

	void Reset()
	{
		PMSIHANDLE hProgressRec = MsiCreateRecord(4);
		// Resets progress bar and sets the expected total number of ticks in the bar.
		MsiRecordSetInteger(hProgressRec, 1, 0);
		// Expected total number of ticks in the progress bar.
		MsiRecordSetInteger(hProgressRec, 2, m_iTotalTicks);
		// Forward progress bar
		MsiRecordSetInteger(hProgressRec, 3, 0); 
		// Execution is in progress
		MsiRecordSetInteger(hProgressRec, 4, 0);
		MsiProcessMessage(m_hInstall, INSTALLMESSAGE_PROGRESS, hProgressRec);
		m_iCurrent = 0;
	}

	void Step()
	{
		m_iCurrent++;
		PMSIHANDLE hProgressRec = MsiCreateRecord(2);
		// Increments the progress bar.
		MsiRecordSetInteger(hProgressRec, 1, 2);
		// Number of ticks the progress bar has moved.
		MsiRecordSetInteger(hProgressRec, 2, m_iCurrent * m_iTickIncrement);
		MsiProcessMessage(m_hInstall, INSTALLMESSAGE_PROGRESS, hProgressRec);
	}
};

#endif