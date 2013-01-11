.386P
.model FLAT, C

        .xlist
;include callconv.inc
        .list   

.DATA

.CODE

_TEXT   SEGMENT DWORD PUBLIC 'CODE'
        ASSUME DS:FLAT, ES:FLAT, SS:NOTHING, FS:NOTHING, GS:NOTHING

;-----------------------------------------------------------------------
; OSDefaultISR
;-----------------------------------------------------------------------

PUBLIC	OSDefaultISR@0
EXTRN	OSDefault:DWORD
EXTRN	OldDefault:DWORD

OSDefaultISR@0	PROC NEAR

		pushad

; Send an end-of-interrupt to the i8259.
		mov	al,20h
		out	20h,al

; Standard uCOS processing.

		call	OSDefault		

		popad
		
;		jmp		OldDefault
		iretd	; Never excute
OSDefaultISR@0 ENDP

;-----------------------------------------------------------------------
; OSTickISR
;-----------------------------------------------------------------------

PUBLIC	OSTickISR@0
EXTRN	OSTimeTick:DWORD
EXTRN	OldTimeTick:DWORD

OSTickISR@0	PROC NEAR

		pushad

; Send an end-of-interrupt to the i8259.
		mov	al,20h
		out	20h,al

; Standard uCOS processing.

		call	OSTimeTick		

		popad
		
;		jmp		OldTimeTick
		iretd	; Never excute
OSTickISR@0 ENDP

;-----------------------------------------------------------------------
;
; void OSNicISR();
;
; uCOS' clock interrupt handler.
;-----------------------------------------------------------------------
public	OSNicISR@0

extrn		do_IRQ_var:DWORD
extrn		nic_irq:DWORD

OSNicISR@0	proc

		pushad

; Send an end-of-interrupt to the i8259.

;		mov	al,20h
;		out	20h,al

; Standard uCOS processing.
		
		push	dword ptr [nic_irq]
		call	[do_IRQ_var]

		popad
		iretd

OSNicISR@0 ENDP

_TEXT   ends
end
