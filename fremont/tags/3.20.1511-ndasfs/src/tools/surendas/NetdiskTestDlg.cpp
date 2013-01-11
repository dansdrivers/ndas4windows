// NetdiskTestDlg.cpp : implementation file
//

#include "stdafx.h"
#include "NetdiskTest.h"
#include "NetdiskTestDlg.h"
//#include "PnpModule.h"
//#include "UpdateModule.h"

extern HANDLE			hUpdateEvent;
extern UINT				uiLot;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CPnpModule				PnpModule;
extern CUpdateModule			UpdateModule;

BOOL bDlgInitialized = FALSE;
HICON hIconGreen = NULL;
HICON hIconRed = NULL;
HICON hIconWhite = NULL;
HICON hIconYellow = NULL;

int iTestSize = 0;		// Default : 5 MB

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestDlg dialog

CNetdiskTestDlg::CNetdiskTestDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CNetdiskTestDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CNetdiskTestDlg)
	m_pass0 = _T("");
	m_pass1 = _T("");
	m_pass2 = _T("");
	m_pass3 = _T("");
	m_pass4 = _T("");
	m_pass5 = _T("");
	m_pass6 = _T("");
	m_pass7 = _T("");
	m_seq0 = _T("");
	m_seq1 = _T("");
	m_seq2 = _T("");
	m_seq3 = _T("");
	m_seq4 = _T("");
	m_seq5 = _T("");
	m_seq6 = _T("");
	m_seq7 = _T("");
	m_eth0 = _T("");
	m_eth1 = _T("");
	m_eth2 = _T("");
	m_eth3 = _T("");
	m_eth4 = _T("");
	m_eth5 = _T("");
	m_eth6 = _T("");
	m_eth7 = _T("");
	m_lot = _T("0");
	m_pass02 = _T("");
	m_pass03 = _T("");
	m_pass12 = _T("");
	m_pass13 = _T("");
	m_pass22 = _T("");
	m_pass23 = _T("");
	m_pass32 = _T("");
	m_pass33 = _T("");
	m_pass42 = _T("");
	m_pass43 = _T("");
	m_pass52 = _T("");
	m_pass53 = _T("");
	m_pass62 = _T("");
	m_pass63 = _T("");
	m_pass72 = _T("");
	m_pass73 = _T("");
	m_strDBG = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CNetdiskTestDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CNetdiskTestDlg)
	DDX_Control(pDX, IDC_STAT_73, m_ctrlStat73);
	DDX_Control(pDX, IDC_STAT_72, m_ctrlStat72);
	DDX_Control(pDX, IDC_STAT_71, m_ctrlStat71);
	DDX_Control(pDX, IDC_STAT_63, m_ctrlStat63);
	DDX_Control(pDX, IDC_STAT_62, m_ctrlStat62);
	DDX_Control(pDX, IDC_STAT_61, m_ctrlStat61);
	DDX_Control(pDX, IDC_STAT_53, m_ctrlStat53);
	DDX_Control(pDX, IDC_STAT_52, m_ctrlStat52);
	DDX_Control(pDX, IDC_STAT_51, m_ctrlStat51);
	DDX_Control(pDX, IDC_STAT_43, m_ctrlStat43);
	DDX_Control(pDX, IDC_STAT_42, m_ctrlStat42);
	DDX_Control(pDX, IDC_STAT_41, m_ctrlStat41);
	DDX_Control(pDX, IDC_STAT_33, m_ctrlStat33);
	DDX_Control(pDX, IDC_STAT_32, m_ctrlStat32);
	DDX_Control(pDX, IDC_STAT_31, m_ctrlStat31);
	DDX_Control(pDX, IDC_STAT_23, m_ctrlStat23);
	DDX_Control(pDX, IDC_STAT_22, m_ctrlStat22);
	DDX_Control(pDX, IDC_STAT_21, m_ctrlStat21);
	DDX_Control(pDX, IDC_STAT_03, m_ctrlStat03);
	DDX_Control(pDX, IDC_STAT_02, m_ctrlStat02);
	DDX_Control(pDX, IDC_STAT_01, m_ctrlStat01);
	DDX_Control(pDX, IDC_STAT_13, m_ctrlStat13);
	DDX_Control(pDX, IDC_STAT_12, m_ctrlStat12);
	DDX_Control(pDX, IDC_STAT_11, m_ctrlStat11);
	DDX_Text(pDX, IDC_EDIT_PASS0, m_pass0);
	DDX_Text(pDX, IDC_EDIT_PASS1, m_pass1);
	DDX_Text(pDX, IDC_EDIT_PASS2, m_pass2);
	DDX_Text(pDX, IDC_EDIT_PASS3, m_pass3);
	DDX_Text(pDX, IDC_EDIT_PASS4, m_pass4);
	DDX_Text(pDX, IDC_EDIT_PASS5, m_pass5);
	DDX_Text(pDX, IDC_EDIT_PASS6, m_pass6);
	DDX_Text(pDX, IDC_EDIT_PASS7, m_pass7);
	DDX_Text(pDX, IDC_EDIT_SEQ0, m_seq0);
	DDX_Text(pDX, IDC_EDIT_SEQ1, m_seq1);
	DDX_Text(pDX, IDC_EDIT_SEQ2, m_seq2);
	DDX_Text(pDX, IDC_EDIT_SEQ3, m_seq3);
	DDX_Text(pDX, IDC_EDIT_SEQ4, m_seq4);
	DDX_Text(pDX, IDC_EDIT_SEQ5, m_seq5);
	DDX_Text(pDX, IDC_EDIT_SEQ6, m_seq6);
	DDX_Text(pDX, IDC_EDIT_SEQ7, m_seq7);
	DDX_Text(pDX, IDC_EDIT_ETH0, m_eth0);
	DDX_Text(pDX, IDC_EDIT_ETH1, m_eth1);
	DDX_Text(pDX, IDC_EDIT_ETH2, m_eth2);
	DDX_Text(pDX, IDC_EDIT_ETH3, m_eth3);
	DDX_Text(pDX, IDC_EDIT_ETH4, m_eth4);
	DDX_Text(pDX, IDC_EDIT_ETH5, m_eth5);
	DDX_Text(pDX, IDC_EDIT_ETH6, m_eth6);
	DDX_Text(pDX, IDC_EDIT_ETH7, m_eth7);
	DDX_Text(pDX, IDC_EDIT1_LOT, m_lot);
	DDV_MaxChars(pDX, m_lot, 3);
	DDX_Text(pDX, IDC_EDIT_PASS02, m_pass02);
	DDX_Text(pDX, IDC_EDIT_PASS03, m_pass03);
	DDX_Text(pDX, IDC_EDIT_PASS12, m_pass12);
	DDX_Text(pDX, IDC_EDIT_PASS13, m_pass13);
	DDX_Text(pDX, IDC_EDIT_PASS22, m_pass22);
	DDX_Text(pDX, IDC_EDIT_PASS23, m_pass23);
	DDX_Text(pDX, IDC_EDIT_PASS32, m_pass32);
	DDX_Text(pDX, IDC_EDIT_PASS33, m_pass33);
	DDX_Text(pDX, IDC_EDIT_PASS42, m_pass42);
	DDX_Text(pDX, IDC_EDIT_PASS43, m_pass43);
	DDX_Text(pDX, IDC_EDIT_PASS52, m_pass52);
	DDX_Text(pDX, IDC_EDIT_PASS53, m_pass53);
	DDX_Text(pDX, IDC_EDIT_PASS62, m_pass62);
	DDX_Text(pDX, IDC_EDIT_PASS63, m_pass63);
	DDX_Text(pDX, IDC_EDIT_PASS72, m_pass72);
	DDX_Text(pDX, IDC_EDIT_PASS73, m_pass73);
	DDX_Text(pDX, IDC_EDIT_DBG, m_strDBG);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CNetdiskTestDlg, CDialog)
	//{{AFX_MSG_MAP(CNetdiskTestDlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_REFRESH, OnButtonRefresh)
	ON_EN_CHANGE(IDC_EDIT1_LOT, OnChangeEdit1Lot)
	ON_BN_CLICKED(IDC_RADIO1, OnRadio1)
	ON_BN_CLICKED(IDC_RADIO2, OnRadio2)
	ON_BN_CLICKED(IDC_RADIO3, OnRadio3)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestDlg message handlers

