#pragma once

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

int get_named_arg_pos(int argc, TCHAR** argv, LPCTSTR arg_name, LPCTSTR* arg_value_ptr);
int get_unnamed_arg_pos(int argc, TCHAR** argv, int pos);
int get_unnamed_arg_count(int argc, TCHAR** argv);

BOOL argp_is_arg_cmd_type(LPCTSTR arg);
BOOL argp_is_arg_in_cmds(LPCTSTR cmds, int pos, LPCTSTR arg);
LPCTSTR argp_find_part_in_cmds(LPCTSTR cmds, int pos);
int argp_cmd_part_len(LPCTSTR part);
int arpg_cmd_part_count(LPCTSTR cmds);

#ifdef __cplusplus
}
#endif

