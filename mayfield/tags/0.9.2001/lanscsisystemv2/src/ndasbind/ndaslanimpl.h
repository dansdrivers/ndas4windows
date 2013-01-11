////////////////////////////////////////////////////////////////////////////
//
// Implemenation of NDAS object for LAN environment
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NDASLANIMPL_H_
#define _NDASLANIMPL_H_

#include <list>
#include <boost/shared_ptr.hpp>

#include "ndasobject.h"
#include "SocketLpx.h"
#include "lanscsiop.h"
#include "landisk.h"	// TODO : Why is this header file marked as obsolete?(Ask cslee) 
#include "ndasinfohandler.h"
#include "ndasinterface.h"
//namespace ximeta
//{
class CLanDiskLocation : 
	public CDiskLocation,
	public UNIT_DISK_LOCATION
{
public:
	CLanDiskLocation()
	{
		::ZeroMemory( 
			static_cast<PUNIT_DISK_LOCATION>(this), 
			sizeof(UNIT_DISK_LOCATION)
			);
	}
	CLanDiskLocation(const unsigned _int8 *pbAddress, unsigned _int8 nUnitNumber)
	{
		::ZeroMemory( 
			static_cast<PUNIT_DISK_LOCATION>(this), 
			sizeof(UNIT_DISK_LOCATION)
			);
		::CopyMemory( this->MACAddr, pbAddress, sizeof(this->MACAddr) );
		this->UnitNumber = nUnitNumber;
	}
	CLanDiskLocation(const UNIT_DISK_LOCATION *pUDL)
	{
		::CopyMemory(
			static_cast<PUNIT_DISK_LOCATION>(this),
			pUDL,
			sizeof(UNIT_DISK_LOCATION)
			);
	}

	virtual const UNIT_DISK_LOCATION *GetUnitDiskLocation() const
	{
		return static_cast<const UNIT_DISK_LOCATION*>(this);
	}
	virtual BOOL Equal(const CDiskLocation *pRef) const;
};


class CLanUnitDiskObject : 
	public CUnitDiskObject
{
protected:
	CDeviceInfoPtr	m_deviceInfo;
	CSession		m_session;
public:
	CLanUnitDiskObject(
		CDeviceInfoPtr deviceInfo,
		UINT nUnitNumber, 
		CUnitDiskInfoHandler *pHandler
		);
	virtual WTL::CString GetStringDeviceID() const;
	virtual ACCESS_MASK GetAccessMask() const;

	virtual void OpenExclusive();
	virtual void CommitDiskInfo(BOOL bSaveToDisk = TRUE);
	virtual void Close();
	virtual BOOL CanAccessExclusive();

	virtual void MarkAllBitmap(BOOL bMarkDirty);
};

//
// A factory class that creates disk object
//
class CLanUnitDiskObjectFactory 
{
public:
	static CUnitDiskObjectPtr Create(
							CDeviceInfoPtr deviceInfo,
							UINT nUnitNumber, 
							TARGET_DATA *pTargetData
							);
};


class CLanDeviceObject : 
	public CDeviceObject,
	public CSession
{
protected:
	
public:
	CLanDeviceObject(CDeviceInfoPtr info) 
		: CDeviceObject(info), 
		  CSession()
	{
	}
	
	void Init();
};
class CLanDeviceObjectBuilder : public CDeviceObjectBuilder
{
public:
	CDeviceObjectList Build(const CDeviceInfoList listDeviceInfo) const;
};

class CLanDiskObjectBuilder : public CDiskObjectBuilder
{
private:
	//
	// Retrieves disk information from the device 
	// and build a list of disks based on the information
	//
	CUnitDiskObjectList BuildDiskObjectList(const CDeviceInfoList listDevice) const;
public:
	CDiskObjectPtr BuildFromDeviceInfo(const CDeviceInfoList listDevice) const;
};

#endif // _NDASLANIMPL_H_