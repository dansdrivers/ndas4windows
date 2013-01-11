// Stress_LPX.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <nb30.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "SocketLpx.h"

#define DATA_SIZE		1024*2
#define DATA_SIZE2		64
#define LPX_SERVER_PORT 0x50011 // DSAP_NETBIOS_OVER_LLC
#define MAX_THREAD		128
#define MAX_CLIENT		10

int 
TcpMain(int argc, char* argv[]);

int
UdpMain(int argc, char * argv[]);

int 
GetInterfaceList(
	LPSOCKET_ADDRESS_LIST socketAddressList
	);


void 
PrintErrorCode(LPTSTR strPrefix, DWORD	ErrorCode)
{
	LPTSTR lpMsgBuf;
	
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
	printf(strPrefix);

	printf(lpMsgBuf);
	
	// Free the buffer.
	LocalFree( lpMsgBuf );
}


DWORD WINAPI ThreadFunc( LPVOID lpParam ) ;
DWORD WINAPI ThreadFunc2( LPVOID lpParam ) ;

typedef struct _TP{ 
	SOCKET Sock;
} TP, * PTP ;


int 
main(int argc, char* argv[])
{


	return TcpMain(argc, argv);
	//return UdpMain(argc, argv);
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
		PrintErrorCode("socket ", WSAGetLastError());
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
		PrintErrorCode("WSAIoctl ", WSAGetLastError());
	}

	closesocket(sock);
	return iErrcode;
}


int 
UdpMain(int argc, char* argv[])
{
	int						i;
	CHAR					buffer[DATA_SIZE2];
	int						iErrcode;
	WSADATA					wsaData;
	SOCKET					sock;
	SOCKADDR_LPX			lpxAddr;
	LPSOCKET_ADDRESS_LIST	socketAddressList;
	DWORD					socketAddressListLength;
	HANDLE					hE_Thread;
	DWORD					dwE_ThreadId;
	BOOL					ISClient = FALSE;
	INT					CliCount = 0;
	UINT				uiListenPort = 10002;
	
	hE_Thread = (HANDLE)0;
	dwE_ThreadId = 0;
	
	
	for(i = 0; i < DATA_SIZE; i++) 
		buffer[i] = (UCHAR)i;

	iErrcode = WSAStartup( MAKEWORD(2, 0), &wsaData );

	if (iErrcode != 0)
	{
		printf("BAD 1 !!!!!!!!!!\n");
		
		return iErrcode;
	}

	socketAddressListLength = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)
							+ sizeof(SOCKET_ADDRESS)*MAX_SOCKETLPX_INTERFACE
							+ sizeof(SOCKADDR_LPX)*MAX_SOCKETLPX_INTERFACE;

	socketAddressList = (LPSOCKET_ADDRESS_LIST)malloc(socketAddressListLength);
							
	iErrcode = GetInterfaceList(
				socketAddressList,
				socketAddressListLength
				);

	if (iErrcode != 0)
	{
		printf("BAD 2 !!!!!!!!!!\n");
		
		return iErrcode;
	}
	
	sock = WSASocket(
			AF_LPX, 
			SOCK_DGRAM, 
			IPPROTO_LPXUDP,
			NULL,
			NULL,
			WSA_FLAG_OVERLAPPED
			);

	if(sock == INVALID_SOCKET) {
		PrintErrorCode(TEXT("UDP socket "), WSAGetLastError());
		return -1;
	}
	
	lpxAddr = *(PSOCKADDR_LPX)(socketAddressList->Address[0].lpSockaddr);

	lpxAddr.LpxAddress.Port = htons(uiListenPort);
	
	iErrcode = bind(sock, (struct sockaddr *)&lpxAddr, sizeof(lpxAddr));
	
	if(iErrcode == SOCKET_ERROR) {
		PrintErrorCode("UDP bind ", WSAGetLastError());
		closesocket(sock);
		sock = INVALID_SOCKET;
		return -1;
	}
	


	closesocket(sock);
	printf("UDP oK create sock\n");
	return 0;
}


