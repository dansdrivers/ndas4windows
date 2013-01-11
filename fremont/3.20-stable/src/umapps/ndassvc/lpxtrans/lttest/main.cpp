#include <winsock2.h>
#include <socketlpx.h>
#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <xtl/xtlautores.h>
#include <xtl/xtltrace.h>
#include "lpxcs.h"
#include <vector>

class CNdasDeviceHeartbeatListener :
	public CLpxDatagramServer::IReceiveProcessor
{
	DWORD m_lastUpdate;

	typedef struct DEV_INFO {
		BYTE Id[6];
		BYTE Type;
		BYTE Version;
		DWORD LastUpdated;
		DWORD UpdateCount;
	} DEV_INFO, *PDEV_INFO;

	typedef std::vector<DEV_INFO> di_vector;
	di_vector m_devs;

public:
	VOID OnReceive(CLpxDatagramSocket& dgSock);
	VOID Run();

	static VOID pWriteDEVINFO(
		HANDLE hOutput, 
		DWORD x, DWORD y, 
		const DEV_INFO& di);
};

VOID
CNdasDeviceHeartbeatListener::pWriteDEVINFO(
	HANDLE hConsoleOutput, DWORD x, DWORD y, const DEV_INFO& di)
{
	static TCHAR szBuffer[255] = {0};

	COORD coord = { x, y };
	SetConsoleCursorPosition(hConsoleOutput, coord);
	size_t cchRem, cch;
	cch = cchRem = 255;

	HRESULT hr = StringCchPrintfEx(
		szBuffer, cchRem,
		NULL, &cchRem, 0, 
		_T("%02X:%02X:%02X:%02X:%02X:%02X %d.%d %08X %08X"),
		di.Id[0], di.Id[1], di.Id[2], 
		di.Id[3], di.Id[4], di.Id[5],
		di.Type, di.Version,
		di.LastUpdated,
		di.UpdateCount);

	cch -= cchRem;

	DWORD cchWritten;
	::WriteConsole(
		hConsoleOutput,
		szBuffer, 
		cch, 
		&cchWritten,
		NULL);
}

VOID
CNdasDeviceHeartbeatListener::OnReceive(CLpxDatagramSocket& dgSock)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived, dwFlags;
	struct { BYTE Type; BYTE Version; } * pData;
	// BYTE* pData = NULL;
	dgSock.GetRecvFromResult(&remoteAddr, &cbReceived, (BYTE**) &pData, &dwFlags);

	HANDLE hConsoleOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD nRow = 0, nCol = 0;
	DEV_INFO di;
	BOOL fExist = FALSE;
		
	DWORD n = 0;
	BOOL bUpdate = FALSE;

	if (::GetTickCount() - m_lastUpdate > (1000)) {
		bUpdate = TRUE;
		m_lastUpdate = ::GetTickCount();
	}

	for (di_vector::iterator itr = m_devs.begin();
		itr != m_devs.end(); ++itr)
	{
		DEV_INFO& di = *itr;
		if (0 == memcmp(di.Id, remoteAddr.LpxAddress.Node, 6)) {
			di.LastUpdated = ::GetTickCount();
			++(di.UpdateCount);
			fExist = TRUE;
		}

		if (bUpdate) {
			pWriteDEVINFO(hConsoleOutput, (n%2==0)?0:40, nRow, di);
			nRow += (n%2==0) ? 0 : 1;
			++n;
		}
	}

	if (!fExist) {
		CopyMemory(di.Id, remoteAddr.LpxAddress.Node, 6);
		di.LastUpdated = ::GetTickCount();
		di.UpdateCount = 1;
		di.Type = pData->Type;
		di.Version = pData->Version;
		m_devs.push_back(di);

		if (bUpdate) {
			pWriteDEVINFO(hConsoleOutput, (n%2==0)?0:40, nRow, di);
			nRow += (n%2==0) ? 0 : 1;
			++n;
		}
	}

}

VOID
CNdasDeviceHeartbeatListener::Run()
{
	CLpxDatagramServer server;
	BOOL fSuccess = server.Initialize();
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Init failed, error=0x%X\n"), GetLastError());
		return;
	}

	HANDLE hConsoleOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO  csbi;
	GetConsoleScreenBufferInfo(hConsoleOutput, &csbi);
	COORD coord;
	coord.X = 0;
	coord.Y = 0;

	DWORD nHeight = (csbi.srWindow.Bottom - csbi.srWindow.Top);
	DWORD nWidth = (csbi.srWindow.Right - csbi.srWindow.Left);
	DWORD nLen = nHeight * nWidth;

	DWORD cbWritten;
	FillConsoleOutputCharacter(
		hConsoleOutput,
		_T(' '),
		nLen,
		coord, &cbWritten);

	m_lastUpdate = ::GetTickCount();

	HANDLE hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	fSuccess = server.Receive(this, 10002, 256, hStopEvent);
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Listen failed, error=0x%X\n"), GetLastError());
		return;
	}
}

