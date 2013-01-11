
; The name of the installer
Name "frhed"

Caption "The Free Hex Editor"
ShowInstDetails show
ShowUninstDetails show

; The file to write
OutFile "..\..\frhed-v1.1.exe"

; Licence
LicenseText "frhed is licenced under the GNU General Public Licence. Please indicate that you agree with the conditions or contact the authors for different terms."
LicenseData "GPL.txt"

; The default installation directory
InstallDir $PROGRAMFILES\frhed

; Registry key to check for directory (so if you install again, it will
; overwrite the old one automatically)
InstallDirRegKey HKLM SOFTWARE\frhed "Install Dir"

; The text to prompt the user to enter a directory
ComponentText "This Setup will install frhed v1.1 on your computer."
EnabledBitmap frhed-checked.bmp
DisabledBitmap frhed-unchecked.bmp

; The text to prompt the user to enter a directory
DirText "Choose a directory to install in to:"

; The stuff to install
Section "frhed (required)"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  ; Put file there
  File "..\..\00-FF.bin"
  File "..\..\Readme.txt"
  File "..\..\sample.tpl"
  File "..\..\frhed.exe"
  File "..\..\after.css"
  File "..\..\before.css"
  File "..\..\frhed.chm"
  File "..\..\frhedx.dll"
  File "..\..\rawio16.dll"
  File "..\..\rawio32.dll"
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\frhed "Install Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\frhed" "DisplayName" "frhed v1.1"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\frhed" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteUninstaller "uninstall.exe"
SectionEnd

; Optional Sourcecode
Section "Sourcecode"
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  File /r "..\..\SOURCE"
SectionEnd

; optional Shortcuts
Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\frhed"
  CreateShortCut "$SMPROGRAMS\frhed\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\frhed\frhed.lnk" "$INSTDIR\frhed.exe" "" "$INSTDIR\frhed.exe" 0
  CreateShortCut "$SMPROGRAMS\frhed\Documentation.lnk" "$INSTDIR\frhed.chm" "" "$INSTDIR\frhed.chm" 0
SectionEnd

; uninstall stuff

UninstallText "This will uninstall frhed. Hit next to continue."

; special uninstall section.
Section "Uninstall"
  ; remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\frhed"
  DeleteRegKey HKCU "HKEY_CURRENT_USER\Software\frhed\v1.1.0"
  DeleteRegKey HKLM SOFTWARE\frhed
  DeleteRegKey HKLM SOFTWARE\frhed
  DeleteRegKey HKCR "*\shell\Open in frhed"
  DeleteRegKey HKCR "Unknown\shell\Open in frhed"
  ReadRegStr $1 HKCR Unknown\shell @
  StrCmp $1 "Open in frhed" NeedToRemoveDefaultInUnknown RemovedDefaultInUnknown
  NeedToRemoveDefaultInUnknown:
    DeleteRegValue HKCR Unknown\shell @
  RemovedDefaultInUnknown:
  ; remove files
  Delete $INSTDIR\00-FF.bin
  Delete $INSTDIR\Readme.txt
  Delete $INSTDIR\sample.tpl
  Delete $INSTDIR\frhed.exe
  Delete $INSTDIR\frhed.chm
  Delete $INSTDIR\frhedx.dll
  Delete $INSTDIR\rawio16.dll
  Delete $INSTDIR\rawio32.dll
  Delete "$INSTDIR\SOURCE\*.*"
  Delete "$INSTDIR\*.*"
  RMDir /r "$INSTDIR\SOURCE"

  ; MUST REMOVE UNINSTALLER, too
  ; Delete $INSTDIR\uninstall.exe
  ; remove shortcuts, if any.
  Delete "$SMPROGRAMS\frhed\*.*"
  ; remove directories used.
  RMDir "$SMPROGRAMS\frhed"
  RMDir "$INSTDIR"
SectionEnd

; eof