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

BOOL IsEmptyDisk(CDiskObjectPtr child)
{
	return (dynamic_cast<CEmptyDiskObject*>(child.get()) != NULL);
}

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
		if(::IsEmptyDisk(*itr)) continue;
		
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

void CDiskObjectComposite::Open( BOOL bWrite )
{
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		(*itr)->Open( bWrite );
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

BOOL CDiskObjectComposite::CanAccessExclusive(BOOL bAllowRead)
{
	BOOL bCanAccessExclusive = TRUE;
	CDiskObjectList::iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		if(::IsEmptyDisk(*itr)) continue;

		bCanAccessExclusive = bCanAccessExclusive && (*itr)->CanAccessExclusive(bAllowRead);
	}
	return bCanAccessExclusive;
}

UINT32 CDiskObjectComposite::GetNDASMediaType() const
{
	UINT32 iMediaType = NMT_INVALID;

	// return the first NDAS Media type of unit disk which is not NMT_INVALID
	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		if(::IsEmptyDisk(*itr))
			continue;

		if(!(*itr)->IsUnitDisk())
		{
			CMirDiskObjectPtr mirDisk = 
				boost::dynamic_pointer_cast<CMirDiskObject>(*itr);

			if(mirDisk)
			{
				iMediaType = mirDisk->GetNDASMediaType();
				if(NMT_INVALID != iMediaType)
					return iMediaType;
			}
			else
			{
				return NMT_INVALID;
			}
		}

		if(0 == (*itr)->GetDiskCount()) // skip empty one
			continue;

		CUnitDiskObjectPtr unitDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);
		CUnitDiskInfoHandlerPtr unitHandler = unitDisk->GetInfoHandler();
		
		iMediaType = unitHandler->GetNDASMediaType();
		if(NMT_INVALID != iMediaType)
			return iMediaType;
	}
	return iMediaType;
}

