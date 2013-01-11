#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "argp.h"

int get_named_arg_pos(int argc, TCHAR** argv, LPCTSTR arg_name, LPCTSTR* arg_value_ptr)
{
	int i;
	int arg_name_len = lstrlen(arg_name);

	for (i = 0; i < argc; ++i)
	{
		int argv_len = lstrlen(argv[i]);
		if (arg_name_len < argv_len)
		{
			if (_T('/') == argv[i][0] || _T('-') == argv[i][0])
			{
				if (_T(':') == argv[i][arg_name_len + 1] || 
					_T('\0') == argv[i][arg_name_len + 1])
				{
					if (0 == _tcsnicmp(arg_name, &argv[i][1], arg_name_len))
					{
						if (arg_value_ptr)
						{
							*arg_value_ptr = (_T(':') == argv[i][arg_name_len + 1]) ?
								&argv[i][arg_name_len + 2] : NULL;
						}
						return i;
					}
				}
			}
		}
	}

	return -1;
}

int get_unnamed_arg_pos(int argc, TCHAR** argv, int pos)
{
	int i, cp;
	for (i = 0, cp = 0; i < argc; ++i)
	{
		if (_T('/') == argv[i][0] || _T('-') == argv[i][0]) continue;
		if (pos == cp) return i;
		++cp;
	}
	return -1;
}

int get_unnamed_arg_count(int argc, TCHAR** argv)
{
	int i, count = 0;
	for (i = 0; i < argc; ++i)
	{
		if (_T('/') == argv[i][0] || _T('-') == argv[i][0]) continue;
		++count;
	}
	return count;
}

int argp_cmd_part_len(LPCTSTR part)
{
	LPCTSTR pch = part;
	int len = 0;
	while (_T(' ') != *pch && _T('\0') != *pch)
	{
		++pch;
		++len;
	}
	return len;
}

int arpg_cmd_part_count(LPCTSTR cmds)
{
	LPCTSTR pch = cmds;
	INT iCount = 1;
	while (_T('\0') != *pch)
	{
		++pch;
		if (_T(' ') == *pch) ++iCount;
	}
	return iCount;
}

LPCTSTR argp_find_part_in_cmds(LPCTSTR cmds, int pos)
{
	LPCTSTR pch = cmds;
	int i;
	for (i = 0; i < pos; ++i)
	{
		while (_T(' ') != *pch && _T('\0') != *pch) ++pch;
		if (_T('\0') == *pch) return pch;
		++pch;
	}
	return pch;
}

BOOL argp_is_arg_in_cmds(LPCTSTR cmds, int pos, LPCTSTR arg)
{
	LPCTSTR cur_cmd = argp_find_part_in_cmds(cmds, pos);
	int part_len = argp_cmd_part_len(cur_cmd);
	int arg_len = lstrlen(arg);
	if (arg_len <= part_len)
		return (0 == _tcsnicmp(cur_cmd, arg, arg_len));
	else
		return FALSE;
}

BOOL argp_is_arg_cmd_type(LPCTSTR arg)
{
	return 
		(arg[0] >= _T('A') && arg[0] <= _T('Z')) ||
		(arg[0] >= _T('a') && arg[0] <= _T('z'));
}
