/*
 * Copyright (C) XIMETA, Inc. All rights reserved.
 *
 */
#ifndef LPXWSOCK_H_INCLUDED
#define LPXWSOCK_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif

#include <winsock2.h>
#include "lpxdef.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/* AF_LPX = AF_UNSPEC, */
	AF_LPX = LPX_ADDRESS_TYPE_IDENTIFIER,
};

typedef struct _LPX_ADDR {
	UCHAR node[6];
	UCHAR zero[10];
} LPX_ADDR, *PLPX_ADDR, * LPLPX_ADDR;

typedef struct _SOCKADDR_LPX {
	USHORT slpx_family;
	USHORT slpx_port;
	LPX_ADDR slpx_addr;
} SOCKADDR_LPX, *PSOCKADDR_LPX, * LPSOCKADDR_LPX;

typedef struct _sockaddr_lpx_pair {
	PSOCKADDR_LPX SourceAddress;
	PSOCKADDR_LPX DestinationAddress;
} SOCKADDR_LPX_PAIR, *PSOCKADDR_LPX_PAIR;

struct lpx_addr {
	u_char s_b[6];
	u_char s_zero[10];
};

struct sockaddr_lpx {
	u_short slpx_family;
	u_short slpx_port;
	struct lpx_addr slpx_addr;
};

#ifdef __cplusplus
}
#endif

#endif /* LPXADDR_H_INCLUDED */
