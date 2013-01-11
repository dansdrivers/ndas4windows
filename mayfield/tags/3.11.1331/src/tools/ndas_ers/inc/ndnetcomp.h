#ifndef NDNETCOMP_H
#define NDNETCOMP_H

#define NDNC_ADAPTER	0
#define NDNC_PROTOCOL	1
#define NDNC_SERVICE	2
#define NDNC_CLIENT		3
#define NDNC_UNKNOWN	4

#ifdef __cplusplus
extern "C" {
#endif

	int __stdcall NDInstallNetComp(PCWSTR szNetComp, UINT nc, PCWSTR szInfFullPath);
	int __stdcall NDUninstallNetComp(PCWSTR szNetComp);

#ifdef __cplusplus
};
#endif

#endif