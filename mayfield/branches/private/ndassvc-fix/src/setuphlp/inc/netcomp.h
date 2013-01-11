#pragma once

#ifdef NDASDI_DLL_EXPORTS
#define NDASDI_API __declspec(dllexport)
#elif NDASDI_DLL_IMPORTS
#define NDASDI_API __declspec(dllimport)
#else
#define NDASDI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

NDASDI_API 
HRESULT 
WINAPI
HrIsNetComponentInstalled(
	IN PCWSTR szComponentId);

typedef enum _NetClass {
	NC_NetAdapter=0,
	NC_NetProtocol,
	NC_NetService,
	NC_NetClient,
	NC_Unknown
} NetClass;

NDASDI_API 
HRESULT 
WINAPI
HrInstallNetComponent(
	IN PCWSTR szComponentId,
	IN NetClass nc,
	IN PCWSTR szInfPath OPTIONAL,
	OUT PWSTR szCopiedInfPath OPTIONAL,
	IN DWORD cchCopiedInfPath OPTIONAL,
	IN LPDWORD pcchUsed OPTIONAL,
	OUT PWSTR* ppCopiedInfFilePart OPTIONAL);

NDASDI_API 
HRESULT 
WINAPI
HrUninstallNetComponent(
	IN PCWSTR szComponentId);

NDASDI_API
LPCTSTR
pNetCfgHrString(HRESULT hr);

#ifdef __cplusplus
}
#endif

