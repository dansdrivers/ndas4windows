/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2008, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 12/11/2008
 -------------------------------------------------------------------------
*/

#include "stdafx.h"

#include "..\inc\socketLpx.h"
#include "..\inc\lsprotospec.h"
#include "..\inc\binparams.h"

#include "ndasemu30.h"
#include "ndasemu20.h"

#include "ndaslock.h"

LONG DbgLevelEmuLock = DBG_LEVEL_EMU_LOCK;

#define NdasEmuDbgCall(l,x,...) do {							\
    if (l <= DbgLevelEmuLock) {									\
		fprintf(stderr,"|%d|%s|%d|",l,__FUNCTION__, __LINE__);	\
		fprintf(stderr,x,__VA_ARGS__);							\
    } 															\
} while(0)


//	Vendor command: SetLock

BOOL
VendorSetLock11 (
	IN  PRAM_DATA_OLD						RamData,
	IN  UINT64								SessionId,
	IN  PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
	)
{
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;

	vendorParam = NTOHLL( RequestHeader->VendorParameter );

	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

	if (lockNo != (RequestHeader->Parameter8[3] & 0x3)) {

		NdasEmuDbgCall( 1,"decoding fail\n" );
	}

	NdasEmuDbgCall( 4, "lockNo = %d\n", lockNo );

	lockNo = RequestHeader->Parameter8[3] & 0x3;

	//	Acquire the mutex for the lock access

	dwWaitResult = WaitForSingleObject( RamData->LockMutex, INFINITE );

	if(dwWaitResult != WAIT_OBJECT_0) {

		return FALSE;
	}

	ReplyHeader->VendorParameter1 = htonl(gpLock->Counter);

	if (gpLock->Acquired == FALSE) {

		//	GP lock acquired

		gpLock->Acquired = TRUE;
		gpLock->Counter ++;
		gpLock->SessionId = SessionId;

		// Set return counter
		
		NdasEmuDbgCall( 4, "VendorSetLock11: lock %d acquired(%ld), gpLock->Counter = %d\n", lockNo, gpLock->SessionId, gpLock->Counter );

	} else {

		//	GP lock Already acquired by others
		// Set return counter

		NdasEmuDbgCall( 4, "VendorSetLock11: lock %d contention!(%ld), gpLock->Counter = %d\n", lockNo, gpLock->SessionId, gpLock->Counter );

		ReleaseMutex(RamData->LockMutex);

		ReplyHeader->Response = LANSCSI_RESPONSE_T_SET_SEMA_FAIL;

		return TRUE;
	}

	ReleaseMutex( RamData->LockMutex );

	return TRUE;
}

//	Vendor command: FreeLock

BOOL
VendorFreeLock11 (
	IN  PRAM_DATA_OLD						RamData,
	IN  UINT64								SessionId,
	IN  PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
	)
{
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;

	vendorParam = NTOHLL(RequestHeader->VendorParameter);

	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

	if (lockNo != (RequestHeader->Parameter8[3] & 0x3)) {

		NdasEmuDbgCall( 1,"decoding fail\n" );
	}

	NdasEmuDbgCall( 4, "lockNo = %d\n", lockNo );

	lockNo = RequestHeader->Parameter8[3] & 0x3;

	//	Acquire the mutex for the lock access
	
	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);
	
	if (dwWaitResult != WAIT_OBJECT_0) {

		return FALSE;
	}

	// Set return counter

	ReplyHeader->VendorParameter = HTONLL((UINT64)gpLock->Counter);
	
	if (SessionId == gpLock->SessionId) {

		if (gpLock->Acquired == FALSE) {

			NdasEmuDbgCall( 4, "VendorFreeLock11: Already freed\n");
		
		} else {
		
			gpLock->Acquired = FALSE;

			NdasEmuDbgCall( 4, "VendorSetLock11: lock %d freed.\n", lockNo );
		}

	} else {
		
		//	Not owner
		
		ReleaseMutex( RamData->LockMutex );

		NdasEmuDbgCall( 1, "VendorFreeLock11: Not lock owner!(%ld, %ld)\n", gpLock->SessionId, SessionId );

		ReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
		
		return TRUE;
	}

	ReleaseMutex( RamData->LockMutex );

	return TRUE;
}

BOOL
VendorGetLock11 (
	IN  PRAM_DATA_OLD						RamData,
	IN  PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
	)
{
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;


	vendorParam = NTOHLL(RequestHeader->VendorParameter);

	lockNo = (UCHAR)((vendorParam>>32) & 0x3);

	if (lockNo != (RequestHeader->Parameter8[3] & 0x3)) {

		NdasEmuDbgCall( 1,"decoding fail\n" );
	}

	NdasEmuDbgCall( 1, "lockNo = %d\n", lockNo );

	lockNo = RequestHeader->Parameter8[3] & 0x3;

	gpLock = &RamData->GPLocks[lockNo];

	//	Acquire the mutex for the lock access

	dwWaitResult = WaitForSingleObject( RamData->LockMutex, INFINITE );

	if (dwWaitResult != WAIT_OBJECT_0) {

		return FALSE;
	}

	// Set return counter

	ReplyHeader->VendorParameter1 = htonl(gpLock->Counter);

	ReleaseMutex( RamData->LockMutex );

	return TRUE;
}

BOOL
VendorGetLockOwner11 (
	IN  PRAM_DATA_OLD						RamData,
	IN  UINT64								SessionId,
	IN  PLANSCSI_VENDOR_REQUEST_PDU_HEADER	RequestHeader,
	OUT PLANSCSI_VENDOR_REPLY_PDU_HEADER	ReplyHeader
	)
{
	UINT64					vendorParam;
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;
	


	vendorParam = NTOHLL(RequestHeader->VendorParameter);

	lockNo = (UCHAR)((vendorParam>>32) & 0x3);
	gpLock = &RamData->GPLocks[lockNo];

	NdasEmuDbgCall( 1, "lockNo = %d\n", lockNo );

	//	Acquire the mutex for the lock access

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);

	if (dwWaitResult != WAIT_OBJECT_0) {

		return FALSE;
	}

	// Set return host mac address if the lock is acquired.

	if (gpLock->Acquired) {

		PUCHAR	byteParam;

		byteParam = (PUCHAR)&ReplyHeader->VendorParameter;
		GetHostMacAddressOfSession( SessionId, byteParam + 2 );

	} else {

		memset(	&ReplyHeader->VendorParameter,
				0,
				sizeof(ReplyHeader->VendorParameter) );
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}

BOOL
CleanupLock11 (
	IN PRAM_DATA_OLD	RamData,
	IN UINT64			SessionId
	)
{
	UCHAR					lockNo;
	PGENERAL_PURPOSE_LOCK	gpLock;
	DWORD					dwWaitResult;

	//	Acquire the lock mutex

	NdasEmuDbgCall( 4, "CleanupLock11: cleaning up aquired locks\n" );

	dwWaitResult = WaitForSingleObject(RamData->LockMutex, INFINITE);

	if (dwWaitResult != WAIT_OBJECT_0) {

		return FALSE;
	}

	for (lockNo = 0; lockNo < 4; lockNo ++) {

		gpLock = &RamData->GPLocks[lockNo];

		if (SessionId == gpLock->SessionId) {

			if (gpLock->Acquired == TRUE) {
		
				NdasEmuDbgCall( 4, "CleanupLock11: Cleaned up lock %d acquired by session %I64u\n", lockNo, SessionId );
				gpLock->Acquired = FALSE;
			}
		}
	}

	ReleaseMutex(RamData->LockMutex);

	return TRUE;
}
