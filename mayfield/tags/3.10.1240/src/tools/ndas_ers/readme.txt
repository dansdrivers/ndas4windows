NDAS Eraser

Author
Gi youl Kim. kykim@ximeta.com - 04/02/04

*********************************************
What is this?

ndas_ers.exe will remove almost all information(install information, registry, driver, files) about netdisk.

*********************************************
Does work for

Tested until Netdisk 2.3.1.517 build. 3.X version will be supported later.

*********************************************
Where you get this project source

http://svn/repos/netdisk_tools/ndas_ers/trunk/
http://svn/repos/netdisk/trunk/installhelper (for library sources)


*********************************************
Build environment

Visual C++ 6.0 and recent service pack
Recent Microsoft SDK
Recent Microsoft DDK

*********************************************
Build how to

This project contains some libraries to be built. Those libraries are from NetDisk install helper project results for removing drivers.

*********************************************
Notice

Result file is command line program but you may run it at anywhere.
Make sure to close all the programs before run this program (This program is almost a hacking program).
It is strongly recommanded to reboot after launch this program.

*********************************************
Usage

.Close Admin.exe , AggrMirUI.exe before launch eraser.
.Run ndas_ers.exe


-------------------------------------------------------------------------------------------
Build Note

04/19/2004
	Surrpot version 3.x
		remove lfs
	Fixed LPX removal problem
		RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E975-E325-11CE-BFC1-08002BE10318}"), _T("ComponentId"), _T("NKC_LPX"));
	Added removing SCSI
		RemoveRegistrySCSI();
	Added 4 digits base registries
		RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Class\\{4D36E97B-E325-11CE-BFC1-08002BE10318}"), _T("InfSection"), _T("lanscsiminiport"), TRUE);
		RemoveRegistryMatch(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Class\\{4D36E97D-E325-11CE-BFC1-08002BE10318}"), _T("InfSection"), _T("lanscsibus"), TRUE);
	Added removing rofilter