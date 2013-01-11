#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xixfs_global.h"
#include "xixfs_debug.h"
#include "xixfs_internal.h"


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, xixfs_CommonCreate)
#endif


NTSTATUS
xixfs_CommonCreate(
	IN PXIXFS_IRPCONTEXT PtrIrpContext
	)
{
	NTSTATUS					RC = STATUS_SUCCESS;
	PIRP						PtrIrp = NULL;
	PIO_STACK_LOCATION			PtrIoStackLocation = NULL;
	PIO_SECURITY_CONTEXT		PtrSecurityContext = NULL;

	PFILE_OBJECT				PtrNewFileObject = NULL;
	PFILE_OBJECT				PtrRelatedFileObject = NULL;
	PXIXFS_CCB					PtrRelatedCCB = NULL;
	PXIXFS_FCB					PtrRelatedFCB = NULL;
	
	PXIXFS_FCB					CurrentFCB = NULL;
	PXIXFS_FCB					NextFCB = NULL;

	PXIXFS_LCB					CurrentLCB = NULL;
	XIXCORE_DIR_EMUL_CONTEXT		DirContext;
	BOOLEAN						CleanUpDirContext =FALSE;


	PXIXFS_FCB					ParentFCB = NULL;
	PXIXFS_CCB					ParentCCB = NULL;
	
	int64						AllocationSize = 0; 	// if we create a new file
	PFILE_FULL_EA_INFORMATION	PtrExtAttrBuffer = NULL;

	// Create Options
	unsigned long				RequestedOptions = 0;
	unsigned long				RequestedDisposition = 0;

	uint8						FileAttributes = 0;
	unsigned short				ShareAccess = 0;
	unsigned long				ExtAttrLength = 0;
	ACCESS_MASK					DesiredAccess;

	BOOLEAN						DeferredProcessing = FALSE;
	PXIXFS_VCB					PtrVCB = NULL;
	BOOLEAN						AcquiredVCB = FALSE;

	//Create Parameters 
	BOOLEAN						DirectoryOnlyRequested = FALSE;
	BOOLEAN						FileOnlyRequested = FALSE;
	BOOLEAN						NoBufferingSpecified = FALSE;
	BOOLEAN						WriteThroughRequested = FALSE;
	BOOLEAN						DeleteOnCloseSpecified = FALSE;
	BOOLEAN						NoExtAttrKnowledge = FALSE;
	BOOLEAN						CreateTreeConnection = FALSE;
	BOOLEAN						OpenByFileId = FALSE;
	uint8						*NameBuffer = NULL;
	uint8						*NameBuffer2 = NULL;


	// Are we dealing with a page file?
	BOOLEAN						PageFileManipulation = FALSE;

	// Is this open for a target directory (used in rename operations)?
	BOOLEAN						OpenTargetDirectory = FALSE;

	// Should we ignore case when attempting to locate the object?
	BOOLEAN						IgnoreCaseWhenChecking = FALSE;




	NTSTATUS					FoundStatus = STATUS_SUCCESS;

	unsigned long				ReturnedInformation;
	UNICODE_STRING				TargetObjectName;
	UNICODE_STRING				RelatedObjectName;
	UNICODE_STRING				AbsolutePathName;
	UNICODE_STRING				RemainTargetName;
	UNICODE_STRING				FinalName;
	UNICODE_STRING				FullPathName;
	UNICODE_STRING				TargetFileName;
	LARGE_INTEGER				FileAllocationSize, FileEndOfFile;

	TYPE_OF_OPEN				RelatedTypeOfOpen = UnopenedFileObject;
					

	PAGED_CODE();

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CREATE, ("Enter xixfs_CommonCreate .\n"));
	
	ASSERT_IRPCONTEXT(PtrIrpContext);
	PtrIrp = PtrIrpContext->Irp;
	
	
	ASSERT(PtrIrp);

	ReturnedInformation = FILE_DOES_NOT_EXIST;

	// check if open request is releated to file system CDO
	{
		PDEVICE_OBJECT	DeviceObject = PtrIrpContext->TargetDeviceObject;
		ASSERT(DeviceObject);
		
		if (DeviceObject == XiGlobalData.XifsControlDeviceObject) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE, 
				("Open Xifs File system itself DevObj %lx .\n", DeviceObject));
		 
			//DbgPrint("Open Xifs File system itself DevObj %lx .\n", DeviceObject);

			RC = STATUS_SUCCESS;
			ReturnedInformation = FILE_OPENED;
			xixfs_ReleaseIrpContext(PtrIrpContext);
			PtrIrp->IoStatus.Status = RC;
			PtrIrp->IoStatus.Information = FILE_OPENED;
			IoCompleteRequest(PtrIrp, IO_NO_INCREMENT);
			return(RC);
		}

	}

	AbsolutePathName.Buffer = NULL;
	AbsolutePathName.Length = AbsolutePathName.MaximumLength = 0;
	
	PtrIoStackLocation = IoGetCurrentIrpStackLocation(PtrIrp);
	ASSERT(PtrIoStackLocation);



	PtrNewFileObject	= PtrIoStackLocation->FileObject;
	TargetObjectName	= PtrNewFileObject->FileName;
	PtrRelatedFileObject = PtrNewFileObject->RelatedFileObject;

	AllocationSize = PtrIrp->Overlay.AllocationSize.QuadPart;

	

	if (PtrIrp->Overlay.AllocationSize.HighPart) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Allocation Size is too big %I64d .\n", PtrIrp->Overlay.AllocationSize.HighPart));
		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);
		return RC;
	}

	PtrSecurityContext = PtrIoStackLocation->Parameters.Create.SecurityContext;
	DesiredAccess = PtrSecurityContext->DesiredAccess;

	RequestedOptions = (PtrIoStackLocation->Parameters.Create.Options & FILE_VALID_OPTION_FLAGS);
	RequestedDisposition = ((PtrIoStackLocation->Parameters.Create.Options >> 24) & 0x000000FF);

	FileAttributes	= (uint8)(PtrIoStackLocation->Parameters.Create.FileAttributes & FILE_ATTRIBUTE_VALID_FLAGS);
	FileAttributes = (uint8) (FileAttributes & ~FILE_ATTRIBUTE_NORMAL);

	ShareAccess	= PtrIoStackLocation->Parameters.Create.ShareAccess;

	PtrExtAttrBuffer	= PtrIrp->AssociatedIrp.SystemBuffer;
	ExtAttrLength		= PtrIoStackLocation->Parameters.Create.EaLength;


	/*
	//ignore ExtAttrLength
	if(ExtAttrLength){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("XiFS do not support ExtAttr .\n"));

		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);	
		return RC;
	}
	*/

