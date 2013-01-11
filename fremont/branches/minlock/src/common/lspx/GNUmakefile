LSPX_INC_DIR=../inc
CC = gcc
CFLAGS = -I . -I $(LSPX_INC_DIR) -I $(LSPX_INC_DIR)/lspx

all: liblspx.a 

HEADERS=\
	$(LSPX_INC_DIR)/lspx/lsp.h \
	$(LSPX_INC_DIR)/lspx/lsp_host.h \
	$(LSPX_INC_DIR)/lspx/lsp_spec.h \
	$(LSPX_INC_DIR)/lspx/lsp_type.h \
	$(LSPX_INC_DIR)/lspx/lsp_hash.h \
	$(LSPX_INC_DIR)/lspx/lsp_ide_def.h \
	$(LSPX_INC_DIR)/lspx/lsp_util.h \
	$(LSPX_INC_DIR)/lspx/lspspecstring.h

SRCS=lsp.c lsp_hash.c lsp_util.c
OBJS=lsp.o lsp_hash.o lsp_util.o

liblspx.a: $(OBJS)
	$(AR) -rc $@ $?

clean:
	$(RM) *.a *.o *~ core paper

TAGS: $(SRCS) $(HEADERS)
	etags $(SRCS) $(HEADERS)

