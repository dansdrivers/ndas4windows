////////////////////////////////////////////////////////////////////////////
//
// Interface class between ndas object classes and NDAS disks
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <map>
#include <algorithm> // for copy function
#include <vector>
#include "windows.h"
#include "ndasinterface.h"

#include "ndasutil.h"
#include "nbdefine.h"
#include "ndaslanimpl.h"
#include "ndasexception.h"
///////////////////////////////////////////////////////////////////////////////
//
// Implementation of CSession , CDiskSector classes's methods
//
///////////////////////////////////////////////////////////////////////////////
CSession::~CSession()
{
	if ( m_path.iSessionPhase != LOGOUT_PHASE )
	{
		Logout();
	}
	if ( m_path.connsock != INVALID_SOCKET )
	{
		Disconnect();
	}
}

BOOL CSession::IsLoggedIn(BOOL bAsWrite)
{
	if ( bAsWrite )
	{
		return m_bLoggedIn && m_bWrite;
	}
	else
	{
		return m_bLoggedIn;
	}
}
void CSession::Connect(const BYTE *pbNode) 
{
	int						i, iErrorCode;
	SOCKADDR_LPX			socketLpx;
	SOCKADDR_LPX			serverSocketLpx;
	// CHeapMemoryPtr will automatically delete memory when the program
	// leaves scope. See ndasutil.h for details.
	CHeapMemoryPtr<SOCKET_ADDRESS_LIST> pSocketAddrList;
	DWORD					dwSocketAddrListLenth;
	SOCKET					sock;

	::CopyMemory( &m_path.address.Node, pbNode, sizeof(m_path.address.Node) );

	// Initialize
	dwSocketAddrListLenth = FIELD_OFFSET(SOCKET_ADDRESS_LIST, Address)	// size of header
		+ sizeof(SOCKET_ADDRESS) * MAX_SOCKETLPX_INTERFACE
		+ sizeof(SOCKADDR_LPX) * MAX_SOCKETLPX_INTERFACE;

	pSocketAddrList = CHeapMemoryPtr<SOCKET_ADDRESS_LIST>( 
						::HeapAlloc( ::GetProcessHeap(), 
						HEAP_ZERO_MEMORY, 
						dwSocketAddrListLenth )
						);

	// Get NIC(Network interface card) list in the system.
	_GetInterfaceList( pSocketAddrList, dwSocketAddrListLenth );
	// Find NIC that is connected to NetDisk and create socket
	sock = INVALID_SOCKET;
	for ( i=0; i < pSocketAddrList->iAddressCount; i++ )
	{
		socketLpx = * reinterpret_cast<PSOCKADDR_LPX>( 
			pSocketAddrList->Address[i].lpSockaddr );

		sock = ::socket( AF_UNSPEC, SOCK_STREAM, IPPROTO_LPXTCP );
		if ( sock == INVALID_SOCKET )
		{
			// ERROR : Fail to create socket
			NDAS_THROW_EXCEPTION( 
				CNetworkException, 
				CNetworkException::ERROR_NETWORK_FAIL_TO_CREATE_SOCKET
				);
			return;
		}

		socketLpx.LpxAddress.Port = 0;	// Unspecified

		// bind NIC
		iErrorCode = ::bind( sock, 
			reinterpret_cast<struct sockaddr*>(&socketLpx), 
			sizeof(socketLpx)
			);
		if ( iErrorCode == SOCKET_ERROR )
		{
			::closesocket(sock);
			sock = INVALID_SOCKET;
			continue;
		}

		// connect
		::ZeroMemory( &serverSocketLpx, sizeof(serverSocketLpx) );
		serverSocketLpx.sin_family = AF_LPX;
		::CopyMemory( 
			serverSocketLpx.LpxAddress.Node, 
			pbNode, 
			sizeof(serverSocketLpx.LpxAddress.Node)
			);
		serverSocketLpx.LpxAddress.Port = htons( LPX_PORT_NUMBER );

		iErrorCode = ::connect( sock, 
			reinterpret_cast<struct sockaddr*>(&serverSocketLpx),
			sizeof(serverSocketLpx)
			);
		if ( iErrorCode == SOCKET_ERROR ) 
		{
			// TODO : DEBUGMSG : NDAS is not connected to this NIC
			::closesocket(sock);
			sock = INVALID_SOCKET;
			continue;
		}

		break;
	}

	if ( sock == INVALID_SOCKET )
	{
		// ERROR : Fail to find connectable NDAS
		NDAS_THROW_EXCEPTION_EXT( 
			CNetworkException, 
			CNetworkException::ERROR_NETWORK_FAIL_TO_FIND_NDAS,
			pbNode
			);
	}
	else
	{
		m_path.connsock = sock;
	}
}
void CSession::Login(UINT nUnitNumber, BOOL bWrite)
{
	ATLASSERT( m_path.connsock != INVALID_SOCKET );
	int iErrorCode;

	m_nUnitNumber = nUnitNumber;
	// Get information about NDAS device
	m_path.iUserID = ( nUnitNumber + 1 );
	if ( bWrite )
	{
		m_path.iUserID |= ( (nUnitNumber+1) << 16 );
	}
	m_path.iPassword = _GetHWPassword( m_path.address.Node );

	iErrorCode = ::Login( &m_path, LOGIN_TYPE_NORMAL );
	if ( iErrorCode != 0 )
	{
		// ERROR : Fail to login
		NDAS_THROW_EXCEPTION_EXT(
			CNetworkException, 
			CNetworkException::ERROR_NETWORK_FAIL_TO_LOGIN,
			m_path.address.Node
			);
	}

	//
	// NOTE : To send/receive data from a disk.
	//		  The fields in PerTarget of m_path structure should be set 
	//		  to have proper values. Thus, we need to initialize PerTarget here.
	// FIXME : The PerTarget fields data can be cached elsewhere.
	//
	iErrorCode = ::GetDiskInfo( &m_path, nUnitNumber );
	if ( iErrorCode != 0 )
	{
		// ERROR : Fail to get disk information
		NDAS_THROW_EXCEPTION_EXT(
			CNetworkException,
			CNetworkException::ERROR_FAIL_TO_GET_DISKINFO,
			m_path.address.Node
			);
	}

	m_bLoggedIn = TRUE;
	m_bWrite = bWrite;
}
void CSession::Logout()
{
	m_bLoggedIn = FALSE;
	m_bWrite = FALSE;
	::Logout(&m_path);
}
void CSession::Disconnect()
{
	m_bLoggedIn = FALSE;
	m_bWrite = FALSE;

	if ( m_path.connsock != INVALID_SOCKET )
	{
		closesocket( m_path.connsock );
	}
}

