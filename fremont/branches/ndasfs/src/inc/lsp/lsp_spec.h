#ifndef _LSP_SPEC_H_
#define _LSP_SPEC_H_

#include "lsp_type.h"

#include <pshpack4.h>

#define LSP_IDEPROTO_VERSION_1_0 0x00
#define LSP_IDEPROTO_VERSION_1_1 0x01
#define LSP_IDEPROTO_VERSION_CUR LSP_IDEPROTO_VERSION_1_1

/* Host to Remote */
#define LSP_OPCODE_NOP_H2R						0x00
#define LSP_OPCODE_LOGIN_REQUEST				0x01
#define LSP_OPCODE_LOGOUT_REQUEST				0x02
#define LSP_OPCODE_TEXT_REQUEST					0x03
#define LSP_OPCODE_TASK_MANAGEMENT_REQUEST		0x04
#define LSP_OPCODE_SCSI_COMMAND					0x05
#define LSP_OPCODE_DATA_H2R						0x06
#define LSP_OPCODE_IDE_COMMAND					0x08
#define LSP_OPCODE_VENDOR_SPECIFIC_COMMAND		0x0F

/* Remote to Host */
#define LSP_OPCODE_NOP_R2H						0x10
#define LSP_OPCODE_LOGIN_RESPONSE				0x11
#define LSP_OPCODE_LOGOUT_RESPONSE				0x12
#define LSP_OPCODE_TEXT_RESPONSE				0x13
#define LSP_OPCODE_TASK_MANAGEMENT_RESPONSE		0x14
#define LSP_OPCODE_SCSI_RESPONSE				0x15
#define LSP_OPCODE_DATA_R2H						0x16
#define LSP_OPCODE_READY_TO_TRANSFER			0x17
#define LSP_OPCODE_IDE_RESPONSE					0x18
#define LSP_OPCODE_VENDOR_SPECIFIC_RESPONSE		0x1F

#define	LSP_PHASE_SECURITY						0x00
#define	LSP_PHASE_LOGIN_OPERATION				0x01
#define	LSP_PHASE_FULL_FEATURE					0x03
#define LSP_PHASE_LOGOUT						0x04

/* Vendor commands */
#define LSP_VCMD_SET_RET_TIME               0x01
#define LSP_VCMD_SET_MAX_CONN_TIME              0x02
#define LSP_VCMD_SET_SUPERVISOR_PW              0x11
#define LSP_VCMD_SET_USER_PW                    0x12
#define LSP_VCMD_SET_ENC_OPT                    0x13
#define LSP_VCMD_SET_STANDBY_TIMER              0x14
#define LSP_VCMD_SET_MAC_ADDRESS				0xFE
#define LSP_VCMD_RESET                          0xFF
#define LSP_VCMD_SET_SEMA                       0x05
#define LSP_VCMD_FREE_SEMA                      0x06
#define LSP_VCMD_GET_SEMA                       0x07
#define LSP_VCMD_GET_OWNER_SEMA                 0x08
#define LSP_VCMD_SET_DELAY                      0x16
#define LSP_VCMD_GET_DELAY                      0x17
#define LSP_VCMD_SET_DYNAMIC_MAX_CONN_TIME      0x18
#define LSP_VCMD_GET_DYNAMIC_RET_TIME       0x19
#define LSP_VCMD_SET_D_ENC_OPT                  0x1A
#define LSP_VCMD_GET_D_ENC_OPT                  0x1B
#define LSP_VCMD_GET_RET_TIME               0x03
#define LSP_VCMD_GET_MAX_CONN_TIME              0x04
#define LSP_VCMD_GET_STANDBY_TIMER              0x15

#define	LSP_VENDOR_ID_XIMETA			0x0001

#define	LSP_VENDOR_OP_CURRENT_VERSION	0x01


/* lsp pdu header */

