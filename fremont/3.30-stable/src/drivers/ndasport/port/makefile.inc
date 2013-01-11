mofcomp: $(O)\$(TARGETNAME).bmf $(O)\$(TARGETNAME).wmi.h

$(O)\$(TARGETNAME).bmf: $(TARGETNAME).mof
        mofcomp -B:$(O)\$(TARGETNAME).bmf $(TARGETNAME).mof
        wmimofck -h$(O)\$(TARGETNAME).wmi.h -t$(O)\$(TARGETNAME).vbs $(O)\$(TARGETNAME).bmf

