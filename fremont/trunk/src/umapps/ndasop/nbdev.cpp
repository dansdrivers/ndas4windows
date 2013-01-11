// nbdev.cpp : implementation of the CNBNdasDev class
//
/////////////////////////////////////////////////////////////////////////////

// revised by William Kim 24/July/2008

#include "stdafx.h"
#include "resource.h"

#include <ndas/ndascomm.h>
#include <ndas/ndasuser.h>
#include <ndas/ndasop.h>
#include <ndas/ndasid.h>

#include <ndas/nbdev.h>


LONG DbgLevelNdasDev = DBG_LEVEL_NDAS_DEV;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelNdasDev) {								\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

const NDASID_EXT_DATA NDAS_ID_EXTENSION_DEFAULT = { 0xCD, NDAS_VID_DEFAULT, 0xFF, 0xFF };

static 
CString pGetMenuString(UINT MenuId)
{
	CMenu menu;
	CString menuString;
	menu.LoadMenu(MAKEINTRESOURCE(IDR_MAINPOPUP));
	menu.GetMenuString(MenuId, menuString, MF_BYCOMMAND);
	menuString.Remove(_T('&'));
	menu.DestroyMenu();
	return menuString;
}

CNBNdasDev::CNBNdasDev (
	LPCTSTR				DeviceName, 
	PNDAS_DEVICE_ID		pDeviceId, 
	NDAS_DEVICE_STATUS	status, 
	ACCESS_MASK			GrantedAccess //= GENERIC_READ | GENERIC_WRITE
	)
{
	m_strDeviceName = DeviceName;
	m_DeviceId		= *pDeviceId;
	m_GrantedAccess = GrantedAccess;
	m_Status		= status;

	NdasUiDbgCall( 1, _T("new CNBNdasDev(%p) : Name(%s), Device(%02X:%02X:%02X:%02X:%02X:%02X), Vid(%02X), status(%08X), Access(%08X)\n"),
						 this, m_strDeviceName,
						 m_DeviceId.Node[0], m_DeviceId.Node[1], m_DeviceId.Node[2], 
						 m_DeviceId.Node[3], m_DeviceId.Node[4], m_DeviceId.Node[5],
						 m_DeviceId.Vid, m_Status, m_GrantedAccess );
}

CNBNdasDev::~CNBNdasDev (VOID)
{
	ClearUnitDevices();
}

UINT8 CNBNdasDev::AddUnitDevices (UINT8 UnitCount /* = 0*/)
{
	UINT8 unit;

	if (m_Status == NDAS_DEVICE_STATUS_NOT_REGISTERED) {

		ATLASSERT(UnitCount);

		for (UINT8 unit = 0; unit < UnitCount; unit++) {

			NdasUiDbgCall( 4, _T("pNdasDevice (%s) is dead add missing disks %d\n"), m_strDeviceName, unit );

			AddUnitDevice( unit, NULL, NDAS_UNITDEVICE_TYPE_UNKNOWN );

			return UnitCount;
		}
	}

	if (!IsAlive()) {

		ATLASSERT(UnitCount);

		for (unit = 0; unit < UnitCount; unit++) {

			NdasUiDbgCall( 4, _T("pNdasDevice (%s) is dead add missing disks %d\n"), m_strDeviceName, unit );

			AddUnitDevice( unit, NULL, NDAS_UNITDEVICE_TYPE_UNKNOWN );

			return UnitCount;
		}
	}

	ATLASSERT(!UnitCount);

	// Read number of unit devices of NDAS device

	NDAS_DEVICE_STAT			devicestat;
	NDASCOMM_CONNECTION_INFO	ci;

	InitConnectionInfo( &ci, FALSE );
	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;

	devicestat.Size = sizeof(NDAS_DEVICE_STAT);

	BOOL bSuccess = ::NdasCommGetDeviceStat( &ci, &devicestat );

	if (!bSuccess) {

		m_Status = NDAS_DEVICE_STATUS_OFFLINE;

		AddUnitDevice( 0, NULL, NDAS_UNITDEVICE_TYPE_UNKNOWN );

		return 1;
	}

	// Add unit devices

	NDASUSER_UNITDEVICE_ENUM_ENTRY UnitDeviceEnumEntry;

	for (unit = 0; unit < devicestat.NumberOfUnitDevices; unit++) {

		// retrieve the type of unit device

		NDASCOMM_CONNECTION_INFO ci;

		InitConnectionInfo( &ci, FALSE, unit );

		HNDAS hNDAS = NdasCommConnect(&ci);
		
		if (hNDAS == NULL) {

			AddUnitDevice( unit, NULL, NDAS_UNITDEVICE_TYPE_UNKNOWN );
			continue;
		}
		
		NDAS_UNITDEVICE_HARDWARE_INFO	unitInfo;
		NDAS_UNITDEVICE_TYPE			unitDeviceType;
			
		unitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
			
		if (NdasCommGetUnitDeviceHardwareInfo(hNDAS, &unitInfo)) {

			switch (unitInfo.MediaType) {

			case NDAS_UNIT_ATA_DIRECT_ACCESS_DEVICE:
					
				unitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
				break;
				
			case NDAS_UNIT_ATAPI_CDROM_DEVICE:

				unitDeviceType = NDAS_UNITDEVICE_TYPE_CDROM;
				break;
				
			case NDAS_UNIT_ATAPI_DIRECT_ACCESS_DEVICE:

				unitDeviceType = NDAS_UNITDEVICE_TYPE_COMPACT_BLOCK;
				break;
				
			case NDAS_UNIT_ATAPI_OPTICAL_MEMORY_DEVICE:
				
				unitDeviceType = NDAS_UNITDEVICE_TYPE_OPTICAL_MEMORY;
				break;
				
			case NDAS_UNIT_UNKNOWN_DEVICE:

			default:

				unitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN;
				break;
			}
			
		} else {

			unitDeviceType = NDAS_UNITDEVICE_TYPE_DISK;
		}

		NdasCommDisconnectEx( hNDAS, 0 );
		hNDAS = NULL;

		CHAR	ndasSimpleSerialNo[20+4] = {0};
			
		if (FAILED(GetNdasSimpleSerialNo(&unitInfo, ndasSimpleSerialNo))) {
			
			ATLASSERT(FALSE);
		}

		AddUnitDevice( unit, ndasSimpleSerialNo, unitDeviceType );
	}

	return (UINT8)devicestat.NumberOfUnitDevices;
}

VOID CNBNdasDev::AddUnitDevice( UINT8 UnitNo, PCHAR UnitSimpleSerialNo, NDAS_UNITDEVICE_TYPE UnitDeviceType /* = NDAS_UNITDEVICE_TYPE_UNKNOWN*/ )
{
	CNBUnitDev *pUnitDevice = new CNBUnitDev( this, UnitNo, UnitSimpleSerialNo, UnitDeviceType );

	UnitDeviceSet( pUnitDevice );

	pUnitDevice->Initialize();

	return;
}

VOID CNBNdasDev::UnitDeviceSet (CNBUnitDev *UnitDevice)
{
	if (m_LogicalDevMap.count(UnitDevice->UnitNo())) {

		// already exist

		ATLASSERT(FALSE);

		delete m_LogicalDevMap[UnitDevice->UnitNo()];
		m_LogicalDevMap[UnitDevice->UnitNo()] = NULL;
	}

	// complete initialization

	NdasUiDbgCall( 4, _T(" - (%s)\n"), m_strDeviceName );

	m_LogicalDevMap[UnitDevice->UnitNo()] = UnitDevice;

	return;
}

void CNBNdasDev::ClearUnitDevices()
{
	for (NBUnitDevPtrMap::iterator itUnitDevice = m_LogicalDevMap.begin();
		 itUnitDevice != m_LogicalDevMap.end();
		 itUnitDevice++) {

		if (itUnitDevice->second) {

			delete itUnitDevice->second;
			itUnitDevice->second = NULL;
		}
	}
}

VOID
CNBNdasDev::GetNdasID (NDAS_ID *ndasId)
{
	ATLASSERT( NULL != ndasId );

	// CNBNdasDev does not store NDASID_EXT_DATA.
	// Instead,CNBNdasDev creates NDASID_EXT_DATA using m_DeviceId.Vid
	// If there are any other 'variable' in the future except Vid, CNBNdasDev must have it.

	NDASID_EXT_DATA IdExtData = NDAS_ID_EXTENSION_DEFAULT;

	IdExtData.Vid = m_DeviceId.Vid;

	ZeroMemory( ndasId, sizeof(NDAS_ID) );

	if (m_DeviceId.Vid == NDAS_VID_NONE) {

		memset( ndasId, 0, sizeof(NDAS_ID) );
	
	} else {
	
		NdasIdDeviceToStringEx( &m_DeviceId, ndasId->Id, NULL, NULL, &IdExtData );
	}
}

CString CNBNdasDev::GetName()
{
	return m_strDeviceName;
}

// creates NDASCOMM_CONNECTION_INFO

BOOL CNBNdasDev::InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ConnectionInfo, BOOL bWriteAccess, UINT8 UnitNo /*= 0*/)
{
	ATLASSERT(ConnectionInfo);

	if (IsAlive() == FALSE) {

		ConnectionInfo->AddressType = NDASCOMM_CIT_UNSPECIFIED;
		return TRUE;
	}

	// write access requires granted write access

	if (!(GENERIC_WRITE & m_GrantedAccess) && bWriteAccess) {

		return FALSE;
	}

	ZeroMemory( ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) );

	ConnectionInfo->Size		= sizeof(NDASCOMM_CONNECTION_INFO);
	ConnectionInfo->LoginType	= NDASCOMM_LOGIN_TYPE_NORMAL;
	ConnectionInfo->UnitNo		= UnitNo;
	ConnectionInfo->WriteAccess = bWriteAccess;
	
	// Following two lines are not necessary, ZeroMemory does these.
	// But just to clarify!
	
	ConnectionInfo->OEMCode.UI64Value			= 0;
	ConnectionInfo->PrivilegedOEMCode.UI64Value = 0;
	ConnectionInfo->Protocol					= NDASCOMM_TRANSPORT_LPX;

	ConnectionInfo->AddressType = NDASCOMM_CIT_NDAS_ID;
	 
	GetNdasID( &ConnectionInfo->Address.NdasId );

	return TRUE;
}


