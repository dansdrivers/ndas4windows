#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <direct.h>
#include <string.h>
#include <objbase.h>
#include <shlobj.h>
#include <limits.h>
#include <shellapi.h>
#include "gtools.h"
#include "compat.h"
#define Zero(a) memset(&(a),0,sizeof(a))

