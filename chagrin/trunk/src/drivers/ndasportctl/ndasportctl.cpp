#include <windows.h>
#include <winsock2.h>
#include <tchar.h>
#include <setupapi.h>
#pragma warning(disable: 4201)
#include <winioctl.h>
#pragma warning(default: 4201)
#include <crtdbg.h>

#include <initguid.h>
#include <ndas/ndasportguid.h>
#include <ndas/ndasdluguid.h>
#include <ntddscsi.h>
#include "binparams.h"
#include "socketlpx.h"
#include "ndasportctl.h"

#ifdef WPP_TRACING
#define WPP_CONTROL_GUIDS \
	WPP_DEFINE_CONTROL_GUID(NdasPortCtlTraceGuid,(14D0A80B,FE91,4182,AD80,C34DF0812572),  \
	WPP_DEFINE_BIT(NDASPORTCTL_GENERAL) \
	)

#define WPP_LEVEL_FLAG_LOGGER(_Level,_Flags) \
	WPP_LEVEL_LOGGER(_Flags)

#define WPP_LEVEL_FLAG_ENABLED(_Level,_Flags) \
	(WPP_LEVEL_ENABLED(_Flags) && WPP_CONTROL(WPP_BIT_ ## _Flags).Level >= _Level)

#ifdef __cplusplus
extern "C" {
#endif
#include "ndasportctl.tmh"
#ifdef __cplusplus
}
#endif
#else

#define NDASPORTCTL_GENERAL 0x00000001

#define TRACE_LEVEL_NONE        0   // Tracing is not on
#define TRACE_LEVEL_FATAL       1   // Abnormal exit or termination
#define TRACE_LEVEL_ERROR       2   // Severe errors that need logging
#define TRACE_LEVEL_WARNING     3   // Warnings such as allocation failure
#define TRACE_LEVEL_INFORMATION 4   // Includes non-error cases(e.g.,Entry-Exit)
#define TRACE_LEVEL_VERBOSE     5   // Detailed traces from intermediate steps
#define TRACE_LEVEL_RESERVED6   6
#define TRACE_LEVEL_RESERVED7   7
#define TRACE_LEVEL_RESERVED8   8
#define TRACE_LEVEL_RESERVED9   9

VOID
CTRACE(DWORD Level, DWORD Flag, LPSTR Format,...) 
{ 
	Level; Flag; Format;
	return; 
}

#endif

namespace
{
	template <typename T> inline
	T ByteOffset(PVOID Pointer, size_t Offset)
	{
		return reinterpret_cast<T>(static_cast<PBYTE>(Pointer) + Offset);
	}
}

HANDLE
NdasPortCtlOpenInterface(
	__in HDEVINFO DeviceInfoSet,
	__in PSP_DEVICE_INTERFACE_DATA DeviceInterfaceData,
	__in DWORD DesiredAccess)
{
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData = NULL;
	ULONG predictedLength = 0;
	ULONG requiredLength = 0;

	//
	// Allocate a function class device data structure to receive the
	// information about this particular device.
	//

	SetupDiGetDeviceInterfaceDetail (
		DeviceInfoSet,
		DeviceInterfaceData,
		NULL, // probing so no output buffer yet
		0, // probing so output buffer length of zero
		&requiredLength,
		NULL); // not interested in the specific dev-node

	if (ERROR_INSUFFICIENT_BUFFER != GetLastError())
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiGetDeviceInterfaceDetail failed %!WINERROR!\n",
			GetLastError());

		return INVALID_HANDLE_VALUE;
	}

	predictedLength = requiredLength;

	deviceInterfaceDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(
		HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, predictedLength));

	if (NULL == deviceInterfaceDetailData)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"HeapAlloc failed for %d bytes\n",
			predictedLength);

		return INVALID_HANDLE_VALUE;
	} 

	deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

	BOOL success = SetupDiGetDeviceInterfaceDetail(
		DeviceInfoSet,
		DeviceInterfaceData,
		deviceInterfaceDetailData,
		predictedLength,
		&requiredLength,
		NULL);

	if (!success)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiGetDeviceInterfaceDetail failed, Error=%!WINERROR!\n",
			GetLastError());

		HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);

		return INVALID_HANDLE_VALUE;
	}

	HANDLE deviceHandle = CreateFile(
		deviceInterfaceDetailData->DevicePath,
		DesiredAccess,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);

	if (INVALID_HANDLE_VALUE == deviceHandle)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"CreateFile(%ws) failed, Error=%!WINERROR!\n",
			deviceInterfaceDetailData->DevicePath,
			GetLastError());

		HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);
		return INVALID_HANDLE_VALUE;
	}

	CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
		"NdasPort Device File Created (%ws), Handle=%p\n",
		deviceInterfaceDetailData->DevicePath,
		deviceHandle);

	HeapFree(GetProcessHeap(), 0, deviceInterfaceDetailData);

	return deviceHandle;
}

