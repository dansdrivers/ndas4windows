// #define PV_VER_MAJOR	2
// #define PV_VER_MINOR	2
// #define PV_VER_BUILD	0
// #define PV_VER_PRIVATE	509
// #define PV_PRODUCTVER  		2,2,0,509

var fso = new ActiveXObject("Scripting.FileSystemObject");

var args = WScript.Arguments;
var vmajor, vminor, vbuild, vpriv;
var vfname;

var rxfn, processfn;

//
// processes arguments
//
try {
	if (args.Length == 2)
	{
		vfname = args(0);
		varr = args(1).split(".");
		if (varr.length != 4)
			throw(usage());
		vmajor = varr[0];
		vminor = varr[1];
		vbuild = varr[2];
		vpriv = varr[3];
	}
	else if (args.Length == 5)
	{
		vfname = args(0);
		vmajor = args(1);
		vminor = args(2);
		vbuild = args(3);
		vpriv = args(4);
	}
} catch(e) {
	WScript.Echo("Error: " + e);
	WScript.Quit(2);
}


var vfpath = fso.GetAbsolutePathName(vfname);
WScript.Echo("Path   : " + vfpath);
WScript.Echo("Version: " + vmajor + "." + vminor + "." + vbuild + "." + vpriv);

var iResult = ProcessISXMLProperty(vfpath, vmajor, vminor, vbuild, vpriv);

WScript.Quit(iResult);

//
// usage
//
function usage()
{
	return "setisxmlver.js ism_xml_filename major[.]minor[.]build[.]private";
}

/// PROCESSOR for PRODVER.H

//
// Regular Expressions (ProdVer.h)
//
function GetProdVerRegExs()
{
	var arr = new Array();
	var i = 0;
	arr[i++] = new RegExp("#define[ \t]+PV_VER_MAJOR[ \t]+.*$");
	arr[i++] = new RegExp("#define[ \t]+PV_VER_MINOR[ \t]+.*$");
	arr[i++] = new RegExp("#define[ \t]+PV_VER_BUILD[ \t]+.*$");
	arr[i++] = new RegExp("#define[ \t]+PV_VER_PRIVATE[ \t]+.*$");
	arr[i++] = new RegExp("#define[ \t]+PV_PRODUCTVER[ \t]+.*$");
	return arr;
}

//
// Process ProdVer.h
//
function ProcessProdVer(line, rxs, vmajor, vminor, vbuild, vpriv)
{
	var newline;

	newline = line
		.replace(rxs[0], "#define PV_VER_MAJOR " + vmajor )
		.replace(rxs[1], "#define PV_VER_MINOR " + vminor )
		.replace(rxs[2], "#define PV_VER_BUILD " + vbuild )
		.replace(rxs[3] , "#define PV_VER_PRIVATE " + vpriv )
		.replace(rxs[4], "#define PV_PRODUCTVER " + vmajor + "," + vminor + "," + vbuild + "," + vpriv);

	return newline;
}

/// PROCESSOR for PROPERTY.IDT
function GetISPropertyRegExs()
{
//ProductName	NetDisk 2.2.0.509	
//ProductVersion	2.2.0.509	
	var arr = new Array();
	var i = 0;
	arr[i++] = new RegExp("ProductVersion\t.*\t$");
	return arr;
}

function ProcessISProperty(line, rxs, vmajor, vminor, vbuild, vpriv)
{
	var newline;
	newline = line
		.replace(rxs[0], "ProductVersion\t" + vmajor + "." + vminor + "." + vbuild + "." + vpriv + "\t");
	return newline;
}

function ProcessISXMLProperty(ismfile, vmajor, vminor, vbuild, vpriv)
{
	var xmlDoc = new ActiveXObject("MSXML.DOMDocument");

	try {
		xmlDoc.async = false;
		xmlDoc.load(ismfile);
		var nodeProp = xmlDoc.selectSingleNode("/msi/table[@name='Property']");

		var nodeProductNameRow = nodeProp.selectSingleNode("row[td = 'ProductName']");
		var nodeProductVersionRow = nodeProp.selectSingleNode("row[td = 'ProductVersion']");

		var nodeProductNameText = nodeProductNameRow.childNodes(1);
		var nodeProductVersionText = nodeProductVersionRow.childNodes(1);
		
		nodeProductNameText.text = nodeProductNameText.text.replace(/[0-9]+\.[0-9]+/, vmajor + "." + vminor);
		nodeProductVersionText.text = vmajor + "." + vminor + "." + vbuild + "." + vpriv;
		
		WScript.Echo (nodeProductNameText.text);
		WScript.Echo (nodeProductVersionText.text);

		xmlDoc.save(ismfile);
	} catch (e) {
		WScript.Echo ("Error" + e.Number, e.Description);
	}

	return 0;
}
