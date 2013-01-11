@echo off
setlocal enableextensions
REM
REM The following value is the thumbprint of the XIMETA certificate
REM When the SPC certificate is updated, the thumbprint value should be updated as well.
REM
set ST_SHA=-sha1 0d038ccdb4eae7b7e724a42fbf1cc6b3ccb7b7d7
set ST_TIMESTAMP=/t http://timestamp.verisign.com/scripts/timestamp.dll 
set ST_FLAGS=%ST_ACERT% %ST_SHA %ST_TIMESTAMP%
"%~dp0signtool.exe" sign %ST_FLAGS% %*