int 
TcpMain(int argc, char* argv[])
{
	int						i;
	CHAR					buffer[DATA_SIZE2];
	int						iErrcode;
	WSADATA					wsaData;
	SOCKET					sock;
	SOCKADDR_LPX			lpxAddr;
	CHAR					command[100];
	LPSOCKET_ADDRESS_LIST	socketAddressList;
	DWORD					socketAddressListLength;
	BOOL					ISClient = FALSE;
	INT					CliCount = 0;	
	

	
	for(i = 0; i < DATA_SIZE; i++) 
		buffer[i] = (UCHAR)i;

	iErrcode = WSAStartup( MAKEWORD(2, 0), &wsaData );
	if (iErrcode != 0)
	{
		printf("BAD 1 !!!!!!!!!!\n");
		
		return iErrcode;
	}


	socketAddressListLength = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)
							+ sizeof(SOCKET_ADDRESS)*MAX_SOCKETLPX_INTERFACE
							+ sizeof(SOCKADDR_LPX)*MAX_SOCKETLPX_INTERFACE;

	socketAddressList = (LPSOCKET_ADDRESS_LIST)malloc(socketAddressListLength);
							
	iErrcode = GetInterfaceList(
				socketAddressList,
				socketAddressListLength
				);

	if (iErrcode != 0)
	{
		printf("BAD 2 !!!!!!!!!!\n");
		
		return iErrcode;
	}

	for(i=0; i<socketAddressList->iAddressCount; i++)
	{
		lpxAddr = *(PSOCKADDR_LPX)(socketAddressList->Address[i].lpSockaddr);
		printf("Remote Address %02X%02X%02X%02X%02X%02X:%04X\n",
				lpxAddr.LpxAddress.Node[0],
				lpxAddr.LpxAddress.Node[1],
				lpxAddr.LpxAddress.Node[2],
				lpxAddr.LpxAddress.Node[3],
				lpxAddr.LpxAddress.Node[4],
				lpxAddr.LpxAddress.Node[5],
				ntohs(lpxAddr.LpxAddress.Port));
	}
	

	
	printf("tcp server or client\n");
	scanf("%s", command);


	if(command[0] == 's')
	{
		sock = socket(AF_LPX, SOCK_STREAM, IPPROTO_LPXTCP);
  		
		if(sock == INVALID_SOCKET) {
			PrintErrorCode("socket ", WSAGetLastError());
			return 0;
		}

		memset(&lpxAddr, 0, sizeof(lpxAddr));
	
		lpxAddr = *(PSOCKADDR_LPX)(socketAddressList->Address[0].lpSockaddr);
		lpxAddr.LpxAddress.Port = htons(LPX_SERVER_PORT);

		iErrcode = bind(sock, (struct sockaddr *)&lpxAddr, sizeof(lpxAddr));
		if(iErrcode == SOCKET_ERROR) {
			PrintErrorCode("bind ", WSAGetLastError());
			goto ErrorOut;
		}

		iErrcode = listen(sock, SOMAXCONN);
		if(iErrcode == SOCKET_ERROR) {
			PrintErrorCode("listen ", WSAGetLastError());
			goto ErrorOut;
		}

		printf("listened\n");
		while(1)
		{
			SOCKADDR_LPX			clientAddr;
			INT						clientAddrLen;
			SOCKET					aSock;
			PTP						param;
			HANDLE					hThread;
			DWORD					dwThreadId;

	


			param =(PTP)malloc(sizeof(TP));
			if(NULL == param) goto ErrorOut;

			clientAddrLen = sizeof(clientAddr);
			aSock = accept(sock, (struct sockaddr *)&clientAddr, &clientAddrLen);

			if(aSock == INVALID_SOCKET) {
				PrintErrorCode("accept ", WSAGetLastError());
				goto ErrorOut;
			}

			printf("Remote Address %02X%02X%02X%02X%02X%02X:%04X\n",
					clientAddr.LpxAddress.Node[0],
					clientAddr.LpxAddress.Node[1],
					clientAddr.LpxAddress.Node[2],
					clientAddr.LpxAddress.Node[3],
					clientAddr.LpxAddress.Node[4],
					clientAddr.LpxAddress.Node[5],
					ntohs(clientAddr.LpxAddress.Port));

			printf("accepted socket %d\n",aSock);
			// thread creation
			param->Sock = aSock;
			hThread = CreateThread( 
					NULL,                        // no security attributes 
					0,                           // use default stack size  
					ThreadFunc,                  // thread function 
					param,                // argument to thread function 
					0,                           // use default creation flags 
					&dwThreadId);                // returns the thread identifier 

			   // Check the return value for success. 

		   if (hThread == NULL) 
		   {
			  closesocket(aSock);
			  goto ErrorOut;
		   }


		}	
		
	} else 
	{

//		HANDLE					hE_Thread;
//		DWORD					dwE_ThreadId;
		SOCKET					sock;
		SOCKADDR_LPX			ServAddr;
		INT						ServAddrLen;
//		PTP						param;
		ISClient = TRUE;

				
		sock = socket(AF_LPX, SOCK_STREAM, IPPROTO_LPXTCP);  	
		if(sock == INVALID_SOCKET) {
			PrintErrorCode("socket ", WSAGetLastError());
			return 0;
		}

		memset(&ServAddr, 0, sizeof(ServAddr));

		ServAddr.sin_family = AF_LPX;
		
		ServAddr.LpxAddress.Node[0] = 0x00;
		ServAddr.LpxAddress.Node[1] = 0x0C;
		ServAddr.LpxAddress.Node[2] = 0x6E;
		ServAddr.LpxAddress.Node[3] = 0xB3;
		ServAddr.LpxAddress.Node[4] = 0x10;
		ServAddr.LpxAddress.Node[5] = 0x7D;
/*

		//sniper-test MAC Address
		ServAddr.LpxAddress.Node[0] = 0x00;
		ServAddr.LpxAddress.Node[1] = 0x40;
		ServAddr.LpxAddress.Node[2] = 0x75;
		ServAddr.LpxAddress.Node[3] = 0xFB;
		ServAddr.LpxAddress.Node[4] = 0xBD;
		ServAddr.LpxAddress.Node[5] = 0x40;
*/
		ServAddr.LpxAddress.Port = htons(LPX_SERVER_PORT);


		ServAddrLen = sizeof(ServAddr);

		printf("Connect start \n");
		iErrcode = connect(sock, (struct sockaddr *)&ServAddr, sizeof(ServAddr));
		if(iErrcode == SOCKET_ERROR) {
			getchar();
			PrintErrorCode("connect", WSAGetLastError());
			goto ErrorOut;
		}
		printf("Connect end \n");


		{
			int recvDataLen,sendDataLen;
			CHAR buffer[DATA_SIZE2];
			CHAR Rbuff[DATA_SIZE];
			int count = 0;
			printf("send start Client \n");

			while(count < 50){
				printf("send start \n");
				sendDataLen = send(sock, buffer, DATA_SIZE2, 0);
				if(sendDataLen == SOCKET_ERROR) {
					PrintErrorCode("LPXSTRESS SEND", WSAGetLastError());
					closesocket(sock);
					goto ErrorOut;
				}else {
					printf("send datalen = %d\n", sendDataLen);
				}
				printf("recv start \n");
				recvDataLen = recv(sock, Rbuff, DATA_SIZE, 0);
				if(recvDataLen == SOCKET_ERROR) {
					PrintErrorCode("LPXSTRESS RECV", WSAGetLastError());
					closesocket(sock);
					goto ErrorOut;
				} else {
					printf("receive Len = %d\n" ,recvDataLen);
				}
				count ++;
			}

		}
		closesocket(sock);
		/*
		param =(PTP)malloc(sizeof(TP));			
		param->Sock = sock;
		
		hE_Thread = CreateThread( 
				NULL,                        // no security attributes 
				0,                           // use default stack size  
				ThreadFunc2,                  // thread function 
				param,                // argument to thread function 
				CREATE_SUSPENDED ,                           // use default creation flags 
				&dwE_ThreadId);                // returns the thread identifier 

		   // Check the return value for success. 

	   if (hE_Thread == NULL) 
	   {
		  fprintf(stderr, "Cannot create thread\n");
		  closesocket(sock);
		   goto ErrorOut;
	   }
		*/	
			

	}


