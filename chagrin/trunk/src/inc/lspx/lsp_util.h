#ifndef _LSP_UTIL_H_INCLUDED_
#define _LSP_UTIL_H_INCLUDED_

#include "lsp.h"

/* WINDOWS PLATFORM SPECIFIC MACROS */

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef __w64 unsigned int  size_t;
#endif  /* !_WIN64 */
#define _SIZE_T_DEFINED
#endif  /* !_SIZE_T_DEFINED */

/* OTHER PLATFORM SPECIFIC MACROS */

#ifdef __cplusplus
extern "C" {
#endif

/* ide command helpers */
lsp_status_t 
lsp_call 
lsp_ide_write(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_uint8 use_dma, 
	lsp_uint8 use_48, 
	lsp_large_integer_t* location, 
	lsp_uint16 sectors, 
	void* mutable_data, 
	lsp_uint32 len);

lsp_status_t 
lsp_call 
lsp_ide_read(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_uint8 use_dma, 
	lsp_uint8 use_48, 
	lsp_large_integer_t* location, 
	lsp_uint16 sectors, 
	void* mutable_data, 
	lsp_uint32 len);

lsp_status_t 
lsp_call 
lsp_ide_verify(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_uint8 use_48, 
	lsp_large_integer_t* location, 
	lsp_uint16 sectors);

lsp_status_t 
lsp_call
lsp_ide_identify(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	struct hd_driveid *info);

lsp_status_t 
lsp_call
lsp_ide_pidentify(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	struct hd_driveid *info);

lsp_status_t 
lsp_call
lsp_ide_setfeatures(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_uint8 subcommand_code,
	lsp_uint8 subcommand_specific_0 /* sector count register */, 
	lsp_uint8 subcommand_specific_1 /* lba low register */, 
	lsp_uint8 subcommand_specific_2 /* lba mid register */, 
	lsp_uint8 subcommand_specific_3 /* lba high register */);

/* utility functions*/
lsp_status_t
lsp_call
lsp_ide_handshake(
	lsp_handle_t h, 
	lsp_uint32 target_id, 
	lsp_uint32 lun0, 
	lsp_uint32 lun1, 
	lsp_uint8 *use_dma, 
	lsp_uint8 *use_48, 
	lsp_uint8 *use_lba, 
	lsp_large_integer_t* capacity);

/* information functions */

#define LSP_SCF_48BIT_LBA 0x00000001
#define LSP_SCF_LBA   0x00000002

lsp_uint32
lsp_verify_hddrive_capacity(
	struct hd_driveid* info,
	lsp_large_integer_t* capacity);

#define LSP_INLINE __forceinline

lsp_status_t
lsp_call
lsp_build_ide_write(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in lsp_uint8 use_dma, 
	__in lsp_uint8 use_48, 
	__in lsp_large_integer_t* location, 
	__in lsp_uint16 sectors, 
	__in void* mutable_data, 
	__in lsp_uint32 len);

lsp_status_t
lsp_call
lsp_build_ide_read(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in lsp_uint8 use_dma, 
	__in lsp_uint8 use_48, 
	__in lsp_large_integer_t* location, 
	__in lsp_uint16 sectors, 
	__in void* mutable_data, 
	__in lsp_uint32 len);

lsp_status_t
lsp_call
lsp_build_ide_verify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in lsp_uint8 use_48, 
	__in lsp_large_integer_t* location, 
	__in lsp_uint16 sectors);

lsp_status_t
lsp_call
lsp_build_ide_identify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in struct hd_driveid *info);

lsp_status_t
lsp_call
lsp_build_ide_pidentify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in struct hd_driveid *info);

lsp_status_t
lsp_call
lsp_build_ide_setfeatures(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in lsp_uint8 subcommand_code,
	__in lsp_uint8 subcommand_specific_0 /* sector count register */, 
	__in lsp_uint8 subcommand_specific_1 /* lba low register */, 
	__in lsp_uint8 subcommand_specific_2 /* lba mid register */, 
	__in lsp_uint8 subcommand_specific_3 /* lba high register */);

void
lsp_call
lsp_build_acquire_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8 lock_number);

void
lsp_call
lsp_build_release_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8 lock_number);

#ifdef __cplusplus
}
#endif

#endif /* _LSP_UTIL_H_INCLUDED_ */
