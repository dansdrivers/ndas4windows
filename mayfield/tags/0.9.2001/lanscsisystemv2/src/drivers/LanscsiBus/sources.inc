!IFDEF LANSCSI_BUS

C_DEFINES = -D__LANSCSI_BUS__

!ENDIF

BROWSER_INFO=1
BROWSERFILE=$(TARGETNAME).bsc -n

MSC_WARNING_LEVEL=-W3 -WX




SOURCES=..\busenum.rc \
	..\busenum.c \
	..\pnp.c \
	..\power.c \
	..\buspdo.c \
	..\wmi.c \
	..\lanscsibus.c \
	..\lanscsiadapter.c \
	..\targetconnection.c \
	..\LanScsiOp.c \
	..\hash.c



