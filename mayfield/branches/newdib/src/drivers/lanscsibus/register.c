reak;

        case IRP_MJ_CLOSE:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_CLOSE%c\t%s\t", 
                                       name, 
                                       (Irp->Flags & IRP_PAGING_IO) || 
                                              (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '*' : ' ',
                                       fullPathName );

            //
            // This fileobject/name association can be discarded now.
            //
            FilemonFreeHashEntry( FileObject );
            break;

        case IRP_MJ_FLUSH_BUFFERS:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_FLUSH\t%s\t", name, fullPathName );
            break;

        case IRP_MJ_QUERY_INFORMATION:
 
            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_QUERY_INFORMATION\t%s\t%s", 
                                       name, fullPathName, 
                                       FileInformation[currentIrpStack->Parameters.QueryFile.FileInformationClass] );
            break;

        case IRP_MJ_SET_INFORMATION:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_SET_INFORMATION%c\t%s\t%s", 
                                       name, 
                                       (Irp->Flags & IRP_PAGING_IO) || 
                                             (Irp->Flags & IRP_SYNCHRONOUS_PAGING_IO) ? '*' : ' ',
                                       fullPathName,
                                       FileInformation[currentIrpStack->Parameters.SetFile.FileInformationClass] );
			//
			//	deny changing information of a file
			//
			Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED ;
			Irp->IoStatus.Information = 0 ;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;

			//
		    // Free the name only if we have to
		    //
		    if( FilterOn && hookExt->Hooked ) {
		        FREEPATHNAME();
		    }

			return STATUS_MEDIA_WRITE_PROTECTED ;


            //
            // If its a rename, cleanup the name association.
            //
//            if( currentIrpStack->Parameters.SetFile.FileInformationClass == 
//                FileRenameInformation ) {

//                FilemonFreeHashEntry( FileObject );
//            }
//            break;

        case IRP_MJ_QUERY_EA:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_QUERY_EA\t%s\t", name, fullPathName );
            break;

        case IRP_MJ_SET_EA:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_SET_EA\t%s\t", name, fullPathName );
			//
			//	deny changing EAs of a file
			//
			Irp->IoStatus.Status = STATUS_MEDIA_WRITE_PROTECTED ;
			Irp->IoStatus.Information = 0 ;
			IoCompleteRequest(Irp, IO_DISK_INCREMENT) ;

			//
			// Free the name only if we have to
			//
			if( FilterOn && hookExt->Hooked ) {
			    FREEPATHNAME();
			}

			return STATUS_MEDIA_WRITE_PROTECTED ;
//            break;

        case IRP_MJ_QUERY_VOLUME_INFORMATION:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_QUERY_VOLUME_INFORMATION\t%s\t%s", 
                                       name, fullPathName,
                                       VolumeInformation[currentIrpStack->Parameters.QueryVolume.FsInformationClass] );
            break;

        case IRP_MJ_SET_VOLUME_INFORMATION:

            hookCompletion = LogRecord( TRUE, &seqNum, &dateTime, NULL, 
                                       "%s\tIRP_MJ_SET_VOLUME_INFORMATION\t%s\t%s", 
                                       name, fullPathName,
                                       VolumeInformation[currentIrpSendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("WM_POWERBROADCAST"));
					LRESULT count = ::SendMessage(hWndListBox, LB_GETCOUNT, 0, 0);
					::SendMessage(hWndListBox, LB_SETCURSEL, count - 1, 0); 
				}
				{
					if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
					DWORD dwEventType = static_cast<DWORD>(wParam);
					LPVOID lpEventData = static_cast<LPVOID>(ULongToPtr(lParam));
					DWORD ret = pT->OnServicePowerEvent(dwEventType, lpEventData);
					return (NO_ERROR == ret) ? TRUE : BROADCAST_QUERY_DENY;
				}
			case WM_ENDSESSION:
				{
					::SendMessage(hWndListBox, LB_ADDSTRING, 0, (LPARAM)_T("WM_ENDSESSION"));
					LRESULT count = ::SendMessage(hWndListBox, LB_GETCOUNT, 0, 0);
					::SendMessage(hWndListBox, LB_SETCURSEL, count - 1, 0); 
				}
				if (wParam)
				{
					if (lParam & ENDSESSION_LOGOFF)
					{
						if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
						XTLVERIFY(NOERROR == pT->OnServiceShutdown());
					}
					else
					{
						if (pT->m_Status.dwCurrentState != SERVICE_RUNNING) return TRUE;
						XTLVERIFY(NOERROR == pT->OnServiceShutdown());
					}
				}
				return 0;
			default:
				return ::DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
	};

};

#define XTL_DECLARE_SERVICE_NAME_RESOURCE(uID) \
	static LPCTSTR GetServiceName() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_DISPLAY_NAME_RESOURCE(uID) \
	static LPCTSTR GetServiceDisplayName() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_DESCRIPTION_RESOURCE(uID) \
	static LPCTSTR GetServiceDescription() \
	{ \
		static TCHAR szBuffer[256] = {0}; \
		if (0 != szBuffer[0]) return szBuffer; \
		int n = ::LoadString(NULL, uID, szBuffer, 256); \
		XTLASSERT(n > 0); \
		return szBuffer; \
	}
#define XTL_DECLARE_SERVICE_NAME(ServiceName) \
	static LPCTSTR GetServiceName() { return _T(ServiceName); }
#define XTL_DECLARE_SERVICE_DISPLAY_NAME(DisplayName) \
	static LPCTSTR GetServiceDisplayName() { return _T(DisplayName); }
#define XTL_DECLARE_SERVICE_DESCRIPTION(Description) \
	static LPCTSTR GetServiceDescription() { return _T(Description); }

