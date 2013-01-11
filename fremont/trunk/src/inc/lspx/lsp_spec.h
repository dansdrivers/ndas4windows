/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

#ifndef _LSP_SPEC_H_
#define _LSP_SPEC_H_

#include "lsp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "pshpack4.h"
/* hardware version */
typedef enum _lsp_hardware_version_t {
	LSP_HARDWARE_VERSION_1_0 = 0,
	LSP_HARDWARE_VERSION_1_1 = 1,
	LSP_HARDWARE_VERSION_2_0 = 2 /* frame.vhd : cHW_VERSION, cVERSION_ACTIVE */
} lsp_hardware_version_t;

typedef enum _lsp_hardware_version_max_t {
	LSP_HARDWARE_VERSION_MAX = LSP_HARDWARE_VERSION_2_0 /* frame.vhd : cVERSION_MAX */
} lsp_hardware_version_max_t;

/* hardware revision */
typedef enum _lsp_hardware_v20_revision_t {
	LSP_HARDWARE_V20_REV_0      = 0x0000,
	LSP_HARDWARE_V20_REV_1_100M = 0x0018,
	LSP_HARDWARE_V20_REV_1_G    = 0x0010
} lsp_hardware_v20_revision_t;

/* hardware protocol version */
typedef enum _lsp_ide_protocol_version_t {
	LSP_IDE_PROTOCOL_VERSION_1_0 = 0,
	LSP_IDE_PROTOCOL_VERSION_1_1 = 1
} lsp_ide_protocol_version_t;

typedef enum _lsp_ide_protocol_version_max_t {
	LSP_IDE_PROTOCOL_VERSION_MAX = LSP_IDE_PROTOCOL_VERSION_1_1
} lsp_ide_protocol_version_max_t;

typedef enum _lsp_opcode_t {
	/* Host to Remote */
	LSP_OPCODE_NOP_H2R					= 0x00,
	LSP_OPCODE_LOGIN_REQUEST			= 0x01,
	LSP_OPCODE_LOGOUT_REQUEST			= 0x02,
	LSP_OPCODE_TEXT_REQUEST				= 0x03,
	LSP_OPCODE_TASK_MANAGEMENT_REQUEST	= 0x04,
	LSP_OPCODE_SCSI_COMMAND				= 0x05,
	LSP_OPCODE_DATA_H2R					= 0x06,
	LSP_OPCODE_IDE_COMMAND				= 0x08,
	LSP_OPCODE_VENDOR_SPECIFIC_COMMAND	= 0x0F,
	/* Remote to Host */
	LSP_OPCODE_NOP_R2H					= 0x10,
	LSP_OPCODE_LOGIN_RESPONSE			= 0x11,
	LSP_OPCODE_LOGOUT_RESPONSE			= 0x12,
	LSP_OPCODE_TEXT_RESPONSE			= 0x13,
	LSP_OPCODE_TASK_MANAGEMENT_RESPONSE	= 0x14,
	LSP_OPCODE_SCSI_RESPONSE			= 0x15,
	LSP_OPCODE_DATA_R2H					= 0x16,
	LSP_OPCODE_READY_TO_TRANSFER		= 0x17,
	LSP_OPCODE_IDE_RESPONSE				= 0x18,
	LSP_OPCODE_VENDOR_SPECIFIC_RESPONSE	= 0x1F
} lsp_opcode_t;

typedef enum _lsp_phase_t {
	LSP_PHASE_SECURITY        = 0x00,
	LSP_PHASE_LOGIN_OPERATION = 0x01,
	LSP_PHASE_FULL_FEATURE    = 0x03,
	LSP_PHASE_LOGOUT          = 0x04
} lsp_phase_t;

/* Text commands */

typedef enum _lsp_text_command_type_t {
	LSP_TEXT_COMMAND_TYPE_TEXT = 0x00,
	LSP_TEXT_COMMAND_TYPE_BINARY = 0x01
} lsp_text_command_type_t;

/* Vendor commands */

