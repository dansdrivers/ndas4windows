#pragma once

class CNdasDeviceComm;

class CNdasDevice;
typedef CNdasDevice *PCNdasDevice;

class CNdasLogicalDevice;
typedef CNdasLogicalDevice *PCNdasLogicalDevice;

class CNdasUnitDevice;
typedef CNdasUnitDevice *PCNdasUnitDevice;

//////////////////////////////////////////////////////////////////////////
//
// Unit Device Information
//
//////////////////////////////////////////////////////////////////////////

typedef struct _NDAS_UNITDEVICE_INFORMATION {

	DWORD dwRWHosts;
	DWORD dwROHosts;

	BOOL bLBA;
	BOOL bLBA48;
	BOOL bPIO;
	BOOL bDMA;
	BOOL bUDMA;

	NDAS_UNITDEVICE_MEDIA_TYPE MediaType;

	unsigned _int64	SectorCount;

	TCHAR szModel[40];
	TCHAR szModelTerm;
	TCHAR szFwRev[8];
	TCHAR szFwRevTerm;
	TCHAR szSerialNo[20];
	TCHAR szSerialTerm;

} NDAS_UNITDEVICE_INFORMATION, *PNDAS_UNITDEVICE_INFORMATION;

//////////////////////////////////////////////////////////////////////////
//
// Unit Device Class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDevice
{
	friend class CNdasDevice;
	friend class CNdasLogicalDevice;

protected:

	//
	// An instance of the NDAS device containing 
	// this unit device
	//
	const PCNdasDevice m_pParentDevice;

	//
	// Unit number in a NDAS device
	//
	DWORD m_dwUnitNo;

	NDAS_UNITDEVICE_INFORMATION m_devInfo;

	NDAS_UNITDEVICE_STATUS m_status;
	NDAS_UNITDEVICE_ERROR m_lastError;

	NDAS_UNITDEVICE_PRIMARY_HOST_INFO m_PrimaryHostInfo;

	NDAS_UNITDEVICE_TYPE m_type;
	NDAS_UNITDEVICE_SUBTYPE m_subType;

protected:

	CNdasUnitDevice(
		PCNdasDevice pParentDevice, 
		DWORD dwUnitNo, 
		NDAS_UNITDEVICE_TYPE m_type,
		NDAS_UNITDEVICE_SUBTYPE m_subType,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);

	virtual VOID SetStatus(NDAS_UNITDEVICE_STATUS newStatus);
	virtual VOID SetHostUsageCount(DWORD nROHosts, DWORD nRWHosts);

public:


	PCNdasLogicalDevice CreateLogicalDevice();

#if 0
	static PCNdasUnitDevice CreateUnitDevice(
		PCNdasDevice pDevice, 
		DWORD dwUnitNo);
#endif

	NDAS_UNITDEVICE_ID GetUnitDeviceId();

	virtual NDAS_UNITDEVICE_TYPE GetType() 
	{ return m_type; }

	virtual NDAS_UNITDEVICE_SUBTYPE GetSubType()
	{ return m_subType; }

	virtual NDAS_UNITDEVICE_STATUS GetStatus();
	virtual NDAS_UNITDEVICE_ERROR GetLastError();

	virtual BOOL SetMountStatus(BOOL bMounted);

	virtual VOID GetUnitDevInfo(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);

	// access mask of the configuration 
	virtual ACCESS_MASK GetGrantedAccess();

	// access mask of the running configuration
	virtual ACCESS_MASK GetAllowingAccess();

	virtual PCNdasDevice GetParentDevice();
	virtual DWORD GetUnitNo();

	virtual BOOL GetHostUsageCount(LPDWORD lpnROHosts, LPDWORD lpnRWHosts, BOOL bUpdate = FALSE);

//	virtual BOOL UpdateDeviceInfo() = 0;

	virtual BOOL RegisterToLDM() = 0;

	virtual VOID UpdatePrimaryHostInfo(
		PNDAS_UNITDEVICE_PRIMARY_HOST_INFO pPrimaryHostInfo);

	LPCTSTR ToString();

private:
	// hide copy constructor
	CNdasUnitDevice(const CNdasUnitDevice &);
	// hide assignment operator
	CNdasUnitDevice& operator = (const CNdasUnitDevice&);

public:

	//
	// ToString buffer
	//
	// Should be enough to hold CNdasDevice::ToString()
	static const size_t CCH_STR_BUF = 30;
protected:
	TCHAR m_szStrBuf[CCH_STR_BUF];

};

//////////////////////////////////////////////////////////////////////////
//
// Unit Disk Device class
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDiskDevice;
typedef CNdasUnitDiskDevice *PCNdasUnitDiskDevice;

