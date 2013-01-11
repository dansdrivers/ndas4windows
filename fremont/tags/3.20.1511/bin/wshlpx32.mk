PUBDIR=..\publish

TARGETFILES=
TARGETFILES=$(TARGETFILES) $(PUBDIR)\chk\amd64\wshlpx32.dll
TARGETFILES=$(TARGETFILES) $(PUBDIR)\fre\amd64\wshlpx32.dll

all: $(TARGETFILES)

$(PUBDIR)\chk\amd64\wshlpx32.dll: $(PUBDIR)\chk\i386\wshlpx.dll
	@echo Copying $** -^> $@
	@copy /y $** $@ > nul

$(PUBDIR)\fre\amd64\wshlpx32.dll: $(PUBDIR)\fre\i386\wshlpx.dll
	@echo Copying $** -^> $@
	@copy /y $** $@ > nul

