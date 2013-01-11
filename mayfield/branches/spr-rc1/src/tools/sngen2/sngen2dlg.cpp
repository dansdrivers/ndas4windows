// SnGen2Dlg.cpp : implementation file
//

#include	<windows.h>
#include	"stdafx.h"
#include	"SnGen2.h"
#include	"SnGen2Dlg.h"
#include <ndas/ndasid.h>
#include <ndas/ndasenc.h>
//#include	"../inc/Serial.h"
//#include	"../inc/Key.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

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
// CSnGen2Dlg dialog

CSnGen2Dlg::CSnGen2Dlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSnGen2Dlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSnGen2Dlg)
	m_addr0 = _T("");
	m_addr1 = _T("");
	m_addr2 = _T("");
	m_addr3 = _T("");
	m_addr4 = _T("");
	m_addr5 = _T("");
	m_key00 = _T(NDKEY10);
	m_key01 = _T(NDKEY11);
	m_key02 = _T(NDKEY12);
	m_key03 = _T(NDKEY13);
	m_key04 = _T(NDKEY14);
	m_key05 = _T(NDKEY15);
	m_key06 = _T(NDKEY16);
	m_key07 = _T(NDKEY17);
	m_key10 = _T(NDKEY20);
	m_key11 = _T(NDKEY21);
	m_key12 = _T(NDKEY22);
	m_key13 = _T(NDKEY23);
	m_key14 = _T(NDKEY24);
	m_key15 = _T(NDKEY25);
	m_key16 = _T(NDKEY26);
	m_key17 = _T(NDKEY27);
	m_random = _T(NDRANDOM);
	m_reserved0 = _T(NDRESERVED0);
	m_reserved1 = _T(NDRESERVED1);
	m_serial0 = _T("");
	m_serial1 = _T("");
	m_serial2 = _T("");
	m_serial3 = _T("");
	m_vid = _T(NDVID);
	m_write_key = _T("");
	m_writable = _T("");
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSnGen2Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSnGen2Dlg)
	DDX_Text(pDX, IDC_EDIT_ADDR0, m_addr0);
	DDV_MaxChars(pDX, m_addr0, 2);
	DDX_Text(pDX, IDC_EDIT_ADDR1, m_addr1);
	DDV_MaxChars(pDX, m_addr1, 2);
	DDX_Text(pDX, IDC_EDIT_ADDR2, m_addr2);
	DDV_MaxChars(pDX, m_addr2, 2);
	DDX_Text(pDX, IDC_EDIT_ADDR3, m_addr3);
	DDV_MaxChars(pDX, m_addr3, 2);
	DDX_Text(pDX, IDC_EDIT_ADDR4, m_addr4);
	DDV_MaxChars(pDX, m_addr4, 2);
	DDX_Text(pDX, IDC_EDIT_ADDR5, m_addr5);
	DDV_MaxChars(pDX, m_addr5, 2);
	DDX_Text(pDX, IDC_EDIT_Key00, m_key00);
	DDV_MaxChars(pDX, m_key00, 2);
	DDX_Text(pDX, IDC_EDIT_Key01, m_key01);
	DDV_MaxChars(pDX, m_key01, 2);
	DDX_Text(pDX, IDC_EDIT_Key02, m_key02);
	DDV_MaxChars(pDX, m_key02, 2);
	DDX_Text(pDX, IDC_EDIT_Key03, m_key03);
	DDV_MaxChars(pDX, m_key03, 2);
	DDX_Text(pDX, IDC_EDIT_Key04, m_key04);
	DDX_Text(pDX, IDC_EDIT_Key05, m_key05);
	DDV_MaxChars(pDX, m_key05, 2);
	DDX_Text(pDX, IDC_EDIT_Key06, m_key06);
	DDV_MaxChars(pDX, m_key06, 2);
	DDX_Text(pDX, IDC_EDIT_Key07, m_key07);
	DDV_MaxChars(pDX, m_key07, 2);
	DDX_Text(pDX, IDC_EDIT_Key10, m_key10);
	DDV_MaxChars(pDX, m_key10, 2);
	DDX_Text(pDX, IDC_EDIT_Key11, m_key11);
	DDV_MaxChars(pDX, m_key11, 2);
	DDX_Text(pDX, IDC_EDIT_Key12, m_key12);
	DDV_MaxChars(pDX, m_key12, 2);
	DDX_Text(pDX, IDC_EDIT_Key13, m_key13);
	DDV_MaxChars(pDX, m_key13, 2);
	DDX_Text(pDX, IDC_EDIT_Key14, m_key14);
	DDV_MaxChars(pDX, m_key14, 2);
	DDX_Text(pDX, IDC_EDIT_Key15, m_key15);
	DDV_MaxChars(pDX, m_key15, 2);
	DDX_Text(pDX, IDC_EDIT_Key16, m_key16);
	DDV_MaxChars(pDX, m_key16, 2);
	DDX_Text(pDX, IDC_EDIT_Key17, m_key17);
	DDV_MaxChars(pDX, m_key17, 2);
	DDX_Text(pDX, IDC_EDIT_RANDOM, m_random);
	DDV_MaxChars(pDX, m_random, 2);
	DDX_Text(pDX, IDC_EDIT_RESERVED0, m_reserved0);
	DDV_MaxChars(pDX, m_reserved0, 2);
	DDX_Text(pDX, IDC_EDIT_RESERVED1, m_reserved1);
	DDV_MaxChars(pDX, m_reserved1, 2);
	DDX_Text(pDX, IDC_EDIT_SERIAL0, m_serial0);
	DDV_MaxChars(pDX, m_serial0, 5);
	DDX_Text(pDX, IDC_EDIT_SERIAL1, m_serial1);
	DDV_MaxChars(pDX, m_serial1, 5);
	DDX_Text(pDX, IDC_EDIT_SERIAL2, m_serial2);
	DDV_MaxChars(pDX, m_serial2, 5);
	DDX_Text(pDX, IDC_EDIT_SERIAL3, m_serial3);
	DDV_MaxChars(pDX, m_serial3, 5);
	DDX_Text(pDX, IDC_EDIT_VID, m_vid);
	DDV_MaxChars(pDX, m_vid, 2);
	DDX_Text(pDX, IDC_EDIT_WRITE_KEY, m_write_key);
	DDV_MaxChars(pDX, m_write_key, 5);
	DDX_Text(pDX, IDC_EDIT_WRITABLE, m_writable);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CSnGen2Dlg, CDialog)
	//{{AFX_MSG_MAP(CSnGen2Dlg)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_EN_CHANGE(IDC_EDIT_ADDR0, OnChangeEditAddr0)
	ON_EN_CHANGE(IDC_EDIT_ADDR1, OnChangeEditAddr1)
	ON_EN_CHANGE(IDC_EDIT_ADDR2, OnChangeEditAddr2)
	ON_EN_CHANGE(IDC_EDIT_ADDR3, OnChangeEditAddr3)
	ON_EN_CHANGE(IDC_EDIT_ADDR4, OnChangeEditAddr4)
	ON_EN_CHANGE(IDC_EDIT_ADDR5, OnChangeEditAddr5)
	ON_EN_CHANGE(IDC_EDIT_Key00, OnChangeEDITKey00)
	ON_EN_CHANGE(IDC_EDIT_Key01, OnChangeEDITKey01)
	ON_EN_CHANGE(IDC_EDIT_Key02, OnChangeEDITKey02)
	ON_EN_CHANGE(IDC_EDIT_Key03, OnChangeEDITKey03)
	ON_EN_CHANGE(IDC_EDIT_Key04, OnChangeEDITKey04)
	ON_EN_CHANGE(IDC_EDIT_Key05, OnChangeEDITKey05)
	ON_EN_CHANGE(IDC_EDIT_Key06, OnChangeEDITKey06)
	ON_EN_CHANGE(IDC_EDIT_Key07, OnChangeEDITKey07)
	ON_EN_CHANGE(IDC_EDIT_Key10, OnChangeEDITKey10)
	ON_EN_CHANGE(IDC_EDIT_Key11, OnChangeEDITKey11)
	ON_EN_CHANGE(IDC_EDIT_Key12, OnChangeEDITKey12)
	ON_EN_CHANGE(IDC_EDIT_Key13, OnChangeEDITKey13)
	ON_EN_CHANGE(IDC_EDIT_Key14, OnChangeEDITKey14)
	ON_EN_CHANGE(IDC_EDIT_Key15, OnChangeEDITKey15)
	ON_EN_CHANGE(IDC_EDIT_Key16, OnChangeEDITKey16)
	ON_EN_CHANGE(IDC_EDIT_Key17, OnChangeEDITKey17)
	ON_EN_CHANGE(IDC_EDIT_RANDOM, OnChangeEditRandom)
	ON_EN_CHANGE(IDC_EDIT_RESERVED0, OnChangeEditReserved0)
	ON_EN_CHANGE(IDC_EDIT_RESERVED1, OnChangeEditReserved1)
	ON_EN_CHANGE(IDC_EDIT_VID, OnChangeEditVid)
	ON_EN_CHANGE(IDC_EDIT_SERIAL0, OnChangeEditSerial0)
	ON_EN_CHANGE(IDC_EDIT_SERIAL1, OnChangeEditSerial1)
	ON_EN_CHANGE(IDC_EDIT_SERIAL2, OnChangeEditSerial2)
	ON_EN_CHANGE(IDC_EDIT_SERIAL3, OnChangeEditSerial3)
	ON_EN_CHANGE(IDC_EDIT_WRITE_KEY, OnChangeEditWriteKey)
	ON_BN_CLICKED(IDC_BUTTON_GEN, OnButtonGen)
	ON_BN_CLICKED(IDC_BUTTON_CHECK, OnButtonCheck)
	ON_BN_CLICKED(IDC_BUTTON_COPY_ADDRESS, OnButtonCopyAddress)
	ON_BN_CLICKED(IDC_BUTTON_COPY_ID, OnButtonCopyId)
	ON_BN_CLICKED(IDC_BUTTON_BURST, OnButtonBurst)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSnGen2Dlg message handlers

