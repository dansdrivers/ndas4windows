// test1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

extern int __cdecl lsmain(int argc, char* argv[]);

int _tmain(int argc, _TCHAR* argv[])
{
	return lsmain(argc, argv);
}

