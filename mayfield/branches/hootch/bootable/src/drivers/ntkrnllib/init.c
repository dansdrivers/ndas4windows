#include "ntkrnlapi.h"

#include "malloc.h"
#include "desc.h"
#include "irq.h"
#include "time.h"
#include "timer.h"

int NtKernelInitialize(void)
{
	ULONG ret;

	_asm cli;

	ret = malloc_init(MAX_MALLOC_SIZE);
	if(ret < 0) {
		return ret;
	}

	StoreDescriptorTable();

	IRQ_init();
	time_init();
	timer_init();

	_asm sti;	

	return ret;
}

void NtKernelDestroy(void)
{
	RestoreDescriptorTable();
}