BOOL CSnGen2Dlg::OnInitDialog()
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
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSnGen2Dlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CSnGen2Dlg::OnPaint() 
{
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
HCURSOR CSnGen2Dlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}

void CheckAndMove2ByteEditBox(CDialog *pDialog, CString &str)
{
	LPSTR	pStr;

	pStr = str.GetBuffer(str.GetLength());

	switch(str.GetLength()) {
	case 1:
		if(!isxdigit(pStr[0])) str.Empty();
		break;
	case 2:
		if(!isxdigit(pStr[1])) str.Delete(1, 1);
		break;
	default:
		break;
	}

	pDialog->UpdateData(FALSE);

	if(str.GetLength() >= 2) {
		pDialog->NextDlgCtrl();
	}

	return;
}

void CSnGen2Dlg::OnChangeEditAddr0() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr0);
}

void CSnGen2Dlg::OnChangeEditAddr1() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr1);
}

void CSnGen2Dlg::OnChangeEditAddr2() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr2);
}

void CSnGen2Dlg::OnChangeEditAddr3() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr3);
}

void CSnGen2Dlg::OnChangeEditAddr4() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr4);
}

void CSnGen2Dlg::OnChangeEditAddr5() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_addr5);
}

void CSnGen2Dlg::OnChangeEDITKey00() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key00);
}

