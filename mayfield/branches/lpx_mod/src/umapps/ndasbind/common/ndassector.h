////////////////////////////////////////////////////////////////////////////
//
// Interface of CDiskSector class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASSECTOR_H_
#define _NDASSECTOR_H_

#include <list>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "ndasctype.h"
#include "ndassession.h"

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
typedef std::vector<CUnitDiskObjectPtr> CUnitDiskObjectVector;

class CDiskLocation;
typedef boost::shared_ptr<CDiskLocation> CDiskLocationPtr;
typedef std::vector<CDiskLocationPtr> CDiskLocationVector;

#define SECTOR_MBR_COUNT		(128)	// Number of sectors to be cleaned when unbinded

///////////////////////////////////////////////////////////////////////////////
// CDiskSector class
//	- This class abstracts sector(s) on the disk.
///////////////////////////////////////////////////////////////////////////////
class CDiskSector
{
protected:
	_int8   m_data[NDAS_BLOCK_SIZE];
	_int8  *m_pdata;
public:
	CDiskSector() { 
		::ZeroMemory( m_data, sizeof(m_data) );
		m_pdata = m_data; 
	}
	// For accessing 'm_data'
	virtual _int8 *GetData() { return m_pdata; }
	// Returns starting address of the sector
	virtual _int64 GetLocation() = 0;
	virtual _int16 GetCount() = 0;

	// Accept visitor class
	// Sectors that require more sophisticated way of reading/writing
	// should override this methods.
	virtual void WriteAccept(CSession *pSession)
	{
		pSession->Write( GetLocation(), GetCount(), GetData() );
	}
	virtual void ReadAccept(CSession *pSession)
	{
		pSession->Read( GetLocation(), GetCount(), GetData() );
	}
};

///////////////////////////////////////////////////////////////////////////////
// CDiskMultiSector
//	- Used for block of sectors which holds more than one sector
///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
// CDataSector
//	- This class can be used as buffer 
//	  to read block of sectors from the disk 
//	  or to write to the disk.
///////////////////////////////////////////////////////////////////////////////
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


///////////////////////////////////////////////////////////////////////////////
// CDiskInfoSector
//	- Base class for classes which wraps NDAS_DIB
///////////////////////////////////////////////////////////////////////////////
// FIXME : This constant should be defined in a common header
#include "ndasdib.h"
#define MAX_BITS_IN_BITMAP		(1024*1024*8)
#define MIN_SECTORS_PER_BIT		(128)

class CDiskInfoSector : public CDiskMultiSector
{
protected:
	NDAS_UNIT_DEVICE_INFO m_UnitDeviceInfo;
public:
	CDiskInfoSector(const PNDAS_UNIT_DEVICE_INFO pUnitDeviceInfo)
	{
		if ( pUnitDeviceInfo != NULL )
			::CopyMemory(&m_UnitDeviceInfo, pUnitDeviceInfo, sizeof(NDAS_UNIT_DEVICE_INFO) );
		else
			::ZeroMemory(&m_UnitDeviceInfo, sizeof(NDAS_UNIT_DEVICE_INFO) );
	}
	_int64 GetTotalSectorCount() const;
	WTL::CString GetModelName() const;
	WTL::CString GetSerialNo() const;

	// Check whether the signature of the block is valid
	virtual BOOL IsValidSignature() const = 0;
	// Check whether the block data is valid
	virtual BOOL IsValid() const = 0;
	virtual BOOL IsBlockV2() const		= 0;
	virtual BOOL IsBound() const		= 0;
	virtual BOOL IsBoundAndNotSingleMirrored() const	= 0;
	virtual BOOL IsMirrored() const		= 0;
	virtual BOOL IsMaster() const		= 0;
	virtual BOOL IsPeerDirty() const		= 0;
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const		= 0;	
	virtual UINT32 GetNDASMediaType() const = 0;

