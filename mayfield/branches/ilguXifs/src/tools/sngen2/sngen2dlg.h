// SnGen2Dlg.h : header file
//

#if !defined(AFX_SNGEN2DLG_H__415B08FA_7259_4761_B3AD_058C468DA5E2__INCLUDED_)
#define AFX_SNGEN2DLG_H__415B08FA_7259_4761_B3AD_058C468DA5E2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CSnGen2Dlg dialog

class CSnGen2Dlg : public CDialog
{
// Construction
public:
	CSnGen2Dlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CSnGen2Dlg)
	enum { IDD = IDD_SNGEN2_DIALOG };
	CString	m_addr0;
	CString	m_addr1;
	CString	m_addr2;
	CString	m_addr3;
	CString	m_addr4;
	CString	m_addr5;
	CString	m_key00;
	CString	m_key01;
	CString	m_key02;
	CString	m_key03;
	CString	m_key04;
	CString	m_key05;
	CString	m_key06;
	CString	m_key07;
	CString	m_key10;
	CString	m_key11;
	CString	m_key12;
	CString	m_key13;
	CString	m_key14;
	CString	m_key15;
	CString	m_key16;
	CString	m_key17;
	CString	m_random;
	CString	m_reserved0;
	CString	m_reserved1;
	CString	m_serial0;
	CString	m_serial1;
	CString	m_serial2;
	CString	m_serial3;
	CString	m_vid;
	CString	m_write_key;
	CString	m_writable;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSnGen2Dlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CSnGen2Dlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnChangeEditAddr0();
	afx_msg void OnChangeEditAddr1();
	afx_msg void OnChangeEditAddr2();
	afx_msg void OnChangeEditAddr3();
	afx_msg void OnChangeEditAddr4();
	afx_msg void OnChangeEditAddr5();
	afx_msg void OnChangeEDITKey00();
	afx_msg void OnChangeEDITKey01();
	afx_msg void OnChangeEDITKey02();
	afx_msg void OnChangeEDITKey03();
	afx_msg void OnChangeEDITKey04();
	afx_msg void OnChangeEDITKey05();
	afx_msg void OnChangeEDITKey06();
	afx_msg void OnChangeEDITKey07();
	afx_msg void OnChangeEDITKey10();
	afx_msg void OnChangeEDITKey11();
	afx_msg void OnChangeEDITKey12();
	afx_msg void OnChangeEDITKey13();
	afx_msg void OnChangeEDITKey14();
	afx_msg void OnChangeEDITKey15();
	afx_msg void OnChangeEDITKey16();
	afx_msg void OnChangeEDITKey17();
	afx_msg void OnChangeEditRandom();
	afx_msg void OnChangeEditReserved0();
	afx_msg void OnChangeEditReserved1();
	afx_msg void OnChangeEditVid();
	afx_msg void OnChangeEditSerial0();
	afx_msg void OnChangeEditSerial1();
	afx_msg void OnChangeEditSerial2();
	afx_msg void OnChangeEditSerial3();
	afx_msg void OnChangeEditWriteKey();
	afx_msg void OnButtonGen();
	afx_msg void OnButtonCheck();
	afx_msg void OnButtonCopyAddress();
	afx_msg void OnButtonCopyId();
	afx_msg void OnButtonBurst();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SNGEN2DLG_H__415B08FA_7259_4761_B3AD_058C468DA5E2__INCLUDED_)
