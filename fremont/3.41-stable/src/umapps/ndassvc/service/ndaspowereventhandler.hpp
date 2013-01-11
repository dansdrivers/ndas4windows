#ifndef _NDAS_POWER_EVENT_HANDLER_H_
#define _NDAS_POWER_EVENT_HANDLER_H_
#pragma once
#include <xtl/xtlpnpevent.h>
#include <xtl/xtlautores.h>

class CNdasService;

class CNdasServicePowerEventHandler :
	public XTL::CPowerEventHandler<CNdasServicePowerEventHandler>
{
public:

	CNdasServicePowerEventHandler(CNdasService& service);
	HRESULT Initialize();

	LRESULT OnQuerySuspend(DWORD dwFlags);
	void OnQuerySuspendFailed();
	void OnSuspend();
	void OnResumeAutomatic();
	void OnResumeSuspend();
	void OnResumeCritical();

protected:
	CNdasService& m_service;
private:
	CNdasServicePowerEventHandler(const CNdasServicePowerEventHandler&);
	const CNdasServicePowerEventHandler& operator=(const CNdasServicePowerEventHandler&);
};

#endif /* _NDAS_POWER_EVENT_HANDLER_H_ */
