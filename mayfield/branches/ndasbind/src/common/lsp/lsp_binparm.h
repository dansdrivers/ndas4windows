#ifndef _LSP_BINPARM_H_
#define _LSP_BINPARM_H_

#include <pshpack4.h>

#include <lsp/lsp_type.h>

/*  Parameter Types */

#define LSP_BINPARM_CURRENT_VERSION	0

#define	LSP_BINPARM_TYPE_SECURITY		0x1
#define	LSP_BINPARM_TYPE_NEGOTIATION	0x2
#define	LSP_BINPARM_TYPE_TARGET_LIST	0x3
#define	LSP_BINPARM_TYPE_TARGET_DATA	0x4

#define LSP_BINPARM_SIZE_LOGIN_FIRST_REQUEST		4
#define	LSP_BINPARM_SIZE_LOGIN_SECOND_REQUEST		8
#define LSP_BINPARM_SIZE_LOGIN_THIRD_REQUEST		32
#define LSP_BINPARM_SIZE_LOGIN_FOURTH_REQUEST		28
#define	LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REQUEST	4
#define	LSP_BINPARM_SIZE_TEXT_TARGET_DATA_REQUEST	16

#define LSP_BINPARM_SIZE_LOGIN_FIRST_REPLY		4
#define	LSP_BINPARM_SIZE_LOGIN_SECOND_REPLY		36
#define LSP_BINPARM_SIZE_LOGIN_THIRD_REPLY		4
#define LSP_BINPARM_SIZE_LOGIN_FOURTH_REPLY		28
#define	LSP_BINPARM_SIZE_TEXT_TARGET_LIST_REPLY	4
#define	LSP_BINPARM_SIZE_TEXT_TARGET_DATA_REPLY	8

#define LSP_BINPARM_SIZE_REPLY					36

typedef struct _lsp_binparm {
	lsp_uint8  parm_type;
	lsp_uint8  _reserved_1;
	lsp_uint16 _reserved_2;
} lsp_binparm, *lsp_binparm_ptr;

C_ASSERT_SIZEOF(lsp_binparm, 4);

/* LSP_BINPARM_TYPE_SECURITY. */

#define	LSP_LOGIN_TYPE_NORMAL   0x00
#define	LSP_LOGIN_TYPE_DISCOVER 0xFF

/* CHAP authentication parameters. */

#define LSP_AUTH_METHOD_NONE 0x0000 /* 1.1 or later only */
#define	LSP_AUTH_METHOD_CHAP 0x0001

#define LSP_FIRST_TARGET_RO_USER  0x00000001
#define LSP_FIRST_TARGET_RW_USER  0x00010001
#define LSP_SECOND_TARGET_RO_USER 0x00000002
#define LSP_SECOND_TARGET_RW_USER 0x00020002
#define	LSP_NDAS_SUPERVISOR       0xFFFFFFFF

typedef struct _lsp_authparm_chap {
	lsp_uint32	chap_a;
	lsp_uint32	chap_i;
	lsp_uint32	chap_n;
	lsp_uint32	chap_r[4];
	lsp_uint32	chap_c[1]; /* variable length */
} lsp_authparm_chap, *lsp_authparm_chap_ptr;

C_ASSERT_SIZEOF(lsp_authparm_chap, 32);

#define LSP_HASH_ALGORITHM_MD5        0x00000005
#define LSP_CHAP_MAX_CHALLENGE_LENGTH 4

typedef struct _lsp_binparm_security {
	lsp_uint8 parm_type;     /* parameter type */
	lsp_uint8 login_type;    /* login type */
	lsp_uint16 auth_method;  /* authentication method */
	union {
		lsp_uint8 auth_parm[1];	// Variable Length.
		lsp_authparm_chap auth_chap;
	};
} lsp_binparm_security, *lsp_binparm_security_ptr;

C_ASSERT_SIZEOF(lsp_binparm_security, 36);

/* LSP_BINPARM_TYPE_NEGOTIATION */

#define LSP_HW_TYPE_ASIC   0x00
#define LSP_HW_TYPE_UNSPEC 0xFF

#define LSP_HW_VER_1_0    0x00
#define LSP_HW_VER_1_1    0x01
#define LSP_HW_VER_2_0    0x02

#define LSP_HW_VER_CUR    0x00
#define LSP_HW_VER_UNSPEC 0xFF

typedef struct _lsp_binparm_negotiation {
	lsp_uint8  parm_type;     /* parameter type */
	lsp_uint8  hw_type;       /* hardware type */
	lsp_uint8  hw_ver;        /* hardware version */
	lsp_uint8  _reserved_;
	lsp_uint32 slot_cnt;      /* number of slots */
	lsp_uint32 max_blocks;    /* maximum blocks */
	lsp_uint32 max_target_id; /* maximum number of Target ID */
	lsp_uint32 max_lun;       /* maximum number of LUN ID */
	lsp_uint16 hdr_enc_alg;   /* header encryption algorithm */
	lsp_uint16 hdr_dig_alg;   /* header digest algorithm */
	lsp_uint16 dat_enc_alg;   /* data encryption algorithm */
	lsp_uint16 dat_dig_alg;   /* data digest algorithm */
} lsp_binparm_negotiation, *lsp_binparm_negotiation_ptr;

C_ASSERT_SIZEOF(lsp_binparm_negotiation, 28);

/* LSP_BINPARM_TYPE_TARGET_LIST */

typedef	struct _lsp_binparm_target_list_elem {
	lsp_uint32 target_id;
	lsp_uint8  rw_hosts_cnt;
	lsp_uint8  ro_hosts_cnt;
	lsp_uint16 _reserved_;
	lsp_uint64 target_data;
} lsp_binparm_target_list_elem, *lsp_binparm_target_list_elem_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_list_elem, 16);

typedef struct _lsp_target_list {
	lsp_uint8  parm_type;
	lsp_uint8  target_cnt;
	lsp_uint16 _reserved_;
//	lsp_binparm_target_list_elem targets[1];
} lsp_binparm_target_list, *lsp_binparm_target_list_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_list, 4);

/* LSP_BINPARM_TYPE_TARGET_DATA */

#define	LSP_BINPARM_OP_GET	0x00
#define	LSP_BINPARM_OP_SET	0xFF

typedef struct _lsp_target_data {
	lsp_uint8  parm_type;
	lsp_uint8  op_get_or_set;
	lsp_uint16 _reserved_;
	lsp_uint32 target_id;
	lsp_uint64 target_data;
} lsp_binparm_target_data, *lsp_binparm_target_data_ptr;

C_ASSERT_SIZEOF(lsp_binparm_target_data, 16);

#include <poppack.h>

#endif /* _LSP_BINPARM_H_ */
