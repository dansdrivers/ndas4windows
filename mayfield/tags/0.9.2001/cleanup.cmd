@echo off
pushd .
cd %0\..\
cd installhelper
call cleanup.cmd
cd ..
cd lanscsisystemv2
call cleanup.cmd
cd ..
popd

