#ifndef _NDAS_HIX_H_
#define _NDAS_HIX_H_
#pragma once

#include <pshpack1.h>
#pragma warning(disable: 4200)

//
// LPX Datagram
//
// NDAS_HIX_LISTEN_PORT 7918
//
#define NDAS_HIX_LISTEN_PORT	0x00EE

//
// The following SIGNATURE value is little endian.
// You should use 0x5869684E for bit-endian machines
//
#define NDAS_HIX_SIGNATURE 0x4E686958
#define NDAS_HIX_SIGNATURE_CHAR_ARRAY {'N','h','i','X'}

#define NHIX_CURRENT_REVISION 0x01

//
// Maximum message length of the HIX packet is 512-bytes.
// Server implementation may allocate a fixed 512 byte buffer
// to receive the data.
//
#define NHIX_MAX_MESSAGE_LEN	512

#define NHIX_TYPE_DISCOVER			0x01
#define NHIX_TYPE_QUERY_HOST_INFO	0x02
#define NHIX_TYPE_SURRENDER_ACCESS	0x03
#define NHIX_TYPE_DEVICE_CHANGE		0x04
#define NHIX_TYPE_UNITDEVICE_CHANGE	0x05

//
// 32 byte header for NHIX packets.
// HIX_HEADER is also declared as a field of each specialized packet types.
// So you don't have to send the header and the data twice.
// 
typedef struct _NDAS_HIX_HEADER {

	union {
		UCHAR SignatureChars[4];
		ULONG Signature;
	};
	UCHAR Revision;
	UCHAR ReplyFlag : 1;
	UCHAR Type : 7;
	USHORT Length; // Including header
	union {
		UCHAR HostId[16]; // Generally 16 byte GUID is used
		GUID HostGuid;
	};
	UCHAR Reserved[8];

} NDAS_HIX_HEADER, *PNDAS_HIX_HEADER;

#define NHIX_UDA_NO_ACCESS 0x00
#define NHIX_UDA_READ_ACCESS	0x80
#define NHIX_UDA_WRITE_ACCESS	0x40
#define NHIX_UDA_READ_WRITE_ACCESS	0xD0
#define NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS	0xE0
#define NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS	0xF0
// #define NHIX_UDA_ANY_ACCESS 0x0F

//11110000
//9 a  b  c  d  e  f
//  10 11 12 13 14 15
//1000 : 80
//0100 : 40
//0010 : 20
//0001 : 10

// #define NHIX_UDA_ANY		NHIX_UDA_ANY_ACCESS
#define NHIX_UDA_NONE		NHIX_UDA_NO_ACCESS
#define NHIX_UDA_RO			NHIX_UDA_READ_ACCESS
#define NHIX_UDA_WO			NHIX_UDA_WRITE_ACCESS
#define NHIX_UDA_RW			NHIX_UDA_READ_WRITE_ACCESS
#define NHIX_UDA_SHRW_PRIM	NHIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS
#define NHIX_UDA_SHRW_SEC	NHIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS

typedef struct _NHIX_UDA_BITSET {
	UCHAR ReadAccess : 1;
	UCHAR WriteAccess : 1;
	UCHAR SharedRW : 1;
	UCHAR SharedRWPrimary : 1;
	UCHAR Reserved : 4;
} NHIX_UDA_BITSET, *PNHIX_UDA_BITSET;

typedef UCHAR NHIX_UDA;

typedef struct _NDAS_HIX_UNITDEVICE_ENTRY_DATA {
	UCHAR DeviceId[6];
	UCHAR UnitNo;
	NHIX_UDA AccessType;
} NDAS_HIX_UNITDEVICE_ENTRY_DATA, *PNDAS_HIX_UNITDEVICE_ENTRY_DATA;

typedef struct _NDAS_HIX_HOST_INFO_VER_INFO {
	USHORT VersionMajor;
	USHORT VersionMinor;
	USHORT VersionBuild;
	USHORT VersionPrivate;
} NDAS_HIX_HOST_INFO_VER_INFO, *PNDAS_HIX_HOST_INFO_VER_INFO;

