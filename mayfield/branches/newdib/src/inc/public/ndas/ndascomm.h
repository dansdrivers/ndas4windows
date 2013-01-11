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
#define NDASCOMM_API __declspec(dllexport)
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

#define NDASCOMM_API_VERSION_MAJOR 0x0001
#define NDASCOMM_API_VERSION_MINOR 0x0010

#define NdasCommInitialize					NCAPI0000
#define NdasCommGetAPIVersion				NCAPI0001
#define NdasCommConnect						NCAPI0002
#define NdasCommDisconnect					NCAPI0003
#define NdasCommBlockDeviceRead				NCAPI0004
#define NdasCommBlockDeviceWrite			NCAPI0005
#define NdasCommBlockDeviceWriteSafeBuffer	NCAPI0006
#define NdasCommBlockDeviceVerify			NCAPI0007
#define NdasCommSetFeatures					NCAPI0008
#define NdasCommGetDeviceInfo				NCAPI0009
#define NdasCommGetUnitDeviceInfo			NCAPI000A
#define NdasCommGetUnitDeviceStat			NCAPI000B
#define NdasCommGetDeviceID					NCAPI000C
#define NdasCommIdeCommand					NCAPI000D
#define NdasCommVendorCommand				NCAPI000E
#define NdasCommUninitialize				NCAPI000F
#define NdasCommGetTransmitTimeout			NCAPI0010
#define NdasCommSetTransmitTimeout			NCAPI0011
#define NdasCommGetHostAddress				NCAPI0012

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
   The NdasCommGetAPIVersion function returns the current
   \version information
   
   \of the loaded library
   
   Description
   The NdasCommGetAPIVersion function returns the current
   \version information
   
   \of the loaded library
   
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
   %PAR1% :  Indicator specifying the time\-out interval, in
             milliseconds. If %PAR1% is 0, the function's time\-out
             interval never expires.
   %PAR2% :  If %PAR2% is not NULL, the function tries the host
             addresses in %PAR2% . If %PAR2% is NULL, the function
             tries all the host addresses until the connection
             established. If protocol is NDASCOMM_TRANSPORT_LPX,
             %PAR2% should be LPSOCKET_ADDRESS_LIST type.
   
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
   ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A;
   ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
   ConnectionInfo.UnitNo = 0;
   ConnectionInfo.bWriteAccess = TRUE;
   ConnectionInfo.ui64OEMCode = NULL;
   ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
   memcpy(ConnectionInfo.sDeviceStringId, "ABCDE12345EFGHI67890", 20);
   memcpy(ConnectionInfo.sDeviceStringKey, "12345", 5);
   
   hNDASDevice = NdasCommConnect(&ConnectionInfo, 0, NULL);
   
   if(!hNDASDevice)
     return FALSE; // failed to create connection
   
   bResult = NdasCommDisconnect(hNDASDevice);
   
   if(!bResult)
     return FALSE;
   
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
	IN CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	IN CONST DWORD dwTimeout,
	IN CONST VOID* hint);

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
	IN HNDAS hNDASDevice);

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
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	OUT PBYTE	pData);

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
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN OUT PBYTE pMutableData);

/* Summary
   The NdasCommBlockDeviceWriteSafeBuffer function writes data
   to a unit device without corrupting data buffer.
   
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
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommBlockDeviceWriteSafeBuffer(
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount,
	IN CONST BYTE* pData);


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
	IN HNDAS	hNDASDevice,
	IN INT64	i64Location,
	IN UINT64	ui64SectorCount);

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
	IN HNDAS hNDASDevice,
	IN BYTE feature,
	IN BYTE param0,
	IN BYTE param1,
	IN BYTE param2,
	IN BYTE param3);

/* Summary
   The NdasCommGetTransmitTimeout retrieves the transmit timeout
   value.
   
   Description
   The NdasCommGetTransmitTimeout retrieves the transmit timeout
   value.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Indicator specifying the time\-out interval, in
             milliseconds. If %PAR1% is 0, the function's time\-out
             interval never expires(acts in synchronous mode).
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   See Also
   NdasCommSetTransmitTimeout, NdasCommConnect                      */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetTransmitTimeout(
	IN HNDAS hNDASDevice,
	OUT LPDWORD dwTimeout);

