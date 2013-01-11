#include		"StdAfx.h"

extern int iTestSize;		// Default : 5 MB
extern HANDLE	hServerStopEvent;
extern HANDLE	hUpdateEvent;

#define		REQBLK		8


BOOL TestInitialize(void)
{
	WSADATA			wsadata;
	int				iErrorCode;

	DebugPrint(2, (TEXT("[NETDISKTEST]TestInitialize: Start Initialization.\n")));

	// Create events
	hServerStopEvent = CreateEvent(
		NULL,		// no security attibutes
		TRUE,		// manual reset event
		FALSE,		// not-signalled
		NULL		// INIT_EVENT_NAME
		);
	if(hServerStopEvent == NULL) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: Cannot create event.\n")));
		return FALSE;
	}
	
	hUpdateEvent = CreateEvent(
		NULL,		// no security attibutes
		TRUE,		// manual reset event
		FALSE,		// not-signalled
		NULL		// INIT_EVENT_NAME
		);
	if(hUpdateEvent == NULL) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: Cannot create event.\n")));
		return FALSE;
	}

	// Init socket
	iErrorCode = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if(iErrorCode != 0) {
		DebugPrint(1, (TEXT("[NetDiskTest]TestInitialize: WSAStartup Failed %d\n"), iErrorCode));
		return FALSE;
	}

	return TRUE;
}

//
//	Connect
//
BOOL
IsConnected(PTEST_NETDISK Disk) {
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use Device ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.Flags |= NDASCOMM_CNF_CONNECT_ONLY; // Connect to the NDAS device without log-in.
	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */

	memcpy(&ci.Address.DeviceId.Node, Disk->ucAddr, 6);
	if(NULL == (hNDAS = NdasCommConnect(&ci)))
	{
		TRACE( "NdasCommConnect Failed %X\n", GetLastError());
		return FALSE;
	}

	NdasCommDisconnect(hNDAS);

	return TRUE;
}


//
//	Connect and retrieve disk information.
//
BOOL
IsValidate(PTEST_NETDISK Disk)
{
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;
	NDAS_UNITDEVICE_HARDWARE_INFO diskInfo;
	BOOL success;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use DEVICE ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	memcpy(&ci.Address.DeviceId.Node, Disk->ucAddr, 6);
	if(NULL == (hNDAS = NdasCommConnect(&ci)))
	{
		TRACE( "NdasCommConnect Failed %X\n", GetLastError());
		return FALSE;
	}
	diskInfo.Size = sizeof(NDAS_UNITDEVICE_HARDWARE_INFO);
	success = NdasCommGetUnitDeviceHardwareInfo(hNDAS, &diskInfo);
	if(success) {
		Disk->SectorCount = diskInfo.SectorCount.QuadPart;
	}
	
	NdasCommDisconnect(hNDAS);

	return success;
}

#define TEST_STAGE2_SIZE		10	// 5 MBytes
#define TEST_BUFFER_LENGTH		(1024 * 1024 * TEST_STAGE2_SIZE)

UCHAR	buffTest[TEST_BUFFER_LENGTH],
		buffTestIdent[TEST_BUFFER_LENGTH],
		buffVerify[TEST_BUFFER_LENGTH],
		buffBak[TEST_BUFFER_LENGTH];

