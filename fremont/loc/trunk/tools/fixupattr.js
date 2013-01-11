
var namedArgs = WScript.Arguments.Named;
var args = WScript.Arguments.Unnamed;
var basepath = ".";
if (args.length > 0)
{
    basepath = args(0);    
}

WScript.StdErr.WriteLine(basepath);

var fso = new ActiveXObject("Scripting.FileSystemObject");
var folder = fso.GetFolder(basepath);

var oem = folder.Name;
var version = namedArgs.Exists("version") ? namedArgs.Item("version") : "";

for (var fc = new Enumerator(folder.Files); !fc.atEnd(); fc.moveNext())
{
    var file = fc.item();
    if ("xml" != fso.GetExtensionName(file.Path))
    {
        WScript.StdErr.WriteLine("ignoring " + file.Name);
        continue;
    }
    
    var fname = fso.GetBaseName(file.Path);
    var ss = fname.split("_");
    if (ss.length != 3 || ss[0] != "loc")
    {
        WScript.StdErr.WriteLine("ignoring " + fname.Name);
        continue;
    }
    
    var lang = ss[1];
    var component = ss[2];
    
    WScript.StdOut.WriteLine(">" + file.Name + ": " + component + " " + version + " " + lang + " "  + oem);
    
    var xml = new ActiveXObject("MSXML.DOMDocument");
    xml.load(file.Path);
    var n = xml.selectSingleNode("/localization");
    n.setAttribute("component", component);
    n.setAttribute("version", version);
    n.setAttribute("lang", lang);
    n.setAttribute("oem", oem);
    xml.save(file.Path);
}

