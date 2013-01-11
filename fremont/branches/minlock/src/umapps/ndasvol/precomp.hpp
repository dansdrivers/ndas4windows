#pragma once

#include <windows.h>
#include <tchar.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Workaround for including winioctl.h with ntddscsi, ntddstor
//
// DEVICE_TYPE is required from ntddstor.h
// which is defined from devioctl.h or winioctl.h
//
// We will provide DEVICE_TYPE here and undefine it right after inclusion
//
#ifndef DEVICE_TYPE
#define DEVICE_TYPE DWORD
#endif

// actually we need the following headers, but to include winioctl.h
// we can omit some which are defined in winioctl.h
// #include <devioctl.h>
// #include <ntdddisk.h>
// #include <ntddvol.h>
#include <ntddscsi.h>
#include <ntddstor.h>

// Remove DEVICE_TYPE, as winioctl.h will redefine it
#undef DEVICE_TYPE

#ifdef __cplusplus
}
#endif
#include <winioctl.h>

#include <crtdbg.h>
#include <cfg.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <strsafe.h>

const DWORD NdasVolTrace = 0x0040000;
