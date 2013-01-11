/*++

Copyright (C)2002-2004 XIMETA, Inc.
All rights reserved.

--*/

#include "stdafx.h"
#include <lanscsi.h>
#include <winsock2.h>
#include <socketlpx.h>
#include <ndas/ndastype.h>
#include <xtl/xtltrace.h>

#include "lpxcomm.h"

#include "trace.h"
#ifdef RUN_WPP
#include "lpxcomm.tmh"
#endif

BOOL IsEqualLpxAddress(
	const LPX_ADDRESS& lhs, 
	const LPX_ADDRESS& rhs)
{
	return
		lhs.Node[0] == rhs.Node[0] &&
		lhs.Node[1] == rhs.Node[1] &&
		lhs.Node[2] == rhs.Node[2] &&
		lhs.Node[3] == rhs.Node[3] &&
		lhs.Node[4] == rhs.Node[4] &&
		lhs.Node[5] == rhs.Node[5];		
}

BOOL IsEqualLpxAddress(
	const PLPX_ADDRESS lhs, 
	const PLPX_ADDRESS rhs)
{
	return
		lhs->Node[0] == rhs->Node[0] &&
		lhs->Node[1] == rhs->Node[1] &&
		lhs->Node[2] == rhs->Node[2] &&
		lhs->Node[3] == rhs->Node[3] &&
		lhs->Node[4] == rhs->Node[4] &&
		lhs->Node[5] == rhs->Node[5];		
}

VOID
LpxCommConvertLpxAddressToTaLsTransAddress(
	__in CONST LPX_ADDRESS * LpxAddress,
	__out PTA_LSTRANS_ADDRESS TaLsTransAddress)
{
	// Set the remote node address
	TaLsTransAddress->TAAddressCount = 1;
	TaLsTransAddress->Address[0].AddressLength = TDI_ADDRESS_LENGTH_LPX;
	TaLsTransAddress->Address[0].AddressType = AF_LPX;
	CopyMemory(&TaLsTransAddress->Address[0].Address, LpxAddress, sizeof(LPX_ADDRESS));
}
