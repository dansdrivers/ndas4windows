#include "stdafx.h"
#include "resource.h"
#include "setupui.h"
#include "setuptask.h"
#include "downloadbsc.h"
#include "msiproc.h"
#include "winutil.h"
#include "ndas/ndupdate.h"

CSetupTask::CSetupTask(ISetupUI* pSetupUI) :
	m_pSetupUI(pSetupUI),
	m_hThread(NULL),
	m_fCanceled(FALSE)
{
}

CSetupTask::~CSetupTask()
{
	if (NULL != m_hThread) {
		::CloseHandle(m_hThread);
	}
}

BOOL CSetupTask::Start()
{
	ATLASSERT(NULL == m_hThread);

	m_fCanceled = FALSE;

	m_hThread = ::CreateThread(
		NULL, 
		0, 
		spThreadProc, 
		this, 
		0, 
		&m_dwThreadID);

	if (NULL == m_hThread) {
		return FALSE;
	}

	return TRUE;
}


DWORD WINAPI 
CSetupTask::spThreadProc(LPVOID lpContext)
{
	CSetupTask* pThis = reinterpret_cast<CSetupTask*>(lpContext);
	DWORD dwRet = pThis->OnTaskStart();
	::ExitThread(dwRet);
	return dwRet;
}

BOOL 
CSetupTask::Cancel()
{
	m_fCanceled = TRUE;
	return TRUE;
}

DWORD
CSetupTask::WaitForStop(DWORD dwWaitTimeout)
{
	return WaitForHandle(m_hThread, dwWaitTimeout);
//	return ::WaitForSingleObject(m_hThread, dwWaitTimeout);
}

BOOL
CSetupTask::HasCanceled()
{
	return m_fCanceled;
}

//
// Initialize Task
//

DWORD 
CSetupInitalize::OnTaskStart()
{
	m_pSetupUI->SetActionText(IDS_INITIALIZE);
	m_pSetupUI->ShowProgressBar();

	m_pSetupUI->InitProgressBar(100);

	m_pSetupUI->SetProgressBar(50);
	::Sleep(100);

	m_pSetupUI->SetProgressBar(100);
	::Sleep(100);

	// msiProc.SetInternalUI()
	m_pSetupUI->NotifyTaskDone();
	return ERROR_SUCCESS;
}


CSetupCheckUpdate::CSetupCheckUpdate(
	ISetupUI* pSetupUI,
	LPCTSTR szUpdateURL,
	CONST NDUPDATE_SYSTEM_INFO& SysInfo) : 
	m_SysInfo(SysInfo),
	CSetupTask(pSetupUI)
{
	HRESULT hr = ::StringCchCopy(
		m_szUpdateURL, 
		MAX_PATH,
        szUpdateURL);

	if (FAILED(hr)) {
		m_szUpdateURL[0] = _T('\0');
	}
}

