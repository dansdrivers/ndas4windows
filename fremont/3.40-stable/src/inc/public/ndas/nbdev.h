// nbdev.h : interface of the CNBNdasDev class
//
/////////////////////////////////////////////////////////////////////////////

// revised by William Kim 24/July/2008

#pragma once

#include <list>

#include <ndas/ndastype.h>
#include <ndas/ndasctype.h>
#include <ndas/ndascomm_type.h>

#include <ndas/ndasdib.h>
#include <ndas/ndasop.h>
#include <ndas/ndasid.h>


class CNBNdasDev;
class CNBUnitDev;
class CNBLogicalDev;

typedef std::list<CNBNdasDev *>		NBNdasDevPtrList;
typedef std::list<CNBUnitDev *>		NBUnitDevPtrList;
typedef std::list<CNBLogicalDev *>	NBLogicalDevPtrList;

typedef std::map<DWORD, CNBNdasDev *>	NBNdasDevPtrMap;
typedef std::map<DWORD, CNBUnitDev *>	NBUnitDevPtrMap;
typedef std::map<DWORD, CNBLogicalDev *> CNBLogicalDevMap;


#define NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED		0x00000001
#define NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY		0x00000002
#define NDASBIND_UNIT_DEVICE_STATUS_MOUNTED				0x00000004
#define NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED		0x00000010
#define NDASBIND_UNIT_DEVICE_STATUS_UNKNOWN_RO_MOUNT	0x00000020

#define NDASBIND_LOGICAL_DEVICE_STATUS_DISCONNECTED			NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED
#define NDASBIND_LOGICAL_DEVICE_STATUS_NO_WRITE_KEY			NDASBIND_UNIT_DEVICE_STATUS_NO_WRITE_KEY
#define NDASBIND_LOGICAL_DEVICE_STATUS_MOUNTED				NDASBIND_UNIT_DEVICE_STATUS_MOUNTED
#define NDASBIND_LOGICAL_DEVICE_STATUS_NOT_REGISTERED		NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED
#define NDASBIND_LOGICAL_DEVICE_STATUS_UNKNOWN_MEDIA_TYPE	NDASBIND_UNIT_DEVICE_STATUS_UNKNOWN_RO_MOUNT

#define NRMX_RAID_STATE_INITIALIZING	0x00	
#define NRMX_RAID_STATE_NORMAL  		0x01
#define NRMX_RAID_STATE_OUT_OF_SYNC		0x02
#define NRMX_RAID_STATE_DEGRADED		0x03
#define NRMX_RAID_STATE_EMERGENCY		0x04
#define NRMX_RAID_STATE_FAILED			0x05
#define NRMX_RAID_STATE_TERMINATED		0x06

//  These macros are used to test, set and clear flags respectively

#ifndef FlagOn
#define FlagOn(_F, _SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F, SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F, _SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F, _SF)     ((_F) &= ~(_SF))
#endif

#define NRMX_NODE_FLAG_UNKNOWN		0x01 	// Node is not initialize by master.No other flag is possible with unknown status
#define NRMX_NODE_FLAG_RUNNING 		0x02	// UNKNOWN/RUNNING/STOP/DEFECTIVE is mutual exclusive.
#define NRMX_NODE_FLAG_STOP			0x04
#define NRMX_NODE_FLAG_DEFECTIVE	0x08	// Disk can be connected but ignore it 
											// because it is defective or not a raid member or replaced by spare.
#define NRMX_NODE_FLAG_OFFLINE		0x10

typedef struct _NDASR_INFO {

	DWORD				Type;					// NMT_*

	UINT64				Size;

	DWORD				TotalBitCount;
	DWORD				OosBitCount;

	GUID				NdasRaidId;
	GUID				ConfigSetId;			// not used

	UINT32				BlocksPerBit;			// From service. Service read from DIBv2.iSectorsPerBit

	UCHAR				ActiveDiskCount;		// Number of RAID member disk excluding spare.
	UCHAR				ParityDiskCount;
	UCHAR				SpareDiskCount;

	BOOLEAN				Striping;
	BOOLEAN				DistributedParity;

	UINT32				MaxDataSendLength;		// From service. 
	UINT32				MaxDataRecvLength;		// From service. 

	// Local node status. This status is reported to arbiter.

	UCHAR				LocalNodeFlags[NDAS_MAX_UNITS_IN_V2];  // NRMX_NODE_FLAG_RUNNING, DRIX_NODE_FLAG_*

	//	Flag that node change is propagated or not. Queue is not necessary assuming any lurn operation is held until status update is propagated.
	// 	DRAID_NODE_CHANGE_FLAG_*. 	
	//	CHANGED flag is set by ide thread and cleared by client thread after receiving(or failed to receive) raid update message.
	//	UPDATING flag is set by client when client detect CHANGED flag. Cleared when CHANGED flag is cleared.

	UCHAR				LocalNodeChanged[NDAS_MAX_UNITS_IN_V2];

	NDAS_RAID_META_DATA	Rmd;
	BOOLEAN				NodeIsUptoDate[NDAS_MAX_UNITS_IN_V2];
	UCHAR				UpToDateNode;

} NDASR_INFO, *PNDASR_INFO;


