#include "stdafx.h"
#include "ndasmgmt.h"
#include "ndastaskdlgs.hpp"


// \x25E6 - White Bullet
// \x2022 - Bullet (\x95)
// \x25DE - Inverse Bullet
// \x25CF - Black Circle

enum {

	UnicodeCharBullet		 = 0x2022,
	UnicodeCharWhiteBullet	 = 0x25E6,
	UnicodeCharInverseBullet = 0x25DE,
	UnicodeCharBlackCircle	 = 0x25CF,
};

LONG DbgLevelMagmTask = DBG_LEVEL_MAGM_TASK;

#define NdasUiDbgCall(l,x,...) do {						\
	if (l <= DbgLevelMagmTask) {						\
	ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
	ATLTRACE (x,__VA_ARGS__);							\
	} 													\
} while(0)

//////////////////////////////////////////////////////////////////////////
//
// Mount Task Dialog
//
//////////////////////////////////////////////////////////////////////////

CMountTaskDialog::CMountTaskDialog(ndas::LogicalDevicePtr LogicalUnit, BOOL WriteMode) :
	m_pLogicalUnit(LogicalUnit),
	m_accessMode(WriteMode),
	m_ignoreWarning(FALSE)
{
	ModifyFlags(0, 
		TDF_ALLOW_DIALOG_CANCELLATION | 
		TDF_CALLBACK_TIMER |
		TDF_SHOW_MARQUEE_PROGRESS_BAR |
		TDF_CAN_BE_MINIMIZED);

	if (!pIsCommCtrl60Available())
	{
		ModifyFlags(TDF_SHOW_MARQUEE_PROGRESS_BAR, 0);
	}

	SetCommonButtons(TDCBF_CLOSE_BUTTON);
	SetWindowTitle(IDS_MAIN_TITLE);
	SetMainIcon(IDR_MAINFRAME);
	SetMainInstructionText(IDC_WAIT_MOUNT_MESSAGE);

	// \x25CF - black circle
	CString name = LogicalUnit->GetName();
	m_LogicalUnitName.Format(_T(" %c %s"), UnicodeCharBullet, static_cast<LPCTSTR>(name));
	SetContentText(static_cast<LPCTSTR>(m_LogicalUnitName));
}

void 
CMountTaskDialog::OnCreated()
{
	m_state = Initialized;

	if (pIsCommCtrl60Available())
	{
		SetMarqueeProgressBar(TRUE);
		SetProgressBarMarquee(TRUE, 300);
	}

	EnableButton(IDCLOSE, FALSE);
	// pEnableTaskDialogButtons(m_hWnd, FALSE);
}

void
CMountTaskDialog::OnDestroyed()
{
	WaitForSingleObject(m_threadCompleted, INFINITE);
}

BOOL 
CMountTaskDialog::OnButtonClicked(int nButton)
{
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_threadCompleted, 0))
	{
		ATLTRACE("Button %d is clicked\n", nButton);
		return FALSE;
	}
	return TRUE;
}

