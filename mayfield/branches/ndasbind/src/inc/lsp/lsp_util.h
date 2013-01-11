#ifndef _LSP_UTIL_H_INCLUDED_
#define _LSP_UTIL_H_INCLUDED_

#include <lsp/lsp.h>

/* WINDOWS PLATFORM SPECIFIC MACROS */

#ifndef _SIZE_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    size_t;
#else
typedef __w64 unsigned int  size_t;
#endif  /* !_WIN64 */
#define _SIZE_T_DEFINED
#endif  /* !_SIZE_T_DEFINED */

#define LSP_MAX_REQUEST_SIZE	1500

/* OTHER PLATFORM SPECIFIC MACROS */

#ifdef __cplusplus
extern "C" {
#endif

/* ide command helpers */
lsp_error_t lsp_call lsp_ide_write(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 use_dma, lsp_uint8 use_48, lsp_uint64_ll_ptr location, lsp_uint16 sectors, void* mutable_data, size_t len);
lsp_error_t lsp_call lsp_ide_read(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 use_dma, lsp_uint8 use_48, lsp_uint64_ll_ptr location, lsp_uint16 sectors, void* mutable_data, size_t len);
lsp_error_t lsp_call lsp_ide_verify(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 use_48, lsp_uint64_ll_ptr location, lsp_uint16 sectors);

lsp_error_t lsp_call lsp_ide_identify(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 *packet, struct hd_driveid *info);
lsp_error_t lsp_call lsp_ide_setfeatures(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 subcommand_code, lsp_uint8 subcommand_specific_0 /* sector count register */, lsp_uint8 subcommand_specific_1 /* lba low register */, lsp_uint8 subcommand_specific_2 /* lba mid register */, lsp_uint8 subcommand_specific_3 /* lba high register */);

/* utility functions*/
lsp_error_t
lsp_call
lsp_ide_handshake(lsp_handle h, lsp_uint32 target_id, lsp_uint32 lun0, lsp_uint32 lun1, lsp_uint8 *use_dma, lsp_uint8 *use_48, lsp_uint8 *use_lba, lsp_uint64_ll_ptr capacity, lsp_uint8 *packet_device, lsp_uint8 *packet_device_type);

/* information functions */


#ifdef __cplusplus
}
#endif

#endif /* _LSP_UTIL_H_INCLUDED_ */
