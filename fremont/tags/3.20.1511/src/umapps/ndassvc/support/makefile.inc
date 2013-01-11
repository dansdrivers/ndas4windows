..\lib\$(O)\$(TARGETNAME).lib: $(O)\$(TARGETNAME).lib
	if not exist ..\lib\$(O) mkdir ..\lib\$(O)
	copy /y $** $@
