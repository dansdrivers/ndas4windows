$(O)\$(TARGETNAME).cdf: ..\$(TARGETNAME).cdf
	copy /y $** $@
	
$(O)\$(TARGETNAME).inf: $(O)\$(TARGETNAME).sys ..\$(TARGETNAME).inf
	$(NDAS_TOOLS)\setinfver $** $@