void CSession::GetTargetData(const UNIT_DISK_LOCATION *pLocation, PTARGET_DATA pTargetData)
{
	int iErrorCode;
	m_path.iUserID = 0x0001;
	m_path.iPassword = _GetHWPassword( pLocation->MACAddr );
	m_path.iSessionPhase = LOGOUT_PHASE;

	iErrorCode = ::Discovery( &m_path );
	if ( iErrorCode != 0 )
	{
		WTL::CString strError;
		strError.Format( _T("Node:%s, ErrorCode:%d"),
			AddrToString(m_path.address.Node), iErrorCode);
		NDAS_THROW_EXCEPTION_STR(
			CNetworkException, 
			CNetworkException::ERROR_FAIL_TO_GET_DISKINFO,
			strError
			);
	}

	Login();
	iErrorCode = ::GetDiskInfo( &m_path, pLocation->UnitNumber );
	Logout();
	if ( iErrorCode != 0 )
	{
		WTL::CString strError;
		strError.Format( _T("Node:%s, ErrorCode:%d"),
			AddrToString(m_path.address.Node), iErrorCode);
		NDAS_THROW_EXCEPTION_STR(
			CNetworkException, 
			CNetworkException::ERROR_FAIL_TO_GET_DISKINFO,
			strError
			);
	}

	::CopyMemory( 
		pTargetData, 
		&m_path.PerTarget[pLocation->UnitNumber],
		sizeof(TARGET_DATA)
		);
}
void CSession::Write(_int64 iLocation, _int16 nSecCount, _int8 *pbData)
{
	_int16 nWrittenCount = 0;
	CHeapMemoryPtr<_int8>	pbAllocatedBuffer;
	_int8	abBuffer[BLOCK_SIZE];
	_int8  *pbBuffer;

	unsigned _int8	iResponse;
	int iErrorCode;

	if ( iLocation < 0 )
	{
		iLocation  = m_path.PerTarget[m_nUnitNumber].SectorCount + iLocation;
	}
	// We need to copy data to a buffer 
	// since the data in the buffer will changed in the sub-fucntions
	// while encrypting/decrypting data
	if ( nSecCount > 1 )
	{
		pbAllocatedBuffer = CHeapMemoryPtr<_int8>(
								::HeapAlloc( ::GetProcessHeap(), 
											HEAP_ZERO_MEMORY,
											BLOCK_SIZE * nSecCount)
								);
		pbBuffer = pbAllocatedBuffer.get();
	}
	else
	{
		pbBuffer = abBuffer;
	}

	while ( nWrittenCount < nSecCount )
	{
		_int16 nCount = (_int16)std::min( MAX_REQUESTBLOCK, nSecCount-nWrittenCount);
		::CopyMemory( pbBuffer, pbData + nWrittenCount*BLOCK_SIZE, nCount * BLOCK_SIZE );
		iErrorCode = ::IdeCommand(
							&m_path, 
							m_nUnitNumber,
							0,
							WIN_WRITE,
							iLocation + nWrittenCount,
							(_int16)std::min( MAX_REQUESTBLOCK, nSecCount-nWrittenCount ),
							0,
							pbBuffer,
							&iResponse
							);
		nWrittenCount += MAX_REQUESTBLOCK;	// MAX_REQUESTBLOCK : max number of blocks
											//				can be placed in a request
		if(iErrorCode != 0 || LANSCSI_RESPONSE_SUCCESS != iResponse)
		{
			// ERROR : Fail to write
			WTL::CString strError;
			strError.Format( _T("Node:%s, ErrorCode:%d, Response:%d"),
				AddrToString(m_path.address.Node), iErrorCode, iResponse );
			NDAS_THROW_EXCEPTION_STR(
				CNetworkException, 
				CNetworkException::ERROR_FAIL_TO_WRITE,
				strError
				);
			break;
		}
	}
}

