/*++

  NDAS Communication Access API Header

  Copyright (C) 2002-2004 XIMETA, Inc.
  All rights reserved.

--*/

#ifndef _NDASCOMM_H_
#define _NDASCOMM_H_

#pragma once

#if defined(_WINDOWS_)

#ifndef _INC_WINDOWS
	#error ndascomm.h requires windows.h to be included first
#endif

#if   defined(NDASCOMM_EXPORTS)
#define NDASCOMM_API 
#elif defined(NDASCOMM_SLIB)
#define NDASCOMM_API
#else
#define NDASCOMM_API __declspec(dllimport)
#endif

#else /* defined(WIN32) || defined(UNDER_CE) */
#define NDASCOMM_API 
#endif

#ifndef _NDAS_TYPE_H_
#include "ndastype.h"
#endif /* _NDAS_TYPE_H_ */

#ifndef _NDASCOMM_TYPE_H_
#include "ndascomm_type.h"
#endif /* _NDASCOMM_TYPE_H_ */

#ifndef NDASAPICALL
#define NDASAPICALL __stdcall
#endif

#ifndef MAKEDWORD
#define MAKEDWORD(l,h) ((DWORD)MAKELONG(l,h))
#endif

#define NDASCOMM_API_VERSION_MAJOR 0x0001
#define NDASCOMM_API_VERSION_MINOR 0x0020
#define NDASCOMM_API_VERSION MAKEDWORD(NDASCOMM_API_VERSION_MAJOR, NDASCOMM_API_VERSION_MINOR)