typedef struct _lsp_pdu_flags {
	union {
		lsp_uint8 op_flags;
		struct {
			lsp_uint8 NSG        : 2;
			lsp_uint8 CSG        : 2;
			lsp_uint8 _reserved_ : 2;
			lsp_uint8 C          : 1;
			lsp_uint8 T          : 1;
		} login; /*  request & response */

		struct {
			lsp_uint8 _reserved_ : 7;
			lsp_uint8 F          : 1;
		} logout; /*  request & response */

		struct {
			lsp_uint8 _reserved_ : 7;
			lsp_uint8 F          : 1;
		} text_command; /*  request & response */

		struct {
			lsp_uint8 _reserved_ : 5;
			lsp_uint8 W : 1;
			lsp_uint8 R : 1;
			lsp_uint8 F : 1;
		} ide_command; /*  request & response */

		struct {
			lsp_uint8 _reserved_ : 7;
			lsp_uint8 F          : 1;
		} vendor_command; /*  request & response */
	};
} lsp_pdu_op_flags;

C_ASSERT_SIZEOF(lsp_pdu_op_flags, 1);

/* used for lsp V1.0 only */
typedef struct _lsp_ide_data_v0 {

	lsp_uint8  _reserved_1_;
	lsp_uint8  feature;
	lsp_uint8  sector_count_prev;
	lsp_uint8  sector_count_cur;
	lsp_uint8  lba_low_prev;
	lsp_uint8  lba_low_cur;
	lsp_uint8  lba_mid_prev;
	lsp_uint8  lba_mid_cur;
	lsp_uint8  lba_high_prev;
	lsp_uint8  lba_high_cur;
	lsp_uint8  _reserved_2_;

	union {
		lsp_uint8 device;
		struct {
			lsp_uint8 lba_head_nr : 4;
			lsp_uint8 dev         : 1;
			lsp_uint8 obs1        : 1;
			lsp_uint8 lba         : 1;
			lsp_uint8 obs2        : 1;
		};
	};

	lsp_uint8  _reserved_3_;
	lsp_uint8  command;
	lsp_uint16 _reserved_4_;

} lsp_ide_data_v0, *lsp_ide_data_v0_ptr;

C_ASSERT_SIZEOF(lsp_ide_data_v0, 16);

/*
	_lsp_ide_header is not an ide reigster data
	but the data used in NDAS chip to process ide command faster & easily
*/

/* !! aligned as 64bit integer */
typedef struct _lsp_ide_header {
	lsp_uint32 com_len       : 26; /* bytes sending or receiving */
	lsp_uint32 com_type_e    : 1; /* 1: EXT Command */
	lsp_uint32 com_type_r    : 1; /* 1: Read */
	lsp_uint32 com_type_w    : 1; /* 1: Write */
	lsp_uint32 com_type_d_p  : 1; /* 0: PIO, 1: DMA */
	lsp_uint32 com_type_k    : 1; /* 1: DVD Key (not used)*/
	lsp_uint32 com_type_p    : 1; /* 0: Non-packet, 1: Packet Command */
} lsp_ide_header, *lsp_ide_header_ptr;

C_ASSERT_SIZEOF(lsp_ide_header, 4);

/*
Commands unique to the 48-bit Address feature set

!51h configure stream
EAh flush cache ext
25h read dma ext
26h read dma queued ext
!2Fh read log ext
29h read multiple ext
27h read native max address ext
24h read sector(s) ext
!2Ah read stream dma ext
!2Bh read stream ext
42h read verify sector(s) ext
37h set max address ext
35h write dma ext
3Dh write dma fua ext
36h write dma queued ext
3Eh write dma queued fua ext
!3Fh write log ext
39h write multiple ext
CEh write multiple fua ext
34h write sector(s) ext
!3Ah write stream dma ext
!3Bh write stream ext
! : not sure
*/

/*
commands using packet io

A0h packet
*/

/*
command using dma

...
*/

/* sent to NDAS */
typedef struct _lsp_ide_register {
	lsp_uint8 feature_prev;
	lsp_uint8 feature_cur;
	lsp_uint8 sector_count_prev;
	lsp_uint8 sector_count_cur;
	lsp_uint8 lba_low_prev;
	lsp_uint8 lba_low_cur;
	lsp_uint8 lba_mid_prev;
	lsp_uint8 lba_mid_cur;
	lsp_uint8 lba_high_prev;
	lsp_uint8 lba_high_cur;
	lsp_uint8 command;
	lsp_uint8 device; /*  status for return */
} lsp_ide_register, *lsp_ide_register_ptr;

