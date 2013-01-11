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

#include "ndasctype.h"	// For NDAS_DEVICE_ID structure
#include "SocketLpx.h"
#include "lanscsiop.h"
#include "landisk.h"	// TODO : Why is this header file marked as obsolete?(Ask cslee) 
#include "ndasinterface.h"
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


class CUnitDiskInfoHandler
{
protected:
public:
	virtual BOOL IsHDD() const			= 0;
	virtual BOOL IsBound() const		= 0;
	virtual BOOL IsAggregated() const	= 0;
	virtual BOOL IsMirrored() const		= 0;
	virtual BOOL IsMaster() const		= 0;	// Whether the disk is master disk in the bind
	virtual BOOL IsDirty() const		= 0;
	// Get the total number of sectors in disk
	virtual _int64 GetTotalSectorCount() const = 0;
	// Get number of sectors for user space
	virtual _int64 GetUserSectorCount() const = 0;

	//
	// Get the location information of the disks bound with
	//
	virtual CDiskLocationVector GetBoundDiskLocations() const = 0;

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
	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType) = 0;
	//
	// Clear binding information
	//
	virtual void UnBind() = 0;
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
	// Write information to the disk/read information from the disk
	//
	virtual void CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk = TRUE) = 0;
};

class CDefaultDiskInfoHandler
	: public CUnitDiskInfoHandler
{
public:
	virtual BOOL IsHDD() const { return FALSE; }
	virtual BOOL IsBound() const{ return FALSE; }
	virtual BOOL IsAggregated() const{ return FALSE; }
	virtual BOOL IsMirrored() const{ return FALSE; }
	virtual BOOL IsMaster() const{ return FALSE; }
	virtual BOOL IsDirty() const{ return FALSE; }
	virtual _int64 GetUserSectorCount() const { return 0; }
	virtual _int64 GetTotalSectorCount() const { return 0; }

	virtual CDiskLocationVector GetBoundDiskLocations() const
	{
		ATLASSERT(FALSE);
		return CDiskLocationVector();
	}
	virtual CDiskLocationPtr GetPeerLocation() const 
	{ 
		ATLASSERT(FALSE);
		return CDiskLocationPtr();
	}
	virtual UINT GetPosInBind() const
	{
		ATLASSERT(FALSE);
		return 0;
	}
	virtual UINT GetDiskCountInBind() const
	{
		ATLASSERT(FALSE);
		return 0;
	}

	virtual void Bind(CDiskObjectVector /*bindDisks*/, UINT /*nIndex*/, int /*nBindType*/) { ATLASSERT(FALSE); }
	virtual void UnBind() { ATLASSERT(FALSE); }
	virtual void Rebind(CUnitDiskObjectPtr /*newDisk*/, UINT /*nIndex*/) { ATLASSERT(FALSE); }
	virtual void Mirror(CUnitDiskObjectPtr /*sourceDisk*/){ ATLASSERT(FALSE); }
	virtual void SetDirty(BOOL /*bDirty*/){ ATLASSERT(FALSE); }
	virtual void CommitDiskInfo(CSession * /*pSession*/, BOOL /*bSaveToDisk = TRUE*/) { ATLASSERT(FALSE); }
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
	virtual BOOL IsAggregated() const { return m_pDiskSector->IsAggregated(); }
	virtual BOOL IsMirrored() const { return m_pDiskSector->IsMirrored(); }
	virtual BOOL IsMaster() const { return m_pDiskSector->IsMaster(); }
	virtual BOOL IsDirty() const { return m_pDiskSector->IsDirty(); }

	virtual CDiskLocationVector GetBoundDiskLocations() const
	{
		return m_pDiskSector->GetBoundDiskLocations();
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
	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType);
	virtual void UnBind();
	virtual void Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex);
	virtual void Mirror(CUnitDiskObjectPtr sourceDisk);
	virtual void SetDirty(BOOL bDirty);
	virtual void CommitDiskInfo(CSession *pSession, BOOL bSaveToDisk = TRUE);
};

//}

#endif // _NDASINFOHANDLER_H_