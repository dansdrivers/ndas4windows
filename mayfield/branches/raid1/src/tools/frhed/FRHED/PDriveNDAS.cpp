#include "precomp.h"
#include "physicaldrive.h"
#include <assert.h>

#include "PDriveNDAS.h"

#define MEDIA_TYPE_UNKNOWN_DEVICE		0		// Unknown(not supported)
#define MEDIA_TYPE_BLOCK_DEVICE			1		// Non-packet mass-storage device (HDD)
#define MEDIA_TYPE_COMPACT_BLOCK_DEVICE 2		// Non-packet compact storage device (Flash card)
#define MEDIA_TYPE_CDROM_DEVICE			3		// CD-ROM device (CD/DVD)
#define MEDIA_TYPE_OPMEM_DEVICE			4		// Optical memory device (MO)

BOOL PNDASPhysicalDrive::GetDriveLayout( LPBYTE lpbMemory, DWORD dwSize )
{
	if(!IsOpen())
		return FALSE;

	BOOL bResults = NdasCommGetUnitDeviceDynInfo(&m_ConnectionInfo, &m_UnitDynInfo);
	if(!bResults)
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
		(m_UnitInfo.MediaType == MEDIA_TYPE_UNKNOWN_DEVICE) ? "Unknown device" :
		(m_UnitInfo.MediaType == MEDIA_TYPE_BLOCK_DEVICE) ? "Non-packet mass-storage device (HDD)" :
		(m_UnitInfo.MediaType == MEDIA_TYPE_COMPACT_BLOCK_DEVICE) ? "Non-packet compact storage device (Flash card)" :
		(m_UnitInfo.MediaType == MEDIA_TYPE_CDROM_DEVICE) ? "CD-ROM device (CD/DVD)" :
		(m_UnitInfo.MediaType == MEDIA_TYPE_OPMEM_DEVICE) ? "Optical memory device (MO)" :
		"Unknown device",
		(m_UnitDynInfo.bPresent) ? "YES" : "NO",
		m_UnitDynInfo.NRRWHost,
		m_UnitDynInfo.NRROHost);


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

BOOL PNDASPhysicalDrive::Open( int iDrive )
{
	CHAR errMsg[200];

	BOOL bResults;
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

	CopyMemory(&m_ConnectionInfo, (PNDAS_CONNECTION_INFO)OpenParameter[1], sizeof(m_ConnectionInfo));

	m_hNDAS = NdasCommConnect(&m_ConnectionInfo);
	if(!m_hNDAS)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	bResults = NdasCommGetDeviceInfo(m_hNDAS, &m_Info);
	if(!bResults)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	bResults = NdasCommGetUnitDeviceInfo(m_hNDAS, &m_UnitInfo);
	if(!bResults)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
		return FALSE;
	}

	bResults = NdasCommGetUnitDeviceDynInfo(&m_ConnectionInfo, &m_UnitDynInfo);
	if(!bResults)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
		return FALSE;
	}

#define MEDIA_TYPE_BLOCK_DEVICE 1

	if(MEDIA_TYPE_BLOCK_DEVICE != m_UnitInfo.MediaType)
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

	*ppPI = &m_PI;

	return TRUE;
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
	CHAR errMsg[200];
	bResults = NdasCommBlockDeviceRead(m_hNDAS, Sector, 1, (char *)lpbMemory);
	if(!bResults)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
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
	CHAR errMsg[200];
	bResults = NdasCommBlockDeviceWriteSafeBuffer(m_hNDAS, Sector, 1, (char *)lpbMemory);
	if(!bResults)
	{
		sprintf(errMsg, "Error %s %d %08x", __FILE__, __LINE__, ::GetLastError());
		MessageBox(NULL, errMsg, "Open NDAS.", MB_OK | MB_ICONERROR);
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
	ZeroMemory(&m_UnitDynInfo, sizeof(m_UnitDynInfo));

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
