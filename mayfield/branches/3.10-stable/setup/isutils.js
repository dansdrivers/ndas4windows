//
// IS Automation Constants
//
var ISPROJ_S_OK = 0;
var ISPROJ_S_LOCKED_BY_OTHER = 1;
var ISPROJ_S_OPENED_AS_READONLY = 2;
var ISPROJ_S_MERGE_PATH_ERROR = 4;
var ISPROJ_E_INVALID_ISM = 1100;
var ISPROJ_E_OLD_VERSION_MSI_ISM = 1101;
var ISPROJ_E_OLD_VERSION_IS_ISM = 1102;
var ISPROJ_E_NEWER_VERSION_IS_ISM = 1103;
var eiIgnore = 0, eiOverwrite = 1;

/////////////////////////////////////////////////////////////////////
//
// CScript Only!
//
if (!IsCScript()) {
    WScript.Echo("This command should be executed from the command line.\n");
    WScript.Quit(1);
}

/////////////////////////////////////////////////////////////////////

WScript.Quit(main());

/////////////////////////////////////////////////////////////////////

function usage()
{
    var s =
"usage: isutils.js <command> <arguments>\n\
\n\
  setmsifilename    <ismfile> <msi-package-file-name> \n\
\n\
  setpubfolder      <ismfile> <publishing folder path>\n\
\n\
  setsupportfolder  <ismfile> <setup support folder path>\n\
\n\
  setproductversion [options] <ismfile>\n\
\n\
    /v:<product-version> or /vf:<product-version-file>\n\
    (default: /vf:..\\PRODUCTVER.TXT) \n\
\n\
    /c:<product-code>    or /cf:<product-code-file\n\
    (default: /cf:ndassetup-product-codes.xml\n\
\n\
  exportisstring    <ismfile> <xmlfile>\n\
  importisstring    <ismfile> <xmlfile>\n\
\n\
    /lang:<language ID> (default: 0 for all languages)\n\
    /nomerge  - do not merge xml file and overwrite it.\n\
    /savetemp - do not delete temporary string files.\n\
\n\
  setmsiproperty    <ismfile> <property> <value>\n\
  removemsiproperty <ismfile> <property>\n\
\n\
  * global options:\n\
\n\
      /output:<output-ism-file> (default: same as ismfile)\n\
";
    WScript.Echo(s);
    return 255;
}

/////////////////////////////////////////////////////////////////////

function main()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 1) return usage();
    switch (args(0))
    {
    case "setmsifilename": return cmd_SetMSIPackageFileName();
    case "setpubfolder": return cmd_SetPublishingFolder();
    case "setsupportfolder": return cmd_SetSupportFolder();
    case "setproductversion": return cmd_SetProductVersion();
    case "exportisstring": return cmd_ExportISString();
    case "importisstring": return cmd_ImportISString();
    case "setmsiprop" : return cmd_MsiProperty_Set();
    case "removemsiprop" : return cmd_MsiProperty_Remove();
    default: return usage();
    }
}

/////////////////////////////////////////////////////////////////////

function cmd_SetMSIPackageFileName()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();

    var ismfile = args(1);
    var msifilename = args(2);
    // remove .msi if given
    msifilename = msifilename.replace(/\.msi/i, "");
    var outputfile = namedArgs.Exists("output") ? namedArgs("output") : ismfile;

    var xml = LoadXMLFile(ismfile);
    if (null == xml) return 1;
    
    SetMSIPackageFileName(xml, msifilename);
    
    xml.save(outputfile);

    return 0;
}

/////////////////////////////////////////////////////////////////////

function cmd_SetPublishingFolder()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();
    
    var ismfile = args(1);
    var path = args(2);
    var outputfile = namedArgs.Exists("output") ? namedArgs("output") : ismfile;
    
    var xml = LoadXMLFile(ismfile);
    if (null == xml) return 1;
    
    SetPublishingFolder(xml, path);
    
    xml.save(outputfile);

    return 0;
}

/////////////////////////////////////////////////////////////////////

function cmd_SetSupportFolder()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();
    
    var ismfile = args(1);
    var path = args(2);
    var outputfile = namedArgs.Exists("output") ? namedArgs("output") : ismfile;
    
    var xml = LoadXMLFile(ismfile);
    if (null == xml) return 1;
    
    SetSetupSupportFolder(xml, path);
    
    xml.save(outputfile);

    return 0;
}

