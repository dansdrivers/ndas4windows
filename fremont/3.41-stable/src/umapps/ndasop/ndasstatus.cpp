// ndasstatus.cpp : implementation of the CNdasStatus class
//
/////////////////////////////////////////////////////////////////////////////

// revised by William Kim 24/July/2008

#include "stdafx.h"
#include "resource.h"

#include <ndas/ndascomm.h>
#include <ndas/ndasop.h>
#include <ndas/appconf.h>

#include <ndas/ndasstatus.h>

LONG DbgLevelNdasStaus = DBG_LEVEL_NDAS_STATUS;

#define NdasUiDbgCall(l,x,...) do {							\
    if (l <= DbgLevelNdasStaus) {							\
        ATLTRACE("|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		ATLTRACE (x,__VA_ARGS__);							\
    } 														\
} while(0)

CNdasStatus::CNdasStatus() :
	m_nAssigned(0)
{
	UINT test = 0;

	NdasUiDbgCall( 1, _T("In %d\n"), test);

	m_hEventThread = CreateEvent(NULL, FALSE, FALSE, NULL);

	ATLASSERT( m_hEventThread != NULL );

	return;
}

CNdasStatus::~CNdasStatus()
{
	ClearDevices();
	CloseHandle(m_hEventThread);
	
	return;
}


// Creates unit devices and adds them to pNBNdasDev.

unsigned
CNdasStatus::ThreadAddUnitDevices (
	CNBNdasDev *pNBNdasDev
	)
{
	ATLASSERT( NULL != pNBNdasDev );

	unsigned uReturn = 0;

	uReturn = pNBNdasDev->AddUnitDevices();

	// Complete

	::InterlockedDecrement(&m_nAssigned);
	::SetEvent(m_hEventThread);

	return uReturn;
}

// Launch add unit devices each CNBNdasDev

struct NdasDeviceInitializer :
	public std::unary_function<CNBNdasDev *, void>
{
private:
	CNdasStatus *m_NdasStatus;
public:

	NdasDeviceInitializer(CNdasStatus *NdasStatus) 
		: m_NdasStatus(NdasStatus) {}

	void operator() (CNBNdasDev *pNBNdasDev) const
	{
		ATLASSERT( NULL != m_NdasStatus );
		ATLASSERT( NULL != pNBNdasDev );

		HANDLE hThread = NULL;

		if (!pNBNdasDev->IsAlive()) {

			pNBNdasDev->AddUnitDevices(1);
			goto out;
		}

		// Create thread to add & init unit device.

		NDAS_STATUS_THREAD_CONTEXT *pThreadContext =
			(NDAS_STATUS_THREAD_CONTEXT *)::malloc(sizeof(NDAS_STATUS_THREAD_CONTEXT));

		if (!pThreadContext) {

			ATLASSERT(FALSE);

			pNBNdasDev->AddUnitDevices();
			goto out;
		}

		pThreadContext->NdasStatus = m_NdasStatus;
		pThreadContext->Parameter  = (void *)pNBNdasDev;

		hThread = (HANDLE)::_beginthreadex( NULL,
										    0,
										    CNdasStatus::_ThreadAddUnitDevices,
										    pThreadContext,
										    0,
										    NULL );

		if (!hThread) {

			ATLASSERT(FALSE);

			pNBNdasDev->AddUnitDevices();
			::free(pThreadContext);
		}

out:

		//If NDAS device is not alive, add dummy unit device.

		if (hThread == NULL) {

			// Non threaded : decrease here

			::InterlockedDecrement(&m_NdasStatus->m_nAssigned);
			::SetEvent( m_NdasStatus->m_hEventThread );
		
		} else {

			// thread will decrease m_nAssigned
			
			::CloseHandle(hThread);
		}
	}
};

// create and add new NDAS devices to list

BOOL 
CALLBACK
CNdasStatus::EnumDevicesCallBack (
	PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry,
	LPVOID						lpContext
	)
{
	NDAS_STATUS_THREAD_CONTEXT *context = (NDAS_STATUS_THREAD_CONTEXT *)lpContext;
	CNdasStatus *NdasStatus = context->NdasStatus;

	ATLASSERT( NULL != NdasStatus );

	// check device connection status
	
	NDAS_DEVICE_STATUS status; 
	
	NDAS_DEVICE_ERROR lastError;

	BOOL fSuccess = ::NdasQueryDeviceStatus( lpEnumEntry->SlotNo, &status, &lastError );

	if (!fSuccess) {

		ATLASSERT(FALSE);
		goto out;
	}

	// Create CNBNdasDev and push it into list

	CNBNdasDev		*pNdasDevice;
	NDAS_DEVICE_ID  DeviceId;
	NDASID_EXT_DATA IdExtData;
	
	ZeroMemory( &DeviceId, sizeof(NDAS_DEVICE_ID) );

	::NdasIdStringToDeviceEx( lpEnumEntry->szDeviceStringId,
							  &DeviceId,
							  NULL,
							  &IdExtData );

	DeviceId.Vid = IdExtData.Vid;

	pNdasDevice = new CNBNdasDev( CString(lpEnumEntry->szDeviceName),
									 &DeviceId,
									 status,
									 lpEnumEntry->GrantedAccess );

	if (!pNdasDevice) {

		ATLASSERT(FALSE);
		goto out;
	}

	NdasStatus->m_listNdasDevices.push_back(pNdasDevice);

out:

	return TRUE;
}

BOOL CNdasStatus::RefreshStatus(CProgressBarCtrl *m_wndRefreshProgress)
{
	ClearDevices();

	ATLASSERT( m_listNdasDevices.begin() == m_listNdasDevices.end() );
	ATLASSERT( m_listMissingNdasDevices.begin() == m_listMissingNdasDevices.end() );
	ATLASSERT( m_listLeafLogicalDevs.begin() == m_listLeafLogicalDevs.end() );
	ATLASSERT( m_LogicalDevList.begin() == m_LogicalDevList.end() );

	// Step 1. Find all the unit devices and initialize them.

	// get NDAS device list

	NDAS_STATUS_THREAD_CONTEXT context;

	context.NdasStatus = this;
	context.Parameter  = NULL;

	if (!NdasEnumDevices(EnumDevicesCallBack, &context)) {

		return FALSE;
	}

	// set progress bar
	
	m_nAssigned = m_listNdasDevices.size();

	m_wndRefreshProgress->SetRange32(0, m_nAssigned);
	m_wndRefreshProgress->SetStep(1);
	m_wndRefreshProgress->SetPos(0);

	// retrieve all the device & unit device information

	context.Parameter = reinterpret_cast<LPVOID>(&m_listNdasDevices);

	::ResetEvent(m_hEventThread);
	
	std::for_each( m_listNdasDevices.begin(), m_listNdasDevices.end(), NdasDeviceInitializer(this) );

	for (;;) {

		if (WAIT_OBJECT_0 != ::WaitForSingleObject(m_hEventThread, INFINITE)) {

			ATLASSERT( FALSE );
			return FALSE;
		}

		m_wndRefreshProgress->SetPos(m_listNdasDevices.size() - m_nAssigned);

		if (::InterlockedCompareExchange(&m_nAssigned, 0, 0) == 0) {

			break;
		}
	}

	// NDAS devices and unit devices are now initialized

	// Step 2. create leaf logical devices

	NdasUiDbgCall( 4, _T("step 2\n") );

	// for each NDAS device

	NBNdasDevPtrList::iterator itrNdasDevice;

	for (itrNdasDevice = m_listNdasDevices.begin();
		 itrNdasDevice != m_listNdasDevices.end();
		 itrNdasDevice++) {

		CNBNdasDev *pNBNdasDev;

		pNBNdasDev = *itrNdasDevice;

		for (UINT32 i = 0; i < pNBNdasDev->UnitDevicesCount(); i++) {

			CNBUnitDev *unitDevice;

			unitDevice = pNBNdasDev->UnitDevice(i);

			if (!unitDevice) {

				ATLASSERT( FALSE );
				continue;
			}

			CNBLogicalDev *logicalDev = new CNBLogicalDev(unitDevice);

			NdasUiDbgCall( 4, _T("use leaf CNBLogicalDev(%p) : %s\n"), logicalDev, logicalDev->GetName() );

			m_listLeafLogicalDevs.push_back(logicalDev);
		}
	}

	// Step 3. create group logical devices

	NdasUiDbgCall( 4, _T("step 3\n") );

	NBLogicalDevPtrList::iterator itrLeafLogicalDev;

	for (itrLeafLogicalDev = m_listLeafLogicalDevs.begin();
		 itrLeafLogicalDev != m_listLeafLogicalDevs.end();
		 itrLeafLogicalDev++) {

		if (!(*itrLeafLogicalDev)->IsMemberOfRaid()) {

			continue;
		}

		NBLogicalDevPtrList::iterator itrLogicalDev;
			
		for (itrLogicalDev = m_LogicalDevList.begin();
			 itrLogicalDev != m_LogicalDevList.end();
			 itrLogicalDev++) {

			if ((*itrLogicalDev)->IsMember((*itrLeafLogicalDev))) {

				// we found the logical device. add unit device here

				NdasUiDbgCall( 4, _T("use CNBLogicalDev(%p) : %s\n"),
							(*itrLeafLogicalDev), (*itrLeafLogicalDev)->GetName() );

				(*itrLogicalDev)->UnitDeviceSet( (*itrLeafLogicalDev), 
												   (*itrLeafLogicalDev)->DIB()->iSequence );

				break;
			}
		}

		if (!(*itrLeafLogicalDev)->IsRoot()) {

			continue;
		}

		CNBLogicalDev *raidLogicalDev = new CNBLogicalDev(NULL);

		m_LogicalDevList.push_back(raidLogicalDev);

		NdasUiDbgCall( 4, _T("new group CNBLogicalDev(%p) : %s\n"),
					(*itrLeafLogicalDev), (*itrLeafLogicalDev)->GetName() );

		raidLogicalDev->UnitDeviceSet( (*itrLeafLogicalDev), (*itrLeafLogicalDev)->DIB()->iSequence );
	}

	// Step 4. fill missing member

	NdasUiDbgCall( 4, _T("step 4\n") );

	NBLogicalDevPtrList::iterator itrLogicalDev;

	for (itrLogicalDev = m_LogicalDevList.begin();
		 itrLogicalDev != m_LogicalDevList.end();
		 itrLogicalDev++) {

		CNBLogicalDev *raidLogicalDev = *itrLogicalDev;

		ATLASSERT( !raidLogicalDev->IsLeaf() );

		for (UINT32 i =0; i<raidLogicalDev->NumberOfRaidMember(); i++) {

			if (raidLogicalDev->Child(i) != NULL) {

				continue;
			}

			// Now we found a missing member.

			NdasUiDbgCall( 4, _T("Filling up empty unit\n") );
			
			// Find NDAS device for this missing member

			for (itrLeafLogicalDev = m_listLeafLogicalDevs.begin();
				 itrLeafLogicalDev != m_listLeafLogicalDevs.end();
				 itrLeafLogicalDev++) {

				if (!(*itrLeafLogicalDev)->IsRoot()) {

					continue;
				}

				if ((*itrLeafLogicalDev)->Equals(raidLogicalDev->DIB()->UnitLocation[i].MACAddr,
												 raidLogicalDev->DIB()->UnitLocation[i].VID,
												 raidLogicalDev->DIB()->UnitSimpleSerialNo[i])) {

					if ((*itrLeafLogicalDev)->IsAlive()) {
					
						ATLASSERT ( (*itrLogicalDev)->IsMember(*itrLeafLogicalDev) == FALSE );
						continue;
					}
					
					NdasUiDbgCall( 4, _T("set missing member CNBLogicalDev(%p) : %s\n"),
								(*itrLeafLogicalDev), (*itrLeafLogicalDev)->GetName() );

					raidLogicalDev->UnitDeviceSet( (*itrLeafLogicalDev), i );
				}
			}

			if (raidLogicalDev->Child(i) != NULL) {

				// filled missing member. just update raid status

				continue;
			}

			NdasUiDbgCall( 4, _T("create missing device\n") );

			CNBNdasDev *missingNdasDevice = NULL;
			CString str;

			// the unit device is not found in NDAS device list
			// This device is not registered device. Create one from RAID info
		
			str.LoadString(IDS_DEV_NAME_NOT_REGISTERED);

			NDAS_DEVICE_ID	ndasDeviceId;

			memcpy( ndasDeviceId.Node, raidLogicalDev->DIB()->UnitLocation[i].MACAddr, 6 );
			ndasDeviceId.Reserved = 0;
			ndasDeviceId.Vid = raidLogicalDev->DIB()->UnitLocation[i].VID;

			missingNdasDevice = new CNBNdasDev( (PCTSTR)str, &ndasDeviceId, NDAS_DEVICE_STATUS_NOT_REGISTERED );

			missingNdasDevice->AddUnitDevice( 0, raidLogicalDev->DIB()->UnitSimpleSerialNo[i], NDAS_UNITDEVICE_TYPE_UNKNOWN );

			m_listMissingNdasDevices.push_back(missingNdasDevice);
							
			//Temp fix: NDAS_RAID_MEMBER_FLAG_NOT_REGISTERED cannot be set by ndasop so set here.
			
			CNBLogicalDev *logicalDev = new CNBLogicalDev( missingNdasDevice->UnitDevice(0) );

			m_listLeafLogicalDevs.push_back(logicalDev);

			raidLogicalDev->UnitDeviceSet( logicalDev, i );
		}
	}

	// Step 5 Add single logical device

	NdasUiDbgCall( 4, _T("step 5\n") );

	for (itrLeafLogicalDev = m_listLeafLogicalDevs.begin();
		 itrLeafLogicalDev != m_listLeafLogicalDevs.end();
		 itrLeafLogicalDev++) {

		if (!(*itrLeafLogicalDev)->IsRoot()) {

			continue;
		}

		m_LogicalDevList.push_back((*itrLeafLogicalDev));

		NdasUiDbgCall( 4, _T("add single CNBLogicalDev(%p) : %s\n"),
					(*itrLeafLogicalDev), (*itrLeafLogicalDev)->GetName() );
	}

	return TRUE;
}

void 
CNdasStatus::ClearDevices()
{
	NBLogicalDevPtrList::iterator itrLogicalDev;

	for (itrLogicalDev = m_LogicalDevList.begin(); 
		 itrLogicalDev != m_LogicalDevList.end(); 
		 ++itrLogicalDev) {

		ATLASSERT(*itrLogicalDev);

		if (!(*itrLogicalDev)->IsLeaf()) {


			NdasUiDbgCall( 4, _T("delete CNBLogicalDev(%p) : %s\n"),
						(*itrLogicalDev), (*itrLogicalDev)->GetName() );
	
			::delete (*itrLogicalDev);
		}
	}

	m_LogicalDevList.clear();

	ATLASSERT( m_LogicalDevList.begin() == m_LogicalDevList.end() );

	for (itrLogicalDev = m_listLeafLogicalDevs.begin(); 
		itrLogicalDev != m_listLeafLogicalDevs.end(); 
		++itrLogicalDev) {

		NdasUiDbgCall( 4, _T("delete CNBLogicalDev(%p) : %s\n"),
					(*itrLogicalDev), (*itrLogicalDev)->GetName() );

		ATLASSERT(*itrLogicalDev);
		ATLASSERT( (*itrLogicalDev)->IsLeaf() );

		::delete (*itrLogicalDev);
		NdasUiDbgCall( 4, _T("delete\n") );
	}

	m_listLeafLogicalDevs.clear();

	ATLASSERT( m_listLeafLogicalDevs.begin() == m_listLeafLogicalDevs.end() );

	NBNdasDevPtrList::iterator itDevice;

	for (itDevice = m_listNdasDevices.begin();
		 itDevice != m_listNdasDevices.end();
		 ++itDevice) {

		ATLASSERT(*itDevice);
		
		::delete (*itDevice);
	}

	m_listNdasDevices.clear();

	for (itDevice = m_listMissingNdasDevices.begin();
		 itDevice != m_listMissingNdasDevices.end();
		 ++itDevice) {

		ATLASSERT(*itDevice);
		
		::delete (*itDevice);
	}

	m_listMissingNdasDevices.clear();
}

///////////////////////////////////////////////////////////////////////////////
//
// Implementation of command handling methods
//
///////////////////////////////////////////////////////////////////////////////

NBLogicalDevPtrList CNdasStatus::GetOperatableSingleDevices()
{
	NBLogicalDevPtrList listUnitDevicesSingle;
	CNBLogicalDev *logicalDev;

	for (NBLogicalDevPtrList::iterator itrLogicalDev = m_LogicalDevList.begin();
		itrLogicalDev != m_LogicalDevList.end(); itrLogicalDev++) {

		logicalDev = *itrLogicalDev;

		if (logicalDev->IsLeaf()			&& 
			logicalDev->IsRoot()			&&
			logicalDev->IsBindOperatable()) {

			listUnitDevicesSingle.push_back(logicalDev);
		}
	}

	return listUnitDevicesSingle;
}

NDAS_BIND_STATUS CNdasStatus::OnBind(NBLogicalDevPtrList *LogicalDevList, UINT DiskCount, NDAS_MEDIA_TYPE BindType)
{
	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[DiskCount];

	ZeroMemory( connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * DiskCount );

	UINT i;
	NBLogicalDevPtrList::const_iterator itr;
	
	for (i = 0, itr = LogicalDevList->begin(); itr != LogicalDevList->end(); ++itr, i++ ) {

		if (!(*itr)->InitConnectionInfo(&connectionInfo[i], TRUE)) {

			ATLASSERT( FALSE );
			delete[] connectionInfo;

			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	for (i = 0, itr = LogicalDevList->begin(); itr != LogicalDevList->end(); ++itr, i++ ) {

		if ((*itr)->IsCommandAvailable(IDM_TOOL_BIND) == FALSE) {
			
			ATLASSERT(FALSE);
			delete [] connectionInfo;
			
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}	
	}

	DWORD dwUserSpace = 0;

	if (!pGetAppConfigValue(_T("UserSpace"), &dwUserSpace)) {

		dwUserSpace = 0;
	}

	HRESULT bindResult = NdasOpBind( connectionInfo, DiskCount, BindType, dwUserSpace );

	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 
	
	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	LPCGUID hostGuid = pGetNdasHostGuid();

	for (i = 0; i < DiskCount; i++ ) {

		BOOL success = NdasCommNotifyUnitDeviceChange( NDAS_DIC_NDAS_ID,
													   connectionInfo[i].Address.NdasId.Id,
													   connectionInfo[i].UnitNo,
													   hostGuid );

		if (!success) {

			NdasUiDbgCall( 4, "NdasCommNotifyUnitDeviceChange failed, error=0x%X\n", ::GetLastError() );
		}
	}

	delete [] connectionInfo;

	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnUnBind( CNBLogicalDev *LogicalDev, BOOL Partial, UINT32 *FailChildIdx )
{
	NDAS_BIND_STATUS bindStatus;

	NBLogicalDevPtrList	 childLogicalDevList = LogicalDev->GetBindOperatableDevices();

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[childLogicalDevList.size()];

	ZeroMemory(connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * childLogicalDevList.size());

	if (LogicalDev->NumberOfChild() != childLogicalDevList.size()) {

		if (Partial == FALSE) {

			ATLASSERT(FALSE);
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	UINT i;
	NBLogicalDevPtrList::const_iterator itr;
	
	for (i = 0, itr = childLogicalDevList.begin(); itr != childLogicalDevList.end(); ++itr, i++ ) {

		if ((*itr)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);
			delete[] connectionInfo;

			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (LogicalDev->IsCommandAvailable(IDM_TOOL_UNBIND) == FALSE) {
			
		ATLASSERT(FALSE);
		delete [] connectionInfo;

		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}	

	HRESULT bindResult = NdasOpBind( connectionInfo, childLogicalDevList.size(), NMT_SINGLE, 0 );

	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	for (NBLogicalDevPtrList::iterator itr = childLogicalDevList.begin();
		itr != childLogicalDevList.end(); 
		itr++) {

		(*itr)->HixChangeNotify(pGetNdasHostGuid());
	}

	delete [] connectionInfo;

	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnAddMirror(CNBLogicalDev *LogicalDev, CNBLogicalDev *DevToAdd)
{
	NBLogicalDevPtrList	 targetLogicalDevList;

	targetLogicalDevList.push_back(LogicalDev);
	targetLogicalDevList.push_back(DevToAdd);

	return OnBind( &targetLogicalDevList, 2, NMT_SAFE_RAID1 );
}

NDAS_BIND_STATUS CNdasStatus::OnAppend(CNBLogicalDev *LogicalDev, CNBLogicalDev *DevToAppend)
{
	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = NULL;
	NDASCOMM_CONNECTION_INFO *appendConnectionInfo = NULL;

	UINT devCount;

	if (LogicalDev->IsRoot() && LogicalDev->IsLeaf()) {

		devCount = 2;
	
	} else {

		devCount = LogicalDev->NumberOfChild() + 1;
	}

	connectionInfo = new NDASCOMM_CONNECTION_INFO[devCount];
	ZeroMemory( connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) );

	if (LogicalDev->IsRoot() && LogicalDev->IsLeaf()) {

		if (LogicalDev->InitConnectionInfo(&connectionInfo[0], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}

	} else {

		for (UINT i = 0; i < LogicalDev->NumberOfChild(); i++ ) {

			if (LogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

				ATLASSERT(FALSE);

				delete[] connectionInfo;
				return NDAS_BIND_STATUS_UNSUCCESSFUL;
			}
		}
	}

	if (DevToAppend->InitConnectionInfo(&connectionInfo[devCount-1], TRUE) == FALSE) {

		ATLASSERT(FALSE);

		delete[] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	if (LogicalDev->IsCommandAvailable(IDM_TOOL_APPEND) == FALSE) {
			
		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}	

	HRESULT bindResult = NdasOpBind( connectionInfo, devCount, NMT_SAFE_AGGREGATE, 0 );

	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	DevToAppend->HixChangeNotify(pGetNdasHostGuid());
	LogicalDev->HixChangeNotify(pGetNdasHostGuid());

	delete [] connectionInfo;
	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnSpareAdd(CNBLogicalDev *LogicalDev, CNBLogicalDev *DevToAdd)
{
	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[LogicalDev->NumberOfChild()+1];
	ZeroMemory(connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * LogicalDev->NumberOfChild()+1);

	UINT i;
	
	for (i = 0; i < LogicalDev->NumberOfChild(); i++ ) {

		if (LogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (DevToAdd->InitConnectionInfo(&connectionInfo[LogicalDev->NumberOfChild()], TRUE) == FALSE) {

		ATLASSERT( FALSE );

		delete[] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	if (LogicalDev->IsCommandAvailable(IDM_TOOL_SPAREADD) == FALSE) {
			
		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	HRESULT bindResult = NdasOpBind( connectionInfo, LogicalDev->NumberOfChild()+1, NMT_SAFE_RAID_ADD, 0 );

	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	DevToAdd->HixChangeNotify(pGetNdasHostGuid());
	LogicalDev->HixChangeNotify(pGetNdasHostGuid());

	delete [] connectionInfo;
	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnReplaceDevice(CNBLogicalDev *DevToReplaced, CNBLogicalDev *DevToReplace) 
{
	if (DevToReplaced == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	CNBLogicalDev *rootLogicalDev = DevToReplaced->RootLogicalDev();

	if (rootLogicalDev == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[rootLogicalDev->NumberOfChild()+1];

	ZeroMemory( connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * rootLogicalDev->NumberOfChild()+1 );

	UINT i;

	for (i = 0; i < rootLogicalDev->NumberOfChild(); i++ ) {

		if (rootLogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (DevToReplace->InitConnectionInfo(&connectionInfo[rootLogicalDev->NumberOfChild()], TRUE) == FALSE) {

		ATLASSERT( FALSE );

		delete[] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	if (DevToReplaced->IsCommandAvailable(IDM_TOOL_REPLACE_DEVICE) == FALSE) {
			
		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}
	
	// Update DIB & RMD including new Unit
	
	HRESULT bindResult = NdasOpBind( connectionInfo, 
									 rootLogicalDev->NumberOfChild()+1, 
									 NMT_SAFE_RAID_REPLACE, 
									 0, 
									 DevToReplaced->Nidx() );
	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
		goto out;
	}

	if (DevToReplaced->IsAlive()) {

		// Convert to basic disk.
		
		NDASCOMM_CONNECTION_INFO selectedConnectionInfo;
		DevToReplaced->InitConnectionInfo( &selectedConnectionInfo, TRUE );
		
		NdasOpBind(	&selectedConnectionInfo, 1, NMT_SINGLE, 0 );
	}

out:

	DevToReplaced->HixChangeNotify(pGetNdasHostGuid());
	DevToReplace->HixChangeNotify(pGetNdasHostGuid());
	rootLogicalDev->HixChangeNotify(pGetNdasHostGuid());

	return bindStatus;
}

// Reconfigure RAID without this disk. If spare disk does not exist, we cannot reconfigure.

NDAS_BIND_STATUS CNdasStatus::OnRemoveFromRaid( CNBLogicalDev *ChildToRemove )
{
	if (ChildToRemove == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	CNBLogicalDev *rootLogicalDev = ChildToRemove->RootLogicalDev();

	if (rootLogicalDev == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[rootLogicalDev->NumberOfChild()];

	ZeroMemory( connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * rootLogicalDev->NumberOfChild() );

	UINT i;

	for (i = 0; i < rootLogicalDev->NumberOfChild(); i++ ) {

		if (rootLogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (ChildToRemove->IsCommandAvailable(IDM_TOOL_REMOVE_FROM_RAID) == FALSE) {
			
		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	HRESULT bindResult = NdasOpBind( connectionInfo, 
									 rootLogicalDev->NumberOfChild(), 
									 NMT_SAFE_RAID_REMOVE, 
									 0, 
									 ChildToRemove->Nidx() );
	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
		goto out;
	}

	if (ChildToRemove->IsAlive()) {

		// Convert to basic disk.
		
		NDASCOMM_CONNECTION_INFO selectedConnectionInfo;
		
		BOOL result;

		result = ChildToRemove->InitConnectionInfo( &selectedConnectionInfo, TRUE );
		
		NdasOpBind(	&selectedConnectionInfo, 1, NMT_SINGLE, 0 );
	}

out:

	rootLogicalDev->HixChangeNotify(pGetNdasHostGuid());

	delete [] connectionInfo;
	return bindStatus;
}


NDAS_BIND_STATUS CNdasStatus::OnClearDefect(CNBLogicalDev *ChildToClear)
{
	if (ChildToClear == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	CNBLogicalDev *rootLogicalDev = ChildToClear->RootLogicalDev();

	if (rootLogicalDev == NULL) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[rootLogicalDev->NumberOfChild()];

	ZeroMemory( connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) * rootLogicalDev->NumberOfChild() );

	UINT i;

	for (i = 0; i < rootLogicalDev->NumberOfChild(); i++ ) {

		if (rootLogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (ChildToClear->IsCommandAvailable(IDM_TOOL_CLEAR_DEFECTIVE) == FALSE) {

		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	HRESULT bindResult = NdasOpBind( connectionInfo, 
									 rootLogicalDev->NumberOfChild(), 
									 NMT_SAFE_RAID_CLEAR_DEFECT, 
									 0, 
									 ChildToClear->Nidx() );
	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	rootLogicalDev->HixChangeNotify(pGetNdasHostGuid());

	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnMigrate(CNBLogicalDev *LogicalDev) 
{
	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO *connectionInfo = new NDASCOMM_CONNECTION_INFO[LogicalDev->NumberOfChild()];

	UINT i;
	NBLogicalDevPtrList::const_iterator itr;
	
	for (i = 0; i < LogicalDev->NumberOfChild(); i++ ) {

		if (LogicalDev->Child(i)->InitConnectionInfo(&connectionInfo[i], TRUE) == FALSE) {

			ATLASSERT(FALSE);

			delete[] connectionInfo;
			return NDAS_BIND_STATUS_UNSUCCESSFUL;
		}
	}

	if (LogicalDev->IsCommandAvailable(IDM_TOOL_MIGRATE) == FALSE) {
			
		ATLASSERT(FALSE);

		delete [] connectionInfo;
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}	

	HRESULT result = NdasOpMigrate(connectionInfo);

	if (SUCCEEDED(result)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	LogicalDev->HixChangeNotify(pGetNdasHostGuid());

	delete [] connectionInfo;
	return bindStatus;
}

NDAS_BIND_STATUS CNdasStatus::OnResetBindInfo( CNBLogicalDev *LogicalDev )
{
	NdasUiDbgCall( 2, "in\n" );

	NDAS_BIND_STATUS bindStatus;

	NDASCOMM_CONNECTION_INFO connectionInfo;

	ZeroMemory( &connectionInfo, sizeof(NDASCOMM_CONNECTION_INFO) );

	if (LogicalDev->InitConnectionInfo(&connectionInfo, TRUE) == FALSE) {

		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}

	if (LogicalDev->IsCommandAvailable(IDM_TOOL_RESET_BIND_INFO) == FALSE) {
			
		ATLASSERT(FALSE);
		return NDAS_BIND_STATUS_UNSUCCESSFUL;
	}	

	HRESULT bindResult = NdasOpBind( &connectionInfo, 1, NMT_SINGLE, 0 );

	if (SUCCEEDED(bindResult)) {

		bindStatus = NDAS_BIND_STATUS_OK; 

	} else {

		bindStatus = NDAS_BIND_STATUS_UNSUCCESSFUL; 
	}

	LogicalDev->HixChangeNotify(pGetNdasHostGuid());

	return bindStatus;
}
