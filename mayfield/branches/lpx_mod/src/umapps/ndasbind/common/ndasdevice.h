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
#include "ndasctype.h"

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
	DWORD			m_SlotNo;

	BOOL			m_bServiceInfo; // TRUE if the information is retrieved from service
	NDAS_DEVICE_STATUS m_DeviceStatus;

public:
	CDeviceInfo( LPCTSTR szName, LPCTSTR szID, NDAS_DEVICE_ID deviceID );
	CDeviceInfo( NDASUSER_DEVICE_ENUM_ENTRY *pEntry );

	WTL::CString	GetName()	{ return m_strName; }
	NDAS_DEVICE_ID *GetDeviceID() { return &m_deviceID; }
	WTL::CString	GetStringDeviceID() { return m_strID; }
	ACCESS_MASK		GetAccessMask() { return m_fAccessMask; }
	DWORD			GetSlotNo() { return m_SlotNo; }

	void			SetServiceInfo(BOOL bServiceInfo) {m_bServiceInfo = bServiceInfo;}
	BOOL			GetServiceInfo() {return m_bServiceInfo;}

	void			SetDeviceStatus(NDAS_DEVICE_STATUS DeviceStatus) {m_DeviceStatus = DeviceStatus;}
	NDAS_DEVICE_STATUS			GetDeviceStatus() {return m_DeviceStatus;}
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
	static BOOL CALLBACK EnumCallBack(PNDASUSER_DEVICE_ENUM_ENTRY lpEnumEntry, LPVOID lParam);

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