typedef enum _lsp_vendor_cmd_code_t {
	LSP_VCMD_SET_RET_TIME              = 0x01,
	LSP_VCMD_SET_MAX_CONN_TIME         = 0x02,
	LSP_VCMD_SET_SUPERVISOR_PW         = 0x11,
	LSP_VCMD_SET_USER_PW               = 0x12,
	LSP_VCMD_SET_ENC_OPT               = 0x13,
	LSP_VCMD_SET_STANDBY_TIMER         = 0x14,
	LSP_VCMD_SET_MAC_ADDRESS           = 0xFE,
	LSP_VCMD_RESET                     = 0xFF,
	LSP_VCMD_SET_SEMA                  = 0x05,
	LSP_VCMD_FREE_SEMA                 = 0x06,
	LSP_VCMD_GET_SEMA                  = 0x07,
	LSP_VCMD_GET_OWNER_SEMA            = 0x08,
	LSP_VCMD_SET_DELAY                 = 0x16,
	LSP_VCMD_GET_DELAY                 = 0x17,
	LSP_VCMD_SET_DYNAMIC_MAX_CONN_TIME = 0x18,
	LSP_VCMD_GET_DYNAMIC_RET_TIME      = 0x19,
	LSP_VCMD_SET_D_ENC_OPT             = 0x1A,
	LSP_VCMD_GET_D_ENC_OPT             = 0x1B,
	LSP_VCMD_GET_RET_TIME              = 0x03,
	LSP_VCMD_GET_MAX_CONN_TIME         = 0x04,
	LSP_VCMD_GET_STANDBY_TIMER         = 0x15,
} lsp_vendor_cmd_code_t;

enum { 
	LSP_VENDOR_ID_XIMETA = 0x0001,
	LSP_VENDOR_OP_VERSION_1_0 = 0x01,
	LSP_VENDOR_OP_CURRENT_VERSION = LSP_VENDOR_OP_VERSION_1_0,
};

/* lsp passwords */

extern const lsp_uint8_t LSP_LOGIN_PASSWORD_ANY[8];
extern const lsp_uint8_t LSP_LOGIN_PASSWORD_SAMPLE[8];
extern const lsp_uint8_t LSP_LOGIN_PASSWORD_DEFAULT[8];
extern const lsp_uint8_t LSP_LOGIN_PASSWORD_SEAGATE[8];
extern const lsp_uint8_t LSP_LOGIN_PASSWORD_WINDOWS_RO[8];
extern const lsp_uint8_t LSP_LOGIN_PASSWORD_AUTO_REGISTRY[8];

/* lsp pdu header */

typedef struct _lsp_pdu_flags_t {
	union {
		lsp_uint8_t op_flags;
		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t T          : 1;
			lsp_uint8_t C          : 1;
			lsp_uint8_t _reserved_ : 2;
			lsp_uint8_t CSG        : 2;
			lsp_uint8_t NSG        : 2;
#else
			lsp_uint8_t NSG        : 2;
			lsp_uint8_t CSG        : 2;
			lsp_uint8_t _reserved_ : 2;
			lsp_uint8_t C          : 1;
			lsp_uint8_t T          : 1;
#endif
		}  __lsp_attr_packed__  login; /*  request & response */

		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t F          : 1;
			lsp_uint8_t _reserved_ : 7;
#else
			lsp_uint8_t _reserved_ : 7;
			lsp_uint8_t F          : 1;
#endif
		}  __lsp_attr_packed__  logout; /*  request & response */

		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t F          : 1;
			lsp_uint8_t _reserved_ : 7;
#else
			lsp_uint8_t _reserved_ : 7;
			lsp_uint8_t F          : 1;
#endif
		}  __lsp_attr_packed__  text_command; /*  request & response */

		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t F : 1;
			lsp_uint8_t R : 1;
			lsp_uint8_t W : 1;
			lsp_uint8_t _reserved_ : 5;
#else
			lsp_uint8_t _reserved_ : 5;
			lsp_uint8_t W : 1;
			lsp_uint8_t R : 1;
			lsp_uint8_t F : 1;
