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
//#include "landisk2.h"	
#include "ndas/ndasdib.h"
#include "ndasinfohandler.h"
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
	CLanDiskLocation(const unsigned _int8 *pbAddress, unsigned _int8 nSlotNumber)
	{
		::ZeroMemory( 
			static_cast<PUNIT_DISK_LOCATION>(this), 
			sizeof(UNIT_DISK_LOCATION)
			);
		::CopyMemory( this->MACAddr, pbAddress, sizeof(this->MACAddr) );
		this->UnitNumber = nSlotNumber;
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
	virtual BOOL Equal(const CDiskLocationPtr ref) const;
};


class CLanUnitDiskObject : 
	public CUnitDiskObject
{
protected:
	CDeviceInfoPtr	m_deviceInfo;
	CLanSession		m_session;
public:
	CLanUnitDiskObject(
		CDeviceInfoPtr deviceInfo,
		unsigned _int8 nSlotNumber, 
		CUnitDiskInfoHandler *pHandler
		);
	virtual WTL::CString GetStringDeviceID() const;
	virtual ACCESS_MASK GetAccessMask() const;

	virtual void Open(BOOL bWrite = FALSE);
	virtual void CommitDiskInfo(BOOL bSaveToDisk = TRUE);
	virtual void Close();
	virtual BOOL CanAccessExclusive(BOOL bAllowRead = FALSE);
	virtual CSession *GetSession();
	virtual DWORD GetSlotNo() const;

	virtual void Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType, BOOL bInit = TRUE);
	virtual void MarkAllBitmap(BOOL bMarkDirty);
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this);
};


#endif // _NDASLANIMPL_H_