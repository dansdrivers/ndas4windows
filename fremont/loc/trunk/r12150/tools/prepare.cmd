..\..\bin\resxml.exe extract publish\base\i386\ndasmgmt.enu.dll ndasmgmt.enu.resxml
..\..\bin\resxml.exe extract publish\base\i386\ndasbind.enu.dll ndasbind.enu.resxml
..\..\bin\msxsl ndasmgmt.enu.resxml ..\..\bin\resxml2loc.xsl -o loc_base_ndasmgmt.xml
..\..\bin\msxsl ndasbind.enu.resxml ..\..\bin\resxml2loc.xsl -o loc_base_ndasbind.xml
..\..\bin\msxsl ndasmsg.xml ..\..\bin\msgxml2loc.xsl -o loc_base_ndasmsg.xml
