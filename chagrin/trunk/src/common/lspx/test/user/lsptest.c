#include <windows.h>
#include <stdio.h>
#include <crtdbg.h>
#include <strsafe.h>
#include <winsock2.h>
#include <strsafe.h>

#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include <socketlpx.h>

/* Send coalescing is not supported in current NDAS devices */
/* #define LSPTEST_USE_COALESCING */ 

#ifndef countof
#define countof(A) (sizeof(A)/sizeof((A)[0]))
#endif

struct lpx_addr {
	u_char node[6];
	char _reserved_[10];
};

struct sockaddr_lpx {
	short           sin_family;
	u_short	        port;
	struct lpx_addr slpx_addr;
};

void dprintfA(char* format, ...)
{
	static char buf[256];
	va_list ap;
	va_start(ap, format);
#if 0
	StringCchVPrintfA(buf, 256, format, ap);
	OutputDebugString(buf);
#else
	vprintf(format, ap);
#endif
	va_end(ap);
}

#define dprintf dprintfA

typedef struct _lsp_socket_context_t lsp_socket_context_t;

typedef struct _lsp_transfer_context_t {
	lsp_socket_context_t* socket_context;
	DWORD index;
	WSABUF wsabuf[2];
	WSAOVERLAPPED overlapped;
	DWORD error;
	DWORD txbytes;
	DWORD flags;
} lsp_transfer_context_t;

typedef struct _lsp_socket_context_t {
	SOCKET socket;
	lsp_handle_t lsp_handle;
	lsp_status_t lsp_status;
	int next_transfer_index;
	lsp_transfer_context_t lsp_transfer_context[LSP_MAX_CONCURRENT_TRANSFER];
	LONG outstanding_transfers;
#ifdef LSPTEST_USE_COALESCING
	lsp_transfer_context_t* deferred_txcontext;
#endif
} lsp_socket_context_t;

int
lsp_socket_context_init(lsp_socket_context_t* scontext, SOCKET socket, lsp_handle_t lsp_handle)
{
	int i;
	
	ZeroMemory(scontext, sizeof(lsp_socket_context_t));
	scontext->socket = socket;
	scontext->lsp_handle = lsp_handle;

	for (i = 0; i < LSP_MAX_CONCURRENT_TRANSFER; ++i)
	{
		lsp_transfer_context_t* txcontext = &scontext->lsp_transfer_context[i];
		txcontext->socket_context = scontext;
		txcontext->index = i;
	}
	return NO_ERROR;
}

void
lsp_socket_context_destroy(lsp_socket_context_t* scontext)
{
}

void 
CALLBACK 
lsp_transfer_completion(
	IN DWORD error,
	IN DWORD txbytes,
	IN LPWSAOVERLAPPED overlapped,
	IN DWORD flags)
{
	lsp_transfer_context_t* txcontext = CONTAINING_RECORD(
		overlapped, 
		lsp_transfer_context_t, 
		overlapped);

	lsp_socket_context_t* scontext = txcontext->socket_context;

	txcontext->error = error;
	txcontext->txbytes = txbytes;
	txcontext->flags = flags;

	if (error != ERROR_SUCCESS)
	{
		dprintf("T[%d]%d ERR=%X,FLAGS=%X ", txcontext->index, txbytes, error, flags);
	}
	else
	{
		// dprintf("T[%d]%d ", txcontext->index, txbytes);
		dprintf("[%d] Transferred %d bytes\n", txcontext->index, txbytes);
	}

	InterlockedDecrement(&scontext->outstanding_transfers);
}