// Implementation of CNBUnitDev class


CNBUnitDev::CNBUnitDev ( 
	CNBNdasDev				*NdasDevice,
	UINT8					UnitNo, 
	PCHAR					UnitSimpleSerialNo,
	NDAS_UNITDEVICE_TYPE	UnitDeviceType
	)
{
	m_NdasDevice = NULL;
	m_UnitNo = 0;

	memset( m_UnitSimpleSerialNo, 0, NDAS_DIB_SERIAL_LEN );
	
	m_PhysicalCapacity = 0;
	m_UnitDeviceType = 0;

	memset( &m_Dib, 0, sizeof(m_Dib) );
	memset( &m_Rmd, 0, sizeof(m_Rmd) );

	memset( &m_MetaData, 0, sizeof(m_MetaData) );

	memset( &m_Definition, 0, sizeof(m_Definition) );

	m_Status = 0;

	m_pLogicalDevice = NULL;

	ATLASSERT(NdasDevice);

	m_NdasDevice		= NdasDevice;	// Can be NULL if unit device is not registered.
	m_UnitNo			= UnitNo;
	
	if (UnitSimpleSerialNo) {

		CopyMemory( m_UnitSimpleSerialNo, UnitSimpleSerialNo, NDAS_DIB_SERIAL_LEN );
	}

	m_UnitDeviceType	= UnitDeviceType;

	NdasUiDbgCall( 4, _T("new CNBUnitDev(%p) : UnitNo %d, Type %d\n"), this, m_UnitNo, m_UnitDeviceType );
}

