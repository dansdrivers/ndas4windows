
Build a setup!

11/17/2004 Chesong Lee

---------------------------------------------------------------------
PACKAGING
---------------------------------------------------------------------

---------------------------------------------------------------------
CUSTOMIZING SETUP (OEM)
---------------------------------------------------------------------

We modify directly XML-format InstallShield Project file (ism-file) 
via Microsoft XML Parser using JScript to support customization
and post-setup-build modifications. This manipulation offers us
a more flexbility to create an OEM-customized setup files,
without maintaining separate setup projects.

However, string table (ISString) uses proprietary encoding scheme
to store strings and its method is not yet reverse-engineered.
We are using export-modify-import method to support ISString
modification.


---------------------------------------------------------------------
RELEASE FLAGS:
---------------------------------------------------------------------

You can include or exclude some features with the following flags:

o RF_NDASUPDATER
o RF_NDASMGMT
o RF_NDASCMD
o RF_NDASBIND

By default, all above features are included.
If you want to exclude one of them, you have to specify all the others.

For example, if you want to exclude Update function, use the following 
commands:

build_msi.cmd -f "RF_NDASMGMT,RF_NDASCMD,RF_NDASBIND"

Be sure to include quotation mark around the flags.


---------------------------------------------------------------------
MSI Properties for customized setup
---------------------------------------------------------------------

ARPNOREPAIR {none,1}
ARPNOREMOVE {none,1}
ARPNOMODIFY {none,1}
AxShowCustomSetup {0,1}
AxShowDestinationFolder {0,1}
UseReadyToInstallNotice {0,1}
UseShellIconCacheRefresh {0,1}
ProductReleaseTag {TagString}

