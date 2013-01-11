#include "ntkrnlapi.h"

#include "time.h"
#include "debug.h"
#include "desc.h"
#include "timer.h"
#include "irq.h"

#define MAX_INCREMENT	1
ULONG KeMaximumIncrement = MAX_INCREMENT;

volatile KSYSTEM_TIME KeTickCount;
LARGE_INTEGER Ticks;
ULONG OSTimeTick, OldTimeTick;


extern void        OSTickISR(void);
void OSUserTick(void);

#define IRQ_TIMER		0

void time_init(void)
{	
	OldTimeTick = GetInvVector(REAL_IRQ(IRQ_TIMER)); 
	SetIntVector(REAL_IRQ(IRQ_TIMER), (ULONG)OSTickISR);
	
	NbDebugPrint(0,("Timer: Offset = 0x%x, OldTimeTick = 0x%x\n", OSTickISR, OldTimeTick));		
	OSTimeTick = (ULONG)OSUserTick;
	
	Ticks.QuadPart = 0;		
}

void OSUserTick(void)
{
	Ticks.QuadPart ++;

	KeTickCount.LowPart = Ticks.LowPart;
	KeTickCount.High1Time = Ticks.HighPart;
	KeTickCount.LowPart = 0;

//	NbDebugPrint(0,("OSUserTick: UserTick = 0x%I64X\n", Ticks.QuadPart));	
//	if(Ticks.QuadPart % 5 == 0) {
		RunTimerList();
//	}
}


VOID
  KeQuerySystemTime(
    OUT PLARGE_INTEGER  CurrentTime
    )
{
	if(CurrentTime) (CurrentTime)->QuadPart = Ticks.QuadPart;
}

LARGE_INTEGER KeQueryPerformanceCounter(
    OUT PLARGE_INTEGER  PerformanceFrequency  OPTIONAL
    )
{
	if(PerformanceFrequency) (PerformanceFrequency)->QuadPart = HZ;
	return Ticks; 
}

VOID  KeQueryTickCount(
    OUT PLARGE_INTEGER  TickCount
    )
{
	*TickCount = Ticks;
}

ULONG
  KeQueryTimeIncrement(
    )
{
	return KeMaximumIncrement;
}


ULONG tmp;
VOID
KeDelayExecution(
	IN ULONG Time
	)
{
	ULONG i,j;

	tmp = Time;

	for(i=0;i<Time;i++)
		for(j=0;j<1000;j++)
			tmp = tmp + Time;	
}
