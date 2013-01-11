////////////////////////////////////////////////////////////////////////////
//
// classes that represent device 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasdevice.h"
#include "ndas/ndasid.h"
#include "ndasexception.h"
// for debugging
#include <fstream>
#include <iostream>
#include <string>

////////////////////////////////////////////////////////////////////////////
// CDeviceInfo classes
////////////////////////////////////////////////////////////////////////////
CDeviceInfo::CDeviceInfo( LPCTSTR szName, LPCTSTR szID, NDAS_DEVICE_ID deviceID )
{
	m_strName = szName;
	m_strID = szID;
	m_fAccessMask = GENERIC_WRITE | GENERIC_READ;
	::CopyMemory( &m_deviceID, &deviceID, sizeof(NDAS_DEVICE_ID) );

	m_bServiceInfo = FALSE;
	m_DeviceStatus = NDAS_DEVICE_STATUS_CONNECTED;
}

CDeviceInfo::CDeviceInfo( NDASUSER_DEVICE_ENUM_ENTRY *pEntry )
{
	m_strName	= pEntry->szDeviceName;
	m_strID		= pEntry->szDeviceStringId;
	m_fAccessMask = pEntry->GrantedAccess;
	m_SlotNo = pEntry->SlotNo;
	::NdasIdConvertStringIdToRealId( m_strID, &m_deviceID );

	m_bServiceInfo = FALSE;
	m_DeviceStatus = NDAS_DEVICE_STATUS_CONNECTED;
}

////////////////////////////////////////////////////////////////////////////
// CDeviceObjectFactory classes
////////////////////////////////////////////////////////////////////////////
CDeviceInfoFactory *CDeviceInfoFactory::m_instance = NULL;
static CServiceDeviceInfoFactory g_deviceFactory;	// Singleton object.
//static CLocalDeviceInfoFactory g_deviceFactory;

CDeviceInfoFactory::CDeviceInfoFactory()
{
	if ( m_instance != NULL )
	{
		// ERROR : CDeviceObjectFactory can have only one instance
		ATLASSERT( FALSE );
		return;
	}
	m_instance = this;
}

CDeviceInfoFactory *CDeviceInfoFactory::GetInstance()
{
	if ( m_instance == NULL )
	{
		// ERROR : there must be one instance.(Put an global variable.)
		ATLASSERT( FALSE );
		return NULL;
	}
	return m_instance;
}

BOOL 
CALLBACK 
CServiceDeviceInfoFactory::
EnumCallBack(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, LPVOID lParam)
{
	CServiceDeviceInfoFactory *pobj = 
		reinterpret_cast<CServiceDeviceInfoFactory*>(lParam);
	return pobj->EnumEntry( lpEnumEntry );
}

BOOL CServiceDeviceInfoFactory::EnumEntry(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry)
{
	CDeviceInfo *pObj = NULL;
	NDAS_DEVICE_ID deviceID;

	BOOL bSuccess = ::NdasIdConvertStringIdToRealId( 
						lpEnumEntry->szDeviceStringId,
						&deviceID);
	if ( !bSuccess )
	{
		// TODO : ERROR : Invalid string id
		return TRUE;	// Skip current device
	}
	pObj = new CDeviceInfo( lpEnumEntry );
	m_listDevice.push_back( CDeviceInfoPtr(pObj) );

	pObj->SetServiceInfo(TRUE);

	NDAS_UNITDEVICE_STATUS status;
	NDAS_UNITDEVICE_ERROR lastError;

	BOOL fSuccess = ::NdasQueryDeviceStatus(
		lpEnumEntry->SlotNo, 
		&status, 
		&lastError);

	pObj->SetDeviceStatus(status);

	return TRUE;
}
CDeviceInfoList CServiceDeviceInfoFactory::Create()
{
	m_listDevice.clear();
	if ( !NdasEnumDevices( EnumCallBack, reinterpret_cast<LPVOID>(this) ) )
	{
		// TODO : ERROR : Fail to get device list
		NDAS_THROW_EXCEPTION(CServiceException, ::GetLastError());
	}
	return m_listDevice;
}

void CLocalDeviceInfoFactory::Convert(LPCSTR szID, NDAS_DEVICE_ID *pID)
{
	// FIXME : Temporary code
	char szBuffer[256];
	strcpy(szBuffer, szID);
	int nLen = (int)::strlen(szBuffer);
	int nVal;
	int i, j, pos;
	
	::ZeroMemory( pID, sizeof(NDAS_DEVICE_ID) );
	if ( nLen != 17 )
		return;
	
	for ( i=0, pos=0 ; i < 6; i++, pos++)
	{
		nVal = 0;
		for ( j=0; j < 2; j++, pos++ )
		{
			nVal *= 16;
			if ( '0' <= szBuffer[pos] && szBuffer[pos] <= '9' )
			{
				nVal += szBuffer[pos] - '0';
			}
			else
			{
				nVal += szBuffer[pos] - 'A' + 10;
			}
		}
		pID->Node[i] = static_cast<BYTE>(nVal);
	}

}

CDeviceInfoList CLocalDeviceInfoFactory::Create()
{
	CDeviceInfoList objectList;

	std::ifstream fin("DeviceList.txt");
	std::string strName, strID, strKey;
	NDAS_DEVICE_ID deviceID;

	CDeviceInfo *pObj;
	while( !fin.eof() ) 
	{
		fin >> strName >> strKey >> strID;
		Convert(strID.c_str(), &deviceID);
#if defined(UNICODE) || defined(_UNICODE)
        WCHAR szName[256], szKey[256];
		::MultiByteToWideChar( 
			CP_ACP, 0, strName.c_str(), -1, szName, sizeof(szName) );
		
		::MultiByteToWideChar( 
			CP_ACP, 0, strKey.c_str(), -1, szKey, sizeof(szKey) );
		pObj = new CDeviceInfo( szName, szKey, deviceID );
#else
		pObj = new CDeviceInfo( strName.c_str(), strKey.c_str(), deviceID );
#endif
		objectList.push_back( CDeviceInfoPtr(pObj) );
	}
	fin.close();
	return objectList;
}

