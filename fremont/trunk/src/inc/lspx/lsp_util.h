/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

#ifndef _LSP_UTIL_H_INCLUDED_
#define _LSP_UTIL_H_INCLUDED_

#include "lsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ide command helpers */

/*
 * ATA handshake configures the ATA/ATAPI required by the
 * version. Drive capacity is calculated. Transfer mode is configured
 * properly as required by the hardware restrictions. 
 *
 * ATA handshake must be called prior to requesting any ATA/ATAPI
 * commands. IDE commands returns LSP_STATUS_REQUIRES_HANDSHAKE if
 * pre-handshaking is not performed or has failed.
 * 
 */
lsp_status_t
lsp_call
lsp_ata_handshake(
	__in lsp_handle_t h);

/*
 * After handshake succeeded, handshake data is available. Returned
 * pointer is valid as long as the handle is valid.
 */

const lsp_ata_handshake_data_t*
lsp_call
lsp_get_ata_handshake_data(
	__in lsp_handle_t h);

/*
 * After handshake succeeded, identify device data is available. Returned
 * pointer is valid as long as the handle is valid.
 */

const lsp_ide_identify_device_data_t*
lsp_call
lsp_get_ide_identify_device_data(
	__in lsp_handle_t h);

lsp_status_t 
lsp_call 
lsp_ide_write(
	__in lsp_handle_t h, 
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors, 
	__inout_bcount(len) void* mutable_data, 
	__in lsp_uint32_t len);

lsp_status_t 
lsp_call 
lsp_ide_read(
	__in lsp_handle_t h, 
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors, 
	__out_bcount(len) void* buf, 
	__in lsp_uint32_t len);

lsp_status_t 
lsp_call 
lsp_ide_verify(
	__in lsp_handle_t h, 
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors,
	__reserved void* reserved_1,
	__reserved lsp_uint32_t reserved_2);

lsp_status_t 
lsp_call
lsp_ide_identify(
	__in lsp_handle_t h, 
	__out_bcount(sizeof(lsp_ide_identify_device_data_t))
		lsp_ide_identify_device_data_t* buf);

lsp_status_t 
lsp_call
lsp_ide_identify_packet_device(
	__in lsp_handle_t h, 
	__out lsp_ide_identify_packet_device_data_t* buf);

lsp_status_t
lsp_call
lsp_ide_packet_cmd(
	__in lsp_handle_t h, 
	__in void* cdb,
	__inout void* mutable_data,
	__in lsp_uint32_t len,
	__in lsp_uint8_t data_to_target);
	
lsp_status_t 
lsp_call
lsp_ide_setfeatures(
	__in lsp_handle_t h, 
	__in lsp_uint8_t subcommand_code,
	__in lsp_uint8_t subcommand_specific_0 /* sector count register */, 
	__in lsp_uint8_t subcommand_specific_1 /* lba low register */, 
	__in lsp_uint8_t subcommand_specific_2 /* lba mid register */, 
	__in lsp_uint8_t subcommand_specific_3 /* lba high register */);


lsp_status_t
lsp_call
lsp_acquire_lock(
    __in lsp_handle_t h, 
    __in lsp_uint8_t lock_number);

lsp_status_t
lsp_call
lsp_release_lock(
    __in lsp_handle_t h, 
    __in lsp_uint8_t lock_number);

lsp_status_t
lsp_call
lsp_reset(
		  __in lsp_handle_t h
		  );
	
lsp_status_t
lsp_call
lsp_set_user_password(
					  __in lsp_handle_t h,
					  __in lsp_uint8_t *password);
	


/* information functions */

#define LSP_SCF_48BIT_LBA 0x00000001
#define LSP_SCF_LBA   0x00000002

void
lsp_call
lsp_build_login(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in const lsp_login_info_t* login_info);

void
lsp_call
lsp_build_logout(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h);

void
lsp_call
lsp_build_ata_handshake(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h);

lsp_status_t
lsp_call
lsp_build_ide_write(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors, 
	__inout_bcount(len) void* mutable_data, 
	__in lsp_uint32_t len);

lsp_status_t
lsp_call
lsp_build_ide_write_fua(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors, 
	__inout_bcount(len) void* mutable_data, 
	__in lsp_uint32_t len);

lsp_status_t
lsp_call
lsp_build_ide_read(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors, 
	__out_bcount(len) void* buffer, 
	__in lsp_uint32_t len);

lsp_status_t 
lsp_call 
lsp_build_ide_verify(
	 __inout lsp_request_packet_t* request,
	 __in lsp_handle_t h,
	__in const lsp_large_integer_t* location, 
	__in lsp_uint16_t sectors,
	__reserved void* reserved_1,
	__reserved lsp_uint32_t reserved_2);

lsp_status_t
lsp_call
lsp_build_ide_identify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_ide_identify_device_data_t* ident);

void
lsp_call
lsp_build_ide_identify_packet_device(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_ide_identify_packet_device_data_t* ident);

lsp_status_t
lsp_call
lsp_build_ide_set_features(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8_t subcommand_code,
	__in lsp_uint8_t sector_count/* sector count register */, 
	__in lsp_uint8_t lba_low/* lba low register */, 
	__in lsp_uint8_t lba_mid/* lba mid register */, 
	__in lsp_uint8_t lba_high/* lba high register */);

lsp_status_t
lsp_call
lsp_build_ide_packet_cmd(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in void* cdb,
	__inout void* buf, 
	__in lsp_uint32_t buflen,
	__in lsp_uint8_t buf_to_target);
	
void
lsp_call
lsp_build_acquire_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8_t lock_number);

void
lsp_call
lsp_build_release_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8_t lock_number);

void
lsp_call
lsp_build_ata_check_power_mode(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h);

void
lsp_call
lsp_build_text_target_list(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h);

void
lsp_call
lsp_build_text_target_data(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h);

lsp_status_t
lsp_call
lsp_build_reset(
				__inout lsp_request_packet_t* request,
				__in lsp_handle_t h);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_UTIL_H_INCLUDED_ */