BOOL CNBUnitDev::Initialize(VOID)
{
	BOOL	bReturn = FALSE;

	NDASCOMM_CONNECTION_INFO ci;
	
	HNDAS	hNDAS = NULL;

	UINT32	nDIBSize = sizeof(m_Dib);

	NDAS_UNITDEVICE_HARDWARE_INFOW unitInfo;

	m_PhysicalCapacity = 0;
	
	m_Definition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);
	
	if (FAILED(NdasVsmInitializeLogicalUnitDefinition(&m_NdasDevice->GetDeviceId(), m_UnitSimpleSerialNo, &m_Definition))) {

		ATLASSERT(FALSE);
		return FALSE;
	}

	UpdateStatus();

	if (m_UnitDeviceType == NDAS_UNITDEVICE_TYPE_UNKNOWN) {

		m_Dib.iMediaType = NMT_INVALID;
		m_Dib.nDiskCount = 1;

		return TRUE;
	}

	ATLASSERT( m_NdasDevice->IsAlive() );

	if (InitConnectionInfo(&ci, FALSE) == FALSE) {

		ATLASSERT(FALSE);
		goto out;
	}

	if (!(hNDAS = NdasCommConnect(&ci))) {
	
		goto out;
	}

	::ZeroMemory(&unitInfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW));

	unitInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFOW);
	
	bReturn = NdasCommGetUnitDeviceHardwareInfoW(hNDAS, &unitInfo);

	if (bReturn == FALSE) {
	
		goto out;
	}

	m_PhysicalCapacity = (unitInfo.SectorCount.QuadPart - NDAS_BLOCK_SIZE_XAREA) * SECTOR_SIZE;
	
	m_Definition.Size = sizeof(NDAS_LOGICALUNIT_DEFINITION);
	
	if (FAILED(NdasVsmReadLogicalUnitDefinition(hNDAS, &m_Definition))) {

		ATLASSERT(FALSE);
		bReturn = FALSE;
		goto out;
	}

	if (FAILED(NdasOpReadDib(hNDAS, &m_Dib, &nDIBSize))) {

		ATLASSERT(FALSE);
		bReturn = FALSE;
		goto out;
	}

	if (NMT_RAID1R2 == m_Dib.iMediaType ||
		NMT_RAID4R2 == m_Dib.iMediaType ||
		NMT_RAID1R3 == m_Dib.iMediaType ||
		NMT_RAID4R3 == m_Dib.iMediaType ||
		NMT_RAID5 == m_Dib.iMediaType ) {

		if (!NdasCommBlockDeviceRead(hNDAS,
									 NDAS_BLOCK_LOCATION_RMD,
									 1,
									 (PBYTE)&m_Rmd)) {
			goto out;
		}
	
	} else if (NMT_MIRROR == m_Dib.iMediaType		||
			   NMT_AGGREGATE == m_Dib.iMediaType	||
			   NMT_RAID0 == m_Dib.iMediaType		||
			   NMT_RAID1 == m_Dib.iMediaType		||
			   NMT_RAID4 == m_Dib.iMediaType) {

		// 3.11 RAID0 and aggregation may have RMD but Pre-3.10 aggregation may not have RMD.
		// And it is safe to ignore them.
	
		::ZeroMemory((PBYTE)&m_Rmd, sizeof(m_Rmd));

	} else {
	
		::ZeroMemory((PBYTE)&m_Rmd, sizeof(m_Rmd));
	}

	if (!NdasCommBlockDeviceRead(hNDAS, NDAS_BLOCK_LOCATION_META_DATA, 5, (PBYTE)&m_MetaData)) {

		ATLASSERT(FALSE);
		goto out;
	}

	//ATLASSERT( memcmp(&m_MetaData.DIB_V2, &m_Dib, 512) == 0 );

	bReturn = TRUE;

out:

	if (!bReturn) {

		// We may be able to connect to device when enumerating but failed to connect or read DIB here.

		m_Dib.iMediaType = NMT_INVALID;
		m_Dib.nDiskCount = 1;
	}

	if (hNDAS) {

		NdasCommDisconnect(hNDAS);
	}

	return bReturn;
}

DWORD CNBUnitDev::UpdateStatus()
{
	m_Status = 0;

	// no write key
	
	if (!(GENERIC_WRITE & m_NdasDevice->GetGrantedAccess())) {

		m_Status |= NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY;
	}

	// disconnected

	if (m_NdasDevice->IsAlive() == FALSE) {

		if (m_NdasDevice->GetStatus() == NDAS_DEVICE_STATUS_NOT_REGISTERED) {

			m_Status |= NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED;
		
		} else {

			m_Status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
		}

		// can not process anymore
		
		return m_Status;
	}

	// Check device is in use.

	NDASCOMM_CONNECTION_INFO ci;
	
	if (!InitConnectionInfo(&ci, FALSE)) {

		ATLASSERT(FALSE);

		m_Status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
		return m_Status;
	}

	ci.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;

	NDAS_UNITDEVICE_STAT udstat = {0};

	udstat.Size = sizeof(NDAS_UNITDEVICE_STAT);

	if (NdasCommGetUnitDeviceStat(&ci, &udstat)) {

		NdasUiDbgCall( 4, __T("udstat.RwHostCount = %d, udstat.RoHostCount = %d\n"), udstat.RwHostCount, udstat.RoHostCount );

		if (udstat.RwHostCount || (udstat.RoHostCount && udstat.RoHostCount != NDAS_HOST_COUNT_UNKNOWN)) {

			m_Status |= NDASBIND_UNIT_DEVICE_STATUS_MOUNTED;

		} else if (udstat.RwHostCount == 0 && NDAS_HOST_COUNT_UNKNOWN == udstat.RoHostCount) {

			m_Status |= NDASBIND_UNIT_DEVICE_STATUS_UNKNOWN_RO_MOUNT;
		}

	} else {

		m_Status |= NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED;
	}

	NdasUiDbgCall( 4, __T("status = %x %s\n"), m_Status, GetName() );

	return m_Status;
}

BOOL
CNBUnitDev::InitConnectionInfo (
	NDASCOMM_CONNECTION_INFO *ConnectionInfo,
	BOOL					 WriteAccess
	)
{
	ATLASSERT( ConnectionInfo != NULL );
	ATLASSERT( m_NdasDevice != NULL );

	return m_NdasDevice->InitConnectionInfo(ConnectionInfo, WriteAccess, m_UnitNo);
}

BOOL CNBUnitDev::Equals (BYTE Node[6], BYTE Vid, PCHAR UnitSimpleSerialNo)
{
#if 0
	return m_NdasDevice->Equals(Node, Vid)  && 
		   memcmp( m_UnitSimpleSerialNo, UnitSimpleSerialNo, NDAS_DIB_SERIAL_LEN );
#else
	return m_NdasDevice->Equals(Node, Vid);
#endif
}

CString CNBUnitDev::GetName()
{
	CString strText;

	if (m_NdasDevice) {
	
		if (1 != m_NdasDevice->UnitDevicesCount() || m_UnitNo>0) {

			strText.Format(_T("%s:%d"), m_NdasDevice->GetName(), m_UnitNo + 1);
		
		} else {

			strText.Format(_T("%s"), m_NdasDevice->GetName());
		}

	} else {
		
		strText = "";
	}

	return strText;
}

LPCTSTR CNBUnitDev::GetName(UINT *UnitNo)
{
	ATLASSERT(m_NdasDevice);

	if (m_NdasDevice->UnitDevicesCount() <= 1) {

		*UnitNo = 0;

	} else {

		*UnitNo = m_UnitNo + 1;
	}

	NdasUiDbgCall( 4, _T("*UnitNo = %d, %s\n"), *UnitNo, m_NdasDevice->GetName() );

	return m_NdasDevice->GetName();
}

DWORD CNBUnitDev::GetAccessMask()
{
	return m_NdasDevice->GetGrantedAccess();
}

BOOL CNBUnitDev::IsMemberOfRaid(VOID)
{
	switch (DIB()->iMediaType) {

	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
	case NMT_RAID4:
	case NMT_RAID4R2:
	case NMT_RAID4R3:		
	case NMT_RAID5:		

		return TRUE;

	case NMT_INVALID:
	case NMT_SINGLE:
	case NMT_SAFE_RAID1:
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	case NMT_CONFLICT:

	default:

		return FALSE;
	}
}


// implementation of CNBLogicalDev class


