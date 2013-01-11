	page	,132

;Thunk Compiler Version 1.8  May 11 1995 13:16:19
;File Compiled Fri Dec 21 08:29:07 2001

;Command Line: thunk -t thk diskio.thk -o diskio.asm 

	TITLE	$diskio.asm

	.386
	OPTION READONLY
	OPTION OLDSTRUCTS

IFNDEF IS_16
IFNDEF IS_32
%out command line error: specify one of -DIS_16, -DIS_32
.err
ENDIF  ;IS_32
ENDIF  ;IS_16


IFDEF IS_32
IFDEF IS_16
%out command line error: you can't specify both -DIS_16 and -DIS_32
.err
ENDIF ;IS_16
;************************* START OF 32-BIT CODE *************************


	.model FLAT,STDCALL


;-- Import common flat thunk routines (in k32)

externDef MapHInstLS	:near32
externDef MapHInstLS_PN	:near32
externDef MapHInstSL	:near32
externDef MapHInstSL_PN	:near32
externDef FT_Prolog	:near32
externDef FT_Thunk	:near32
externDef QT_Thunk	:near32
externDef FT_Exit0	:near32
externDef FT_Exit4	:near32
externDef FT_Exit8	:near32
externDef FT_Exit12	:near32
externDef FT_Exit16	:near32
externDef FT_Exit20	:near32
externDef FT_Exit24	:near32
externDef FT_Exit28	:near32
externDef FT_Exit32	:near32
externDef FT_Exit36	:near32
externDef FT_Exit40	:near32
externDef FT_Exit44	:near32
externDef FT_Exit48	:near32
externDef FT_Exit52	:near32
externDef FT_Exit56	:near32
externDef SMapLS	:near32
externDef SUnMapLS	:near32
externDef SMapLS_IP_EBP_8	:near32
externDef SUnMapLS_IP_EBP_8	:near32
externDef SMapLS_IP_EBP_12	:near32
externDef SUnMapLS_IP_EBP_12	:near32
externDef SMapLS_IP_EBP_16	:near32
externDef SUnMapLS_IP_EBP_16	:near32
externDef SMapLS_IP_EBP_20	:near32
externDef SUnMapLS_IP_EBP_20	:near32
externDef SMapLS_IP_EBP_24	:near32
externDef SUnMapLS_IP_EBP_24	:near32
externDef SMapLS_IP_EBP_28	:near32
externDef SUnMapLS_IP_EBP_28	:near32
externDef SMapLS_IP_EBP_32	:near32
externDef SUnMapLS_IP_EBP_32	:near32
externDef SMapLS_IP_EBP_36	:near32
externDef SUnMapLS_IP_EBP_36	:near32
externDef SMapLS_IP_EBP_40	:near32
externDef SUnMapLS_IP_EBP_40	:near32

MapSL	PROTO NEAR STDCALL p32:DWORD



	.code 

;************************* COMMON PER-MODULE ROUTINES *************************

	.data

public thk_ThunkData32	;This symbol must be exported.
thk_ThunkData32 label dword
	dd	3130534ch	;Protocol 'LS01'
	dd	080a5h	;Checksum
	dd	0	;Jump table address.
	dd	3130424ch	;'LB01'
	dd	0	;Flags
	dd	0	;Reserved (MUST BE 0)
	dd	0	;Reserved (MUST BE 0)
	dd	offset QT_Thunk_thk - offset thk_ThunkData32
	dd	offset FT_Prolog_thk - offset thk_ThunkData32



	.code 


externDef ThunkConnect32@24:near32

public thk_ThunkConnect32@16
thk_ThunkConnect32@16:
	pop	edx
	push	offset thk_ThkData16
	push	offset thk_ThunkData32
	push	edx
	jmp	ThunkConnect32@24
thk_ThkData16 label byte
	db	"thk_ThunkData16",0


		


pfnQT_Thunk_thk	dd offset QT_Thunk_thk
pfnFT_Prolog_thk	dd offset FT_Prolog_thk
	.data
QT_Thunk_thk label byte
	db	32 dup(0cch)	;Patch space.

FT_Prolog_thk label byte
	db	32 dup(0cch)	;Patch space.


	.code 





;************************ START OF THUNK BODIES************************




;
public WritePhysicalSector@12
WritePhysicalSector@12:
	mov	cl,4
	jmp	IIWritePhysicalSector@12
public ReadPhysicalSector@12
ReadPhysicalSector@12:
	mov	cl,5
; WritePhysicalSector(16) = WritePhysicalSector(32) {}
;
; dword ptr [ebp+8]:  s
; dword ptr [ebp+12]:  lpBuffer
; dword ptr [ebp+16]:  cbBuffSize
;
public IIWritePhysicalSector@12
IIWritePhysicalSector@12:
	push	ebp
	mov	ebp,esp
	push	ecx
	sub	esp,60
	call	SMapLS_IP_EBP_8
	push	eax
	call	SMapLS_IP_EBP_12
	push	eax
	push	dword ptr [ebp+16]	;cbBuffSize: dword->dword
	call	dword ptr [pfnQT_Thunk_thk]
	shl	eax,16
	shrd	eax,edx,16
	call	SUnMapLS_IP_EBP_8
	call	SUnMapLS_IP_EBP_12
	leave
	retn	12





;
public ReadDiskGeometry@4
ReadDiskGeometry@4:
	mov	cl,3
	jmp	IIReadDiskGeometry@4