/////////////////////////////////////////////////////////////////////

function cmd_SetProductVersion()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 2) return usage();

    var ismfile = args(1);

    //
    // product version
    //
    var productVersion = null;
    if (namedArgs.Exists("v"))
    {
        productVersion = namedArgs.Item("v");
    }
    else
    {
        var vffile = namedArgs.Exists("vf") ? namedArgs.Item("vf") : "..\\PRODUCTVER.TXT" ;
        productVersion = GetProductVersionFromProject(vffile);
    }

    if (null == productVersion || "" == productVersion) return 1;

    //
    // product code
    //
    var productCode = null;
    if (namedArgs.Exists("c"))
    {
        productCode = namedArgs.Item("c");
    }
    else
    {
        var pcfile = namedArgs.Exists("cf") ? namedArgs.Item("cf") : "ndassetup-product-codes.xml";
        productCode = GetVersionProductCode(pcfile, productVersion);
    }

    if (null == productCode || "" == productCode) return 1;

    //
    // output file
    //
    var outputfile = namedArgs.Exists("output") ? 
        namedArgs("output") : 
        ismfile;

    var wshShell = WScript.CreateObject("WScript.Shell");
    var fso = WScript.CreateObject("Scripting.FileSystemObject");
    
    if (fso.GetAbsolutePathName(outputfile).toLowerCase() !=
        fso.GetAbsolutePathName(ismfile).toLowerCase())
    {
        fso.CopyFile(ismfile, outputfile, true);
    }
    
    WScript.Echo(productVersion, productCode);

    //
    // Replace {{PRODUCT_VERSSION}} in ISString Table
    //
    // wshShell.Run("cscript.exe //nologo isstrings.js /nomerge export " + outputfile + " isstrings.tmp", 8, true);
    var xmlfile = "isstrings.tmp";
    
    //
    // Export to a xmlfile
    //
	{
		var ret = is_ExportStringsToXML(ismfile, xmlfile, 0, true, false);
		if (!ret)
		{
			WScript.Echo("Exporting the string table failed!");
			return 1;
		}
	}
	

    var xml = LoadXMLFile(xmlfile);
    if (null == xml) return 1;

	//
	// Replace strings in the xml file
	//
    ReplaceVersionString_ISSTRING(xml, productVersion);

    xml.save(xmlfile);
    xml = null;

    //
    // Import from the xml file
    //
    // wshShell.Run("cscript.exe //nologo isstrings.js import " + outputfile + " isstrings.tmp", 8, true);
    {
		var ret = is_ImportStringsFromXML(outputfile, xmlfile, 0, false);
		if (!ret)
		{
			WScript.Echo("Importing a string table failed!");
			return 1;
		}
	}
    
    fso.DeleteFile(xmlfile);
    
    var xml = LoadXMLFile(outputfile);
    if (null == xml) return 1;

    //
    // Replace {{PRODUCT_VERSSION}} in xml
    //
    ReplaceVersionString_ISM(xml, productVersion);
    
    //
    // Set Product Version
    //
    SetProductVersion(xml, productVersion);
    
    //
    // Set Product Code
    //
    SetProductCode(xml, productCode);

    //
    // Fix Upgrade Table
    //
    FixUpgradeTable(xml, "9999.9999.9999.9999", productVersion);
    
    xml.save(outputfile);
    xml = null;

    return 0;
}

function cmd_MsiProperty_Set()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 4) return usage();

    var ismfile = args(1);
    var prop = args(2);
    var value = args(3);
    var outputfile = namedArgs.Exists("output") ? namedArgs("output") : ismfile;
    
    var xml = LoadXMLFile(ismfile);
    if (null == xml) return 1;
    
    MsiProperty_Set(xml, prop, value);

	xml.save(outputfile);
	
	return 0;
}

function cmd_MsiProperty_Remove()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();

    var ismfile = args(1);
    var prop = args(2);
    var outputfile = namedArgs.Exists("output") ? namedArgs("output") : ismfile;
    
    var xml = LoadXMLFile(ismfile);
    if (null == xml) return 1;
	
	MsiProperty_Remove(xml, prop)

	xml.save(outputfile);

	return 0;
}

