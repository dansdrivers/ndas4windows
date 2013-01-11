#ifndef _RC_PACKET_
#define	_RC_PACKET_

#define	RC_SIGNATURE	0x04a201d0
#define RC_NAME			"NetDisk Remote Control Packet"

#define	RC_OP_QUERY_REMOVE	1
#define	RC_OP_EJECT			11
#define	RC_OP_REMOVE		12

#define RC_REQUEST			0
#define	RC_REPLY			0xFF

#define RC_STATUS_SUCCESS	0x0
#define RC_STATUS_ERROR		0xFFFFFFFF

typedef struct _REMOTE_CONTROL_PACKET {
	
	ULONG	ulSignature;
	char	uPacketDesc[32];

	ULONG	ulSequence;

	UCHAR	ucMajorVersion;
	UCHAR	ucMinorVersino;
	UCHAR	ucOperation;
	UCHAR	ucReqRep;

	ULONG	ulStatus;

	UCHAR	ucNetDiskAddr[6];
	USHORT	usNetDiskUnitNumber;
// added by hootch
	UCHAR	ucSrcAddr[14] ;
	USHORT	uSrcAddrLen ;
// Socket takes wrong source IP address on multi-NIC PC
//


} REMOTE_CONTROL_PACKET, *PREMOTE_CONTROL_PACKET;

#endif