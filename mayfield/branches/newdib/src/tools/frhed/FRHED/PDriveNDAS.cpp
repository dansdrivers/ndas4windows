#include "precomp.h"
#include "physicaldrive.h"
#include <assert.h>

#include "PDriveNDAS.h"

#define MEDIAYPE_UNKNOWN_DEVICE		0		// Unknown(not supported)
#define MEDIAYPE_BLOCK_DEVICE			1		// Non-packet mass-storage device (HDD)
#define MEDIAYPE_COMPACT_BLOCK_DEVICE 2		// Non-packet compact storage device (Flash card)
#define MEDIAYPE_CDROM_DEVICE			3		// CD-ROM device (CD/DVD)
#define MEDIAYPE_OPMEM_DEVICE			4		// Optical memory device (MO)

static
VOID
GetDescription(LPSTR lpszDescription, DWORD dwError)
{
	BOOL fSuccess = FALSE;
	if (dwError & APPLICATION_ERROR_MASK) {

		HMODULE hModule = ::LoadLibraryEx(
			("ndasmsg.dll"), 
			NULL, 
			LOAD_LIBRARY_AS_DATAFILE);

		LPTSTR lpszErrorMessage = NULL;

		if (NULL != hModule) {

			INT iChars = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}

		//
		// Facility: NDAS 0x%1!04X!\r\n
		// Error Code: %2!u! (0x%2!04X!)\r\n
		// %3!s!			
		//

		sprintf(lpszDescription,
			"Facility: NDAS 0x%04X\r\n"
			"Error Code: 0x%04X\r\n"
			"%s",
			(dwError & 0x0FFF000) >> 12,
			(dwError & 0xFFFF),
			(lpszErrorMessage) ? lpszErrorMessage : (""));

		if (NULL != lpszErrorMessage) {
			::LocalFree(lpszErrorMessage);
		}

		if (NULL != hModule) {
			::FreeLibrary(hModule);
		}


	} else {

		LPTSTR lpszErrorMessage = NULL;
		INT iChars = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, 
			dwError, 
			0, 
			(LPTSTR) &lpszErrorMessage, 
			0, 
			NULL);

		//
		// Error Code: %1!u! (0x%1!04X!)
		// %2!s!
		//
		sprintf(lpszDescription,
			"Error Code: %08X\r\n"
			"%s",
			dwError,
			(lpszErrorMessage) ? lpszErrorMessage : (""));

		if (NULL != lpszErrorMessage) {
			::LocalFree(lpszErrorMessage);
		}
	}
}