/*
	if(XIXCORE_TEST_FLAGS(RequestedOptions,  FILE_OPEN_REPARSE_POINT )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("XiFS do not support FILE_OPEN_REPARSE_POINT .\n"));

		RC = STATUS_INVALID_PARAMETER;
		xixfs_CompleteRequest(PtrIrpContext, RC, 0);	
		return RC;
	}
*/

	DirectoryOnlyRequested = ((RequestedOptions & FILE_DIRECTORY_FILE) ? TRUE : FALSE);
	FileOnlyRequested = ((RequestedOptions & FILE_NON_DIRECTORY_FILE) ? TRUE : FALSE);
	NoBufferingSpecified = ((RequestedOptions & FILE_NO_INTERMEDIATE_BUFFERING) ? TRUE : FALSE);
	WriteThroughRequested = ((RequestedOptions & FILE_WRITE_THROUGH) ? TRUE : FALSE);
	DeleteOnCloseSpecified = ((RequestedOptions & FILE_DELETE_ON_CLOSE) ? TRUE : FALSE);
	NoExtAttrKnowledge = ((RequestedOptions & FILE_NO_EA_KNOWLEDGE) ? TRUE : FALSE);

	OpenByFileId = ((RequestedOptions & FILE_OPEN_BY_FILE_ID) ? TRUE : FALSE);

	// Check IO mager supplied flag
	PageFileManipulation = ((PtrIoStackLocation->Flags & SL_OPEN_PAGING_FILE) ? TRUE : FALSE);
	OpenTargetDirectory = ((PtrIoStackLocation->Flags & SL_OPEN_TARGET_DIRECTORY) ? TRUE : FALSE);
	IgnoreCaseWhenChecking = ((PtrIoStackLocation->Flags & SL_CASE_SENSITIVE) ? FALSE : TRUE);

	// Ensure that the operation has been directed to a valid VCB ...
	PtrVCB =	PtrIrpContext->VCB;


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE|DEBUG_TARGET_ALL,
		("[CREATE PARAM]\n"
		"\t\tTName(%wZ) Length(%ld):RFObj(%p)\n\t\tAllocSize(%I64d):DAcess(0x%x):ReqOption(0x%x)\n\t\tReqDeposition(0x%x):Attr(0x%x):ShareAccess(0x%x)\n",
				&TargetObjectName, TargetObjectName.Length, PtrRelatedFileObject, AllocationSize, DesiredAccess,
				RequestedOptions, RequestedDisposition, FileAttributes, ShareAccess));


	
	/*
		DbgPrint("[CREATE PARAM]\n"
		"\t\tTName(%wZ) Length(%ld):RFObj(%p)\n\t\tAllocSize(%I64d):DAcess(0x%x):ReqOption(0x%x)\n\t\tReqDeposition(0x%x):Attr(0x%x):ShareAccess(0x%x)\n",
				&TargetObjectName, TargetObjectName.Length, PtrRelatedFileObject, AllocationSize, DesiredAccess,
				RequestedOptions, RequestedDisposition, FileAttributes, ShareAccess);
	*/

	ASSERT_VCB(PtrVCB);



	if(PageFileManipulation){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("XiFS do not support PageFileManipulation .\n"));
		RC = STATUS_ACCESS_DENIED;
		xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);	
		return RC;		
	}

	// Added by ILGU HONG 04112006
	// Added by ILGU End
	

	// Added by ILGU HONG for readonly 09052006
	if(DeleteOnCloseSpecified){
		if(PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
			RC = STATUS_MEDIA_WRITE_PROTECTED;
			xixfs_CompleteRequest( PtrIrpContext, RC, ReturnedInformation );
			return RC;
		}
	}
	
	if ((RequestedDisposition == FILE_OVERWRITE) || 
			(RequestedDisposition == FILE_OVERWRITE_IF) || 
			(RequestedDisposition == FILE_SUPERSEDE)	)
	{
		if(PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
			RC = STATUS_MEDIA_WRITE_PROTECTED;
			xixfs_CompleteRequest( PtrIrpContext, RC, ReturnedInformation );
			return RC;
		}
	}
	// Added by ILGU HONG for readonly end
	
	




	if(PtrExtAttrBuffer){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("XiFS do not support EA for file .\n"));
		RC = STATUS_EAS_NOT_SUPPORTED;
		xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);
		return RC;	
	}


	if (PtrRelatedFileObject)
	{

		RelatedTypeOfOpen = xixfs_DecodeFileObject(PtrRelatedFileObject, &PtrRelatedFCB, &PtrRelatedCCB);
		
		if(RelatedTypeOfOpen < UserVolumeOpen){
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);
			return RC;
			
		}

		RelatedObjectName = PtrRelatedFileObject->FileName;
	}

	if (!NT_SUCCESS(RC = xixfs_CheckAndVerifyVolume(PtrVCB))) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("XiFS Volume is not accessible VCB %lx .\n", PtrVCB));
			RC = STATUS_INVALID_PARAMETER;
			xixfs_CompleteRequest(PtrIrpContext, RC, ReturnedInformation);
			return RC;
	}


	if (!(PtrIrpContext->IrpContextFlags & XIFSD_IRP_CONTEXT_WAIT)) {
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_IRPCONTEXT), 
			("Xifs Post request IrpContext %lx .\n", PtrIrpContext));
		RC = xixfs_PostRequest(PtrIrpContext, PtrIrp);
	    DeferredProcessing = TRUE;
		if(DirectoryOnlyRequested){
				DirectoryOnlyRequested = TRUE;
		}
		return RC;
	}


	AcquiredVCB = XifsdAcquireVcbExclusive(TRUE,PtrVCB,FALSE);
	DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE| DEBUG_TARGET_RESOURCE| DEBUG_TARGET_VCB), 
		("Acq exclusive VCB(%p) VCBResource (%p).\n", PtrVCB, &PtrVCB->VCBResource));	

	try {

		if (!NT_SUCCESS(RC = xixfs_CheckAndVerifyVolume(PtrVCB))) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("XiFS Volume is not accessible VCB %lx .\n", PtrVCB));
				try_return(RC);
		}

		// If a volume open is requested, satisfy it now
		if ((PtrNewFileObject->FileName.Length == 0) 
			&& (PtrRelatedFileObject == NULL || (RelatedTypeOfOpen <= UserVolumeOpen)) 
			&& (!OpenByFileId)) 
		{

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_VOLINFO), 
					("Open for volume.\n"));

			// Added by ILGU HONG 041120006
			if ((RequestedDisposition == FILE_OVERWRITE) || 
				(RequestedDisposition == FILE_OVERWRITE_IF) ||
				(RequestedDisposition == FILE_SUPERSEDE) )
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("XiFS is Not allowed to Vol file as overwrite// overwrite if  .\n"));

				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			// Added by ILGU HONG End

			if ((OpenTargetDirectory)) 
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("XiFS is Not allowed to Vol file as Directory |ExtAttr  .\n"));
				
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			if (DirectoryOnlyRequested) 
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XiFS is Not allowed to Vol file as Directory  .\n"));
				RC = STATUS_NOT_A_DIRECTORY;
				try_return(RC);
			}

			if(DeleteOnCloseSpecified)
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XiFS is Not allowed to Vol file as delete  .\n"));
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}




			if ((RequestedDisposition != FILE_OPEN) ) 
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("XiFS is allowed only for file  .\n"));
				RC = STATUS_ACCESS_DENIED;
				try_return(RC);
			}


			CurrentFCB = PtrVCB->VolumeDasdFCB;
			XifsdAcquireFcbExclusive(TRUE, CurrentFCB, FALSE);
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE| DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB), 
				("Acq exclusive VolumeDasdFCB(%p) FCBResource (%p).\n", CurrentFCB, &CurrentFCB->FCBResource));	

			RC = xixfs_OpenExistingFCB(PtrIrpContext,
										PtrIoStackLocation,
										&CurrentFCB,
										NULL,
										UserVolumeOpen,
										FALSE,
										IgnoreCaseWhenChecking,
										RequestedDisposition,
										NULL,
										NULL);


			ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
			try_return(RC);
		}

		//DbgPrint("2\n");
		if (OpenByFileId) 
		{
			int64 FileId;

			RtlCopyMemory( &FileId, PtrIoStackLocation->FileObject->FileName.Buffer, sizeof(uint64));

			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE, 
					("Open using file Id.\n"));




			if((RequestedDisposition != FILE_OPEN)
				&& (RequestedDisposition != FILE_OPEN_IF))
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail Open By FileId Invalid deposition %lx.\n", RequestedDisposition));
				RC = STATUS_ACCESS_DENIED;
				try_return(RC);
			}




			RC = xixfs_OpenByFileId(PtrIrpContext,
								PtrIoStackLocation,
								PtrVCB,
								DeleteOnCloseSpecified,
								IgnoreCaseWhenChecking,
								DirectoryOnlyRequested,
								FileOnlyRequested,
								RequestedDisposition,
								&CurrentFCB);

			ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
			try_return(RC);
		}

		/*
		 *	Set Absolute Path
		 */
		if ((PtrRelatedFileObject != NULL)		
			&& (!OpenByFileId)) 
		{


			if(DirectoryOnlyRequested){
				DirectoryOnlyRequested = TRUE;
			}


			if(TargetObjectName.Length == 0){
				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}


			/*
				Check Name Rules
			*/
			if (PtrRelatedFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Related file is Not Directory Type(%ld):Id(0x%I64d).\n",
					PtrRelatedFCB->XixcoreFcb.FCBType,
					PtrRelatedFCB->XixcoreFcb.LotNumber
					));

					

				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			if ((RelatedObjectName.Length == 0) || (RelatedObjectName.Buffer[0] != L'\\')) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Related file is Not specified.\n"));
				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}

			if ((TargetObjectName.Length != 0) && (TargetObjectName.Buffer[0] == L'\\')) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Related file is Not specified.\n"));
				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}


			if ((TargetObjectName.Length != 0) && (TargetObjectName.Buffer[0] == L':')) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Stream Not supported.\n"));
				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}			


			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
					("RelatedObjectName(%wZ).\n", &RelatedObjectName));


			

			if(PtrRelatedFCB == PtrVCB->RootDirFCB){
				AbsolutePathName.MaximumLength = TargetObjectName.Length + RelatedObjectName.Length;
			}else{
				if(TargetObjectName.Length == 0){
					AbsolutePathName.MaximumLength = TargetObjectName.Length + RelatedObjectName.Length;
				}else {
					AbsolutePathName.MaximumLength = TargetObjectName.Length + RelatedObjectName.Length + sizeof(WCHAR);
				}
			}
				
			if (!(NameBuffer = ExAllocatePool(NonPagedPool, AbsolutePathName.MaximumLength))) {
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Insufficient memory.\n"));
				RC = STATUS_INSUFFICIENT_RESOURCES;
				PtrIrpContext->SavedExceptionCode = RC;
				XIXCORE_SET_FLAGS(PtrIrpContext->IrpContextFlags,XIFSD_IRP_CONTEXT_EXCEPTION );
				try_return(RC);
			}

			AbsolutePathName.Buffer = (PWSTR)NameBuffer;

			RtlZeroMemory(AbsolutePathName.Buffer, AbsolutePathName.MaximumLength);

			RtlCopyMemory((void *)(AbsolutePathName.Buffer), (void *)(RelatedObjectName.Buffer), RelatedObjectName.Length);
			AbsolutePathName.Length = RelatedObjectName.Length;



			if(PtrRelatedFCB != PtrVCB->RootDirFCB){
				if(TargetObjectName.Length != 0){
					xixfs_RemoveLastSlashOfName(&AbsolutePathName);
					RtlAppendUnicodeToString(&AbsolutePathName, L"\\");
				}
			}

			if(TargetObjectName.Length != 0){

				// slash -->get rid of one
				if(TargetObjectName.Buffer[0] == L'\\'){
					TargetObjectName.Buffer = &TargetObjectName.Buffer[1];
					TargetObjectName.Length -= sizeof(WCHAR);
				}

				/*
					Parsing Pointer setting
				*/
				RemainTargetName.Length = TargetObjectName.Length;
				RemainTargetName.MaximumLength = TargetObjectName.MaximumLength;
				RemainTargetName.Buffer = Add2Ptr((uint8 *)AbsolutePathName.Buffer, AbsolutePathName.Length);
				




				RtlAppendUnicodeToString(&AbsolutePathName, TargetObjectName.Buffer);
				xixfs_RemoveLastSlashOfName(&RemainTargetName);
				
				DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
					("AbsoluteName (%wZ) : TargetObjectName (%wZ) : RemainTargetName(%wZ).\n",
					&AbsolutePathName, &TargetObjectName, &RemainTargetName
					));
			}else{
				RemainTargetName.Length = 0;
			}
				
			// Remove Slash from Absolute path
			xixfs_RemoveLastSlashOfName(&AbsolutePathName);

			CurrentFCB = PtrRelatedFCB;


		} else {

			if(DirectoryOnlyRequested){
				DirectoryOnlyRequested = TRUE;
			}

			if(TargetObjectName.Length == 0){
				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}


			if (TargetObjectName.Buffer[0] != L'\\') {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Target is NULL.\n"));
				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}


			if ((TargetObjectName.Length > 2) && (TargetObjectName.Buffer[1] == L':')) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Target is Stream : Stream is not supported.\n"));
				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_INVALID_PARAMETER;
				try_return(RC);
			}			

			// two slash -->get rid of one
			if(TargetObjectName.Buffer[1] == L'\\'){
				TargetObjectName.Buffer = &TargetObjectName.Buffer[1];
				TargetObjectName.Length -= sizeof(WCHAR);
			}


			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE, 
					("TargetObjectName(%wZ).\n", &TargetObjectName));
				
			AbsolutePathName.MaximumLength = TargetObjectName.Length;
			if (!(NameBuffer = ExAllocatePool(NonPagedPool, AbsolutePathName.MaximumLength))) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Insufficient memory.\n"));
				
				RC = STATUS_INSUFFICIENT_RESOURCES;
				PtrIrpContext->SavedExceptionCode = RC;
				ReturnedInformation = 0;
				XIXCORE_SET_FLAGS(PtrIrpContext->IrpContextFlags,XIFSD_IRP_CONTEXT_EXCEPTION);
				try_return(RC);
			}




			AbsolutePathName.Buffer = (PWSTR)NameBuffer;
			RtlZeroMemory(AbsolutePathName.Buffer, AbsolutePathName.MaximumLength);

			RtlCopyMemory((void *)(AbsolutePathName.Buffer), (void *)(TargetObjectName.Buffer), TargetObjectName.Length);
			AbsolutePathName.Length = TargetObjectName.Length;

			// Remove Slash from Absolute path
			xixfs_RemoveLastSlashOfName(&AbsolutePathName);


			/*
				Pasing Pointer setting
			*/
			RemainTargetName.Length = AbsolutePathName.Length;
			RemainTargetName.MaximumLength = AbsolutePathName.MaximumLength;
			RemainTargetName.Buffer = AbsolutePathName.Buffer ;	

			CurrentFCB = PtrVCB->RootDirFCB;
		
		}


		
		XifsdAcquireFcbExclusive(TRUE, CurrentFCB, FALSE);
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_CREATE| DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB), 
				("Acq exclusive CurrentFCB(%p) (%I64d)  FCBResource (%p).\n", 
					CurrentFCB,
					CurrentFCB->XixcoreFcb.LotNumber,
					&CurrentFCB->FCBResource));	


		if (AbsolutePathName.Length == 2) {
			// this is an open of the root directory, ensure that	the caller has not requested a file only
			if (FileOnlyRequested || (RequestedDisposition == FILE_SUPERSEDE) || (RequestedDisposition == FILE_OVERWRITE) ||
				 (RequestedDisposition == FILE_OVERWRITE_IF)) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Invalid Operation for root directory .\n"));
				RC = STATUS_FILE_IS_A_DIRECTORY;
				try_return(RC);
			}


			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE, 
					("Open XifsdOpenRootDirectory .\n"));

			if(OpenTargetDirectory){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Invalid Parameter OpenTargetDirectory for root directory .\n"));

				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER ;
				try_return(RC);
			}


			if(DeleteOnCloseSpecified){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Invalid Parameter Delete On close for  root directory .\n"));

				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER ;
				try_return(RC);
			}

			// ADDED by ILGU 20060618
			if(FileOnlyRequested){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Invalid Parameter FileOnlyRequested is  Set for Directory .\n"));

				ReturnedInformation = 0;
				RC = STATUS_INVALID_PARAMETER ;
				try_return(RC);
			}
			

			/*
			// ADDED by ILGU 20060618
			if(!DirectoryOnlyRequested){
				if(XIXCORE_TEST_FLAGS(DesiredAccess, FILE_LIST_DIRECTORY)){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Invalid Parameter Access Directory for Read like file .\n"));

					ReturnedInformation = 0;
					RC = STATUS_ACCESS_DENIED ;
					try_return(RC);
				}
				
			}
			*/

			// ADDED by ILGU 20070227
			if(RequestedDisposition == FILE_CREATE)
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Req Create but Target Is already existed \n"));

				ReturnedInformation = FILE_EXISTS;
				// CHANGED BY ILGU HONG 20061113
				RC = STATUS_OBJECT_NAME_COLLISION;
				try_return(RC);
			}



			RC = xixfs_OpenExistingFCB(PtrIrpContext,
										PtrIoStackLocation,
										&CurrentFCB,
										NULL,
										UserDirectoryOpen,
										FALSE,
										IgnoreCaseWhenChecking,
										RequestedDisposition,
										NULL,
										&AbsolutePathName);

			ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
			try_return(RC);
		}else {

			USHORT LastFileNameOffset = 0;
				
			FullPathName.Length = FullPathName.MaximumLength = AbsolutePathName.Length;
			NameBuffer2 = ExAllocatePoolWithTag(NonPagedPool, FullPathName.Length, TAG_BUFFER);
			
			if(!NameBuffer2){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Insufficient memory.\n"));
					
				RC = STATUS_INSUFFICIENT_RESOURCES;
				ReturnedInformation = 0;
				try_return(RC);
			}

			FullPathName.Buffer = (PWSTR)NameBuffer2;

			RtlZeroMemory((void *)FullPathName.Buffer, FullPathName.Length);
			RtlCopyMemory((void *)FullPathName.Buffer, (void *)AbsolutePathName.Buffer, FullPathName.Length);


			LastFileNameOffset =	(uint16) xixfs_SearchLastComponetOffsetOfName(&(FullPathName));

			TargetFileName.MaximumLength = 
			TargetFileName.Length = FullPathName.Length  - LastFileNameOffset;
			TargetFileName.Buffer = (PWSTR) Add2Ptr( FullPathName.Buffer, LastFileNameOffset);   
				
			if(IgnoreCaseWhenChecking){
				RtlDowncaseUnicodeString(&AbsolutePathName, &AbsolutePathName, FALSE);
			}
		}


		if(RemainTargetName.Length != 0)
		{
			CurrentLCB = xixfs_FCBTLBFindPrefix(PtrIrpContext,
											&CurrentFCB,
											&RemainTargetName,
											IgnoreCaseWhenChecking);

			if(CurrentLCB){
				if ( XIXCORE_TEST_FLAGS(CurrentLCB->LCBFlags, 
					(XIFSD_LCB_STATE_LINK_IS_GONE | XIFSD_LCB_STATE_DELETE_ON_CLOSE)) )
				{
					//DbgPrint(" !!!Delete Pending Entry From table (%wZ)  .\n", &CurrentLCB->FileName);

					ReturnedInformation = 0;
					RC = STATUS_DELETE_PENDING;
					try_return(RC);
				}


				
			}

		}

		

		if(RemainTargetName.Length == 0)
		{
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_CREATE, 
					("Founded FileName(%wZ).\n", &PtrNewFileObject->FileName));

			if(OpenTargetDirectory){


				if(XIXCORE_TEST_FLAGS(RequestedOptions,  FILE_OPEN_REPARSE_POINT )){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("[OpenTargetDirectory] dose not support FILE_OPEN_REPARSE_POINT Name.\n"));
					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}


				/*
				PtrNewFileObject->FileName.Length = CurrentLCB->FileName.Length;
				RtlCopyMemory(PtrNewFileObject->FileName.Buffer, 
								CurrentLCB->FileName.Buffer, 
								CurrentLCB->FileName.Length
								);

				*/
				PtrNewFileObject->FileName.Length = FullPathName.Length;
				RtlCopyMemory(PtrNewFileObject->FileName.Buffer, 
								FullPathName.Buffer, 
								FullPathName.Length
								);

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
						(" [OpenTargetDirectory] Set file Name for reqested file Name(%wZ).\n", &PtrNewFileObject->FileName));

				if(CurrentLCB->ParentFcb){

					RC = xixfs_OpenExistingFCB( PtrIrpContext,
											PtrIoStackLocation,
											&(CurrentLCB->ParentFcb),
											NULL,
											UserDirectoryOpen,
											DeleteOnCloseSpecified,
											IgnoreCaseWhenChecking,
											RequestedDisposition,
											PtrRelatedCCB,
											NULL);
					
					ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
					try_return(RC);

				}else{
					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}
			}


			if (CurrentFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) 
			{
				
				if (XIXCORE_TEST_FLAGS( PtrIoStackLocation->Parameters.Create.Options, FILE_DIRECTORY_FILE )) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Is Not File is a Directory\n"));
					ReturnedInformation = FILE_EXISTS;
					RC = STATUS_NOT_A_DIRECTORY;
					try_return(RC);
				}
		
				

				// ADDED by ILGU 20060618
				if(DirectoryOnlyRequested){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Invalid Parameter DirectoryOnlyRequested is  Set for File .\n"));

					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}

				// ADDED by ILGU 20070227
				if(RequestedDisposition == FILE_CREATE)
				{
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
									("Req Create but Target Is already existed \n"));

					ReturnedInformation = FILE_EXISTS;
					// CHANGED BY ILGU HONG 20061113
					RC = STATUS_OBJECT_NAME_COLLISION;
					try_return(RC);
				}




				//
				//  The only create disposition we allow is OPEN.
				//

				RC = xixfs_OpenExistingFCB( PtrIrpContext,
										PtrIoStackLocation,
										&CurrentFCB,
										CurrentLCB,
										UserFileOpen,
										DeleteOnCloseSpecified,
										IgnoreCaseWhenChecking,
										RequestedDisposition,
										PtrRelatedCCB,
										&FullPathName);

				ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
				try_return(RC);
				//
				//  This is a directory.  Verify the user didn't want to open
				//  as a file.
				//

			} else if (XIXCORE_TEST_FLAGS( PtrIoStackLocation->Parameters.Create.Options, FILE_NON_DIRECTORY_FILE )) {

				RC = STATUS_FILE_IS_A_DIRECTORY;
				try_return(RC);

			//
			//  Open the file as a directory.
			//

			} else {


				// ADDED by ILGU 20060618
				if(FileOnlyRequested){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Invalid Parameter FileOnlyRequested is  Set for Directory .\n"));

					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}

				if(RequestedDisposition == FILE_CREATE)
				{
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
									("Req Create but Target Is already existed \n"));

					ReturnedInformation = FILE_EXISTS;
					// CHANGED BY ILGU HONG 20061113
					RC = STATUS_OBJECT_NAME_COLLISION;
					try_return(RC);
				}

				/*
				// ADDED by ILGU 20060618
				if(!DirectoryOnlyRequested){
					if(XIXCORE_TEST_FLAGS(DesiredAccess, FILE_LIST_DIRECTORY)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Invalid Parameter Access Directory for Read like file .\n"));

						ReturnedInformation = 0;
						RC = STATUS_ACCESS_DENIED ;
						try_return(RC);
					}
					
				}
				*/

				//
				//  The only create disposition we allow is OPEN.
				//

				RC = xixfs_OpenExistingFCB( PtrIrpContext,
										PtrIoStackLocation,
										&CurrentFCB,
										CurrentLCB,
										UserDirectoryOpen,
										DeleteOnCloseSpecified,
										IgnoreCaseWhenChecking,
										RequestedDisposition,
										PtrRelatedCCB,
										&FullPathName);



				ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
				try_return(RC);


			}

		}





		//
		//  Our starting Fcb better be a directory.
		//

		if (!XIXCORE_TEST_FLAGS( CurrentFCB->XixcoreFcb.FileAttribute, FILE_ATTRIBUTE_DIRECTORY )) {

			RC = STATUS_OBJECT_PATH_NOT_FOUND;
			try_return(RC);
		}

		RtlZeroMemory(&DirContext, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
		RC =xixcore_InitializeDirContext( &PtrVCB->XixcoreVcb, &DirContext);


		if(!NT_SUCCESS(RC)){
			try_return(RC);
		}

		CleanUpDirContext =TRUE;

		ReturnedInformation = FILE_DOES_NOT_EXIST;


		while(TRUE)
		{
			
			uint64 EntryIndex;

			//
			//  Split off the next component from the name.
			//

			FsRtlDissectName(RemainTargetName,
							&FinalName, 
							&RemainTargetName);
			
			//
			//  Go ahead and look this entry up in the directory.
			//

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB),
				("RemainN(%wZ): SearchName(%wZ):CurrentFCB LotNumber(%I64d)\n",
					&RemainTargetName,&FinalName,CurrentFCB->XixcoreFcb.LotNumber));

			FoundStatus = xixcore_FindDirEntry (&PtrVCB->XixcoreVcb,
									&CurrentFCB->XixcoreFcb,
									(xc_uint8 *)FinalName.Buffer,
									FinalName.Length,
									&DirContext,
									&EntryIndex,
									IgnoreCaseWhenChecking
									);

			if(!NT_SUCCESS(FoundStatus)){


				if( (RemainTargetName.Length == 0)
					&& (OpenTargetDirectory))
				{
					
					if(XIXCORE_TEST_FLAGS(RequestedOptions,  FILE_OPEN_REPARSE_POINT )){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("[OpenTargetDirectory] dose not support FILE_OPEN_REPARSE_POINT\n"));

							ReturnedInformation = 0;
							RC = STATUS_INVALID_PARAMETER ;
							try_return(RC);
					}
					/*
					PtrNewFileObject->FileName.Length = FinalName.Length;
					RtlCopyMemory(PtrNewFileObject->FileName.Buffer, FinalName.Buffer, FinalName.Length);
					*/
					PtrNewFileObject->FileName.Length = FullPathName.Length;
					RtlCopyMemory(PtrNewFileObject->FileName.Buffer, 
								FullPathName.Buffer, 
								FullPathName.Length
								);
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB),
						(" [OpenTargetDirectory] Set file Name for reqested file Name(%wZ).\n", &PtrNewFileObject->FileName));


					RC = xixfs_OpenExistingFCB( PtrIrpContext,
											PtrIoStackLocation,
											&CurrentFCB,
											NULL,
											UserDirectoryOpen,
											DeleteOnCloseSpecified,
											IgnoreCaseWhenChecking,
											RequestedDisposition,
											PtrRelatedCCB,
											NULL);

					ReturnedInformation = FILE_DOES_NOT_EXIST;
					try_return(RC);	
				
				
				}

				if ((RequestedDisposition == FILE_OPEN) ||
					(RequestedDisposition == FILE_OVERWRITE) ||
					(RequestedDisposition == FILE_SUPERSEDE)) 
				{
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Not supported deposition (0x%x)\n", RequestedDisposition));
					ReturnedInformation = FILE_DOES_NOT_EXIST;
					RC = STATUS_OBJECT_PATH_NOT_FOUND;
					try_return(RC);
				}

				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE|DEBUG_TARGET_FCB),
					("RemainTarget Length = (%ld).\n", RemainTargetName.Length));			

 				if (RemainTargetName.Length == 0) {
					ReturnedInformation = FILE_DOES_NOT_EXIST;
					RC =  STATUS_OBJECT_PATH_NOT_FOUND;
					break;
				}
				//
				//  Any other operation return STATUS_ACCESS_DENIED.
				//
				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_OBJECT_PATH_NOT_FOUND;
				try_return(RC);
			}


			if (RemainTargetName.Length == 0) 
			{

				if(OpenTargetDirectory)
				{
					
					if(XIXCORE_TEST_FLAGS(RequestedOptions,  FILE_OPEN_REPARSE_POINT )){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("[OpenTargetDirectory] dose not support FILE_OPEN_REPARSE_POINT\n"));

							ReturnedInformation = 0;
							RC = STATUS_INVALID_PARAMETER ;
							try_return(RC);
					}
					/*
					PtrNewFileObject->FileName.Length = FinalName.Length;
					RtlCopyMemory(PtrNewFileObject->FileName.Buffer, FinalName.Buffer, FinalName.Length);
					*/
					PtrNewFileObject->FileName.Length = FullPathName.Length;
					RtlCopyMemory(PtrNewFileObject->FileName.Buffer, 
								FullPathName.Buffer, 
								FullPathName.Length
								);
					DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
						(" [OpenTargetDirectory] Set file Name for reqested file Name(%wZ).\n", &PtrNewFileObject->FileName));
				
					
					RC = xixfs_OpenExistingFCB( PtrIrpContext,
											PtrIoStackLocation,
											&CurrentFCB,
											NULL,
											UserDirectoryOpen,
											DeleteOnCloseSpecified,
											IgnoreCaseWhenChecking,
											RequestedDisposition,
											PtrRelatedCCB,
											NULL);
					
					ReturnedInformation = FILE_EXISTS;
					try_return(RC);	
				}

				ReturnedInformation = FILE_OPENED;
				RC = STATUS_SUCCESS;
				break;
			}


			if (DirContext.Type != XIFS_FD_TYPE_DIRECTORY) 
			{
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Target Path is not Directory \n"));

					RC = STATUS_OBJECT_PATH_NOT_FOUND;
					try_return(RC);
			}

			RC = xixfs_OpenObjectFromDirContext( PtrIrpContext,
											PtrIoStackLocation,
											PtrVCB,
											&CurrentFCB,
											&DirContext,
											FALSE,
											DeleteOnCloseSpecified,
											IgnoreCaseWhenChecking,
											RequestedDisposition,
											NULL,
											NULL);



			ASSERT(NT_SUCCESS(RC));

		}


		if(RC == STATUS_SUCCESS){

			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
					("Founded Target Entry FileName(%wZ).\n", &FinalName));


			if((DirContext.VirtualDirIndex > 2) 
				&& !(DirContext.Type == XIFS_FD_TYPE_DIRECTORY)
				&&XIXCORE_TEST_FLAGS(PtrIoStackLocation->Parameters.Create.Options, FILE_DIRECTORY_FILE))
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Req Directory but Target Is File\n"));

				ReturnedInformation = FILE_EXISTS;
				RC = STATUS_NOT_A_DIRECTORY;
				try_return(RC);
			}


			if( ((DirContext.VirtualDirIndex < 2) 
				|| (DirContext.Type == XIFS_FD_TYPE_DIRECTORY))
				&& XIXCORE_TEST_FLAGS(PtrIoStackLocation->Parameters.Create.Options, FILE_NON_DIRECTORY_FILE))
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Req File but Target Is Directory\n"));
				
				ReturnedInformation = FILE_EXISTS;
				RC = STATUS_FILE_IS_A_DIRECTORY;
				try_return(RC);
			}		


			if(RequestedDisposition == FILE_CREATE)
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Req Create but Target Is already existed \n"));

				ReturnedInformation = FILE_EXISTS;
				// CHANGED BY ILGU HONG 20061113
				RC = STATUS_OBJECT_NAME_COLLISION;
				try_return(RC);
			}



			if(DirContext.Type == XIFS_FD_TYPE_FILE){
				// ADDED by ILGU 20060618
				if(DirectoryOnlyRequested){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Invalid Parameter DirectoryOnlyRequested is  Set for File .\n"));

					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}				
			}


			if(DirContext.Type == XIFS_FD_TYPE_DIRECTORY){
				// ADDED by ILGU 20060618
				if(FileOnlyRequested){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Invalid Parameter FileOnlyRequested is  Set for Directory .\n"));

					ReturnedInformation = 0;
					RC = STATUS_INVALID_PARAMETER ;
					try_return(RC);
				}

				/*
				// ADDED by ILGU 20060618
				if(!DirectoryOnlyRequested){
					if(XIXCORE_TEST_FLAGS(DesiredAccess, FILE_LIST_DIRECTORY)){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Invalid Parameter Access Directory for Read like file .\n"));

						ReturnedInformation = 0;
						RC = STATUS_ACCESS_DENIED ;
						try_return(RC);
					}
					
				}
				*/
				


			}


			RC = xixfs_OpenObjectFromDirContext( PtrIrpContext,
												PtrIoStackLocation,
												PtrVCB,
												&CurrentFCB,
												&DirContext,
												TRUE,
												DeleteOnCloseSpecified,
												IgnoreCaseWhenChecking,
												RequestedDisposition,
												PtrRelatedCCB,
												&FullPathName);





			
			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Target open fail\n"));

				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_OBJECT_NAME_NOT_FOUND;
				try_return(RC);
			}

			ReturnedInformation = (uint32)PtrIrp->IoStatus.Information;
			
		}else{


			uint64 NewFileId = 0;
			PXIXFS_FCB pPFCB = NULL;
			pPFCB = CurrentFCB;
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
					("Create Name(%wZ).\n", &FinalName));


			// Added by ILGU HONG for readonly
			if(PtrVCB->XixcoreVcb.IsVolumeWriteProtected){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("device is write protected\n"));

				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_MEDIA_WRITE_PROTECTED;
				try_return(RC);
			}
			// Added by ILGU HONG end


			RC = xixcore_CreateNewFile (
					&PtrVCB->XixcoreVcb,
					&CurrentFCB->XixcoreFcb,
					((FileOnlyRequested == TRUE)?1:0),
					(xc_uint8*)TargetFileName.Buffer,
					TargetFileName.Length,
					FileAttributes,
					&DirContext,
					&NewFileId 
					);


			if(!NT_SUCCESS(RC)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
								("Target create fail\n"));

				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_OBJECT_NAME_NOT_FOUND;
				try_return(RC);
			}

			XifsdLockFcb(NULL, CurrentFCB);
			CurrentFCB->AllocationSize.QuadPart = CurrentFCB->XixcoreFcb.RealAllocationSize;
			XifsdUnlockFcb(NULL, CurrentFCB);


			RC = xixfs_OpenNewFileObject(PtrIrpContext,
								PtrIoStackLocation,
								PtrVCB,
								&CurrentFCB,
								NewFileId,
								((FileOnlyRequested)?XIFS_FD_TYPE_FILE:XIFS_FD_TYPE_DIRECTORY),
								DeleteOnCloseSpecified,
								IgnoreCaseWhenChecking,
								&TargetFileName,
								&FullPathName
								);

			if(!NT_SUCCESS(RC)){

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Target open Failed.\n"));
				ReturnedInformation = FILE_DOES_NOT_EXIST;
				RC = STATUS_OBJECT_NAME_NOT_FOUND;
				try_return(RC);
			}



			ReturnedInformation = FILE_CREATED;

		}
		
		xixfs_NotifyReportChangeToXixfs(
					CurrentFCB,
					((CurrentFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)
							?FILE_NOTIFY_CHANGE_FILE_NAME
							:FILE_NOTIFY_CHANGE_DIR_NAME),
					FILE_ACTION_ADDED
					);




		xixcore_CleanupDirContext(&DirContext);
		CleanUpDirContext = FALSE;
		

		;
	} finally {
		

		//
		//  The result of this open could be success, pending or some error
		//  condition.
		//

		if (AbnormalTermination()) {


			//
			//  In the error path we start by calling our teardown routine if we
			//  have a CurrentFcb.
			//

			if (CurrentFCB != NULL) {

				BOOLEAN RemovedFcb;


				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  ("Abnormal termination!!.\n"));
				xixfs_TeardownStructures(TRUE, CurrentFCB, FALSE, &RemovedFcb );

				if (RemovedFcb) {

					CurrentFCB = NULL;
				}
			}

			//
			//  No need to complete the request.
			//

			PtrIrpContext = NULL;
			PtrIrp = NULL;


			if(NameBuffer) ExFreePool(NameBuffer);
		//
		//  If we posted this request through the oplock package we need
		//  to show that there is no reason to complete the request.
		//

		} else {

			if (RC == STATUS_PENDING) {

				PtrIrpContext = NULL;
				PtrIrp = NULL;
			}
			
			if (CleanUpDirContext) {

				xixcore_CleanupDirContext(&DirContext);
			}
			

			if (CurrentFCB != NULL) {
				
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB), 
					("CurrentFCB (%p).\n", PtrVCB));	
				XifsdReleaseFcb( PtrIrpContext, CurrentFCB );
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_FCB),  ("Releae CurrentFCB.\n"));
			}


			//
			//  Release the Vcb.
			//
			

			if(AcquiredVCB){
				XifsdReleaseVcb( TRUE, PtrVCB );
				DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_CREATE| DEBUG_TARGET_VCB),  ("Releae Current VCB.\n"));
			}
			

			if(NameBuffer) ExFreePool(NameBuffer);

			if(NameBuffer2) ExFreePool(NameBuffer2);
		}
		
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL, ("Exit Create Status(0x%x) Information(0x%x) .\n", RC, ReturnedInformation));
		xixfs_CompleteRequest( PtrIrpContext, RC, ReturnedInformation );

	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, ("Exit xixfs_CommonCreate .\n"));

	return(RC);
}
