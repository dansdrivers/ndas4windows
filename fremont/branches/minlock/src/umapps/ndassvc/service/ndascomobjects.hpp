#pragma once
#include <ndas/ndastypeex.h>
#include <ndas/ndasop.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasid.h>
#include <ndas/ndascntenc.h>
#include <ndas/ndasportioctl.h>
#include <objbase.h>

struct INdasDevice;
struct INdasUnit;
struct INdasDiskUnit;
struct INdasLogicalUnit;
struct INdasDeviceInternal;

struct INdasUnitPnpSink;
struct INdasDevicePnpSink;
struct INdasLogicalUnitPnpSink;
struct INdasTimerEventSink;

struct INdasDeviceCollection;
struct INdasDeviceRegistrar;
struct INdasDeviceRegistrarInternal;

struct INdasLogicalUnitManager;
struct INdasLogicalUnitManagerInternal;

typedef enum _NDAS_DEVICE_REGISTER_FLAGS {
	NDAS_REGFLAG_AUTOREGISTERED = 0x1,
	NDAS_REGFLAG_HIDDEN = 0x2,
	NDAS_REGFLAG_NON_PERSISTENT = 0x4
} NDAS_DEVICE_REGISTER_FLAGS;

typedef enum _NDAS_ENUM_FLAGS {
	NDAS_ENUM_DEFAULT = 0x0,
	NDAS_ENUM_EXCLUDE_HIDDEN = 0x01,
} NDAS_ENUM_FLAGS;

struct INativeLock
{
	STDMETHOD_(void,LockInstance)() = 0;
	STDMETHOD_(void,UnlockInstance)() = 0;
};

DECLARE_INTERFACE_IID_(ILock, IUnknown, "{A6DE0149-B3C1-495f-87A8-8552CA4DBFFF}")
{
	STDMETHOD_(void,LockInstance)();
	STDMETHOD_(void,UnlockInstance)();
};

DECLARE_INTERFACE_IID_(INdasLogicalUnit, IUnknown, "{636FD58C-A7ED-4f75-B6A7-D8FCB6004D35}")
{
	STDMETHOD(get_Id)(__out NDAS_LOGICALDEVICE_ID* Id);
	STDMETHOD(get_Type)(__out NDAS_LOGICALDEVICE_TYPE * Type);
	STDMETHOD(get_LogicalUnitDefinition)(__out NDAS_LOGICALDEVICE_GROUP* LogicalUnitDefinition);
	STDMETHOD(get_NdasLocation)(__out NDAS_LOCATION* NdasLocation);
	STDMETHOD(get_LogicalUnitAddress)(__out NDAS_LOGICALUNIT_ADDRESS* LogicalUnitAddress);

	STDMETHOD(get_Status)(__out NDAS_LOGICALDEVICE_STATUS* Status);
	STDMETHOD(get_Error)(__out NDAS_LOGICALDEVICE_ERROR* Error);

	STDMETHOD(PlugIn)(__in ACCESS_MASK Access, DWORD FlagMask, DWORD Flags);
	STDMETHOD(Eject)();
	STDMETHOD(EjectEx)(
		__out CONFIGRET* ConfigRet, 
		__out PNP_VETO_TYPE* VetoType,
		__out BSTR* VetoName);
	STDMETHOD(Unplug)();


	STDMETHOD(get_GrantedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_AllowedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_MountedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_MountedDriveSet)(__out DWORD* DriveSet);

	STDMETHOD(get_UserBlocks)(__out UINT64* Blocks);
	STDMETHOD(get_DevicePath)(__deref_out BSTR* DevicePath);

	STDMETHOD(get_ContentEncryption)(__out NDAS_CONTENT_ENCRYPT* Encryption);

	STDMETHOD(AddNdasUnitInstance)(__in INdasUnit* pNdasUnit);
	STDMETHOD(RemoveNdasUnitInstance)(__in INdasUnit* pNdasUnit);

	STDMETHOD(get_AdapterStatus)(__out ULONG * AdapterStatus);
	STDMETHOD(put_AdapterStatus)(__out ULONG AdapterStatus);

	STDMETHOD(get_DisconnectEvent)(__out HANDLE* EventHandle);
	STDMETHOD(get_AlarmEvent)(__out HANDLE* EventHandle);

	STDMETHOD(get_NdasUnitInstanceCount)(__out DWORD* UnitCount);
	STDMETHOD(get_NdasUnitCount)(__out DWORD* UnitCount);
	STDMETHOD(get_NdasUnitId)(__in DWORD Sequence, __out NDAS_UNITDEVICE_ID* NdasUnitId);

	STDMETHOD(get_IsHidden)(__out BOOL * Hidden);

	STDMETHOD(GetSharedWriteInfo)(__out BOOL * SharedWrite, __out BOOL * PrimaryHost);
	STDMETHOD(SetRiskyMountFlag)(__in BOOL RiskyState);
	STDMETHOD(SetMountOnReady)(__in ACCESS_MASK access, __in BOOL fReducedMountOnReadyAccess /* = FALSE */);
};

