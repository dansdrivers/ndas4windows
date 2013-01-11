#pragma once
#include "xguid.h"
#include "ndasmgmt.h"

// #define WM_NDAS_EVENT (WM_APP + 0x00EE)

#define NDAS_EVENT_WINDOW_MESSAGE_NAME _T("{B38AF876-F267-4dbf-A813-DF81CC89DD93}")

template<typename T>
class ATL_NO_VTABLE CNdasEventMessageMap :
	public CMessageMap
{
private:

	HNDASEVENTCALLBACK m_hNdasEvent;
	HWND m_hWndNdasEventReceipient;

	void NdasEventHandler(DWORD dwError, PNDAS_EVENT_INFO pEventInfo);

public:

	const UINT WM_NDAS_EVENT;

	BEGIN_MSG_MAP_EX(CNdasEventMessageMap<T>)
		MESSAGE_HANDLER_EX(WM_NDAS_EVENT, OnNdasEvent)
	END_MSG_MAP()

	CNdasEventMessageMap() :
		m_hWndNdasEventReceipient(0),
		m_hNdasEvent(0),
		WM_NDAS_EVENT(::RegisterWindowMessage(NDAS_EVENT_WINDOW_MESSAGE_NAME))
	{
		ATLASSERT(0 != WM_NDAS_EVENT);
	}

	static void CALLBACK NdasEventProc(
		DWORD dwError, 
		PNDAS_EVENT_INFO pEventInfo,
		LPVOID lpContext) 
	{
		CNdasEventMessageMap<T>* pThis = 
			reinterpret_cast<CNdasEventMessageMap<T>*>(lpContext);
		pThis->NdasEventHandler(dwError, pEventInfo);
	}

	BOOL NdasEventSubscribe(HWND hWndReceipient)
	{
		 m_hNdasEvent = ::NdasRegisterEventCallback(NdasEventProc, this);
		 if (0 == m_hNdasEvent) 
		 {
			 return FALSE;
		 }
		 m_hWndNdasEventReceipient = hWndReceipient;
		 return TRUE;
	}
	
	BOOL NdasEventUnsubscribe()
	{
		if (0 == m_hNdasEvent) 
		{
			return FALSE;
		}
		return ::NdasUnregisterEventCallback(m_hNdasEvent);
	}

	LRESULT OnNdasEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam);

	void OnNdasDeviceEntryChanged() 
	{
		ATLTRACE("Device Entry Changed.\n"); 
	}

	void OnNdasDeviceStatusChanged(
		DWORD dwSlotNo,
		NDAS_DEVICE_STATUS oldStatus,
		NDAS_DEVICE_STATUS newStatus)
	{ 
		ATLTRACE("Device Status Changed at slot %d.\n", dwSlotNo); 
	}

	void OnNdasDevicePropChanged(DWORD dwSlotNo)
	{
		ATLTRACE("Device Property Changed at slot %d.\n", dwSlotNo); 
	}

	void OnNdasUnitDevicePropChanged(
		DWORD dwSlotNo, 
		DWORD dwUnitNo)
	{
		ATLTRACE("Unit Device Property Changed at slot %d, unit %d.\n", dwSlotNo, dwUnitNo); 
	}

	void OnNdasLogDeviceEntryChanged()
	{
		ATLTRACE("Logical Device Entry Changed.\n"); 
	}

	void OnNdasLogDeviceStatusChanged(
		NDAS_LOGICALDEVICE_ID logDevId, 
		NDAS_LOGICALDEVICE_STATUS oldStatus,
		NDAS_LOGICALDEVICE_STATUS newStatus)
	{
		ATLTRACE("Logical Device (%d) Status Changed.\n", logDevId); 
	}

	void OnNdasLogDeviceDisconnected(
		NDAS_LOGICALDEVICE_ID logDevId)
	{
		ATLTRACE("Logical Device (%d) is disconnected.\n", logDevId); 
	}

	void OnNdasLogDeviceAlarmed(
		NDAS_LOGICALDEVICE_ID logDevId,
		ULONG AdapterStatus)
	{
		ATLTRACE("Logical Device (%d) is alarmed(%08lx).\n", logDevId, AdapterStatus); 
	}

	void OnNdasLogDevicePropertyChanged(
		NDAS_LOGICALDEVICE_ID logDevId)
	{
		ATLTRACE("Logical Device (%d) is OnNdasLogDevicePropertyChanged.\n", logDevId); 
	}

	void OnNdasLogDeviceRelationChanged(
		NDAS_LOGICALDEVICE_ID logDevId)
	{
		ATLTRACE("Logical Device (%d) is OnNdasLogDevicePropertyChanged.\n", logDevId); 
	}

	void OnNdasServiceRejectedSuspend()
	{
		ATLTRACE("Service rejected suspend/hibernation request.\n");
	}

	void OnNdasSurrenderAccessRequest(
		DWORD dwSlotNo, 
		DWORD dwUnitNo, 
		UCHAR requestFlags,
		LPCGUID requestHostGuid)
	{
		ATLTRACE("SurrenderAccessRequest to (%d,%d) flags: %d from %s)\n",
			dwSlotNo, 
			dwUnitNo, 
			requestFlags, 
			ximeta::CGuid(requestHostGuid).ToStringA());
	}

	void OnNdasServiceConnectRetry()
	{ 
		ATLTRACE("Retrying connecting to the service.\n"); 
	}

	void OnNdasServiceConnectFailed()
	{
		ATLTRACE("Service connection failed.\n"); 
	}

	void OnNdasServiceConnectConnected()
	{
		ATLTRACE("Service connected.\n"); 
	}

	void OnNdasServiceTerminating()
	{
		ATLTRACE("Service is terminating.\n"); 
	}

};

