
After exporting as Makefile, don't forget to change the absolute paths to the relative in the makefiles.
This can be done manually or by using sed. For example

sed -e s/\\Development\\svnwork\\trunk\\lanscsisystemv2\\src\\/..\\/ Admin.mak > tmpfile
move /q tmpfile Admin.mak

Run ..\bin\recmake.cmd <makefile> in the source directory to do above.
