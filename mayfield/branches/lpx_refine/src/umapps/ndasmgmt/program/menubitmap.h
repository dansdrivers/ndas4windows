#pragma once

typedef union _NDSI_DATA{
	struct {
		BYTE Status;
		BYTE nParts;
		BYTE StatusPart[2];
	};
	ULONG_PTR ulongCaster;
} NDSI_DATA, *PNDSI_DATA;

// Device Status
static const BYTE NDSI_UNKNOWN = 0xFF;
static const BYTE NDSI_ERROR = 0xF0;
static const BYTE NDSI_DISCONNECTED = 0x11;
static const BYTE NDSI_DISABLED = 0x80;
static const BYTE NDSI_CONNECTED = 0x12;
// Unit Device Status
static const BYTE NDSI_PART_UNMOUNTED = 0x21;
static const BYTE NDSI_PART_MOUNTED_RW = 0x22;
static const BYTE NDSI_PART_MOUNTED_RO = 0x23;

class CNdasMenuBitmapHandler :
	public CMessageMap
{
public:

	BEGIN_MSG_MAP_EX(CNdasMenuBitmapHandler)
		MSG_WM_DRAWITEM(OnDrawItem)
		MSG_WM_MEASUREITEM(OnMeasureItem)
	END_MSG_MAP()

	void OnDrawItem(UINT nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	void OnMeasureItem(UINT nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	void OnDrawStatusText(LPDRAWITEMSTRUCT lpDrawItemStruct);
};
