#ifndef __TABLE_H__
#define __TABLE_H__


typedef struct _LFS_TABLE
{
	KSPIN_LOCK			SpinLock;
    LONG				ReferenceCount;

} LFS_TABLE, *PLFS_TABLE;


#endif