// 1 per 1 NDAS Device

class CNBNdasDev
{
private:

	// stores unit devices

	NBUnitDevPtrMap	m_LogicalDevMap;

	// device information

	NDAS_DEVICE_STATUS	m_Status;
	CString				m_strDeviceName;
	NDAS_DEVICE_ID		m_DeviceId;
	ACCESS_MASK			m_GrantedAccess;

	VOID UnitDeviceSet( CNBUnitDev *UnitDevice );
	VOID ClearUnitDevices(VOID);

public:
	
	CNBNdasDev (
		LPCTSTR				DeviceName, 
		PNDAS_DEVICE_ID		pDeviceId, 
		NDAS_DEVICE_STATUS	status, 
		ACCESS_MASK			GrantedAccess = GENERIC_READ | GENERIC_WRITE
		);

	~CNBNdasDev(VOID);

	UINT8 AddUnitDevices( UINT8 UnitCount = 0 );

	VOID AddUnitDevice(	UINT8 UnitNo, PCHAR NdasSimpleSerialNo, NDAS_UNITDEVICE_TYPE UnitDeviceType = NDAS_UNITDEVICE_TYPE_UNKNOWN );

	VOID GetNdasID( NDAS_ID *ndasId );
	NDAS_DEVICE_ID GetDeviceId() { return m_DeviceId; }

	CString GetName();

	ACCESS_MASK	GetGrantedAccess() { return m_GrantedAccess; }
	
	BOOL	IsAlive()	{ return ( m_Status == NDAS_DEVICE_STATUS_ONLINE || m_Status == NDAS_DEVICE_STATUS_CONNECTING ); }
	DWORD   GetStatus()	{ return m_Status; };

	UINT32 UnitDevicesCount() { return m_LogicalDevMap.size(); }
	CNBUnitDev *UnitDevice(int i) { return m_LogicalDevMap.count(i) ? m_LogicalDevMap[i] : NULL; }

	BOOL InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ci, BOOL bWriteAccess, UINT8 UnitNo = 0);
	
	BOOL Equals (BYTE Node[6], BYTE Vid) {

		return !memcmp(m_DeviceId.Node, Node, sizeof(m_DeviceId.Node)) && m_DeviceId.Vid == Vid; 
	}
};

// 1 per 1 NDAS Unit Device

class CNBUnitDev
{
private:

	CNBNdasDev				*m_NdasDevice;	// Can be NULL if unit device is not registered.
	CHAR					m_UnitSimpleSerialNo[NDAS_DIB_SERIAL_LEN];
	UINT8					m_UnitNo;

	UINT64					m_PhysicalCapacity;	// Set by Initialize.
	NDAS_UNITDEVICE_TYPE	m_UnitDeviceType;

	NDAS_DIB_V2				m_Dib; // NdasOpReadDIB
	NDAS_RAID_META_DATA		m_Rmd; // NdasOpRMDRead

	NDAS_META_DATA			m_MetaData;

	NDAS_LOGICALUNIT_DEFINITION m_Definition;		// Can be used for key of CNBLogicalDev

	DWORD					m_Status;

	CNBLogicalDev			*m_pLogicalDevice;

public:

	CNBUnitDev( CNBNdasDev *NdasDevice, UINT8 UnitNo, PCHAR UnitSimpleSerialNo, NDAS_UNITDEVICE_TYPE UnitDeviceType );

	BOOL Initialize(VOID);

	UINT8 UnitNo() { return m_UnitNo; };
	PCHAR UnitSimpleSerialNo() { return m_UnitSimpleSerialNo; };

	BOOL IsAlive()	{ return m_NdasDevice->IsAlive() && !(m_Status & NDASBIND_UNIT_DEVICE_STATUS_DISCONNECTED); }

	BOOL IsMemberOfRaid (VOID);

	UINT64 PhysicalCapacityInByte() { return m_PhysicalCapacity; }

	VOID SetLogicalDevice(CNBLogicalDev *LogicalDevice) { 
	
		ATLASSERT( !LogicalDevice || !m_pLogicalDevice );

		m_pLogicalDevice = LogicalDevice;
	}

	DWORD	UpdateStatus();

	DWORD	GetStatus() { return m_Status; }

