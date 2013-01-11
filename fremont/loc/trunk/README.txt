
-------------------------
Localization and Branding
-------------------------

2008/05/29 Giyoul Lee (kykim@ximeta.com)
2006/07/17 Chesong Lee (cslee@ximeta.com)

-----
Steps
-----

1. Get base files from the archive

  After the official build, following files should be available from
  the internal build server:

  - ndas_3.31.1701_base_bin.zip
  - ndas_3.31.1701_base_meta.zip
  - ndas_3.31.1701_base_resdata.zip

  Extract them into base\bin, base\meta, base\resdata respectively.

  or

  Look at 'File Name Convention' section of this readme file for how to build get the files manually.

2. Create a base localization XML files

   2.1. Create resxml files

   tools\resxml extract base\bin\i386\ndasmgmt.enu.dll base\loc\ndasmgmt.enu.resxml
   tools\resxml extract base\bin\i386\ndasbind.enu.dll base\loc\ndasbind.enu.resxml

   2.2. Convert resxml files to localization xml files

   tools\msxsl base\loc\ndasmgmt.enu.resxml tools\resxml2loc.xsl -o base\loc\loc_enu_ndasmgmt.xml version=3.31.1701 component=ndasmgmt oem=base lang=enu

   tools\msxsl base\loc\ndasbind.enu.resxml tools\resxml2loc.xsl -o base\loc\loc_enu_ndasbind.xml version=3.31.1701 component=ndasbind oem=base lang=enu

   2.3. Convert ndasmsg.xml to localization xml file

   tools\msxsl base\meta\ndasmsg.xml tools\msgxml2loc.xsl -o base\loc\loc_enu_ndasmsg.xml version=3.31.1701 component=ndasmsg oem=base lang=enu

   2.4. Wrap it up!

   Instead of typing above lengthy commands, use the following
   command:

		tools\runmsbuild.cmd tools\createbaseloc.proj
   or
		tools\createbaseloc.cmd

3. OEM Localization

   3.1. Create a OEM directory and resdata directory

   		mkdir oem\generic
		mkdir oem\generic\resdata

   3.2. Copy base files into the OEM directory

   		copy base\loc\*.xml oem\generic

   3.3. Create a localization project file in the OEM directory

   		copy con loc.proj
		
		<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
		  <Import Project="../loc.targets" />
		</Project>

   3.4. Copy resdata files into the oem\generic\resdata
   		Copy only files to modify. No changes will be made in each
   		file, do not put those files in resdata directory.
		
		copy base\resdata\*.* oem\generic\resdata

		and delete some files...

   3.5. Add more languages

   		cd oem\generic
   		copy loc_enu_ndasmgmt.xml loc_deu_ndasmgmt.xml
		copy loc_enu_ndasbind.xml loc_deu_ndasbind.xml
		copy loc_enu_ndasmsg.xml loc_deu_ndasmsg.xml

4. Translation and branding

   Now you can send oem\generic\*.* to the partners to translate the
   XML files and edit icon or bitmap files.

   You may edit loc_XXX.xml file with any UTF-8-capable text editors
   or use tools\locedit.exe.

5. Build localized and branded binaries

		cd oem\generic
		..\build.cmd
		..\build.cmd /p:platform=x64

6. You get the results at oem\generic\branded\

--------------------
File Name Convention
--------------------

* Base Binary Files
  
  ndas_3.31.1701_base_bin.zip

  Files in publish\i386 and publish\amd64 in official build machine
  after the offical build excluding *.pdb, *.cdf.

* Base Resource Meta Data File
  
  ndas_3.11_1327_base_meta.zip

  Meta data includes resmap (Resource mapping between binary data
  files and resource IDs in resource section), and ndasmsg.xml
  (Message XML format).

  Example of ndasbind.resmap

  <resmap module="ndasbind.exe">
  <resource type="ICON_GROUP" name="#128" fileName="ndasbind.ico" symbolName="IDR_MAINFRAME" />
  <resource type="ICON_GROUP" name="#329" fileName="ndasbind_res_device_normal.ico" symbolName="IDI_DEVICE_BASIC" />
  <resource type="ICON_GROUP" name="#330" fileName="ndasbind_res_device_fail.ico" symbolName="IDI_DEVICE_FAIL" />
  <resource type="ICON_GROUP" name="#331" fileName="ndasbind_res_device_bound.ico" symbolName="IDI_DEVICE_BOUND" />
  <resource type="BITMAP" name="#128" fileName="ndasbind_res_toolbar.bmp" symbolName="IDR_MAINFRAME" />
  <resource type="BITMAP" name="#130" fileName="ndasbind_res_toolbar_empty.bmp" symbolName="IDR_EMPTY_TOOLBAR" />
  <resource type="BITMAP" name="#111" fileName="ndasbind_res_wizard_watermark.bmp" symbolName="IDB_WATERMARK256" />
  <resource type="BITMAP" name="#103" fileName="ndasbind_res_wizard_banner.bmp" symbolName="IDB_BANNER256" />
  <resource type="IMAGE" name="#216" fileName="ndasbind_res_info_header.jpg" symbolName="IDB_ABOUT_HEADER" />
  </resmap>

  ResMap files are generated from resmap.exe in tools directory.

  ndasmsg.xml is a copy from src\umapps\ndasmsg\ndasmsg.xml

* Base Resource Data Files

  ndas_3.11_1327_base_resdata.zip

  Base resource data files are brandable (not-localizable yet) files
  for icons, bitmaps, etc in application resources. Mapping between
  data file names and actual resources are defined in ResMap files.

  List of current resdata files:

  ndasbind.ico
  ndasbind_res_device_bound.ico
  ndasbind_res_device_fail.ico
  ndasbind_res_device_normal.ico
  ndasbind_res_info_header.jpg
  ndasbind_res_toolbar.bmp
  ndasbind_res_toolbar_empty.bmp
  ndasbind_res_wizard_banner.bmp
  ndasbind_res_wizard_watermark.bmp
  ndasmgmt.ico
  ndasmgmt_res_findhosts.avi
  ndasmgmt_res_info_header.jpg
  ndasmgmt_res_nif.ico
  ndasmgmt_res_option_icons.bmp
  ndasmgmt_res_proptree.bmp
  ndasmgmt_res_status.bmp
  ndasmgmt_res_taskbar.ico
  ndasmgmt_res_taskbar_fail.ico
  ndasmgmt_res_unitdevices.bmp
  ndasmgmt_res_wizard_banner.bmp
  ndasmgmt_res_wizard_watermark.bmp