HANDLE
WINAPI
NdasPortCtlCreateControlDevice(
	DWORD DesiredAccess)
{
	//
	// Open a handle to the device interface information set of all 
	// present toaster bus enumerator interfaces.
	//

	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_NDASPORT_INTERFACE_CLASS,
		NULL,
		NULL, 
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (INVALID_HANDLE_VALUE == deviceInfoSet)
	{
		DWORD winError = GetLastError();

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiGetClassDevs failed, Error=%!WINERROR!\n", 
			winError);

		SetLastError(winError);
		return INVALID_HANDLE_VALUE;
	}

	SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {0};
	deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL success = SetupDiEnumDeviceInterfaces(
		deviceInfoSet,
		0, // No care about specific PDOs
		&GUID_DEVINTERFACE_NDASPORT,
		0, //
		&deviceInterfaceData);

	if (!success)
	{
		DWORD winError = GetLastError();

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiEnumDeviceInterfaces failed, Error=%!WINERROR!\n", 
			winError);

		SetupDiDestroyDeviceInfoList(deviceInfoSet);
		SetLastError(winError);
		/* ERROR_NO_MORE_ITEMS */
		return INVALID_HANDLE_VALUE;
	}

	HANDLE deviceHandle = NdasPortCtlOpenInterface(
		deviceInfoSet, 
		&deviceInterfaceData,
		DesiredAccess);

	if (INVALID_HANDLE_VALUE == deviceHandle)
	{
		DWORD winError = GetLastError();

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlOpenInterface failed, Error=%!WINERROR!\n", 
			winError);

		SetupDiDestroyDeviceInfoList(deviceInfoSet);
		SetLastError(winError);
		return INVALID_HANDLE_VALUE;
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	return deviceHandle;
}

BOOL
WINAPI
NdasPortCtlGetPortNumber(
	__in HANDLE NdasPortHandle,
	__in PUCHAR PortNumber)
{
	DWORD portNumberInULong;
	DWORD bytesReturned;

	*PortNumber = 0;

	BOOL success = DeviceIoControl(
		NdasPortHandle,
		IOCTL_NDASPORT_GET_PORT_NUMBER,
		NULL,
		0,
		&portNumberInULong,
		sizeof(DWORD),
		&bytesReturned,
		NULL);

	if (!success)
	{
		DWORD winError = GetLastError();
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_GET_PORT_NUMBER failed, Error=%!WINERROR!\n", 
			winError);
		SetLastError(winError);
	}

	*PortNumber = static_cast<UCHAR>(portNumberInULong);

	return success;
}

BOOL
WINAPI
NdasPortCtlPlugInLogicalUnit(
	HANDLE NdasPortHandle,
	PNDAS_LOGICALUNIT_DESCRIPTOR Descriptor)
{
	DWORD bytesReturned;
	
	BOOL success = DeviceIoControl(
		NdasPortHandle,
		IOCTL_NDASPORT_PLUGIN_LOGICALUNIT,
		Descriptor,
		Descriptor->Size,
		NULL,
		0,
		&bytesReturned,
		NULL);

	if (!success)
	{
		DWORD winError = GetLastError();
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_PLUGIN_LOGICALUNIT failed, Error=%!WINERROR!\n", 
			winError);
		SetLastError(winError);
	}

	return success;
}

BOOL
WINAPI
NdasPortCtlEjectLogicalUnit(
	HANDLE NdasPortHandle,
	NDAS_LOGICALUNIT_ADDRESS Address,
	ULONG Flags)
{
	NDASPORT_LOGICALUNIT_EJECT ejectParam = {0};
	ejectParam.Size = sizeof(NDASPORT_LOGICALUNIT_EJECT);
	ejectParam.LogicalUnitAddress = Address;
	ejectParam.Flags = Flags;

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		NdasPortHandle,
		IOCTL_NDASPORT_EJECT_LOGICALUNIT,
		&ejectParam,
		sizeof(NDASPORT_LOGICALUNIT_EJECT),
		NULL,
		0,
		&bytesReturned,
		NULL);

	if (!success)
	{
		DWORD winError = GetLastError();
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_EJECT_LOGICALUNIT failed, Error=%!WINERROR!\n", 
			winError);
		SetLastError(winError);
	}

	return success;
}

BOOL
WINAPI
NdasPortCtlUnplugLogicalUnit(
	HANDLE NdasPortHandle,
	NDAS_LOGICALUNIT_ADDRESS Address,
	ULONG Flags)
{
	NDASPORT_LOGICALUNIT_UNPLUG unplugParam = {0};
	unplugParam.Size = sizeof(NDASPORT_LOGICALUNIT_EJECT);
	unplugParam.LogicalUnitAddress = Address;
	unplugParam.Flags = Flags;

	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		NdasPortHandle,
		IOCTL_NDASPORT_UNPLUG_LOGICALUNIT,
		&unplugParam,
		sizeof(NDASPORT_LOGICALUNIT_UNPLUG),
		NULL,
		0,
		&bytesReturned,
		NULL);

	if (!success)
	{
		DWORD winError = GetLastError();
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_UNPLUG_LOGICALUNIT failed, Error=%!WINERROR!\n", 
			winError);
		SetLastError(winError);
	}

	return success;
}

