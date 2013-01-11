#
# Enable NDAS booting
#

NDASBOOT=ndasboot

!IFDEF NDASBOOT
C_DEFINES = -DDEBUG -D__NDASBOOT__ -DSK_USE_CSUM -DSK_DIAG_SUPPORT -DGENESIS -DCONFIG_SK98LIN_ZEROCOPY -DYUKON -DYUK2 $(C_DEFINES)
!ENDIF

MSC_WARNING_LEVEL=-W3 -WX

INCLUDES=$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel;$(NDAS_INC_PATH)

SOURCES=	debug.c \
			ndasboot.c \
			pci.c \
			skbuff.c \
			iov.c \
			sock.c \
			rtl8139.c \
			natsemi.c \
			pcnet32.c \
			tg3.c \
			skge.c \
			skgeinit.c \
			sktimer.c \
			skrlmt.c \
			skaddr.c \
			skgepnmi.c \
			skdim.c \
			ski2c.c \
			sky2.c \
			sky2le.c \
			skqueue.c \
			skcsum.c \
			skgesirq.c \
			skxmac2.c \
			skgehwt.c \
			sklm80.c \
			skvpd.c \
			netdevice.c \
			af_lpx.c
