!message BUILDMSG: >>> Ignore error on _ndasmsg.mc(0): null byte at offset 3.
!message BUILDMSG: >>> Build cannot handle unicode files.

MSXSL=$(NDAS_TOOLS)\msxsl.exe

$(O)\_ndasmsg.mc: ndasmsg.xml string2mc.xsl
	$(MSXSL) -o $@ 	$**

$(NDAS_INC_PATH)\ndas\ndasmsg.h: $(O)\_ndasmsg.h
	copy /y $** $@


