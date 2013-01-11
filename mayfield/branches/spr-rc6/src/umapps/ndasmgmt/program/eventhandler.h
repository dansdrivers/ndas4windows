#pragma once
#include "xguid.h"
#include "ndasmgmt.h"

#define WM_NDAS_EVENT (WM_APP + 0x00EE)

template<typename T>
class ATL_NO_VTABLE CNdasEventHandler :
	public CMessageMap
{
private:

	HNDASEVENTCALLBACK m_hNdasEvent;
	HWND m_hWndNdasEventReceipient;

public:

	BEGIN_MSG_MAP_EX(CNdasEventHandler<T>)
		MESSAGE_HANDLER_EX(WM_NDAS_EVENT, OnNdasEvent)
	END_MSG_MAP()


	CNdasEventHandler() :
		m_hWndNdasEventReceipient(0),
		m_hNdasEvent(0)
	{
	}

	static VOID CALLBACK spNdasEventProc(
		DWORD dwError, 
		PNDAS_EVENT_INFO pEventInfo,
		LPVOID lpContext) 
	{
		CNdasEventHandler<T>* pThis = reinterpret_cast<CNdasEventHandler<T>*>(lpContext);
		pThis->NdasEventProc(dwError, pEventInfo);
	}

	VOID NdasEventProc(DWORD dwError, PNDAS_EVENT_INFO pEventInfo);

	BOOL NdasEventSubscribe(HWND hWndReceipient)
	{
		 m_hNdasEvent = ::NdasRegisterEventCallback(spNdasEventProc, this);
		 if (0 == m_hNdasEvent) {
			 return FALSE;
		 }
		 m_hWndNdasEventReceipient = hWndReceipient;
		 return TRUE;
	}
	
	BOOL NdasEventUnsubscribe()
	{
		if (0 == m_hNdasEvent) {
			return FALSE;
		}
		return ::NdasUnregisterEventCallback(m_hNdasEvent);
	}

	LRESULT OnNdasEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam);

	VOID OnNdasDevEntryChanged() 
	{ ATLTRACE(_T("Device Entry Changed.\n")); }

	VOID OnNdasDevStatusChanged(
		DWORD dwSlotNo,
		NDAS_DEVICE_STATUS oldStatus,
		NDAS_DEVICE_STATUS newStatus)
	{ ATLTRACE(_T("Device Status Changed at slot %d.\n"), dwSlotNo); }

	VOID OnNdasDevPropChanged(DWORD dwSlotNo)
	{ ATLTRACE(_T("Device Property Changed at slot %d.\n"), dwSlotNo); }

	VOID OnNdasUnitDevPropChanged(
		DWORD dwSlotNo, 
		DWORD dwUnitNo)
	{ ATLTRACE(_T("Unit Device Property Changed at slot %d, unit %d.\n"), dwSlotNo, dwUnitNo); }

	VOID OnNdasLogDevEntryChanged()
	{ ATLTRACE(_T("Logical Device Entry Changed.\n")); }

	VOID OnNdasLogDevStatusChanged(
		NDAS_LOGICALDEVICE_ID logDevId, 
		NDAS_LOGICALDEVICE_STATUS oldStatus,
		NDAS_LOGICALDEVICE_STATUS newStatus)
	{ ATLTRACE(_T("Logical Device (%d) Status Changed.\n"), logDevId); }

	VOID OnNdasLogDevDisconnected(
		NDAS_LOGICALDEVICE_ID logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is disconnected.\n"), logDevId); }

	//VOID OnNdasLogDevReconnecting(
	//	NDAS_LOGICALDEVICE_ID logDevId)
	//{ ATLTRACE(_T("Logical Device (%d) is being reconnecting.\n"), logDevId); }

	//VOID OnNdasLogDevReconnected(
	//	NDAS_LOGICALDEVICE_ID logDevId)
	//{ ATLTRACE(_T("Logical Device (%d) is reconnected.\n"), logDevId); }

	//VOID OnNdasLogDevEmergency(
	//	NDAS_LOGICALDEVICE_ID logDevId)
	//{ ATLTRACE(_T("Logical Device (%d) is running under emergency mode.\n"), logDevId); }	

	VOID OnNdasLogDevAlarmed(
		NDAS_LOGICALDEVICE_ID logDevId,
		ULONG AdapterStatus)
	{ ATLTRACE(_T("Logical Device (%d) is alarmed(%08lx).\n"), logDevId, AdapterStatus); }

	VOID OnNdasLogDevPropertyChanged(
		NDAS_LOGICALDEVICE_ID logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is OnNdasLogDevPropertyChanged.\n"), logDevId); }

	VOID OnNdasLogDevRelationChanged(
		NDAS_LOGICALDEVICE_ID logDevId)
	{ ATLTRACE(_T("Logical Device (%d) is OnNdasLogDevPropertyChanged.\n"), logDevId); }

	VOID OnNdasServiceRejectedSuspend()
	{
		ATLTRACE(_T("Service rejected suspend/hibernation request.\n"));
	}

	VOID OnNdasSurrenderAccessRequest(
		DWORD dwSlotNo, 
		DWORD dwUnitNo, 
		UCHAR requestFlags,
		LPCGUID requestHostGuid)
	{
		ATLTRACE(_T("SurrenderAccessRequest to (%d,%d) flags: %d from %s)"),
			dwSlotNo, 
			dwUnitNo, 
			requestFlags, 
			ximeta::CGuid(requestHostGuid).ToString());
	}

	VOID OnNdasServiceConnectRetry()
	{ ATLTRACE(_T("Retrying connecting to the service.\n")); }

	VOID OnNdasServiceConnectFailed()
	{ ATLTRACE(_T("Service connection failed.\n")); }

	VOID OnNdasServiceConnectConnected()
	{ ATLTRACE(_T("Service connected.\n")); }

	VOID OnNdasServiceTerminating()
	{ ATLTRACE(_T("Service is terminating.\n")); }

};

template <typename T>
inline
LRESULT 
CNdasEventHandler<T>::OnNdasEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ATLASSERT(WM_NDAS_EVENT == uMsg);
	if (WM_NDAS_EVENT != uMsg) {
		return TRUE;
	}

	T* pThis = static_cast<T*>(this);

	NDAS_EVENT_TYPE eventType = static_cast<NDAS_EVENT_TYPE>(wParam);
	PNDAS_EVENT_INFO pEventInfo = reinterpret_cast<PNDAS_EVENT_INFO>(lParam);

	if (NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED == eventType) {
		pThis->OnNdasDevEntryChanged();
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED == eventType) {
		pThis->OnNdasLogDevEntryChanged();
	} else if (NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasDevStatusChanged(
				pEventInfo->DeviceInfo.SlotNo,
				pEventInfo->DeviceInfo.OldStatus,
				pEventInfo->DeviceInfo.NewStatus);
		}
	} else if (NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasDevStatusChanged(
				pEventInfo->DeviceInfo.SlotNo,
				pEventInfo->DeviceInfo.OldStatus,
				pEventInfo->DeviceInfo.NewStatus);
		}
	} else if (NDAS_EVENT_TYPE_UNITDEVICE_PROPERTY_CHANGED== eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasUnitDevPropChanged(
				pEventInfo->UnitDeviceInfo.SlotNo,
				pEventInfo->UnitDeviceInfo.UnitNo);
		}
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDevStatusChanged(
				pEventInfo->LogicalDeviceInfo.LogicalDeviceId,
				pEventInfo->LogicalDeviceInfo.OldStatus,
				pEventInfo->LogicalDeviceInfo.NewStatus);
		}
	//} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING == eventType) {
	//	ATLASSERT(NULL != pEventInfo);
	//	if (NULL != pEventInfo) {
	//		pThis->OnNdasLogDevReconnecting(
	//			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
	//	}
	//} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED == eventType) {
	//	ATLASSERT(NULL != pEventInfo);
	//	if (NULL != pEventInfo) {
	//		pThis->OnNdasLogDevReconnected(
	//			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
	//	}
	//} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_EMERGENCY == eventType) {
	//	ATLASSERT(NULL != pEventInfo);
	//	if (NULL != pEventInfo) {
	//		pThis->OnNdasLogDevEmergency(
	//			pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
	//	}
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDevAlarmed(
				pEventInfo->LogicalDeviceInfo.LogicalDeviceId,
				pEventInfo->LogicalDeviceInfo.AdapterStatus);
		}
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDevDisconnected(
				pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		}
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDevPropertyChanged(
				pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		}
	} else if (NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDevRelationChanged(
				pEventInfo->LogicalDeviceInfo.LogicalDeviceId);
		}
	} else if (NDAS_EVENT_TYPE_SURRENDER_REQUEST == eventType) {
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasSurrenderAccessRequest(
				pEventInfo->SurrenderRequestInfo.SlotNo,
				pEventInfo->SurrenderRequestInfo.UnitNo,
				pEventInfo->SurrenderRequestInfo.RequestFlags,
				&pEventInfo->SurrenderRequestInfo.RequestHostGuid);
		}
	} else if (NDAS_EVENT_TYPE_SUSPEND_REJECTED == eventType) {
		pThis->OnNdasServiceRejectedSuspend();
	} else if (NDAS_EVENT_TYPE_TERMINATING == eventType) {
		pThis->OnNdasServiceTerminating();
	} else if (NDAS_EVENT_TYPE_RETRYING_CONNECTION == eventType) {
		pThis->OnNdasServiceConnectRetry();
	} else if (NDAS_EVENT_TYPE_CONNECTED == eventType) {
		pThis->OnNdasServiceConnectConnected();
	} else if (NDAS_EVENT_TYPE_CONNECTION_FAILED == eventType) {
		pThis->OnNdasServiceConnectFailed();
	} else {
		SetMsgHandled(FALSE);
		return TRUE;
	}

	if (NULL != pEventInfo) {
		::GlobalFree(static_cast<HGLOBAL>(pEventInfo));
	}

	return TRUE;
}

