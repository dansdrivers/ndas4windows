#include <pch.cxx>

#define _NTAPI_ULIB_
#define _UFAT_MEMBER_

#include "ulib.hxx"
#include "uxixfs.hxx"
#include "error.hxx"
#include "xixfs.hxx"


DEFINE_CONSTRUCTOR( XIXFS, SECRUN );

XIXFS::~XIXFS()
{
    Destroy();
}

VOID
XIXFS::Construct ()
{
	(void)(this);
}


VOID
XIXFS::Destroy()
{

}


BOOLEAN
XIXFS::Initialize(
      IN OUT  PMEM                Mem,
      IN OUT  PLOG_IO_DP_DRIVE    Drive,
      IN      LBN                 StartSector,
      IN      ULONG               NumOfEntries,
      IN      ULONG               NumSectors
      )
{
	return FALSE;
}



BOOLEAN
XIXFS::Initialize(
      IN OUT  PSECRUN             Srun,
      IN OUT  PMEM                Mem,
      IN OUT  PLOG_IO_DP_DRIVE    Drive,
      IN      LBN                 StartSector,
      IN      ULONG               NumOfEntries,
      IN      ULONG               NumSectors
      )
{
	return FALSE;
}