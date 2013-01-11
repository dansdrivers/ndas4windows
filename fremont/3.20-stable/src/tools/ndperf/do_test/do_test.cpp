// Do_perform.cpp : Defines the entry point for the console application.
//
//		Do_perform
//				execute test program 
//				Do_perform test_file [options]
//					
//					options
//						input is stdin or input data
//						-i input data
//						output is stdout or output log
//						-r output log						
//
#include	<stdio.h>


#include	<fcntl.h>
#include	<io.h>
#include	<direct.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<malloc.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include <winsock2.h>
#include <ws2tcpip.h>


#include	"Perf_Test.h"

#define		BLK_SIZE	(1 << 13)
#define		MAX_LinE	300
#define		SECTOR		(1 << 9)

void	print_time(FILE * out);
int		READ(int FILE,__int64 sector, int size);
int		WRITE(int FILE,__int64 sector, int size);
void	Print_Usage();

__int64	rsize, wsize;

int main(int argc, char* argv[])
{

	FILE		*in;
	FILE		*out;
	int			TEST;
	char		*input_file_name = NULL;
	char		*output_file_name = NULL;

	int			op;
	__int64		sector;
	int			size;
	
	clock_t		start, finish;
	double		duration;

	char		do_sync = TRUE;
	
	
	argc--; argv++;

	if(argc < 1) 
	{
		Print_Usage();
		exit(-1);
	}

	if(argv[0][0] == '-')
	{
		Print_Usage();
		exit(-1);
	}

	TEST = open(*argv,_O_RDWR|_O_BINARY );

	if(-1 == TEST){
		fprintf(stderr,"Error in open test file %s\n",*argv);
		exit(-1);
	}

	argc--; argv++;

	while(argc > 0)
	{
		if ( !strcmp("-i", *argv) )
		{
			argc--; argv++;
			input_file_name = *argv;
			argc--; argv++;

		}else if ( !strcmp("-o", *argv) )
		{
			argc--; argv++;
			output_file_name = *argv;
			argc--; argv++;
		}else if ( !strcmp("-nosync", *argv) )
		{
			argc--; argv++;
			do_sync = FALSE;
		} else
		{
			Print_Usage();
			goto e_out1;
		}
	}
	if(!input_file_name) in = stdin;
	else {
		in = fopen(input_file_name,"r");
		if (!in) 
		{
			fprintf(stderr,"Error in open input file :%s\n", input_file_name);
			goto e_out1;
		}
	}

	if(!output_file_name) out = stdout;
	else {
		in = fopen(output_file_name,"a");
		if (!in) 
		{
			fprintf(stderr,"Error in open output file :%s\n", output_file_name);
			goto e_out2;
		}
	}

	if(do_sync) {
		int			sockid;
		int			sock_read;
		char		tempbuff[10];
		sockaddr_in	bin_addr;
		int			result;
		int			addr_size;
		WSADATA		wsaData;

		result = WSAStartup( MAKEWORD(2, 0), &wsaData );

		sockid = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	
		if(!sockid) {
			fprintf(stderr,"Error in open udp socket\n");
			goto e_out2;
		}
		result = 0;
		bin_addr.sin_family = AF_INET;
		bin_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		bin_addr.sin_port = htons(20010);
	
		result = bind(sockid,(struct sockaddr *)&bin_addr,sizeof(bin_addr));
		if(result) {
			fprintf(stderr,"Error in open udp bind\n");
			fprintf(stderr, "error code : %d\n", WSAGetLastError());
			goto e_out2;
		}

		fprintf(stderr, "Waiting for Start signal\n");
		do{
			int err_result;
			addr_size = sizeof(bin_addr);
			sock_read = recvfrom(sockid, tempbuff, 10,0,(struct sockaddr *)&bin_addr,&addr_size);
			err_result = WSAGetLastError();
			if(err_result) fprintf(stderr,"error num %d \n",err_result);
		}while( sock_read <= 0 );
		WSACleanup();
	}

	rsize = wsize = 0;
	
	print_time(out);
	start = clock();

	while( fscanf(in,"%d %I64d %d",&op,&sector,&size) != EOF )
	{
		if(op == OP_READ) {
			if( READ(TEST,sector,size) == -1) goto e_out3;
		}
		else if(op == OP_WRITE) {
			if( WRITE(TEST,sector,size) == -1) goto e_out3;
		}
		else {
			goto e_out3;
		}
	}

	finish = clock();
	duration = (double)(finish - start) / CLOCKS_PER_SEC;
	fprintf(out, "\t[TIME] : %10.1f seconds\n", duration );
	fprintf(out, "\tRead : %I64d, \tWrite : %I64d\n", rsize, wsize);
	
	fclose(in);
	fclose(out);
	close(TEST);
	return 0;

e_out3:
	fprintf(stderr, "Error 3\n");
	fclose(out);
	exit(-1);
e_out2:
	WSACleanup();
	fprintf(stderr, "Error 2\n");
	fclose(in);
e_out1:
	fprintf(stderr, "Error 1\n");
	close(TEST);
	exit(-1);

}

int READ(int TEST,__int64 sector, int size)
{
	__int64	result = size * SECTOR;
	int		opsize;
	int		tmp;
	__int64 pos = 0L;
	char	buff[BLK_SIZE];

	pos = _lseeki64( TEST, (__int64)(sector * SECTOR), SEEK_SET);
	if(pos == -1L) return -1;

	rsize += result;

	while(  result > 0  ) {
		opsize = (int) ((result > BLK_SIZE) ? BLK_SIZE : result);
		tmp = read(TEST,buff,opsize);
		result -= tmp;
	}
	return 0;
}

int WRITE(int TEST, __int64 sector, int size)
{
	__int64	result = size * SECTOR;
	int		opsize;
	int		tmp;
	__int64	pos = 0L;
	char	buff[BLK_SIZE];

	pos = _lseeki64( TEST, (__int64)(sector * SECTOR), SEEK_SET);
	if(pos == -1L) return -1;

	wsize += result;

	while(  result > 0  )
	{
		opsize = (int) ((result > BLK_SIZE) ? BLK_SIZE : result);
		tmp = write(TEST, buff, opsize);
		result -= tmp;
	}
	return 0;
}


void print_time(FILE * out)
{
	time_t ltime;

	_tzset();

	time( &ltime );
	fprintf(out, "\n\n\n[Time and date]:\t%s\n", ctime( &ltime ) );
}


void Print_Usage()
{
	fprintf(stderr,"[USAGE] : Do_Test test_file_name [-i input data][-o output file][-nosync]\n");
}