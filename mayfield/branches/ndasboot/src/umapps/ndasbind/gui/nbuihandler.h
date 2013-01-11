////////////////////////////////////////////////////////////////////////////
//
// Adapter class between ndas object classes and UI
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _UI_HANDLER_H_
#define _UI_HANDLER_H_

#include <vector>
#include <list>
#include "ndasobject.h"
//namespace ximeta
//{

typedef struct _PropertyListItem
{
	WTL::CString strName;
	WTL::CString strValue;
	WTL::CString strToolTip;
} PropertyListItem;
typedef std::list<PropertyListItem> PropertyList;

class CCommand
{
protected:
	UINT m_nID;
	BOOL m_bDisabled;
public:
	UINT GetID() const { return m_nID; } 
	BOOL IsDisabled() const { return m_bDisabled; }
	CCommand(UINT nID, BOOL bEnabled=TRUE)
		: m_nID(nID), m_bDisabled(!bEnabled)
	{

	}
	CCommand(const CCommand &cmd)  
		: m_nID(cmd.m_nID), m_bDisabled(cmd.m_bDisabled)
	{
	}
};

class CCommandSet : public std::vector<CCommand>
{
public:
	void InsertMenu(HMENU hMenu);
};
//
// Adapter class for CDiskObject class
// This class converts the interface of CDiskObject class into UI interface
//
class CObjectUIHandler
{
private:
	static const UINT anIconIDs[];
	static UINT GetIconIndexFromID(UINT nID);
	
public:
	static const CObjectUIHandler *GetUIHandler(CDiskObjectPtr obj);
	static CImageList GetImageList();
	static BOOL IsValidDiskCount(UINT nDiskCount);

	WTL::CString GetTitle(CDiskObjectPtr obj) const;
	UINT GetIconIndex(CDiskObjectPtr obj) const
	{
		return GetIconIndexFromID( GetIconID(obj) );
	}
	UINT GetSelectedIconIndex(CDiskObjectPtr obj) const 
	{ 
		return GetIconIndexFromID( GetSelectedIconID(obj) ); 
	}

	virtual UINT GetSelectedIconID(CDiskObjectPtr obj) const 
	{ 
		return GetIconID(obj); 
	}

	// Get the size of disk in MB
	virtual UINT GetSizeInMB(CDiskObjectPtr obj) const;
	virtual WTL::CString GetStringID(CDiskObjectPtr obj) const;


	//
	// Adjust comamnds to the menu
	//
	// @param hMenu			[in] handle to the menu to modify
	//
	void InsertMenu(CDiskObjectPtr obj, HMENU hMenu) const;

	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const = 0;
	virtual UINT GetIconID(CDiskObjectPtr obj) const = 0;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const = 0;
	//
	// Dispatch command on the object.
	//
	// @return TRUE if the command is dispatched by the object
	//
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const = 0;

};

class CAggrDiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);

};

class CRAID0DiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);

};

class CRAID4DiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);
	BOOL OnRecover(CDiskObjectPtr obj) const;
};

class CMirDiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnSynchronize(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);
};

class CUnitDiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);
};

class CEmptyDiskUIHandler 
	: public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual UINT GetIconID(CDiskObjectPtr obj) const;
	virtual PropertyList GetPropertyList(CDiskObjectPtr obj) const;
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);
	WTL::CString GetStringID(CDiskObjectPtr obj) const;
};

class CUnsupportedDiskUIHandler : 
	public CUnitDiskUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObjectPtr obj) const;
	BOOL OnClearDIB(CDiskObjectPtr obj) const;
	BOOL OnProperty(CDiskObjectPtr obj) const;
public:
	virtual BOOL OnCommand(CDiskObjectPtr obj, UINT nCommandID) const;
	static BOOL IsValidDiskCount(UINT nDiskCount);
};

//}

#endif // _UI_HANDLER_H_