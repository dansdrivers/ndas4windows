var fso = new ActiveXObject("Scripting.FileSystemObject");

var args = WScript.Arguments.Unnamed;
var namedArgs = WScript.Arguments.Named;

// usage: createcat.js /o:objd\i386\ndasport.cdf ..\port\objd\i386\ndasport.inf
var output = namedArgs.Item("o");
var catname = fso.GetBaseName(output);

var stm = fso.CreateTextFile(output, true, false);

stm.WriteLine("[CatalogHeader]");
stm.WriteLine("name=" + catname + ".cat");
stm.WriteLine("PublicVersion=0x00000001");
stm.WriteLine("EncodingType=0x00010001");
stm.WriteLine("CATATTR1=0x10010001:OSAttr:1:4.x,2:4.x,2:5.x,2:6.0");
stm.WriteLine("[CatalogFiles]");

for (var i = 0; i < args.length; ++i)
{
	var fn = fso.GetFileName(args(i));
	var fullpath = fso.GetAbsolutePathName(args(i));
	stm.WriteLine("<hash>" + fn + "=" + fullpath);
}

stm.Close();
