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
	ciDiscovery.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ciDiscovery.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;

	ZeroMemory(&m_udstat, sizeof(m_udstat));
	m_udstat.Size = sizeof(m_udstat);
	if (!NdasCommGetUnitDeviceStat(&ciDiscovery, &m_udstat))
	{
		return FALSE;
	}

	PCHAR lpszDesc = (PCHAR)lpbMemory;

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
		"Number of hosts with RW privilege : %d\n"
		"Number of hosts with RO privilege : %d\n",
		m_dinfo.HardwareType,
		m_dinfo.HardwareVersion,
		m_dinfo.ProtocolType,
		m_dinfo.ProtocolVersion,
		m_dinfo.NumberOfCommandProcessingSlots,
		m_dinfo.MaximumTransferBlocks,
		m_dinfo.MaximumNumberOfTargets,
		m_dinfo.MaximumNumberOfLUs,
		(m_dinfo.HeaderEncryptionMode) ? "YES" : "NO",
		(m_dinfo.DataEncryptionMode) ? "YES" : "NO",
		m_udinfo.SectorCount.QuadPart,
		(m_udinfo.LBA) ? "YES" : "NO",
		(m_udinfo.LBA48) ? "YES" : "NO",
		(m_udinfo.PIO) ? "YES" : "NO",
		(m_udinfo.DMA) ? "YES" : "NO",
		(m_udinfo.UDMA) ? "YES" : "NO",
		m_udinfo.Model,
		m_udinfo.FirmwareRevision,
		m_udinfo.SerialNumber,
		(m_udinfo.MediaType == MEDIAYPE_UNKNOWN_DEVICE) ? "Unknown device" :
		(m_udinfo.MediaType == MEDIAYPE_BLOCK_DEVICE) ? "Non-packet mass-storage device (HDD)" :
		(m_udinfo.MediaType == MEDIAYPE_COMPACT_BLOCK_DEVICE) ? "Non-packet compact storage device (Flash card)" :
		(m_udinfo.MediaType == MEDIAYPE_CDROM_DEVICE) ? "CD-ROM device (CD/DVD)" :
		(m_udinfo.MediaType == MEDIAYPE_OPMEM_DEVICE) ? "Optical memory device (MO)" :
		"Unknown device",
		(m_udstat.IsPresent) ? "YES" : "NO",
		m_udstat.RwHostCount,
		m_udstat.RoHostCount);


	return TRUE;
} // GetDriveLayout()

BOOL PNDASPhysicalDrive::GetDriveLayoutEx( LPBYTE lpbMemory, DWORD dwSize )
{
	return TRUE;
} // GetDriveLayout()

BOOL PNDASPhysicalDrive::GetDriveGeometry( DISK_GEOMETRY* lpDG )
{
	Zero(*lpDG);

	lpDG->Cylinders.QuadPart = m_udinfo.SectorCount.QuadPart;
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
	BYTE UnitNo;
	BYTE vid;
	if(!NdasCommGetDeviceID(m_hNDAS, NULL, (PBYTE)&DeviceId, &UnitNo, &vid))
	{
		return;
	}

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

	if(!(m_hNDAS = NdasCommConnect(&m_ConnectionInfo)))
		goto fail;

	m_bReadOnly = (m_ConnectionInfo.WriteAccess) ? FALSE : TRUE;

	ZeroMemory(&m_dinfo, sizeof(NDAS_DEVICE_HARDWARE_INFO));
	m_dinfo.Size = sizeof(NDAS_DEVICE_HARDWARE_INFO);
	
	if (!NdasCommGetDeviceHardwareInfo(m_hNDAS, &m_dinfo))
	{
		goto fail;
	}

	ZeroMemory(&m_udinfo, sizeof(NDAS_UNITDEVICE_HARDWARE_INFO));
	m_udinfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);

	if (!NdasCommGetUnitDeviceHardwareInfo(m_hNDAS, &m_udinfo))
	{
		goto fail;
	}

	NDASCOMM_CONNECTION_INFO ciDiscovery;

	CopyMemory(&ciDiscovery, &m_ConnectionInfo, sizeof(NDASCOMM_CONNECTION_INFO));
	ciDiscovery.LoginType = NDASCOMM_LOGIN_TYPE_DISCOVER;

	ZeroMemory(&m_udstat, sizeof(NDAS_UNITDEVICE_STAT));
	m_udstat.Size = sizeof(NDAS_UNITDEVICE_STAT);

	if(!NdasCommGetUnitDeviceStat(&ciDiscovery, &m_udstat))
	{
		goto fail;
	}

#define MEDIAYPE_BLOCK_DEVICE 1

	if(MEDIAYPE_BLOCK_DEVICE != m_udinfo.MediaType)
	{
		MessageBox(NULL, "Packet device not supported.", "Open NDAS", MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	m_BytesPerSector = 512;

	m_PI.m_dwDrive = 0xff;
	m_PI.m_dwPartition = 0xff;
	m_PI.m_bIsPartition = TRUE;
	m_PI.m_dwBytesPerSector = (unsigned long)m_BytesPerSector;
	m_PI.m_NumberOfSectors = m_udinfo.SectorCount.QuadPart;
	m_PI.m_StartingOffset = 0;
	m_PI.m_StartingSector = 0;
	m_PI.m_PartitionLength = m_udinfo.SectorCount.QuadPart * m_BytesPerSector;

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
	ZeroMemory(&m_dinfo, sizeof(m_dinfo));
	ZeroMemory(&m_udinfo, sizeof(m_udinfo));
	ZeroMemory(&m_udstat, sizeof(m_udstat));

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
