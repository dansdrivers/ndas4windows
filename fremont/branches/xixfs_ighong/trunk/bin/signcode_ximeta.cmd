@echo off
setlocal enableextensions
set ST_TIMESTAMP=/t http://timestamp.verisign.com/scripts/timestamp.dll 
set ST_SUBJECT=/n "XIMETA, Inc."
set ST_ACERT=/ac "%~dp0cert\MSCV-VSClass3.cer"
set ST_FLAGS=%ST_ACERT% %ST_SUBJECT% %ST_TIMESTAMP%
if defined DDKBUILDENV echo BUILDMSG: Signing Executable (XIMETA) - %*
"%~dp0signtool.exe" sign %ST_FLAGS% %*
