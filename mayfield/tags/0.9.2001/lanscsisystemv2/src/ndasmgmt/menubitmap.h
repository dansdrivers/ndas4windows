#pragma once

class CNdasMenuBitmapHandler :
	public CMessageMap
{
public:

	typedef struct _SI_DATA{
		BYTE nStatus;
		BYTE Status0;
		BYTE Status1;
		BYTE Reserved;
	} SI_DATA, *PSI_DATA;

	static const BYTE SI_DISCONNECTED = 0x11;
	static const BYTE SI_CONNECTED = 0x12;
	static const BYTE SI_MOUNTED_RW = 0x13;
	static const BYTE SI_MOUNTED_RO = 0x14;
	static const BYTE SI_DISABLED = 0x80;
	static const BYTE SI_ERROR = 0xF0;
	static const BYTE SI_UNKNOWN = 0xFF;

	BEGIN_MSG_MAP_EX(CNdasMenuBitmapHandler)
		MSG_WM_DRAWITEM(OnDrawItem)
		MSG_WM_MEASUREITEM(OnMeasureItem)
	END_MSG_MAP()

	void OnDrawItem(UINT nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	void OnMeasureItem(UINT nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);

};