void CSession::Read(_int64 iLocation, _int16 nSecCount, _int8 *pbData)
{
	_int16 nReadCount = 0;
	unsigned _int8	iResponse;
	int iErrorCode;

	if ( iLocation < 0 )
	{
		iLocation = m_path.PerTarget[m_nUnitNumber].SectorCount + iLocation;
	}

	while ( nReadCount < nSecCount )
	{
		iErrorCode = ::IdeCommand(
			&m_path, 
			m_nUnitNumber,
			0,
			WIN_READ,
			iLocation + nReadCount,
			(_int16)std::min( MAX_REQUESTBLOCK, nSecCount-nReadCount ),
			0,
			pbData + nReadCount*BLOCK_SIZE,
			&iResponse
			);
		nReadCount += MAX_REQUESTBLOCK;	// MAX_REQUESTBLOCK : max number of blocks
										//					 can be placed in a request
		if(iErrorCode != 0 || LANSCSI_RESPONSE_SUCCESS != iResponse)
		{
			// ERROR : Fail to read
			WTL::CString strError;
			strError.Format( _T("Node:%s, ErrorCode:%d, Response:%d"), 
				AddrToString(m_path.address.Node), iErrorCode, iResponse );
			NDAS_THROW_EXCEPTION_STR(
				CNetworkException, 
				CNetworkException::ERROR_FAIL_TO_READ,
				strError
				);
			break;
		}
	}
}

