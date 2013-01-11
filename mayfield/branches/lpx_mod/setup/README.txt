
Build a setup!

11/17/2004 Chesong Lee

---------------------------------------------------------------------
PACKAGING
---------------------------------------------------------------------

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