BOOL
WINAPI
NdasPortCtlIsLogicalUnitAddressInUse(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	DWORD bytesReturned;
	BOOL success = DeviceIoControl(
		NdasPortHandle,
		IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE,
		&LogicalUnitAddress, 
		sizeof(NDAS_LOGICALUNIT_ADDRESS),
		NULL, 
		0, 
		&bytesReturned,
		NULL);

	if (!success)
	{
		DWORD winError = GetLastError();
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE failed, Error=%!WINERROR!\n", 
			winError);
		SetLastError(winError);
	}

	return success;
}

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlBuildNdasAtaDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in ULONG LogicalUnitFlags,
	__in CONST NDAS_DEVICE_IDENTIFIER* DeviceIdentifier,
	__in ULONG NdasDeviceFlagMask,
	__in ULONG NdasDeviceFlags,
	__in ACCESS_MASK AccessMode,
	__in ULONG VirtualBytesPerBlock,
	__in PLARGE_INTEGER VirtualLogicalBlockAddress)
{
	DWORD requiredBytes = sizeof(NDAS_ATA_DEVICE_DESCRIPTOR);
	
	PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor = 
		static_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(
			HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredBytes));

	if (NULL == ndasAtaDeviceDescriptor)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"Memory allocation failure for %d bytes\n", requiredBytes);
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}

	PNDAS_LOGICALUNIT_DESCRIPTOR logicalUnitDescriptor = 
		&ndasAtaDeviceDescriptor->Header;

	logicalUnitDescriptor->Version = sizeof(NDAS_LOGICALUNIT_DESCRIPTOR);
	logicalUnitDescriptor->Size = requiredBytes;
	logicalUnitDescriptor->Address = LogicalUnitAddress;
	logicalUnitDescriptor->Type = NdasAtaDevice;
	logicalUnitDescriptor->Flags = LogicalUnitFlags;

	ndasAtaDeviceDescriptor->NdasDeviceId = *DeviceIdentifier;
	ndasAtaDeviceDescriptor->NdasDeviceFlagMask = NdasDeviceFlagMask;
	ndasAtaDeviceDescriptor->NdasDeviceFlags = NdasDeviceFlags;
	ndasAtaDeviceDescriptor->AccessMode = AccessMode;

	const BYTE DEFAULT_NDAS_DEVICE_PASSWORD[8] = { 
		0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };

	CopyMemory(
		ndasAtaDeviceDescriptor->DevicePassword,
		DEFAULT_NDAS_DEVICE_PASSWORD,
		sizeof(ndasAtaDeviceDescriptor->DevicePassword));

	C_ASSERT(
		sizeof(DEFAULT_NDAS_DEVICE_PASSWORD) == 
		sizeof(ndasAtaDeviceDescriptor->DevicePassword));

	ndasAtaDeviceDescriptor->VirtualBytesPerBlock = VirtualBytesPerBlock;
	ndasAtaDeviceDescriptor->VirtualLogicalBlockAddress = *VirtualLogicalBlockAddress;

	return logicalUnitDescriptor;
}

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlSetNdasDiskEncryption(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in CONST NDAS_DISK_ENCRYPTION_DESCRIPTOR* EncryptionDescriptor)
{
	_ASSERT(NULL != LogicalUnitDescriptor);

	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		SetLastError(ERROR_INVALID_DATA);
		return NULL;
	}

	switch(LogicalUnitDescriptor->Type) {
		case NdasAtaDevice: {
			PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor = 
				reinterpret_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(LogicalUnitDescriptor);

			PNDAS_ATA_DEVICE_DESCRIPTOR newNdasAtaDeviceDescriptor;

			if (0 == ndasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset)
			{
				DWORD descriptorLength = LogicalUnitDescriptor->Size;
				DWORD newDescriptorLength = descriptorLength + sizeof(NDAS_DISK_ENCRYPTION_DESCRIPTOR);

				newNdasAtaDeviceDescriptor = static_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(
					HeapReAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					LogicalUnitDescriptor,
					newDescriptorLength));

				if (NULL == newNdasAtaDeviceDescriptor)
				{
					CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
						"Memory allocation failure for %d bytes\n", newDescriptorLength);
					SetLastError(ERROR_OUTOFMEMORY);
					return NULL;
				}

				newNdasAtaDeviceDescriptor->Header.Size = newDescriptorLength;
				newNdasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset = descriptorLength;
			}
			else
			{
				newNdasAtaDeviceDescriptor = ndasAtaDeviceDescriptor;
			}

			CopyMemory(
				ByteOffset<PVOID>(
				newNdasAtaDeviceDescriptor, 
				newNdasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset),
				EncryptionDescriptor,
				sizeof(NDAS_DISK_ENCRYPTION_DESCRIPTOR));

			return &newNdasAtaDeviceDescriptor->Header;
		}
		case NdasExternalType: {
			//
			// Convert NDAS_DISK_ENCRYPTION_DESCRIPTOR to the LUR descriptor's
			// built-in structure.
			//

			PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
				reinterpret_cast<PNDAS_DLU_DESCRIPTOR>(LogicalUnitDescriptor);

			PLURELATION_DESC	lurDesc = &ndasDluDeviceDescriptor->LurDesc;

			switch(EncryptionDescriptor->EncryptType) {
			case NdasDiskEncryptionNone:
				lurDesc->CntEcrMethod = NDAS_CONTENTENCRYPT_METHOD_NONE;
				break;
			case NdasDiskEncryptionSimple:
				lurDesc->CntEcrMethod = NDAS_CONTENTENCRYPT_METHOD_SIMPLE;
				break;
			case NdasDiskEncryptionAES:
				lurDesc->CntEcrMethod = NDAS_CONTENTENCRYPT_METHOD_AES;
				break;
				}

				lurDesc->CntEcrKeyLength = EncryptionDescriptor->EncryptKeyLength;
				if(lurDesc->CntEcrKeyLength > NDAS_CONTENTENCRYPT_KEY_LENGTH) {
					HeapFree(GetProcessHeap(), 0, LogicalUnitDescriptor);
					return NULL;
				}
				CopyMemory(
					lurDesc->CntEcrKey, 
					EncryptionDescriptor->EncryptKey,
					lurDesc->CntEcrKeyLength);

			return LogicalUnitDescriptor;
		}
		default:
			_ASSERT(FALSE);
			SetLastError(ERROR_INVALID_DATA);
			return NULL;
	}

}

