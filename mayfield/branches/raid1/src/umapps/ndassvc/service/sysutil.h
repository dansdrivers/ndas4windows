#pragma once

namespace sysutil {

	BOOL WINAPI DisableDiskWriteCache(HANDLE hDisk);
	BOOL WINAPI DisableDiskWriteCache(LPCTSTR szDiskPath);

	BOOL WINAPI GetStorageHotplugInfo(
		HANDLE hStorage, 
		LPBOOL lpbMediaRemovable,
		LPBOOL lpbMediaHotplug, 
		LPBOOL lpbDeviceHotplug);

	BOOL WINAPI SetStorageHotplugInfo(
		HANDLE hStorage,
		BOOL bMediaRemovable,
		BOOL bMediaHotplug,
		BOOL bDeviceHotplug);

	//
	// from ntddndis.h
	//
	// Don't worry about the namespace collision
	// We are using different name space 'sysutil'
	//

	//
	// Medium the Ndis Driver is running on (OID_GEN_MEDIA_SUPPORTED/ OID_GEN_MEDIA_IN_USE).
	//
	typedef enum _NDIS_MEDIUM
	{
		NdisMedium802_3,
		NdisMedium802_5,
		NdisMediumFddi,
		NdisMediumWan,
		NdisMediumLocalTalk,
		NdisMediumDix,              // defined for convenience, not a real medium
		NdisMediumArcnetRaw,
		NdisMediumArcnet878_2,
		NdisMediumAtm,
		NdisMediumWirelessWan,
		NdisMediumIrda,
		NdisMediumBpc,
		NdisMediumCoWan,
		NdisMedium1394,
		NdisMediumInfiniBand,
		NdisMediumMax               // Not a real medium, defined as an upper-bound
	} NDIS_MEDIUM, *PNDIS_MEDIUM;

	//
	// Physical Medium Type definitions. Used with OID_GEN_PHYSICAL_MEDIUM.
	//
	typedef enum _NDIS_PHYSICAL_MEDIUM
	{
		NdisPhysicalMediumUnspecified,
		NdisPhysicalMediumWirelessLan,
		NdisPhysicalMediumCableModem,
		NdisPhysicalMediumPhoneLine,
		NdisPhysicalMediumPowerLine,
		NdisPhysicalMediumDSL,      // includes ADSL and UADSL (G.Lite)
		NdisPhysicalMediumFibreChannel,
		NdisPhysicalMedium1394,
		NdisPhysicalMediumWirelessWan,
		NdisPhysicalMediumMax       // Not a real physical type, defined as an upper-bound
	} NDIS_PHYSICAL_MEDIUM, *PNDIS_PHYSICAL_MEDIUM;

	BOOL WINAPI
	GetNetConnCharacteristics(
		IN DWORD cbAddr, 
		IN BYTE* pPhysicalAddr,
		OUT PLONGLONG pllLinkSpeed,
		OUT sysutil::NDIS_MEDIUM* pMediumInUse,
		OUT sysutil::NDIS_PHYSICAL_MEDIUM* pPhysicalMedium);

	//
	// llLinkSpeed is a unit of 100 bps
	// llLinkSpeed / 10 -> Kbps
	// llLinkSpeed / 10000 -> Mbps
	//
	LONGLONG WINAPI 
	GetNetConnLinkSpeed(
		DWORD cbAddr, 
		BYTE* pPhysicalAddr);

} // sysutil
