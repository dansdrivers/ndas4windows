#pragma once
#include <msi.h>
//--------------------------------------------------------------------------------------
// MSI API -- delay load
//--------------------------------------------------------------------------------------

#define MSI_DLL _T("msi.dll")

#define MSIAPI_MsiSetInternalUI "MsiSetInternalUI"
typedef INSTALLUILEVEL (WINAPI* PFnMsiSetInternalUI)(INSTALLUILEVEL dwUILevel, HWND *phWnd);

#define MSIAPI_MsiInstallProductA "MsiInstallProductA"
typedef UINT (WINAPI* PFnMsiInstallProductA)(LPCSTR szPackagePath, LPCSTR szCommandLine);

#define MSIAPI_MsiInstallProductW "MsiInstallProductW"
typedef UINT (WINAPI* PFnMsiInstallProductW)(LPCWSTR szPackagePath, LPCWSTR szCommandLine);

#ifdef UNICODE
#define PFnMsiInstallProduct PFnMsiInstallProductW
#define MSIAPI_MsiInstallProduct MSIAPI_MsiInstallProductW
#else
#define PFnMsiInstallProduct PFnMsiInstallProductA
#define MSIAPI_MsiInstallProduct MSIAPI_MsiInstallProductA
#endif

#define MSIAPI_MsiApplyPatchA "MsiApplyPatchA"
typedef UINT (WINAPI* PFnMsiApplyPatchA)(LPCSTR szPatchPackage, LPCSTR szInstallPackage, INSTALLTYPE eInstallType, LPCSTR szCommandLine);

#define MSIAPI_MsiApplyPatchW "MsiApplyPatchW"
typedef UINT (WINAPI* PFnMsiApplyPatchW)(LPCWSTR szPatchPackage, LPCWSTR szInstallPackage, INSTALLTYPE eInstallType, LPCWSTR szCommandLine);

#ifdef UNICODE
#define PFnMsiApplyPatch PFnMsiApplyPatchW
#define MSIAPI_MsiApplyPatch MSIAPI_MsiApplyPatchW
#else
#define PFnMsiApplyPatch PFnMsiApplyPatchA
#define MSIAPI_MsiApplyPatch MSIAPI_MsiApplyPatchA
#endif

#define MSIAPI_MsiReinstallProductA "MsiReinstallProductA"
typedef UINT (WINAPI* PFnMsiReinstallProductA)(LPCSTR szProduct, DWORD dwReinstallMode);

#define MSIAPI_MsiReinstallProductW "MsiReinstallProductW"
typedef UINT (WINAPI* PFnMsiReinstallProductW)(LPCWSTR szProduct, DWORD dwReinstallMode);

#ifdef UNICODE
#define PFnMsiReinstallProduct PFnMsiReinstallProductW
#define MSIAPI_MsiReinstallProduct MSIAPI_MsiReinstallProductW
#else
#define PFnMsiReinstallProduct PFnMsiReinstallProductA
#define MSIAPI_MsiReinstallProduct MSIAPI_MsiReinstallProductA
#endif

#define MSIAPI_MsiQueryProductStateA "MsiQueryProductStateA"
typedef INSTALLSTATE (WINAPI* PFnMsiQueryProductStateA)(LPCSTR szProduct);

#define MSIAPI_MsiQueryProductStateW "MsiQueryProductStateW"
typedef INSTALLSTATE (WINAPI* PFnMsiQueryProductStateW)(LPCWSTR szProduct);

#ifdef UNICODE
#define PFnMsiQueryProductState PFnMsiQueryProductStateW
#define MSIAPI_MsiQueryProductState MSIAPI_MsiQueryProductStateW
#else
#define PFnMsiQueryProductState PFnMsiQueryProductStateA
#define MSIAPI_MsiQueryProductState MSIAPI_MsiQueryProductStateA
#endif

#define MSIAPI_MsiOpenDatabaseA "MsiOpenDatabaseA"
typedef UINT (WINAPI* PFnMsiOpenDatabaseA)(LPCSTR szDatabasePath, LPCSTR szPersist, MSIHANDLE *phDatabase);

#define MSIAPI_MsiOpenDatabaseW "MsiOpenDatabaseW"
typedef UINT (WINAPI* PFnMsiOpenDatabaseW)(LPCWSTR szDatabasePath, LPCWSTR szPersist, MSIHANDLE *phDatabase);