class CNdasHIXServer :
	public CLpxDatagramServer::IReceiveProcessor
{
public:
	VOID OnReceive(CLpxDatagramSocket& cListener);
	VOID Run();
};

VOID
CNdasHIXServer::OnReceive(CLpxDatagramSocket& cListener)
{
	SOCKADDR_LPX remoteAddr;
	DWORD cbReceived, dwFlags;
	BYTE* pData = NULL;
	cListener.GetRecvFromResult(&remoteAddr, &cbReceived, &pData, &dwFlags);
	_tprintf(_T("Received %d bytes from %s\n"),
		cbReceived, CSockLpxAddr(&remoteAddr).ToString());
}

VOID
CNdasHIXServer::Run()
{
	CLpxDatagramServer server;
	BOOL fSuccess = server.Initialize();
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Init failed, error=0x%X\n"), GetLastError());
		return;
	}

	HANDLE hStopEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	fSuccess = server.Receive(this, 8080, 256, hStopEvent);
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Listen failed, error=0x%X\n"), GetLastError());
		return;
	}
}


class CNdasHIXBroadcaster
{
public:

};

class CNdasHIXClient
{
protected:
	CLpxDatagramSocket m_client;
public:
	VOID Send();
};

VOID
CNdasHIXClient::Send()
{
	BOOL fSuccess = m_client.Initialize();
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Init failed, error=0x%X\n"), GetLastError());
		return;
	}

	fSuccess = m_client.Create();
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Creation failed, error=0x%X\n"), GetLastError());
		return;
	}

	BYTE addr[] = {0x02, 0x0C, 0x29, 0x77, 0x23, 0x43};
	CSockLpxAddr remoteAddr( addr, 8080);
	//SOCKADDR_LPX remoteAddr = {0};
	//remoteAddr.sin_family = AF_LPX;
	//remoteAddr.LpxAddress.Node[0] = 0x02;
	//remoteAddr.LpxAddress.Node[1] = 0x0C;
	//remoteAddr.LpxAddress.Node[2] = 0x29;
	//remoteAddr.LpxAddress.Node[3] = 0x77;
	//remoteAddr.LpxAddress.Node[4] = 0x23;
	//remoteAddr.LpxAddress.Node[5] = 0x43;
	//remoteAddr.LpxAddress.Port = HTONS(8080);

	BYTE data[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

	fSuccess = m_client.SendTo(&(SOCKADDR_LPX)remoteAddr, 16, data);
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("Send failed, error=0x%X\n"), GetLastError());
		return;
	}

	DWORD cbSent = 0;
	fSuccess = m_client.GetSendToResult(&cbSent);
	if (!fSuccess) {
		XTLTRACE1(TRACE_LEVEL_ERROR, 
			_T("GetSendToResult failed, error=0x%X\n"), GetLastError());
		return;
	}

	_tprintf(_T("sent %d bytes\n"), cbSent);

	fSuccess = m_client.Close();

}

void usage()
{
	_tprintf(_T("usage: lttest client server hbmon\n"));
}

int __cdecl wmain(int argc, WCHAR** argv)
{
	DWORD dwError = ERROR_SUCCESS;
	WSADATA wsaData;

	INT iResult = ::WSAStartup(MAKEWORD(2,2), &wsaData);
	if (0 != iResult) {
		dwError = ::GetLastError();
		return -1;
	}

	if (argc < 2) {
	  usage();
	  return 0;
	}

	if (0 == lstrcmpi(argv[1], _T("server")))
	{
		CNdasHIXServer server;
		// server.Initialize();
		// server.Start();
		server.Run();
	} 
	if (0 == lstrcmpi(argv[1], _T("hbmon")))
	{
		CNdasDeviceHeartbeatListener server;
		server.Run();
	} 
	else if (0 == lstrcmpi(argv[1], _T("client")))
	{
		CNdasHIXClient client;
		client.Send();
		client.Send();
		client.Send();
		_tprintf(_T("%08d: Initializing...\n"), GetTickCount());
//		client.Initialize();
		_tprintf(_T("%08d: Initialized...\n"), GetTickCount());
		_tprintf(_T("%08d: Discovering...\n"), GetTickCount());
//		client.Discover(10, 500);
		_tprintf(_T("%08d: Discovered...\n"), GetTickCount());
	} else {
	  _tprintf(_T("unknown command: %s\n"), argv[1]);
	  usage();
	  return 0;
	}

	iResult = ::WSACleanup();
	_ASSERTE(0 == iResult);

	return 0;
}