#endif
		}  __lsp_attr_packed__  ide_command; /*  request & response */

		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t F          : 1;
			lsp_uint8_t _reserved_ : 7;
#else
			lsp_uint8_t _reserved_ : 7;
			lsp_uint8_t F          : 1;
#endif
		}  __lsp_attr_packed__  vendor_command; /*  request & response */
	} __lsp_attr_packed__  u;
}  __lsp_attr_packed__  lsp_pdu_op_flags_t;

LSP_C_ASSERT_SIZEOF(lsp_pdu_op_flags_t, 1);

/* used for lsp V1.0 only */
typedef struct _lsp_ide_data_v0_t {

	lsp_uint8_t  _reserved_1_;
	lsp_uint8_t  feature;
	lsp_uint8_t  sector_count_prev;
	lsp_uint8_t  sector_count_cur;
	lsp_uint8_t  lba_low_prev;
	lsp_uint8_t  lba_low_cur;
	lsp_uint8_t  lba_mid_prev;
	lsp_uint8_t  lba_mid_cur;
	lsp_uint8_t  lba_high_prev;
	lsp_uint8_t  lba_high_cur;
	lsp_uint8_t  _reserved_2_;

	union {
		lsp_uint8_t device;
		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t obs2        : 1;
			lsp_uint8_t lba         : 1;
			lsp_uint8_t obs1        : 1;
			lsp_uint8_t dev         : 1;
			lsp_uint8_t lba_head_nr : 4;
#else
			lsp_uint8_t lba_head_nr : 4;
			lsp_uint8_t dev         : 1;
			lsp_uint8_t obs1        : 1;
			lsp_uint8_t lba         : 1;
			lsp_uint8_t obs2        : 1;
#endif
		}  __lsp_attr_packed__  s;
	}  __lsp_attr_packed__  u;

	lsp_uint8_t  _reserved_3_;
	lsp_uint8_t  command;
	lsp_uint16_t _reserved_4_;

} __lsp_attr_packed__   lsp_ide_data_v0_t;

LSP_C_ASSERT_SIZEOF(lsp_ide_data_v0_t, 16);

/*
	_lsp_ide_header is not an ide register data
	but the data used in NDAS chip to process ide command faster & easily
*/

/* !! aligned as 64bit integer */
typedef struct _lsp_ide_header_t {
#if defined(__BIG_ENDIAN__)
	lsp_uint32_t com_type_p    : 1; /* 0: Non-packet, 1: Packet Command */
	lsp_uint32_t com_type_k    : 1; /* 1: DVD Key (not used)*/
	lsp_uint32_t com_type_d_p  : 1; /* 0: PIO, 1: DMA */
	lsp_uint32_t com_type_w    : 1; /* 1: Write */
	lsp_uint32_t com_type_r    : 1; /* 1: Read */
	lsp_uint32_t com_type_e    : 1; /* 1: EXT Command */
	lsp_uint32_t com_len       : 26; /* bytes sending or receiving */
#else
	lsp_uint32_t com_len       : 26; /* bytes sending or receiving */
	lsp_uint32_t com_type_e    : 1; /* 1: EXT Command */
	lsp_uint32_t com_type_r    : 1; /* 1: Read */
	lsp_uint32_t com_type_w    : 1; /* 1: Write */
	lsp_uint32_t com_type_d_p  : 1; /* 0: PIO, 1: DMA */
	lsp_uint32_t com_type_k    : 1; /* 1: DVD Key (not used)*/
	lsp_uint32_t com_type_p    : 1; /* 0: Non-packet, 1: Packet Command */
#endif
}  __lsp_attr_packed__  lsp_ide_header_t;

LSP_C_ASSERT_SIZEOF(lsp_ide_header_t, 4);

/* sent to NDAS */
typedef struct _lsp_ide_register_t {
	lsp_uint8_t feature_prev;
	lsp_uint8_t feature_cur;
	lsp_uint8_t sector_count_prev;
	lsp_uint8_t sector_count_cur;
	lsp_uint8_t lba_low_prev;
	lsp_uint8_t lba_low_cur;
	lsp_uint8_t lba_mid_prev;
	lsp_uint8_t lba_mid_cur;
	lsp_uint8_t lba_high_prev;
	lsp_uint8_t lba_high_cur;
	lsp_uint8_t command;
	lsp_uint8_t device; /*  status for return */
}  __lsp_attr_packed__  lsp_ide_register_t;

