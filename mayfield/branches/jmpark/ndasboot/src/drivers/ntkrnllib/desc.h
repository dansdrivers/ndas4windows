#ifndef _DESC_H
#define _DESC_H

typedef struct _KGDTENTRY {
    USHORT  LimitLow;
    USHORT  BaseLow;
    union {
        struct {
            UCHAR   BaseMid;
            UCHAR   Flags1;     // Declare as bytes to avoid alignment
            UCHAR   Flags2;     // Problems.
            UCHAR   BaseHi;
        } Bytes;
        struct {
            ULONG   BaseMid : 8;
            ULONG   Type : 5;
            ULONG   Dpl : 2;
            ULONG   Pres : 1;
            ULONG   LimitHi : 4;
            ULONG   Sys : 1;
            ULONG   Reserved_0 : 1;
            ULONG   Default_Big : 1;
            ULONG   Granularity : 1;
            ULONG   BaseHi : 8;
        } Bits;
    } HighWord;
} KGDTENTRY, *PKGDTENTRY;

//
// Entry of Interrupt Descriptor Table (IDTENTRY)
//

typedef struct _KIDTENTRY {
   USHORT Offset;
   USHORT Selector;
   USHORT Access;
   USHORT ExtendedOffset;
} KIDTENTRY, *PKIDTENTRY;

#define IDTGATE_TRAP			0x8f00	// Trap gate type
#define IDTGATE_INT				0x8e00	// Interrupt gate type
#define IDTGATE_TASK			0x8500	// Task gate type

#define	CS_SELECTOR				0x08	// CS selector in GDT

#pragma pack(2)
struct Descriptor {
	USHORT		Limit;
    ULONG  		Base;
};
#pragma pack(4)

void StoreDescriptorTable();
void RestoreDescriptorTable();

void intvec_init(void);
ULONG GetInvVector(ULONG Irq);
void SetIntVector(ULONG Irq, ULONG Vector);

#endif // __DESC_H
