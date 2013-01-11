#pragma once
#include "ndasevent.h"

static const TCHAR NDAS_EVENT_PIPE_NAME[] = TEXT("\\\\.\\pipe\\ndas\\event");
static const DWORD MAX_NDAS_EVENT_PIPE_INSTANCES = 10;

UNALIGNED struct NDAS_EVENT_MESSAGE {
	DWORD MessageSize; // including header
	NDAS_EVENT_TYPE EventType;
	union {
		NDAS_EVENT_DEVICE_INFO DeviceEventInfo;
		NDAS_EVENT_LOGICALDEVICE_INFO LogicalDeviceEventInfo;
		NDAS_EVENT_VERSION_INFO VersionInfo;
		BYTE Reserved[26];
	};
};

typedef NDAS_EVENT_MESSAGE* PNDAS_EVENT_MESSAGE;

//
// Helper class for Event Message
class CNdasEventMessage
{
	TCHAR m_szStringBuffer[1024];
	const NDAS_EVENT_MESSAGE m_msg;
public:
	CNdasEventMessage(const NDAS_EVENT_MESSAGE& eventMsg) :
	  m_msg(eventMsg)
	{
	}

	LPCTSTR ToString()
	{

	}
};