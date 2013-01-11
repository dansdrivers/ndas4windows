#include "lsp_impl.h"
#include "lsp_type_internal.h"
#include "lsp_hash.h"
#include "lsp_binparm.h"
#include <lsp_util.h>
#include <lsp.h>

#define lsp_debug __noop

enum { LSP_DISK_SECTOR_SIZE = 512 };

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
	lsp_uint32 len)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session = &context->session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if (!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <= location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (!mutable_data)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (session->max_transfer_blocks < sectors ||
	   sectors * LSP_DISK_SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_WRITE :
		(use_dma) ?
		((use_48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA) :
		((use_48) ? WIN_WRITE_EXT : WIN_WRITE);

	if (use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	/* set data buffer */
	data_buf.send_buffer = (lsp_uint8*) mutable_data;
	data_buf.send_size = len;

	//lsp_set_chained_return_proc(h, lsp_write_verify_chain);

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, 0);
}

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
	lsp_uint32 len)
{
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session = &context->session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <=location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(!mutable_data)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(session->max_transfer_blocks < sectors ||
	   sectors * LSP_DISK_SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_READ:
		(use_dma) ?
		((use_48) ? WIN_READDMA_EXT : WIN_READDMA) :
		((use_48) ? WIN_READ_EXT : WIN_READ);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	/* set data buffer */
	data_buf.recv_buffer = (lsp_uint8*) mutable_data;
	data_buf.recv_size = len;

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, 0);
}

lsp_status_t
lsp_call
lsp_ide_verify(
	lsp_handle_t h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	lsp_uint8 use_48,
	lsp_large_integer_t* location,
	lsp_uint16 sectors)
{
	lsp_status_t err;
	lsp_handle_context_t *context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session = &context->session;

	lsp_ide_register_param_t p;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <= location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(session->max_transfer_blocks < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_VERIFY :
		((use_48) ? WIN_VERIFY_EXT : WIN_VERIFY);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	return lsp_ide_command(h, target_id, lun0, lun1, &p, 0, 0);
}

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
	lsp_uint8 subcommand_specific_3 /* lba high register */)
{
	lsp_status_t err;
	lsp_handle_context_t* context = (lsp_handle_context_t*) h;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_SETFEATURES;

	p.reg.basic.reg[0] = subcommand_code;
	p.reg.basic.reg[1] = subcommand_specific_0;
	p.reg.basic.reg[2] = subcommand_specific_1;
	p.reg.basic.reg[3] = subcommand_specific_2;
	p.reg.basic.reg[4] = subcommand_specific_3;

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, 0);
}

lsp_status_t
lsp_call
lsp_ide_identify(
	lsp_handle_t h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	struct hd_driveid *info)
{
	lsp_handle_context_t *context = (lsp_handle_context_t*) h;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_IDENTIFY;

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, 0);
}

