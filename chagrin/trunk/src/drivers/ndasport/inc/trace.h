#pragma once
#include <evntrace.h>

#ifdef RUN_WPP
//
// Global Logger enables boot-time logging
//
#define WPP_GLOBALLOGGER
//
// If we use ntstrsafe.h, we should define WPP_STRSAFE 
// to use StrSafe functions in WPP macros. Otherwise, compiler
// warnings will be issued.
// 
#define WPP_STRSAFE
//
// If WPP_POOLTYPE is PagedPool by default.
// In rare cases, when the parameters are quite large, memory allocate
// is required. If the allocation is required at DISPATCH_LEVEL,
// we can only NonPagedPool. (Seen once a bug check for this.)
//
#define WPP_POOLTYPE NonPagedPool

#include <wpp/ndasport.wpp.h>
#include <wpp/xtdi.wpp.h>

#define WPP_CONTROL_GUIDS \
	NDASPORT_WPP_CONTROL_GUIDS \
	FILEDISK_WPP_CONTROL_GUIDS \
	NDASDISK_WPP_CONTROL_GUIDS \
	RAMDISK_WPP_CONTROL_GUIDS \
	XTDI_WPP_CONTROL_GUIDS

#define WPP_FLAG_LEVEL_LOGGER(_Flags,_Level) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_FLAG_LEVEL_ENABLED(_Flags,_Level) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

VOID 
NdasPortWppInitTracing2K(
	PDEVICE_OBJECT DeviceObject, 
	PUNICODE_STRING RegistryPath);

#define INIT_DBGPRINTEX()

#else /* RUN_WPP */

extern __declspec(selectany) ULONG NdasPortDebugLevel = 0x00000003;
extern __declspec(selectany) ULONG NdasPortDebugFlags = 0x8FFFFFFF;

extern __declspec(selectany) ULONG NdasPortDebugLevel_GENERAL     = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_POWER       = 4;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_GENERAL = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_PNP     = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_IOCTL   = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_SCSI    = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_GENERAL = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_PNP     = 4;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_IOCTL   = 4;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_SCSI    = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LSPIO       = 0x00000003;
extern __declspec(selectany) ULONG NdasPortDebugLevel_SRB         = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LSP_IO_PREF = -1;

extern __declspec(selectany) struct {
	PULONG Value;
} NdasPortDebugFlagLevel[] = {
	&NdasPortDebugLevel_GENERAL,
	&NdasPortDebugLevel_POWER,
	&NdasPortDebugLevel_FDO_GENERAL,
	&NdasPortDebugLevel_FDO_PNP,
	&NdasPortDebugLevel_FDO_IOCTL,
	&NdasPortDebugLevel_FDO_SCSI,
	&NdasPortDebugLevel_PDO_GENERAL,
	&NdasPortDebugLevel_PDO_PNP,
	&NdasPortDebugLevel_PDO_IOCTL,
	&NdasPortDebugLevel_PDO_SCSI,
	&NdasPortDebugLevel_LSPIO,
	&NdasPortDebugLevel_SRB,
	&NdasPortDebugLevel_LSP_IO_PREF
};

#define NDASPORT_GENERAL            0x000001
#define NDASPORT_POWER              0x000002
#define NDASPORT_FDO_GENERAL        0x000004
#define NDASPORT_FDO_PNP            0x000008
#define NDASPORT_FDO_IOCTL          0x000010
#define NDASPORT_FDO_SCSI           0x000020
#define NDASPORT_PDO_GENERAL        0x000040
#define NDASPORT_PDO_PNP            0x000080
#define NDASPORT_PDO_IOCTL          0x000100
#define NDASPORT_PDO_SCSI           0x000200
#define NDASPORT_LSPIO              0x000400
#define NDASPORT_SRB                0x000800
#define NDASPORT_LSPIO_PERF         0x001000

extern ULONG NdasAtaDebugLevel;
extern ULONG NdasAtaDebugFlags;

#define NDASATA_INIT    0x00000001
#define NDASATA_PROP    0x00000002
#define NDASATA_IO      0x00000003

extern ULONG FileDiskDebugLevel;
extern ULONG FileDiskDebugFlags;

#define FILEDISK_INIT   0x00000001
#define FILEDISK_PROP   0x00000002
#define FILEDISK_IO     0x00000003

extern ULONG RamDiskDebugLevel;
extern ULONG RamDiskDebugFlags;

#define RAMDISK_INIT    0x00000001
#define RAMDISK_PROP    0x00000002
#define RAMDISK_IO      0x00000003

extern ULONG NdasDluDebugLevel;
extern ULONG NdasDluDebugFlags;

#define NDASDLU_INIT   0x00000001
#define NDASDLU_PROP   0x00000002
#define NDASDLU_IO     0x00000003

//#define WPP_FLAG_LEVEL_ENABLED(_Flags,_Level) \
//	((NdasPortDebugFlags & _Flags) && (NdasPortDebugLevel >= _Level))

#define WPP_FLAG_LEVEL_ENABLED WppFlagLevelEnabled

#ifndef BitScanForward
#define BitScanForward _BitScanForward

