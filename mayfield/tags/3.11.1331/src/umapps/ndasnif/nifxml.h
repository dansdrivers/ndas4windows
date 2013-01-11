#pragma once
#include <atlcoll.h>

struct NifDeviceEntry
{
	CComBSTR Name;
	CComBSTR DeviceId;
	CComBSTR WriteKey;
	CComBSTR Description;
};

typedef CSimpleArray<NifDeviceEntry> NifDeviceArray;

class NifXmlSerializer
{
public:

	NifXmlSerializer();
	~NifXmlSerializer();

	HRESULT Load(BSTR FileName, NifDeviceArray& Array);
	HRESULT Save(BSTR FileName, const NifDeviceArray& Array);
};


