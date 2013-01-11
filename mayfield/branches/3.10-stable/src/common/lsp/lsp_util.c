#include "lsp.h"
#include "lsp_util.h"
#include "lsp_spec.h"
#include "lsp_impl.h"
#include "lsp_type_internal.h"
#include "lsp_hash.h"

#define lsp_debug __noop

static const SECTOR_SIZE = 512;

lsp_error_t
lsp_call
lsp_ide_write(
			  lsp_handle h,
			  lsp_uint32 target_id,
			  lsp_uint32 lun0,
			  lsp_uint32 lun1,
			  lsp_uint8 use_dma,
			  lsp_uint8 use_48,
			  lsp_uint64_ll_ptr location,
			  lsp_uint16 sectors,
			  void* mutable_data,
			  size_t len)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_ide_data_buffer data_buf;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(!location ||
		(use_48 && 0x00010000 <= location->high) ||
		(!use_48 && (0x00000001 <= location->high || 0x10000000 <=location->low)))
		return LSP_ERR_INVALID_PARAMETER;

	if(0 == sectors || 128 < sectors)
		return LSP_ERR_INVALID_PARAMETER;

	if(!mutable_data)
		return LSP_ERR_INVALID_PARAMETER;

	if(session->iMaxBlocks < sectors ||
		sectors * SECTOR_SIZE != len)
		return LSP_ERR_INVALID_PARAMETER;

	memset(&p, 0, sizeof(lsp_ide_register_param));
	memset(&data_buf, 0, sizeof(lsp_ide_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_48;

	p.command.command =
		(LSP_PROTO_VERSION_1_0 == session->HWProtoVersion) ? WIN_WRITE :
			(use_dma) ?
				((use_48) ? WIN_WRITEDMA_EXT : WIN_WRITEDMA) :
				((use_48) ? WIN_WRITE_EXT : WIN_WRITE);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->low & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->low & 0xFF000000) >> 24);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->low & 0x0F000000) >> 24);
	}

	// set data buffer
	data_buf.send_buffer = mutable_data;
	data_buf.send_size = len;

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf);
}

lsp_error_t
lsp_call
lsp_ide_read(
			  lsp_handle h,
			  lsp_uint32 target_id,
			  lsp_uint32 lun0,
			  lsp_uint32 lun1,
			  lsp_uint8 use_dma,
			  lsp_uint8 use_48,
			  lsp_uint64_ll_ptr location,
			  lsp_uint16 sectors,
			  void* mutable_data,
			  size_t len)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_ide_data_buffer data_buf;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	if(!location ||
		(use_48 && 0x00010000 <= location->high) ||
		(!use_48 && (0x00000001 <= location->high || 0x10000000 <=location->low)))
		return LSP_ERR_INVALID_PARAMETER;

	if(0 == sectors || 128 < sectors)
		return LSP_ERR_INVALID_PARAMETER;

	if(!mutable_data)
		return LSP_ERR_INVALID_PARAMETER;

	if(session->iMaxBlocks < sectors ||
		sectors * SECTOR_SIZE != len)
		return LSP_ERR_INVALID_PARAMETER;

	memset(&p, 0, sizeof(lsp_ide_register_param));
	memset(&data_buf, 0, sizeof(lsp_ide_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_48;

	p.command.command =
		(LSP_PROTO_VERSION_1_0 == session->HWProtoVersion) ? WIN_READ:
			(use_dma) ?
				((use_48) ? WIN_READDMA_EXT : WIN_READDMA) :
				((use_48) ? WIN_READ_EXT : WIN_READ);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->low & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->low & 0xFF000000) >> 24);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->high & 0x000000FF) >> 0);
		p.reg.named_48.prev.lba_high = (lsp_uint8)((location->high & 0x0000FF00) >> 8);
	}
	else
	{
		p.reg.named.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named.lba_low = (lsp_uint8)((location->low & 0x000000FF) >> 0);
		p.reg.named.lba_mid = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named.lba_high = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.device.lba_head_nr = (lsp_uint8)((location->low & 0x0F000000) >> 24);
	}

	// set data buffer
	data_buf.recv_buffer = mutable_data;
	data_buf.recv_size = len;

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf);
}

lsp_error_t
lsp_call
lsp_ide_verify(
			   lsp_handle h,
			   lsp_uint32 target_id,
			   lsp_uint32 lun0,
			   lsp_uint32 lun1,
			   lsp_int64 location,
			   lsp_int16 sectors);

lsp_error_t 
lsp_call 
lsp_ide_setfeatures(
					lsp_handle h,
					lsp_uint32 target_id,
					lsp_uint32 lun0,
					lsp_uint32 lun1,
					lsp_uint8 subcommand_code,
					lsp_uint8 subcommand_specific_0 /* sector count register */,
					lsp_uint8 subcommand_specific_1 /* lba low register */,
					lsp_uint8 subcommand_specific_2 /* lba mid register */,
					lsp_uint8 subcommand_specific_3 /* lba high register */)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_ide_data_buffer data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	memset(&p, 0, sizeof(lsp_ide_register_param));
	memset(&data_buf, 0, sizeof(lsp_ide_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_48;

	p.command.command = WIN_SETFEATURES;

	p.reg.basic.reg[0] = subcommand_code;
	p.reg.basic.reg[1] = subcommand_specific_0;
	p.reg.basic.reg[2] = subcommand_specific_1;
	p.reg.basic.reg[3] = subcommand_specific_2;
	p.reg.basic.reg[4] = subcommand_specific_3;

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf);
}

lsp_error_t
lsp_call
lsp_ide_identify(
				 lsp_handle h,
				 lsp_uint32 target_id,
				 lsp_uint32 lun0,
				 lsp_uint32 lun1,
				 struct hd_driveid *info)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_ide_data_buffer data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;

	if(!context)
		return LSP_ERR_INVALID_HANDLE;

	memset(&p, 0, sizeof(lsp_ide_register_param));
	memset(&data_buf, 0, sizeof(lsp_ide_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_48;

	p.command.command = WIN_IDENTIFY;

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf);
}