template <typename T, class TServiceTraits>
inline DWORD
CService<T,TServiceTraits>::ServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
{
	T* pT = static_cast<T*>(this);
	switch (dwControl) 
	{
	// Standard Handlers
	case SERVICE_CONTROL_CONTINUE:
		return pT->OnServiceContinue();
	case SERVICE_CONTROL_INTERROGATE:
		return pT->OnServiceInterrogate();
	case SERVICE_CONTROL_PARAMCHANGE:
		return pT->OnServiceParamChange();
	case SERVICE_CONTROL_PAUSE:
		return pT->OnServicePause();
	case SERVICE_CONTROL_SHUTDOWN:
		return pT->OnServiceShutdown();
	case SERVICE_CONTROL_STOP:
		return pT->OnServiceStop();

	// Extended Handlers
	case SERVICE_CONTROL_DEVICEEVENT:
		return pT->OnServiceDeviceEvent(dwEventType, lpEventData);
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		return pT->OnServiceHardwareProfileChange(dwEventType, lpEventData);
	case SERVICE_CONTROL_POWEREVENT:
		return pT->OnServicePowerEvent(dwEventType, lpEventData);
	case SERVICE_CONTROL_SESSIONCHANGE:
		return pT->OnServiceSessionChange(dwEventType, lpEventData);

	// This control code has been deprecated. use PnP functionality instead
	case SERVICE_CONTROL_NETBINDADD:
	case SERVICE_CONTROL_NETBINDDISABLE:
	case SERVICE_CONTROL_NETBINDENABLE:
	case SERVICE_CONTROL_NETBINDREMOVE:
		return ERROR_CALL_NOT_IMPLEMENTED;

	default: // User-defined control code
		if (dwControl >= 128 && dwControl <= 255) 
		{
			return pT->OnServiceUserControl(dwControl, dwEventType, lpEventData);
		}
		else 
		{
			return ERROR_CALL_NOT_IMPLEMENTED;
		}
	}Microsoft Visual Studio Solution File, Format Version 9.00
# Visual Studio 2005
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "dsig", "dsig\_vs80_dsig.vcproj", "{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "_inc_ndas_setup", "inc\_vs80__inc_ndas_setup.vcproj", "{7F757832-2DB1-42DF-A977-A92F359AC679}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasdi", "ndasdi\_vs80_ndasdi.vcproj", "{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndasetup", "ndasetup\_vs80_ndasetup.vcproj", "{4D13E995-E31D-4CF7-A273-8A4EF0670161}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndmsica", "ndmsica\_vs80_ndmsica.vcproj", "{F1B1DE6A-1D50-4156-BAE0-8FAA592CC27E}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "ndupdate", "ndupdate\_vs80_ndupdate.vcproj", "{579AB25C-E753-4092-A423-DED628F41DF6}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "_inc_ndas_private", "..\inc\_vs80__inc_ndas_private.vcproj", "{0BB4ECC0-7DDC-44B8-B624-8A1E15426A62}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "_inc_boost", "..\inc\boost\_vs80__inc_boost.vcproj", "{5DE951CF-1ECD-4ABF-AB47-EA296AA7DBC1}"
EndProject
Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "_inc_ndas_public", "..\inc\public\_vs80__inc_ndas_public.vcproj", "{5052B92F-1150-493D-815F-0A4B3AF06410}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Debug|Win32 = Debug|Win32
		Debug|x64 = Debug|x64
		Release|Win32 = Release|Win32
		Release|x64 = Release|x64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Debug|Win32.ActiveCfg = Debug|Win32
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Debug|Win32.Build.0 = Debug|Win32
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Debug|x64.ActiveCfg = Debug|x64
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Debug|x64.Build.0 = Debug|x64
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Release|Win32.ActiveCfg = Release|Win32
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Release|Win32.Build.0 = Release|Win32
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Release|x64.ActiveCfg = Release|x64
		{FB26A50E-693E-4CFC-A585-D70B0B3E3F7C}.Release|x64.Build.0 = Release|x64
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Debug|Win32.ActiveCfg = Debug|Win32
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Debug|Win32.Build.0 = Debug|Win32
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Debug|x64.ActiveCfg = Debug|x64
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Debug|x64.Build.0 = Debug|x64
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Release|Win32.ActiveCfg = Release|Win32
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Release|Win32.Build.0 = Release|Win32
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Release|x64.ActiveCfg = Release|x64
		{7F757832-2DB1-42DF-A977-A92F359AC679}.Release|x64.Build.0 = Release|x64
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Debug|Win32.ActiveCfg = Debug|Win32
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Debug|Win32.Build.0 = Debug|Win32
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Debug|x64.ActiveCfg = Debug|x64
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Debug|x64.Build.0 = Debug|x64
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Release|Win32.ActiveCfg = Release|Win32
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Release|Win32.Build.0 = Release|Win32
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Release|x64.ActiveCfg = Release|x64
		{C9B6CCBB-80BA-49ED-9DFF-ECD6C9EEB2A2}.Release|x64.Build.0 = Release|x64
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Debug|Win32.ActiveCfg = Debug|Win32
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Debug|Win32.Build.0 = Debug|Win32
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Debug|x64.ActiveCfg = Debug|x64
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Debug|x64.Build.0 = Debug|x64
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Release|Win32.ActiveCfg = Release|Win32
		{4D13E995-E31D-4CF7-A273-8A4EF0670161}.Release|Win32.Build.0 = Release|Win32
		{4D13E995-E31D-4CF7-A273-8A4EF0670NTRY lpEntry, 
	LPVOID lpContext);

/* <TITLE NdasEnumLogicalDevices>

Declaration

BOOL 
NdasEnumLogicalDevices(
	IN NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

Summary

	NdasEnumLogicalDevices enumerates all NDAS logical device instances
	in the system by calling application-defined callback functions.

*/

NDASUSER_LINKAGE 
BOOL 
NDASUSERAPI
NdasEnumLogicalDevices(
	IN NDASLOGICALDEVICEENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

/* <TITLE NdasQueryLogicalDeviceStatus>

Declaration

BOOL
NdasQueryLogicalDeviceStatus(
	IN  NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT NDAS_LOGICALDEVICE_STATUS* pStatus,
	OUT NDAS_LOGICALDEVICE_ERROR* pLastError);

Summary

	NdasQueryLogicalDeviceStatus queries the status of the NDAS logical
	device specified by the NDAS logical device ID.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryLogicalDeviceStatus(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId,
	OUT NDAS_LOGICALDEVICE_STATUS* pStatus,
	OUT NDAS_LOGICALDEVICE_ERROR* pLastError);


/* @@NdasQueryHostEnumProc

<TITLE NdasQueryHostEnumProc>

Declaration

BOOL
NdasQueryHostEnumProc(
	LPCGUID lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext);

Summary

	lpHostInfo members and their data pointed by members are valid 
	only for the scope of the enumerator function.
	If any data is need persistent should be copied to another buffer

*/

/* <COMBINE NdasQueryHostEnumProc> */

typedef BOOL (CALLBACK* NDASQUERYHOSTENUMPROC)(
	LPCGUID lpHostGuid,
	ACCESS_MASK Access,
	LPVOID lpContext);

/* <TITLE NdasQueryHostsForLogicalDevice>

Declaration

BOOL
NdasQueryHostsForLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	IN NDASQUERYHOSTENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

Summary

	NdasQueryHostsForLogicalDevice searches NDAS hosts which
	is using the logical device.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForLogicalDevice(
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId, 
	IN NDASQUERYHOSTENUMPROC lpEnumProc, 
	IN LPVOID lpContext);

/* <TITLE NdasQueryHostsForUnitDevice>

Declaration

BOOL
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

BOOL
NdasQueryHostsForUnitDeviceById(
	LPCTSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

Summary

	NdasQueryHostInfo queries the information of the NDAS host
	specified by the NDAS host GUID.

	NDAS host GUIDs can be obtained by calling NdasQueryHostsForUnitDevice.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDevice(
	DWORD dwSlotNo, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasQueryHostsForUnitDevice> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDeviceByIdW(
	LPCWSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/* <COMBINE NdasQueryHostsForUnitDevice> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostsForUnitDeviceByIdA(
	LPCSTR lpszDeviceStringId, 
	DWORD dwUnitNo, 
	NDASQUERYHOSTENUMPROC lpEnumProc, 
	LPVOID lpContext);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryHostsForUnitDeviceById NdasQueryHostsForUnitDeviceByIdW
#else
#define NdasQueryHostsForUnitDeviceById NdasQueryHostsForUnitDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryHostInfo>

Declaration

BOOL
NdasQueryHostInfo(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFO* pHostInfo);

Summary

	NdasQueryHostInfo queries the information of the NDAS host
	specified by the NDAS host GUID.

	NDAS host GUIDs can be obtained by calling NdasQueryHostsForUnitDevice.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoW(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFOW* pHostInfo);

/* <COMBINE NdasQueryHostInfoW> */

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryHostInfoA(
	IN LPCGUID lpHostGuid, 
	OUT NDAS_HOST_INFOA* pHostInfo);

/*DOM-IGNORE-BEGIN*/
#if#pragma once
#include <dbt.h>
#include <pbt.h>
#include "xtldef.h"

namespace XTL
{

template <typename T>
class CDeviceEventHandler
{
public:

	//
	// Device Event Dispatcher
	//
	LRESULT DeviceEventProc(WPARAM wParam, LPARAM lParam);

protected:

	void OnConfigChangeCanceled() {}
	void OnConfigChanged() {}
	void OnDevNodesChanged() {}
	LRESULT OnQueryChangeConfig() { return TRUE; }

	void OnCustomEvent(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceArrival(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR /*pdbhdr*/) { return TRUE; }
	void OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceRemoveComplete(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceRemovePending(PDEV_BROADCAST_HDR /*pdbhdr*/) {}
	void OnDeviceTypeSpecific(PDEV_BROADCAST_HDR /*pdbhdr*/) {}

	void OnUserDefined(_DEV_BROADCAST_USERDEFINED* /*pdbuser*/) {}

};

template <typename T>
class CPowerEventHandler
{
public:

	//
	// Power Event Handler Dispatcher
	//
	LRESULT PowerEventProc(WPARAM wParam, LPARAM lParam);

protected:

	//
	// Battery power is low.
	//
	void OnBatteryLow() {}
	//
	// OEM-defined event occurred.
	//
	void OnOemEvent(DWORD /*dwEventCode*/) {}
	//
	// Power status has changed.
	//
	void OnPowerStatusChange() {}
	//
	// Request for permission to suspend.
	//
	// A DWORD value dwFlags specifies action flags. 
	// If bit 0 is 1, the application can prompt the user for directions 
	// on how to prepare for the suspension; otherwise, the application 
	// must prepare without user interaction. 
	// All other bit values are reserved. 
	//
	// Return TRUE to grant the request to suspend. 
	// To deny the request, return BROADCAST_QUERY_DENY.
	//
	LRESULT OnQuerySuspend(DWORD /*dwFlags*/) { return TRUE; }
	//
	// Suspension request denied.
	//
	void OnQuerySuspendFailed() {}
	//
	// Operation resuming automatically after event.
	//
	void OnResumeAutomatic() {}
	//
	// Operation resuming after critical suspension.
	//
	void OnResumeCritical() {}
	//
	// Operation resuming after suspension.
	//
	void OnResumeSuspend() {}
	//
	// System is suspending operation.
	//
	void OnSuspend() {}
};

//
// Return code for services and Windows applications
// for handling Device Events are different
// Device Event Handler is based on Windows application,
// which will return TRUE or BROADCAST_QUERY_DENY
//
// For services:
//
// If your service handles HARDWAREPROFILECHANGE, 
// return NO_ERROR to grant the request 
// and an error code to deny the request.
//
// CServiceDeviceEventHandler and CServicePowerEventHandler
// shim the differeces of parameters and the return code.
//
template <typename T>
class CServiceDeviceEventHandler : public CDeviceEventHandler<T>
{
public:
	// Forward Device Event to DeviceEventProc
	DWORD ServiceDeviceEventHandler(DWORD dwEventType, LPVOID lpEventData)
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->DeviceEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
	// Forward HardwareProfileChange Event to DeviceEventProc
	DWORD ServiceHardwareProfileChangeHandler(DWORD dwEventType, LPVOID lpEventData) 
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->DeviceEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
};

template <typename T>
class CServicePowerEventHandler : public CPowerEventHandler<T>
{
public:
	// Forward Power Event to PowerEventProc
	DWORD ServicePowerEventHandler(DWORD dwEventType, LPVOID lpEventData) 
	{
		T* pT = static_cast<T*>(this);
		LRESULT res = pT->PowerEventProc(
			static_cast<WPARAM>(dwEventType), 
			reinterpret_cast<LPARAM>(lpEventData));
		return (TRUE == res) ? NO_ERROR : res;
	}
};


template <typename T> inline LRESULT
CDeviceEventHandler<T>::DeviceEventProc(WPARAM wParam, LPARAM lParam)
{
	T* pT = static_casDebugMessage)
{
	OutputDebugStringW(DebugMessage);
}

DBGPRNAPI _DbgVPrintA(LPCSTR DebugMessage, va_list ap)
{
	DWORD err = GetLastError();
	CHAR szBuffer[DBGPRN_MAX_CHARS];
	HRESULT hr;
	hr = StringCchVPrintfA(szBuffer, DBGPRN_MAX_CHARS, DebugMessage, ap);
	if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		_DbgPrintA(szBuffer);
	}
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintW(LPCWSTR DebugMessage, va_list ap)
{
	DWORD err = GetLastError();
	WCHAR szBuffer[DBGPRN_MAX_CHARS];
	HRESULT hr;
	hr = StringCchVPrintfW(szBuffer, DBGPRN_MAX_CHARS, DebugMessage, ap);
	if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr) {
		_DbgPrintW(szBuffer);
	}
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintErrA(DWORD ErrorCode, LPCSTR Format, va_list ap)
{
	DWORD err = GetLastError();
	CHAR szBuffer[DBGPRN_MAX_CHARS];
	LPSTR pszNext = szBuffer;
	size_t cchRemaining = DBGPRN_MAX_CHARS;
	DWORD cch = 0;
	HRESULT hr;

	hr = StringCchVPrintfExA(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS, 
		Format, ap);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintA("FAILED_ON_DbgVPrintErr\n"); 
		return; 
	}

	hr = StringCchPrintfExA(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS,
		"Error %d (0x%08X) ", ErrorCode, ErrorCode);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintA("_DbgVPrintErr\n");
		return;
	}

	cch = FormatMessageA( 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pszNext,
		(DWORD)cchRemaining,
		NULL);

	if (0 == cch) {
		hr = StringCchCopyA(pszNext, cchRemaining, 
			"(no description available)\n");
		if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
			_DbgPrintA("_DbgVPrintErr\n");
			return;
		}
	} else {
		cchRemaining -= cch;
	}

	_DbgPrintA(szBuffer);
	SetLastError(err);
}

DBGPRNAPI _DbgVPrintErrW(DWORD ErrorCode, LPCWSTR Format, va_list ap)
{
	DWORD err = GetLastError();
	WCHAR szBuffer[DBGPRN_MAX_CHARS];
	LPWSTR pszNext = szBuffer;
	size_t cchRemaining = DBGPRN_MAX_CHARS;
	DWORD cch = 0;
	HRESULT hr;

	hr = StringCchVPrintfExW(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS, 
		Format, ap);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintW(L"FAILED_ON_DbgVPrintErr\n"); 
		return; 
	}

	hr = StringCchPrintfExW(
		pszNext, cchRemaining,
		&pszNext, &cchRemaining,
		STRSAFE_IGNORE_NULLS,
		L"Error %d (0x%08X) ", ErrorCode, ErrorCode);

	if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
		_DbgPrintW(L"_DbgVPrintErr\n");
		return;
	}

	cch = FormatMessageW( 
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pszNext,
		(DWORD)cchRemaining,
		NULL);

	if (0 == cch) {
		hr = StringCchCopyW(pszNext, cchRemaining, 
			L"(no description available)\n");
		if (FAILED(hr) && STRSAFE_E_INSUFFICIENT_BUFFER != hr) { 
			_DbgPrintW(L"_DbgVPrintErr\n");
			return;
		}
	} else {
		cchRemaining -= cch;
	}

	_DbgPrintW(szBuffer);
	SetLastError(err);
}
#endif // DBGPRN_INLINE

DBGPRN_INLINE_API _DbgPrintLevelA(ULONG Level, LPCSTR Format, ...)
{
	if(Level <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap,Format);
		_DbgVPrintA(Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintLevelW(ULONG Level, LPCWSTR Format, ...)
{
	if(Level <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap,Format);
		_DbgVPrintW(Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintErrA(DWORD Error, LPCSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrA(Error, Format, ap);
		va_end(ap);
	}
}

DBGPRN_INLINE_API _DbgPrintErrW(DWORD Error, LPCWSTR Format, ...)
{
	if (_DbgPrintErrorLevel <= _DbgPrintLevel) {
		va_list ap;
		va_start(ap, Format);
		_DbgVPrintErrW(Error, Format, ap);
		va_end(ap)AL_DEV
//
// MessageText:
//
//  CR_NO_SUCH_LOGICAL_DEV
//
#define NDAS_CR_NO_SUCH_LOGICAL_DEV      ((DWORD)0xE1F00014L)

//
// MessageId: NDAS_CR_CREATE_BLOCKED
//
// MessageText:
//
//  CR_CREATE_BLOCKED
//
#define NDAS_CR_CREATE_BLOCKED           ((DWORD)0xE1F00015L)

//
// MessageId: NDAS_CR_NOT_SYSTEM_VM
//
// MessageText:
//
//  CR_NOT_SYSTEM_VM
//
#define NDAS_CR_NOT_SYSTEM_VM            ((DWORD)0xE1F00016L)

//
// MessageId: NDAS_CR_REMOVE_VETOED
//
// MessageText:
//
//  CR_REMOVE_VETOED
//
#define NDAS_CR_REMOVE_VETOED            ((DWORD)0xE1F00017L)

//
// MessageId: NDAS_CR_APM_VETOED
//
// MessageText:
//
//  CR_APM_VETOED
//
#define NDAS_CR_APM_VETOED               ((DWORD)0xE1F00018L)

//
// MessageId: NDAS_CR_INVALID_LOAD_TYPE
//
// MessageText:
//
//  CR_INVALID_LOAD_TYPE
//
#define NDAS_CR_INVALID_LOAD_TYPE        ((DWORD)0xE1F00019L)

//
// MessageId: NDAS_CR_BUFFER_SMALL
//
// MessageText:
//
//  CR_BUFFER_SMALL
//
#define NDAS_CR_BUFFER_SMALL             ((DWORD)0xE1F0001AL)

//
// MessageId: NDAS_CR_NO_ARBITRATOR
//
// MessageText:
//
//  CR_NO_ARBITRATOR
//
#define NDAS_CR_NO_ARBITRATOR            ((DWORD)0xE1F0001BL)

//
// MessageId: NDAS_CR_NO_REGISTRY_HANDLE
//
// MessageText:
//
//  CR_NO_REGISTRY_HANDLE
//
#define NDAS_CR_NO_REGISTRY_HANDLE       ((DWORD)0xE1F0001CL)

//
// MessageId: NDAS_CR_REGISTRY_ERROR
//
// MessageText:
//
//  CR_REGISTRY_ERROR
//
#define NDAS_CR_REGISTRY_ERROR           ((DWORD)0xE1F0001DL)

//
// MessageId: NDAS_CR_INVALID_DEVICE_ID
//
// MessageText:
//
//  CR_INVALID_DEVICE_ID
//
#define NDAS_CR_INVALID_DEVICE_ID        ((DWORD)0xE1F0001EL)

//
// MessageId: NDAS_CR_INVALID_DATA
//
// MessageText:
//
//  CR_INVALID_DATA
//
#define NDAS_CR_INVALID_DATA             ((DWORD)0xE1F0001FL)

//
// MessageId: NDAS_CR_INVALID_API
//
// MessageText:
//
//  CR_INVALID_API
//
#define NDAS_CR_INVALID_API              ((DWORD)0xE1F00020L)

//
// MessageId: NDAS_CR_DEVLOADER_NOT_READY
//
// MessageText:
//
//  CR_DEVLOADER_NOT_READY
//
#define NDAS_CR_DEVLOADER_NOT_READY      ((DWORD)0xE1F00021L)

//
// MessageId: NDAS_CR_NEED_RESTART
//
// MessageText:
//
//  CR_NEED_RESTART
//
#define NDAS_CR_NEED_RESTART             ((DWORD)0xE1F00022L)

//
// MessageId: NDAS_CR_NO_MORE_HW_PROFILES
//
// MessageText:
//
//  CR_NO_MORE_HW_PROFILES
//
#define NDAS_CR_NO_MORE_HW_PROFILES      ((DWORD)0xE1F00023L)

//
// MessageId: NDAS_CR_DEVICE_NOT_THERE
//
// MessageText:
//
//  CR_DEVICE_NOT_THERE
//
#define NDAS_CR_DEVICE_NOT_THERE         ((DWORD)0xE1F00024L)

//
// MessageId: NDAS_CR_NO_SUCH_VALUE
//
// MessageText:
//
//  CR_NO_SUCH_VALUE
//
#define NDAS_CR_NO_SUCH_VALUE            ((DWORD)0xE1F00025L)

//
// MessageId: NDAS_CR_WRONG_TYPE
//
// MessageText:
//
//  CR_WRONG_TYPE
//
#define NDAS_CR_WRONG_TYPE               ((DWORD)0xE1F00026L)

//
// MessageId: NDAS_CR_INVALID_PRIORITY
//
// MessageText:
//
//  CR_INVALID_PRIORITY
//
#define NDAS_CR_INVALID_PRIORITY         ((DWORD)0xE1F00027L)

//
// MessageId: NDAS_CR_NOT_DISABLEABLE
//
// MessageText:
//
//  CR_NOT_DISABLEABLE
//
#define NDAS_CR_NOT_DISABLEABLE          ((DWORD)0xE1F00028L)

//
// MessageId: NDAS_CR_FREE_RESOURCES
//
// MessageText:
//
//  CR_FREE_RESOURCES
//
#define NDAS_CR_FREE_RESOURCES           ((DWORD)0xE1F00029L)

//
// MessageId: NDAS_CR_QUERY_VETOED
//
// MessageText:
//
//  CR_QUERY_VETOED
//
#define NDAS_CR_QUERY_VETOED             ((DWORD)0xE1F0002AL)

//
// MessageId: NDAS_CR_CANT_SHARE_IRQ
//
// MessageText:
//
//  CR_CANT_SHARE_IRQ
//
#define NDAS_CR_CANT_SHARE_IRQ           ((DWORD)0xE1F0002BL)

//
// MessageId: NDAS_CR_NO_DEPENDENT
//
// MessageText:
//
//  CR_NO_DEPENDENT
//
#define NDAS_CR_NO_DEPENDENT             ((DWORD)0xE1F0002CL)

//
// MessageId: NDAS_CR_SAME_RESOURCES
//
// MessageText:
//
//  CR_SAME_RESOURCES
//
#define NDAS_CR_SAME_RESOURCES           ((DWORD)0xE1F00WaitObject, 
		HANDLE hObject, 
		const XTL::WaitOrTimerCallback<T,P>& Callback,
		P Context,
		ULONG dwMilliseconds, 
		ULONG dwFlags = WT_EXECUTEDEFAULT)
	{
		ThisClass* pT = static_cast<ThisClass*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThisClass)));
		if (NULL == pT)
		{
			::SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}
		HANDLE hWaitHandle;
		BOOL ret = ::RegisterWaitForSingleObject(
			&hWaitHandle, hObject, 
			ThisClass::WaitOrTimerCallback, pT, 
			dwMilliseconds, dwFlags);
		if (!ret)
		{
			XTLVERIFY( ::HeapFree(::GetProcessHeap(), HEAP_ZERO_MEMORY, pT) );
		}
		else
		{
			pT->m_pThis = pT;
			pT->m_hWaitHandle = hWaitHandle;
			pT->m_cb = Callback;
			pT->m_final = false;
			pT->m_context = Context;
			*phNewWaitObject = pT;
		}
		return ret;
	}
	static BOOL UnregisterWait(WaitOrTimerCallbackAdapter<T,P>* hWaitHandle)
	{
		BOOL ret = ::UnregisterWait(hWaitHandle->m_hWaitHandle);
		if (ret || ERROR_IO_PENDING == ::GetLastError())
		{
			hWaitHandle->m_final = true;
		}
		return ret;
	}
	static BOOL UnregisterWaitEx(WaitOrTimerCallbackAdapter<T,P>* hWaitHandle, HANDLE CompletionEvent)
	{
		BOOL ret = ::UnregisterWaitEx(hWaitHandle->m_hWaitHandle, CompletionEvent);
		if (ret || ERROR_IO_PENDING == ::GetLastError())
		{
			hWaitHandle->m_final = true;
		}
		return ret;
	}
};

template <typename T, typename P> 
class XtlWaitObject : private WaitOrTimerCallbackAdapter<T,P> {};

template <typename T, typename P>
inline 
BOOL 
XtlRegisterWaitForSingleObject(
	XtlWaitObject<T,P>** phNewWaitObject, 
	HANDLE hObject, 
	const WaitOrTimerCallback<T,P>& Callback,
	P Context,
	ULONG dwMilliseconds, 
	ULONG dwFlags = WT_EXECUTEDEFAULT)
{
	if (0 == phNewWaitObject || ::IsBadWritePtr(phNewWaitObject, sizeof(XtlWaitObject<T,P>*)))
	{
		XTLASSERT(FALSE && "NULL Pointer");
		::SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>** ppWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>**>(phNewWaitObject);
	return WaitOrTimerCallbackAdapter<T,P>::RegisterWaitForSingleObject(
		ppWaitObject, hObject, Callback, Context, dwMilliseconds, dwFlags);
}

template <typename T, typename P>
inline 
BOOL
XtlUnregisterWaitEx(XtlWaitObject<T,P>* hWaitHandle, HANDLE CompletionEvent)
{
	if (::IsBadWritePtr(hWaitHandle, sizeof(XtlWaitObject<T,P>)))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>* pWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>*>(hWaitHandle);
	return WaitOrTimerCallbackAdapter<T,P>::UnregisterWaitEx(pWaitObject, CompletionEvent);
}

template <typename T, typename P>
inline 
BOOL
XtlUnregisterWait(XtlWaitObject<T,P>* hWaitHandle)
{
	if (::IsBadWritePtr(hWaitHandle, sizeof(XtlWaitObject<T,P>)))
	{
		::SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	WaitOrTimerCallbackAdapter<T,P>* pWaitObject = 
		reinterpret_cast<WaitOrTimerCallbackAdapter<T,P>*>(hWaitHandle);
	return WaitOrTimerCallbackAdapter<T,P>::UnregisterWait(pWaitObject);
}


template <typename T, typename P>
struct APCProcContext
{
	T* ClassInstance;
	void (T::*MemberFunc)(P);
	P Data;
	APCProcContext();
	APCProcContext(T* ClassInstance, void (T::*MemberFunc)(P), P Data) :
		ClassInstance(ClassInstance), MemberFunc(MemberFunc), Data(Data) 
	{}
};

template <typename T, typename P>
class APCProcAdapter
{
	typedef APCProcAdapter<T,P> ThisClass;
	typedef void (T::*MemberFunT)(P);
	APCProcContext<T,P> m_context;
public:
	static void CALLBACK APCProc(ULONG_PTR dwParam)
	{
		ThisClass* pT = reinterpret_cast<ThisClass*>(dwParam);
		(pT->m_context->ClassInstance>*(pT->m_context->MemberFunc))(pT->m_context->Data);
		XTLVERIFY( ::HeapFree(::GetProcessHeap(), 0, pT) );
	}
	static DWORD QueueUserAPC(
		HANDLE hThread,
		const APCProcContext<T,P>& Context)
	{
		ThisClass* pT = static_cast<ThisClass*>(
			::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ThisClass)));
		if (NULL == pT)
		
NDASUSERAPI
NdasQueryDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats);

/* <COMBINE NdasQueryDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryDeviceStatsByIdA(
	IN LPCSTR lpszNdasId,
	IN OUT PNDAS_DEVICE_STAT pDeviceStats);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryDeviceStatsById NdasQueryDeviceStatsByIdW
#else
#define NdasQueryDeviceStatsById NdasQueryDeviceStatsByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasQueryDeviceStats>

Declaration

BOOL
NdasQueryUnitDeviceStats(
	DWORD dwSlotNo, DWORD dwUnitNo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

BOOL
NdasQueryUnitDeviceStatsById(
	LPCTSTR lpszNdasId, DWORD dwUnitNo,
	PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

Summary

	Query statistics of the NDAS unit device.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device of the unit device

	dwUnitNo:
	[in] Unit number of the NDAS unit device

	pUnitDeviceStats:
	[in,out] A pointer to NDAS_UNITDEVICE_STAT structure,
	         containing device statistics.
			 Size field should be set as sizeof(NDAS_UNITDEVICE_STAT)
			 when calling the function.

*/

NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStats(
	IN DWORD dwSlotNo, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/* <COMBINE NdasQueryUnitDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdW(
	IN LPCWSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/* <COMBINE NdasQueryUnitDeviceStats> */
NDASUSER_LINKAGE
BOOL
NDASUSERAPI
NdasQueryUnitDeviceStatsByIdA(
	IN LPCSTR lpszNdasId, IN DWORD dwUnitNo,
	IN OUT PNDAS_UNITDEVICE_STAT pUnitDeviceStat);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasQueryUnitDeviceStatsById NdasQueryUnitDeviceStatsByIdW
#else
#define NdasQueryUnitDeviceStatsById NdasQueryUnitDeviceStatsByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasFindLogicalDeviceOfUnitDevice>

Declaration

BOOL 
NdasFindLogicalDeviceOfUnitDevice(
	IN  DWORD dwSlotNo, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

BOOL 
NdasFindLogicalDeviceOfUnitDeviceById(
	IN  LPCTSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

Summary

	Find the NDAS logical device associated with the NDAS unit device.

Parameters

	dwSlotNo:
	[in] Slot number of the NDAS device

	dwUnitNo:
	[in] Unit number of the NDAS unit device of the NDAS device
	specified by dwSlotNo

	pLogicalDeviceId:
	[out] Pointer to a NDAS_LOGICALDEVICE_ID that receives the 
	logical device ID. This can be used in subsequent calls to 
	functions related to NDAS logical devices.

Returns

	If the function succeeds, the return value is non-zero.
	If the function fails, the return value is zero. To get
	extended error information, call GetLastError.
	
*/

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDevice(
	IN  DWORD dwSlotNo, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/* <COMBINE NdasFindLogicalDeviceOfUnitDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdW(
	IN  LPCWSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/* <COMBINE NdasFindLogicalDeviceOfUnitDevice> */

NDASUSER_LINKAGE
BOOL 
NDASUSERAPI
NdasFindLogicalDeviceOfUnitDeviceByIdA(
	IN  LPCSTR lpszDeviceStringId, 
	IN  DWORD dwUnitNo,
	OUT NDAS_LOGICALDEVICE_ID* pLogicalDeviceId);

/*DOM-IGNORE-BEGIN*/
#ifdef UNICODE
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdW
#else
#define NdasFindLogicalDeviceOfUnitDeviceById NdasFindLogicalDeviceOfUnitDeviceByIdA
#endif
/*DOM-IGNORE-END*/

/* <TITLE NdasPlugInLogicalDevice>

Declaration:

BOOL 
NdasPlugInLogicalDevice(
	IN BOOL bWritable, 
	IN NDAS_LOGICALDEVICE_ID logicalDeviceId);

Summary:

	Plug in a NDAS logical device to the system.

Parameters:

	logicalDeviceId:
	[in] Logical Device ERR)
#define BEGIN_DBGPRT_BLOCK_WARN() BEGIN_DBGPRT_BLOCK(XDebug::OL_WARNING)
#define BEGIN_DBGPRT_BLOCK_INFO() BEGIN_DBGPRT_BLOCK(XDebug::OL_INFO)
#define BEGIN_DBGPRT_BLOCK_TRACE() BEGIN_DBGPRT_BLOCK(XDebug::OL_TRACE)
#define BEGIN_DBGPRT_BLOCK_NOISE() BEGIN_DBGPRT_BLOCK(XDebug::OL_NOISE)

XDEBUGV(XDebugAlways,  XDebug::OL_ALWAYS)
XDEBUGV(XDebugError,   XDebug::OL_ERROR)
XDEBUGV_SYSERR(XDebugErrorEx,   XDebug::OL_ERROR)
XDEBUGV_USERERR(XDebugErrorExUser,	XDebug::OL_ERROR)
XDEBUGV(XDebugWarning, XDebug::OL_WARNING)
XDEBUGV_SYSERR(XDebugWarningEx, XDebug::OL_WARNING)
XDEBUGV_USERERR(XDebugWarningExUser, XDebug::OL_WARNING)
XDEBUGV(XDebugInfo,    XDebug::OL_INFO)
XDEBUGV(XDebugTrace,   XDebug::OL_TRACE)
XDEBUGV(XDebugNoise,   XDebug::OL_NOISE)

XVDEBUGV(XVDebugInfo, XDebug::OL_INFO)
XVDEBUGV(XVDebugWarning, XDebug::OL_WARNING)
XVDEBUGV(XVDebugError, XDebug::OL_ERROR)
XVDEBUGV(XVDebugTrace, XDebug::OL_TRACE)
XVDEBUGV(XVDebugNoise, XDebug::OL_NOISE)

#ifdef NO_XDEBUG
#define DebugPrintf __noop
#define DPAny		__noop
#define DPAlways    __noop
#define DPError     __noop
#define DPErrorEx   __noop
#define DPErrorEx2  __noop
#define DPWarning   __noop
#define DPWarningEx __noop
#define DPWarningEx2 __noop
#define DPInfo      __noop
#define DPTrace     __noop
#define DPNoise     __noop

#else /* NO_XDEBUG */
#define DebugPrintf XDebugPrintf
#define DPAny		XDebugPrintf
#define DPAlways    XDEBUG_IS_ENABLED(XDebug::OL_ALWAYS) && XDebugAlways
#define DPError     XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugError
#define DPErrorEx   XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugErrorEx
#define DPErrorEx2  XDEBUG_IS_ENABLED(XDebug::OL_ERROR) && XDebugErrorExUser
#define DPWarning   XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarning
#define DPWarningEx XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarningEx
#define DPWarningEx2 XDEBUG_IS_ENABLED(XDebug::OL_WARNING) && XDebugWarningExUser
#define DPInfo      XDEBUG_IS_ENABLED(XDebug::OL_INFO) && XDebugInfo
#define DPTrace     XDEBUG_IS_ENABLED(XDebug::OL_TRACE) && XDebugTrace
#define DPNoise     XDEBUG_IS_ENABLED(XDebug::OL_NOISE) && XDebugNoise

#endif /* NO_XDEBUG */

#define DBGPRT_ERR DPError
#define DBGPRT_WARN DPWarning
#define DBGPRT_INFO DPInfo
#define DBGPRT_NOISE DPNoise
#define DBGPRT_TRACE DPTrace
#define DBGPRT_ERR_EX DPErrorEx
#define DBGPRT_WARN_EX DPWarningEx
#define DBGPRT_LEVEL DPAny

#define CHARTOHEX(dest, ch) \
	if ((ch) <= 9) { dest = TCHAR(CHAR(ch + '0')); } \
	else           { dest = TCHAR(CHAR(ch - 0xA + 'A')); }

template <typename T>
void
DPType(
	DWORD dwLevel, 
	const T* lpTypedData, 
	DWORD cbSize)
{
	_ASSERTE(!IsBadReadPtr(lpTypedData, cbSize));
	// default is a byte dump
	TCHAR szBuffer[4096];
	TCHAR* psz = szBuffer;
	*psz = TEXT('\n');
	++psz;
	const BYTE* lpb = reinterpret_cast<const BYTE*>(lpTypedData);
	for (DWORD i = 0; i < cbSize; ++i) {
		CHARTOHEX(*psz, (lpb[i] >> 4) )
			++psz;
		CHARTOHEX(*psz, (lpb[i] & 0x0F))
			++psz;
		*psz = (i > 0 && (((i + 1)% 20) == 0)) ? TEXT('\n') : TEXT(' ');
		++psz;
	}
	*psz = TEXT('\n');
	++psz;
	*psz = TEXT('\0');

	XDebugPrintf(dwLevel, szBuffer);
}

#undef CHARTOHEX

#define _LINE_STR3_(x) #x
#define _LINE_STR2_(x) _LINE_STR3_(x)
#define _LINE_STR_ _LINE_STR2_(__LINE__)

#ifndef XDBG_USE_FILENAME
	#ifdef _DEBUG
		#define XDBG_USE_FILENAME
	#endif
#endif
#ifndef XDBG_USE_FILENAME
	#ifdef DBG
		#define XDBG_USE_FILENAME
	#endif
#endif

// XDBG_USE_FUNC is used by default
// define XDBG_NO_FUNC to disable it
#ifdef XDBG_NO_FUNC
	#undef XDBG_USE_FUNC
#elif !defined(XDBG_USE_FUNC)
	#define XDBG_USE_FUNC
#endif

#ifdef XDBG_USE_FILENAME
#ifndef XDBG_FILENAME
#define XDBG_FILENAME_PREFIX _T(__FILE__) _T("(") _T(_LINE_STR_) _T("): ") 
#else
#define XDBG_FILENAME_PREFIX _T(XDBG_FILENAME) _T("(") _T(_LINE_STR_) _T("): ") 
#endif
#else
#define XDBG_FILENAME_PREFIX 
#endif

#ifdef XDBG_USE_FUNC
#define XDBG_FUNC_PREFIX _T(__FUNCTION__) _T(": ")
#else
#define XDBG_FUNC_PREFIX
#endif

#dDevInst;
	PPDO_DEVICE_DATA			pdoData;
	PBUSENUM_PLUGIN_HARDWARE_EX2	plugIn;
	PLANSCSI_ADD_TARGET_DATA		addTargetData;
	TA_LSTRANS_ADDRESS			bindAddr;

	UNREFERENCED_PARAMETER(PlugInTimeMask);

	keyInfo = (PKEY_BASIC_INFORMATION)ExAllocatePoolWithTag(PagedPool, 512, NDBUSREG_POOLTAG_KEYINFO);
	if(!keyInfo) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag(KEY_BASIC_INFORMATION) failed.\n"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	status = STATUS_SUCCESS;
	for(idxKey = 0 ; idxKey < MAX_DEVICES_IN_NDAS_REGISTRY; idxKey ++) {

		//
		//	Enumerate subkeys under the NDAS device root
		//

		status = ZwEnumerateKey(
						NdasDeviceReg,
						idxKey,
						KeyBasicInformation,
						keyInfo,
						512,
						&outLength
						);

		if(status == STATUS_NO_MORE_ENTRIES) {
			status = STATUS_SUCCESS;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("No more entry\n"));
			break;
		}
		if(status != STATUS_SUCCESS) {
			ASSERT(status != STATUS_BUFFER_OVERFLOW && status != STATUS_BUFFER_TOO_SMALL);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwEnumerateKey() failed. NTSTATUS:%08lx\n", status));
			ExFreePool(keyInfo);
			return STATUS_SUCCESS;
		}

		//
		//	Name verification
		//
		//	TODO
		//


		//
		//	Open a sub key (NdasDevices\Devxx) and plug in device with the registry key.
		//

		objectName.Length = objectName.MaximumLength = (USHORT)keyInfo->NameLength;
		objectName.Buffer = keyInfo->Name;
		InitializeObjectAttributes(		&objectAttributes,
										&objectName,
										OBJ_KERNEL_HANDLE,
										NdasDeviceReg,
										NULL
								);
		status = ZwOpenKey(&ndasDevInst, KEY_READ, &objectAttributes);
		if(!NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwOpenKey() failed. NTSTATUS:%08lx\n", status));
			continue;
		}

		Bus_KdPrint_Def(BUS_DBG_SS_INFO, ("'%wZ' opened.\n", &objectName));

		//
		//	Read NDAS device instance.
		//

		status = ReadNDASDevInstFromRegistry(ndasDevInst, 0, NULL, &outLength);
		if(status != STATUS_BUFFER_TOO_SMALL) {
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadNDASDevInstFromRegistry() failed. NTSTATUS:%08lx\n", status));
			continue;
		}
		plugIn = ExAllocatePoolWithTag(PagedPool, outLength, NDBUSREG_POOLTAG_PLUGIN);
		if(plugIn == NULL) {
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ExAllocatePoolWithTag() failed. NTSTATUS:%08lx\n", status));
			continue;
		}
		status = ReadNDASDevInstFromRegistry(ndasDevInst, outLength, plugIn, &outLength);
		if(!NT_SUCCESS(status)) {
			ExFreePool(plugIn);
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ReadNDASDevInstFromRegistry() failed. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Allocate AddTarget Data and read target and unit devices.
		//

		status = ReadTargetInstantly(ndasDevInst, 0, &addTargetData);
		if(!NT_SUCCESS(status)) {
			ExFreePool(plugIn);
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenTarget() failed. NTSTATUS:%08lx\n", status));
			continue;
		}
		addTargetData->ulSlotNo = plugIn->SlotNo;


		//
		//	Check to see if the NDAS device already plug-ined
		//

		pdoData = LookupPdoData(FdoData, plugIn->SlotNo);
		if(pdoData) {
			ExFreePool(addTargetData);
			ExFreePool(plugIn);
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PDO already exists. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Verify AddTargetData structure with DIBs in NDAS devices.
		//

		bindAddr.TAAddressCount = 1;
		bindAddr.Address[0].AddressLength = AddedAddress->AddressLength;
		bindAddr.Address[0].AddressType = AddedAddress->AddressType;
		RtlCopyMemory(&bindAddr.Address[0].Address, AddedAddress->Address, AddedAddress->AddressLength);

		status = NCommVerifyNdasDevWithDIB(addTargetData, &bindAddr, plugIn->MaxRequestBlocks);
		if(!NT_SUCCESS(status)) {
			ExFreePool(addTargetData);
			ExFreePool(plugIn);
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_VerifyLurDescWithDIB() failed. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Rewrite AddTargetData
		//

		status =RewriteTargetInstantly(ndasDevInst, 0, addTargetData);
		if(!NT_SUCCESS(status)) {
			ExFreePool(addTargetData);
			ExFreePool(plugIn);
			ZwClose(ndasDevInst);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenTarget() failed. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Close handle here
		//

		ZwClose(ndasDevInst);


		//
		//	Plug in a LanscsiBus device.
		//

		status = LSBus_PlugInLSBUSDevice(FdoData, plugIn);
		if(!NT_SUCCESS(status)) {
			ExFreePool(addTargetData);
			ExFreePool(plugIn);
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_PlugInLSBUSDevice() failed. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Add a target to the device which was just plugged in.
		//

		status = LSBus_AddTarget(FdoData, addTargetData);
		if(!NT_SUCCESS(status)) {
			LSBus_PlugOutLSBUSDevice(FdoData, plugIn->SlotNo, FALSE);

			ExFreePool(addTargetData);
			ExFreePool(plugIn);

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LSBus_AddTarget() failed. NTSTATUS:%08lx\n", status));
			continue;
		}


		//
		//	Free resources
		//

		ExFreePool(addTargetData);
		ExFreePool(plugIn);
	}

	ExFreePool(keyInfo);

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Enumerating NDAS devices from registry is completed. NTSTATUS:%08lx\n", status));

	return STATUS_SUCCESS;
}


//
//	Plug in NDAS devices by reading registry.
//

NTSTATUS
LSBus_PlugInDeviceFromRegistry(
		PFDO_DEVICE_DATA	FdoData,
		PTA_ADDRESS			AddedAddress,
		ULONG				PlugInTimeMask
) {
	NTSTATUS			status;
	HANDLE				DeviceReg;
	HANDLE				NdasDeviceReg;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);


	UNREFERENCED_PARAMETER(PlugInTimeMask);

	DeviceReg = NULL;
	NdasDeviceReg = NULL;

	//
	//	Parameter check
	//
	if(!FdoData) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FdoData NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	//
	//	Open the bus registry and NDAS device root.
	//
	ExAcquireFastMutexUnsafe(&FdoData->RegMutex);

	do {

		if(FdoData->PersistentPdo == FALSE) {
			status = STATUS_UNSUCCESSFUL;

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PersistentPDO has been off.\n"));
			break;
		}

		if(FdoData->StartStopRegistrarEnum == FALSE) {
			status = STATUS_UNSUCCESSFUL;

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("The registrar has stopped.\n"));
			break;
		}

		status = Reg_OpenDeviceControlRoot(&DeviceReg, KEY_READ|KEY_WRITE);
		if(!NT_SUCCESS(status)) {

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
			break;
		}
		status = Reg_OpenNdasDeviceRoot(&NdasDeviceReg, KEY_READ, DeviceReg);
		if(!NT_SUCCESS(status)) {
			ZwClose(DeviceReg);

			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
			break;
		}

		//
		//	Start to enumerate devices by reading the registry.
		//

		status = EnumerateByRegistry(FdoData, NdasDeviceReg, AddedAddress, PlugInTimeMask);

	} while(FALSE);


	//
	//	Close handles
	//

	if(NdasDeviceReg)
		ZwClose(NdasDeviceReg);
	if(DeviceReg)
		ZwClose(DeviceReg);

	ExReleaseFastMutexUnsafe(&FdoData->RegMutex);

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Worker to enumerate NDAS devices at late time.
//

#define NDBUSWRK_MAX_ADDRESSLEN	128

typedef struct _NDBUS_ENUMWORKER {

	PIO_WORKITEM		IoWorkItem;
	PFDO_DEVICE_DATA	FdoData;
	BOOLEAN				AddedAddressVaild;
	UCHAR				AddedTaAddress[NDBUSWRK_MAX_ADDRESSLEN];
	ULONG				PlugInTimeMask;

} NDBUS_ENUMWORKER, *PNDBUS_ENUMWORKER;


VOID
EnumWorker(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID Context
){
	PNDBUS_ENUMWORKER	ctx = (PNDBUS_ENUMWORKER)Context;
	PFDO_DEVICE_DATA	fdoData = ctx->FdoData;
	ULONG				timeMask = ctx->PlugInTimeMask;

	UNREFERENCED_PARAMETER(DeviceObject);


	//
	//	IO_WORKITEM is rare resource, give it back to the system now.
	//

	IoFreeWorkItem(ctx->IoWorkItem);


	//
	//	Start enumerating
	//

	if(ctx->AddedAddressVaild) {
		TA_LSTRANS_ADDRESS			bindAddr;
		PTA_ADDRESS					addedAddress = (PTA_ADDRESS)ctx->AddedTaAddress;

		bindAddr.TAAddressCount = 1;
		bindAddr.Address[0].AddressLength = addedAddress->AddressLength;
		bindAddr.Address[0].AddressType = addedAddress->AddressType;
		RtlCopyMemory(&bindAddr.Address[0].Address, addedAddress->Address, addedAddress->AddressLength);

		//
		//	We have to wait for the added address is accessible.
		//	Especially, we can not open LPX device early time on MP systems.
		//

		LstransWaitForAddress(&bindAddr, 10);

		LSBus_PlugInDeviceFromRegistry(fdoData, addedAddress, timeMask);
	}
	else
		LSBus_PlugInDeviceFromRegistry(fdoData,  NULL, timeMask);

	ExFreePool(Context);

}


//
//	Queue a workitem to plug in NDAS device by reading registry.
//

NTSTATUS
LSBUS_QueueWorker_PlugInByRegistry(
		PFDO_DEVICE_DATA	FdoData,
		PTA_ADDRESS			AddedAddress,
		ULONG				PlugInTimeMask
	) {
	NTSTATUS			status;
	PNDBUS_ENUMWORKER	workItemCtx;
	ULONG				addrLen;

	Bus_KdPrint_Def(BUS_DBG_SS_TRACE, ("entered.\n"));

	//
	//	Parameter check
	//
	if(!FdoData) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("FdoData NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}


	ExAcquireFastMutex(&FdoData->RegMutex);

	if(FdoData->PersistentPdo == FALSE) {
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("PersistentPDO has been off.\n"));
		return STATUS_INVALID_PARAMETER;
	}

	ExReleaseFastMutex(&FdoData->RegMutex);

	workItemCtx = (PNDBUS_ENUMWORKER)ExAllocatePoolWithTag(NonPagedPool, sizeof(NDBUS_ENUMWORKER), NDBUSREG_POOLTAG_WORKITEM);
	if(!workItemCtx) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	workItemCtx->PlugInTimeMask = PlugInTimeMask;
	workItemCtx->FdoData		= FdoData;


	//
	//	set binding address which is added.
	//
	addrLen = FIELD_OFFSET(TA_ADDRESS,Address) + AddedAddress->AddressLength;
	if(AddedAddress && addrLen <= NDBUSWRK_MAX_ADDRESSLEN) {
		RtlCopyMemory(	&workItemCtx->AddedTaAddress,
						AddedAddress,
						addrLen
						);
		workItemCtx->AddedAddressVaild = TRUE;
	} else {
		workItemCtx->AddedAddressVaild = FALSE;
	}

	workItemCtx->IoWorkItem = IoAllocateWorkItem(FdoData->Self);
	if(workItemCtx->IoWorkItem == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	IoQueueWorkItem(
		workItemCtx->IoWorkItem,
		EnumWorker,
		DelayedWorkQueue,
		workItemCtx
		);

	return STATUS_SUCCESS;

cleanup:
	if(workItemCtx) {
		ExFreePool(workItemCtx);
	}

	return status;
}

//////////////////////////////////////////////////////////////////////////
//
//	TDI client
//
NTSTATUS
Reg_InitializeTdiPnPHandlers(
	PFDO_DEVICE_DATA	FdoData
)

/*++

Routine Description:

    Register address handler routinges with TDI

Arguments:
    
    None


Return Value:

    NTSTATUS -- Indicates whether registration succeded

--*/

{
    NTSTATUS                    status;
    TDI_CLIENT_INTERFACE_INFO   info;
    UNICODE_STRING              clientName;
    
    PAGED_CODE ();

 
    //
    // Setup the TDI request structure
    //

    RtlZeroMemory (&info, sizeof (info));
    RtlInitUnicodeString(&clientName, L"NDASBUS");
#ifdef TDI_CURRENT_VERSION
    info.TdiVersion = TDI_CURRENT_VERSION;
#else
    info.MajorTdiVersion = 2;
    info.MinorTdiVersion = 0;
#endif
    info.Unused = 0;
    info.ClientName = &clientName;
    info.BindingHandler = NULL;
    info.AddAddressHandlerV2 = Reg_AddAddressHandler;
    info.DelAddressHandlerV2 = Reg_DelAddressHandler;
    info.PnPPowerHandler = NULL;

    //
    // Register handlers with TDI
    //

    status = TdiRegisterPnPHandlers (&info, sizeof (info), &FdoData->TdiClient);
    if (!NT_SUCCESS (status)) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
					"AfdInitializeAddressList: Failed to register PnP handlers: %lx .\n",
					status));
        return status;
    }

    return STATUS_SUCCESS;
}


VOID
Reg_DeregisterTdiPnPHandlers (
	PFDO_DEVICE_DATA	FdoData
){

    if (FdoData->TdiClient) {
        TdiDeregisterPnPHandlers (FdoData->TdiClient);
        FdoData->TdiClient = NULL;
	}
}

VOID
Reg_AddAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING  DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI add address handler

Arguments:
    
    NetworkAddress  - new network address available on the system

    DeviceName      - name of the device to which address belongs

    Context         - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

    PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
			"AfdDelAddressHandler: "
			"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}
	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
										DeviceName->Buffer,
										(ULONG)NetworkAddress->AddressType,
										(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE) &&
		NetworkAddress->AddressType == TDI_ADDRESS_TYPE_LPX
		){
			PTDI_ADDRESS_LPX	lpxAddr;

			lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: %02x:%02x:%02x:%02x:%02x:%02x\n",
									lpxAddr->Node[0],
									lpxAddr->Node[1],
									lpxAddr->Node[2],
									lpxAddr->Node[3],
									lpxAddr->Node[4],
									lpxAddr->Node[5]));
			//
			//	LPX may leave dummy values.
			//
			RtlZeroMemory(lpxAddr->Reserved, sizeof(lpxAddr->Reserved));

			//
			//	Check to see if FdoData for TdiPnP is created.
			//

			ExAcquireFastMutex(&Globals.Mutex);
			if(Globals.PersistentPdo && Globals.FdoDataTdiPnP) {
				LSBUS_QueueWorker_PlugInByRegistry(Globals.FdoDataTdiPnP, NetworkAddress, 0);
				ExReleaseFastMutex(&Globals.Mutex);
			} else {
				ExReleaseFastMutex(&Globals.Mutex);
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: Binding address came up, but there is no FdoData for TdiPnP\n"));
			}

	//
	//	IP	address
	//
	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}

VOID
Reg_DelAddressHandler ( 
	IN PTA_ADDRESS NetworkAddress,
	IN PUNICODE_STRING DeviceName,
	IN PTDI_PNP_CONTEXT Context
    )
/*++

Routine Description:

    TDI delete address handler

Arguments:
    
    NetworkAddress  - network address that is no longer available on the system

    Context1        - name of the device to which address belongs

    Context2        - PDO to which address belongs


Return Value:

    None

--*/
{
	UNICODE_STRING	lpxPrefix;

	PAGED_CODE ();

	UNREFERENCED_PARAMETER(Context);

	if (DeviceName==NULL) {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, (
			"AfdDelAddressHandler: "
			"NO DEVICE NAME SUPPLIED when deleting address of type %d.\n",
			NetworkAddress->AddressType));
		return;
	}
	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DeviceName=%ws AddrType=%u AddrLen=%u\n",
		DeviceName->Buffer,
		(ULONG)NetworkAddress->AddressType,
		(ULONG)NetworkAddress->AddressLength));

	//
	//	LPX
	//
	RtlInitUnicodeString(&lpxPrefix, LPX_DEVICE_NAME_PREFIX);

	if(	RtlPrefixUnicodeString(&lpxPrefix, DeviceName, TRUE)){
		PTDI_ADDRESS_LPX	lpxAddr;

		lpxAddr = (PTDI_ADDRESS_LPX)NetworkAddress->Address;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("LPX: %02x:%02x:%02x:%02x:%02x:%02x\n",
			lpxAddr->Node[0],
			lpxAddr->Node[1],
			lpxAddr->Node[2],
			lpxAddr->Node[3],
			lpxAddr->Node[4],
			lpxAddr->Node[5]));

		//
		//	IP	address
		//
	} else if(NetworkAddress->AddressType == TDI_ADDRESS_TYPE_IP) {
		PTDI_ADDRESS_IP	ipAddr;
		PUCHAR			digit;

		ipAddr = (PTDI_ADDRESS_IP)NetworkAddress->Address;
		digit = (PUCHAR)&ipAddr->in_addr;
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("IP: %u.%u.%u.%u\n",digit[0],digit[1],digit[2],digit[3]));
	} else {
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("AddressType %u discarded.\n", (ULONG)NetworkAddress->AddressType));
	}
}


