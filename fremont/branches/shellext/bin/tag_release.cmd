@echo off
setlocal enableextensions
set FIND=%SYSTEMROOT%\system32\find.exe

if not exist .\PRODUCTVER.txt echo Run this script from the project root directory && exit /b 1
for /f %%a in (PRODUCTVER.txt) do set PRODUCT_VERSION=%%a
set PRODUCT_VERSION=%PRODUCT_VERSION: =%
for /f "usebackq tokens=2,3* delims= " %%a in (`svn info ^| %FIND% "URL: "`) do set SVN_TRUNK_URL=%%a
set SVN_TAGS_ROOT=%SVN_TRUNK_URL:/trunk=/tags/%
set SVN_TAGS_URL=%SVN_TRUNK_URL:/trunk=/tags/%%PRODUCT_VERSION%

echo Product Version: ^<%PRODUCT_VERSION%^>
echo Current URL: %SVN_TRUNK_URL%
echo Tagging URL: %SVN_TAGS_URL%

echo Testing for existing directory: %SVN_TAGS_URL%
for /f "usebackq" %%a in (`svn ls %SVN_TAGS_ROOT% ^| %FIND% "%PRODUCT_VERSION%/"`) do set EXISTING=%%a
if errorlevel 1 (
   echo Error: test failed ^(%ERRORLEVEL%^).
   echo /b %ERRORLEVEL%
)
if "%EXISTING%" neq "" (
   echo Error: %SVN_TAGS_URL% already exists.
   exit /b 1
)

echo Tagging...
svn copy %SVN_TRUNK_URL% %SVN_TAGS_URL% -m "Tagged for %PRODUCT_VERSION%"
if errorlevel 1 (
   echo Error: tagging failed ^(%ERRORLEVEL%^).
   exit /b %ERRORLEVEL%
)

echo Tagging done.
echo Make sure to increment the PRODUCTVER.txt and ndas.ver in the project.


