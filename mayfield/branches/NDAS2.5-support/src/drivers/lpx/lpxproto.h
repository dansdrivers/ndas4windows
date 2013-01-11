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

#define LSCTL_CONNREQ        (USHORT)0x0001
#define LSCTL_DATA            (USHORT)0x0002
#define LSCTL_DISCONNREQ    (USHORT)0x0004
#define LSCTL_ACKREQ        (USHORT)0x0008
#define LSCTL_ACK            (USHORT)0x1000

#define LSCTL_SET_JUMBO		(USHORT)0x2000 	// Added NDAS 2.5
#define LSCTL_CLEAR_JUMBO	(USHORT)0x4000 	// Added NDAS 2.5
#define LSCTL_MASK		(USHORT)(LSCTL_CONNREQ | LSCTL_DATA|LSCTL_DISCONNREQ|LSCTL_ACKREQ|LSCTL_ACK)

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
			USHORT    Sequence;
			USHORT    AckSequence;
			UCHAR    ServerTag;
			UCHAR    ReservedS1[2];
			union {
				UCHAR    Options;
				struct {
					UCHAR ReservedO1:6;
					UCHAR NoDack:1; // Bit 6. If this field is set, peer does not support delayed ACK.
								// Set by NDAS2.5 chip only. 
								// If this flag is set, OPC field needs to be set according to current network status.
								// Valid for CONREQ packet only.
								// If this flag is not set, peer does not support OPC or peer can handle delayed ACK by itself.
					UCHAR PCont:1; 
								// Bit 7. Pcont: Packet Continue.
								//	If this field is set, peer does not need to send ACK for this packet.
								//	Host must clear this field for the last packet of unit data 
								//		and set this field for intermediate packets to reduce ACK.
								// Valid for DATA packet only.
				};
			};
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

extern ULONG                ulRxPacketDropRate;   // number of packet to drop over 1000 packets.
extern ULONG                ulTxPacketDropRate;   // number of packet to drop over 1000 packets.

#include <poppack.h>

#define    SMP_SYS_PKT_LEN    (sizeof(LPX_HEADER2))

#endif
