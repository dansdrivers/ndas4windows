#pragma once
#include <atlmisc.h>

typedef union _NDSI_DATA{
	struct {
		BYTE Status;
		BYTE nParts;
		BYTE StatusPart[2];
	};
	ULONG_PTR ulongCaster;
} NDSI_DATA, *PNDSI_DATA;

// Device Status
const BYTE NDSI_UNKNOWN = 0xFF;
const BYTE NDSI_ERROR = 0xF0;
const BYTE NDSI_DISCONNECTED = 0x11;
const BYTE NDSI_DISABLED = 0x80;
const BYTE NDSI_CONNECTED = 0x12;
const BYTE NDSI_CONNECTING = 0x13;
// Unit Device Status
const BYTE NDSI_PART_CONTENT_IS_ENCRYPTED = 0x80;
const BYTE NDSI_PART_UNMOUNTED = 0x01;
const BYTE NDSI_PART_MOUNTED_RW = 0x02;
const BYTE NDSI_PART_MOUNTED_RO = 0x03;
const BYTE NDSI_PART_ERROR      = 0x0F;

class CNdasMenuBitmapHandler : public CMessageMap
{
public:

	BEGIN_MSG_MAP_EX(CNdasMenuBitmapHandler)
		MSG_WM_DRAWITEM(OnDrawItem)
		MSG_WM_MEASUREITEM(OnMeasureItem)
	END_MSG_MAP()

	BOOL Initialize();
	void Cleanup();

	void OnDrawItem(UINT nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	void OnMeasureItem(UINT nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	void OnDrawStatusText(LPDRAWITEMSTRUCT lpDrawItemStruct);

private:

	WTL::CImageList m_imageList;

};