template <typename T>
inline
VOID 
CNdasEventHandler<T>::NdasEventProc(
	DWORD dwError, 
	PNDAS_EVENT_INFO pEventInfo)
{
	if (NULL == pEventInfo) {
		ATLTRACE(_T("Event Error %d (0x%08X)\n"), dwError, dwError);
		return;
	}

	HWND hWnd = m_hWndNdasEventReceipient;
	if (!::IsWindow(hWnd)) {
		ATLTRACE(_T("Invalid Ndas Event Receipient Window: %p\n"), hWnd);
		ATLASSERT(FALSE);
		return;
	}

	WPARAM wParam = static_cast<WPARAM>(pEventInfo->EventType);
	LPARAM lParam = 0;
	LPVOID lpEventContext = NULL;

	switch (pEventInfo->EventType) 
	{
	case NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED:
		break;
	case NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED:
	case NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED:
	case NDAS_EVENT_TYPE_UNITDEVICE_PROPERTY_CHANGED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED:
	//case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTING:
	//case NDAS_EVENT_TYPE_LOGICALDEVICE_RECONNECTED:
	//case NDAS_EVENT_TYPE_LOGICALDEVICE_EMERGENCY:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED:
	case NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED:
	case NDAS_EVENT_TYPE_SURRENDER_REQUEST:
		{
			lpEventContext = ::GlobalAlloc(
				GPTR, 
				sizeof(NDAS_EVENT_INFO));
			if (NULL == lpEventContext) {
				ATLTRACE(_T("Memory allocation failed for WM_NDAS_EVENT\n"));
				return;
			}
			::CopyMemory(
				lpEventContext, 
				pEventInfo, 
				sizeof(NDAS_EVENT_INFO));
		}
		break;
	case NDAS_EVENT_TYPE_TERMINATING:
	case NDAS_EVENT_TYPE_RETRYING_CONNECTION:
	case NDAS_EVENT_TYPE_CONNECTED:
	case NDAS_EVENT_TYPE_CONNECTION_FAILED:
	case NDAS_EVENT_TYPE_SUSPEND_REJECTED:
		break;
	default:
		ATLTRACE(_T("Unknown event type: %08X\n"), pEventInfo->EventType);
		return;
	}

	if (NULL != lpEventContext) {
		lParam = reinterpret_cast<LPARAM>(lpEventContext);
	}

	BOOL fSuccess = ::PostMessage(hWnd, WM_NDAS_EVENT, wParam, lParam);
	if (!fSuccess) {
		ATLTRACE(_T("PostMessage to hWnd %p failed.\n"), hWnd);
		if (NULL != lpEventContext) {
			::GlobalFree(static_cast<HGLOBAL>(lpEventContext));
		}
	}

}