BOOL
WINAPI
NdasPortCtlSetNdasDeviceUserIdPassword(
	__inout PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex,
	__in ULONG UserId,
	__in_bcount(UserPasswordLength) CONST BYTE* UserPassword,
	__in ULONG UserPasswordLength
){
	_ASSERT(NULL != LogicalUnitDescriptor);

	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	switch(LogicalUnitDescriptor->Type) {
		case NdasAtaDevice: {
			SetLastError(ERROR_NOT_SUPPORTED);
			return FALSE;
		}
		case NdasExternalType: {

			PLURELATION_NODE_DESC lurTargetNodeDesc;

			lurTargetNodeDesc = NdasPortCtlFindNodeDesc(LogicalUnitDescriptor, TargetNodeIndex);
			if(lurTargetNodeDesc == NULL)
				return FALSE;

			//
			// Allow only for ATA/ATAPI devices.
			//

			if(lurTargetNodeDesc->LurnType == LURN_IDE_DISK ||
				lurTargetNodeDesc->LurnType == LURN_IDE_ODD ||
				lurTargetNodeDesc->LurnType == LURN_IDE_MO) {

				lurTargetNodeDesc->LurnIde.UserID = UserId;
				if(UserPassword && UserPasswordLength) {
					SetLastError(ERROR_NOT_SUPPORTED);
					return FALSE;
				}
			} else {
				SetLastError(ERROR_NOT_SUPPORTED);
				return FALSE;
			}
		}
		default:
			_ASSERT(FALSE);
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
	}
}

BOOL
WINAPI
NdasPortCtlSetNdasDeviceOemCode(
	__inout PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex,
	__in_bcount(DeviceOemCodeLength) CONST BYTE* DeviceOemCode,
	__in ULONG DeviceOemCodeLength)
{
	_ASSERT(NULL != LogicalUnitDescriptor);

	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	switch(LogicalUnitDescriptor->Type) {
		case NdasAtaDevice: {
			PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor;

			ndasAtaDeviceDescriptor = 
				reinterpret_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(LogicalUnitDescriptor);

			CopyMemory(
				ndasAtaDeviceDescriptor->DevicePassword,
				DeviceOemCode,
				DeviceOemCodeLength);
			break;
		}
		case NdasExternalType: {

			PLURELATION_NODE_DESC lurTargetNodeDesc;

			lurTargetNodeDesc = NdasPortCtlFindNodeDesc(LogicalUnitDescriptor, TargetNodeIndex);
			if(lurTargetNodeDesc == NULL)
				return FALSE;

			//
			// Allow only for ATA/ATAPI devices.
			//

			if(lurTargetNodeDesc->LurnType == LURN_IDE_DISK ||
				lurTargetNodeDesc->LurnType == LURN_IDE_ODD ||
				lurTargetNodeDesc->LurnType == LURN_IDE_MO) {

					CopyMemory(
						&lurTargetNodeDesc->LurnIde.Password,
						DeviceOemCode,
						DeviceOemCodeLength);
			} else {
				return FALSE;
			}
		}
		default:
			_ASSERT(FALSE);
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
	}


	return TRUE;
}


PNDAS_BLOCK_ACL
WINAPI
NdasPortCtlGetNdasBacl(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR	LogicalUnitDescriptor
){
	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	switch(LogicalUnitDescriptor->Type) {
		case NdasExternalType:{ 
			PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
				reinterpret_cast<PNDAS_DLU_DESCRIPTOR>(LogicalUnitDescriptor);
			PLURELATION_DESC	lurDesc = &ndasDluDeviceDescriptor->LurDesc;

			if(lurDesc->BACLLength && lurDesc->BACLOffset) {
				return (PNDAS_BLOCK_ACL)((PBYTE)lurDesc + lurDesc->BACLOffset);
			} else {
				return NULL;
			}
		}
		default:
			_ASSERT(FALSE);
			SetLastError(ERROR_INVALID_DATA);
			return NULL;
	}

}

//////////////////////////////////////////////////////////////////////////
//
// NDAS Down level Logical Unit descriptor manipulation
//


//
// Create a logical unit descriptor for multiple node LUR.
// LeafCount must be zero with single node LUR.
//