BOOL			TestStep2(PTEST_NETDISK Disk)
{
	INT					i, j;

	unsigned _int64		start;
	unsigned _int64		loop;

	BOOL				bReturnVal;

	//
	//	Connect and login
	//
	BOOL bResult;
	BOOL bReturn = FALSE;
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use DEVICE ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	memcpy(&ci.Address.DeviceId.Node, Disk->ucAddr, 6);
	//	_tcsncpy(ci.Address.NdasId.Id, argv[0], 20); /* ID */
	//	_tcsncpy(ci.Address.NdasId.Key, argv[1], 5); /* Key */
	if(NULL == (hNDAS = NdasCommConnect(&ci)))
	{
		TRACE( "NdasCommConnect Failed %X\n", GetLastError());
		return FALSE;
	}

	/////////////////////////////////////////////////
	bReturnVal = FALSE;
	memset(buffVerify, 0, sizeof(buffVerify));
	memset(buffTestIdent, 0, sizeof(buffTestIdent));
	memset(buffBak, 0, sizeof(buffBak));

	srand(clock());
	for (i = 0; i < iTestSize * 1024 * 1024; i++) {
		buffTestIdent[i] = buffTest[i] = (UCHAR)rand();
	}

	// At the beginning of the disk address.
	// Save the original data to memory.
	start = 0;
	loop = iTestSize * 2 * 1024 / REQBLK / 2;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffBak+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nREAD(Backup stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffBak+(j*8)  ), *(buffBak+(j*8)+1), *(buffBak+(j*8)+2), *(buffBak+(j*8)+3), *(buffBak+(j*8)+4), *(buffBak+(j*8)+5), *(buffBak+(j*8)+6), *(buffBak+(j*8)+7) );
	}
	TRACE("\n");

	// At the beginning of the disk address.
	// Write test data to the disk.

	start = 0;
	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffTest+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nWrite(Test stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffTestIdent+(j*8)  ), *(buffTestIdent+(j*8)+1), *(buffTestIdent+(j*8)+2), *(buffTestIdent+(j*8)+3), *(buffTestIdent+(j*8)+4), *(buffTestIdent+(j*8)+5), *(buffTestIdent+(j*8)+6), *(buffTestIdent+(j*8)+7) );
	}
	TRACE("\n");

	// At the beginning of the disk address.
	// Verify the disk with test data in memory.
	start = 0;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffVerify+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	//
	//if (0 == memcmp(buffTestIdent, buffVerify, TEST_BUFFER_LENGTH/2)) {
	if (0 == memcmp(buffTestIdent, buffVerify, iTestSize * 1024 * 1024 / 2)) {
		bReturnVal = TRUE;
	}

	TRACE( "\nREAD(Test-Verify stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffVerify+(j*8)  ), *(buffVerify+(j*8)+1), *(buffVerify+(j*8)+2), *(buffVerify+(j*8)+3), *(buffVerify+(j*8)+4), *(buffVerify+(j*8)+5), *(buffVerify+(j*8)+6), *(buffVerify+(j*8)+7) );
	}
	TRACE("\n");

	// At the beginning of the disk address.
	// Restore original data
	start = 0;
	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffBak+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	// At the beginning of the disk address.
	// Restore Verification
	start = 0;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffVerify+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nREAD(Restore-Verify stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffVerify+(j*8)  ), *(buffVerify+(j*8)+1), *(buffVerify+(j*8)+2), *(buffVerify+(j*8)+3), *(buffVerify+(j*8)+4), *(buffVerify+(j*8)+5), *(buffVerify+(j*8)+6), *(buffVerify+(j*8)+7) );
	}
	TRACE("\n");

	if (bReturnVal == FALSE) goto log_out2;

	// In the middle of the disk address
	for (i = 0; i < iTestSize * 1024 * 1024; i++) {
		buffTestIdent[i] = buffTest[i] = (UCHAR)rand();
	}
	// In the middle of the disk address
	// Save the original data to memory.
	start = Disk->SectorCount / 2;
	//loop = iTestSize * 2 * 1024 / REQBLK;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffBak+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nREAD(Backup stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffBak+(j*8)  ), *(buffBak+(j*8)+1), *(buffBak+(j*8)+2), *(buffBak+(j*8)+3), *(buffBak+(j*8)+4), *(buffBak+(j*8)+5), *(buffBak+(j*8)+6), *(buffBak+(j*8)+7) );
	}
	TRACE("\n");

	// In the middle of the disk address
	// Write test data to the disk

	start = Disk->SectorCount / 2;
	for (i = 0; i < loop; i++) {
		// write
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffTest+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nWrite(Test stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffTestIdent+(j*8)  ), *(buffTestIdent+(j*8)+1), *(buffTestIdent+(j*8)+2), *(buffTestIdent+(j*8)+3), *(buffTestIdent+(j*8)+4), *(buffTestIdent+(j*8)+5), *(buffTestIdent+(j*8)+6), *(buffTestIdent+(j*8)+7) );
	}
	TRACE("\n");

	// In the middle of the disk address
	// Verify the disk with test data in memory.
	start = Disk->SectorCount / 2;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffVerify+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	//
	if (0 == memcmp(buffTestIdent, buffVerify, iTestSize * 1024 * 1024 / 2)) {
		bReturnVal = TRUE;
	}

	TRACE( "\nREAD(Test-Verify stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffVerify+(j*8)  ), *(buffVerify+(j*8)+1), *(buffVerify+(j*8)+2), *(buffVerify+(j*8)+3), *(buffVerify+(j*8)+4), *(buffVerify+(j*8)+5), *(buffVerify+(j*8)+6), *(buffVerify+(j*8)+7) );
	}
	TRACE("\n");

	// In the middle of the disk address
	// Restore original data
	start = Disk->SectorCount / 2;
	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffBak+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	// In the middle of the disk address
	// Restore Verification
	start = Disk->SectorCount / 2;
	for (i = 0; i < loop; i++) {
		// read
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffVerify+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	TRACE( "\nREAD(Restore-Verify stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffVerify+(j*8)  ), *(buffVerify+(j*8)+1), *(buffVerify+(j*8)+2), *(buffVerify+(j*8)+3), *(buffVerify+(j*8)+4), *(buffVerify+(j*8)+5), *(buffVerify+(j*8)+6), *(buffVerify+(j*8)+7) );
	}
	TRACE("\n");
	
	bReturn = TRUE;
log_out2:
	//
	// Logout Packet.
	//
	DebugPrint( 1, ( "[LanScsiCli]main: Logout..\n"));

	NdasCommDisconnect(hNDAS);

	return bReturn;
}

