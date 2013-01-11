CREATECAT_CMD=$(NDAS_TOOLS)\create_catalog.cmd
CREATECDF_CMD=cscript //nologo $(NDAS_TOOLS)\create_cdf.js

!IFNDEF CATNAME
CATNAME=$(TARGETNAME)
!ENDIF

catalog: $(O)\$(CATNAME).cat

$(O)\$(CATNAME).cat: $(O)\$(CATNAME).cdf $(CATSOURCES)
	set BUILDMSG=Creating $@
	$(CREATECAT_CMD) $(O)\$(CATNAME).cdf
!IFDEF SIGNCODE_CMD
	$(SIGNCODE_CMD) $(O)\$(CATNAME).cat
!ENDIF
	$(BINPLACE_CMD)

$(O)\$(CATNAME).cdf: $(CATSOURCES)
	$(CREATECDF_CMD) /o:$(O)\$(CATNAME).cdf $(CATSOURCES)
