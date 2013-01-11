/*++

Copyright (C) 2002-2007 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPX_PROTO_H_
#define _LPX_PROTO_H_

#define LPX_VERSION_1			1
#define LPX_CURRENT_VERSION		LPX_VERSION_1

#define LPX_HOST_DGMSG_ID        0


//
// Ethernet packet type for LPX
//

#define ETH_P_LPX				((UINT16)0x88AD)        // 0x8200


#include <pshpack1.h>

typedef UNALIGNED struct _LPX_HEADER {

	union {

		struct {

			UCHAR    PacketSizeMsb:6;
			UCHAR    LpxType:2;

#define LPX_TYPE_RAW			0
#define LPX_TYPE_DATAGRAM		2
#define LPX_TYPE_STREAM			3
#define LPX_TYPE_MASK			((UINT16)0x00C0)

			UCHAR    PacketSizeLsb;
		};

		UINT16        PacketSize;
	};

	USHORT    DestinationPort;
	USHORT    SourcePort;

	union {

		struct {
		
			UCHAR	Reserved[9];
			UCHAR	Option;

#define LPX_OPTION_PACKETS_CONTINUE_BIT			((UCHAR)0x80)
#define LPX_OPTION_NONE_DELAYED_ACK				((UCHAR)0x40)

#if __LPX_OPTION_ADDRESSS__

#define LPX_OPTION_DESTINATION_ADDRESS			((UCHAR)0x01)
#define LPX_OPTION_SOURCE_ADDRESS				((UCHAR)0x02)
#define LPX_OPTION_DESTINATION_ADDRESS_ACCEPT	((UCHAR)0x04)
#define LPX_OPTION_SOURCE_ADDRESS_ACCEPT		((UCHAR)0x08)

#endif

		};

		struct {

			USHORT    Lsctl;

#define LSCTL_CONNREQ		((UINT16)0x0001)
#define LSCTL_DATA          ((UINT16)0x0002)
#define LSCTL_DISCONNREQ	((UINT16)0x0004)
#define LSCTL_ACKREQ        ((UINT16)0x0008)
#define LSCTL_ACK           ((UINT16)0x1000)
#define LSCTL_MASK			((UINT16)(LSCTL_CONNREQ | LSCTL_DATA|LSCTL_DISCONNREQ|LSCTL_ACKREQ|LSCTL_ACK))

			USHORT	Sequence;
			USHORT  AckSequence;
			UCHAR   ServerTag;
			UCHAR   ReservedS1[2];

			union {

				UCHAR	Option1;

				struct {
				
					UCHAR	Reserved2: 6;
					UCHAR	NonDelayedAck: 1;
					UCHAR	Pcb: 1;
				};
			};

		};

		struct {

			USHORT  MessageId;
			USHORT  MessageLength;
			USHORT  FragmentId;
			USHORT  FragmentLength;
			UCHAR   ResevedU1;
			UCHAR	Option2;
		};
	};

} LPX_HEADER, *PLPX_HEADER;


#include <poppack.h>


#endif