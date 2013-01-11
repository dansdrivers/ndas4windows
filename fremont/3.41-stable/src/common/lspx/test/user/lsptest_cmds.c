#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#ifndef min
#define min(a,b) (((a) > (b)) ? (b) : (a))
#endif

#include "lsptest.h"

int lsptest_show_idereg_in = 0;
int lsptest_show_idereg_out = 1;

char* string_byte_bits(__in unsigned char bits)
{
	static char buf[256] = {0};
	int i;
	char* p = buf;
	for (i = 0; i < 8; ++i)
	{
		*p++ = ((bits << i) & 0x80) ? '1' : '0';
		if ((i + 1) % 4 == 0) *p++ = ' ';
	}
	*(p-1) = 0;
	return buf;
}

char* string_word_bits(__in unsigned short bits)
{
	static char buf[256] = {0};
	int i;
	char* p = buf;
	for (i = 0; i < 16; ++i)
	{
		*p++ = ((bits << i) & 0x8000) ? '1' : '0'; 
		if ((i + 1) % 4 == 0) *p++ = ' ';
	}
	*(p-1) = 0;
	return buf;
}

char* string_dword_bits(__in lsp_uint32_t bits)
{
	static char buf[256] = {0};
	lsp_uint32_t i;
	char* p = buf;
	for (i = 0; i < 32; ++i)
	{
		*p++ = ((bits << i) & 0x80000000) ? '1' : '0'; 
		if ((i + 1) % 4 == 0) *p++ = ' ';
	}
	*(p-1) = 0;
	return buf;
}

char* string_qword_bits(__in lsp_uint64_t bits)
{
	static char buf[256] = {0};
	lsp_uint64_t i;
	char* p = buf;
	for (i = 0; i < 64; ++i)
	{
		*p++ = ((bits << i) & 0x8000000000000000ULL) ? '1' : '0'; 
		if ((i + 1) % 4 == 0) *p++ = ' ';
	}
	*(p-1) = 0;
	return buf;
}

void disp_byte_bits(__in unsigned char bits)
{
	puts(string_byte_bits(bits));
}

void fputs_word_bits(__in unsigned short bits, __inout FILE* f)
{
	fputs(string_word_bits(bits), f);
}

void disp_word_bits(__in unsigned short bits)
{
	fputs_word_bits(bits, stdout);
}

void disp_dword_bits(__in lsp_uint32_t bits)
{
	puts(string_dword_bits(bits));
}

void disp_qword_bits(__in lsp_uint64_t bits)
{
	puts(string_qword_bits(bits));
}

void 
print_word_bits_desc(
	__in FILE* out,
	__in unsigned short bits, 
	__in_ecount(16) const char** desctable)
{
	int i;
	for (i = 0; i < 16; ++i)
	{
		fprintf(out, " [%c] %s\n", (bits << i) & 0x8000 ? 'X' : ' ', desctable[i]);
	}
}

void disp_bytes(FILE* out, unsigned char* buffer, size_t len)
{
	size_t i, j;

	fprintf(out, "     | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	for (i = 0; i < len; i += 16)
	{
		if ((i%512) == 0)
		{
			fprintf(out, "-----+-[%03d]------------------------------------------\n", i / 512);
		}
		fprintf(out, "%04X | ", i);
		for (j = 0; j < 16; ++j)
		{
			if(i + j < len)
			{
				fprintf(out, "%02X ", buffer[i+j]);
			}
			else
			{
				fprintf(out, "   ", buffer[i+j]);
			}
		}
		for (j = 0; j < 16 && (i + j < len); ++j)
		{
			unsigned char c = buffer[i+j];
			if (!isprint(c)) c = '.';
			fprintf(out, "%c", c);
		}
		fprintf(out, "\n");
	}
}

void disp_ide_register(FILE* out, const lsp_ide_register_param_t* idereg)
{
	fprintf(out, "Feature/Error : %02X %s", idereg->reg.named_48.cur.features, string_byte_bits(idereg->reg.named_48.cur.features));
	fprintf(out, " | %02X %s\n", idereg->reg.named_48.prev.features, string_byte_bits(idereg->reg.named_48.prev.features));
	fprintf(out, "Sector Count  : %02X %s", idereg->reg.named_48.cur.sector_count, string_byte_bits(idereg->reg.named_48.cur.sector_count));
	fprintf(out, " | %02X %s\n", idereg->reg.named_48.prev.sector_count, string_byte_bits(idereg->reg.named_48.prev.sector_count));
	fprintf(out, "LBA Low       : %02X %s", idereg->reg.named_48.cur.lba_low, string_byte_bits(idereg->reg.named_48.cur.lba_low));
	fprintf(out, " | %02X %s\n", idereg->reg.named_48.prev.lba_low, string_byte_bits(idereg->reg.named_48.prev.lba_low));
	fprintf(out, "LBA Mid       : %02X %s", idereg->reg.named_48.cur.lba_mid, string_byte_bits(idereg->reg.named_48.cur.lba_mid));
	fprintf(out, " | %02X %s\n", idereg->reg.named_48.prev.lba_mid, string_byte_bits(idereg->reg.named_48.prev.lba_mid));
	fprintf(out, "LBA High      : %02X %s", idereg->reg.named_48.cur.lba_high, string_byte_bits(idereg->reg.named_48.cur.lba_high));
	fprintf(out, " | %02X %s\n", idereg->reg.named_48.prev.lba_high, string_byte_bits(idereg->reg.named_48.prev.lba_high));
	fprintf(out, "Device        : %02X %s\n", idereg->device.device, string_byte_bits(idereg->device.device));
	fprintf(out, "Command/Status: %02X %s\n", idereg->command.command, string_byte_bits(idereg->command.command));
}

void disp_smart_log_directory(
	FILE* out, 
	__in_ecount(512) const unsigned char* data)
{
	int i;
	fprintf(out, "Smart Logging Version: %02X %02X\n", data[0], data[1]);
	fprintf(out, "Number of sectors in the log at log address\n");
	for (i = 0; i < 255; ++i)
	{
		/* i + 1  */
		fprintf(out, "%4d ", data[2*i+2]);
		if (((i+1)%12)== 0) fprintf(out, "\n");
	}
	fprintf(out, "\n");
}

int lsptest_check_lsp_status(lsptest_context_t* context, int ret)
{
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		lsp_status_detail_t dstatus;
		
		dstatus.status = context->lsp_status;
		if (dstatus.detail.function != LSP_ERR_FUNC_NONE)
		{
			fprintf(stderr, "lsp error: status=0x%08X "
				" (function=%d, sequence=%d, type=%d, response=0x%02X)\n", 
				context->lsp_status,
				dstatus.detail.function, 
				dstatus.detail.sequence,
				dstatus.detail.type, 
				dstatus.detail.response);
		}
		else
		{
			fprintf(stderr, "lsp error: status=0x%08X\n", context->lsp_status);
		}
		return -1;
	}
	return 0;
}

int
lsptest_login(
	lsptest_context_t* context,
	const lsp_login_info_t* login_info)
{
	int ret;
	context->lsp_status = lsp_login(context->lsp_handle, login_info);
	ret = lsptest_transport_process_transfer(context);
	return lsptest_check_lsp_status(context, ret);
}

int
lsptest_logout(
	lsptest_context_t* context)
{
	int ret;
	context->lsp_status = lsp_logout(context->lsp_handle);
	ret = lsptest_transport_process_transfer(context);
	return lsptest_check_lsp_status(context, ret);
}

