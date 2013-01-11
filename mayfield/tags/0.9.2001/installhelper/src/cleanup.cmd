@echo off
if exist .\logtime.log del .\logtime.log
call ..\bin\cleanup.cmd %*
for /d %%a in (*) do @( pushd . & cd %%a & ..\..\bin\cleanup.cmd %* & popd )