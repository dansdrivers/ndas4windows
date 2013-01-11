NDAS_SDK_INC_PATH=..\..\inc
CFLAGS=/I$(NDAS_SDK_INC_PATH) -DUNICODE -D_UNICODE -D_DEBUG
LIBS=user32.lib
LINK=link.exe
LINK_OPTIONS=/nologo /DEBUG

all: msgtest.exe

clean:
	@echo -

msgtest.exe: msgtest.obj
	$(LINK) $(LINK_OPTIONS) $*.obj $(LIBS)
