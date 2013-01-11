// Gen_randnum.cpp : Defines the entry point for the console application.
//
//		Gen_randnum	
//				generate random test number
//				output file format
//						op : sector : size(in sector)
//				output is stdout or output file	
//				options
//						-p pattern
//							-p 1 --> sequential test
//							-p 2 --> random test
//						-op operation
//							-op 1 --> read
//							-op 2 --> write
//							-op 3 --> read|write
//						-ss start sector
//						-ms max sector
//						-s  size per op
//						-c	count
//						-o	output file

#include	<stdio.h>
#include	<fcntl.h>
#include	<io.h>
#include	<direct.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<malloc.h>
#include	<time.h>
#include	"..\Do_Test\Perf_Test.h"

#define	DEF_PATTERN			P_SEQ
#define	DEF_OP				OP_RANDOM
#define	DEF_START_SECTOR	0
#define DEF_MAX_SECTOR		(120 * (1 << 20) * 2)
#define DEF_MAX_SIZE		128
#define DEF_COUNT			(1 << 10)
#define	DEF_READ_RATIO		0.5

void Print_Usage();

int main(int argc, char* argv[])
{
	long	pattern			= DEF_PATTERN;
	int		op				= DEF_OP;

	__int64	start_sector	= DEF_START_SECTOR;
	__int64	max_sector		= DEF_MAX_SECTOR;
	long	sector_range;

	long	max_size		= DEF_MAX_SIZE;
	long	count			= DEF_COUNT;
	char	*out_file_name	= NULL;
	FILE	*OUT;

	float	read_ratio		= DEF_READ_RATIO;

	long	i;

	argc--; argv++;
	while(argc > 0) {
		if(!strcmp("-p", *argv)) {
			argc--; argv++;
			pattern = atoi(*argv);
			argc--; argv++;
			if((pattern < 1) || (pattern > 2)) {
				fprintf(stderr, "Invalid pattern %d\n", pattern);
				Print_Usage();
			}
		} else if(!strcmp("-op", *argv)) {
			argc--; argv++;
			op = atoi(*argv);
			argc--; argv++;
			if((op < 1) || (op > 3)) {
				fprintf(stderr, "Invalid OP code : %x\n", op);
				Print_Usage();
			}
		} else if(!strcmp("-ss", *argv)) {
			argc--; argv++;
			start_sector = _atoi64(*argv);
			argc--; argv++;
		} else if(!strcmp("-ms", *argv)) {
			argc--; argv++;
			max_sector = _atoi64(*argv);
			argc--; argv++;
		} else if(!strcmp("-s", *argv)) {
			argc--; argv++;
			max_size = atoi(*argv);
			argc--; argv++;
		} else if(!strcmp("-c", *argv)) {
			argc--; argv++;
			count = atoi(*argv);
			argc--; argv++;
		} else if(!strcmp("-rr", *argv)) {
			argc--; argv++;
			read_ratio = (float) atof(*argv);
			argc--; argv++;
		} else if ( !strcmp("-o", *argv) ) {
			argc--; argv++;
			out_file_name = *argv;
			argc--; argv++;
		} else {
			Print_Usage();
			exit(-1);
		}
	}
	
	
	if(!out_file_name) OUT = stdout;
	else {
		OUT = fopen(out_file_name,"w+");
		if(!OUT) {
			fprintf(stderr,"Error open out_file %s \n",out_file_name);
			exit(-1);
		}
	}

	//random number init
	srand( (unsigned)time( NULL ) );

	sector_range = (long) (max_sector - start_sector);

	fprintf(stderr, "Generating Test Pattern...\n");
	fprintf(stderr, "Access pattern : %s\n", (pattern == 1) ? "seqential" : "random");
	fprintf(stderr, "Operation : %s\n", (op == 1) ? "read" : ( (op == 2) ? "write" : "random" ) );
	fprintf(stderr, "start sector : %I64d\n", start_sector);
	fprintf(stderr, "max sector : %I64d\n", max_sector);
	fprintf(stderr, "sector range : %ld\n", sector_range);
	fprintf(stderr, "max size : %ld\n", max_size);
	fprintf(stderr, "count : %ld\n", count);
	for(i = 0; i < count; i++) {
		int		t_op;
		__int64	t_sector;
		int		t_size;

		t_op = (op == OP_RANDOM) ? ( (((float)rand()/RAND_MAX) < read_ratio) ? OP_READ : OP_WRITE ) : op;
		t_sector = (pattern == P_SEQ) ? start_sector + max_size * i : start_sector + (rand() * rand()) % sector_range;
		t_size = (pattern == P_SEQ) ? max_size : rand() % max_size + 1;
		fprintf(OUT,"%d %10I64d %5d\n", t_op, t_sector, t_size);
	
	}

	fclose(OUT);
	
	return 0;
}

void Print_Usage()
{
	fprintf(stderr,"Usage: Gen_randnum.exe [-p pattern][-op operation] [-ss start sector] [-ms max sector] [-s size per op] [-c count] [-rr read ratio] [-o out file]\n");
	exit(-1);
}