// Ad Hoc Information
typedef struct _NDAS_HIX_HOST_INFO_AD_HOC {
	BOOL Unicode;
	UCHAR FieldLength; // up to 255 chars
	UCHAR DataLength; // up to 255 chars
	// BYTE Field[];
	// BYTE Data[];
} NDAS_HIX_HOST_INFO_AD_HOC, *PNDAS_HIX_HOST_INFO_AD_HOC;

//
// NHDP Host Info Class Type and Fields
//

#define NHIX_HIC_OS_FAMILY				0x00000001
// Required
// Field: UCHAR, one of NHIX_HIC_OS_FAMILY_xxxx

typedef UCHAR NHIX_HIC_OS_FAMILY_TYPE;

#define NHIX_HIC_OS_UNKNOWN		0x00
#define NHIX_HIC_OS_WIN9X		0x01
#define NHIX_HIC_OS_WINNT		0x02
#define NHIX_HIC_OS_LINUX		0x03
#define NHIX_HIC_OS_WINCE		0x04
#define NHIX_HIC_OS_PS2			0x05
#define NHIX_HIC_OS_MAC			0x06
#define NHIX_HIC_OS_EMBEDDED_OS	0x07
#define NHIX_HIC_OS_OTHER		0xFF

// Required
// Field: VER_INFO
#define NHIX_HIC_OS_VER_INFO			0x00000002

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_HOSTNAME				0x00000004

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_FQDN					0x00000008

// Optional
// Field: CHAR[], null terminated
#define NHIX_HIC_NETBIOSNAME			0x00000010

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_HOSTNAME		0x00000020

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_FQDN			0x00000040

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define NHIX_HIC_UNICODE_NETBIOSNAME	0x00000080

// Optional
// Field: UCHAR Count, UCHAR AddressLen = 6, UCHAR[][6]
#define NHIX_HIC_ADDR_LPX				0x00000100

// Optional
// Field: UCHAR Count, UCHAR AddressLen = 4, UCHAR[][4]
#define NHIX_HIC_ADDR_IPV4				0x00000200

// Optional
// Field: UCHAR Count, UCHAR AddressLen = 16, UCHAR[][16]
#define NHIX_HIC_ADDR_IPV6				0x00000400

// Required
// Field: VER_INFO
#define NHIX_HIC_NDAS_SW_VER_INFO		0x00000800

// Required if using NHIX_UDA_SHRW_xxx
// Field: VER_INFO
#define NHIX_HIC_NDFS_VER_INFO			0x00001000

// Required
// Field: ULONG, flags (network byte order)
#define NHIX_HIC_HOST_FEATURE			0x00002000

// #define NHIX_HFF_SHARED_WRITE_HOST		0x00000001
#define NHIX_HFF_DEFAULT				0x00000001 // should be always set
#define NHIX_HFF_SHARED_WRITE_CLIENT	0x00000002
#define NHIX_HFF_SHARED_WRITE_SERVER	0x00000004
#define NHIX_HFF_AUTO_REGISTER			0x00000100 // reserved
#define NHIX_HFF_UPNP_HOST				0x00000200 // reserved

typedef ULONG NHIX_HIC_HOST_FEATURE_TYPE;

// Required
// Field: UCHAR, one or more of NHIX_HIC_TRANSPORT_{LPX,IP}
#define NHIX_HIC_TRANSPORT				0x00004000

#define NHIX_TF_LPX		0x01
#define NHIX_TF_IP		0x02

typedef UCHAR NHIX_HIC_TRANSPORT_TYPE;

// Optional
// Field: AD_HOC
#define NHIX_HIC_AD_HOC					0x80000000

