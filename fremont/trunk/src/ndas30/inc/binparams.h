#ifndef _BINARY_PARAMETERS_H_
#define _BINARY_PARAMETERS_H_

#include <pshpack1.h>

//////////////////////////////////////////////////////////////////////////
//
//	Binary parameter
//

typedef struct _BIN_PARAM {
	unsigned _int8	ParamType;
	unsigned _int8	Reserved1;
	unsigned _int16	Reserved2;
} BIN_PARAM, *PBIN_PARAM;


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
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST		28
#define	BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST	4
#define	BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST	16

#define BIN_PARAM_SIZE_LOGIN_FIRST_REPLY		4
#define	BIN_PARAM_SIZE_LOGIN_SECOND_REPLY		36
#define BIN_PARAM_SIZE_LOGIN_THIRD_REPLY		4
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY		28
#define	BIN_PARAM_SIZE_TEXT_TARGET_LIST_REPLY	4
#define	BIN_PARAM_SIZE_TEXT_TARGET_DATA_REPLY	8

#define BIN_PARAM_SIZE_REPLY					36


//////////////////////////////////////////////////////////////////////////
//
// Security parameter
// BIN_PARAM_TYPE_SECURITY.
//

//
// CHAP Auth Paramter embedded in Security parameter
//

#if 0

typedef struct _AUTH_PARAMETER_CHAP {
	unsigned _int32	CHAP_A;			// Algorithm: HASH_ALGORITHM_MD5
	unsigned _int32	CHAP_I;			// Stamp
	unsigned _int32	CHAP_N;			// User number(ID)
	unsigned _int32	CHAP_R[4];		// Result from the host
	unsigned _int32	CHAP_C[1];		// Challenge value
} AUTH_PARAMETER_CHAP, *PAUTH_PARAMETER_CHAP;

#else

typedef struct _AUTH_PARAMETER_CHAP {
	unsigned _int32	CHAP_A;			// Algorithm: HASH_ALGORITHM_MD5( 1.0~2.0) or HASH_ALGORITHM_AES128(2.5)
	unsigned _int32	CHAP_I;			// Stamp
	unsigned _int32	CHAP_N;			// User number(ID)
	union {
		struct {
			unsigned _int32	CHAP_R[4];		// Result from the host
			unsigned _int32	CHAP_C[1];		// Challenge value
		} V1;	// NDAS 1.0, 1.1, 2.0
		struct {
			unsigned _int32	Reserved;
			unsigned _int32	CHAP_CR[4];	// CHAP_CR is used both for Challenge and result
		} V2;	// NDAS 2.5
	};
} AUTH_PARAMETER_CHAP, *PAUTH_PARAMETER_CHAP;

#endif

#define HASH_ALGORITHM_MD5			0x00000005 // Uses Hash32To128 function 
#define HASH_ALGORITHM_AES128		0x00000006 // Uses AES128

#define CHAP_MAX_CHALLENGE_LENGTH	4

#define FIRST_TARGET_RO_USER	0x00000001
#define FIRST_TARGET_RW_USER	0x00010001
#define SECOND_TARGET_RO_USER	0x00000002
#define SECOND_TARGET_RW_USER	0x00020002
#define	NDAS_SUPERVISOR			0xFFFFFFFF

// User account added at NDAS 2.5 
// User Id is 1~7, Super user id is 0

#define USER_PERMISSION_RO		0x0001
#define USER_PERMISSION_SW		0x0002
#define USER_PERMISSION_EW		0x0004
#define USER_PERMISSION_MASK	0x0007
#define MAKE_USER_ID(_usernum, _perm) (((_usernum)<<16) | (_perm))
#define USER_NUM_FROM_USER_ID(_uid) ((_uid)>>16)
#define USER_PERM_FROM_USER_ID(_uid) ((_uid) & 0x0ffff)
#define	USER_SUPERVISOR				0x00000000
#define SUPERVISOR_USER_NUM			0x0000		
#define DEFAULT_USER_NUM			0x0001		// User number 1 is preconfigured 

#define HAS_USER_WRITE_ACCESS(_uid)   (_uid & (USER_PERMISSION_SW| USER_PERMISSION_EW))
#define IS_SUPERVISOR(_uid)	(USER_NUM_FROM_USER_ID(_uid) == SUPERVISOR_USER_NUM)

//
//	Security parameter body
//

typedef struct _BIN_PARAM_SECURITY {
	unsigned _int8	ParamType;		// BIN_PARAM_TYPE_SECURITY
	unsigned _int8	LoginType;		// LOGIN_TYPE_NORMAL / DISCOVERY
	unsigned _int16	AuthMethod;		// AUTH_METHOD_CHAP
	union {
		unsigned _int8	AuthParamter[1];	// Variable Length.
		AUTH_PARAMETER_CHAP ChapParam;
	};
} BIN_PARAM_SECURITY, *PBIN_PARAM_SECURITY;

