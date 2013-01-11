#include "stdafx.h"
#include "ndas/ndasautoregscope.h"
#include "ndas/ndassyscfg.h"
#include "xs/xautores.h"
#include <algorithm>
#include <vector>

// {5D303CCE-0F53-4351-A881-D1CC828F47AE}
static CONST GUID NDAS_SYS_GUID = 
{ 0x5d303cce, 0xf53, 0x4351, { 0xa8, 0x81, 0xd1, 0xcc, 0x82, 0x8f, 0x47, 0xae } };

static LPCTSTR NDAS_SYS_REGKEY = _T("Software\\NDAS");
static LPCTSTR NDAS_SYS_SERVICE_REGKEY	= _T("ndassvc");
static LPCTSTR NDAS_SYS_ARFLAGS_REGVAL	= _T("ARFlags");

class CNdasAutoRegScope: public _NDAS_AUTOREG_SCOPE 
{
public:
	CNdasAutoRegScope()
	{
		::ZeroMemory(rBegin, sizeof(rBegin));
		::ZeroMemory(rEnd, sizeof(rEnd));
		::ZeroMemory(reserved, sizeof(reserved));
		grantedAccess = 0;
	}

	CNdasAutoRegScope(
		CONST NDAS_DEVICE_ID& x, 
		CONST NDAS_DEVICE_ID& y, 
		ACCESS_MASK access)
	{
		CNdasAutoRegScope();
		::CopyMemory(rBegin, x.Node, sizeof(x.Node));
		::CopyMemory(rEnd, y.Node, sizeof(y.Node));
		grantedAccess = access;
	}

	CNdasAutoRegScope(CONST BYTE* pbData)
	{
		_ASSERTE(!IsBadReadPtr(pbData, sizeof(NDAS_AUTOREG_SCOPE)));
		NDAS_AUTOREG_SCOPE* pScope = reinterpret_cast<NDAS_AUTOREG_SCOPE*>(this);
		::CopyMemory(pScope, pbData, sizeof(NDAS_AUTOREG_SCOPE));
	}

	operator const NDAS_AUTOREG_SCOPE&()
	{
		return *reinterpret_cast<NDAS_AUTOREG_SCOPE*>(this);
	}

};

class CNdasAutoRegScopeDataSerializer
{
	DWORD m_cbUsed;
	DWORD m_cbRem;
	LPBYTE m_lpCur;
public:
	CNdasAutoRegScopeDataSerializer(DWORD cbData, LPBYTE lpData) :
		m_cbUsed(0),
		m_cbRem(cbData),
		m_lpCur(lpData)
	{}

	void operator()(const NDAS_AUTOREG_SCOPE& e)
	{
		if (m_cbRem >= sizeof(NDAS_AUTOREG_SCOPE)) {
			::CopyMemory(m_lpCur, &e, sizeof(NDAS_AUTOREG_SCOPE));
			m_cbRem -= sizeof(NDAS_AUTOREG_SCOPE);
			m_lpCur += sizeof(NDAS_AUTOREG_SCOPE);
			m_cbUsed += sizeof(NDAS_AUTOREG_SCOPE);
		}
	}

	// returns the number of entries serialized
	operator DWORD()
	{
		return m_cbUsed;
	}
};

CNdasAutoRegScopeData::CNdasAutoRegScopeData()
{
}

CNdasAutoRegScopeData::~CNdasAutoRegScopeData()
{
}

BOOL
CNdasAutoRegScopeData::AddScope(
	CONST NDAS_DEVICE_ID& rBegin, 
	CONST NDAS_DEVICE_ID& rEnd, 
	ACCESS_MASK access)
{
	NDAS_AUTOREG_SCOPE entry = {0};
	
	::CopyMemory(entry.rBegin, rBegin.Node, sizeof(rBegin.Node));
	::CopyMemory(entry.rEnd, rEnd.Node, sizeof(rEnd.Node));
	entry.grantedAccess = access;

	m_scopes.push_back(entry);
	return TRUE;
}

DWORD
CNdasAutoRegScopeData::GetCount()
{
	return (DWORD) m_scopes.size();
}

BOOL
CNdasAutoRegScopeData::RemoveScope(DWORD dwIndex)
{
	if (dwIndex >= m_scopes.size()) {
		return FALSE;
	}

	std::vector<NDAS_AUTOREG_SCOPE>::iterator itr = m_scopes.begin();
	itr += dwIndex;
	m_scopes.erase(itr);

	return TRUE;
}

