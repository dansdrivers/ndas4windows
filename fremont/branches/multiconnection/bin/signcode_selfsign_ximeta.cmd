@echo off
setlocal enableextensions
set CERT_PFX="%~dp0cert\ximeta_selfsign_200608.pfx"
set CERT_PFX_PASSWD=ximeta
rem We do not use Timestamping in self-signing options
rem set TIMESTAMP_FLAGS=/t http://timestamp.verisign.com/scripts/timestamp.dll
set TIMESTAMP_FLAGS=
set SIGN_FLAGS=/v /f %CERT_PFX% /p %CERT_PFX_PASSWD%
if defined DDKBUILDENV echo BUILDMSG: Signing Executable (Self-Sign) - %*
"%~dp0signtool.exe" sign %SIGN_FLAGS% %TIMESTAMP_FLAGS% %*
