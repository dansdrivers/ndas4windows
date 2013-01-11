/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"


extern NDEMU_INTERFACE NdemuAtaDiskInterface;
extern NDEMU_INTERFACE NdemuTMDiskInterface;

PNDEMU_INTERFACE NdemuInterface[NDEMU_MAX_INTERFACE] = {
					&NdemuAtaDiskInterface,
					&NdemuTMDiskInterface};