CNBLogicalDev::CNBLogicalDev( CNBUnitDev *UnitDevice )
{
	m_ParentLogicalDev = NULL;
	m_UptodateUnit	   = NULL;

	m_Nidx = 0;	
	m_Ridx = 0;

	m_UptodateUnit = NULL;	

	m_Status = 0;

	memset( &m_NdasrInfo, 0, sizeof(m_NdasrInfo) );

	m_NdasrState = NRMX_RAID_STATE_INITIALIZING;
	m_OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;

	m_FixRequired = FALSE;
	m_MigrationRequired = FALSE;

	m_NdasDevUnit = UnitDevice;

	if (!IsRoot()) {

		return;
	}
	
	if (UnitDevice) {

		UnitDevice->SetLogicalDevice(this);
		m_Status = m_NdasDevUnit->GetStatus();
	}

	return;
}

CNBLogicalDev::~CNBLogicalDev()
{
	if (m_NdasDevUnit) {

		m_NdasDevUnit->SetLogicalDevice(NULL);
		m_NdasDevUnit = NULL;
	}
}

BOOL CNBLogicalDev::IsMember(CNBLogicalDev *TargetLogicalDev)
{
	NDAS_LOGICALUNIT_DEFINITION *definition;
	NDAS_LOGICALUNIT_DEFINITION *targetDefinition;

	ATLASSERT(!IsLeaf());

	ATLASSERT(m_LogicalDevMap.size());	
	ATLASSERT(TargetLogicalDev);

	definition		 = m_UptodateUnit->m_NdasDevUnit->Definition();
	targetDefinition = TargetLogicalDev->m_NdasDevUnit->Definition();

	return memcmp(definition, targetDefinition, sizeof(NDAS_LOGICALUNIT_DEFINITION)) == 0;
}

VOID CNBLogicalDev::UnitDeviceSet(CNBLogicalDev *childLogicalDev, UINT32 SequenceNum)
{
	NDAS_DEVICE_STATUS deviceStatus;

	ATLASSERT( !IsLeaf() );
	
	if (!m_LogicalDevMap.size()) {

		ATLASSERT( m_UptodateUnit == NULL );

		m_UptodateUnit = childLogicalDev;

	} else if (!m_UptodateUnit->IsAlive()) {

		m_UptodateUnit = childLogicalDev;
		
	} else if (childLogicalDev->IsAlive()) {

		// If this unit has larger USN, then make this device as primary device.

		if (((INT32)(childLogicalDev->m_NdasDevUnit->RMD()->uiUSN - m_UptodateUnit->m_NdasDevUnit->RMD()->uiUSN)) > 0) {

			m_UptodateUnit = childLogicalDev;
		}
	}

	deviceStatus = childLogicalDev->m_NdasDevUnit->GetNdasDevice()->GetStatus();

	m_LogicalDevMap[SequenceNum] = childLogicalDev;

	NdasUiDbgCall( 4, _T("CNBLogicalDev(%p).m_LogicalDevMap[%d] = (%p) : %s\n"),
			 this, SequenceNum, childLogicalDev, childLogicalDev->GetName() );

	childLogicalDev->m_ParentLogicalDev = this;

	childLogicalDev->m_Nidx = SequenceNum;

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		if (m_LogicalDevMap[i] == NULL) {

			return;
		}
	}

	if (m_LogicalDevMap.size() == DIB()->nDiskCount + DIB()->nSpareCount) { // All member is set

		UCHAR	newNdasrState;
		UINT8	ridx;

		NdasUiDbgCall( 4, _T("All member is set\n") );

		m_OutOfSyncRoleIndex = NO_OUT_OF_SYNC_ROLE;

		InitNdasrInfo();

		if (m_NdasrInfo.Type == NMT_MIRROR	||
			m_NdasrInfo.Type == NMT_RAID1	||
			m_NdasrInfo.Type == NMT_RAID4	||
			m_NdasrInfo.Type == NMT_RAID1R2	||
			m_NdasrInfo.Type == NMT_RAID4R2) {

			m_MigrationRequired = TRUE;

		} else {

			m_MigrationRequired = FALSE;
		}

		newNdasrState = NRMX_RAID_STATE_NORMAL;

		if (m_OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE) {

			newNdasrState = NRMX_RAID_STATE_OUT_OF_SYNC;
		}

		for (ridx = 0; ridx < m_NdasrInfo.ActiveDiskCount; ridx++) { // i : role index
			
			if (!FlagOn(m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_RUNNING) ||
				FlagOn(m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_OFFLINE)	 ||
				FlagOn(m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

				switch (newNdasrState) {

				case NRMX_RAID_STATE_NORMAL: {

					m_OutOfSyncRoleIndex = ridx;
					newNdasrState = NRMX_RAID_STATE_DEGRADED;

					break;
				}

				case NRMX_RAID_STATE_OUT_OF_SYNC: {

					if (m_OutOfSyncRoleIndex == ridx) {
					
						newNdasrState = NRMX_RAID_STATE_DEGRADED;
				
					} else {

						newNdasrState = NRMX_RAID_STATE_FAILED;
					}

					break;
				}

				case NRMX_RAID_STATE_DEGRADED: {

					ATLASSERT( m_OutOfSyncRoleIndex != NO_OUT_OF_SYNC_ROLE && m_OutOfSyncRoleIndex != ridx );
				
					newNdasrState = NRMX_RAID_STATE_FAILED;

					break;
				}

				default:

					break;			
				}		
			}
		}

		ATLASSERT( newNdasrState == NRMX_RAID_STATE_NORMAL		||
				   newNdasrState == NRMX_RAID_STATE_DEGRADED	||
				   newNdasrState == NRMX_RAID_STATE_OUT_OF_SYNC	||
				   newNdasrState == NRMX_RAID_STATE_FAILED );

		if (newNdasrState == NRMX_RAID_STATE_FAILED) {

			CHAR	aliveCount = 0;

			for (ridx = 0; ridx < m_NdasrInfo.ActiveDiskCount; ridx++) { // i : role index

				if (FlagOn(m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_RUNNING)) {

					aliveCount ++;
				}
			}

			if (aliveCount >= m_NdasrInfo.ActiveDiskCount - m_NdasrInfo.ParityDiskCount) {

				newNdasrState = NRMX_RAID_STATE_EMERGENCY;
			}
		}

		if (newNdasrState != NRMX_RAID_STATE_FAILED) {

			for (ridx = 0; ridx < m_LogicalDevMap.size(); ridx++) { // i : role index

				if (m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx >= m_LogicalDevMap.size()) {

					ATLASSERT(FALSE);

					for (UINT8 ridx2 = 0; ridx2 < m_LogicalDevMap.size(); ridx2++) { // i : role index

						m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx = ridx2;
					}

					newNdasrState = NRMX_RAID_STATE_FAILED;
					break;
				}

				if (m_LogicalDevMap[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx]->IsAlive()) {

					continue;
				}

				if (FlagOn(m_NdasrInfo.Rmd.UnitMetaData[ridx].UnitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_OFFLINE) == FALSE) {

					m_FixRequired = TRUE;
					break;
				}
			}
		}

		m_NdasrState = newNdasrState;

		if (IsFaultTolerant()) {

			for (ridx=0; ridx<DIB()->nDiskCount+DIB()->nSpareCount; ridx++) {

				NdasUiDbgCall( 4, _T("m_NdasrInfo.Rmd.UnitMetaData[%d].Nidx = %d, m_NdasrInfo.Rmd.UnitMetaData[%d].Nidx = %X\n"), 
						  ridx, m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx, ridx, m_NdasrInfo.Rmd.UnitMetaData[ridx].UnitDeviceStatus );

				ATLASSERT( m_NdasrInfo.Rmd.UnitMetaData[ridx].iUnitDeviceIdx == m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx );

				m_LogicalDevMap[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx]->SetRidx(ridx);
			}
		}

		for (UINT32 nidx=0; nidx<DIB()->nDiskCount+DIB()->nSpareCount; nidx++) {

			if (m_LogicalDevMap[nidx]->IsAlive()) {

				UCHAR zeroUnitSimpleSerialNo[NDAS_DIB_SERIAL_LEN] = {0};

				ATLASSERT( memcmp(m_LogicalDevMap[nidx]->UnitSimpleSerialNo(), 
								  DIB()->UnitSimpleSerialNo[nidx], 
								  sizeof(NDAS_DIB_SERIAL_LEN)) == 0 ||
						   memcmp(zeroUnitSimpleSerialNo, 
								  DIB()->UnitSimpleSerialNo[nidx], 
								  sizeof(NDAS_DIB_SERIAL_LEN)) == 0 );
			}
		}

		UpdateStatus();
	}

	NdasUiDbgCall( 4, _T("m_NdasrState = %d\n"), m_NdasrState );
}