BOOL CDiskObjectComposite::HasWriteAccess()
{
	UINT32 iMediaType = NMT_INVALID;

	// return the first NDAS Media type of unit disk which is not NMT_INVALID
	const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )
	{
		if(::IsEmptyDisk(*itr))
			return FALSE;

		if(!(*itr)->IsUnitDisk())
		{
			CMirDiskObjectPtr mirDisk = 
				boost::dynamic_pointer_cast<CMirDiskObject>(*itr);

			if(mirDisk)
			{
				if(!mirDisk->HasWriteAccess())
					return FALSE;
			}
			else
			{
				return FALSE;
			}
		}

//		if(2 != (*itr)->GetDiskCount())
//			return FALSE;

		CUnitDiskObjectPtr unitDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(*itr);

		if(!(unitDisk->GetAccessMask() & GENERIC_WRITE))
			return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// CRootDiskObject
///////////////////////////////////////////////////////////////////////////////
void CRootDiskObject::Accept(CDiskObjectPtr _this, CDiskObjectVisitor *v)
{
	ATLASSERT( this == _this.get() );
	//v->IncDepth();
	iterator itr;
	for ( itr = begin(); itr != end(); itr++ )
	{
		(*itr)->Accept(*itr, v);
	}
	//v->DecDepth();
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
void CRootDiskObject::MigrateV1()
{
	ATLASSERT(FALSE);	// Root disk cannot be migrated
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

void CAggrDiskObject::MigrateV1()
{
	//
	// To migrate, all the disks in the bound must exist
	// In version 1, only 2 disks can be bound.
	//
	ATLASSERT( GetDiskCount() == 2 );
	CUnitDiskObjectPtr firstDisk, secondDisk;
	CUnitDiskObjectVector vtDisks;
	
	firstDisk = boost::dynamic_pointer_cast<CUnitDiskObject>( front() );
	secondDisk = boost::dynamic_pointer_cast<CUnitDiskObject>( back() );
	vtDisks.push_back( firstDisk );
	vtDisks.push_back( secondDisk );

	//
	// Unbind disks & bind again with version 2 information
	// 
	firstDisk->UnBind( firstDisk );
	secondDisk->UnBind( secondDisk );

	firstDisk->Bind( vtDisks, 0, NMT_AGGREGATE );
	secondDisk->Bind( vtDisks, 1, NMT_AGGREGATE	);

}

///////////////////////////////////////////////////////////////////////////////
// CRAID0DiskObject
///////////////////////////////////////////////////////////////////////////////
WTL::CString CRAID0DiskObject::GetTitle() const
{
	return front()->GetTitle();
}

BOOL CRAID0DiskObject::IsUsable() const
{
	if ( GetDiskCount() < GetDiskCountInBind() )
		return FALSE;
	// requires all disks in bind be usable
	BOOL bUsable = TRUE;
	CDiskObjectList::const_iterator itr;
	for ( itr = begin(); itr != end(); ++itr )	// Since CRAID0DiskObject is subclass of std::list, 
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
	// If at least one disk is usable, we can still use mirror.
	if(FirstDisk()->IsUsable() || SecondDisk()->IsUsable())
		return TRUE;
	
	return FALSE;
}

BOOL CMirDiskObject::IsBroken() const
{
	if(!FirstDisk()->IsUsable() || !SecondDisk()->IsUsable())
		return TRUE;

	return FALSE;
}

BOOL CMirDiskObject::IsDirty() const
{
	// If any of the disks is dirty, the mirror is dirty
	BOOL bFirstDefected, bSecondDefected;

	BOOL bResults = GetDirtyDiskStatus(&bFirstDefected, &bSecondDefected);

	return bFirstDefected || bSecondDefected;
}

BOOL CMirDiskObject::GetDirtyDiskStatus(BOOL *pbFirstDefected, BOOL *pbSecondDefected) const
{
	BOOL bResults;
	LAST_WRITTEN_SECTOR FirstLWS, SecondLWS;

	*pbFirstDefected = FALSE;
	*pbSecondDefected = FALSE;

	if(::IsEmptyDisk(FirstDisk()) || ::IsEmptyDisk(SecondDisk()))
		return 0;

	CUnitDiskInfoHandlerPtr Firsthandler = FirstDisk()->GetInfoHandler();
	CUnitDiskInfoHandlerPtr Secondhandler = SecondDisk()->GetInfoHandler();

	bResults = Firsthandler->GetLastWrittenSectorInfo(&FirstLWS);
	if(!bResults) return FALSE;
	bResults = Secondhandler->GetLastWrittenSectorInfo(&SecondLWS);
	if(!bResults) return FALSE;

	if ( Firsthandler->IsPeerDirty() || FirstLWS.timeStamp > SecondLWS.timeStamp)
	{
		*pbSecondDefected = TRUE;
	}
	else
	{
		*pbSecondDefected = FALSE;
	}

	if ( Secondhandler->IsPeerDirty() || SecondLWS.timeStamp > SecondLWS.timeStamp)
	{
		*pbFirstDefected = TRUE;
	}
	else
	{
		*pbFirstDefected = FALSE;
	}

	return TRUE;
}

_int64 CMirDiskObject::GetUserSectorCount() const
{
	if(!::IsEmptyDisk(FirstDisk()))
	{
		return FirstDisk()->GetUserSectorCount();
	}

	if(!::IsEmptyDisk(SecondDisk()))
	{
		return SecondDisk()->GetUserSectorCount();
	}

	return 0;
}

void CMirDiskObject::MigrateV1()
{
	if(::IsEmptyDisk(FirstDisk()) || ::IsEmptyDisk(SecondDisk()))
		return;
	//
	// To migrate, all the disks in the bound must exist
	// In version 1, only 2 disks can be bound.
	//
	ATLASSERT( GetDiskCount() == 2 );
//	CUnitDiskObjectPtr firstDisk, secondDisk;
	CUnitDiskObjectVector vtDisks;
	
//	firstDisk = boost::dynamic_pointer_cast<CUnitDiskObject>( front() );
//	secondDisk = boost::dynamic_pointer_cast<CUnitDiskObject>( back() );
	vtDisks.push_back( FirstDisk() );
	vtDisks.push_back( SecondDisk() );

	//
	// Unbind disks & bind again with version 2 information
	// 
	FirstDisk()->UnBind( FirstDisk() );
	SecondDisk()->UnBind( SecondDisk() );

	FirstDisk()->Bind( vtDisks, 0, NMT_RAID1 );
	SecondDisk()->Bind( vtDisks, 1, NMT_RAID1 );

}

UINT32
CMirDiskObject::GetNDASMediaType() const
{
	if(::IsEmptyDisk(FirstDisk()))
	{
		if(::IsEmptyDisk(SecondDisk()))
			return NMT_INVALID;
		else
		{
			CUnitDiskObjectPtr unitDisk = 
				boost::dynamic_pointer_cast<CUnitDiskObject>(SecondDisk());
			CUnitDiskInfoHandlerPtr unitHandler = unitDisk->GetInfoHandler();

			return unitHandler->GetNDASMediaType();
		}
	}
	else
	{
		CUnitDiskObjectPtr unitDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(FirstDisk());
		CUnitDiskInfoHandlerPtr unitHandler = unitDisk->GetInfoHandler();

		return unitHandler->GetNDASMediaType();
	}
}

CUnitDiskObjectPtr
CMirDiskObject::FirstDisk() const
{
	return boost::dynamic_pointer_cast<CUnitDiskObject>( front() );
}

CUnitDiskObjectPtr
CMirDiskObject::SecondDisk() const
{
	return boost::dynamic_pointer_cast<CUnitDiskObject>( back() );
}

///////////////////////////////////////////////////////////////////////////////
// CRAID4DiskObject
///////////////////////////////////////////////////////////////////////////////
WTL::CString CRAID4DiskObject::GetTitle() const
{
	return front()->GetTitle();
}

BOOL CRAID4DiskObject::IsUsable() const
{
	UINT32 nUnusable = 0;
	CUnitDiskObjectPtr pUnitDisk;
	std::list<CDiskObjectPtr>::iterator it;

	for(it = begin(); it != end(); ++it)
	{
		pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
		if(!pUnitDisk->IsUsable())
			nUnusable++;
	}	

	return (nUnusable < 2);
}

BOOL CRAID4DiskObject::IsBroken() const
{
	return !IsUsable();
}

BOOL CRAID4DiskObject::IsDirty() const
{
	return (GetDirtyDisk() >= 0);
}

// -2 : clean
// -1 : failed
// 0 ~ cnt -1 : broken disk idx

#define RAID_CRITICAL_CHECK
#undef RAID_CRITICAL_CHECK

INT32 CRAID4DiskObject::GetDirtyDisk() const
{
	LAST_WRITTEN_SECTORS Base, Compare;
	BOOL bIsDirty;
	INT32 iDirtyDisk, i;

	if(size() < 3)
		return -1;

	CUnitDiskObjectPtr pUnitDisk;
	CUnitDiskInfoHandlerPtr pUnitDiskHandler;
	std::list<CDiskObjectPtr>::iterator it;

	bIsDirty = FALSE;
	i = 0;
	iDirtyDisk = -2;

	// checking bitmaps
	for(it = begin(); it != end(); ++it, i++)
	{
		pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
		if(::IsEmptyDisk(pUnitDisk))
			return -1;

		pUnitDiskHandler = pUnitDisk->GetInfoHandler();
		if(pUnitDiskHandler->IsPeerDirty())
		{
#ifdef RAID_CRITICAL_CHECK
			if(bIsDirty)
				return -1;
#endif
			bIsDirty = TRUE;
			
			iDirtyDisk = (i == 0) ? size() -1 : i -1; // previous device is dirty
		}
	}

	// checking LWS
	// 1 pass
	INT32 iSuspect;
	INT32 nFailCnt;
	i = 0;
	nFailCnt = 0;
	for(it = begin(); it != end(); ++it, i++)
	{
		pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
		if(::IsEmptyDisk(pUnitDisk))
		{
			return -1;
		}

		pUnitDiskHandler = pUnitDisk->GetInfoHandler();

		if(!i)
		{
			if(!pUnitDiskHandler->GetLastWrittenSectorsInfo(&Base))
				return -1;

			continue;
		}

		if(!pUnitDiskHandler->GetLastWrittenSectorsInfo(&Compare))
			return -1;

		if(memcmp(&Base, &Compare, sizeof(Base)))
		{
			nFailCnt++;
			iSuspect = i;
		}
	}

	if(0 == nFailCnt) // all are same
	{
		return iDirtyDisk;
	}
	else if(1 == nFailCnt)
	{
		if(bIsDirty && iSuspect != iDirtyDisk)
#ifdef RAID_CRITICAL_CHECK
			return -1;
#else
			return iDirtyDisk;
#endif
		return iSuspect;
	}
	else if(size() -1 == nFailCnt) // 1st device(0) is the suspect
	{
		iSuspect = 0;
		i = 1;

		for(it = begin(), ++it; it != end(); ++it)
		{
			pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
			if(::IsEmptyDisk(pUnitDisk))
			{
				return -1;
			}

			pUnitDiskHandler = pUnitDisk->GetInfoHandler();

			if(1 == i)
			{
				if(!pUnitDiskHandler->GetLastWrittenSectorsInfo(&Base))
					return -1;

				continue;
			}

			if(!pUnitDiskHandler->GetLastWrittenSectorsInfo(&Compare))
				return -1;

			if(memcmp(&Base, &Compare, sizeof(Base)))
			{
#ifdef RAID_CRITICAL_CHECK
				return -1;
#else
				return (-2 == iDirtyDisk) ? iSuspect : iDirtyDisk;
#endif
			}

			i++;
		}

		// all the others are same
		if(bIsDirty && iSuspect != iDirtyDisk)
#ifdef RAID_CRITICAL_CHECK
			return -1;
#else
			return (-2 == iDirtyDisk) ? iSuspect : iDirtyDisk;
#endif
		
		return iSuspect;
	}
	else // 2 or more devices are not same
	{
#ifdef RAID_CRITICAL_CHECK
		return -1;
#else
		return (-2 == iDirtyDisk) ? iSuspect : iDirtyDisk;
#endif
	}
}

_int64 CRAID4DiskObject::GetUserSectorCount() const
{
	CUnitDiskObjectPtr pUnitDisk;
	std::list<CDiskObjectPtr>::iterator it;

	for(it = begin(); it != end(); ++it)
	{
		pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
		if(!::IsEmptyDisk(pUnitDisk))
		{
			return pUnitDisk->GetUserSectorCount() * (size() -1);
		}
	}	

	return 0;
}

UINT32
CRAID4DiskObject::GetNDASMediaType() const
{
	CUnitDiskObjectPtr pUnitDisk;
	std::list<CDiskObjectPtr>::iterator it;

	for(it = begin(); it != end(); ++it)
	{
		pUnitDisk = boost::dynamic_pointer_cast<CUnitDiskObject>(*it);
		if(!::IsEmptyDisk(pUnitDisk))
		{
			CUnitDiskInfoHandlerPtr unitHandler = pUnitDisk->GetInfoHandler();

			return unitHandler->GetNDASMediaType();
		}
	}	

	return NMT_INVALID;
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
CDiskLocationPtr CUnitDiskObject::GetLocation() const 
{
	return m_pLocation;
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

DWORD CUnitDiskObject::GetSlotNo() const
{
	return 0xFFFFFFFF;
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
	return m_pHandler->HasValidInfo();
}

BOOL CUnitDiskObject::IsUnitDisk() const
{
	return TRUE;
}

BOOL CUnitDiskObject::IsBroken() const
{
	return FALSE;
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


void CUnitDiskObject::Initialize(CDiskObjectPtr _this)
{
	ATLASSERT( this == _this.get() );
	m_pHandler->Initialize(
		boost::dynamic_pointer_cast<CUnitDiskObject>(_this)
		);
}

CEmptyDiskObject::CEmptyDiskObject() : 
	CUnitDiskObject(WTL::CString(_T("")), NULL, new CEmptyDiskInfoHandler)
{
}