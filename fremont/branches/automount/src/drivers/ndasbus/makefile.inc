mofcomp: $(O)\busenum.bmf

$(O)\busenum.bmf: busenum.mof
        mofcomp -B:$(O)\busenum.bmf busenum.mof
        wmimofck $(O)\busenum.bmf

$(O)\$(CDFFILENAME).cdf: $(CDFSRCNAME).cdf
	copy /y $** $@

$(O)\$(INFFILENAME).inf: $(O)\$(TARGETNAME).sys $(INFSRCNAME).inf
	$(NDAS_TOOLS)\setinfver $** $@
