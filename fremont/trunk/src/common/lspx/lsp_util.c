#include "lsp_type_internal.h"
#include "lsp_binparm.h"
#include "lsp_debug.h"
#include <lspx/lsp_hash.h>
#include <lspx/lsp_util.h>
#include <lspx/lsp.h>
#include <lspx/lsp_ide_def.h>

lsp_status_t
lsp_call
lsp_complete_request(
					 lsp_handle_context_t* h, 
					 lsp_request_packet_t* request, 
					 lsp_status_t status);

typedef enum _lsp_ide_base_command_t {
	LSP_IDE_BASE_READ,
	LSP_IDE_BASE_WRITE,
	LSP_IDE_BASE_VERIFY,
	LSP_IDE_BASE_PACKET_CMD,
	LSP_IDE_BASE_PACKET_CMD_READ,
	LSP_IDE_BASE_PACKET_CMD_WRITE
} lsp_ide_base_command_t;

enum { LSP_DISK_SECTOR_SIZE = 512 };

lsp_status_t
lsp_call
lsp_ide_write(
			  lsp_handle_t h,
			  const lsp_large_integer_t* location,
			  lsp_uint16_t sectors,
			  void* mutable_data,
			  lsp_uint32_t len)
{
	lsp_status_t			status;
	lsp_request_packet_t*	request;
	
	LSP_ASSERT(NULL != h);
	
	request = &h->session.internal_packets[0];
	if(LSP_STATUS_SUCCESS != (status = lsp_build_ide_write(request, h, location, sectors, mutable_data, len))) {
		return status;
	}
	
	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_ide_read(
			 lsp_handle_t h,
			 const lsp_large_integer_t* location,
			 lsp_uint16_t sectors,
			 void* mutable_data,
			 lsp_uint32_t len)
{
	lsp_status_t			status;
	lsp_request_packet_t*	request;
	
	LSP_ASSERT(NULL != h);
	
	request = &h->session.internal_packets[0];
	if(LSP_STATUS_SUCCESS != (status = lsp_build_ide_read(request, h, location, sectors, mutable_data, len))) {
		return status;
	}
	
	return lsp_request(h, request);
}

lsp_status_t 
lsp_call 
lsp_ide_verify(
			   __in lsp_handle_t h, 
			   __in const lsp_large_integer_t* location, 
			   __in lsp_uint16_t sectors,
			   __reserved void* reserved_1,
			   __reserved lsp_uint32_t reserved_2)
{
	lsp_status_t			status;
	lsp_request_packet_t*	request;
	
	LSP_UNREFERENCED_PARAMETER(reserved_1);
	LSP_UNREFERENCED_PARAMETER(reserved_2);
	
	LSP_ASSERT(NULL != h);
	
	request = &h->session.internal_packets[0];
	if(LSP_STATUS_SUCCESS != (status = lsp_build_ide_verify(request, h, location, sectors, 0, 0))) {
		return status;
	}
	
	return lsp_request(h, request);
}

lsp_status_t 
lsp_call 
lsp_ide_setfeatures(
					lsp_handle_t h,
					lsp_uint8_t subcommand_code,
					lsp_uint8_t subcommand_specific_0 /* sector count register */,
					lsp_uint8_t subcommand_specific_1 /* lba low register */,
					lsp_uint8_t subcommand_specific_2 /* lba mid register */,
					lsp_uint8_t subcommand_specific_3 /* lba high register */)
{
	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;
	
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
	
	/* set registers */
	idereg.device.s.dev = (0 == h->session.login_info.unit_no) ? 0 : 1;
	idereg.device.s.lba = 0;
	
	idereg.command.command = LSP_IDE_CMD_SET_FEATURES;
	
	idereg.reg.basic.reg[0] = subcommand_code;
	idereg.reg.basic.reg[1] = subcommand_specific_0;
	idereg.reg.basic.reg[2] = subcommand_specific_1;
	idereg.reg.basic.reg[3] = subcommand_specific_2;
	idereg.reg.basic.reg[4] = subcommand_specific_3;
	
	return lsp_ide_command(h, &idereg, &data_buf, 0);
}

lsp_status_t
lsp_call
lsp_ide_identify(
				 lsp_handle_t h,
				 lsp_ide_identify_device_data_t* identify_data)
{
	lsp_request_packet_t* request;
	lsp_status_t status;
	request = &h->session.internal_packets[0];
	
	status = lsp_build_ide_identify(
									request,
									h,
									identify_data);
	
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}
	
	return lsp_request(h, request);
}

