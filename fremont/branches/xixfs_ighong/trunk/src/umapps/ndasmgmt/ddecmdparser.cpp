#include "stdafx.h"
#include <windows.h>
#include <tchar.h>
#include "ddecmdparser.h"

#ifdef DDECMDPARSER_TEST

int __cdecl _tmain(int argc, TCHAR** argv)
{
	CDdeCommandParser parser;

	LPCTSTR cmd = (argc > 1) ? argv[1] : _T("[open(\"C:\\program files\\my.ndas\")]");
	LPCTSTR lpNext = cmd;
	bool success = false;
	_tprintf(_T("cmd=%s\n"), cmd);
	success = parser.Parse(lpNext, &lpNext);
	_tprintf(_T("opcode=%s\n"), parser.GetOpCode());
	_tprintf(_T("param=%s\n"), parser.GetParam());
	if (!success)
	{
		_tprintf(_T("Failed\n"));
	}
	return 0;
}

#endif