#ifdef __cplusplus
extern "C" {
#endif

/* 

Use the following definition for WinCE specifics
#if defined(_WIN32_WCE_)
#endif

*/

/* Summary
   The %SYMBOLNAME% function initiates use of NdasComm.DLL or
   NdasComm.LIB by a process.
   
   Description
   The NdasCommInitialize function must be the first NdasComm
   function called by an application or DLL.
   
   Returns
   The NdasCommInitialize function returns TRUE if successful.
   Otherwise, it returns FALSE.                              */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommInitialize();

/* Summary
   The NdasCommUninitialize function makes end use of NdasComm.DLL or
   NdasComm.LIB by a process.
   
   Description
   The NdasCommUnnitialize function must be the last NdasComm
   function called by an application or DLL.
   
   Returns
   The NdasCommUnnitialize function returns TRUE if successful.
   Otherwise, it returns FALSE.                              */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUninitialize();

/* Summary
   The NdasCommGetAPIVersion function returns the current version information
   of the loaded library
   
   Description
   The NdasCommGetAPIVersion function returns the current
   version information of the loaded library
   
   Returns
   Lower word contains the major version number and higher word the
   minor
   
   \version number.                                             */
NDASCOMM_API
DWORD
NDASAPICALL
NdasCommGetAPIVersion();

/* Summary
   The NdasCommConnect function connects to a NDAS Device using
   given information.
   
   Description
   The NdasCommConnect function connects to a NDAS Device using
   given connection information. The NdasCommConnect function
   connects to the NDAS Device and process login with given
   login information. When login process succeeds and if the
   NDAS Device is a block device(non-packet device), the
   NdasCommconnect function process handshake to the connect and
   retrieve DMA, LBA, LBA48 information to the handle which are
   used to read/write/verify the NDAS Device. Re-handshaking is
   available using NdasCommIdeCommand but handshake information
   is not stored to the handle.
   
   Parameters
   %PAR0% :  A connection information to a NDAS Device.
   
   Returns
   If the function succeeds, the return value is an open handle
   to the NDAS Device.
   
   If the function fails, the return value is zero. To get
   extended error information, call GetLastError.
   
   Example
   
   <CODE>// Connects to and disconnects from a NDAS Device
   
   BOOL bResult;
   NDASCOMM_CONNECTION_INFO ConnectionInfo;
   HNDAS hNDASDevice;
   
   ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
   // Be sure to set the structure size before calling the function
   ConnectionInfo.Size = sizeof(NDASCOMM_CONNECTION_INFO);
   ConnectionInfo.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
   ConnectionInfo.UnitNo = 0;
   ConnectionInfo.WriteAccess = TRUE;
   ConnectionInfo.Protocol = NDASCOMM_TRANSPORT_LPX;
   ConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_ID;
   // Other fields are zero to be default.
   CopyMemory(ConnectionInfo.Address.NdasId.Id, "ABCDE12345EFGHI67890", 20);
   CopyMemory(ConnectionInfo.Address.NdasId.Key, "12345", 5);
   
   hNDASDevice = NdasCommConnect(&ConnectionInfo);
   
   if (!hNDASDevice) return FALSE; // failed to create connection
   
   bResult = NdasCommDisconnect(hNDASDevice);
   
   if (!bResult) return FALSE;
   
   return TRUE;
   </CODE>
   See Also
   NdasCommBlockDeviceRead, NdasCommBlockDeviceVerify,
   NdasCommBlockDeviceWrite, NdasCommBlockDeviceWriteSafeBuffer,
   NdasCommDisconnect, _NDASCOMM_CONNECTION_INFO                       */
NDASCOMM_API
HNDAS
NDASAPICALL
NdasCommConnect(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo);

/* Summary
   The NdasCommDisconnect function closes connection to NDAS
   Device.
   
   Description
   The NdasCommDisconnect function closes connection to NDAS
   Device. The NdasCommDisconnect function try to logout from the
   NDAS Device. And whether logout process succeed or
   not, the function disconnects connection.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   See Also
   NdasCommConnect                                               */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnect(
	__in HNDAS NdasHandle);

typedef enum _NDASCOMM_DISCONNECT_FLAG {
	NDASCOMM_DF_NONE = 0x0,
	NDASCOMM_DF_DONT_LOGOUT = 0x1
} NDASCOMM_DISCONNECT_FLAG;

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommDisconnectEx(
	__in HNDAS NdasHandle,
	__in DWORD DisconnectFlags);

/* Summary
   The NdasCommBlockDeviceRead function reads data from the
   unit device.
   
   Description
   The NdasCommBlockDeviceRead function reads data from the
   unit device. The function sends one of WIN_READ,
   WIN_READDMA, WIN_READ_EXT and WIN_READDMA_EXT by unit device's
   DMA, Ultra DMA and LBA48 capability. The function should not be
   called to read data from a non-block device or non LBA
   supporting block device.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Sector position of the NDAS Device to read data
             from. If value is negative, the function counts back
             from the last sector. (ex\: \-1 indicates the last
             sector)
   %PAR2% :  Number of data size in sectors to read.
   %PAR3% :  A pointer to the read data buffer.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// Reads 4 sectors of data from sector 10 of unit device. Suppose hNDASDevice is valid handle to a NDAS Device.
   BOOL bResult;
   CHAR data_buffer[4 * 512];
   
   bResult = NdasCommBlockDeviceRead(hNDASDevice, 10, 4, data_buffer);
   </CODE>                                                                                                                    */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceRead(
	__in HNDAS NdasHandle,
	__in INT64 Location,
	__in UINT64 SectorCount,
	__out_bcount(SectorCount*512) VOID* Data);

/* Summary
   The NdasCommBlockDeviceWrite function writes data to a unit
   device.
   
   Description
   The NdasCommBlockDeviceWrite function writes data to a unit
   device. The function sends one of WIN_WRITE, WIN_WRITEDMA,
   WIN_WRITE_EXT and WIN_WRITEDMA_EXT by unit device's DMA,
   Ultra DMA and LBA48 capability. The function should not be
   called to write data to a non-block device or non LBA
   supporting block device.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Sector position of the Unit Device to write data to. If
             value is negative, the function counts back from the
             last sector. (ex\: \-1 indicates the last sector)
   %PAR2% :  Number of data size in sectors to write.
   %PAR3% :  A pointer to the write data buffer. Contents in buffer
             will be filled with invalid data after function returns.
             Use NdasCommBlockDeviceWriteSafeBuffer function to
             protect buffer.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// Writes 2 sectors of data to last 2 sectors of unit device. Suppose hNDASDevice is valid handle to a NDAS Device.
   BOOL bResult;
   CHAR data_buffer[2 * 512];
   
   bResult = NdasCommBlockDeviceWrite(hNDASDevice, -2, 2, data_buffer);
   </CODE>
   See Also
   NdasCommBlockDeviceWriteSafeBuffer                                                                                        */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceWrite(
	__in HNDAS NdasHandle,
	__in INT64 LogicalBlockAddress,
	__in UINT64	TransferBlocks,
	__inout_bcount(SectorCount*512) CONST VOID* Buffer);

/* Summary
   The NdasCommBlockDeviceWriteSafeBuffer function writes data
   to a unit device without corrupting data buffer.
   
   This function is now deprecated. Use NdasCommBlockDeviceWrite instead.
   
   Description
   The NdasCommBlockDeviceWriteSafeBuffer does act same way as
   NdasCommBlockDeviceWrite function does except one thing.
   While the NdasCommBlockDeviceWrite function corrupts data
   buffer after the function returns, the
   NdasCommBlockDeviceWriteSafeBuffer function does not.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Sector position of the Unit Device to write data
             to. If value is negative, the function counts back from
             the last sector. (ex\: \-1 indicates the last sector)
   %PAR2% :  Number of data size in sectors to write.
   %PAR3% :  A pointer to the write data buffer.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   See Also
   NdasCommBlockDeviceWrite                                          */

#define NdasCommBlockDeviceWriteSafeBuffer \
	NdasCommBlockDeviceWrite

/* Summary
   The NdasCommBlockDeviceVerify function verifies the unit
   device.
   
   Description
   The NdasCommBlockDeviceVerify function verifies the unit
   device. The function sends one of WIN_VERIFY, WIN_VERIFY_EXT
   by device's LBA48 capability. The function should not be
   called to verify a non-block device or non LBA supporting
   block device.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Sector position of the Unit Device to verify. If value
             is negative, the function counts back from the last
             sector. (ex\: \-1 indicates the last sector)
   %PAR2% :  Number of data size in sectors to verify.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.                            */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceVerify(
	__in HNDAS NdasHandle,
	__in INT64 Location,
	__in UINT64 SectorCount);

/* Summary
   The NdasCommSetFeatures set features of the unit device.
   
   Description
   The NdasCommSetFeatures function send WIN_SETFEATURES command
   and parameters to the unit device. See ATA/ATAPI
   specification document for detailed information.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Subcommand code. Features register.
   %PAR2% :  Subcommand specific. Sector Count register.
   %PAR3% :  Subcommand specific. LBA Low register.
   %PAR4% :  Subcommand specific. LBA Mid register.
   %PAR5% :  Subcommand specific. LBA High register.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// Set dma of the unit device to Ultra DMA level 5
   BOOL bResult;
   BYTE feature = 0x03; // Set transfer mode based on value in Sector Count register.
   BYTE dma_feature_mode = 0x40 | 5; // Ultra DMA 5
   bResult = NdasCommSetFeatures(hNDASDevice, feature, dma_feature_mode, 0, 0, 0);
   </CODE>                                                                            */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetFeatures(
	__in HNDAS NdasHandle,
	__in BYTE Feature,
	__in BYTE Param0,
	__in BYTE Param1,
	__in BYTE Param2,
	__in BYTE Param3);


/* 

	Summary:

	NdasCommSetReceiveTimeout, NdasCommSetSendTimeout,
	NdasCommGetReceiveTimeout, NdasCommGetSendTimeout functions
	are used for settings timeout values for data-transfer functions.

	NdasCommSetReceiveTimeout, NdasCommSetSendTimeout 
	sets the timeout value for corresponding data-transfer functions. 

	NdasCommGetReceiveTimeout, NdasCommGetSendTimeout
	retrieves the timeout value for corresponding data-transfer functions.

	Once timeout occurred, the given handle is marked as invalid and
	cannot be used for any further transmission functions.
	   
	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Indicator specifying the time\-out interval, in
			 milliseconds. If %PAR1% is 0, the function's time\-out
			 interval never expires (acts in synchronous mode).

	Returns

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	See Also

	NdasCommSetSendTimeout, 
	NdasCommGetSendTimeout, 
	NdasCommGetReceiveTimeout, 
	NdasCommConnect                      

*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetReceiveTimeout(
	__in HNDAS NdasHandle, 
	__in DWORD Timeout);

/* Refer NdasCommSetReceiveTimeout for more information. */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetReceiveTimeout(
	__in HNDAS NdasHandle, 
	__out LPDWORD Timeout);


/* Refer NdasCommSetReceiveTimeout for more information. */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetSendTimeout(
	__in HNDAS NdasHandle,
	__in DWORD Timeout);

/* Refer NdasCommSetReceiveTimeout for more information. */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetSendTimeout(
	__in HNDAS NdasHandle, 
	__out LPDWORD Timeout);

/*
	Summary:

	The NdasCommGetHostAddress function retrieves the host
	address which is connected to the NDAS Device.

	The NdasCommGetHostAddress function retrieves the host
	address of the NDAS Device. If the
	connection type is NDASCOMM_TRANSPORT_LPX, the
	NdasCommGetHostAddress function returns 6 bytes of LPX
	Address.

	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Pointer to a data buffer to store the host
			 address. If NULL, NdasCommGetHostAddress returns
			 required buffer length to %PAR2%
	%PAR2% :  A size of the address buffer.

	Returns:

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.                         

*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetHostAddress(
	__in HNDAS NdasHandle,
	__out_opt PBYTE Buffer,
	__inout LPDWORD BufferLength);

/* 
	Summary:

	The NdasCommGetDeviceHardwareInfo function retrieves the hardware
	information of the NDAS device.

	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Pointer to a NDAS_DEVICE_HARDWARE_INFO structure that
	          receives the information.
			  Size field should be set as sizeof(NDAS_DEVICE_HARDWARE_INFO)
			  before calls this function.

	Returns:

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	Example:

	<CODE>
	
	BOOL bResult;
	UINT32 MaxBlocks;
	NDAS_DEVICE_HARDWARE_INFO dinfo;
	
	ZeroMemory(&dinfo, sizeof(NDAS_DEVICE_HARDWARE_INFO));
	dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);

	bResult = NdasCommGetDeviceHardwareInfo(hNdasDevice, &dinfo);

	</CODE>

	See Also:

	NDAS_DEVICE_HARDWARE_INFO
*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceHardwareInfo(
	__in HNDAS NdasHandle,
	__inout PNDAS_DEVICE_HARDWARE_INFO HardwareInfo);

/*
	<TITLE NdasCommGetUnitDeviceHardwareInfo>

	Summary

	The NdasCommGetUnitDeviceHardwareInfo function retrieves the hardware
	information of the specified unit device from the NDAS device.

	Description

	NdasCommGetUnitDeviceStat is also available for counter information,

	Parameters

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Pointer to a NDAS_UNITDEVICE_HARDWARE_INFO structure that
              receives the information.
			  Size field should be set as sizeof(NDAS_UNITDEVICE_HARDWARE_INFO)
			  before calls this function.

	Returns

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	See Also

	NDAS_UNITDEVICE_HARDWARE_INFO, NdasCommGetUnitDeviceStat
*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoW(
	__in HNDAS NdasHandle,
	__inout PNDAS_UNITDEVICE_HARDWARE_INFOW pUnitInfo);

/* <COMBINE NdasCommGetUnitDeviceHardwareInfoW> */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceHardwareInfoA(
	__in HNDAS NdasHandle,
	__inout PNDAS_UNITDEVICE_HARDWARE_INFOA pUnitInfo);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasCommGetUnitDeviceHardwareInfo NdasCommGetUnitDeviceHardwareInfoW
#else
#define NdasCommGetUnitDeviceHardwareInfo NdasCommGetUnitDeviceHardwareInfoA
#endif
/*DOM-IGNORE-END*/

/*
	Summary
	
	NdasCommGetDeviceStat and NdasCommGetUnitDeviceStat retrieves 
	counter information from the NDAS Device.

	Description

	Unlike other functions, these functions make use of 
	NDASCOMM_CONNECTION_INFO structure instead of using HNDAS handle.
	These uses NDASCOMM_LOGIN_TYPE_DISCOVER as a login type,
	which does not increase RO or RW host count.
	Connection to the NDAS device is disconnected immediately after
	retrieving counter information.

	NdasCommGetDeviceStat contains statistics for containing unit devices.
	Thus it is recommended to use NdasCommGetDeviceStat instead of 
	calling NdasCommGetUnitDeviceStat twice,

	Parameters

	pConnectionInfo:
	
			Pointer to a NDASCOMM_CONNECTION_INFO structure which
			contains connection information to the NDAS Device.
			UnitNo is not used for NdasCommGetDeviceStat.
			LoginType should be NDASCOMM_LOGIN_TYPE_DISCOVER.

	pDeviceStat: 
			
			Pointer to a NDAS_DEVICE_STAT structure that receives
			the information.
			Size field should be set as sizeof(NDAS_UDEVICE_STAT),

	pUnitDeviceStat:

	        Pointer to a NDAS_UNITDEVICE_STAT structure that receives
			the information.
			Size field should be set as sizeof(NDAS_UNITDEVICE_STAT),

	        Indicator specifying the time\-out interval, in
			milliseconds. If %PAR2% is 0, the function's time\-out
			interval never expires.
			
	Returns

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	Example

	<CODE>
	
	// retrieve unit device statistics

	BOOL bResult;
	NDASCOMM_CONNECTION_INFO ConnectionInfo;
	NDAS_UNITDEVICE_STAT ustat;

	ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	// Be sure to set the structure size before calling the function
	ConnectionInfo.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_ID;
	ConnectionInfo.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ConnectionInfo.UnitNo = 0;
	ConnectionInfo.WriteAccess = FALSE; // Not need RW access
	ConnectionInfo.Protocol = NDASCOMM_TRANSPORT_LPX;
	CopyMemory(ConnectionInfo.Address.NdasId.Id, "ABCDE12345EFGHI67890", 20);
	CopyMemory(ConnectionInfo.Address.NdasId.Key, "12345", 5);

	ZeroMemory(&ustat, sizeof(NDAS_UNITDEVICE_STAT);
	// Be sure to set the structure size before calling the function
	ustat.Size = sizeof(NDAS_UNITDEVICE_STAT);

	bResult = NdasCommGetUnitDeviceStat(&ConnectionInfo, &ustat);

	</CODE>
	
	See Also:

	_NDASCOMM_CONNECTION_INFO, 
	_NDAS_DEVICE_STAT,
	_NDAS_UNITDEVICE_STAT,
	NdasCommGetUnitDeviceInfo                                                    
*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceStat(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo,
	__inout PNDAS_DEVICE_STAT DeviceStat);

/* Consult NdasCommGetDeviceStat for more information */

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceStat(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo,
	__inout PNDAS_UNITDEVICE_STAT UnitDeviceStat);


/* 
	Summary

	The NdasCommGetDeviceID function retrieve device ID and unit
	number from the NDAS Device.

	Description

	The NdasCommGetDeviceID function retrieve device ID and unit
	number from the NDAS Device.
	Either %PAR0% or %PAR1% can be used to specify the NDAS device.
	Both cannot be specified at the same time.

	Parameters

	%PAR0% :  Handle to the NDAS Device. If not NULL, %PAR1% must be NULL
	%PAR1% :  Connection Information to the NDAS device. If not NULL, %PAR0% must be NULL
	%PAR2% :  Pointer to a BYTE array to retrieve device ID.
	%PAR3% :  Pointer to a BYTE variable to retrieve unit number.
	%PAR4% :  Pointer to a BYTE variable to retrieve VID.

	Returns

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	Example

	<CODE>// retrieve NDAS Device ID & unit number

	BOOL bResult;
	BYTE DeviceID[6];
	BYTE UnitNo;

	bResult = NdasCommGetDeviceID(hNDASDevice, DeviceID, &UnitNo);

	</CODE>                                                        
*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceID (
	__in_opt HNDAS							 NdasHandle,
	__in_opt const PNDASCOMM_CONNECTION_INFO ConnectionInfo,
	__out	 PBYTE							 DeviceId,
	__out	 LPBYTE							 UnitNo,
	__out	 LPBYTE							 Vid
	);

/*

	Summary

	NdasCommIdeCommand sends IDE command and receives the result.

	Description
	The NdasCommIdeCommand sends IDE command and receives the result. 
	The function is designed to process all commands
	including WIN_READ, WIN_WRITE, WIN_VERIFY, etc. Caller MUST
	set NDASCOMM_IDE_REGISTER structure correctly. Unless, the
	NdasCommIdeCommand function will fail or halt. Because the
	NdasCommIdeCommand function does not use handshaked DMA, LBA
	information, use_dma, use_48 member in NDASCOMM_IDE_REGISTER
	must be set or reset correctly. See ATA/ATAPI specification
	document and NDASCOMM_IDE_REGISTER for detailed information.

	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  A structure with IDE register and command information.
	%PAR2% :  If a command needs additional data to send to the NDAS
			 Device, the variable is a pointer to a send data
			 buffer. Otherwise, NULL.
	%PAR3% :  If %PAR2% is not NULL, %PAR3% is number of bytes
			 sending by this call.
	%PAR4% :  If a command needs additional data to receive from the
			 NDAS Device, the variable is a pointer to a receive
			 data buffer. Otherwise, NULL.
	%PAR5% :  If %PAR4% is not NULL, %PAR5% is number of bytes
			 receiving by this call.

	Returns:

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	Example:

	<CODE>// write 1 sector to sector 0 using DMA and LBA 48

	BOOL bResult;
	NDASCOMM_IDE_REGISTER reg;
	BYTE send_buffer[512];
	UINT64 Location = 0;
	UINT SectorCount = 1;

	reg.device.lba_head_nr = 0; // sector : 0
	reg.device.dev = 0; // target id : 0
	reg.device.lba = 1; // use lba
	reg.command.command = 0x35; // == WIN_WRITEDMA_EXT;
	reg.reg.named_48.cur.features; // reserved
	reg.reg.named_48.prev.features; // reserved
	reg.reg.named_48.cur.sector_count = (BYTE)((SectorCount & 0x00FF) \>\> 0);
	reg.reg.named_48.prev.sector_count = (BYTE)((SectorCount & 0xFF00) \>\> 8);
	reg.reg.named_48.cur.lba_low = (BYTE)((Location & 0x00000000000000FF) \>\> 0);
	reg.reg.named_48.prev.lba_low = (BYTE)((Location & 0x000000000000FF00) \>\> 8);
	reg.reg.named_48.cur.lba_mid = (BYTE)((Location & 0x0000000000FF0000) \>\> 16);
	reg.reg.named_48.prev.lba_mid = (BYTE)((Location & 0x00000000FF000000) \>\> 24);
	reg.reg.named_48.cur.lba_high = (BYTE)((Location & 0x000000FF00000000) \>\> 32);
	reg.reg.named_48.prev.lba_high = (BYTE)((Location & 0x0000FF0000000000) \>\> 40);

	bResult = NdasCommIdeCommand(hNDASDevice, &reg, send_buffer, sizeof(send_buffer), NULL, 0);

	</CODE>

	See Also:

	_NDASCOMM_IDE_REGISTER                                                                      

*/
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommIdeCommand(
   __in HNDAS NdasHandle,
   __inout PNDASCOMM_IDE_REGISTER IdeRegister,
   __inout_bcount(WriteDataLen) PBYTE WriteData,
   __in UINT32 WriteDataLen,
   __out_bcount(ReadDataLen) PBYTE ReadData,
   __in UINT32 ReadDataLen);

/*
	Summary:

	The NdasCommVendorCommand sends vendor specific command and
	receives the results.

	Description:

	The NdasCommVendorCommand sends vendor specific command and
	receives the results. Some commands need to be executed by
	supervisor permission.

	See NDASCOMM_VCMD_PARAM for detailed commands and parameters.

	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Specifies vendor command code. Some command codes
			 need supervisor permission. See Description for
			 details.
	%PAR2% :  Pointer to parameter for vendor command.
	%PAR3% :  If not NULL, Additional data to send with
			 vendor command. Contents in the buffer will corrupted
			 after sent.
	%PAR4% :  Length of the write buffer.
	%PAR5% :  If not NULL, Additional data to receive with
			 vendor command.
	%PAR6% :  Length of the read buffer.

	Returns:

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.

	Example:

	<CODE>
	// Get and Set HDD standby timer
	// Make sure that The NDAS Device is connected with supervisor permission.

	   NDASCOMM_VCMD_PARAM param_vcmd;

	   // read standby timer
	   bResult = NdasCommVendorCommand(
		   hNDAS,
		   ndascomm_vcmd_get_standby_timer,
		   &param_vcmd,
		   NULL, 0, NULL, 0);

	   // set standby timer to 30 minutes
	   param_vcmd.SET_STANDBY_TIMER.EnableTimer = TRUE;
	   param_vcmd.SET_STANDBY_TIMER.TimeValue = 30;

	   bResult = NdasCommVendorCommand(
		   hNDAS,
		   ndascomm_vcmd_set_standby_timer,
		   &param_vcmd,
		   NULL, 0, NULL, 0);

	   // read standby timer again
	   bResult = NdasCommVendorCommand(
		   hNDAS,
		   ndascomm_vcmd_get_standby_timer,
		   &param_vcmd,
		   NULL, 0, NULL, 0);
	</CODE>

	See Also:

	_NDASCOMM_VCMD_PARAM, 
	NdasCommConnect

*/

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommVendorCommand(
	__in HNDAS NdasHandle,
	__in NDASCOMM_VCMD_COMMAND VopCode,
	__inout PNDASCOMM_VCMD_PARAM Param,
	__inout_bcount_opt(WriteDataLen) PBYTE WriteData,
	__in_opt UINT32 WriteDataLen,
	__inout_bcount_opt(ReadDataLen) PBYTE ReadData,
	__in_opt UINT32 ReadDataLen);

/* 

	Summary:

	NdasCommLockDevice sends the lock command to the NDAS device.

	Description:

	NdasCommLockDevice sends the command to lock the NDAS device,
	which is used for locked operations. All other NDAS hosts should
	use the same lock index to make use of this feature.
	Locks are generally used for supporting Shared Write capabilities.

	Any NDAS hosts using Shared Write operations should use the lock
	to ensure that NDAS device buffer is not overlapped when multiple 
	write commands are sent from multiple NDAS hosts.

	For general purpose Shared Write locked functionalities,
	NdasCommBlockDeviceLockedWrite is recommended.
	  
	Parameters:

	%PAR0% :  Handle to the NDAS Device.
	%PAR1% :  Index of the lock number (0 - 3)
	%PAR2% :  Optional pointer to the DWORD to retrieve the lock counter
			 for diagnostic purposes

	Returns:

	If the function succeeds, the return value is non-zero. If
	the function fails, the return value is zero. To get extended
	error information, call GetLastError.
*/
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommLockDevice(
	__in HNDAS NdasHandle, 
	__in DWORD Index,
	__out_opt LPDWORD Counter);

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommUnlockDevice(
	__in HNDAS NdasHandle, 
	__in DWORD Index,
	__out_opt LPDWORD Counter);

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommCleanupDeviceLocks(
	__in CONST NDASCOMM_CONNECTION_INFO* ConnectionInfo);

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceLockedWrite(
	__in HNDAS NdasHandle,
	__in INT64 Location,
	__in UINT64	SectorCount,
	__inout_bcount(SectorCount*512) CONST VOID* Data);

#define NdasCommBlockDeviceLockedWriteSafeBuffer \
	NdasCommBlockDeviceLockedWrite

typedef enum _NDAS_DEVICE_ID_CLASS {
	NDAS_DIC_NDAS_IDW = 0,
	NDAS_DIC_NDAS_IDA = 1,
	NDAS_DIC_DEVICE_ID = 2
} NDAS_DEVICE_ID_CLASS;

#ifdef UNICODE
#define NDAS_DIC_NDAS_ID NDAS_DIC_NDAS_IDW
#else
#define NDAS_DIC_NDAS_ID NDAS_DIC_NDAS_IDA
#endif

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyDeviceChange(
	__in NDAS_DEVICE_ID_CLASS DeviceIdentifierClass,
	__in CONST VOID* DeviceIdentifier,
	__in_opt LPCGUID HostIdentifier);

NDASCOMM_API
BOOL
NDASAPICALL
NdasCommNotifyUnitDeviceChange(
	__in NDAS_DEVICE_ID_CLASS DeviceIdentifierClass,
	__in CONST VOID* DeviceIdentifier,
	__in DWORD UnitNumber,
	__in_opt LPCGUID HostIdentifier);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASCOMM_H_ */