lsp_status_t 
lsp_call
lsp_ide_identify_packet_device(
							   __in lsp_handle_t h, 
							   __out lsp_ide_identify_packet_device_data_t* ident)
{
	lsp_request_packet_t* request;
	request = &h->session.internal_packets[0];
	
	lsp_build_ide_identify_packet_device(
										 request,
										 h,
										 ident);
	
	return lsp_request(h, request);
}

lsp_status_t
lsp_call
lsp_ide_packet_cmd(
				   __in lsp_handle_t h, 
				   __in void* cdb,
				   __inout void* mutable_data,
				   __in lsp_uint32_t len,
				   __in lsp_uint8_t data_to_target)
{
	lsp_request_packet_t* request;
	
	LSP_ASSERT(NULL != h);
	
	request = &h->session.internal_packets[0];
	lsp_build_ide_packet_cmd(request, h, cdb, mutable_data, len, data_to_target);
	
	return lsp_request(h, request);
}

/*
 returns
 1 : if capacity is ok
 0 : otherwise
 */
static
lsp_uint32_t
lspp_is_valid_lba24(
					__in const lsp_ide_identify_device_data_t* devdata,
					__out lsp_uint32_t* fixed_lba24)
{
	lsp_uint32_t lba24, chs_sects, head, tail;
	
	*fixed_lba24 = lsp_letohl(devdata->lba28_capacity);
	
	/* If the drive is capable of handling 48 Bit LBA Address 0400h 
	 and it is enabled, we assume that there is no restrictions
	 on the capacity */
	
	if (devdata->command_set_support.big_lba &&
		devdata->command_set_active.big_lba)
	{
		return 1;
	}
	
	/*
	 The ATA spec tells large drivers to return
	 C/H/S = 16383/16/63 independent of their size.
	 Some drives can be jumpered to use 15 heads instead of 16.
	 Some drives can be jumpered to use 4092 cyls instead of 16383
	 */
	
	/*
	 if((info->cyls == 16383 || (info->cyls == 4092 && info->cur_cyls== 16383)) &&
	 info->sectors == 63 && 
	 (info->heads == 15 || info->heads == 16) &&
	 info->lba_capacity >= (unsigned)(16383 * 63 * info->heads))
	 */
	if ((devdata->num_cylinders == 16383 ||
		 (devdata->num_cylinders == 4092 && 
		  devdata->number_of_current_cylinders == 16383)) &&
		devdata->num_sectors_per_track == 63 &&
		(devdata->num_heads == 15 || devdata->num_heads == 16) &&
		devdata->lba28_capacity >= (lsp_uint32_t)(
												  16383 * 63 * devdata->num_heads))
	{
		return 1;
	}
	
	/* lba_sects = info->lba_capacity; */
	lba24 = devdata->lba28_capacity;
	/* chs_sects = info->cyls * info->heads * info->sectors; */
	chs_sects = 
	devdata->num_cylinders * 
	devdata->num_heads * 
	devdata->num_sectors_per_track;
	
	/* Perform a rough sanity check on lba_sects: within 10% is OK */
	if((lba24 - chs_sects) < chs_sects / 10) 
	{
		return 1;
	}
	
	/* Some drives have the word order reversed */
	head = ((lba24 >> 16) & 0xffff);
	tail = (lba24 & 0xffff);
	lba24 = (head | (tail << 16));
	if((lba24 - chs_sects) < chs_sects / 10) 
	{
		/* info->lba_capacity = lba_sects; */
		*fixed_lba24 = lba24;
		/* Capacity reversed.... */
		return 1;
	}
	
	return 0;
}

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
lsp_uint32_t
lsp_verify_hddrive_capacity(
							__in const void *_info,
							__out lsp_large_integer_t* capacity)
{
	lsp_uint32_t flags = 0;
	lsp_ide_identify_device_data_t *info;
	lsp_uint64_t capacity64;
	
	info = (lsp_ide_identify_device_data_t *)_info;
	
	/* determine lba mode according to disk capacity. */
	if (info->command_set_support.big_lba && 
		info->command_set_active.big_lba)
	{
		/* support LBA 48 mode */
		flags |= (LSP_SCF_48BIT_LBA | LSP_SCF_LBA);
		
		/* identify data is little endian */
		capacity64 = (lsp_uint64_t) lsp_letohl(info->lba48_capacity_lsw) +
		((lsp_uint64_t) lsp_letohl(info->lba48_capacity_msw) << 32);
		capacity->u.low = (lsp_uint32_t)(capacity64 & 0x00000000FFFFFFFF);
		capacity->u.high = (lsp_uint32_t)(capacity64 >> 32);
	}
	else if (info->capabilities.lba_supported)
	{
		lsp_uint32_t fixed_lba28;
		if (lspp_is_valid_lba24(info, &fixed_lba28))
		{
			/* support LBA 28 mode */
			flags |= LSP_SCF_LBA;
			capacity->u.low = fixed_lba28;
			capacity->u.high = 0;
		}
	}
	else
	{
		/* CHS mode */
		capacity64 = info->num_cylinders;
		capacity64 *= info->num_heads;
		capacity64 *= info->num_sectors_per_track;
		capacity->u.low = (lsp_uint32_t)(capacity64 & 0x00000000FFFFFFFF);
		capacity->u.high = (lsp_uint32_t)(capacity64 >> 32);
		
		/* we do not support CHS mode */
	}
	return flags;
}

