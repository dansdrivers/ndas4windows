////////////////////////////////////////////////////////////////////////////
//
// Interface class between ndas object classes and NDAS disks
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NDASINTERFACE_H_
#define _NDASINTERFACE_H_

#include <list>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "ndasctype.h"
#include "SocketLpx.h"
#include "lanscsiop.h"
#include "landisk.h"	// TODO : Why is this header file marked as obsolete?(Ask cslee) 

//namespace ximeta
//{
class CDiskSector;
typedef boost::shared_ptr<CDiskSector> CDiskSectorPtr;
class CDiskInfoSector;
typedef boost::shared_ptr<CDiskInfoSector> CDiskInfoSectorPtr;

class CDiskObject;
typedef boost::shared_ptr<CDiskObject> CDiskObjectPtr;
typedef std::list<CDiskObjectPtr> CDiskObjectList;
typedef std::vector<CDiskObjectPtr> CDiskObjectVector;

class CUnitDiskObject;
typedef boost::shared_ptr<CUnitDiskObject> CUnitDiskObjectPtr;

class CDiskLocation;
typedef boost::shared_ptr<CDiskLocation> CDiskLocationPtr;
typedef std::vector<CDiskLocationPtr> CDiskLocationVector;

class CSession
{
protected:
	BOOL			m_bWrite;		// TRUE if the connection is for write
	BOOL			m_bLoggedIn;
	LANSCSI_PATH	m_path;
	UINT			m_nUnitNumber;	// UNIT number of the disk.
									// This variable will be set in Login method
	//
	// You should really use these function in the sub-library.
	// (But I can't find any now.)
	//
	void _GetInterfaceList(
		LPSOCKET_ADDRESS_LIST socketAddressList, 
		DWORD socketAddressListLength 
		);
	unsigned _int64 _GetHWPassword(const BYTE *pbAddress);

public:
	CSession()
		: m_bLoggedIn(FALSE), m_bWrite(FALSE)
	{
		::ZeroMemory( &m_path, sizeof(LANSCSI_PATH) );
		m_path.connsock = INVALID_SOCKET;
		m_path.iSessionPhase = LOGOUT_PHASE;
	}
	~CSession();
	//
	// Return TRUE if there's session for the specified operation
	//
	BOOL IsLoggedIn(BOOL bAsWrite);
	// Connect to a device
	void Connect(const BYTE *pbNode);
	// Login to a disk
	void Login(UINT nUnitNumber = 0, BOOL bWrite = FALSE);
	void Logout();
	void Disconnect();
	void Write(_int64 iLocation, _int16 nSecCount, _int8 *pbData);	// Write data to a disk
	void Read(_int64 iLocation, _int16 nSecCount, _int8 *pbData);	// Read data from a disk
	void GetTargetData(const UNIT_DISK_LOCATION *pLocation, PTARGET_DATA pTargetData);
};

class CDiskSector
{
protected:
	_int8   m_data[BLOCK_SIZE];
	_int8  *m_pdata;
public:
	CDiskSector() { 
		::ZeroMemory( m_data, sizeof(m_data) );
		m_pdata = m_data; 
	}
	// from accessing 'm_data'
	virtual _int8 *GetData() { return m_pdata; }
	virtual _int64 GetLocation() = 0;
	virtual _int16 GetCount() = 0;
	// Accept visitor class
	virtual void WriteAccept(CSession *pSession)
	{
		pSession->Write( GetLocation(), GetCount(), GetData() );
	}
	virtual void ReadAccept(CSession *pSession)
	{
		pSession->Read( GetLocation(), GetCount(), GetData() );
	}
};

class CDiskMultiSector : public CDiskSector
{
protected:
	boost::shared_ptr<_int8>	m_dataExpanded;
	_int16						m_nCount;
public:
	CDiskMultiSector();
	CDiskMultiSector(UINT nSectorCount);
	void Resize(UINT nSectorCount);
	virtual _int16 GetCount() { return m_nCount; }
};

class CDataSector : public CDiskMultiSector
{
protected:
	_int64 m_nLocation;
public:
	CDataSector() 
		: CDiskMultiSector()
	{
	}
	CDataSector(UINT nSectorCount) 
		: CDiskMultiSector(nSectorCount)
	{
	}
	virtual _int64 GetLocation(){ return m_nLocation; }
	void SetLocation(_int64 nLocation) { m_nLocation = nLocation; }
};
// FIXME : This constant should be defined in a common header
#define DIB_LOCATION	(-1)
#define DIBV2_LOCATION	(-2)

class CDiskInfoSector : public CDiskMultiSector
{
protected:
	TARGET_DATA m_targetData;
public:
	CDiskInfoSector(const TARGET_DATA *pTargetData)
	{
		::CopyMemory(&m_targetData, pTargetData, sizeof(TARGET_DATA) );
	}
	_int64 GetTotalSectorCount() const
	{
		return m_targetData.SectorCount;
	}

