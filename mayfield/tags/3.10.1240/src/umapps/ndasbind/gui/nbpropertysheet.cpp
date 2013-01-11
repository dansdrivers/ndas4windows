////////////////////////////////////////////////////////////////////////////
//
// Implementation of CDiskPropertySheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "nbpropertysheet.h"

CDiskPropertySheet::CDiskPropertySheet()
{
	AddPage( m_page1 );
	m_page1.SetParentSheet( this );
	AddPage( m_page2 );
	m_page2.SetParentSheet( this );
}

void CDiskPropertySheet::SetDiskObject(CDiskObjectPtr disk)
{
	m_disk = disk;
}

CDiskObjectPtr CDiskPropertySheet::GetDiskObject()
{
	return m_disk;
}

CUnsupportedDiskPropertySheet::CUnsupportedDiskPropertySheet()
{
	AddPage( m_page );
}