lsp_status_t
lsp_call
lsp_acquire_lock(lsp_handle_t h, lsp_uint8_t lock_number)
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
lsp_release_lock(lsp_handle_t h, lsp_uint8_t lock_number)
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

lsp_status_t
lsp_call
lsp_set_user_password(
					  __in lsp_handle_t h,
					  __in lsp_uint8_t *password)
{
	lsp_request_packet_t*			request;
	lsp_vendor_command_request_t*	vc_request;
	char							passwordBuffer[8];
	int								i;
	
	request = &h->session.internal_packets[0];
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_SET_USER_PW;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;

	// Reverse Password.
	for (i = 0; i < 8; i++) {
		passwordBuffer[i] = password[7 - i];
	}
	
	vc_request->param = passwordBuffer;
	vc_request->param_length = 8;
		
	return lsp_request(h, request);	
}

lsp_status_t
lsp_call
lsp_reset(
		  __in lsp_handle_t h
		  )
{
	lsp_request_packet_t* request;
	lsp_status_t status;
	request = &h->session.internal_packets[0];
	
	status = lsp_build_reset(request,
							 h);
	
	if (LSP_STATUS_SUCCESS != status)
	{
		return status;
	}
	
	return lsp_request(h, request);	
}

void
lsp_call
lsp_build_login(
				__inout lsp_request_packet_t* request,
				__in lsp_handle_t h,
				__in const lsp_login_info_t* login_info)
{
	lsp_login_request_t* subreq;
	h = h;
	request->type = LSP_REQUEST_LOGIN;
	subreq = &request->u.login.request;
	subreq->login_info = *login_info;
}

void
lsp_call
lsp_build_logout(
				 __inout lsp_request_packet_t* request,
				 __in lsp_handle_t h)
{
//	lsp_logout_request_t* subreq;
	h = h;
	request->type = LSP_REQUEST_LOGOUT;
//	subreq = &request->u.logout.request;
}