#ifdef UNICODE
#define PFnMsiOpenDatabase PFnMsiOpenDatabaseW
#define MSIAPI_MsiOpenDatabase MSIAPI_MsiOpenDatabaseW
#else
#define PFnMsiOpenDatabase PFnMsiOpenDatabaseA
#define MSIAPI_MsiOpenDatabase MSIAPI_MsiOpenDatabaseA
#endif

#define MSIAPI_MsiDatabaseOpenViewA "MsiDatabaseOpenViewA"
typedef UINT (WINAPI* PFnMsiDatabaseOpenViewA)(MSIHANDLE hDatabase, LPCSTR szQuery, MSIHANDLE *phView);

#define MSIAPI_MsiDatabaseOpenViewW "MsiDatabaseOpenViewW"
typedef UINT (WINAPI* PFnMsiDatabaseOpenViewW)(MSIHANDLE hDatabase, LPCWSTR szQuery, MSIHANDLE *phView);

#ifdef UNICODE
#define PFnMsiDatabaseOpenView PFnMsiDatabaseOpenViewW
#define MSIAPI_MsiDatabaseOpenView MSIAPI_MsiDatabaseOpenViewW
#else
#define PFnMsiDatabaseOpenView PFnMsiDatabaseOpenViewA
#define MSIAPI_MsiDatabaseOpenView MSIAPI_MsiDatabaseOpenViewA
#endif

#define MSIAPI_MsiViewExecute "MsiViewExecute"
typedef UINT (WINAPI* PFnMsiViewExecute)(MSIHANDLE hView, MSIHANDLE hRecord);

#define MSIAPI_MsiViewFetch "MsiViewFetch"
typedef UINT (WINAPI* PFnMsiViewFetch)(MSIHANDLE hView, MSIHANDLE *phRecord);

#define MSIAPI_MsiRecordGetStringA "MsiRecordGetStringA"
typedef UINT (WINAPI* PFnMsiRecordGetStringA)(MSIHANDLE hRecord, unsigned int uiField, LPSTR szValue, DWORD *pcchValueBuf);

#define MSIAPI_MsiRecordGetStringW "MsiRecordGetStringW"
typedef UINT (WINAPI* PFnMsiRecordGetStringW)(MSIHANDLE hRecord, unsigned int uiField, LPWSTR szValue, DWORD *pcchValueBuf);

#ifdef UNICODE
#define PFnMsiRecordGetString PFnMsiRecordGetStringW
#define MSIAPI_MsiRecordGetString MSIAPI_MsiRecordGetStringW
#else
#define PFnMsiRecordGetString PFnMsiRecordGetStringA
#define MSIAPI_MsiRecordGetString MSIAPI_MsiRecordGetStringA
#endif

#define MSIAPI_MsiEnableLogA "MsiEnableLogA"
typedef UINT (WINAPI* PFnMsiEnableLogA)(DWORD dwLogMode, LPCSTR szLogFile, DWORD dwLogAttributes);

#define MSIAPI_MsiEnableLogW "MsiEnableLogW"
typedef UINT (WINAPI* PFnMsiEnableLogW)(DWORD dwLogMode, LPCWSTR szLogFile, DWORD dwLogAttributes);

#ifdef UNICODE
#define PFnMsiEnableLog PFnMsiEnableLogW
#define MSIAPI_MsiEnableLog MSIAPI_MsiEnableLogW
#else
#define PFnMsiEnableLog PFnMsiEnableLogA
#define MSIAPI_MsiEnableLog MSIAPI_MsiEnableLogA
#endif

#define MSIAPI_MsiCloseHandle "MsiCloseHandle"
typedef UINT (WINAPI* PFnMsiCloseHandle)(MSIHANDLE h);

#define MSIAPI_MsiGetSummaryInformationA "MsiGetSummaryInformationA"
typedef UINT (WINAPI* PFnMsiGetSummaryInformationA)(MSIHANDLE, LPCSTR, UINT, MSIHANDLE*);

#define MSIAPI_MsiGetSummaryInformationW "MsiGetSummaryInformationW"
typedef UINT (WINAPI* PFnMsiGetSummaryInformationW)(MSIHANDLE, LPCWSTR, UINT, MSIHANDLE*);

