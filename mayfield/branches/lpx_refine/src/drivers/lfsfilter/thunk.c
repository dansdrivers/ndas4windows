/********************************************************************
	created:	2005/08/12
	created:	12:8:2005   18:36
	filename: 	thunk.c
	file path:	trunk\src\drivers\lfsfilter
	file base:	thunk
	file ext:	c
	author:		XIMETA, Inc.
	
	purpose:	Thunk primary and secondary node for ndfs protocol.
*********************************************************************/


#include "LfsProc.h"


#ifdef _WIN64


typedef struct _ID32RANGE {
	UINT32	FromId;
	UINT32	ToId;
} ID32RANGE, *PID32RANGE;


//////////////////////////////////////////////////////////////////////////
//
//	Splay tree support routines
//
//	NOTE: extent-based union and subtract routine are implemented.
//


//
//	return 0 If the first item embraces the second item.
//

LONG
XCTREE_API
IdNodeCompare(
	PXCTREE_CTX TreeCtx,
	PUCHAR  FirstStruct,
	PUCHAR  SecondStruct
){
	PID32RANGE	first = (PID32RANGE)FirstStruct;
	PID32RANGE	second = (PID32RANGE)SecondStruct;

	UNREFERENCED_PARAMETER(TreeCtx);

	if(	first->FromId <= second->FromId &&
		first->ToId >= second->ToId
		) {

		return 0;
	} else if(first->FromId < second->FromId) {

		//
		//	Do not allow to compare between items which have intersection.
		//

		ASSERT(first->ToId < second->FromId);

		return -1;
	} else {

		//
		//	Do not allow to compare between items which have intersection.
		//

		ASSERT(first->FromId > second->ToId);

		return 1;
	}
}


//
//	Return FALSE to insert the second item.
//
//	TODO: This function can not do union between two nodes and new item.
//

LONG
XCTREE_API
IdNodeUnion(
	PXCTREE_CTX TreeCtx,
	PUCHAR  FirstStruct,
	PUCHAR  SecondStruct
){
	PID32RANGE	first = (PID32RANGE)FirstStruct;
	PID32RANGE	second = (PID32RANGE)SecondStruct;

	UNREFERENCED_PARAMETER(TreeCtx);

	if(	first->ToId >= second->FromId &&
		first->ToId <= second->ToId) {

		//
		//		+-----------------+
		//		| First    +------|------------+
		//		|          |      |     Second |
		//		+----------+------+------------+
		//

		if(first->FromId > second->FromId) {
			first->FromId = second->FromId;
		}
		first->ToId = second->ToId;

		return TRUE;

	} else if(	first->FromId >= second->FromId &&
		first->FromId <= second->ToId) {

		//
		//		+-----------------+
		//		| Second   +------|------------+
		//		|          |      |     First  |
		//		+----------+------+------------+
		//
		
		first->FromId = second->FromId;
		if(first->ToId < second->ToId) {
			first->ToId = second->ToId;
		}

		return TRUE;
	} else if(first->ToId < (_U32)(-1) && first->ToId + 1 == second->FromId) {

		//
		//		+----------+
		//		| First    |------------+
		//		|          |    Second  |
		//		+----------+------------+
		//

		first->ToId = second->ToId;
		return TRUE;
	} else if(first->FromId > 0 && first->FromId -1 == second->ToId) {


		//
		//		+----------+
		//		| Second   |------------+
		//		|          |    First   |
		//		+----------+------------+
		//

		first->FromId = second->FromId;
		return TRUE;
	}

	//
	//	Return FALSE for caller to insert new node.
	//

	return FALSE;
}


//
//	Return TRUE to delete the first item.
//

LONG
XCTREE_API
IdNodeSubtract(
	PXCTREE_CTX TreeCtx,
	PUCHAR  FirstStruct,
	PUCHAR  SecondStruct,
	PLONG	Split
){
	PID32RANGE	first = (PID32RANGE)FirstStruct;
	PID32RANGE	second = (PID32RANGE)SecondStruct;

	UNREFERENCED_PARAMETER(TreeCtx);

	*Split = FALSE;

	if(	first->ToId >= second->FromId &&
		first->ToId <= second->ToId) {

		//
		//		+-----------------+
		//		| First    +------|------------+
		//		|          |      |     Second |
		//		+----------+------+------------+
		//

		if(first->FromId >= second->FromId) {

			//
			//	Return TRUE for caller to delete the item.
			//

			return TRUE;
		}
		ASSERT(second->FromId > 0);
		first->ToId = second->FromId - 1;

	} else if(	first->FromId >= second->FromId &&
		first->FromId <= second->ToId) {

		//
		//		+-----------------+
		//		| Second   +------|------------+
		//		|          |      |     First  |
		//		+----------+------+------------+
		//
		ASSERT(second->ToId != (_U32)(-1));
		first->FromId = second->ToId + 1;
		if(first->ToId <= second->ToId) {

			//
			//	Return TRUE for caller to delete the item.
			//

			return TRUE;
		}

	} else if(	first->FromId < second->FromId &&
		first->ToId > second->ToId) {
		_U32	firstToId;

		//
		// In this case, we will not delete the first item
		// by setting Split boolean.
		// we will split it into two nodes.
		//
		//		+--------------------+
		//		| First  +------+    |
		//		|        |Second|    |
		//		+--------+------+----+
		//

		//
		// Update the first item.
		//

		ASSERT(second->FromId);

		firstToId = first->ToId;
		first->ToId = second->FromId - 1;

		//
		//	Update the second item to be inserted.
		//

		ASSERT(second->ToId != (_U32)(-1));

		second->FromId = second->ToId + 1;
		second->ToId = firstToId;

		*Split = TRUE;

		return TRUE;
	}


	//
	//	Return FALSE for caller to do nothing.
	//

	return FALSE;
}


