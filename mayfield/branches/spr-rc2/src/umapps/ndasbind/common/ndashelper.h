////////////////////////////////////////////////////////////////////////////
//
// Helper functions for ndasbind
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASHELPER_H_
#define _NDASHELPER_H_

#include "ndasobject.h"

// function object used with 'find_if' and 'bind1st' 
// to find whether a disk object's location matches the location provided
class CDiskLocationEqual : public std::binary_function<CDiskLocationPtr, CUnitDiskObjectPtr, BOOL>
{
public:
	BOOL operator() (first_argument_type first, second_argument_type second) const
	{
		return first->Equal( second->GetLocation() );
	}
};

//
// find whether two disk's bind list matches
//
BOOL 
HasSameBoundDiskList(CUnitDiskObjectPtr first, CUnitDiskObjectPtr second);

CEmptyDiskObjectPtr
CreateEmptyDiskObject();

//
// Find the mirror of a disk from list of disks
// 
// @param src	[in] disk which is mirrored by the disk to find
// @param disks [in] list of disks
//
// @return : The mirror disk found. If the mirror disk does not exist
//			in the list. an empty CUnitDiskObjectPtr is returned.
//			(You should check whether it is an empty return by
//			 compare 'returned_value.get()' with 'NULL'
//
CUnitDiskObjectPtr 
FindMirrorDisk(CUnitDiskObjectPtr src, CUnitDiskObjectList disks);

///////////////////////////////////////////////////////////////////////////////
// classes and functions that use visitor class
///////////////////////////////////////////////////////////////////////////////
//
// visitor class used to find a list of disks that satisfy condition.
// 
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
	//
	// Find a list of disks that satisfy condition
	//
	// @param root [In] root of the disks to search.
	// @param pred [In] compare function that returns TRUE for disks 
	//					that satisfy condition.
	//
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
BOOL IsWritable(CDiskObjectPtr obj);
BOOL IsWritableUnitDisk(CDiskObjectPtr obj);

#endif // _NDASHELPER_H_