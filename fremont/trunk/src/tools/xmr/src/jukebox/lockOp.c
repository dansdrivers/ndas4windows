#include <stdio.h>
#include <stdlib.h>

#include "../inc/LanScsiOp.h"
#include "../inc/MediaDisk.h"
#include "../inc/diskmgr.h"




int debuglevelLockOp = 1;

#define DEBUGLOCKCALL( _l_, _x_ )		\
		do{								\
			if(_l_ < debuglevelLockOp)	\
				printf _x_;				\
		}	while(0)					\

/********************************************************************

	Disc Lock Operation

*********************************************************************/
int GetLock( struct nd_diskmgr_struct * diskmgr, int lock)
{
	int ndStatus;
	unsigned int *tempParam;
	unsigned _int64 Parameter;
	unsigned int retry_count = 0;
DEBUGLOCKCALL(2,("GetLock Enter %p\n", diskmgr));
	
	tempParam = (unsigned int *)&Parameter;
	tempParam[0] = htonl(DISK_LOCK);
	tempParam[1] = 0;

	ndStatus = VenderCommand(&diskmgr->Path, VENDER_OP_SET_SEMA, &Parameter);

	if(ndStatus < 0)
	{
		DEBUGLOCKCALL(2,("Error get DISK LOCK sleep retry\n"));
		return 0;
	}

	
DEBUGLOCKCALL(2,("---> GetLock Leave %p\n", diskmgr));
	return 1;
}

int ReleaseLock( struct nd_diskmgr_struct * diskmgr, int lock)
{
	int ndStatus;
	unsigned int *tempParam;
	unsigned _int64 Parameter;

DEBUGLOCKCALL(2,("ReleasLock Enter %p\n", diskmgr));


	
	tempParam = (unsigned int *)&Parameter;
	tempParam[0] = htonl(DISK_LOCK);
	tempParam[1] = 0;

	ndStatus = VenderCommand(&diskmgr->Path, VENDER_OP_FREE_SEMA, &Parameter);

	if(ndStatus < 0)
	{
		DEBUGLOCKCALL(2,("Error free DISK LOCK\n"));
			
	}
	
DEBUGLOCKCALL(2,("---> ReleaseLock Leave %p\n", diskmgr));
	return 1;
}