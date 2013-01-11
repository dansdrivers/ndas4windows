#pragma once

class CNdasUnit;

//////////////////////////////////////////////////////////////////////////
//
// Unit Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitImpl :
	public ILockImpl<INdasUnit>,
	public INdasUnitPnpSink
{
private:
	friend class CNdasUnitDeviceFactory;

	typedef CAutoLock<CNdasUnitImpl> CAutoInstanceLock;

public:

	HRESULT ImplInitialize(
		__in INdasDevice* pNdasDevice, 
		__in DWORD UnitNo,
		__in NDAS_UNITDEVICE_TYPE Type,
		__in NDAS_UNITDEVICE_SUBTYPE SubType,
		__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo,
		__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
		__in DWORD LuSequence);

	STDMETHODIMP get_ParentNdasDevice(__out INdasDevice** ppNdasDevice);
	
	STDMETHODIMP get_NdasUnitId(__out NDAS_UNITDEVICE_ID* NdasUnitId);
	STDMETHODIMP get_UnitNo(__out DWORD* UnitNo);

	STDMETHODIMP get_LogicalUnitSequence(__out DWORD* Sequence);
	STDMETHODIMP get_LogicalUnitDefinition(__out NDAS_LOGICALUNIT_DEFINITION* LuDefinition);

	STDMETHODIMP get_BlockAclSize(__in DWORD ElementsToSkip, __out DWORD* TotalSize);
	STDMETHODIMP FillBlockAcl(__in PVOID BlockAcl);

	STDMETHODIMP RegisterToLogicalUnitManager();
	STDMETHODIMP UnregisterFromLogicalUnitManager();

	STDMETHODIMP get_HardwareInfo(__out PNDAS_UNITDEVICE_HARDWARE_INFO HardwareInfo);
	STDMETHODIMP get_UnitStat(__out NDAS_UNITDEVICE_STAT* UnitStat);
	// access mask of the configuration 
	STDMETHODIMP get_GrantedAccess(__out ACCESS_MASK* Access);
	// access mask of the running configuration
	STDMETHODIMP get_AllowedAccess(__out ACCESS_MASK* Access);
	STDMETHODIMP get_NdasDevicePassword(__out UINT64* Password);
	STDMETHODIMP get_NdasDeviceUserId(__in ACCESS_MASK Access, __out DWORD* UserId);
	STDMETHODIMP get_Status(__out NDAS_UNITDEVICE_STATUS* Status);
	STDMETHODIMP get_Error(__out NDAS_UNITDEVICE_ERROR* Error);
	STDMETHODIMP get_Type(__out NDAS_UNITDEVICE_TYPE* Type);
	STDMETHODIMP get_SubType(__out NDAS_UNITDEVICE_SUBTYPE* SubType);

	STDMETHODIMP GetHostUsageCount(__out DWORD* ROHosts, __out DWORD* RWHosts, __in BOOL Update);
	STDMETHODIMP GetActualHostUsageCount(__out DWORD* ROHosts, __out DWORD* RWHosts, __in BOOL Update);

	STDMETHODIMP get_UserBlocks(__out UINT64 * Blocks);
	STDMETHODIMP get_PhysicalBlocks(__out UINT64 * Blocks);

	STDMETHODIMP CheckNdasfsCompatibility();
	STDMETHODIMP get_OptimalMaxTransferBlocks(__out DWORD * Blocks);

	STDMETHODIMP get_NdasLogicalUnit(__out INdasLogicalUnit** ppNdasLogicalUnit);

	STDMETHODIMP VerifyNdasLogicalUnitDefinition();
	STDMETHODIMP 
	GetRaidSimpleStatus (
		NDAS_LOGICALUNIT_DEFINITION *NdasLogicalUnitDefinition,
		UINT8						*NdasUnitNo,
		DWORD						*RaidSimpleStatusFlags
		);

	// INdasUnitDismountSink interface
	STDMETHOD_(void, DismountCompleted)();
	STDMETHOD_(void, MountCompleted)();

	static NDAS_UNITDEVICE_SUBTYPE CreateSubType(WORD t)
	{
		NDAS_UNITDEVICE_SUBTYPE subType = {t};
		return subType;
	}

protected:

	virtual void pSetStatus(NDAS_UNITDEVICE_STATUS newStatus);
	HRESULT ConnectUnitDevice(HNDAS *ndasHandle);
	HRESULT DisonnectUnitDevice(HNDAS ndasHandle);

protected:

	// weak reference to prevent circular reference
	INdasDevice* m_pParentNdasDevice;

	CComPtr<INdasLogicalUnit> m_pNdasLogicalUnit;

	NDAS_UNITDEVICE_ID m_unitDeviceId;
	NDAS_UNITDEVICE_TYPE m_type;
	NDAS_UNITDEVICE_SUBTYPE m_subType;
	NDAS_LOGICALUNIT_DEFINITION m_NdasLogicalUnitDefinition;
	DWORD m_NdasLogicalUnitSequence;

	NDAS_UNITDEVICE_HARDWARE_INFO m_udinfo;
	NDAS_UNITDEVICE_STATUS m_status;
	NDAS_UNITDEVICE_ERROR m_lastError;

	TCHAR m_szRegContainer[30];
};

//////////////////////////////////////////////////////////////////////////
//
// Unit Disk Device class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnit :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CNdasUnitImpl
{
	friend class CNdasUnitDeviceFactory;

public:

	void FinalRelease();

	BEGIN_COM_MAP(CNdasUnit)
		COM_INTERFACE_ENTRY(INdasUnit)
		COM_INTERFACE_ENTRY(INdasUnitPnpSink)
	END_COM_MAP()
};