BOOL CNetdiskTestDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
	//m_ctrlStat01.SetIcon( (HICON)::LoadImage(AfxGetInstanceHandle(),  MAKEINTRESOURCE(IDI_CIRCLE_RED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR) );
#ifndef _DEBUG
	AfxGetMainWnd()->SetWindowPos(&wndTop, 0, 0, 730, 390, SWP_SHOWWINDOW | SWP_NOMOVE);
#endif
	hIconRed = (HICON)::LoadImage(AfxGetInstanceHandle(),  MAKEINTRESOURCE(IDI_CIRCLE_RED), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	hIconGreen = (HICON)::LoadImage(AfxGetInstanceHandle(),  MAKEINTRESOURCE(IDI_CIRCLE_GREEN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	hIconWhite = (HICON)::LoadImage(AfxGetInstanceHandle(),  MAKEINTRESOURCE(IDI_CIRCLE_WHITE), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	hIconYellow = (HICON)::LoadImage(AfxGetInstanceHandle(),  MAKEINTRESOURCE(IDI_CIRCLE_YELLOW), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	GetDlgItem(IDC_STATIC_T1)->SetWindowPos(&wndTop,   7,  20, 350,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_T2)->SetWindowPos(&wndTop, 360,  20, 350,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_STATIC_N0)->SetWindowPos(&wndTop,  20,  70,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N1)->SetWindowPos(&wndTop,  20, 130,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N2)->SetWindowPos(&wndTop,  20, 190,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N3)->SetWindowPos(&wndTop,  20, 250,  60,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_STATIC_N4)->SetWindowPos(&wndTop, 375,  70,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N5)->SetWindowPos(&wndTop, 375, 130,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N6)->SetWindowPos(&wndTop, 375, 190,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_STATIC_N7)->SetWindowPos(&wndTop, 375, 250,  60,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_EDIT_SEQ0)->SetWindowPos(&wndTop, 100,  70, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ1)->SetWindowPos(&wndTop, 100, 130, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ2)->SetWindowPos(&wndTop, 100, 190, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ3)->SetWindowPos(&wndTop, 100, 250, 120,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_EDIT_SEQ4)->SetWindowPos(&wndTop, 455,  70, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ5)->SetWindowPos(&wndTop, 455, 130, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ6)->SetWindowPos(&wndTop, 455, 190, 120,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_SEQ7)->SetWindowPos(&wndTop, 455, 250, 120,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_EDIT_ETH0)->SetWindowPos(&wndTop,  20, 100, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH1)->SetWindowPos(&wndTop,  20, 160, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH2)->SetWindowPos(&wndTop,  20, 220, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH3)->SetWindowPos(&wndTop,  20, 280, 200,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_EDIT_ETH4)->SetWindowPos(&wndTop, 375, 100, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH5)->SetWindowPos(&wndTop, 375, 160, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH6)->SetWindowPos(&wndTop, 375, 220, 200,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_EDIT_ETH7)->SetWindowPos(&wndTop, 375, 280, 200,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_GRP_LEFT)-> SetWindowPos(&wndTop,   7,  50, 350, 255, SWP_SHOWWINDOW);
	GetDlgItem(IDC_GRP_RIGHT)->SetWindowPos(&wndTop, 360,  50, 360, 255, SWP_SHOWWINDOW);

	GetDlgItem(IDC_RADIO1)->   SetWindowPos(&wndTop,  10, 320,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_RADIO2)->   SetWindowPos(&wndTop,  90, 320,  60,  20, SWP_SHOWWINDOW);
	GetDlgItem(IDC_RADIO3)->   SetWindowPos(&wndTop, 170, 320,  60,  20, SWP_SHOWWINDOW);

	GetDlgItem(IDC_BUTTON_REFRESH)->SetWindowPos(&wndTop, 300, 320, 120, 25, SWP_SHOWWINDOW);

	m_ctrlStat01.SetIcon(hIconWhite);
	m_ctrlStat02.SetIcon(hIconWhite);
	m_ctrlStat03.SetIcon(hIconWhite);

	m_ctrlStat01.SetWindowPos(&wndTop, 220,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat02.SetWindowPos(&wndTop, 260,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat03.SetWindowPos(&wndTop, 300,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat11.SetIcon(hIconWhite);
	m_ctrlStat12.SetIcon(hIconWhite);
	m_ctrlStat13.SetIcon(hIconWhite);

	m_ctrlStat11.SetWindowPos(&wndTop, 220, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat12.SetWindowPos(&wndTop, 260, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat13.SetWindowPos(&wndTop, 300, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat21.SetIcon(hIconWhite);
	m_ctrlStat22.SetIcon(hIconWhite);
	m_ctrlStat23.SetIcon(hIconWhite);

	m_ctrlStat21.SetWindowPos(&wndTop, 220, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat22.SetWindowPos(&wndTop, 260, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat23.SetWindowPos(&wndTop, 300, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat31.SetIcon(hIconWhite);
	m_ctrlStat32.SetIcon(hIconWhite);
	m_ctrlStat33.SetIcon(hIconWhite);

	m_ctrlStat31.SetWindowPos(&wndTop, 220, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat32.SetWindowPos(&wndTop, 260, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat33.SetWindowPos(&wndTop, 300, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat41.SetIcon(hIconWhite);
	m_ctrlStat42.SetIcon(hIconWhite);
	m_ctrlStat43.SetIcon(hIconWhite);

	m_ctrlStat41.SetWindowPos(&wndTop, 580,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat42.SetWindowPos(&wndTop, 620,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat43.SetWindowPos(&wndTop, 660,  66, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat51.SetIcon(hIconWhite);
	m_ctrlStat52.SetIcon(hIconWhite);
	m_ctrlStat53.SetIcon(hIconWhite);

	m_ctrlStat51.SetWindowPos(&wndTop, 580, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat52.SetWindowPos(&wndTop, 620, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat53.SetWindowPos(&wndTop, 660, 126, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat61.SetIcon(hIconWhite);
	m_ctrlStat62.SetIcon(hIconWhite);
	m_ctrlStat63.SetIcon(hIconWhite);

	m_ctrlStat61.SetWindowPos(&wndTop, 580, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat62.SetWindowPos(&wndTop, 620, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat63.SetWindowPos(&wndTop, 660, 186, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	
	m_ctrlStat71.SetIcon(hIconWhite);
	m_ctrlStat72.SetIcon(hIconWhite);
	m_ctrlStat73.SetIcon(hIconWhite);

	m_ctrlStat71.SetWindowPos(&wndTop, 580, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat72.SetWindowPos(&wndTop, 620, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
	m_ctrlStat73.SetWindowPos(&wndTop, 660, 246, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);

	bDlgInitialized = TRUE;

	// Init PnpModule & start Thread
	if(PnpModule.Initialize(10002, this) == FALSE) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: PnpModule Initialization Failed\n")));
		exit(0);
		return FALSE;
	}

	// Init Updatemodule & start Thread
	if(UpdateModule.Initialize(this) == FALSE) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: UpdateModule Initialization Failed\n")));
		exit(0);
		return FALSE;
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CNetdiskTestDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CNetdiskTestDlg::OnPaint() 
{
	UpdateData(FALSE);
	//UpdateWindow();
	
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CNetdiskTestDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CNetdiskTestDlg::OnButtonRefresh() 
{
	// TODO: Add your control notification handler code here
	DebugPrint(0, ("Button Refresh : Event Set\n"));
	SetEvent(hUpdateEvent);
}

void CNetdiskTestDlg::OnChangeEdit1Lot() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.

	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	uiLot = (UINT) atoi(m_lot.GetBuffer(3));
}

BOOL CNetdiskTestDlg::PreTranslateMessage(MSG* pMsg) 
{
	switch(pMsg->message) {
		case WM_KEYDOWN:
			// Ignore RETURN key.
			if(pMsg->wParam == VK_RETURN)
				return TRUE;
		break ;
	}

	return CDialog::PreTranslateMessage(pMsg);
}

void CNetdiskTestDlg::OnRadio1() 
{
	// TODO: Add your control notification handler code here
	iTestSize = 1;
}

void CNetdiskTestDlg::OnRadio2() 
{
	// TODO: Add your control notification handler code here
	iTestSize = 5;
}

void CNetdiskTestDlg::OnRadio3() 
{
	// TODO: Add your control notification handler code here
	iTestSize = 10;
}