typedef struct _NDAS_HIX_HOST_INFO_ENTRY {
	UCHAR Length; // Entire length (e.g. 2 + class data)
	ULONG Class;
	UCHAR Data[1]; // at least one byte data is required
	// Class Specific Data
} NDAS_HIX_HOST_INFO_ENTRY, *PNDAS_HIX_HOST_INFO_ENTRY;

typedef CONST NDAS_HIX_HOST_INFO_ENTRY* PCNDAS_HIX_HOST_INFO_ENTRY;

typedef struct _NDAS_HIXHOST_INFO_DATA {
	UCHAR Length;
	ULONG Contains; // Class Contains Flags
	UCHAR Count;
	NDAS_HIX_HOST_INFO_ENTRY Entry[1];
} NDAS_HIX_HOST_INFO_DATA, *PNDAS_HIX_HOST_INFO_DATA;

typedef CONST NDAS_HIX_HOST_INFO_DATA* PCNDAS_HIX_HOST_INFO_DATA;

//
// Discover
//

typedef struct _NDAS_HIX_DISCOVER_REQUEST_DATA {
	UCHAR EntryCount;
	NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
} NDAS_HIX_DISCOVER_REQUEST_DATA, *PNDAS_HIX_DISCOVER_REQUEST_DATA;

typedef struct _NDAS_HIX_DISCOVER_REPLY_DATA {
	UCHAR EntryCount;
	NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
} NDAS_HIX_DISCOVER_REPLY_DATA, *PNDAS_HIX_DISCOVER_REPLY_DATA;

typedef struct _NDAS_HIX_DISCOVER_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_DISCOVER_REQUEST_DATA Data;
} NDAS_HIX_DISCOVER_REQUEST, *PNDAS_HIX_DISCOVER_REQUEST;

typedef struct _NDAS_HIX_DISCOVER_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_DISCOVER_REPLY_DATA Data;
} NDAS_HIX_DISCOVER_REPLY, *PNDAS_HIX_DISCOVER_REPLY;

//
// Query Host Info
//

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA {
	UCHAR Reserved[32];
} NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA, *PNDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA;

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA {
	UCHAR Reserved[32];
	NDAS_HIX_HOST_INFO_DATA HostInfoData;
} NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA, *PNDAS_HIX_QUERY_HOST_INFO_REPLY_DATA;

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA Data;
} NDAS_HIX_QUERY_HOST_INFO_REQUEST, *PNDAS_HIX_QUERY_HOST_INFO_REQUEST;

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA Data;
} NDAS_HIX_QUERY_HOST_INFO_REPLY, *PNDAS_HIX_QUERY_HOST_INFO_REPLY;

//
// Surrender Access
//
// Requesting surrender access to the unit device with UDA
// is interpreted with the following criteria.
// For a given UDA of the unit device,
//
// - Only READ_ACCESS and WRITE_ACCESS bit can be set.
//
//   If other bits are set, the request may be invalidated
//   or just the entry can be ignored.
//
//   * Only WRITE_ACCESS and (READ_ACCESS | WRITE_ACCESS) 
//     requests are valid for Windows hosts.
//     Single invalid entry invalidates the entire request.
//
// - READ_ACCESS | WRITE_ACCESS
//
//   To request to surrender both READ and WRITE access, turn the both bit on.
//   The host is recommended to disconnect the connection from the device
//   on successful reply.
//
// - WRITE_ACCESS
//
//   To request to surrender WRITE access only.
//   the host can 
//   - change the access mode to READ-ONLY or
//   - disconnect the access. 
//   Final reply contains the changed access mode.
//
// - READ_ACCESS (DO NOT USE AT THIS TIME)
//
//   To request to surrender READ access, the requester can turn on
//   only READ_ACCESS bit. READ_ACCESS bit implies to surrender
//   the READ_ACCESS and the host may not surrender WRITE_ACCESS
//   when there are WRITE-ONLY-ACCESSIBLE NDAS devices.
//   This request may be treated as invalid request.
//

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA {
	struct {
		UCHAR EntryCount;
		NDAS_HIX_UNITDEVICE_ENTRY_DATA Entry[1];
	};
} NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA, *PNDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA;

