mofcomp: $(O)\busenum.bmf

$(O)\busenum.bmf: busenum.mof
        mofcomp -B:$(O)\busenum.bmf busenum.mof
        wmimofck $(O)\busenum.bmf
