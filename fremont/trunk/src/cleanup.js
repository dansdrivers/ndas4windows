
//
// cleaning up build residues
//

var fso = new ActiveXObject("Scripting.FileSystemObject");

var unnamedArgs = WScript.Arguments.Unnamed; 
var targetPath = unnamedArgs.Count > 0 ? unnamedArgs(0) : ".";
var folder = fso.GetFolder(targetPath);
var g_prefix = folder.Path;
var g_prefix_length = g_prefix.length + 1;

if (!IsCScript()) {
	WScript.Echo( "This script should be run by cscript.exe");
	WScript.Quit();
}

var bDeleteFolder = ! WScript.Arguments.Named.Exists("logonly");
var g_bDryRun = WScript.Arguments.Named.Exists("dryrun");
var VerboseMode = WScript.Arguments.Named.Exists("v");

CleanupFolder(folder, true, bDeleteFolder);

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

function CleanupFolder(folder, recursive, bDeleteFolder)
{
	if (VerboseMode)
	{
		WScript.Echo(">" + folder.Path.substr(g_prefix_length));
	}
	
	var path = folder.Path;

	if (bDeleteFolder)
	{
		DeleteFolder(path, "objd");
		DeleteFolder(path, "obj");

		DeleteFolder(path, "objchk_w2k_x86");
		DeleteFolder(path, "objchk_wxp_x86");
		DeleteFolder(path, "objchk_wnet_x86");
		DeleteFolder(path, "objchk_wnet_ia64");
		DeleteFolder(path, "objchk_wnet_amd64");

		DeleteFolder(path, "objfre_w2k_x86");
		DeleteFolder(path, "objfre_wxp_x86");
		DeleteFolder(path, "objfre_wnet_x86");
		DeleteFolder(path, "objfre_wnet_ia64");
		DeleteFolder(path, "objfre_wnet_amd64");
	}

	DeleteFile(path, "buildschema.xml");
	
	DeleteFile(path, "buildchk_w2k_x86.log");
	DeleteFile(path, "buildchk_w2k_x86.err");
	DeleteFile(path, "buildchk_w2k_x86.wrn");
	DeleteFile(path, "buildchk_w2k_x86.xml");

	DeleteFile(path, "buildchk_wxp_x86.log");
	DeleteFile(path, "buildchk_wxp_x86.err");
	DeleteFile(path, "buildchk_wxp_x86.wrn");
	DeleteFile(path, "buildchk_wxp_x86.xml");

	DeleteFile(path, "buildchk_wnet_x86.log");
	DeleteFile(path, "buildchk_wnet_x86.err");
	DeleteFile(path, "buildchk_wnet_x86.wrn");
	DeleteFile(path, "buildchk_wnet_x86.xml");

	DeleteFile(path, "buildchk_wnet_ia64.log");
	DeleteFile(path, "buildchk_wnet_ia64.err");
	DeleteFile(path, "buildchk_wnet_ia64.wrn");
	DeleteFile(path, "buildchk_wnet_ia64.xml");

	DeleteFile(path, "buildchk_wnet_amd64.log");
	DeleteFile(path, "buildchk_wnet_amd64.err");
	DeleteFile(path, "buildchk_wnet_amd64.wrn");
	DeleteFile(path, "buildchk_wnet_amd64.xml");

	DeleteFile(path, "buildfre_w2k_x86.log");
	DeleteFile(path, "buildfre_w2k_x86.err");
	DeleteFile(path, "buildfre_w2k_x86.wrn");
	DeleteFile(path, "buildfre_w2k_x86.xml");

	DeleteFile(path, "buildfre_wxp_x86.log");
	DeleteFile(path, "buildfre_wxp_x86.err");
	DeleteFile(path, "buildfre_wxp_x86.wrn");
	DeleteFile(path, "buildfre_wxp_x86.xml");

	DeleteFile(path, "buildfre_wnet_x86.log");
	DeleteFile(path, "buildfre_wnet_x86.err");
	DeleteFile(path, "buildfre_wnet_x86.wrn");
	DeleteFile(path, "buildfre_wnet_x86.xml");

	DeleteFile(path, "buildfre_wnet_ia64.log");
	DeleteFile(path, "buildfre_wnet_ia64.err");
	DeleteFile(path, "buildfre_wnet_ia64.wrn");
	DeleteFile(path, "buildfre_wnet_ia64.xml");

	DeleteFile(path, "buildfre_wnet_amd64.log");
	DeleteFile(path, "buildfre_wnet_amd64.err");
	DeleteFile(path, "buildfre_wnet_amd64.wrn");
	DeleteFile(path, "buildfre_wnet_amd64.xml");
	
	DeleteFile(path, "build.log");
	DeleteFile(path, "build.err");
	DeleteFile(path, "build.wrn");
	DeleteFile(path, "buildd.log");
	DeleteFile(path, "buildd.err");
	DeleteFile(path, "buildd.wrn");

	DeleteFile(path, "binplace.log");

	if (recursive) {
		var fc = new Enumerator(folder.SubFolders);
		for ( ;!fc.atEnd(); fc.moveNext()) {
			var subFolder = fc.item();
			if (subFolder.Name != ".svn") {
				CleanupFolder(subFolder, recursive, bDeleteFolder);
			}
		}
	}
}

function DeleteFile(parentPath, fileName)
{
	var targetPath = fso.BuildPath(parentPath, fileName);
	if (fso.FileExists(targetPath)) {
		WScript.Echo(targetPath.substr(g_prefix_length));
		if (!g_bDryRun)
		{
			try {
				fso.DeleteFile(targetPath);
			} catch(e) {
				WScript.Echo("Error: " + e.description);
			}
		}
	}
}

function DeleteFolder(parentPath, folderName)
{
	var targetPath = fso.BuildPath(parentPath, folderName);
	if (fso.FolderExists(targetPath)) {
		WScript.Echo(targetPath.substr(g_prefix_length));
		if (!g_bDryRun)
		{
			try {
				fso.DeleteFolder(targetPath);
			} catch(e) {
				WScript.Echo("Error: " + e.description);
			}
		}
	}
}
