/*++

Copyright (C) 2002-2005 XIMETA, Inc.
All rights reserved.

--*/
#ifndef NDAS_HEAR_H_INCLUDED
#define NDAS_HEAR_H_INCLUDED
#pragma once

#define NDASHEAR_DLL_EXPORT
#ifdef NDASHEAR_DLL_EXPORT
#define NDASHEAR_LINKAGE
#elif defined NDASHEAR_STATIC
#define NDASHEAR_LINKAGE 
#else
#define NDASHEAR_LINKAGE __declspec(dllimport)
#endif

#define NDASHEARAPI __stdcall
#define NDASHEARAPI_CALLBACK __stdcall

#ifdef __cplusplus
extern "C" {
#endif

/* <TITLE NDAS_DEVICE_HEARTBEAT_INFO>
   
   Summary
   The NDAS_DEVICE_HEARTBEAT_INFO structure represents the heartbeat
   information of a NDAS device.

*/
typedef struct _NDAS_DEVICE_HEARTBEAT_INFO {
	/* System tick count when the heartbeat is arrived */
	DWORD	Timestamp;
	/* Type of the NDAS device. Always 0 */
	BYTE	Type;
	/* Version of the NDAS device. 0 for 1.0, 1 for 1.1, 2 for 2.0 */
	BYTE	Version;
	/* Reserved */
	BYTE    Reserved[2];
	/* LPX address of the NDAS device which sent a heartbeat packet */
	struct
	{
		BYTE Node[6];
		BYTE Reserved[2];
	} DeviceAddress;
	/* LPX address of the local interface which received a heartbeat packet */
	struct
	{
		BYTE Node[6];
		BYTE Reserved[2];
	} LocalAddress;
} NDAS_DEVICE_HEARTBEAT_INFO, *PNDAS_DEVICE_HEARTBEAT_INFO;

/* 
   Summary
   The %SYMBOLNAME% function initiates use of NDASHEAR.DLL or
   NDASHEAR.LIB by a process.
   
   Description
   The NdasHeartbeatInitialize function must be the first NdasHeartbeat
   function called by an application or DLL. It initializes internal
   data structures and underlying operating system services.

   An application must call one NdasHeartbeatUninitialize call for
   every successful NdasHearbeatInitialize call. This means, for example,
   that if an application calls NdasHeartbeatInitialize twice, it must call
   NdasHeartbeatUninitialize twice. 

   Returns
   If the function succeeds, the return value is nonzero.
   If the function fails, the return value is zero. 
   To get extended error information, call GetLastError. 
*/
NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatInitialize();

/* 
   Summary
   The %SYMBOLNAME% function terminates use of the NDASHEAR.DLL.
   
   Description
   An application or DLL is required to perform a successful 
   NdasHeartbeatInitialize call before it can use other NdasHeartbeat API
   calls. When it has completed the use of NdasHeartbeat API, 
   the application or DLL must call NdasHeartbeatUninitialize to
   deregister itself from a NdasHeartbeat API implementation and
   allow the implementation to free any resources allocated on behalf of
   the application or DLL.
   
   Before cleaning up NdasHeartbeat API, be sure to clean up all registered 
   handles, or some resources may leak.

   Returns
   If the function succeeds, the return value is nonzero.
   If the function fails, the return value is zero. 
   To get extended error information, call GetLastError. 
*/
NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatUninitialize();

/*
   <TITLE NdasHeartbeatProc>
   
   Summary
   The NdasHeartbeatProc function is an application-defined function used
   with the NdasHeartbeatRegisterNotification function.

   The NDASHEARPROC type defines a pointer to this callback function.
   NdasHeartbeatProc is a placeholder for the application-defined function
   name.

   Parameters
   %PAR0% :  [in] Pointer to a NDAS_DEVICE_HEARTBEAT_INFO structure
             containing NDAS device heartbeat information.
   %PAR1% :  [in] Application-defined parameter passed to the 
             NdasHeartbeatRegisterNotification function.
*/

typedef VOID (NDASHEARAPI_CALLBACK *NDASHEARPROC)
	(CONST NDAS_DEVICE_HEARTBEAT_INFO* pHeartbeat, LPVOID lpContext);

/* 
   Summary
   The %SYMBOLNAME% registers a callback .
   
   Description
   Application can use the %SYMBOLNAME% function to register to receive
   NDAS device heartbeat notification.

   It is limited to a single process to use this function.
   In a process, you can register multiple callbacks.
   If any other process is using this function, no notification will be sent,
   including NDAS service. It means that if an application registers 
   a callback function, NDAS service will be disrupted.
   No error messages will be generated in case of this conflict at this time.

   Notification handles returned by %SYMBOLNAME% must be closed by calling 
   the NdasHeartbeatUnregisterNotification function when they are no longer 
   needed.

   Parameters
   %PAR0% :  [in] Specifies the callback function that will be 
             executed when a NDAS heartbeat packet is arrived.
   %PAR1% :  [in] Pointer to an application-defined context block.
             For each call to the callback function, this will be supplied.

   Returns
   If the function succeeds, the return value is a handle to the registered
   callback function. You can get 

   If the function fails, the return value is zero. To get extended error
   information, call GetLastError.
*/
NDASHEAR_LINKAGE
HANDLE
NDASHEARAPI
NdasHeartbeatRegisterNotification(
	IN NDASHEARPROC proc, 
	IN LPVOID lpContext);

/* 
   Summary
   The %SYMBOLNAME% function closes the specified NDAS device heartbeat
   notification handle.
   
   Parameters
   %PAR0% :  [in] Heartbeat notification handle returned by 
              NdasHeartbeatRegisterNotification function.

   Description
   
   Before cleaning up NdasHeartbeat API, be sure to clean up all registered 
   handles, or some resources may leak.

   Returns
   If the function succeeds, the return value is nonzero.
   If the function fails, the return value is zero. 
   To get extended error information, call GetLastError. 
*/
NDASHEAR_LINKAGE
BOOL
NDASHEARAPI
NdasHeartbeatUnregisterNotification(
	IN HANDLE h);

#ifdef __cplusplus
}
#endif

#endif /* NDAS_HEAR_H_INCLUDED */