DWORD CNBLogicalDev::UpdateStatus (BOOL UpdateUnitDevice /*= FALSE*/)
{
	m_Status = 0;

	if (IsLeaf()) {

		if (UpdateUnitDevice) {

			m_Status = m_NdasDevUnit->UpdateStatus();

		} else {

			m_Status = m_NdasDevUnit->GetStatus();
		}

		if (!IsRoot() && IsMemberOffline()) {

			m_Status |= NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED;
		}

		return m_Status;
	}

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		if (!m_LogicalDevMap[i]) {

			continue;
		}

		m_Status |= m_LogicalDevMap[i]->UpdateStatus(UpdateUnitDevice);
	}

	return m_Status;
}

UINT CNBLogicalDev::GetStatusId()
{
	if (IsRoot() && IsLeaf()) {
	
		if (NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED & m_Status) {

			return IDS_STATUS_NOT_CONNECTED;
		}

		if (NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED & m_Status) {

			ATLASSERT(FALSE);

			return IDS_STATUS_INVALID;
		}
				
		if (NMT_INVALID == GetType()) {

			// Maybe failed to connect in spite of reported as connected.
			// Safe to handle as if not connected?

			return IDS_STATUS_NOT_CONNECTED;
		} 
		
		if (NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY & m_Status) {

			return IDS_STATUS_READ_ONLY;
		}

		if (NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED & m_Status) {

			return IDS_STATUS_IN_USE;
		} 

		return IDS_STATUS_FINE;
	}

	if (!IsRoot() && IsLeaf()) {

		if (NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED & m_Status) {

			return IDS_STATUS_INVALID;
		} 

		if (NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED & m_Status) {

			return IDS_STATUS_NOT_CONNECTED;
		}

		if (NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY & m_Status) {

			return IDS_STATUS_READ_ONLY;
		}

		if (NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED & m_Status) {

			return IDS_STATUS_IN_USE;
		} 

		return IDS_STATUS_FINE;
	}

	ATLASSERT( IsRoot() && !IsLeaf() );
	
	if (m_NdasrState > NRMX_RAID_STATE_DEGRADED) {
		
		return IDS_STATUS_UNMOUNTABLE;
	}

	if (NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED & m_Status) {

		return IDS_STATUS_IN_USE;
	} 
	
	return IDS_STATUS_FINE;
}

CString CNBLogicalDev::GetName()
{
	if (IsLeaf()) {

		return m_NdasDevUnit->GetName();
	}

	CString strText;

	switch(GetType()) {

	case NMT_AGGREGATE: strText.LoadString(IDS_LOGDEV_TYPE_AGGREGATED_DISK); break;
	case NMT_MIRROR:	strText.LoadString(IDS_LOGDEV_TYPE_MIRRORED_DISK); break;
	case NMT_RAID0:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID0); break;
	case NMT_RAID1:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1); break;
	case NMT_RAID1R2:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R2); break;
	case NMT_RAID1R3:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID1R3); break;	
	case NMT_RAID4:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4); break;
	case NMT_RAID4R2:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R2); break;
	case NMT_RAID4R3:	strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID4R3); break;	
	case NMT_RAID5:		strText.LoadString(IDS_LOGDEV_TYPE_DISK_RAID5); break;
	default:			strText.FormatMessage(IDS_LOGDEV_TYPE_UNKNOWN_FMT, GetType());

	}

	return strText;
}

LPCTSTR CNBLogicalDev::GetName(UINT *UnitNo)
{
	if (IsLeaf()) {

		return m_NdasDevUnit->GetName(UnitNo);
	}

	ATLASSERT(FALSE);

	return NULL;
}

VOID CNBLogicalDev::GetNdasID(NDAS_ID *NdasId) {

	return m_NdasDevUnit->GetNdasID(NdasId);
}

BOOL CNBLogicalDev::IsHDD()
{
	switch (GetType()) {

	case NMT_SINGLE:
	case NMT_MIRROR:
	case NMT_AGGREGATE:
	case NMT_RAID0:
	case NMT_RAID1:
	case NMT_RAID1R2:
	case NMT_RAID1R3:		
	case NMT_RAID4:
	case NMT_RAID4R2:
	case NMT_RAID4R3:		
	case NMT_RAID5:		
	case NMT_AOD:
	case NMT_VDVD:
	case NMT_SAFE_RAID1:
	case NMT_CONFLICT:

		return TRUE;
	
	case NMT_INVALID:
	case NMT_CDROM:
	case NMT_OPMEM:
	case NMT_FLASH:
	
		return FALSE;
	}

	return TRUE;
}

DWORD CNBLogicalDev::GetAccessMask()
{
	DWORD dwGrantedAccess = 0xFFFFFFFF;

	if (IsLeaf()) {

		return m_NdasDevUnit->GetAccessMask();
	}

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		dwGrantedAccess &= m_LogicalDevMap[i]->GetAccessMask();
	}

	return dwGrantedAccess;
}

