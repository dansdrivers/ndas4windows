////////////////////////////////////////////////////////////////////////////
//
// classes that represent disk
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NDASOBJECT_H_
#define _NDASOBJECT_H_

#include <list>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "ndasutil.h"
#include "ndasdevice.h"
#include "ndasinfohandler.h"

//
// Following classes are used to represent various type of disks.
// Here is the hierarchy of the classes
// 
//                    +-------------+
//   +--------------->| CDiskObject |
//   |                +-------------+
//   |                       ^
//   |                       |
//   |            +----------+-------------+
//   |            |                        |
//   |   +----------------------+  +-----------------+
//   +-<>| CDiskObjectComposite |  | CUnitDiskObject |
//       +----------------------+  +-----------------+
//
//  Use of builder
//	- From users or local system , only address and access key of NDAS device
//	  can be obtained. Therefore a builder interface, CDiskObjectBuilder, is
//	  defined to build CDiskObject objects from the information we can get
//	  from users or local system.
//
//  Note
//  - boost library's(http://www.boost.org) shared_ptr is used to manage 
//	  disk objects since it can be shared all over the program and 
//	  manual management of these object will be quite painful.

//namespace ximeta {

///////////////////////////////////////////////////////////////////////////////
// classes that represents Disk
///////////////////////////////////////////////////////////////////////////////
class CDiskObject;
class CDiskObjectComposite;
class CAggrDiskObject;
class CMirDiskObject;
class CUnitDiskObject;
class CDiskObjectVisitor;
typedef boost::shared_ptr<CDiskObject> CDiskObjectPtr;
typedef boost::shared_ptr<CDiskObjectComposite> CDiskObjectCompositePtr;
typedef boost::shared_ptr<CAggrDiskObject> CAggrDiskObjectPtr;
typedef boost::shared_ptr<CMirDiskObject> CMirDiskObjectPtr;
typedef boost::shared_ptr<CUnitDiskObject> CUnitDiskObjectPtr;

typedef std::list<CDiskObjectPtr> CDiskObjectList;
typedef std::vector<CDiskObjectPtr> CDiskObjectVector;
typedef std::list<CUnitDiskObjectPtr> CUnitDiskObjectList;
typedef std::vector<CUnitDiskObjectPtr> CUnitDiskObjectVector;

class CDiskObject
{
protected:
	static UINT		m_idLastAssigned;
	UINT			m_idUnique;
	CDiskObjectPtr	m_pParent;
public:
	CDiskObject()
		: m_pParent(CDiskObjectPtr())
	{
		m_idUnique = m_idLastAssigned++;
	}

	///////////////////////////////////////////////////////////////////////////////
	// Methods for management of objects
	///////////////////////////////////////////////////////////////////////////////

	// Set parent node
	void SetParent(CDiskObjectPtr parent){ m_pParent = parent; }
	const CDiskObjectPtr GetParent() { return m_pParent; }
	//
	// Get ID that uniquely identifies an object.
	//
	UINT GetUniqueID() const { return m_idUnique; }

