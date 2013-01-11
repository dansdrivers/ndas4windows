#include "lsp.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <strsafe.h>
#include <winsock2.h>

struct lpx_addr {
	u_char node[6];
	char _reserved_[10];
};

struct sockaddr_lpx {
	short           sin_family;
	u_short	        port;
	struct lpx_addr slpx_addr;
};

typedef	struct _BIN_PARAM_TARGET_LIST_ELEMENT {
	unsigned _int32	TargetID;
	unsigned _int8	NRRWHost;
	unsigned _int8	NRROHost;
	unsigned _int16	Reserved1;
	unsigned _int64	TargetData;
} BIN_PARAM_TARGET_LIST_ELEMENT, *PBIN_PARAM_TARGET_LIST_ELEMENT;

typedef struct _BIN_PARAM_TARGET_LIST {
	unsigned _int8	ParamType;
	unsigned _int8	NRTarget;
	unsigned _int16	Reserved1;
	BIN_PARAM_TARGET_LIST_ELEMENT	PerTarget[2];
} BIN_PARAM_TARGET_LIST, *PBIN_PARAM_TARGET_LIST;


void* 
lsp_proc_call 
lsp_proc_mem_alloc(void* context, size_t size) 
{ return malloc(size); }

void 
lsp_proc_call 
lsp_proc_mem_free(void* context, void* pblk) 
{ free(pblk); }

lsp_trans_error_t lsp_proc_call 
lsp_proc_send(void* context, const void* buf, size_t len, size_t* sent)
{
	SOCKET s = (SOCKET) context;
	int sent_bytes = send(s, (const char*) buf, len, 0);
	if (sent_bytes == SOCKET_ERROR)
		return LSP_TRANS_ERROR;
	*sent = sent_bytes;
	return LSP_TRANS_SUCCESS;
}

lsp_trans_error_t lsp_proc_call 
lsp_proc_recv(void* context, void* buf, size_t len, size_t* recvd)
{
	SOCKET s = (SOCKET) context;
	int recv_bytes = recv(s, (char*) buf, len, 0);
	if (recv_bytes == SOCKET_ERROR)
		return LSP_TRANS_ERROR;
	*recvd = recv_bytes;
	return LSP_TRANS_SUCCESS;
}

#define IPPROTO_LPXTCP 214
#define	NDAS_CTRL_REMOTE_PORT 10000
#define AF_LPX AF_UNSPEC

