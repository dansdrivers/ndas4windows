
var fso = new ActiveXObject("Scripting.FileSystemObject");

var namedArgs = WScript.Arguments.Named;
var args = WScript.Arguments.Unnamed;

var TARGETNAME=namedArgs("t");
var FRIENDLYNAME=namedArgs("f");
var APPLAUNCHED=namedArgs("app");
var ADMINQUIETCMD=namedArgs("aq");
var USERQUIETCMD=namedArgs("uq");
var BASESED=namedArgs("basesed");

if (null == TARGETNAME ||
	null == FRIENDLYNAME ||
	null == APPLAUNCHED) 
{
	Usage();
	WScript.Quit(1);
}

if (null == BASESED) {
	BASESED="package.sed.base";
}

var lstfile=args(0);
if (null == lstfile) {
	Usage();
	WScript.Quit(1);
}

var lststm = fso.OpenTextFile(lstfile, 1, false, 0);
var FILES = new Array();
for (var i = 0; !lststm.AtEndOfStream; ++i) {
	var line = lststm.ReadLine();
	if (line.replace(/^ */, "").replace(/ *$/, "").length > 0) {
		FILES.push(line);
	}
}

for (var i = 0; i < FILES.length; ++i) {
	WScript.Echo("Package File: " + FILES[i]);
}

var basefile=BASESED;
var outfile=fso.GetBaseName(TARGETNAME) + ".sed";

var outstm = fso.CreateTextFile(outfile, true, false);
var instm = fso.OpenTextFile(basefile, 1, false, 0);
while (!instm.AtEndOfStream) {
	var line = instm.ReadLine();
	line = SubstituteParams(line);
	outstm.WriteLine(line);
}
instm.Close();

CreateSourceEntry(outstm, FILES);
outstm.Close();

WScript.Quit(0);

function Usage()
{
	var usage = 
	"\n makesed: [options] file-list-file\n" +
	"  /t:<targetname> (required)\n" +
	"  /f:<friendlyname> (required)\n" +
	"  /app:<applaunched> (required)\n" +
	"  /basesed:<base-sed-file> (default: package.sed.base)\n" +
	"  /aq:<admin-quiet> (optional)\n" +
	"  /uq:<user-quiet> (optional)\n";
	WScript.Echo(usage);
}

function SubstituteParams(line)
{
	line = line.replace("${TARGETNAME}", TARGETNAME);
	line = line.replace("${FRIENDLYNAME}", FRIENDLYNAME);
	line = line.replace("${APPLAUNCHED}", APPLAUNCHED);
	line = line.replace("${ADMINQUIETINSTCMD}", "");
	line = line.replace("${USERQUIETINSTCMD}", "");
	return line;
	// ${TARGETNAME}
	// ${FRIENDLYNAME}
	// ${APPLAUNCHED}
	// ${ADMINQUIETINSTCMD}
	// ${USERQUIETINSTCMD}
}

/*
:generate_source_proc
FILE0="ndas.msi"
FILE1="ndasetup.exe" 
[SourceFiles]
SourceFiles0=ALL\ndas.msi 
SourceFiles1=ALL\ndasetup.exe 
[SourceFiles0] 
%FILE0%=
[SourceFiles1] >> package.tmp
%FILE1%=
*/

function CreateSourceEntry(stm, files)
{
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	// Create FILE0="file0.ext"
	for (i = 0; i < files.length; ++i) {
		stm.WriteLine("FILE" + i + "=\"" + fso.GetFileName(files[i]) + "\"");
	}
	// Create [SourceFiles]
	stm.WriteLine("[SourceFiles]");
	// Create SourceFiles0=<path>
	for (i = 0; i < files.length; ++i) {
		var filepath = fso.GetAbsolutePathName(files[i]);
		var folderpath = fso.GetParentFolderName(filepath);
		stm.WriteLine("SourceFiles" + i + "=" + folderpath);
	}
	// Create [SourceFile0] and %FILE0%=
	for (i = 0; i < files.length; ++i) {
		stm.WriteLine("[SourceFiles" + i + "]");
		stm.WriteLine("%FILE" + i + "%=");
	}
}