class CNdasDiskUnit :
	public CComObjectRootEx<CComMultiThreadModel>,
	public CNdasUnitImpl,
	public INdasDiskUnit
{
private:

	typedef CAutoLock<CNdasDiskUnit> CAutoInstanceLock;
	friend class CNdasUnitDeviceFactory;

public:

	BEGIN_COM_MAP(CNdasDiskUnit)
		COM_INTERFACE_ENTRY(INdasUnit)
		COM_INTERFACE_ENTRY(INdasDiskUnit)
		COM_INTERFACE_ENTRY(INdasUnitPnpSink)
	END_COM_MAP()

	void FinalRelease();

	HRESULT UnitInitialize(
		__in INdasDevice* pNdasDevice, 
		__in DWORD UnitNo, 
		__in NDAS_DISK_UNIT_TYPE SubType, 
		__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo, 
		__in const NDAS_LOGICALUNIT_DEFINITION& NdasLogicalUnitDefinition,
		__in DWORD LuSequence,
		__in UINT64 UserBlocks,
		__in PVOID AddTargetInfo,
		__in const NDAS_CONTENT_ENCRYPT& Encryption,
		__in NDAS_DIB_V2* pDIBv2,
		__in BLOCK_ACCESS_CONTROL_LIST *pBACL);

	STDMETHODIMP get_Dib(__out NDAS_DIB_V2* Dibv2);
	STDMETHODIMP IsDibUnchanged();
	STDMETHODIMP IsBitmapClean();

	STDMETHODIMP get_BlockAclSize(__in DWORD ElementsToSkip, __out DWORD* TotalSize);
	STDMETHODIMP FillBlockAcl(__in PVOID BlockAcl);

	STDMETHODIMP get_RaidInfo(__out PVOID* Info);
	STDMETHODIMP get_ContentEncryption(__out NDAS_CONTENT_ENCRYPT* Encryption);
	STDMETHODIMP get_UserBlocks(__out UINT64 * Blocks);

protected:

	NDAS_DISK_UNIT_TYPE m_diskType;
	CHeapPtr<void> m_pNdasLogicalUnitRaidInfo;
	UINT64 m_ulUserBlocks;
	CHeapPtr<NDAS_DIB_V2> m_pDIBv2;
	CHeapPtr<BLOCK_ACCESS_CONTROL_LIST> m_pBACL;
	NDAS_CONTENT_ENCRYPT m_contentEncrypt;

};

const NDAS_UNITDEVICE_HARDWARE_INFO NDAS_UNITDEVICE_HARDWARE_INFO_NONE = {
	static_cast<DWORD>(sizeof(NDAS_UNITDEVICE_HARDWARE_INFO))
};

const NDAS_LOGICALUNIT_DEFINITION NDAS_NULL_LOGICALUNIT_DEFINITION = {
	sizeof(NDAS_LOGICALUNIT_DEFINITION),
	NDAS_LOGICALDEVICE_TYPE_UNKNOWN
}; 

const NDAS_UNITDEVICE_SUBTYPE NDAS_UNITDEVICE_SUBTYPE_NONE = {0};

class CNdasNullDiskUnit : public CNdasDiskUnit
{

public:

	//BEGIN_COM_MAP(CNdasNullDiskUnit)
	//	COM_INTERFACE_ENTRY_IID(IID_INdasDiskUnit, INdasDiskUnit)
	//END_COM_MAP()

	void FinalRelease();

	HRESULT Initialize(
		__in INdasDevice* pNdasDevice, 
		__in DWORD UnitNo,
		__in const NDAS_UNITDEVICE_HARDWARE_INFO& HardwareInfo,
		__in NDAS_UNITDEVICE_ERROR Error);

	STDMETHODIMP RegisterToLogicalUnitManager();
	STDMETHODIMP UnregisterFromLogicalUnitManager();
	STDMETHODIMP get_AllowedAccess(__out ACCESS_MASK* AccessMask)
	{
		ATLASSERT(FALSE);
		*AccessMask = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP get_UserBlocks(__out UINT64* Blocks)
	{
		ATLASSERT(FALSE);
		*Blocks = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP get_PhysicalBlocks(__out UINT64* Blocks)
	{
		ATLASSERT(FALSE);
		*Blocks = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP CheckNdasfsCompatibility() 
	{ 
		ATLASSERT(FALSE); 
		return E_NOTIMPL; 
	}
};

class CNdasNullUnit : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public CNdasUnitImpl
{
public:

	BEGIN_COM_MAP(CNdasNullUnit)
		COM_INTERFACE_ENTRY(INdasUnit)
	END_COM_MAP()

	void FinalRelease();

	HRESULT Initialize(__in INdasDevice* pNdasDevice, __in DWORD UnitNo);

	STDMETHODIMP RegisterToLogicalUnitManager();
	STDMETHODIMP UnregisterFromLogicalUnitManager();
	STDMETHODIMP get_AllowedAccess(__out ACCESS_MASK* AccessMask)
	{
		ATLASSERT(FALSE);
		*AccessMask = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP get_UserBlocks(__out UINT64* Blocks)
	{
		ATLASSERT(FALSE);
		*Blocks = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP get_PhysicalBlocks(__out UINT64* Blocks)
	{
		ATLASSERT(FALSE);
		*Blocks = 0;
		return E_NOTIMPL;
	}
	STDMETHODIMP CheckNdasfsCompatibility() 
	{ 
		ATLASSERT(FALSE); 
		return E_NOTIMPL; 
	}
};

