MSXSL=$(NDAS_TOOLS)\msxsl.exe
MC=mc.exe
MC_OPTIONS=-suc
	
clean:
	del /q _ndasmsg.h _ndasmsg.mc _ndasmsg.rc _ndasmsg*.bin

_ndasmsg.h: _ndasmsg.mc

_ndasmsg.rc: _ndasmsg.mc 
	$(MC) $(MC_OPTIONS) $*.mc
	
_ndasmsg.mc: ndasmsg.xml string2mc.xsl
	$(MSXSL) -o $@ 	$**

$(NDAS_INC_PATH)\ndas\ndasmsg.h: _ndasmsg.h
	copy /y $** $@

