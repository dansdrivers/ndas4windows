#include		"StdAfx.h"
#include		"LanScsiCli.h"

#define		REQBLK		8
extern int iTestSize;		// Default : 5 MB

static const BYTE NdasPrivOemCodeX1[8] = { 0x1e, 0x13, 0x50, 0x47, 0x1a, 0x32, 0x2b, 0x3e };
/* Little endian UINT64 representation */
static const NDAS_OEM_CODE NDAS_DEFAULT_PRIVILEGED_OEM_CODE = { 0x3E2B321A4750131E };

INT verify_data(UCHAR * data,UCHAR *data2,unsigned _int64 count)
{
	unsigned _int64 size = count*512;
	unsigned _int64 i ;
	for(i=0; i< size ; i++)
		if(data[i] != data[2]) return -1;
	return 0;
}


BOOL	IsConnected(PTEST_NETDISK Disk) {
	BOOL			result;
	SOCKET			TargetSock;
	LPX_ADDRESS		TargetAddress;
	
	//maket LPX Address
	TargetAddress.Port = htons(LPX_PORT_NUMBER);
	memcpy(&TargetAddress.Node,Disk->ucAddr,6);

	//call MakeConnection
	
	result = MakeConnection(&TargetAddress,&TargetSock);
	if(result == TRUE)
		DebugPrint(1, ("Make connection to %.2X:%.2X:%.2X:%.2X:%.2X:%.2X\n",
					Disk->ucAddr[0],
					Disk->ucAddr[1],
					Disk->ucAddr[2],
					Disk->ucAddr[3],
					Disk->ucAddr[4],
					Disk->ucAddr[5]));
	closesocket(TargetSock);
	return result;
}


BOOL	IsValidate(PTEST_NETDISK Disk)
{
	//
	//	Connect and login
	//
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use NDAS ID */
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

	NdasCommDisconnect(hNDAS);

	return TRUE;
}

#define TEST_STAGE2_SIZE		10	// 5 MBytes
#define TEST_BUFFER_LENGTH		(1024 * 1024 * TEST_STAGE2_SIZE)

UCHAR	buffTest[TEST_BUFFER_LENGTH],
		buffTestIdent[TEST_BUFFER_LENGTH],
		buffVerify[TEST_BUFFER_LENGTH],
		buffBak[TEST_BUFFER_LENGTH];

BOOL			TestStep2(PTEST_NETDISK Disk)
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
	BOOL bReturn = FALSE;
	HNDAS hNDAS = NULL;
	NDASCOMM_CONNECTION_INFO ci;

	ZeroMemory(&ci, sizeof(ci));
	ci.Size = sizeof(NDASCOMM_CONNECTION_INFO);
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use NDAS ID */
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

	// (disk앞쪽)
	// 테스트에 사용할 영역 백업
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

	// (disk앞쪽)
	// 테스트 영역 기록

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

	// (disk앞쪽)
	// 기록된 데이터 확인
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

	// (disk앞쪽)
	// backup된 데이터 restore
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

	// (disk앞쪽)
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

	// (disk가운데)
	for (i = 0; i < iTestSize * 1024 * 1024; i++) {
		buffTestIdent[i] = buffTest[i] = (UCHAR)rand();
	}
	// (disk가운데)
	// 테스트에 사용할 영역 백업
	start = PerTarget[iTargetID].SectorCount / 2;
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

	// (disk가운데)
	// 테스트 영역 기록

	start = PerTarget[iTargetID].SectorCount / 2;
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

	// (disk가운데)
	// 기록된 데이터 확인
	start = PerTarget[iTargetID].SectorCount / 2;
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

	// (disk가운데)
	// backup된 데이터 restore
	start = PerTarget[iTargetID].SectorCount / 2;
	for (i = 0; i < loop; i++) {
		bResult = NdasCommBlockDeviceWrite(hNDAS, start, REQBLK, (PBYTE)(buffBak+512*REQBLK*i));
		if(!bResult)
		{
			DebugPrint( 0, ( "NdasCommBlockDeviceWrite Failed... Sector %d (%X)\n", i, GetLastError()));
			goto log_out2;
		}
		start+= REQBLK;
	}

	// (disk가운데)
	// Restore Verification
	start = PerTarget[iTargetID].SectorCount / 2;
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
	ci.AddressType = NDASCOMM_CIT_DEVICE_ID; /* use NDAS ID */
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
	memset(buffTest, 0, sizeof(buffTest));
	memset(buffVerify, 0, sizeof(buffVerify));

	start = PerTarget[iTargetID].SectorCount - (2 * 1024 * 2);	// 2MB at the Tail
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

	start = PerTarget[iTargetID].SectorCount - (2 * 1024 * 2);	// 2MB at the Tail
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
