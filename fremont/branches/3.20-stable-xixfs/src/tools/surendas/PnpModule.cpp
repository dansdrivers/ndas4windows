
// PnpModule.cpp: implementation of the CPnpModule class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "PnpModule.h"

extern HANDLE	hServerStopEvent;
extern HANDLE	hUpdateEvent;
extern UCHAR	ucCurrentNetdisk[6];
extern UCHAR	ucVersion;
extern BOOL		bNewNetdiskArrived;

extern BOOL bDlgInitialized;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPnpModule::CPnpModule()
{
	uiListenPort = 0;
	ListenSocket = INVALID_SOCKET;
}

CPnpModule::~CPnpModule()
{
}

DWORD WINAPI 
PnpModuleThreadProc(
					LPVOID lpParameter   // thread data
					)
{
	int						iResult;
	CPnpModule				*pRC;
	WSAOVERLAPPED			overlapped;
	WSABUF					buffer[1];
	DWORD					dwFlag;
	DWORD					dwRecvDataLen;
	SOCKADDR_LPX			PeerAddr;
	INT						iFromLen;
	BOOL					bFinish;
	HANDLE					hEvents[2];
	PNP_MESSAGE				message;

	DebugPrint(2, (TEXT("[NetdiskTest]PnpModuleThread: PnpModule Thread Started.\n")));

	pRC = (CPnpModule *) lpParameter;

	// Server Stop Event.
	hEvents[0] = hServerStopEvent;

	// Overlapped event
	//
	hEvents[1] = WSACreateEvent();
	memset(&overlapped, 0, sizeof(WSAOVERLAPPED));
	overlapped.hEvent = hEvents[1];

	buffer[0].len = sizeof(message);
	buffer[0].buf = (PCHAR)&message;
	 
	
	bFinish = FALSE;

	do {
		DWORD	dwResult;

		// Flag
		dwFlag = 0;
	
		WSAResetEvent(hEvents[1]);

		iFromLen = sizeof(PeerAddr);

		iResult = WSARecvFrom(
			pRC->ListenSocket,
			buffer,
			1,
			&dwRecvDataLen,
			&dwFlag,
			(struct sockaddr *)&PeerAddr,
			&iFromLen,
			&overlapped,
			NULL
			);
		if(iResult != SOCKET_ERROR) {
			DebugPrint(4, ("[NetDiskTest]PnpModuleThreadProc: no Error when Recv.\n"));
			continue;
		}

		if((iResult = WSAGetLastError()) != WSA_IO_PENDING) {
			DebugPrint(1, ("[NetDiskTest]PnpModuleThreadProc: Error when Recv. %d\n", iResult));

			bFinish = TRUE;

			continue;
		}

		// Wait
		dwResult = WaitForMultipleObjects(
			2,
			hEvents,
			FALSE,
			3000 //INFINITE
			);
		switch(dwResult) {
		case WAIT_TIMEOUT:
			TRACE("\nWAIT_TIMEOUT\n");
			SetEvent(hUpdateEvent);
			break;
		case WAIT_OBJECT_0:
			{
				TRACE("\nWAIT_OBJECT_0\n");
				bFinish = TRUE;
				DebugPrint(1, ("[NetDiskTest]PnpModuleThreadProc: Recv Stop Event.\n"));
			}
			break;
		case WAIT_OBJECT_0 + 1:
			{
				TRACE("\nWAIT_OBJECT_0+1\n");
				BOOL		bResult;

				bResult = WSAGetOverlappedResult(
					pRC->ListenSocket,
					&overlapped,
					&dwRecvDataLen,
					TRUE,
					&dwFlag
					);

				// Check Size.
				if(dwRecvDataLen != sizeof(message)) {
					DebugPrint(1, ("[NetDiskTest]PnpModuleThreadProc: Recv Packet size = %d. Ignore...\n", dwRecvDataLen));
					break;
				}

				// Is Valid?
				if (message.ucType != 0 || 
					(
						message.ucVersion != 0 &&
						message.ucVersion != 1 &&
						message.ucVersion != 2
					)
				)
				{
					DebugPrint(1, ("[NetDiskTest]PnpModuleThreadProc: Bad Packet 0x%x. 0x%x. Ignore...\n", message.ucType, message.ucVersion));
					break;
				}
				
			}
//			DebugPrint(4, ("[NetDiskTest]PnpModuleThreadProc: %.2X%.2X%.2X\n",
//				PeerAddr.LpxAddress.Node[0], PeerAddr.LpxAddress.Node[1], PeerAddr.LpxAddress.Node[2]));
			memcpy((void *) ucCurrentNetdisk, (void *) PeerAddr.LpxAddress.Node, 6);
			ucVersion = message.ucVersion;
			bNewNetdiskArrived = TRUE;
			SetEvent(hUpdateEvent);
			break;

		default:
			break;
		}

	} while(bFinish != TRUE);

	CloseHandle(hEvents[1]);

	ExitThread(0);

	return 0;
}

BOOL CPnpModule::Initialize(UINT uiPort)
{
	int					iResult;
	SOCKADDR_LPX		lpxAddr;


	// Create Socket.
	ListenSocket = WSASocket(
		AF_LPX,
		SOCK_DGRAM,
		IPPROTO_LPXUDP,
		NULL,
		NULL,
		WSA_FLAG_OVERLAPPED
		);
	if(ListenSocket == INVALID_SOCKET) {
		AfxMessageBox("Cannot Initialize Socket\n(Maybe not installed LPX)", MB_OK, 0);
//		PrintErrorCode(TEXT("[NetdiskTest]CPnpModule::Initialize: socket "), WSAGetLastError());
		return FALSE;
	}

	memset(&lpxAddr, 0, sizeof(lpxAddr));
	
	lpxAddr.sin_family = AF_LPX;
	
	lpxAddr.LpxAddress.Port = htons(uiPort);
	
	iResult = bind(ListenSocket, (struct sockaddr *)&lpxAddr, sizeof(lpxAddr));
	
	if(iResult == SOCKET_ERROR) {
		AfxMessageBox("Cannot bind socket\n(Maybe Test Program Already Rrunning)", MB_OK, 0);
//		PrintErrorCode("[NetdiskTest]CPnpModule::Initialize: bind ", WSAGetLastError());
		closesocket(ListenSocket);
		ListenSocket = INVALID_SOCKET;
		
		return FALSE;
	}

	// Create Thread.
	hThread = CreateThread( 
        NULL,                       // no security attributes 
        0,                          // use default stack size  
        PnpModuleThreadProc,		// thread function 
        this,						// argument to thread function 
        NULL,
        NULL);						// returns the thread identifier 
	if(hThread == NULL) {
		AfxMessageBox("Cannot Initialize Thread", MB_OK, 0);
//		PrintErrorCode(TEXT("[LDServ]CPnpModule::Initialize: Thread Creation Failed "), GetLastError());
		
		return FALSE;
	}
	
	return TRUE;
}

BOOL CPnpModule::Initialize(UINT uiPort, CNetdiskTestDlg *pDlg)
{
	return Initialize(uiPort);
}