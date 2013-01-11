$(O)\lfsfilt.cdf: lfsfilt.cdf
	copy /y $** $@

$(O)\lfsflt2k.cdf: lfsflt2k.cdf
	copy /y $** $@
	
$(O)\$(TARGETNAME).inf: $(O)\$(TARGETNAME).sys $(TARGETNAME).inf
	$(NDAS_TOOLS)\setinfver $** $@

$(O)\lfsflt2k.inf: $(O)\$(TARGETNAME).sys lfsflt2k.inf
	$(NDAS_TOOLS)\setinfver $** $@