//
//
//

PVOID
XCTREE_API
IdNodeAllocate(
	PXCTREE_CTX TreeCtx,
	LONG_PTR	ByteSize
){
	UNREFERENCED_PARAMETER(TreeCtx);

	return ExAllocatePoolWithTag(NonPagedPool, ByteSize, IDNODECALLBACK_TAG);
}


//
//
//

VOID
XCTREE_API
IdNodeFree(
	PXCTREE_CTX TreeCtx,
	PVOID  Buffer
){
	UNREFERENCED_PARAMETER(TreeCtx);

	ExFreePoolWithTag(Buffer, IDNODECALLBACK_TAG);
}


//////////////////////////////////////////////////////////////////////////
//
//	Thunker32
//


//
//	Init Thunker32
//

NTSTATUS
Th32Init(
	PTHUNKER32	Thunker32
){
	NTSTATUS	status;
	LONG		iret;
	ID32RANGE	item;

	status = STATUS_SUCCESS;

	ExInitializeFastMutex(&Thunker32->FreeIdMutex);

	Thunker32->SplayTreeRoot = NULL;
	Thunker32->NextFreeId = 0;

	iret = XCSplayInit(&Thunker32->SplayTreeCtx, sizeof(ID32RANGE),
		IdNodeCompare, IdNodeUnion, IdNodeSubtract, IdNodeAllocate, IdNodeFree);
	if(iret != 0) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
		("XCSplayInit() failed.\n"));
		return STATUS_UNSUCCESSFUL;
	}

	//
	//	Insert initial ID range.
	//

	item.FromId = 0;
	item.ToId = 0xffffffff;

	Thunker32->SplayTreeRoot = 
		XCSplayInsert(	&Thunker32->SplayTreeCtx,
						(PUCHAR)&item,
						Thunker32->SplayTreeRoot);

	if(Thunker32->SplayTreeRoot == NULL) {
		return STATUS_UNSUCCESSFUL;
	}

	return status;
}


//
//	Destroy Thunker32
//

VOID
Th32Destroy(
	PTHUNKER32	Thunker32
){
	XCFreeTree(&Thunker32->SplayTreeCtx, Thunker32->SplayTreeRoot);
	XCSplayDestroy(&Thunker32->SplayTreeCtx);
	ASSERT(Thunker32->SplayTreeCtx.SizeOfTree == 0);
}


//
//	Th32RegisterPointer
//
//	Argument: 
//		Thunker32 -
//		ThunkPointer - 
//
//	Return: pointer ID
//
//
//	TODO: Splay tree may be too much fragmented.
//			E.g)	1. Register 1000 IDs.
//					2. Deregister 0, 2, 4, ... , 1000.
//					3. Deregister 1, 3, 5, ... , 999.
//					4. Finally, splay tree will have 0~1, 2~3, 4~5, ... , 999~1000.
//					Needs to union inter-node.
//