unsigned _int64 CSession::_GetHWPassword(const BYTE *pbAddress)
{
	if ( pbAddress == NULL )
		return 0;
	// password
	// if it's sample's address, use its password
	if(	pbAddress[0] == 0x00 &&
		pbAddress[1] == 0xf0 &&
		pbAddress[2] == 0x0f)
	{
		return HASH_KEY_SAMPLE;
	}
#ifdef OEM_RUTTER
	else if(	
		pbAddress[0] == 0x00 &&
		pbAddress[1] == 0x0B &&
		pbAddress[2] == 0xD0 &&
		pbAddress[3] & 0xFE == 0x20
		)
	{
		return HASH_KEY_RUTTER;
	}
#endif // OEM_RUTTER
	else if(	
		pbAddress[0] == 0x00 &&
		pbAddress[1] == 0x0B &&
		pbAddress[2] == 0xD0)
	{
		return HASH_KEY_USER;
	}
	else
	{
		return HASH_KEY_USER;
	}
}

void CSession::_GetInterfaceList(
								LPSOCKET_ADDRESS_LIST socketAddressList, 
								DWORD socketAddressListLength 
								)
{
	int		iErrcode;
	SOCKET	sock;
	DWORD	outputBytes;

	//
	// Initialize the variable to make sure that 
	// it always has a proper value even in case of error.
	//
	socketAddressList->iAddressCount = 0;
	//
	//	Please notice that socket() sometimes does illegal memory reference during NICs are enabled or disabled.
	//
	sock = ::socket(AF_LPX, SOCK_STREAM, IPPROTO_LPXTCP);

	if(sock == INVALID_SOCKET) {
		iErrcode = ::WSAGetLastError();
		// ERROR : Fail to create socket
		NDAS_THROW_EXCEPTION(
			CNetworkException,
			CNetworkException::ERROR_NETWORK_FAIL_TO_CREATE_SOCKET
			);
		return;
	}

	outputBytes = 0;

	iErrcode = ::WSAIoctl(
		sock,							// SOCKET s,
		SIO_ADDRESS_LIST_QUERY, 		// DWORD dwIoControlCode,
		NULL,							// LPVOID lpvInBuffer,
		0,								// DWORD cbInBuffer,
		socketAddressList,				// LPVOID lpvOutBuffer,
		socketAddressListLength,		// DWORD cbOutBuffer,
		&outputBytes,					// LPDWORD lpcbBytesReturned,
		NULL,							// LPWSAOVERLAPPED lpOverlapped,
		NULL							// LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
		);

	//DebugPrint( 3, ( "[LDServ] GetInterfaceList: WSAIoctol: ErrCode:%d outputBytes:%d\n", iErrcode, outputBytes));
	::closesocket(sock);

	if(iErrcode == SOCKET_ERROR) {
		// ERROR : Fail to get NIC list
		NDAS_THROW_EXCEPTION(
			CNetworkException,
			CNetworkException::ERROR_NETWORK_FAIL_TO_GET_NIC_LIST
			);
	}

	return;	
}
///////////////////////////////////////////////////////////////////////////////
// CDiskMultiSector
///////////////////////////////////////////////////////////////////////////////
CDiskMultiSector::CDiskMultiSector()
	: m_nCount(1)
{
}
CDiskMultiSector::CDiskMultiSector(UINT nSectorCount)
	: m_nCount(static_cast<_int16>(nSectorCount))
{
	// 
	// When m_nCount is 1, member variables are initialized by 
	// the parent class 'CDiskSector'
	if ( m_nCount > 1)
	{
		m_dataExpanded = boost::shared_ptr<_int8>(new _int8[BLOCK_SIZE*m_nCount]);
		::ZeroMemory( m_dataExpanded.get(), BLOCK_SIZE*m_nCount);
		m_pdata = m_dataExpanded.get();
	}
}
void CDiskMultiSector::Resize(UINT nSectorCount)
{
	ATLASSERT ( nSectorCount >= static_cast<UINT>(m_nCount) ); // Cannot reduce the size
	if ( nSectorCount == static_cast<UINT>(m_nCount) ) return;
	
	// Allocate a new buffer
	_int8 *pNewData = new _int8[BLOCK_SIZE*nSectorCount];
	// Initialize the new buffer and copy data from the old buffer
	::ZeroMemory( pNewData, BLOCK_SIZE*nSectorCount );
	::CopyMemory( pNewData, m_pdata, BLOCK_SIZE*m_nCount);
	// Change status variables's values.
	m_nCount = static_cast<_int16>(nSectorCount);
	m_dataExpanded = boost::shared_ptr<_int8>(pNewData);
	m_pdata = m_dataExpanded.get();
}

