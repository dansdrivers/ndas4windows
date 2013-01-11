////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSession class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "ndassession.h"
#include "ndasexception.h"
#include "hash.h"

int CLanSession::m_nGlobalRecentNIC = -1;
CLanSession::CLanSession(const BYTE *pbNode, unsigned _int8 nSlotNumber)
: m_bLoggedIn(FALSE), m_bWrite(FALSE)
{
	m_hNDAS = NULL;

	if ( pbNode == NULL )
	{
		::ZeroMemory( m_abNode, sizeof(m_abNode) );
	}
	else
	{
		::CopyMemory( m_abNode, pbNode, sizeof(m_abNode) );
	}
	m_nSlotNumber = nSlotNumber;
	m_nRecentNIC = -1;
}

CLanSession::~CLanSession()
{
	if(m_hNDAS)
		NdasCommDisconnect(m_hNDAS);


	m_hNDAS = NULL;
}
BOOL CLanSession::IsLoggedIn(BOOL bAsWrite)
{
	if(bAsWrite)
	{
		return ((m_hNDAS) ? 1 : 0)&& m_bWrite;
	}
	else
	{
		return ((m_hNDAS) ? 1 : 0);
	}
}

#define EXCEPTION_IF_NOT_CONNECTED(HANDLE_NDAS) \
{	\
	if(!HANDLE_NDAS)	\
{	\
	WTL::CString strError;	\
	strError.Format( _T("Connection not established"));	\
	NDAS_THROW_EXCEPTION_STR(	\
	CNetworkException,	\
	CNetworkException::ERROR_NETWORK_FAIL_NOT_CONNECTED,	\
	strError	\
	);	\
}	\
}

void CLanSession::Write(_int64 iLocation, _int16 nSecCount, _int8 *pbData)
{
	BOOL bRet;

	EXCEPTION_IF_NOT_CONNECTED(m_hNDAS);

	bRet = NdasCommBlockDeviceWriteSafeBuffer(m_hNDAS, iLocation, nSecCount, (PBYTE)pbData);
	if(!bRet)
	{
		// AING_TO_DO : format message here
	}

	return;
}
void CLanSession::Read(_int64 iLocation, _int16 nSecCount, _int8 *pbData)
{
	BOOL bRet;

	EXCEPTION_IF_NOT_CONNECTED(m_hNDAS);

	bRet = NdasCommBlockDeviceRead(m_hNDAS, iLocation, nSecCount, (PBYTE)pbData);
	if(!bRet)
	{
		// AING_TO_DO : format message here
	}

	return;
//		if(iErrorCode != 0 || LANSCSI_RESPONSE_SUCCESS != iResponse)
//		{
//			// ERROR : Fail to read
//			WTL::CString strError;
//			strError.Format( _T("Node:%s, ErrorCode:%d, Response:%d"), 
//				AddrToString(m_path.address.Node), iErrorCode, iResponse );
//			NDAS_THROW_EXCEPTION_STR(
//				CNetworkException, 
//				CNetworkException::ERROR_FAIL_TO_READ,
//				strError
//				);
//			break;
//		}
//	}
}

void CLanSession::GetTargetData(PNDASCOMM_UNIT_DEVICE_INFO pUnitDeviceInfo)
{
	BOOL bRet;

	EXCEPTION_IF_NOT_CONNECTED(m_hNDAS);

	bRet = NdasCommGetUnitDeviceInfo(m_hNDAS, pUnitDeviceInfo);
	if(!bRet)
	{
		// AING_TO_DO : format message here
	}

	return;
}

UINT CLanSession::GetDiskCount()
{
	BOOL bRet;

	EXCEPTION_IF_NOT_CONNECTED(m_hNDAS);

	NDASCOMM_UNIT_DEVICE_STAT UnitDeviceStat;
	NDASCOMM_CONNECTION_INFO ConnectionInfo;

	ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ConnectionInfo.bWriteAccess = m_bWrite;
	ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ConnectionInfo.UnitNo = m_nSlotNumber;
	ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
	ConnectionInfo.ui64OEMCode = NULL;
	ConnectionInfo.bSupervisor = FALSE;
	CopyMemory(ConnectionInfo.AddressLPX, m_abNode, sizeof(ConnectionInfo.AddressLPX));
	bRet = NdasCommGetUnitDeviceStat(&ConnectionInfo, &UnitDeviceStat, 0, NULL);
	if(!bRet)
	{
		// AING_TO_DO : format message here
	}

	return UnitDeviceStat.iNRTargets;
}

ConnectedHosts CLanSession::GetHostCount()
{
	BOOL bRet;

	EXCEPTION_IF_NOT_CONNECTED(m_hNDAS);

	NDASCOMM_UNIT_DEVICE_STAT UnitDeviceStat;
	NDASCOMM_CONNECTION_INFO ConnectionInfo;

	ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ConnectionInfo.bWriteAccess = m_bWrite;
	ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;
	ConnectionInfo.UnitNo = m_nSlotNumber;
	ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
	ConnectionInfo.ui64OEMCode = NULL;
	ConnectionInfo.bSupervisor = FALSE;
	CopyMemory(ConnectionInfo.AddressLPX, m_abNode, sizeof(ConnectionInfo.AddressLPX));

	bRet = NdasCommGetUnitDeviceStat(&ConnectionInfo, &UnitDeviceStat, 0, NULL);
	if(!bRet)
	{
		// AING_TO_DO : format message here
	}

	UnitDeviceStat.NRRWHost = (0xFFFFFFFF == UnitDeviceStat.NRRWHost) ? 0 : UnitDeviceStat.NRRWHost;
	UnitDeviceStat.NRROHost = (0xFFFFFFFF == UnitDeviceStat.NRROHost) ? 0 : UnitDeviceStat.NRROHost;

	ConnectedHosts hosts;
	hosts.nRWHosts = UnitDeviceStat.NRRWHost;
	hosts.nROHosts = UnitDeviceStat.NRROHost;

	return hosts;
}

void CLanSession::Connect(BOOL bWrite)
{
	NDASCOMM_CONNECTION_INFO ConnectionInfo;

	ZeroMemory(&ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ConnectionInfo.bSupervisor = FALSE;
	ConnectionInfo.bWriteAccess = bWrite;
	ConnectionInfo.address_type = NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX;
	ConnectionInfo.login_type = NDASCOMM_LOGIN_TYPE_NORMAL;
	ConnectionInfo.UnitNo = m_nSlotNumber;
	ConnectionInfo.protocol = NDASCOMM_TRANSPORT_LPX;
	ConnectionInfo.ui64OEMCode = NULL;
	ConnectionInfo.bSupervisor = FALSE;
	CopyMemory(ConnectionInfo.AddressLPX, m_abNode, sizeof(ConnectionInfo.AddressLPX));

	m_hNDAS = NdasCommConnect(&ConnectionInfo, 0, NULL);
	if(!m_hNDAS)
	{
		// AING_TO_DO : format message here
	}

	m_bLoggedIn = TRUE;
	m_bWrite = bWrite;

	return;
}

void CLanSession::Disconnect()
{
	if(m_hNDAS)
		NdasCommDisconnect(m_hNDAS);
	m_hNDAS = NULL;
}


void CLanSession::SetAddress(const BYTE *pbNode)
{
	::CopyMemory( m_abNode, pbNode, sizeof(m_abNode) );
}
void CLanSession::SetSlotNumber(unsigned _int8 nSlotNumber)
{
	ATLASSERT( !m_bLoggedIn );
	m_nSlotNumber = nSlotNumber;
}