#ifndef __FAST_IO_DISPATCH_H__
#define __FAST_IO_DISPATCH_H__


extern PFAST_IO_DISPATCH	LfsFastIoDispatch;


typedef struct _LFS_FAST_IO_RESULT
{
	union
	{
		BOOLEAN	Result;
		struct 
		{
			BOOLEAN	LfsResult:4;
			BOOLEAN	PastIoResult:4;
		};
	};

}LFS_FAST_IO_RESULT, *PLFS_FAST_IO_RESULT;


VOID
InitializeFastIoDispatch(
	VOID
	);

#endif