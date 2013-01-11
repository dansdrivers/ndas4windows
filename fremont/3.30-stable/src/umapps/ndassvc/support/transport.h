#pragma once
#include <winsock2.h>

struct ITransport
{
	// virtual BOOL Accept(LPOVERLAPPED lpOverlapped) = 0;
	virtual BOOL Send(
		LPCVOID lpBuffer, 
		DWORD cbToSend, 
		LPDWORD lpcbSent, 
		LPOVERLAPPED lpOverlapped = NULL, 
		DWORD dwFlags = 0) = 0;
	
	virtual BOOL Receive(
		LPVOID lpBuffer, 
		DWORD cbToReceive, 
		LPDWORD lpcbReceived,
		LPOVERLAPPED lpOverlapped = NULL, 
		LPDWORD lpdwFlags = NULL) = 0;
};

class CStreamSocketTransport :
	public ITransport
{
protected:
	SOCKET m_sock;

public:
	CStreamSocketTransport(SOCKET sock);
	// virtual BOOL Accept(LPOVERLAPPED lpOverlapped = NULL);
	virtual BOOL Send(
		LPCVOID lpBuffer, 
		DWORD cbToSend, 
		LPDWORD lpcbSent, 
		LPOVERLAPPED lpOverlapped = NULL, 
		DWORD dwFlags = 0);

	virtual BOOL Receive(
		LPVOID lpBuffer, 
		DWORD cbToReceive, 
		LPDWORD lpcbReceived, 
		LPOVERLAPPED lpOverlapped = NULL, 
		LPDWORD lpdwFlags = NULL);
};

class CNamedPipeTransport : 
	public ITransport
{
protected:
	static const DWORD TRANSMIT_TIMEOUT = 5000; // 5 sec
	HANDLE m_hPipe;

public:
	CNamedPipeTransport(HANDLE hExistingPipe);

	virtual BOOL Accept(
		LPOVERLAPPED lpOverlapped = NULL);

	virtual BOOL Send(
		LPCVOID lpBuffer, 
		DWORD cbToSend, 
		LPDWORD lpcbSent, 
		LPOVERLAPPED lpOverlapped = NULL, 
		DWORD dwFlags = 0);

	virtual BOOL Receive(
		LPVOID lpBuffer, 
		DWORD cbToReceive, 
		LPDWORD lpcbReceived, 
		LPOVERLAPPED lpOverlapped = NULL, 
		LPDWORD lpdwFlags = 0);
};