int lspsh_ata_handshake(lsptest_context_t* context)
{
	int ret;
	const lsp_ata_handshake_data_t* handshake_data;
	context->lsp_status = lsp_ata_handshake(context->lsp_handle);
	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (0 != ret)
	{
		return ret;
	}
	handshake_data = lsp_get_ata_handshake_data(context->lsp_handle);
	if (handshake_data->device_type == 0)
	{
		fputs("block device, ", stdout);
	}
	else
	{
		fputs("packet device, ", stdout);
	}
	fprintf(stdout, "lba=%d, ", handshake_data->lba);
	fprintf(stdout, "lba48=%d, ", handshake_data->lba48);
	fprintf(stdout, "dma_supported=%d\n", handshake_data->dma_supported);

	fprintf(stdout, "DMA Transfer mode: ");

	switch (handshake_data->active.dma_mode)
	{
	case LSP_IDE_TRANSFER_MODE_SINGLEWORD_DMA:
		fprintf(stdout, "Single word DMA mode %d\n", handshake_data->active.dma_level);
		break;
	case LSP_IDE_TRANSFER_MODE_MULTIWORD_DMA:
		fprintf(stdout, "Multi word DMA mode %d\n", handshake_data->active.dma_level);
		break;
	case LSP_IDE_TRANSFER_MODE_ULTRA_DMA:
		fprintf(stdout, "Ultra DMA mode %d\n", handshake_data->active.dma_level);
		break;
	default:
		fprintf(stdout, "No DMA transfer mode is active.\n");
		break;
	}

	fprintf(stdout, "PIO Transfer mode: PIO %d\n", handshake_data->support.pio_level);

#if defined(_MSC_VER)
	fprintf(stdout, "LBA capacity: %I64u (%I64Xh) - %I64u GB or %I64u Billion Bytes\n", 
		handshake_data->lba_capacity.quad,
		handshake_data->lba_capacity.quad,
		handshake_data->lba_capacity.quad * 512 / 1024 / 1024 / 1024,
		handshake_data->lba_capacity.quad * 512 / 1000 / 1000 / 1000);
#else
	fprintf(stdout, "LBA capacity: %llu (%llXh) - %llu GB or %llu Billion Bytes\n", 
		handshake_data->lba_capacity.quad,
		handshake_data->lba_capacity.quad,
		handshake_data->lba_capacity.quad * 512 / 1024 / 1024 / 1024,
		handshake_data->lba_capacity.quad * 512 / 1000 / 1000 / 1000);
#endif
	return 0;
}

int 
lsptest_set_features(
	lsptest_context_t* context,
	lsp_ide_register_param_t* reg_in)
{
	int ret;
	lsp_ide_register_param_t reg_out = {0};
	//reg_in->reg.named.features = 0xDA;
	//reg_in->reg.named.lba_mid = 0x4F;
	//reg_in->reg.named.lba_high = 0xC2;
	reg_in->command.command = LSP_IDE_CMD_SET_FEATURES;
	reg_in->device.device = 0;
	context->lsp_status = lsp_ide_command(context->lsp_handle, reg_in, 0, 0);
	ret = lsptest_transport_process_transfer(context);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		return -1;
	}
	context->lsp_status = lsp_get_ide_command_output_register(
		context->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		fprintf(stderr, "lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			context->lsp_status, 
			context->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		disp_ide_register(stdout, &reg_out);
	}
	return 0;
}

/* level is 0x80 - 0xFE */
int lsptest_set_feature_enable_automatic_acoustic_management(
	lsptest_context_t* context, lsp_uint8_t level)
{
	lsp_ide_register_param_t reg_in = {0};
	/* Enable Automatic Acoustic Management Feature Set */
	reg_in.reg.named.features = LSP_IDE_SET_FEATURES_ENABLE_AAM_FEATURE_SET;
	reg_in.reg.named.sector_count = level;
	return lsptest_set_features(context, &reg_in);
}

int lsptest_set_feature_disable_automatic_acoustic_management(lsptest_context_t* context)
{
	lsp_ide_register_param_t reg_in = {0};
	/* Disable Automatic Acoustic Management Feature Set */
	reg_in.reg.named.features = LSP_IDE_SET_FEATURES_DISABLE_AAM;
	return lsptest_set_features(context, &reg_in);
}

int lsptest_smart_read_log(lsptest_context_t* context)
{
	/* SMART READ LOG */
	int ret;
	unsigned char data[512] = {0};
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_io_data_buffer_t iobuf = {0};
	iobuf.recv_buffer = data;
	iobuf.recv_size = sizeof(data);
	reg_in.reg.named.features = LSP_IDE_SMART_READ_LOG;
	reg_in.reg.named.sector_count = 1;
	reg_in.reg.named.lba_low = 0x00; /* log address */
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = LSP_IDE_CMD_SMART;
	reg_in.device.device = 0;
	context->lsp_status = lsp_ide_command(context->lsp_handle, &reg_in, &iobuf, 0);
	ret = lsptest_transport_process_transfer(context);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		return -1;
	}
	context->lsp_status = lsp_get_ide_command_output_register(
		context->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		fprintf(stderr, "lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			context->lsp_status, 
			context->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		disp_ide_register(stdout, &reg_out);
		disp_bytes(stdout, data, sizeof(data));
	}
	return 0;
}

int lsptest_smart_read_data(lsptest_context_t* context)
{
	/* SMART READ DATA */
	int ret;
	unsigned char data[512] = {0};
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_io_data_buffer_t iobuf = {0};
	iobuf.recv_buffer = data;
	iobuf.recv_size = sizeof(data);
	reg_in.reg.named.features = LSP_IDE_SMART_READ_DATA;
    reg_in.reg.named.sector_count = 1;
    reg_in.reg.named.lba_low = 1;
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = LSP_IDE_CMD_SMART;
	reg_in.device.device = 0xa0;

    fprintf(stdout, ">> Input Register\n");
	disp_ide_register(stdout, &reg_in);

	context->lsp_status = lsp_ide_command(
		context->lsp_handle, &reg_in, &iobuf, 0);
	ret = lsptest_transport_process_transfer(context);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		return -1;
	}
	context->lsp_status = lsp_get_ide_command_output_register(
		context->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		fprintf(stderr, "lsp_get_ide_command_output_register failed: %d(0x%08x)\n",
			context->lsp_status, 
			context->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
        fprintf(stdout, ">> Output Register\n");
		disp_ide_register(stdout, &reg_out);
		// printbytes(data, sizeof(data));
		disp_smart_log_directory(stdout, data);
	}
	return 0;
}

