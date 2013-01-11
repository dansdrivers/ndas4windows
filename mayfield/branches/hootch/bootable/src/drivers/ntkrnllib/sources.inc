MSC_WARNING_LEVEL=-W3 -WX

INCLUDES=$(NDAS_DRIVER_INC_PATH);$(NDAS_DRIVER_INC_PATH)\kernel;$(NDAS_INC_PATH)

SOURCES=	ntkrnlapi.c \
			init.c \
			time.c \
			timer.c \
			descriptor.c \
			malloc.c \
			irq.c