BOOL 
CMountTaskDialog::OnTimer(DWORD dwTickCount)
{
	//
	// return TRUE to reset counter,
	//        FALSE not to reset counter
	//

	BOOL success;
	NDAS_LOGICALDEVICE_STATUS status;

	switch (m_state)
	{
	case Initialized:

		ATLVERIFY( ResetEvent(m_threadCompleted) );

		success = QueueUserWorkItem(
			CMountTaskBase<CMountTaskDialog>::ThreadStart, 
			this, 
			WT_EXECUTEDEFAULT);

		if (!success)
		{
			m_hr = HRESULT_FROM_WIN32(GetLastError());
			ATLTRACE("QueueUserWorkItem failed, hr=0x%X\n", m_hr);
			m_state = Completed;
		}
		else
		{
			m_state = Executing;
		}
		return TRUE; 

	case Executing:

		ATLTRACE("Executing for %d ms\n", dwTickCount);

		if (WAIT_OBJECT_0 == WaitForSingleObject(m_threadCompleted, 0))
		{
			EnableButton(IDCLOSE, TRUE);
			m_state = Completed;
			return TRUE;
		}

		return FALSE;

	case Completed:

#ifdef _DEBUG
		if (dwTickCount < 5 * 1000) return FALSE;
#endif

		ATLTRACE("Completed in %d ms\n", dwTickCount);

		//
		// Check for error
		//
		if (FAILED(m_hr))
		{
			ATLTRACE("Task failed, hr=0x%X\n", m_hr);

			ClickButton(IDCANCEL);

			return TRUE;
		}

		//
		// Wait until mounting is completed
		//
		success = m_pLogicalUnit->UpdateStatus();
		if (!success)
		{
			m_hr = HRESULT_FROM_WIN32(GetLastError());
			ATLTRACE("LogicalUnit->UpdateStatus failed, hr=0x%X\n", m_hr); 

			ClickButton(IDCANCEL);

			return FALSE;
		}

		status = m_pLogicalUnit->GetStatus();
		if (NDAS_LOGICALDEVICE_STATUS_MOUNT_PENDING != status)
		{
			ClickButton(IDCANCEL);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;   // don't reset counter
}

void
CMountTaskDialog::SetIgnoreWarning(BOOL ignoreWarning)
{
	m_ignoreWarning = ignoreWarning;
}

DWORD 
CMountTaskDialog::ThreadStart()
{
	BOOL success = m_pLogicalUnit->PlugIn(m_accessMode, m_ignoreWarning);

	if (!success) {

		m_hr = HRESULT_FROM_WIN32(GetLastError());
	
	} else {

		m_hr = S_OK;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
// Dismount Task Dialog
//
//////////////////////////////////////////////////////////////////////////

CDismountTaskDialog::CDismountTaskDialog(ndas::LogicalDevicePtr LogicalUnit) :
	m_pLogicalUnit(LogicalUnit)
{
	ModifyFlags(0, 
		TDF_ALLOW_DIALOG_CANCELLATION | 
		TDF_CALLBACK_TIMER |
		TDF_SHOW_MARQUEE_PROGRESS_BAR |
		TDF_CAN_BE_MINIMIZED);

	if (!pIsCommCtrl60Available())
	{
		ModifyFlags(TDF_SHOW_MARQUEE_PROGRESS_BAR, 0);
	}

	SetCommonButtons(TDCBF_CLOSE_BUTTON);
	SetWindowTitle(IDS_MAIN_TITLE);
	SetMainIcon(IDR_MAINFRAME);
	SetMainInstructionText(IDC_WAIT_UNMOUNT_MESSAGE);

	CString name = LogicalUnit->GetName();
	m_LogicalUnitName.Format(_T(" %c %s"), UnicodeCharBullet, static_cast<LPCTSTR>(name));
	SetContentText(static_cast<LPCTSTR>(m_LogicalUnitName));

	m_EjectParam.Size = sizeof(NDAS_LOGICALDEVICE_EJECT_PARAM);
	m_EjectParam.LogicalDeviceId = m_pLogicalUnit->GetLogicalDeviceId();
}

void 
CDismountTaskDialog::OnCreated()
{
	m_state = Initialized;

	if (pIsCommCtrl60Available())
	{
		SetMarqueeProgressBar(TRUE);
		SetProgressBarMarquee(TRUE, 300);
	}

	EnableButton(IDCLOSE, FALSE);
	// pEnableTaskDialogButtons(m_hWnd, FALSE);
}

void
CDismountTaskDialog::OnDestroyed()
{
	WaitForSingleObject(m_threadCompleted, INFINITE);
}

BOOL 
CDismountTaskDialog::OnButtonClicked(int nButton)
{
	if (WAIT_OBJECT_0 == WaitForSingleObject(m_threadCompleted, 0))
	{
		ATLTRACE("Button %d is clicked\n", nButton);
		return FALSE;
	}
	return TRUE;
}

BOOL 
CDismountTaskDialog::OnTimer(DWORD dwTickCount)
{
	//
	// return TRUE to reset counter,
	//        FALSE not to reset counter
	//

	BOOL success;
	NDAS_LOGICALUNIT_STATUS status;

	switch (m_state)
	{
	case Initialized:

		ATLVERIFY( ResetEvent(m_threadCompleted) );

		success = QueueUserWorkItem(
			CMountTaskBase<CDismountTaskDialog>::ThreadStart, 
			this, 
			WT_EXECUTEDEFAULT);

		if (!success)
		{
			m_hr = HRESULT_FROM_WIN32(GetLastError());
			ATLTRACE("QueueUserWorkItem failed, hr=0x%X\n", m_hr);
			m_state = Completed;
		}
		else
		{
			m_state = Executing;
		}
		return TRUE; 

	case Executing:

		NdasUiDbgCall( 3, "Executing for %d ms\n", dwTickCount );

		if (WaitForSingleObject(m_threadCompleted, 0) == WAIT_OBJECT_0) {

			EnableButton(IDCLOSE, TRUE);
			m_state = Completed;
			return TRUE;
		}

		return FALSE;

	case Completed:

		ATLTRACE("Completed in %d ms, hr=0x%X\n", dwTickCount, m_hr);

		//
		// Wait until mounting is completed
		//
		success = m_pLogicalUnit->UpdateStatus();

		if (!success)
		{
			m_hr =	AtlHresultFromLastError();
			ATLTRACE("LogicalUnit->UpdateStatus failed, hr=0x%X\n", m_hr); 

			ClickButton(IDCANCEL);
			return FALSE;
		}

		status = m_pLogicalUnit->GetStatus();

		ATLTRACE("NdasLogicalUnit status=%d\n", status);

		if (S_OK == m_hr && NDAS_LOGICALUNIT_STATUS_MOUNTED == status)
		{
			//
			// Eject is not vetoed.
			// We have to wait until the status changes to DISMOUNTED
			//
			return TRUE;
		}

		ClickButton(IDCANCEL);
		return FALSE;
	}

	return FALSE;   // don't reset counter
}

DWORD 
CDismountTaskDialog::ThreadStart()
{
	BOOL success = m_pLogicalUnit->Eject(&m_EjectParam);
	if (!success)
	{
		m_hr = AtlHresultFromLastError();
	}
	else
	{
		m_hr = S_OK;
	}
	return 0;
}

