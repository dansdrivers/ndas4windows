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

/* returns 0 for non-match, 1 for partial match, and 2 for exact match */
int argp_is_arg_in_cmds(LPCTSTR cmds, int pos, LPCTSTR arg);

/* returns the pointer to the first character of the 
  'pos'th token from the command string */
LPCTSTR argp_find_part_in_cmds(LPCTSTR cmds, int pos);

/* returns the length of the part string which its part ends with 
   white space or null character */
int argp_cmd_part_len(LPCTSTR part);

/* returns the number of tokens in command string */
int arpg_cmd_part_count(LPCTSTR cmds);

#ifdef __cplusplus
}
#endif

