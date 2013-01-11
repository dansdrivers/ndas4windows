// LanScsiCli.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "LanScsiCli.h"
#define	_LPX_

TARGET_DATA		PerTarget[NR_MAX_TARGET];

void
PrintError(
		   int		ErrorCode,
		   PCHAR	prefix
		   )
{
	LPVOID lpMsgBuf;
	
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
		);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	DebugPrint( 1, ("%s: %s", prefix, (LPCSTR)lpMsgBuf));
	
	//MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

int 
GetInterfaceList(
				 LPSOCKET_ADDRESS_LIST	socketAddressList,
				 DWORD					socketAddressListLength
				 )
{
	int					iErrcode;
	SOCKET				sock;
	DWORD				outputBytes;
	
	
	sock = socket(AF_LPX, SOCK_STREAM, IPPROTO_LPXTCP);
	
	if(sock == INVALID_SOCKET) {
		PrintError(WSAGetLastError(), "GetInterfaceList: socket ");
		return SOCKET_ERROR;
	}
	
	outputBytes = 0;
	
	iErrcode = WSAIoctl(
		sock,							// SOCKET s,
		SIO_ADDRESS_LIST_QUERY, 		// DWORD dwIoControlCode,
		NULL,							// LPVOID lpvInBuffer,
		0,								// DWORD cbInBuffer,
		socketAddressList,				// LPVOID lpvOutBuffer,
		socketAddressListLength,		// DWORD cbOutBuffer,
		&outputBytes,					// LPDWORD lpcbBytesReturned,
		NULL,							// LPWSAOVERLAPPED lpOverlapped,
		NULL							// LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
		);
	
	if(iErrcode == SOCKET_ERROR) {
		PrintError(WSAGetLastError(), "GetInterfaceList: WSAIoctl ");
	}
	
	closesocket(sock);
	
	return iErrcode;
}

BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   OUT	SOCKET				*pSocketData
			   )
{
	int						iErrcode;
	SOCKADDR_LPX			socketLpx;
	SOCKADDR_LPX			serverSocketLpx;
	LPSOCKET_ADDRESS_LIST	socketAddressList;
	DWORD					socketAddressListLength;
	int						i;
	SOCKET					sock;
	
	socketAddressListLength = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)
		+ sizeof(SOCKET_ADDRESS) * MAX_SOCKETLPX_INTERFACE
		+ sizeof(SOCKADDR_LPX) * MAX_SOCKETLPX_INTERFACE;
	
	socketAddressList = (LPSOCKET_ADDRESS_LIST)malloc(socketAddressListLength);
	
	//
	// Get NICs
	//
	iErrcode = GetInterfaceList(
		socketAddressList,
		socketAddressListLength
		);
	
	if(iErrcode != 0) {
	DebugPrint( 1, ( "[NetDiskTest]MakeConnection: Error When Get NIC List!!!!!!!!!!\n"));
		free(socketAddressList);
		return FALSE;
	} else {
		DebugPrint( 1, ( "[NetDiskTest]MakeConnection: Number of NICs : %d\n", socketAddressList->iAddressCount));
	}
	
	//
	// Find NIC that is connected to LanDisk.
	//
	for(i = 0; i < socketAddressList->iAddressCount; i++) {
		
		socketLpx = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);
		
		DebugPrint(1, ("[NetDiskTest]MakeConnection: NIC %02d: Address %02X:%02X:%02X:%02X:%02X;%02X\n",
			i,
			socketLpx.LpxAddress.Node[0],
			socketLpx.LpxAddress.Node[1],
			socketLpx.LpxAddress.Node[2],
			socketLpx.LpxAddress.Node[3],
			socketLpx.LpxAddress.Node[4],
			socketLpx.LpxAddress.Node[5]
			));
		
		sock = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		if(sock == INVALID_SOCKET) {
			PrintError(WSAGetLastError(), "MakeConnection: socket ");
			free(socketAddressList);
			return FALSE;
		}
		
		socketLpx.LpxAddress.Port = 0; // unspecified
		
		// Bind NIC.
		iErrcode = bind(sock, (struct sockaddr *)&socketLpx, sizeof(socketLpx));
		if(iErrcode == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "MakeConnection: bind ");
			closesocket(sock);
			sock = INVALID_SOCKET;
			
			continue;
		}
		
		// LanDisk Address.
		memset(&serverSocketLpx, 0, sizeof(serverSocketLpx));
		serverSocketLpx.sin_family = AF_LPX;
		memcpy(serverSocketLpx.LpxAddress.Node, pAddress->Node, 6);
		serverSocketLpx.LpxAddress.Port = htons(LPX_PORT_NUMBER);
		
		iErrcode = connect(sock, (struct sockaddr *)&serverSocketLpx, sizeof(serverSocketLpx));
		if(iErrcode == SOCKET_ERROR) {
			PrintError(WSAGetLastError(), "MakeConnection: connect ");
			closesocket(sock);
			sock = INVALID_SOCKET;
			
			DebugPrint( 1, ( "[NetDiskTest]MakeConnection: LanDisk is not connected with NIC Number %d\n", i));
			
			continue;
		} else {
			*pSocketData = sock;
			
			break;
		}
	}
	
	if(sock == INVALID_SOCKET) {
		DebugPrint( 1, ( "[NetDiskTest]MakeConnection: No LanDisk!!!\n"));
		free(socketAddressList);
		return FALSE;
	}
	free(socketAddressList);
	return TRUE;
}