/* sent to ide_command */
typedef struct _lsp_ide_register_param_t {

	/*  registers */
	union {
		lsp_uint8_t device;
		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t obs2        : 1; /*  obsolete */
			lsp_uint8_t lba         : 1; /*  1 : use lba */
			lsp_uint8_t obs1        : 1; /*  obsolete */
			lsp_uint8_t dev         : 1; /*  set with target id */
			lsp_uint8_t lba_head_nr : 4; /*  reserved(used as 4 MSBs) */
#else
			lsp_uint8_t lba_head_nr : 4; /*  reserved(used as 4 MSBs) */
			lsp_uint8_t dev         : 1; /*  set with target id */
			lsp_uint8_t obs1        : 1; /*  obsolete */
			lsp_uint8_t lba         : 1; /*  1 : use lba */
			lsp_uint8_t obs2        : 1; /*  obsolete */
#endif
		}  __lsp_attr_packed__   s;
	}  __lsp_attr_packed__  device; /*  1 bytes */

	union {
		lsp_uint8_t  command;
		struct {
#if defined(__BIG_ENDIAN__)
			lsp_uint8_t bsy         : 1; /*  BSY */
			lsp_uint8_t drdy        : 1; /*  DRDY */
			lsp_uint8_t df          : 1; /*  DF */
			lsp_uint8_t obs         : 1; /*  obsolete */
			lsp_uint8_t drq         : 1; /*  DRQ */
			lsp_uint8_t obs2        : 1; /*  obsolete */
			lsp_uint8_t obs1        : 1; /*  obsolete */
			lsp_uint8_t err         : 1; /*  ERR (only valid  bit) */
#else
			lsp_uint8_t err         : 1; /*  ERR (only valid  bit) */
			lsp_uint8_t obs1        : 1; /*  obsolete */
			lsp_uint8_t obs2        : 1; /*  obsolete */
			lsp_uint8_t drq         : 1; /*  DRQ */
			lsp_uint8_t obs         : 1; /*  obsolete */
			lsp_uint8_t df          : 1; /*  DF */
			lsp_uint8_t drdy        : 1; /*  DRDY */
			lsp_uint8_t bsy         : 1; /*  BSY */
#endif
		} __lsp_attr_packed__   status;
	} __lsp_attr_packed__  command; /*  1 bytes */

	union {
		/*  if use_48 is not set, use basic or named */
		struct {
			lsp_uint8_t reserved[5];
			lsp_uint8_t reg[5];
		}  __lsp_attr_packed__  basic; /*  5 bytes */

		struct {
			lsp_uint8_t reserved[5];
			lsp_uint8_t  features;
			lsp_uint8_t  sector_count;
			lsp_uint8_t  lba_low;
			lsp_uint8_t  lba_mid;
			lsp_uint8_t  lba_high;
		}  __lsp_attr_packed__  named; /*  5 bytes */

		/*  if use_48 is set, use basic_48 or named_48 */
		struct {
			lsp_uint8_t reg_prev[5];
			lsp_uint8_t reg_cur[5];
		}  __lsp_attr_packed__  basic_48; /*  10 bytes */

		struct {
			struct {
				lsp_uint8_t  features;
				lsp_uint8_t  sector_count;
				lsp_uint8_t  lba_low;
				lsp_uint8_t  lba_mid;
				lsp_uint8_t  lba_high;
			}  __lsp_attr_packed__  prev; /*  5 bytes, previous */

			struct {
				lsp_uint8_t  features;
				lsp_uint8_t  sector_count;
				lsp_uint8_t  lba_low;
				lsp_uint8_t  lba_mid;
				lsp_uint8_t  lba_high;
			}  __lsp_attr_packed__  cur; /*  5 bytes, current */
		}  __lsp_attr_packed__  named_48; /*  10 bytes */

		struct {
			lsp_uint8_t obs1[5];
			union{
				lsp_uint8_t  err_na;
				struct {
#if defined(__BIG_ENDIAN__)
					lsp_uint8_t  icrc   : 1;
					lsp_uint8_t  wp     : 1;
					lsp_uint8_t  mc     : 1;
					lsp_uint8_t  idnf   : 1;
					lsp_uint8_t  mcr    : 1;
					lsp_uint8_t  abrt   : 1;
					lsp_uint8_t  nm     : 1;
					lsp_uint8_t  obs    : 1;
#else
					lsp_uint8_t  obs    : 1;
					lsp_uint8_t  nm     : 1;
					lsp_uint8_t  abrt   : 1;
					lsp_uint8_t  mcr    : 1;
					lsp_uint8_t  idnf   : 1;
					lsp_uint8_t  mc     : 1;
					lsp_uint8_t  wp     : 1;
					lsp_uint8_t  icrc   : 1;
#endif
				}  __lsp_attr_packed__  err_op; /*  valid if command.status.err is set */
			} __lsp_attr_packed__   err;
			lsp_uint8_t obs2[4];
		} __lsp_attr_packed__   ret;

	} __lsp_attr_packed__   reg;
	
}  __lsp_attr_packed__  lsp_ide_register_param_t;

