
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NDDRVINST_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NDDRVINST_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef NDINST_EXPORTS
#define NDINST_API __declspec(dllexport)
#else
#define NDINST_API __declspec(dllimport)
#endif

NDINST_API UINT __stdcall NDMsiUninstallDriver(MSIHANDLE hInstall);
NDINST_API UINT __stdcall NDMsiCheckDriver(MSIHANDLE hInstall);
NDINST_API UINT __stdcall NDMsiInstallDriver(MSIHANDLE hInstall);

#define ADMIN_UID	"{08687EE0-663A-4535-8875-9A1037278AF4}"
#define AGTOOL_UID	"{A643ACEA-03D9-4B08-9F45-A95650A55C23}"
