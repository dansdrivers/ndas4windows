#ifndef PDriveNDAS_h
#define PDriveNDAS_h

#include <ndas/ndascomm.h>
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
		NDASCOMM_CONNECTION_INFO m_ConnectionInfo;
		NDAS_DEVICE_HARDWARE_INFO m_dinfo;
		NDAS_UNITDEVICE_HARDWARE_INFO m_udinfo;
		NDAS_UNITDEVICE_STAT m_udstat;
		BOOL m_bReadOnly;

		void GetAddressAsString(LPTSTR lpszAddress);
};

#endif // PDriveNT_h