///////////////////////////////////////////////////////////////////////////////
// CDIBSector
///////////////////////////////////////////////////////////////////////////////

CDIBSector::CDIBSector(const TARGET_DATA *pTargetData)
: CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>(pTargetData)
{
	DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();

	pDIB->Signature = DISK_INFORMATION_SIGNATURE;
	pDIB->MajorVersion = DISK_INFORMATION_VERSION_MAJOR;
	pDIB->MinorVersion = DISK_INFORMATION_VERSION_MINOR;

}

BOOL CDIBSector::IsValid() const
{
	if ( get()->Signature != DISK_INFORMATION_SIGNATURE 
		|| IS_WRONG_VERSION(*get()) )
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CDIBSector::IsBound() const 
{
	return IsAggregated() && IsMirrored();
}

BOOL CDIBSector::IsMirrored() const
{
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	BOOL bMirrored = FALSE;
	// For DISK_INFORMATION_BLOCK, 
	// we need to check disk type to see whether it is mirrored.
	switch( pDIB->DiskType )
	{
	case UNITDISK_TYPE_MIRROR_MASTER:
	case UNITDISK_TYPE_MIRROR_SLAVE:
		bMirrored = TRUE;
		break;
	default:
		break;
	}
	return bMirrored;
}

BOOL CDIBSector::IsAggregated() const
{
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	BOOL bAggr = FALSE;
	// For DISK_INFORMATION_BLOCK, 
	// we need to check disk type to see whether it is mirrored.
	switch( pDIB->DiskType )
	{
	case UNITDISK_TYPE_AGGREGATION_FIRST:
	case UNITDISK_TYPE_AGGREGATION_SECOND:
	case UNITDISK_TYPE_AGGREGATION_THIRD:
	case UNITDISK_TYPE_AGGREGATION_FOURTH:
		bAggr = TRUE;
		break;
	default:
		break;
	}
	return bAggr;
}

BOOL CDIBSector::IsMaster() const
{
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	if ( IsAggregated() )
		return (pDIB->DiskType == UNITDISK_TYPE_AGGREGATION_FIRST);
	else if ( IsMirrored() )
		return (pDIB->DiskType == UNITDISK_TYPE_MIRROR_MASTER);
	else
		return TRUE;
}

CDiskLocationVector CDIBSector::GetBoundDiskLocations() const
{
	//
	// NOTE : Previous version which uses DISK_INFORMATION_BLOCK(ver1)
	//		allows only 2 disks to be bound.
	//		( UNITDISK_TYPE_AGGREGATION_THIRD,
	//		  UNITDISK_TYPE_AGGREGATION_FOURTH are not used.)
	//
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	CDiskLocationVector vtLocation;
	UNIT_DISK_LOCATION udlFirst, udlSecond;
	::ZeroMemory( &udlFirst, sizeof(UNIT_DISK_LOCATION) );
	::ZeroMemory( &udlSecond, sizeof(UNIT_DISK_LOCATION) );

	ATLASSERT( IsBound() );

	switch ( pDIB->DiskType )
	{
	case UNITDISK_TYPE_AGGREGATION_FIRST:
	case UNITDISK_TYPE_MIRROR_MASTER:
		::CopyMemory( &udlFirst.MACAddr, pDIB->EtherAddress, sizeof( udlFirst.MACAddr) );
		udlFirst.UnitNumber = pDIB->UnitNumber;
		::CopyMemory( &udlSecond.MACAddr, pDIB->PeerAddress, sizeof(udlSecond.MACAddr) );
		udlSecond.UnitNumber = pDIB->PeerUnitNumber;
		break;
	case UNITDISK_TYPE_MIRROR_SLAVE:
	case UNITDISK_TYPE_AGGREGATION_SECOND:
	case UNITDISK_TYPE_AGGREGATION_THIRD:
	case UNITDISK_TYPE_AGGREGATION_FOURTH:
	default:
		::CopyMemory( &udlSecond.MACAddr, pDIB->EtherAddress, sizeof( udlSecond.MACAddr) );
		udlSecond.UnitNumber = pDIB->UnitNumber;
		::CopyMemory( &udlFirst.MACAddr, pDIB->PeerAddress, sizeof(udlFirst.MACAddr) );
		udlFirst.UnitNumber = pDIB->PeerUnitNumber;
		break;
	}

	vtLocation.push_back( CDiskLocationPtr(new CLanDiskLocation( &udlFirst )) );
	vtLocation.push_back( CDiskLocationPtr(new CLanDiskLocation( &udlSecond)) );

	return vtLocation;
}
CDiskLocationPtr CDIBSector::GetPeerLocation() const
{
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	//
	// Peer disk's location is stored in PeerAddress field
	//
	UNIT_DISK_LOCATION udlPeer;
	::ZeroMemory( &udlPeer, sizeof(UNIT_DISK_LOCATION) );
	::CopyMemory( &udlPeer.MACAddr, pDIB->PeerAddress, sizeof(udlPeer.MACAddr) );
	udlPeer.UnitNumber = pDIB->PeerUnitNumber;

	return CDiskLocationPtr( new CLanDiskLocation( &udlPeer ) );
}

UINT CDIBSector::GetPosInBind() const
{
	const DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	ATLASSERT( IsBound() );
	switch( pDIB->DiskType )
	{
	case UNITDISK_TYPE_MIRROR_MASTER:
		return 0;
	case UNITDISK_TYPE_MIRROR_SLAVE:
		return 1;
	case UNITDISK_TYPE_AGGREGATION_FIRST:
		return 0;
	case UNITDISK_TYPE_AGGREGATION_SECOND:
		return 1;
	case UNITDISK_TYPE_AGGREGATION_THIRD:
		return 2;
	case UNITDISK_TYPE_AGGREGATION_FOURTH:
		return 3;
	default:
		ATLASSERT(FALSE);
		break;
	}
	return 0;
}

UINT CDIBSector::GetDiskCountInBind() const
{
	ATLASSERT( IsBound() );
	return 2;
}

_int64 CDIBSector::GetUserSectorCount() const
{
	return ::CalcUserSectorCount( GetTotalSectorCount() );
}

void CDIBSector::UnBind()
{
	DISK_INFORMATION_BLOCK *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK>::get();
	pDIB->DiskType = UNITDISK_TYPE_SINGLE;
	::ZeroMemory(pDIB->PeerAddress, sizeof(pDIB->PeerAddress));
	pDIB->PeerUnitNumber = 0;
}

///////////////////////////////////////////////////////////////////////////////
// CDIBV2Sector
///////////////////////////////////////////////////////////////////////////////
CDIBV2Sector::CDIBV2Sector(const TARGET_DATA *pTargetData)
: CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>(pTargetData),
  m_nCount(1), m_dataExpanded()
{
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();

	pDIB->Signature = DISK_INFORMATION_SIGNATURE_V2;
	pDIB->MajorVersion = CURRENT_MAJOR_VERSION_V2;
	pDIB->MinorVersion = CURRENT_MINOR_VERSION_V2;
	
}
CDIBV2Sector::CDIBV2Sector(const CDIBSector *pDIBSector)
: CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>(&pDIBSector->m_targetData),
  m_nCount(1), m_dataExpanded()
{
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();

	pDIB->Signature = DISK_INFORMATION_SIGNATURE_V2;
	pDIB->MajorVersion = CURRENT_MAJOR_VERSION_V2;
	pDIB->MinorVersion = CURRENT_MINOR_VERSION_V2;
}

BOOL CDIBV2Sector::IsValid() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	if ( pDIB->Signature != DISK_INFORMATION_SIGNATURE_V2
		|| IS_HIGHER_VERSION_V2(*pDIB) )
	{
		return FALSE;
	}
	return TRUE;
}
void CDIBV2Sector::ReadAccept(CSession *pSession)
{
	// Read DISK_INFORMATIO_BLOCK_V2 from the disk.
	pSession->Read( DIBV2_LOCATION, 1, m_pdata );	// NOTE : m_data is a member variable of CTypedDiskSector..

	if ( !IsValid() )
	{
		return;
	}

	if ( m_nCount < static_cast<_int16>(CalcRequiredSectorCount(get())) )
	{
		m_nCount = (_int16)CalcRequiredSectorCount(get());
		if ( m_nCount > 1 )
		{
			m_dataExpanded = boost::shared_ptr<_int8>( new _int8[m_nCount*BLOCK_SIZE] );
			::CopyMemory( m_dataExpanded.get(), m_pdata, BLOCK_SIZE );
			m_pdata = m_dataExpanded.get();
		}
	}

	// Read trailing sectors
	for (int i=1; i < m_nCount; i++ )
	{
		pSession->Read( GetLocation()-i, 1, m_pdata );
	}
}

