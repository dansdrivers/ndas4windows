#include "stdatl.hpp"
#include "eventwnd.hpp"

CAppModule _Module;

int __cdecl wmain(int argc, WCHAR** argv)
{
	HRESULT hr;
	int ret;

	hr = CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hr));

	hr = _Module.Init(NULL, GetModuleHandle(NULL));
	ATLASSERT(SUCCEEDED(hr));

	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CPnpEventConsumerWindow wnd;
	if (NULL != wnd.Create(NULL))
	{
		wnd.ShowWindow(SW_MINIMIZE);
		wnd.UpdateWindow();
		ret = theLoop.Run();
	}

	_Module.RemoveMessageLoop();
	_Module.Term();

	CoUninitialize();

	return ret;
}
