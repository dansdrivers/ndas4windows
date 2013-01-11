#ifndef		__TIME_H
#define		__TIME_H

#define		HZ					40

void time_init(void);

extern LARGE_INTEGER Ticks;
__inline LARGE_INTEGER CurrentTime() { return Ticks; }

#endif		__TIME_H