//////////////////////////////////////////////////////////////////////////
//
//	Exported functions to IOCTL.
//

//
//	Register a device by writing registry.
//

NTSTATUS
LSBus_RegisterDevice(
		PFDO_DEVICE_DATA				FdoData,
		PBUSENUM_PLUGIN_HARDWARE_EX2	Plugin
){
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				ndasDevInst;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;


	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//

	ExAcquireFastMutex(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenServiceRegistry() failed.\n"));
		return status;
	}

	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {

		ZwClose(busDevReg);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("OpenNdasDeviceRegistry() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, Plugin->SlotNo, TRUE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {

		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return	status;
	}

	//
	//	Before writing information, clean up the device instance key.
	//

	DrDeleteAllSubKeys(ndasDevInst);

	//
	//	Write plug in information
	//

	status = WriteNDASDevToRegistry(ndasDevInst, Plugin);


	//
	//	Close handles
	//

	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);

	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutex(&FdoData->RegMutex);

	return status;
}


NTSTATUS
LSBus_RegisterTarget(
	PFDO_DEVICE_DATA			FdoData,
	PLANSCSI_ADD_TARGET_DATA	AddTargetData
){
	NTSTATUS	status;
	HANDLE		busDevReg;
	HANDLE		ndasDevRoot;
	HANDLE		ndasDevInst;
	HANDLE		targetKey;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;
	targetKey = NULL;

	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	ExAcquireFastMutex(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, AddTargetData->ulSlotNo, FALSE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return	status;
	}

	status = Reg_OpenTarget(&targetKey, AddTargetData->ucTargetId, TRUE, ndasDevInst);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevRoot);
		ZwClose(ndasDevInst);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenTarget() failed.\n"));

		return	status;
	}

	//
	//	Before writing information, clean up the target key.
	//

	DrDeleteAllSubKeys(targetKey);

	//
	//	Write target information
	//

	status = WriteTargetToRegistry(targetKey, AddTargetData);


	//
	//	Close handles
	//
	if(targetKey)
		ZwClose(targetKey);
	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutex(&FdoData->RegMutex);

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Adding an NDAS device into registry is completed. NTSTATUS:%08lx\n", status));

	return status;
}



