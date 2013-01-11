@echo off
setlocal

set SRCDIR=%0\..\

echo Cleaning up intermediate files
for /r %SRCDIR% %%a in (*.idb *.obj *.pch *.bsc *.sbr *.res) do echo "%%a" && del /q "%%a"
echo Cleaning up Visual Studio residues
for /r %SRCDIR% %%a in (*.plg *.ncb *.ilk *.clw *.aps *.opt) do echo "%%a" && del /q "%%a"
echo Cleaning up DDK build residues
for /r %SRCDIR% %%a in (build*.wrn build*.err build*.log _obj*.mac) do echo "%%a" && del /q "%%a"
echo Cleaning up miscellaneous files
attrib -r -h -s /s Thumbs.db
for /r %SRCDIR% %%a in (*.positions *.Tags.WW *.bak Thumb?.db) do echo "%%a" && del /q "%%a"
echo Cleaning up Release and Debug directories
for /d /r %SRCDIR% %%a in (Releas? Debu? objchk* objfre*) do echo "%%a" && rmdir /s /q "%%a"
echo Cleaning up _*.h _*.rc2 _*.js in UIResource
for /r %SRCDIR%src\UIResource\  %%a in (_*.h _*.rc2) do echo "%%a" && del /q "%%a"
for /r %SRCDIR%src\UIResource\html %%a in (_*.js) do echo "%%a" && del /q "%%a"

endlocal
