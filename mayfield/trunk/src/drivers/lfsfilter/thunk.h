#ifndef __LFS_THUNKER_H__
#define __LFS_THUNKER_H__


#include "top-down-splay.h"

//
//	Thunking context for 32bit thunking.
//

typedef struct _PTHUNKER32 {


	//
	//	Free ID management
	//

	FAST_MUTEX	FreeIdMutex;
	XCTREE_CTX	SplayTreeCtx;
	PXCTREE		SplayTreeRoot;
	UINT32		NextFreeId;

} THUNKER32, *PTHUNKER32;


#endif