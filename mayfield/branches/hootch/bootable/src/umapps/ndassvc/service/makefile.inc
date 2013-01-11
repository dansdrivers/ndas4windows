mofcomp: $(O)\ndassvc.bmf

$(O)\ndassvc.bmf: ndassvc.mof
        mofcomp -B:$(O)\ndassvc.bmf ndassvc.mof
        wmimofck $(O)\ndassvc.bmf