VOID CNBLogicalDev::InitNdasrInfo() 
{
	UCHAR nidx, ridx;

	do {

		RtlZeroMemory( &m_NdasrInfo, sizeof(NDASR_INFO) );

		m_NdasrInfo.Type = GetType();

		switch (m_NdasrInfo.Type) {

		case NMT_INVALID:
		case NMT_SINGLE: 
		case NMT_CDROM: 
		case NMT_OPMEM: 
		case NMT_FLASH:

			ATLASSERT(FALSE);
			return;

		case NMT_CONFLICT: 

			ATLASSERT(FALSE);
			return;
		
		case NMT_MIRROR:
		case NMT_RAID1: 
		case NMT_RAID1R2: 

			m_NdasrInfo.Size = m_UptodateUnit->LogicalCapacityInByte();

			return;

		case NMT_AGGREGATE: { 

			for (nidx = 0; nidx < m_LogicalDevMap.size(); nidx++) {

				if (!m_LogicalDevMap[nidx]->IsAlive()) {

					m_NdasrInfo.Size = 0;
					break;
				}

				m_NdasrInfo.Size += m_LogicalDevMap[nidx]->LogicalCapacityInByte();
			}

			break;
		}

		case NMT_RAID0: 

			m_NdasrInfo.Size = m_UptodateUnit->LogicalCapacityInByte() * DIB()->nDiskCount;

			break;

		case NMT_RAID1R3: 
		case NMT_RAID4R3: 
		case NMT_RAID5: 		

			m_NdasrInfo.Size = m_UptodateUnit->LogicalCapacityInByte() * (DIB()->nDiskCount-1);

			break;

		default:

			ATLASSERT(FALSE);
			return;
		}

		switch (m_NdasrInfo.Type) {

		case NMT_AGGREGATE:
			
			m_NdasrInfo.Striping = FALSE;

			m_NdasrInfo.ParityDiskCount = 0;	
			m_NdasrInfo.DistributedParity = FALSE;

			break;

		case NMT_RAID0:

			m_NdasrInfo.Striping = TRUE;

			m_NdasrInfo.ParityDiskCount = 0;	
			m_NdasrInfo.DistributedParity = FALSE;

			break;

		case NMT_RAID1R3:

			m_NdasrInfo.Striping = TRUE;

			m_NdasrInfo.ParityDiskCount = 1;	
			m_NdasrInfo.DistributedParity = FALSE;

			break;

		case NMT_RAID4R3:

			//ATLASSERT( FlagOn(Lurn->Lur->SupportedNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE) );

			m_NdasrInfo.Striping = TRUE;

			m_NdasrInfo.ParityDiskCount = 1;	
			m_NdasrInfo.DistributedParity = FALSE;

			break;

		case NMT_RAID5:

			//ATLASSERT( FlagOn(Lurn->Lur->SupportedNdasFeatures, NDASFEATURE_SIMULTANEOUS_WRITE) );

			m_NdasrInfo.Striping = TRUE;

			m_NdasrInfo.ParityDiskCount = 1;	
			m_NdasrInfo.DistributedParity = TRUE;

			break;

		default:

			ATLASSERT(FALSE);
			break;
		}

		if (m_NdasrInfo.ParityDiskCount == 0) {

			m_NdasrInfo.ActiveDiskCount	= (UCHAR)DIB()->nDiskCount;
			m_NdasrInfo.SpareDiskCount	= 0;

		} else {

			m_NdasrInfo.ActiveDiskCount	= (UCHAR)DIB()->nDiskCount;
			m_NdasrInfo.SpareDiskCount	= (UCHAR)DIB()->nSpareCount;

			m_NdasrInfo.BlocksPerBit	= DIB()->iSectorsPerBit;
		}

		ATLASSERT( m_NdasrInfo.ActiveDiskCount + m_NdasrInfo.SpareDiskCount == m_LogicalDevMap.size() );
		ATLASSERT( m_NdasrInfo.SpareDiskCount == 0 || m_NdasrInfo.SpareDiskCount == 1 );

//		runningChildCount = 0;
	
		for (nidx = 0; nidx < m_LogicalDevMap.size(); nidx++) {

			UCHAR	nodeFlags;

			if (m_LogicalDevMap[nidx]->IsMissingMember()) {

				m_NdasrInfo.LocalNodeFlags[nidx] = NRMX_NODE_FLAG_UNKNOWN;

			} else if (m_LogicalDevMap[nidx]->IsAlive()) {

				m_NdasrInfo.LocalNodeFlags[nidx] = NRMX_NODE_FLAG_RUNNING;

			} else {

				m_NdasrInfo.LocalNodeFlags[nidx] = NRMX_NODE_FLAG_STOP;
			}

			m_NdasrInfo.LocalNodeChanged[nidx] = 0;
		}

		// Read Rmd

		if (m_NdasrInfo.ParityDiskCount) {

			m_NdasrInfo.Rmd = *m_UptodateUnit->RMD();

			NDASCOMM_CONNECTION_INFO ci;
			HNDAS hNDAS = NULL;
			BOOL bResult;

			bResult = InitConnectionInfo( &ci, FALSE );

			if (!bResult) {

				break;
			}

			if (!(hNDAS = NdasCommConnect(&ci))) {

				ATLASSERT(FALSE);
				break;
			}

			NdasOpGetOutOfSyncStatus( hNDAS, DIB(), &m_NdasrInfo.TotalBitCount, &m_NdasrInfo.OosBitCount );

			NdasCommDisconnect(hNDAS);

			for (ridx = 0; ridx < m_LogicalDevMap.size(); ridx++) { // i : role index. 
		
				UCHAR unitDeviceStatus = m_NdasrInfo.Rmd.UnitMetaData[ridx].UnitDeviceStatus;

				if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_NOT_SYNCED)) {

					//ATLASSERT( m_NdasrInfo.NodeIsUptoDate[ndasrArbitrator->RoleToNodeMap[ridx]] == FALSE );

					if (ridx < m_NdasrInfo.ActiveDiskCount) {

						//ATLASSERT( !FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE) );
						ATLASSERT( m_OutOfSyncRoleIndex == NO_OUT_OF_SYNC_ROLE );

						m_OutOfSyncRoleIndex = (UCHAR)ridx;
					}
				}

				if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_DEFECTIVE)) {

					SetFlag( m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], 
							 NRMX_NODE_FLAG_DEFECTIVE );

					continue;
				}

				if (FlagOn(unitDeviceStatus, NDAS_UNIT_META_BIND_STATUS_OFFLINE)) {

					ATLASSERT( !FlagOn(m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_RUNNING) );
	
					SetFlag( m_NdasrInfo.LocalNodeFlags[m_NdasrInfo.Rmd.UnitMetaData[ridx].Nidx], NRMX_NODE_FLAG_OFFLINE );
					
					continue;
				}
			}
		}

	} while (0);
}

