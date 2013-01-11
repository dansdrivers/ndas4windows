$(O)\$(CDFFILENAME).cdf: $(CDFSRCNAME).cdf
	copy /y $** $@

$(O)\$(INFFILENAME).inf: $(O)\$(TARGETNAME).sys $(INFSRCNAME).inf
	$(NDAS_TOOLS)\setinfver $** $@

