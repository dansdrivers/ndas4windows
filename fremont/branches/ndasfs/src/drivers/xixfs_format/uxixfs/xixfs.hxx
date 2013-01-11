#ifndef XIXFS_DEFN
#define XIXFS_DEFN

#include "hmem.hxx"
#include "secrun.hxx"
#include "drive.hxx"
#include "message.hxx"

#if defined ( _AUTOCHECK_ )
#define UXIXFS_EXPORT
#elif defined ( _UXIXFS_MEMBER_ )
#define UXIXFS_EXPORT    __declspec(dllexport)
#else
#define UXIXFS_EXPORT    __declspec(dllimport)
#endif

DECLARE_CLASS(	SECRUN	);
DECLARE_CLASS(	MEM	);
DECLARE_CLASS(	LOG_IO_DP_DRIVE	);
DECLARE_CLASS(	XIXFS	);


class XIXFS : public SECRUN {

	public:

		DECLARE_CONSTRUCTOR(XIXFS);

		VIRTUAL
		~XIXFS(
			);

		NONVIRTUAL
		BOOLEAN
		Initialize(
				  IN OUT  PMEM                Mem,
				  IN OUT  PLOG_IO_DP_DRIVE    Drive,
				  IN      LBN                 StartSector,
				  IN      ULONG               NumOfEntries,
				  IN      ULONG               NumSectors  DEFAULT 0
				  );

		NONVIRTUAL
		BOOLEAN
		Initialize(
				  IN OUT  PSECRUN             Srun,
				  IN OUT  PMEM                Mem,
				  IN OUT  PLOG_IO_DP_DRIVE    Drive,
				  IN      LBN                 StartSector,
				  IN      ULONG               NumOfEntries,
				  IN      ULONG               NumSectors  DEFAULT 0
				  );




	private:

		NONVIRTUAL
		VOID
		Construct();

		NONVIRTUAL
		VOID
		Destroy();

		LARGE_INTEGER	_num_lot_;
		ULONG			_bytes_per_lot_;
		LARGE_INTEGER	_num_sec_;
		ULONG			_bytes_per_sec;
  
};


#endif //#ifndef XIXFS_DEFN