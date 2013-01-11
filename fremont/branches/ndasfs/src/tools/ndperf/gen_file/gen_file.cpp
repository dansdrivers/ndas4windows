// Gen_file.cpp : Defines the entry point for the console application.
//

#include	<stdio.h>
#include	<io.h>
#include	<fcntl.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<malloc.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>

#define BLK_SIZE	(1 << 13)
#define DEF_BLK_M	(120 * ( 1 << 10))
#define MEGA		(1 << 7)

void Print_Usage();


int main(int argc, char* argv[])
{
	int		OUT;
	char	*output_file_name = NULL;
	long	max_blk = DEF_BLK_M;
	char	buff[BLK_SIZE];
	long	i;

	argc--; argv++;


	while(argc > 0)
	{
		if ( !strcmp("-o",*argv))
		{
			argc--; argv++;
			output_file_name = *argv;
			argc--; argv++;
		}else if (!strcmp("-s",*argv))
		{
			argc--; argv++;
			max_blk = atoi(*argv);
			argc--; argv++;
		}else 
		{
			Print_Usage();
			exit(-1);
		}

	}

	if(! output_file_name ) OUT = _fileno(stdout);
	else{
		OUT = open(output_file_name, _O_CREAT|_O_TRUNC|
							_O_WRONLY|_S_IREAD | _S_IWRITE);
		if(-1 == OUT) 
		{
			fprintf(stderr,"Error in creat test file\n");
			exit(-1);
		}
	}

	for(i = 0; i< BLK_SIZE; i++)
		buff[i]='A';

	for(i = 0; i < max_blk * MEGA; i++)
	{
		write(OUT,buff,BLK_SIZE);
	}
	close(OUT);
	return 0;
}

void Print_Usage()
{
	fprintf(stderr,"\n[Usnage]: Gen_file.exe [-o output file][-s size (M)]\n");
	exit(-1);
}