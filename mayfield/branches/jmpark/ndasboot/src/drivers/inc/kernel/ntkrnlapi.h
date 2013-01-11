#ifndef __NTKERNEL_API_H__
#define __NTKERNEL_API_H__

#undef __declspec
#define __declspec(dllimport)

#include <ntddk.h>

int NtKernelInitialize(void);
void NtKernelDestroy(void);

int request_irq(unsigned int irq, void (*handler)(struct _KDPC *, PVOID, PVOID, PVOID), unsigned long irqflags, 
		const char * devname, void *dev_id); 
void free_irq(unsigned int irq, void *dev_id);
void enable_irq(unsigned int irq) ; 
void disable_irq(unsigned int irq) ; 

#define SA_INTERRUPT	0x20000000
#define SA_SHIRQ		0x04000000

extern unsigned int nic_irq;

#undef KeQueryTickCount
VOID  KeQueryTickCount(
    OUT PLARGE_INTEGER  TickCount
    );

VOID
KeDelayExecution(
	IN ULONG Time
	);

#endif __NTKERNEL_API_H__