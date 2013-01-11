// StartTest.cpp : Defines the entry point for the console application.
//

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<malloc.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include <winsock2.h>
#include <ws2tcpip.h>

int main(int argc, char* argv[])
{

	int		sockid;
	char	tempbuff[10] = "ILGU";
	sockaddr_in bin_addr;
	int	result;
	int flag;
	flag = TRUE;
	int i ;
	WSADATA						wsaData;
	
	result = WSAStartup( MAKEWORD(2, 0), &wsaData );
	
	sockid = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	setsockopt(sockid,SOL_SOCKET,SO_BROADCAST,(char *)&flag,sizeof(int));

	if(!sockid) {
		fprintf(stderr,"Error in open udp socket\n");
		goto e_out1;
	}
	bin_addr.sin_family = AF_INET;
	bin_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	bin_addr.sin_port = htons(20010);

	for(i = 0; i < 100; i++)
	{
		int err_result;
		result =sendto(sockid,tempbuff,strlen(tempbuff),0,(struct  sockaddr *)&bin_addr,sizeof(bin_addr));
		err_result = WSAGetLastError();
		if(err_result)fprintf(stderr," %d times size %d\n",i,err_result);
	}

e_out1:
	WSACleanup();
	return 0;
}