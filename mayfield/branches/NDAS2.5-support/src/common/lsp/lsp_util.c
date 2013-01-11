#include "lsp_impl.h"
#include "lsp_type_internal.h"
#include "lsp_hash.h"
#include "lsp_binparm.h"
#include <lsp_util.h>
#include <lsp.h>

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
	lsp_io_data_buffer data_buf;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!location ||
	   (use_48 && 0x00010000 <= location->high) ||
	   (!use_48 && (0x00000001 <= location->high || 0x10000000 <=location->low)))
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

	if(session->iMaxBlocks < sectors ||
	   sectors * SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

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
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->high & 0x000000FF) >> 0);
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

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
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
	lsp_io_data_buffer data_buf;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!location ||
	   (use_48 && 0x00010000 <= location->high) ||
	   (!use_48 && (0x00000001 <= location->high || 0x10000000 <=location->low)))
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

	if(session->iMaxBlocks < sectors ||
	   sectors * SECTOR_SIZE != len)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

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
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->high & 0x000000FF) >> 0);
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

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
}

lsp_error_t
lsp_call
lsp_ide_verify(
	lsp_handle h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	lsp_uint8 use_48,
	lsp_uint64_ll_ptr location,
	lsp_uint16 sectors)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_lba = 1;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!location ||
	   (use_48 && 0x00010000 <= location->high) ||
	   (!use_48 && (0x00000001 <= location->high || 0x10000000 <=location->low)))
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(0 == sectors || 128 < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	if(session->iMaxBlocks < sectors)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command =
		(LSP_PROTO_VERSION_1_0 == session->HWProtoVersion) ? WIN_VERIFY :
		((use_48) ? WIN_VERIFY_EXT : WIN_VERIFY);

	if(use_48)
	{
		p.reg.named_48.cur.sector_count = (lsp_uint8)((sectors & 0x00FF) >> 0);
		p.reg.named_48.prev.sector_count = (lsp_uint8)((sectors & 0xFF00) >> 8);
		p.reg.named_48.cur.lba_low = (lsp_uint8)((location->low & 0x000000FF) >> 0);
		p.reg.named_48.cur.lba_mid = (lsp_uint8)((location->low & 0x0000FF00) >> 8);
		p.reg.named_48.cur.lba_high = (lsp_uint8)((location->low & 0x00FF0000) >> 16);
		p.reg.named_48.prev.lba_low = (lsp_uint8)((location->low & 0xFF000000) >> 24);
		p.reg.named_48.prev.lba_mid = (lsp_uint8)((location->high & 0x000000FF) >> 0);
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

	return lsp_ide_command(h, target_id, lun0, lun1, &p, (void *)0, (void *)0);
}

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
	lsp_io_data_buffer data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer));

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

	return lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
}

lsp_error_t
lsp_call
lsp_ide_identify(
	lsp_handle h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	lsp_uint8 *packet_device,
	struct hd_driveid *info)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;

	lsp_ide_register_param p;
	lsp_io_data_buffer data_buf;
	lsp_uint8 use_dma = 0;
	lsp_uint8 use_48 = 0;
	lsp_uint8 use_lba = 0;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!packet_device)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	*packet_device = 0;

	lsp_memset(&p, 0, sizeof(lsp_ide_register_param));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer));

	// set registers
	p.use_dma = use_dma;
	p.use_48 = use_48;
	p.device.dev = (0 == target_id) ? 0 : 1;
	p.device.lba = use_lba;

	p.command.command = WIN_IDENTIFY;

	// set data buffer
	data_buf.recv_buffer = (lsp_uint8 *)info;
	data_buf.recv_size = sizeof(struct hd_driveid);

	err = lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
	
	if(LSP_ERR_SUCCESS != err && LSP_PROTO_VERSION_1_0 != session->HWProtoVersion)
	{
		// assume packet device
		*packet_device = 1;

		memset(&p, 0, sizeof(lsp_ide_register_param));
		memset(&data_buf, 0, sizeof(lsp_io_data_buffer));

		// set registers
		p.use_dma = use_dma;
		p.use_48 = use_48;
		p.device.dev = (0 == target_id) ? 0 : 1;
		p.device.lba = use_lba;

		p.command.command = WIN_PIDENTIFY;

		// set data buffer
		data_buf.recv_buffer = (lsp_uint8 *)info;
		data_buf.recv_size = sizeof(struct hd_driveid);

		err = lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
	}

	return err;
}

