#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>
#include "lsptest_transport.h"

#define sock_printf
/* #define sock_printf printf */

#define LSP_TEST_NON_BLOCKING

#define PF_LPX 127  /* TEMP - move to socket.h */
#define AF_LPX PF_LPX   /* If it ain't broke... */
#define LPX_NODE_LEN    6

typedef unsigned char u_char;

union lpx_host {
  u_char  c_host[6];
  u_short s_host[3];
};

union lpx_net {
  u_char  c_net[4];
  u_short s_net[2];
};

union lpx_net_u {
  union   lpx_net net_e;
  u_long      long_e;
};

struct lpx_addr {
  union {
    struct {
      union lpx_net   ux_net;
      union lpx_host  ux_host;
    } i;
    struct {
      u_char  ux_zero[4];
      u_char  ux_node[LPX_NODE_LEN];
    } l;
  } u;
  u_short     x_port;
}__attribute__ ((packed));

struct sockaddr_lpx {
  u_char      slpx_len;
  u_char      slpx_family;
  union {
    struct {
      u_char      uslpx_zero[4];
      unsigned char   uslpx_node[LPX_NODE_LEN];
      unsigned short  uslpx_port;
      u_char      uslpx_zero2[2];
    } n;
    struct {
      struct lpx_addr usipx_addr;
      char        usipx_zero[2];
    } o;
  } u;
}__attribute__ ((packed));

#define slpx_port   u.n.uslpx_port
#define slpx_node   u.n.uslpx_node

typedef uint8_t u_char;
typedef int32_t SOCKET;

typedef struct _lsp_socket_context_t *lsp_socket_context_t_ptr;

typedef struct _lsp_transfer_context_t {
	lsp_socket_context_t_ptr socket_context;
	char *buffer_send, *buffer_recv;
	int len_buffer;
} lsp_transfer_context_t;

typedef struct _lsp_socket_context_t {
	lsptest_context_t test_context;
	SOCKET socket;
	int transfer_contexts_queued;
	lsp_transfer_context_t lsp_transfer_context[LSP_MAX_CONCURRENT_TRANSFER];
#ifdef LSPTEST_USE_COALESCING
	lsp_transfer_context_t* deferred_txcontext;
#endif
} lsp_socket_context_t;

const char* lpx_addr_node_str(const unsigned char* nodes)
{
        static char buf[30] = {0};
        snprintf(buf, 30, "%02X:%02X:%02X:%02X:%02X:%02X",
                nodes[0], nodes[1], nodes[2], nodes[3], nodes[4], nodes[5]);
        return buf;
}       
                
const char* lpx_addr_str(const struct sockaddr_lpx* lpx_addr)
{       
        return lpx_addr_node_str(lpx_addr->slpx_node);
}               
                
int 
lsptest_transport_static_initialize()
{
	return 0;
}

void 
lsptest_transport_static_cleanup()
{

}

lsptest_context_t*
lsptest_transport_create()
{
	lsp_socket_context_t* scontext;
	
	scontext = (lsp_socket_context_t*) malloc(sizeof(lsp_socket_context_t));
	if (0 == scontext)
	{
		return 0;
	}

	memset(scontext, 0, sizeof(lsp_socket_context_t));

	return &scontext->test_context;
}

void lsptest_transport_delete(
	__in lsptest_context_t* context)
{
	free(context);
}