lsp_status_t
lsp_call
lsp_ata_handshake(
				  __in lsp_handle_t h)
{
	lsp_request_packet_t* request;
	lsp_status_t status;
	request = &h->session.internal_packets[0];
	
	lsp_build_ata_handshake(request, h);
	
	status = lsp_request(h, request);
	
	return status;
}

const lsp_ata_handshake_data_t*
lsp_call
lsp_get_ata_handshake_data(
						   __in lsp_handle_t h)
{
	return &h->session.handshake_data;
}

const lsp_ide_identify_device_data_t*
lsp_call
lsp_get_ide_identify_device_data(
								 __in lsp_handle_t h)
{
	return &h->session.identify_data.ata;
}

const lsp_hardware_data_t*
lsp_call
lsp_get_hardware_data(
					  __in lsp_handle_t h)
{
	return &h->session.hardware_data;
}

void
lsp_call
lsp_build_ata_handshake(
						__inout lsp_request_packet_t* request,
						__in lsp_handle_t h)
{
//	lsp_ata_handshake_request_t* subreq;
	
	LSP_UNREFERENCED_PARAMETER(h);
	
	request->type = LSP_REQUEST_EX_ATA_HANDSHAKE_COMMAND;
//	subreq = &request->u.ata_handshake_command.request;
}

