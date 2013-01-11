// NetdiskTestDlg.h : header file
//

#if !defined(AFX_NETDISKTESTDLG_H__8001E82E_8B83_40EA_97A6_4BB67AE431DE__INCLUDED_)
#define AFX_NETDISKTESTDLG_H__8001E82E_8B83_40EA_97A6_4BB67AE431DE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestDlg dialog

class CNetdiskTestDlg : public CDialog
{
// Construction
public:
	CNetdiskTestDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CNetdiskTestDlg)
	enum { IDD = IDD_NETDISKTEST_DIALOG };
	CStatic	m_ctrlStat73;
	CStatic	m_ctrlStat72;
	CStatic	m_ctrlStat71;
	CStatic	m_ctrlStat63;
	CStatic	m_ctrlStat62;
	CStatic	m_ctrlStat61;
	CStatic	m_ctrlStat53;
	CStatic	m_ctrlStat52;
	CStatic	m_ctrlStat51;
	CStatic	m_ctrlStat43;
	CStatic	m_ctrlStat42;
	CStatic	m_ctrlStat41;
	CStatic	m_ctrlStat33;
	CStatic	m_ctrlStat32;
	CStatic	m_ctrlStat31;
	CStatic	m_ctrlStat23;
	CStatic	m_ctrlStat22;
	CStatic	m_ctrlStat21;
	CStatic	m_ctrlStat03;
	CStatic	m_ctrlStat02;
	CStatic	m_ctrlStat01;
	CStatic	m_ctrlStat13;
	CStatic	m_ctrlStat12;
	CStatic	m_ctrlStat11;
	CString	m_pass0;
	CString	m_pass1;
	CString	m_pass2;
	CString	m_pass3;
	CString	m_pass4;
	CString	m_pass5;
	CString	m_pass6;
	CString	m_pass7;
	CString	m_seq0;
	CString	m_seq1;
	CString	m_seq2;
	CString	m_seq3;
	CString	m_seq4;
	CString	m_seq5;
	CString	m_seq6;
	CString	m_seq7;
	CString	m_eth0;
	CString	m_eth1;
	CString	m_eth2;
	CString	m_eth3;
	CString	m_eth4;
	CString	m_eth5;
	CString	m_eth6;
	CString	m_eth7;
	CString	m_lot;
	CString	m_pass02;
	CString	m_pass03;
	CString	m_pass12;
	CString	m_pass13;
	CString	m_pass22;
	CString	m_pass23;
	CString	m_pass32;
	CString	m_pass33;
	CString	m_pass42;
	CString	m_pass43;
	CString	m_pass52;
	CString	m_pass53;
	CString	m_pass62;
	CString	m_pass63;
	CString	m_pass72;
	CString	m_pass73;
	CString	m_strDBG;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetdiskTestDlg)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	//{{AFX_MSG(CNetdiskTestDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnButtonRefresh();
	afx_msg void OnChangeEdit1Lot();
	afx_msg void OnRadio1();
	afx_msg void OnRadio2();
	afx_msg void OnRadio3();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETDISKTESTDLG_H__8001E82E_8B83_40EA_97A6_4BB67AE431DE__INCLUDED_)
