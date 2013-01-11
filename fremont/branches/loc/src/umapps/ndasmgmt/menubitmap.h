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

enum 
{
	// Device Status
	NDSI_UNKNOWN = 0xFF,
	NDSI_ERROR = 0xF0,
	NDSI_DISCONNECTED = 0x11,
	NDSI_DISABLED = 0x80,
	NDSI_CONNECTED = 0x12,
	NDSI_CONNECTING = 0x13,
	// Unit Device Status
	NDSI_PART_CONTENT_IS_ENCRYPTED = 0x80,
	NDSI_PART_UNMOUNTED = 0x01,
	NDSI_PART_MOUNTED_RW = 0x02,
	NDSI_PART_MOUNTED_RO = 0x03,
	NDSI_PART_ERROR = 0x0F,

	NDSI_StatusTextItemId = 100,

	// Image List Indexes

	nDISCONNECTED = 0,
	nCONNECTING = 1,
	nCONNECTED = 2,
	nMOUNTED_RW = 3,
	nMOUNTED_RO = 4,
	nERROR = 5,
	nDEACTIVATED = 6,
	nENCRYPTED = 7,
	nOV_ENCRYPTED = 1,

	// 14 by 14 images
	cxStatusBitmap = 14, 
};

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
