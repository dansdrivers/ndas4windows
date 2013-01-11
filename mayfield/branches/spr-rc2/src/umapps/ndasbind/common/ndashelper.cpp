////////////////////////////////////////////////////////////////////////////
//
// Helper functions for ndasbind
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndashelper.h"
#include <algorithm>

CUnitDiskObjectPtr 
FindMirrorDisk(CUnitDiskObjectPtr src, CUnitDiskObjectList disks)
{
	CUnitDiskInfoHandlerPtr handler = src->GetInfoHandler();
	ATLASSERT( handler->IsMirrored() );
	CUnitDiskObjectList::const_iterator found;

	found = std::find_if ( disks.begin(), disks.end(),
		std::bind1st( CDiskLocationEqual(), handler->GetPeerLocation() )
		);
	if ( found == disks.end() )
	{
		return CUnitDiskObjectPtr();
	}
	return *found;
}


BOOL 
HasSameBoundDiskList(CUnitDiskObjectPtr first, CUnitDiskObjectPtr second)
{
	CDiskLocationVector vtFirst, vtSecond;
	CUnitDiskInfoHandlerPtr handlerFirst, handlerSecond;

	handlerFirst = first->GetInfoHandler();
	handlerSecond = second->GetInfoHandler();

	if ( !handlerFirst->IsBound() || !handlerSecond->IsBound() )
	{
		return FALSE;
	}

	vtFirst = handlerFirst->GetBoundDiskLocations( first->GetLocation() );
	vtSecond = handlerSecond->GetBoundDiskLocations( second->GetLocation() );

	if ( vtFirst.size() != vtSecond.size() )
		return FALSE;

	for ( UINT i=0; i < vtFirst.size(); i++ )
	{
		if ( !vtFirst[i]->Equal(vtSecond[i]) )
			return FALSE;
	}
	return TRUE;
}

CEmptyDiskObjectPtr
CreateEmptyDiskObject()
{
	CEmptyDiskObjectPtr pEmptyDiskObjectPtr =
		boost::shared_ptr<CEmptyDiskObject>(new CEmptyDiskObject());

	return pEmptyDiskObjectPtr;
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

BOOL IsWritable(CDiskObjectPtr obj)
{
	return obj->GetAccessMask() & GENERIC_WRITE;
}

BOOL IsWritableUnitDisk(CDiskObjectPtr obj)
{
	return IsUnitDisk(obj) && IsWritable(obj);
}