ErrorOut:
	WSACleanup();
	return 0;
}




DWORD WINAPI ThreadFunc( LPVOID lpParam ) 
{ 
	int recvDataLen,sendDataLen;
	SOCKET aSock;
	PTP  param;
	CHAR buffer[DATA_SIZE2];
	CHAR Rbuff[DATA_SIZE];
	param = (PTP)lpParam;
	aSock = param->Sock;
	
	printf("accepted socket %d\n",aSock);

	while(1){
		recvDataLen = recv(aSock, buffer, DATA_SIZE2, 0);
		if(recvDataLen == SOCKET_ERROR) {
			PrintErrorCode("LPXSTRESS RECV", WSAGetLastError());
			closesocket(aSock);
			goto ErrorOut;
		} else {
			printf("receive Len = %d\n" ,recvDataLen);
		}

		sendDataLen = send(aSock, Rbuff, DATA_SIZE, 0);
		if(sendDataLen == SOCKET_ERROR) {
			PrintErrorCode("LPXSTRESS SEND", WSAGetLastError());
			closesocket(aSock);
			goto ErrorOut;
		}else {
			printf("send datalen = %d\n", sendDataLen);
		}
	}
	closesocket(aSock);
	ExitThread(0);
	return 0;
ErrorOut:
	closesocket(aSock);
	ExitThread(-1);
	return -1;
} 

DWORD WINAPI ThreadFunc2( LPVOID lpParam ) 
{ 
	int recvDataLen,sendDataLen;
	SOCKET aSock;
	PTP  param;
	CHAR buffer[DATA_SIZE2];
	CHAR Rbuff[DATA_SIZE];
	param = (PTP)lpParam;
	aSock = param->Sock;
	printf("send start Client \n");

	while(1){
		printf("send start \n");
		sendDataLen = send(aSock, buffer, DATA_SIZE2, 0);
		if(sendDataLen == SOCKET_ERROR) {
			PrintErrorCode("LPXSTRESS SEND", WSAGetLastError());
			closesocket(aSock);
			goto ErrorOut;
		}else {
			printf("send datalen = %d\n", sendDataLen);
		}
		printf("recv start \n");
		recvDataLen = recv(aSock, Rbuff, DATA_SIZE, 0);
		if(recvDataLen == SOCKET_ERROR) {
			PrintErrorCode("LPXSTRESS RECV", WSAGetLastError());
			closesocket(aSock);
			goto ErrorOut;
		} else {
			printf("receive Len = %d\n" ,recvDataLen);
		}

	}
	closesocket(aSock);
	ExitThread(0);
	return 0;
ErrorOut:
closesocket(aSock);
ExitThread(-1);
	return -1;
} 



