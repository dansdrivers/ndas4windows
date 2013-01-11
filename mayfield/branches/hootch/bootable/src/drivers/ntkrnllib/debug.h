#ifndef __NDASBOOT_DEBUG_H__
#define __NDASBOOT_DEBUG_H__

VOID _NbDebugPrintE(IN PCCHAR	DebugMessage,...);
extern LONG	NbDebugLevel;

#ifdef DBG
#define	NbDebugPrint(DEBUGLEVEL, FORMAT) do { if((DEBUGLEVEL) <= NbDebugLevel) { _NbDebugPrintE##FORMAT; } } while(0);
//#define NbDebugPrint(DEBUGLEVEL, FORMAT) 
#else	
#define NbDebugPrint(DEBUGLEVEL, FORMAT) __noop
#endif

#endif __NDASBOOT_DEBUG_H__