public ResetDisk@4
ResetDisk@4:
	mov	cl,6
; ReadDiskGeometry(16) = ReadDiskGeometry(32) {}
;
; dword ptr [ebp+8]:  s
;
public IIReadDiskGeometry@4
IIReadDiskGeometry@4:
	push	ebp
	mov	ebp,esp
	push	ecx
	sub	esp,60
	call	SMapLS_IP_EBP_8
	push	eax
	call	dword ptr [pfnQT_Thunk_thk]
	shl	eax,16
	shrd	eax,edx,16
	call	SUnMapLS_IP_EBP_8
	leave
	retn	4





;
public EI13GetDriveParameters@4
EI13GetDriveParameters@4:
	mov	cx, (1 SHL 10) + (0 SHL 8) + 2
; EI13GetDriveParameters(16) = EI13GetDriveParameters(32) {}
;
; dword ptr [ebp+8]:  b
;
public IIEI13GetDriveParameters@4
IIEI13GetDriveParameters@4:
	call	dword ptr [pfnFT_Prolog_thk]
	sub	esp,12
	mov	esi,[ebp+8]
	or	esi,esi
	jnz	L0
	push	esi
	jmp	L1
L0:
	lea	edi,[ebp-76]
	push	edi	;b: lpstruct32->lpstruct16
	or	dword ptr [ebp-20],01h	;Set flag to fixup ESP-rel argument.
	movsb
	add	esi,3
	inc	edi
	movsd
	movsd
	movsw
L1:
	call	FT_Thunk
	shrd	ebx,edx,16
	mov	bx,ax
	mov	edi,[ebp+8]
	or	edi,edi
	jz	L2
	lea	esi,[ebp-76]	;b  Struct16->Struct32
	movsb
	inc	esi
	add	edi,3
	movsd
	movsd
	movsw
L2:
	jmp	FT_Exit4





;
public EI13WriteSector@12
EI13WriteSector@12:
	mov	cx, (3 SHL 10) + (0 SHL 8) + 0
	jmp	IIEI13WriteSector@12
public EI13ReadSector@12
EI13ReadSector@12:
	mov	cx, (3 SHL 10) + (0 SHL 8) + 1
; EI13WriteSector(16) = EI13WriteSector(32) {}
;
; dword ptr [ebp+8]:  b
; dword ptr [ebp+12]:  lpBuffer
; dword ptr [ebp+16]:  bufferSize
;
public IIEI13WriteSector@12
IIEI13WriteSector@12:
	call	dword ptr [pfnFT_Prolog_thk]
	sub	esp,12
	mov	esi,[ebp+8]
	or	esi,esi
	jz	@F
	or	byte ptr [esi], 0
	or	byte ptr [esi + 15], 0
@@:
	mov	esi,[ebp+8]
	or	esi,esi
	jnz	L3
	push	esi
	jmp	L4
L3:
	lea	edi,[ebp-76]
	push	edi	;b: lpstruct32->lpstruct16
	or	dword ptr [ebp-20],010h	;Set flag to fixup ESP-rel argument.
	movsb
	add	esi,3
	inc	edi
	movsd
	movsd
	movsw
L4:
	call	SMapLS_IP_EBP_12
	push	eax
	push	dword ptr [ebp+16]	;bufferSize: dword->dword
	call	FT_Thunk
	shrd	ebx,edx,16
	mov	bx,ax
	mov	edi,[ebp+8]
	or	edi,edi
	jz	L5
	lea	esi,[ebp-76]	;b  Struct16->Struct32
	movsb
	inc	esi
	add	edi,3
	movsd
	movsd
	movsw
L5:
	call	SUnMapLS_IP_EBP_12
	jmp	FT_Exit12




ELSE
;************************* START OF 16-BIT CODE *************************




	OPTION SEGMENT:USE16
	.model LARGE,PASCAL


	.code	



externDef EI13WriteSector:far16
externDef EI13ReadSector:far16
externDef EI13GetDriveParameters:far16
externDef ReadDiskGeometry:far16
externDef WritePhysicalSector:far16
externDef ReadPhysicalSector:far16
externDef ResetDisk:far16


FT_thkTargetTable label word
	dw	offset EI13WriteSector
	dw	   seg EI13WriteSector
	dw	offset EI13ReadSector
	dw	   seg EI13ReadSector
	dw	offset EI13GetDriveParameters
	dw	   seg EI13GetDriveParameters
	dw	offset ReadDiskGeometry
	dw	   seg ReadDiskGeometry
	dw	offset WritePhysicalSector
	dw	   seg WritePhysicalSector
	dw	offset ReadPhysicalSector
	dw	   seg ReadPhysicalSector
	dw	offset ResetDisk
	dw	   seg ResetDisk




	.data

public thk_ThunkData16	;This symbol must be exported.
thk_ThunkData16	dd	3130534ch	;Protocol 'LS01'
	dd	080a5h	;Checksum
	dw	offset FT_thkTargetTable
	dw	seg    FT_thkTargetTable
	dd	0	;First-time flag.



	.code 


externDef ThunkConnect16:far16

public thk_ThunkConnect16
thk_ThunkConnect16:
	pop	ax
	pop	dx
	push	seg    thk_ThunkData16
	push	offset thk_ThunkData16
	push	seg    thk_ThkData32
	push	offset thk_ThkData32
	push	cs
	push	dx
	push	ax
	jmp	ThunkConnect16
thk_ThkData32 label byte
	db	"thk_ThunkData32",0





ENDIF
END
