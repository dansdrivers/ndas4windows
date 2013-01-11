LSPX_INC_DIR=../../../inc
LSPX_LIB_DIR=../..
CC = gcc
CFLAGS = -g -I . -I $(LSPX_INC_DIR) -I $(LSPX_INC_DIR)/lspx
LIBS=$(LSPX_LIB_DIR)/liblspx.a
OBJS=lsptest_macsock.o lsptest_cmds.o lsptest.o mbr.o

all: lsptest

lsptest: $(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm *.o *~ core paper
