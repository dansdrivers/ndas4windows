/********************************************************************
	created:	2004/03/24
	created:	24:3:2004   19:04
	filename: 	installhelper\src\NDInst\SvcQuery.cpp
	file path:	installhelper\src\NDInst
	file base:	SvcQuery
	file ext:	cpp
	author:		Hootch
	
	purpose:	Query NetDisk information to NetDisk Helper service.
*********************************************************************/
#include <windows.h>
#include <tchar.h>
#include <msiquery.h>
#include <msi.h>
#include <WinSnmp.h>

#include "NDInst.h"
#include "NDLog.h"
#include "SvcQuery.h"
#include "user/Snmp.h"



UINT
GetNumOfEnabledND(
		PULONG Enabled
	) ;

UINT
CheckIfAnyEnabledND(
	PULONG		EnabledND
	) {
	UINT	status    ;
	BOOL	SnmpStart ;
	ULONG	Enabled ;

	status = ERROR_SUCCESS ;
	SnmpStart = FALSE ;

	//
	//	Start Log facility.
	//	Start SNMP Manager as if this were Admin Tool.
	//

	DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] CheckIfAnyEnabledND: Entered.\n"));
	SnmpStart = AdminSnmpInit() ;

	if( SnmpStart == FALSE) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] CheckIfAnyEnabledND: AdminSnmpInit() Failed...\n"));
		status = ERROR_GEN_FAILURE ;
		goto cleanup ;
	}

	status = GetNumOfEnabledND(&Enabled) ;
	if(status != ERROR_SUCCESS) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] CheckIfAnyEnabledND: GetNumOfEnabledND() Failed...\n"));
		status = ERROR_GEN_FAILURE ;
		goto cleanup ;
	}

	*EnabledND = Enabled ;
	DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] CheckIfAnyEnabledND: Enabled NetDisk #:%d\n"), Enabled);

cleanup:
	//
	//	End Log facility.
	//	Stop SNMP Manager.
	//
	if(SnmpStart) {
		AdminSnmpCleanup();
	}

	return status ;
}


UINT
GetNumOfEnabledND(
		PULONG Enabled
	) {

	BOOL					bResult;
	ULONG					lNumberOfLanDisks;
	PQUERY_LANDISKS_RESULT	lanDisks = NULL;
	ULONG					idx_nd, idx_ud ;
	ULONG					enabledND ;

	
	enabledND = 0 ;

	//
	// Refresh the list of NetDisks in LDServ
	//
	bResult = OperationRefresh();
	if(bResult == FALSE) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] GetNumOfEnabledND: Error When OperationRefresh!!!\n"));
		return ERROR_GEN_FAILURE;
	}

	//
	// Query the number of NetDisk registered.
	//
	bResult = OperationQueryNetDiskNumber(&lNumberOfLanDisks);
	if(bResult == FALSE) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] GetNumOfEnabledND: Error When OperationQueryLanDiskNumber!!!\n"));
		return ERROR_GEN_FAILURE ;
	}

	if(lNumberOfLanDisks == 0) {
		*Enabled = 0 ;
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] GetNumOfEnabledND: Number of Unit Disks is 0.\n"));
		return ERROR_SUCCESS ;
	}

	//
	//	Query NetDisk list and count enabled ones.
	//
	bResult = OperationGetNetDiskList(lNumberOfLanDisks, &lanDisks);
	if(bResult == FALSE) {
		DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] GetNumOfEnabledND: Error When OperationGetNetDiskList!!!\n"));	
		return ERROR_GEN_FAILURE;
	}

	for(idx_nd = 0; idx_nd < lNumberOfLanDisks; idx_nd++) {
		for(idx_ud = 0; (LONG)idx_ud < lanDisks[idx_nd].NumberOfDisks; idx_ud++) {
			DebugPrintf(TEXT(__FILE__) TEXT("[NDInst] GetNumOfEnabledND: NetDisk %d/%d STATUS:%x\n"), idx_nd, idx_ud, lanDisks[idx_nd].unitDisks[idx_ud].m_Status);
			if(lanDisks[idx_nd].unitDisks[idx_ud].m_Status == UNITDISK_STATUS_ENABLE) {
				enabledND ++ ;
			}
		}
	}

	*Enabled = enabledND ;

	return ERROR_SUCCESS ;
}
