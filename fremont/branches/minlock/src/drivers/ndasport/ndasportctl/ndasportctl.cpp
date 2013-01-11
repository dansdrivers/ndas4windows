#include <windows.h>
// #include <winsock2.h>
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

HRESULT
WINAPI
NdasPortCtlCreateControlDevice(
	__in DWORD DesiredAccess,
	__deref_out HANDLE* DeviceHandle)
{
	//
	// Open a handle to the device interface information set of all 
	// present ndasport interfaces.
	//
	HRESULT hr;
	
	_ASSERT(NULL != DeviceHandle);

	if (NULL == DeviceHandle)
	{
		return E_POINTER;
	}

	*DeviceHandle = INVALID_HANDLE_VALUE;
	
	HDEVINFO deviceInfoSet = SetupDiGetClassDevs(
		&GUID_NDASPORT_INTERFACE_CLASS,
		NULL,
		NULL, 
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (INVALID_HANDLE_VALUE == deviceInfoSet)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiGetClassDevs failed, Error=%!WINERROR!\n", 
			hr);

		return hr;
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
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"SetupDiEnumDeviceInterfaces failed, Error=%!WINERROR!\n", 
			hr);

		SetupDiDestroyDeviceInfoList(deviceInfoSet);
		
		/* ERROR_NO_MORE_ITEMS */
		return hr;
	}

	HANDLE deviceHandle = NdasPortCtlOpenInterface(
		deviceInfoSet, 
		&deviceInterfaceData,
		DesiredAccess);

	if (INVALID_HANDLE_VALUE == deviceHandle)
	{
		hr = HRESULT_FROM_SETUPAPI(GetLastError());

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlOpenInterface failed, Error=%!WINERROR!\n", 
			hr);

		SetupDiDestroyDeviceInfoList(deviceInfoSet);

		return hr;
	}

	SetupDiDestroyDeviceInfoList(deviceInfoSet);
	
	*DeviceHandle = deviceHandle;
	
	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlGetPortNumber(
	__in HANDLE NdasPortHandle,
	__in PUCHAR PortNumber)
{
	HRESULT hr;
	DWORD portNumberInULong;
	DWORD bytesReturned;

	_ASSERTE(NULL != PortNumber);
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
		hr = HRESULT_FROM_WIN32(GetLastError());
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_GET_PORT_NUMBER failed, Error=%!WINERROR!\n", 
			hr);
		return hr;
	}

	*PortNumber = static_cast<UCHAR>(portNumberInULong);

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlGetLogicalUnitAddress(
	__in HANDLE LogicalUnitHandle,
	__out PNDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress)
{
	HRESULT hr;
	DWORD byteReturned;

	_ASSERTE(NULL != LogicalUnitAddress);

	BOOL success = DeviceIoControl(
		LogicalUnitHandle,
		IOCTL_NDASPORT_LOGICALUNIT_GET_ADDRESS,
		NULL, 0,
		LogicalUnitAddress, 
		sizeof(NDAS_LOGICALUNIT_ADDRESS),
		&byteReturned,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		return hr;
	}

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlPlugInLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in PNDAS_LOGICALUNIT_DESCRIPTOR Descriptor)
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
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_PLUGIN_LOGICALUNIT failed, Error=%!WINERROR!\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlEjectLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags)
{
	NDASPORT_LOGICALUNIT_EJECT ejectParam = {0};
	ejectParam.Size = sizeof(NDASPORT_LOGICALUNIT_EJECT);
	ejectParam.LogicalUnitAddress = Address;
	ejectParam.LogicalUnitAddress.PortNumber = 0;
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
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());		
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_EJECT_LOGICALUNIT failed, Error=%!WINERROR!\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlUnplugLogicalUnit(
	__in HANDLE NdasPortHandle,
	__in NDAS_LOGICALUNIT_ADDRESS Address,
	__in ULONG Flags)
{
	NDASPORT_LOGICALUNIT_UNPLUG unplugParam = {0};
	unplugParam.Size = sizeof(NDASPORT_LOGICALUNIT_EJECT);
	unplugParam.LogicalUnitAddress = Address;
	unplugParam.LogicalUnitAddress.PortNumber = 0;
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
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());		
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_UNPLUG_LOGICALUNIT failed, Error=%!WINERROR!\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
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
		HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"IOCTL_NDASPORT_IS_LOGICALUNIT_ADDRESS_IN_USE failed, Error=%!WINERROR!\n", hr);
		return hr;
	}

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlBuildNdasAtaDeviceDescriptor(
	__in NDAS_LOGICALUNIT_ADDRESS LogicalUnitAddress,
	__in ULONG LogicalUnitFlags,
	__in CONST NDAS_DEVICE_IDENTIFIER* DeviceIdentifier,
	__in ULONG NdasDeviceFlagMask,
	__in ULONG NdasDeviceFlags,
	__in ACCESS_MASK AccessMode,
	__in ULONG VirtualBytesPerBlock,
	__in PLARGE_INTEGER VirtualLogicalBlockAddress,
	__deref_out PNDAS_LOGICALUNIT_DESCRIPTOR* LogicalUnitDescriptor)
{

	_ASSERT(NULL != LogicalUnitDescriptor);
	if (NULL == LogicalUnitDescriptor)
	{
		return E_POINTER;
	}
	*LogicalUnitDescriptor = NULL;
	
	DWORD requiredBytes = sizeof(NDAS_ATA_DEVICE_DESCRIPTOR);
	
	PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor = 
		static_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(
			HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredBytes));

	if (NULL == ndasAtaDeviceDescriptor)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"Memory allocation failure for %d bytes\n", requiredBytes);
		return E_OUTOFMEMORY;
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

	const BYTE NDAS_DEVICE_PASSWORD_ANY[8] = { 
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	CopyMemory(
		ndasAtaDeviceDescriptor->DevicePassword,
		NDAS_DEVICE_PASSWORD_ANY,
		sizeof(ndasAtaDeviceDescriptor->DevicePassword));

	C_ASSERT(
		sizeof(NDAS_DEVICE_PASSWORD_ANY) == 
		sizeof(ndasAtaDeviceDescriptor->DevicePassword));

	ndasAtaDeviceDescriptor->VirtualBytesPerBlock = VirtualBytesPerBlock;
	ndasAtaDeviceDescriptor->VirtualLogicalBlockAddress = *VirtualLogicalBlockAddress;

	*LogicalUnitDescriptor = logicalUnitDescriptor;

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlSetNdasDiskEncryption(
	__deref_inout PNDAS_LOGICALUNIT_DESCRIPTOR* LogicalUnitDescriptor,
	__in CONST NDAS_DISK_ENCRYPTION_DESCRIPTOR* EncryptionDescriptor)
{
	_ASSERT(NULL != LogicalUnitDescriptor);
	if (NULL == LogicalUnitDescriptor)
	{
		return E_POINTER;
	}

	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == (*LogicalUnitDescriptor)->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != (*LogicalUnitDescriptor)->Version)
	{
		return E_INVALIDARG;
	}

	switch((*LogicalUnitDescriptor)->Type) 
	{
	case NdasAtaDevice: 
		{
			PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor = 
				reinterpret_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(*LogicalUnitDescriptor);

			if (0 == ndasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset)
			{
				DWORD descriptorLength = (*LogicalUnitDescriptor)->Size;
				DWORD newDescriptorLength = descriptorLength + sizeof(NDAS_DISK_ENCRYPTION_DESCRIPTOR);

				PVOID p = HeapReAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					ndasAtaDeviceDescriptor,
					newDescriptorLength);

				if (NULL == p)
				{
					CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
						"Memory allocation failure for %d bytes\n", newDescriptorLength);
					return E_OUTOFMEMORY;
				}

				ndasAtaDeviceDescriptor = static_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(p);
				ndasAtaDeviceDescriptor->Header.Size = newDescriptorLength;
				ndasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset = descriptorLength;
			}

			CopyMemory(
				ByteOffset<PVOID>(
					ndasAtaDeviceDescriptor, 
					ndasAtaDeviceDescriptor->NdasDiskEncryptionDescriptorOffset),
				EncryptionDescriptor,
				sizeof(NDAS_DISK_ENCRYPTION_DESCRIPTOR));

			*LogicalUnitDescriptor = 
				reinterpret_cast<PNDAS_LOGICALUNIT_DESCRIPTOR>(ndasAtaDeviceDescriptor);

			return S_OK;
		}
	case NdasExternalType: 
		{
			//
			// Convert NDAS_DISK_ENCRYPTION_DESCRIPTOR to the LUR descriptor's
			// built-in structure.
			//

			PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
				reinterpret_cast<PNDAS_DLU_DESCRIPTOR>(*LogicalUnitDescriptor);

			PLURELATION_DESC lurDesc = &ndasDluDeviceDescriptor->LurDesc;
			if (EncryptionDescriptor->EncryptKeyLength > NDAS_CONTENTENCRYPT_KEY_LENGTH)
			{
				return E_INVALIDARG;
			}
			
			lurDesc->CntEcrKeyLength = EncryptionDescriptor->EncryptKeyLength;

			switch(EncryptionDescriptor->EncryptType) 
			{
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
			
			CopyMemory(
				lurDesc->CntEcrKey, 
				EncryptionDescriptor->EncryptKey,
				lurDesc->CntEcrKeyLength);

			*LogicalUnitDescriptor = 
				reinterpret_cast<PNDAS_LOGICALUNIT_DESCRIPTOR>(ndasDluDeviceDescriptor);
			
			return S_OK;
		}
	default:
		_ASSERT(FALSE);
		return E_INVALIDARG;
	}

}

