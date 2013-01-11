// UpdateModule.h: interface for the CUpdateModule class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_UPDATEMODULE_H__31D127FB_54F6_491E_9AEA_576E3143F66F__INCLUDED_)
#define AFX_UPDATEMODULE_H__31D127FB_54F6_491E_9AEA_576E3143F66F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct _TEST_NETDISK {
	BOOL		bExist;

	// Device connection information
	UCHAR		ucAddr[6];
	UCHAR		ucVersion;
	UCHAR		ucPassword[8];

	// Disk information
	unsigned _int64	SectorCount;

	// Test results
	BOOL		bIsOK;
	BOOL		bTest2Passed;
	BOOL		bTest3Passed;
	BOOL		bTestFinished;
} TEST_NETDISK, *PTEST_NETDISK;

class CUpdateModule  
{
public:
	HANDLE					hThread;
	TEST_NETDISK			Netdisk[8];
	
	CUpdateModule();
	virtual ~CUpdateModule();

	BOOL Initialize();
	BOOL Initialize(CNetdiskTestDlg *pDlg);
};

#endif // !defined(AFX_UPDATEMODULE_H__31D127FB_54F6_491E_9AEA_576E3143F66F__INCLUDED_)