function MsiProperty_Set(xml, prop, value)
{
	var xpath;
    
    xpath = "msi/table[@name='Property']";
    var propTableNode = xml.selectSingleNode(xpath);
    if (null == propTableNode)
	{
		throw new Error("No MSI Property table found");
	}
	
	xpath = "row/td[text()='" + prop + "']";
	var propNameNode = propTableNode.selectSingleNode(xpath);
	if (null == propNameNode)
	{
		var row = xml.createElement("row");

		var td = xml.createElement("td");
		td.text = prop;
		row.appendChild(td);

		td = xml.createElement("td");
		td.text = value;
		row.appendChild(td);

		td = xml.createElement("td");
		row.appendChild(td);

		WScript.Echo(row.xml);
		propTableNode.appendChild(xml.createTextNode("\t"));
		propTableNode.appendChild(row);
		propTableNode.appendChild(xml.createTextNode("\n\t"));

		WScript.Echo("Added " + prop + ": '" + value + "'");  
	}
	else
	{
		var propValueNode = propNameNode.nextSibling;
		var oldValue = propValueNode.text;
		propValueNode.text = value;
		WScript.Echo(prop + ": '" + oldValue + "' -> '" + propValueNode.text + "'");  
	}
	WScript.Echo(propTableNode.xml);
}

function MsiProperty_Remove(xml, prop)
{
    xpath = "msi/table[@name='Property']";
    var propTableNode = xml.selectSingleNode(xpath);
    if (null == propTableNode)
	{
		throw new Error("No MSI Property table found");
	}
	
	xpath = "row/td[text()='" + prop + "']";
	var propNameNode = propTableNode.selectSingleNode(xpath);
	if (null == propNameNode)
	{
		WScript.Echo("MSI Property '" + prop + "' does not exist.");
	}
	else
	{
		var propValueNode = propNameNode.nextSibling;
		var oldValue = propValueNode.text;
		var row = propNameNode.parentNode;
		propTableNode.removeChild(row);
		WScript.Echo(prop + ": '" + oldValue + "' -> (Removed)");
	}
	WScript.Echo(propTableNode.xml);
}

/////////////////////////////////////////////////////////////////////

function is_ExportStringsToXML(ismfile, xmlfile, lang, nomerge, savetemp)
{
    var langs = (0 == lang) ? GetISLanguages(ismfile) : new Array(lang);
    var isproject = CreateISProjectObject();
    var ret = isproject.OpenProject(ismfile, false);
    if (0 != ret) 
    {
        WScript.Echo("Failed to open " + ismfile + " (Error: " + ret + ")");
        return false;
    }
    
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    if (nomerge && fso.FileExists(xmlfile))
    {
        fso.DeleteFile(xmlfile);    
    }
    
    for (var i = 0; i < langs.length; ++i)
    {
        WScript.Echo("Exporting strings (" + langs[i] + ")");
        ExportISStringTable(isproject, xmlfile, langs[i], savetemp);
    }
    
    isproject.CloseProject();
    return true;
}

function is_ImportStringsFromXML(ismfile, xmlfile, lang, savetemp)
{
    var langs = (0 == lang) ? GetISLanguages(ismfile) : new Array(lang);
    var isproject = CreateISProjectObject();
    var ret = isproject.OpenProject(ismfile, true);
    if (0 != ret) 
    {
        WScript.Echo("Failed to open " + ismfile + " (Error: " + ret + ")");
        return false;
    }

    for (var i = 0; i < langs.length; ++i)
    {
        var stringfile = "_is-" + langs[i] + "-import.txt";
        var logfile = "_is-" + langs[i] + "-import.log";
        WScript.Echo("Importing strings (" + langs[i] + ")");

        CreateCSV(xmlfile, langs[i], stringfile);
        ImportISStringTable(isproject, stringfile, langs[i], logfile);

        if (!savetemp)
        {
            var fso = new ActiveXObject("Scripting.FileSystemObject");
            fso.DeleteFile(stringfile);
            fso = null;
        }
    }

    isproject.SaveProject();
    isproject.CloseProject();
    return true;
}

function cmd_ImportISString()
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();

    var ismfile = args(1);
	var xmlfile = args(2);
	
	var opt_savetemp = namedArgs.Exists("savetemp");
	var opt_lang = namedArgs.Exists("lang") ? namedArg("lang") : 0;

	ret = is_ImportStringsFromXML(ismfile, xmlfile, opt_lang, opt_savetemp);
	if (!ret)
	{
		WScript.Echo("Import failed!");
		return 1;
	}
	
    return 0;  
}

