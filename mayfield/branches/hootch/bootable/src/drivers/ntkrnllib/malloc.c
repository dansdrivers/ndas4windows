#include "ntkrnlapi.h"

#include "malloc.h"
#include "debug.h"

#define MEM_BOUND	3
#define CELL_OFF 	(sizeof(unsigned long) + MEM_BOUND & ~MEM_BOUND)
#define CELL_SIZE	((sizeof(struct cell) + MEM_BOUND & ~MEM_BOUND) - CELL_OFF)

struct cell {
	unsigned long size ;
	struct cell *next ;
};

struct altab {
	struct cell **plast ;
	struct cell *head ;
};

static struct altab aldata;
static unsigned long malloc_memory_size; 

unsigned char mbuffer[MAX_MALLOC_SIZE];
unsigned char *malloc_buffer = NULL;
KSPIN_LOCK	malloc_spinlock;

unsigned long malloc_init(unsigned long size)
{
	struct cell	*q;

	NbDebugPrint(0,("malloc_init size = 0x%x\n", size));	


	malloc_buffer = (unsigned char *) mbuffer;

	KeInitializeSpinLock(&malloc_spinlock);

	q = (struct cell *)malloc_buffer;
	q->size = (size & ~MEM_BOUND) - CELL_OFF;
	q->next = aldata.head;

	aldata.head = q;
	aldata.plast = &q->next;

	return TRUE; 
}

struct cell **FindMem(unsigned long size)
{
	struct cell *q, **qb;

	if( (qb = aldata.plast) == NULL ) {
		for( qb = &aldata.head; *qb; qb = &(*qb)->next )
			if( size <= (*qb)->size ) return (qb);
	} else {
		for(; *qb; qb = &(*qb)->next )
			if( size <= (*qb)->size ) return (qb);
		q = *aldata.plast;
		for( qb = &aldata.head; *qb != q; qb = &(*qb)->next )
			if( size <= (*qb)->size ) return (qb);
	}

	return NULL;
}

void *Malloc(unsigned long size) 
{
	struct cell *q, **qb;
	KIRQL	OldIrql;

//	NbDebugPrint(0, ("Malloc size = %d, sizeof(KIRQL) = %d\n", size, sizeof(KIRQL)));

	KeAcquireSpinLock(&malloc_spinlock, &OldIrql);

	if( size < CELL_SIZE ) size = CELL_SIZE; 
	size = (size + MEM_BOUND) & ~MEM_BOUND; 

	if((qb = FindMem(size)) == NULL) {
		KeReleaseSpinLock(&malloc_spinlock, OldIrql);
		return NULL; 
	}

	q = *qb;
	if(q->size < size + CELL_OFF + CELL_SIZE )
		*qb = q->next;
	else {
		*qb = (struct cell *)((unsigned char *)q + CELL_OFF + size );
		(*qb)->next = q->next; 
		(*qb)->size = q->size - CELL_OFF - size;
		q->size = size;
	}

	aldata.plast = (qb ? qb : NULL); 
	KeReleaseSpinLock(&malloc_spinlock, OldIrql);

	NbDebugPrint(3, ("Malloc ptr = %p\n", ((char*)q + CELL_OFF)));

	return ((char*)q + CELL_OFF); 
}

void Free(void *ptr)
{
	struct cell *q; 
	KIRQL	OldIrql;

	NbDebugPrint(3, ("Free ptr = %p\n", ptr));

	KeAcquireSpinLock(&malloc_spinlock, &OldIrql);

	if(ptr == NULL) {
		KeReleaseSpinLock(&malloc_spinlock, OldIrql);
		return; 
	}	

	q = (struct cell *)((char*)ptr - CELL_OFF); 
	if( q->size & MEM_BOUND ) {
		KeReleaseSpinLock(&malloc_spinlock, OldIrql);
		return;
	}

	malloc_memory_size += q->size;

	if( aldata.head == NULL || q < aldata.head ) {
		q->next = aldata.head; 
		aldata.head = q;
	} else {
		struct cell *qp; 
		char *qpp; 

		for( qp = aldata.head; qp->next && q > qp->next; qp = qp->next );
		qpp = (char*)qp + CELL_OFF + qp->size;
		if( (char*)q < qpp ) {
			KeReleaseSpinLock(&malloc_spinlock, OldIrql);
			return; 
		}
		else if( (char*)q == qpp ) {
			qp->size += CELL_OFF + q->size; 
			q = qp;
		}
		else {
			q->next = qp->next;
			qp->next = q; 
		}
	}
	if( q->next && ((char*)q + CELL_OFF + q->size) == (char*)q->next ) {
		q->size += CELL_OFF + q->next->size; 
		q->next = q->next->next;
	}
	aldata.plast = &q->next;

	KeReleaseSpinLock(&malloc_spinlock, OldIrql);
}


//////////////////////////////////////////////////	
//		Memory Allocation/Free Functions		//
//////////////////////////////////////////////////

/*
PVOID
  ExAllocatePool(
    IN POOL_TYPE  PoolType,
    IN SIZE_T  NumberOfBytes
    )
{
	UNREFERENCED_PARAMETER(PoolType);

	return (PVOID) Malloc(NumberOfBytes);
}
*/

PVOID
  ExAllocatePoolWithTag(
    IN POOL_TYPE  PoolType,
    IN SIZE_T  NumberOfBytes,
    IN ULONG  Tag
    )
{
	UNREFERENCED_PARAMETER(PoolType);
	UNREFERENCED_PARAMETER(Tag);

	return (PVOID) Malloc(NumberOfBytes);
}


VOID
  ExFreePool(
    IN PVOID  P
    )
{
	Free(P);
}


VOID
  ExFreePoolWithTag(
    IN PVOID  P,
    IN ULONG  Tag 
    )
{
	UNREFERENCED_PARAMETER(Tag);

	Free(P);
}