void CSnGen2Dlg::OnChangeEDITKey01() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key01);
}

void CSnGen2Dlg::OnChangeEDITKey02() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key02);
}

void CSnGen2Dlg::OnChangeEDITKey03() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key03);
}

void CSnGen2Dlg::OnChangeEDITKey04() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key04);
}

void CSnGen2Dlg::OnChangeEDITKey05() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key05);
}

void CSnGen2Dlg::OnChangeEDITKey06() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key06);
}

void CSnGen2Dlg::OnChangeEDITKey07() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key07);
}

void CSnGen2Dlg::OnChangeEDITKey10() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key10);
}

void CSnGen2Dlg::OnChangeEDITKey11() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key11);
}

void CSnGen2Dlg::OnChangeEDITKey12() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key12);
}

void CSnGen2Dlg::OnChangeEDITKey13() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key13);
}

void CSnGen2Dlg::OnChangeEDITKey14() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key14);
}

void CSnGen2Dlg::OnChangeEDITKey15() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key15);
}

void CSnGen2Dlg::OnChangeEDITKey16() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key16);
}

void CSnGen2Dlg::OnChangeEDITKey17() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_key17);
}

void CSnGen2Dlg::OnChangeEditRandom() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_random);
}

void CSnGen2Dlg::OnChangeEditReserved0() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_reserved0);
}

