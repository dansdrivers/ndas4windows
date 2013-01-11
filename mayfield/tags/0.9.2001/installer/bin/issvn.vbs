Option Explicit

Dim fso
Dim baseDir, isvFile, ismFile
Dim command, projectName
Dim isproj
Dim ret

'
' checks argument count
'
If WScript.Arguments.Count <> 2 Then
	Usage
	WScript.Quit -1
End If

command = LCase(WScript.Arguments(0))

'
' resolves the absolute path of the project files
'

projectName = WScript.Arguments(1)
isvFile = projectName & ".isv"
ismFile = projectName & ".ism"

Set fso = WScript.CreateObject("Scripting.FileSystemObject")

isvFile = fso.GetAbsolutePathName(isvFile)
ismFile = fso.GetAbsolutePathName(ismFile)

'
'
'

Select Case command

  Case "export"

  	If Not fso.FileExists(ismFile) Then
  		WScript.Echo "Error: " & ismFile & " not exists."
  		WScript.Quit -3
  	End If
	baseDir = fso.GetFile(ismFile).ParentFolder.Path
	WScript.Echo "Project : " & projectName
	WScript.Echo "Base Dir: " & baseDir
	WScript.Echo "ISV File: " & isvFile
	WScript.Echo "ISM File: " & ismFile

	ExportToISV ismFile, isvFile
	
  Case "import"

	If Not fso.FileExists(isvFile) Then
		WScript.Echo "Error: " & isvFile & " not exists."
		WScript.Quit -3
	End If

	baseDir = fso.GetFile(isvFile).ParentFolder.Path
	WScript.Echo "Project : " & projectName
	WScript.Echo "Base Dir: " & baseDir
	WScript.Echo "ISV File: " & isvFile
	WScript.Echo "ISM File: " & ismFile

	ImportFromISV ismFile, isvFile

  Case Else

    Usage
    WScript.Quit -2

End Select

  
WScript.Quit

Sub Usage
	WScript.Echo "usage: cscript.exe isv2ism.vbs <export|import> projectname"
End Sub

Sub ImportFromISV(ismFile, isvFile)
	Dim isproj

	WScript.Echo "Importing from ISV to ISM ..."
	Set isproj = CreateObject("ISWiAutomation.ISWiProject")
	
	' Convert .isv to .ism
	ret = isproj.ImportProject(ismFile, isvFile)

	WScript.Echo "Returned " & ret

	Set isproj = Nothing
End Sub

Sub ExportToISV(ismFile, isvFile)
	Dim isproj

	WScript.Echo "Importing from ISV to ISM ..."
	Set isproj = CreateObject("ISWiAutomation.ISWiProject")
	
	' Convert .isv to .ism
	ret = isproj.ExportProject(isvFile, ismFile)

	WScript.Echo "Returned " & ret

	Set isproj = Nothing
End Sub

