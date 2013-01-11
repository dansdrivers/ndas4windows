/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"



//
//	Vendor command: SetLock
//

BOOL
VendorSetLock11(
	IN PRAM_DATA							RamData,
	IN UINT64								SessionId,
	IN PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
){
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	LONG					acquired;
	DWORD					dwWaitResult;


	vendorParam = NTOHLL(RequestHeader->VendorParameter);
	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

//	fprintf(stderr, "VendorSetLock11: Acquiring lock %d.\n", lockNo);

	//
	//	Acquire the mutex for the lock access
	//

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	if(dwWaitResult != WAIT_OBJECT_0)
		return FALSE;

	if(gpLock->Acquired == FALSE) {
		//
		//	GP lock acquired
		//
		gpLock->Acquired = TRUE;
		gpLock->Counter ++;
		gpLock->SessionId = SessionId;

		//
		// Set return counter
		//
		
		ReplyHeader->VendorParameter = HTONLL((UINT64)gpLock->Counter);
		
//		fprintf(stderr, "VendorSetLock11: lock %d acquired.\n", lockNo);

	//
	//	GP lock Already acquired by others
	//

	} else {
		//
		// Set return counter
		//

		ReplyHeader->VendorParameter = HTONLL((UINT64)gpLock->Counter);

		ReleaseMutex(RamData->LockMutex);

		fprintf(stderr, "VendorSetLock11: Lock contention!\n");
		ReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		return TRUE;
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}

//
//	Vendor command: FreeLock
//

BOOL
VendorFreeLock11(
	IN PRAM_DATA							RamData,
	IN UINT64								SessionId,
	IN PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
){
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	LONG					acquired;
	DWORD					dwWaitResult;

	vendorParam = NTOHLL(RequestHeader->VendorParameter);
	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];
//	fprintf(stderr, "VendorSetLock11: freeing lock %d.\n", lockNo);


	//
	//	Acquire the mutex for the lock access
	//

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	if(dwWaitResult != WAIT_OBJECT_0)
		return FALSE;

	//
	// Set return counter
	//

	ReplyHeader->VendorParameter = HTONLL((UINT64)gpLock->Counter);
	
	if(SessionId == gpLock->SessionId) {

		if(gpLock->Acquired == FALSE) {
			fprintf(stderr, "VendorFreeLock11: Already freed\n");
		} else {
			gpLock->Acquired = FALSE;
//			fprintf(stderr, "VendorSetLock11: lock %d freed.\n", lockNo);
		}
	} else {
		//
		//	Not owner
		//
		ReleaseMutex(RamData->LockMutex);

		fprintf(stderr, "VendorFreeLock11: Not lock owner!\n");
		ReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		return TRUE;
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}

BOOL
VendorGetLock11(
	IN PRAM_DATA							RamData,
	IN PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
){
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;


	vendorParam = NTOHLL(RequestHeader->VendorParameter);
	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

	//
	//	Acquire the mutex for the lock access
	//

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	if(dwWaitResult != WAIT_OBJECT_0)
		return FALSE;

	//
	// Set return counter
	//

	ReplyHeader->VendorParameter = HTONLL((UINT64)gpLock->Counter);

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}

BOOL
VendorGetLockOwner11(
	IN PRAM_DATA							RamData,
	IN UINT64								SessionId,
	IN PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
){
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;
	


	vendorParam = NTOHLL(RequestHeader->VendorParameter);
	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

	//
	//	Acquire the mutex for the lock access
	//

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	if(dwWaitResult != WAIT_OBJECT_0)
		return FALSE;

	//
	// Set return host mac address if the lock is acquired.
	//

	if(gpLock->Acquired) {
		PUCHAR					byteParam;

		byteParam = (PUCHAR)&ReplyHeader->VendorParameter;
		GetHostMacAddressOfSession(SessionId, byteParam + 2);
	} else {
		memset(	&ReplyHeader->VendorParameter,
				0,
				sizeof(ReplyHeader->VendorParameter));
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}

BOOL
CleanupLock11(
	IN PRAM_DATA	RamData,
	IN UINT64		SessionId
){
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;

	//
	//	Acquire the lock mutex
	//
	fprintf(stderr, "CleanupLock11: cleaning up aquired locks\n");

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	if(dwWaitResult != WAIT_OBJECT_0)
		return FALSE;

	for(lockNo = 0; lockNo < 4; lockNo ++) {
		gpLock = &RamData->GPLocks[lockNo];

		if(SessionId == gpLock->SessionId) {

			if(gpLock->Acquired == TRUE) {
				fprintf(stderr, "CleanupLock11: Cleaned up lock %d acquired by session %I64u\n",
					lockNo, SessionId);
				gpLock->Acquired = FALSE;
			}
		}
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}
