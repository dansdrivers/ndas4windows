#define	__PRIMARY__
#define __SECONDARY__

#include "LfsProc.h"


NTSTATUS
LfsPreAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension);
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt);

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

	    PLFS_FCB fcb = (PLFS_FCB)fileObject->FsContext;
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForSectionSynchronization Called\n") );

		if (fcb->Header.PagingIoResource != NULL) {

			NDAS_ASSERT( LFS_REQUIRED );
			return STATUS_NOT_IMPLEMENTED;
		}	
	}

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_READONLY && 
		lfsDeviceExt->Readonly && 
		ReadonlyLookUpCcb(lfsDeviceExt->Readonly, fileObject)) {

	    PNDAS_FCB fcb = (PNDAS_FCB)fileObject->FsContext;
		
		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForSectionSynchronization Called\n") );

		if (fcb->Header.PagingIoResource != NULL) {

			NDAS_ASSERT( LFS_REQUIRED );
			return STATUS_NOT_IMPLEMENTED;
		}	
	}

    *CompletionContext = NULL;
	return STATUS_SUCCESS;
}


VOID
LfsPostAcquireForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );

	return;
}


NTSTATUS
LfsPreReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    )
{
	UNREFERENCED_PARAMETER(	Data );
	UNREFERENCED_PARAMETER(	CompletionContext );

    *CompletionContext = NULL;
	return STATUS_SUCCESS;
}


VOID
LfsPostReleaseForSectionSynchronization (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    )
{
	UNREFERENCED_PARAMETER(	Data );
	UNREFERENCED_PARAMETER(	OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );

	return;
}


NTSTATUS
LfsPreAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension);
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt);

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

	    PLFS_FCB fcb = (PLFS_FCB)fileObject->FsContext;
		
		if (fcb->Header.PagingIoResource != NULL) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForCcFlush Called\n") );

			NDAS_ASSERT( LFS_REQUIRED );
			return STATUS_NOT_IMPLEMENTED;
		}	
	}

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_READONLY && 
		lfsDeviceExt->Readonly && 
		ReadonlyLookUpCcb(lfsDeviceExt->Readonly, fileObject)) {

	    PNDAS_FCB fcb = (PNDAS_FCB)fileObject->FsContext;
		
		if (fcb->Header.PagingIoResource != NULL) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForCcFlush Called\n") );

			NDAS_ASSERT( LFS_REQUIRED );
			return STATUS_NOT_IMPLEMENTED;
		}	
	}

    *CompletionContext = NULL;
	return STATUS_SUCCESS;
}


VOID
LfsPostAcquireForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );

	return;
}


NTSTATUS
LfsPreReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID				    *CompletionContext
    )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( CompletionContext );

    *CompletionContext = NULL;
	return STATUS_SUCCESS;
}


VOID
LfsPostReleaseForCcFlush (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    )
{
	UNREFERENCED_PARAMETER( Data );
	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );

	return;
}


NTSTATUS
LfsPreAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    OUT PVOID					*CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension);
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt);

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForModifiedPageWriter Called\n") );
	
		//return STATUS_NOT_IMPLEMENTED ;
	}

    *CompletionContext = NULL;
	return STATUS_SUCCESS;
}


VOID
LfsPostAcquireForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    IN NTSTATUS OperationStatus,
    IN PVOID CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;


	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );
	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPostAcquireForModifiedPageWriter Called\n") );
	
		return;
	}

	return;
}


NTSTATUS
LfsPreReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA Data,
    OUT PVOID *CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension);
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt);

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPreAcquireForModifiedPageWriter Called\n") );
	
		//return STATUS_NOT_IMPLEMENTED;
	}

    *CompletionContext = NULL;

	return STATUS_SUCCESS;
}


VOID
LfsPostReleaseForModifiedPageWriter (
    IN PFS_FILTER_CALLBACK_DATA	Data,
    IN NTSTATUS					OperationStatus,
    IN PVOID					CompletionContext
    )
{
	PDEVICE_OBJECT				deviceObject;
	PFILESPY_DEVICE_EXTENSION	fileSpyExt;
	PLFS_DEVICE_EXTENSION		lfsDeviceExt;
	PFILE_OBJECT				fileObject;

	
	UNREFERENCED_PARAMETER( OperationStatus );
	UNREFERENCED_PARAMETER( CompletionContext );

    deviceObject = Data->DeviceObject;
	fileObject   = Data->FileObject;


    ASSERT( IS_FILESPY_DEVICE_OBJECT(deviceObject) );

	fileSpyExt = (PFILESPY_DEVICE_EXTENSION)((deviceObject)->DeviceExtension) ;
	lfsDeviceExt = (PLFS_DEVICE_EXTENSION)(&fileSpyExt->LfsDeviceExt) ;

	ASSERT( fileSpyExt );
	ASSERT( lfsDeviceExt );

	//
	//	we do this only for Secondary volume.
	//

	if (lfsDeviceExt && 
		lfsDeviceExt->FilteringMode  == LFS_SECONDARY && 
		lfsDeviceExt->Secondary && 
		Secondary_LookUpCcb(lfsDeviceExt->Secondary, fileObject)) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, ("LfsPostAcquireForModifiedPageWriter Called\n") );
	
		return;
	}

	return;
}