NTSTATUS
LSBus_UnregisterDevice(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
) {
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				devInstTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	busDevReg = NULL;
	ndasDevRoot = NULL;
	devInstTobeDeleted = NULL;


	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	ExAcquireFastMutex(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);

		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	if(SlotNo != NDASBUS_SLOT_ALL) {
		status = Reg_OpenDeviceInst(&devInstTobeDeleted, SlotNo, FALSE, ndasDevRoot);
		if(NT_SUCCESS(status)) {

			//
			//	Delete a NDAS device instance.
			//
			status = DrDeleteAllSubKeys(devInstTobeDeleted);
			if(NT_SUCCESS(status)) {
				status = ZwDeleteKey(devInstTobeDeleted);
			}
#if DBG
			else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DrDeleteAllSubkeys() failed. SlotNo:%u NTSTATUS:%08lx\n", SlotNo, status));
			}
#endif

			ZwClose(devInstTobeDeleted);

#if DBG
			if(NT_SUCCESS(status)) {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("A device(Slot %d) is deleted.\n", SlotNo));
			} else {
				Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwDeleteKey() failed. SlotNo:%u NTSTATUS:%08lx\n", SlotNo, status));
			}
#endif
		}
	} else {
		status = DrDeleteAllSubKeys(ndasDevRoot);
	}


	//
	//	Close handles
	//

	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutex(&FdoData->RegMutex);

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Removing a DNAS device from registry is completed. NTSTATUS:%08lx\n", status));

	return status;
}