BOOL 
CNdasAutoRegScopeData::GetScope(
	DWORD dwIndex, 
	NDAS_DEVICE_ID& rBegin,
	NDAS_DEVICE_ID& rEnd,
	ACCESS_MASK& access)
{
	if (dwIndex >= m_scopes.size()) {
		return FALSE;
	}

	const NDAS_AUTOREG_SCOPE& scope = m_scopes.at(dwIndex);
	::CopyMemory(rBegin.Node, scope.rBegin, sizeof(scope.rBegin));
	::CopyMemory(rEnd.Node, scope.rEnd , sizeof(scope.rEnd ));
	access = scope.grantedAccess;

	return TRUE;
}

BOOL
CNdasAutoRegScopeData::LoadFromSystem()
{
	//
	// Set MD5 hash to the system key
	//
	m_scopes.clear();

	BYTE ARFlagsData[1024] = {0};
	DWORD cbUsed = 0;

	BOOL fSuccess = ::NdasSysGetConfigValue(
		NDAS_SYS_SERVICE_REGKEY,
		NDAS_SYS_ARFLAGS_REGVAL,
		ARFlagsData,
		sizeof(ARFlagsData),
		&cbUsed);

	if (!fSuccess) {
		return FALSE;
	}

	for (DWORD i = 0; i < cbUsed / sizeof(NDAS_AUTOREG_SCOPE); ++i) {
		m_scopes.push_back(
			CNdasAutoRegScope(ARFlagsData + i * sizeof(NDAS_AUTOREG_SCOPE)));
	}

	return TRUE;
}

BOOL
CNdasAutoRegScopeData::SaveToSystem()
{
	BYTE ARFlagsData[1024] = {0};

	DWORD cbUsed = std::for_each(
		m_scopes.begin(), 
		m_scopes.end(), 
		CNdasAutoRegScopeDataSerializer(sizeof(ARFlagsData), ARFlagsData));

	BOOL fSuccess = ::NdasSysSetConfigValue(
		NDAS_SYS_SERVICE_REGKEY,
		NDAS_SYS_ARFLAGS_REGVAL,
		ARFlagsData,
		cbUsed);

	if (!fSuccess) {
		return FALSE;
	}

	return TRUE;
}

BOOL
CNdasAutoRegScopeData::LoadFromFile(LPCTSTR lpFileName)
{
	//
	// Set MD5 hash to the system key
	//
	m_scopes.clear();

	BYTE ARFlagsData[1024] = {0};
	DWORD cbUsed = 0;

	AutoFileHandle hFile = ::CreateFile(
		lpFileName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (INVALID_HANDLE_VALUE == (HANDLE) hFile) {
		return FALSE;
	}

	DWORD cbRead;
	BOOL fSuccess = ::ReadFile(hFile, ARFlagsData, 1024, &cbRead, NULL);
	if (!fSuccess) {
		return FALSE;
	}

	for (DWORD i = 0; i < cbUsed / sizeof(NDAS_AUTOREG_SCOPE); ++i) {
		m_scopes.push_back(
			CNdasAutoRegScope(ARFlagsData + i * sizeof(NDAS_AUTOREG_SCOPE)));
	}

	return TRUE;
}

struct InAutoRegScope {

	CONST NDAS_DEVICE_ID& m_deviceID;

	InAutoRegScope(CONST NDAS_DEVICE_ID& id) : m_deviceID(id) {}

	bool operator()(NDAS_AUTOREG_SCOPE& scope) 
	{
		return 
			(::memcmp(m_deviceID.Node, scope.rBegin, sizeof(m_deviceID.Node)) >= 0) &&
			(::memcmp(m_deviceID.Node, scope.rEnd, sizeof(m_deviceID.Node)) <= 0);
	}
};

ACCESS_MASK 
CNdasAutoRegScopeData::GetAutoRegAccess(CONST NDAS_DEVICE_ID& deviceID)
{
	std::vector<NDAS_AUTOREG_SCOPE>::iterator fitr = 
		std::find_if(
			m_scopes.begin(), 
			m_scopes.end(), 
			InAutoRegScope(deviceID));

	if (m_scopes.end() == fitr) {
		return 0; // NO_ACCESS
	}

	return fitr->grantedAccess;
}
