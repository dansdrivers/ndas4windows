LSPX_INC_DIR=../inc
CC = gcc
CFLAGS = -I . -I $(LSPX_INC_DIR) -I $(LSPX_INC_DIR)/lspx

all: liblspx.a 

OBJS=lsp.o lsp_hash.o lsp_util.o

liblspx.a: $(OBJS)
	$(AR) -rc $@ $?

clean:
	-rm *.a *.o *~ core paper