PNDAS_LOGICALUNIT_DESCRIPTOR
WINAPI
NdasPortCtlBuildNdasDluDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS	LogicalUnitAddress,
	__in ULONG						LogicalUnitFlags,
	__in NDAS_DEV_ACCESSMODE		NdasDevAccessMode,
	__in USHORT						LurDeviceType,
	__in PLARGE_INTEGER				LurVirtualLogicalBlockAddress,
	__in ULONG						LeafCount,
	__in ULONG						NdasBaclLength,
	__in PNDASPORTCTL_NODE_INITDATA	RootNodeInitData
	)
{
	BOOL	bret;
	DWORD requiredBytesWithOutBACL = 
		FIELD_OFFSET(NDAS_DLU_DESCRIPTOR, LurDesc) +
		SIZE_OF_LURELATION_DESC() +
		SIZE_OF_LURELATION_NODE_DESC(LeafCount) +
		SIZE_OF_LURELATION_NODE_DESC(0) * LeafCount;
	DWORD requiredBytes = 	requiredBytesWithOutBACL + NdasBaclLength;

	PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
		static_cast<PNDAS_DLU_DESCRIPTOR>(
			HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredBytes));

	if (NULL == ndasDluDeviceDescriptor)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"Memory allocation failure for %d bytes\n", requiredBytes);
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}
	if(RootNodeInitData == NULL) {
		if(LeafCount > 0) {
			CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
				"More than one leaf without root node.\n");
			SetLastError(ERROR_INVALID_DATA);
			_ASSERT(FALSE);
			return NULL;
		}
	}

	PNDAS_LOGICALUNIT_DESCRIPTOR logicalUnitDescriptor = 
		&ndasDluDeviceDescriptor->Header;

	//
	// Initialize the logical unit descriptor header
	//

	logicalUnitDescriptor->Version = sizeof(NDAS_LOGICALUNIT_DESCRIPTOR);
	logicalUnitDescriptor->Size = requiredBytes;
	logicalUnitDescriptor->Address = LogicalUnitAddress;
	logicalUnitDescriptor->Type = NdasExternalType;
	logicalUnitDescriptor->Flags = LogicalUnitFlags;
	CopyMemory(&logicalUnitDescriptor->ExternalTypeGuid, &NDASPORT_NDAS_DLU_TYPE_GUID, sizeof(GUID));


	//
	// Initialize the logical unit relation descriptor
	//
	ndasDluDeviceDescriptor->LurDesc.Length = (USHORT)requiredBytes;
	ndasDluDeviceDescriptor->LurDesc.Type = LUR_DESC_STRUCT_TYPE;
		ndasDluDeviceDescriptor->LurDesc.DevType = LurDeviceType;
	ndasDluDeviceDescriptor->LurDesc.DevSubtype = 0;
	ndasDluDeviceDescriptor->LurDesc.MaxOsRequestLength = 0;
		ndasDluDeviceDescriptor->LurDesc.DeviceMode = NdasDevAccessMode;
	ndasDluDeviceDescriptor->LurDesc.LurOptions = 0;
	ndasDluDeviceDescriptor->LurDesc.LurId[0] = LogicalUnitAddress.PathId;
	ndasDluDeviceDescriptor->LurDesc.LurId[1] = LogicalUnitAddress.TargetId;
	ndasDluDeviceDescriptor->LurDesc.LurId[2] = LogicalUnitAddress.Lun;
	ndasDluDeviceDescriptor->LurDesc.LurId[3] = LogicalUnitAddress.PortNumber;
	ndasDluDeviceDescriptor->LurDesc.EndingBlockAddr = LurVirtualLogicalBlockAddress->QuadPart;
	// Set LU relation node count
	// Include root node automatically allocated by this function.
	ndasDluDeviceDescriptor->LurDesc.LurnDescCount = 1 + LeafCount;

	//
	// BACL settings
	//

	if(NdasBaclLength) {
		ndasDluDeviceDescriptor->LurDesc.BACLLength = NdasBaclLength;
		ndasDluDeviceDescriptor->LurDesc.BACLOffset = requiredBytesWithOutBACL;
	}

	//
	// Build node relation tree
	// Currently, we only support 1 or 2 level depth.
	// Root node, or Root node + child nodes.
	//

	PLURELATION_NODE_DESC	lurRootNodeDesc = &ndasDluDeviceDescriptor->LurDesc.LurnDesc[0];

	// Init root node.
	lurRootNodeDesc->LurnType = LURN_NULL;
	lurRootNodeDesc->NextOffset = 0;
	lurRootNodeDesc->LurnChildrenCnt = LeafCount;

	// Init child nodes if exist.
	if(LeafCount) {
		ULONG idx_child;
		ULONG	childOffset;
		PLURELATION_NODE_DESC	childNodeDesc;

		childOffset = SIZE_OF_LURELATION_DESC() + SIZE_OF_LURELATION_NODE_DESC(LeafCount);
		// Set the first child node's offset to the root node.
		lurRootNodeDesc->NextOffset = (UINT16)childOffset;

		// Set offset to each child node.
		for(idx_child = 0; idx_child < LeafCount; idx_child ++) {
			// Get the current child node.
			childNodeDesc = (PLURELATION_NODE_DESC)((PBYTE)&ndasDluDeviceDescriptor->LurDesc + childOffset);
			childNodeDesc->LurnType = LURN_NULL;
			// Increase offset to the next sibling.
			childOffset += SIZE_OF_LURELATION_NODE_DESC(0);

			// Set offset for the next sibling.
			// Set zero offset if the node is the last node.
			if(idx_child == LeafCount - 1)
				childNodeDesc->NextOffset = 0;
			else
				childNodeDesc->NextOffset = (UINT16)childOffset;

			childNodeDesc->LurnChildrenCnt = 0;
			// Set the parent node index to the current child node.
			childNodeDesc->LurnParent = 0;

			// Set child index to the root node.
			lurRootNodeDesc->LurnChildren[idx_child] = idx_child + 1;
		}
	}

	//
	// Initialize the root node if init data is available.
	//

	if(RootNodeInitData) {
		bret = NdasPortCtlSetupLurNode(
					lurRootNodeDesc,
					NdasDevAccessMode,
					RootNodeInitData);
		if(bret == FALSE) {
			HeapFree(GetProcessHeap(), 0, logicalUnitDescriptor);
			return NULL;
		}
	}

	return logicalUnitDescriptor;
}


//////////////////////////////////////////////////////////////////////////
//
// LUR node manipulation
//

//
// Find an LUR node matching an node index.
//


PLURELATION_NODE_DESC
WINAPI
NdasPortCtlFindNodeDesc(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in ULONG TargetNodeIndex
){
	ULONG idx_node;
	PLURELATION_DESC	lurDesc;
	PLURELATION_NODE_DESC targetNode;
	PNDAS_DLU_DESCRIPTOR	ndasDluDesc;

	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		SetLastError(ERROR_INVALID_DATA);
		return NULL;
	}

	_ASSERT(NdasExternalType == LogicalUnitDescriptor->Type);
	if (NdasExternalType != LogicalUnitDescriptor->Type)
	{
		SetLastError(ERROR_INVALID_DATA);
		return NULL;
	}

	ndasDluDesc = (PNDAS_DLU_DESCRIPTOR)LogicalUnitDescriptor;
	lurDesc = &ndasDluDesc->LurDesc;
	if(TargetNodeIndex >= lurDesc->LurnDescCount)
		return NULL;

	targetNode = lurDesc->LurnDesc;
	for(idx_node = 0; idx_node < TargetNodeIndex; idx_node ++) {
		if(targetNode->NextOffset == 0)
			return NULL;
		targetNode = (PLURELATION_NODE_DESC)((PUCHAR)lurDesc + targetNode->NextOffset);
	}

	return targetNode;
}