NTSTATUS
LSBus_UnregisterTarget(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo,
		ULONG				TargetId
) {
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				ndasDevInst;
	HANDLE				targetIdTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	busDevReg = NULL;
	ndasDevRoot = NULL;
	ndasDevInst = NULL;
	targetIdTobeDeleted = NULL;

	ExAcquireFastMutex(&FdoData->RegMutex);

	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&ndasDevInst, SlotNo, FALSE, ndasDevRoot);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ZwClose(ndasDevInst);
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceInst() failed.\n"));
		return status;
	}

	status = Reg_OpenTarget(&targetIdTobeDeleted, TargetId, FALSE, ndasDevInst);
	if(NT_SUCCESS(status)) {

		//
		//	Delete an NDAS device instance.
		//
		status = DrDeleteAllSubKeys(targetIdTobeDeleted);
		if(NT_SUCCESS(status)) {
			status = ZwDeleteKey(targetIdTobeDeleted);
		}
#if DBG
		else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("DrDeleteAllSubKeys() failed. SlotNo:%u Target %u NTSTATUS:%08lx\n", SlotNo, TargetId, status));
		}
#endif
		ZwClose(targetIdTobeDeleted);
#if DBG
		if(NT_SUCCESS(status)) {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("A device(Slot %d Target %u) is deleted.\n", SlotNo, TargetId));
		} else {
			Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("ZwDeleteKey() failed. SlotNo:%u Target %u NTSTATUS:%08lx\n", SlotNo, TargetId, status));
		}
