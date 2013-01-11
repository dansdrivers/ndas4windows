
//
// cleaning up build residues
//

var fso = new ActiveXObject("Scripting.FileSystemObject");

var folder = fso.GetFolder(".");
var g_prefix = folder.Path;
var g_prefix_length = g_prefix.length + 1;

if (!IsCScript()) {
	WScript.Echo( "This script should be run by cscript.exe");
	WScript.Quit();
}

CleanupFolder(folder, true);

function IsCScript()
{
	var CSCRIPT = "cscript.exe";
	var fullname = WScript.FullName;
	var scriptHost = fullname.substr(fullname.length - CSCRIPT.length).toLowerCase();
	if (CSCRIPT == scriptHost) {
		return true;
	} else {
		return false;
	}
}

function CleanupFolder(folder, recursive)
{
	var path = folder.Path;

	DeleteFolder(path, "objchk_w2k_x86");
	DeleteFolder(path, "objchk_wxp_x86");
	DeleteFolder(path, "objchk_wnet_x86");
	DeleteFolder(path, "objfre_w2k_x86");
	DeleteFolder(path, "objfre_wxp_x86");
	DeleteFolder(path, "objfre_wnet_x86");

	DeleteFile(path, "buildchk_w2k_x86.log");
	DeleteFile(path, "buildchk_w2k_x86.err");
	DeleteFile(path, "buildchk_w2k_x86.wrn");

	DeleteFile(path, "buildchk_wxp_x86.log");
	DeleteFile(path, "buildchk_wxp_x86.err");
	DeleteFile(path, "buildchk_wxp_x86.wrn");

	DeleteFile(path, "buildchk_wnet_x86.log");
	DeleteFile(path, "buildchk_wnet_x86.err");
	DeleteFile(path, "buildchk_wnet_x86.wrn");

	DeleteFile(path, "buildfre_w2k_x86.log");
	DeleteFile(path, "buildfre_w2k_x86.err");
	DeleteFile(path, "buildfre_w2k_x86.wrn");

	DeleteFile(path, "buildfre_wxp_x86.log");
	DeleteFile(path, "buildfre_wxp_x86.err");
	DeleteFile(path, "buildfre_wxp_x86.wrn");

	DeleteFile(path, "buildfre_wnet_x86.log");
	DeleteFile(path, "buildfre_wnet_x86.err");
	DeleteFile(path, "buildfre_wnet_x86.wrn");
	
	if (recursive) {
		var fc = new Enumerator(folder.SubFolders);
		for ( ;!fc.atEnd(); fc.moveNext()) {
			var subFolder = fc.item();
			if (subFolder.Name != ".svn") {
				CleanupFolder(subFolder, recursive);
			}
		}
	}
}

function DeleteFile(parentPath, fileName)
{
	var targetPath = fso.BuildPath(parentPath, fileName);
	if (fso.FileExists(targetPath)) {
		WScript.Echo(targetPath.substr(g_prefix_length));
		try {
			fso.DeleteFile(targetPath);
		} catch(e) {
			WScript.Echo("Error: " + e.description);
		}
	}
}

function DeleteFolder(parentPath, folderName)
{
	var targetPath = fso.BuildPath(parentPath, folderName);
	if (fso.FolderExists(targetPath)) {
		WScript.Echo(targetPath.substr(g_prefix_length));
		try {
			fso.DeleteFolder(targetPath);
		} catch(e) {
			WScript.Echo("Error: " + e.description);
		}
	}
}