	// Accept visitor class
	// we use '_this' parameter for shared_ptr
	// For more information, 
	//  see http://www.boost.org/libs/smart_ptr/sp_techniques.html 
	virtual void Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v) = 0;

	///////////////////////////////////////////////////////////////////////////////
	// Methods for retrieving object information 
	///////////////////////////////////////////////////////////////////////////////
	
	//
	// Get the number of disks it has.
	//
	virtual UINT GetDiskCount() const = 0;
	
	//
	// Get the number of disks in bind
	//
	virtual UINT GetDiskCountInBind() const = 0;

	//
	// Get the size of disks usable
	//
	virtual _int64 GetUserSectorCount() const = 0;

	// Return name of the object
	virtual WTL::CString GetTitle() const = 0;

	// Return device id of the object
	virtual WTL::CString GetStringDeviceID() const = 0;

	// Returns true if the disk object is usable.
	// 'Usable' means as below
	//	- For aggregated disks : All disks in binds exist and are accessable
	//  - For mirrored disks : at least one of the two disks exists and is accessable
	//  - For a single disks : the disk exists and is accessable
	virtual BOOL IsUsable() const = 0;
	// Returns true if the disk object is not a composite object
	virtual BOOL IsUnitDisk() const = 0;
	// Returns true if one or more disks are missing.
	virtual BOOL IsBroken() const = 0;
	// Returns true if the disk object is an instance of CRootDiskObject
	virtual BOOL IsRoot() const { return FALSE; }
	virtual ACCESS_MASK GetAccessMask() const = 0;

	///////////////////////////////////////////////////////////////////////////////
	// Methods that change disk status
	///////////////////////////////////////////////////////////////////////////////

	//
	// Open a connection to a disk for write
	// 
	virtual void OpenExclusive() = 0;
	//
	// Write disk information to the physical disk 
	// or Read disk information from the physical disk.
	//
	// @param bSaveToDisk	[IN] If true, write information to the disk.
	//							if false, read information from the disk.
	//
	virtual void CommitDiskInfo(BOOL bSaveToDisk = TRUE) = 0;
	//
	// Close the connection
	//
	virtual void Close() = 0;
	//
	// Check whether the disk is accessed by other program/computer
	//
	virtual BOOL CanAccessExclusive() = 0;

	//
	// Bind disks
	// 
	// @param bindDisks		[in] List of disks to bind.
	// @param nIndex		[in] index of the disk that owns this block
	// @param nBindType		[in] 0 : Aggregation only, 1 : RAID1, 2 : RAID5
	//							(NOTE : This type can be changed )
	//
	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType) = 0;
	//
	// Unbind disks
	// 
	// @return	List of disks unbound
	//
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this) = 0;
	//
	// Rebind disk
	// bind a new disk into the position indicated by the index.
	//
	virtual void Rebind(CDiskObjectPtr newDisk, UINT nIndex) = 0;

};

//
// Visitor class used to enumerate objects
// - Because of the hierarchical structure of objects
//   this template class has one method called VisitInto().
//	 if it returns true, all the objects in the subtree will be visited
//	 otherwise only the object on the highest level will be visited
//
class CDiskObjectVisitor
{
public:
	// Called by visitee
	virtual void Visit(CDiskObjectPtr o) = 0;
	virtual BOOL VisitInto() = 0;
	// Called by visitee when the depth increase/decreases
	virtual void IncDepth() = 0;
	virtual void DecDepth() = 0;
};

template<BOOL bVisitInto>
class CDiskObjectVisitorT : public CDiskObjectVisitor
{
public:
	virtual BOOL VisitInto() { return bVisitInto; }
	virtual void IncDepth() { }
	virtual void DecDepth() { }
};

//
// Composite class of CDiskObject
//
class CDiskObjectComposite : 
						public CDiskObject, 
						public CDiskObjectList
{
protected:
	void push_back(const CDiskObjectPtr& _Val);	// Use Addchild instead
public:
	///////////////////////////////////////////////////////////////////////////////
	// Methods for management of objects
	///////////////////////////////////////////////////////////////////////////////
	void AddChild(CDiskObjectPtr _this, CDiskObjectPtr child);
	void DeleteChild(CDiskObjectPtr child);
	virtual void Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v);

	///////////////////////////////////////////////////////////////////////////////
	// Methods for retrieving object information 
	///////////////////////////////////////////////////////////////////////////////
	virtual WTL::CString GetStringDeviceID() const { return _T(""); }
	virtual BOOL IsUnitDisk() const { return FALSE; }
	virtual BOOL IsBroken() const;
	virtual UINT GetDiskCount() const;
	virtual UINT GetDiskCountInBind() const;
	virtual ACCESS_MASK GetAccessMask() const;
	virtual _int64 GetUserSectorCount() const;

	///////////////////////////////////////////////////////////////////////////////
	// Methods that change disk status
	///////////////////////////////////////////////////////////////////////////////
	virtual void OpenExclusive();
	virtual void CommitDiskInfo(BOOL bSaveToDisk = TRUE);
	virtual void Close();
	virtual BOOL CanAccessExclusive();

	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType);
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this);
	virtual void Rebind(CDiskObjectPtr newDisk, UINT nIndex);
};