template <typename T>
inline
LRESULT 
CNdasEventMessageMap<T>::OnNdasEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	ATLASSERT(WM_NDAS_EVENT == uMsg);
	if (WM_NDAS_EVENT != uMsg) {
		return TRUE;
	}

	T* pThis = static_cast<T*>(this);

	NDAS_EVENT_TYPE eventType = static_cast<NDAS_EVENT_TYPE>(wParam);
	PNDAS_EVENT_INFO pEventInfo = reinterpret_cast<PNDAS_EVENT_INFO>(lParam);

	if (NDAS_EVENT_TYPE_DEVICE_ENTRY_CHANGED == eventType) 
	{
		pThis->OnNdasDeviceEntryChanged();
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_ENTRY_CHANGED == eventType) 
	{
		pThis->OnNdasLogDeviceEntryChanged();
	}
	else if (NDAS_EVENT_TYPE_DEVICE_STATUS_CHANGED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasDeviceStatusChanged(
				pEventInfo->EventInfo.DeviceInfo.SlotNo,
				pEventInfo->EventInfo.DeviceInfo.OldStatus,
				pEventInfo->EventInfo.DeviceInfo.NewStatus);
		}
	}
	else if (NDAS_EVENT_TYPE_DEVICE_PROPERTY_CHANGED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasDevicePropChanged(
				pEventInfo->EventInfo.DeviceInfo.SlotNo);
		}
	}
	else if (NDAS_EVENT_TYPE_UNITDEVICE_PROPERTY_CHANGED== eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasUnitDevicePropChanged(
				pEventInfo->EventInfo.UnitDeviceInfo.SlotNo,
				pEventInfo->EventInfo.UnitDeviceInfo.UnitNo);
		}
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_STATUS_CHANGED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasLogDeviceStatusChanged(
				pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId,
				pEventInfo->EventInfo.LogicalDeviceInfo.OldStatus,
				pEventInfo->EventInfo.LogicalDeviceInfo.NewStatus);
		}
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_ALARMED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) {
			pThis->OnNdasLogDeviceAlarmed(
				pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId,
				pEventInfo->EventInfo.LogicalDeviceInfo.AdapterStatus);
		}
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_DISCONNECTED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasLogDeviceDisconnected(
				pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
		}
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_PROPERTY_CHANGED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasLogDevicePropertyChanged(
				pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
		}
	}
	else if (NDAS_EVENT_TYPE_LOGICALDEVICE_RELATION_CHANGED == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasLogDeviceRelationChanged(
				pEventInfo->EventInfo.LogicalDeviceInfo.LogicalDeviceId);
		}
	}
	else if (NDAS_EVENT_TYPE_SURRENDER_REQUEST == eventType) 
	{
		ATLASSERT(NULL != pEventInfo);
		if (NULL != pEventInfo) 
		{
			pThis->OnNdasSurrenderAccessRequest(
				pEventInfo->EventInfo.SurrenderRequestInfo.SlotNo,
				pEventInfo->EventInfo.SurrenderRequestInfo.UnitNo,
				static_cast<UCHAR>(pEventInfo->EventInfo.SurrenderRequestInfo.RequestFlags),
				&pEventInfo->EventInfo.SurrenderRequestInfo.RequestHostGuid);
		}
	}
	else if (NDAS_EVENT_TYPE_SUSPEND_REJECTED == eventType) 
	{
		pThis->OnNdasServiceRejectedSuspend();
	}
	else if (NDAS_EVENT_TYPE_TERMINATING == eventType) 
	{
		pThis->OnNdasServiceTerminating();
	}
	else if (NDAS_EVENT_TYPE_RETRYING_CONNECTION == eventType) 
	{
		pThis->OnNdasServiceConnectRetry();
	}
	else if (NDAS_EVENT_TYPE_CONNECTED == eventType) 
	{
		pThis->OnNdasServiceConnectConnected();
	}
	else if (NDAS_EVENT_TYPE_CONNECTION_FAILED == eventType) 
	{
		pThis->OnNdasServiceConnectFailed();
	}
	else 
	{
		SetMsgHandled(FALSE);
		return TRUE;
	}

	if (NULL != pEventInfo) 
	{
		::GlobalFree(static_cast<HGLOBAL>(pEventInfo));
	}

	return TRUE;
}

template <typename T>
inline
void 
CNdasEventMessageMap<T>::NdasEventHandler(
	DWORD dwError, 
	PNDAS_EVENT_INFO pEventInfo)
{
	if (NULL == pEventInfo) 
	{
		ATLTRACE("Event Error %d (0x%08X)\n", dwError, dwError);
		return;
	}

	HWND hWnd = m_hWndNdasEventReceipient;
	if (!::IsWindow(hWnd)) 
	{
		ATLTRACE("Invalid Ndas Event Receipient Window: %p\n", hWnd);
		// ATLASSERT(FALSE);
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
				ATLTRACE("Memory allocation failed for WM_NDAS_EVENT\n");
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
		ATLTRACE("Unknown event type: %08X\n", pEventInfo->EventType);
		return;
	}

	if (NULL != lpEventContext) 
	{
		lParam = reinterpret_cast<LPARAM>(lpEventContext);
	}

	BOOL fSuccess = ::PostMessage(hWnd, WM_NDAS_EVENT, wParam, lParam);
	if (!fSuccess) 
	{
		ATLTRACE("PostMessage to hWnd %p failed.\n", hWnd);
		if (NULL != lpEventContext) 
		{
			::GlobalFree(static_cast<HGLOBAL>(lpEventContext));
		}
	}

}
