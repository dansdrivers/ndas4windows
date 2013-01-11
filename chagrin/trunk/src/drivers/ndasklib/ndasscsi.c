#include "ndasscsiproc.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__

#define __MODULE__ "ndasscsi"

#if DBG

NDASSCSI_DEBUG_FLAGS NdasScsiDebugLevel	=	0x00000000					|
											NDASSCSI_DEBUG_LURN_ERROR	|
											NDASSCSI_DEBUG_LURN_INFO	;

#endif
