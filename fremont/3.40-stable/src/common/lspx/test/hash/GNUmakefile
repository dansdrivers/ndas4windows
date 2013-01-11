LSPX_INC_DIR=../../../inc
LSPX_LIB_DIR=../..
CC = gcc
CFLAGS = -g -I . -I $(LSPX_INC_DIR) -I $(LSPX_INC_DIR)/lspx
LIBS=$(LSPX_LIB_DIR)/liblspx.a
OBJS=lsp_hash_test.o 

all: lsp_hash_test

lsp_hash_test: $(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm *.o *~ core paper
