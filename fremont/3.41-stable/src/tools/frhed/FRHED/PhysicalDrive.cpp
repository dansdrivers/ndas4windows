#include "precomp.h"
#include "physicaldrive.h"
#include <assert.h>
#include "pdrive95.h"
#include "pdrivent.h"
#include "pdriveNDAS.h"

WINDOWS_VERSION RefreshWindowsVersion()
{
	OSVERSIONINFOEX osvi;
	BOOL bOsVersionInfoEx;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	if( !(bOsVersionInfoEx = GetVersionEx( (OSVERSIONINFO*) &osvi)) )
	{
		osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
		if( !GetVersionEx( (OSVERSIONINFO*) &osvi) )
			return IS_WINDOWS_UNKNOWN;
	}
	switch (osvi.dwPlatformId)
	{
	case VER_PLATFORM_WIN32_NT:
		if( osvi.dwMajorVersion <= 4 )
			return IS_WINDOWS_NT;

		if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
			return IS_WINDOWS_2000;

		if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
			return IS_WINDOWS_XP;

		return IS_WINDOWS_NT;

	case VER_PLATFORM_WIN32_WINDOWS:
		if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
			return IS_WINDOWS_95;

		if(osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
		{
			if( osvi.szCSDVersion[1] == 'A' )
				return IS_WINDOWS_98SE;
			return IS_WINDOWS_98;
		}
		if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
			return IS_WINDOWS_ME;

		return IS_WINDOWS_95;
	}
	return IS_WINDOWS_UNKNOWN;
}

static WINDOWS_VERSION wv;

IPhysicalDrive* CreatePhysicalDriveInstance()
{
	wv = RefreshWindowsVersion();
	if( (wv == IS_WINDOWS_NT) || (wv == IS_WINDOWS_2000) || (wv == IS_WINDOWS_XP) )
	{
		return new PNtPhysicalDrive;
	}
	else
	{
		return new P9xPhysicalDrive;
	}
}

IPhysicalDrive* CreateNDASDriveInstance()
{
	return new PNDASPhysicalDrive;
}

PString PartitionInfo::GetNameAsString()
{
	CHAR szFormat[80];

	if(strlen(m_szInfo))
		sprintf( szFormat, "%s, (%s)", m_szInfo, (LPCSTR) GetSizeAsString());
	else if( m_bIsPartition )
		sprintf( szFormat, "Drive %d, Partition %d (%s)", (m_dwDrive+1), (m_dwPartition+1), (LPCSTR) GetSizeAsString() );
	else
		sprintf( szFormat, "Drive %d (%s)", (m_dwDrive+1), (LPCSTR) GetSizeAsString() );
	return PString(szFormat);
}

PString PartitionInfo::GetSizeAsString()
{
	double SizeInMB = (double) m_PartitionLength;
	SizeInMB /= (1000*1000);
	double SizeInGB = SizeInMB;
	SizeInGB /= (1000);
	if( SizeInGB > 1.0 )
		return PString( 0, "%.2f GB", SizeInGB );
	return PString( 0, "%.2f MB", SizeInMB );
}

void IPhysicalDrive::GetPartitionInfo(PList* lpList)
{
	lpList->DeleteContents();

	BYTE bLayoutInfo[20240];
	DISK_GEOMETRY dg;

	for( int iDrive = 0; iDrive < 8; iDrive++ )
	{
		if( !Open(iDrive) )
			continue;

		if( GetDriveGeometryEx( (DISK_GEOMETRY_EX*) bLayoutInfo, sizeof(bLayoutInfo) ) )
		{
			DISK_GEOMETRY& dgref = (((DISK_GEOMETRY_EX*)bLayoutInfo)->Geometry);
			dg = dgref;
			PartitionInfo* p = new PartitionInfo();
			p->m_dwDrive = (DWORD) iDrive;
			p->m_dwPartition = 0;
			p->m_bIsPartition = TRUE;
			p->m_dwBytesPerSector = dg.BytesPerSector;
			p->m_NumberOfSectors = dg.Cylinders.QuadPart;
			p->m_NumberOfSectors *= dg.SectorsPerTrack;
			p->m_NumberOfSectors *= dg.TracksPerCylinder;
			p->m_StartingOffset = 0;
			p->m_StartingSector = 0;
			p->m_PartitionLength = p->m_NumberOfSectors;
			p->m_PartitionLength *= dg.BytesPerSector;

			lpList->AddTail(p);
			if( GetDriveLayoutEx( bLayoutInfo, sizeof(bLayoutInfo) ) )
			{
				PDRIVE_LAYOUT_INFORMATION_EX pLI = (PDRIVE_LAYOUT_INFORMATION_EX)bLayoutInfo;
				for( DWORD iPartition = 0; iPartition < pLI->PartitionCount; iPartition++ )
				{
					PARTITION_INFORMATION_EX* pi = &(pLI->PartitionEntry[iPartition]);

					PartitionInfo* p = new PartitionInfo();
					p->m_dwDrive = (DWORD) iDrive;
					p->m_dwPartition = (DWORD) iPartition;
					p->m_bIsPartition = TRUE;
					p->m_dwBytesPerSector = dg.BytesPerSector;
					p->m_NumberOfSectors = pi->PartitionLength.QuadPart;
					p->m_NumberOfSectors /= dg.BytesPerSector;
					p->m_StartingOffset = pi->StartingOffset.QuadPart;
					p->m_StartingSector = p->m_StartingOffset;
					p->m_StartingSector /= dg.BytesPerSector;
					p->m_PartitionLength = pi->PartitionLength.QuadPart;
					lpList->AddTail(p);
				}
			}
		}
		else
		{
			if( GetDriveGeometry( &dg ) )
			{
				PartitionInfo* p = new PartitionInfo();
				p->m_dwDrive = (DWORD) iDrive;
				p->m_dwPartition = 0;
				p->m_bIsPartition = FALSE;
				p->m_dwBytesPerSector = dg.BytesPerSector;
				p->m_NumberOfSectors = dg.Cylinders.QuadPart;
				p->m_NumberOfSectors *= dg.SectorsPerTrack;
				p->m_NumberOfSectors *= dg.TracksPerCylinder;
				p->m_StartingOffset = 0;
				p->m_StartingSector = 0;
				p->m_PartitionLength = p->m_NumberOfSectors;
				p->m_PartitionLength *= dg.BytesPerSector;

				lpList->AddTail(p);

				if( GetDriveLayout( bLayoutInfo, sizeof(bLayoutInfo) ) )
				{
					PDRIVE_LAYOUT_INFORMATION pLI = (PDRIVE_LAYOUT_INFORMATION)bLayoutInfo;
					for( DWORD iPartition = 0; iPartition < pLI->PartitionCount; iPartition++ )
					{
						PARTITION_INFORMATION* pi = &(pLI->PartitionEntry[iPartition]);

						if( !pi->PartitionLength.QuadPart )
							continue;

						PartitionInfo* p = new PartitionInfo();
						p->m_dwDrive = (DWORD) iDrive;
						p->m_dwPartition = (DWORD) iPartition;
						p->m_bIsPartition = TRUE;
						p->m_dwBytesPerSector = dg.BytesPerSector;
						p->m_NumberOfSectors = pi->PartitionLength.QuadPart;
						p->m_NumberOfSectors /= dg.BytesPerSector;
						p->m_StartingOffset = pi->StartingOffset.QuadPart;
						p->m_StartingSector = p->m_StartingOffset;
						p->m_StartingSector /= dg.BytesPerSector;
						p->m_PartitionLength = pi->PartitionLength.QuadPart;
						lpList->AddTail(p);
					}
				}
			}
		}
		Close();
	}
}
