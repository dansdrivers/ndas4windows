////////////////////////////////////////////////////////////////////////////
//
// Interface of CSession class
//
// @author Ji Young Park(jypark@ximeta.com)
//
////////////////////////////////////////////////////////////////////////////

#ifndef _NDASSESSION_H_
#define _NDASSESSION_H_

#include "SocketLpx.h"
#include "ndas/ndasdib.h"
#include "ndastype.h"
// #include "landisk2.h"

class CSession
{
protected:
	HNDAS			m_hNDAS;

public:
	//
	// Return TRUE if there's session for the specified operation
	//
	virtual BOOL IsLoggedIn(BOOL bAsWrite = FALSE) = 0;
	
	// 
	// Write data to the disk
	//
	// @param iLocation	[in] starting address of sectors
	// @param nSecCount	[in] number of sectors to write
	// @param pbData	[in] pointer to the buffer which has the data to write
	//
	virtual void Write(_int64 iLocation, _int16 nSecCount, _int8 *pbData) = 0;

	//
	// Read data from the disk
	//
	// @param iLocation	[In] starting address of sectors
	// @param nSecCount	[In] number of sectors to read
	// @param pbData	[In] pointer to the buffer to which the data will be stored
	//
	virtual void Read(_int64 iLocation, _int16 nSecCount, _int8 *pbData) = 0;

	HNDAS GetHandle() const {return m_hNDAS;}
};

//
// Session class for lan connection
//
typedef struct _ConnectedHosts
{
	UINT nRWHosts;
	UINT nROHosts;
} ConnectedHosts;

class CLanSession
	: public CSession
{
protected:
	BYTE			m_abNode[6];
	unsigned _int8	m_nSlotNumber;	// UNIT number of the disk.
	BOOL			m_bWrite;		// TRUE if the connection is for write
	BOOL			m_bLoggedIn;

	static	int		m_nGlobalRecentNIC;
	int				m_nRecentNIC;

public:
	CLanSession(const BYTE *pbNode=NULL, unsigned _int8 nSlotNumber=0);
	virtual ~CLanSession();
	//
	// Methods from CSession
	//
	virtual BOOL IsLoggedIn(BOOL bAsWrite = FALSE);
	virtual void Write(_int64 iLocation, _int16 nSecCount, _int8 *pbData);
	virtual void Read(_int64 iLocation, _int16 nSecCount, _int8 *pbData);
	//
	// Get target data from the disk.
	// 
	// @param pTargetData [Out] pointer to the buffer to which the data will be stored
	//
	void GetTargetData(PNDAS_UNIT_DEVICE_INFO pUnitDeviceInfo);
	//
	// return number of disks in the device connected.
	// Connect must be called before this method is called.
	//
	UINT GetDiskCount();
	//
	// return number of hosts connected to the disk
	//
	ConnectedHosts GetHostCount();

	//
	// Connect to/Disconnect from the device
	//
	// @param bWrite [In] If TRUE login with the permission to write
	//
	void Connect(BOOL bWrite = FALSE);
	void Disconnect();

	//
	// Methods used to change address
	//
	void SetAddress(const BYTE *pbNode);
	void SetSlotNumber(unsigned _int8 nSlotNumber);

};

#endif // _NDASSESSION_H_