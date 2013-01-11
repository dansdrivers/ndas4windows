#ifndef LSPTEST_TRANSPORT_H_INCLUDED
#define LSPTEST_TRANSPORT_H_INCLUDED

#include <stdio.h>
#include <lspx/lsp.h>

typedef struct _lsptest_context_t {
    lsp_handle_t lsp_handle;
    lsp_status_t lsp_status;
} lsptest_context_t;

int 
lsptest_transport_static_initialize();

void 
lsptest_transport_static_cleanup();

lsptest_context_t*
lsptest_transport_create();

void
lsptest_transport_delete(
	__in lsptest_context_t* context);

int 
lsptest_transport_connect(
	__in lsptest_context_t* context,
	__in_bcount(6) unsigned char* ndas_dev_addr);

int 
lsptest_transport_disconnect(
	__in lsptest_context_t* context);

int 
lsptest_transport_process_transfer(
	__in lsptest_context_t* lsptest);

#endif /* LSPTEST_TRANSPORT_H_INCLUDED */