DECLARE_INTERFACE_IID_(INdasUnit,IUnknown,"{1695B284-528F-45a3-8DF6-4F8FAF465ACC}")
{
	STDMETHOD(get_ParentNdasDevice)(__out INdasDevice** ppNdasDevice);

	STDMETHOD(get_NdasLogicalUnit)(__out INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHOD(get_NdasUnitId)(__out NDAS_UNITDEVICE_ID* NdasUnitId);
	STDMETHOD(get_UnitNo)(__out DWORD* UnitNo);
	STDMETHOD(get_GrantedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_AllowedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_HardwareInfo)(__out PNDAS_UNITDEVICE_HARDWARE_INFO HardwareInfo);
	STDMETHOD(get_UnitStat)(__out NDAS_UNITDEVICE_STAT* UnitStat);
	STDMETHOD(get_NdasDevicePassword)(__out UINT64* Password);
	STDMETHOD(get_NdasDeviceUserId)(__in ACCESS_MASK Access, __out DWORD* UserId);
	STDMETHOD(get_Status)(__out NDAS_UNITDEVICE_STATUS* Status);
	STDMETHOD(get_Error)(__out NDAS_UNITDEVICE_ERROR* Error);
	STDMETHOD(get_Type)(__out NDAS_UNITDEVICE_TYPE* Type);
	STDMETHOD(get_SubType)(__out NDAS_UNITDEVICE_SUBTYPE* SubType);

	STDMETHOD(get_LogicalUnitSequence)(__out DWORD* Sequence);
	STDMETHOD(get_LogicalUnitDefinition)(__out NDAS_LOGICALDEVICE_GROUP* LuDefinition);

	STDMETHOD(get_BlockAclSize)(__in DWORD SkipLength, __out DWORD* TotalSize);
	STDMETHOD(FillBlockAcl)(__in PVOID BlockAcl);

	STDMETHOD(get_UserBlocks)(__out UINT64 * Blocks);
	STDMETHOD(get_PhysicalBlocks)(__out UINT64 * Blocks);
	STDMETHOD(get_OptimalMaxTransferBlocks)(__out DWORD * Blocks);

	STDMETHOD(RegisterToLogicalUnitManager)();
	STDMETHOD(UnregisterFromLogicalUnitManager)();

	STDMETHOD(CheckNDFSCompatibility)();
	STDMETHOD(GetHostUsageCount)(__out DWORD * ROHosts, __out DWORD * RWHosts, __in BOOL Update /*= FALSE */);
	STDMETHOD(GetActualHostUsageCount)(__out DWORD * ROHosts, __out DWORD * RWHosts, __in BOOL Update /* = FALSE */);

};

DECLARE_INTERFACE_IID_(INdasDiskUnit, IUnknown, "{139A3FA1-BE5B-482d-94F4-E552BE9D5D03}")
{
	STDMETHOD(get_Dib)(__out NDAS_DIB_V2* Dibv2);
	STDMETHOD(IsDibUnchanged)();
	STDMETHOD(IsBitmapClean)();

	STDMETHOD(get_ContentEncryption)(__out NDAS_CONTENT_ENCRYPT* Encryption);

	// RAID Information
	STDMETHOD(get_RaidInfo)(__out PVOID* Info);
};

DECLARE_INTERFACE_IID_(INdasDevice, IUnknown, "{14112591-AB63-4009-BD71-0BE2D4713D08}")
{
	STDMETHOD(get_NdasDeviceId)(__out NDAS_DEVICE_ID* NdasDeviceId);
	STDMETHOD(get_SlotNo)(__out DWORD* SlotNo);
	STDMETHOD(put_Name)(__in BSTR Name);
	STDMETHOD(get_Name)(__out BSTR* Name);
	STDMETHOD(get_Status)(__out NDAS_DEVICE_STATUS* Status);
	STDMETHOD(get_DeviceError)(__out NDAS_DEVICE_ERROR* Error);
	STDMETHOD(put_GrantedAccess)(__in ACCESS_MASK access);
	STDMETHOD(get_GrantedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_AllowedAccess)(__out ACCESS_MASK* Access);
	STDMETHOD(get_HardwareType)(__out DWORD* HardwareType);
	STDMETHOD(get_HardwareVersion)(__out DWORD* HardwareVersion);
	STDMETHOD(get_HardwareRevision)(__out DWORD* HardwareRevision);
	STDMETHOD(get_HardwarePassword)(__out UINT64* HardwarePassword);
	STDMETHOD(put_OemCode)(__in const NDAS_OEM_CODE* OemCode);
	STDMETHOD(get_OemCode)(__out NDAS_OEM_CODE* OemCode);
	STDMETHOD(get_HardwareInfo)(__out NDAS_DEVICE_HARDWARE_INFO* HardwareInfo);
	STDMETHOD(get_DeviceStat)(__out NDAS_DEVICE_STAT* DeviceStat);
	STDMETHOD(get_NdasIdExtension)(__out NDASID_EXT_DATA* IdExtension);
	STDMETHOD(get_MaxTransferBlocks)(__out LPDWORD MaxTransferBlocks);
	STDMETHOD(put_Enabled)(__in BOOL Enabled);
	STDMETHOD(get_RemoteAddress)(__inout SOCKET_ADDRESS * SocketAddress);
	STDMETHOD(get_LocalAddress)(__inout SOCKET_ADDRESS * SocketAddress);
	STDMETHOD(get_RegisterFlags)(__in DWORD* Flags);

	STDMETHOD(UpdateStats)();
	STDMETHOD(InvalidateNdasUnit)(INdasUnit* pNdasUnit);

	//
	// TODO: change to COM-compatible collections
	//
	STDMETHOD(get_NdasUnits)(__inout CInterfaceArray<INdasUnit> & NdasUnits);
	STDMETHOD(get_NdasUnit)(__in DWORD UnitNo, __deref_out INdasUnit** ppNdasUnit);
};

DECLARE_INTERFACE_IID_(INdasDeviceInternal, INdasDevice, "{012F7787-B1A4-4ef3-B891-2A5AB9A37DED}")
{
};

DECLARE_INTERFACE_IID_(INdasUnitPnpSink, IUnknown, "{248F3499-7577-44e5-8079-7648AAAFD494}")
{
	STDMETHOD_(void, MountCompleted)();
	STDMETHOD_(void, DismountCompleted)();
};

DECLARE_INTERFACE_IID_(INdasDevicePnpSink, IUnknown, "{5DCDC009-982E-4d40-95EA-6495E43AD9F8}")
{
	// STDMETHOD_(void, OnUnitMountCompleted)(__in DWORD UnitNo);
	STDMETHOD_(void, UnitDismountCompleted)(__in INdasUnit* pNdasUnit);
};

DECLARE_INTERFACE_IID_(INdasLogicalUnitPnpSink, IUnknown, "{3B17DF1F-1A05-4dec-9A41-7D4C46602380}")
{
	STDMETHOD_(void, OnMounted)(
		__in BSTR DevicePath, 
		__in NDAS_LOGICALUNIT_ABNORMALITIES Abnormalities);
	STDMETHOD_(void, OnDismounted)();
	STDMETHOD_(void, OnDismountFailed)();
	STDMETHOD_(void, OnSystemShutdown)();
	STDMETHOD_(void, OnDisconnected)();
	STDMETHOD_(void, OnMountedDriveSetChanged)(
		__in DWORD RemoveBits, 
		__in DWORD AddBits);
};

DECLARE_INTERFACE_IID_(INdasTimerEventSink, IUnknown, "{58F3E3FB-B4C3-4f86-8575-BBB792C48F1F}")
{
	STDMETHOD_(void, OnTimer)();
};

DECLARE_INTERFACE_IID_(INdasDeviceCollection, IUnknown, "{1EAB48FA-0CFE-461c-BD22-61A4471C2FF0}")
{

};

DECLARE_INTERFACE_IID_(INdasDeviceRegistrar, IUnknown, "{0CB28BA9-9121-4b40-8C42-BFBA0D9D2743}")
{
	STDMETHOD(Register)(
		__in_opt DWORD SlotNo,
		__in const NDAS_DEVICE_ID& DeviceId,
		__in DWORD Flags,
		__in_opt const NDASID_EXT_DATA* NdasIdExtension,
		__in BSTR Name,
		__in ACCESS_MASK GrantedAccess,
		__in_opt const NDAS_OEM_CODE* NdasOemCode,
		__deref_out INdasDevice** ppNdasDevice);
	STDMETHOD(Deregister)(__in INdasDevice* pNdasDevice);
	STDMETHOD(get_NdasDevice)(__in NDAS_DEVICE_ID* DeviceId, __deref_out INdasDevice** ppNdasDevice);
	STDMETHOD(get_NdasDevice)(__in DWORD SlotNo, __deref_out INdasDevice** ppNdasDevice);
	STDMETHOD(get_NdasDevice)(__in NDAS_DEVICE_ID_EX* DeviceId, __deref_out INdasDevice** ppNdasDevice);

	STDMETHOD(get_NdasDevices)(__in DWORD Flags, __deref_out CInterfaceArray<INdasDevice>& NdasDevices);
};

DECLARE_INTERFACE_IID_(INdasDeviceRegistrarInternal, INdasDeviceRegistrar, "{53EBC903-06FE-4bb5-9B71-71C0FFBBBF01}")
{
	STDMETHOD(Bootstrap)();
	STDMETHOD(Shutdown)();
};

DECLARE_INTERFACE_IID_(INdasLogicalUnitManager, IUnknown, "{2E75E5DC-A036-4bd1-819C-95DEB7FA057A}")
{
	STDMETHOD(Register)(INdasUnit* pNdasUnit, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHOD(Unregister)(INdasUnit* pNdasUnit);

	STDMETHOD(get_NdasLogicalUnit)(NDAS_LOGICALDEVICE_ID id, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHOD(get_NdasLogicalUnit)(NDAS_LOGICALDEVICE_GROUP* config, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHOD(get_NdasLogicalUnitByNdasLocation)(NDAS_LOCATION location, INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHOD(get_NdasLogicalUnits)(__in DWORD Flags /*= NDAS_ENUM_DEFAULT*/, __out CInterfaceArray<INdasLogicalUnit>& v);
};

DECLARE_INTERFACE_IID_(INdasLogicalUnitManagerInternal, INdasLogicalUnitManager, "{23882EE6-8DD0-47d1-9564-EFD2695915E1}")
{
	STDMETHOD(RegisterNdasLocation)(NDAS_LOCATION location, INdasLogicalUnit* logicalDevice);
	STDMETHOD(UnregisterNdasLocation)(NDAS_LOCATION location, INdasLogicalUnit* logicalDevice);

	STDMETHOD(OnSystemShutdown)();
	STDMETHOD(Cleanup)();
};

typedef struct _NDAS_DEVICE_HEARTBEAT_DATA NDAS_DEVICE_HEARTBEAT_DATA;
struct INdasHeartbeatSink;

DECLARE_INTERFACE_IID(INdasHeartbeatSink, "{F23FFB3B-315A-4f4a-BC19-BB724CC03ED1}")
{
	STDMETHOD_(void, NdasHeartbeatReceived)(const NDAS_DEVICE_HEARTBEAT_DATA* Data);
};