HRESULT
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
		return E_INVALIDARG;
	}

	switch(LogicalUnitDescriptor->Type) 
	{
	case NdasAtaDevice: 
		{
			return E_NOTIMPL;
		}
	case NdasExternalType: 
		{
			PLURELATION_NODE_DESC lurTargetNodeDesc = NdasPortCtlFindNodeDesc(
				LogicalUnitDescriptor, TargetNodeIndex);
			
			if (lurTargetNodeDesc == NULL)
			{
				return E_INVALIDARG;
			}

			//
			// Allow only for ATA/ATAPI devices.
			//

			if (lurTargetNodeDesc->LurnType == LURN_IDE_DISK ||
				lurTargetNodeDesc->LurnType == LURN_IDE_ODD ||
				lurTargetNodeDesc->LurnType == LURN_IDE_MO) 
			{
				//
				// TODO: incomplete
				//
				lurTargetNodeDesc->LurnIde.UserID = UserId;
				if (UserPassword && UserPasswordLength) 
				{
					return E_INVALIDARG;
				}
			} 
			else
			{
				return E_INVALIDARG;
			}
			return E_NOTIMPL;
		}
	default:
		_ASSERT(FALSE);
		return E_INVALIDARG;
	}
}

HRESULT
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
		return E_INVALIDARG;
	}

	switch(LogicalUnitDescriptor->Type) 
	{
	case NdasAtaDevice: 
		{
			PNDAS_ATA_DEVICE_DESCRIPTOR ndasAtaDeviceDescriptor;

			ndasAtaDeviceDescriptor = 
				reinterpret_cast<PNDAS_ATA_DEVICE_DESCRIPTOR>(LogicalUnitDescriptor);

			CopyMemory(
				ndasAtaDeviceDescriptor->DevicePassword,
				DeviceOemCode,
				DeviceOemCodeLength);

			return S_OK;
		}
	case NdasExternalType: 
		{
			PLURELATION_NODE_DESC lurTargetNodeDesc = NdasPortCtlFindNodeDesc(
				LogicalUnitDescriptor, TargetNodeIndex);
			
			if (lurTargetNodeDesc == NULL)
			{
				return E_INVALIDARG;
			}

			//
			// Allow only for ATA/ATAPI devices.
			//

			if (lurTargetNodeDesc->LurnType != LURN_IDE_DISK &&
				lurTargetNodeDesc->LurnType != LURN_IDE_ODD &&
				lurTargetNodeDesc->LurnType != LURN_IDE_MO)
			{
				return E_INVALIDARG;
			}
			
			CopyMemory(
				&lurTargetNodeDesc->LurnIde.Password,
				DeviceOemCode,
				DeviceOemCodeLength);

			return S_OK;
		}
	default:
		_ASSERT(FALSE);
		return E_INVALIDARG;
	}

}


