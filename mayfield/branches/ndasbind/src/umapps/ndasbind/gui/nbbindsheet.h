////////////////////////////////////////////////////////////////////////////
//
// Interface of CBindSheet class 
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NBBINDSHEET_H_
#define _NBBINDSHEET_H_

#include "resource.h"
#include "ndasobject.h"
#include "nbbindpages.h"

class CBindSheet
	: public CPropertySheetImpl<CBindSheet>
{
protected:
	CBindPage1 m_page1;
	CBindPage2 m_page2;

	CDiskObjectList m_listSingle;
	CUnitDiskObjectVector m_vtBound;
	UINT m_nBindType;
	UINT m_nDiskCount;
public:
	CBindSheet(void);

	BEGIN_MSG_MAP_EX(CBindSheet)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP();

	LRESULT OnInitDialog(HWND hWndFocus, LPARAM lParam);

	//
	// Methods to handle data set by each page.
	//
	//
	// NOTE : This bind type must conform to 
	//		the iMediaType of DISK_INFORMATION_BLOCK_V2
	//
	void SetBindType(UINT nType);
	UINT GetBindType() const;
	void SetDiskCount(UINT nCount);
	UINT GetDiskCount() const;
	void SetSingleDisks(CDiskObjectList singleDisks);
	CDiskObjectList GetSingleDisks() const;
	void SetBoundDisks(CUnitDiskObjectVector vtDisks);
	CUnitDiskObjectVector GetBoundDisks() const;
};


#endif // _NBBINDSHEET_H_