#endif
	}


	//
	//	Close handles
	//
	if(ndasDevInst)
		ZwClose(ndasDevInst);
	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutex(&FdoData->RegMutex);

	Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Removing a target into registry is completed. NTSTATUS:%08lx\n", status));

	return status;
}


NTSTATUS
LSBus_IsRegistered(
		PFDO_DEVICE_DATA	FdoData,
		ULONG				SlotNo
){
	NTSTATUS			status;
	HANDLE				busDevReg;
	HANDLE				ndasDevRoot;
	HANDLE				devInstTobeDeleted;

	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	status = STATUS_SUCCESS;
	busDevReg = NULL;
	ndasDevRoot = NULL;
	devInstTobeDeleted = NULL;


	//
	//	Open a BUS device registry, an NDAS device root, and device instance key.
	//
	ExAcquireFastMutex(&FdoData->RegMutex);

	status = Reg_OpenDeviceControlRoot(&busDevReg, KEY_READ|KEY_WRITE);
	if(!NT_SUCCESS(status)) {
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenDeviceControlRoot() failed.\n"));
		return status;
	}
	status = Reg_OpenNdasDeviceRoot(&ndasDevRoot, KEY_READ|KEY_WRITE, busDevReg);
	if(!NT_SUCCESS(status)) {
		ZwClose(busDevReg);
		ExReleaseFastMutex(&FdoData->RegMutex);

		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Reg_OpenNdasDeviceRoot() failed.\n"));
		return status;
	}

	status = Reg_OpenDeviceInst(&devInstTobeDeleted, SlotNo, FALSE, ndasDevRoot);

	//
	//	Close handles
	//

	if(ndasDevRoot)
		ZwClose(ndasDevRoot);
	if(busDevReg)
		ZwClose(busDevReg);

	ExReleaseFastMutex(&FdoData->RegMutex);

	return status;
}


