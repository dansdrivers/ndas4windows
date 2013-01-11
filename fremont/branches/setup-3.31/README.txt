
NDAS Setup on WiX

June 19, 2008 Giyoul Kim

Comments about revised oembuild

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

################################################################################
# If you just want build english only, not branded installer msi.

1. Make sure this project is located under NDAS software. That means, you should be able to find built binaries at ..\publish\fre\i386 ...

2. MSBuild setup.proj or run build_setup.cmd

ex : Building english only, ndasscsi, 64bit, 3.30.1602
setup>build_setup.cmd /p:sku=ndasscsi /p:platform=amd64 /p:ProductVersion=3.30.1602

ex : Building english only, ndasport, 32bit & 64bit, include ndasntfs, 3.30.1602
setup>build_setup.cmd /p:sku=ndasport /p:platform=all /p:includendasntfs=true /p:ProductVersion=3.30.1602

3. You will get the results at publish\

################################################################################
# If you want branded and/or localized installer.

* Here is full step-by-step howto, you'd better to commit them once you finished.

ex : I want to create German & English version. Let's get started.

1. Checkout this project

2. Create oem directory under oembuild\, for example 'oembuild\deu\'

3. finish directories like following

deu\
	branded\
		fre\
			amd64\ : copy the branded & localized 64bit binaries to here. make sure ndasmgmt.deu.dll, ndasbind.deu.dll is also included
				setup\ : copy the files from publish\fre\amd64\setup\* to here
			i386\ : copy the branded & localized 32bit binaries to here. make sure ndasmgmt.deu.dll, ndasbind.deu.dll is also included
				setup\ : copy the files from publish\fre\i386\setup\* to here
	eula\ : place eula_de-DE.rtf, eula_en-US.rtf here
	ibd\ : place setup resources here, refer setup\wixsrc\ibd\
	ico\ : not used for now
	msi_de-DE\
		loc\ loc_ndaswixoem.xml, loc_ndaswixmsm.xml, loc_ndaswixstd.xml, loc_ndaswixstdex.xml. If you don't place any of these files, WiX will use those in setup\wixsrc\loc by default.
	msi_en-US\
		loc\
	msi_neutral\ : required for any configuration. loc\ is not required
	package\

4. prepare branded resources.

setup\oembuild>build_resource.cmd deu

This command will create followings
deu\
	branded\
		eula\ : eula files
		ibd\ : setup resources
		ico\
	msi_de-DE\
		wxl\ : localized texts, ui
	msi_en-US\
		wxl\
	msi_neutral\
		wxl\

5. build the installer

setup>build_oem.cmd deu 3.30.1602

Look inside and edit build_oem.cmd if you need different build options.

This command will create followings
deu\
	msi_de-DE\
		obj\
			amd64\ : compiled 64bit wix object files
			i386\ : compiled 32bit wix object files
			bin\
				amd64\ : finished 64bit msi file
				i386\ : finished 32bit msi file
	msi_en-US\
	msi_neutral\
	package\
		amd64\ : setup.exe, setup.ini, *.mst, msi file 
		i386\ : setup.exe, setup.ini, *.mst, msi file
		
ex: finished 32bit 

i386\
	setup.exe : 
	setup.ini : If setup.exe does not run correctly, use setup\supplements\bootstrap\x86\setup.ini.template as template file.
	instmsiw.exe : required for Windows 2000 with no service pack.
	ndasscsi-3.30.1602-x86-de-DE.mst : transform file for Germany
	ndasscsi-3.30.1602-x86-en-US.mst : transform file for English
	ndasscsi-3.30.1602-x86.msi : neutral msi file. Used with mst file.

7. You may zip or self extracted file with the results. Or put them into CD.

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

