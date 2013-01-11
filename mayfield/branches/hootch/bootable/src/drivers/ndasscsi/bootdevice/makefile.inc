cdfcopy: $(CDFFILE)

$(CDFFILE): $(CDFSRCFILE)
	copy /y $(CDFSRCFILE) $(CDFFILE) 
