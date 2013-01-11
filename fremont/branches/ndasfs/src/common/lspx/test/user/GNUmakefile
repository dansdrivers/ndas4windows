LSPX_INC_DIR=../../../inc
LSPX_LIB_DIR=../..
CC = gcc
CFLAGS = -g -I . -I $(LSPX_INC_DIR) -I $(LSPX_INC_DIR)/lspx -O2

OBJS=\
	lsptest_cmds.o \
	lsptest.o \
	mbr.o

ifeq ($(LSPX_HOST), xplat)
OBJS+=lsptest_xplatsock.o 
LIBS+=$(LSPX_LIB_DIR)/libndasum.a -lpthread
CFLAGS+=-I ~/xplat/inc -DLINUX -DLINUXUM -D_X86 -D__LITTLE_ENDIAN_BITFIELD
else
LIBS=$(LSPX_LIB_DIR)/liblspx.a
OBJS+=lsptest_macsock.o
endif

all: lsptest

lsptest: $(OBJS) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	-rm *.o *~ core paper
