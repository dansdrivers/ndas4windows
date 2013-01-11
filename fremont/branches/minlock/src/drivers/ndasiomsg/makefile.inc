!IF 0
Ignore error on _ndasmsg.mc(0): null byte at offset 3.
Build cannot handle unicode files.
!ENDIF

MSXSL=$(NDAS_TOOLS)\msxsl.exe

$(O)\_ndasiomg.h: $(O)\_ndasiomg.mc

$(O)\_ndasiomg.mc: ndasiomg.xml msgxml.xsl
	$(MSXSL) -o $@ 	$**
	