/*
  returns
  1 : if capacity is ok
  0 : else
*/
static
lsp_uint8
_is_lba_capacity_ok(
	struct hd_driveid *info)
{
	lsp_uint32	lba_sects, chs_sects, head, tail;

	if((info->command_set_2 & 0x0400) && (info->cfs_enable_2 & 0x0400)) 
	{
		// 48 Bit Drive.
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


lsp_error_t
lsp_call
lsp_ide_handshake(
	lsp_handle h,
	lsp_uint32 target_id,
	lsp_uint32 lun0,
	lsp_uint32 lun1,
	lsp_uint8 *use_dma,
	lsp_uint8 *use_48,
	lsp_uint8 *use_lba,
	lsp_uint64_ll_ptr capacity,
	lsp_uint8 *packet_device,
	lsp_uint8 *packet_device_type
	)
{
	lsp_error_t					err;
	lsp_handle_context			*context = (lsp_handle_context*) h;
	lsp_session_data*			session = &context->session;
	lsp_transport_proc*			trans   = &context->proc;
	
	struct hd_driveid info;
	lsp_uint8 set_dma_feature = 0;

	lsp_uint16 media_type;

	if(!context)
	{
		return LSP_ERR_INVALID_HANDLE;
	}

	if(!use_dma || !use_48 || !use_lba || !capacity)
	{
		return LSP_ERR_INVALID_PARAMETER;
	}

	err = lsp_ide_identify(h, target_id, lun0, lun1, packet_device, &info);
	if(LSP_ERR_SUCCESS != err)
	{
		return err;
	}

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

	//	determine IO mode ( UltraDMA, DMA, and PIO ) according to hardware versions

	{
		lsp_uint8 set_dma_feature_mode = 0; // set feature sub command specific
		lsp_uint8 dma_mode = 0;

		*use_dma = 0;

		if(LSP_HW_VERSION_2_0 <= session->HWVersion)
		{
			// ultra dma
			// find fastest ultra dma mode
			if(info.dma_ultra & 0x0001)
				dma_mode = 0;
			if(info.dma_ultra & 0x0002)
				dma_mode = 1;
			if(info.dma_ultra & 0x0004)
				dma_mode = 2;
#ifdef __LSP_CHECK_CABLE80__
			if(info.hw_config & 0x2000)	// true : device detected CBLID - above V_ih
			{
				// try higher ultra dma mode (cable 80 needed)
#endif
				if(info.dma_ultra & 0x0008)
					dma_mode = 3;

				if(info.dma_ultra & 0x0010)
					dma_mode = 4;

				// Disable UDMA mode 5 and over for NDAS 2.0 Rev 0
				if (!(session->HWVersion == LSP_HW_VERSION_2_0 && 
					session->HWRevision == 0)) {
					if(info.dma_ultra & 0x0020)
						dma_mode = 5;
					if(info.dma_ultra & 0x0040)
						dma_mode = 6;
					// level 7 is not supported level
//					if(info.dma_ultra & 0x0080)
//						dma_mode = 7;
				}
#ifdef __LSP_CHECK_CABLE80__
			}
			else
			{
				// cable 80 not detected
			}
#endif

			// Limit UDMA mode 2 for 2.0G 100M test revision
			if (session->HWVersion == LANSCSIIDE_VERSION_2_0 && session->HWRevision == 0x01f) {
				if (dma_mode > 2)
					dma_mode = 2;
			}


#if 0
			if(!(info.dma_ultra /* current ultra dma mode */ &
				 (0x0100 << dma_mode) /* selected ultra dma mode */))
			{
				// current ultra dma mode is not high enough
				// select ultra dma mode higher
				set_dma_feature = 1;
				set_dma_feature_mode = 0x40 /* ultra dma */ | dma_mode;
			}
#else
			// Always set DMA mode because NDAS chip and HDD may have different DMA setting.
			set_dma_feature = 1;
			set_dma_feature_mode = 0x40 /* ultra dma */ | dma_mode;			
#endif
			*use_dma = 1;
		}
		else if(info.dma_mword & 0x00ff)
		{
			// dma
			// find fastest dma mode

			// dma mode 2, 1 and(or) 0 is supported
			if(info.dma_mword & 0x0001)
			{
				/* multiword dma mode 0 is supported*/
				dma_mode = 0;
			}
			if(info.dma_mword & 0x0002)
			{
				/* multiword dma mode 1 is supported*/
				dma_mode = 1;
			}
			if(info.dma_mword & 0x0004)
			{
				/* multiword dma mode 2 is supported*/
				dma_mode = 2;
			}
#if 0
			if(!(info.dma_mword /* current dma mode */ &
				 (0x0100 << dma_mode) /* selected dma mode */))
			{
				// current dma mode is not high enough
				// select dma mode higher
				set_dma_feature = 1;
				set_dma_feature_mode = 0x20 /* dma */ | dma_mode;
			}
#else
			// Always set DMA mode because NDAS chip and HDD may have different DMA setting.
			set_dma_feature = 1;
			set_dma_feature_mode = 0x20 /* dma */ | dma_mode;
#endif
			*use_dma = 1;
		}
		else
		{
			// PIO
		}

		if(set_dma_feature)
		{
			// 0x03 : set transfer mode based on value in sector count register
			err = lsp_ide_setfeatures(h, target_id, lun0, lun1, 0x03, set_dma_feature_mode, 0, 0, 0);
			if(LSP_ERR_SUCCESS != err)
				return err;

			// ensure the (ultra) dma mode is really changed
			err = lsp_ide_identify(h, target_id, lun0, lun1, packet_device, &info);
			if(LSP_ERR_SUCCESS != err)
				return err;

			if(*packet_device)
				return LSP_ERR_COMMAND_FAILED;

			if(set_dma_feature_mode & 0x40)
			{
				/* ultra dma mode*/
				if(!(info.dma_ultra & 0x0100 << dma_mode))
					return LSP_ERR_COMMAND_FAILED;
			}

			if(set_dma_feature_mode & 0x20)
			{
				/* dma mode*/
				if(!(info.dma_mword & 0x0100 << dma_mode))
					return LSP_ERR_COMMAND_FAILED;
			}
		}		
	}

	*use_48 = 0;
	*use_lba = 0;
	// determine lba mode according to disk capacity.
	if((info.command_set_2 & 0x0400) /* 48-bit address feature set */ &&
	   (info.cfs_enable_2 & 0x0400) /* 48-bit address feature set */)
	{
		// support lba 48 mode
		*use_48 = 1;
		*use_lba = 1;
		capacity->low = (lsp_uint32)(info.lba_capacity_2 & 0x00000000FFFFFFFF);
		capacity->high = (lsp_uint32)(info.lba_capacity_2 >> 32);
	}
	else if((info.capability & 0x02) /* lba */ && _is_lba_capacity_ok(&info))
	{
		// support lba 28 mode
		*use_48 = 0;
		*use_lba = 1;
		capacity->low = info.lba_capacity;
		capacity->high = 0;
	}
	else
	{
		// chs mode
		lsp_uint64 capacity64;
		capacity64 = info.cyls;
		capacity64 *= info.heads;
		capacity64 *= info.sectors;
		capacity->low = (lsp_uint32)(capacity64 & 0x00000000FFFFFFFF);
		capacity->high = (lsp_uint32)(capacity64 >> 32);

		// we do not support chs
		return LSP_ERR_COMMAND_FAILED;
	}

	return LSP_ERR_SUCCESS;
}
