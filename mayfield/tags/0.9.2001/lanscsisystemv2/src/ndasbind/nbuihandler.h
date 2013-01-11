////////////////////////////////////////////////////////////////////////////
//
// Adapter class between ndas object classes and UI
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _UI_HANDLER_H_
#define _UI_HANDLER_H_

#include <vector>
#include "ndasobject.h"
//namespace ximeta
//{

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
	virtual CCommandSet GetCommandSet(CDiskObject *o) const = 0;
	static const CObjectUIHandler	*GetUIHandler(const CDiskObject *o);
	static CImageList GetImageList();

	WTL::CString GetTitle(CDiskObject *o) const;
	UINT GetIconIndex(CDiskObject *o) const
	{
		return GetIconIndexFromID( GetIconID(o) );
	}
	UINT GetSelectedIconIndex(CDiskObject *o) const 
	{ 
		return GetIconIndexFromID( GetSelectedIconID(o) ); 
	}

	virtual UINT GetSelectedIconID(CDiskObject *o) const 
	{ 
		return GetIconID(o); 
	}

	// Get the size of disk in MB
	virtual UINT GetSizeInMB(CDiskObject *o) const;
	WTL::CString GetStringID(CDiskObject *o) const;


	//
	// Adjust comamnds to the menu
	//
	// @param hMenu			[in] handle to the menu to modify
	//
	void InsertMenu(CDiskObject *o, HMENU hMenu) const;

	//
	// Abstract methods
	//
	virtual UINT GetIconID(CDiskObject *o) const = 0;

};

class CAggrDiskUIHandler : public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObject *o) const;
public:
	virtual UINT GetIconID(CDiskObject *o) const;

};

class CMirDiskUIHandler : public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObject *o) const;
public:
	virtual UINT GetIconID(CDiskObject *o) const;
};

class CUnitDiskUIHandler : public CObjectUIHandler
{
protected:
	virtual CCommandSet GetCommandSet(CDiskObject *o) const;
public:
	virtual UINT GetIconID(CDiskObject *o) const;
};

//}

#endif // _UI_HANDLER_H_