int lsptest_transport_connect(
	__in lsptest_context_t* context,
	__in_bcount(6) unsigned char* ndas_dev_addr)
{
	lsp_socket_context_t* scontext = (lsp_socket_context_t*) context;
	int ret;
	int s;
	struct sockaddr_lpx devaddr;
	struct sockaddr_lpx hostaddr;
	struct timeval timeout;
	socklen_t optlen;

	memset(&devaddr, 0, sizeof(struct sockaddr_lpx));
	devaddr.slpx_family = AF_LPX;
	devaddr.slpx_len = sizeof(struct sockaddr_lpx);
	devaddr.u.n.uslpx_port = htons(10000);
	memcpy(devaddr.u.n.uslpx_node, ndas_dev_addr, sizeof(devaddr.u.n.uslpx_node));
	scontext->socket = connect_to_device(&devaddr, &hostaddr);

	if (-1 == scontext->socket)
	{
		return -1;
	}

	memset(&timeout, 0, sizeof(timeout));
	timeout.tv_sec = 30;
	optlen = sizeof(timeout);

	ret = setsockopt(scontext->socket,
			   SOL_SOCKET,
			   SO_SNDTIMEO,
			   &timeout,
			   optlen);

	if (-1 == ret)
	{
		fprintf(stderr, "warning: SO_SNDTIMEO failed, err=%d, %s\n",
				errno, strerror(errno));
	}

	timeout.tv_sec = 30;
	optlen = sizeof(timeout);

	ret = setsockopt(scontext->socket,
			   SOL_SOCKET,
			   SO_RCVTIMEO,
			   &timeout,
			   optlen);

	if (-1 == ret)
	{
		fprintf(stderr, "warning: SO_RCVTIMEO failed, err=%d, %s\n",
				errno, strerror(errno));
	}

	return 0;
}

int lsptest_transport_disconnect(
	__in lsptest_context_t* context)
{

}

int complete_lsp_tranfer_context(
	lsp_transfer_context_t *transfer_context)
{
	int ret = 0;
	int is_send = 0;
	SOCKET socket;
	char *buffer = NULL;
	int len_buffer = 0;

	struct timeval tv;
	fd_set sock_set;
	int valopt;
	socklen_t lon;

	FD_ZERO(&sock_set);

	if (NULL == transfer_context)
	{
		sock_printf("invalid lsp_transfer_context\n");
		return -1;
	}

	socket = transfer_context->socket_context->socket;
	len_buffer = transfer_context->len_buffer;

	if (transfer_context->buffer_send)
	{
		is_send = 1;
		buffer = transfer_context->buffer_send;

		sock_printf("completing send(%d, %p, %d)\n", socket, buffer, len_buffer);
	}
	else if (transfer_context->buffer_recv)
	{
		is_send = 0;
		buffer = transfer_context->buffer_recv;

		sock_printf("completing recv(%d, %p, %d)\n", socket, buffer, len_buffer);
	}
	else
	{
		sock_printf("bad lsp_transfer_context\n");
		return -1;
	}

	while (len_buffer > 0)
	{
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		FD_ZERO(&sock_set);
		FD_SET(socket, &sock_set); // insert s to sock_set for select()
		ret = select(
			socket +1,
			(is_send) ? NULL : &sock_set,
			(is_send) ? &sock_set: NULL,
			NULL,
			&tv);
		if (-1 == ret)
		{
			sock_printf("select failed with %d. %s\n", errno, strerror(errno));
			return -1;
		}

		if (!FD_ISSET(socket, &sock_set))
		{
			sock_printf("sock_set is not set with socket\n");
			return -1;
		}

		lon = sizeof(int);
		ret = getsockopt(socket, SOL_SOCKET, SO_ERROR, &valopt, &lon);			
		if (-1 == ret)
		{
			sock_printf("getsockopt failed with %d. %s\n", errno, strerror(errno));
			return -1;
		}
		else if (0 != valopt)
		{
			sock_printf("recv failed with %d. %s\n", valopt, strerror(valopt));
			return -1;
		}
		else
		{
			/* synced */
			if (is_send)
			{
				ret = send(socket, buffer, len_buffer, 0);
				if (-1 == ret)
				{
					sock_printf("send(%d, %p, %d) failed\n", socket, buffer, len_buffer);
					return -1;
				}

				sock_printf("send(%d, %p, %d) complete\n", socket, buffer, len_buffer);

				buffer += ret;
				len_buffer -= ret;
			}
			else
			{
				ret = recv(socket, buffer, len_buffer, 0);
				if (-1 == ret)
				{
					sock_printf("recv(%d, %p, %d) failed\n", socket, buffer, len_buffer);
					return -1;
				}

				sock_printf("recv(%d, %p, %d) complete\n", socket, buffer, len_buffer);

				buffer += ret;
				len_buffer -= ret;
			}
		}
	}

	return 0;
}

