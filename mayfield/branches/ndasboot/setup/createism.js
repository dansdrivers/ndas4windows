
//
// CScript Only!
//
if (!isCscript()) {
    WScript.Echo("This command should be executed from the command line.\n");
    WScript.Quit(1);
}

var PRODUCT_CODE_XML_FILE = "ndassetup-product-codes.xml";
var DEFAULT_BASE_ISM_FILE = "ndassetup.ism";
var DEFAULT_OUTPUT_ISM_FILE = "_" + DEFAULT_BASE_ISM_FILE;

var namedArgs = WScript.Arguments.Named;

/////////////////////////////////////////////////////////////////////
//
// Arguments
//
/////////////////////////////////////////////////////////////////////

//
// ProductVersion
//

var ProductVersion = namedArgs.Exists("v") ? namedArgs.Item("v") : 
    getProductVersionFromProject("..\\PRODUCTVER.TXT");

if (null == ProductVersion) {
    WScript.Echo("Unable to retrieve the product version. Use /v to set the version explicitly.");
    WScript.Quit(1);
}

WScript.Echo("Product Version : " + ProductVersion);

//
// Get the product codes
//
var ProductCode = namedArgs.Exists("c") ? namedArgs.Item("c") : 
    getVersionProductCode(PRODUCT_CODE_XML_FILE, ProductVersion);

if (null == ProductCode) {
    WScript.Echo("Unable to retrieve the product code. Use /c to set the version explicitly.");
    WScript.Quit(1);
}

WScript.Echo("Product Code    : " + ProductCode);

//
// Base ISM File
//
var BaseISMFile = namedArgs.Exists("i") ? namedArgs.Item("i") : 
    DEFAULT_BASE_ISM_FILE;

WScript.Echo("Base ISM        : " + BaseISMFile);

//
// Output ISM File
//

var OutputISMFile = namedArgs.Exists("o") ? namedArgs.Item("o") : 
    DEFAULT_OUTPUT_ISM_FILE;

WScript.Echo("Output ISM      : " + OutputISMFile);

/////////////////////////////////////////////////////////////////////
//
// Processing
//
/////////////////////////////////////////////////////////////////////

//
// Replace {{ProductVersion}}
//

//
// Copying ISM file
//
try {
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	fso.CopyFile(BaseISMFile, OutputISMFile, true);
} catch(e) {
	WScript.Echo("Copying " + BaseISMFile + " to " + OutputISMFile + "failed: ", e.description);
	WScript.Quit(1);
}

//
// Load Output File to process ProductCode and ProductVersion
//
WScript.Echo("Loading " + OutputISMFile + "...");

var outputXML = new ActiveXObject("MSXML2.DOMDocument");
outputXML.async = false;
outputXML.preserveWhiteSpace = true;
ret = outputXML.load(OutputISMFile);
if (!ret) {
    WScript.Echo("Unable to load " + OutputISMFile);
    WScript.Echo("ParseError: (line: " + outputXML.parseError.line + ")" + outputXML.parseError.reason);
    WScript.Quit(1);
}

//
// Processing ProductVersion
//
WScript.Echo("Replacing ProductVersion strings...");
replaceVersionString(outputXML, ProductVersion);

//
// Get the base version
//
var BaseProductVersion = getBaseVersion(outputXML);

//
// Fix Product Version
//
WScript.Echo("Fixing ProductCode: " + ProductCode);
setProductCode(outputXML, ProductCode);

//
// Fix Product Code
//
WScript.Echo("Fixing ProductVersion: " + ProductVersion);
setProductVersion(outputXML, ProductVersion);

//
// Fix Upgrade Table
//
WScript.Echo("Fixing Upgrade table: " + BaseProductVersion + " -> " + ProductVersion);
fixUpgradeTable(outputXML, BaseProductVersion, ProductVersion);

//
// Save the final output file
//
outputXML.save(OutputISMFile);

/////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
/////////////////////////////////////////////////////////////////////

function replaceVersionString(outputXML, version_string)
{
	var nodes = outputXML.selectNodes("//msi/table/row/td");
	var re = /\{\{ProductVersion\}\}/g;
	for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
		if (re.test(node.text)) {
			var newString = node.text.replace(re, version_string);
			WScript.Echo(" >> " + node.text + " -> " + newString);
			node.text = newString;	
		}
	}
}

//
// This may not working in MBCS environment
//
function replaceVersionString__(infilename, outfilename, version_string)
{
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    var infile = fso.OpenTextFile(infilename, 1, false, 0);
    var outfile = fso.CreateTextFile(outfilename, true, false);

    //
    /// Replaces {{ProductVersion}} to the version string
    //
    while (!infile.AtEndOfStream) {
        var line = infile.ReadLine();
        var newline = line.replace(/\{\{ProductVersion\}\}/g, version_string);
    	outfile.WriteLine(newline);
    }

    infile.Close();
    outfile.Close();
}

/////////////////////////////////////////////////////////////////////

function isCscript()
{
    return !( WScript.FullName.match(/cscript.exe$/i) == null )
}

/////////////////////////////////////////////////////////////////////

function setProductCode(xml, code)
{
    var nameNode = xml.selectSingleNode(
        "msi/table[@name='Property']/row/td[text()='ProductCode']");
    var valueNode = nameNode.nextSibling;
    valueNode.text = code;
}

/////////////////////////////////////////////////////////////////////

function setProductVersion(xml, version)
{
    var nameNode = xml.selectSingleNode(
        "msi/table[@name='Property']/row/td[text()='ProductVersion']");
    var valueNode = nameNode.nextSibling;
    valueNode.text = version;
}

/////////////////////////////////////////////////////////////////////

function getBaseVersion(xml)
{
    var nameNode = xml.selectSingleNode(
        "msi/table[@name='Property']/row/td[text()='ProductVersion']");
    var valueNode = nameNode.nextSibling;
    return valueNode.text;
}

/////////////////////////////////////////////////////////////////////

function fixUpgradeTable(xml, baseVersion, newVersion)
{

/*	var nodes = outputXML.selectNodes("//msi/table/row/td");
	var re = /\{\{ProductVersion\}\}/g;
	for (var node = nodes.nextNode(); node != null; node = nodes.nextNode()) {
		if (re.test(node.text)) {
			var newString = node.text.replace(re, version_string);
			WScript.Echo(" >> " + node.text + " -> " + newString);
			node.text = newString;	
		}
	}
*/
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

function getVersionProductCode(datafile, version)
{
    var xml = new ActiveXObject("MSXML2.DOMDocument");
    xml.async = false;
    
    var ret = xml.load(datafile);
    if (!ret) {
        WScript.Echo("Error: Unable to read product code file (" + datafile + ")" );
        return null;
    }
    
    var query = "product-codes/product-code[@version='" + version + "']";
    var node = xml.selectSingleNode(query);
    if (null == node) {
        WScript.Echo("Error: Unable to find product code from " + datafile + " for " + version);
        return null;
    }
    
    return node.text;
}

function getProductVersionFromProject(filename)
{
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    try {
        var stm = fso.OpenTextFile(filename, 1, false, 0);
        var version = stm.ReadLine();
        return version;
    } catch (e) {
        return null;
    } finally {
        if (null != stm) stm.Close();
    }
}
