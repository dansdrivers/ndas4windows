#
# WSHLPX32 is a 32-bit DLL required for AMD64 distributions
#
!IF "$(_BUILDARCH)" == "AMD64"

wshlpx32: $(NDASTREE)\wshlpx32.dll

$(NDASTREE)\wshlpx32.dll: $(NDASX86TREE)\wshlpx.dll
	set BUILDMSG=Copying $** -^> $@
	@copy /y $** $@ > nul

!ELSE

wshlpx32:

!ENDIF
