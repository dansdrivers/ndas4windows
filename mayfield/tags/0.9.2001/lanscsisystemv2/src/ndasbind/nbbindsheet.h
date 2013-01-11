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
	// Vector used for the following variables
	// since the order of disks does matter with them.
	CDiskObjectVector m_vtPrimary;
	CDiskObjectVector m_vtMirror;
	CDiskObjectVector m_vtBound;
	UINT m_nBindType;
	UINT m_nDiskCount;
	BOOL m_bUseMirror;
public:
	CBindSheet(void);

	BEGIN_MSG_MAP_EX(CBindSheet)
		MSG_WM_INITDIALOG(OnInitDialog)
	END_MSG_MAP();

	LRESULT OnInitDialog(HWND hWndFocus, LPARAM lParam);

	//
	// Methods to handle data set by each page.
	//
	void SetBindType(UINT nType);
	UINT GetBindType() const;
	void SetDiskCount(UINT nCount);
	UINT GetDiskCount() const;
	void SetSingleDisks(CDiskObjectList singleDisks);
	CDiskObjectList GetSingleDisks() const;
	void SetPrimaryDisks(CDiskObjectVector vtDisks);
	CDiskObjectVector GetPrimaryDisks() const;
	void SetMirrorDisks(CDiskObjectVector vtDisks);
	CDiskObjectVector GetMirrorDisks() const;
	void SetBoundDisks(CDiskObjectVector vtDisks);
	CDiskObjectVector GetBoundDisks() const;
};


#endif // _NBBINDSHEET_H_