/*
 * Copyright (C) XIMETA, Inc. All rights reserved.
 *
 */
#ifndef LPXDEF_H_INCLUDED
#define LPXDEF_H_INCLUDED

#if _MSC_VER > 1000
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {

	LPX_ADDRESS_TYPE_IDENTIFIER = 0,
	/* LPX_ADDRESS_TYPE_IDENTIFIER = 63, */

	LPXPROTO_STREAM = 214,
	LPXPROTO_TCP = LPXPROTO_STREAM,

	LPXPROTO_DGRAM = 215,
	LPXPROTO_UDP = LPXPROTO_DGRAM,

	MAX_SOCKETLPX_INTERFACE = 8,
};

typedef enum _LPX_KNOWN_PORTS {

	/* Stream */
	LPXPORT_NDAS_TARGET = 0x2710,

	/* Datagram */
	LPXPORT_NDAS_HEARTBEAT_SOURCE = 0x2711,
	LPXPORT_NDAS_HEARTBEAT_DESTINATION = 0x2712,

	/* Stream */
	LPXPORT_LFS_PRIMARY =  0x0001,
	LPXPORT_LFS_CALLBACK = 0x0002,
	LPXPORT_LFS_DATAGRAM = 0x0003,

	/* Datagram */
	LPXPORT_LSHELPER_INFOEX = 0x0011,

	/* Datagram */
	LPXPORT_NDAS_HIX = 0x00EE,

	/* Stream */
	LPXPORT_NRMX_ARBITRATOR = 0x0010,
	LPXPORT_NRMX_ARBITER_PORT = LPXPORT_NRMX_ARBITRATOR,
};

#ifdef __cplusplus
}
#endif

#endif /* LPXDEF_H_INCLUDED */