NTSTATUS
Th32RegisterPointer(
	IN PTHUNKER32	Thunker32,
	IN PVOID		ThunkPointer,
	OUT _U32		*PointerId
){
	ID32RANGE	item;
	ULONG		found;
	_U32		pointerId;

	UNREFERENCED_PARAMETER(ThunkPointer);

	ExAcquireFastMutex(&Thunker32->FreeIdMutex);

	item.FromId = item.ToId = Thunker32->NextFreeId;

	Thunker32->SplayTreeRoot = XCSplayDelete(
					&Thunker32->SplayTreeCtx,
					(PUCHAR)&item,
					Thunker32->SplayTreeRoot,
					&found);


	//
	//	Determine the pointer ID.
	//

	if(found == TRUE) {
		pointerId = Thunker32->NextFreeId;

		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
			("Th32RegisterPointer: Found ID:%u\n", pointerId));

		Thunker32->NextFreeId++;

	} else {


		//
		//	If the specified ID is not available,
		//	take available ID from the root node.
		//

		if(Thunker32->SplayTreeRoot) {
			PID32RANGE	availRange;

			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
				("Th32RegisterPointer: Could not find an ID. Try any available ID.\n"));

			availRange = (PID32RANGE)Thunker32->SplayTreeRoot->Item;

			Thunker32->NextFreeId = availRange->FromId;
			item.FromId = item.ToId = Thunker32->NextFreeId;

			Thunker32->SplayTreeRoot = XCSplayDelete(
							&Thunker32->SplayTreeCtx,
							(PUCHAR)&item,
							Thunker32->SplayTreeRoot,
							&found);

			if(found == TRUE) {
				pointerId = Thunker32->NextFreeId;

				SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
					("Th32RegisterPointer: Found ID:%u\n", pointerId));

				Thunker32->NextFreeId++;
			} else {
				ExReleaseFastMutex(&Thunker32->FreeIdMutex);

				SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
					("Th32RegisterPointer: No node avail at the 2nd try.\n"));
				return STATUS_INSUFFICIENT_RESOURCES;
			}

		} else {
			ExReleaseFastMutex(&Thunker32->FreeIdMutex);

			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
				("Th32RegisterPointer: No node avail.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}


	//
	//	Determine the next free ID.
	//

	ExReleaseFastMutex(&Thunker32->FreeIdMutex);


	//
	//	set return values
	//

	*PointerId = pointerId;

	return STATUS_SUCCESS;
}

VOID
Th32UnregisterPointer(
	PTHUNKER32	Thunker32,
	_U32		PointerId
){
	ID32RANGE	item;

	ExAcquireFastMutex(&Thunker32->FreeIdMutex);

	item.FromId = item.ToId = PointerId;

	Thunker32->SplayTreeRoot = XCSplayInsert(
					&Thunker32->SplayTreeCtx,
					(PUCHAR)&item,
					Thunker32->SplayTreeRoot);

	ASSERT(Thunker32->SplayTreeRoot != NULL);

	ExReleaseFastMutex(&Thunker32->FreeIdMutex);
}




#if DBG

VOID
SplayTest(){
	THUNKER32	thunker32;
	LONG		iret;
	ULONG		found;

	SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
			  ("Splay tree test...\n"));

	ExInitializeFastMutex(&thunker32.FreeIdMutex);

	thunker32.SplayTreeRoot = NULL;
	thunker32.NextFreeId = 0;

	iret = XCSplayInit(&thunker32.SplayTreeCtx, sizeof(ID32RANGE),
		IdNodeCompare, IdNodeUnion, IdNodeSubtract, IdNodeAllocate, IdNodeFree);
	if(iret != 0) {
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
			("TEST: XCSplayInit() failed.\n"));
		return;
	}

	do {
		_U32		i;
		ID32RANGE	item;


		//
		//	Insert 0~0xfffffff
		//

		item.FromId = 0;
		item.ToId = 0xffffffff;
		thunker32.SplayTreeRoot = 
			XCSplayInsert(	&thunker32.SplayTreeCtx,
							(PUCHAR)&item,
							thunker32.SplayTreeRoot);
		ASSERT(thunker32.SplayTreeRoot != NULL);
		SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
			("tree size after root inserted:%u\n", thunker32.SplayTreeCtx.SizeOfTree));

		//
		//	Insert
		//
		for(i = 0; i < 100; i += 2) {
			item.FromId = item.ToId = i;
			thunker32.SplayTreeRoot = 
				XCSplayInsert(	&thunker32.SplayTreeCtx,
				(PUCHAR)&item,
				thunker32.SplayTreeRoot);
			ASSERT(thunker32.SplayTreeRoot != NULL);

			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
				("tree size:%u\n", thunker32.SplayTreeCtx.SizeOfTree));
		}

		//
		// Delete
		//

		for(i = 0; i < 100; i += 2) {
			item.FromId = item.ToId = i;
			thunker32.SplayTreeRoot = 
				XCSplayDelete(	&thunker32.SplayTreeCtx,
				(PUCHAR)&item,
				thunker32.SplayTreeRoot,
				&found);

			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
				("tree size:%u found:%u\n", thunker32.SplayTreeCtx.SizeOfTree, found));
		}


		//
		//	Insert
		//

		for(i = 0; i < 100; i += 2) {
			item.FromId = item.ToId = i;
			thunker32.SplayTreeRoot = 
				XCSplayInsert(	&thunker32.SplayTreeCtx,
								(PUCHAR)&item,
								thunker32.SplayTreeRoot);
			ASSERT(thunker32.SplayTreeRoot != NULL);

			SPY_LOG_PRINT(LFS_DEBUG_LFS_ERROR,
				("tree size:%u\n", thunker32.SplayTreeCtx.SizeOfTree));
		}

		Th32Destroy(
			&thunker32
		);

	} while(FALSE);
}
#endif


//////////////////////////////////////////////////////////////////////////
//
//	ID database using splay tree
//



#endif