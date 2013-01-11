#include "lspkrnl.h"
/*++

Cancel Safe Queue Support Routines

--*/

VOID 
LspInsertIrp (
    IN PIO_CSQ   Csq,
    IN PIRP Irp)
{
    PDEVICE_EXTENSION   deviceExtension;

    deviceExtension = CONTAINING_RECORD(
		Csq, 
        DEVICE_EXTENSION, 
		CancelSafeQueue);

    InsertTailList(&deviceExtension->PendingIrpQueue, 
                         &Irp->Tail.Overlay.ListEntry);
}

VOID 
LspRemoveIrp(
    IN PIO_CSQ Csq,
    IN PIRP Irp)
{
    PDEVICE_EXTENSION   deviceExtension;

    deviceExtension = CONTAINING_RECORD(
		Csq, 
        DEVICE_EXTENSION,
		CancelSafeQueue);
    
    RemoveEntryList(&Irp->Tail.Overlay.ListEntry);
}


PIRP 
LspPeekNextIrp(
    IN PIO_CSQ Csq,
    IN PIRP    Irp,
    IN PVOID   PeekContext)
{
    PDEVICE_EXTENSION      deviceExtension;
    PIRP                    nextIrp = NULL;
    PLIST_ENTRY             nextEntry;
    PLIST_ENTRY             listHead;
    PIO_STACK_LOCATION     irpStack;

    deviceExtension = CONTAINING_RECORD(
		Csq, 
        DEVICE_EXTENSION, 
		CancelSafeQueue);
    
    listHead = &deviceExtension->PendingIrpQueue;

    // 
    // If the IRP is NULL, we will start peeking from the listhead, else
    // we will start from that IRP onwards. This is done under the
    // assumption that new IRPs are always inserted at the tail.
    //
        
    if(Irp == NULL) {       
        nextEntry = listHead->Flink;
    } else {
        nextEntry = Irp->Tail.Overlay.ListEntry.Flink;
    }
    
    while(nextEntry != listHead) {
        
        nextIrp = CONTAINING_RECORD(nextEntry, IRP, Tail.Overlay.ListEntry);

        irpStack = IoGetCurrentIrpStackLocation(nextIrp);
        
        //
        // If context is present, continue until you find a matching one.
        // Else you break out as you got next one.
        //
        
        if(PeekContext) {
            if(irpStack->FileObject == (PFILE_OBJECT) PeekContext) {       
                break;
            }
        } else {
            break;
        }
        nextIrp = NULL;
        nextEntry = nextEntry->Flink;
    }

    return nextIrp;
    
}


VOID 
LspAcquireLock(
    IN  PIO_CSQ Csq,
    OUT PKIRQL Irql)
{
    PDEVICE_EXTENSION   deviceExtension;

    deviceExtension = CONTAINING_RECORD(
		Csq, 
        DEVICE_EXTENSION, 
		CancelSafeQueue);

    KeAcquireSpinLock(&deviceExtension->QueueLock, Irql);
}

VOID 
LspReleaseLock(
    IN PIO_CSQ Csq,
    IN KIRQL Irql)
{
    PDEVICE_EXTENSION   deviceExtension;

    deviceExtension = CONTAINING_RECORD(
		Csq, 
		DEVICE_EXTENSION, 
		CancelSafeQueue);
    
    KeReleaseSpinLock(&deviceExtension->QueueLock, Irql);
}

VOID 
LspCompleteCanceledIrp(
    IN  PIO_CSQ pCsq,
    IN  PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    
	LSP_KDPRINT(("cancelled irp\n"));
    
	IoCompleteRequest(
		Irp, 
		IO_NO_INCREMENT);
}