	CString GetName();
	LPCTSTR GetName(UINT *UnitNo);

	VOID GetNdasID(NDAS_ID *NdasId) {
		
		return m_NdasDevice->GetNdasID(NdasId);
	}

	NDAS_DEVICE_ID GetDeviceId() { return m_NdasDevice->GetDeviceId(); }

	DWORD GetAccessMask();
	
	BOOL InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ci, BOOL bWriteAccess);
	
	CNBNdasDev *GetNdasDevice()	{ return m_NdasDevice; }

	NDAS_DIB_V2 *DIB(VOID)			{ return &m_Dib; }
	NDAS_RAID_META_DATA *RMD(VOID)	{ return &m_Rmd; }

	NDAS_LOGICALUNIT_DEFINITION *Definition(VOID) { return &m_Definition; }


	BOOL Equals( BYTE Node[6], BYTE Vid, PCHAR UnitSimpleSerialNo );
};


// 1 per 1 Unit

class CNBLogicalDev
{
private:

	CNBLogicalDev		*m_ParentLogicalDev;

	CNBUnitDev			*m_NdasDevUnit;

	UINT32				m_Nidx;	// Determined when added logical device.
	UINT8				m_Ridx;

	CNBLogicalDevMap	m_LogicalDevMap;

	CNBLogicalDev		*m_UptodateUnit;	// Metadata will be read based on this unit.

	DWORD				m_Status;

	NDASR_INFO			m_NdasrInfo;

	UCHAR				m_NdasrState;			// NRMX_RAID_STATE_INITIALIZING, NRMX_RAID_STATE_*
	UINT8				m_OutOfSyncRoleIndex;	// NO_OUT_OF_SYNC_ROLE when there is no out-of-sync node.

#define NO_OUT_OF_SYNC_ROLE ((UINT8)-1)

	BOOL				m_FixRequired;
	BOOL				m_MigrationRequired;

public:

	CNBLogicalDev( CNBUnitDev *UnitDevice );
	~CNBLogicalDev();

	BOOL IsMember( CNBLogicalDev *pUnitDevice );

	BOOL IsMemberOfRaid(VOID) {

		ATLASSERT( IsRoot() && IsLeaf() );
		return m_NdasDevUnit->IsMemberOfRaid(); 
	}

	BOOL IsAlive() { 

		ATLASSERT( IsLeaf() );

		return m_NdasDevUnit->IsAlive(); 
	}

	PCHAR  UnitSimpleSerialNo() { return m_NdasDevUnit->UnitSimpleSerialNo(); };

	VOID UnitDeviceSet( CNBLogicalDev *pUnitDevice, UINT32 Sequence );

	__declspec(dllexport) BOOL IsRoot() { return m_ParentLogicalDev ? FALSE : TRUE; }
	__declspec(dllexport) BOOL IsLeaf() { return m_NdasDevUnit ? TRUE : FALSE; }

	CNBLogicalDev *RootLogicalDev() {

		ATLASSERT( !IsRoot() );
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		return m_ParentLogicalDev;
	}

	DWORD UpdateStatus( BOOL UpdateUnitDevice = FALSE );
	
	DWORD CNBLogicalDev::GetStatus() {

		return m_Status;
	}

	__declspec(dllexport) UINT GetStatusId(VOID);

	BOOL IsMissingMember(VOID) { 

		ATLASSERT( !IsRoot() );

		return FlagOn( m_Status, NDASBIND_UNIT_DEVICE_STATUS_NOT_REGISTERED ); 
	}

	CString GetName();
	__declspec(dllexport) LPCTSTR GetName(UINT *UnitNo);

	__declspec(dllexport) VOID GetNdasID(NDAS_ID *NdasId);

	NDAS_DEVICE_ID GetDeviceId(VOID) {

		ATLASSERT( IsLeaf() );

		return m_NdasDevUnit->GetDeviceId();
	}

	__declspec(dllexport) BOOL IsHDD();
	DWORD GetAccessMask();

	NDAS_DIB_V2 *DIB() {

		return m_NdasDevUnit ? m_NdasDevUnit->DIB() : m_UptodateUnit->m_NdasDevUnit->DIB(); 
	}

	UINT32 CNBLogicalDev::GetType() {

		return DIB()->iMediaType;
	}

	NDAS_RAID_META_DATA *RMD() { 

		return m_NdasDevUnit ? m_NdasDevUnit->RMD() : m_UptodateUnit->m_NdasDevUnit->RMD(); 
	}

	UINT32 NumberOfRaidMember(VOID) {

		ATLASSERT(IsRoot() && !IsLeaf());

		return DIB()->nDiskCount + DIB()->nSpareCount;
	}

	VOID InitNdasrInfo(VOID);