int
process_lsp_transfer(
	lsp_socket_context_t* scontext)
{
	int i;
	int ret;

	lsp_status_t lsp_status = scontext->lsp_status;

	scontext->next_transfer_index = 0;
	scontext->outstanding_transfers = 0;

#ifdef LSPTEST_USE_COALESCING
	scontext->deferred_txcontext = NULL;
#endif

	while (TRUE)
	{
#ifdef LSPTEST_USE_COALESCING
		if (LSP_REQUIRES_SEND != lsp_status && scontext->deferred_txcontext)
		{
			lsp_transfer_context_t* txcontext = scontext->deferred_txcontext;
			DWORD txlen;
			
			dprintf("[%d] Sending deferred %d bytes\n", 
				txcontext->index,
				txcontext->wsabuf[0].len);

			scontext->deferred_txcontext = NULL;

			InterlockedIncrement(&scontext->outstanding_transfers);
			ret = WSASend(
				scontext->socket, 
				txcontext->wsabuf,
				txcontext->wsabuf[1].len > 0 ? 2 : 1, 
				&txlen, 
				0, 
				&txcontext->overlapped, 
				lsp_transfer_completion);

			if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
			{
				dprintf("WSASend failed with error %d\n", WSAGetLastError());
				return SOCKET_ERROR;
			}
			if (0 == ret)
			{
				dprintf("Sent %d bytes already\n", txlen);
			}
		}
#endif
		if (LSP_REQUIRES_SEND == lsp_status)
		{
			DWORD txlen;
			DWORD txindex = scontext->next_transfer_index++;
			lsp_transfer_context_t* txcontext = &scontext->lsp_transfer_context[txindex];
			LPWSAOVERLAPPED overlapped = &txcontext->overlapped;
#ifdef LSPTEST_USE_EVENT
			WSAEVENT event = overlapped->hEvent;
#endif
			ZeroMemory(&txcontext->overlapped, sizeof(WSAOVERLAPPED));
#ifdef LSPTEST_USE_EVENT
			txcontext->overlapped.hEvent = event;
#endif

#ifdef LSPTEST_USE_COALESCING
			if (NULL == scontext->deferred_txcontext)
			{
				txcontext->wsabuf[0].buf = (char*) lsp_get_buffer_to_send(
					scontext->lsp_handle, 
					&txcontext->wsabuf[0].len);
				scontext->deferred_txcontext = txcontext;
			}
			else
			{
				lsp_transfer_context_t* dtxcontext = scontext->deferred_txcontext;
				dtxcontext->wsabuf[1].buf = (char*) lsp_get_buffer_to_send(
					scontext->lsp_handle,
					&dtxcontext->wsabuf[1].len);
				scontext->deferred_txcontext = NULL;

				dprintf("[%d] Sending deferred %d + %d bytes\n", 
					dtxcontext->index,
					dtxcontext->wsabuf[0].len,
					dtxcontext->wsabuf[1].len);

				InterlockedIncrement(&scontext->outstanding_transfers);
				ret = WSASend(
					scontext->socket, 
					dtxcontext->wsabuf,
					2, 
					&txlen, 
					0, 
					&dtxcontext->overlapped, 
					lsp_transfer_completion);

				if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
				{
					dprintf("WSASend failed with error %d\n", WSAGetLastError());
					return SOCKET_ERROR;
				}
				if (0 == ret)
				{
					dprintf("Sent %d bytes already\n", txlen);
				}
			}
#else
			txcontext->wsabuf[0].buf = (char*) lsp_get_buffer_to_send(
				scontext->lsp_handle, 
				&txcontext->wsabuf[0].len);

			dprintf("[%d] Sending %d bytes\n", 
				txcontext->index, 
				txcontext->wsabuf[0].len);

			InterlockedIncrement(&scontext->outstanding_transfers);

			ret = WSASend(
				scontext->socket, 
				&txcontext->wsabuf[0],
				1, 
				&txlen, 
				0, 
				&txcontext->overlapped, 
				lsp_transfer_completion);

			if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
			{
				dprintf("WSASend failed with error %d\n", WSAGetLastError());
				return SOCKET_ERROR;
			}
			if (0 == ret)
			{
				dprintf("Sent %d bytes already\n", txlen);
			}
#endif
		}
		else if (LSP_REQUIRES_RECEIVE == lsp_status)
		{
			DWORD txlen, txflags = 0;
			DWORD txindex = scontext->next_transfer_index++;
			lsp_transfer_context_t* txcontext = &scontext->lsp_transfer_context[txindex];
			LPWSAOVERLAPPED overlapped = &txcontext->overlapped;

			ZeroMemory(&txcontext->overlapped, sizeof(WSAOVERLAPPED));
			txcontext->wsabuf[0].buf = (char*) lsp_get_buffer_to_receive(
				scontext->lsp_handle, 
				&txcontext->wsabuf[0].len);

			InterlockedIncrement(&scontext->outstanding_transfers);

			dprintf("[%d] Receiving %d bytes\n", 
				txcontext->index, 
				txcontext->wsabuf[0].len);

			ret = WSARecv(
				scontext->socket, 
				&txcontext->wsabuf[0],
				1, 
				&txlen, 
				&txflags,
				&txcontext->overlapped, 
				lsp_transfer_completion);

			if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
			{
				dprintf("WSARecv failed with error %d\n", WSAGetLastError());
				return SOCKET_ERROR;
			}
			if (0 == ret)
			{
				dprintf("Received %d bytes already\n", txlen);
			}
		}
		else if (LSP_REQUIRES_SYNCHRONIZE == lsp_status)
		{
			while (scontext->outstanding_transfers > 0)
			{
				DWORD waitResult = SleepEx(INFINITE, TRUE);
				_ASSERTE(waitResult == WAIT_IO_COMPLETION);
			}

			scontext->next_transfer_index = 0;
		}
		else
		{
			dprintf("LSP:0x%x\n", lsp_status, lsp_status);
			scontext->lsp_status = lsp_status;
			break;
		}
		lsp_status = lsp_process_next(scontext->lsp_handle);
	}

	return 0;
}