void CDIBV2Sector::WriteAccept(CSession *pSession)
{
	for ( int i=0; i< m_nCount; i++ )
	{
		pSession->Write( GetLocation()-i, 1, m_pdata );
	}
}

BOOL CDIBV2Sector::IsBound() const 
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	return ( pDIB->nDiskCount > 1 );
}
BOOL CDIBV2Sector::IsMirrored() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	return ( (pDIB->flagDiskAttr & NDT_MIRROR) != 0 );
}

BOOL CDIBV2Sector::IsAggregated() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	if ( !IsMirrored() )
	{
		return IsBound();
	}
	else
	{
		return ( pDIB->nDiskCount > 2);
	}
}
BOOL CDIBV2Sector::IsMaster() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	if ( IsMirrored() )
		return ( (pDIB->iPositionInBind & 0x01) == 0);
	else
		return (pDIB->iPositionInBind == 0);
}

BOOL CDIBV2Sector::IsDirty() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	return ( pDIB->FlagDirty & NDAS_DIRTY_MIRROR_DIRTY );
}

void CDIBV2Sector::SetDirty(BOOL bDirty)
{
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	if ( bDirty )
		pDIB->FlagDirty = NDAS_DIRTY_MIRROR_DIRTY;
	else
		pDIB->FlagDirty = 0;
}
_int64 CDIBV2Sector::GetUserSectorCount() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	if ( pDIB->sizeUserSpace == 0 )
		return ::CalcUserSectorCount( GetTotalSectorCount() );
	else
		return pDIB->sizeUserSpace;
}

