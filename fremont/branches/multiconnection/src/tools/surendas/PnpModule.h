// PnpModule.h: interface for the CPnpModule class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PNPMODULE_H__AABCDD43_4E89_4E10_905E_7BCFDB0D7107__INCLUDED_)
#define AFX_PNPMODULE_H__AABCDD43_4E89_4E10_905E_7BCFDB0D7107__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CPnpModule  
{
public:
	UINT	uiListenPort;
	SOCKET	ListenSocket;
	HANDLE	hThread;

	BOOL Initialize(UINT iPort);
	BOOL Initialize(UINT uiPort, CNetdiskTestDlg *pDlg);

	CPnpModule();
	virtual ~CPnpModule();

};

typedef struct _PNP_MESSAGE {
	UCHAR	ucType;
	UCHAR	ucVersion;
} PNP_MESSAGE, *PPNP_MESSAGE;

#endif // !defined(AFX_PNPMODULE_H__AABCDD43_4E89_4E10_905E_7BCFDB0D7107__INCLUDED_)
