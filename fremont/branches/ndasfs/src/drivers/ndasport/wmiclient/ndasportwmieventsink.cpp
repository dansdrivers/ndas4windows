#pragma once
#include <atlbase.h>
#include <atlcom.h>
#include <ndas/ndasport.wmi.h>
#include "ndasportwmieventsink.hpp"

STDMETHODIMP
CCoNdasPortWmiEventSink::Indicate(
	LONG ObjectCount, 
	IWbemClassObject __RPC_FAR *__RPC_FAR *apObjArray )
{
	HRESULT hr = S_OK;
	NdasPortWmiEvent wmiEvent;

	for (int i = 0; i < ObjectCount; i++)
	{
		IWbemClassObject* pClassObject = apObjArray[i];
		CComVariant v;
		hr = pClassObject->Get(CComBSTR(L"__CLASS"), 0, &v, 0, 0);
		if (FAILED(hr) || V_VT(&v) != VT_BSTR)
		{
			printf("class name is not available, hr=0x%X\n", hr);
			return WBEM_E_FAILED;
		}
		printf("class=%ls\n", V_BSTR(&v));
		v.Clear();

		hr = pClassObject->Get(CComBSTR(L"EventType"), 0, &v, NULL, NULL);
		if (FAILED(hr))
		{
			printf("Getting EventType failed, hr=0x%X\n", hr);
			return WBEM_E_FAILED;
		}

		ATLVERIFY(SUCCEEDED(v.ChangeType(VT_UINT)));

		wmiEvent.EventType = V_UINT(&v);
		v.Clear();

		hr = pClassObject->Get(CComBSTR(L"LogicalUnitAddress"), 0, &v, NULL, NULL);
		if (FAILED(hr))
		{
			printf("Getting LogicalUnitAddress failed, hr=0x%X\n", hr);
			return WBEM_E_FAILED;
		}

		ATLVERIFY(SUCCEEDED(v.ChangeType(VT_UINT)));

		wmiEvent.LogicalUnitAddress = V_UINT(&v);
		v.Clear();

		printf("NdasPortWmiEvent, type=%u, lu=%08X\n",
			wmiEvent.EventType, wmiEvent.LogicalUnitAddress);
	}

	return WBEM_S_NO_ERROR;
}

STDMETHODIMP
CCoNdasPortWmiEventSink::SetStatus(
	LONG Flags, 
	HRESULT Result, 
	BSTR Param, 
	IWbemClassObject __RPC_FAR *pObjParam )
{
	if (Flags == WBEM_STATUS_COMPLETE)
	{
		printf("Call complete. hr=0x%X\n", Result);
	}
	else if (Flags == WBEM_STATUS_PROGRESS)
	{
		printf("Call in progress.\n");
	}

	return WBEM_S_NO_ERROR;
}
