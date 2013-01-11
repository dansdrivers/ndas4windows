@echo off
setlocal enableextensions
set ST_TIMESTAMP=/t http://timestamp.verisign.com/scripts/timestamp.dll 
REM
REM Instead of using /n "XIMETA, Inc", we explicitly uses the SHA1
REM hash value of the certificate. Build machine should has this
REM certificate in certificate store with the private key.
REM
REM SHA1 hash value can be found in certificate details when you open
REM the certificate from the explorer.
REM
REM set ST_SUBJECT=/n "XIMETA, Inc."
set ST_SHA1_HASH=/sha1 0D038CCDB4EAE7B7E724A42FBF1CC6B3CCB7B7D7
REM
REM Windows Kernel-mode driver signing policy requires
REM a cross-certificate between Microsoft and VeriSign
REM /ac MSCV-VSClass3.cer appends the certificate
REM
set ST_ADDITIONAL_CERT=/ac "%~dp0cert\MSCV-VSClass3.cer"

set ST_FLAGS=%ST_ADDITIONAL_CERT% %ST_SUBJECT% %ST_SHA1_HASH% %ST_TIMESTAMP%
if defined DDKBUILDENV echo BUILDMSG: Signing Executable (XIMETA) - %*
"%~dp0signtool.exe" sign %ST_FLAGS% %*