_int32 CDIBV2Sector::GetSectorsPerBit() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	return ( pDIB->iSectorsPerBits );
}

CDiskLocationVector CDIBV2Sector::GetBoundDiskLocations() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	CDiskLocationVector vtLocation;
	ATLASSERT( IsBound() );	

	for ( UINT i=0; i < pDIB->nDiskCount; i++ )
	{
		vtLocation.push_back ( 
			CDiskLocationPtr( 
				new CLanDiskLocation(
					&pDIB->UnitDisks[i]
					)
				)
			);
	}
	return vtLocation;
}
CDiskLocationPtr CDIBV2Sector::GetPeerLocation() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	ATLASSERT( IsMirrored() );
	return CDiskLocationPtr( 
		new CLanDiskLocation(&pDIB->UnitDisks[pDIB->iPositionInBind ^ 0x01]) 
		);
}

UINT CDIBV2Sector::GetPosInBind() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB =
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();

	ATLASSERT( IsBound() );
	return pDIB->iPositionInBind;
}

UINT CDIBV2Sector::GetDiskCountInBind() const
{
	const DISK_INFORMATION_BLOCK_V2 *pDIB =
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();

	ATLASSERT( IsBound() );
	return pDIB->nDiskCount;
}

void CDIBV2Sector::Bind(CDiskObjectVector bindDisks, UINT nIndex, BOOL bUseMirror)
{
	DISK_INFORMATION_BLOCK_V2 *pDIB =
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	_int64 nSizeTotal;

	if ( bUseMirror )
	{
		ATLASSERT( (bindDisks.size() % 2) == 0 );
		CUnitDiskObjectPtr peerDisk = 
			boost::dynamic_pointer_cast<CUnitDiskObject>(bindDisks[nIndex^0x01]);
		ATLASSERT( peerDisk.get() != NULL );
		CUnitDiskInfoHandlerPtr peerInfoHandler = peerDisk->GetInfoHandler();
		nSizeTotal = std::min( 
						CDiskInfoSector::GetTotalSectorCount(),
						peerInfoHandler->GetTotalSectorCount()
						);
	}
	else
	{
		nSizeTotal = CDiskInfoSector::GetTotalSectorCount();
	}
	
	pDIB->sizeXArea			= X_AREA_SIZE / BLOCK_SIZE;
	pDIB->sizeUserSpace		= ::CalcUserSectorCount( nSizeTotal );
	pDIB->iSectorsPerBits	= ::CalcSectorPerBit( pDIB->sizeUserSpace );
	pDIB->flagDiskAttr		= (bUseMirror? NDT_MIRROR : 0);
	pDIB->nDiskCount		= bindDisks.size();
	pDIB->iPositionInBind	= nIndex;

	if ( CalcRequiredSectorCount(pDIB) > static_cast<UINT>(GetCount()) )
	{
		CDiskMultiSector::Resize( CalcRequiredSectorCount(pDIB) );
		pDIB =
			CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	}

	for ( UINT i=0; i < bindDisks.size(); i++ )
	{
		::CopyMemory( 
			&pDIB->UnitDisks[i], 
			boost::dynamic_pointer_cast<CUnitDiskObject>(bindDisks[i])->
				GetLocation()->GetUnitDiskLocation(),
			sizeof(UNIT_DISK_LOCATION)
			);
	}
}

