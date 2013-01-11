// NetdiskTest.h : main header file for the NETDISKTEST application
//

#if !defined(AFX_NETDISKTEST_H__8B13DA8C_EFA0_46BF_9B15_B61DC3198BBF__INCLUDED_)
#define AFX_NETDISKTEST_H__8B13DA8C_EFA0_46BF_9B15_B61DC3198BBF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CNetdiskTestApp:
// See NetdiskTest.cpp for the implementation of this class
//

class CNetdiskTestApp : public CWinApp
{
public:
	CNetdiskTestApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNetdiskTestApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CNetdiskTestApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NETDISKTEST_H__8B13DA8C_EFA0_46BF_9B15_B61DC3198BBF__INCLUDED_)
