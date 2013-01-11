!IF 0
Ignore error on _ndasmsg.mc(0): null byte at offset 3.
Build cannot handle unicode files.
!ENDIF

MSXSL=$(NDAS_TOOLS)\msxsl.exe

$(O)\_ndasmsg.mc: ndasmsg.xml msgxml.xsl
	$(MSXSL) -o $@ 	$**

$(NDAS_INC_PATH)\ndas\ndasmsg.h: $(O)\_ndasmsg.h
	copy /y $** $@


