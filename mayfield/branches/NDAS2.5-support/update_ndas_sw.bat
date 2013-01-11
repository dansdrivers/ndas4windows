net stop ndassvc
@del %SYSTEMROOT%\system32\wshlpx.dll-
ren %SYSTEMROOT%\system32\wshlpx.dll wshlpx.dll-
copy .\*.sys %SYSTEMROOT%\system32\drivers\
copy .\*.sys "%PROGRAMFILES%\NDAS\Drivers\"
copy .\*.exe "%PROGRAMFILES%\NDAS\System\"
copy .\*.dll "%PROGRAMFILES%\NDAS\System\"

copy .\wshlpx.dll %SYSTEMROOT%\system32\
pause
 