lsp_status_t
lsp_call
lsp_ide_pidentify(
	lsp_handle_t h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	struct hd_driveid *info)
{
	lsp_handle_context_t *context = (lsp_handle_context_t*) h;
	lsp_session_data_t* session = &context->session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if (!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if (LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver)
	{
		return LSP_ERR_NOT_SUPPORTED;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_PIDENTIFY;

	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, 0);
}

/*
  returns
  1 : if capacity is ok
  0 : otherwise
*/
static
lsp_uint32
_is_lba_capacity_ok(
	struct hd_driveid *info)
{
	lsp_uint32 lba_sects, chs_sects, head, tail;

	/* If the drive is capable of handling 48 Bit LBA Address 0400h 
	   and it is enabled, we assume that there is no restrictions
	   on the capacity */

	if((info->command_set_2 & 0x0400) && 
		(info->cfs_enable_2 & 0x0400)) 
	{
		return 1;
	}

	/*
	  The ATA spec tells large drivers to return
	  C/H/S = 16383/16/63 independent of their size.
	  Some drives can be jumpered to use 15 heads instead of 16.
	  Some drives can be jumpered to use 4092 cyls instead of 16383
	*/

	if((info->cyls == 16383 || (info->cyls == 4092 && info->cur_cyls== 16383)) &&
	   info->sectors == 63 && 
	   (info->heads == 15 || info->heads == 16) &&
	   info->lba_capacity >= (unsigned)(16383 * 63 * info->heads))
	{
		return 1;
	}

	lba_sects = info->lba_capacity;
	chs_sects = info->cyls * info->heads * info->sectors;

	/* Perform a rough sanity check on lba_sects: within 10% is OK */
	if((lba_sects - chs_sects) < chs_sects / 10) 
	{
		return 1;
	}

	/* Some drives have the word order reversed */
	head = ((lba_sects >> 16) & 0xffff);
	tail = (lba_sects & 0xffff);
	lba_sects = (head | (tail << 16));
	if((lba_sects - chs_sects) < chs_sects / 10) 
	{
		info->lba_capacity = lba_sects;
		/* Capacity reversed.... */
		return 1;
	}

	return 0;
}

/* get lock count after lsp_acquire_lock or lsp_release_lock */
//lsp_status_t
//lsp_call
//lsp_get_lock_count(lsp_handle_t h, lsp_uint32 lock_count)
//{
//
//}
//

lsp_status_t
lsp_call
lsp_acquire_lock(lsp_handle_t h, lsp_uint8 lock_number)
{
	lsp_request_packet_t* request;
	lsp_vendor_command_request_t* vc_request;

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_SET_SEMA;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	vc_request->param[0] = lock_number;

	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_release_lock(lsp_handle_t h, lsp_uint8 lock_number)
{
	lsp_request_packet_t* request;
	lsp_vendor_command_request_t* vc_request;

	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_FREE_SEMA;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	vc_request->param[0] = lock_number;

	return lsp_request(h, request);
}

#if 0
lsp_status_t
lsp_call
lsp_cleanup_locks(lsp_handle_t h)
{
	lsp_status_t lsp_status;
	
	lsp_status = lsp_acquire_lock(h, 0);
	// if (lsp_status)
}
#endif

//lsp_status_t
//lsp_call
//lsp_ide_handshake_chain_1(lsp_handle_t h, lsp_status_t status)
//{
//
//}

/*
if(*packet_device)
{
// 0x00: // Direct-access device
// 0x01: // Sequential-access device
// 0x02: // Printer device
// 0x03: // Processor device
// 0x04: // Write-once device
// 0x05: // CD-ROM device
// 0x06: // Scanner device
// 0x07: // Optical memory device
// 0x08: // Medium changer device
// 0x09: // Communications device
// 0x0A: 0x0B: // Reserved for ACS IT8 (Graphic arts pre-press devices)
// 0x0C: // Array controller device
// 0x0D: // Enclosure services device
// 0x0E: // Reduced block command devices
// 0x0F: // Optical card reader/writer device
// 0x1F: // Unknown or no device type

*packet_device_type = (info.config >> 8) & 0x1f; // Bits(12:8)
// nothing to do anymore
return LSP_ERR_SUCCESS;
}
*/

/*++

Parameters:

lsp_verify_hddrive_capacity

Return values:

Flags set none or more of LSP_SCF_48BIT_LBA or LSP_SCF_LBA

--*/
lsp_uint32
lsp_verify_hddrive_capacity(
	struct hd_driveid* info,
	lsp_large_integer_t* capacity)
{
	lsp_uint32 flags = 0;
	/* determine lba mode according to disk capacity. */
	if((info->command_set_2 & 0x0400) /* 48-bit address feature set */ &&
		(info->cfs_enable_2 & 0x0400) /* 48-bit address feature set */)
	{
		/* support LBA 48 mode */
		flags |= (LSP_SCF_48BIT_LBA | LSP_SCF_LBA);
		capacity->u.low = (lsp_uint32)(info->lba_capacity_2 & 0x00000000FFFFFFFF);
		capacity->u.high = (lsp_uint32)(info->lba_capacity_2 >> 32);
	}
	else if((info->capability & 0x02) /* lba */ && _is_lba_capacity_ok(info))
	{
		/* support LBA 28 mode */
		flags |= LSP_SCF_LBA;
		capacity->u.low = info->lba_capacity;
		capacity->u.high = 0;
	}
	else
	{
		/* CHS mode */
		lsp_uint64 capacity64;
		capacity64 = info->cyls;
		capacity64 *= info->heads;
		capacity64 *= info->sectors;
		capacity->u.low = (lsp_uint32)(capacity64 & 0x00000000FFFFFFFF);
		capacity->u.high = (lsp_uint32)(capacity64 >> 32);

		/* we do not support CHS mode */
	}
	return flags;
}

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
	__in lsp_uint32 len)
{
	lsp_session_data_t* session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_lba = 1;

	if(!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	session = &h->session;

	if (!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <= location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (!mutable_data)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (session->max_transfer_blocks < sectors ||
	   sectors * LSP_DISK_SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_WRITE :
		(use_dma) ?
		((use_48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA) :
		((use_48) ? WIN_WRITE_EXT : WIN_WRITE);

	if (use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	/* set data buffer */
	data_buf.send_buffer = (lsp_uint8*) mutable_data;
	data_buf.send_size = len;

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		&data_buf,
		0);

	return LSP_STATUS_SUCCESS;
}

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
	__in lsp_uint32 len)
{
	lsp_session_data_t* session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_lba = 1;

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	session = &h->session;

	if (!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <=location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (!mutable_data)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (session->max_transfer_blocks < sectors ||
	   sectors * LSP_DISK_SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_READ:
		(use_dma) ?
		((use_48) ? WIN_READDMA_EXT : WIN_READDMA) :
		((use_48) ? WIN_READ_EXT : WIN_READ);

	if (use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	/* set data buffer */
	data_buf.recv_buffer = (lsp_uint8*) mutable_data;
	data_buf.recv_size = len;

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		&data_buf,
		0);

	return LSP_STATUS_SUCCESS;
}

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
	__in lsp_uint16 sectors)
{
	lsp_status_t err;
	lsp_session_data_t* session;

	lsp_ide_register_param_t p;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_lba = 1;

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	session = &h->session;

	if (!location ||
	   (use_48 && 0x00010000 <= location->u.high) ||
	   (!use_48 && (0x00000001 <= location->u.high || 0x10000000 <= location->u.low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if (session->max_transfer_blocks < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));

	/* set registers */
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver) ? WIN_VERIFY :
		((use_48) ? WIN_VERIFY_EXT : WIN_VERIFY);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->u.low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->u.high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->u.low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->u.low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->u.low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->u.low & 0x0F000000) >> 24);
	}

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		0,
		0);

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_build_ide_identify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in struct hd_driveid *info)
{
	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_IDENTIFY;

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		&data_buf,
		0);

	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_build_ide_pidentify(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint32 target_id, 
	__in lsp_uint32 lun0, 
	__in lsp_uint32 lun1, 
	__in struct hd_driveid *info)
{
	lsp_session_data_t* session;

	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	session = &h->session;

	if (LSP_IDE_PROTOCOL_VERSION_1_0 == session->hw_prot_ver)
	{
		return LSP_ERR_NOT_SUPPORTED;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_PIDENTIFY;

	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		&data_buf,
		0);

	return LSP_STATUS_SUCCESS;
}

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
	__in lsp_uint8 subcommand_specific_3 /* lba high register */)
{
	lsp_ide_register_param_t p;
	lsp_io_data_buffer_t data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if (!h)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_SETFEATURES;

	p.reg.basic.reg[0] = subcommand_code;
	p.reg.basic.reg[1] = subcommand_specific_0;
	p.reg.basic.reg[2] = subcommand_specific_1;
	p.reg.basic.reg[3] = subcommand_specific_2;
	p.reg.basic.reg[4] = subcommand_specific_3;

	lsp_build_ide_command(
		request,
		h,
		target_id,
		lun0,
		lun1,
		&p,
		&data_buf,
		0);

	return LSP_STATUS_SUCCESS;
}

void
lsp_call
lsp_build_acquire_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8 lock_number)
{
	lsp_vendor_command_request_t* vc_request;
	h;
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_SET_SEMA;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	vc_request->param[0] = lock_number;
}

void
lsp_call
lsp_build_release_lock(
	__inout lsp_request_packet_t* request,
	__in lsp_handle_t h,
	__in lsp_uint8 lock_number)
{
	lsp_vendor_command_request_t* vc_request;
	h;
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_FREE_SEMA;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	vc_request->param[0] = lock_number;
}
