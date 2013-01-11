#ifndef PDriveNDAS_h
#define PDriveNDAS_h

#include <ndas/ndascomm.h>
#include "physicaldrive.h"

typedef struct _NDAS_DEVICE_INFO
{
	BYTE			HWType;
	BYTE			HWVersion;
	BYTE			HWProtoType;
	BYTE			HWProtoVersion;
	UINT32			iNumberofSlot;
	UINT32			iMaxBlocks;
	UINT32			iMaxTargets;
	UINT32			iMaxLUs;
	UINT16			iHeaderEncryptAlgo;
	UINT16			iDataEncryptAlgo;
} NDAS_DEVICE_INFO, *PNDAS_DEVICE_INFO;

class PNDASPhysicalDrive : public IPhysicalDrive
{
	public:
		PNDASPhysicalDrive();
		virtual ~PNDASPhysicalDrive();

		// path must look like this: "\\.\PhysicalDrive0" (of course, \ maps to \\, and \\ to \\\\)
		BOOL Open( int iDrive );
		void Close();
		BOOL GetDriveGeometry( DISK_GEOMETRY* lpDG );
		BOOL GetDriveGeometryEx( DISK_GEOMETRY_EX* lpDG, DWORD dwSize );
		BOOL GetDriveLayout( LPBYTE lpbMemory, DWORD dwSize );
		BOOL GetDriveLayoutEx( LPBYTE lpbMemory, DWORD dwSize );
		BOOL ReadAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector );
		BOOL WriteAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector );
		BOOL IsOpen();
		void GetPartitionInfo(PList* lpList);

		INT64 m_BytesPerSector;

	private:
		PartitionInfo m_PI;

		// LanscsiBus
		BOOL m_bInitialized;
		HNDAS m_hNDAS;
		NDASCOMM_CONNECTION_INFO m_ConnectionInfo;
		NDAS_DEVICE_INFO m_Info;
		NDASCOMM_UNIT_DEVICE_INFO m_UnitInfo;
		NDASCOMM_UNIT_DEVICE_STAT m_UnitStat;
		BOOL m_bReadOnly;
};

#endif // PDriveNT_h


