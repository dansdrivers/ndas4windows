// UpdateModule.cpp: implementation of the CUpdateModule class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#define	MAX_RETRY_COUNT	6


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

// BOOL Dummy() {return TRUE;}

extern BOOL				bDlgInitialized;
extern CUpdateModule	UpdateModule;

extern HICON hIconGreen;
extern HICON hIconRed;
extern HICON hIconWhite;
extern HICON hIconYellow;

extern int iTestSize;		// Default : 5 MB


HANDLE		hServerStopEvent = NULL;
HANDLE		hUpdateEvent = NULL;
UCHAR		ucCurrentNetdisk[6];
UCHAR		ucVersion;
BOOL		bNewNetdiskArrived = FALSE;
UINT		uiLot = 0;


//////////////////////////////////////////////////////////////////////
// NDAS heart beat callback
//////////////////////////////////////////////////////////////////////

VOID 
NDASHEARAPI_CALLBACK
NdasHeartbeatCallback(
	CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, 
	LPVOID lpContext)
{
	CUpdateModule *pUM = (CUpdateModule *)lpContext;
	int iCount;


	// Check if arrived Netdisk already exists
	// TODO: we need synchonization here.
	for(iCount = 0; iCount < 8; iCount++) {
		if(pUM->Netdisk[iCount].bExist)
			if(!memcmp(pUM->Netdisk[iCount].ucAddr, pHeartbeat->DeviceAddress.Node, 6)) {
				return;
		}
	}

	// Notify to UpdateModule thread
	memcpy((void *) ucCurrentNetdisk, (void *) pHeartbeat->DeviceAddress.Node, 6);
	ucVersion = pHeartbeat->Version;
	bNewNetdiskArrived = TRUE;
	SetEvent(hUpdateEvent);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CUpdateModule::CUpdateModule()
{
	int					iCount;

	hThread = NULL;
	for(iCount = 0; iCount < 8; iCount++) {
		Netdisk[iCount].bExist = FALSE;
		Netdisk[iCount].bTest2Passed = FALSE;
		Netdisk[iCount].bTest3Passed = FALSE;
		Netdisk[iCount].bTestFinished = FALSE;
	}
}

CUpdateModule::~CUpdateModule()
{

}

void DlgDisplayDiskInfo(
						LPTSTR strSeq,
						LPTSTR strPass,
						CStatic* led,
						CStatic* led2,
						CStatic* led3,
						LPTSTR strEth,
						PTEST_NETDISK testDisk
						)
{
	CHAR					ucBuff[15];
	UINT					iAddr;
	UINT					iLot;

	if(testDisk->bExist == TRUE) {
		iLot = (testDisk->ucAddr[3] << 1) | (testDisk->ucAddr[4] >> 7);
		iAddr = (testDisk->ucAddr[4] << 8) | (testDisk->ucAddr[5]);
		sprintf(ucBuff, "%.3d - %.5d", iLot, iAddr & 0x7fff);
		strncpy(strSeq, ucBuff, 11); strSeq[11] = 0;

		//if(uiLot && (iLot != uiLot)) {
			//sprintf(strPass, "%7s", "Bad LOT");
		//	led->SetIcon( hIconRed );
		//} else if(testDisk->bIsOK) {
		if (testDisk->bIsOK) {
			//sprintf(strPass, "%6s", "Passed");
			led->SetIcon( hIconGreen );
		} else {
			//sprintf(strPass, "%5s", "Error");
			led->SetIcon( hIconRed );
		}
		
		//if (testDisk->bTest2Passed) sprintf(strPass2, "%6s", "Passed");
		//else sprintf(strPass2, "%5s", "Error");

		//if (testDisk->bTest3Passed) sprintf(strPass3, "%6s", "Passed");
		//else sprintf(strPass3, "%5s", "Error");

		sprintf(strEth, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X - (%s)",
			testDisk->ucAddr[0], testDisk->ucAddr[1],
			testDisk->ucAddr[2], testDisk->ucAddr[3],
			testDisk->ucAddr[4], testDisk->ucAddr[5],
			(0 == testDisk->ucVersion) ? "1.0" :
			(1 == testDisk->ucVersion) ? "1.1" :
			(2 == testDisk->ucVersion) ? "2.0" : "?"
			);
	} else {
		strSeq[0] = 0; strPass[0] = 0; strEth[0] = 0;
		led->SetIcon( hIconWhite );
		led2->SetIcon( hIconWhite );
		led3->SetIcon( hIconWhite );
	}
}

void DlgDisplayDiskInfo2(
						LPTSTR strSeq,
						LPTSTR strPass2,
						CStatic* led,
						LPTSTR strEth,
						PTEST_NETDISK testDisk
						)
{
	CHAR					ucBuff[15];
	UINT					iAddr;
	UINT					iLot;

	if(testDisk->bExist == TRUE) {
		iLot = (testDisk->ucAddr[3] << 1) | (testDisk->ucAddr[4] >> 7);
		iAddr = (testDisk->ucAddr[4] << 8) | (testDisk->ucAddr[5]);
		sprintf(ucBuff, "%.3d - %.5d", iLot, iAddr & 0x7fff);
		strncpy(strSeq, ucBuff, 11); strSeq[11] = 0;

		if (testDisk->bTest2Passed) {
			led->SetIcon( hIconGreen );
			TRACE("ICON 2 COLOR : GREEN\n");
			//sprintf(strPass2, "%6s", "Passed");
		} else {
			//sprintf(strPass2, "%5s", "Error");
			led->SetIcon( hIconRed );
			TRACE("ICON 2 COLOR : RED\n");
		}

		//if (testDisk->bTest3Passed) sprintf(strPass3, "%6s", "Passed");
		//else sprintf(strPass3, "%5s", "Error");

		sprintf(strEth, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X - (%s)",
			testDisk->ucAddr[0], testDisk->ucAddr[1],
			testDisk->ucAddr[2], testDisk->ucAddr[3],
			testDisk->ucAddr[4], testDisk->ucAddr[5],
			(0 == testDisk->ucVersion) ? "1.0" :
			(1 == testDisk->ucVersion) ? "1.1" :
			(2 == testDisk->ucVersion) ? "2.0" : "?"
			);
	} else {
		strSeq[0] = 0; strEth[0] = 0;
		led->SetIcon( hIconWhite );
	}
}

void DlgDisplayDiskInfo3(
						LPTSTR strSeq,
						LPTSTR strPass3,
						CStatic * led,
						LPTSTR strEth,
						PTEST_NETDISK testDisk,
						char *szPN
						)
{
	CHAR					ucBuff[15];
	UINT					iAddr;
	UINT					iLot;

	if(testDisk->bExist == TRUE) {
		iLot = (testDisk->ucAddr[3] << 1) | (testDisk->ucAddr[4] >> 7);
		iAddr = (testDisk->ucAddr[4] << 8) | (testDisk->ucAddr[5]);
		sprintf(ucBuff, "%.3d - %.5d", iLot, iAddr & 0x7fff);
		strncpy(strSeq, ucBuff, 11); strSeq[11] = 0;

		if (testDisk->bTest3Passed) {
			//sprintf(strPass3, "%6s", "Passed");
			led->SetIcon( hIconGreen );
		} else {
			//sprintf(strPass3, "%5s", "Error");
			led->SetIcon( hIconRed );
		}

		sprintf(strEth, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X - (%s) : %s",
			testDisk->ucAddr[0], testDisk->ucAddr[1],
			testDisk->ucAddr[2], testDisk->ucAddr[3],
			testDisk->ucAddr[4], testDisk->ucAddr[5],
			(0 == testDisk->ucVersion) ? "1.0" :
			(1 == testDisk->ucVersion) ? "1.1" :
			(2 == testDisk->ucVersion) ? "2.0" : "?",
			szPN
			);
	} else {
		strSeq[0] = 0; strEth[0] = 0;
		led->SetIcon( hIconWhite );
	}
}



DWORD WINAPI 
UpdateModuleThreadProc(
					LPVOID lpParameter   // thread data
					)
{
	CUpdateModule			*pRC;
	BOOL					bFinish;
	HANDLE					hEvents[2];
	DWORD					dwResult;
	int						iCount;
	int						iFailCount1=0;
	UCHAR					cAddrBuff[6];
	UCHAR					cVersionBuff;
	HANDLE					ndasHearHandle;

	int i;

	CNetdiskTestDlg	*pDlg;

	//while (!bDlgInitialized) Sleep(0);

	//pDlg = (CNetdiskTestDlg *)AfxGetMainWnd();
	pDlg = (CNetdiskTestDlg*)lpParameter;

	DebugPrint(2, (TEXT("[NetdiskTest]UpdateModuleThread: UpdateModule Thread Started.\n")));

	//pRC = (CUpdateModule *) lpParameter;
	pRC = &UpdateModule;

	// Server Stop Event.
	hEvents[0] = hServerStopEvent;
	hEvents[1] = hUpdateEvent;

	bFinish = FALSE;

	CString * strzSeq[8];
	CString * strzPass[8];
	CString * strzPass2[8];
	CString * strzPass3[8];
	CString * strzEth[8];
	CStatic * ledzPass1[8];
	CStatic * ledzPass2[8];
	CStatic * ledzPass3[8];
	
	strzSeq[0] = &(pDlg->m_seq0);		strzSeq[1] = &(pDlg->m_seq1);
	strzSeq[2] = &(pDlg->m_seq2);		strzSeq[3] = &(pDlg->m_seq3);
	strzSeq[4] = &(pDlg->m_seq4);		strzSeq[5] = &(pDlg->m_seq5);
	strzSeq[6] = &(pDlg->m_seq6);		strzSeq[7] = &(pDlg->m_seq7);

	strzPass[0] = &(pDlg->m_pass0);		strzPass[1] = &(pDlg->m_pass1);
	strzPass[2] = &(pDlg->m_pass2);		strzPass[3] = &(pDlg->m_pass3);
	strzPass[4] = &(pDlg->m_pass4);		strzPass[5] = &(pDlg->m_pass5);
	strzPass[6] = &(pDlg->m_pass6);		strzPass[7] = &(pDlg->m_pass7);

	strzPass2[0] = &(pDlg->m_pass02);		strzPass2[1] = &(pDlg->m_pass12);
	strzPass2[2] = &(pDlg->m_pass22);		strzPass2[3] = &(pDlg->m_pass32);
	strzPass2[4] = &(pDlg->m_pass42);		strzPass2[5] = &(pDlg->m_pass52);
	strzPass2[6] = &(pDlg->m_pass62);		strzPass2[7] = &(pDlg->m_pass72);

	strzPass3[0] = &(pDlg->m_pass03);		strzPass3[1] = &(pDlg->m_pass13);
	strzPass3[2] = &(pDlg->m_pass23);		strzPass3[3] = &(pDlg->m_pass33);
	strzPass3[4] = &(pDlg->m_pass43);		strzPass3[5] = &(pDlg->m_pass53);
	strzPass3[6] = &(pDlg->m_pass63);		strzPass3[7] = &(pDlg->m_pass73);

	ledzPass1[0] = &(pDlg->m_ctrlStat01);	ledzPass1[1] = &(pDlg->m_ctrlStat11);
	ledzPass1[2] = &(pDlg->m_ctrlStat21);	ledzPass1[3] = &(pDlg->m_ctrlStat31);
	ledzPass1[4] = &(pDlg->m_ctrlStat41);	ledzPass1[5] = &(pDlg->m_ctrlStat51);
	ledzPass1[6] = &(pDlg->m_ctrlStat61);	ledzPass1[7] = &(pDlg->m_ctrlStat71);

	ledzPass2[0] = &(pDlg->m_ctrlStat02);	ledzPass2[1] = &(pDlg->m_ctrlStat12);
	ledzPass2[2] = &(pDlg->m_ctrlStat22);	ledzPass2[3] = &(pDlg->m_ctrlStat32);
	ledzPass2[4] = &(pDlg->m_ctrlStat42);	ledzPass2[5] = &(pDlg->m_ctrlStat52);
	ledzPass2[6] = &(pDlg->m_ctrlStat62);	ledzPass2[7] = &(pDlg->m_ctrlStat72);

	ledzPass3[0] = &(pDlg->m_ctrlStat03);	ledzPass3[1] = &(pDlg->m_ctrlStat13);
	ledzPass3[2] = &(pDlg->m_ctrlStat23);	ledzPass3[3] = &(pDlg->m_ctrlStat33);
	ledzPass3[4] = &(pDlg->m_ctrlStat43);	ledzPass3[5] = &(pDlg->m_ctrlStat53);
	ledzPass3[6] = &(pDlg->m_ctrlStat63);	ledzPass3[7] = &(pDlg->m_ctrlStat73);

	strzEth[0] = &(pDlg->m_eth0);		strzEth[1] = &(pDlg->m_eth1);
	strzEth[2] = &(pDlg->m_eth2);		strzEth[3] = &(pDlg->m_eth3);
	strzEth[4] = &(pDlg->m_eth4);		strzEth[5] = &(pDlg->m_eth5);
	strzEth[6] = &(pDlg->m_eth6);		strzEth[7] = &(pDlg->m_eth7);

	//
	// Register the heartbeat handler
	//
	ndasHearHandle = NdasHeartbeatRegisterNotification(::NdasHeartbeatCallback, pRC);
	if(NULL == ndasHearHandle) {
		AfxMessageBox("Failed to register heartbeat handler", MB_OK, 0);
		goto error_out;
	}

	do {

		// Wait
		dwResult = WaitForMultipleObjects(
			2,
			hEvents,
			FALSE,
			3000
			);

		switch(dwResult) {
			// Stop Event
		case WAIT_OBJECT_0:
			{
				bFinish = TRUE;
				DebugPrint(1, ("[NetDiskTest]UpdateModuleThreadProc: Recv Stop Event.\n"));
			}
			break;
			// Update Event
		case WAIT_TIMEOUT:
		case WAIT_OBJECT_0 + 1:
			ResetEvent(hEvents[1]);
			if (iTestSize == 0) continue;
			DebugPrint(0, (TEXT("Update Event Received %d\n"), pRC->Netdisk[0].bExist));
			memcpy(cAddrBuff, ucCurrentNetdisk, 6); // save caught address
			cVersionBuff = ucVersion; // save caught version
			DebugPrint(4, (TEXT("[NetDiskTest]UpdateModuleThreadProc: New Netdisk arrived. %.2X:%.2X:%.2X%:%.2X:%.2X:%.2X\n"),
				cAddrBuff[0], cAddrBuff[1], cAddrBuff[2],
				cAddrBuff[3], cAddrBuff[4], cAddrBuff[5]));
			// Check for Disconnected Netdisk
			for(iCount = 0; iCount < 8; iCount++) {
				if(pRC->Netdisk[iCount].bExist)
					pRC->Netdisk[iCount].bExist = IsConnected(&pRC->Netdisk[iCount]);
				if (pRC->Netdisk[iCount].bExist==FALSE) {
					DebugPrint(0, (TEXT("NetDisk #%d disconnected\n"), iCount));
					pRC->Netdisk[iCount].bIsOK=FALSE;
					pRC->Netdisk[iCount].bTest2Passed=FALSE;
					pRC->Netdisk[iCount].bTest3Passed=FALSE;
					pRC->Netdisk[iCount].bTestFinished=FALSE;
					memset(pRC->Netdisk[iCount].ucAddr, 0, sizeof(UCHAR)*6);
					memset(pRC->Netdisk[iCount].ucPassword, 0, sizeof(UCHAR)*8);
					DlgDisplayDiskInfo(
						strzSeq[iCount]->GetBuffer(15),
						strzPass[iCount]->GetBuffer(15),
						ledzPass1[iCount], ledzPass2[iCount], ledzPass3[iCount],
						strzEth[iCount]->GetBuffer(30),
						&(pRC->Netdisk[iCount]));
					pDlg->PostMessage(WM_PAINT, 0, 0);
				}
			}
			// When New Netdisk broadcast catched
			if(bNewNetdiskArrived) {
				BOOL		bExist = FALSE;
				int			iEmptySlot = -1;

				bNewNetdiskArrived = FALSE;

				// Check if arrived Netdisk already exists
				for(iCount = 0; (iCount < 8) && (!bExist); iCount++) {
					if(pRC->Netdisk[iCount].bExist)
						if(!memcmp(pRC->Netdisk[iCount].ucAddr, cAddrBuff, 6)) {
							bExist = TRUE;
					}
				}
				// If not, insert it to netdisk list
				if(!bExist) {
					for(iCount = 0; iCount < 8; iCount++) {
						if(!pRC->Netdisk[iCount].bExist) {
							iEmptySlot = iCount;
							break;
						}
					}
					if(iEmptySlot != -1) {
						pRC->Netdisk[iEmptySlot].bExist = TRUE;
						pRC->Netdisk[iEmptySlot].bIsOK = FALSE;
						pRC->Netdisk[iEmptySlot].bTest2Passed = FALSE;
						pRC->Netdisk[iEmptySlot].bTest3Passed = FALSE;
						memcpy(pRC->Netdisk[iEmptySlot].ucAddr, cAddrBuff, 6);
						pRC->Netdisk[iEmptySlot].ucVersion = cVersionBuff;
/*
						if(cAddrBuff[0] == 0x00 && cAddrBuff[1] == 0x0B && cAddrBuff[2] == 0xD0) {
							DebugPrint(2, (TEXT("[NetdiskTest]UpdateModule: New Version Password %d\n"), iEmptySlot));
							memcpy(pRC->Netdisk[iEmptySlot].ucPassword, "\xbb\xea\x30\x15\x73\x50\x4a\x1f", 8);
						} else if(cAddrBuff[0] == 0x00 && cAddrBuff[1] == 0xF0 && cAddrBuff[2] == 0x0F) {
							DebugPrint(2, (TEXT("[NetdiskTest]UpdateModule: Old Version Password %d\n"), iEmptySlot));
							memcpy(pRC->Netdisk[iEmptySlot].ucPassword, "\x07\x06\x05\x04\x03\x02\x01\x00", 8);
							DebugPrint(2, (TEXT("%.2X\n"), pRC->Netdisk[iEmptySlot].ucPassword[1]));
						} else {
							pRC->Netdisk[iEmptySlot].bExist = FALSE;
							pRC->Netdisk[iEmptySlot].bIsOK=FALSE;
							pRC->Netdisk[iEmptySlot].bTest2Passed=FALSE;
							pRC->Netdisk[iEmptySlot].bTest3Passed=FALSE;
							pRC->Netdisk[iEmptySlot].bTestFinished=FALSE;
							memset(pRC->Netdisk[iEmptySlot].ucAddr, 0, sizeof(UCHAR)*6);
							memset(pRC->Netdisk[iEmptySlot].ucPassword, 0, sizeof(UCHAR)*8);
							pRC->Netdisk[iEmptySlot].ucVersion = cVersionBuff;

						}
*/
						if(cAddrBuff[0] == 0x00 && cAddrBuff[1] == 0xF0 && cAddrBuff[2] == 0x0F) {
							DebugPrint(2, (TEXT("[NetdiskTest]UpdateModule: Old Version Password %d\n"), iEmptySlot));
							memcpy(pRC->Netdisk[iEmptySlot].ucPassword, "\x07\x06\x05\x04\x03\x02\x01\x00", 8);
							DebugPrint(2, (TEXT("%.2X\n"), pRC->Netdisk[iEmptySlot].ucPassword[1]));
						}
						else {
							DebugPrint(2, (TEXT("[NetdiskTest]UpdateModule: New Version Password %d\n"), iEmptySlot));
							memcpy(pRC->Netdisk[iEmptySlot].ucPassword, "\xbb\xea\x30\x15\x73\x50\x4a\x1f", 8);
						}
					}
				}
			}

			// Now Test each existing Netdisk - Stage 1
			for(iCount = 0; iCount < 8; iCount++) {
				i=iCount;
				if (pRC->Netdisk[iCount].bTestFinished == TRUE) continue;
				if (pRC->Netdisk[iCount].bExist != TRUE) {
					pRC->Netdisk[iCount].bIsOK=FALSE;
					pRC->Netdisk[iCount].bTest2Passed=FALSE;
					pRC->Netdisk[iCount].bTest3Passed=FALSE;
					pRC->Netdisk[iCount].bTestFinished=FALSE;
					memset(pRC->Netdisk[iCount].ucAddr, 0, sizeof(UCHAR)*6);
					memset(pRC->Netdisk[iCount].ucPassword, 0, sizeof(UCHAR)*8);
					DlgDisplayDiskInfo(
						strzSeq[i]->GetBuffer(15),
						strzPass[i]->GetBuffer(15),
						ledzPass1[i], ledzPass2[i], ledzPass3[i],
						strzEth[i]->GetBuffer(30),
						&(pRC->Netdisk[i]));
					pDlg->PostMessage(WM_PAINT, 0, 0);
					continue;
				}
				//
				// Phase #1
				// Connection test
				// See if a connecton can be made.
phase_1:
				ledzPass1[i]->SetIcon(hIconYellow);
				if(pRC->Netdisk[iCount].bExist && !pRC->Netdisk[iCount].bIsOK) {
					// Perform login test
					pRC->Netdisk[iCount].bIsOK = IsValidate(&pRC->Netdisk[iCount]);
				}
				if (pRC->Netdisk[iCount].bIsOK) {
					DlgDisplayDiskInfo(
						strzSeq[i]->GetBuffer(15),
						strzPass[i]->GetBuffer(15),
						ledzPass1[i], ledzPass2[i], ledzPass3[i],
						strzEth[i]->GetBuffer(30),
						&(pRC->Netdisk[i]));
					pDlg->PostMessage(WM_PAINT, 0, 0);
					iFailCount1 = 0;
				} else {
					//iFailCount1++;
					TRACE("ETH: %.2X:%.2X, FailCount = %d\n", pRC->Netdisk[i].ucAddr[4], pRC->Netdisk[i].ucAddr[5], iFailCount1+1);
					if ( ++iFailCount1 > MAX_RETRY_COUNT) {
						DlgDisplayDiskInfo(
							strzSeq[i]->GetBuffer(15),
							strzPass[i]->GetBuffer(15),
							ledzPass1[i], ledzPass2[i], ledzPass3[i],
							strzEth[i]->GetBuffer(30),
							&(pRC->Netdisk[i]));
						pRC->Netdisk[i].bTestFinished = TRUE;
						pDlg->PostMessage(WM_PAINT, 0, 0);
						iFailCount1 = 0;
						continue;
					} else {
						if ((pRC->Netdisk[iCount].bExist = IsConnected(&pRC->Netdisk[iCount])) != TRUE) {
							pRC->Netdisk[iCount].bIsOK=FALSE;
							pRC->Netdisk[iCount].bTest2Passed=FALSE;
							pRC->Netdisk[iCount].bTest3Passed=FALSE;
							pRC->Netdisk[iCount].bTestFinished=FALSE;
							memset(pRC->Netdisk[iCount].ucAddr, 0, sizeof(UCHAR)*6);
							memset(pRC->Netdisk[iCount].ucPassword, 0, sizeof(UCHAR)*8);
							DlgDisplayDiskInfo(
								strzSeq[i]->GetBuffer(15),
								strzPass[i]->GetBuffer(15),
								ledzPass1[i], ledzPass2[i], ledzPass3[i],
								strzEth[i]->GetBuffer(30),
								&(pRC->Netdisk[i]));
							pDlg->PostMessage(WM_PAINT, 0, 0);
							iFailCount1 = 0;
							continue;
						}
						Sleep(10);
						goto phase_1;
					}
				}

				//
				// Phase #2
				// Read/write test
				// Recover the target disk's content after tests.
				ledzPass2[i]->SetIcon(hIconYellow);
				if( (pRC->Netdisk[iCount].bExist == TRUE) && (pRC->Netdisk[iCount].bTest2Passed == FALSE) ) {
					// Perform RW test
					pRC->Netdisk[iCount].bTest2Passed = TestStep2(&pRC->Netdisk[iCount]);
				}
				DlgDisplayDiskInfo2(
					strzSeq[i]->GetBuffer(15),
					strzPass2[i]->GetBuffer(15),
					ledzPass2[i],
					strzEth[i]->GetBuffer(30),
					&(pRC->Netdisk[i]));
				if (pRC->Netdisk[iCount].bTest2Passed == FALSE) {
					pRC->Netdisk[iCount].bTestFinished = TRUE;
					continue;
				}
				//
				// Phase #3
				// Clear 2MB at the end of the disk.
				ledzPass3[i]->SetIcon(hIconYellow);
				if( (pRC->Netdisk[iCount].bExist == TRUE) && (pRC->Netdisk[iCount].bTest3Passed == FALSE)) {
					// Perform zeroing of the ending 2M bytes.
					pRC->Netdisk[iCount].bTest3Passed = TestStep3(&pRC->Netdisk[iCount]);
				}

				char szPN[32] = {0,};
				
				if (ReadProductNumber(&pRC->Netdisk[iCount], szPN)) {
				}
				DlgDisplayDiskInfo3(
					strzSeq[i]->GetBuffer(15),
					strzPass3[i]->GetBuffer(15),
					ledzPass3[i],
					strzEth[i]->GetBuffer(60),
					&(pRC->Netdisk[i]),
					szPN);
				if (pRC->Netdisk[iCount].bTest3Passed == FALSE) {
					pRC->Netdisk[iCount].bTestFinished = TRUE;
					continue;
				}
			}

			break;
		default:
			break;
		}

	} while(bFinish != TRUE);

error_out:
	if(ndasHearHandle) {
		NdasHeartbeatUnregisterNotification(ndasHearHandle);
	}

	CloseHandle(hEvents[1]);

	ExitThread(0);

	return 0;
}

BOOL CUpdateModule::Initialize()
{
	
	// Create Thread.
	hThread = CreateThread( 
        NULL,                       // no security attributes 
        0,                          // use default stack size  
        UpdateModuleThreadProc,		// thread function 
        this,						// argument to thread function 
        NULL,
        NULL);						// returns the thread identifier 
	if(hThread == NULL) {
		AfxMessageBox("Cannot Initialize Thread", MB_OK, 0);
//		PrintErrorCode(TEXT("[LDServ]CUpdateModule::Initialize:  Create Thread is failed "), GetLastError());
		
		return FALSE;
	}
	
	return TRUE;
}

BOOL CUpdateModule::Initialize(CNetdiskTestDlg *pDlg)
{
	
	// Create Thread.
	hThread = CreateThread( 
        NULL,                       // no security attributes 
        0,                          // use default stack size  
        UpdateModuleThreadProc,		// thread function 
        pDlg,						// argument to thread function 
        NULL,
        NULL);						// returns the thread identifier 
	if(hThread == NULL) {
		AfxMessageBox("Cannot Initialize Thread", MB_OK, 0);
//		PrintErrorCode(TEXT("[LDServ]CUpdateModule::Initialize:  Create Thread is failed "), GetLastError());
		
		return FALSE;
	}

	return TRUE;
}