lsp_status_t
lsp_call
lsp_build_ide_base_command(
						   __inout lsp_request_packet_t* request,
						   __in lsp_handle_t h,
						   __in const lsp_large_integer_t* location, 
						   __in lsp_uint16_t sectors, 
						   __in void* buf, 
						   __in lsp_uint32_t buflen,
						   __in lsp_ide_base_command_t basecmd,
						   __in void* cdb)
{
	lsp_session_data_t* session;
	
	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;
	lsp_extended_command_t ext_cmd;
	
	session = &h->session;
	
	if (!session->handshake_data.valid)
	{
		return LSP_STATUS_REQUIRES_HANDSHAKE;
	}
	
	if (session->handshake_data.lba)
	{
		if (session->handshake_data.lba48)
		{
			if (location->u.high >= 0x00010000)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
		}
		else
		{
			if (location->u.high >= 0x00000001 || 
				location->u.low >= 0x10000000)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
		}
	}
	else
	{
		LSP_ASSERT(FALSE);
	}
	
	/* parameter validation */
	
	switch (basecmd)
	{
		case LSP_IDE_BASE_READ:
		case LSP_IDE_BASE_WRITE:
			if (0 == sectors)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
			if (!buf)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
			if (sectors * session->handshake_data.logical_block_size > session->hardware_data.maximum_transfer_blocks * LSP_DISK_SECTOR_SIZE ||
				buflen != (lsp_uint32_t) sectors * session->handshake_data.logical_block_size)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
			break;
		case LSP_IDE_BASE_PACKET_CMD_READ:
		case LSP_IDE_BASE_PACKET_CMD_WRITE:
			if (!buf)
			{
				return LSP_STATUS_INVALID_PARAMETER;
			}
			break;
    	case LSP_IDE_BASE_VERIFY:
		case LSP_IDE_BASE_PACKET_CMD:
            break;
	}
	
	/* set registers */
	
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	lsp_memset(&ext_cmd, 0, sizeof(lsp_extended_command_t));
	
	if (LSP_IDE_BASE_PACKET_CMD == basecmd ||
		LSP_IDE_BASE_PACKET_CMD_READ == basecmd ||
		LSP_IDE_BASE_PACKET_CMD_WRITE == basecmd)
	{
		/* set cdb to ext_cmd. this will be copied to ahs later */
		ext_cmd.cmd_buffer = cdb;
		ext_cmd.cmd_size = LSP_PACKET_COMMAND_SIZE;
		
		/* see the spec of PACKET section in ATA-ATAPI */
		/* this will set com_type_p to 1 */
		idereg.command.command = LSP_IDE_CMD_PACKET; 
		
		idereg.device.s.dev = (0 == session->login_info.unit_no) ? 0 : 1;
		
		switch (((unsigned char *)cdb)[0])
		{			
				// Read Data.
			case 0x28://kSCSICmd_READ_10:
			case 0xA8://kSCSICmd_READ_12:
			case 0xBE://kSCSICmd_READ_CD:
				if (session->handshake_data.dma_supported) 
				{
					// use DMA.
					idereg.reg.named.features = 0x01;
				}
				break;
				
				// Write Data.
			case 0x2A://kSCSICmd_WRITE_10:
			case 0xAA://kSCSICmd_WRITE_12:
			case 0x2E://kSCSICmd_WRITE_AND_VERIFY_10:
			case 0x15://kSCSICmd_MODE_SELECT_6:
			case 0x55://kSCSICmd_MODE_SELECT_10:
				if (session->handshake_data.dma_supported) 
				{
					// use DMA.
					idereg.reg.named.features = 0x01;
				}
				break;
				
				// DVD Key
			case 0xA3://kSCSICmd_SEND_KEY:
				break;
				
			default:
				break;
		}
	}
	else
	{
		idereg.device.s.dev = (0 == session->login_info.unit_no) ? 0 : 1;
		idereg.device.s.lba = session->handshake_data.lba;
		
		if (session->handshake_data.dma_supported)
		{
			if (session->handshake_data.lba48)
			{
				switch (basecmd)
				{
					case LSP_IDE_BASE_READ:
						idereg.command.command = LSP_IDE_CMD_READ_DMA_EXT;
						break;
					case LSP_IDE_BASE_WRITE:
						idereg.command.command = LSP_IDE_CMD_WRITE_DMA_EXT;
						break;
					case LSP_IDE_BASE_VERIFY:
						idereg.command.command = LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT;
						break;
					default:
						LSP_ASSERT(0);
						break;
				}
			}
			else
			{
				switch (basecmd)
				{
					case LSP_IDE_BASE_READ:
						idereg.command.command = LSP_IDE_CMD_READ_DMA;
						break;
					case LSP_IDE_BASE_WRITE:
						idereg.command.command = LSP_IDE_CMD_WRITE_DMA;
						break;
					case LSP_IDE_BASE_VERIFY:
						idereg.command.command = LSP_IDE_CMD_READ_VERIFY_SECTORS;
						break;
					default:
						LSP_ASSERT(0);
						break;                        
				}
			}
		}
		else
		{
			if (session->handshake_data.lba48)
			{
				switch (basecmd)
				{
					case LSP_IDE_BASE_READ:
						idereg.command.command = LSP_IDE_CMD_READ_SECTORS_EXT;
						break;
					case LSP_IDE_BASE_WRITE:
						idereg.command.command = LSP_IDE_CMD_WRITE_SECTORS_EXT;
						break;
					case LSP_IDE_BASE_VERIFY:
						idereg.command.command = LSP_IDE_CMD_READ_VERIFY_SECTORS_EXT;
						break;
					default:
						LSP_ASSERT(0);
						break;                        
				}
			}
			else
			{
				switch (basecmd)
				{
					case LSP_IDE_BASE_READ:
						idereg.command.command = LSP_IDE_CMD_READ_SECTORS;
						break;
					case LSP_IDE_BASE_WRITE:
						idereg.command.command = LSP_IDE_CMD_WRITE_SECTORS;
						break;
					case LSP_IDE_BASE_VERIFY:
						idereg.command.command = LSP_IDE_CMD_READ_VERIFY_SECTORS;
						break;
					default:
						LSP_ASSERT(0);
						break;                        
				}
			}
		}
	}
	
	if (session->handshake_data.lba48)
	{
		idereg.reg.named_48.cur.sector_count = (lsp_uint8_t)((sectors & 0x00FF) >> 0);
		idereg.reg.named_48.prev.sector_count = (lsp_uint8_t)((sectors & 0xFF00) >> 8);
		idereg.reg.named_48.cur.lba_low = (lsp_uint8_t)((location->u.low & 0x000000FF) >> 0);
		idereg.reg.named_48.cur.lba_mid = (lsp_uint8_t)((location->u.low & 0x0000FF00) >> 8);
		idereg.reg.named_48.cur.lba_high = (lsp_uint8_t)((location->u.low & 0x00FF0000) >> 16);
		idereg.reg.named_48.prev.lba_low = (lsp_uint8_t)((location->u.low & 0xFF000000) >> 24);
		idereg.reg.named_48.prev.lba_mid = (lsp_uint8_t)((location->u.high & 0x000000FF) >> 0);
		idereg.reg.named_48.prev.lba_high = (lsp_uint8_t)((location->u.high & 0x0000FF00) >> 8);
	}
	else
	{
		idereg.reg.named.sector_count = (lsp_uint8_t)((sectors & 0x00FF) >> 0);
		idereg.reg.named.lba_low = (lsp_uint8_t)((location->u.low & 0x000000FF) >> 0);
		idereg.reg.named.lba_mid = (lsp_uint8_t)((location->u.low & 0x0000FF00) >> 8);
		idereg.reg.named.lba_high = (lsp_uint8_t)((location->u.low & 0x00FF0000) >> 16);
		idereg.device.s.lba_head_nr = (lsp_uint8_t)((location->u.low & 0x0F000000) >> 24);
	}
	
	/* set data buffer */
	
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
	
	switch (basecmd)
	{
		case LSP_IDE_BASE_READ:
		case LSP_IDE_BASE_PACKET_CMD_READ:
			data_buf.recv_buffer = (lsp_uint8_t*) buf;
			data_buf.recv_size = buflen;
			break;
		case LSP_IDE_BASE_WRITE:
		case LSP_IDE_BASE_PACKET_CMD_WRITE:
			data_buf.send_buffer = (lsp_uint8_t*) buf;
			data_buf.send_size = buflen;
			break;
		case LSP_IDE_BASE_VERIFY:
		case LSP_IDE_BASE_PACKET_CMD:
			break;        
	}
	
	lsp_build_ide_command(
						  request,
						  h,
						  &idereg,
						  &data_buf,
						  &ext_cmd);
	
	return LSP_STATUS_SUCCESS;
}