#define	LOGIN_TYPE_NORMAL		0x00
#define	LOGIN_TYPE_DISCOVERY	0xFF

#define	AUTH_METHOD_CHAP		0x0001


//////////////////////////////////////////////////////////////////////////
//
// Hardware spec and procotol negotiation
// BIN_PARAM_TYPE_NEGOTIATION
//

typedef struct _BIN_PARAM_NEGOTIATION {
	unsigned _int8	ParamType;		// BIN_PARAM_TYPE_NEGOTIATION
	unsigned _int8	HWType;
	unsigned _int8	HWVersion;
	unsigned _int8	Reserved1;
	unsigned _int32	NRSlot;
	unsigned _int32	MaxBlocks;
	unsigned _int32	MaxTargetID;
	unsigned _int32	MaxLUNID;
	unsigned _int8	Options;	
	unsigned _int8	HeaderEncryptAlgo; 	// ENCRYPT_ALGO_xxx. ENCRYPT_ALGO_NKCDE_1 is used for 1.0~2.0. ENCRYPT_ALGO_AES128 is used for 2.5
	unsigned _int8	Reserved2;		
	unsigned _int8	HeaderDigestAlgo; 	// Not used till 2.0. DIGEST_ALGO_NONE or DIGEST_ALGO_CRC32 for 2.5
	unsigned _int8	Reserved3;		
	unsigned _int8	DataEncryptAlgo;	// ENCRYPT_ALGO_xxx
	unsigned _int8	Reserved4;
	unsigned _int8	DataDigestAlgo;		// Not used till 2.0. DIGEST_ALGO_NONE or DIGEST_ALGO_CRC32 for 2.5
} BIN_PARAM_NEGOTIATION, *PBIN_PARAM_NEGOTIATION;

#define HW_TYPE_ASIC			0x00
#define HW_TYPE_UNSPECIFIED		0xFF

#define HW_VERSION_CURRENT		0x00
#define HW_VERSION_UNSPECIFIIED	0xFF

#define ENCRYPT_ALGO_NONE	0x00
#define ENCRYPT_ALGO_NKCDE_1	0x01 // Uses Decrypt32/Encrypt32
#define ENCRYPT_ALGO_AES128	0x02	//  AES32

#define DIGEST_ALGO_NONE	0x00
#define DIGEST_ALGO_CRC32	0x02


//////////////////////////////////////////////////////////////////////////
//
//
// BIN_PARAM_TYPE_TARGET_LIST.
//

typedef	struct _BIN_PARAM_TARGET_LIST_ELEMENT {

	unsigned _int32	TargetID;
	
	union {
	
		struct {

			unsigned _int8	NRRWHost;
			unsigned _int8	NRROHost;
			unsigned _int16	Reserved1;

		} V1; 	// NDAS 1.0, 1.1, 2.0

		struct {

			unsigned _int8	NREWHost;
			unsigned _int8	NRSWHost;
			unsigned _int8	NRROHost;
			unsigned _int8	Reserved1;

		} V2;	// NDAS 2.5
	};

	unsigned _int64	TargetData;

} BIN_PARAM_TARGET_LIST_ELEMENT, *PBIN_PARAM_TARGET_LIST_ELEMENT;

typedef struct _BIN_PARAM_TARGET_LIST {

	unsigned _int8		ParamType;
	unsigned _int8		NRTarget;
	unsigned _int16		Reserved1;

	BIN_PARAM_TARGET_LIST_ELEMENT	PerTarget[1];

} BIN_PARAM_TARGET_LIST, *PBIN_PARAM_TARGET_LIST;


//////////////////////////////////////////////////////////////////////////
//
//
// BIN_PARAM_TYPE_TARGET_DATA.
//

#define	PARAMETER_OP_GET	0x00
#define	PARAMETER_OP_SET	0xFF

typedef struct _BIN_PARAM_TARGET_DATA {
	unsigned _int8	ParamType;
	unsigned _int8	GetOrSet;		// PARAMETER_OP_GET or PARAMETER_OP_SET
	unsigned _int16	Reserved1;
	unsigned _int32	TargetID;
	unsigned _int64	TargetData;
} BIN_PARAM_TARGET_DATA, *PBIN_PARAM_TARGET_DATA;

//
// Targetdata fields.
// Protected by NDAS extension device lock.
//

#define TARGETDATA_REQUEST_COUNT_MASK	0x0000000000ffffffLL

#include <poppack.h>

#endif