/* SMART RETURN STATUS */
int lsptest_smart_return_status(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.reg.named.features = LSP_IDE_SMART_RETURN_STATUS;
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = LSP_IDE_CMD_SMART;
	reg_in.device.device = 0;
	context->lsp_status = lsp_ide_command(context->lsp_handle, &reg_in, 0, 0);
	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	context->lsp_status = lsp_get_ide_command_output_register(
		context->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		fprintf(stderr, "lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			context->lsp_status, 
			context->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		disp_ide_register(stdout, &reg_out);
	}
	return 0;
}

void fputs_ata_version(lsp_uint16_t major, lsp_uint16_t minor, FILE* out)
{
	static const char* defs[] = {
		/*  0 */ "not reported",
		/*  1 */ "obsolete",
		/*  2 */ "obsolete",
		/*  3 */ "obsolete",
		/*  4 */ "ATA-2 published, ANSI X3.279-1996 (obsolete)",
		/*  5 */ "ATA-2 X3T10 948D prior to revision 2k (obsolete)",
		/*  6 */ "ATA-3 X3T10 2008D revision 1",
		/*  7 */ "ATA-2 X3T10 948D revision 2k (obsolete)",
		/*  8 */ "ATA-3 X3T10 2008D revision 0",
		/*  9 */ "ATA-2 X3T10 948D revision 3 (obsolete)",
		/*  A */ "ATA-3 published, ANSI X3.298-199x",
		/*  B */ "ATA-3 X3T10 2008D revision 6",
		/*  C */ "ATA-3 X3T13 2008D revision 7 and 7a",
		/*  D */ "ATA/ATAPI-4 X3T13 1153D revision 6",
		/*  E */ "ATA/ATAPI-4 T13 1153D revision 13",
		/*  F */ "ATA/ATAPI-4 X3T13 1153D revision 7",
		/* 10 */ "ATA/ATAPI-4 T13 1153D revision 18",
		/* 11 */ "ATA/ATAPI-4 T13 1153D revision 15",
		/* 12 */ "ATA/ATAPI-4 published, ANSI NCITS 317-1998",
		/* 13 */ "ATA/ATAPI-5 T13 1321D revision 3",
		/* 14 */ "ATA/ATAPI-4 T13 1153D revision 14",
		/* 15 */ "ATA/ATAPI-5 T13 1321D revision 1",
		/* 16 */ "ATA/ATAPI-5 published, ANSI NCITS 340-2000",
		/* 17 */ "ATA/ATAPI-4 T13 1153D revision 17",
		/* 18 */ "ATA/ATAPI-6 T13 1410D revision 0",
		/* 19 */ "ATA/ATAPI-6 T13 1410D revision 3a",
		/* 1A */ "Reserved",
		/* 1B */ "ATA/ATAPI-6 T13 1410D revision 2",
		/* 1C */ "ATA/ATAPI-6 T13 1410D revision 1",
		/* 1D */ "ATA/ATAPI-7 published ANSI INCITS 397-2005",
		/* 1E */ "ATA/ATAPI-7 T13 1532D revision 0",
		/* 1F */ "Reserved",
		/* 20 */ "Reserved",
		/* 21 */ "ATA/ATAPI-7 T13 1532D revision 4a",
		/* 22 */ "ATA/ATAPI-6 published, ANSI INCITS 361-2002",
		/* 23 */ "Reserved",
		/* 24 */ "Reserved",
		/* 25 */ "Reserved",
		/* 26 */ "Reserved"
	};
	static const struct { 
		lsp_uint16_t minor;
		const char* rev; 
	} ndefs[] = {
		0x0027, "ATA8-ACS revision 3c",
		0x0033, "ATA8-ACS Revision 3e",
		0x0042, "ATA8-ACS Revision 3f",
		0x0052, "ATA8-ACS revision 3b",
		0x0107, "ATA8-ACS revision 2d",
		0xFFFF, "Minor revision is not reported" 
	};

	int i;
#define countof(A) (sizeof(A)/sizeof(A[0]))

	if (major == 0x0000 || major == 0xFFFF)
	{
		fprintf(out, "* ATA/ATAPI major version not reported.\n");
	}
	else
	{
		for (i = 14; i > 0; --i)
		{
			if (major & (i << i))
			{
				fprintf(out, "* Major: ATA/ATAPI-%d\n", i + 1);
				break;
			}
		}
	}
	if (minor < countof(defs))
	{
		fprintf(out, "* Minor: %s\n", defs[minor]);
		return;
	}
	else
	{
		int i;
		for (i = 0; i < countof(ndefs); ++i)
		{
			if (ndefs[i].minor == minor)
			{
				fprintf(out, "* Minor: %s\n", ndefs[i].rev);
				return;
			}
		}
		return;
	}
}

int disp_identify_device_data(
	__in FILE* out,
	__in lsp_ide_identify_device_data_t* ident)
{
	size_t i;
	unsigned char buf[64];

#define IDAT_FIELD_BYTE_DESC(dat, bitvar) \
	#bitvar, dat->bitvar, dat->bitvar

#define IDAT_FIELD_WORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohs(dat->bitvar), lsp_letohs(dat->bitvar)

#define IDAT_FIELD_DWORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohl(dat->bitvar), lsp_letohl(dat->bitvar)

#define IDAT_NESTED_FIELD_WORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohs(dat.bitvar), lsp_letohs(dat.bitvar)

#define IDAT_NESTED_FIELD_BYTE_DESC(dat, bitvar) \
	#bitvar, dat.bitvar, dat.bitvar

	fprintf(out, "[  0    ] general configuration\n");
	
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, device_type));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, retired_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, removable_media));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, fixed_device));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, retired_2));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, response_incomplete));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, retired_3));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, reserved_1));

	fprintf(out, "[  1    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, num_cylinders));
	fprintf(out, "[  2    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_2));
	fprintf(out, "[  3    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, num_heads));
	fprintf(out, "[  4    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, retired_1[0]));
	fprintf(out, "[  5    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, retired_1[1]));
	fprintf(out, "[  6    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, num_sectors_per_track));
	fprintf(out, "[  7    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, vendor_unique_1[0]));
	fprintf(out, "[  8    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, vendor_unique_1[1]));
	fprintf(out, "[  9    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, vendor_unique_1[2]));

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->serial_number, sizeof(ident->serial_number));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 10    ] %-25s: %s\n", "serial_number", buf);

	fprintf(out, "[ 20    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, retired_2[0]));
	fprintf(out, "[ 21    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, retired_2[1]));
	fprintf(out, "[ 22    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, obsolete_1));

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->firmware_revision, sizeof(ident->firmware_revision));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 23    ] %-25s: %s\n", "firmware_revision", buf);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->model_number, sizeof(ident->model_number));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 27    ] %-25s: %s\n", "model_number", buf);

	fprintf(out, "[ 47    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, maximum_block_transfer));
	fprintf(out, "[ 47    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, vendor_unique_2));

	fprintf(out, "[ 48    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_48));

	fprintf(out, "[ 49- 50] %-25s\n", "capabilities");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, reserved_2));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, standyby_timer_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, reserved_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, iordy_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, iordy_disable));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, lba_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, dma_supported));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, reserved_byte_49));

	fprintf(out, "[ 51    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, obsolete_words_51[0]));
	fprintf(out, "[ 52    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, obsolete_words_51[1]));

	fprintf(out, "[ 53    ] %-25s: %u\n", "reserved_3_2", ident->reserved_3_2);
	fprintf(out, "          %-25s: %u\n", "reserved_3_1", ident->reserved_3_1);
	fprintf(out, "          %-25s: %u\n", "translation_fields_valid", ident->translation_fields_valid);

	fprintf(out, "[ 54    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, number_of_current_cylinders));
	fprintf(out, "[ 55    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, number_of_current_heads));
	fprintf(out, "[ 56    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, current_sectors_per_track));

	fprintf(out, "[ 57- 58] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, current_sector_capacity));

	fprintf(out, "[ 59    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_byte_59));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, multi_sector_setting_valid));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, current_multi_sector_setting));

	fprintf(out, "[ 60- 61] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, lba28_capacity));

	fprintf(out, "[ 62    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, singleword_dma_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, singleword_dma_active));

	fprintf(out, "[ 63    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, multiword_dma_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, multiword_dma_active));

	fprintf(out, "[ 64    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, advanced_pio_modes));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_byte_64));

	fprintf(out, "[ 65    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_mwx_fer_cycle_time));
	fprintf(out, "[ 66    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, recommended_mwx_fer_cycle_time));
	fprintf(out, "[ 67    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_pio_cycle_time));
	fprintf(out, "[ 68    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_pio_cycle_time_iordy));
	fprintf(out, "[ 69    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[0]));
	fprintf(out, "[ 70    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[1]));
	fprintf(out, "[ 71    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[2]));
	fprintf(out, "[ 72    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[3]));
	fprintf(out, "[ 73    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[4]));
	fprintf(out, "[ 74    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_69[5]));

	fprintf(out, "[ 75    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_75_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, queue_depth));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_75_2));

	fprintf(out, "[ 76    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_76[0]));
	fprintf(out, "[ 77    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_76[1]));
	fprintf(out, "[ 78    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_76[2]));
	fprintf(out, "[ 79    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_words_76[3]));

	fprintf(out, "[ 80    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, major_revision));
	fprintf(out, "[ 81    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minor_revision));

	/* support */

#define IDAT_FIELD_PAIR_CSS(dat,bitvar) \
	#bitvar, \
	dat->command_set_support.bitvar, \
	dat->command_set_active.bitvar, \
	dat->command_set_support.bitvar, \
	dat->command_set_active.bitvar

	fprintf(out, "[ 82- 87] command set support/active (82-84,85-87)\n");

	fprintf(out, "          15 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,obsolete2));
	fprintf(out, "          14 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,nop));
	fprintf(out, "          13 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,read_buffer));
	fprintf(out, "          12 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_buffer));
	fprintf(out, "          11 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,obsolete1));
	fprintf(out, "          10 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,host_protected_area));
	fprintf(out, "           9 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,device_reset));
	fprintf(out, "           8 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,service_interrupt));

	fprintf(out, "           7 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,release_interrupt));
	fprintf(out, "           6 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,look_ahead));
	fprintf(out, "           5 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_cache));
	fprintf(out, "           4 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,no_packet_clear_to_zero));
	fprintf(out, "           3 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,power_management));
	fprintf(out, "           2 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,removable_media_feature));
	fprintf(out, "           1 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,security_mode));
	fprintf(out, "           0 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_commands));

	fprintf(out, "          15 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_86_83_15));
	fprintf(out, "          14 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_86_83_14));
	fprintf(out, "          13 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,flush_cache_ext));
	fprintf(out, "          12 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,flush_cache));
	fprintf(out, "          11 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,device_config_overlay));
	fprintf(out, "          10 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,big_lba));
	fprintf(out, "           9 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,acoustics));
	fprintf(out, "           8 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,set_max));

	fprintf(out, "           7 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_86_83_7));
	fprintf(out, "           6 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,manual_power_up));
	fprintf(out, "           5 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,power_up_in_standby));
	fprintf(out, "           4 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,msn));
	fprintf(out, "           3 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,advanced_pm));
	fprintf(out, "           2 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,cfa));
	fprintf(out, "           1 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,dma_queued));
	fprintf(out, "           0 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,download_microcode));

	fprintf(out, "          15 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,valid_clear_to_zero));
	fprintf(out, "          14 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,valid_set_to_one));
	fprintf(out, "          13 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,idle_with_unload_feature));
	fprintf(out, "          12 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_for_tech_report_87_84_12));
	fprintf(out, "          11 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_for_tech_report_87_84_11));
	fprintf(out, "          10 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,urg_write_stream));
	fprintf(out, "           9 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,urg_read_stream));
	fprintf(out, "           8 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,wwn_64_bit));

	fprintf(out, "           7 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_queued_fua));
	fprintf(out, "           6 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_fua));
	fprintf(out, "           5 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,gp_logging));
	fprintf(out, "           4 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,streaming_feature));
	fprintf(out, "           3 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,media_card_pass_through));
	fprintf(out, "           2 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,media_serial_number));
	fprintf(out, "           1 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_self_test));
	fprintf(out, "           0 %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_error_log));

	fprintf(out, "[ 88    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, ultra_dma_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, ultra_dma_active));

	fprintf(out, "[ 89    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89[0]));
	fprintf(out, "[ 90    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89[1]));
	fprintf(out, "[ 91    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89[2]));
	fprintf(out, "[ 92    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89[3]));

	fprintf(out, "[ 93    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, hardware_reset_result));

	fprintf(out, "[ 94    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, current_acoustic_value));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, recommended_acoustic_value));

	fprintf(out, "[ 95    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95[0]));
	fprintf(out, "[ 96    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95[1]));
	fprintf(out, "[ 97    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95[2]));
	fprintf(out, "[ 98    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95[3]));
	fprintf(out, "[ 99    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95[4]));

	fprintf(out, "[100-101] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, lba48_capacity_lsw));

	fprintf(out, "[102-103] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, lba48_capacity_msw));

	fprintf(out, "[104    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, streaming_transfer_time));
	fprintf(out, "[105    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_105));

	fprintf(out, "[106    ] physical_logical_sector_size\n");
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, cleared_to_zero));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, set_to_one));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, multiple_logical_sectors_per_physical_sector));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, logical_sector_longer_than_256_words));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, reserved_0_2));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, reserved_0_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->physical_logical_sector_size, logical_sectors_per_physical_sector_pwr2));

	fprintf(out, "[107    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, inter_seek_delay));
	fprintf(out, "[108    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[0]));
	fprintf(out, "[109    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[1]));
	fprintf(out, "[110    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[2]));
	fprintf(out, "[111    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[3]));

	fprintf(out, "[112    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[0]));
	fprintf(out, "[113    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[1]));
	fprintf(out, "[114    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[2]));
	fprintf(out, "[115    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[3]));

	fprintf(out, "[116    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_tlc_technical_report));

	fprintf(out, "[117-118] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, words_per_logical_sector));

	fprintf(out, "[119-120] command_set_support_ext/command_set_active_ext\n");

#define IDAT_PAIR_FIELD_CSS_EX(dat,bitvar) \
	#bitvar, \
	dat->command_set_support_ext.bitvar, \
	dat->command_set_active_ext.bitvar, \
	dat->command_set_support_ext.bitvar, \
	dat->command_set_active_ext.bitvar

	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, reserved_1));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, freefall_control));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, segmented_for_download_microcode));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, rw_dma_ext_gpl));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, write_uncorrectable_ext));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, write_read_verify));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, reserved_for_drq_technical_report));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, clear_to_zero));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, set_to_one));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%u,%u)\n", IDAT_PAIR_FIELD_CSS_EX(ident, reserved_0 ));

	fprintf(out, "[121    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[0]));
	fprintf(out, "[122    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[1]));
	fprintf(out, "[123    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[2]));
	fprintf(out, "[124    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[3]));
	fprintf(out, "[125    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[4]));
	fprintf(out, "[126    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_expanded_supportand_active[5]));

	fprintf(out, "[127    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_127_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, msn_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_127_2));

	fprintf(out, "[128    ] security_status\n");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, reserved_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_level));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, enhanced_security_erase_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_count_expired));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_frozen));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_locked));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_enabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_supported));

	fprintf(out, "[129-159] reserved_word_129\n");

	fprintf(out, "[160    ] cfa_power_mode_1\n");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, word_160_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, cfa_power_mode_1_required));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, cfa_power_mode_1_disabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, maximum_current_in_ma_msb));

	fprintf(out, "          %-25s: %04Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, maximum_current_in_ma_lsb));

	fprintf(out, "[161    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[0]));
	fprintf(out, "[162    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[1]));
	fprintf(out, "[163    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[2]));
	fprintf(out, "[164    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[3]));
	fprintf(out, "[165    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[4]));
	fprintf(out, "[166    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[5]));
	fprintf(out, "[167    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[6]));
	fprintf(out, "[168    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[7]));
	fprintf(out, "[169    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[8]));
	fprintf(out, "[170    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[9]));
	fprintf(out, "[171    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[10]));
	fprintf(out, "[172    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[11]));
	fprintf(out, "[173    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[12]));
	fprintf(out, "[174    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[13]));
	fprintf(out, "[175    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[14]));

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->current_media_serial_number, sizeof(ident->current_media_serial_number));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[176-205] %-25s: %s\n", "current_media_serial_number", buf);

	fprintf(out, "[206    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_206));

	fprintf(out, "[207    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_207[0]));
	fprintf(out, "[208    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_207[1]));

	fprintf(out, "[209    ] %-25s\n", "block_alignment");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->block_alignment, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->block_alignment, word_209_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->block_alignment, alignment_of_logical_within_physical_msb));

	fprintf(out, "          %-25s: %04Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->block_alignment, alignment_of_logical_within_physical_lsb));

	fprintf(out, "[210-211] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, write_read_verify_sector_count_mode_3_only));

	fprintf(out, "[212-213] %-25s: %08Xh (%u)\n", IDAT_FIELD_DWORD_DESC(ident, write_read_verify_sector_count_mode_2_only));

	fprintf(out, "[214    ] %-25s\n", "nv_cache_capabilities");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, nv_cache_feature_set_version));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, nv_cache_power_mode_version));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, reserved_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, nv_cache_feature_set_enabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_capabilities, nv_cache_power_mode_enabled));

	fprintf(out, "[215    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, nv_cache_size_lsw));
	fprintf(out, "[216    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, nv_cache_size_msw));
	fprintf(out, "[217    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, nv_cache_read_speed));
	fprintf(out, "[218    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, nv_cache_write_speed));

	fprintf(out, "[219    ] %-25s\n", "nv_cache_options");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_options, nv_cache_estimated_time_to_spin_up_in_sec));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->nv_cache_options, reserved));

	fprintf(out, "[220-254] reserved_word_220\n");

	fprintf(out, "[255    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, signature));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, check_sum));

	return 0;
}

int disp_identify_packet_device_data(
	__in FILE* out,
	__in lsp_ide_identify_packet_device_data_t* ident)
{
	size_t i;
	unsigned char buf[64];

#define IDAT_FIELD_BYTE_DESC(dat, bitvar) \
	#bitvar, dat->bitvar, dat->bitvar

#define IDAT_FIELD_WORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohs(dat->bitvar), lsp_letohs(dat->bitvar)

#define IDAT_FIELD_DWORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohl(dat->bitvar), lsp_letohl(dat->bitvar)

#define IDAT_NESTED_FIELD_WORD_DESC(dat, bitvar) \
	#bitvar, lsp_letohs(dat.bitvar), lsp_letohs(dat.bitvar)

#define IDAT_NESTED_FIELD_BYTE_DESC(dat, bitvar) \
	#bitvar, dat.bitvar, dat.bitvar

	fprintf(out, "[  0    ] general configuration\n");
	
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, atapi_device));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, reserved_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, command_packet_set));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, removable_media));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, drq_response_time));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, reserved_2));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, response_incomplete));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->general_configuration, command_packet_type));

	fprintf(out, "[  1    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_1));
	fprintf(out, "[  2    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_2));
	fprintf(out, "[  3    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_3));
	fprintf(out, "[  4    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_4));
	fprintf(out, "[  5    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_5));
	fprintf(out, "[  6    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_6));
	fprintf(out, "[  7    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_7));
	fprintf(out, "[  8    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_8));
	fprintf(out, "[  9    ] %-25s: %u\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_9));

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->serial_number, sizeof(ident->serial_number));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 10    ] %-25s: %s\n", "serial_number", buf);

	fprintf(out, "[ 20    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_20));
	fprintf(out, "[ 21    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_21));
	fprintf(out, "[ 22    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_22));

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->firmware_revision, sizeof(ident->firmware_revision));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 23    ] %-25s: %s\n", "firmware_revision", buf);

	memset(buf, 0, sizeof(buf));
	memcpy(buf, ident->model_number, sizeof(ident->model_number));
	for (i = 0; i < sizeof(buf); i += 2)
	{
		unsigned short* s = (unsigned short*) &buf[i];
		*s = lsp_byteswap_ushort(*s);
	}
	fprintf(out, "[ 27    ] %-25s: %s\n", "model_number", buf);

	fprintf(out, "[ 47    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_47));
	fprintf(out, "[ 48    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_48));

	fprintf(out, "[ 49- 50] %-25s\n", "capabilities");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, interleaved_dma_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, command_queuing_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, overlapped_operation_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, ata_software_request_required));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, iordy_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, iordy_may_be_disabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, lba_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, dma_supported));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, vendor_specific));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, cleared_to_zero));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, set_to_one));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, reserved_50_13_8 ));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, reserved_50_7_2));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, obsolete_50_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->capabilities, device_specific_standby_timer));

	fprintf(out, "[ 51    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, vendor_specific_word_51_7_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, pio_data_transfer_mode));

	fprintf(out, "[ 52    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_52));

	fprintf(out, "[ 53    ] %-25s: %u\n", "reserved_3_2", ident->reserved_word_53_15_8);
	fprintf(out, "          %-25s: %u\n", "reserved_3_1", ident->reserved_word_53_7_3 );
	fprintf(out, "          %-25s: %u\n", "word_88_valid", ident->word_88_valid);
	fprintf(out, "          %-25s: %u\n", "word_64_70_valid", ident->word_64_70_valid);
	fprintf(out, "          %-25s: %u\n", "word_54_58_valid", ident->word_54_58_valid);

	fprintf(out, "[ 54- 62] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_54_to_62[0]));

	fprintf(out, "[ 63    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, multiword_dma_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, multiword_dma_active));

	fprintf(out, "[ 64    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, advanced_pio_modes));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_byte_64));

	fprintf(out, "[ 65    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_mwx_fer_cycle_time));
	fprintf(out, "[ 66    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, recommended_mwx_fer_cycle_time));
	fprintf(out, "[ 67    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_pio_cycle_time));
	fprintf(out, "[ 68    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minimum_pio_cycle_time_iordy));
	fprintf(out, "[ 69    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_69_to_70[0]));
	fprintf(out, "[ 70    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_69_to_70[1]));
	fprintf(out, "[ 71    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, packet_command_to_bus_release_time));
	fprintf(out, "[ 72    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, service_command_to_busy_clear_time));
	fprintf(out, "[ 73    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_73_to_74[0]));
	fprintf(out, "[ 74    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_73_to_74[1]));

	fprintf(out, "[ 75    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_75_15_8));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_75_7_5));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, queue_depth));

	fprintf(out, "[ 76    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_76_to_79[0]));
	fprintf(out, "[ 77    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_76_to_79[1]));
	fprintf(out, "[ 78    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_76_to_79[2]));
	fprintf(out, "[ 79    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_76_to_79[3]));

	fprintf(out, "[ 80    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, major_revision));
	fprintf(out, "[ 81    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, minor_revision));

	/* support */

#define IDAT_FIELD_PAIR_CSS(dat,bitvar) \
	#bitvar, \
	dat->command_set_support.bitvar, \
	dat->command_set_active.bitvar, \
	dat->command_set_support.bitvar, \
	dat->command_set_active.bitvar

	fprintf(out, "[ 82- 87] command set support/active (82-84,85-87)\n");

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,obsolete2));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,nop));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,read_buffer));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_buffer));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,obsolete1));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,host_protected_area));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,device_reset));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,service_interrupt));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,release_interrupt));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,look_ahead));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_cache));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,packet_command));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,power_management));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,removable_media_feature));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,security_mode));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_commands));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_2));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,manual_power_up));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,power_up_in_standby));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,msn));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,advanced_pm));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,cfa));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,dma_queued));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,download_microcode));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_3));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,flush_cache_ext));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,flush_cache));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,device_config_overlay));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,big_lba));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,acoustics));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,set_max));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_queued_fua));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,write_fua));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,gp_logging));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,streaming_feature));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,media_card_pass_through));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,media_serial_number));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_self_test));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,smart_error_log));

	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_4));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,idle_with_unload_feature));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,reserved_for_tech_report));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,urg_write_stream));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,urg_read_stream));
	fprintf(out, "          %-25s: %02Xh, %02Xh (%d, %d)\n", IDAT_FIELD_PAIR_CSS(ident,wwn_64_bit));

	fprintf(out, "[ 88    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, ultra_dma_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, ultra_dma_active));

	fprintf(out, "[ 89    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89_to_92[0]));
	fprintf(out, "[ 90    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89_to_92[1]));
	fprintf(out, "[ 91    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89_to_92[2]));
	fprintf(out, "[ 92    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_89_to_92[3]));

	fprintf(out, "[ 93    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, hardware_reset_result));

	fprintf(out, "[ 94    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, current_acoustic_value));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, recommended_acoustic_value));

	fprintf(out, "[ 95-107] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_95_to_107[0]));

	fprintf(out, "[108    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[0]));
	fprintf(out, "[109    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[1]));
	fprintf(out, "[110    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[2]));
	fprintf(out, "[111    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, world_wide_name[3]));

	fprintf(out, "[112    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[0]));
	fprintf(out, "[113    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[1]));
	fprintf(out, "[114    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[2]));
	fprintf(out, "[115    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_world_wide_name128[3]));

	fprintf(out, "[116-124] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_116_to_124[0]));

	fprintf(out, "[125    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, atapi_zero_byte_count_behavior));
	fprintf(out, "[126    ] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, obsolete_126));

	fprintf(out, "[127    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_127_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, msn_support));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, reserved_word_127_2));

	fprintf(out, "[128    ] security_status\n");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, reserved_1));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_level));

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, enhanced_security_erase_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_count_expired));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_frozen));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_locked));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_enabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->security_status, security_supported));

	fprintf(out, "[129-159] reserved_word_129_to_159\n");

	fprintf(out, "[160    ] cfa_power_mode_1\n");

	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, word_160_supported));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, reserved_0));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, cfa_power_mode_1_required));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, cfa_power_mode_1_disabled));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, maximum_current_in_ma_msb));

	fprintf(out, "          %-25s: %04Xh (%u)\n", IDAT_NESTED_FIELD_BYTE_DESC(ident->cfa_power_mode_1, maximum_current_in_ma_lsb));

	fprintf(out, "[161-175] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_for_cfa[0]));

	fprintf(out, "[176-254] %-25s: %04Xh (%u)\n", IDAT_FIELD_WORD_DESC(ident, reserved_word_176_to_254[0]));

	fprintf(out, "[255    ] %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, signature));
	fprintf(out, "          %-25s: %02Xh (%u)\n", IDAT_FIELD_BYTE_DESC(ident, check_sum));

	return 0;
}

int 
lspsh_ata_identify(
	lsptest_context_t* context)
{
	int ret, i;
	lsp_ide_identify_device_data_t ident_buf;
	lsp_ide_identify_device_data_t* ident;

	FILE* out = stdout;

	ident = &ident_buf;
	context->lsp_status = lsp_ide_identify(
		context->lsp_handle, ident);
	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	{
		lsp_uint64_t cap_sects;
		lsp_uint64_t cap_bytes;
		lsp_uint64_t cap_kb;
		lsp_uint64_t cap_mb;
		lsp_uint64_t cap_gb;

		/* 48-bit address feature set is available and enabled? */
		if (ident->command_set_support.big_lba && ident->command_set_active.big_lba)
		{
			cap_sects = ((lsp_uint64_t) lsp_letohl(ident->lba48_capacity_lsw)) + 
				((lsp_uint64_t) lsp_letohl(ident->lba48_capacity_msw) << 32);
		}
		else
		{
			cap_sects = (lsp_uint64_t) lsp_letohl(ident->lba28_capacity);
		}
		cap_bytes = cap_sects << 9; /* * 512 */
		cap_kb = cap_bytes >> 10; /* / 1024; */
		cap_mb = cap_kb >> 10; /* / 1024; */
		cap_gb = cap_mb >> 10; /* / 1024; */
#ifdef _MSC_VER
		fprintf(out, "* Total sectors: %I64u (%I64Xh) ", cap_sects, cap_sects);
#else
		fprintf(out, "* Total sectors: %llu (%llXh) ", cap_sects, cap_sects);
#endif
		fprintf(out, "%llu KB, ", cap_kb);
		fprintf(out, "%llu MB, ", cap_mb);
		fprintf(out, "%llu GB\n", cap_gb);
	}

	fprintf(out, "* Singleword DMA modes: support=%02Xh active=%02Xh\n", 
		ident->singleword_dma_support,
		ident->singleword_dma_active);

	fprintf(out, "* Multiword DMA modes: support=%02Xh active=%02Xh\n", 
		ident->multiword_dma_support,
		ident->multiword_dma_active);

	fprintf(out, "* Ultra DMA modes: support=%02Xh active=%02Xh\n", 
		ident->ultra_dma_support,
		ident->ultra_dma_active);

	fprintf(out, "* PIO modes: support=%02Xh\n",
		ident->advanced_pio_modes);

	{
		int i;

		fprintf(out, "* Current DMA transfer mode: ");
		for (i = 0; i < 8; ++i)
		{
			if (ident->singleword_dma_active == (1 << i))
			{
				fprintf(out, "Singleword DMA %d", i);
				break;
			}
		}
		for (i = 0; i < 8; ++i)
		{
			if (ident->multiword_dma_active == (1 << i))
			{
				fprintf(out, "Multiword DMA %d", i);
				break;
			}
		}
		for (i = 0; i < 8; ++i)
		{
			if (ident->ultra_dma_active == (1 << i))
			{
				fprintf(out, "Ultra DMA %d", i);
				break;
			}
		}
		fprintf(out, "\n");
	}

	fprintf(out, "* Major Rev: %04X (%u) ", 
		lsp_letohs(ident->major_revision), 
		lsp_letohs(ident->major_revision));

	fputs_word_bits(lsp_letohs(ident->major_revision), out);

	fputs("\n", out);

	fprintf(out, "* Minor Rev: %04X (%u) ", 
		lsp_letohs(ident->minor_revision), 
		lsp_letohs(ident->minor_revision));

	fputs_word_bits(lsp_letohs(ident->minor_revision), out);

	fputs("\n", out);

	fputs_ata_version(
		lsp_letohs(ident->major_revision), 
		lsp_letohs(ident->minor_revision),
		out);

	disp_identify_device_data(stdout, ident);

	return 0;
}

int 
lspsh_ata_identify_packet_device(
	lsptest_context_t* context)
{
	int ret, i;
	lsp_ide_identify_packet_device_data_t ident_buf;
	lsp_ide_identify_packet_device_data_t* ident;

	FILE* out = stdout;

	ident = &ident_buf;
	context->lsp_status = lsp_ide_identify_packet_device(
		context->lsp_handle, ident);
	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	fprintf(out, "* Multiword DMA modes: support=%02Xh active=%02Xh\n", 
		ident->multiword_dma_support,
		ident->multiword_dma_active);

	fprintf(out, "* Ultra DMA modes: support=%02Xh active=%02Xh\n", 
		ident->ultra_dma_support,
		ident->ultra_dma_active);

	fprintf(out, "* PIO modes: support=%02Xh\n",
		ident->advanced_pio_modes);

	{
		int i;

		fprintf(out, "* Current transfer mode: ");
		for (i = 0; i < 8; ++i)
		{
			if (ident->multiword_dma_active == 1 << i)
			{
				fprintf(out, "Multiword DMA %d", i);
				break;
			}
		}
		for (i = 0; i < 8; ++i)
		{
			if (ident->ultra_dma_active == 1 << i)
			{
				fprintf(out, "Ultra DMA %d", i);
				break;
			}
		}
		fprintf(out, "\n");
	}

	fprintf(out, "* Major Rev: %04X (%u) ", 
		lsp_letohs(ident->major_revision), 
		lsp_letohs(ident->major_revision));

	fputs_word_bits(lsp_letohs(ident->major_revision), out);

	fputs("\n", out);

	fprintf(out, "* Minor Rev: %04X (%u) ", 
		lsp_letohs(ident->minor_revision), 
		lsp_letohs(ident->minor_revision));

	fputs_word_bits(lsp_letohs(ident->minor_revision), out);

	fputs("\n", out);

	fputs_ata_version(
		lsp_letohs(ident->major_revision), 
		lsp_letohs(ident->minor_revision),
		out);

	disp_identify_packet_device_data(stdout, ident);

	return 0;
}

int 
lsptest_read(
	lsptest_context_t* context,
	lsp_large_integer_t* location,
	FILE* out,
	size_t sectors,
	lspsh_read_mode_t parse_mode)
{
	int written;
	int ret;
	size_t bufblks, rblocks, cblocks;
	lsp_uint8_t* buf;
	const lsp_hardware_data_t* hwdata;
	lsp_large_integer_t lba, elba;
	size_t sector_size = 512;
	int i;

	hwdata = lsp_get_hardware_data(context->lsp_handle);
	bufblks = min(hwdata->maximum_transfer_blocks, sectors);

	buf = calloc(bufblks, sector_size);
	if (NULL == buf)
	{
		fprintf(stderr, "error: memory allocation failed, bytes=%d\n", 
			bufblks * sector_size);
		return 2;
	}

	rblocks = sectors;
	lba = *location;

	fprintf(stderr, "reading 0x%I64X-0x%I64X 0x%X(%d) sectors, split into %d requests\n", 
		lba.quad, 
		lba.quad + sectors - 1,
		sectors, sectors,
		sectors/bufblks);

	i = 0;

	while (rblocks > 0)
	{
		cblocks = min(bufblks, rblocks);
		elba.quad = lba.quad + cblocks - 1;

		fprintf(stderr, 
#if defined(_MSC_VER)
			"reading (%d:%d/%d): LBA 0x%I64X-0x%I64X (%d sectors)...\n",
#else
			"reading (%d:%d/%d): LBA 0x%llX-0x%llX (%d sectors)...\n",
#endif
			++i, sectors - rblocks, sectors, lba.quad, elba.quad, cblocks);

		context->lsp_status = lsp_ide_read(
			context->lsp_handle, &lba, cblocks, buf, cblocks * sector_size);
		ret = lsptest_transport_process_transfer(context);
		ret = lsptest_check_lsp_status(context, ret);

		if (0 != ret)
		{
			free(buf);
			return ret;
		}

		assert(rblocks >= cblocks);

		rblocks -= cblocks;
		lba.quad += cblocks;

		switch (parse_mode)
		{
		case lspsh_read_text:
			disp_bytes(out, buf, cblocks * sector_size);
			break;
		case lspsh_read_bin:
			written = fwrite(buf,  sector_size, cblocks, out);
			if (written < (int) cblocks)
			{
				fprintf(stderr, "error: writing failed\n");
				free(buf);
				return 3;
			}
			break;
		case lspsh_read_mbr:
			ndavs_disp_mbr(buf);
			break;
		}
	}

	fprintf(stderr, "done.\n");

	free(buf);
	return 0;
}

int 
lsptest_write(
	lsptest_context_t* context,
	lsp_large_integer_t* location,
	FILE* in,
	size_t sectors)
{
	int written;
	size_t rc;
	int ret;
	lsp_uint8_t* buf;
	size_t bufblks, rblocks, cblocks;

	const lsp_hardware_data_t* hwdata;
	lsp_large_integer_t lba, elba;
	size_t sector_size = 512;
	int i;

	hwdata = lsp_get_hardware_data(context->lsp_handle);
	bufblks = min(hwdata->maximum_transfer_blocks, sectors);

	buf = calloc(bufblks, sector_size);
	if (NULL == buf)
	{
		fprintf(stderr, "error: memory allocation failed, bytes=%d\n", 
			bufblks * sector_size);
		return 2;
	}

	rblocks = sectors;
	lba = *location;

	fprintf(stderr, "writing 0x%I64X-0x%I64X 0x%X(%d) sectors, split into %d requests\n", 
		lba.quad, 
		lba.quad + sectors - 1,
		sectors, sectors,
		sectors/bufblks);

	i = 0;

	while (rblocks > 0)
	{
		cblocks = min(bufblks, rblocks);
		elba.quad = lba.quad + cblocks - 1;

		fprintf(stderr, 
#if defined(_MSC_VER)
			"writing (%d:%d/%d): LBA 0x%I64X-0x%I64X (%d sectors)...\n",
#else
			"writing (%d:%d/%d): LBA 0x%llX-0x%llX (%d sectors)...\n",
#endif
			++i, sectors - rblocks, sectors, lba.quad, elba.quad, cblocks);

		if (NULL != in)
		{
			rc = fread(buf, sector_size, cblocks, in);
			if (rc < cblocks)
			{
				fprintf(stderr, "error: reading failed\n");
				free(buf);
				return 2;
			}
		}

		context->lsp_status = lsp_ide_write(
			context->lsp_handle, &lba, cblocks, buf, cblocks * sector_size);
		ret = lsptest_transport_process_transfer(context);
		ret = lsptest_check_lsp_status(context, ret);

		if (0 != ret)
		{
			free(buf);
			return ret;
		}

		assert(rblocks >= cblocks);

		rblocks -= cblocks;
		lba.quad += cblocks;
	}

	fprintf(stderr, "done.\n");

	free(buf);
	return 0;
}

int 
lspsh_ata_p_non_data_command(
	lsptest_context_t* context,
	lsp_ide_register_param_t* reg_in,
	lsp_ide_register_param_t* reg_out)
{
	int ret;

	if (lsptest_show_idereg_in) disp_ide_register(stdout, reg_in);

	context->lsp_status = lsp_ide_command(context->lsp_handle, reg_in, 0, 0);
	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	context->lsp_status = lsp_get_ide_command_output_register(
		context->lsp_handle, reg_out);
	if (LSP_STATUS_SUCCESS != context->lsp_status)
	{
		fprintf(stderr, "lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			context->lsp_status, context->lsp_status);
	}

	if (reg_out->command.status.err)
	{
		fprintf(stderr, "ATA error register is set.\n");
	}

	if (lsptest_show_idereg_out) disp_ide_register(stdout, reg_out);

	return 0;
}

int 
lsptest_read_native_max_address(
	lsptest_context_t* context)
{
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	int ret;

	reg_in.device.s.lba = 1;
	reg_in.command.command = LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	if (0 == ret)
	{
		lsp_uint32_t native_lba;

		if (reg_out.command.status.err)
		{
			fprintf(stdout, "ATA command returned error.\n");
			if (reg_out.reg.ret.err.err_op.abrt)
			{
				fprintf(stdout, "ABRT: command is not supported or cannot be able to complete the command.\n");
			}
		}
		else
		{
			native_lba = 0;
			native_lba |= 0x000000FF & (lsp_uint32_t) reg_out.reg.named.lba_low;
			native_lba |= 0x0000FF00 & ((lsp_uint32_t) reg_out.reg.named.lba_mid << 8);
			native_lba |= 0x00FF0000 & ((lsp_uint32_t) reg_out.reg.named.lba_high << 16);
			native_lba |= 0xFF000000 & ((lsp_uint32_t) (reg_out.device.device & 0x0F) << 24);

			if (native_lba == 0x00FFFFFE)
			{
				fprintf(stdout, "48-bit address feature set is required.\n");
			}
			else
			{
				disp_dword_bits(native_lba);

				fprintf(stdout, "%u sectors\n", native_lba);
				fprintf(stdout, "%u bytes\n", native_lba * 512);
				fprintf(stdout, "%u KB\n", native_lba / 1024 * 512);
				fprintf(stdout, "%u MB\n", native_lba * 512 / 1024 / 1024);
				fprintf(stdout, "%u GB\n", native_lba * 512 / 1024 / 1024 / 1024);
			}
		}
	}

	return ret;
}

int 
lsptest_read_native_max_address_ext(
	lsptest_context_t* context)
{
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_status_t lsp_status;
	int ret;

	reg_in.device.s.lba = 1;
	reg_in.command.command = LSP_IDE_CMD_READ_NATIVE_MAX_ADDRESS_EXT;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	if (0 != ret) return ret;

	if (0 == ret)
	{
		lsp_uint64_t native_lba;

		native_lba = 0;
		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.cur.lba_low);
		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.cur.lba_mid) << 8;
		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.cur.lba_high) << 16;

		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.prev.lba_low) << 24;
		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.prev.lba_mid) << 32;
		native_lba += ((lsp_uint64_t) reg_out.reg.named_48.prev.lba_high) << 40;

		disp_qword_bits(native_lba);

		fprintf(stdout, "%llu sectors\n", native_lba);
		fprintf(stdout, "%llu bytes\n", native_lba * 512ULL);
		fprintf(stdout, "%llu KB\n", native_lba / 1024ULL * 512ULL);
		fprintf(stdout, "%llu MB\n", native_lba * 512ULL / 1024ULL / 1024ULL);
		fprintf(stdout, "%llu GB\n", native_lba * 512ULL / 1024ULL / 1024ULL / 1024ULL);
	}

	return ret;
}