/* sent to ide_command */
typedef struct _lsp_ide_register_param {
	lsp_uint8 use_dma; /*  1 : DMA, 0 : PIO */
	lsp_uint8 use_48; /*  1 : 48 bit address feature, 0 : use not */

	/*  registers */
	union {
		lsp_uint8 device;
		struct {
			lsp_uint8 lba_head_nr : 4; /*  reserved(used as 4 MSBs) */
			lsp_uint8 dev         : 1; /*  set with target id */
			lsp_uint8 obs1        : 1; /*  obsolete */
			lsp_uint8 lba         : 1; /*  1 : use lba */
			lsp_uint8 obs2        : 1; /*  obsolete */
		};
	} device; /*  1 bytes */

	union {
		lsp_uint8  command;
		struct {
			lsp_uint8 err         : 1; /*  ERR (only valid  bit) */
			lsp_uint8 obs1        : 1; /*  obsolete */
			lsp_uint8 obs2        : 1; /*  obsolete */
			lsp_uint8 drq         : 1; /*  DRQ */
			lsp_uint8 obs         : 1; /*  obsolete */
			lsp_uint8 df          : 1; /*  DF */
			lsp_uint8 drdy        : 1; /*  DRDY */
			lsp_uint8 bsy         : 1; /*  BSY */
		} status;
	} command; /*  1 bytes */

	union {
		/*  if use_48 is not set, use basic or named */
		struct {
			lsp_uint8 reg[5];
		} basic; /*  5 bytes */

		struct {
			lsp_uint8  features;
			lsp_uint8  sector_count;
			lsp_uint8  lba_low;
			lsp_uint8  lba_mid;
			lsp_uint8  lba_high;
		} named; /*  5 bytes */

		/*  if use_48 is set, use basic_48 or named_48 */
		struct {
			lsp_uint8 reg_prev[5];
			lsp_uint8 reg_cur[5];
		} basic_48; /*  10 bytes */

		struct {
			struct {
				lsp_uint8  features;
				lsp_uint8  sector_count;
				lsp_uint8  lba_low;
				lsp_uint8  lba_mid;
				lsp_uint8  lba_high;
			} prev; /*  5 bytes, previous */

			struct {
				lsp_uint8  features;
				lsp_uint8  sector_count;
				lsp_uint8  lba_low;
				lsp_uint8  lba_mid;
				lsp_uint8  lba_high;
			} cur; /*  5 bytes, current */
		} named_48; /*  10 bytes */

		struct {
			lsp_uint8 obs1[5];
			union{
				lsp_uint8  err_na;
				struct {
					lsp_uint8  obs    : 1;
					lsp_uint8  nm     : 1;
					lsp_uint8  abrt   : 1;
					lsp_uint8  mcr    : 1;
					lsp_uint8  idnf   : 1;
					lsp_uint8  mc     : 1;
					lsp_uint8  wp     : 1;
					lsp_uint8  icrc   : 1;
				} err_op; /*  valid if command.status.err is set */
			} err;
			lsp_uint8 obs2[4];
		} ret;

	} reg;
	
} lsp_ide_register_param, *lsp_ide_register_param_ptr;

C_ASSERT_SIZEOF(lsp_ide_register_param, 2 + 2 + 5 + 5);

