/*
* Copyright (C) XIMETA, Inc. All rights reserved.
*
*/
#ifndef LPXTDIDEF_H_INCLUDED
#define LPXTDIDEF_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif

#include "lpxdef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef UNALIGNED struct _TDI_ADDRESS_LPX {
	USHORT Port;
	UCHAR  Node[6];
	UCHAR  Reserved[10];	// To make the same size as NetBios
} TDI_ADDRESS_LPX, *PTDI_ADDRESS_LPX;

enum { 
	TDI_ADDRESS_LENGTH_LPX = sizeof(TDI_ADDRESS_LPX),
	TDI_ADDRESS_TYPE_LPX = LPX_ADDRESS_TYPE_IDENTIFIER,
};

typedef struct _TA_ADDRESS_LPX {

    LONG  TAAddressCount;
    struct  _AddrLpx {
        USHORT				AddressLength;
        USHORT				AddressType;
        TDI_ADDRESS_LPX		Address[1];
    } Address [1];

} TA_LPX_ADDRESS, *PTA_LPX_ADDRESS;

#ifdef __cplusplus
}
#endif

#endif /* LPXTDIDEF_H_INCLUDED */
