
if (WScript.Arguments.length < 2) {
	WScript.Echo("usage: fdtcomp <file1> <file2>");
	WScript.Echo(" compares last modified dates of file1 and file2:");
	WScript.Echo(" return values:");
	WScript.Echo("  99: f1 < f2");
	WScript.Echo("  100: f1 = f2");
	WScript.Echo("  101: f1 > f2");
	WScript.Quit(255);
}

try {
    var filenameA = WScript.Arguments(0);
    var filenameB = WScript.Arguments(1);
    
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var fileA = fso.GetFile(filenameA);
    var fileB = fso.GetFile(filenameB);
    
    WScript.Echo("A: " + fileA.DateLastModified);
    WScript.Echo("B: " + fileB.DateLastModified);
    if (fileA.DateLastModified < fileB.DateLastModified) {
        WScript.Quit(99);
    } else if (fileA.DateLastModified == fileB.DateLastModified) {
        WScript.Quit(100);
    } else { // fileA.DateLastModified < fileB.DateLastModified
        WScript.Quit(101);
    }
} catch (e) {
    WScript.Echo("Error: " + (e.number & 0xFFFF) + e.description);
    WScript.Quit(255);
}
