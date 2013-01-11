#include "FatProcs.h"

#undef CONSTANT_UNICODE_STRING

#include <filespy.h>
#include <fspyKern.h>

#include "W2kFatLib.h"


VOID
W2kFatFlushOnDirectoryControl (
    IN PDEVICE_OBJECT		SpyFsDeviceObject,
	IN PDEVICE_OBJECT		baseDeviceObject,
	IN PIO_STACK_LOCATION	IrpSp
	)
{
	PVCB					vcb;
	PVOLUME_DEVICE_OBJECT	volDo;
	PFILE_OBJECT			fileObject;
	PDCB					dcb;
	TYPE_OF_OPEN			typeOfOpen;
	PCCB					ccb;
	BOOLEAN					result0, result1;

	UNREFERENCED_PARAMETER(SpyFsDeviceObject) ;
	
//	baseDeviceObject 
//		= ((PFILESPY_DEVICE_EXTENSION) (SpyFsDeviceObject->DeviceExtension))->BaseVolumeDeviceObject;
	volDo = (PVOLUME_DEVICE_OBJECT)baseDeviceObject;
	vcb = &volDo->Vcb;
			
	fileObject = IrpSp->FileObject;

	SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
					("fileObject->FileName = %Z\n",
					&fileObject->FileName)); 
			
				
				
	ASSERT(fileObject->FsContext);
	
	ccb = (PCCB)fileObject->FsContext2;
	ASSERT(ccb);
	
	typeOfOpen = ( ccb == NULL ? DirectoryFile : UserDirectoryOpen );

	dcb = (PDCB)fileObject->FsContext;
	ASSERT(dcb != NULL);
	
	if(vcb->VirtualVolumeFile)
		result0 = CcPurgeCacheSection (
						&vcb->SectionObjectPointers,
						NULL,
						0,
						FALSE
						);
	else {
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						("No Vcb VirtualVolumeFile\n"));
	}
		
	if(dcb->Specific.Dcb.DirectoryFile)
		result1 = CcPurgeCacheSection (
						&dcb->NonPaged->SectionObjectPointers,
						NULL,
						0,
						FALSE
						);
	else {
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						("No DirectoryFile\n"));
	}
}


VOID
W2kFatPurgeVolume (
    IN PDEVICE_OBJECT	SpyFsDeviceObject,
    IN PDEVICE_OBJECT	BaseDeviceObject
	)
{
	PVCB					vcb;
	PVOLUME_DEVICE_OBJECT	volDo;
	BOOLEAN					result0, result1;
				

	UNREFERENCED_PARAMETER(SpyFsDeviceObject) ;

	volDo = (PVOLUME_DEVICE_OBJECT)BaseDeviceObject;
	vcb = &volDo->Vcb;
			
	if(vcb->VirtualVolumeFile)
		result0 = CcPurgeCacheSection (
						&vcb->SectionObjectPointers,
						NULL,
						0,
						FALSE
						);
	else {
		SPY_LOG_PRINT( LFS_DEBUG_LIB_NOISE, 
						("No Vcb VirtualVolumeFile\n"));
	}

	if(vcb && vcb->RootDcb && vcb->RootDcb->Specific.Dcb.DirectoryFile)
		result1 = CcPurgeCacheSection (
						&vcb->RootDcb->NonPaged->SectionObjectPointers,
						NULL,
						0,
						FALSE
						);

	return;
}
