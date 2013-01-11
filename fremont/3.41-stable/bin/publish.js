//
// a simple emulation of publish.pl to make use of PASS1_PUBLISH
//
// publish.js -F %temp%\tmpfile.tmp
//

// input file has the following format:
//
// {source=dest;dest2}
// ...

var inputfile;

//
// in WDK 6000, command line may contain /O_BINARY_METADATA:
// this script only make use of -f <filename> argument.
// other options are ignored at this time.
//

var args = WScript.Arguments;

if (args.Length < 1)
{
    WScript.StdErr.WriteLine("BUILDMSG: No arguments are specified.");
    usage();
    WScript.Quit(1);
}

var inputfile = null;
for (var i = 0; i < args.Length; ++i)
{
    var l = args(i).toLowerCase();
    if (l == "-f" || l == "/f")
    {
	if (i + 1 < args.Length)
	{
	    inputfile = args(i+1);
	} 
	else
        {
	    WScript.StdErr.WriteLine("BUILDMSG: File name is not specified after -f");
	    usage();
	    WScript.Quit(1);
	}

    }
}

if (null == inputfile)
{
    WScript.StdErr.WriteLine("BUILDMSG: No input file is specified");
    usage();
    WScript.Quit(1);
}

var fso = new ActiveXObject("Scripting.FileSystemObject");
var scriptName = WScript.ScriptFullName;
var scriptPath = fso.GetFile(scriptName).ParentFolder.Path;

try
{
	var istm = fso.OpenTextFile(inputfile, 1, false, -2);
	while (!istm.AtEndOfStream)
	{
		var line = istm.ReadLine();
		processLine(line);
	}
}
catch (e)
{
	WScript.StdErr.WriteLine("BUILDMSG: " + e.name + " " + (e.number & 0xffff)+ ": " + e.message);
	WScript.Quit(1);
}

function iswhitespace(c)
{
	switch (c)
	{
	case " ":
	case "\t":
	case "\r":
	case "\n":
		 return true;
	default:
		return false;
	}
}

function processLine(line)
{
	// {src=dest1;dest2;...}
	// { } is optional

	// trim whitespaces
	var preceding = /^[ \t\r\n]*\{?[ \t\r\n]*/;
	line = line.replace(preceding, "");
	var trailing = /[ \t\r\n]*\}?[ \t\r\n]*$/;
	line = line.replace(trailing, "");

	if (line.length == 0) { return true; }	// no chars 

	var comps = line.split("=");
	if (comps.length != 2) 
	{
		WScript.StdErr.WriteLine("Parse Error: " + line);
		return false;
	}
	
	var src = comps[0];
	var dests = comps[1].split(";");
	
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	for (var i = 0; i < dests.length; ++i)
	{
	    var dstpath = dests[i];
		createParentFolder(dstpath);
		WScript.StdErr.WriteLine("BUILDMSG: publishing " + src + " -> " + dests[i]);
		var dstfile;
		try
		{
		    //
		    // When the file does not exist, an exception will be thrown
		    //
			dstfile = fso.GetFile(dstpath);
			//
			// Change the file attribute to normal to prevent CopyFile error
			//
			dstfile.Attributes = 0;
		}
		catch (e)
		{
		}
		fso.CopyFile(src,dstpath,true);
		switch (fso.GetExtensionName(dstpath).toLowerCase())
		{
		case "h":
	    case "hpp":
	    case "hxx":
    		dstfile = fso.GetFile(dstpath);
    		dstfile.Attributes = 1;
		}
		var shell = WScript.CreateObject("WScript.Shell");
		var ex = shell.Exec("\"" + scriptPath + "\\ftsync.exe\" \"" + src + "\" \"" + dests[i] + "\"");
		while (0 == ex.Status)
		{
			WScript.Sleep(10);
		}
	}
	
	return true;	
}

function createParentFolder(file)
{
	var comp = file.split("\\");
	var parent = "";
	var i;
	for (i = 0; i < comp.length - 1; ++i) {
		parent += comp[i];
		if (comp[i] != ".." && 
			comp[i] != "." && 
			comp[i].charAt(comp[i].length - 1) != ":")
			// skip relative path and drive letters
		{
			if (!fso.FolderExists(parent)) {
				WScript.StdErr.WriteLine("creating folder " + parent);
				fso.CreateFolder(parent);
			}
		}
		parent += "\\";
	}
		
}

function usage() 
{
	WScript.StdErr.WriteLine("usage: cscript.exe publish.js -F publishfile");
}
