// xmrlocal.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../inc/dvdcopy.h"

#define DEVSIZE			512
#define PATHSIZE		2048

static int opt_v;

static void usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "jukeboxtest DVD_SOURCE_FILE  TARGET_DIRECTORY \n");
}

static void version(void)
{
	fprintf(stderr, "version 1.0\n");
	return;
}

static char * safe_strncpy(char *dst, const char * src, size_t size)
{
	errno_t err = 0;
	dst[size-1] = '\0';
	err = strncpy_s(dst, size, src, size-1);
	if(err != 0) {
		return NULL;
	}
	return dst;
}


int main(int argc, char* argv[])
{
	char	dvd_dev_name[DEVSIZE];
	char	target_dir_name[PATHSIZE];

	if(argc < 3) 
		return -1;

	/* Find any options. */
	argc--;
	argv++;
	while (argc && *argv[0] == '-') {
		if (!strcmp(*argv, "-v"))
			opt_v = 1;

		if (!strcmp(*argv, "-V") || !strcmp(*argv, "-version") ||
		    !strcmp(*argv, "--version"))
			version();

		if (!strcmp(*argv, "-?") || !strcmp(*argv, "-h") ||
		    !strcmp(*argv, "-help") || !strcmp(*argv, "--help")) {
			usage();
			return -1;
		}

		argv++;
		argc--;
	}	

	safe_strncpy(dvd_dev_name, *argv++, DEVSIZE);
	safe_strncpy(target_dir_name, *argv++, PATHSIZE);

	return FileDVDCopytoMedia(dvd_dev_name, target_dir_name);	
}

