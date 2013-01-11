////////////////////////////////////////////////////////////////////////////
//
// Delegate classes that provides interface for handling information 
// of a disk
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NDASINFOHANDLER_H_
#define _NDASINFOHANDLER_H_

#include <list>
#include <boost/shared_ptr.hpp>

#include "ndassession.h"
#include "ndassector.h"
//namespace ximeta
//{
class CDiskObject;
class CUnitDiskObject;
typedef boost::shared_ptr<CDiskObject> CDiskObjectPtr;
typedef boost::shared_ptr<CUnitDiskObject> CUnitDiskObjectPtr;
typedef std::list<CDiskObjectPtr> CDiskObjectList;

class CDiskLocation;
typedef boost::shared_ptr<CDiskLocation> CDiskLocationPtr;
typedef std::vector<CDiskLocationPtr> CDiskLocationVector;
/*
class CDiskInfoHandler;
typedef boost::shared_ptr<CDiskInfoHandler> CDiskInfoHandlerPtr;
*/
class CUnitDiskInfoHandler;
typedef boost::shared_ptr<CUnitDiskInfoHandler> CUnitDiskInfoHandlerPtr;

class CEmptyDiskInfoHandler;
typedef boost::shared_ptr<CEmptyDiskInfoHandler> CEmptyDiskInfoHandlerPtr;

class CUnitDiskInfoHandler
{
protected:
public:
	virtual BOOL IsHDD() const			= 0;
	virtual BOOL IsBound() const		= 0;
	virtual BOOL IsBoundAndNotSingleMirrored() const	= 0;
	virtual BOOL IsMirrored() const		= 0;
	virtual BOOL IsMaster() const		= 0;	// Whether the disk is master disk in the bind
	virtual BOOL IsPeerDirty() const		= 0;
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const = 0;
	virtual BOOL GetLastWrittenSectorsInfo(PLAST_WRITTEN_SECTORS pLWS) const = 0;
	virtual UINT32 GetNDASMediaType() const = 0;
	//
	// Return TRUE if the disk's information block contains valid(recognizable) 
	// information
	//
	virtual BOOL HasValidInfo() const	= 0;	
	// Get the total number of sectors in disk
	virtual _int64 GetTotalSectorCount() const = 0;
	// Get number of sectors for user space
	virtual _int64 GetUserSectorCount() const = 0;

	//
	// Get the location information of the disks bound with
	//
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const = 0;

	// 
	// Get the location information of the peer disk in the mirroring
	//
	virtual CDiskLocationPtr GetPeerLocation() const = 0;
	//
	// Get the position in bind
	//
	virtual UINT GetPosInBind() const = 0;
	//
	// Get the number of disks in bind
	//
	virtual UINT GetDiskCountInBind() const = 0;
	//
	// Set binding information
	//
	virtual void Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType) = 0;
	//
	// Clear binding information
	//
	virtual void UnBind(CUnitDiskObjectPtr disk) = 0;
	//
	// bind a new disk into the position indicated by the index.
	//
	virtual void Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex) = 0;
	//
	// Set the disk's information as a mirror disk to the source disk.
	//
	virtual void Mirror(CUnitDiskObjectPtr sourceDisk) = 0;
	//
	// Set disk as dirty or clean
	// 
	virtual void SetDirty(BOOL bDirty = TRUE) = 0;
	//
	// Initialize the disk's information block
	//
	virtual void Initialize(CUnitDiskObjectPtr disk) = 0;
	
	//
	// Write information to the disk/read information from the disk
	//
	virtual void CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk = TRUE) = 0;

	//
	// Return TRUE if the disk has NDAS_DIB_V2
	//
	virtual BOOL HasBlockV2() const		= 0;
};

class CEmptyDiskInfoHandler : public CUnitDiskInfoHandler
{
protected:
public:
	virtual BOOL IsHDD() const { return TRUE; }
	virtual BOOL IsBound() const { return FALSE; }
	virtual BOOL IsBoundAndNotSingleMirrored() const { return FALSE; }
	virtual BOOL IsMirrored() const { return FALSE; }
	virtual BOOL IsMaster() const { return FALSE; }
	virtual BOOL IsPeerDirty() const { return FALSE; }
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const {return FALSE; }
	virtual BOOL GetLastWrittenSectorsInfo(PLAST_WRITTEN_SECTORS pLWS) const {return FALSE; }
	virtual UINT32 GetNDASMediaType() const { return NMT_INVALID; }
	//
	// Return TRUE if the disk's information block contains valid(recognizable) 
	// information
	//
	virtual BOOL HasValidInfo() const { return FALSE; }
	// Get the total number of sectors in disk
	virtual _int64 GetTotalSectorCount() const { return 0; }
	// Get number of sectors for user space
	virtual _int64 GetUserSectorCount() const { return 0; }