/////////////////////////////////////////////////////////////////////

function cmd_ExportISString(ismfile, xmlfile, lang, bSaveTempFile, bNoMerge)
{
    var args = WScript.Arguments.Unnamed;
    var namedArgs = WScript.Arguments.Named;
    if (args.length < 3) return usage();

	//
	// arguments
	//
	var opt_savetemp = namedArgs.Exists("savetemp");
	var opt_nomerge = namedArgs.Exists("nomerge");
	var opt_lang = namedArgs.Exists("lang") ? namedArg("lang") : 0;

    var ismfile = args(1);
    var xmlfile = args(2);

	ret = is_ExportStringsToXML(ismfile, xmlfile, opt_lang, opt_nomerge, opt_savetemp);
	if (!ret)
	{
		WScript.Echo("Export failed!");
		return 1;
	}
	
    return 0;  
}

/////////////////////////////////////////////////////////////////////
//
// Utility Functions
//

function LoadXMLFile(xmlFile)
{
    var xml = new ActiveXObject("MSXML.DOMDocument");
    xml.async = false;
    xml.preserveWhiteSpace = true;
    var fSuccess = xml.load(xmlFile);
    if (!fSuccess)
    {
        WScript.Echo("Unable to load " + xmlFile);
        WScript.Echo("ParseError: (line: " +  xml.parseError.line + ")" +  xml.parseError.reason);
        return null;
    }
    return xml;
}

/////////////////////////////////////////////////////////////////////
//
// value Column is starting from 1 which is nextSibling of the key column
//

function UpdateMSITable(xmlDoc, tableName, keyValue, valueCol, value)
{
    var keyNode = xmlDoc.selectSingleNode(
        "/msi/table[@name='" + tableName + "']/row/td[text()='" + keyValue + "']");
    var valueNode = keyNode;
    for (var i = 0; i < valueCol; ++i)
    {
        valueNode = valueNode.nextSibling;
    }
    valueNode.text = value;   
}

/////////////////////////////////////////////////////////////////////

function SetISPathVariable(xmlDoc, pathVariable, newValue)
{
    UpdateMSITable(xmlDoc, "ISPathVariable", pathVariable, 1, newValue);
}

/////////////////////////////////////////////////////////////////////

function SetPublishingFolder(xmlDoc, path)
{
    SetISPathVariable(xmlDoc, "PublishingFolder", path);
}

/////////////////////////////////////////////////////////////////////

function SetSetupSupportFolder(xmlDoc, path)
{
    SetISPathVariable(xmlDoc, "SetupSupportFolder", path);
}

/////////////////////////////////////////////////////////////////////

function SetMSIPackageFileName(outputXML, name)
{
    var nodes = outputXML.selectNodes(
        "/msi/table[@name='ISProductConfigurationProperty']/row/td[text()='MSIPackageFileName']");
	for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
        var valueNode = node.nextSibling;
        WScript.Echo(node.text + ": " + valueNode.text + " -> " + name );
        valueNode.text = name;
	}
    return 0;
}

/////////////////////////////////////////////////////////////////////

function ReplaceStringInNodes(nodes, re, v)
{
	for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
		if (re.test(node.text)) {
			var newString = node.text.replace(re, v);
			WScript.Echo(" >> " + node.text + " -> " + newString);
			node.text = newString;
		}
	}
}

/////////////////////////////////////////////////////////////////////

function ReplaceVersionString_ISSTRING(parentNode, version_string)
{
	var nodes = parentNode.selectNodes("/isstrings/isstring/text");
	var re = /\{\{ProductVersion\}\}/g;
    ReplaceStringInNodes(nodes, re, version_string);
}

/////////////////////////////////////////////////////////////////////

function ReplaceVersionString_ISM(parentNode, version_string)
{
	var nodes = parentNode.selectNodes("/msi/table/row/td");
	var re = /\{\{ProductVersion\}\}/g;
    ReplaceStringInNodes(nodes, re, version_string);
}

/////////////////////////////////////////////////////////////////////

function IsCScript()
{
    return !( WScript.FullName.match(/cscript.exe$/i) == null )
}

/////////////////////////////////////////////////////////////////////