BOOLEAN
_BitScanForward (
	__out ULONG *Index,
	__in ULONG Mask
	);

#pragma intrinsic(_BitScanForward)
#endif

BOOLEAN 
FORCEINLINE
WppFlagLevelEnabled(ULONG Flag, ULONG Level)
{
	ULONG index, currentLevel;
	if (!(NdasPortDebugFlags & Flag))
	{
		return FALSE;
	}
	if (BitScanForward(&index, Flag))
	{
		if (index >= countof(NdasPortDebugFlagLevel))
		{
			ASSERTMSG("Undefined Debug Flag", FALSE);
			currentLevel = NdasPortDebugLevel;
		}
		else
		{
			currentLevel = *NdasPortDebugFlagLevel[index].Value;
			if (-1 == currentLevel) 
			{
				currentLevel = NdasPortDebugLevel;
			}
		}
	}
	return currentLevel >= Level;
}

#define WPP_LEVEL_ENABLED(_Level) \
	((NdasPortDebugLevel >= _Level))

#ifndef TRACE_LEVEL_FATAL
#define TRACE_LEVEL_FATAL 1
#define TRACE_LEVEL_ERROR 2
#define TRACE_LEVEL_WARNING 3
#define TRACE_LEVEL_INFORMATION 4
#define TRACE_LEVEL_VERBOSE 5
#endif

#if DBG
//
// Declared in init.c
//

#define NdasPortTrace (*_DbgPrintEx)

// typedef ULONG (* PDBGPRINTEX)(ULONG, ULONG, PCHAR, ...);
typedef ULONG (__cdecl *PDBGPRINTEX)(ULONG, ULONG, PCHAR, ...);
typedef ULONG (NTAPI *PVDBGPRINTEX)(ULONG, ULONG, PCH, va_list);

FORCEINLINE
VOID 
INIT_DBGPRINTEX();

FORCEINLINE
ULONG 
__cdecl
DbgPrintExImpl(
	ULONG ComponentId, 
	ULONG Level, 
	LPSTR Format, ...);

FORCEINLINE
ULONG 
NTAPI 
vDbgPrintExImpl(
	__in ULONG ComponentId,
	__in ULONG Level,
	__in PCH Format,
	__in va_list arglist);

__declspec(selectany) extern PDBGPRINTEX _DbgPrintEx = &DbgPrintExImpl;
__declspec(selectany) extern PVDBGPRINTEX _vDbgPrintEx = &vDbgPrintExImpl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, INIT_DBGPRINTEX)
#endif

FORCEINLINE
VOID 
INIT_DBGPRINTEX()
{
	static CONST struct {
		PCWSTR RoutineName;
		PVOID* RoutineAddressPointer;
	} Routines[] = {
		L"DbgPrintEx", &(PVOID)(_DbgPrintEx)
	};
	ULONG i;
	PAGED_CODE();
	for (i = 0; i < sizeof(Routines)/sizeof(Routines[0]); ++i)
	{
		PVOID RoutineAddress;
		UNICODE_STRING name;
		RtlInitUnicodeString(&name, Routines[i].RoutineName);
		ASSERT(NULL != Routines[i].RoutineAddressPointer);
		RoutineAddress = MmGetSystemRoutineAddress(&name);
		if (NULL != RoutineAddress)
		{
			*Routines[i].RoutineAddressPointer = RoutineAddress;
		}
	}
}

FORCEINLINE
ULONG 
__cdecl
DbgPrintExImpl(
	__in ULONG ComponentId, 
	__in ULONG Level, 
	__in LPSTR Format, ...)
{
	if (WPP_FLAG_LEVEL_ENABLED(ComponentId, Level))
	{
		NTSTATUS status;
		CHAR buffer[512];
		va_list ap;
		va_start(ap, Format);
		status = RtlStringCbVPrintfA(buffer, sizeof(buffer), Format, ap);
		ASSERT(STATUS_SUCCESS == status || STATUS_BUFFER_OVERFLOW == status);
		status = DbgPrint(buffer);
		va_end(ap);
		return status;
	}
	return STATUS_SUCCESS;
}

FORCEINLINE
ULONG
NTAPI
vDbgPrintExImpl(
	__in ULONG ComponentId,
	__in ULONG Level,
	__in PCH Format,
	__in va_list arglist)
{
	UNREFERENCED_PARAMETER(ComponentId);
	if (WPP_LEVEL_ENABLED(Level))
	{
		NTSTATUS status;
		CHAR buffer[512];
		status = RtlStringCbVPrintfA(buffer, sizeof(buffer), Format, arglist);
		ASSERT(STATUS_SUCCESS == status || STATUS_BUFFER_OVERFLOW == status);
		status = DbgPrint(buffer);
		return status;
	}
	return STATUS_SUCCESS;
}

NTSTATUS
W2kTraceMessage(
	IN TRACEHANDLE LoggerHandle, 
	IN ULONG TraceOptions, 
	IN LPGUID MessageGuid, 
	IN USHORT MessageNumber, ...);

#else /* DBG */
#define NdasPortTrace __noop
#define INIT_DBGPRINTEX() 
#endif /* DBG */

#endif /* RUN_WPP */

