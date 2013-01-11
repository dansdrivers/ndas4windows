mofcomp: busenum.bmf

busenum.bmf: busenum.mof
        mofcomp -B:busenum.bmf busenum.mof
        wmimofck busenum.bmf


sysplace:
	echo $(TARGETPATH)\$(TARGETNAME).$(TARGETEXT) ..\..\sys\$(BUILD_ALT_DIR)\
	rem copy $(TARGETPATH)\$(TARGETNAME).$(TARGETEXT) ..\..\sys\$(BUILD_ALT_DIR)\