function SetProductCode(xml, productCode)
{
    UpdateMSITable(xml, "Property", "ProductCode", 1, productCode); 
}

/////////////////////////////////////////////////////////////////////

function SetProductVersion(xml, version)
{
    UpdateMSITable(xml, "Property", "ProductVersion", 1, version); 
}

/////////////////////////////////////////////////////////////////////

function GetBaseVersion(xml)
{
    var nameNode = xml.selectSingleNode(
        "msi/table[@name='Property']/row/td[text()='ProductVersion']");
    var valueNode = nameNode.nextSibling;
    return valueNode.text;
}

/////////////////////////////////////////////////////////////////////

function FixUpgradeTable(xml, baseVersion, newVersion)
{
    //
    // select the values columns from Upgrade Table which has a same baseVersion
    //
    var nodes = xml.selectNodes(
        "msi/table[@name='Upgrade']/row/td[text()='" + baseVersion + "']");

    if (0 == nodes.length) {
        WScript.Echo("Warning: No rows in Upgrade Table with " + baseVersion);
    }

    for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
        node.text = newVersion;
    }
    
}

/////////////////////////////////////////////////////////////////////

function GetVersionProductCode(datafile, version)
{
    var xml = LoadXMLFile(datafile);
    if (null == xml) return null;    
    
    var query = "product-codes/product-code[@version='" + version + "']";
    var node = xml.selectSingleNode(query);
    if (null == node) {
        WScript.Echo("Error: Unable to find product code from " + datafile + " for " + version);
        return null;
    }
    
    return node.text;
}

/////////////////////////////////////////////////////////////////////

function GetProductVersionFromProject(filename)
{
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    try {
        var stm = fso.OpenTextFile(filename, 1, false, 0);
        if (null == stm)
        {
            WScript.Echo("Error: Unable to open " + filename);
            return null;
        }
        var version = stm.ReadLine();
        return version;
    } catch (e) {
        WScript.Echo("Error: Unable to open " + filename + "\n" + e);
        return null;
    } finally {
        if (null != stm) stm.Close();
    }
}

/////////////////////////////////////////////////////////////////////

function ImportISStringTable(isproject, stringfile, lang, logfile)
{
    if (logfile)
    {
        isproject.ImportStrings(stringfile, lang, eiOverwrite);
    }
    else
    {
        isproject.ImportStrings(stringfile, lang, eiOverwrite, logfile);
    }
}

/////////////////////////////////////////////////////////////////////

function ExportISStringTable(isproject, xmlfile, lang, bSaveTempFile)
{
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var isstrfile = "_is-" + lang + "-export.txt";
    isstrfile = fso.GetAbsolutePathName(isstrfile);

    isproject.ActiveLanguage = lang;
    var fSuccess = isproject.ExportStrings(isstrfile, 0, "");

    if (fSuccess)
    {
        var isstrfile_unicode = isstrfile.replace(/\.txt$/, " (UNICODE).txt");
 
        UpdateXMLFromCSV(isstrfile_unicode, lang, xmlfile);

        if (!bSaveTempFile)
        {
            fso.DeleteFile(isstrfile);
            fso.DeleteFile(isstrfile_unicode);
        }
    }
    else
    {
    }
}

/////////////////////////////////////////////////////////////////////

function IsBuiltInText(stringID)
{
    return (null != stringID.match(/^IDS_/)) ||
        (null != stringID.match(/^DN_AlwaysInstall/)) ||
        (null != stringID.match(/^IDPROP_/)) ||
        (null != stringID.match(/^IIDS_/));
}

/////////////////////////////////////////////////////////////////////

function CreateCSV(xmlfile, langid, outputfile)
{
    var xml = new ActiveXObject("MSXML.DOMDocument");
    xml.async = false;
    var fSuccess = xml.load(xmlfile);
    if (!fSuccess)
    {
        throw new Error("Unable to load " + xmlfile);
    }
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var ostm = fso.CreateTextFile(outputfile, true, true);
    var xpath = "/isstrings/isstring/text[@langid='" + langid + "']";
    var nodes = xml.selectNodes(xpath);
    for (var node = nodes.nextNode(); node != null; node = nodes.nextNode())
    {
        var parentNode = node.parentNode;
        var stringID = parentNode.getAttribute("id");
        var stringValue = node.text;
        var stringComment = node.getAttribute("comment") ? node.getAttribute("comment") : "";
        ostm.WriteLine(stringID + "\t" + stringValue + "\t" + stringComment);
    }
    ostm.Close();
}

