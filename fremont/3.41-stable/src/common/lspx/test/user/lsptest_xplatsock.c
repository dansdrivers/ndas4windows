#include <stdlib.h>
#include "lpx/lpxutil.h"
#include "lsptest_transport.h"

typedef struct _lsp_socket_context_t {
    lsptest_context_t lsp_context;
    /* transfer context */
    int socket;
} lsp_socket_context_t;

int 
lsptest_transport_static_initialize()
{
	sal_init();
	ndas_init(0,0,0);
/*	dpc_start(50);
	lpx_init(64); */
	lpx_register_dev("eth0");
	lpx_register_dev("eth1");

    return 0;
}

void 
lsptest_transport_static_cleanup()
{
	lpx_unregister_dev("eth1");
	lpx_unregister_dev("eth0");
	ndas_cleanup();
	sal_cleanup();	
}

lsptest_context_t*
lsptest_transport_create()
{
    /* allocate a context and initialize the context data such as
       creating a socket */

    lsp_socket_context_t* scontext;

    scontext = malloc(sizeof(lsp_socket_context_t));
    if (!scontext)
    {
	/* out of memory */
	return NULL;
    }	
	memset(scontext, 0, sizeof(lsp_socket_context_t));

    return (lsptest_context_t*)scontext;
}

void
lsptest_transport_delete(
    __in lsptest_context_t* context)
{
    /* clean up context data and deallocate the context */
	free(context);
}

int 
lsptest_transport_connect(
    __in lsptest_context_t* context,
    __in_bcount(6) unsigned char* ndas_dev_addr)
{
	lsp_socket_context_t* scontext = (lsp_socket_context_t*) context;
	struct sockaddr_lpx slpx;
	int sockfd;
	ndas_error_t res;

	scontext->socket = sockfd = lpx_socket(LPX_TYPE_STREAM, 0);
	if (sockfd < 0) {
		return -1;
	}
	memset(&slpx, 0, sizeof(slpx));
	slpx.slpx_family = AF_LPX;
	res = lpx_bind(sockfd, &slpx, sizeof(slpx));
	if (!NDAS_SUCCESS(res)) {
		return -2;
	}
	memcpy(slpx.slpx_node, ndas_dev_addr, sizeof(slpx.slpx_node));
	slpx.slpx_port = htons(10000);

	res = lpx_connect(sockfd, &slpx, sizeof(slpx));
	if (!NDAS_SUCCESS(res)) {
		return -3;
	}

	res = lpx_set_rtimeout(sockfd, 1000);
	if (!NDAS_SUCCESS(res)) {
		return -4;
	}
	res = lpx_set_wtimeout(sockfd, 1000);
	if (!NDAS_SUCCESS(res)) {
		return -5;
	}

    return 0;
}

int 
lsptest_transport_disconnect(
    __in lsptest_context_t* context)
{
    /* disconnect from the ndas device */
	lsp_socket_context_t* scontext = (lsp_socket_context_t*) context;
	lpx_close(scontext->socket);
    return 0;
}

int 
lsptest_transport_process_transfer(
    __in lsptest_context_t* context)
{
    int ret;
    lsp_socket_context_t* scontext = (lsp_socket_context_t*) context;
    char *buffer;
    lsp_uint32_t len_buffer;
    int sockfd = scontext->socket;

    for (;; 
	 context->lsp_status = lsp_process_next(context->lsp_handle))
    {
	if (LSP_REQUIRES_SEND == context->lsp_status)
	{
	    /* lspx's call to send */
	    buffer = lsp_get_buffer_to_send(
		context->lsp_handle, &len_buffer);

	    ret = lpx_send(scontext->socket, buffer, len_buffer, 0);
	    if (len_buffer != ret)
	    {
		/* send failed */
		return -1;
	    }
	}
	else if (LSP_REQUIRES_RECEIVE == context->lsp_status)
	{
	    /* lspx's call to receive */
	    buffer = lsp_get_buffer_to_receive(
		context->lsp_handle, &len_buffer);
           
	    ret = lpx_recv(scontext->socket, buffer, len_buffer, 0);
	    if (len_buffer != ret)
	    {
		/* receive failed */
		return -1;
	    }
	}
	else if (LSP_REQUIRES_SYNCHRONIZE == context->lsp_status)
	{
	    /* wait until all pending tx/rx are completed */
	    /* nothing to do on synchronous socket */
	}
	else
	{
	    /* all other return values indicate the completion or
	       error of the process */

	    return (context->lsp_status == LSP_STATUS_SUCCESS) ? 0 : -2;
	}		
    }

    /* unreachable here */
}