BOOL			TestStep3(PTEST_NETDISK Disk)
{
	//UCHAR				data[MAX_DATA_BUFFER_SIZE], data2[MAX_DATA_BUFFER_SIZE];
	INT					i, j;
	int					iTargetID = 0;

	unsigned _int64		start;
	unsigned _int64		loop;

	BOOL				bReturnVal;

	//
	//	Connect and login
	//
	BOOL bResult;
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use DEVICE ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	memcpy(&ci.Address.DeviceId.Node, Disk->ucAddr, 6);
	if(NULL == (hNDAS = NdasCommConnect(&ci)))
	{
		TRACE( "NdasCommConnect Failed %X\n", GetLastError());
		return FALSE;
	}
	
	/////////////////////////////////////////////////
	bReturnVal = FALSE;
	memset(buffTest, 0, sizeof(buffTest));
	memset(buffVerify, 0, sizeof(buffVerify));

	start = Disk->SectorCount - (2 * 1024 * 2);	// 2MB at the Tail
	loop = 4096 / REQBLK;	// == 512

	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffTest+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out3;
		}
		start+= REQBLK;
	}

	start = Disk->SectorCount - (2 * 1024 * 2);	// 2MB at the Tail
	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceRead(hNDAS, start, REQBLK, (PBYTE)(buffTest+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceRead Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out3;
		}
		start+= REQBLK;
	}

	TRACE( "\nWrite(Test stage) done...(started at : %d)", start );
	for (j = 0; j < 8; j++) {
		TRACE("\n\t%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X", *(buffTest+(j*8)  ), *(buffTest+(j*8)+1), *(buffTest+(j*8)+2), *(buffTest+(j*8)+3), *(buffTest+(j*8)+4), *(buffTest+(j*8)+5), *(buffTest+(j*8)+6), *(buffTest+(j*8)+7) );
	}
	TRACE("\n");

	if (0 == memcmp(buffTest, buffVerify, TEST_BUFFER_LENGTH)) {
		bReturnVal = TRUE;
	}

	/////////////////////////////////////////////////