/* Summary
   The NdasCommSetTransmitTimeout sets the transmit timeout
   value.
   
   Description
   The NdasCommSetTransmitTimeout sets the transmit timeout
   value.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Indicator specifying the time\-out interval, in
             milliseconds. If %PAR1% is 0, the function's time\-out
             interval never expires(acts in synchronous mode).
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   See Also
   NdasCommGetTransmitTimeout, NdasCommConnect                      */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommSetTransmitTimeout(
	IN HNDAS hNDASDevice,
	IN CONST DWORD dwTimeout);

/* Summary
   The NdasCommGetHostAddress function retrieves the host
   address which is connected to the NDAS Device.
   
   Description
   The NdasCommGetHostAddress function retrieves the host
   address of the NDAS Device. If the
   connection type is NDASCOMM_TRANSPORT_LPX, the
   NdasCommGetHostAddress function returns 6 bytes of LPX
   Address.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Pointer to a data buffer to store the host
             address. If NULL, NdasCommGetHostAddress returns
             required buffer length to %PAR2%
   %PAR2% :  A size of the address buffer.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.                         */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetHostAddress(
	IN HNDAS hNDASDevice,
	OUT PBYTE Buffer,
	IN OUT LPDWORD lpBufferLen);


/* Summary
   The NdasCommGetDeviceInfo function retrieves static NDAS
   Device informations from the NDAS Device.
   
   Description
   The NdasCommGetDeviceInfo function retrieves static NDAS
   Device handle informations from the NDAS Device. A
   static information is not changed as long as the NDAS Device
   is connected. See NDASCOMM_HANDLE_INFO_TYPE for detailed
   information.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Handle information type to retrieve.
   %PAR2% :  Pointer to a data buffer to store the
             information.
   %PAR3% :  A size of the data buffer.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// retrieve maximum transfer blocks that the NDAS Device supports.
   
   BOOL bResult;
   UINT32 MaxBlocks;
   
   bResult = NdasCommGetDeviceInfo(hNDASDevice, ndascomm_handle_info_hw_max_blocks, &MaxBlocks, sizeof(MaxBlocks));
   </CODE>
   See Also
   _NDASCOMM_HANDLE_INFO_TYPE                                                                                        */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceInfo(
	IN HNDAS hNDASDevice,
	IN NDASCOMM_HANDLE_INFO_TYPE info_type,
	OUT PBYTE data,
	IN UINT32 data_len);

/* Summary
   The NdasCommGetUnitDeviceInfo retrieves static Unit
   Device information from the NDAS Device.
   
   Description
   The NdasCommGetUnitDeviceInfo retrieves static Unit
   Device information from the NDAS Device. See
   NDASCOMM_UNIT_DEVICE_INFO for detailed information.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  A structure to retrieve static unit device
             information.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   See Also
   _NDASCOMM_UNIT_DEVICE_INFO, NdasCommGetUnitDeviceStat       */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceInfo(
	IN HNDAS hNDASDevice,
	OUT PNDASCOMM_UNIT_DEVICE_INFO pUnitInfo);

/* Summary
   The NdasCommGetUnitDeviceStat retrieves dynamic Unit Device
   information from the NDAS Device.
   
   Description
   The NdasCommGetUnitDeviceStat retrieves dynamic Unit Device
   information from the NDAS Device. Because the function makes
   new connection to the NDAS Device as discovery mode, the
   function receives not a HNDAS handle parameter but
   NDASCOMM_CONNECTION_INFO parameter with
   NDASCOMM_LOGIN_TYPE_DISCOVER. The NdasCommGetUnitDeviceStat
   function does not increase RO or RW count. The function
   disconnects connection to the NDAS Device after retrieved
   dynamic information. See PNDASCOMM_UNIT_DEVICE_INFO for
   detailed information.
   
   Parameters
   %PAR0% :  A connection information to a NDAS Device.
   %PAR1% :  A structure to retrieve dynamic unit device information.
   %PAR2% :  Indicator specifying the time\-out interval, in
             milliseconds. If %PAR2% is 0, the function's time\-out
             interval never expires.
   %PAR3% :  If %PAR3% is not NULL, the function tries the host
             addresses in %PAR3% . If %PAR3% is NULL, the function
             tries all the host addresses until the connection
             established. If protocol is NDASCOMM_TRANSPORT_LPX,
             %PAR3% should be LPSOCKET_ADDRESS_LIST type.
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// retrieve dynamic unit device information
   
   BOOL bResult;
   NDASCOMM_CONNECTION_INFO ConnectionInfo;
   PNDASCOMM_UNIT_DEVICE_INFO UnitDynInfo;
   
   ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
   ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ID_A;
   ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
   ConnectionInfo.UnitNo = 0;
   ConnectionInfo.bWriteAccess = FALSE; // Not need RW access
   ConnectionInfo.ui64OEMCode = NULL;
   ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
   memcpy(ConnectionInfo.sDeviceStringId, "ABCDE12345EFGHI67890", 20);
   memcpy(ConnectionInfo.sDeviceStringKey, "12345", 5);
   
   bResult = NdasCommGetUnitDeviceStat(&ConnectionInfo, &UnitDynInfo, 0, NULL);
   </CODE>
   See Also
   _NDASCOMM_CONNECTION_INFO, _NDASCOMM_UNIT_DEVICE_INFO,
   NdasCommGetUnitDeviceInfo                                                    */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetUnitDeviceStat(
	IN CONST NDASCOMM_CONNECTION_INFO* pConnectionInfo,
	OUT PNDASCOMM_UNIT_DEVICE_STAT pUnitDynInfo,
	IN CONST DWORD dwTimeout,
	IN CONST VOID *hint);