class CRootDiskObject : public CDiskObjectComposite
{
public:
	virtual void Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v);

	///////////////////////////////////////////////////////////////////////////////
	// Methods for retrieving object information 
	///////////////////////////////////////////////////////////////////////////////
	virtual WTL::CString GetTitle() const; 
	virtual BOOL IsUsable() const { return TRUE; }
	virtual BOOL IsRoot() const { return TRUE; }

	///////////////////////////////////////////////////////////////////////////////
	// Methods that change disk status
	///////////////////////////////////////////////////////////////////////////////
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this);
	virtual void Rebind(CDiskObjectPtr newDisk, UINT nIndex);
};

//
// Abstract class that provides interface for the location of a disk
//
class CDiskLocation;
typedef boost::shared_ptr<CDiskLocation> CDiskLocationPtr;
typedef std::list<CDiskLocationPtr> CDiskLocationList;
class CDiskLocation
{
public:
	// NOTE : This method can be changed,
	//		 if other type of network is added.(Like USB)
	virtual const UNIT_DISK_LOCATION *GetUnitDiskLocation() const = 0;

	// Returns TRUE if 'ref' points to the same location
	virtual BOOL Equal(const CDiskLocation *pRef) const = 0;
};

//
// Class for aggregated disks
//
class CAggrDiskObject 
	: public CDiskObjectComposite
{
public:
	virtual WTL::CString GetTitle() const;
	virtual BOOL IsUsable() const;
};

//
// Class for mirroring/mirrored disks
// (Precisely there's no difference between them)
//
class CMirDiskObject 
	: public CDiskObjectComposite
{
protected:
public:
	virtual WTL::CString GetTitle() const;
	virtual BOOL IsUsable() const;
	virtual BOOL IsBroken() const;

	///////////////////////////////////////////////////////////////////////////////
	// Methods that are unique for CMirDiskObject
	///////////////////////////////////////////////////////////////////////////////
	BOOL IsDirty() const;
	UINT GetDirtyDiskCount() const;
	UINT GetDirtyDiskIndex() const;

};

class CUnitDiskObject : 
	public CDiskObject
{
protected:
	CUnitDiskInfoHandlerPtr m_pHandler;
	CDiskLocationPtr		m_pLocation;
	WTL::CString			m_strName;
public:
	CUnitDiskObject(WTL::CString strName, 
					CDiskLocation *pLocation, 
					CUnitDiskInfoHandler *pHandler);

	virtual void Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v);
	virtual WTL::CString GetTitle() const ;

	virtual BOOL IsUsable() const;
	virtual BOOL IsUnitDisk() const;
	virtual BOOL IsBroken() const;
	virtual UINT GetDiskCount() const;
	virtual UINT GetDiskCountInBind() const;
	virtual _int64 GetUserSectorCount() const;
	///////////////////////////////////////////////////////////////////////////////
	// Methods that change disk status
	///////////////////////////////////////////////////////////////////////////////
	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType);
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this);
	virtual void Rebind(CDiskObjectPtr newDisk, UINT nIndex);

	///////////////////////////////////////////////////////////////////////////////
	// Methods that are unique for CUnitDiskObject
	///////////////////////////////////////////////////////////////////////////////
	virtual BOOL IsHDD() const;
	CDiskLocation *GetLocation() const;
	CUnitDiskInfoHandlerPtr GetInfoHandler();
	//
	// Mark all the bitmap in this disk.
	//
	// @param bMark	[In] If TRUE, then the bitmap will be marked as dirty.
	//					otherwise it will be marked as clean.
	// 
	virtual void MarkAllBitmap(BOOL bMarkDirty = TRUE) = 0;
	//
	// Set the disk's information as a mirror disk to the source disk.
	// This method is used when we reestablish mirror.
	//
	virtual void Mirror(CDiskObjectPtr pSource);
	//
	// Set disk as dirty or clean
	// 
	virtual void SetDirty(BOOL bDirty = TRUE);
};