	//
	// Get the location information of the disks bound with
	//
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const { CDiskLocationVector vtLocation; return vtLocation;	}

	// 
	// Get the location information of the peer disk in the mirroring
	//
	virtual CDiskLocationPtr GetPeerLocation() const { CDiskLocationPtr pLocation; return pLocation; }
	//
	// Get the position in bind
	//
	virtual UINT GetPosInBind() const {	return 0;}
	//
	// Get the number of disks in bind
	//
	virtual UINT GetDiskCountInBind() const { return 0;}
	//
	// Set binding information
	//
	virtual void Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType) {}
	//
	// Clear binding information
	//
	virtual void UnBind(CUnitDiskObjectPtr disk) {}
	//
	// bind a new disk into the position indicated by the index.
	//
	virtual void Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex) {}
	//
	// Set the disk's information as a mirror disk to the source disk.
	//
	virtual void Mirror(CUnitDiskObjectPtr sourceDisk) {}
	//
	// Set disk as dirty or clean
	// 
	virtual void SetDirty(BOOL bDirty = TRUE) {}
	//
	// Initialize the disk's information block
	//
	virtual void Initialize(CUnitDiskObjectPtr disk) {}

	//
	// Write information to the disk/read information from the disk
	//
	virtual void CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk = TRUE) {}

	//
	// Return TRUE if the disk has NDAS_DIB_V2
	//
	virtual BOOL HasBlockV2() const { return FALSE; }

	virtual _int32 GetSectorsPerBit() const {return 0;}

	//
	// Methods unique for CHDDDiskInfoHandler
	//
	WTL::CString GetModelName() const {return WTL::CString(_T(""));}
	WTL::CString GetSerialNo() const {return WTL::CString(_T(""));}
};

class CHDDDiskInfoHandler : public CUnitDiskInfoHandler
{
protected:
	CDiskInfoSectorPtr	m_pDiskSector;
public:
	CHDDDiskInfoHandler(CDiskInfoSectorPtr sector)
		: m_pDiskSector(sector)
	{
	}

	virtual BOOL IsHDD() const { return TRUE; }
	virtual BOOL IsBound() const { return m_pDiskSector->IsBound(); }
	virtual BOOL IsBoundAndNotSingleMirrored() const { return m_pDiskSector->IsBoundAndNotSingleMirrored(); }
	virtual BOOL IsMirrored() const { return m_pDiskSector->IsMirrored(); }
	virtual BOOL IsMaster() const { return m_pDiskSector->IsMaster(); }
	virtual BOOL IsPeerDirty() const { return m_pDiskSector->IsPeerDirty(); }
	virtual BOOL GetLastWrittenSectorInfo(PLAST_WRITTEN_SECTOR pLWS) const {return m_pDiskSector->GetLastWrittenSectorInfo(pLWS); }
	virtual BOOL GetLastWrittenSectorsInfo(PLAST_WRITTEN_SECTORS pLWS) const {return m_pDiskSector->GetLastWrittenSectorsInfo(pLWS); }
	virtual BOOL HasValidInfo() const { return m_pDiskSector->IsValid(); }
	virtual UINT32 GetNDASMediaType() const { return m_pDiskSector->GetNDASMediaType(); }
	virtual CDiskLocationVector GetBoundDiskLocations(CDiskLocationPtr thisLocation) const
	{
		return m_pDiskSector->GetBoundDiskLocations(thisLocation);
	}
	virtual CDiskLocationPtr GetPeerLocation() const
	{
		return m_pDiskSector->GetPeerLocation();
	}
	virtual UINT GetPosInBind() const
	{
		return m_pDiskSector->GetPosInBind();
	}
	virtual UINT GetDiskCountInBind() const
	{
		return m_pDiskSector->GetDiskCountInBind();
	}

	virtual _int64 GetUserSectorCount() const
	{
		return m_pDiskSector->GetUserSectorCount();
	}
	virtual _int64 GetTotalSectorCount() const 
	{
		return m_pDiskSector->GetTotalSectorCount();
	}
	virtual _int32 GetSectorsPerBit() const;
	virtual void Bind(CUnitDiskObjectVector bindDisks, UINT nIndex, int nBindType);
	virtual void UnBind(CUnitDiskObjectPtr disk);
	virtual void Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex);
	virtual void Mirror(CUnitDiskObjectPtr sourceDisk);

	virtual void SetDirty(BOOL bDirty);
	virtual void Initialize(CUnitDiskObjectPtr disk);
	virtual void CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk = TRUE);
	virtual BOOL HasBlockV2() const;

	//
	// Methods unique for CHDDDiskInfoHandler
	//
	WTL::CString GetModelName() const;
	WTL::CString GetSerialNo() const;
};


//}

#endif // _NDASINFOHANDLER_H_