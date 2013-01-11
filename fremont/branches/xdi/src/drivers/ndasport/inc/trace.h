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

#ifndef TRACE_LEVEL_FATAL
#define TRACE_LEVEL_FATAL 1
#define TRACE_LEVEL_ERROR 2
#define TRACE_LEVEL_WARNING 3
#define TRACE_LEVEL_INFORMATION 4
#define TRACE_LEVEL_VERBOSE 5
#endif

extern __declspec(selectany) ULONG NdasPortDebugLevel = TRACE_LEVEL_WARNING; // TRACE_LEVEL_WARNING;
extern __declspec(selectany) ULONG NdasPortDebugFlags = 0x8FFFFFFF;

extern __declspec(selectany) ULONG NdasPortDebugLevel_GENERAL     = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_POWER       = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_GENERAL = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_PNP     = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_IOCTL   = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_FDO_SCSI    = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_GENERAL = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_PNP     = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_IOCTL   = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_SCSI    = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_PDO_SCSIEX  = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LSPIO       = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_SRB         = -1;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LSP_IO_PREF = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_RESERVED    = -1;

extern __declspec(selectany) ULONG NdasPortDebugLevel_LU_GENERAL  = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LU_INIT     = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LU_PROP     = TRACE_LEVEL_INFORMATION;
extern __declspec(selectany) ULONG NdasPortDebugLevel_LU_IO       = -1;

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
	&NdasPortDebugLevel_PDO_SCSIEX,
	&NdasPortDebugLevel_LSPIO,
	&NdasPortDebugLevel_SRB,
	&NdasPortDebugLevel_LSP_IO_PREF,
	&NdasPortDebugLevel_RESERVED,
	&NdasPortDebugLevel_RESERVED,
	&NdasPortDebugLevel_LU_GENERAL,
	&NdasPortDebugLevel_LU_INIT,
	&NdasPortDebugLevel_LU_PROP,
	&NdasPortDebugLevel_LU_IO,
	&NdasPortDebugLevel_RESERVED,
	&NdasPortDebugLevel_RESERVED,
	&NdasPortDebugLevel_RESERVED,
	&NdasPortDebugLevel_RESERVED
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
#define NDASPORT_PDO_SCSIEX         0x000400
#define NDASPORT_LSPIO              0x000800
#define NDASPORT_SRB                0x001000
#define NDASPORT_LSPIO_PERF         0x002000

extern ULONG NdasAtaDebugLevel;
extern ULONG NdasAtaDebugFlags;

#define NDASATA_GENERAL 0x010000
#define NDASATA_INIT    0x020000
#define NDASATA_PROP    0x040000
#define NDASATA_IO      0x080000

extern ULONG FileDiskDebugLevel;
extern ULONG FileDiskDebugFlags;

#define FILEDISK_GENERAL 0x010000
#define FILEDISK_INIT    0x020000
#define FILEDISK_PROP    0x040000
#define FILEDISK_IO      0x080000

extern ULONG RamDiskDebugLevel;
extern ULONG RamDiskDebugFlags;

#define RAMDISK_GENERAL 0x010000
#define RAMDISK_INIT    0x020000
#define RAMDISK_PROP    0x040000
#define RAMDISK_IO      0x080000

extern ULONG NdasDluDebugLevel;
extern ULONG NdasDluDebugFlags;

#define NDASDLU_GENERAL 0x010000
#define NDASDLU_INIT    0x020000
#define NDASDLU_PROP    0x040000
#define NDASDLU_IO      0x080000

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

FORCEINLINE
BOOLEAN 
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
	else
	{
		currentLevel = 0;
	}
	return currentLevel >= Level;
}

#define WPP_LEVEL_ENABLED(_Level) \
	((NdasPortDebugLevel >= _Level))

#if DBG
//
// Declared in init.c
//

typedef
ULONG
__cdecl
DBGPRINTEX(
    __in ULONG ComponentId,
    __in ULONG Level,
    __in PCCH Format,
    ...);

typedef DBGPRINTEX *PDBGPRINTEX;

typedef
ULONG
NTAPI
VDBGPRINTEX(
    __in ULONG ComponentId,
    __in ULONG Level,
    __in PCCH Format,
    __in va_list arglist);

typedef VDBGPRINTEX *PVDBGPRINTEX;

typedef
ULONG
NTAPI
VDBGPRINTEXWITHPREFIX(
    __in PCCH Prefix,
    __in ULONG ComponentId,
    __in ULONG Level,
    __in PCCH Format,
    __in va_list arglist);

typedef VDBGPRINTEXWITHPREFIX *PVDBGPRINTEXWITHPREFIX;


