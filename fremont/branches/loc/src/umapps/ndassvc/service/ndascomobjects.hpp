#pragma once
#include <ndas/ndastypeex.h>
#include <ndas/ndasop.h>
#include <ndas/ndasdib.h>
#include <ndas/ndasid.h>
#include <ndas/ndascntenc.h>
#include <ndas/ndasportioctl.h>
#include <objbase.h>

interface INdasDevice;
interface INdasDeviceInternal;

interface INdasUnit;
interface INdasDiskUnit;

interface INdasLogicalUnit;
interface INdasLogicalUnitInternal;

interface INdasUnitPnpSink;
interface INdasDevicePnpSink;
interface INdasLogicalUnitPnpSink;
interface INdasTimerEventSink;

interface INdasDeviceCollection;

interface INdasDeviceRegistrar;
interface INdasDeviceRegistrarInternal;

interface INdasLogicalUnitManager;
interface INdasLogicalUnitManagerInternal;

typedef enum _NDAS_DEVICE_REGISTER_FLAGS {
	NDAS_REGFLAG_AUTOREGISTERED = 0x1,
	NDAS_REGFLAG_HIDDEN = 0x2,
	NDAS_REGFLAG_NON_PERSISTENT = 0x4
} NDAS_DEVICE_REGISTER_FLAGS;

typedef enum _NDAS_ENUM_FLAGS {
	NDAS_ENUM_DEFAULT = 0x0,
	NDAS_ENUM_EXCLUDE_HIDDEN = 0x01,
} NDAS_ENUM_FLAGS;

interface 
DECLSPEC_NOVTABLE
INativeLock
{
	STDMETHOD_(void,LockInstance)();
	STDMETHOD_(void,UnlockInstance)();
};

// DECLARE_INTERFACE_IID_(ILock, IUnknown, "{A6DE0149-B3C1-495f-87A8-8552CA4DBFFF}")
interface 
DECLSPEC_UUID("{A6DE0149-B3C1-495f-87A8-8552CA4DBFFF}") 
DECLSPEC_NOVTABLE 
ILock : public IUnknown
{
	STDMETHOD_(void,LockInstance)();
	STDMETHOD_(void,UnlockInstance)();
};

// DECLARE_INTERFACE_IID_(INdasLogicalUnit, IUnknown, "{636FD58C-A7ED-4f75-B6A7-D8FCB6004D35}")
interface 
DECLSPEC_UUID("{636FD58C-A7ED-4f75-B6A7-D8FCB6004D35}") 
DECLSPEC_NOVTABLE 
INdasLogicalUnit : public IUnknown
{
	STDMETHOD(get_Id)(__out NDAS_LOGICALDEVICE_ID* Id);
	STDMETHOD(get_Type)(__out NDAS_LOGICALDEVICE_TYPE * Type);
	STDMETHOD(get_LogicalUnitDefinition)(__out NDAS_LOGICALUNIT_DEFINITION* LogicalUnitDefinition);
	STDMETHOD(get_LogicalUnitAddress)(__out NDAS_LOGICALUNIT_ADDRESS* LogicalUnitAddress);

	STDMETHOD(get_Status)(__out NDAS_LOGICALDEVICE_STATUS* Status);
	STDMETHOD(get_Error)(__out NDAS_LOGICALDEVICE_ERROR* Error);

	STDMETHOD(PlugIn)(
		__in ACCESS_MASK Access, 
		__in DWORD LdpFlagMask, 
		__in DWORD LdpFlags, 
		__in DWORD PlugInFlags);

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
	STDMETHOD(get_Abnormalities)(__out DWORD * Abnormalities);

	STDMETHOD(GetSharedWriteInfo)(__out BOOL * SharedWrite, __out BOOL * PrimaryHost);
	STDMETHOD(SetRiskyMountFlag)(__in BOOL RiskyState);
	STDMETHOD(SetMountOnReady)(__in ACCESS_MASK access, __in BOOL fReducedMountOnReadyAccess /* = FALSE */);
};

