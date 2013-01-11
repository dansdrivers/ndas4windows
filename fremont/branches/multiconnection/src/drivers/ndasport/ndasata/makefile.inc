!IFNDEF TARGETMOFNAME
TARGETMOFNAME=$(TARGETNAME)
!ENDIF

mofcomp: $(O)\$(TARGETMOFNAME).bmf $(O)\$(TARGETMOFNAME).wmi.h

$(O)\$(TARGETMOFNAME).bmf: $(TARGETMOFNAME).mof
        mofcomp -B:$(O)\$(TARGETMOFNAME).bmf $(TARGETMOFNAME).mof
        wmimofck -h$(O)\$(TARGETMOFNAME).wmi.h -t$(O)\$(TARGETMOFNAME).vbs $(O)\$(TARGETMOFNAME).bmf