class CNdasUnitDiskDevice :
	public CNdasUnitDevice
{
protected:

	//
	// Unit disk type
	//
	NDAS_UNITDEVICE_DISK_TYPE m_diskType;

	PVOID m_pAddTargetInfo;

	//
	// Aggregated or Mirrored Member information
	//
	DWORD m_nAssocUnitDevices;
	DWORD m_dwAssocSequence;
	PNDAS_UNITDEVICE_ID m_pAssocUnitDevices;
	ULONG m_ulUserBlocks;

	// virtual VOID ProcessDiskInfoBlock_(PNDAS_DIB pDib);
	virtual VOID ProcessDiskInfoBlock_(PNDAS_DIB_V2 pDib);

public:

#if 0
	//
	// Creator
	//
	static
	PCNdasUnitDiskDevice
	CreateUnitDiskDevice(
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);

	static
	PCNdasUnitDiskDevice
	CreateUnitDiskDevice(
		CNdasDeviceComm& devComm,
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);
#endif

	//
	// Constructor
	//
	CNdasUnitDiskDevice(
		PCNdasDevice pParentDevice, 
		DWORD dwUnitNo, 
		NDAS_UNITDEVICE_DISK_TYPE diskType,
		DWORD dwAssocSequence,
		DWORD nAssocUnitDevices,
		PNDAS_UNITDEVICE_ID pAssocUnitDevices,
		ULONG ulUserBlocks,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo,
		PVOID pAddTargetInfo);

#if 0
	CNdasUnitDiskDevice(
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		NDAS_UNITDEVICE_DISK_TYPE diskType,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo,
		PNDAS_DIB_V2 pDiskInfo);
#endif

	//
	// Destructor
	//
	~CNdasUnitDiskDevice();

	// ULONG GetBlocks();
	ULONG GetUserBlockCount();
	ULONG GetPhysicalBlockCount();
	PVOID GetAddTargetInfo();

//	virtual BOOL UpdateDeviceInfo();

	virtual BOOL RegisterToLDM();

private:
	// hide copy constructor
	CNdasUnitDiskDevice(const CNdasUnitDevice &);
	// hide assignment operator
	CNdasUnitDiskDevice& operator = (const CNdasUnitDiskDevice&);

protected:

#if 0
	static BOOL ReadDIB(
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		PNDAS_DIB_V2* ppDIB_V2,
		CNdasDeviceComm &devComm);

	static BOOL ReadDIBv1AndConvert(
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		PNDAS_DIB_V2 pDIBv2,
		CNdasDeviceComm &devComm);

	static BOOL ConvertDIBv1toDIBv2(
		const NDAS_DIB* pDIBv1, 
		PNDAS_DIB_V2 pDIBv2,
		UINT64 nDiskSectorCount);

	static VOID InitializeDIBv2(
		PNDAS_DIB_V2 pDIB_V2, 
		UINT64 nDiskSectorCount);
#endif

};

//////////////////////////////////////////////////////////////////////////
//
// Unit DVD Device class (incomplete)
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitCDROMDevice;
typedef CNdasUnitCDROMDevice *PCNdasUnitCDROMDevice;

class CNdasUnitCDROMDevice :
	public CNdasUnitDevice
{

public:

	//
	// Constructor
	//
	CNdasUnitCDROMDevice(
		PCNdasDevice pParentDevice,
		DWORD dwUnitNo,
		NDAS_UNITDEVICE_CDROM_TYPE cdromType,
		PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);

	//
	// Destructor
	//
	~CNdasUnitCDROMDevice();

private:
	// hide copy constructor
	CNdasUnitCDROMDevice(const CNdasUnitCDROMDevice&);
	// hide assignment operator
	CNdasUnitCDROMDevice& operator = (const CNdasUnitCDROMDevice&);
};

/*++

A utility class for processing communications 
between the host and NDAS devices.

--*/

class CNdasDeviceComm
{
protected:

	BOOL m_bInitialized;
	BOOL m_bWriteAccess;
	const PCNdasDevice m_pDevice;
	const DWORD m_dwUnitNo;

	LANSCSI_PATH m_lspath;

	INT32 GetUserId();
	VOID InitializeLANSCSIPath();

public:

	explicit CNdasDeviceComm(PCNdasDevice pDevice, DWORD dwUnitNo);
	virtual ~CNdasDeviceComm();

	BOOL Initialize(BOOL bWriteAccess = FALSE);
	BOOL Cleanup();
	BOOL GetUnitDeviceInformation(PNDAS_UNITDEVICE_INFORMATION pUnitDevInfo);
	BOOL GetDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	BOOL ReadDiskBlock(PBYTE pBlockBuffer, INT64 i64DiskBlock);
	BOOL WriteDiskInfoBlock(PNDAS_DIB pDiskInfoBlock);
	UINT64 GetDiskSectorCount();

};

//////////////////////////////////////////////////////////////////////////
//
// NDAS Unit Device Instance Creator
//
//////////////////////////////////////////////////////////////////////////

class CNdasUnitDeviceCreator
{
	PCNdasDevice m_pDevice;
	DWORD m_dwUnitNo;
	CNdasDeviceComm m_devComm;
	NDAS_UNITDEVICE_INFORMATION m_unitDevInfo;

public:

	CNdasUnitDeviceCreator(PCNdasDevice pDevice, DWORD dwUnitNo);
	PCNdasUnitDevice CreateUnitDevice();

protected:

	PCNdasUnitDiskDevice CreateUnitDiskDevice();

	BOOL ReadDIB(PNDAS_DIB_V2* ppDIB_V2);
	BOOL ReadDIBv1AndConvert(PNDAS_DIB_V2 pDIBv2);

	static BOOL ConvertDIBv1toDIBv2(
		const NDAS_DIB* pDIBv1, 
		PNDAS_DIB_V2 pDIBv2,
		UINT64 nDiskSectorCount);

	static VOID InitializeDIBv2(
		PNDAS_DIB_V2 pDIB_V2, 
		UINT64 nDiskSectorCount);

};
