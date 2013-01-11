$(O)\$(INFFILENAME).inf: $(INFFILENAME).inf
	copy /y $** $@ 

$(O)\$(INFFILENAME).cat: $(INFFILENAME).cat
	copy /y $** $@ 

#clean:
#    del aha154dt.h aha154x.bmf

#aha154dt.h: aha154x.bmf
#    wmimofck -haha154dt.h aha154x.bmf