lsp_status_t
lsp_call
lsp_build_ide_write(
					__inout lsp_request_packet_t* request,
					__in lsp_handle_t h,
					__in const lsp_large_integer_t* location, 
					__in lsp_uint16_t sectors,
					__in void* buf, 
					__in lsp_uint32_t buflen)
{
	return lsp_build_ide_base_command(
									  request, h, location, sectors, 
									  buf, buflen, 
									  LSP_IDE_BASE_WRITE, NULL);
}

lsp_status_t
lsp_call
lsp_build_ide_read(
				   __inout lsp_request_packet_t* request,
				   __in lsp_handle_t h,
				   __in const lsp_large_integer_t* location, 
				   __in lsp_uint16_t sectors, 
				   __in void* buf, 
				   __in lsp_uint32_t buflen)
{
	return lsp_build_ide_base_command(
									  request, h, location, sectors, 
									  buf, buflen, 
									  LSP_IDE_BASE_READ, NULL);
}

lsp_status_t 
lsp_call 
lsp_build_ide_verify(
					 __inout lsp_request_packet_t* request,
					 __in lsp_handle_t h,
					 __in const lsp_large_integer_t* location, 
					 __in lsp_uint16_t sectors,
					 __reserved void* reserved_1,
					 __reserved lsp_uint32_t reserved_2)
{
	return lsp_build_ide_base_command(
									  request, h, location, sectors, 
									  reserved_1, reserved_2, 
									  LSP_IDE_BASE_VERIFY, NULL);
}