LSP_C_ASSERT_SIZEOF(lsp_ide_register_param_t, 2 + 5 + 5);

/*  send to lsp */
typedef union _lsp_vendor_param_t {
	/*  under only super user permission */
	struct {
		lsp_uint64_t time; /*  in : micro second (value +1) */
	}  __lsp_attr_packed__  set_RET_TIME;

	struct {
		lsp_uint64_t time; /*  in : micro second */
	}  __lsp_attr_packed__  set_max_conn_time;

	struct {
		lsp_uint8_t password[8]; /*  in : new password */
	}  __lsp_attr_packed__  set_supervisor_pw;

	struct {
		lsp_uint8_t password[8]; /*  in : new password */
	}  __lsp_attr_packed__  set_user_pw;

	struct {
		lsp_uint8_t header;
		lsp_uint8_t data;
	}  __lsp_attr_packed__  set_enc_opt;

	struct {
		lsp_uint8_t enable; /*  1 : in : enable, 0 : disable */
		lsp_uint32_t standby_time; /*  in : minute 31 LSBs accepted */
	}  __lsp_attr_packed__  set_standby_timer;

	struct {
		lsp_uint8_t no_param; /*  do not set any parameters */
	}  __lsp_attr_packed__  reset;

	/*  under only normal permission */
	struct {
		lsp_uint8_t sema_index; /*  in : 0 ~ 3 */
		lsp_uint32_t sema_counter; /*  out */
	}  __lsp_attr_packed__  set_sema;

	struct {
		lsp_uint8_t sema_index; /*  in : 0 ~ 3 */
		lsp_uint32_t sema_counter; /*  out */
	}  __lsp_attr_packed__  free_sema;

	struct {
		lsp_uint8_t sema_index; /*  in : 0 ~ 3 */
		lsp_uint32_t sema_counter; /*  out */
	}  __lsp_attr_packed__  get_sema;

	struct {
		lsp_uint8_t sema_index; /*  in : 0 ~ 3 */
		lsp_uint8_t max_address[6]; /*  out : owner's mac address */
	}  __lsp_attr_packed__  get_owner_sema;

	struct {
		lsp_uint64_t time; /*  in : micro second */
	}  __lsp_attr_packed__  set_dynamic_RET_TIME;

	struct {
		lsp_uint64_t time; /*  in : micro second */
	}  __lsp_attr_packed__  set_dynamic_max_conn_time;

	/*  under all user permission */

	struct {
		lsp_uint64_t time; /*  out : micro second */
	}  __lsp_attr_packed__  get_RET_TIME;

	struct {
		lsp_uint64_t time; /*  out : micro second */
	}  __lsp_attr_packed__  get_max_conn_time;

	struct {
		lsp_uint64_t time; /*  out : minute */
	}  __lsp_attr_packed__  get_max_standby_timer;
}  __lsp_attr_packed__   lsp_vendor_param_t;