FORCEINLINE
VOID 
INIT_DBGPRINTEX();

FORCEINLINE
ULONG 
__cdecl
DbgPrintExImpl(
	ULONG ComponentId, 
	ULONG Level, 
	PCCH Format, 
	...);

FORCEINLINE
ULONG 
NTAPI 
vDbgPrintExImpl(
	__in ULONG ComponentId,
	__in ULONG Level,
	__in PCCH Format,
	__in va_list arglist);

FORCEINLINE
ULONG
NTAPI
vDbgPrintExWithPrefixImpl(
    __in PCCH Prefix,
    __in ULONG ComponentId,
    __in ULONG Level,
    __in PCCH Format,
    __in va_list arglist);

__declspec(selectany) extern PDBGPRINTEX pDbgPrintEx = &DbgPrintExImpl;
__declspec(selectany) extern PVDBGPRINTEX pvDbgPrintEx = &vDbgPrintExImpl;
__declspec(selectany) extern PVDBGPRINTEXWITHPREFIX pvDbgPrintExWithPrefix = &vDbgPrintExWithPrefixImpl;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, INIT_DBGPRINTEX)
#endif

#ifndef DPFLTR_ERROR_LEVEL
#include <dpfilter.h>
#endif
#define DPFLTR_NDASPORT DPFLTR_IHVBUS_ID

__forceinline
VOID
NdasPortTrace(
	ULONG ComponentId, 
	ULONG Level, 
	PCCH Format, ...)
{
	if (WPP_FLAG_LEVEL_ENABLED(ComponentId, Level))
	{
		va_list ap;
		va_start(ap, Format);
		(*pvDbgPrintEx)(DPFLTR_NDASPORT, DPFLTR_ERROR_LEVEL, Format, ap);
		va_end(ap);
	}
}

FORCEINLINE
VOID 
INIT_DBGPRINTEX()
{
	static CONST struct {
		PCWSTR RoutineName;
		PVOID* RoutineAddressPointer;
		UCHAR WdmMajorVersion;
		UCHAR WdmMinorVersion;
	} Routines[] = {
		L"DbgPrintEx", &(PVOID)(pDbgPrintEx), 1, 0x20, // Windows XP or later
		L"vDbgPrintEx", &(PVOID)(pvDbgPrintEx), 1, 0x20, // Windows XP or later
		L"vDbgPrintExWithPrefix", &(PVOID)(pvDbgPrintExWithPrefix), 1, 0x20
	};
	ULONG i;

	PAGED_CODE();

	for (i = 0; i < sizeof(Routines)/sizeof(Routines[0]); ++i)
	{
		PVOID RoutineAddress;
		UNICODE_STRING name;
		RoutineAddress = NULL;
		if (IoIsWdmVersionAvailable(
			Routines[i].WdmMajorVersion, 
			Routines[i].WdmMinorVersion))
		{
			RtlInitUnicodeString(&name, Routines[i].RoutineName);
			ASSERT(NULL != Routines[i].RoutineAddressPointer);
			RoutineAddress = MmGetSystemRoutineAddress(&name);
			ASSERT(NULL != RoutineAddress);
			if (NULL != RoutineAddress)
			{
				*Routines[i].RoutineAddressPointer = RoutineAddress;
			}
		}
	}
}

FORCEINLINE
ULONG 
__cdecl
DbgPrintExImpl(
	__in ULONG ComponentId, 
	__in ULONG Level, 
	__in PCCH Format, ...)
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

FORCEINLINE
ULONG
NTAPI
vDbgPrintExImpl(
	__in ULONG ComponentId,
	__in ULONG Level,
	__in PCCH Format,
	__in va_list arglist)
{
	NTSTATUS status;
	CHAR buffer[512];
	status = RtlStringCbVPrintfA(buffer, sizeof(buffer), Format, arglist);
	ASSERT(STATUS_SUCCESS == status || STATUS_BUFFER_OVERFLOW == status);
	status = DbgPrint(buffer);
	return status;
}

FORCEINLINE
ULONG
NTAPI
vDbgPrintExWithPrefixImpl(
    __in PCCH Prefix,
    __in ULONG ComponentId,
    __in ULONG Level,
    __in PCCH Format,
    __in va_list arglist)
{
	return STATUS_NOT_IMPLEMENTED;
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

#if DBG
#define KdbgPrintEx (*pDbgPrintEx)
#define vKdbgPrintEx (*pvDbgPrintEx)
#else
#define KdbgPrintEx __noop
#define vKdbgPrintEx __noop
#endif /* DBG */

#endif /* RUN_WPP */



