////////////////////////////////////////////////////////////////////////////
//
// classes that represent device 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NDASDEVICE_H_
#define _NDASDEVICE_H_

//namespace ximeta {
#include <list>
#include <sstream>
#include <boost/shared_ptr.hpp>

#include "ndasuser.h"
#include "ndasstringid.h"

class CDeviceInfo;
typedef boost::shared_ptr<CDeviceInfo> CDeviceInfoPtr;
typedef std::list<CDeviceInfoPtr> CDeviceInfoList;

class CDeviceInfo
{
protected:
	WTL::CString	m_strName;		// Human-readable name of the device
	WTL::CString	m_strID;
	NDAS_DEVICE_ID	m_deviceID;		// ID of the device(MAC address)
	ACCESS_MASK		m_fAccessMask;

public:
	CDeviceInfo( LPCTSTR szName, NDAS_DEVICE_ID deviceID, LPCTSTR szID)
	{
		m_strName = szName;
		m_deviceID = deviceID;
		m_strID = szID;
		m_fAccessMask = GENERIC_WRITE | GENERIC_READ;
	}

	CDeviceInfo( NDASUSER_DEVICE_ENUM_ENTRY *pEntry )
	{
		m_strName	= pEntry->szDeviceName;
		m_strID		= pEntry->szDeviceStringId;
		m_fAccessMask = pEntry->GrantedAccess;
		::ConvertStringIdToRealId( m_strID, &m_deviceID);
	}

	WTL::CString	GetName()	{ return m_strName; }
	NDAS_DEVICE_ID *GetDeviceID() { return &m_deviceID; }
	WTL::CString	GetStringDeviceID() { return m_strID; }
	ACCESS_MASK		GetAccessMask() { return m_fAccessMask; }
};


class CDeviceInfoFactory
{
protected:
	// For singleton
	static CDeviceInfoFactory *m_instance;
	CDeviceInfoFactory();
public:
	//
	// Method for singleton
	//
	static	CDeviceInfoFactory *GetInstance();
	virtual CDeviceInfoList Create() = 0;
};

//
// This class read device list from service.
//
class CServiceDeviceInfoFactory : public CDeviceInfoFactory
{
protected:
	CDeviceInfoList m_listDevice;
	BOOL EnumEntry(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry);
	static BOOL CALLBACK EnumCallBack(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, LPARAM lParam);
public:
	virtual CDeviceInfoList Create();
};

//
// This class reads device list from local file
// (For debugging)
//
class CLocalDeviceInfoFactory : public CDeviceInfoFactory
{
protected:
	// Convert string(xx:xx:xx:xx:xx:xx) to ID
	void Convert(LPCSTR szID, NDAS_DEVICE_ID *pID);
public:
	virtual CDeviceInfoList Create();
};

//}

#endif // _NDASDEVICE_H_