/*  LSP_C_ASSERT_SIZEOF(lsp_vendor, 8); */

/*  send to NDAS */
typedef union _lsp_vendor_v1_t {
	lsp_uint64_t param_64;

	/*  under only super user permission */
	struct {
		lsp_uint64_t time; /*  micro second (value +1) */
	} __lsp_attr_packed__ set_RET_TIME;

	struct {
		lsp_uint64_t time; /*  micro second */
	} __lsp_attr_packed__ set_max_conn_time;

	struct {
		lsp_uint8_t password[8]; /*  new password */
	} __lsp_attr_packed__ set_supervisor_pw;

	struct {
		lsp_uint8_t password[8]; /*  new password */
	} __lsp_attr_packed__ set_user_pw;

	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs    : 62;
		lsp_uint64_t data   :1; /*  1 : encrypt, 0 : not encrypt */
		lsp_uint64_t header :1; /*  1 : encrypt, 0 : not encrypt */
#else
		lsp_uint64_t header :1; /*  1 : encrypt, 0 : not encrypt */
		lsp_uint64_t data   :1; /*  1 : encrypt, 0 : not encrypt */
		lsp_uint64_t obs    : 62;
#endif
	} __lsp_attr_packed__ set_enc_opt;

	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs    : 32;
		lsp_uint64_t enable : 1; /*  1 : enable, 0 : disable */
		lsp_uint64_t time   : 31; /*  minute */
#else
		lsp_uint64_t time   : 31; /*  minute */
		lsp_uint64_t enable : 1; /*  1 : enable, 0 : disable */
		lsp_uint64_t obs    : 32;
#endif
	} __lsp_attr_packed__ set_standby_timer;

	struct {
		lsp_uint64_t no_param; /*  invalid */
	} __lsp_attr_packed__ reset;

	/*  under only normal permission */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs          : 30;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t sema_counter : 32;
#else
		lsp_uint64_t sema_counter : 32;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t obs          : 30;
#endif
	} __lsp_attr_packed__ set_sema;

	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs          : 30;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t sema_counter : 32;
#else
		lsp_uint64_t sema_counter : 32;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t obs          : 30;
#endif
	} __lsp_attr_packed__ free_sema;

	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs          : 30;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t sema_counter : 32;
#else
		lsp_uint64_t sema_counter : 32;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t obs          : 30;
#endif
	} __lsp_attr_packed__ get_sema;

	/*  in/out get_owner_sema parameter overlaps each other */
	/*  so, we neeed each struct for request/return */
	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs2         : 30;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t obs1         : 32;
#else
		lsp_uint64_t obs1         : 32;
		lsp_uint64_t sema_index   : 2;
		lsp_uint64_t obs2         : 30;
#endif
	} __lsp_attr_packed__ get_owner_sema_request;

	struct {
#if defined(__BIG_ENDIAN__)
		lsp_uint64_t obs          : 16;
		lsp_uint64_t mac_address  : 48;
#else
		lsp_uint64_t mac_address  : 48;
		lsp_uint64_t obs          : 16;
#endif
	} __lsp_attr_packed__ get_owner_sema_return;

	struct {
		lsp_uint64_t time; /*  micro second */
	} __lsp_attr_packed__ set_dynamic_RET_TIME;

	struct {
		lsp_uint64_t time; /*  micro second */
	} __lsp_attr_packed__ set_dynamic_max_conn_time;

	/*  under all user permission */

	struct {
		lsp_uint64_t time; /*  micro second */
	} __lsp_attr_packed__ get_RET_TIME;

	struct {
		lsp_uint64_t time; /*  micro second */
	} __lsp_attr_packed__ get_max_conn_time;

	struct {
		lsp_uint64_t time; /*  minute */
	} __lsp_attr_packed__ get_max_standby_timer;

}__lsp_attr_packed__ lsp_vendor_v1_t;

LSP_C_ASSERT_SIZEOF(lsp_vendor_v1_t, 8);

