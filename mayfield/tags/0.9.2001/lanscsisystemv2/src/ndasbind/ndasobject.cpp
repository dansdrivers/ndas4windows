////////////////////////////////////////////////////////////////////////////
//
// classes that represent device & disk
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasobject.h"
#include <map>

UINT CDiskObject::m_idLastAssigned = 0;

///////////////////////////////////////////////////////////////////////////////
// CDiskObjectComposite
///////////////////////////////////////////////////////////////////////////////
void CDiskObjectComposite::AddChild(CDiskObjectPtr _this, CDiskObjectPtr child)
{
	ATLASSERT( this == _this.get() );
	child->SetParent(_this);
	CDiskObjectList::push_back(child);

}

void CDiskObjectComposite::DeleteChild(CDiskObjectPtr child)
{
	CDiskObjectList::remove( child );
}

void CDiskObjectComposite::Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v)
{
	ATLASSERT( this == _this.get() );
	v->Visit( _this );
	if ( v->VisitInto() )
	{
		v->IncDepth();
		iterator itr;
		for ( itr = begin(); itr != end(); ++itr )
		{
			(*itr)->Accept(*itr, v);
		}
		v->DecDepth();
	}
}

BOOL CDiskObjectComposite::IsBroken() const
{
	BOOL bBroken = FALSE;

	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		bBroken = bBroken || (*itr)->IsBroken();
	}
	return bBroken;
}

UINT CDiskObjectComposite::GetDiskCount() const
{
	UINT nCount = 0;

	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		nCount += (*itr)->GetDiskCount();
	}
	return nCount;
}

UINT CDiskObjectComposite::GetDiskCountInBind() const
{
	ATLASSERT( CDiskObjectList::size() > 0 );
	return front()->GetDiskCountInBind();
}

_int64 CDiskObjectComposite::GetUserSectorCount() const
{
	_int64 nSize = 0;

	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		nSize += (*itr)->GetUserSectorCount();
	}
	return nSize;
}

ACCESS_MASK CDiskObjectComposite::GetAccessMask() const
{
	ACCESS_MASK fMask = ~static_cast<ACCESS_MASK>(0x00L);

	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		fMask &= (*itr)->GetAccessMask();
	}
	return fMask;
}

CDiskObjectList CDiskObjectComposite::UnBind(CDiskObjectPtr _this)
{
	ATLASSERT( this == _this.get() );
	CDiskObjectList unboundDisks;
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		CDiskObjectList subUnboundDisks = (*itr)->UnBind(*itr);
		unboundDisks.splice(  
			unboundDisks.end(), 
			subUnboundDisks
			);
	}
	return unboundDisks;
}

void CDiskObjectComposite::Rebind(CDiskObjectPtr newDisk, UINT nIndex)
{
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		if ( !(*itr)->IsUsable() )
			continue;	// Skip unusable disk
		(*itr)->Rebind(newDisk, nIndex);
	}
}

void CDiskObjectComposite::OpenExclusive()
{
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		(*itr)->OpenExclusive();
	}
}
void CDiskObjectComposite::CommitDiskInfo(BOOL bSaveToDisk)
{
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		(*itr)->CommitDiskInfo(bSaveToDisk);
	}
}
void CDiskObjectComposite::Close()
{
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		(*itr)->Close();
	}
}
CDiskObjectComposite::CanAccessExclusive()
{
	BOOL bCanAccessExclusive = TRUE;
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		bCanAccessExclusive = bCanAccessExclusive && (*itr)->CanAccessExclusive();
	}
	return bCanAccessExclusive;
}

void CDiskObjectComposite::Bind(CDiskObjectVector bindDisks, UINT, BOOL)
{
	// Composite disks cannot be bound with other disks.
	// NOTE : Maybe mirroring disks can be bound.
	//		  But not for this version.
	ATLASSERT( FALSE );
}

///////////////////////////////////////////////////////////////////////////////
// CRootDiskObject
///////////////////////////////////////////////////////////////////////////////
void CRootDiskObject::Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v)
{
	ATLASSERT( this == _this.get() );
	v->IncDepth();
	iterator itr;
	for ( itr = begin(); itr != end(); itr++ )
	{
		(*itr)->Accept(*itr, v);
	}
	v->DecDepth();
}

WTL::CString CRootDiskObject::GetTitle() const 
{
	return WTL::CString( _T("root") );
}