// DECLARE_INTERFACE_IID_(INdasUnit,IUnknown,"{1695B284-528F-45a3-8DF6-4F8FAF465ACC}")
interface 
DECLSPEC_UUID("{1695B284-528F-45a3-8DF6-4F8FAF465ACC}") 
DECLSPEC_NOVTABLE 
INdasUnit : public IUnknown
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
	STDMETHOD(get_LogicalUnitDefinition)(__out NDAS_LOGICALUNIT_DEFINITION* LuDefinition);

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

	STDMETHOD(GetRaidSimpleStatus)(__out DWORD *RaidSimpleStatusFlags);

	STDMETHOD(VerifyNdasLogicalUnitDefinition)();
};

// DECLARE_INTERFACE_IID_(INdasDiskUnit, IUnknown, "{139A3FA1-BE5B-482d-94F4-E552BE9D5D03}")
interface 
DECLSPEC_UUID("{139A3FA1-BE5B-482d-94F4-E552BE9D5D03}") 
DECLSPEC_NOVTABLE 
INdasDiskUnit : public IUnknown
{
	STDMETHOD(get_Dib)(__out NDAS_DIB_V2* Dibv2);
	STDMETHOD(IsDibUnchanged)();
	STDMETHOD(IsBitmapClean)();

	STDMETHOD(get_ContentEncryption)(__out NDAS_CONTENT_ENCRYPT* Encryption);

	// RAID Information
	STDMETHOD(get_RaidInfo)(__out PVOID* Info);
};

// DECLARE_INTERFACE_IID_(INdasDevice, IUnknown, "{14112591-AB63-4009-BD71-0BE2D4713D08}")
interface
DECLSPEC_UUID("{14112591-AB63-4009-BD71-0BE2D4713D08}") 
DECLSPEC_NOVTABLE 
INdasDevice : public IUnknown
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

// DECLARE_INTERFACE_IID_(INdasDeviceInternal, INdasDevice, "{012F7787-B1A4-4ef3-B891-2A5AB9A37DED}")
interface
DECLSPEC_UUID("{012F7787-B1A4-4ef3-B891-2A5AB9A37DED}") 
DECLSPEC_NOVTABLE 
INdasDeviceInternal : public INdasDevice
{
};

// DECLARE_INTERFACE_IID_(INdasUnitPnpSink, IUnknown, "{248F3499-7577-44e5-8079-7648AAAFD494}")
interface
DECLSPEC_UUID("{248F3499-7577-44e5-8079-7648AAAFD494}") 
DECLSPEC_NOVTABLE 
INdasUnitPnpSink : public IUnknown
{
	STDMETHOD_(void, MountCompleted)();
	STDMETHOD_(void, DismountCompleted)();
};

// DECLARE_INTERFACE_IID_(INdasDevicePnpSink, IUnknown, "{5DCDC009-982E-4d40-95EA-6495E43AD9F8}")
interface
DECLSPEC_UUID("{5DCDC009-982E-4d40-95EA-6495E43AD9F8}") 
DECLSPEC_NOVTABLE 
INdasDevicePnpSink : public IUnknown
{
	// STDMETHOD_(void, OnUnitMountCompleted)(__in DWORD UnitNo);
	STDMETHOD_(void, UnitDismountCompleted)(__in INdasUnit* pNdasUnit);
};

// DECLARE_INTERFACE_IID_(INdasLogicalUnitPnpSink, IUnknown, "{3B17DF1F-1A05-4dec-9A41-7D4C46602380}")
interface
DECLSPEC_UUID("{3B17DF1F-1A05-4dec-9A41-7D4C46602380}") 
DECLSPEC_NOVTABLE 
INdasLogicalUnitPnpSink : public IUnknown
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

// DECLARE_INTERFACE_IID_(INdasTimerEventSink, IUnknown, "{58F3E3FB-B4C3-4f86-8575-BBB792C48F1F}")
interface
DECLSPEC_UUID("{58F3E3FB-B4C3-4f86-8575-BBB792C48F1F}") 
DECLSPEC_NOVTABLE 
INdasTimerEventSink : public IUnknown
{
	STDMETHOD_(void, OnTimer)();
};