#ifdef UNICODE
#define PFnMsiGetSummaryInformation PFnMsiGetSummaryInformationW
#define MSIAPI_MsiGetSummaryInformation  MSIAPI_MsiGetSummaryInformationW
#else
#define PFnMsiGetSummaryInformation PFnMsiGetSummaryInformationA
#define MSIAPI_MsiGetSummaryInformation  MSIAPI_MsiGetSummaryInformationA
#endif

#define MSIAPI_MsiSummaryInfoGetPropertyA "MsiSummaryInfoGetPropertyA"
typedef UINT (WINAPI* PFnMsiSummaryInfoGetPropertyA)(
	MSIHANDLE hSummaryInfo,
	UINT     uiProperty,     // property ID, one of allowed values for summary information
	UINT     *puiDataType,   // returned type: VT_I4, VT_LPSTR, VT_FILETIME, VT_EMPTY
	INT      *piValue,       // returned integer property data
	FILETIME *pftValue,      // returned datetime property data
	LPSTR  szValueBuf,     // buffer to return string property data
	DWORD    *pcchValueBuf); // in/out buffer character count

#define MSIAPI_MsiSummaryInfoGetPropertyW "MsiSummaryInfoGetPropertyW"
typedef UINT (WINAPI* PFnMsiSummaryInfoGetPropertyW)(
	MSIHANDLE hSummaryInfo,
	UINT     uiProperty,     // property ID, one of allowed values for summary information
	UINT     *puiDataType,   // returned type: VT_I4, VT_LPSTR, VT_FILETIME, VT_EMPTY
	INT      *piValue,       // returned integer property data
	FILETIME *pftValue,      // returned datetime property data
	LPWSTR  szValueBuf,     // buffer to return string property data
	DWORD    *pcchValueBuf); // in/out buffer character count

#ifdef UNICODE
#define PFnMsiSummaryInfoGetProperty PFnMsiSummaryInfoGetPropertyW
#define MSIAPI_MsiSummaryInfoGetProperty MSIAPI_MsiSummaryInfoGetPropertyW
#else
#define PFnMsiSummaryInfoGetProperty PFnMsiSummaryInfoGetPropertyA
#define MSIAPI_MsiSummaryInfoGetProperty MSIAPI_MsiSummaryInfoGetPropertyA
#endif // !UNICODE

const DWORD lcidLOCALE_INVARIANT = MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT);

typedef struct _MSI_API {
	HMODULE					hModule;
	PFnMsiSetInternalUI		SetInternalUI;
	PFnMsiInstallProduct	InstallProduct;
	PFnMsiApplyPatch		ApplyPatch;
	PFnMsiReinstallProduct	ReinstallProduct;
	PFnMsiQueryProductState	QueryProductState;
	PFnMsiOpenDatabase		OpenDatabase;
	PFnMsiDatabaseOpenView	DatabaseOpenView;
	PFnMsiViewExecute		ViewExecute;
	PFnMsiViewFetch			ViewFetch;
	PFnMsiRecordGetString	RecordGetString;
	PFnMsiCloseHandle		CloseHandle;
	PFnMsiEnableLog			EnableLog;
	PFnMsiSummaryInfoGetProperty	SummmaryInfoGetProperty;
	PFnMsiGetSummaryInformation		GetSummaryInformation;
} MSIAPI, *PMSIAPI;

UINT MsiApiLoad(PMSIAPI pMsiApi);
UINT MsiApiUnload(PMSIAPI pMsiApi);

#define ERROR_LOAD_MSI_API_FAILED 0x80000001

class CMsiApi :
	public MSIAPI
{
public:

	CMsiApi()
	{
		PMSIAPI pApi = reinterpret_cast<PMSIAPI>(this);
		::ZeroMemory(pApi, sizeof(MSIAPI));
	}

	~CMsiApi()
	{
		if (NULL != hModule) {
			DWORD dwErr = ::GetLastError();
			(VOID) MsiApiUnload(reinterpret_cast<PMSIAPI>(this));
			::SetLastError(dwErr);
		}
	}

	BOOL Initialize()
	{
		UINT uiRet = MsiApiLoad(reinterpret_cast<PMSIAPI>(this));
		if (ERROR_SUCCESS != uiRet) {
			SetLastError(uiRet);
			return FALSE;
		}
		return TRUE;
	}
};
