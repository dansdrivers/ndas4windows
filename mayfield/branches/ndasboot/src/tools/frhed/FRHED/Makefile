#CANNOT compile properly yet - _pgmptr seems to resolve to NULL, causing segfaults on exit, help & context menu
#This can be resolved if you link with crt1.o instead of crt2.o
#To do so alter your specs file (under /lib/gcc-lib somewhere) as follows
#Under the line '*startfile:' replace 'crt2' with 'crt1'

#Headers below are in the /usr/include/w32api cygwin directory

#The w32api header objidl.h mistakenly says that the STGMEDIUM structure
#contains a LPCSTR member called lpszFileName. That member should be a LPOLESTR

#If the shlwapi.h is missing from cygwin then use http://doc.ddart.net/msdn/header/include/shlwapi.h.html
#Extract the text from this html file & save it in shlwapi.h (The above url contains all the Win32 SDK headers)

#Also you need htmlhelp.h (& .lib) from HTMLHelp Workshop from Micro$oft

COLON=;
VPATH = .$(COLON)../help
srcdir = $(VPATH)

OBJ=.o
EXE=.exe

LIBS = ole32 uuid comctl32 shlwapi crtdll msvcrt htmlhelp oleaut32
INPUT = diagbox gktools gtools hexwnd ido ids idt main PDrive95 PDriveNT PhysicalDrive PMemoryBlock precomp toolbar InvokeHtmlHelp script1
OBJS = $(addsuffix $(OBJ),$(INPUT))

#Add -ggdb3 for GDB source level debugging with macros
#Add -gcoff for M$VC debugging (won't compile yet) - main.s:60337: Fatal error: C_EFCN symbol out of scope
#Other useful flags -H show path to headers -v compiler version & other stuff -Wall all warnings -print-multi-lib prints the libs used
CXXFLAGS:=$(CXXFLAGS) -fvtable-thunks -D_WIN32_IE=0xffff
LDFLAGS:=$(LDLAGS) -mno-cygwin -mwindows

.PHONY: all
all : frhed$(EXE)

frhed$(EXE) : $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(addprefix -l,$(LIBS))


#Useful flags for windres -v verbose mode --version show version
#Remove syntax errors caused by M$VC - dlg fonts have some numbers after them | Build	
script1.o : script1.rc resource.h icon1.ico
	sed -e 's/FONT 8, "MS Sans Serif", 0, 0, 0x1/FONT 8, "MS Sans Serif"/' $< | windres -o $@

.PHONY: clean

clean:
	rm -f frhed frhed$(EXE) frhed$(HLP) $(OBJS)