/*  send to lsp */
typedef union _lsp_vendor_param {
	/*  under only super user permission */
	struct {
		lsp_uint64 time; /*  in : micro second (value +1) */
	} set_RET_TIME;

	struct {
		lsp_uint64 time; /*  in : micro second */
	} set_max_conn_time;

	struct {
		lsp_uint64 password; /*  in : new password */
	} set_supervisor_pw;

	struct {
		lsp_uint64 password; /*  in : new password */
	} set_user_pw;

	struct {
		lsp_uint8 header;
		lsp_uint8 data;
	} set_enc_opt;

	struct {
		lsp_uint8 enable; /*  1 : in : enable, 0 : disable */
		lsp_uint32 standby_time; /*  in : minute 31 LSBs accepted */
	} set_standby_timer;

	struct {
		lsp_uint8 no_param; /*  do not set any parameters */
	} reset;

	/*  under only normal permission */
	struct {
		lsp_uint8 sema_index; /*  in : 0 ~ 3 */
		lsp_uint32 sema_counter; /*  out */
	} set_sema;

	struct {
		lsp_uint8 sema_index; /*  in : 0 ~ 3 */
		lsp_uint32 sema_counter; /*  out */
	} free_sema;

	struct {
		lsp_uint8 sema_index; /*  in : 0 ~ 3 */
		lsp_uint32 sema_counter; /*  out */
	} get_sema;

	struct {
		lsp_uint8 sema_index; /*  in : 0 ~ 3 */
		lsp_uint8 max_address[6]; /*  out : owner's mac address */
	} get_owner_sema;

	struct {
		lsp_uint64 time; /*  in : micro second */
	} set_dynamic_RET_TIME;

	struct {
		lsp_uint64 time; /*  in : micro second */
	} set_dynamic_max_conn_time;

	/*  under all user permission */

	struct {
		lsp_uint64 time; /*  out : micro second */
	} get_RET_TIME;

	struct {
		lsp_uint64 time; /*  out : micro second */
	} get_max_conn_time;

	struct {
		lsp_uint64 time; /*  out : minute */
	} get_max_standby_timer;
} lsp_vendor_param, *lsp_vendor_param_ptr;

/*  C_ASSERT_SIZEOF(lsp_vendor, 8); */

/*  send to NDAS */
typedef union _lsp_vendor_v1 {
	lsp_uint64 param_64;

	/*  under only super user permission */
	struct {
		lsp_uint64 time; /*  micro second (value +1) */
	} set_RET_TIME;

	struct {
		lsp_uint64 time; /*  micro second */
	} set_max_conn_time;

	struct {
		lsp_uint64 password; /*  new password */
	} set_supervisor_pw;

	struct {
		lsp_uint64 password; /*  new password */
	} set_user_pw;

	struct {
		lsp_uint64 header :1; /*  1 : encrypt, 0 : not encrypt */
		lsp_uint64 data   :1; /*  1 : encrypt, 0 : not encrypt */
		lsp_uint64 obs    : 62;
	} set_enc_opt;

	struct {
		lsp_uint64 time   : 31; /*  minute */
		lsp_uint64 enable : 1; /*  1 : enable, 0 : disable */
		lsp_uint64 obs    : 32;
	} set_standby_timer;

	struct {
		lsp_uint64 no_param; /*  invalid */
	} reset;

	/*  under only normal permission */
	struct {
		lsp_uint64 sema_counter : 32;
		lsp_uint64 sema_index   : 2;
		lsp_uint64 obs          : 30;
	} set_sema;

	struct {
		lsp_uint64 sema_counter : 32;
		lsp_uint64 sema_index   : 2;
		lsp_uint64 obs          : 30;
	} free_sema;

	struct {
		lsp_uint64 sema_counter : 32;
		lsp_uint64 sema_index   : 2;
		lsp_uint64 obs          : 30;
	} get_sema;

	/*  in/out get_owner_sema parameter overlaps each other */
	/*  so, we neeed each struct for request/return */
	struct {
		lsp_uint64 obs1         : 32;
		lsp_uint64 sema_index   : 2;
		lsp_uint64 obs2         : 30;
	} get_owner_sema_request;

	struct {
		lsp_uint64 mac_address  : 48;
		lsp_uint64 obs          : 16;
	} get_owner_sema_return;

	struct {
		lsp_uint64 time; /*  micro second */
	} set_dynamic_RET_TIME;

	struct {
		lsp_uint64 time; /*  micro second */
	} set_dynamic_max_conn_time;

	/*  under all user permission */

	struct {
		lsp_uint64 time; /*  micro second */
	} get_RET_TIME;

	struct {
		lsp_uint64 time; /*  micro second */
	} get_max_conn_time;

	struct {
		lsp_uint64 time; /*  minute */
	} get_max_standby_timer;

} lsp_vendor_v1, *lsp_vendor_v1_ptr;

C_ASSERT_SIZEOF(lsp_vendor_v1, 8);

