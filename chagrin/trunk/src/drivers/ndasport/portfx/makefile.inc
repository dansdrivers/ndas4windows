LNG=$(LANGUAGE)
_INX=.
STAMP=stampinf -f $@ -a $(_BUILDARCH)

$(OBJ_PATH)\$(O)\$(INF_NAME).inf: $(_INX)\$(INF_NAME).inx 
    copy $(_INX)\$(@B).inx $@
    $(STAMP)

mofcomp: ndasportfx.bmf

ndasportfx.bmf: ndasportfx.mof
        mofcomp -B:$(OBJ_PATH)\$O\ndasportfx.bmf ndasportfx.mof
        wmimofck $(OBJ_PATH)\$O\ndasportfx.bmf
