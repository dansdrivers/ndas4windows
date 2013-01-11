/*++


Copyright (c) 2005  Ximeta Coporation

Module Name:

    Descriptor.c

Abstract:

    This module provides the x86 emulation for the Arc routines which are
    built into the firmware on ARC machines.

    N. B.   This is where all the initialization of the SYSTEM_PARAMETER_BLOCK
            takes place.  If there is any non-standard hardware, some of the
            vectors may have to be changed.  This is where to do it.


Author:

    Park,Junmo 8-April-2005

Environment:

    x86 only

Revision History:

--*/
#include "ntkrnlapi.h"

#include "desc.h"
#include "malloc.h"
#include "debug.h"
#include "irq.h"

PKIDTENTRY Idt;
void *DescriptorTablePtr = NULL;

#define IRQ_KEYBOARD	1
ULONG OSDefault, OldDefault;

void StoreDescriptorTable()
{
   	struct Descriptor GdtDef, IdtDef;
   	ULONG	BlockSize;

	//
   	// Get the current location of the GDT & IDT
   	//
	
	_asm {
       	sgdt GdtDef;
       	sidt IdtDef;
    }
	
	Idt = (PKIDTENTRY)IdtDef.Base; 

    if (GdtDef.Base + GdtDef.Limit + 1 != IdtDef.Base) {

       //
       // Just a sanity check to make sure that the IDT immediately
       // follows the GDT.  (As set up in SUDATA.ASM)
       //

       	NbDebugPrint(1, ("ERROR - GDT and IDT are not contiguous!\n"));
       	NbDebugPrint(1, ("GDT - %lx (%x)  IDT - %lx (%x)\n",
           		GdtDef.Base, GdtDef.Limit,
           		IdtDef.Base, IdtDef.Limit));
       	while (1);
    }

    BlockSize = GdtDef.Limit+1 + IdtDef.Limit+1;

	DescriptorTablePtr = Malloc(BlockSize);

	RtlCopyMemory(DescriptorTablePtr, (PVOID)GdtDef.Base, BlockSize);
	
}


void RestoreDescriptorTable()
{
   	struct Descriptor GdtDef, IdtDef;
    ULONG 	BlockSize;

	__asm {
		cli
	}

	//
    // Get the current location of the GDT & IDT
    //
    _asm {
       	sgdt GdtDef;
       	sidt IdtDef;
    }

    if (GdtDef.Base + GdtDef.Limit + 1 != IdtDef.Base) {

       //
       // Just a sanity check to make sure that the IDT immediately
       // follows the GDT.  (As set up in SUDATA.ASM)
       //

       	NbDebugPrint(1, ("ERROR - GDT and IDT are not contiguous!\n"));
       	NbDebugPrint(1, ("GDT - %lx (%x)  IDT - %lx (%x)\n",
           		GdtDef.Base, GdtDef.Limit,
           		IdtDef.Base, IdtDef.Limit));
       	while (1);
    }

    BlockSize = GdtDef.Limit+1 + IdtDef.Limit+1;

	RtlCopyMemory((PVOID)GdtDef.Base, DescriptorTablePtr, BlockSize);

	DescriptorTablePtr = NULL;

}


ULONG GetInvVector(ULONG Irq)
{
	ULONG Vector;

	Vector = (Idt[Irq].ExtendedOffset << 16) | (Idt[Irq].Offset); 

	return Vector;
}

void SetIntVector(ULONG VecNo, ULONG Vector) 
{
	ULONG OldVector;

	OldVector = (Idt[VecNo].ExtendedOffset << 16) | (Idt[VecNo].Offset); 

	Idt[VecNo].Offset   = (USHORT) (Vector & 0xffff);
	Idt[VecNo].ExtendedOffset  = (USHORT) (Vector >> 16);
	Idt[VecNo].Selector = CS_SELECTOR;
	Idt[VecNo].Access     = IDTGATE_INT;

	NbDebugPrint(0,("Timer: VecNO = %d, OldVector = 0x%x ==> Vector = 0x%x\n", VecNo, OldVector, Vector));
}

extern void        OSDefaultISR(void);
void OSUserDefault(void);

void intvec_init(void)
{
	OldDefault = GetInvVector(REAL_IRQ(IRQ_KEYBOARD));
	SetIntVector(REAL_IRQ(IRQ_KEYBOARD), (ULONG)OSDefaultISR);
	OSDefault = (ULONG) OSUserDefault;
}

void OSUserDefault(void)
{
#if 0
// Intel 8259 ports
#define I8259_A0	0x020				// 8259 #1, port #1
#define I8259_A1	0x021				// 8259 #1, port #2
#define I8259_B0	0x0a0				// 8259 #2, port #1
#define I8259_B1	0x0a1				// 8259 #2, port #2
	{
		unsigned char mastermask, slavemask;

		mastermask = ScsiPortReadPortUchar((PUCHAR)(I8259_A1));		
		slavemask = ScsiPortReadPortUchar((PUCHAR)I8259_B1);		

		NbDebugPrint(0,	("mastermask = %02X, slavemask = %02X\n", mastermask, slavemask));	
	

	}

#endif
	NbDebugPrint(0,("OSUserDefault called !!!!\n"));	
}

void DisplayIdt()
{
	struct Descriptor GdtDef, IdtDef;
	PKIDTENTRY Idt;
	int i;

	_asm {
        	sgdt GdtDef;
        	sidt IdtDef;
    	}

	Idt = (PKIDTENTRY)IdtDef.Base; 
			
	NbDebugPrint(1, (" Idt = %lx\n", Idt));
	for(i=0x00;i<0x100; i+=0x10) {
		NbDebugPrint(1, (" %x : ", i+1));
		NbDebugPrint(1, ("Offset = %x,", Idt[i+1].Offset));
		NbDebugPrint(1, ("Selector = %x,", Idt[i+1].Selector));
		NbDebugPrint(1, ("Access = %x,", Idt[i+1].Access));	
		NbDebugPrint(1, ("ExtendedOffset = %x\n", Idt[i+1].ExtendedOffset));
	}
	       
}
