////////////////////////////////////////////////////////////////////////////
//
// Utility classes and functions
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ndasutil.h"
#include "nbdefine.h"

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

_int64 CalcUserSectorCount(_int64 nTotalSectorCount)
{
	_int64 nSize;
	nSize = nTotalSectorCount-X_AREA_SIZE;	// Subtracts area reserved
	nSize = nSize - (nSize%CalcSectorPerBit(nSize));

	return nSize;
}

UINT CalcSectorPerBit(_int64 nUserSectorCount)
{
	int nMinSectorPerBit =  static_cast<int>( (nUserSectorCount)/MAX_BITS_IN_BITMAP );
	int nSectorPerBit;
	if ( nMinSectorPerBit < 128 )
	{
		return 128;
	}
	else
	{
		nSectorPerBit = 256;
		while (1)
		{
			ATLASSERT( nSectorPerBit < 1024*1024 ); // In case for bug.
			if ( nMinSectorPerBit < nSectorPerBit )
				return nSectorPerBit;
			nSectorPerBit *= 2;
		}
	}
}