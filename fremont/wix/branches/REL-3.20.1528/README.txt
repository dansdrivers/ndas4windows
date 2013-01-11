
NDAS Setup on WiX

June 6, 2006 Chesong Lee

Additional Comments

March 1, 2006 Chesong Lee

Build system for the wix is changed to use MS Build, that is included
in Microsoft .NET Framework 2.0.

June 30, 2005 Chesong Lee

NDAS System Components Merge Module is based on WiX.
The Windows Installer XML (WiX) is a toolset that builds Windows
installation packages from XML source code. For more details on WiX,
go to http://wix.sourceforge.net/

** HOW TO BUILD

1. Copy files from the main project publish directory into sourcedir

   e.g. robocopy c:\mayfield\publish\fre\i386 sourcedir\i386 /xd *.pdb /xf *.cdf
   

2. Run "build_publish_x86.cmd" to build the setup for x86 platform.

3. Output MSI and MSM files are located in .\publish directory

							-------------
							Prerequisites
							-------------

* Microsoft .NET Framework 2.0 (New Requirement for msbuild)

  x86 version:

    http://www.microsoft.com/downloads/details.aspx?familyid=0856eacb-4362-4b0d-8edd-aab15c5e04f5

  x64 version:

  	http://www.microsoft.com/downloads/details.aspx?familyid=B44A0000-ACF8-4FA1-AFFB-40E78D788B00

* Microsoft .NET Framework 1.1 Service Pack 1 or higher

  http://www.microsoft.com/downloads/details.aspx?familyid=A8F5654F-088E-40B2-BBDB-A83353618B38

  (or \\liah\public\devel\redist\NDP1.1sp1-KB867460-X86.exe )

* Microsoft .NET Framework 1.1 Service Pack 1 for Windows Server 2003

  http://www.microsoft.com/downloads/details.aspx?familyid=AE7EDEF7-2CB7-4864-8623-A1038563DF23

  (or \\liah\public\devel\redist\WindowsServer2003-KB867460-X86-ENU.EXE )

* Windows Installer XML (WiX) toolset (included in bin\wix-2.0.3309.0)

  http://wix.sourceforge.net

  included under bin\wix-2.0.3309.0 directory (Currently tested version)

							 ------------
							 BUILD ISSUES
							 ------------

* If you have a following error during compilation, .NET Framework 1.1 SP1 
  is not installed. Install SP1 will solve the problem.

retail\candle.exe : error CNDL0001 : Cannot have '?>' inside an XML processing instruction.
NMAKE :  U1077: 'C:\PROGRA~2\WiX\2.0.3116.0\candle.exe' : return code '0x1'

