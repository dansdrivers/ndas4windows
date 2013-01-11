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
	if ((args.Length == 3 || args.Length == 6))
	{
		if (args(0) == "prodver")
		{
			rxfn = GetProdVerRegExs;
			processfn = ProcessProdVer;
		}
		else if (args(0) == "isprop")
		{
			rxfn = GetISPropertyRegExs;
			processfn = ProcessISProperty;
		}
		else
			throw(usage());
	}
	else
		throw(usage());

	if (args.Length == 3)
	{
		vfname = args(1);
		varr = args(2).split(".");
		if (varr.length != 4)
			throw(usage());
		vmajor = varr[0];
		vminor = varr[1];
		vbuild = varr[2];
		vpriv = varr[3];
	}
	else if (args.Length == 6)
	{
		vfname = args(1);
		vmajor = args(2);
		vminor = args(3);
		vbuild = args(4);
		vpriv = args(5);
	}
}

catch(e)
{
	WScript.Echo(e);
	WScript.Quit(2);
}


var vfpath = fso.GetAbsolutePathName(vfname);
WScript.Echo("Path   : " + vfpath);
WScript.Echo("Version: " + vmajor + "." + vminor + "." + vbuild + "." + vpriv);

var rxs, i;
rxs = rxfn();
i = ProcessFile(vfpath, processfn, rxs, vmajor, vminor, vbuild, vpriv);

WScript.Quit(i);

//
// usage
//
function usage()
{
	return "setprodver.js [prodver|isprop] filename major[.]minor[.]build[.]private";
}

//
// process a file
//
function ProcessFile(vfilename, fn, rxs, vmajor, vminor, vbuild, vpriv)
{
	var is, out;
	var tmpdir, tmpname, TemporaryFolder = 2;

	try {
		tmpdir = fso.GetSpecialFolder(TemporaryFolder);
		tmpname = fso.GetTempName();
		out = tmpdir.CreateTextFile(tmpname);
	} catch(e) {
		WScript.Echo("Error creating a temp file " + tmpname);
		WScript.Echo("(" + e.description + ")");
		return 1;
	}
		
	try {
		is = fso.OpenTextFile(vfilename, 1, false, 0);
	} catch(e) {
		WScript.Echo("Error opening " + vfilename);
		WScript.Echo("(" + e.description + ")");
		return 1;
	}

	while (!is.AtEndOfStream)
		out.WriteLine(fn(is.ReadLine(), rxs, vmajor, vminor, vbuild, vpriv));

	is.Close();
	out.Close();

	fso.CopyFile(fso.BuildPath(tmpdir, tmpname), vfilename, true);
	fso.DeleteFile(fso.BuildPath(tmpdir, tmpname));
	
	return 0;
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