BOOL
WINAPI
NdasPortCtlSetupLurNode(
	__inout PLURELATION_NODE_DESC		LurNodeDesc,
	__in NDAS_DEV_ACCESSMODE			NdasDevAccessMode,
	__in PNDASPORTCTL_NODE_INITDATA		NodeInitData
){
	const BYTE DEFAULT_NDAS_DEVICE_OEMCODE[8] = { 
		0xBB, 0xEA, 0x30, 0x15, 0x73, 0x50, 0x4A, 0x1F };

		C_ASSERT(
			sizeof(DEFAULT_NDAS_DEVICE_OEMCODE) == 
			sizeof(LurNodeDesc->LurnIde.Password));

	//
	// LUR node common fields.
	//

	LurNodeDesc->LurnType = NodeInitData->NodeType;
	LurNodeDesc->LurnId = 0;
	LurNodeDesc->StartBlockAddr = NodeInitData->StartLogicalBlockAddress.QuadPart;
	LurNodeDesc->EndBlockAddr = NodeInitData->EndLogicalBlockAddress.QuadPart;
	LurNodeDesc->UnitBlocks = 
		NodeInitData->EndLogicalBlockAddress.QuadPart -
		NodeInitData->StartLogicalBlockAddress.QuadPart + 1;
	LurNodeDesc->MaxDataSendLength = 65536;
	LurNodeDesc->MaxDataRecvLength = 65536;
	LurNodeDesc->LurnOptions = 0;
	LurNodeDesc->LurnParent = 0;


	//
	// Initialize the specific part of the logical unit relation node
	//
	switch(NodeInitData->NodeType) {
		case LURN_AGGREGATION:
		case LURN_MIRRORING:
		case LURN_RAID0:
		case LURN_RAID1R:
		case LURN_RAID4R: {
			PNDASPORTCTL_INIT_RAID	initRaid = &NodeInitData->NodeSpecificData.Raid;
			PINFO_RAID	infoRaid = &LurNodeDesc->LurnInfoRAID;
			//
			// Initialize RAID information
			//
			infoRaid->SectorsPerBit = initRaid->SectorsPerBit;
			infoRaid->nSpareDisk =initRaid->SpareDisks;
			CopyMemory(&infoRaid->RaidSetId, &initRaid->RaidSetId, sizeof(GUID));
			CopyMemory(&infoRaid->ConfigSetId, &initRaid->ConfigSetId, sizeof(GUID));
			break;
		}
		case LURN_IDE_DISK:
		case LURN_IDE_ODD:
		case LURN_IDE_MO: {
			PNDASPORTCTL_INIT_ATADEV initAtaDev = &NodeInitData->NodeSpecificData.Ata;

			// Set the remote node address
			if(initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_TRANSPORT_PORTNO) {
				NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
					&initAtaDev->DeviceIdentifier,
					(USHORT)initAtaDev->TransportPortNumber,
					&LurNodeDesc->LurnIde.TargetAddress);
			} else {
				NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
					&initAtaDev->DeviceIdentifier,
					LPXRP_NDAS_PROTOCOL,
					&LurNodeDesc->LurnIde.TargetAddress);
			}

			if(initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_BINDING_ADDRESS) {
				CopyMemory(
					&LurNodeDesc->LurnIde.BindingAddress,
					&initAtaDev->BindingAddress,
					sizeof(LurNodeDesc->LurnIde.BindingAddress));
			}
			LurNodeDesc->LurnIde.HWType = 0;	// ASIC
			LurNodeDesc->LurnIde.HWVersion = initAtaDev->HardwareVersion;
			LurNodeDesc->LurnIde.HWRevision = initAtaDev->HardwareRevision;
			LurNodeDesc->LurnIde.EndBlockAddrReserved = 0;
			if(initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_USERID) {
				LurNodeDesc->LurnIde.UserID = initAtaDev->UserId;
			} else {
				// Set default password
				if(initAtaDev->DeviceIdentifier.UnitNumber == 0) {
					if(NdasDevAccessMode & NDASACCRIGHT_WRITE)
						LurNodeDesc->LurnIde.UserID = FIRST_TARGET_RW_USER;
					else
						LurNodeDesc->LurnIde.UserID = FIRST_TARGET_RO_USER;
				} else {
					if(NdasDevAccessMode & NDASACCRIGHT_WRITE)
						LurNodeDesc->LurnIde.UserID = SECOND_TARGET_RW_USER;
					else
						LurNodeDesc->LurnIde.UserID = SECOND_TARGET_RO_USER;
				}
			}

			if(initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_USERPASSWORD) {
				// Nothing to do currently.
			}

			if(initAtaDev->DeviceIdentifier.UnitNumber == 0) {
				LurNodeDesc->LurnIde.LanscsiTargetID = 0;
				LurNodeDesc->LurnIde.LanscsiLU = 0;
			} else {
				LurNodeDesc->LurnIde.LanscsiTargetID = 1;
				LurNodeDesc->LurnIde.LanscsiLU = 0;
			}

			if(initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_OEMCODE) {
				CopyMemory(
					&LurNodeDesc->LurnIde.Password,
					initAtaDev->DeviceOemCode,
					sizeof(LurNodeDesc->LurnIde.Password));
			} else {
				// Set default oem code
				CopyMemory(
					&LurNodeDesc->LurnIde.Password,
					DEFAULT_NDAS_DEVICE_OEMCODE,
					sizeof(LurNodeDesc->LurnIde.Password));
			}
			break;
		}
		default:
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
	}
 
	return TRUE;
}