int lsptest_transport_process_transfer(
	lsptest_context_t* context)
{
	lsp_socket_context_t* scontext = (lsp_socket_context_t*) context;
	int ret;
	static int sc = 0;
	char *buffer;
	lsp_uint32_t len_buffer;
	int i;
	int txerr;

	lsp_status_t lsp_status = context->lsp_status;

	scontext->transfer_contexts_queued = 0;

	for (;; lsp_status = lsp_process_next(context->lsp_handle))
	{
		if (LSP_REQUIRES_SEND == lsp_status)
		{
			buffer = lsp_get_buffer_to_send(
			  context->lsp_handle, &len_buffer);

			sock_printf("[%d] send(%d, %p, %d)\n",
			  ++sc, scontext->socket, buffer, len_buffer);

			txerr = 0;
			if (0 == scontext->transfer_contexts_queued)
			{
				/* we initiate send only if there are no pending tx */
				ret = send(scontext->socket, buffer, len_buffer, 0);
				if (-1 == ret)
				{
					txerr = errno;
				}
			}
			else
			{
				txerr = EAGAIN;
			}

			if (EAGAIN == txerr)
			{
				lsp_transfer_context_t *tc;
				
				sock_printf("send pending in queue=%d\n",
					   scontext->transfer_contexts_queued);
				
				tc = &scontext->lsp_transfer_context[scontext->transfer_contexts_queued++];
				memset(tc, 0, sizeof(lsp_transfer_context_t));
				tc->socket_context = scontext;
				tc->buffer_send = buffer;
				tc->len_buffer = len_buffer;
			}
			else if (0 != txerr)
			{
				sock_printf("Sending %d bytes failed, error=%d, %s\n", 
					   len_buffer, errno, strerror(errno));
				return -1;
			}
		}
		else if (LSP_REQUIRES_RECEIVE == lsp_status)
		{
			buffer = lsp_get_buffer_to_receive(
			  context->lsp_handle, &len_buffer);

			sock_printf("[%d] recv(%d, %p, %d)\n", 
			  ++sc, scontext->socket, buffer, len_buffer);

			txerr = 0;
			if (0 == scontext->transfer_contexts_queued)
			{
				/* we initiate recv only if there are no pending tx */
				ret = recv(scontext->socket, buffer, len_buffer, 0);
				if (-1 == ret)
				{
					txerr = errno;
				}
			}
			else
			{
				txerr = EAGAIN;
			}

			if (EAGAIN == txerr)
			{
				lsp_transfer_context_t *tc;
				
				sock_printf("recv pending in queue=%d\n",
					   scontext->transfer_contexts_queued);
				
				tc = &scontext->lsp_transfer_context[scontext->transfer_contexts_queued++];
				memset(tc, 0, sizeof(lsp_transfer_context_t));
				tc->socket_context = scontext;
				tc->buffer_recv = buffer;
				tc->len_buffer = len_buffer;
			}
			else if (0 != txerr)
			{
				sock_printf("Receiving %d bytes failed, error=%d, %s\n", 
					   len_buffer, errno, strerror(errno));
				return -1;
			}
		}
		else if (LSP_REQUIRES_SYNCHRONIZE == lsp_status)
		{
			sock_printf("[%d] sync, queued=%d\n", 
				   ++sc, scontext->transfer_contexts_queued);

			for (i = 0; i < scontext->transfer_contexts_queued; i++)
			{
				ret = complete_lsp_tranfer_context(
				  &scontext->lsp_transfer_context[i]);

				if (-1 == ret)
				{
					return -1;
				}
			}

			scontext->transfer_contexts_queued = 0;
		}
		else
		{
			/* sock_printf("LSP:0x%x\n", lsp_status); */
			context->lsp_status = lsp_status;
			break;
		}		
	}

	return 0;
}

