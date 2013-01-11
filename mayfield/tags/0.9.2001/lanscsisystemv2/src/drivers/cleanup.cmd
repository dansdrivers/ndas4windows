@echo on
pushd .
call ..\..\bin\cleanup.cmd %*
for /d %%a in (*) do @( pushd . & cd %%a & call ..\..\..\bin\cleanup.cmd %* & popd )
popd