BOOL PNDASPhysicalDrive::GetDriveLayout( LPBYTE lpbMemory, DWORD dwSize )
{
	if(!IsOpen())
		return FALSE;

	NDASCOMM_CONNECTION_INFO ciDiscovery;

	CopyMemory(&ciDiscovery, &m_ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ciDiscovery.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;

	if(!NdasCommGetUnitDeviceStat(&ciDiscovery, &m_UnitStat, 0, NULL))
		return FALSE;

	PCHAR lpszDesc = (PCHAR)lpbMemory;
	CHAR bufferModel[100];
	CHAR bufferFwRev[100];
	CHAR bufferSerialNo[100];

	memcpy(bufferModel, (const char *)m_UnitInfo.Model, sizeof(m_UnitInfo.Model)); bufferModel[sizeof(m_UnitInfo.Model)] = '\0';
	memcpy(bufferFwRev, (const char *)m_UnitInfo.FwRev, sizeof(m_UnitInfo.FwRev)); bufferFwRev[sizeof(m_UnitInfo.FwRev)] = '\0';
	memcpy(bufferSerialNo, (const char *)m_UnitInfo.SerialNo, sizeof(m_UnitInfo.SerialNo)); bufferSerialNo[sizeof(m_UnitInfo.SerialNo)] = '\0';

	sprintf(
		lpszDesc,
		"Hardware Type : %d\n"
		"Hardware Version : %d\n"
		"Hardware Protocol Type : %d\n"
		"Hardware Protocol Version : %d\n"
		"Number of slot : %d\n"
		"Maximum transfer blocks : %d\n"
		"Maximum targets : %d\n"
		"Maximum LUs : %d\n"
		"Header Encryption : %s\n"
		"Data Encryption : %s\n"
		"\n"
		"Sector count : %I64d\n"
		"Supports LBA : %s\n"
		"Supports LBA48 : %s\n"
		"Supports PIO : %s\n"
		"Supports DMA : %s\n"
		"Supports UDMA : %s\n"
		"Model : %s\n"
		"Firmware Rev : %s\n"
		"Serial number : %s\n"
		"Media type : %s\n"
		"\n"
		"Present : %s\n"
		"Number of hosts with RW priviliage : %d\n"
		"Number of hosts with RO priviliage : %d\n",
		m_Info.HWType,
		m_Info.HWVersion,
		m_Info.HWProtoType,
		m_Info.HWProtoVersion,
		m_Info.iNumberofSlot,
		m_Info.iMaxBlocks,
		m_Info.iMaxTargets,
		m_Info.iMaxLUs,
		(m_Info.iHeaderEncryptAlgo) ? "YES" : "NO",
		(m_Info.iDataEncryptAlgo) ? "YES" : "NO",
		m_UnitInfo.SectorCount,
		(m_UnitInfo.bLBA) ? "YES" : "NO",
		(m_UnitInfo.bLBA48) ? "YES" : "NO",
		(m_UnitInfo.bPIO) ? "YES" : "NO",
		(m_UnitInfo.bDma) ? "YES" : "NO",
		(m_UnitInfo.bUDma) ? "YES" : "NO",
		bufferModel,
		bufferFwRev,
		bufferSerialNo,
		(m_UnitInfo.MediaType == MEDIAYPE_UNKNOWN_DEVICE) ? "Unknown device" :
		(m_UnitInfo.MediaType == MEDIAYPE_BLOCK_DEVICE) ? "Non-packet mass-storage device (HDD)" :
		(m_UnitInfo.MediaType == MEDIAYPE_COMPACT_BLOCK_DEVICE) ? "Non-packet compact storage device (Flash card)" :
		(m_UnitInfo.MediaType == MEDIAYPE_CDROM_DEVICE) ? "CD-ROM device (CD/DVD)" :
		(m_UnitInfo.MediaType == MEDIAYPE_OPMEM_DEVICE) ? "Optical memory device (MO)" :
		"Unknown device",
		(m_UnitStat.bPresent) ? "YES" : "NO",
		m_UnitStat.NRRWHost,
		m_UnitStat.NRROHost);


	return TRUE;
} // GetDriveLayout()

BOOL PNDASPhysicalDrive::GetDriveLayoutEx( LPBYTE lpbMemory, DWORD dwSize )
{
	return TRUE;
} // GetDriveLayout()

BOOL PNDASPhysicalDrive::GetDriveGeometry( DISK_GEOMETRY* lpDG )
{
	Zero(*lpDG);

	lpDG->Cylinders.QuadPart = m_UnitInfo.SectorCount;
	lpDG->MediaType = FixedMedia;
	lpDG->TracksPerCylinder = 1;
	lpDG->SectorsPerTrack = 1;
	lpDG->BytesPerSector = (unsigned long)m_BytesPerSector;
	return TRUE;
} // GetDriveGeometry()

// does not support
BOOL PNDASPhysicalDrive::GetDriveGeometryEx( DISK_GEOMETRY_EX* lpDG, DWORD dwSize )
{
	return FALSE;
} // GetDriveGeometry()

// does not support
void PNDASPhysicalDrive::GetPartitionInfo(PList* lpList)
{
	return;
}

void PNDASPhysicalDrive::GetAddressAsString(LPTSTR lpszAddress)
{
	BYTE DeviceId[6];
	DWORD UnitNo;
	if(!NdasCommGetDeviceID(m_hNDAS, (PBYTE)&DeviceId,	&UnitNo))
		return;

	sprintf(lpszAddress, "%02X:%02X:%02X:%02X:%02X:%02X-%d",
		DeviceId[0], DeviceId[1], DeviceId[2], DeviceId[3], DeviceId[4], DeviceId[5], UnitNo);
}

BOOL PNDASPhysicalDrive::Open( int iDrive )
{
	if(!m_bInitialized)
	{
		MessageBox(NULL, "API not initialized", "Open NDAS.", MB_OK | MB_ICONERROR);
		return FALSE;
	}
	
	char **OpenParameter = (char **)iDrive;

	if(NULL == OpenParameter)
		return FALSE;

	if(NULL == OpenParameter[0])
		return FALSE;
	
	PartitionInfo **ppPI = (PartitionInfo **)OpenParameter[0];

	if(NULL == OpenParameter[1])
		return FALSE;

	CopyMemory(&m_ConnectionInfo, (PNDASCOMM_CONNECTION_INFO)OpenParameter[1], sizeof(m_ConnectionInfo));

	if(!(m_hNDAS = NdasCommConnect(&m_ConnectionInfo, 0, NULL)))
		goto fail;

	m_bReadOnly = (m_ConnectionInfo.bWriteAccess) ? FALSE : TRUE;

	if(
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_type, &m_Info.HWType, sizeof(m_Info.HWType)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_version, &m_Info.HWVersion, sizeof(m_Info.HWVersion)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_proto_type, &m_Info.HWProtoType, sizeof(m_Info.HWProtoType)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_proto_version, &m_Info.HWProtoVersion, sizeof(m_Info.HWProtoVersion)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_num_slot, (PBYTE)&m_Info.iNumberofSlot, sizeof(m_Info.iNumberofSlot)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_max_blocks, (PBYTE)&m_Info.iMaxBlocks, sizeof(m_Info.iMaxBlocks)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_max_targets, (PBYTE)&m_Info.iMaxTargets, sizeof(m_Info.iMaxTargets)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_max_lus, (PBYTE)&m_Info.iMaxLUs, sizeof(m_Info.iMaxLUs)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_header_encrypt_algo, (PBYTE)&m_Info.iHeaderEncryptAlgo, sizeof(m_Info.iHeaderEncryptAlgo)) ||
		!NdasCommGetDeviceInfo(m_hNDAS, ndascomm_handle_info_hw_data_encrypt_algo, (PBYTE)&m_Info.iDataEncryptAlgo, sizeof(m_Info.iDataEncryptAlgo))
		)
		goto fail;

	if(!NdasCommGetUnitDeviceInfo(m_hNDAS, &m_UnitInfo))
		goto fail;

	NDASCOMM_CONNECTION_INFO ciDiscovery;

	CopyMemory(&ciDiscovery, &m_ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ciDiscovery.login_type = NDASCOMM_LOGIN_TYPE_DISCOVER;

	if(!NdasCommGetUnitDeviceStat(&ciDiscovery, &m_UnitStat, 0, NULL))
		goto fail;

#define MEDIAYPE_BLOCK_DEVICE 1

	if(MEDIAYPE_BLOCK_DEVICE != m_UnitInfo.MediaType)
	{
		MessageBox(NULL, "Packet device not supported.", "Open NDAS", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	m_BytesPerSector = 512;

	m_PI.m_dwDrive = 0xff;
	m_PI.m_dwPartition = 0xff;
	m_PI.m_bIsPartition = TRUE;
	m_PI.m_dwBytesPerSector = (unsigned long)m_BytesPerSector;
	m_PI.m_NumberOfSectors = m_UnitInfo.SectorCount;
	m_PI.m_StartingOffset = 0;
	m_PI.m_StartingSector = 0;
	m_PI.m_PartitionLength = m_UnitInfo.SectorCount * m_BytesPerSector;

	GetAddressAsString(m_PI.m_szInfo);

	*ppPI = &m_PI;

	return TRUE;

fail:
	CHAR szDescription[1024];
	GetDescription(szDescription, ::GetLastError());
	MessageBox(NULL, szDescription, "PNDASPhysicalDrive::Open()", MB_OK | MB_ICONERROR);
	return FALSE;
}

void PNDASPhysicalDrive::Close()
{
	if(m_hNDAS)
	{
		NdasCommDisconnect(m_hNDAS);
	}
	m_hNDAS = NULL;
}

BOOL PNDASPhysicalDrive::ReadAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector )
{
	if(dwSize != m_BytesPerSector)
		return FALSE;

	BOOL bResults;
	bResults = NdasCommBlockDeviceRead(m_hNDAS, Sector, 1, lpbMemory);
	if(!bResults)
	{
		CHAR szDescription[1024];
		GetDescription(szDescription, ::GetLastError());
		MessageBox(NULL, szDescription, "PNDASPhysicalDrive::ReadAbsolute()", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	return TRUE;
} // ReadAbsolute()

BOOL PNDASPhysicalDrive::WriteAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector )
{
	if(m_bReadOnly)
	{
		MessageBox(NULL, "Read only mode.", "NDAS Writing sector", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	if(dwSize != m_BytesPerSector)
		return FALSE;

	BOOL bResults;
	bResults = NdasCommBlockDeviceWriteSafeBuffer(m_hNDAS, Sector, 1, lpbMemory);
	if(!bResults)
	{
		CHAR szDescription[1024];
		GetDescription(szDescription, ::GetLastError());
		MessageBox(NULL, szDescription, "PNDASPhysicalDrive::WriteAbsolute()", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	return TRUE;
} // WriteAbsolute()

PNDASPhysicalDrive::PNDASPhysicalDrive()
{
	m_bInitialized = NULL;
	m_hNDAS = NULL;
	ZeroMemory(&m_Info, sizeof(m_Info));
	ZeroMemory(&m_UnitInfo, sizeof(m_UnitInfo));
	ZeroMemory(&m_UnitStat, sizeof(m_UnitStat));

	BOOL bResults = NdasCommInitialize();	
	m_bInitialized = bResults;
} // PNDASPhysicalDrive()

PNDASPhysicalDrive::~PNDASPhysicalDrive()
{
} // ~PNDASPhysicalDrive()

BOOL PNDASPhysicalDrive::IsOpen()
{
	return m_hNDAS != NULL;
}