/*++

returns the connected socket(blocking or not) to the device
and hostaddr fills with the connected host address
__*/

SOCKET connect_to_device(
	const struct sockaddr_lpx *devaddr,
	struct sockaddr_lpx *hostaddr)
{
	int ret = 0;
	int i;
	int socket_count = 0;
	typedef struct _socket_ifa{
		int s; // socket
		struct ifaddrs *ifa; // backup matching ifa here. used to fill hostaddr
	} socket_ifa;
	int s = -1, s_connected = -1;
	socket_ifa *sockets = NULL;
	//	int s = -1, s_connected = -1, *sockets = NULL; // socket
	struct ifaddrs *ifap = NULL, *ifa = NULL; // to get if addresses

#ifdef LSP_TEST_NON_BLOCKING
	int flags;
	struct timeval tv;
	fd_set sock_set, sock_set_temp;
	int valopt;
	socklen_t lon;

	FD_ZERO(&sock_set);
#endif

	/* get interface addresses */
	/* allocate & create sockets as many as ifa of AF_LINK(which has mac address) */
	{
		ret = getifaddrs(&ifap);

		if (0 != ret)
		{
			ifap = NULL;
			goto out;
		}

		for (socket_count = 0, ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
		{
			if (AF_LINK != ifa->ifa_addr->sa_family)
				continue;
			socket_count++;
		}

		if (0 == socket_count)
		{
			sock_printf("no ifa with AF_LINK\n");
			goto out;
		}

		sockets = malloc(sizeof(socket_ifa) * socket_count);
		if (!sockets)
		{
			goto out;
		}
		memset(sockets, 0, sizeof(socket_ifa) * socket_count);
	}

	/* try all ifa in ifap */
	for (i = 0, ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (AF_LINK != ifa->ifa_addr->sa_family)
			continue;

		/* create socket */
		{
			s = socket(AF_LPX, SOCK_STREAM, 0);
			if (-1 == s)
			{
				sock_printf("Failed to create a socket with error %d. %s\n", errno, strerror(errno));
				goto out;
			}
			sockets[i].s = s;
			sockets[i].ifa = ifa;
			i++;

#ifdef LSP_TEST_NON_BLOCKING
			/* set socket to non-blocking */
			flags = fcntl(s, F_GETFL, 0);
			ret = fcntl(s, F_SETFL, flags | O_NONBLOCK);
			if (-1 == ret)
			{
				sock_printf("Failed to set flag O_NONBLOCK with error %d. %s\n", errno, strerror(errno));
				goto out;
			}
			//flags = fcntl(*soc, F_GETFL, 0);
#endif
		}

		/* initialize hostaddr */
		{
			hostaddr->slpx_family = AF_LPX;
			hostaddr->slpx_len = sizeof(struct sockaddr_lpx);
			hostaddr->slpx_port = htons(0);

			memset(hostaddr->slpx_node, 0, 6);
			if (0 == memcmp(hostaddr->slpx_node, &ifa->ifa_addr->sa_data[9], 6))
			{
				sock_printf("skipping [%s] 00:00:00:00:00:00\n", ifa->ifa_name);
				continue;
			}
			memcpy(hostaddr->slpx_node, &ifa->ifa_addr->sa_data[9], 6);
		}

		/* bind socket to hostaddr */
		{
			sock_printf("binding to [%s] %s\n", ifa->ifa_name, lpx_addr_str(hostaddr));

			ret = bind(s, (const struct sockaddr*)hostaddr, sizeof(struct sockaddr_lpx));

			if (ret < 0)
			{
				sock_printf("Bind failed with error %d. %s\n", errno, strerror(errno));
				continue;
			}

			sock_printf("binded to %s\n", lpx_addr_str(hostaddr));
		}

		/* connect socket to devaddr */
		{
			sock_printf("connecting to %s", lpx_addr_str(devaddr));
			sock_printf(" at %s\n", lpx_addr_str(hostaddr));

			ret = connect(s, (const struct sockaddr*) devaddr, sizeof(struct sockaddr_lpx));
			if (0 == ret)
			{
				/*
				We found the successful socket s.
				Make sure the socket is blocking!
				*/
				sock_printf("connected to %s", lpx_addr_str(devaddr));
				sock_printf(" at %s\n", lpx_addr_str(hostaddr));
				s_connected = s;

				goto out;
			}
			else
			{
				if (EINPROGRESS != errno)
				{
					sock_printf("Connect failed with error %d. %s\n", errno, strerror(errno));
					continue;
				}

#ifdef LSP_TEST_NON_BLOCKING
				FD_SET(s, &sock_set); // insert s to sock_set for select()
				continue;
#endif				
			}
		}
	}

#ifdef LSP_TEST_NON_BLOCKING
	tv.tv_sec = 5;
	tv.tv_usec = 0;

	/*
	If 1 or more socket connection were 'EINPROGRESS', we wait here using select()
	sock_set has the list of sockets to wait for.
	if selected socket does not return error with getsockopt, the socket is connected.
	*/
	while (1)
	{
		/* is sock_set empty? */
		FD_ZERO(&sock_set_temp);
		if (0 == memcmp(&sock_set, &sock_set_temp, sizeof(fd_set)))
		{
			goto out;
		}
		FD_COPY(&sock_set, &sock_set_temp);
		ret = select(sockets[socket_count -1].s +1, NULL, &sock_set, NULL, &tv);
		if (-1 == ret)
		{
			sock_printf("select failed with %d. %s\n", errno, strerror(errno));
			return -1;
		}
		for (i = 0; i < socket_count; i++)
		{
			s = sockets[i].s;
			ifa = sockets[i].ifa;
			if (0 == s || !FD_ISSET(s, &sock_set))
				continue;

			memcpy(hostaddr->slpx_node, &ifa->ifa_addr->sa_data[9], 6);

			sock_printf("connection from [%s] to %s is selected\n",
				ifa->ifa_name, lpx_addr_str(hostaddr));

			lon = sizeof(int);
			ret = getsockopt(s, SOL_SOCKET, SO_ERROR, &valopt, &lon);

			if (-1 == ret)
			{
				sock_printf("getsockopt failed with %d. %s\n", errno, strerror(errno));
			}
			else if (0 != valopt)
			{
				sock_printf("connection failed with %d. %s\n", valopt, strerror(valopt));
			}
			else
			{
				/*
				We found the successful socket s.
				Make sure the socket is non-blocking!
				*/
				sock_printf("connected to %s", lpx_addr_str(devaddr));

				sock_printf(" at %s\n", lpx_addr_str(hostaddr));
				s_connected = s;

				goto out;
			}

			FD_CLR(s, &sock_set_temp); /* we don't need to select this socket again */
		}

		/* revert sock_set for select() */
		FD_COPY(&sock_set, &sock_set_temp);
	}
#endif
out:

	if (sockets && socket_count > 0)
	{
		// save connected socket
		for (i = 0; i < socket_count; i++)
		{
			if (0 == sockets[i].s || s_connected == sockets[i].s)
				continue;

			close(sockets[i].s);
			sockets[i].s = 0;
			sockets[i].ifa = NULL;
		}
		free(sockets);
	}

	if (ifap)
	{
		freeifaddrs(ifap);
		ifap = NULL;
	}

	return s_connected;
}


