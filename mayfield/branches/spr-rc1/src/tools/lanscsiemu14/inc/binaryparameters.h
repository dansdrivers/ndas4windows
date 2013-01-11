#ifndef _BINARY_PARAMETERS_H_
#define _BINARY_PARAMETERS_H_

#include <pshpack1.h>

//
// Parameter Types.
//
#define PARAMETER_CURRENT_VERSION	0

#define	BIN_PARAM_TYPE_SECURITY		0x1
#define	BIN_PARAM_TYPE_NEGOTIATION	0x2
#define	BIN_PARAM_TYPE_TARGET_LIST	0x3
#define	BIN_PARAM_TYPE_TARGET_DATA	0x4

#define BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST		4
#define	BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST		8
#define BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST		32
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST		20
#define	BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST	4
#define	BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST	16

#define BIN_PARAM_SIZE_REPLY					36

typedef struct _BIN_PARAM {
	UCHAR	ParamType;
	UCHAR	Reserved1;
	_int16	Reserved2;
} BIN_PARAM, *PBIN_PARAM;

//
// BIN_PARAM_TYPE_SECURITY.
//

#define	LOGIN_TYPE_NORMAL		0x00
#define	LOGIN_TYPE_DISCOVERY	0xFF

//
// CHAP Auth Paramter.
//
#define	AUTH_METHOD_CHAP		0x0001

#define FIRST_TARGET_RO_USER	0x00000001
#define FIRST_TARGET_RW_USER	0x00010001
#define SECOND_TARGET_RO_USER	0x00000002
#define SECOND_TARGET_RW_USER	0x00020002
#define	SUPERVISOR				0xFFFFFFFF

typedef struct _AUTH_PARAMETER_CHAP {
	unsigned	CHAP_A;
	unsigned	CHAP_I;
	unsigned	CHAP_N;
	unsigned	CHAP_R[4];
	unsigned	CHAP_C[1];
} AUTH_PARAMETER_CHAP, *PAUTH_PARAMETER_CHAP;

#define HASH_ALGORITHM_MD5			0x00000005
#define CHAP_MAX_CHALLENGE_LENGTH	4

typedef struct _BIN_PARAM_SECURITY {
	UCHAR	ParamType;
	UCHAR	LoginType;
	_int16	AuthMethod;
	union {
		UCHAR	AuthParamter[1];	// Variable Length.
		AUTH_PARAMETER_CHAP ChapParam;
	};
} BIN_PARAM_SECURITY, *PBIN_PARAM_SECURITY;

//
// BIN_PARAM_TYPE_NEGOTIATION.	
//
typedef struct _BIN_PARAM_NEGOTIATION {
	UCHAR			ParamType;
	UCHAR			HWType;
	UCHAR			HWVersion;
	UCHAR			Reserved;
	unsigned		NRSlot;
	unsigned		MaxBlocks;
	unsigned		MaxTargetID;
	unsigned		MaxLUNID;
	unsigned _int16	HeaderEncryptAlgo;
	unsigned _int16	HeaderDigestAlgo;
	unsigned _int16	DataEncryptAlgo;
	unsigned _int16	DataDigestAlgo;
} BIN_PARAM_NEGOTIATION, *PBIN_PARAM_NEGOTIATION;

#define HW_TYPE_ASIC			0x00
#define HW_TYPE_UNSPECIFIED		0xFF

#define HW_VERSION_CURRENT		0x00
#define HW_VERSION_UNSPECIFIIED	0xFF

//
// BIN_PARAM_TYPE_TARGET_LIST.
//
typedef	struct _BIN_PARAM_TARGET_LIST_ELEMENT {
	unsigned _int32	TargetID;
	UCHAR			NRRWHost;
	UCHAR			NRROHost;
	_int16			Reserved1;
	unsigned _int64	TargetData;
} BIN_PARAM_TARGET_LIST_ELEMENT, *PBIN_PARAM_TARGET_LIST_ELEMENT;

typedef struct _BIN_PARAM_TARGET_LIST {
	UCHAR	ParamType;
	UCHAR	NRTarget;
	_int16	Reserved1;
	BIN_PARAM_TARGET_LIST_ELEMENT	PerTarget[1];
} BIN_PARAM_TARGET_LIST, *PBIN_PARAM_TARGET_LIST;

//
// BIN_PARAM_TYPE_TARGET_DATA.
//

#define	PARAMETER_OP_GET	0x00
#define	PARAMETER_OP_SET	0xFF

typedef struct _BIN_PARAM_TARGET_DATA {
	unsigned _int8	ParamType;
	unsigned _int8	GetOrSet;
	unsigned _int16	Reserved1;
	unsigned _int32	TargetID;
	unsigned _int64	TargetData;
} BIN_PARAM_TARGET_DATA, *PBIN_PARAM_TARGET_DATA;

#include <poppack.h>

#endif