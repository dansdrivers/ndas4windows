////////////////////////////////////////////////////////////////////////////
//
// Interface of CNBListViewCtrl class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _NBLISTVIEW_H_
#define _NBLISTVIEW_H_

#include <map>
#include "nbdev.h"

#define WM_USER_NB_VIEW_LDBLCLK (WM_USER + 0x150)
#define WM_USER_NB_BIND_VIEW_LDBLCLK (WM_USER + 0x151)

class CNBListViewCtrl : public CWindowImpl<CNBListViewCtrl, CListViewCtrl>
{
protected:
	UINT m_iColSort;	// index of the column to sort by
	UINT m_nColCount;
	BOOL m_abSortAsc[10];	// TRUE if sorted in ascending order.
	BOOL m_bSort;
//	NBUnitDevicePtrMap m_mapUnitDevices;

public:

	BEGIN_MSG_MAP_EX(CNBListViewCtrl)
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_GETDISPINFO, OnGetDispInfo)
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_COLUMNCLICK, OnColumnClick)
		MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
		DEFAULT_REFLECTION_HANDLER()
	END_MSG_MAP()

	//
	// Constructor
	//
	// @param nColCount	[in] Number of columns
	//						1 : Name only
	//						2 : Name and ID
	//						3 : Name, ID and size
	CNBListViewCtrl(UINT nColCount = 2, BOOL bSort = TRUE) 
		: m_iColSort(0), m_nColCount(nColCount), m_bSort(bSort)
	{
		for ( int i=0; i < sizeof(m_abSortAsc)/sizeof(m_abSortAsc[0]); i++ )
		{
			m_abSortAsc[i] = TRUE;
		}
	}

	int FindDiskObjectItem(CNBUnitDevice *o);
	void AddDiskObject(CNBUnitDevice *o);
	void AddDiskObjectList(NBUnitDevicePtrList &mapUnitDevices);
	void DeleteDiskObject(CNBUnitDevice *o);
	void DeleteDiskObjectList(NBUnitDevicePtrList &mapUnitDevices);
	void SelectDiskObject(CNBUnitDevice *o);
	void UnselectDiskObject(CNBUnitDevice *o);
	void SelectDiskObjectList(NBUnitDevicePtrList &mapUnitDevices);
	void SelectAllDiskObject();
	//
	// Returns list of disks in the listview
	//
	NBUnitDevicePtrList GetDiskObjectList();
	NBUnitDevicePtrList GetSelectedDiskObjectList();

	// Notifying message handler
	LRESULT OnGetDispInfo(LPNMHDR lParam);
	LRESULT OnColumnClick(LPNMHDR lParam);
	virtual void OnLButtonDblClk(UINT /*nFlags*/, CPoint point);
	// Methods used to sort items
	static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
	void SortItems();

	//
	// Virtual methods
	//
	virtual void InitColumn();
	virtual int  CompareItems(CNBUnitDevice *obj1, CNBUnitDevice *obj2);
};

class CCustomStaticCtrl :
	public CWindowImpl<CCustomStaticCtrl, CStatic>
{
public:
	BEGIN_MSG_MAP_EX(CCustomStaticCtrl)
		MSG_WM_PAINT(OnPaint)
	END_MSG_MAP()

	void OnPaint(HDC wParam);
};

class CNBBindListViewCtrl: 
	public CNBListViewCtrl,
	public CCustomDraw<CNBBindListViewCtrl>
{
protected:
	std::list<CCustomStaticCtrl*> m_vtRowHeader;
	UINT m_nMaxCount;
public:
	~CNBBindListViewCtrl();
	BEGIN_MSG_MAP_EX(CNBBindListViewCtrl)
		MSG_WM_PAINT(OnPaint)
		MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
		REFLECTED_NOTIFY_CODE_HANDLER_EX(LVN_GETDISPINFO, OnGetDispInfo)
		CHAIN_MSG_MAP_ALT(CCustomDraw<CNBBindListViewCtrl>, 1)
		CHAIN_MSG_MAP(CNBListViewCtrl)
	END_MSG_MAP()

	CNBBindListViewCtrl(UINT nColCount = 2) 
	: CNBListViewCtrl(nColCount+1)
	{
		// One more column is needed to display index.
	}

	virtual void InitColumn();
	virtual int  CompareItems(CNBUnitDevice *obj1, CNBUnitDevice *obj2);
	// Notifying message handler
	LRESULT OnGetDispInfo(LPNMHDR lParam);
	virtual void OnLButtonDblClk(UINT /*nFlags*/, CPoint point);

	void MoveSelectedItemUp();
	void MoveSelectedItemDown();
	BOOL IsItemMovable(BOOL bUp);
	void SetMaxItemCount(UINT nCount);

	// Custom draw
	void OnPaint(HDC wParam);

};
#endif // _NBLISTVIEW_H_