#define SOCK_CALL(stmt) \
	do { int err = stmt; if (0 != err) { printf("err %u from " #stmt "\n", WSAGetLastError()); return err; } } while(0)

const char* lpx_addr_node_str(unsigned char* nodes)
{
	static char buf[30] = {0};
	StringCchPrintf(buf, 30, "%02X:%02X:%02X:%02X:%02X:%02X",
		nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
	return buf;
}

const char* lpx_addr_str(struct sockaddr_lpx* lpx_addr)
{
	return lpx_addr_node_str(lpx_addr->slpx_addr.node);
}

int __cdecl main(int argc, char** argv)
{
	int result;
	BOOL connected = FALSE;

	lsp_handle h = 0;
	lsp_transport_proc proc;
	struct sockaddr_lpx host_addr;
	struct sockaddr_lpx ndas_dev_addr = {0};

	WSADATA wsa_data;
	SOCKET s;
	lsp_error_t              err;

	ndas_dev_addr.port = htons( NDAS_CTRL_REMOTE_PORT );
	ndas_dev_addr.sin_family = AF_LPX;
	ndas_dev_addr.slpx_addr.node[0] = 0x00;
	ndas_dev_addr.slpx_addr.node[1] = 0x0b;
	ndas_dev_addr.slpx_addr.node[2] = 0xd0;
	ndas_dev_addr.slpx_addr.node[3] = 0xfe;
	ndas_dev_addr.slpx_addr.node[4] = 0x02;
	ndas_dev_addr.slpx_addr.node[5] = 0x7c;

	SOCK_CALL( WSAStartup(MAKEWORD(2,0), &wsa_data) );

	{
		int i;
		size_t list_size;
		LPSOCKET_ADDRESS_LIST addr_list = 0;

		s = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		result = WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addr_list, 0, &list_size, 0, 0);

		printf("list: %d\n", list_size);

		addr_list = malloc(list_size);
		SOCK_CALL( WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addr_list, list_size, &list_size, 0, 0) );

		for (i = 0; i < addr_list->iAddressCount; ++i)
		{
			host_addr = * (struct sockaddr_lpx*) addr_list->Address[i].lpSockaddr;
			host_addr.port = 0;
			host_addr.sin_family = AF_UNSPEC;

			printf("binding    to %s\n", lpx_addr_str(&host_addr));

			SOCK_CALL( bind(s, (const struct sockaddr*) &host_addr, sizeof(struct sockaddr_lpx)) );

			printf("binded     to %s\n", lpx_addr_str(&host_addr));

			printf("connecting to %s", lpx_addr_str(&ndas_dev_addr));
			printf(" at %s\n", lpx_addr_str(&host_addr));
			result = connect(
				s,
				(const struct sockaddr*) &ndas_dev_addr, 
				sizeof(ndas_dev_addr));
			if (result == 0)
			{
				connected = TRUE;
				break;
			}
			closesocket(s);
			s = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP);
		}
	}

	if (!connected) 
	{
		printf("connection failed.\n");
		return 1;
	}

	printf("connected  to %s", lpx_addr_str(&ndas_dev_addr));
	printf(" at %s\n", lpx_addr_str(&host_addr));

	proc.mem_alloc = lsp_proc_mem_alloc;
	proc.mem_free = lsp_proc_mem_free;
	proc.send = lsp_proc_send;
	proc.recv = lsp_proc_recv;

	//
	// test functions
	//
#define __LSP_TEST_TYPE_NORMAL__
	{
		lsp_login_info login_info;

#ifdef __LSP_TEST_TYPE_NORMAL__
		login_info.login_type = LSP_LOGIN_TYPE_NORMAL;
		login_info.password = 0x1F4A50731530EABB;
		login_info.unit_no = 0;
		login_info.write_access = 1;

		h = lsp_create_session(&proc, (void *)s);
		err = lsp_login(h, &login_info);

		{
			// send no op command to NDAS
			err = lsp_noop_command(h);
		}

		{
			// writes 1 sector to sector 0 using dma, lba 48
			lsp_ide_register_param p;
			lsp_ide_data_buffer data_buf;
			lsp_uint8 buffer[512];
			lsp_uint32 target_id; 
			lsp_uint32 lun0, lun1;
			lsp_uint32 i;
			
			for(i = 0; i < sizeof(buffer); i++)
				buffer[i] = (lsp_uint8)(i % sizeof(buffer));
			
			memset(&p, 0, sizeof(lsp_ide_register_param));
			memset(&data_buf, 0, sizeof(lsp_ide_data_buffer));

			// set registers
			p.use_dma = 1;
			p.use_48 = 1;
			p.device.lba_head_nr = 0; // sector : 0
			p.device.dev = 0; // target id : 0
			p.device.lba = 1; // use lba
			p.command.command = 0x35; // == WIN_WRITEDMA_EXT;
			p.reg.named_48.cur.features; // reserved
			p.reg.named_48.prev.features; // reserved
			p.reg.named_48.cur.sector_count = 1; // sector count : 1
			p.reg.named_48.prev.sector_count = 0; // sector count : 1
			p.reg.named_48.cur.lba_low = 0; // sector : 0
			p.reg.named_48.prev.lba_low = 0; // sector : 0
			p.reg.named_48.cur.lba_mid = 0; // sector : 0
			p.reg.named_48.prev.lba_mid = 0; // sector : 0
			p.reg.named_48.cur.lba_high = 0; // sector : 0
			p.reg.named_48.prev.lba_high = 0; // sector : 0

			// set data buffer
			data_buf.send_buffer = buffer;
			data_buf.send_size = sizeof(buffer);

			target_id = 0;
			lun0 = 0;
			lun1 = 0;

			err = lsp_ide_command(h, target_id, lun0, lun1, &p, &data_buf, (void *)0);
		}

		{
			// read max retransmission time

			lsp_uint8 param[8]; // assume HW version 1.1

			err = lsp_vendor_command(
				h,
				LSP_VENDOR_ID_XIMETA,
				LSP_VENDOR_OP_CURRENT_VERSION,
				LSP_VCMD_GET_MAX_RET_TIME,
				param,
				sizeof(param),
				0);
		}

		err = lsp_logout(h);
		lsp_destroy_session(h);
#elif defined __LSP_TEST_TYPE_DISCOVER__

		login_info.login_type = LSP_LOGIN_TYPE_DISCOVER;
		login_info.password = 0x1F4A50731530EABB;
		login_info.unit_no = 0;
		login_info.write_access = 1;

		h = lsp_create_session(&proc, (void *)s);
		err = lsp_login(h, &login_info);

		{
			// read target list
			BIN_PARAM_TARGET_LIST param;
			lsp_uint16 data_out_length; 
			param.ParamType = 0x03; // list
			data_out_length = sizeof(param);
			

			err =lsp_text_command(
				h,
				0x01, // LSP_PARM_TYPE_BINARY,
				0x00, // LSP_BINPARM_CURRENT_VERSION,
				(lsp_uint8 *)&param,
				4,
				&data_out_length
				);

		}
		
		err = lsp_logout(h);
		lsp_destroy_session(h);
#endif
	}

	closesocket(s);
	WSACleanup();

	return 0;
}
