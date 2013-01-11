$(O)\$(TARGETNAME).inf: $(O)\$(TARGETNAME).sys $(TARGETNAME).inf
	$(NDAS_TOOLS)\setinfver $** $@

$(O)\lfsflt2k.inf: $(O)\$(TARGETNAME).sys lfsflt2k.inf
	$(NDAS_TOOLS)\setinfver $** $@