UINT64 CNBLogicalDev::LogicalCapacityInByte()
{
	UINT64 size = 0;

	if (IsLeaf()) {

		return DIB()->sizeUserSpace * SECTOR_SIZE;
	}

	return m_NdasrInfo.Size;
}

BOOL CNBLogicalDev::IsDefective(VOID) 
{
	if (IsLeaf()) {

		ATLASSERT(!IsRoot());
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		if (RaidMemberStatus() & (NDAS_UNIT_META_BIND_STATUS_BAD_DISK | NDAS_UNIT_META_BIND_STATUS_BAD_SECTOR)) {

			return TRUE;

		} else {

			return FALSE;
		}
	}

	ATLASSERT(IsRoot());
	ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

	for (UINT i=0; i<m_LogicalDevMap.size(); i++) {

		if (m_LogicalDevMap[i]->IsDefective()) {

			return TRUE;
		}
	}

	return FALSE;
}

BOOL CNBLogicalDev::IsMountable(BOOL IncludeEmergency/* = FALSE*/) {

	ATLASSERT(IsRoot()); 

	if (IsLeaf()) {

		return IsAlive();
	}

	ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

	NdasUiDbgCall( 4, _T("m_NdasrState = %d\n"), m_NdasrState );

	if (m_NdasrState == NRMX_RAID_STATE_NORMAL		||
		m_NdasrState == NRMX_RAID_STATE_OUT_OF_SYNC	||
		m_NdasrState == NRMX_RAID_STATE_DEGRADED) {

		return TRUE;
	}

	if (IncludeEmergency) {

		if (m_NdasrState == NRMX_RAID_STATE_EMERGENCY) {

			return TRUE;
		}
	}

	return FALSE;
}


BOOL CNBLogicalDev::IsBindOperatable(BOOL AliveMeberOnly /*= FALSE*/)
{
	if (IsLeaf()) {

		if (!IsAlive()) {

			return FALSE;
		}

		if (!IsHDD()) {

			return FALSE;
		}

		if (NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY & m_Status) {

			return FALSE;
		}

		if (NDASBIND_UNIT_DEVICE_STATUS_MOUNTED & m_Status) {

			return FALSE;
		}

		return TRUE;
	}

	ATLASSERT(IsRoot());
	ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		ATLASSERT( m_LogicalDevMap.count(i) );

		if (AliveMeberOnly) {

			if (m_LogicalDevMap[i]->IsAlive() == FALSE) {

				continue;
			}
		}

		if (m_LogicalDevMap[i]->IsBindOperatable() == FALSE) {
			
			return FALSE;
		}
	}

	return TRUE;
}

NBLogicalDevPtrList CNBLogicalDev::GetBindOperatableDevices()
{
	NBLogicalDevPtrList listUnitDevices;

	ATLASSERT( !IsLeaf() );

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		if (!m_LogicalDevMap.count(i)) {

			ATLASSERT( FALSE );
			continue;
		}

		if (m_LogicalDevMap[i]->IsBindOperatable() == FALSE) {

			continue;
		}

		listUnitDevices.push_back(m_LogicalDevMap[i]);
	}

	return listUnitDevices;
}

BOOL CNBLogicalDev::IsCommandAvailable(int nID)
{
	//NdasUiDbgCall( 4, _T("IsCommandAvailable %s, IsRoot() && IsLeaf() = %d, nID = %d\n"), GetName(), IsRoot() && IsLeaf(), nID );

	if (IsRoot() && IsLeaf()) {	// single

		switch (nID) {

		case IDM_TOOL_BIND:

			NdasUiDbgCall( 4, _T("IsCommandAvailable %s, nID = %d IsBindOperatable() = %d\n"), GetName(), nID, IsBindOperatable() );

			return IsBindOperatable() && GetType() == NMT_SINGLE;

		case IDM_TOOL_UNBIND:
			
			return FALSE;

		case IDM_TOOL_ADDMIRROR:
			
			return IsBindOperatable() && GetType() == NMT_SINGLE;

		case IDM_TOOL_APPEND:

			return IsBindOperatable() && GetType() == NMT_SINGLE;

		case IDM_TOOL_SPAREADD:

			return FALSE;

		case IDM_TOOL_REPLACE_DEVICE:

			return FALSE;

		case IDM_TOOL_REMOVE_FROM_RAID:

			return FALSE;

		case IDM_TOOL_CLEAR_DEFECTIVE:

			return FALSE;

		case IDM_TOOL_MIGRATE:

			return FALSE;

		case IDM_TOOL_RESET_BIND_INFO:

			return IsBindOperatable() && GetType() == NMT_CONFLICT;

		default:

		ATLASSERT(FALSE);
		return FALSE;
		}
	}

	if (IsRoot()) {	// raid root

		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

		switch (nID) {

		case IDM_TOOL_BIND:

			return FALSE;

		case IDM_TOOL_UNBIND:
			
			return IsBindOperatable(TRUE);

		case IDM_TOOL_ADDMIRROR:
			
			return FALSE;

		case IDM_TOOL_APPEND:

			return !IsMigrationRequired() && GetType() == NMT_AGGREGATE && IsBindOperatable();

		case IDM_TOOL_SPAREADD:

			NdasUiDbgCall( 4, _T("m_NdasrInfo.ParityDiskCount = %d, IsBindOperatable() = %d, m_NdasrInfo.SpareDiskCount = %d\n"),
						m_NdasrInfo.ParityDiskCount, IsBindOperatable(), m_NdasrInfo.SpareDiskCount );

			return !IsMigrationRequired() && m_NdasrInfo.ParityDiskCount && 
				   IsBindOperatable() && m_NdasrInfo.SpareDiskCount == 0;

		case IDM_TOOL_REPLACE_DEVICE:

			return FALSE;

		case IDM_TOOL_CLEAR_DEFECTIVE:

			return !IsMigrationRequired() &&IsBindOperatable();

		case IDM_TOOL_REMOVE_FROM_RAID:

			return FALSE;

		case IDM_TOOL_MIGRATE:

			return IsMigrationRequired() && IsBindOperatable();

		case IDM_TOOL_RESET_BIND_INFO:

			return IsBindOperatable() && GetType() == NMT_CONFLICT;

		default:

		ATLASSERT(FALSE);
		return FALSE;
		
		}
	}

	ATLASSERT( !IsRoot() && IsLeaf() ); // raid member

	if (RootLogicalDev()->IsMigrationRequired()) {

		return FALSE;
	}

	switch (nID) {

	case IDM_TOOL_BIND:

		return FALSE;

	case IDM_TOOL_UNBIND:
			
		return FALSE;

	case IDM_TOOL_ADDMIRROR:
			
		return FALSE;

	case IDM_TOOL_APPEND:

		return FALSE;

	case IDM_TOOL_SPAREADD:

		return FALSE;

	case IDM_TOOL_REPLACE_DEVICE: {

		NdasUiDbgCall( 4, _T("%s IsDefective= %d RootLogicalDev()->IsMountable() = %d\n"), 
					GetName(), IsDefective(), RootLogicalDev()->IsMountable() );

		if (!RootLogicalDev()->IsFaultTolerant()) {

			return FALSE;
		}

		if (RootLogicalDev()->IsBindOperatable(TRUE) == FALSE) {

			return FALSE;
		}

		if (RootLogicalDev()->NumberOfAliveChild()+1 < RootLogicalDev()->NumberOfChild()) {

			return FALSE;
		}

		if (RootLogicalDev()->IsMountable() == FALSE) {
			
			return FALSE;
		}
								  
		if (IsAlive() && !IsDefective()) {
		
			return FALSE;
		}

		return TRUE;
	}

	case IDM_TOOL_REMOVE_FROM_RAID: {

		if (RootLogicalDev()->IsFaultTolerant() == FALSE) {

			return FALSE;
		}

		if (RootLogicalDev()->IsBindOperatable(TRUE) == FALSE) {

			return FALSE;
		}

		if (IsMemberSpare() == FALSE && RootLogicalDev()->NumberOfAliveChild()+1 < RootLogicalDev()->NumberOfChild()) {

			return FALSE;
		}

		NDAS_DEVICE_ID ndasDeviceId = GetDeviceId();

		if (ndasDeviceId.Vid == NDAS_VID_NONE) {

			return FALSE;
		}

		if (RootLogicalDev()->IsMountable() == FALSE) {

			return FALSE;
		
		}

		if (IsAlive() == FALSE) {
			
			return TRUE;
		}

		if (IsMemberSpare()) {

			return TRUE;
		}

		if (IsDefective()) {

			return TRUE;
		}

		return FALSE;
	}

	case IDM_TOOL_CLEAR_DEFECTIVE:

		NdasUiDbgCall( 4, _T("%s IsDefective= %d, IsMemberSpare() = %d\n"), GetName(), IsDefective(), IsMemberSpare() );

		return IsBindOperatable() && IsDefective() && !IsMemberSpare();

	case IDM_TOOL_MIGRATE:

		return FALSE;

	case IDM_TOOL_RESET_BIND_INFO:

		return IsBindOperatable() && GetType() == NMT_CONFLICT;

	default:

		ATLASSERT(FALSE);
		return FALSE;		
	}
}

