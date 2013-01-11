#include "dload.h"

struct GenericDllGetVersion : DelayedLoader<GenericDllGetVersion>
{
	GenericDllGetVersion(HMODULE hModule) :
		DelayedLoader<GenericDllGetVersion>(hModule)
	{
	}
	HRESULT DllGetVersion(DLLVERSIONINFO* pdvi)
	{
		return Invoke<DLLVERSIONINFO*, HRESULT>("DllGetVersion", pdvi);
	}
};