void CDIBV2Sector::UnBind()
{
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	UINT nPrevBoundDisks = pDIB->nDiskCount;

	pDIB->flagDiskAttr		= 0;
	pDIB->nDiskCount		= 1;
	pDIB->iPositionInBind	= 0;
	pDIB->FlagDirty			= 0;
	::ZeroMemory( pDIB->UnitDisks, sizeof(UNIT_DISK_LOCATION) * nPrevBoundDisks );
}

void CDIBV2Sector::Rebind(CUnitDiskObjectPtr newDisk, UINT nIndex)
{
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	ATLASSERT( nIndex < pDIB->nDiskCount );
	::CopyMemory( 
		&pDIB->UnitDisks[nIndex], 
		newDisk->GetLocation()->GetUnitDiskLocation(),
		sizeof(UNIT_DISK_LOCATION)
		);
}

void CDIBV2Sector::Mirror(CDIBV2Sector *pSourceSector)
{
	ATLASSERT( 
		GetUserSectorCount() >= pSourceSector->GetUserSectorCount() );
	CDiskMultiSector::Resize( pSourceSector->GetCount() );
	DISK_INFORMATION_BLOCK_V2 *pDIB = 
		CTypedDiskInfoSector<DISK_INFORMATION_BLOCK_V2>::get();
	DISK_INFORMATION_BLOCK_V2 *pSourceDIB = pSourceSector->get();
	pDIB->sizeXArea			= pSourceDIB->sizeXArea;
	pDIB->sizeUserSpace		= pSourceDIB->sizeUserSpace;
	pDIB->iSectorsPerBits	= pSourceDIB->iSectorsPerBits;
	pDIB->flagDiskAttr		= NDT_MIRROR;
	pDIB->nDiskCount		= pSourceDIB->nDiskCount;
	pDIB->iPositionInBind	= pSourceDIB->iPositionInBind ^ 0x01;
	pDIB->FlagDirty			= 0;
	
	::CopyMemory( 
		pDIB->UnitDisks, 
		pSourceDIB->UnitDisks, 
		sizeof(UNIT_DISK_LOCATION) * pDIB->nDiskCount 
		);
}
///////////////////////////////////////////////////////////////////////////////
// CBitmapSector
///////////////////////////////////////////////////////////////////////////////
CBitmapSector::CBitmapSector()
: CDiskMultiSector(SECTOR_BITMAP_COUNT)
{
}
_int64 CBitmapSector::GetLocation()
{
	return SECTOR_BITMAP_LOCATION;	
}