UINT CNBLogicalDev::FindIconIndex (UINT idicon, const UINT	*anIconIDs, int	nCount, int	iDefault /* = 0*/ )
{
	for (int i = 0; i < nCount; i++) {

		if (idicon == anIconIDs[i]) {

			return i;
		}
	}

	return (iDefault < nCount) ? iDefault : 0;
}

UINT CNBLogicalDev::GetIconIndex(const UINT *anIconIDs, int nCount)
{
	if (IsLeaf()) {

		if (!IsRoot()) {

			if (FlagOn(m_Status, NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED)) {

				return FindIconIndex(IDI_DEVICE_INVALID, anIconIDs, nCount);
			}

			if (FlagOn(m_NdasrInfo.LocalNodeFlags[m_Nidx], NRMX_NODE_FLAG_DEFECTIVE)) {

				return FindIconIndex(IDI_DEVICE_WARN, anIconIDs, nCount);
			}
		}
	
		if (IsAlive() == FALSE) {

			return FindIconIndex(IDI_DEVICE_FAIL, anIconIDs, nCount);
		}

		return FindIconIndex(IDI_DEVICE_BASIC, anIconIDs, nCount);
	}

	const struct {

		UINT32	Type; 
		UINT	IconID; 

	} ImageIndexTable[] = {

		NMT_INVALID,    IDI_DEVICE_FAIL,
		NMT_AOD,        IDI_DEVICE_BASIC,
		NMT_MIRROR,     IDI_DEVICE_BOUND,
		NMT_AGGREGATE,  IDI_DEVICE_BOUND,
		NMT_RAID0,      IDI_DEVICE_BOUND,
		NMT_RAID1,      IDI_DEVICE_BOUND,
		NMT_RAID1R2,	IDI_DEVICE_BOUND,
		NMT_RAID1R3,	IDI_DEVICE_BOUND,		
		NMT_RAID4,      IDI_DEVICE_BOUND,
		NMT_RAID4R2,	IDI_DEVICE_BOUND,
		NMT_RAID4R3,	IDI_DEVICE_BOUND,		
		NMT_RAID5,      IDI_DEVICE_BOUND,		
		NMT_VDVD,       IDI_DEVICE_BASIC,
		NMT_CDROM,      IDI_DEVICE_BASIC,
		NMT_FLASH,      IDI_DEVICE_BASIC,
		NMT_OPMEM,      IDI_DEVICE_BASIC,
		NMT_SAFE_RAID1, IDI_DEVICE_BASIC,
		NMT_CONFLICT,	IDI_DEVICE_WARN,
	};

	for (size_t i = 0; i < RTL_NUMBER_OF(ImageIndexTable); ++i) {

		if (GetType() == ImageIndexTable[i].Type) {

			return FindIconIndex( ImageIndexTable[i].IconID, anIconIDs, nCount );
		}
	}

	return 1;
}

UINT CNBLogicalDev::GetSelectIconIndex(const UINT *anIconIDs, int nCount)
{
	ATLASSERT(m_NdasDevUnit || m_LogicalDevMap.size());

	return GetIconIndex(anIconIDs, nCount);
}

BOOL CNBLogicalDev::InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ci, BOOL bWriteAccess)
{
	if (IsLeaf()) {

		return m_NdasDevUnit->InitConnectionInfo(ci, bWriteAccess);
	}

	return m_UptodateUnit->InitConnectionInfo(ci, bWriteAccess);
}
/*
* Broadcast HIX 'Unit device changed'
*/

VOID CNBLogicalDev::HixChangeNotify(LPCGUID guid)
{
	NDAS_ID ndasId;

	if (IsLeaf()) {

		m_NdasDevUnit->GetNdasID(&ndasId);

		NdasCommNotifyUnitDeviceChange( NDAS_DIC_NDAS_ID, ndasId.Id, m_NdasDevUnit->UnitNo(), guid );

		return;
	}

	for (UINT32 i = 0; i < m_LogicalDevMap.size(); i++) {

		if (m_LogicalDevMap[i]->IsAlive()) {

			m_LogicalDevMap[i]->HixChangeNotify(guid);
		}
	}
}

