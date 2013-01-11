#ifndef _PNPEVENT_PROCESSOR_H_
#define _PNPEVENT_PROCESSOR_H_
#pragma once

#include <dbt.h>
#include <pbt.h>

namespace ximeta {

	class CDeviceEventHandler
	{
	public:

		virtual LRESULT OnConfigChangeCanceled();
		virtual LRESULT OnConfigChanged();
		virtual LRESULT OnDevNodesChanged();
		virtual LRESULT OnQueryChangeConfig();

		virtual LRESULT OnCustomEvent(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceArrival(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceQueryRemove(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceQueryRemoveFailed(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceRemoveComplete(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceRemovePending(PDEV_BROADCAST_HDR pdbhdr);
		virtual LRESULT OnDeviceTypeSpecific(PDEV_BROADCAST_HDR pdbhdr);

		virtual LRESULT OnUserDefined(_DEV_BROADCAST_USERDEFINED* pdbuser);

		LRESULT OnDeviceEvent(WPARAM wParam, LPARAM lParam);
	};

	class CPowerEventHandler
	{
	public:

		//
		// Battery power is low.
		//
		virtual void OnBatteryLow();
		//
		// OEM-defined event occurred.
		//
		virtual void OnOemEvent(DWORD dwEventCode);
		//
		// Power status has changed.
		//
		virtual void OnPowerStatusChange();
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
		virtual LRESULT OnQuerySuspend(DWORD dwFlags);
		//
		// Suspension request denied.
		//
		virtual void OnQuerySuspendFailed();
		//
		// Operation resuming automatically after event.
		//
		virtual void OnResumeAutomatic();
		//
		// Operation resuming after critical suspension.
		//
		virtual void OnResumeCritical();
		//
		// Operation resuming after suspension.
		//
		virtual void OnResumeSuspend();
		//
		// System is suspending operation.
		//
		virtual void OnSuspend();

		//
		// Power Event Handler Dispatcher
		//
		LRESULT OnPowerEvent(WPARAM wParam, LPARAM lParam);
	};

}

#endif