	virtual void SetDirty(BOOL bDirty)  = 0;
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const = 0;
	virtual CDiskLocationPtr GetPeerLocation() const = 0;
	virtual _int64 GetUserSectorCount() const = 0;
	virtual UINT GetPosInBind() const = 0;
	virtual UINT GetDiskCountInBind() const = 0;
	// Clear bind information
	virtual void UnBind(CUnitDiskObjectPtr disk) = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Template class to provide more convenient way of
// accessing m_data field.
///////////////////////////////////////////////////////////////////////////////
template<typename T>
class CTypedDiskInfoSector : public CDiskInfoSector
{
public:
	CTypedDiskInfoSector(const PNDAS_UNIT_DEVICE_INFO pUnitDeviceInfo)
		: CDiskInfoSector(pUnitDeviceInfo)
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
};

///////////////////////////////////////////////////////////////////////////////
// CDIBSector
//	- class that abstracts NDAS_DIB
///////////////////////////////////////////////////////////////////////////////
class CDIBV2Sector;

class CDIBSector : public CTypedDiskInfoSector<NDAS_DIB>
{
public:
	friend class CDIBV2Sector;
	CDIBSector(const PNDAS_UNIT_DEVICE_INFO pUnitDeviceInfo);
	virtual _int64 GetLocation() { return NDAS_BLOCK_LOCATION_DIB_V1;	}
	virtual _int16 GetCount() {	return 1; }
	virtual BOOL IsValidSignature() const;
	virtual BOOL IsValid() const;
	virtual BOOL IsBlockV2() const { return FALSE; }
	virtual BOOL IsBound() const;
	virtual BOOL IsBoundAndNotSingleMirrored() const;
	virtual BOOL IsMirrored() const;
	virtual BOOL IsMaster() const;
	virtual UINT32 GetNDASMediaType() const;
	virtual BOOL IsPeerDirty() const { return FALSE; }
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const { return FALSE; }
	virtual void SetDirty(BOOL /* bDirty */) { return; }
	virtual _int64 GetUserSectorCount() const;
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const;
	virtual CDiskLocationPtr GetPeerLocation() const;
	virtual UINT GetPosInBind() const;
	virtual UINT GetDiskCountInBind() const;
	virtual void UnBind(CUnitDiskObjectPtr disk);
};

///////////////////////////////////////////////////////////////////////////////
// CDIBSector2
//	- class that abstracts NDAS_DIB_V2
///////////////////////////////////////////////////////////////////////////////
class CDIBV2Sector : public CTypedDiskInfoSector<NDAS_DIB_V2>
{
protected:
	//
	// m_dataExpanded contains NDAS_DIB_V2 and trailing sectors.
	// NDAS_DIB appears first in m_dataExpanded
	// while it is stored last in the disk.
	//
	boost::shared_ptr<_int8>	m_dataExpanded;
	_int16						m_nCount;
	// Calculate number of sectors required to store DIBV2
	static UINT	CalcRequiredSectorCount(NDAS_DIB_V2 *pDIB)
	{
		return (GET_TRAIL_SECTOR_COUNT_V2(pDIB->nDiskCount) + 1);
	}
public:
	CDIBV2Sector(const PNDAS_UNIT_DEVICE_INFO pUnitDeviceInfo);
	CDIBV2Sector(const CDIBSector *pDIBSector);

	virtual BOOL IsBlockV2() const { return TRUE; }
	virtual BOOL IsValid() const;
	virtual BOOL IsValidSignature() const;
	virtual _int64 GetLocation() 
	{ 
		return NDAS_BLOCK_LOCATION_DIB_V2 - m_nCount + 1;
	}
	virtual _int16 GetCount() { return m_nCount; }
	virtual void ReadAccept(CSession *pSession);
	virtual void WriteAccept(CSession *pSession);

	virtual BOOL IsBound() const;
	virtual BOOL IsBoundAndNotSingleMirrored() const;
	virtual BOOL IsMirrored() const;
	virtual BOOL IsMaster() const;
	virtual BOOL IsPeerDirty() const;
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const;
	virtual UINT32 GetNDASMediaType() const;
	virtual void SetDirty(BOOL bDirty);
	virtual _int64 GetUserSectorCount() const;
	virtual _int32 GetSectorsPerBit() const;
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const;
	virtual CDiskLocationPtr GetPeerLocation() const;
	virtual UINT GetPosInBind() const;
	virtual UINT GetDiskCountInBind() const;
	//
	// Fill binding information of the block
	//
	// @param bindDisks		[in] vector of disks to bind
	// @param nIndex		[in] index of the disk that owns this block
	// @param nBindType		[in] iMediaType of the disk 
	//							see NDAS_DIB_V2 for more information.
	//
	void Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindtype);
	virtual void UnBind(CUnitDiskObjectPtr disk);
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
	//
	// Initialize block information as a single disk
	//
	void Initialize(CUnitDiskObjectPtr disk);
};

///////////////////////////////////////////////////////////////////////////////
// CBitmapSector
//	- class that abstracts bitmap area	
///////////////////////////////////////////////////////////////////////////////
class CBitmapSector : public CDiskMultiSector
{
public:
	CBitmapSector();
	virtual _int64 GetLocation();

	//
	// Merge other bitmap's marked area into the bitmap.
	// Two disk's bitmap size must be the same
	//
	void Merge(CBitmapSector *sec);
};

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////
//
// Calculate the number of sectors used for user-area
// 
// @param nTotalSectorCount	[in] Total number of sectors in the disk.
// @return number of sectors used for user
// 
_int64 CalcUserSectorCount(_int64 nTotalSectorCount, _int32 nSectorsPerBit);

//
// Calculate the number of sectors assigned to a bit in the bitmap
//
// @param nUserSectorCount	[in] Number of sectors used for user-area.
// @return number of sectors per bit
//
UINT   CalcSectorsPerBit(_int64 nTotalSectorCount);


#endif // _NDASSECTOR_H_