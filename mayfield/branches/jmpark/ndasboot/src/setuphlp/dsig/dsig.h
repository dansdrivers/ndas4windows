#pragma once

#define DRIVERSIGN_NOT_SET 0xFF

#ifndef DRIVERSIGN_NONE
#define DRIVERSIGN_NONE             0x00000000
#endif
#ifndef DRIVERSIGN_WARNING
#define DRIVERSIGN_WARNING          0x00000001
#endif
#ifndef DRIVERSIGN_BLOCKING
#define DRIVERSIGN_BLOCKING         0x00000002
#endif

#define ERROR_NO_ADMINISTRATORS 0x80C00001

#define GetDriverSigningPolicyInSystem			_DSABB__
#define GetDriverSigningPolicyInUserPreference	_DSA99__
#define GetDriverSigningPolicyInUserPolicy		_DSACC__
#define GetEffectiveDriverSigningPolicy         _DSAAF__
#define CalcEffectiveDriverSigningPolicy		_DSA23__
#define ApplyDriverSigningPolicy                _DSA43__

#ifdef DSIG_DLL_IMPL
#define DSIGAPI __declspec(dllexport) 
#endif

#ifdef DSIG_USE_DLL
#define DSIGAPI __declspec(dllimport)
#else
#define DSIGAPI 
#endif

#ifdef __cplusplus
extern "C" {
#endif

DSIGAPI DWORD WINAPI GetDriverSigningPolicyInSystem();
DSIGAPI DWORD WINAPI GetDriverSigningPolicyInUserPreference();
DSIGAPI DWORD WINAPI GetDriverSigningPolicyInUserPolicy();

DSIGAPI DWORD WINAPI GetEffectiveDriverSigningPolicy();
DSIGAPI DWORD WINAPI CalcEffectiveDriverSigningPolicy(DWORD dwUser, DWORD dwPolicy, DWORD dwSystem);

DSIGAPI BOOL WINAPI ApplyDriverSigningPolicy(DWORD dwPolicy, BOOL bGlobal);

#ifdef __cplusplus
}
#endif
