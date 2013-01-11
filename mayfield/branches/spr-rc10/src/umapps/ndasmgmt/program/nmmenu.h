#pragma once
#include "ndasmgmt.h"
#include "menubitmap.h"

#define LOGDEV_MAKE_ULONG_PTR(s,t,l) \
	(((ULONG_PTR)((WORD)(s))) | \
	((ULONG_PTR)(MAKEWORD(t,l)) << 16))

//#define LOGDEV_MAKE_ULONG_PTR_FROM_LDID(id) \
//	LOGDEV_MAKE_ULONG_PTR(id.SlotNo, id.TargetId, id.LUN)
//
//#define LOGDEV_ULONG_PTR_TO_SLOTNO(x) \
//	((DWORD)(LOWORD((x))))
//
//#define LOGDEV_ULONG_PTR_TO_TARGETID(x) \
//	((DWORD)(HIWORD(LOBYTE(x))))
//
//#define LOGDEV_ULONG_PTR_TO_LUN(x) \
//	((DWORD)(HIWORD(HIBYTE((x)))))
//
//#define LOGDEV_ULONG_PTR_TO_LDID(u) { \
//	LOGDEV_ULONG_PTR_TO_SLOTNO(u), \
//	LOGDEV_ULONG_PTR_TO_TARGETID(u), \
//	LOGDEV_ULONG_PTR_TO_LUN(u) }

#define UNITDEV_MAKE_ULONG_PTR(devid,un) \
	((ULONG_PTR)(((WORD)(devid)) | (((WORD)(un)) << 16)))

#define UNITDEV_ULONG_PTR_TO_DEVID(p) \
	((DWORD)((LOWORD)(p)))

#define UNITDEV_ULONG_PTR_TO_UNITNO(p) \
	((DWORD)((HIWORD)(p)))


class CNdasDeviceMenu
{
	CWindow m_wnd;

public:

	CNdasDeviceMenu(HWND hWnd);

	VOID AppendUnitDeviceMenuItem(
		ndas::UnitDevice* pUnitDevice,
		HMENU hMenu,
		PBYTE psiPartData);

	VOID CreateDeviceMenuItem(
		IN ndas::Device* pDevice,
		IN OUT MENUITEMINFO& mii);

	HMENU CreateDeviceSubMenu(
		ndas::Device* pDevice,
		NDSI_DATA* psiData);
};

template <typename WindowT>
class CNdasDeviceMenuHandler :
	// Derive from the CMessageMap to receive dynamically
	// chained messages
	public CMessageMap
{
public:
	BEGIN_MSG_MAP_EX(CNdasDeviceMenuHandler)
	// Handle messages from the view and the main frame
		COMMAND_ID_HANDLER_EX(IDR_NDD_MOUNT_RO, OnMountRO)
		COMMAND_ID_HANDLER_EX(IDR_NDD_MOUNT_RW, OnMountRW)
		COMMAND_ID_HANDLER_EX(IDR_NDD_UNMOUNT, OnUnmount)
	END_MSG_MAP()

	VOID OnMountRO(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnMountRO: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}

	VOID OnMountRW(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnMountRW: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}

	VOID OnUnmount(UINT fControl, int id, HWND hWndCtl)
	{
		ATLTRACE(_T("OnUnmount: fControl %d, id %d, hWndCtl %d\n"), fControl, id, hWndCtl);
	}
};

