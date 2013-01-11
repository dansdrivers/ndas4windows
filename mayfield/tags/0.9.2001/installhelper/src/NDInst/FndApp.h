#ifndef FNDAPP_H
#define FNDAPP_H

#ifdef _UNICODE
#define FindInstance FindInstanceW
#else
#define FindInstance FindInstanceA
#endif

BOOL APIENTRY FindInstanceA(LPCSTR lpszUID);
BOOL APIENTRY FindInstanceW(LPCWSTR lpszUID);

#endif