CDiskObjectList CRootDiskObject::UnBind(CDiskObjectPtr _this)
{
	ATLASSERT(FALSE);	// Root disk cannot be unbound
	return CDiskObjectList();
}
void CRootDiskObject::Rebind(CDiskObjectPtr /*newDisk*/, UINT /*nIndex*/)
{
	ATLASSERT(FALSE);	// Root disk cannot be rebound
}
///////////////////////////////////////////////////////////////////////////////
// CAggrDiskObject
///////////////////////////////////////////////////////////////////////////////
WTL::CString CAggrDiskObject::GetTitle() const
{
	return front()->GetTitle();
}

BOOL CAggrDiskObject::IsUsable() const
{
	if ( GetDiskCount() < GetDiskCountInBind() )
		return FALSE;
	// Aggregation requires all disks in bind be usable
	BOOL bUsable = TRUE;
	CDiskObjectList::const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )	// Since CAggrDiskObject is subclass of std::list, 
												// we can use begin, end directly
	{
		if ( !(*itr)->IsUsable() )
		{
			bUsable = FALSE;
			break;
		}
	}
	return bUsable;
}

///////////////////////////////////////////////////////////////////////////////
// CMirDiskObject
///////////////////////////////////////////////////////////////////////////////
WTL::CString CMirDiskObject::GetTitle() const
{
	return front()->GetTitle();
}

BOOL CMirDiskObject::IsUsable() const
{
	BOOL bUsable = FALSE;
	// If at least one disk is usable, we can still use mirror.
	CDiskObjectList::const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		if ( (*itr)->IsUsable() )
		{
			bUsable = TRUE;
			break;
		}
	}
	return bUsable;
}

BOOL CMirDiskObject::IsBroken() const
{
	BOOL bBroken = FALSE;

	CDiskObjectList::const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )	
	{
		if ( !(*itr)->IsUsable() )
		{
			bBroken = TRUE;
			break;
		}
	}
	return bBroken;
}

BOOL CMirDiskObject::IsDirty() const
{
	// If any of the disks is dirty, the mirror is dirty
	return GetDirtyDiskCount() > 0;
}

UINT CMirDiskObject::GetDirtyDiskCount() const
{
	UINT nCount = 0;
	CDiskObjectList::const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		CUnitDiskObjectPtr obj = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);
		ATLASSERT( obj.get() != NULL );
		CUnitDiskInfoHandlerPtr handler = obj->GetInfoHandler();
		if ( handler->IsDirty() )
		{
			nCount++;
		}
	}
	return nCount;
}

UINT CMirDiskObject::GetDirtyDiskIndex() const
{
	ATLASSERT( GetDirtyDiskCount() == 1 );
	CUnitDiskObjectPtr obj = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(CDiskObjectList::front());
	ATLASSERT( obj.get() != NULL );
	CUnitDiskInfoHandlerPtr pHandler = obj->GetInfoHandler(); 
	
	if ( pHandler->IsDirty() )
		return 0;
	else 
		return 1;
}
///////////////////////////////////////////////////////////////////////////////
// CUnitDiskObject
///////////////////////////////////////////////////////////////////////////////
CUnitDiskObject::CUnitDiskObject(WTL::CString strName, 
                                 CDiskLocation *pLocation, 
                                 CUnitDiskInfoHandler *pHandler)
: m_strName(strName), m_pLocation(pLocation), m_pHandler(pHandler)
{
}

BOOL CUnitDiskObject::IsHDD() const
{
	return m_pHandler->IsHDD();
}
CDiskLocation *CUnitDiskObject::GetLocation() const 
{
	return m_pLocation.get();
}
CUnitDiskInfoHandlerPtr CUnitDiskObject::GetInfoHandler()
{ 
	return m_pHandler;
}

void CUnitDiskObject::Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v)
{
	ATLASSERT( this == _this.get() );
	v->Visit(_this);
}

UINT CUnitDiskObject::GetDiskCount() const 
{ 
	return 1;
}

UINT CUnitDiskObject::GetDiskCountInBind() const
{
	return m_pHandler->GetDiskCountInBind();
}

_int64 CUnitDiskObject::GetUserSectorCount() const
{
	return m_pHandler->GetUserSectorCount();
}

WTL::CString CUnitDiskObject::GetTitle() const
{
	return m_strName;
}

BOOL CUnitDiskObject::IsUsable() const 
{
	return TRUE;	// TODO : More elaborated method would be needed.
}

BOOL CUnitDiskObject::IsUnitDisk() const
{
	return TRUE;
}

