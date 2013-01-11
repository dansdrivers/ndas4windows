'/////////////////////////////////////////////////////////////////////////////
'BEGIN ImportExportStrings.vbs
'  
'  InstallShield (R)
'  (c) 2001 - 2003  InstallShield Software Corporation
'  All Rights Reserved.
'  
'
'  This utility was designed to assist in the importing and exporting of string tables
'
'
'  File Name:  ImportExportStrings.vbs
'
'  Description:  Sample VBScript file imports or exports a string table text file
'
'  Usage:  To use the objects in this script, make sure you have the following
'          items in place on your system:
'          1. InstallShield DevStudio 9 must be installed so 
'             the end-user automation is available
'          2. You must have Windows Scripting Host installed.(Wscrip.exe)
'          3. The script expects the following command-line arguments, in 
'             this order:
'             a. The fully qualified path to an existing .ism file.
'             b. The fully qualified path to a text file for the string table entries
'             c. Decimal language identifier
'             d. Import or Export
'
'/////////////////////////////////////////////////////////////////////////////


If Wscript.Arguments.Count < 1 Then
    Wscript.Echo "ISWI Subfolder Utility" & _
        vbNewLine & "1st argument is the full path to the .ism file" & _
        vbNewLine & "2nd argument is the full path to the string table text file" & _
	vbNewLine & "3rd argument is the decimal language identifier" & _
	vbNewLine & "4th argument is either E or I for Export or Import"
    Wscript.Quit 1
End If

' Resolve the full path
Dim sProjectPath, sTextFilePath
Dim fso, oFile
Set fso = CreateObject("Scripting.FileSystemObject"): CheckError

Set oFile = fso.GetFile(WScript.Arguments(0)) : CheckError
sProjectPath = oFile.Path

Set oFile = fso.GetFile(WScript.Arguments(1)) : CheckError
sTextFilePath = oFile.Path

WScript.Echo "Project File: " & sProjectPath
WScript.Echo "Text File: " & sTextFilePath

' Create the end-user automation object
Dim ISWIProject
Set ISWIProject = CreateObject("ISWiAutomation9.ISWiProject"): CheckError

' Open the project specified at the command line
ISWIProject.OpenProject Wscript.Arguments(0): CheckError

if(Wscript.Arguments(3)="E")then
    StringsExport ISWIProject,Wscript.Arguments(1),Wscript.Arguments(2)
elseif(Wscript.Arguments(3)="I")then
    StringsImport ISWIProject,Wscript.Arguments(1),Wscript.Arguments(2)
	' Save and close the project
	ISWIProject.SaveProject: CheckError
end if

' Close the project
ISWIProject.SaveProject: CheckError


'/////////////////////////////////////////////////////////////////////////////
' Export the string table
Sub StringsExport(byref p,byval path, byval language)
	p.ActiveLanguage = language
    p.ExportStrings path, 0 , language
    Wscript.Echo "Exported String table for language " & language & " to " & path
End Sub

'/////////////////////////////////////////////////////////////////////////////
' Import the string table
Sub StringsImport(byref p,byval path, byval language)
	p.ActiveLanguage = language
    p.ImportStrings path, language
    Wscript.Echo "Imported String table for language " & language & " from " & path
End Sub

'/////////////////////////////////////////////////////////////////////////////
Sub CheckError()
    Dim message, errRec
    If Err = 0 Then Exit Sub
    message = Err.Source & " " & Hex(Err) & ": " & Err.Description
    Wscript.Echo message
    Wscript.Quit 2
End Sub

'END ImportExportStrings.vbs
