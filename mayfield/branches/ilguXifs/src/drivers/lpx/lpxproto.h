/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPX_PROTO_H_
#define _LPX_PROTO_H_

#define LPX_VERSION_1			1
#define LPX_CURRENT_VERSION		LPX_VERSION_1

#define LPX_HOST_DGMSG_ID        0

#include <pshpack1.h>

typedef UNALIGNED struct _LPX_HEADER2 
{
	union {
		struct {
			UCHAR    PacketSizeMsb:6;
			UCHAR    LpxType:2;
#define LPX_TYPE_RAW        0
#define LPX_TYPE_DATAGRAM    2
#define LPX_TYPE_STREAM        3
#define LPX_TYPE_MASK        (USHORT)0x00C0
			UCHAR    PacketSizeLsb;
		};
		USHORT        PacketSize;
	};

	USHORT    DestinationPort;
	USHORT    SourcePort;

	union {
		USHORT    Reserved[5];
		struct {
			USHORT    Lsctl;
#define LSCTL_CONNREQ        (USHORT)0x0001
#define LSCTL_DATA            (USHORT)0x0002
#define LSCTL_DISCONNREQ    (USHORT)0x0004
#define LSCTL_ACKREQ        (USHORT)0x0008
#define LSCTL_ACK            (USHORT)0x1000
#define LSCTL_MASK		(USHORT)(LSCTL_CONNREQ | LSCTL_DATA|LSCTL_DISCONNREQ|LSCTL_ACKREQ|LSCTL_ACK)
			USHORT    Sequence;
			USHORT    AckSequence;
			UCHAR    ServerTag;
			UCHAR    ReservedS1[3];
		};

		struct {
			USHORT    MessageId;
			USHORT    MessageLength;
			USHORT    FragmentId;
			USHORT    FragmentLength;
			USHORT    ResevedU1;
		};
	};

} LPX_HEADER2, *PLPX_HEADER2;

extern ULONG                ulPacketDropRate;   // number of packet to drop over 1000 packets.

#include <poppack.h>

#define    SMP_SYS_PKT_LEN    (sizeof(LPX_HEADER2))

#endif