log_out3:
	//
	// Logout Packet.
	//
	DebugPrint( 1, ( "[LanScsiCli]main: Logout..\n"));

	NdasCommDisconnect(hNDAS);

	return bReturnVal;
}

BOOL 
pNdasCommDisconnectAfterReset(HNDAS NdasHandle)
{
	__try
	{
		ATLVERIFY( NdasCommDisconnectEx(NdasHandle, NDASCOMM_DF_DONT_LOGOUT) );
	}
	__except(0xC06D007E == GetExceptionCode() ? 
EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ATLTRACE("NdasCommDisconnectEx is not available\n");
		ATLVERIFY( NdasCommDisconnect(NdasHandle) );
		// ATLVERIFY( m_ndasDeviceListWnd.ModifyStyle(0, CBS_DROPDOWN) );
	}

	return TRUE;
}


BOOL			ReadProductNumber(PTEST_NETDISK Disk, char *szPN)
{
	//UCHAR				data[MAX_DATA_BUFFER_SIZE], data2[MAX_DATA_BUFFER_SIZE];
	int					iTargetID = 0;

	BOOL				bReturnVal = FALSE;
	NDASCOMM_VCMD_PARAM vcmdparam = {0};

	//
	//	Connect and login
	//
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use DEVICE ID */
	ci.UnitNo = 0; /* Use first Unit Device */
	ci.WriteAccess = TRUE; /* Connect with read-write privilege */
	ci.Protocol = NDASCOMM_TRANSPORT_LPX; /* Use LPX protocol */
	ci.OEMCode.UI64Value = 0; /* Use default password */
	ci.PrivilegedOEMCode.UI64Value = 0x3E2B321A4750131E;
	ci.Flags = 0;

	ci.Address.DeviceId.Vid = 1;

	ci.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL; /* Normal operations */
	memcpy(&ci.Address.DeviceId.Node, Disk->ucAddr, 6);
	if(NULL == (hNDAS = NdasCommConnect(&ci)))
	{
		TRACE( "NdasCommConnect Failed %X\n", GetLastError());
		return FALSE;
	}

	{
		vcmdparam.GETSET_EEPROM.Address = 0xb0;
//		RtlCopyMemory(vcmdparam.GETSET_EEPROM.Data, &m_eepromImage[address], 16);
		BOOL success;
		success = NdasCommVendorCommand(
			hNDAS,
			ndascomm_vcmd_get_eep,
			&vcmdparam,
			NULL, 0,
			NULL, 0);

		if (success)
		{
			bReturnVal = TRUE;
			memcpy(szPN, vcmdparam.GETSET_EEPROM.Data, 16);
		}

		vcmdparam.GETSET_EEPROM.Address = 0xc0;
		success = NdasCommVendorCommand(
			hNDAS,
			ndascomm_vcmd_get_eep,
			&vcmdparam,
			NULL, 0,
			NULL, 0);

		if (success)
		{
			bReturnVal = TRUE;
			memcpy(szPN + 16, vcmdparam.GETSET_EEPROM.Data, 16);
		}
	}

	//
	// Logout Packet.
	//
	DebugPrint( 1, ( "[LanScsiCli]main: Logout..\n"));

	RtlZeroMemory(&vcmdparam, sizeof(vcmdparam));
	NdasCommVendorCommand(
		hNDAS, 
		ndascomm_vcmd_reset, 
		&vcmdparam, 
		NULL, 0, 
		NULL, 0);

	pNdasCommDisconnectAfterReset(hNDAS);
//	NdasCommDisconnect(hNDAS);

	return bReturnVal;
}