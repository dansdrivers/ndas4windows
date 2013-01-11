
var Verbose = false;

//
// constants
//
var ForReading = 1;
var ForWriting = 2;
var ForAppending = 8;
var TristateUseDefault = -2;
var TristateTrue = -1;
var TristateFalse = 0;

//
// Global Objects
//
var fso = new ActiveXObject("Scripting.FileSystemObject");
var shell = new ActiveXObject("WScript.Shell");

var args = WScript.Arguments.Unnamed;

var namedArgs = WScript.Arguments.Named;

if (namedArgs.Exists("v")) Verbose = true;

var baseDir = ".";
if (args.Length > 0)
{
		baseDir = args(0);
}

DisplayMessage("base: " + fso.GetAbsolutePathName(baseDir));
baseDir = fso.GetAbsolutePathName(baseDir);

var productVerInfo = ReadVersion(baseDir + "\\PRODUCTVER.txt");
productVerInfo.QFE = SvnGetCommitRevision(baseDir);

WScript.Echo("PRODUCT_VERSION=" + 
    productVerInfo.Major + "." + 
    productVerInfo.Minor + "." + 
    productVerInfo.Build + "." + 
    productVerInfo.QFE);
    
var verHeaderFile = baseDir + "\\src\\__productver.h";
var revHeaderFile = baseDir + "\\src\\__productrev.h";

var headerVerInfo = ReadProductVersionHeader(verHeaderFile);
DisplayMessage(headerVerInfo.Major, headerVerInfo.Minor, headerVerInfo.Build);

var headerRevision = ReadProductRevisionHeader(revHeaderFile);
DisplayMessage(headerRevision);

/* Rewrite product version header file when the product version text file 
    is updated. */
if (productVerInfo.Major != headerVerInfo.Major ||
    productVerInfo.Minor != headerVerInfo.Minor ||
    productVerInfo.Build != headerVerInfo.Build)
{
    WScript.Echo("PRODUCT VERSION HEADER UPDATED");
    WriteProductVersionHeader(verHeaderFile, productVerInfo);
}
if (productVerInfo.QFE != headerRevision || !fso.FileExists(revHeaderFile))
{
    WScript.Echo("PRODUCT REVISION HEADER UPDATED");
    WriteProductRevisionHeader(revHeaderFile, productVerInfo.QFE);
}

WScript.Quit(0);

function VerInfo()
{
	this.Major = 0;
	this.Minor = 0;
	this.Build = 0;
	this.QFE = 0;
}

function SvnGetCommitRevision(path)
{
	try
	{		
		var cmd = "svn info --xml " + path;
		DisplayMessage(cmd);

		var exec = shell.Exec(cmd);
		while (0 == exec.Status)
		{
			WScript.Sleep(0);
		}

	    if (exec.ExitCode != 0)
	    {
		    DisplayMessage(exec.StdErr.ReadAll());
		    DisplayError("svn info error (" + exec.ExitCode + ")");
		    return 0;
	    }

	    var xml = exec.StdOut.ReadAll();
    	
	    DisplayMessage(xml);

	    var dom = new ActiveXObject("MSXML.DOMDocument");
	    dom.async = false;
	    dom.loadXML(xml);
    	
	    if (dom.parseError.errorCode != 0) 
	    {
	       var e = dom.parseError;
	       DisplayError("XML parse error - " + e.reason);
	       return 0;
	    }
	    else
	    {
		    var commitNode = dom.selectSingleNode("/info/entry/commit");
		    commitRevision = commitNode.getAttribute("revision");
		    return commitRevision;
	    }
    }
    catch (e)
    {
	    var n = e.number & 0xFFFF;
	    if (2 == n)
	    {
		    //
		    // file not found error (for svn.exe)
		    //
		    DisplayError("svn.exe is not available. revision is set to 0.");
	    }
	    else
	    {
		    DisplayError("exception(" + n + ") " + e.message);
	    }
    }
    return 0;
}

function ReadVersion(ifn)
{
	var f = fso.OpenTextFile(ifn, ForReading, false, TristateFalse);
	var line = f.ReadLine();
	f.Close();
	var vs = line.split(".");
	var vi = new VerInfo();
	if (vs.length < 3)
	{
	    DisplayError("Version file does not contain valid version information");
	    return vi;
	}
	vi.Major = vs[0];
	vi.Minor = vs[1];
	vi.Build = vs[2];
	vi.QFE = 0;
	return vi;
}

function ReadProductVersionHeader(fn)
{
	var vi = new VerInfo();
	if (!fso.FileExists(fn))
	{
	    return vi;
	}
	var f = fso.OpenTextFile(fn, ForReading, false, TristateFalse);
	var line = f.ReadLine();
	f.Close();

    DisplayMessage(line);

	var sp = line.split(" ");
	if (4 != sp.length)
	{
        DisplayMessage("sp.length=" + sp.length);
		return vi;
	}
	if ("VT01" != sp[1])
	{
        DisplayMessage("sp[1]=" + sp[1]);
		return vi;
	}
	var vsp = sp[2].split(".");
	if (3 != vsp.length)
	{
        DisplayMessage("vsp.length=" + vsp.length);
		return vi;	
	}
	vi.Major = vsp[0];
	vi.Minor = vsp[1];
	vi.Build = vsp[2];
	return vi;
}

function ReadProductRevisionHeader(fn)
{
	if (!fso.FileExists(fn)) return 0;
	var f = fso.OpenTextFile(fn, ForReading, false, TristateFalse);
	var line = f.ReadLine();
	f.Close();

    DisplayMessage(line);

	var sp = line.split(" ");
	if (4 != sp.length)
	{
		return 0;
	}
	if ("VT01" != sp[1])
	{
		return 0;
	}
	var rev = sp[2];
	return rev;	
}

function WriteProductVersionHeader(ofn, vi)
{
	var f = fso.CreateTextFile(ofn, true, false);
	f.WriteLine("/* VT01 " + vi.Major + "." + vi.Minor + "." + vi.Build + " */");
	f.WriteLine("#define VER_PRODUCTMAJORVERSION " + vi.Major);
	f.WriteLine("#define VER_PRODUCTMINORVERSION " + vi.Minor);
	f.WriteLine("#define VER_PRODUCTBUILD " + vi.Build);
	f.Close();
}

function WriteProductRevisionHeader(ofn, qfe)
{
	var f = fso.CreateTextFile(ofn, true, false);
	f.WriteLine("/* VT01 " + qfe + " */");
	f.WriteLine("#define VER_PRODUCTBUILD_QFE " + qfe);
	f.Close();
}

function DisplayMessage(str)
{
	if (Verbose) WScript.Echo(str);
}

function DisplayError(str)
{
	WScript.StdErr.WriteLine("BUILDMSG: (UPDATE VERSION ERROR) " + str);
}
