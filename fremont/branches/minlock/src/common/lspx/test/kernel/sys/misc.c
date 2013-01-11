#include "lspkrnl.h"

NTSTATUS
pAllocateAndLockMdl(
	__in PVOID Buffer,
	__in ULONG BufferLen,
	__in BOOLEAN NonPagedPool,
	__in KPROCESSOR_MODE AccessMode,
	__in LOCK_OPERATION Operation,
	__out PMDL* Mdl)
{
	NTSTATUS status;
	PMDL localMdl = NULL;

	localMdl = IoAllocateMdl(
		Buffer,
		BufferLen,
		FALSE,
		FALSE,
		NULL);

	if (localMdl == NULL) 
	{
		DebugPrint((1, "nspAllocateMdlAndIrp : Failed to allocate MDL\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	// Initialize the MDL. If the buffer is from NonPaged pool
	// use MmBuildMdlForNonPagedPool. Else, use MmProbeAndLockPages
	//
	if (NonPagedPool == TRUE) 
	{
		MmBuildMdlForNonPagedPool(localMdl);
	}
	else 
	{
		__try 
		{
			MmProbeAndLockPages(localMdl, AccessMode, Operation);
		}
		__except(EXCEPTION_EXECUTE_HANDLER) 
		{
			DebugPrint((1, 
				"nspAllocateMdlAndIrp : Failed to Lockpaged\n"));
			IoFreeMdl(localMdl);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	*Mdl = localMdl;

	return STATUS_SUCCESS;
}

VOID
pUnlockAndFreeMdl(
    __in PMDL Mdl)
{
	MmUnlockPages(Mdl);
	IoFreeMdl(Mdl);
}
