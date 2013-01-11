////////////////////////////////////////////////////////////////////////////
//
// Utility classes and functions
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasutil.h"

WTL::CString AddrToString(const BYTE *pbNode)
{
	WTL::CString strNode;
	strNode.Format( _T("%02X:%02X:%02X:%02X:%02X:%02X"), 
		pbNode[0], 
		pbNode[1],
		pbNode[2], 
		pbNode[3],
		pbNode[4], 
		pbNode[5]
		);
		return strNode;
}