lsp_status_t
lsp_call
lsp_build_ide_packet_cmd(
						 __inout lsp_request_packet_t* request,
						 __in lsp_handle_t h,
						 __in void* cdb,
						 __inout void* buf, 
						 __in lsp_uint32_t buflen,
						 __in lsp_uint8_t buf_to_target)
{
	lsp_large_integer_t location;
	
	lsp_memset(&location, 0, sizeof(lsp_large_integer_t));
	
	/* see the spec of PACKET section in ATA-ATAPI */
	
	location.u.low = buflen << 8; 
	
	return lsp_build_ide_base_command(
									  request,
									  h,
									  &location, 
									  0, 
									  buf, 
									  buflen,
									  (NULL == buf) ? 
									  LSP_IDE_BASE_PACKET_CMD :
									  (buf_to_target) ?
									  LSP_IDE_BASE_PACKET_CMD_WRITE : LSP_IDE_BASE_PACKET_CMD_READ,
									  cdb);
}

lsp_status_t
lsp_call
lsp_build_ide_write_fua(
						__inout lsp_request_packet_t* request,
						__in lsp_handle_t h,
						__in const lsp_large_integer_t* location, 
						__in lsp_uint16_t sectors, 
						__inout_bcount(len) void* buf, 
						__in lsp_uint32_t buf_len)
{
	lsp_session_data_t* session;
	
	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;
	
	session = &h->session;
	
	if (!session->handshake_data.valid)
	{
		return LSP_STATUS_REQUIRES_HANDSHAKE;
	}
	
	if (!h->session.handshake_data.support.write_fua ||
		!h->session.handshake_data.lba48)
	{
		return LSP_STATUS_NOT_SUPPORTED;
	}
	
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	
	idereg.device.s.dev = (0 == session->login_info.unit_no) ? 0 : 1;
	idereg.device.s.lba = session->handshake_data.lba;
	
	LSP_ASSERT(session->handshake_data.lba48);
	
	if (session->handshake_data.dma_supported)
	{
		idereg.command.command = LSP_IDE_CMD_WRITE_DMA_FUA_EXT;
	}
	else
	{
		idereg.command.command = LSP_IDE_CMD_WRITE_MULTIPLE_FUA_EXT;
	}
	
	idereg.reg.named_48.cur.sector_count = (lsp_uint8_t)((sectors & 0x00FF) >> 0);
	idereg.reg.named_48.prev.sector_count = (lsp_uint8_t)((sectors & 0xFF00) >> 8);
	idereg.reg.named_48.cur.lba_low = (lsp_uint8_t)((location->u.low & 0x000000FF) >> 0);
	idereg.reg.named_48.cur.lba_mid = (lsp_uint8_t)((location->u.low & 0x0000FF00) >> 8);
	idereg.reg.named_48.cur.lba_high = (lsp_uint8_t)((location->u.low & 0x00FF0000) >> 16);
	idereg.reg.named_48.prev.lba_low = (lsp_uint8_t)((location->u.low & 0xFF000000) >> 24);
	idereg.reg.named_48.prev.lba_mid = (lsp_uint8_t)((location->u.high & 0x000000FF) >> 0);
	idereg.reg.named_48.prev.lba_high = (lsp_uint8_t)((location->u.high & 0x0000FF00) >> 8);
	
	/* set data buffer */
	
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
	
	data_buf.send_buffer = (lsp_uint8_t*) buf;
	data_buf.send_size = buf_len;
	
	lsp_build_ide_command(
						  request,
						  h,
						  &idereg,
						  &data_buf,
						  0);
	
	return LSP_STATUS_SUCCESS;
	
}

lsp_status_t
lsp_call
lsp_build_ide_identify(
					   __inout lsp_request_packet_t* request,
					   __in lsp_handle_t h,
					   __out_bcount(sizeof(lsp_ide_identify_device_data_t)) 
					   lsp_ide_identify_device_data_t* ident)
{
	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;
	
	/* set registers */
	
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	idereg.device.s.dev = (0 == h->session.login_info.unit_no) ? 0 : 1;
	idereg.command.command = LSP_IDE_CMD_IDENTIFY_DEVICE;
	
	/* set data buffer */
	
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
	data_buf.recv_buffer = (lsp_uint8_t *) ident;
	data_buf.recv_size = sizeof(lsp_ide_identify_device_data_t);
	
	lsp_build_ide_command(
						  request,
						  h,
						  &idereg,
						  &data_buf, 0);
	
	return LSP_STATUS_SUCCESS;
}

