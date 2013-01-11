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

if (WScript.Arguments.length > 0 &&
	WScript.Arguments(0).toLowerCase() == "-f" &&
	WScript.Arguments.length > 1)
{
	inputfile = WScript.Arguments(1);
}
else
{
	usage();
	WScript.Quit(1);
}

var fso = new ActiveXObject("Scripting.FileSystemObject");

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
		createParentFolder(dests[i]);
		WScript.StdErr.WriteLine("BUILDMSG: publishing " + src + " -> " + dests[i]);
		fso.CopyFile(src,dests[i],true);
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