//
// Class for disks that does not exist
//
class CNoExistDiskObject : public CUnitDiskObject
{

public:
	class CNoExistLocation : public CDiskLocation 
	{
	public:
		virtual BOOL Equal(const CDiskLocation * /*pRef*/) const
		{
			return FALSE;
		}
		virtual const UNIT_DISK_LOCATION *GetUnitDiskLocation() const
		{
			return NULL;
		}
	};
	CNoExistDiskObject()
		: CUnitDiskObject( 
			_T("Not exist"), 
			new CNoExistLocation(), 
			new CDefaultDiskInfoHandler() )
		  
	{
		// TODO : Reconsider about the name of missing mirror disk.
	}
	virtual ACCESS_MASK GetAccessMask() const { return static_cast<ACCESS_MASK>(0x00L); }
	virtual WTL::CString GetStringDeviceID() const { return _T(""); }
	virtual BOOL IsUsable() const { return FALSE; }
	virtual BOOL IsUnitDisk() const { return FALSE; }
	virtual BOOL IsBroken() const { return TRUE; }

	virtual void Bind(CDiskObjectVector bindDisks, UINT nIndex, BOOL bUseMirror);
	virtual CDiskObjectList UnBind(CDiskObjectPtr _this);
	virtual void Rebind(CDiskObjectPtr newDisk, UINT nIndex);
	virtual void OpenExclusive();
	virtual void CommitDiskInfo(BOOL bSaveToDisk = TRUE);
	virtual void Close();
	virtual BOOL CanAccessExclusive();
	
	virtual void MarkAllBitmap(BOOL bMarkDirty);
	virtual void Mirror(CDiskObjectPtr pSource);
};

//
// class that builds disk object structure from device list
//
class CDiskObjectBuilder
{
protected:
	// For singleton
	static CDiskObjectBuilder *m_instance;
	CDiskObjectBuilder();
public:
	static CDiskObjectBuilder *GetInstance();
	virtual CDiskObjectPtr BuildFromDeviceInfo(const CDeviceInfoList listDevice) const = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Visitor classes
///////////////////////////////////////////////////////////////////////////////
template<BOOL bVisitInto>
class CFindIfVisitor : public CDiskObjectVisitorT<bVisitInto>
{
protected:
	CDiskObjectList	m_listFound;
	typedef BOOL (*Predicate)(CDiskObjectPtr);
	Predicate		m_pred;
public:
	virtual void Visit(CDiskObjectPtr o)
	{
		if ( m_pred(o) )
			m_listFound.push_back( o );
	}
	template<class Predicate>
	CDiskObjectList FindIf(CDiskObjectPtr root, Predicate pred)
	{
		m_listFound.clear();
		m_pred = pred;
		root->Accept(root, this);
		return m_listFound;
	}
	CDiskObjectList GetResult() { return m_listFound; }
};

// Helper function for CFindIfVisitor
BOOL IsUnitDisk(CDiskObjectPtr obj);

///////////////////////////////////////////////////////////////////////////////
// classes that represents Device
///////////////////////////////////////////////////////////////////////////////
class CDeviceObject;
typedef boost::shared_ptr<CDeviceObject> CDeviceObjectPtr;
typedef std::list<CDeviceObjectPtr> CDeviceObjectList;

class CDeviceObject 
{
protected:
	CDeviceInfoPtr m_info;		// stores address, name of the device
	CUnitDiskObjectList m_listDisk;	// List of disks in the device
public:
	CDeviceObject(CDeviceInfoPtr info) { m_info = info; }
	CUnitDiskObjectList GetDiskObjectList() const { return m_listDisk; }
};

class CDeviceObjectBuilder
{
protected:
	// For signleton
	static CDeviceObjectBuilder *m_instance;
	CDeviceObjectBuilder();
public:
	static CDeviceObjectBuilder *GetInstance();
	virtual CDeviceObjectList Build(const CDeviceInfoList listDeviceInfo) const = 0;
};

//}

#endif // _NDASOBJECT_H_