#define NHIX_SURRENDER_REPLY_STATUS_QUEUED 0x01
#define NHIX_SURRENDER_REPLY_STATUS_ERROR  0x02
#define NHIX_SURRENDER_REPLY_STATUS_NO_ACCESS  0x03
#define NHIX_SURRENDER_REPLY_STATUS_INVALID_REQUEST 0xFF

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA {
	UCHAR Status;
} NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA, *PNDAS_HIX_SURRENDER_ACCESS_REPLY_DATA;

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA Data;
} NDAS_HIX_SURRENDER_ACCESS_REQUEST, *PNDAS_HIX_SURRENDER_ACCESS_REQUEST;

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REPLY {
	NDAS_HIX_HEADER Header;
	NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA Data;
} NDAS_HIX_SURRENDER_ACCESS_REPLY, *PNDAS_HIX_SURRENDER_ACCESS_REPLY;

typedef struct _NDAS_HIX_DEVICE_CHANGE_NOTIFY {
	NDAS_HIX_HEADER Header;
	UCHAR DeviceId[6];
} NDAS_HIX_DEVICE_CHANGE_NOTIFY, *PNDAS_HIX_DEVICE_CHANGE_NOTIFY;

typedef struct _NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY {
	NDAS_HIX_HEADER Header;
	UCHAR DeviceId[6];
	UCHAR UnitNo;
} NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY, *PNDAS_HIX_UNITDEVICE_CHANGE_NOTIFY;

#ifdef __cplusplus
//
// Nested Type Definitions for C++
//
struct NDAS_HIX {

	typedef NDAS_HIX_HEADER HEADER, *PHEADER;
	typedef NDAS_HIX_UNITDEVICE_ENTRY_DATA 
		UNITDEVICE_ENTRY_DATA, *PUNITDEVICE_ENTRY_DATA;

	struct HOST_INFO {
		typedef struct _DATA : NDAS_HIX_HOST_INFO_DATA {
			typedef NDAS_HIX_HOST_INFO_ENTRY ENTRY, *PENTRY;
		} DATA, *PDATA;
		typedef NDAS_HIX_HOST_INFO_VER_INFO VER_INFO, *PVER_INFO;
		typedef NDAS_HIX_HOST_INFO_AD_HOC AD_HOC, *PAD_HOC;
	};

	struct DISCOVER {
		typedef struct _REQUEST : NDAS_HIX_DISCOVER_REQUEST {
			typedef NDAS_HIX_DISCOVER_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_DISCOVER_REPLY {
			typedef NDAS_HIX_DISCOVER_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct QUERY_HOST_INFO {
		typedef struct _REQUEST : NDAS_HIX_QUERY_HOST_INFO_REQUEST {
			typedef NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_QUERY_HOST_INFO_REPLY {
			typedef NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct SURRENDER_ACCESS {
		typedef struct _REQUEST : NDAS_HIX_SURRENDER_ACCESS_REQUEST {
			typedef NDAS_HIX_SURRENDER_ACCESS_REQUEST_DATA DATA, *PDATA;
		} REQUEST, *PREQUEST;
		typedef struct _REPLY : NDAS_HIX_SURRENDER_ACCESS_REPLY {
			typedef NDAS_HIX_SURRENDER_ACCESS_REPLY_DATA DATA, *PDATA;
		} REPLY, *PREPLY;
	};

	struct DEVICE_CHANGE {
		typedef NDAS_HIX_DEVICE_CHANGE_NOTIFY NOTIFY, *PNOTIFY;
	};

	struct UNITDEVICE_CHANGE {
		typedef NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY NOTIFY, *PNOTIFY;
	};

};
#endif // __cplusplus

#pragma warning(default: 4200)
#include <poppack.h>

#endif // _NDAS_HIX_H_