HRESULT
WINAPI
NdasPortCtlGetNdasBacl(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR	LogicalUnitDescriptor,
	__out PNDAS_BLOCK_ACL* Bacl)
{
	_ASSERT(NULL != Bacl);
	if (NULL == Bacl)
	{
		return E_POINTER;
	}
	*Bacl = NULL;
	
	_ASSERT(sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) == LogicalUnitDescriptor->Version);
	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		return E_INVALIDARG;
	}

	switch(LogicalUnitDescriptor->Type) 
	{
	case NdasExternalType:
		{ 
			PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
				reinterpret_cast<PNDAS_DLU_DESCRIPTOR>(LogicalUnitDescriptor);

			PLURELATION_DESC lurDesc = &ndasDluDeviceDescriptor->LurDesc;

			if (lurDesc->BACLLength && lurDesc->BACLOffset) 
			{
				*Bacl = (PNDAS_BLOCK_ACL)((PBYTE)lurDesc + lurDesc->BACLOffset);
				return S_OK;
			}
			else 
			{
				return E_INVALIDARG;
			}
		}
	default:
		_ASSERT(FALSE);
		return E_INVALIDARG;
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
	__in DWORD						LogicalUnitFlags,
	__in NDAS_DEV_ACCESSMODE		NdasDevAccessMode,
	__in WORD						Reserved,
	__in PLARGE_INTEGER				LurVirtualLogicalBlockAddress,
	__in DWORD						LeafCount,
	__in WORD						LurNameLength,
	__in DWORD						NdasBaclLength,
	__in PNDASPORTCTL_NODE_INITDATA	RootNodeInitData
	)
{
	BOOL	bret;
	DWORD lurDescLengthWithoutBACL = SIZE_OF_LURELATION_DESC() +
		SIZE_OF_LURELATION_NODE_DESC(LeafCount) +
		SIZE_OF_LURELATION_NODE_DESC(0) * LeafCount;
	DWORD lurDescLength =
		lurDescLengthWithoutBACL +
		NdasBaclLength;
	DWORD requiredBytes = 
		FIELD_OFFSET(NDAS_DLU_DESCRIPTOR, LurDesc) +
		lurDescLength;

	PNDAS_DLU_DESCRIPTOR ndasDluDeviceDescriptor = 
		static_cast<PNDAS_DLU_DESCRIPTOR>(
			HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, requiredBytes));

	UNREFERENCED_PARAMETER(Reserved);

	if (NULL == ndasDluDeviceDescriptor)
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"Memory allocation failure for %d bytes\n", requiredBytes);
		SetLastError(ERROR_OUTOFMEMORY);
		return NULL;
	}
	if (RootNodeInitData == NULL) {
		if (LeafCount > 0) {
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
	ndasDluDeviceDescriptor->LurDesc.Length = (USHORT)lurDescLength;
	ndasDluDeviceDescriptor->LurDesc.Type = LUR_DESC_STRUCT_TYPE;
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
	// LUR name settings
	//

	ndasDluDeviceDescriptor->LurDesc.LurNameLength = LurNameLength;

	//
	// BACL settings
	//

	if (NdasBaclLength) {
		ndasDluDeviceDescriptor->LurDesc.BACLLength = NdasBaclLength;
		ndasDluDeviceDescriptor->LurDesc.BACLOffset = lurDescLengthWithoutBACL;
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
	if (LeafCount) {
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
			if (idx_child == LeafCount - 1)
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

	if (RootNodeInitData) {
		bret = NdasPortCtlSetupLurNode(
					lurRootNodeDesc,
					NdasDevAccessMode,
					RootNodeInitData);
		if (bret == FALSE) {
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
	if (TargetNodeIndex >= lurDesc->LurnDescCount)
		return NULL;

	targetNode = lurDesc->LurnDesc;
	for(idx_node = 0; idx_node < TargetNodeIndex; idx_node ++) {
		if (targetNode->NextOffset == 0)
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
	LurNodeDesc->LurnDeviceInterface = NodeInitData->NodeDeviceInterface;
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
	switch(NodeInitData->NodeType) 
	{
		case LURN_NDAS_AGGREGATION:
		case LURN_NDAS_RAID0:
		case LURN_NDAS_RAID1:
		case LURN_NDAS_RAID4:
		case LURN_NDAS_RAID5: {
			PNDASPORTCTL_INIT_RAID	initRaid = &NodeInitData->NodeSpecificData.Raid;
			PNDAS_RAID_INFO	infoRaid = &LurNodeDesc->LurnInfoRAID;
			//
			// Initialize RAID information
			//
			infoRaid->BlocksPerBit = initRaid->BlocksPerBit;
			infoRaid->SpareDiskCount =(UCHAR)initRaid->SpareDiskCount;
			CopyMemory(&infoRaid->NdasRaidId, &initRaid->NdasRaidId, sizeof(GUID));
			CopyMemory(&infoRaid->ConfigSetId, &initRaid->ConfigSetId, sizeof(GUID));
			break;
		}
		case LURN_DIRECT_ACCESS:
		case LURN_CDROM:
		case LURN_OPTICAL_MEMORY:
		case LURN_SEQUENTIAL_ACCESS:
		case LURN_PRINTER:
		case LURN_PROCCESSOR:
		case LURN_WRITE_ONCE:
		case LURN_SCANNER:
		case LURN_MEDIUM_CHANGER:
		case LURN_COMMUNICATIONS:
		case LURN_ARRAY_CONTROLLER:
		case LURN_ENCLOSURE_SERVICES:
		case LURN_REDUCED_BLOCK_COMMAND:
		case LURN_OPTIOCAL_CARD:
			{
			PNDASPORTCTL_INIT_ATADEV initAtaDev = &NodeInitData->NodeSpecificData.Ata;

			// Set the remote node address
			if (initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_TRANSPORT_PORTNO) 
			{
				NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
					&initAtaDev->DeviceIdentifier,
					(USHORT)initAtaDev->TransportPortNumber,
					&LurNodeDesc->LurnIde.TargetAddress);
			}
			else 
			{
				NdasPortCtlConvertDeviceIndentiferToLpxTaLsTransAddress(
					&initAtaDev->DeviceIdentifier,
					LPXRP_NDAS_PROTOCOL,
					&LurNodeDesc->LurnIde.TargetAddress);
			}

			if (initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_BINDING_ADDRESS) 
			{
				CopyMemory(
					&LurNodeDesc->LurnIde.BindingAddress,
					&initAtaDev->BindingAddress,
					sizeof(LurNodeDesc->LurnIde.BindingAddress));
			}
			LurNodeDesc->LurnIde.HWType = 0;	// ASIC
			LurNodeDesc->LurnIde.HWVersion = initAtaDev->HardwareVersion;
			LurNodeDesc->LurnIde.HWRevision = initAtaDev->HardwareRevision;
			LurNodeDesc->LurnIde.EndBlockAddrReserved = 0;
			if (initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_USERID) 
			{
				LurNodeDesc->LurnIde.UserID = initAtaDev->UserId;
			}
			else 
			{
				// Set default password
				if (initAtaDev->DeviceIdentifier.UnitNumber == 0) 
				{
					if (NdasDevAccessMode & NDASACCRIGHT_WRITE)
						LurNodeDesc->LurnIde.UserID = FIRST_TARGET_RW_USER;
					else
						LurNodeDesc->LurnIde.UserID = FIRST_TARGET_RO_USER;
				}
				else 
				{
					if (NdasDevAccessMode & NDASACCRIGHT_WRITE)
						LurNodeDesc->LurnIde.UserID = SECOND_TARGET_RW_USER;
					else
						LurNodeDesc->LurnIde.UserID = SECOND_TARGET_RO_USER;
				}
			}

			if (initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_USERPASSWORD) 
			{
				// Nothing to do currently.
			}

			if (initAtaDev->DeviceIdentifier.UnitNumber == 0) 
			{
				LurNodeDesc->LurnIde.LanscsiTargetID = 0;
				LurNodeDesc->LurnIde.LanscsiLU = 0;
			}
			else 
			{
				LurNodeDesc->LurnIde.LanscsiTargetID = 1;
				LurNodeDesc->LurnIde.LanscsiLU = 0;
			}

			if (initAtaDev->ValidFieldMask & NDASPORTCTL_ATAINIT_VALID_OEMCODE) 
			{
				CopyMemory(
					&LurNodeDesc->LurnIde.Password,
					initAtaDev->DeviceOemCode,
					sizeof(LurNodeDesc->LurnIde.Password));
			}
			else 
			{
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

FORCEINLINE
USHORT htons(__in USHORT hostshort)
{
	return (((hostshort&0x00FF) << 8) | ((hostshort & 0xFF00) >> 8));
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
	lpxAddress->Port = htons(LpxPortNumber);
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

		if (Value & mask)
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

		if (Value & mask)
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
	if (MemberCount == 0)
		return FALSE;
	switch(LurnType) {
		case LURN_NDAS_RAID0:
		{
			INT	leastSignificantBitOfMemberCount = NdasPortCtlFindLeastSignificantBit(MemberCount);

			// Make sure the number of members is power of two.
			if (	leastSignificantBitOfMemberCount
				!= NdasPortCtlFindMostSignificantBit(MemberCount)) {
					return FALSE;
			}
			// Make sure the input end address is times of MemberCount.
			// It avoid dropping last few blocks.
			if ( leastSignificantBitOfMemberCount > NdasPortCtlFindLeastSignificantBit(InputEndAddress + 1)) {
				return FALSE;
			}

			*RaidEndAddress = (InputEndAddress + 1)/ MemberCount - 1;
			break;
		}
		case LURN_NDAS_RAID1:
		case LURN_NDAS_RAID4:
		case LURN_NDAS_RAID5:
		{
			*RaidEndAddress = (InputEndAddress + 1)/ MemberCount - 1;
			break;
		}
		case LURN_NDAS_AGGREGATION:
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


HRESULT
NdasPortCtlSrbIoctl(
	__in HANDLE hTargetDevice,
	__in ULONG SrbIoctlCode,
	__inout PVOID Buffer,
	__in ULONG BufferLength)
{
	HRESULT hr;
	//
	// Make up IOCTL In-parameter
	//
	const DWORD cbHeader = sizeof(SRB_IO_CONTROL);
	DWORD cbInBuffer = cbHeader + BufferLength;
	PBYTE lpInBuffer;
	DWORD cbReturned = 0;
	PSRB_IO_CONTROL pSrbIoControl;

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

	BOOL success = DeviceIoControl(
		hTargetDevice,
		IOCTL_SCSI_MINIPORT,
		lpInBuffer,
		cbInBuffer,
		lpInBuffer, // we use the same buffer for output
		cbInBuffer,
		&cbReturned,
		NULL);

	if (!success)
	{
		hr = HRESULT_FROM_WIN32(GetLastError());

		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL, 
			"DeviceIoControl(IOCTL_SCSI_MINIPORT, %x) failed, hr=0x%X\n", 
			SrbIoctlCode, hr);

		//
		// copy the output data regardless of failure
		//
		RtlCopyMemory(Buffer, lpInBuffer + cbHeader, BufferLength);
		
		HeapFree(GetProcessHeap(), 0, lpInBuffer);

		return hr;
	}
	else
	{
		hr = S_OK;
	}

	RtlCopyMemory(Buffer, lpInBuffer + cbHeader, BufferLength);

	if (0 != pSrbIoControl->ReturnCode &&
		pSrbIoControl->ReturnCode != 1 /* SRB_STATUS_SUCCESS */ ) 
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL, 
			"DeviceIoControl (IOCTL_SCSI_MINIPORT, %x) failed, "
			"returnCode=0x%X, hr=0x%X\n",
			SrbIoctlCode, pSrbIoControl->ReturnCode, hr);

		HeapFree(GetProcessHeap(), 0, lpInBuffer);

		return E_FAIL;
	}

	HeapFree(GetProcessHeap(), 0, lpInBuffer);

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlQueryNodeAlive(
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress)
{
	HANDLE handle;

	HRESULT hr = NdasPortCtlCreateControlDevice(GENERIC_READ, &handle);
	if (FAILED(hr)) 
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlCreateControlDevice failed %!WINERROR!\n", hr);
		return hr;
	}

	hr = NdasPortCtlGetPortNumber(
		handle, 
		&NdasLogicalUnitAddress.PortNumber);

	if (FAILED(hr))
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlGetPortNumber failed %!WINERROR!\n", hr);
		CloseHandle(handle);
		return hr;
	}

	hr = NdasPortCtlIsLogicalUnitAddressInUse(handle, NdasLogicalUnitAddress);

	if (FAILED(hr))
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlIsLogicalUnitAddressInUse failed %!WINERROR!\n", hr);
		CloseHandle(handle);
		return hr;
	}

	CloseHandle(handle);
	return S_OK;
	
}

HRESULT
WINAPI
NdasPortCtlQueryInformation(
	__in HANDLE	DiskHandle,
	__in PNDASSCSI_QUERY_INFO_DATA Query,
	__in DWORD	QueryLength,
	__out PVOID	Information,
	__in DWORD	InformationLength)
{
	HRESULT hr;
	PVOID queryBuffer;
	DWORD queryBufferLength;

	if (Query == NULL ||
		QueryLength < FIELD_OFFSET(NDASSCSI_QUERY_INFO_DATA, QueryData))
	{
		return E_INVALIDARG;
	}

	queryBufferLength = (QueryLength > InformationLength) ? QueryLength : InformationLength;
	queryBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, queryBufferLength);

	if (NULL == queryBuffer)
	{
		return E_OUTOFMEMORY;
	}

	CopyMemory(queryBuffer, Query, QueryLength);

	hr = NdasPortCtlSrbIoctl(
		DiskHandle, 
		(ULONG)NDASSCSI_IOCTL_QUERYINFO_EX, 
		queryBuffer, 
		queryBufferLength);

	if (FAILED(hr))
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlSrbIoctl failed %!WINERROR!\n",
			hr);

		HeapFree(GetProcessHeap(), 0, queryBuffer);

		return hr;
	}
	
	CopyMemory(Information, queryBuffer, InformationLength);

	HeapFree(GetProcessHeap(), 0, queryBuffer);

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlQueryDeviceMode(
	__in HANDLE DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__out PULONG DeviceMode,
	__out PULONG SupportedFeatures,
	__out PULONG EnabledFeatures,
	__out PULONG ConnectionCount)
{
	HRESULT hr;
	NDASSCSI_QUERY_INFO_DATA query;
	NDSCIOCTL_PRIMUNITDISKINFO	primaryUnitDiskInformation;

	query.InfoClass = NdscPrimaryUnitDiskInformation;
	query.NdasScsiAddress.PathId = NdasLogicalUnitAddress.PathId;
	query.NdasScsiAddress.TargetId = NdasLogicalUnitAddress.TargetId;
	query.NdasScsiAddress.Lun = NdasLogicalUnitAddress.Lun;
	query.NdasScsiAddress.PortId = 0;
	query.Length = sizeof(NDASSCSI_QUERY_INFO_DATA);
	query.QueryDataLength = 0;

	hr = NdasPortCtlQueryInformation(
		DiskHandle,
		&query,
		sizeof(NDASSCSI_QUERY_INFO_DATA),
		&primaryUnitDiskInformation,
		sizeof(NDSCIOCTL_PRIMUNITDISKINFO));

	if (FAILED(hr))
	{
		CTRACE(TRACE_LEVEL_ERROR, NDASPORTCTL_GENERAL,
			"NdasPortCtlQueryInformation failed %!WINERROR!\n",
			hr);
		return hr;
	}

	if (DeviceMode)
	{
		*DeviceMode = primaryUnitDiskInformation.Lur.DeviceMode;
	}
	if (SupportedFeatures)
	{
		*SupportedFeatures = primaryUnitDiskInformation.Lur.SupportedFeatures;
	}
	if (EnabledFeatures)
	{
		*EnabledFeatures = primaryUnitDiskInformation.Lur.EnabledFeatures;
	}
	if(ConnectionCount)
	{
		*ConnectionCount = primaryUnitDiskInformation.UnitDisk.Connections;
	}

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlQueryLurFullInformation(
	__in HANDLE DiskHandle,
	__in NDAS_LOGICALUNIT_ADDRESS NdasLogicalUnitAddress,
	__deref_out PNDSCIOCTL_LURINFO * LurFullInfo) 
{
	HRESULT hr;
	NDASSCSI_QUERY_INFO_DATA query;
	LONG queryLen;
	NDSCIOCTL_LURINFO tmpLurFullInfo;
	PNDSCIOCTL_LURINFO lurFullInfo;
	LONG infoLen;

	_ASSERT(NULL != LurFullInfo);
	if (NULL == LurFullInfo)
	{
		return E_POINTER;
	}
	*LurFullInfo = NULL;

	queryLen = FIELD_OFFSET(NDASSCSI_QUERY_INFO_DATA, QueryData);
	query.Length = queryLen;
	query.InfoClass = NdscLurInformation;
	query.NdasScsiAddress.SlotNo = NdasLogicalUnitAddress.Address;
	query.QueryDataLength = 0;

	hr = NdasPortCtlQueryInformation(
		DiskHandle,
		&query,
		queryLen,
		&tmpLurFullInfo,
		sizeof(tmpLurFullInfo));

	if (FAILED(hr)) 
	{
		if (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) != hr)
		{
			return hr;
		}
	}

	infoLen = tmpLurFullInfo.Length;
	lurFullInfo = (PNDSCIOCTL_LURINFO)HeapAlloc(GetProcessHeap(), 0, infoLen);

	if (NULL == lurFullInfo) 
	{
		return E_OUTOFMEMORY;
	}

	hr = NdasPortCtlQueryInformation(
		DiskHandle,
		&query,
		queryLen,
		lurFullInfo,
		infoLen);

	if (FAILED(hr)) 
	{
		HeapFree(GetProcessHeap(), 0, lurFullInfo);
		return hr;
	}

	*LurFullInfo = lurFullInfo;

	return S_OK;
}

HRESULT
WINAPI
NdasPortCtlStartStopRegistrarEnum(
	__in BOOL bOnOff,
	__out LPBOOL pbPrevState)
{
	UNREFERENCED_PARAMETER(bOnOff);

	if (pbPrevState)
	{
		*pbPrevState = FALSE;
	}
	
	return S_OK;
}


