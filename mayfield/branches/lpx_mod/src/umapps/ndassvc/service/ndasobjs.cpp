#include "stdafx.h"
#include "ndascfg.h"
#include "ndasobjs.h"
#include "objbase.h"

LPCGUID
pGetNdasHostGuid()
{
	static LPGUID pHostGuid = NULL;
	static GUID hostGuid;

	if (NULL != pHostGuid) {
		return pHostGuid;
	}

	BOOL fSuccess = _NdasSystemCfg.GetValueEx(
		_T("Host"),
		_T("HostID"),
		&hostGuid,
		sizeof(GUID));

	if (!fSuccess) {
		HRESULT hr = ::CoCreateGuid(&hostGuid);
		_ASSERTE(SUCCEEDED(hr));

		fSuccess = _NdasSystemCfg.SetValueEx(
			_T("Host"),
			_T("HostID"),
			REG_BINARY,
			&hostGuid,
			sizeof(GUID));

	}

	pHostGuid = &hostGuid;
	return pHostGuid;
}

CNdasHostInfoCache*
pGetNdasHostInfoCache()
{
	static CNdasHostInfoCache* phi = NULL;
	if (NULL != phi) {
		return phi;
	}
	phi = new CNdasHostInfoCache();
	_ASSERTE(NULL != phi);
	return phi;
}

