#include <windows.h>
#include <winsock2.h>
#include <socketlpx.h>
#include <crtdbg.h>
#include <stdio.h>
#include <stdlib.h>
#include "lsptest_transport.h"

#ifndef countof
#define countof(A) (sizeof(A)/sizeof((A)[0]))
#endif

int lsptest_winsock_debug_mode = 0;
FILE* lsptest_winsock_debug_out = 0; // stderr;

void
lsptest_winsock_debug(char* format, ...)
{
	va_list ap;
	if (lsptest_winsock_debug_mode) 
	{
		va_start(ap, format);
		vfprintf(lsptest_winsock_debug_out, format, ap);
		va_end(ap);
	}
}

/* Send coalescing is not supported in current NDAS devices */
/* #define LSPTEST_USE_COALESCING */ 

struct lpx_addr {
	u_char node[6];
	char _reserved_[10];
};

struct sockaddr_lpx {
	short           sin_family;
	u_short	        port;
	struct lpx_addr slpx_addr;
};

typedef struct _lsp_winsock_context_t lsp_winsock_context_t;

typedef struct _lsp_transfer_context_t {
	lsp_winsock_context_t* socket_context;
	DWORD index;
	WSABUF wsabuf[2];
	WSAOVERLAPPED overlapped;
	DWORD error;
	DWORD txbytes;
	DWORD flags;
} lsp_transfer_context_t;

typedef struct _lsp_winsock_context_t {
	lsptest_context_t lsp_context;
	SOCKET socket;
	int next_transfer_index;
	lsp_transfer_context_t lsp_transfer_context[LSP_MAX_CONCURRENT_TRANSFER];
	long outstanding_transfers;
#ifdef LSPTEST_USE_COALESCING
	lsp_transfer_context_t* deferred_txcontext;
#endif
} lsp_winsock_context_t;

SOCKET 
lsptest_winsock_connect_to_device(
	const struct sockaddr_lpx* devaddr,
	struct sockaddr_lpx* hostaddr);

int lsptest_transport_static_initialize()
{
	int ret;
	WSADATA wsadata;
	ret = WSAStartup(MAKEWORD(2,0), &wsadata);
	if (0 != ret)
	{
		fprintf(stderr, "WSAStartup failed, code=0x%X\n", WSAGetLastError());
		return ret;
	}
	fprintf(stderr, "Transport: Winsock Version %d.%d (%s).\n", 
		LOBYTE(wsadata.wVersion),
		HIBYTE(wsadata.wVersion),
		wsadata.szDescription);
	return 0;
}

void lsptest_transport_static_cleanup()
{
	WSACleanup();
}

lsptest_context_t*
lsptest_transport_create()
{
	lsp_winsock_context_t* wscontext;
	int i;

	wscontext = (lsp_winsock_context_t*) malloc(
		sizeof(lsp_winsock_context_t));

	if (0 == wscontext) return 0;

	memset(wscontext, 0, sizeof(lsp_winsock_context_t));

	wscontext->socket = INVALID_SOCKET;

	for (i = 0; i < LSP_MAX_CONCURRENT_TRANSFER; ++i)
	{
		lsp_transfer_context_t* txcontext = &wscontext->lsp_transfer_context[i];
		txcontext->socket_context = wscontext;
		txcontext->index = i;
	}

	return &wscontext->lsp_context;
}

int 
lsptest_transport_connect(
	__in lsptest_context_t* context,
	__in_bcount(6) unsigned char* ndas_dev_addr)
{
	lsp_winsock_context_t* wscontext = (lsp_winsock_context_t*) context; 
	struct sockaddr_lpx remote_addr;
	struct sockaddr_lpx local_addr;
	SOCKET s;

	memset(&remote_addr, 0, sizeof(remote_addr));
	remote_addr.port = htons( 10000 );
	remote_addr.sin_family = AF_LPX;
	memcpy(
		remote_addr.slpx_addr.node, 
		ndas_dev_addr, 
		sizeof(remote_addr.slpx_addr.node));

	wscontext->socket = lsptest_winsock_connect_to_device(&remote_addr, &local_addr);
	if (INVALID_SOCKET == wscontext->socket)
	{
		return -1;
	}
	return 0;
}

int 
lsptest_transport_disconnect(
	lsptest_context_t* context)
{
	lsp_winsock_context_t* wscontext = (lsp_winsock_context_t*) context;
	closesocket(wscontext->socket);
	wscontext->socket = INVALID_SOCKET;
	return 0;
}

