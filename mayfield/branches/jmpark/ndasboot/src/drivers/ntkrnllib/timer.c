#include "ntkrnlapi.h"

#include "time.h"
#include "debug.h"
#include "timer.h"

#define	jiffies	(LONGLONG)(CurrentTime().QuadPart)

LIST_ENTRY TimerListHead;
KSPIN_LOCK TimerListLock;

void timer_init(void)
{
	InitializeListHead(&TimerListHead);
	KeInitializeSpinLock(&TimerListLock);
}

VOID  KeInitializeDpc(
    IN PRKDPC  Dpc,
    IN PKDEFERRED_ROUTINE  DeferredRoutine,
    IN PVOID  DeferredContext
    )
{
	NbDebugPrint(3, ("KeInitializeDpcA: Dpc = %p, DeferredRoutine = %p, DeferredContext = %p\n", Dpc, DeferredRoutine, DeferredContext));

	Dpc->Type = 0;
    Dpc->Number = 0;
    Dpc->Importance =0;
	(VOID) InitializeListHead(&Dpc->DpcListEntry);
    Dpc->DeferredRoutine = DeferredRoutine;
    Dpc->DeferredContext = DeferredContext;
    Dpc->SystemArgument1 = 0;
    Dpc->SystemArgument2 = 0;
    Dpc->Lock = 0;
	
}

VOID
  KeInitializeTimer(
    IN PKTIMER  Timer
    )
{
	NbDebugPrint(3, ("KeInitializeTimerA: Timer = %p\n", Timer));

	Timer->Header;
    Timer->DueTime.QuadPart = 0;
    InitializeListHead(&Timer->TimerListEntry);
    Timer->Dpc = 0;
    Timer->Period = 0;
}

VOID
  KeInitializeTimerEx(
    IN PKTIMER  Timer,
    IN TIMER_TYPE  Type
    )
{
	NbDebugPrint(3, ("KeInitializeTimerA: Timer = %p\n", Timer));

	Timer->Header;
    Timer->DueTime.QuadPart = 0;
    InitializeListHead(&Timer->TimerListEntry);
    Timer->Dpc = 0;
    Timer->Period = 0;
}
 
BOOLEAN
  KeSetTimer(
    IN PKTIMER  Timer,
    IN LARGE_INTEGER  DueTime,
    IN PKDPC  Dpc  OPTIONAL
    ) 
{
	KIRQL OldIrql; 
	
	NbDebugPrint(3, ("KeSetTimerA: Timer = %p, CurrentTime = %I64X, DueTime = %I64X, Dpc = %p\n", Timer, CurrentTime().QuadPart, DueTime.QuadPart, Dpc));	
	
	KeAcquireSpinLock(&TimerListLock, &OldIrql);
	NbDebugPrint(3, ("KeSetTimerA:OldIrql = %X\n", (ULONG)OldIrql));

	if(DueTime.QuadPart < 0) Timer->DueTime.QuadPart = CurrentTime().QuadPart - DueTime.QuadPart;
	else Timer->DueTime.QuadPart = DueTime.QuadPart;
	Timer->Dpc = Dpc;

	InsertTailList(&TimerListHead, &Timer->TimerListEntry);

	KeReleaseSpinLock(&TimerListLock, OldIrql);
	NbDebugPrint(3, ("KeSetTimerA End:TimerListLock = %X\n", (ULONG)TimerListLock));			

	return TRUE;
}



BOOLEAN
  KeCancelTimer(
    IN PKTIMER  Timer
    )
{
	KIRQL OldIrql;
	NbDebugPrint(3, ("KeCancelTimerA: Timer = %p\n", Timer));

	KeAcquireSpinLock(&TimerListLock, &OldIrql);
	RemoveEntryList(&Timer->TimerListEntry);
	KeReleaseSpinLock(&TimerListLock, OldIrql);

	return TRUE;
}

NTSTATUS
  KeDelayExecutionThread(
    IN KPROCESSOR_MODE  WaitMode,
    IN BOOLEAN  Alertable,
    IN PLARGE_INTEGER  Interval
    )
{
	LARGE_INTEGER DueTime;

	if(Interval->QuadPart < 0) DueTime.QuadPart = CurrentTime().QuadPart - Interval->QuadPart;
	else DueTime.QuadPart = Interval->QuadPart;

	while(CurrentTime().QuadPart < DueTime.QuadPart);

	return STATUS_SUCCESS;
}

void RunTimerList(void)
{
	PLIST_ENTRY entry;
	PKTIMER Timer;
	ULARGE_INTEGER DueTime;
	PKDPC  Dpc;
	KIRQL OldIrql;

	NbDebugPrint(3, ("RunTimerList Entered\n"));

	KeAcquireSpinLock(&TimerListLock, &OldIrql);
	do {
		for (entry = TimerListHead.Flink; entry != &TimerListHead; entry = entry->Flink) {
			Timer = CONTAINING_RECORD(entry, KTIMER, TimerListEntry); 
			DueTime = Timer->DueTime;
			NbDebugPrint(3, ("RunTimerList: CurrentTime = %I64X, DueTime = %I64X\n", CurrentTime().QuadPart, DueTime.QuadPart));
			if(((LONGLONG)DueTime.QuadPart - CurrentTime().QuadPart) <= 0) {
//				NbDebugPrint(0, ("RunTimerList: TimerExpired: CurrentTime = %I64X, DueTime = %I64X\n", CurrentTime().QuadPart, DueTime.QuadPart));				
				Dpc = Timer->Dpc;
				Dpc->DeferredRoutine(Dpc, Dpc->DeferredContext, Dpc->SystemArgument1, Dpc->SystemArgument2);
				break;
			}		
		}
	} while(entry != &TimerListHead);	
	KeReleaseSpinLock(&TimerListLock, OldIrql);
}
