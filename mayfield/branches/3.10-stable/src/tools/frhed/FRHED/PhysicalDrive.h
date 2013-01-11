#ifndef PNtPhysicalDrive_h
#define PNtPhysicalDrive_h

#include "ntdiskspec.h"

typedef enum
{
	IS_WINDOWS_NT,
	IS_WINDOWS_2000,
	IS_WINDOWS_XP,
	IS_WINDOWS_95,
	IS_WINDOWS_98,
	IS_WINDOWS_98SE,
	IS_WINDOWS_ME,
	IS_WINDOWS_UNKNOWN,
} WINDOWS_VERSION;

WINDOWS_VERSION RefreshWindowsVersion();

class PartitionInfo : public PNode
{
	public:
		DWORD m_dwDrive;
		DWORD m_dwPartition;
		BOOL m_bIsPartition;
		DWORD m_dwBytesPerSector;
		INT64 m_NumberOfSectors;
		INT64 m_StartingOffset;
		INT64 m_StartingSector;
		INT64 m_PartitionLength;

		PString GetNameAsString();
		PString GetSizeAsString();
};


//This is an abstract class for a physical drive layout
class IPhysicalDrive
{
	public:
		virtual BOOL GetDriveGeometry( DISK_GEOMETRY* lpDG ) = 0;
		virtual BOOL GetDriveGeometryEx( DISK_GEOMETRY_EX* lpDG, DWORD dwSize ) = 0;
		virtual BOOL GetDriveLayout( LPBYTE lpbMemory, DWORD dwSize ) = 0;
		virtual BOOL GetDriveLayoutEx( LPBYTE lpbMemory, DWORD dwSize ) = 0;

		virtual BOOL Open( int iDrive ) = 0;
		virtual void Close() = 0;
		virtual BOOL ReadAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector ) = 0;
		virtual BOOL WriteAbsolute( LPBYTE lpbMemory, DWORD dwSize, INT64 Sector ) = 0;
		virtual BOOL IsOpen() = 0;

		//Creates a list of PartitionInfo elements
		virtual void GetPartitionInfo(PList* lpList);
};

IPhysicalDrive* CreatePhysicalDriveInstance();
IPhysicalDrive* CreateNDASDriveInstance();

#endif // PNtPhysicalDrive_h