typedef struct _lsp_pdu_op_data_t {
	union {
		lsp_uint8_t op_data[16];
		struct {
			lsp_uint32_t reserved2;
			lsp_uint32_t reserved3;
			lsp_uint32_t reserved4;
			lsp_uint32_t reserved5;
		} __lsp_attr_packed__ reserved; /* comply with older library */

		struct {
			lsp_uint8_t  parm_type;
			lsp_uint8_t  parm_ver;
			lsp_uint8_t  ver_max;
			lsp_uint8_t  ver_min;
			lsp_uint8_t  _reserved_[12];
		} __lsp_attr_packed__ login;  /*  request */

		struct {
			lsp_uint8_t  parm_type;
			lsp_uint8_t  parm_ver;
			lsp_uint8_t  ver_max;
			lsp_uint8_t  ver_active;
			lsp_uint16_t revision;
			lsp_uint8_t  _reserved_[10];
		} __lsp_attr_packed__ login_response; /*  response */

		struct {
			lsp_uint8_t  _reserved_[16];
		} __lsp_attr_packed__ logout; /*  request & response */

		struct {
			lsp_uint8_t  parm_type;
			lsp_uint8_t  parm_ver;
			lsp_uint8_t  _reserved_[14];
		} __lsp_attr_packed__ text_command; /*  request & response */

		struct {
			lsp_ide_header_t   header;
			lsp_ide_register_t register_data;
		} __lsp_attr_packed__ ide_command; /*  request & response */

		lsp_ide_data_v0_t ide_command_v0;

		struct {
			lsp_uint16_t vendor_id; /* 0x0001 as XIMETA */
			lsp_uint8_t vop_ver;
			lsp_uint8_t vop_code;
			lsp_uint8_t vop_parm[12];
		} __lsp_attr_packed__ vendor_command; /*  request & response */
	}__lsp_attr_packed__  u;
} __lsp_attr_packed__  lsp_pdu_op_data_t;

LSP_C_ASSERT_SIZEOF(lsp_pdu_op_data_t, 16);

/* LANSCSI PDU */
/*

basic header segment ( 15 * 4 = 60 bytes) -> lsp_pdu_hdr
[ packet_command (v 1.1 or later and IDE command only) ]
additional header segment (optional)
header digest (optional)
data segment (optional)
data digest (optional)

*/

typedef struct _lsp_pdu_hdr_t {

	lsp_uint8_t  op_code;
	lsp_pdu_op_flags_t op_flags;   /* lsp_pdu_op_flags */
	lsp_uint8_t  response;         /* remote to host only */
	lsp_uint8_t  status;           /* remote to host and IDE only */

	lsp_uint32_t hpid;             /* host path id */
	lsp_uint16_t rpid;             /* remote path id */
	lsp_uint16_t cpslot;           /* command processing slot no */
	lsp_uint32_t dataseg_len;      /* data segment length */
	lsp_uint16_t ahs_len;          /* additional header segment length */
	lsp_uint16_t cmd_subpkt_seq;   /* command sub-packet sequence no */

	lsp_uint32_t path_cmd_tag;     /* path command tag */
	lsp_uint32_t init_task_tag;    /* initiator task tag */

	lsp_uint32_t data_trans_len;   /* data transfer len */

	lsp_uint32_t target_id;        /* target id */
	lsp_uint32_t lun0;              /* lun */
	lsp_uint32_t lun1;              /* lun */

	lsp_pdu_op_data_t op_data;     /* lsp_pdu_op_data */

} __lsp_attr_packed__ lsp_pdu_hdr_t;

LSP_C_ASSERT_SIZEOF(lsp_pdu_hdr_t, 60);

/* lsp packet command */

#define LSP_PACKET_COMMAND_SIZE	12

typedef struct _lsp_packet_command_t {
	lsp_uint8_t  command[LSP_PACKET_COMMAND_SIZE];
} __lsp_attr_packed__ lsp_packet_command_t;

#if !defined(__GNUC__)
#include <poppack.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* _LSP_SPEC_H_ */