void CSnGen2Dlg::OnChangeEditReserved1() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_reserved1);
}

void CSnGen2Dlg::OnChangeEditVid() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove2ByteEditBox(this, m_vid);
}

void CheckAndMove5ByteEditBox(CDialog *pDialog, CString &str)
{
	LPSTR	pStr;

	pStr = str.GetBuffer(str.GetLength());

	switch(str.GetLength()) {
	case 1:
		if(!isalnum(pStr[0])) str.Empty();
		break;
	case 2:
		if(!isalnum(pStr[1])) str.Delete(1, 1);
		break;
	case 3:
		if(!isalnum(pStr[2])) str.Delete(2, 2);
		break;
	case 4:
		if(!isalnum(pStr[3])) str.Delete(3, 3);
		break;
	case 5:
		if(!isalnum(pStr[4])) str.Delete(4, 4);
		break;
	default:
		break;
	}

	pDialog->UpdateData(FALSE);

	if(str.GetLength() >= 5) {
		pDialog->NextDlgCtrl();
	}

	return;
}

void CSnGen2Dlg::OnChangeEditSerial0() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove5ByteEditBox(this, m_serial0);
}

void CSnGen2Dlg::OnChangeEditSerial1() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove5ByteEditBox(this, m_serial1);
}

void CSnGen2Dlg::OnChangeEditSerial2() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove5ByteEditBox(this, m_serial2);
}

void CSnGen2Dlg::OnChangeEditSerial3() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove5ByteEditBox(this, m_serial3);
}

void CSnGen2Dlg::OnChangeEditWriteKey() 
{
	// TODO: If this is a RICHEDIT control, the control will not
	// send this notification unless you override the CDialog::OnInitDialog()
	// function and call CRichEditCtrl().SetEventMask()
	// with the ENM_CHANGE flag ORed into the mask.
	
	// TODO: Add your control notification handler code here
	UpdateData(TRUE);

	CheckAndMove5ByteEditBox(this, m_write_key);
}