DWORD
CSetupCheckUpdate::OnTaskStart()
{
	CDownloadBindStatusCallback* pBSC = 
		new CDownloadBindStatusCallback(m_pSetupUI);

	NDUPDATE_UPDATE_INFO updateinfo = {0};

	CString strURL;
	BOOL fSuccess = strURL.LoadString(IDS_UPDATE_URL);
	if (!fSuccess) {
		m_pSetupUI->NotifyTaskDone();
		pBSC->Release();
		return ERROR_SUCCESS;
	}

	while (TRUE) {

		fSuccess = ::NdasUpdateGetUpdateInfo(
			pBSC, 
			m_szUpdateURL,
			&m_SysInfo, 
			&updateinfo);

		DWORD dwError = ::GetLastError();
		if (!fSuccess) {
			INT_PTR iResponse = m_pSetupUI->PostErrorMessageBox(
				dwError,
				IDS_ERR_CHECK_UPDATE, 
				MB_ICONERROR | MB_ABORTRETRYIGNORE);

			if (IDABORT == iResponse) {
				m_pSetupUI->NotifyFatalExit(ERROR_INSTALL_USEREXIT);
			} else if (IDIGNORE == iResponse) {
				m_pSetupUI->NotifyTaskDone();
				pBSC->Release();
				return ERROR_SUCCESS;
			}
			continue;
		}
		break;
	}

	if (!updateinfo.fNeedUpdate) {
		m_pSetupUI->NotifyTaskDone();
		pBSC->Release();
		return ERROR_SUCCESS;
	}

	CString str, strVersion;
	strVersion.Format(
		_T("%d.%d.%d"), 
		updateinfo.ProductVersion.wMajor,
		updateinfo.ProductVersion.wMinor,
		updateinfo.ProductVersion.wBuild);

	str.FormatMessage(IDS_UPDATE_AVAILABLE_FMT, (LPCTSTR) strVersion);

	INT_PTR iResponse = m_pSetupUI->PostMessageBox(
		str, 
		MB_YESNO | MB_ICONQUESTION);

	if (IDYES != iResponse) {
		m_pSetupUI->NotifyTaskDone();
		pBSC->Release();
		return ERROR_SUCCESS;
	}

	CString strFilter;
	fSuccess = strFilter.LoadString(IDS_EXE_FILES);
	ATLASSERT(fSuccess);

	TCHAR szFilter[MAX_PATH] = _T("Executable Files\0*.exe\0\0");
	//HRESULT hr = CopyMemory(
	//	szFilter, 
	//	MAX_PATH,
	//	_T("%s\0*.exe\0\0"), 
	//	(LPCTSTR)strFilter);
	//ATLASSERT(SUCCEEDED(hr));

	WTL::CFileDialog dlgSave(
		FALSE, 
		_T("*.exe"), 
		updateinfo.szUpdateFileName,
		OFN_OVERWRITEPROMPT, 
		szFilter,
		NULL);

	while (TRUE) {

		//
		// Save As
		//

		iResponse = dlgSave.DoModal(
			m_pSetupUI->GetCurrentWindow());

		if (IDOK != iResponse) {
			m_pSetupUI->NotifyTaskDone();
			pBSC->Release();
			return ERROR_INSTALL_USEREXIT;
		}

		HRESULT hr = ::URLDownloadToFile(
			NULL, 
			updateinfo.szUpdateFileURL,
			dlgSave.m_szFileName,
			0,
			pBSC);

		if (m_pSetupUI->HasUserCanceled()) {
			m_pSetupUI->NotifyTaskDone();
			pBSC->Release();
			return ERROR_INSTALL_USEREXIT;
		}

		if (FAILED(hr)) {

			INT_PTR iResponse = m_pSetupUI->PostErrorMessageBox(
				hr, 
				IDS_ERR_DOWNLOAD_UPDATE, 
				MB_ABORTRETRYIGNORE | MB_ICONERROR);

			if (IDABORT == iResponse) {
				m_pSetupUI->NotifyFatalExit(hr);
				pBSC->Release();
				return ERROR_INSTALL_USEREXIT;
			} else if (IDRETRY == iResponse) {
				continue;
			} else if (IDIGNORE == iResponse) {
				m_pSetupUI->NotifyTaskDone();
				pBSC->Release();
				return ERROR_SUCCESS;
			}
		}

		break;
	}

	pBSC->Release();
	m_pSetupUI->SetPostExecuteFile(dlgSave.m_szFileName);
	m_pSetupUI->NotifyTaskDone(TASKRET_EXIT);
	return ERROR_SUCCESS;

}

static CONST TCHAR MSI_UPDATER_WINNT[] = _T("instmsiw.exe");
static CONST ULONG MSI_UPDATER_VER = 200;

DWORD
CSetupUpgradeMsi::OnTaskStart()
{
	TCHAR szModuleName[MAX_PATH];
	DWORD nChars = ::GetModuleFileName(NULL, szModuleName, MAX_PATH);
	if (0 == nChars) {
		DWORD dwError = GetLastError();
		m_pSetupUI->PostErrorMessageBox(dwError, IDS_ERR_OUTOFMEM);
		m_pSetupUI->NotifyFatalExit(dwError);
		return ERROR_INSTALL_FAILURE;
	}

	m_pSetupUI->SetActionText(IDS_ACTION_UPGRADE_MSI);
	m_pSetupUI->InitProgressBar(100);

	UINT uiRet = UpgradeMsi(
		m_pSetupUI, 
		szModuleName, 
		MSI_UPDATER_WINNT, 
		MSI_UPDATER_VER);

	if (ERROR_SUCCESS != uiRet &&
		ERROR_SUCCESS_REBOOT_REQUIRED != uiRet) 
	{
		m_pSetupUI->NotifyFatalExit(uiRet);
		return ERROR_INSTALL_FAILURE;
	}

	m_pSetupUI->NotifyTaskDone();
	m_pSetupUI->SetActionText(_T(""));
	return ERROR_SUCCESS;
}