int lspsh_ata_idle(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_IDLE;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_idle_immediate(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_IDLE_IMMEDIATE;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_flush_cache(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_FLUSH_CACHE;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_flush_cache_ext(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_FLUSH_CACHE_EXT;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_check_power_mode(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_CHECK_POWER_MODE;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	/*
	Value Description
	00h Device is in Standby mode.
	40h Device is in NV Cache Power Mode and the spindle is spun down or
	spinning down.
	41h device is in NV Cache Power Mode and the spindle is spun up or
	spinning up.
	80h Device is in Idle mode.
	FFh Device is in Active mode or Idle mode.
	*/

	if (0 != ret) return ret;

	switch (reg_out.reg.named.sector_count)
	{
	case 0x00: fprintf(stdout, "Device is in Standby mode.\n"); break;
	case 0x40: fprintf(stdout, "Device is in NV Cache Power Mode and the spindle is spun down or spinning down.\n"); break;
	case 0x41: fprintf(stdout, "Device is in NV Cache Power Mode and the spindle is spun up or spinning up.\n"); break;
	case 0x80: fprintf(stdout, "Device is in Idle mode.\n"); break;
	case 0xFF: fprintf(stdout, "Device is in Active mode or Idle mode.\n"); break;
	default: fprintf(stdout, "Device is in unknown mode (%02Xh).\n", reg_out.reg.named.sector_count); break;
	}

	return ret;

}

int lspsh_ata_device_reset(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_DEVICE_RESET;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_execute_device_diagnostic(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_EXECUTE_DEVICE_DIAGNOSTIC;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_sleep(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_SLEEP;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_standby(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_STANDBY;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_ata_standby_immediate(lsptest_context_t* context)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.command.command = LSP_IDE_CMD_STANDBY_IMMEDIATE;
	reg_in.device.device = 0;

	ret = lspsh_ata_p_non_data_command(context, &reg_in, &reg_out);

	return ret;
}

int lspsh_text_target_list(lsptest_context_t* context)
{
	lsp_text_target_list_t list;
	int i, ret;

	memset(&list, 0, sizeof(list));
	list.type = LSP_TEXT_BINPARAM_TYPE_TARGET_LIST;

	context->lsp_status = lsp_text_command(
		context->lsp_handle, 
		0x01, /* text_type_binary */
		0x00, /* text_version */
		&list,
		sizeof(list),
		&list,
		sizeof(list));

	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	fprintf(stdout, "type=%02X, elements=%d\n", 
		list.type, list.number_of_elements);

	for (i = 0; i < list.number_of_elements; ++i)
	{
		const lsp_text_target_list_element_t* element;
		element = &list.elements[i];

		fprintf(stdout, " - target=%d, rw_hosts=%d, ro_hosts=%d, data=",
			element->target_id,
			element->rw_hosts,
			element->ro_hosts);

		for (i = 0; i < sizeof(element->target_data) - 1; ++i)
		{
			fprintf(stdout, "%02X ", element->target_data[i]);
		}
		fprintf(stdout, "%02X\n", element->target_data[i]);
	}

	return 0;
}

int lspsh_text_target_data(
	__in lsptest_context_t* context,
	__in int to_set,
	__in_bcount(8) const unsigned char* data)
{
	lsp_text_target_data_t tdata;
	int i, ret;
	const lsp_login_info_t *login_info;

	memset(&tdata, 0, sizeof(tdata));
	tdata.type = LSP_TEXT_BINPARAM_TYPE_TARGET_DATA;
	tdata.to_set = (lsp_uint8_t) to_set;
	memcpy(tdata.target_data, data, 8);

	login_info = lsp_get_login_info(context->lsp_handle);
	tdata.target_id = login_info->unit_no;

	context->lsp_status = lsp_text_command(
		context->lsp_handle, 
		0x01, /* text_type_binary */
		0x00, /* text_version */
		&tdata,
		sizeof(tdata),
		&tdata,
		sizeof(tdata));

	ret = lsptest_transport_process_transfer(context);
	ret = lsptest_check_lsp_status(context, ret);
	if (ret != 0)
	{
		return ret;
	}

	fprintf(stdout, "target_id=%d, target_data=", tdata.target_id);
	for (i = 0; i < sizeof(tdata.target_data) - 1; ++i)
	{
		fprintf(stdout, "%02X ", tdata.target_data[i]);
	}
	fprintf(stdout, "%02X\n", tdata.target_data[i]);

	return 0;
}


