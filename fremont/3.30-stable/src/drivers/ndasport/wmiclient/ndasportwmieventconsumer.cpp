#include <atlbase.h>
#include <atlcom.h>
#include "coinit.hpp"
#include "ndasportwmieventsink.hpp"

CComModule _Module;

int rpterr(HRESULT hr, int retcode = -1)
{
	LPWSTR description = NULL;
	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		hr,
		MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
		(LPWSTR)&description,
		0,
		NULL);
	fprintf(stderr, "%ls\n", description);
	return retcode;
}
int __cdecl wmain(int argc, WCHAR** argv)
{
	HRESULT hr;
	CCoInit coinit;

	printf("Initializing COM...\n");

	hr = coinit.Initialize(COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		fprintf(stderr, "CoInitialize failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	printf("Initializing COM Security...\n");

	hr = CoInitializeSecurity(
		NULL,
		-1,
		NULL,
		NULL,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL,
		EOAC_NONE,
		NULL);

	if (FAILED(hr))
	{
		fprintf(stderr, "CoInitializeSecurity failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}
	
	CComPtr<IWbemLocator> pLocator;

	printf("Creating IWbemLocator ...\n");

	hr = pLocator.CoCreateInstance(
		CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER);

	if (FAILED(hr))
	{
		fprintf(stderr, "CoCreateInstance(CLSID_WbemLocator, IWbemLocator) failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	CComPtr<IWbemServices> pServices;

	printf("Connecting to IWbemServices(ROOT\\WMI) ...\n");

	hr = pLocator->ConnectServer(
		CComBSTR(L"ROOT\\WMI"),
		NULL,
		NULL,
		0,
		NULL,
		NULL,
		NULL,
		&pServices);

	if (FAILED(hr))
	{
		fprintf(stderr, "IWbemServices->ConnectServer failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	fprintf(stdout, "Connected to ROOT\\WMI\n");

	//
	// Set security levels on the proxy 
	//

	hr = CoSetProxyBlanket(
		pServices,                   // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx 
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx 
		NULL,                        // Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE);                  // proxy capabilities 

	//
	// Receive event notifications 
	//

	CComPtr<IUnsecuredApartment> pUnsecApp;

	printf("Creating IUnsecuredApartment...\n");

	hr = pUnsecApp.CoCreateInstance(CLSID_UnsecuredApartment);

	if (FAILED(hr))
	{
		fprintf(stderr, "CoCreateInstance(CLSID_UnsecuredApartment, IUnsecuredApartment) failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	CComObject<CCoNdasPortWmiEventSink>* pEventSinkClass;

	printf("Creating CCoNdasPortWmiEventSink...\n");

	hr = CComObject<CCoNdasPortWmiEventSink>::CreateInstance(&pEventSinkClass);

	if (FAILED(hr))
	{
		fprintf(stderr, "CCoNdasPortWmiEventSink::CreateInstance failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	CComPtr<IUnknown> pStubUnknown;

	printf("Creating ObjectStub...\n");

	hr = pUnsecApp->CreateObjectStub(pEventSinkClass, &pStubUnknown);

	if (FAILED(hr))
	{
		fprintf(stderr, "IUnsecuredApartment->CreateObjectStub failed, hr=0x%X\n", hr);
		return rpterr(hr);
	}

	CComQIPtr<IWbemObjectSink> pStubSink = pStubUnknown;

	if (NULL == pStubSink.p)
	{
		fprintf(stderr, "QueryInterface(IWbemObjectSink) failed, hr=0x%x\n",
			E_NOINTERFACE);
		return rpterr(hr);
	}

	printf("Running ExecNotificationQueryAsync...\n");

	// LPCWSTR query = L"SELECT * FROM NdasPortWmiEvent";
	LPCWSTR query = L"SELECT * FROM NdasAtaWmi_Connection_Link_Event";

	// The ExecNotificationQueryAsync method will call
	// The EventQuery::Indicate method when an event occurs
	hr = pServices->ExecNotificationQueryAsync(
		CComBSTR(L"WQL"),
		CComBSTR(query),
		WBEM_FLAG_SEND_STATUS,
		NULL,
		pStubSink);

	if (FAILED(hr))
	{
		fprintf(stderr, "ExecNotificationQueryAsync failed, hr=0x%x\n", hr);
		return rpterr(hr);
	}

	// Wait for events
	fprintf(stdout, "Press any key to stop...\n");
	CHAR key;
	fread(&key, 1, 1, stdin);

	hr = pServices->CancelAsyncCall(pStubSink);

	if (FAILED(hr))
	{
		fprintf(stderr, "WARN: CancelAsyncCall failed, hr=0x%x\n", hr);
		rpterr(hr);
	}

	return 0;
}