void CSnGen2Dlg::OnButtonGen() 
{
	// TODO: Add your control notification handler code here
	SERIAL_INFO		pInfo;
	LPSTR			strTemp;
	PCHAR			stopstring;

	UpdateData(TRUE);

	// Address
	strTemp = m_addr0.GetBuffer(2);
	pInfo.ucAddr[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_addr1.GetBuffer(2);
	pInfo.ucAddr[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_addr2.GetBuffer(2);
	pInfo.ucAddr[2] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_addr3.GetBuffer(2);
	pInfo.ucAddr[3] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_addr4.GetBuffer(2);
	pInfo.ucAddr[4] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_addr5.GetBuffer(2);
	pInfo.ucAddr[5] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Vid
	strTemp = m_vid.GetBuffer(2);
	pInfo.ucVid = (UCHAR) strtol(strTemp, &stopstring, 16);

	// reserved
	strTemp = m_reserved0.GetBuffer(2);
	pInfo.reserved[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_reserved1.GetBuffer(2);
	pInfo.reserved[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Random
	strTemp = m_random.GetBuffer(2);
	pInfo.ucRandom = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Key1
	strTemp = m_key00.GetBuffer(2);
	pInfo.ucKey1[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key01.GetBuffer(2);
	pInfo.ucKey1[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key02.GetBuffer(2);
	pInfo.ucKey1[2] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key03.GetBuffer(2);
	pInfo.ucKey1[3] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key04.GetBuffer(2);
	pInfo.ucKey1[4] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key05.GetBuffer(2);
	pInfo.ucKey1[5] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key06.GetBuffer(2);
	pInfo.ucKey1[6] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key07.GetBuffer(2);
	pInfo.ucKey1[7] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Key2
	strTemp = m_key10.GetBuffer(2);
	pInfo.ucKey2[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key11.GetBuffer(2);
	pInfo.ucKey2[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key12.GetBuffer(2);
	pInfo.ucKey2[2] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key13.GetBuffer(2);
	pInfo.ucKey2[3] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key14.GetBuffer(2);
	pInfo.ucKey2[4] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key15.GetBuffer(2);
	pInfo.ucKey2[5] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key16.GetBuffer(2);
	pInfo.ucKey2[6] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key17.GetBuffer(2);
	pInfo.ucKey2[7] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Now Generate Serial
	EncryptSerial(&pInfo);

	// copy Serial
	strTemp = m_serial0.GetBuffer(5);
	strncpy(strTemp, pInfo.ucSN[0], 5);

	strTemp = m_serial1.GetBuffer(5);
	strncpy(strTemp, pInfo.ucSN[1], 5);

	strTemp = m_serial2.GetBuffer(5);
	strncpy(strTemp, pInfo.ucSN[2], 5);

	strTemp = m_serial3.GetBuffer(5);
	strncpy(strTemp, pInfo.ucSN[3], 5);

	strTemp = m_write_key.GetBuffer(5);
	strncpy(strTemp, pInfo.ucWKey, 5);

	m_writable = CString("Yes");

	UpdateData(FALSE);
}

void CSnGen2Dlg::OnButtonCheck() 
{
	// TODO: Add your control notification handler code here
	LPSTR			strTemp;
	SERIAL_INFO		pInfo;
	PCHAR			stopstring;

	UpdateData(TRUE);

	m_addr0 = m_addr1 = m_addr2 = m_addr3 = m_addr4 = m_addr5
		= m_vid = m_reserved0 = m_reserved1 = m_random = m_writable = CString("");

	// Serial
	strTemp = m_serial0.GetBuffer(5);
	strncpy(pInfo.ucSN[0], strTemp, 5);

	strTemp = m_serial1.GetBuffer(5);
	strncpy(pInfo.ucSN[1], strTemp, 5);

	strTemp = m_serial2.GetBuffer(5);
	strncpy(pInfo.ucSN[2], strTemp, 5);

	strTemp = m_serial3.GetBuffer(5);
	strncpy(pInfo.ucSN[3], strTemp, 5);

	// Write Key
	strTemp = m_write_key.GetBuffer(5);
	strncpy(pInfo.ucWKey, strTemp, 5);

	// Key1
	strTemp = m_key00.GetBuffer(2);
	pInfo.ucKey1[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key01.GetBuffer(2);
	pInfo.ucKey1[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key02.GetBuffer(2);
	pInfo.ucKey1[2] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key03.GetBuffer(2);
	pInfo.ucKey1[3] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key04.GetBuffer(2);
	pInfo.ucKey1[4] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key05.GetBuffer(2);
	pInfo.ucKey1[5] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key06.GetBuffer(2);
	pInfo.ucKey1[6] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key07.GetBuffer(2);
	pInfo.ucKey1[7] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Key2
	strTemp = m_key10.GetBuffer(2);
	pInfo.ucKey2[0] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key11.GetBuffer(2);
	pInfo.ucKey2[1] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key12.GetBuffer(2);
	pInfo.ucKey2[2] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key13.GetBuffer(2);
	pInfo.ucKey2[3] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key14.GetBuffer(2);
	pInfo.ucKey2[4] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key15.GetBuffer(2);
	pInfo.ucKey2[5] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key16.GetBuffer(2);
	pInfo.ucKey2[6] = (UCHAR) strtol(strTemp, &stopstring, 16);

	strTemp = m_key17.GetBuffer(2);
	pInfo.ucKey2[7] = (UCHAR) strtol(strTemp, &stopstring, 16);

	// Now, decrypt Password 
	if(!DecryptSerial(&pInfo)) {
		UpdateData(FALSE);
		AfxMessageBox("Invalid Serial !!");
		return;
	}

	// Address
	strTemp = m_addr0.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[0]);

	strTemp = m_addr1.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[1]);

	strTemp = m_addr2.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[2]);

	strTemp = m_addr3.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[3]);

	strTemp = m_addr4.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[4]);

	strTemp = m_addr5.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucAddr[5]);

	// Vid
	strTemp = m_vid.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucVid);

	// reserved
	strTemp = m_reserved0.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.reserved[0]);

	strTemp = m_reserved1.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.reserved[1]);

	// random
	strTemp = m_random.GetBuffer(2);
	sprintf(strTemp, "%.2X", pInfo.ucRandom);
	
	// writable ?
	m_writable = pInfo.bIsReadWrite ? CString("Yes") : CString("No");

	UpdateData(FALSE);
}

void CSnGen2Dlg::OnButtonCopyAddress() 
{
	// TODO: Add your control notification handler code here
	
    LPTSTR  lptstrCopy; 
    HGLOBAL hglbCopy; 

	UpdateData(TRUE);

	if(!OpenClipboard())
		return;
	
	EmptyClipboard();
	
	hglbCopy = GlobalAlloc(GMEM_MOVEABLE, 18); 
	if (hglbCopy == NULL) 
	{ 
		CloseClipboard(); 
		return; 
	} 
	
	// Lock the handle and copy the text to the buffer. 
	
	lptstrCopy = (char *)GlobalLock(hglbCopy); 
	sprintf(lptstrCopy, "%s:%s:%s:%s:%s:%s",
		m_addr0, m_addr1, m_addr2, m_addr3, m_addr4, m_addr5);

	GlobalUnlock(hglbCopy); 
	
	// Place the handle on the clipboard. 
	
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

void CSnGen2Dlg::OnButtonCopyId() 
{
	// TODO: Add your control notification handler code here
    LPTSTR  lptstrCopy; 
    HGLOBAL hglbCopy; 

	UpdateData(TRUE);

	if(!OpenClipboard())
		return;
	
	EmptyClipboard();
	
	hglbCopy = GlobalAlloc(GMEM_MOVEABLE, 30); 
	if (hglbCopy == NULL) 
	{ 
		CloseClipboard(); 
		return; 
	} 
	
	// Lock the handle and copy the text to the buffer. 
	
	lptstrCopy = (char *)GlobalLock(hglbCopy); 
	sprintf(lptstrCopy, "%s-%s-%s-%s-%s",
		m_serial0, m_serial1, m_serial2, m_serial3, m_write_key);

	GlobalUnlock(hglbCopy); 
	
	// Place the handle on the clipboard. 
	
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();

}

void CSnGen2Dlg::OnButtonBurst() 
{
	// TODO: Add your control notification handler code here

	OnButtonCheck();
	OnButtonGen();

    LPTSTR  lptstrCopy; 
    HGLOBAL hglbCopy; 

	UpdateData(TRUE);

	if(!OpenClipboard())
		return;
	
	EmptyClipboard();
	
	hglbCopy = GlobalAlloc(GMEM_MOVEABLE, 18 + 30); 
	if (hglbCopy == NULL) 
	{ 
		CloseClipboard(); 
		return; 
	} 
	
	// Lock the handle and copy the text to the buffer. 
	
	lptstrCopy = (char *)GlobalLock(hglbCopy); 
	sprintf(lptstrCopy, "%s-%s-%s-%s-%s %s:%s:%s:%s:%s:%s",
		m_serial0, m_serial1, m_serial2, m_serial3, m_write_key,
		m_addr0, m_addr1, m_addr2, m_addr3, m_addr4, m_addr5);

	GlobalUnlock(hglbCopy); 
	
	// Place the handle on the clipboard. 
	
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}