/////////////////////////////////////////////////////////////////////

function xml_AppendCR(xml, parentNode)
{
	parentNode.appendChild(xml.createTextNode("\n"));
}

/////////////////////////////////////////////////////////////////////

function CreateTextNode(xml, parentNode, langID, tag)
{
    var textNode = parentNode.selectSingleNode( tag ? 
        "text[@langid='" + langID + "' and @tag='" + tag + "']" :
        "text[@langid='" + langID + "']");
    if (null == textNode)
    {
        var newElement = xml.createElement("text");
        newElement.setAttribute("langid", langID);
        tag && newElement.setAttribute("tag", tag);
        textNode = parentNode.appendChild(newElement);
		xml_AppendCR(xml, parentNode);     
    }
    return textNode;
}


/////////////////////////////////////////////////////////////////////


function CreateISStringNode(xml, parentNode, stringID)
{
    var isStringNode = parentNode.selectSingleNode("isstring[@id='" + stringID + "']");
    if (null == isStringNode)
    {
        var newElement = xml.createElement("isstring");
        newElement.setAttribute("id", stringID);
        isStringNode = parentNode.appendChild(newElement);
		xml_AppendCR(xml, isStringNode);
		xml_AppendCR(xml, parentNode);
    }
    return isStringNode;
}

/////////////////////////////////////////////////////////////////////
//
// create a ISSTRINGS xml file
//
// <isstrings>
//  <isstring id="AX_MY_STRINGS">
//      <text langid="1033" comment="aaa">text here</text>
// </istrings>
//

function UpdateXMLFromCSV(csvfile, langid, xmlfile)
{
    var ForReading = 1, ForWriting = 2, ForAppending = 8;

    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var xml = new ActiveXObject("MSXML.DOMDocument");
    xml.async = false;
    xml.preserveWhiteSpace = true;

    if (fso.FileExists(xmlfile))
    {
        // load existing xml file
        xml.load(xmlfile);
    }
    else
    {
        // create a new xml file
        WScript.Echo("Creating a new xml file: " + xmlfile);
        xml.loadXML("<?xml version='1.0' encoding='UTF-8'?>\n<isstrings>\n</isstrings>");
    }
    
    var rootNode = xml.selectSingleNode("/isstrings");
    var instm = fso.OpenTextFile(csvfile, ForReading, false, -1);
    while (!instm.AtEndOfStream)
    {
        var line = instm.ReadLine();
        var ss = line.split("\t", 3);
        // stringid, text, comment
        var stringID = ss[0];
        // exclude Built-in Strings -- string with ID_
        if (IsBuiltInText(stringID)) continue;

        var text = ss[1];
        var comment = ss[2];

        var isStringNode = CreateISStringNode(xml, rootNode, stringID);
        var textNode = CreateTextNode(xml, isStringNode, langid);

        // set text
        textNode.text = text;
        // set a comment string if it exists.
        comment && textNode.attributes.setNamedItem("comment", comment);
    }

    instm.Close();
    xml.save(xmlfile);
}

/////////////////////////////////////////////////////////////////////

function GetISLanguages(ismfile)
{
    var xml = new ActiveXObject("MSXML.DOMDocument");
    xml.async = false;
    xml.load(ismfile);
    var langs = new Array();
    var nodes = xml.selectNodes("/msi/table[@name='ISLanguage']/row/td[0]");
	for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
        langs.push(node.text);
	}
    xml = null;
    return langs;
}

/////////////////////////////////////////////////////////////////////

function CreateISProjectObject()
{
    // Use Standalone Builder Automation if available
    // Otherwise, use InstallShield 9 Automation
    try 
    {
        var isproject = new ActiveXObject("SAAuto9SP1.ISWiProject");
    }
    catch (e) 
    {
        if (-2146827859 == e.number)
        {
            try
            {
                var isproject = new ActiveXObject("ISWiAutomation9.ISWiProject");
            }
            catch (e2)
            {
                WScript.Echo(e2);
                WScript.Quit(1);
            }
        }
        else
        {
            WScript.Echo(e);
            WScript.Quit(1);
        }
    }
    return isproject;
}

