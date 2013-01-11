#pragma once

FORCEINLINE
VOID
RtfInitializeRuntimeFunctions();

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, RtfInitializeRuntimeFunctions)
#endif

typedef
ULONG
KEQUERYACTIVEPROCESSORCOUNT(
    __out_opt PKAFFINITY ActiveProcessors);

typedef
VOID
IOSETSTARTIOATTRIBUTES(
	__in PDEVICE_OBJECT DeviceObject, 
	__in BOOLEAN DeferredStartIo, 
	__in BOOLEAN NonCancelable);

FORCEINLINE 
ULONG 
KeQueryActiveProcessorCountImpl(__out_opt PKAFFINITY ActiveProcessors)
{
	ULONG pc;
	if (IoIsWdmVersionAvailable(1, 0x20))
	{
		// Windows XP or later (CCHAR)
		pc = (ULONG) (CCHAR) KeNumberProcessors;
	}
	else
	{
		// Windows 2000 (KeNumberProcessors is PCCHAR)
		pc = (ULONG) *(PCCHAR) KeNumberProcessors;
	}
	if (ActiveProcessors)
	{
		*ActiveProcessors = KeQueryActiveProcessors();
	}
	return pc;
}

FORCEINLINE 
VOID 
IoSetStartIoAttributesImpl(
	__in PDEVICE_OBJECT DeviceObject, 
	__in BOOLEAN DeferredStartIo, 
	__in BOOLEAN NonCancelable)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(DeferredStartIo);
	UNREFERENCED_PARAMETER(NonCancelable);
	return;
}

DECLSPEC_SELECTANY KEQUERYACTIVEPROCESSORCOUNT* pKeQueryActiveProcessorCount = KeQueryActiveProcessorCountImpl;
DECLSPEC_SELECTANY IOSETSTARTIOATTRIBUTES* pIoSetStartIoAttributes = IoSetStartIoAttributesImpl;

#ifndef RTL_NUMBER_OF
#define RTL_NUMBER_OF(A) (sizeof(A)/sizeof((A)[0]))
#endif

typedef struct _RTF_ENTRY {
	PCWSTR RoutineName;
	PVOID* Function;
} RTF_ENTRY, PRTF_ENTRY;

typedef struct _RTF_VT_ENTRY {
	UCHAR WdmMajor;
	UCHAR WdmMinor;
	const RTF_ENTRY* Entries;
	ULONG EntryCount;
} RTF_VT_ENTRY, *PRTF_VT_ENTRY;

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg("RTF_INIT")
#endif

DECLSPEC_SELECTANY 
const RTF_ENTRY RTF_WINXP[] = {
	L"IoSetStartIoAttributes", &(PVOID)pIoSetStartIoAttributes, 
};

DECLSPEC_SELECTANY 
const RTF_ENTRY RTF_WS03[] = {
	NULL, NULL,
};

DECLSPEC_SELECTANY 
const RTF_ENTRY RTF_LONGHORN[] = {
	L"KeQueryActiveProcessorCount", &(PVOID)pKeQueryActiveProcessorCount,
};

DECLSPEC_SELECTANY 
const RTF_VT_ENTRY RTF_TABLE[] = {
	1, 0x20, RTF_WINXP, RTL_NUMBER_OF(RTF_WINXP),
//	1, 0x30, RTF_WS03, RTL_NUMBER_OF(RTF_WS03),
	6, 0x00, RTF_LONGHORN, RTL_NUMBER_OF(RTF_LONGHORN),
};

#ifdef ALLOC_DATA_PRAGMA
#pragma data_seg()
#endif

FORCEINLINE
VOID
RtfInitializeRuntimeFunctions()
{
	ULONG i;

	for (i = 0; i < RTL_NUMBER_OF(RTF_TABLE); ++i)
	{
		if (IoIsWdmVersionAvailable(RTF_TABLE[i].WdmMajor, RTF_TABLE[i].WdmMinor))
		{
			ULONG j;
			for (j = 0; j < RTF_TABLE[i].EntryCount; ++j)
			{
				UNICODE_STRING routineName;

				RtlInitUnicodeString(
					&routineName, 
					RTF_TABLE[i].Entries[j].RoutineName);
				
				*RTF_TABLE[i].Entries[j].Function = 
					MmGetSystemRoutineAddress(&routineName);

				ASSERT(NULL != *RTF_TABLE[i].Entries[j].Function);
			}
		}
	}
}