const char* lpx_addr_node_str(const unsigned char* nodes)
{
	static char buf[30] = {0};
#pragma warning(disable: 4995)
	_snprintf(buf, 30, "%02X:%02X:%02X:%02X:%02X:%02X",
		nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
#pragma warning(default: 4995)
	return buf;
}

const char* lpx_addr_str(const struct sockaddr_lpx* lpx_addr)
{
	return lpx_addr_node_str(lpx_addr->slpx_addr.node);
}

void 
CALLBACK 
lsptest_winsock_transfer_completion(
	IN DWORD error,
	IN DWORD txbytes,
	IN LPWSAOVERLAPPED overlapped,
	IN DWORD flags)
{
	LONG ioc;
	UINT i;
	lsp_transfer_context_t* txcontext = CONTAINING_RECORD(
		overlapped, 
		lsp_transfer_context_t, 
		overlapped);

	lsp_winsock_context_t* wscontext = txcontext->socket_context;

	txcontext->error = error;
	txcontext->txbytes = txbytes;
	txcontext->flags = flags;

	if (error != ERROR_SUCCESS)
	{
		lsptest_winsock_debug("T[%d]%d ERR=%X,FLAGS=%X ", txcontext->index, txbytes, error, flags);
	}
	else
	{
		// printf("T[%d]%d ", txcontext->index, txbytes);
		lsptest_winsock_debug("[%d] Transferred %d bytes\n", txcontext->index, txbytes);
	}

	ioc = InterlockedDecrement(&wscontext->outstanding_transfers);

	if (ioc == 0)
	{
		wscontext->lsp_context.lsp_status = 
			lsp_process_next(wscontext->lsp_context.lsp_handle);
	}
}

int
lsptestp_transport_process_transfer(
	lsptest_context_t* context,
	int nonwait);

int
lsptest_transport_process_transfer_non_wait(
	__in lsptest_context_t* context)
{
	return lsptestp_transport_process_transfer(context, 1);
}

int
lsptest_transport_process_transfer(
	lsptest_context_t* context)
{
	return lsptestp_transport_process_transfer(context, 0);
}

int
lsptestp_transport_process_transfer(
	lsptest_context_t* context,
	int nonwait)
{
	lsp_winsock_context_t* wscontext = (lsp_winsock_context_t*) context;
	int i;
	int ret;

	lsp_status_t lsp_status = context->lsp_status;

	if (nonwait && wscontext->outstanding_transfers > 0)
	{
		return 1;
	}

	wscontext->next_transfer_index = 0;
	wscontext->outstanding_transfers = 0;

	while (TRUE)
	{
		if (LSP_REQUIRES_SEND == lsp_status)
		{
			DWORD txlen;
			DWORD txindex = wscontext->next_transfer_index++;
			lsp_transfer_context_t* txcontext = &wscontext->lsp_transfer_context[txindex];
			LPWSAOVERLAPPED overlapped = &txcontext->overlapped;
#ifdef LSPTEST_USE_EVENT
			WSAEVENT event = overlapped->hEvent;
#endif
			memset(&txcontext->overlapped, 0, sizeof(WSAOVERLAPPED));
#ifdef LSPTEST_USE_EVENT
			txcontext->overlapped.hEvent = event;
#endif

			txcontext->wsabuf[0].buf = (char*) lsp_get_buffer_to_send(
				context->lsp_handle, 
				&txcontext->wsabuf[0].len);

			lsptest_winsock_debug("[%d] Sending %d bytes\n", 
				txcontext->index, 
				txcontext->wsabuf[0].len);

			InterlockedIncrement(&wscontext->outstanding_transfers);

			ret = WSASend(
				wscontext->socket, 
				&txcontext->wsabuf[0],
				1, 
				&txlen, 
				0, 
				&txcontext->overlapped, 
				lsptest_winsock_transfer_completion);

			if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
			{
				lsptest_winsock_debug("WSASend failed with error %d\n", WSAGetLastError());
				return SOCKET_ERROR;
			}
			if (0 == ret)
			{
				lsptest_winsock_debug("Sent %d bytes already\n", txlen);
			}
			lsp_status = lsp_process_next(context->lsp_handle);
		}
		else if (LSP_REQUIRES_RECEIVE == lsp_status)
		{
			DWORD txlen, txflags = 0;
			DWORD txindex = wscontext->next_transfer_index++;
			lsp_transfer_context_t* txcontext = &wscontext->lsp_transfer_context[txindex];
			LPWSAOVERLAPPED overlapped = &txcontext->overlapped;

			memset(&txcontext->overlapped, 0, sizeof(WSAOVERLAPPED));
			txcontext->wsabuf[0].buf = (char*) lsp_get_buffer_to_receive(
				context->lsp_handle, 
				&txcontext->wsabuf[0].len);

			InterlockedIncrement(&wscontext->outstanding_transfers);

			lsptest_winsock_debug("[%d] Receiving %d bytes\n", 
				txcontext->index, 
				txcontext->wsabuf[0].len);

			ret = WSARecv(
				wscontext->socket, 
				&txcontext->wsabuf[0],
				1, 
				&txlen, 
				&txflags,
				&txcontext->overlapped, 
				lsptest_winsock_transfer_completion);

			if (SOCKET_ERROR == ret && WSA_IO_PENDING != WSAGetLastError())
			{
				lsptest_winsock_debug("WSARecv failed with error %d\n", WSAGetLastError());
				return SOCKET_ERROR;
			}
			if (0 == ret)
			{
				lsptest_winsock_debug("Received %d bytes already\n", txlen);
			}
			lsp_status = lsp_process_next(context->lsp_handle);
		}
		else if (LSP_REQUIRES_SYNCHRONIZE == lsp_status)
		{
			while (wscontext->outstanding_transfers > 0)
			{
				if (nonwait) 
				{
					return 1;
				}
				else
				{
					DWORD waitResult = SleepEx(INFINITE, TRUE);
					_ASSERTE(waitResult == WAIT_IO_COMPLETION);
				}
			}
			wscontext->next_transfer_index = 0;
			lsp_status = wscontext->lsp_context.lsp_status;
		}
		else
		{
			context->lsp_status = lsp_status;
			break;
		}
	}

	return 0;
}

int
lsptest_transport_mpath_wait()
{
	return SleepEx(INFINITE, TRUE);
}

/*++

 returns the connected socket to the device
 and hostaddr fills with the connected host address 

--*/
SOCKET 
lsptest_winsock_connect_to_device(
	__in const struct sockaddr_lpx* devaddr,
	__out struct sockaddr_lpx* hostaddr)
{
	int i, j;
	int ret;
	BOOL connected;
	SOCKET s;
	SOCKET* cs_list;
	DWORD list_size;
	LPSOCKET_ADDRESS_LIST addrlist;
	u_long mode;
	int c;
	fd_set readfds, writefds, exceptfds;

	connected = FALSE;
	addrlist = NULL;
	s = WSASocket(AF_LPX, SOCK_STREAM, LPXPROTO_STREAM, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == s)
	{
		fprintf(stderr, "error: socket creation failed, error=0x%X\n", WSAGetLastError());
		return INVALID_SOCKET;
	}

	ret = WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addrlist, 0, &list_size, 0, 0);
	if (0 != ret && WSAEFAULT != WSAGetLastError())
	{
		fprintf(stderr, "error: WSAIoctl failed, error=0x%X\n", WSAGetLastError());
		closesocket(s);
		return INVALID_SOCKET;
	}

	lsptest_winsock_debug("number interfaces: %d\n", list_size);

	addrlist = malloc(list_size);
	if (NULL == addrlist)
	{
		fprintf(stderr, "error: memory allocation failed.\n");
		closesocket(s);
		return INVALID_SOCKET;
	}

	ret = WSAIoctl(s, SIO_ADDRESS_LIST_QUERY, 0, 0, addrlist, list_size, &list_size, 0, 0);
	if (0 != ret)
	{
		fprintf(stderr, "error: WSAIoctl failed, error=0x%X\n", WSAGetLastError());
		closesocket(s);
		return INVALID_SOCKET;
	}

	closesocket(s);

	cs_list = calloc(addrlist->iAddressCount, sizeof(SOCKET));
	if (0 == cs_list)
	{
		fprintf(stderr, "error: memory allocation failed\n");
		free(addrlist);
		return INVALID_SOCKET;
	}

	for (i = 0; i < addrlist->iAddressCount; ++i)
	{
		memcpy(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
		hostaddr->port = 0;
		hostaddr->sin_family = AF_LPX;

		// fprintf(stderr, "binding to %s\n", lpx_addr_str(hostaddr));

		cs_list[i] = WSASocket(
			AF_LPX, SOCK_STREAM, LPXPROTO_STREAM, 
			NULL, 0, WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == cs_list[i])
		{
			continue;
		}

		ret = bind(
			cs_list[i], 
			(const struct sockaddr*)hostaddr, 
			sizeof(*hostaddr));

		if (0 != ret)
		{
			fprintf(stderr, "bind(%s) failed, error=0x%x\n", 
				lpx_addr_str(hostaddr),
				WSAGetLastError());

			closesocket(cs_list[i]);
			cs_list[i] = INVALID_SOCKET;
			continue;
		}

		/* enable nonblocking socket */
		mode = 1;
		ret = ioctlsocket(cs_list[i], FIONBIO, &mode);

		if (0 != ret)
		{
			fprintf(stderr, "ioctlsocket(%s) failed, error=0x%X\n", 
				lpx_addr_str(hostaddr), WSAGetLastError());

			closesocket(cs_list[i]);
			cs_list[i] = INVALID_SOCKET;
			continue;
		}
	}

	c = 0;

	for (i = 0; i < addrlist->iAddressCount; ++i)
	{
		if (INVALID_SOCKET == cs_list[i])
		{
			continue;
		}

		ret = connect(
			cs_list[i],
			(const struct sockaddr*) devaddr, 
			sizeof(*devaddr));

		if (0 != ret && WSAEWOULDBLOCK != WSAGetLastError())
		{
			memcpy(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
			hostaddr->port = 0;
			hostaddr->sin_family = AF_LPX;

			fprintf(stderr, "connect(%s->", lpx_addr_str(hostaddr));
			fprintf(stderr, "%s) failed, error=0x%X\n",
				lpx_addr_str(devaddr), WSAGetLastError());

			closesocket(cs_list[i]);
			cs_list[i] = INVALID_SOCKET;
			continue;
		}

		++c;
	}

	if (0 == c)
	{
		// fprintf(stderr, "error: no more connectible sockets\n");
		free(cs_list);
		free(addrlist);
		return INVALID_SOCKET;
	}

	for (i = 0; i < addrlist->iAddressCount; ++i)
	{
		if (INVALID_SOCKET == cs_list[i])
		{
			continue;
		}

		memcpy(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
		hostaddr->port = 0;
		hostaddr->sin_family = AF_LPX;

		fprintf(stderr, "connect(%s->", lpx_addr_str(hostaddr));
		fprintf(stderr, "%s) pending\n", lpx_addr_str(devaddr));
	}

	s = INVALID_SOCKET;

	while (INVALID_SOCKET == s)
	{
		c = 0;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_ZERO(&exceptfds);

		for (i = 0; i < addrlist->iAddressCount; ++i)
		{
			if (INVALID_SOCKET != cs_list[i])
			{
				++c;
				FD_SET(cs_list[i], &readfds);
				FD_SET(cs_list[i], &writefds);
				FD_SET(cs_list[i], &exceptfds);
			}
		}

		if (0 == c)
		{
			// fprintf(stderr, "error: no more connectible sockets\n");
			free(cs_list);
			free(addrlist);
			return INVALID_SOCKET;
		}

		ret = select(0, NULL, &writefds, &exceptfds, NULL);
		if (SOCKET_ERROR == ret)
		{
			fprintf(stderr, "select() failed, error=0x%X\n", WSAGetLastError());
			free(cs_list);
			free(addrlist);
			return INVALID_SOCKET;
		}

		for (i = 0; i < addrlist->iAddressCount; ++i)
		{
			if (INVALID_SOCKET != cs_list[i])
			{
				if (FD_ISSET(cs_list[i], &exceptfds))
				{
					memcpy(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
					hostaddr->port = 0;
					hostaddr->sin_family = AF_LPX;

					fprintf(stderr, "connect(%s->", lpx_addr_str(hostaddr));
					fprintf(stderr, "%s) failed\n", lpx_addr_str(devaddr));

					closesocket(cs_list[i]);
					cs_list[i] = INVALID_SOCKET;
				}
				if (FD_ISSET(cs_list[i], &writefds))
				{

					s = cs_list[i];
					memcpy(hostaddr, addrlist->Address[i].lpSockaddr, sizeof(struct sockaddr_lpx));
					hostaddr->port = 0;
					hostaddr->sin_family = AF_LPX;

					cs_list[i] = INVALID_SOCKET;

					for (j = 0; j < addrlist->iAddressCount; ++j)
					{
						if (INVALID_SOCKET != cs_list[i])
						{
							closesocket(cs_list[i]);
							cs_list[i] = INVALID_SOCKET;
						}
					}
				}
			}
		}
	}

	free(cs_list);
	free(addrlist);

	return s;
}
