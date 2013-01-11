var args = WScript.Arguments.Unnamed;

var d = new Date(); // .getUTCDate();
var format = "";

format += d.getYear();
if (d.getMonth() < 9) format += "0";
format += d.getMonth() + 1;
format += d.getDate();
format += ".";
format += d.getHours();
format += d.getMinutes();

WScript.Echo("BUILDDATE=" + format);

if (args.length > 0)
{
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	var ts = fso.CreateTextFile(args(0), true, false);
	ts.WriteLine("BUILDDATE=" + format);
	ts.Close();
}