/* Summary
   The NdasCommGetDeviceID function retrieve device ID and unit
   number from the NDAS Device.
   
   Description
   The NdasCommGetDeviceID function retrieve device ID and unit
   number from the NDAS Device.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  Pointer to a BYTE array to retrieve device ID.
   %PAR2% :  Pointer to a BYTE variable to retrive unit
             number.
   
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
   </CODE>                                                        */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommGetDeviceID(
	IN HNDAS hNDASDevice,
	OUT PBYTE pDeviceId,
	OUT LPDWORD pUnitNo);

/* Summary
   The NdasCommIdeCommand sends ide command and receives the
   \result.
   
   Description
   The NdasCommIdeCommand sends ide command and receives the
   \result. The function is designed to process all commands
   including WIN_READ, WIN_WRITE, WIN_VERIFY, etc. Caller MUST
   set NDASCOMM_IDE_REGISTER structure correctly. Unless, the
   NdasCommIdeCommand function will fail or halt. Because the
   NdasCommIdeCommand function does not use handshaked DMA, LBA
   information, use_dma, use_48 member in NDASCOMM_IDE_REGISTER
   must be set or reset correctly. See ATA/ATAPI specification
   document and NDASCOMM_IDE_REGISTER for detailed information.
   
   Parameters
   %PAR0% :  Handle to the NDAS Device.
   %PAR1% :  A structure with ide register and command information.
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
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   Example
   
   <CODE>// write 1 sector to sector 0 using DMA and LBA 48
   
   BOOL bResult;
   NDASCOMM_IDE_REGISTER reg;
   BYTE send_buffer[512];
   UINT64 Location = 0;
   UINT SectorCount = 1;
   
   reg.use_dma = 1;
   reg.use_48 = 1;
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
   See Also
   _NDASCOMM_IDE_REGISTER                                                                      */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommIdeCommand(
   IN HNDAS hNDASDevice,
   IN OUT PNDASCOMM_IDE_REGISTER pIdeRegister,
   IN OUT PBYTE pWriteData,
   IN UINT32 uiWriteDataLen,
   OUT PBYTE pReadData,
   IN UINT32 uiReadDataLen);

/* Summary
   The NdasCommVendorCommand sends vendor specific command and
   receives the results.
   
   Description
   The NdasCommVendorCommand sends vendor specific command and
   receives the results. Some commands need to be executed by
   supervisor permission.
   
   
   
   See NDASCOMM_VCMD_PARAM for detailed commands and parameters.
   
   Parameters
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
   
   Returns
   If the function succeeds, the return value is non-zero. If
   the function fails, the return value is zero. To get extended
   error information, call GetLastError.
   
   
   
   Example
   
   <CODE>// Get and Set HDD standby timer
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
   See Also
   _NDASCOMM_VCMD_PARAM, NdasCommConnect                                       */
NDASCOMM_API
BOOL
NDASAPICALL
NdasCommVendorCommand(
	IN HNDAS hNDASDevice,
	IN NDASCOMM_VCMD_COMMAND vop_code,
	IN OUT PNDASCOMM_VCMD_PARAM param,
	IN OUT PBYTE pWriteData,
	IN UINT32 uiWriteDataLen,
	IN OUT PBYTE pReadData,
	IN UINT32 uiReadDataLen);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _NDASCOMM_H_ */