void
lsp_call
lsp_build_ide_identify_packet_device(
									 __inout lsp_request_packet_t* request,
									 __in lsp_handle_t h,
									 __in lsp_ide_identify_packet_device_data_t* ident)
{
	lsp_ide_register_param_t idereg;
	lsp_io_data_buffer_t data_buf;
	
	/* set registers */
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	idereg.device.s.dev = (0 == h->session.login_info.unit_no) ? 0 : 1;
	idereg.command.command = LSP_IDE_CMD_IDENTIFY_PACKET_DEVICE;
	
	/* set data buffer */
	lsp_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
	data_buf.recv_buffer = (lsp_uint8_t *) ident;
	data_buf.recv_size = sizeof(lsp_ide_identify_device_data_t);
	
	lsp_build_ide_command(
						  request,
						  h,
						  &idereg,
						  &data_buf, 0);
}

lsp_status_t
lsp_call
lsp_build_ide_set_features(
						   __inout lsp_request_packet_t* request,
						   __in lsp_handle_t h,
						   __in lsp_uint8_t subcommand_code,
						   __in lsp_uint8_t subcommand_specific_0 /* sector count register */, 
						   __in lsp_uint8_t subcommand_specific_1 /* lba low register */, 
						   __in lsp_uint8_t subcommand_specific_2 /* lba mid register */, 
						   __in lsp_uint8_t subcommand_specific_3 /* lba high register */)
{
	lsp_ide_register_param_t idereg;
	
	/* set registers */
	lsp_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));
	idereg.device.s.dev = (0 == h->session.login_info.unit_no) ? 0 : 1;
	idereg.device.s.lba = 0;
	
	idereg.command.command = LSP_IDE_CMD_SET_FEATURES;
	
	idereg.reg.basic.reg[0] = subcommand_code;
	idereg.reg.basic.reg[1] = subcommand_specific_0;
	idereg.reg.basic.reg[2] = subcommand_specific_1;
	idereg.reg.basic.reg[3] = subcommand_specific_2;
	idereg.reg.basic.reg[4] = subcommand_specific_3;
	
	lsp_build_ide_command(request, h, &idereg, 0, 0);
	
	return LSP_STATUS_SUCCESS;
}

void
lsp_call
lsp_build_acquire_lock(
					   __inout lsp_request_packet_t* request,
					   __in lsp_handle_t h,
					   __in lsp_uint8_t lock_number)
{
	lsp_vendor_command_request_t* vc_request;
	
	LSP_UNREFERENCED_PARAMETER(h);
	
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
					   __in lsp_uint8_t lock_number)
{
	lsp_vendor_command_request_t* vc_request;
	
	LSP_UNREFERENCED_PARAMETER(h);
	
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_FREE_SEMA;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	vc_request->param[0] = lock_number;
}

lsp_status_t
lsp_call
lsp_build_reset(
				__inout lsp_request_packet_t* request,
				__in lsp_handle_t h
)
{
	lsp_vendor_command_request_t* vc_request;
	
	LSP_UNREFERENCED_PARAMETER(h);
	
	lsp_memset(request, 0, sizeof(lsp_request_packet_t));
	request->type = LSP_REQUEST_VENDOR_COMMAND;
	vc_request = &request->u.vendor_command.request;
	vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
	vc_request->vop_code = LSP_VCMD_RESET;
	vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
	
	return LSP_STATUS_SUCCESS;
}

#if 0
void
lsp_call
lsp_build_text_target_list(
						   __inout lsp_request_packet_t* request, 
						   __in lsp_handle_t h)
{
	
}

void
lsp_call
lsp_build_text_target_list(
						   __inout lsp_request_packet_t* request, 
						   __in lsp_handle_t h)
{
	
}
#endif