BOOL CUnitDiskObject::IsBroken() const
{
	return FALSE;
}

void CUnitDiskObject::Bind(CDiskObjectVector bindDisks, UINT nIndex, int nBindType)
{
	m_pHandler->Bind(bindDisks, nIndex, nBindType);
}

CDiskObjectList CUnitDiskObject::UnBind(CDiskObjectPtr _this)
{
	ATLASSERT( this == _this.get() );
	CDiskObjectList listUnbound;

	m_pHandler->UnBind();

	listUnbound.push_back(_this);
	return listUnbound;
}

void CUnitDiskObject::Rebind(CDiskObjectPtr newDisk, UINT nIndex)
{
	CUnitDiskObjectPtr newUnitDisk = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(newDisk);
	ATLASSERT( newUnitDisk.get() );
	m_pHandler->Rebind(newUnitDisk, nIndex);
}

void CUnitDiskObject::Mirror(CDiskObjectPtr source)
{
	CUnitDiskObjectPtr unitSource = 
		boost::dynamic_pointer_cast<CUnitDiskObject>(source);
	ATLASSERT( unitSource.get() );
	m_pHandler->Mirror(unitSource);
}

void CUnitDiskObject::SetDirty(BOOL bDirty)
{
	m_pHandler->SetDirty( bDirty );
}
///////////////////////////////////////////////////////////////////////////////
// CNoExistDiskObject
///////////////////////////////////////////////////////////////////////////////
CDiskObjectList CNoExistDiskObject::UnBind(CDiskObjectPtr _this)
{
	ATLASSERT( this == _this.get() );
	
	return CDiskObjectList();
}

void CNoExistDiskObject::Bind(CDiskObjectVector, UINT, BOOL)
{
	ATLASSERT( FALSE );
	return;
}

void CNoExistDiskObject::Rebind(CDiskObjectPtr , UINT )
{
	ATLASSERT( FALSE );
	// TODO : Think about when this happens.
	return;
}

void CNoExistDiskObject::MarkAllBitmap(BOOL /*bMarkDirty*/)
{
	ATLASSERT( FALSE );
	return;
}

void CNoExistDiskObject::Mirror(CDiskObjectPtr /*pSource*/)
{
	ATLASSERT( FALSE );
}

void CNoExistDiskObject::OpenExclusive()
{
	ATLASSERT( FALSE );
}

void CNoExistDiskObject::CommitDiskInfo(BOOL /*bSaveToDisk*/)
{
	ATLASSERT( FALSE );
}
void CNoExistDiskObject::Close()
{
	ATLASSERT( FALSE );
}
BOOL CNoExistDiskObject::CanAccessExclusive()
{
	ATLASSERT( FALSE );
	return FALSE;
}


///////////////////////////////////////////////////////////////////////////////
// Helper function for CFindIfVisitor
///////////////////////////////////////////////////////////////////////////////
BOOL IsUnitDisk(CDiskObjectPtr obj)
{
	CUnitDiskObject *pUnitDisk 
		= dynamic_cast<CUnitDiskObject*>(obj.get());
	return (pUnitDisk != NULL && pUnitDisk->IsUsable() );
}

///////////////////////////////////////////////////////////////////////////////
// CDiskObjectBuilder
///////////////////////////////////////////////////////////////////////////////
CDiskObjectBuilder* CDiskObjectBuilder::m_instance = NULL;

CDiskObjectBuilder::CDiskObjectBuilder()
{
	if ( m_instance != NULL)
	{
		ATLASSERT(FALSE);
		return;
	}

	m_instance = this;
}
CDiskObjectBuilder *CDiskObjectBuilder::GetInstance()
{
	if ( m_instance == NULL )
	{
		// Error: you should have one global instance
		ATLASSERT(FALSE);
		return NULL;
	}
	return m_instance;
}

///////////////////////////////////////////////////////////////////////////////
// CDeviceObjectBuilder
///////////////////////////////////////////////////////////////////////////////
CDeviceObjectBuilder* CDeviceObjectBuilder::m_instance = NULL;
CDeviceObjectBuilder::CDeviceObjectBuilder()
{
	if ( m_instance != NULL)
	{
		ATLASSERT(FALSE);
		return;
	}

	m_instance = this;
}
CDeviceObjectBuilder *CDeviceObjectBuilder::GetInstance()
{
	if ( m_instance == NULL )
	{
		// Error: you should have one global instance
		ATLASSERT(FALSE);
		return NULL;
	}
	return m_instance;
}