//////////////////////////////////////////////////////////////////////////
//
//	Init & Destory the registrar
//
NTSTATUS
LSBus_RegInitialize(
	PFDO_DEVICE_DATA	FdoData
){
	NTSTATUS status;
	
	ExAcquireFastMutex(&Globals.Mutex);
	if(Globals.FdoDataTdiPnP == NULL) {
		Globals.FdoDataTdiPnP = FdoData;
		ExReleaseFastMutex(&Globals.Mutex);
	} else {
		ExReleaseFastMutex(&Globals.Mutex);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Another FDO owns it.\n"));
		return STATUS_UNSUCCESSFUL;
	}

	status = Reg_InitializeTdiPnPHandlers(FdoData);

	return status;
}


VOID
LSBus_RegDestroy(
	PFDO_DEVICE_DATA	FdoData
){
	ExAcquireFastMutex(&Globals.Mutex);
	if(Globals.FdoDataTdiPnP == FdoData) {
		Globals.FdoDataTdiPnP = NULL;
		ExReleaseFastMutex(&Globals.Mutex);
	} else {
		ExReleaseFastMutex(&Globals.Mutex);
		Bus_KdPrint_Def(BUS_DBG_SS_ERROR, ("Not owner.\n"));
		return;
	}

	Reg_DeregisterTdiPnPHandlers(FdoData);

}