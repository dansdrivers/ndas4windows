// SnGen2.h : main header file for the SNGEN2 application
//

#if !defined(AFX_SNGEN2_H__E0915913_B8B7_4E37_A21F_EDA0F258CC8F__INCLUDED_)
#define AFX_SNGEN2_H__E0915913_B8B7_4E37_A21F_EDA0F258CC8F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CSnGen2App:
// See SnGen2.cpp for the implementation of this class
//

class CSnGen2App : public CWinApp
{
public:
	CSnGen2App();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSnGen2App)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CSnGen2App)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SNGEN2_H__E0915913_B8B7_4E37_A21F_EDA0F258CC8F__INCLUDED_)
