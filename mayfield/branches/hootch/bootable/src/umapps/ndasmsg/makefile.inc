!IF 0
Ignore error on _ndasmsg.mc(0): null byte at offset 3.
Build cannot handle unicode files.
!ENDIF

MSXSL=$(NDAS_TOOLS)\msxsl.exe

msgcomp: $(O)\_ndasmsg.mc $(O)\_ndasmsg.rc

$(O)\_ndasmsg.mc: ndasmsg.xml msgxml.xsl
	$(MSXSL) -o $@ $**

$(O)\_ndasmsg.rc: $(O)\_ndasmsg.mc
    $(MC) -h $(PASS0_HEADERDIR) -r $(MC_SOURCEDIR) $(MC_FLAGS) $**

$(NDAS_PUBLIC_INC_PATH)\ndas\ndasmsg.h: $(O)\_ndasmsg.h
	copy /y $** $@
