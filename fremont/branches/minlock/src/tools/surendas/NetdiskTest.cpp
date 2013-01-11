// NetdiskTest.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "NetdiskTest.h"
#include "NetdiskTestDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CPnpModule				PnpModule;
CUpdateModule			UpdateModule;

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestApp

BEGIN_MESSAGE_MAP(CNetdiskTestApp, CWinApp)
	//{{AFX_MSG_MAP(CNetdiskTestApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG
	ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestApp construction

CNetdiskTestApp::CNetdiskTestApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CNetdiskTestApp object

CNetdiskTestApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestApp initialization

BOOL CNetdiskTestApp::InitInstance()
{
	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Initialize Test - Create Events and initialize it
	if(TestInitialize() == FALSE) {
		AfxMessageBox("Cannot Initialize Program\n(Event Creation or Socket Initialization Error)", MB_OK, 0);
		return FALSE;
	}

	if(!NdasCommInitialize()) {
		AfxMessageBox(1, (TEXT("Cannot Initialize NDASCOMM\n %d\n"), GetLastError()));
		return FALSE;
	}

	/*
	// Init PnpModule & start Thread
	if(PnpModule.Initialize(10002) == FALSE) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: PnpModule Initialization Failed\n")));
		return FALSE;
	}

	// Init Updatemodule & start Thread
	if(UpdateModule.Initialize() == FALSE) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: UpdateModule Initialization Failed\n")));
		return FALSE;
	}
	*/

	CNetdiskTestDlg dlg;
	m_pMainWnd = &dlg;

	// Init Updatemodule & start Thread
//	if(UpdateModule.Initialize(&dlg) == FALSE) {
//		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: UpdateModule Initialization Failed\n")));
//		return FALSE;
//	}

	int nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	if(!NdasCommUninitialize()) {
		AfxMessageBox(1, (TEXT("Cannot Uninitialize NDASCOMM\n %d\n"), GetLastError()));
		return FALSE;
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	WSACleanup();
	return FALSE;
}
