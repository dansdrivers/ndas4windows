thunk -t thk rawio.thk -o rawio.asm 
ml /DIS_32 /c /W3 /nologo /coff /Fo thk32.obj rawio.asm 
ml /DIS_16 /c /W3 /nologo /Fo thk16.obj rawio.asm 