typedef struct _lsp_pdu_op_data {
	union {
		lsp_uint8 op_data[16];
		struct {
			lsp_uint32 reserved2;
			lsp_uint32 reserved3;
			lsp_uint32 reserved4;
			lsp_uint32 reserved5;
		} reserved; /* comply with older library */

		struct {
			lsp_uint8  parm_type;
			lsp_uint8  parm_ver;
			lsp_uint8  ver_max;
			lsp_uint8  ver_min;
			lsp_uint8  _reserved_[12];
		} login;  /*  request */

		struct {
			lsp_uint8  parm_type;
			lsp_uint8  parm_ver;
			lsp_uint8  ver_max;
			lsp_uint8  ver_active;
			lsp_uint16 revision;
			lsp_uint8  _reserved_[10];
		} login_response; /*  response */

		struct {
			lsp_uint8  _reserved_[16];
		} logout; /*  request & response */

		struct {
			lsp_uint8  parm_type;
			lsp_uint8  parm_ver;
			lsp_uint8  _reserved_[14];
		} text_command; /*  request & response */

		struct {
			lsp_ide_header    header;
			lsp_ide_register  register_data;
		} ide_command; /*  request & response */

		lsp_ide_data_v0 ide_command_v0;

		struct {
			lsp_uint16 vendor_id; /* 0x0001 as XIMETA */
			lsp_uint8 vop_ver;
			lsp_uint8 vop_code;
			lsp_uint8 vop_parm[12];
		} vendor_command; /*  request & response */
	};
} lsp_pdu_op_data;

C_ASSERT_SIZEOF(lsp_pdu_op_data, 16);

/* LANSCSI PDU */
/*

basic header segment ( 15 * 4 = 60 bytes) -> lsp_pdu_hdr
[ packet_command (v 1.1 or later and IDE command only) ]
additional header segment (optional)
header digest (optional)
data segment (optional)
data digest (optional)

*/

typedef struct _lsp_pdu_hdr {

	lsp_uint8  op_code;
	lsp_pdu_op_flags op_flags;   /* lsp_pdu_op_flags */
	lsp_uint8  response;         /* remote to host only */
	lsp_uint8  status;           /* remote to host and IDE only */

	lsp_uint32 hpid;             /* host path id */
	lsp_uint16 rpid;             /* remote path id */
	lsp_uint16 cpslot;           /* command processing slot no */
	lsp_uint32 dataseg_len;      /* data segment length */
	lsp_uint16 ahs_len;          /* additional header segment length */
	lsp_uint16 cmd_subpkt_seq;   /* command sub-packet sequence no */

	lsp_uint32 path_cmd_tag;     /* path command tag */
	lsp_uint32 init_task_tag;    /* initiator task tag */

	lsp_uint32 data_trans_len;   /* data transfer len */

	lsp_uint32 target_id;        /* target id */
	lsp_uint32 lun0;              /* lun */
	lsp_uint32 lun1;              /* lun */

	lsp_pdu_op_data op_data;     /* lsp_pdu_op_data */

} lsp_pdu_hdr;

C_ASSERT_SIZEOF(lsp_pdu_hdr, 60);

/* lsp packet command */

#define LSP_PACKET_COMMAND_SIZE	12

typedef struct _lsp_packet_command {
	lsp_uint8  command[LSP_PACKET_COMMAND_SIZE];
} lsp_packet_command, *lsp_packet_command_ptr;

typedef struct _lsp_pdu_pointers {
	lsp_pdu_hdr *header_ptr;
	lsp_uint8  *ahs_ptr;
	lsp_uint8  *header_dig_ptr;
	lsp_uint8  *data_seg_ptr;
	lsp_uint8  *data_dig_ptr;
} lsp_pdu_pointers;

typedef struct _lsp_io_data_buffer {
	lsp_uint8 *recv_buffer;
	lsp_uint32 recv_size;
	lsp_uint8 *send_buffer;
	lsp_uint32 send_size;
} lsp_io_data_buffer, *lsp_io_data_buffer_ptr;

// used for packet command. V 1.1 or later.
typedef struct _lsp_extended_command {
	lsp_uint8 *cmd_buffer;
	lsp_uint32 cmd_size; // ATM, 12 bytes of command only
} lsp_extended_command, *lsp_extended_command_ptr;

#include <poppack.h>

#endif /* _LSP_SPEC_H_ */
