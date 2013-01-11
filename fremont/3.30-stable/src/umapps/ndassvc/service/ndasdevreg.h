/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#pragma once
#include "ndascomobjectsimpl.hpp"

class CNdasDeviceRegistrar : 
	public CComObjectRootEx<CComMultiThreadModel>,
	public INdasDeviceRegistrarInternal
{
public:

	CNdasDeviceRegistrar();
	~CNdasDeviceRegistrar();

	BEGIN_COM_MAP(CNdasDeviceRegistrar)
		COM_INTERFACE_ENTRY(INdasDeviceRegistrarInternal)
	END_COM_MAP()

	STDMETHODIMP Register(
		__in_opt DWORD SlotNo,
		__in const NDAS_DEVICE_ID& DeviceId,
		__in DWORD Flags,
		__in_opt const NDASID_EXT_DATA* NdasIdExtension,
		__in BSTR Name,
		__in ACCESS_MASK GrantedAccess,
		__in_opt const NDAS_OEM_CODE* NdasOemCode,
		__deref_out INdasDevice** ppNdasDevice);

	STDMETHODIMP Deregister(__in INdasDevice* pNdasDevice);

	STDMETHODIMP get_NdasDevice(__in NDAS_DEVICE_ID* DeviceId, __deref_out INdasDevice** ppNdasDevice);
	STDMETHODIMP get_NdasDevice(__in DWORD SlotNo, __deref_out INdasDevice** ppNdasDevice);
	STDMETHODIMP get_NdasDevice(__in NDAS_DEVICE_ID_EX* DeviceId, __deref_out INdasDevice** ppNdasDevice);

	STDMETHODIMP get_NdasDevices(__in DWORD Flags, __deref_out CInterfaceArray<INdasDevice>& NdasDevices);

	STDMETHODIMP Bootstrap();
	STDMETHODIMP Shutdown();

private:

	typedef std::map<DWORD,INdasDevice*> DeviceSlotMap;
	typedef std::map<NDAS_DEVICE_ID,INdasDevice*> DeviceIdMap;

	enum { MAX_SLOT_NUMBER = 255 };
	static const TCHAR CFG_CONTAINER[];

	CLock m_DataLock;
	BOOL m_fBootstrapping;

	DeviceSlotMap m_deviceSlotMap;
	DeviceIdMap m_deviceIdMap;

	CInterfaceArray<INdasDevice> m_NdasDevices;
	std::vector<bool> m_slotbit;

	//
	// Look up empty slot
	//
	// returns an empty slot number if successful.
	// otherwise returns 0 - as the slot no is one-based index.
	//
	DWORD pLookupEmptySlot();

	//
	// Import Legacy Settings function
	//
	BOOL pImportLegacyEntry(DWORD dwSlotNo, HKEY hEntryKey);
	BOOL pImportLegacySettings();

private:
	// NOT IMPLEMENTED
	CNdasDeviceRegistrar(const CNdasDeviceRegistrar&);
	CNdasDeviceRegistrar& operator=(const CNdasDeviceRegistrar&);
};