void printbytes(char* buffer, size_t len)
{
	size_t i, j;

	printf("00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
	for (i = 0; i < len; i += 16)
	{
		if ((i%512) == 0)
		{
			printf("-----------------------------------------------\n");
		}
		for (j = 0; j < 16 && (i + j < len); ++j)
		{
			printf("%02X ", (unsigned char) buffer[i+j]);
		}
		for (j = 0; j < 16 && (i + j < len); ++j)
		{
			unsigned char c = buffer[i+j];
			if (!isprint(c)) c = '.';
			printf("%c", c);
		}
		printf("\n");
	}
}

static const char* command_set_1_desc[] = {
	/* 15 */ "Obsolete",
	/* 14 */ "NOP command",
	/* 13 */ "READ_BUFFER",
	/* 12 */ "WRITE_BUFFER",
	/* 11 */ "Obsolete",
	/* 10 */ "Host Protected Area",
	/*  9 */ "DEVICE Reset",
	/*  8 */ "SERVICE Interrupt",
	/*  7 */ "Release Interrupt",
	/*  6 */ "look-ahead",
	/*  5 */ "write cache",
	/*  4 */ "PACKET Command",
	/*  3 */ "Power Management Feature Set",
	/*  2 */ "Removable Feature Set",
	/*  1 */ "Security Feature Set",
	/*  0 */ "SMART Feature Set"
};

static const char* command_set_2_desc[] = {
	/* 15 */ "Shall be ZERO",
	/* 14 */ "Shall be ONE",
	/* 13 */ "FLUSH CACHE EXT",
	/* 12 */ "FLUSH CACHE",
	/* 11 */ "Device Configuration Overlay",
	/* 10 */ "48-bit Address Feature Set",
	/*  9 */ "Automatic Acoustic Management",
	/*  8 */ "SET MAX security",
	/*  7 */ "reserved 1407DT PARTIES",
	/*  6 */ "SetF sub-command Power-Up",
	/*  5 */ "Power-Up in Standby Feature Set",
	/*  4 */ "Removable Media Notification",
	/*  3 */ "APM Feature Set",
	/*  2 */ "CFA Feature Set",
	/*  1 */ "READ/WRITE DMA QUEUED",
	/*  0 */ "Download MicroCode"
};

static const char* cfsse_desc[] = {
	/* 15 */ "Shall be ZERO",
	/* 14 */ "Shall be ONE",
	/* 13 */ "reserved",
	/* 12 */ "reserved",
	/* 11 */ "reserved",
	/* 10 */ "reserved",
	/*  9 */ "reserved",
	/*  8 */ "reserved",
	/*  7 */ "reserved",
	/*  6 */ "reserved",
	/*  5 */ "General Purpose Logging",
	/*  4 */ "Streaming Feature Set",
	/*  3 */ "Media Card Pass Through",
	/*  2 */ "Media Serial Number Valid",
	/*  1 */ "SMART self-test supported",
	/*  0 */ "SMART error logging"
};

static const char* cfs_enable_1_desc[] = {
	/* 15 */ "Obsolete",
	/* 14 */ "NOP command",
	/* 13 */ "READ_BUFFER",
	/* 12 */ "WRITE_BUFFER",
	/* 11 */ "Obsolete",
	/* 10 */ "Host Protected Area",
	/*  9 */ "DEVICE Reset",
	/*  8 */ "SERVICE Interrupt",
	/*  7 */ "Release Interrupt",
	/*  6 */ "look-ahead",
	/*  5 */ "write cache",
	/*  4 */ "PACKET Command",
	/*  3 */ "Power Management Feature Set",
	/*  2 */ "Removable Feature Set",
	/*  1 */ "Security Feature Set",
	/*  0 */ "SMART Feature Set"
};

static const char* cfs_enable_2_desc[] = {
	/* 15 */ "Shall be ZERO",
	/* 14 */ "Shall be ONE",
	/* 13 */ "FLUSH CACHE EXT",
	/* 12 */ "FLUSH CACHE",
	/* 11 */ "Device Configuration Overlay",
	/* 10 */ "48-bit Address Feature Set",
	/*  9 */ "Automatic Acoustic Management",
	/*  8 */ "SET MAX security",
	/*  7 */ "reserved 1407DT PARTIES",
	/*  6 */ "SetF sub-command Power-Up",
	/*  5 */ "Power-Up in Standby Feature Set",
	/*  4 */ "Removable Media Notification",
	/*  3 */ "APM Feature Set",
	/*  2 */ "CFA Feature Set",
	/*  1 */ "READ/WRITE DMA QUEUED",
	/*  0 */ "Download MicroCode"
};

static const char* csf_default_desc[] = {
	/* 15 */ "Shall be ZERO",
	/* 14 */ "Shall be ONE",
	/* 13 */ "reserved",
	/* 12 */ "reserved",
	/* 11 */ "reserved",
	/* 10 */ "reserved",
	/*  9 */ "reserved",
	/*  8 */ "reserved",
	/*  7 */ "reserved",
	/*  6 */ "reserved",
	/*  5 */ "General Purpose Logging enabled",
	/*  4 */ "Valid CONFIGURE STREAM executed",
	/*  3 */ "Media Card Pass Through enabled",
	/*  2 */ "Media Serial Number Valid",
	/*  1 */ "SMART self-test supported",
	/*  0 */ "SMART error logging"
};

char* string_byte_bits(__in BYTE bits)
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

char* string_word_bits(__in WORD bits)
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

void print_byte_bits(__in BYTE bits)
{
	puts(string_byte_bits(bits));
}

void print_word_bits(__in WORD bits)
{
	puts(string_word_bits(bits));
}

void print_word_bits_desc(__in WORD bits, __in_ecount(16) const char** desctable)
{
	int i;
	for (i = 0; i < 16; ++i)
	{
		printf(" [%c] %s\n", (bits << i) & 0x8000 ? 'X' : ' ', desctable[i]);
	}
}

void print_ide_register(const lsp_ide_register_param_t* idereg)
{
	printf("Feature/Error : %02X %s", idereg->reg.named_48.cur.features, string_byte_bits(idereg->reg.named_48.cur.features));
	printf(" %02X %s\n", idereg->reg.named_48.prev.features, string_byte_bits(idereg->reg.named_48.prev.features));
	printf("Sector Count  : %02X %s", idereg->reg.named_48.cur.sector_count, string_byte_bits(idereg->reg.named_48.cur.sector_count));
	printf(" %02X %s\n", idereg->reg.named_48.prev.sector_count, string_byte_bits(idereg->reg.named_48.prev.sector_count));
	printf("LBA Low       : %02X %s", idereg->reg.named_48.cur.lba_low, string_byte_bits(idereg->reg.named_48.cur.lba_low));
	printf(" %02X %s\n", idereg->reg.named_48.prev.lba_low, string_byte_bits(idereg->reg.named_48.prev.lba_low));
	printf("LBA Mid       : %02X %s", idereg->reg.named_48.cur.lba_mid, string_byte_bits(idereg->reg.named_48.cur.lba_mid));
	printf(" %02X %s\n", idereg->reg.named_48.prev.lba_mid, string_byte_bits(idereg->reg.named_48.prev.lba_mid));
	printf("LBA High      : %02X %s", idereg->reg.named_48.cur.lba_high, string_byte_bits(idereg->reg.named_48.cur.lba_high));
	printf(" %02X %s\n", idereg->reg.named_48.prev.lba_high, string_byte_bits(idereg->reg.named_48.prev.lba_high));
	printf("Device        : %02X %s\n", idereg->device.device, string_byte_bits(idereg->device.device));
	printf("Command/Status: %02X %s\n", idereg->command.command, string_byte_bits(idereg->command.command));
}

void print_smart_log_directory(__in_ecount(512) const BYTE* data)
{
	int i;
	printf("Smart Logging Version: %02X %02X\n", data[0], data[1]);
	printf("Number of sectors in the log at log address\n");
	for (i = 0; i < 255; ++i)
	{
		/* i + 1  */
		printf("%4d ", data[2*i+2]);
		if (((i+1)%12)== 0) printf("\n");
	}
	printf("\n");
}

int smart_read_log(lsp_socket_context_t* scontext)
{
	/* SMART READ LOG */
	int ret;
	unsigned char data[512] = {0};
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_io_data_buffer_t iobuf = {0};
	iobuf.recv_buffer = data;
	iobuf.recv_size = sizeof(data);
	reg_in.reg.named.features = 0xD5; /* SMART READ LOG FEATURE */
	reg_in.reg.named.sector_count = 1;
	reg_in.reg.named.lba_low = 0x00; /* log address */
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = 0xB0; /* SMART COMMAND */
	reg_in.device.device = 0;
	scontext->lsp_status = lsp_ide_command(scontext->lsp_handle, 0, 0, 0, &reg_in, &iobuf, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	scontext->lsp_status = lsp_get_ide_command_output_register(
		scontext->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		printf("lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			scontext->lsp_status, 
			scontext->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		print_ide_register(&reg_out);
		printbytes(data, sizeof(data));
	}
	return 0;
}

int smart_read_data(lsp_socket_context_t* scontext)
{
	/* SMART READ DATA */
	int ret;
	unsigned char data[512] = {0};
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_io_data_buffer_t iobuf = {0};
	iobuf.recv_buffer = data;
	iobuf.recv_size = sizeof(data);
	reg_in.reg.named.features = 0xD0; /* SMART READ DATA FEATURE */
    reg_in.reg.named.sector_count = 1;
    reg_in.reg.named.lba_low = 1;
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = 0xB0; /* SMART COMMAND */
	reg_in.device.device = 0xa0;

    dprintf(">> Input Register\n");
	print_ide_register(&reg_in);

	scontext->lsp_status = lsp_ide_command(
		scontext->lsp_handle, 0, 0, 0, &reg_in, &iobuf, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	scontext->lsp_status = lsp_get_ide_command_output_register(
		scontext->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		printf("lsp_get_ide_command_output_register failed: %d(0x%08x)\n",
			scontext->lsp_status, 
			scontext->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
        dprintf(">> Output Register\n");
		print_ide_register(&reg_out);
		// printbytes(data, sizeof(data));
		print_smart_log_directory(data);
	}
	return 0;
}

/* SMART RETURN STATUS */
int smart_return_status(lsp_socket_context_t* scontext)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	reg_in.reg.named.features = 0xDA;
	reg_in.reg.named.lba_mid = 0x4F;
	reg_in.reg.named.lba_high = 0xC2;
	reg_in.command.command = 0xB0;
	reg_in.device.device = 0;
	scontext->lsp_status = lsp_ide_command(scontext->lsp_handle, 0, 0, 0, &reg_in, 0, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	scontext->lsp_status = lsp_get_ide_command_output_register(
		scontext->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		printf("lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			scontext->lsp_status, 
			scontext->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		print_ide_register(&reg_out);
	}
	return 0;
}

// WIN_STANDBYNOW1
int win_standbynow1(lsp_socket_context_t* scontext)
{
	int ret;
	lsp_ide_register_param_t reg_in = {0};
	reg_in.command.command = 0xE0; /* WIN_STANDBYNOW1 */
	reg_in.device.device = 0;
	scontext->lsp_status = lsp_ide_command(scontext->lsp_handle, 0, 0, 0, &reg_in, 0, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	return 0;
}

int win_identify(lsp_socket_context_t* scontext, struct hd_driveid* driveid)
{
	int ret;
	unsigned short *identify_words;

	scontext->lsp_status = lsp_ide_identify(
		scontext->lsp_handle, 0, 0, 0, driveid);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}

	identify_words = (unsigned short*) driveid;

	printf("Multiword DMA modes: %04Xh (bits: %s)\n", 
		identify_words[63], 
		string_word_bits(identify_words[63]));

	printf("Ultra DMA modes: %04Xh (bits: %s)\n", 
		identify_words[88],
		string_word_bits(identify_words[88]));

	printf("PIO modes supported: %04Xh (bits: %s)\n",
		identify_words[64],
		string_word_bits(identify_words[64]));

	/* 48-bit address feature set is available and enabled? */
	if ((driveid->command_set_2 & 0x0400) && 
		(driveid->cfs_enable_2 & 0x0400))
	{
		UINT64 totalsectors = driveid->lba_capacity_2;
		UINT64 bytes = totalsectors << 9; /* * 512 */
		UINT64 kb = bytes >> 10; /* / 1024; */
		UINT64 mb = kb >> 10; /* / 1024; */
		UINT64 gb = mb >> 10; /* / 1024; */
		printf("Total sectors: %I64d (%08Xh), ", totalsectors, totalsectors);
		printf("%dKB ", kb);
		printf("%dMB ", mb);
		printf("%dGB\n", gb);
	}
	else
	{
		UINT64 totalsectors = driveid->lba_capacity;
		UINT64 bytes = totalsectors << 9; /* * 512 */
		UINT64 kb = bytes >> 10; /* / 1024; */
		UINT64 mb = kb >> 10; /* / 1024; */
		UINT64 gb = mb >> 10; /* / 1024; */
		printf("Total sectors: %I64d (%08Xh), ", totalsectors, totalsectors);
		printf("%dKB ", kb);
		printf("%dMB ", mb);
		printf("%dGB\n", gb);
	}

	printf("* Major Rev: %04X %s\n", driveid->major_rev_num, string_word_bits(driveid->major_rev_num));
	printf("* Minor Rev: %04X %s\n", driveid->minor_rev_num, string_word_bits(driveid->minor_rev_num));

	printf("* Acoustic: %04X, %d,%d\n", driveid->acoustic, HIBYTE(driveid->acoustic), LOBYTE(driveid->acoustic));	

	printf("* cmd set: %s %s\n", 
		string_word_bits(driveid->command_set_1),
		string_word_bits(driveid->command_set_2));

	print_word_bits_desc(driveid->command_set_1, command_set_1_desc);
	printf("---\n");
	print_word_bits_desc(driveid->command_set_2, command_set_2_desc);

	printf("* cmd set-feature supported extensions: %s\n", 
		string_word_bits(driveid->cfsse));

	print_word_bits_desc(driveid->cfsse, cfsse_desc);

	printf("---\n");

	printf("* cmd set-feature enabled: %s %s\n", 
		string_word_bits(driveid->cfs_enable_1),
		string_word_bits(driveid->cfs_enable_2));

	print_word_bits_desc(driveid->cfs_enable_1, cfs_enable_1_desc);
	printf("---\n");
	print_word_bits_desc(driveid->cfs_enable_2, cfs_enable_2_desc);

	printf("* cmd set-feature default: %s\n",
		string_word_bits(driveid->csf_default));

	print_word_bits_desc(driveid->csf_default, csf_default_desc);

	return 0;
}

int ide_read(
	lsp_socket_context_t* scontext,
	lsp_large_integer_t* location,
	void* buf,
	size_t len)
{
	int ret;
	scontext->lsp_status = lsp_ide_read(
		scontext->lsp_handle, 0, 0, 0, 1, 0, 
		location, len/512, buf, len);

	ret = process_lsp_transfer(scontext);
	dprintf("lsp_ide_read LSP:0x%x\n", scontext->lsp_status);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}

	printbytes(buf, len);

	return 0;
}

int ide_write(
	lsp_socket_context_t* scontext,
	lsp_large_integer_t* location,
	void* buf,
	size_t len)
{
	int ret;
	scontext->lsp_status = lsp_ide_write(
		scontext->lsp_handle, 0, 0, 0, 1, 0, 
		location, len/512, buf, len);

	ret = process_lsp_transfer(scontext);
	dprintf("lsp_ide_write LSP:0x%x\n", scontext->lsp_status);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}

	return 0;
}

int ide_set_feature(
	lsp_socket_context_t* scontext,
	lsp_ide_register_param_t* reg_in)
{
	int ret;
	lsp_ide_register_param_t reg_out = {0};
	//reg_in->reg.named.features = 0xDA;
	//reg_in->reg.named.lba_mid = 0x4F;
	//reg_in->reg.named.lba_high = 0xC2;
	reg_in->command.command = 0xEF; /* SET FEATURE */
	reg_in->device.device = 0;
	scontext->lsp_status = lsp_ide_command(scontext->lsp_handle, 0, 0, 0, reg_in, 0, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	scontext->lsp_status = lsp_get_ide_command_output_register(
		scontext->lsp_handle, &reg_out);
	if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		printf("lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			scontext->lsp_status, 
			scontext->lsp_status);
	}
	else
	{
		/* lba_mid = 0x4f, lba_high = 0xc2 
		*   a threshold exceeded condition NOT detected
		* lba_mid = 0xf4, lba_high = 0x2c
		*   a threshold exceeded condition detected
		*/
		print_ide_register(&reg_out);
	}
	return 0;
}

/* level is 0x80 - 0xFE */
int ide_set_feature_enable_automatic_acoustic_management(
	lsp_socket_context_t* scontext,
	BYTE level)
{
	lsp_ide_register_param_t reg_in = {0};
	/* Enable Automatic Acoustic Management Feature Set */
	reg_in.reg.named.features = 0x42;
	reg_in.reg.named.sector_count = level;
	return ide_set_feature(scontext, &reg_in);
}

int ide_set_feature_disable_automatic_acoustic_management(
	lsp_socket_context_t* scontext)
{
	lsp_ide_register_param_t reg_in = {0};
	/* Disable Automatic Acoustic Management Feature Set */
	reg_in.reg.named.features = 0xC2;
	return ide_set_feature(scontext, &reg_in);
}

int ide_non_data_command(
	lsp_socket_context_t* scontext,
	lsp_ide_register_param_t* reg_in,
	lsp_ide_register_param_t* reg_out)
{
	int ret;
	scontext->lsp_status = lsp_ide_command(scontext->lsp_handle, 0, 0, 0, reg_in, 0, 0);
	ret = process_lsp_transfer(scontext);
	if (ret != 0)
	{
		return ret;
	}
	else if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		return -1;
	}
	scontext->lsp_status = lsp_get_ide_command_output_register(
		scontext->lsp_handle, reg_out);
	if (LSP_STATUS_SUCCESS != scontext->lsp_status)
	{
		printf("lsp_get_ide_command_output_register failed: %d(0x%08x)\n", 
			scontext->lsp_status, scontext->lsp_status);
	}
	return 0;
}

int ata_read_native_max_address(
	lsp_socket_context_t* scontext)
{
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	int ret;

	reg_in.device.lba = 1;
	reg_in.command.command = 0xF8;

	print_ide_register(&reg_in);

	ret = ide_non_data_command(scontext, &reg_in, &reg_out);

	if (0 == ret)
	{
		print_ide_register(&reg_out);
	}

	return ret;
}

int ata_read_native_max_address_ext(
	lsp_socket_context_t* scontext)
{
	lsp_ide_register_param_t reg_in = {0};
	lsp_ide_register_param_t reg_out = {0};
	lsp_status_t lsp_status;
	int ret;

	reg_in.use_48 = 1;
	reg_in.device.lba = 1;
	reg_in.command.command = 0x27;

	print_ide_register(&reg_in);

	ret = ide_non_data_command(scontext, &reg_in, &reg_out);

	if (0 == ret)
	{
		print_ide_register(&reg_out);
	}

	return ret;
}

int lsp_test(SOCKET s)
{
	int ret, lasterr, i;
	lsp_handle_t     lsp_handle;
	lsp_login_info_t lsp_login_info;
	lsp_status_t     lsp_status;

	void*            session_buffer;

	struct hd_driveid driveid = {0};
	lsp_socket_context_t scontext;

	lsp_large_integer_t location = {0};

	session_buffer = HeapAlloc(GetProcessHeap(), 0, LSP_SESSION_BUFFER_SIZE);
	if (NULL == session_buffer)
	{
		dprintf("Out of memory\n");
		return 1;
	}

	lsp_handle = lsp_create_session(session_buffer, (void*) s);

	ZeroMemory(&lsp_login_info, sizeof(lsp_login_info_t));
	lsp_login_info.login_type = LSP_LOGIN_TYPE_NORMAL;
	lsp_login_info.password = 0x1F4A50731530EABB;
	lsp_login_info.unit_no = 0;

	ret = lsp_socket_context_init(&scontext, s, lsp_handle);
	if (0 != ret)
	{
		return ret;
	}

	//lsp_login_info.write_access = 1;

	//dprintf("--- login rw ---\n");
	//lsp_status = lsp_login(lsp_handle, &lsp_login_info);
	//ret = process_lsp_transfer(&scontext);

	lsp_login_info.write_access = 0;

	dprintf("--- login ro ---\n");
	scontext.lsp_status = lsp_login(scontext.lsp_handle, &lsp_login_info);
	ret = process_lsp_transfer(&scontext);

	// dprintf("--- standbynow1 ---\n");
	// win_standbynow1(&scontext);

	dprintf("--- identify ---\n");
	win_identify(&scontext, &driveid);

#if 0
	dprintf("--- read native max address ---\n");
	ata_read_native_max_address(&scontext);

	dprintf("--- read native max address ext ---\n");
	ata_read_native_max_address_ext(&scontext);

	/*
	dprintf("--- set_feature_disable_automatic_acoustic_management ---\n");
	ide_set_feature_disable_automatic_acoustic_management(&scontext);
	*/

	//dprintf("--- set_feature_enable_automatic_acoustic_management ---\n");
	//ide_set_feature_enable_automatic_acoustic_management(&scontext, 0x80);

    dprintf("--- smart read data ---\n");
    smart_read_data(&scontext);
    
	dprintf("--- read ---\n");
	location.quad = 0x08F2E6F0;
	ide_read(&scontext, &location);

#endif
	//{
	//	char buf[512];
	//	location.quad = 0x08F2E6EF;

	//	dprintf("--- read ---\n");
	//	ide_read(&scontext, &location, buf, sizeof(buf));

	//	ZeroMemory(buf, sizeof(buf));
	//	dprintf("--- write ---\n");
	//	ide_write(&scontext, &location, buf, sizeof(buf));
	//}

	dprintf("--- logout ---\n");
	scontext.lsp_status = lsp_logout(scontext.lsp_handle);
	ret = process_lsp_transfer(&scontext);

	lsp_socket_context_destroy(&scontext);
	lsp_destroy_session(lsp_handle);
	HeapFree(GetProcessHeap(), 0, session_buffer);

	return 0;
}

const char* lpx_addr_node_str(const unsigned char* nodes)
{
	static char buf[30] = {0};
	StringCchPrintf(buf, 30, "%02X:%02X:%02X:%02X:%02X:%02X",
		nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
	return buf;
}

const char* lpx_addr_str(const struct sockaddr_lpx* lpx_addr)
{
	return lpx_addr_node_str(lpx_addr->slpx_addr.node);
}

/*
 * returns the value of the char represented in hexadecimal.
 * this function returns the value between 0 to 15 if c is valid,
 * otherwise returns -1.
 */
int parse_hex_numchar(char c)
{
	if (c >= '0' && c <= '9') return c - '0' + 0x0;
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xa;
	return -1;
}

/*
 * parses arg of which a form is '00:0b:d0:01:6b:76', where
 * ':' may be substituted with '-' or omitted.
 * addr must be a pointer to a buffer holding at least 6 bytes
 * returns 0 if successful, otherwise returns -1
 */
/*
 * parses arg of which a form is '00:0b:d0:01:6b:76', where
 * ':' may be substituted with '-' or omitted.
 * addr must be a pointer to a buffer holding at least 6 bytes
 * returns 0 if successful, otherwise returns -1
 */
int parse_addr(char* arg, unsigned char* addr)
{
	int i;
	int u, l;
	char* p;
	
	p = arg;
	for (i = 0; i < 6; ++i)
	{
		u = *p ? parse_hex_numchar(*p++) : -1;
		if (-1 == u) return -1;

		l = *p ? parse_hex_numchar(*p++) : -1;
		if (-1 == l) return -1;

		addr[i] = u * 0x10 + l;

		if (*p == ':' || *p == '-') ++p;
	}
	return 0;
}

/*
 * returns the connected socket to the device
 * and hostaddr fills with the connected host address 
 */
SOCKET 
connect_to_device(
	const struct sockaddr_lpx* devaddr,
	struct sockaddr_lpx* hostaddr);

int run(int argc, char** argv)
{
	static const char* usage = "usage: lsptest <addr>";

	int ret;
	SOCKET s;
	struct sockaddr_lpx devaddr;
	struct sockaddr_lpx hostaddr;
	u_char devaddrnodes[6];

	/* 1.1 */
	// u_char devaddrnodes[6] = {0x00,0x0b,0xd0,0x01,0x6b,0x76};
	/* 1.0 */
	// u_char devaddrnodes[6] = {0x00,0x0b,0xd0,0xfe,0x02,0x79};
	// u_char devaddrnodes[6] = {0x00,0x0b,0xd0,0x00,0x8a,0x29};
	/* 2.0 */
	// u_char devaddrnodes[6] = {0x00,0x0d,0x0b,0x5d,0x80,0x03};

	if (argc < 2)
	{
		fprintf(stderr, "%s\n", usage);
		return 1;
	}

	if (0 != parse_addr(argv[1], devaddrnodes))
	{
		fprintf(stderr, "error: invalid address format\n");
		return 1;
	}

	ZeroMemory(&devaddr, sizeof(devaddr));
	devaddr.port = htons( 10000 );
	devaddr.sin_family = AF_LPX;
	CopyMemory(devaddr.slpx_addr.node, devaddrnodes, sizeof(devaddr.slpx_addr.node));

	s = connect_to_device(&devaddr, &hostaddr);
	if (INVALID_SOCKET == s)
	{
		return 1;
	}

	ret = lsp_test(s);

	dprintf("Closing the socket.\n");
	closesocket(s);
	dprintf("Closed.\n");

	return ret;
}

int __cdecl main(int argc, char** argv)
{
	int ret;
	WSADATA wsadata;

	ret = WSAStartup(MAKEWORD(2,0), &wsadata);
	if (0 == ret)
	{
		ret = run(argc, argv);
		(void) WSACleanup();
		return ret;
	}
	else
	{
		dprintf("WSAStartup failed with error %d", ret);
	}

	return ret;
}

/*++

 returns the connected socket to the device
 and hostaddr fills with the connected host address 

--*/
SOCKET 
connect_to_device(
	const struct sockaddr_lpx* devaddr,
	struct sockaddr_lpx* hostaddr)
{
	int i;
	int ret;
	BOOL connected;
	SOCKET s;
	DWORD list_size;
	LPSOCKET_ADDRESS_LIST addrlist;

	connected = FALSE;
	addrlist = NULL;
	s = WSASocket(AF_LPX, SOCK_STREAM, LPXPROTO_STREAM, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == s)
	{
		dprintf("Failed to create a socket with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	ret = WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addrlist, 0, &list_size, 0, 0);
	if (0 != ret && WSAEFAULT != WSAGetLastError())
	{
		dprintf("WSAIoctl failed with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	dprintf("list: %d\n", list_size);

	addrlist = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, list_size);
	if (NULL == addrlist)
	{
		dprintf("Memory allocation failed.\n");
		return INVALID_SOCKET;
	}

	ret = WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addrlist, list_size, &list_size, 0, 0);
	if (0 != ret)
	{
		dprintf("WSAIoctl failed with error %d\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	for (i = 0; i < addrlist->iAddressCount; ++i)
	{
		CopyMemory(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
		hostaddr->port = 0;
		hostaddr->sin_family = AF_LPX;

		dprintf("binding to %s\n", lpx_addr_str(hostaddr));

		ret = bind(s, (const struct sockaddr*)hostaddr, sizeof(struct sockaddr_lpx));

		if (0 == ret)
		{
			dprintf("binded to %s\n", lpx_addr_str(hostaddr));

			dprintf("connecting to %s", lpx_addr_str(devaddr));
			dprintf(" at %s\n", lpx_addr_str(hostaddr));

			ret = connect(s, (const struct sockaddr*) devaddr, sizeof(struct sockaddr_lpx));
			if (0 == ret)
			{
				dprintf("connected to %s", lpx_addr_str(devaddr));
				dprintf(" at %s\n", lpx_addr_str(hostaddr));
				connected = TRUE;
				HeapFree(GetProcessHeap(), 0, addrlist);
				return s;
			}
			else
			{
				dprintf("Connect failed with error %d\n", WSAGetLastError());
			}
		}

		closesocket(s);

		s = WSASocket(AF_LPX, SOCK_STREAM, LPXPROTO_STREAM, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (INVALID_SOCKET == s)
		{
			dprintf("Failed to create a socket with error %d\n", WSAGetLastError());
			HeapFree(GetProcessHeap(), 0, addrlist);
			return INVALID_SOCKET;
		}
	}

	HeapFree(GetProcessHeap(), 0, addrlist);
	closesocket(s);
	return INVALID_SOCKET;
}