VOID
WINAPI
NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
	__in PNDAS_DEVICE_IDENTIFIER	NdasDeviceIndentifier,
	__in USHORT						LpxPortNumber,
	__out PTA_LSTRANS_ADDRESS		TaLsTransAddress
){
	PLPX_ADDRESS	lpxAddress = (PLPX_ADDRESS)&TaLsTransAddress->Address[0].Address;

	// Set the remote node address
	TaLsTransAddress->TAAddressCount = 1;
	TaLsTransAddress->Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
	TaLsTransAddress->Address[0].AddressType = AF_LPX;
	CopyMemory(lpxAddress->Node, NdasDeviceIndentifier->Identifier, sizeof(lpxAddress->Node));
	lpxAddress->Port = HTONS(LpxPortNumber);
}

//////////////////////////////////////////////////////////////////////////
//
// RAID LUR node manipulation
//

#define __NDASPORTCTL_ULONLONG_BITS		(sizeof(ULONGLONG) * 8)

static
INT
NdasPortCtlFindLeastSignificantBit(
	ULONGLONG Value
){
	INT idx_bit;
	ULONGLONG	mask = 1;

	for(idx_bit = 0; idx_bit < __NDASPORTCTL_ULONLONG_BITS; idx_bit++) {

		if(Value & mask)
			return idx_bit;

		mask <<= 1;
	}

	return -1;
}

static
INT
NdasPortCtlFindMostSignificantBit(
	ULONGLONG Value
){
	INT idx_bit;
	ULONGLONG	mask = ( (ULONGLONG)1 << (__NDASPORTCTL_ULONLONG_BITS - 1));

	for(idx_bit = __NDASPORTCTL_ULONLONG_BITS - 1; idx_bit >= 0; idx_bit--) {

		if(Value & mask)
			return idx_bit;

		mask >>= 1;
	}

	return -1;
}

#undef __NDASPORTCTL_ULONLONG_BITS

BOOL
WINAPI
NdasPortCtlGetRaidEndAddress(
	__in LURN_TYPE	LurnType,
	__in  LONGLONG	InputEndAddress,
	__in ULONG		MemberCount,
	__out PLONGLONG	RaidEndAddress
){
	if(MemberCount == 0)
		return FALSE;
	switch(LurnType) {
		case LURN_RAID0: {
			INT	leastSignificantBitOfMemberCount = NdasPortCtlFindLeastSignificantBit(MemberCount);

			// Make sure the number of members is power of two.
			if(	leastSignificantBitOfMemberCount
				!= NdasPortCtlFindMostSignificantBit(MemberCount)) {
					return FALSE;
			}
			// Make sure the input end address is times of MemberCount.
			// It avoid dropping last few blocks.
			if( leastSignificantBitOfMemberCount > NdasPortCtlFindLeastSignificantBit(InputEndAddress + 1)) {
				return FALSE;
			}

			*RaidEndAddress = (InputEndAddress + 1)/ MemberCount - 1;
			break;
		}
		case LURN_AGGREGATION:
		case LURN_RAID1R:
			*RaidEndAddress = InputEndAddress;
			break;
		default:
			return FALSE;
	}

	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
//
// Legacy NDASBus interface support.
//
//////////////////////////////////////////////////////////////////////////


BOOL
NdasPortCtlSrbIoctl(
	HANDLE	hTargetDevice,
	ULONG	SrbIoctlCode,
	PVOID	Buffer,
	ULONG	BufferLength
){
	//
	// Make up IOCTL In-parameter
	//
	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	DWORD cbInBuffer = cbHeader + BufferLength;
	PBYTE lpInBuffer;
	DWORD cbReturned = 0;
	PSRB_IO_CONTROL pSrbIoControl;
	BOOL fSuccess;

	lpInBuffer = (PBYTE)HeapAlloc(GetProcessHeap(), 0, cbInBuffer);

	pSrbIoControl = (PSRB_IO_CONTROL) lpInBuffer;
	pSrbIoControl->HeaderLength = cbHeader;
	CopyMemory(pSrbIoControl->Signature, NDASSCSI_IOCTL_SIGNATURE, 8);
	pSrbIoControl->Timeout = 10;
	pSrbIoControl->ControlCode = SrbIoctlCode;
	pSrbIoControl->Length = BufferLength;

	RtlCopyMemory(lpInBuffer + cbHeader, Buffer, BufferLength);

	//
	// Send the SRB IOCTL.
	//

	fSuccess = DeviceIoControl(
		hTargetDevice,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	RtlCopyMemory(Buffer, lpInBuffer + cbHeader, BufferLength);

	if (!fSuccess) 
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL, "DeviceIoControl(IOCTL_SCSI_MINIPORT, "
			"%x) failed, error=0x%X\n", SrbIoctlCode, GetLastError());
		HeapFree(GetProcessHeap(), 0, lpInBuffer);
		return FALSE;
	}

	if (0 != pSrbIoControl->ReturnCode &&
		pSrbIoControl->ReturnCode != 1 /* SRB_STATUS_SUCCESS */ ) 
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL, "DeviceIoControl"
			"(IOCTL_SCSI_MINIPORT, %x) failed, returnCode=0x%X, error=0x%X\n",
			SrbIoctlCode, pSrbIoControl->ReturnCode, GetLastError());
		HeapFree(GetProcessHeap(), 0, lpInBuffer);
		return FALSE;
	}

	HeapFree(GetProcessHeap(), 0, lpInBuffer);
	return TRUE;
}

