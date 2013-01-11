#ifndef PDriveNDAS_h
#define PDriveNDAS_h

#include <winsock2.h>
#include "../inc/ndastype.h"
#include "../../../inc/ndas/ndascomm_api.h"
#include "physicaldrive.h"

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
		NDAS_CONNECTION_INFO m_ConnectionInfo;
		NDAS_DEVICE_INFO m_Info;
		NDAS_UNIT_DEVICE_INFO m_UnitInfo;
		NDAS_UNIT_DEVICE_DYN_INFO m_UnitDynInfo;
		BOOL m_bReadOnly;
};

#endif // PDriveNT_h


