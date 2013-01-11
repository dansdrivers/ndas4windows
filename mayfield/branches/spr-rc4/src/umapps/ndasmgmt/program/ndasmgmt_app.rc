// Do not open this file from Resource Editor
#ifndef APSTUDIO_INVOKED
#include <windows.h>

LANGUAGE LANG_NEUTRAL, SUBLANG_NEUTRAL
#pragma code_page(1252)

#include "ndasmgmt.ver"
#include "ndasverp.h"

// #define VER_NO_PRODUCT_VERSION
#define VER_FILETYPE			VFT_APP
#define VER_FILESUBTYPE			VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR "NDAS Device Management"
#define VER_INTERNALNAME_STR	"ndasmgmt.exe"

#include "ndascommon.ver"

/////////////////////////////////////////////////////////////////////////////
//
// String Table
//

#define IDR_MAINFRAME 128

STRINGTABLE 
BEGIN
    IDR_MAINFRAME           "ndasmgmt"
END

/////////////////////////////////////////////////////////////////////////////
//
// Icon
//

// Icon with lowest ID value placed first to ensure application icon
// remains consistent on all systems.
IDR_MAINFRAME           ICON                    "ndasmgmt.ico"

/////////////////////////////////////////////////////////////////////////////
//
// PROCESS MANIFEST
//

CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "ndasmgmt.manifest"

#include "ndasmgmt_appiniterr.h"

LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
STRINGTABLE
BEGIN
	IDS_APP_INIT_ERROR_TITLE				"Application Initialization Error"
	IDS_APP_INIT_ERROR_ADMIN_PRIV_REQUIRED	"This application requires administrative privileges.\r\nLog on as a user in Administrators Group and run this application again."
	IDS_APP_INIT_ERROR_OUT_OF_MEMORY		"Application cannot start due to the following reason.\r\nOut of memory.\r\nClose other applications and try again."
	IDS_APP_INIT_ERROR_NO_NDASUSER			"Loading NDASUSER.DLL failed.\r\nPlease check the installation of the software."
	IDS_APP_INIT_ERROR_INVALID_NDASUSER		"Invalid NDASUSER.DLL is installed.\r\nPlease check the installation of the software."
	IDS_APP_INIT_ERROR_CREATE_WINDOW		"Creating a Window failed. Possibly system is running out of memory.\r\nClose other applications and try again."
	IDS_APP_INIT_ERROR_NO_RESOURCE			"No compatible resources DLL's are available.\r\nPlease check the installation of the software."
	IDS_APP_INIT_ERROR_RESOURCE_LOAD		"Cannot load resource DLL.\r\nPlease check the installation of the software."
END

#endif /* APSTUDIO_INVOKED */
