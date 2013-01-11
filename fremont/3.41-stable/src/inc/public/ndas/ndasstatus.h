// ndasstatus.h : interface of the CNdasStatus class
//
/////////////////////////////////////////////////////////////////////////////

// revised by William Kim 24/July/2008

#pragma once

#include <algorithm>

#include <atlctrlw.h>

#include <ndas/ndasuser.h>
#include <ndas/nbdev.h>


typedef enum _NDAS_BIND_STATUS {

	NDAS_BIND_STATUS_OK				= 0x00000000,
	NDAS_BIND_STATUS_UNSUCCESSFUL	= 0xC0000001,

} NDAS_BIND_STATUS, *PNDAS_BIND_STATUS;

class CNdasStatus;

typedef struct _NDAS_STATUS_THREAD_CONTEXT {

	CNdasStatus *NdasStatus;
	void		*Parameter;

} NDAS_STATUS_THREAD_CONTEXT, *PNDAS_STATUS_THREAD_CONTEXT;

class CNdasStatus
{
	static 
	BOOL	
	CALLBACK
	EnumDevicesCallBack (
		PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry,
		LPVOID lpContext
		);

protected:

	NBNdasDevPtrList		m_listNdasDevices;
	NBNdasDevPtrList		m_listMissingNdasDevices;	// Unit devices to represent non usable RAID members

	NBLogicalDevPtrList	m_listLeafLogicalDevs;

public:

	NBLogicalDevPtrList m_LogicalDevList;

	__declspec(dllexport) CNdasStatus();
	__declspec(dllexport) ~CNdasStatus();

	__declspec(dllexport) BOOL RefreshStatus (CProgressBarCtrl *m_wndRefreshProgress);

	void ClearDevices();

	NBLogicalDevPtrList GetOperatableSingleDevices();

	__declspec(dllexport) NDAS_BIND_STATUS OnBind( NBLogicalDevPtrList *LogicalDevBindList, UINT32 DiskCount, NDAS_MEDIA_TYPE BindType );
	__declspec(dllexport) NDAS_BIND_STATUS OnUnBind( CNBLogicalDev *LogicalDev, BOOL Partial, UINT32 *FailChildIdx );

	__declspec(dllexport) NDAS_BIND_STATUS OnAddMirror( CNBLogicalDev *LogicalDev, CNBLogicalDev *UnitDeviceToAdd );
	__declspec(dllexport) NDAS_BIND_STATUS OnAppend( CNBLogicalDev *LogicalDev, CNBLogicalDev *UnitDevice );
	__declspec(dllexport) NDAS_BIND_STATUS OnSpareAdd( CNBLogicalDev *LogicalDev, CNBLogicalDev *UnitDevice );

	__declspec(dllexport) NDAS_BIND_STATUS OnReplaceDevice( CNBLogicalDev *SelectedLogicalDev, CNBLogicalDev *NewUnitDevice );
	__declspec(dllexport) NDAS_BIND_STATUS OnRemoveFromRaid( CNBLogicalDev *SelectedLogicalDev );
	__declspec(dllexport) NDAS_BIND_STATUS OnClearDefect( CNBLogicalDev *ChildToClear );

	__declspec(dllexport) NDAS_BIND_STATUS OnMigrate( CNBLogicalDev *LogicalDev );
	__declspec(dllexport) NDAS_BIND_STATUS OnResetBindInfo( CNBLogicalDev *LogicalDev );

	// Threads related

	HANDLE m_hEventThread;
	LONG   m_nAssigned;

	static unsigned WINAPI _ThreadAddUnitDevices(LPVOID Context)
	{
		NDAS_STATUS_THREAD_CONTEXT *pThreadContext = (NDAS_STATUS_THREAD_CONTEXT *)Context;
		CNBNdasDev *pNBNdasDev = (CNBNdasDev *)pThreadContext->Parameter;

		unsigned exitcode = pThreadContext->NdasStatus->ThreadAddUnitDevices(pNBNdasDev);

		free(pThreadContext);
		pThreadContext = NULL;

		return exitcode;
	}

	unsigned ThreadAddUnitDevices(CNBNdasDev *pNBNdasDev);
};
