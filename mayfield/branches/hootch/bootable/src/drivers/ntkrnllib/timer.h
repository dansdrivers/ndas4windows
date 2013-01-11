#ifndef __TIMER_H
#define __TIMER_H

VOID  KeInitializeDpcA(
    IN PRKDPC  Dpc,
    IN PKDEFERRED_ROUTINE  DeferredRoutine,
    IN PVOID  DeferredContext
    );

VOID
  KeInitializeTimerA(
    IN PKTIMER  Timer
    );

BOOLEAN
  KeSetTimerA(
    IN PKTIMER  Timer,
    IN LARGE_INTEGER  DueTime,
    IN PKDPC  Dpc  OPTIONAL
    );

BOOLEAN
  KeCancelTimerA(
    IN PKTIMER  Timer
    );

NTSTATUS
  KeDelayExecutionThreadA(
    IN KPROCESSOR_MODE  WaitMode,
    IN BOOLEAN  Alertable,
    IN PLARGE_INTEGER  Interval
    );

void RunTimerList(void);
void timer_init(void);

#endif