	NDASR_INFO *NdasRinfo() { 

		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );
		return &m_NdasrInfo; 
	}

	__declspec(dllexport) UCHAR NdasrStatus(VOID) { return m_NdasrState; }	// NRMX_RAID_STATE_INITIALIZING, NRMX_RAID_STATE_*

	VOID SetRidx(UINT8 Ridx) { 

		ATLASSERT( !IsRoot() );
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		m_Ridx = Ridx;
		return;
	}

	__declspec(dllexport) UINT32 NumberOfChild(VOID) {

		ATLASSERT(IsRoot() && !IsLeaf());

		return m_LogicalDevMap.size();
	}

	__declspec(dllexport) UINT32 NumberOfAliveChild(VOID) {

		UINT32 aliveChildCount=0;

		ATLASSERT(IsRoot() && !IsLeaf());

		for (UINT32 nidx=0; nidx<m_LogicalDevMap.size(); nidx++) {

			if (m_LogicalDevMap[nidx]->IsAlive()) {

				aliveChildCount++;
			}
		}

		return aliveChildCount;
	}

	__declspec(dllexport) CNBLogicalDev *Child(int i) { 
		
		return m_LogicalDevMap.count(i) ? m_LogicalDevMap[i] : NULL; 
	}
	
	__declspec(dllexport) CNBLogicalDev *UptodateChild(VOID) { 
		
		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );
		return m_UptodateUnit;
	}

	UINT64 PhysicalCapacityInByte(VOID) { 
		
		ATLASSERT(IsLeaf());
		return m_NdasDevUnit->PhysicalCapacityInByte(); 
	}

	__declspec(dllexport) UINT64 LogicalCapacityInByte();

	UINT32 Nidx(VOID) {
		
		ATLASSERT( !IsRoot() );
		return m_Nidx;
	}		

	BOOL Equals (BYTE Node[6], BYTE Vid, PCHAR UnitSimpleSerialNo) {
		
		ATLASSERT( IsLeaf() );
		return m_NdasDevUnit->Equals( Node, Vid, UnitSimpleSerialNo ); 
	}

	__declspec(dllexport) DWORD  RaidMemberStatus(VOID) {

		ATLASSERT( !IsRoot() );
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		return m_ParentLogicalDev->m_NdasrInfo.Rmd.UnitMetaData[m_Ridx].UnitDeviceStatus; 
	}

	BOOL IsMemberSpare(VOID) {

		ATLASSERT( !IsRoot() );
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		if (FlagOn(RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_SPARE) ||
			FlagOn(RaidMemberStatus(), NDAS_UNIT_META_BIND_STATUS_REPLACED_BY_SPARE)) {

			return TRUE;
		
		} else {

			return FALSE;
		}
	}

	BOOL IsMemberOffline(VOID) {

		ATLASSERT( !IsRoot() );
		ATLASSERT( m_ParentLogicalDev->NdasrStatus() != NRMX_RAID_STATE_INITIALIZING );

		if (RaidMemberStatus() & NDAS_UNIT_META_BIND_STATUS_OFFLINE) {

			return TRUE;
		
		} else {

			return FALSE;
		}
	}

	__declspec(dllexport) BOOL IsDefective(VOID);

	BOOL IsFaultTolerant(VOID) {

		ATLASSERT( IsRoot() && !IsLeaf() ); 
		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

		return m_NdasrInfo.ParityDiskCount ? TRUE : FALSE;
	}

	__declspec(dllexport) BOOL IsFixRequired(VOID) { 

		ATLASSERT( IsRoot() && !IsLeaf() ); 
		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

		return m_FixRequired;
	}

	__declspec(dllexport) BOOL IsMigrationRequired(VOID) { 

		ATLASSERT( IsRoot() && !IsLeaf() ); 
		ATLASSERT( m_NdasrState != NRMX_RAID_STATE_INITIALIZING );

		return m_MigrationRequired;
	}

	__declspec(dllexport) BOOL IsMountable(BOOL IncludeEmergency = FALSE);

	__declspec(dllexport) BOOL IsBindOperatable(BOOL AliveMeberOnly = FALSE);	
	NBLogicalDevPtrList GetBindOperatableDevices();

	__declspec(dllexport) BOOL IsCommandAvailable(int nID);

	UINT FindIconIndex( UINT idicon, const UINT	*anIconIDs, int	nCount, int	iDefault = 0 );
	__declspec(dllexport) UINT GetIconIndex(const UINT *anIconIDs, int nCount);
	__declspec(dllexport) UINT GetSelectIconIndex(const UINT *anIconIDs, int nCount);

	BOOL InitConnectionInfo(NDASCOMM_CONNECTION_INFO *ci, BOOL bWriteAccess);
	VOID HixChangeNotify(LPCGUID guid);
};
