#include "stdafx.h"
#include "ndasmgmt.h"
#include "menubitmap.h"

void 
CNdasMenuBitmapHandler::
OnDrawItem(
	UINT nIDCtl, 
	LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	ndas::Device* pDevice = 
		reinterpret_cast<ndas::Device*>(lpDrawItemStruct->itemData);
	CDCHandle dc = lpDrawItemStruct->hDC;

	CRect rect(&lpDrawItemStruct->rcItem);
	rect.DeflateRect(3,3);

	switch (pDevice->GetStatus()) {
	case NDAS_DEVICE_STATUS_DISABLED:
		{
			CBrush brush;
			brush.CreateSolidBrush(RGB(255,255,255)); // ::GetSysColor(COLOR_MENU));
			HBRUSH hOldBrush = dc.SelectBrush(brush);

			CPen pen;
			pen.CreatePen(PS_SOLID | PS_COSMETIC, 2, RGB(254,0,0)); 
			HPEN hOldPen = dc.SelectPen(pen);

			dc.Pie(rect, 
				CPoint(rect.right, rect.top), 
				CPoint(rect.left, rect.bottom));

			dc.Pie(rect, 
				CPoint(rect.left, rect.bottom),
				CPoint(rect.right, rect.top)); 

			dc.SelectBrush(hOldBrush);
			dc.SelectPen(hOldPen);
		}
		break;
	case NDAS_DEVICE_STATUS_CONNECTED:
		{
			CBrush brush;

			DWORD nUnitDevices = pDevice->GetUnitDeviceCount();
			for (DWORD i = 0; i < nUnitDevices; ++i) {
				
				ndas::UnitDevice* pUnitDevice = pDevice->GetUnitDevice(i);
				if (NULL == pUnitDevice) {
					break;
				}

				NDAS_LOGICALDEVICE_ID logDevId = 
					pUnitDevice->GetLogicalDeviceId();
				
				ndas::LogicalDevice* pLogDev = 
					_pLogDevColl->FindLogicalDevice(logDevId);
				if (NULL == pLogDev) {
					break;
				}
			
				NDAS_LOGICALDEVICE_STATUS status = pLogDev->GetStatus();

				switch (status) {
				case NDAS_LOGICALDEVICE_STATUS_MOUNTED:
					{
						ACCESS_MASK mountedAccess = pLogDev->GetMountedAccess();
						if (mountedAccess & GENERIC_WRITE) {
							brush.CreateSolidBrush(RGB(53,53,225));
						} else {
							brush.CreateSolidBrush(RGB(53,255,53));
						}
					}
					break;
				case NDAS_LOGICALDEVICE_STATUS_UNMOUNTED:
					brush.CreateSolidBrush(RGB(244,244,244));
					break;
				case NDAS_LOGICALDEVICE_STATUS_NOT_MOUNTABLE:
					break;
				default:
					;
				}

			}
			// Sub-status
			if (NULL == brush.m_hBrush) {
				brush.CreateSolidBrush(RGB(102,5,5));
			}

			CPen pen;
			// pen.CreatePen(PS_SOLID, 0, ::GetSysColor(COLOR_MENU)); 
			pen.CreatePen(PS_SOLID | PS_COSMETIC, 1, RGB(100, 100, 100));
			HPEN hOldPen = dc.SelectPen(pen);
			HBRUSH hOldBrush = dc.SelectBrush(brush);
			// dc.Ellipse(rect);
			dc.Rectangle(rect); 
			dc.SelectBrush(hOldBrush);
			dc.SelectPen(hOldPen);
		}
		break;
	case NDAS_DEVICE_STATUS_DISCONNECTED:
		{
			// Block dot? Error?
			CBrush brush;
			brush.CreateSolidBrush(RGB(10,10,10));
			HBRUSH hOldBrush = dc.SelectBrush(brush);
			// dc.Ellipse(rect); 
			dc.Rectangle(rect); 
			dc.SelectBrush(hOldBrush);
		}
		break;
	case NDAS_DEVICE_STATUS_UNKNOWN:
		{
			// ?
			CBrush brush;
			brush.CreateSolidBrush(RGB(128,128,0));
			HBRUSH hOldBrush = dc.SelectBrush(brush);
			// dc.Ellipse(rect); 
			dc.Rectangle(rect); 
			dc.TextOut(0,0,_T("?"));
			dc.SelectBrush(hOldBrush);
		}
		break;
	default:
		;
	}

}

void 
CNdasMenuBitmapHandler::
OnMeasureItem(UINT nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
	lpMeasureItemStruct->itemWidth = lpMeasureItemStruct->itemHeight;
}