BOOL 
WINAPI
NdasPortCtlQueryNodeAlive(
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__out LPBOOL pbAlive,
	__out LPBOOL pbAdapterHasError
){
	BOOL	bret;
	HANDLE	handle;

	handle = NdasPortCtlCreateControlDevice(GENERIC_READ);
	if(handle == INVALID_HANDLE_VALUE) {
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlCreateControlDevice failed %!WINERROR!\n",
			GetLastError());
		return FALSE;
	}

	bret = NdasPortCtlGetPortNumber(
		handle, 
		&NdasLogicalUnitAddress.PortNumber);

	if (!bret)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlGetPortNumber failed %!WINERROR!\n",
			GetLastError());
		CloseHandle(handle);
		return FALSE;
	}

	bret = NdasPortCtlIsLogicalUnitAddressInUse(handle, NdasLogicalUnitAddress);
	if(bret == TRUE) {

		if(pbAlive)
			*pbAlive = TRUE;
		if(pbAdapterHasError)
			*pbAdapterHasError = FALSE;

	} else {

		if(pbAlive)
			*pbAlive = FALSE;
		if(pbAdapterHasError)
			*pbAdapterHasError = FALSE;
	}

	CloseHandle(handle);

	return TRUE;
}

BOOL
WINAPI
NdasPortCtlQueryInformation(
	__in HANDLE	DiskHandle,
	__in PNDASSCSI_QUERY_INFO_DATA Query,
	__in DWORD	QueryLength,
	__out PVOID	Information,
	__in DWORD	InformationLength
){
	BOOL	bret;
	PVOID	queryBuffer;
	DWORD	queryBufferLength;

	if(Query == NULL ||
		QueryLength < FIELD_OFFSET(NDASSCSI_QUERY_INFO_DATA, QueryData)){
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
	}

	queryBufferLength = (QueryLength > InformationLength) ? QueryLength : InformationLength;
	queryBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, queryBufferLength);

	CopyMemory(queryBuffer, Query, QueryLength);
	bret = NdasPortCtlSrbIoctl(DiskHandle, (ULONG)NDASSCSI_IOCTL_QUERYINFO_EX, queryBuffer, queryBufferLength);
	if(bret == FALSE) {
		HeapFree(GetProcessHeap(), 0, queryBuffer);

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlSrbIoctl failed %!WINERROR!\n",
			GetLastError());
		return FALSE;
	}

	CopyMemory(Information, queryBuffer, InformationLength);

	HeapFree(GetProcessHeap(), 0, queryBuffer);
	return TRUE;
}

BOOL
WINAPI
NdasPortCtlQueryDeviceMode(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PULONG					DeviceMode,
	__out PULONG					SupportedFeatures,
	__out PULONG					EnabledFeatures
){
	BOOL	bret;
	NDASSCSI_QUERY_INFO_DATA query;
	NDSCIOCTL_PRIMUNITDISKINFO	primaryUnitDiskInformation;

	query.InfoClass = NdscPrimaryUnitDiskInformation;
	query.NdasScsiAddress.PathId = NdasLogicalUnitAddress.PathId;
	query.NdasScsiAddress.TargetId = NdasLogicalUnitAddress.TargetId;
	query.NdasScsiAddress.Lun = NdasLogicalUnitAddress.Lun;
	query.NdasScsiAddress.PortId = 0;
	query.Length = sizeof(NDASSCSI_QUERY_INFO_DATA);
	query.QueryDataLength = 0;

	bret = NdasPortCtlQueryInformation(
		DiskHandle,
		&query,
		sizeof(NDASSCSI_QUERY_INFO_DATA),
		&primaryUnitDiskInformation,
		sizeof(NDSCIOCTL_PRIMUNITDISKINFO)
		);
	if(bret == TRUE) {
		if(DeviceMode)
			*DeviceMode = primaryUnitDiskInformation.Lur.DeviceMode;
		if(SupportedFeatures)
			*SupportedFeatures = primaryUnitDiskInformation.Lur.SupportedFeatures;
		if(EnabledFeatures)
			*EnabledFeatures = primaryUnitDiskInformation.Lur.EnabledFeatures;
	} else {
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlQueryInformation failed %!WINERROR!\n",
			GetLastError());
		return FALSE;
	}

	return TRUE;
}

BOOL 
WINAPI
NdasPortCtlStartStopRegistrarEnum(
	__in BOOL	bOnOff,
	__out LPBOOL	pbPrevState
){
	UNREFERENCED_PARAMETER(bOnOff);

	if(pbPrevState)
		*pbPrevState = FALSE;
	return TRUE;
}

BOOL
WINAPI
NdasPortCtlQueryEvent(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PULONG					pStatus
){
	UNREFERENCED_PARAMETER(DiskHandle);
	UNREFERENCED_PARAMETER(NdasLogicalUnitAddress);
	UNREFERENCED_PARAMETER(pStatus);

	SetLastError(ERROR_NO_MORE_ITEMS);
	return FALSE;
}


BOOL 
WINAPI
NdasPortCtlQueryPdoEventPointers(
	__in HANDLE						DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS	NdasLogicalUnitAddress,
	__out PHANDLE					AlarmEvent,
	__out PHANDLE					DisconnectEvent
){
	UNREFERENCED_PARAMETER(DiskHandle);
	UNREFERENCED_PARAMETER(NdasLogicalUnitAddress);
	UNREFERENCED_PARAMETER(AlarmEvent);
	UNREFERENCED_PARAMETER(DisconnectEvent);

	return FALSE;
}
