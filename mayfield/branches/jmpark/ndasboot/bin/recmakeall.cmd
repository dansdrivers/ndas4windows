@echo off
setlocal ENABLEEXTENSIONS
pushd .
cd %0\..\..\src
cd Admin && call ..\..\bin\recmake.cmd Admin.mak && cd ..
cd AggrMirUI && call ..\..\bin\recmake.cmd AggrMirUI.mak && cd ..
cd LDServ && call ..\..\bin\recmake.cmd LDServ.mak && cd ..

popd
endlocal
