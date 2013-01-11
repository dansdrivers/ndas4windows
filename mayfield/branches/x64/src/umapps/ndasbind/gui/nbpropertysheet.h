////////////////////////////////////////////////////////////////////////////
//
// Interface of CDiskPropertySheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBPROPERTYSHEET_H_
#define _NBPROPERTYSHEET_H_

#include "resource.h"
#include "ndasobject.h"
#include "nbpropertypages.h"

class CDiskPropertySheet
	: public CPropertySheetImpl<CDiskPropertySheet>
{
protected:
	CDiskPropertyPage1 m_page1;
	CDiskPropertyPage2 m_page2;
	
	CDiskObjectPtr m_disk;
public:
	CDiskPropertySheet();
	BEGIN_MSG_MAP_EX(CDiskPropertySheet)
	END_MSG_MAP();

	void SetDiskObject(CDiskObjectPtr disk);
	CDiskObjectPtr GetDiskObject();
};

class CUnsupportedDiskPropertySheet
	: public CPropertySheetImpl<CUnsupportedDiskPropertySheet>
{
protected:
	CDiskPropertyPage3 m_page;
public:
	CUnsupportedDiskPropertySheet();
	BEGIN_MSG_MAP_EX(CUnsupportedDiskPropertySheet)
	END_MSG_MAP()
};
#endif // _NBPROPERTYSHEET_H_