// DECLARE_INTERFACE_IID_(INdasDeviceCollection, IUnknown, "{1EAB48FA-0CFE-461c-BD22-61A4471C2FF0}")
interface
DECLSPEC_UUID("{1EAB48FA-0CFE-461c-BD22-61A4471C2FF0}") 
DECLSPEC_NOVTABLE 
INdasDeviceCollection : public IUnknown
{

};

// DECLARE_INTERFACE_IID_(INdasDeviceRegistrar, IUnknown, "{0CB28BA9-9121-4b40-8C42-BFBA0D9D2743}")
interface
DECLSPEC_UUID("{0CB28BA9-9121-4b40-8C42-BFBA0D9D2743}") 
DECLSPEC_NOVTABLE 
INdasDeviceRegistrar : public IUnknown
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

// DECLARE_INTERFACE_IID_(INdasDeviceRegistrarInternal, INdasDeviceRegistrar, "{53EBC903-06FE-4bb5-9B71-71C0FFBBBF01}")
interface
DECLSPEC_UUID("{53EBC903-06FE-4bb5-9B71-71C0FFBBBF01}") 
DECLSPEC_NOVTABLE 
INdasDeviceRegistrarInternal : public INdasDeviceRegistrar
{
	STDMETHOD(Bootstrap)();
	STDMETHOD(Shutdown)();
};

// DECLARE_INTERFACE_IID_(INdasLogicalUnitManager, IUnknown, "{2E75E5DC-A036-4bd1-819C-95DEB7FA057A}")
interface
DECLSPEC_UUID("{2E75E5DC-A036-4bd1-819C-95DEB7FA057A}") 
DECLSPEC_NOVTABLE 
INdasLogicalUnitManager : public IUnknown
{
	STDMETHOD(Register)(INdasUnit* pNdasUnit, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHOD(Unregister)(INdasUnit* pNdasUnit);

	STDMETHOD(get_NdasLogicalUnit)(NDAS_LOGICALDEVICE_ID id, INdasLogicalUnit** ppNdasLogicalUnit);
	STDMETHOD(get_NdasLogicalUnit)(NDAS_LOGICALUNIT_DEFINITION* config, INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHOD(get_NdasLogicalUnits)(__in DWORD Flags /*= NDAS_ENUM_DEFAULT*/, __out CInterfaceArray<INdasLogicalUnit>& v);
};

// DECLARE_INTERFACE_IID_(INdasLogicalUnitManagerInternal, INdasLogicalUnitManager, "{23882EE6-8DD0-47d1-9564-EFD2695915E1}")
interface
DECLSPEC_UUID("{23882EE6-8DD0-47d1-9564-EFD2695915E1}") 
DECLSPEC_NOVTABLE 
INdasLogicalUnitManagerInternal : public INdasLogicalUnitManager
{
	STDMETHOD(OnSystemShutdown)();
	STDMETHOD(Cleanup)();
};

typedef struct _NDAS_DEVICE_HEARTBEAT_DATA NDAS_DEVICE_HEARTBEAT_DATA;
interface INdasHeartbeatSink;

// DECLARE_INTERFACE_IID(INdasHeartbeatSink, "{F23FFB3B-315A-4f4a-BC19-BB724CC03ED1}")
interface
DECLSPEC_UUID("{F23FFB3B-315A-4f4a-BC19-BB724CC03ED1}") 
DECLSPEC_NOVTABLE 
INdasHeartbeatSink
{
	STDMETHOD_(void, NdasHeartbeatReceived)(const NDAS_DEVICE_HEARTBEAT_DATA* Data);
};

interface INdasThreadedServer;

// DECLARE_INTERFACE_IID(INdasThreadedServer, "{957F0D56-10DD-431e-BAC2-2EF8D3B5DD30}")
interface
DECLSPEC_UUID("{957F0D56-10DD-431e-BAC2-2EF8D3B5DD30}") 
DECLSPEC_NOVTABLE 
INdasThreadedServer
{
	STDMETHOD(ThreadStart)();
};