	virtual BOOL IsBlockV2() const		= 0;
	virtual BOOL IsBound() const		= 0;
	virtual BOOL IsAggregated() const	= 0;
	virtual BOOL IsMirrored() const		= 0;
	virtual BOOL IsMaster() const		= 0;
	virtual BOOL IsDirty() const		= 0;
	virtual void SetDirty(BOOL bDirty)  = 0;
	virtual CDiskLocationVector GetBoundDiskLocations() const = 0;
	virtual CDiskLocationPtr GetPeerLocation() const = 0;
	virtual _int64 GetUserSectorCount() const = 0;
	virtual UINT GetPosInBind() const = 0;
	virtual UINT GetDiskCountInBind() const = 0;
	// Clear bind information
	virtual void UnBind() = 0;
};

template<typename T>
class CTypedDiskInfoSector : public CDiskInfoSector
{
public:
	CTypedDiskInfoSector(const TARGET_DATA *pTargetData)
		: CDiskInfoSector(pTargetData)
	{
	}
	T* operator->()
	{
		return reinterpret_cast<T*>(m_pdata);
	}
	const T* operator->() const
	{
		return reinterpret_cast<const T*>(m_pdata);
	}
	T* get()
	{
		return reinterpret_cast<T*>(m_pdata);
	}
	const T* get() const
	{
		return reinterpret_cast<const T*>(m_pdata);
	}
	// Check whether the block data is valid
	virtual BOOL IsValid() const = 0;
};


class CDIBV2Sector;

class CDIBSector : public CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>
{
public:
	friend class CDIBV2Sector;
	CDIBSector(const TARGET_DATA *pTargetData);
	virtual _int64 GetLocation() { return DIB_LOCATION;	}
	virtual _int16 GetCount() {	return 1; }
	virtual BOOL IsValid() const;
	virtual BOOL IsBlockV2() const { return FALSE; }
	virtual BOOL IsBound() const;
	virtual BOOL IsAggregated() const;
	virtual BOOL IsMirrored() const;
	virtual BOOL IsMaster() const;
	virtual BOOL IsDirty() const { return FALSE; }
	virtual void SetDirty(BOOL /* bDirty */) { return; }
	virtual _int64 GetUserSectorCount() const;
	virtual CDiskLocationVector GetBoundDiskLocations() const;
	virtual CDiskLocationPtr GetPeerLocation() const;
	virtual UINT GetPosInBind() const;
	virtual UINT GetDiskCountInBind() const;
	virtual void UnBind();
};

class CDIBV2Sector : public CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>
{
protected:
	//
	// m_dataExpanded contains DISK_INFORMATION_BLOCK_V2 and trailing sectors.
	// DISK_INFORMATION_BLOCK appears first in m_dataExpanded
	// while it is stored last in the disk.
	//
	boost::shared_ptr<_int8>	m_dataExpanded;
	_int16						m_nCount;
	// Calcualte number of sectors required to store DIBV2
	static UINT	CalcRequiredSectorCount(DISK_INFORMATION_BLOCK_V2 *pDIB)
	{
		return (GET_TRAIL_SECTOR_COUNT_V2(pDIB->nDiskCount) + 1);
	}
public:
	CDIBV2Sector(const TARGET_DATA *pTargetData);
	CDIBV2Sector(const CDIBSector *pDIBSector);

	virtual BOOL IsBlockV2() const { return TRUE; }
	virtual BOOL IsValid() const;
	virtual _int64 GetLocation() 
	{ 
		return DIBV2_LOCATION - m_nCount + 1;
	}
	virtual _int8 *GetData() { return m_dataExpanded.get(); }
	virtual _int16 GetCount() { return m_nCount; }
	virtual void ReadAccept(CSession *pSession);
	virtual void WriteAccept(CSession *pSession);

	virtual BOOL IsBound() const;
	virtual BOOL IsAggregated() const;
	virtual BOOL IsMirrored() const;
	virtual BOOL IsMaster() const;
	virtual BOOL IsDirty() const;
	virtual void SetDirty(BOOL bDirty);
	virtual _int64 GetUserSectorCount() const;
	virtual _int32 GetSectorsPerBit() const;
	virtual CDiskLocationVector GetBoundDiskLocations() const;
	virtual CDiskLocationPtr GetPeerLocation() const;
	virtual UINT GetPosInBind() const;
	virtual UINT GetDiskCountInBind() const;
	//
	// Fill binding information of the block
	//
	// @param bindDisks		[in] vector of disks to bind
	// @param nIndex		[in] index of the disk that owns this block
	// @param nBindType		[in] 0 : Aggregation only, 1 : RAID1, 2 : RAID5
	//							(NOTE : This type can be changed )
	//
	void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindtype);
	virtual void UnBind();
	//
	// Replace binding information at 'nIndex' by the 'newDisk'
	//
	// @param newDisk	[in] disk to bind with
	// @param nIndex	[in] index of the position to bind the new disk into
	//
	void Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex);
	//
	// Create mirror information based on the information of the source disk
	// NOTE : Newly bound disk's location must be updated to the information
	//		  block of the source disk in advance since here we just copy
	//		  the UnitDisks field.
	//
	void Mirror(CDIBV2Sector *pSourceSector);
};

class CBitmapSector : public CDiskMultiSector
{
public:
	CBitmapSector();
	virtual _int64 GetLocation();
};

//}

#endif // _NDASINTERFACE_H_