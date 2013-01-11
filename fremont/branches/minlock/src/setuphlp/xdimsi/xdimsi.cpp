#include "precomp.hpp"
#include "xdimsi.h"
#include "xmsitrace.h"
#include "xmsiutil.h"
#include "xdimsiproc.h"
#include "xdimsiprocdata.h"

enum { xDiProgressTick = 64 / 4 * 1024 };

const LPCWSTR XDIMSI_PROP_PROCESS_DRIVERS_PHASE = L"xDiMsiProcessDriversPhase";
const LPCWSTR XDIMSI_PROP_INITIALIZED = L"xDiMsiInitialized";

const LPCWSTR XDIMSI_ACTION_INITIALIZE = L"xDiMsiInitialize";
const LPCWSTR XDIMSI_ACTION_VALIDATE_DRIVER_SERVICES = L"xDiMsiValidateDriverServices";
const LPCWSTR XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED = L"xDiMsiProcessDriverScheduled";
const LPCWSTR XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK = L"xDiMsiProcessDriverRollback";

const XDIMSI_PROC_FUNC xMsiProcs[] = {
	xDiMsipInitializeScheduledAction, NULL,
	xDiMsipInstallFromInfSection, xDiMsipInstallFromInfSection,
	xDiMsipInstallLegacyPnpDevice, xDiMsipInstallLegacyPnpDeviceRollback,
	xDiMsipInstallPnpDeviceInf, xDiMsipInstallPnpDeviceInfRollback,
	xDiMsipInstallNetworkComponent, xDiMsipInstallNetworkComponentRollback,
	xDiMsipUninstallPnpDevice, NULL,
	xDiMsipUninstallNetworkComponent, NULL,
	xDiMsipCleanupOEMInf, NULL,
	xDiMsipStartService, NULL,
	xDiMsipStopService, NULL,
	xDiMsipQueueScheduleReboot, NULL,
	xDiMsipCheckServicesInfSection, NULL
};

C_ASSERT(_XDIMSI_PROCESS_TYPE_INVALID == (RTL_NUMBER_OF(xMsiProcs) / 2));

#ifdef _WIN64

HRESULT
xMsiPatchRunOnceEntryInWin64(
	__in MSIHANDLE hInstall);

#endif

static LPWSTR DuplicateString(LPCWSTR Data)
{
	if (NULL == Data) return NULL;
	int len = lstrlenW(Data);
	LPWSTR p = (LPWSTR) calloc(len + 1, sizeof(WCHAR));
	if (NULL == p) return NULL;
	StringCchCopyW(p, len + 1, Data);
	return p;
}

class CXdiMsiProcessRecord : public XDIMSI_PROCESS_RECORD
{
public:

	CXdiMsiProcessRecord* NextRecord;

	CXdiMsiProcessRecord()
	{
		ZeroMemory(this, sizeof(CXdiMsiProcessRecord));
	}
	
	~CXdiMsiProcessRecord()
	{
		free(this->ActionData);
		free(this->HardwareId);
		free(this->InfPath);
		free(this->RegKey);
		free(this->RegName);
	}
};

class CXdiMsiProcessRecordList
{
public:
	CXdiMsiProcessRecord* Head;
	CXdiMsiProcessRecordList() : Head(NULL)
	{

	}
	~CXdiMsiProcessRecordList()
	{
		for  (CXdiMsiProcessRecord* p = Head; p != NULL;)
		{
			CXdiMsiProcessRecord* next = p->NextRecord;
			delete p;
			p = next;
		}
	}
};

XDIMSI_PROCESS_TYPE 
xDiMsipGetProcessType(LPCWSTR TypeString)
{
	for (size_t i = 0; i < RTL_NUMBER_OF(xMsiProcessTypes); ++i)
	{
		if (0 == lstrcmpiW(TypeString, xMsiProcessTypes[i]))
		{
			return (XDIMSI_PROCESS_TYPE) i;
		}
	}
	return _XDIMSI_PROCESS_TYPE_INVALID;
}

UINT
xMsiQueryActionText(
	__in MSIHANDLE hInstall,
	__in LPCWSTR ActionName,
	__deref_out LPWSTR* Description,
	__deref_out LPWSTR* Template);

UINT
xDiMsipQueryActionText(
	__in MSIHANDLE hInstall,
	__in XDIMSI_PROCESS_TYPE ProcessType,
	__deref_out LPWSTR* Description,
	__deref_out LPWSTR* Template)
{
	LPCWSTR ActionName = xDiMsiProcessActionNames[ProcessType];
	return xMsiQueryActionText(hInstall, ActionName, Description, Template);
}

UINT
xMsiQueryActionText(
	__in MSIHANDLE hInstall,
	__in LPCWSTR ActionName,
	__deref_out LPWSTR* Description,
	__deref_out LPWSTR* Template)
{
	*Description = NULL;
	*Template = NULL;

	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);
	if (NULL == hDatabase)
	{
		return ERROR_INVALID_HANDLE_STATE;
	}

	enum { PActionField = 1 };
	enum { QDescriptionField = 1, QTemplateField };

	const LPCWSTR query = 
		L"SELECT `Description`, `Template` "
		L" FROM `ActionText`"
		L" WHERE `ActionText`.`Action` = ?";

	PMSIHANDLE hView;
	UINT ret = MsiDatabaseOpenViewW(
		hDatabase, query, &hView);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiDatabaseOpenViewW failed, ret=0x%x\n", ret);
		return ret;
	}

	PMSIHANDLE hParamRecord = MsiCreateRecord(1);
	if (NULL == hParamRecord)
	{
		XMSITRACEH(hInstall, L"MsiCreateRecord failed.\n");
		return ERROR_OUTOFMEMORY;
	}

	ret = MsiRecordSetStringW(hParamRecord, PActionField, ActionName);
	if (ret != ERROR_SUCCESS)
	{
		XMSITRACEH(hInstall, L"MsiRecordSetString failed, ret=0x%x\n", ret);
		return ret;
	}

	ret = MsiViewExecute(hView, hParamRecord);
	if (ret != ERROR_SUCCESS)
	{
		XMSITRACEH(hInstall, L"MsiViewExecute failed, ret=0x%x\n", ret);
		return ret;
	}

	PMSIHANDLE hFetchRecord;

	__try
	{
		ret = MsiViewFetch(hView, &hFetchRecord);
	}
	__except(STATUS_NO_MEMORY == GetExceptionCode() ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ret = ERROR_OUTOFMEMORY;
	}

	if (ERROR_SUCCESS != ret)
	{
		return ret;
	}

	LPWSTR actionDescription = NULL;
	DWORD actionDescriptionLength = 0;
	LPWSTR actionTemplate = NULL;
	DWORD actionTemplateLength = 0;

	ret = pxMsiRecordGetString(
		hFetchRecord, QDescriptionField, 
		&actionDescription, &actionDescriptionLength);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiRecordGetString('Description') failed, ret=0x%x\n", ret);
		return ret;
	}

	ret = pxMsiRecordGetString(
		hFetchRecord, QTemplateField, 
		&actionTemplate, &actionTemplateLength);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiRecordGetString('Template') failed, ret=0x%x\n", ret);
		free(actionDescription);
		return ret;
	}

	*Description = actionDescription;
	*Template = actionTemplate;

	return ret;
}

UINT
xMsiSetActionText(
	__in MSIHANDLE hInstall,
	__in LPCWSTR ActionName,
	__in LPCWSTR ActionDescription,
	__in LPCWSTR ActionTemplate)
{
	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);
	if (NULL == hDatabase)
	{
		XMSITRACE(L"pxMsiSetActionText: MsiGetActiveDatabase failed.\n");
		return ERROR_INVALID_HANDLE_STATE;
	}

	LPCWSTR deleteQuery = L"DELETE FROM `ActionText` WHERE `Action` = ?";

	PMSIHANDLE hView;
	UINT ret = MsiDatabaseOpenViewW(hDatabase, deleteQuery, &hView);
	if (ERROR_SUCCESS !=  ret)
	{
		XMSITRACE(L"DELETE: MsiDatabaseOpenViewW failed, ret=0x%x\n", ret);
	}
	else
	{
		PMSIHANDLE hRecord = MsiCreateRecord(1);
		if (NULL != hRecord)
		{
			MsiRecordSetStringW(hRecord, 1, ActionName);

			ret = MsiViewExecute(hView, hRecord);
			if (ERROR_SUCCESS != ret)
			{
				XMSITRACE(L"DELETE: MsiViewExecute failed, ret=0x%x\n", ret);
			}
		}
	}

	LPCWSTR insertQuery = 
		L"INSERT INTO `ActionText` "
		L" (`Action`, `Description`, `Template`) "
		L" VALUES (?, ?, ?) "
		L" TEMPORARY";

	ret = MsiDatabaseOpenViewW(hDatabase, insertQuery, &hView);
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACE(L"INSERT: MsiDatabaseOpenViewW failed, ret=0x%x\n", ret);
	}
	else
	{
		PMSIHANDLE hRecord = MsiCreateRecord(3);
		if (NULL != hRecord)
		{
			MsiRecordSetStringW(hRecord, 1, ActionName);
			MsiRecordSetStringW(hRecord, 2, ActionDescription);
			MsiRecordSetStringW(hRecord, 3, ActionTemplate);

			ret = MsiViewExecute(hView, hRecord);
			if (ERROR_SUCCESS != ret)
			{
				XMSITRACE(L"INSERT: MsiViewExecute failed, ret=0x%x\n", ret);
#ifdef _DEBUG
				PMSIHANDLE hErrorRecord = MsiGetLastErrorRecord();
				MsiProcessMessage(hInstall, INSTALLMESSAGE_WARNING, hErrorRecord);
#endif
			}
		}
	}

	return ret;
}

UINT
xDiMsipProcessDriverRecord(
	__in MSIHANDLE hInstall,
	__in const XDIMSI_PROCESS_RECORD* ProcessRecord)
{
	HRESULT hr;
	PXDIMSI_PROCESS_DATA processData = NULL;

	XMSITRACE(L"\tActionData=%s, "
		L"Type=%d, HardwareId=%s, Inf=%s, " 
		L"Flags=0x%08X, Error=%d, Ticks=%d\n", 
		ProcessRecord->ActionData,
		ProcessRecord->ProcessType, 
		ProcessRecord->HardwareId, 
		ProcessRecord->InfPath, 
		ProcessRecord->Flags, 
		ProcessRecord->ErrorNumber,
		ProcessRecord->ProgressTicks);

	CHeapPtr<WCHAR> actionDescription;
	CHeapPtr<WCHAR> actionTemplate;

	UINT ret = xDiMsipQueryActionText(
		hInstall, 
		ProcessRecord->ProcessType,
		&actionDescription,
		&actionTemplate);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"xDiMsipQueryActionText failed, ret=0x%x\n", ret);
	}
	else
	{
		XMSITRACE(L"ActionDescription=%s, Template=%s\n",
			actionDescription, actionTemplate);
	}

	hr = xDiMsipProcessDataCreate(&processData, ProcessRecord);

	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiProcessDataCreate failed, hr=0x%x\n", hr);

		return ERROR_INSTALL_FAILURE;
	}

	CHeapPtr<WCHAR> propValue;
	
	hr = xDiMsipProcessDataEncode(
		processData,
		&propValue,
		NULL);

	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, L"pxMsiProcessDataEncode failed, hr=0x%x\n", hr);
		xDiMsipProcessDataFree(processData);

		return ERROR_INSTALL_FAILURE;
	}

	//
	// Schedule a roll-back action
	//

	ret = MsiSetPropertyW(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK, 
		propValue);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiSetProperty(%ls) failed, ret=0x%x\n",
			XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK, ret);

		xDiMsipProcessDataFree(processData);

		return ERROR_INSTALL_FAILURE;
	}

	ret = xMsiSetActionText(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK,
		L"Rollback action", 
		L"[1]");

	if (ERROR_SUCCESS != ret)
	{
		// non-critical error
		XMSITRACEH(hInstall, L"xMsipSetActionText(%ls) failed, ret=0x%x\n", 
			XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK, ret);
	}

	ret = MsiDoActionW(hInstall, XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiDoActionW(%ls) failed, ret=0x%x\n",
			XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK, ret);

		xDiMsipProcessDataFree(processData);

		return ERROR_INSTALL_FAILURE;
	}

	//
	// Schedule a deferred action
	//

	ret = MsiSetPropertyW(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED, 
		propValue);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiSetProperty(%ls) failed, ret=0x%x\n",
			XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED, ret);

		xDiMsipProcessDataFree(processData);

		return ERROR_INSTALL_FAILURE;
	}

	ret = xMsiSetActionText(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED,
		actionDescription, 
		actionTemplate);

	if (ERROR_SUCCESS != ret)
	{
		// non-critical error
		XMSITRACEH(hInstall, L"pxMsiSetActionText(%ls) failed, ret=0x%x\n", 
			XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED, ret);
	}

	ret = MsiDoActionW(hInstall, XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED);
	
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiDoActionW(%ls) failed, ret=0x%x\n", 
			XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED, ret);

		xDiMsipProcessDataFree(processData);

		return ERROR_INSTALL_FAILURE;
	}

	//
	// Add a tick to the progress bar
	//

	PMSIHANDLE hProgressRecord = MsiCreateRecord(4);
	if (NULL != hProgressRecord)
	{
		MsiRecordSetInteger(hProgressRecord, 1, 3);
		MsiRecordSetInteger(hProgressRecord, 2, ProcessRecord->ProgressTicks);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hProgressRecord);
	}

	xDiMsipProcessDataFree(processData);

	//
	// Restore the action text
	//

	actionDescription.Free();
	actionTemplate.Free();

	ret = xMsiQueryActionText(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED,
		&actionDescription,
		&actionTemplate);

	if (ERROR_SUCCESS == ret)
	{
		xMsiSetActionText(
			hInstall, 
			XDIMSI_ACTION_PROCESS_DRIVER_SCHEDULED,
			actionDescription,
			actionTemplate);
	}

	actionDescription.Free();
	actionTemplate.Free();

	ret = xMsiQueryActionText(
		hInstall, 
		XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK,
		&actionDescription,
		&actionTemplate);

	if (ERROR_SUCCESS == ret)
	{
		xMsiSetActionText(
			hInstall, 
			XDIMSI_ACTION_PROCESS_DRIVER_ROLLBACK,
			actionDescription,
			actionTemplate);
	}

	return ERROR_SUCCESS;
}

UINT
xDiMsipGetRegistryData(
	__in MSIHANDLE hInstall,
	__in LPCWSTR RegistryId,
	__out DWORD* RegRoot,
	__deref_out LPWSTR* RegKeyPath,
	__deref_out LPWSTR* RegValueName)
{
	enum { RegQuery_Root = 1, RegQuery_Key, RegQuery_Name };

	const LPCWSTR registryQuery = 
		L"SELECT `Root`, `Key`, `Name` " 
		L" FROM `Registry` WHERE `Registry` = ?";

	*RegRoot = 0;
	*RegKeyPath = NULL;
	*RegValueName = NULL;

	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);
	if (NULL == hDatabase)
	{
		XMSITRACEH(hInstall, L"MsiGetActiveDatabase failed\n");
		return ERROR_INVALID_HANDLE;
	}

	PMSIHANDLE hRegView;
	UINT ret = MsiDatabaseOpenViewW(hDatabase, registryQuery, &hRegView);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiDatabaseOpenViewW(`Registry`) failed, ret=0x%x\n", ret);
		return ret;
	}

	PMSIHANDLE hRegParam = MsiCreateRecord(1);
	if (NULL == hRegParam)
	{
		XMSITRACEH(hInstall, L"MsiCreateRecord(1) failed, ret=0x%x\n", ret);
		return ret;
	}

	ret = MsiRecordSetStringW(hRegParam, 1, RegistryId);
	_ASSERT(ERROR_SUCCESS == ret);

	ret = MsiViewExecute(hRegView, hRegParam);
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiViewExecute(`Registry`) failed, ret=0x%x\n", ret);
		return ret;
	}

	PMSIHANDLE hFetchRecord;

	__try
	{
		ret = MsiViewFetch(hRegView, &hFetchRecord);
	}
	__except(STATUS_NO_MEMORY == GetExceptionCode() ? 
		EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		ret = ERROR_OUTOFMEMORY;
	}

	if (ERROR_NO_MORE_ITEMS == ret)
	{
		return ERROR_SUCCESS;
	}

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiViewFetch(`Registry`) failed, ret=0x%x\n", ret);
		return ret;
	}

	DWORD regRoot = MsiRecordGetInteger(hFetchRecord, RegQuery_Root);
	_ASSERTE(MSI_NULL_INTEGER != regRoot);

	if (-1 == regRoot)
	{
		DWORD allUsersPropLength = 0;
		CMsiProperty allUsersProp;
		ret = allUsersProp.GetPropertyW(hInstall, L"ALLUSERS", &allUsersPropLength);
		//
		// ALLUSERS may be 1, 2 or empty
		// We consider 2 or empty as per-user installation,
		// otherwise per-machine
		//
		regRoot = 0x2; // per-machine
		if (ERROR_SUCCESS == ret)
		{
			// empty
			if (0 == lstrcmp(allUsersProp, L"") ||
				0 == lstrcmpi(allUsersProp, L"2"))
			{
				regRoot = 0x1; // per-user
			}
		}
	}

	XMSITRACE(L"RegRoot=0x%x\n", regRoot);

	//
	// Key and Name are both formatted type. This requires MsiFormatRecord.
	//

	DWORD length;
	LPWSTR keyPath = NULL;
	LPWSTR keyPathFormat = NULL;

	ret = pxMsiRecordGetString(hFetchRecord, RegQuery_Key, &keyPathFormat, &length);
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"pxMsiRecordGetString(`RegQuery_Key`) failed, ret=0x%x\n", ret);
		return ret;
	}
	ret = pxMsiFormatRecord(hInstall, keyPathFormat, &keyPath, &length);
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"pxMsiFormatRecord(`RegQuery_Key`) failed, ret=0x%x\n", ret);
		free(keyPathFormat);
		return ret;
	}
	free(keyPathFormat);

	LPWSTR valueName = NULL;
	if (!MsiRecordIsNull(hFetchRecord, RegQuery_Name))
	{
		LPWSTR valueNameFormat = NULL;
		ret = pxMsiRecordGetString(hFetchRecord, RegQuery_Name, &valueNameFormat, &length);
		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"pxMsiRecordGetString(`RegQuery_Key`) failed, ret=0x%x\n", ret);
			free(keyPath);
			return ret;
		}
		ret = pxMsiFormatRecord(hInstall, valueNameFormat, &valueName, &length);
		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"pxMsiFormatRecord(`RegQuery_Key`) failed, ret=0x%x\n", ret);
			free(keyPath);
			free(valueNameFormat);
			return ret;
		}
		free(valueNameFormat);
	}

	XMSITRACE(L" RegRoot=0x%x, KeyPath=%ls, ValueName=%ls\n", 
		regRoot, keyPath, valueName);

	*RegRoot = regRoot;
	*RegKeyPath = keyPath;
	*RegValueName = valueName;

	return ERROR_SUCCESS;
}

UINT
xDiMsipProcessDrivers(
	__in MSIHANDLE hInstall, 
	__in INT Phase)
{
	//
	// Display action name in progress bar in immediate action
	// (We should do this manually in immediate actions)
	//

	DWORD actionPropertyLength;
	CMsiProperty actionProperty;
	UINT ret = actionProperty.GetPropertyW(hInstall, L"Action", &actionPropertyLength);

	if (ERROR_SUCCESS == ret && actionPropertyLength > 0)
	{
		XMSITRACE(L"\tAction=%ls\n", static_cast<LPCWSTR>(actionProperty));

		LPCWSTR actionName = actionProperty;
		CHeapPtr<WCHAR> actionDescription;
		CHeapPtr<WCHAR> actionTemplate;

		ret = xMsiQueryActionText(hInstall, actionProperty, &actionDescription, &actionTemplate);
		if (ERROR_SUCCESS == ret)
		{
			PMSIHANDLE hProgressRecord = MsiCreateRecord(3);
			MsiRecordSetStringW(hProgressRecord, 1, actionName);
			MsiRecordSetStringW(hProgressRecord, 2, actionDescription);
			MsiRecordSetStringW(hProgressRecord, 3, actionTemplate);
			MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONSTART, hProgressRecord);
		}
	}

	//
	// Database query
	//

	PMSIHANDLE hDatabase = MsiGetActiveDatabase(hInstall);

	if (0 == hDatabase)
	{
		XMSITRACEH(hInstall, L"MsiGetActiveDatabase failed.\n");
		return ERROR_INSTALL_FAILURE;
	}

	enum { 
		QTypeField = 1,
		QHardwareIdField,
		QInfField,
		QFlagsField,
		QConditionField,
		QErrorField,
		QIdField,
		QActionDataField,
		QRegistry
	};

	const LPCWSTR query = 
		L"SELECT `Type`, `HardwareId`, `INF`, `Flags`, "
		L" `Condition`, `Error`, `Id`, `ActionData`, "
		L" `Registry_` "
		L" FROM `xDriverInstall`" 
		L" WHERE `xDriverInstall`.`Phase` = ? "
		L" ORDER BY `xDriverInstall`.`Sequence` ";

	PMSIHANDLE hView;
	ret = MsiDatabaseOpenViewW(hDatabase, query, &hView);

	if (ret != ERROR_SUCCESS)
	{
		XMSITRACEH(hInstall, L"MsiDatabaseOpenViewW failed, ret=0x%x\n", ret);
		return ERROR_INSTALL_FAILURE;
	}

	PMSIHANDLE hParamRecord = MsiCreateRecord(1);
	if (NULL == hParamRecord)
	{
		XMSITRACEH(hInstall, L"MsiCreateRecord failed.\n");
		return ERROR_INSTALL_FAILURE;
	}

	ret = MsiRecordSetInteger(hParamRecord, 1, Phase);
	if (ret != ERROR_SUCCESS)
	{
		XMSITRACEH(hInstall, L"MsiRecordSetInteger failed, ret=0x%x\n", ret);
		return ERROR_INSTALL_FAILURE;
	}

	ret = MsiViewExecute(hView, hParamRecord);
	if (ret != ERROR_SUCCESS)
	{
		XMSITRACEH(hInstall, L"MsiViewExecute failed, ret=0x%x\n", ret);
		return ERROR_INSTALL_FAILURE;
	}

	CXdiMsiProcessRecordList processRecordList;
	CXdiMsiProcessRecord* currentRecord = NULL;

	while (TRUE)
	{
		PMSIHANDLE hFetchRecord;

		__try
		{
			ret = MsiViewFetch(hView, &hFetchRecord);
		}
		__except(STATUS_NO_MEMORY == GetExceptionCode() ? 
			EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			ret = ERROR_OUTOFMEMORY;
		}

		// XMSITRACE(L"(FetchRecord=%d)\n", hFetchRecord);

		if (ERROR_NO_MORE_ITEMS == ret)
		{
			break;
		}

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiViewFetch failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		//
		// Evaluate the condition first
		//

		DWORD length = 0;
		CHeapPtr<WCHAR> condition;

		ret = pxMsiRecordGetString(
			hFetchRecord, QConditionField, &condition, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetString(`Condition`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		MSICONDITION e = MsiEvaluateConditionW(hInstall, condition);
		
		if (MSICONDITION_ERROR == e)
		{
			XMSITRACEH(hInstall, L"MsiEvaluateCondition error, condition=%ls\n", condition);
			return ERROR_INSTALL_FAILURE;
		}
		else if (MSICONDITION_FALSE == e)
		{
			//
			// Skip this record
			//
			continue;
		}

		condition.Free();

		//
		// Process Type String Field
		//

		CHeapPtr<WCHAR> processTypeString;

		ret = pxMsiRecordGetString(
			hFetchRecord, QTypeField, &processTypeString, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetStringW(`Type`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XDIMSI_PROCESS_TYPE processType = xDiMsipGetProcessType(processTypeString);

		if (_XDIMSI_PROCESS_TYPE_INVALID == processType)
		{
			XMSITRACEH(hInstall, L"Invalid process type, type=%ls\n", processTypeString);
			return ERROR_INSTALL_FAILURE;
		}

		processTypeString.Free();

		//
		// INF Path Format
		//

		CHeapPtr<WCHAR> infPathFormat;

		ret = pxMsiRecordGetString(
			hFetchRecord, QInfField, &infPathFormat, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetStringW(`INF`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XMSITRACE(L"InfPathFormat=%ls\n", infPathFormat);

		//
		// Inf Path
		//

		CHeapPtr<WCHAR> infPath;

		ret = pxMsiFormatRecord(hInstall, infPathFormat, &infPath, NULL);
		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiFormatRecord(`INF`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		infPathFormat.Free();

		XMSITRACE(L"infPath=%ls\n", infPath);

		CHeapPtr<WCHAR> hardwareIdFormat;

		ret = pxMsiRecordGetString(
			hFetchRecord, QHardwareIdField, &hardwareIdFormat, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetStringW(`HardwareId`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XMSITRACE(L"HardwareIdFormat=%ls\n", hardwareIdFormat);

		CHeapPtr<WCHAR> hardwareId;

		ret = pxMsiFormatRecord(hInstall, hardwareIdFormat, &hardwareId, NULL);
		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiFormatRecord(`HardwareId`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XMSITRACE(L"HardwareId=%ls\n", hardwareId);

		hardwareIdFormat.Free();

		//
		// ProcessFlags
		//

		DWORD processFlags = (DWORD) MsiRecordGetInteger(hFetchRecord, QFlagsField);
		
		if (MSI_NULL_INTEGER == processFlags)
		{
			processFlags = 0;
		}

		//
		// Error
		//

		int errorNumber = MsiRecordGetInteger(hFetchRecord, QErrorField);

		if (MSI_NULL_INTEGER == errorNumber)
		{
			errorNumber = 0;
		}

		//
		// Action Id
		//

		CHeapPtr<WCHAR> actionId;

		ret = pxMsiRecordGetString(hFetchRecord, QIdField, &actionId, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetStringW(`Id`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XMSITRACE(L"ActionId=%ls\n", actionId);

		//
		// ActionData
		//

		CHeapPtr<WCHAR> actionData;

		ret = pxMsiRecordGetString(hFetchRecord, QActionDataField, &actionData, &length);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiRecordGetStringW(`ActionData`) failed, ret=0x%x\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		XMSITRACE(L"ActionData=%ls\n", actionData);

		DWORD regRoot = 0;

		CHeapPtr<WCHAR> regKeyPath;
		CHeapPtr<WCHAR> regValueName;

		//
		// Registry Id 
		//

		if (!MsiRecordIsNull(hFetchRecord, QRegistry))
		{
			CHeapPtr<WCHAR> registryId;
			ret = pxMsiRecordGetString(hFetchRecord, QRegistry, &registryId, &length);
			if (ERROR_SUCCESS != ret)
			{
				XMSITRACEH(hInstall, L"MsiRecordGetStringW(`Registry_`) failed, ret=0x%x\n", ret);
				return ERROR_INSTALL_FAILURE;
			}
			
			ret = xDiMsipGetRegistryData(
				hInstall, registryId, 
				&regRoot, &regKeyPath, &regValueName);

			if (ERROR_SUCCESS != ret)
			{
				XMSITRACEH(hInstall, L"xDiMsiGetRegistryData failed, ret=0x%x\n", ret);
				return ERROR_INSTALL_FAILURE;
			}
		}

		//
		// Process the record
		//

		CXdiMsiProcessRecord* newProcessRecord = new CXdiMsiProcessRecord();
		if (NULL == newProcessRecord)
		{
			XMSITRACEH(hInstall, L"CXMsiProcessRecord creation failed, ret=0x%x\n", ERROR_OUTOFMEMORY);
			return ERROR_INSTALL_FAILURE;
		}

		if (NULL == processRecordList.Head)
		{
			currentRecord = newProcessRecord;
			processRecordList.Head = currentRecord;
		}
		else
		{
			currentRecord->NextRecord = newProcessRecord;
			currentRecord = newProcessRecord;
		}

		currentRecord->ProcessType = processType;
		currentRecord->ActionData = DuplicateString(actionData);
		currentRecord->HardwareId = DuplicateString(hardwareId);
		currentRecord->InfPath = DuplicateString(infPath);
		currentRecord->Flags = processFlags;
		currentRecord->ErrorNumber = errorNumber;
		currentRecord->RegRoot = regRoot;
		currentRecord->RegKey = DuplicateString(regKeyPath);
		currentRecord->RegName = DuplicateString(regValueName);
		currentRecord->ProgressTicks = 8 * xDiProgressTick;
	}

	//
	// Pass 0 for validating driver services before installation
	//

	WCHAR initProperty[4];
	DWORD initPropertyLength = RTL_NUMBER_OF(initProperty);
	ret = MsiGetPropertyW(hInstall, XDIMSI_PROP_INITIALIZED, initProperty, &initPropertyLength);

	if (ERROR_SUCCESS != ret || 0 == initPropertyLength)
	{
		//
		// XDIMSI_PROP_INITIALIZED is not set
		//
		XDIMSI_PROCESS_RECORD record;
		ZeroMemory(&record, sizeof(XDIMSI_PROCESS_RECORD));
		record.ProcessType = _XDIMSI_PROCESS_INITIALIZE;

		ret = MsiSetPropertyW(hInstall, XDIMSI_PROP_INITIALIZED, L"1");

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"MsiSetProperty(XDIMSI_PROP_INITIALIZED) failed, ret=0x%X\n", ret);
			return ERROR_INSTALL_FAILURE;
		}

		ret = xDiMsipProcessDriverRecord(hInstall, &record);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"pxMsiProcessRecord(pass0) failed, ret=0x%X\n", ret);
			return ERROR_INSTALL_FAILURE;
		}
	}

	//
	// Pass 1 for validating driver services before installation
	//

	for (currentRecord = processRecordList.Head; 
		NULL != currentRecord; 
		currentRecord = currentRecord->NextRecord)
	{
		XDIMSI_PROCESS_RECORD record;
		ZeroMemory(&record, sizeof(XDIMSI_PROCESS_RECORD));

		record.ActionData = currentRecord->ActionData;
		record.ProcessType = currentRecord->ProcessType;
		record.HardwareId = currentRecord->HardwareId;
		record.InfPath = currentRecord->InfPath;
		// record.Flags = currentRecord->Flags;

		switch (record.ProcessType)
		{
		case XDIMSI_PROCESS_INSTALL_FROM_INF_SECTION:
			record.ProcessType = _XDIMSI_PROCESS_CHECK_SERVICES_INF_SECTION;
			record.Flags = XDIMSI_CHECK_SERVICES_USE_FORCEREBOOT;
			record.ProgressTicks = 2 * xDiProgressTick;
			break;
		case XDIMSI_PROCESS_INSTALL_LEGACY_PNP_DEVICE:
		case XDIMSI_PROCESS_INSTALL_PNP_DEVICE_INF:
		case XDIMSI_PROCESS_INSTALL_NETWORK_COMPONENT:
			record.ProcessType = _XDIMSI_PROCESS_CHECK_SERVICES_INF_SECTION;
			record.Flags = XDIMSI_CHECK_SERVICES_USE_HARDWARE_ID | 
				XDIMSI_CHECK_SERVICES_USE_FORCEREBOOT;
			record.ProgressTicks = 2 * xDiProgressTick;
			break;
		case XDIMSI_PROCESS_UNINSTALL_PNP_DEVICE:
		case XDIMSI_PROCESS_CLEANUP_OEM_INF:
		default:
			record.ProcessType = _XDIMSI_PROCESS_TYPE_INVALID;
		}

		if (_XDIMSI_PROCESS_TYPE_INVALID != record.ProcessType)
		{
			ret = xDiMsipProcessDriverRecord(hInstall, &record);

			if (ERROR_SUCCESS != ret)
			{
				XMSITRACEH(hInstall, L"pxMsiProcessRecord(pass1) failed, ret=0x%X\n", ret);
				return ERROR_INSTALL_FAILURE;
			}
		}
	}

	//
	// Pass 2 for installation
	//

	for (currentRecord = processRecordList.Head; 
		NULL != currentRecord; 
		currentRecord = currentRecord->NextRecord)
	{
		ret = xDiMsipProcessDriverRecord(hInstall, currentRecord);

		if (ERROR_SUCCESS != ret)
		{
			XMSITRACEH(hInstall, L"pxMsiProcessRecord(pass2) failed, ret=0x%X\n", ret);
			return ERROR_INSTALL_FAILURE;
		}
	}

	//
	// Pass 3 for service status check for removal
	//

	for (currentRecord = processRecordList.Head; 
		NULL != currentRecord; 
		currentRecord = currentRecord->NextRecord)
	{
		XDIMSI_PROCESS_RECORD record;
		ZeroMemory(&record, sizeof(XDIMSI_PROCESS_RECORD));

		record.ActionData = currentRecord->ActionData;
		record.ProcessType = currentRecord->ProcessType;
		record.HardwareId = currentRecord->HardwareId;
		record.InfPath = currentRecord->InfPath;
		// record.Flags = currentRecord->Flags;

		switch (record.ProcessType)
		{
		case XDIMSI_PROCESS_INSTALL_FROM_INF_SECTION:
			record.ProcessType = _XDIMSI_PROCESS_CHECK_SERVICES_INF_SECTION;
			// temporarily record is not freed
			record.Flags = 0;
			record.ProgressTicks = 2 * xDiProgressTick;
			break;

		case XDIMSI_PROCESS_INSTALL_LEGACY_PNP_DEVICE:
		case XDIMSI_PROCESS_INSTALL_PNP_DEVICE_INF:
		case XDIMSI_PROCESS_INSTALL_NETWORK_COMPONENT:
			
		case XDIMSI_PROCESS_UNINSTALL_PNP_DEVICE:
		case XDIMSI_PROCESS_CLEANUP_OEM_INF:

		default:
			record.ProcessType = _XDIMSI_PROCESS_TYPE_INVALID;
		}

		if (_XDIMSI_PROCESS_TYPE_INVALID != record.ProcessType)
		{
			ret = xDiMsipProcessDriverRecord(hInstall, &record);

			if (ERROR_SUCCESS != ret)
			{
				XMSITRACEH(hInstall, L"pxMsiProcessRecord(pass3) failed, ret=0x%X\n", ret);
				return ERROR_INSTALL_FAILURE;
			}
		}
	}

	return ERROR_SUCCESS;
}

UINT
MSICALL
xDiMsiProcessDrivers(
	__in MSIHANDLE hInstall)
{
	//
	// Resolve the current phase and call the actual function (pxMsiProcessDrivers)
	// to process the device driver installation
	//

	WCHAR buffer[64];
	DWORD bufferLength = RTL_NUMBER_OF(buffer);

	UINT ret = MsiGetPropertyW(
		hInstall, XDIMSI_PROP_PROCESS_DRIVERS_PHASE, buffer, &bufferLength);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"GetProperty('XDIMSI_PROP_PROCESS_DRIVERS_PHASE') failed, code=0x%x\n", ret);
		return ERROR_INSTALL_FAILURE;
	}

	INT phase = 0;
	if (!StrToIntExW(buffer, STIF_SUPPORT_HEX, &phase))
	{
		XMSITRACEH(hInstall, L"Property 'XDIMSI_PROP_PROCESS_DRIVERS_PHASE' is not an integer.\n");
		// invalid phase
		return ERROR_INSTALL_FAILURE;
	}

	return xDiMsipProcessDrivers(hInstall, phase);
}

UINT
MSICALL
xDiMsiProcessDriverScheduled(
	__in MSIHANDLE hInstall)
{
	CMsiProperty cadata;
	UINT ret = cadata.GetPropertyW(hInstall, IPROPNAME_CUSTOMACTIONDATA, NULL);
	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, 
			L"MsiGetProperty(IPROPNAME_CUSTOMACTIONDATA) failed, ret=0x%x\n", ret);
		return ERROR_INSTALL_FAILURE;
	}

	PXDIMSI_PROCESS_DATA processData = NULL;
	DWORD size;
	HRESULT hr = xDiMsipProcessDataDecode(cadata, &processData, &size);

	if (FAILED(hr))
	{
		XMSITRACEH(hInstall, 
			L"pxMsiProcessDataDecode(IPROPNAME_CUSTOMACTIONDATA) failed, hr=0x%x\n", hr);
		return ERROR_INSTALL_FAILURE;
	}

	if (size < FIELD_OFFSET(XDIMSI_PROCESS_DATA, AdditionalData))
	{
		XMSITRACEH(hInstall, 
			L"pxMsiProcessDataDecode(IPROPNAME_CUSTOMACTIONDATA) failed, hr=0x%x\n", hr);
		free(processData);
		return ERROR_INSTALL_FAILURE;

	}

	if (processData->Version != sizeof(XDIMSI_PROCESS_DATA))
	{
		free(processData);
		return ERROR_INSTALL_FAILURE;
	}

	LPWSTR actionName = (LPWSTR) pxOffsetOf(processData, processData->ActionOffset);
	LPWSTR actionData = (LPWSTR) pxOffsetOf(processData, processData->ActionDataOffset);
	LPWSTR hardwareId = (LPWSTR)pxOffsetOf(processData, processData->HardwareIdOffset);
	LPWSTR infPath = (LPWSTR)pxOffsetOf(processData, processData->InfPathOffset);
	LPWSTR regKey = (LPWSTR)pxOffsetOf(processData, processData->RegKeyOffset);
	LPWSTR regName = (LPWSTR)pxOffsetOf(processData, processData->RegNameOffset);;

	XMSITRACEH(hInstall, L"  %ls(S) Data=%s, Type=%d, HardwareId=%s, Inf=%s, Ticks=%d\n",
		actionName, actionData, processData->ProcessType, hardwareId, infPath, processData->ProgressTicks);

	//
	// Use explicit progress messages
	//

	PMSIHANDLE hProgressRecord = MsiCreateRecord(4);
	if (hProgressRecord)
	{
		MsiRecordSetInteger(hProgressRecord, 1, 1);
		MsiRecordSetInteger(hProgressRecord, 2, 0);
		MsiRecordSetInteger(hProgressRecord, 3, 0);
		MsiRecordSetInteger(hProgressRecord, 4, 0);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hProgressRecord);
	}

	//
	// Publish event data
	//

	PMSIHANDLE hActionDataRecord = MsiCreateRecord(3);
	if (NULL != hActionDataRecord)
	{
		MsiRecordSetStringW(hActionDataRecord, 1, actionData);
		MsiRecordSetStringW(hActionDataRecord, 2, hardwareId);
		MsiRecordSetStringW(hActionDataRecord, 3, infPath);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_ACTIONDATA, hActionDataRecord);
	}

	if (processData->Flags)
	{
		XMSITRACEH(hInstall, L"  Flags=0x%x\n", processData->Flags);
	}

	if (0 != processData->ErrorNumber)
	{
		XMSITRACEH(hInstall, L"  ErrorDialog=%d\n", processData->ErrorNumber);
	}

	if (regKey && regKey[0])
	{
		XMSITRACEH(hInstall, L"  Registry Root=0x%x, Key=%s, Name=%s\n",
			processData->RegRoot, regKey, regName);
	}

	if (processData->ProcessType >= _XDIMSI_PROCESS_TYPE_INVALID)
	{
		free(processData);
		return ERROR_INSTALL_FAILURE;
	}

	if (!MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK) && hProgressRecord)
	{
		MsiRecordSetInteger(hProgressRecord, 1, 2);
		MsiRecordSetInteger(hProgressRecord, 2, processData->ProgressTicks / 2);
		MsiRecordSetInteger(hProgressRecord, 3, 0);
		MsiRecordSetInteger(hProgressRecord, 4, 0);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hProgressRecord);
	}

	XDIMSI_PROCESS_RECORD processRecord;
	ZeroMemory(&processRecord, sizeof(XDIMSI_PROCESS_RECORD));
	processRecord.ProcessType = processData->ProcessType;
	processRecord.HardwareId = hardwareId;
	processRecord.InfPath = infPath;
	processRecord.Flags = processData->Flags;
	processRecord.ErrorNumber = processData->ErrorNumber;
	processRecord.RegRoot = processData->RegRoot;
	processRecord.RegKey = regKey;
	processRecord.RegName = regName;

	if (MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK))
	{
		XDIMSI_PROC_FUNC pfn = *xMsiProcs[processData->ProcessType*2 + 1];
		if (NULL != pfn)
		{
			ret = (*pfn)(hInstall, &processRecord);
		}
		else
		{
			ret = ERROR_SUCCESS;
		}
	}
	else
	{
		XDIMSI_PROC_FUNC pfn = *xMsiProcs[processData->ProcessType*2];
		_ASSERTE(NULL != pfn);
		ret = (*pfn)(hInstall, &processRecord);
	}

	//
	// Progress a tick to the progress bar
	//

	if (!MsiGetMode(hInstall, MSIRUNMODE_ROLLBACK) && hProgressRecord)
	{
		MsiRecordSetInteger(hProgressRecord, 1, 2);
		MsiRecordSetInteger(hProgressRecord, 2, processData->ProgressTicks / 2);
		MsiRecordSetInteger(hProgressRecord, 3, 0);
		MsiRecordSetInteger(hProgressRecord, 4, 0);
		MsiProcessMessage(hInstall, INSTALLMESSAGE_PROGRESS, hProgressRecord);
	}

	free(processData);

	return ret;		
}

UINT MSICALL xDiMsiProcessDrivers1(MSIHANDLE hInstall) { return xDiMsipProcessDrivers(hInstall, 1); }
UINT MSICALL xDiMsiProcessDrivers2(MSIHANDLE hInstall) { return xDiMsipProcessDrivers(hInstall, 2); }
UINT MSICALL xDiMsiProcessDrivers3(MSIHANDLE hInstall) { return xDiMsipProcessDrivers(hInstall, 3); }
UINT MSICALL xDiMsiProcessDrivers4(MSIHANDLE hInstall) { return xDiMsipProcessDrivers(hInstall, 4); }
UINT MSICALL xDiMsiProcessDrivers5(MSIHANDLE hInstall) { return xDiMsipProcessDrivers(hInstall, 5); }

//
// Immediate action sequenced after InstallFinalize
// to update Scheduled Reboot parameter from deferred custom actions
//
UINT
MSICALL
xDiMsiUpdateScheduledReboot(
	__in MSIHANDLE hInstall)
{
	if (pxMsiIsScheduleRebootQueued(hInstall))
	{
		XMSITRACEH(hInstall, L"xDiMsiUpdateScheduledReboot: Reboot is required to complete the installation\n");
		return MsiSetMode(hInstall, MSIRUNMODE_REBOOTATEND, TRUE);
	}
	else
	{
		XMSITRACEH(hInstall, L"xDiMsiUpdateScheduledReboot: No reboot is necessary to complete the installation\n");
		return ERROR_SUCCESS;
	}
}

UINT
MSICALL
xDiMsiUpdateForceReboot(
	__in MSIHANDLE hInstall)
{
	if (pxMsiIsForceRebootQueued(hInstall))
	{
		XMSITRACEH(hInstall, L"xDiMsiUpdateForceReboot: Reboot is required to continue the installation\n");

		UINT ret = MsiDoActionW(hInstall, L"ForceReboot");

#ifdef _WIN64
		//
		// Move HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Installer\RunOnceEntries
		// where value data includes the package code {XXXX}
		// to HKLM\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Installer\RunOnceEntries
		//
		xMsiPatchRunOnceEntryInWin64(hInstall);
#endif
		return ret;
	}
	else
	{
		XMSITRACEH(hInstall, L"xDiMsiUpdateForceReboot: No reboot is necessary to continue the installation\n");
		return ERROR_SUCCESS;
	}
}

#ifdef _WIN64

HRESULT
xMsiPatchRunOnceEntryInWin64(
	__in MSIHANDLE hInstall)
{
	HRESULT hr;
	WCHAR productCode[48];
	DWORD productCodeLength = RTL_NUMBER_OF(productCode);
	UINT ret = MsiGetProperty(hInstall, L"ProductCode", productCode, &productCodeLength);

	if (ERROR_SUCCESS != ret)
	{
		XMSITRACEH(hInstall, L"MsiGetProperty('ProductCode') failed, ret=0x%x\n", ret);
		return ret;
	}

	XMSITRACE(L"\tProductCode=%ls\n", productCode);

	HKEY entries32KeyHandle = (HKEY) INVALID_HANDLE_VALUE;
	HKEY runonce32KeyHandle = (HKEY) INVALID_HANDLE_VALUE;

	LONG result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Installer\\RunOnceEntries",
		0, KEY_ALL_ACCESS, &entries32KeyHandle);

	if (ERROR_SUCCESS != result) 
	{
		hr = HRESULT_FROM_WIN32(result);
		XMSITRACEH(hInstall, L"RegOpenKeyEx(Wow6432Node...RunOnceEntries) failed, hr=0x%x\n", hr);
		return hr;
	}

	DWORD valueNameAllocLength = 0;
	DWORD roentryValueDataAllocBytes = 0;

	result = RegQueryInfoKeyW(
		entries32KeyHandle, 
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		&valueNameAllocLength,
		&roentryValueDataAllocBytes,
		NULL,
		NULL);

	if (ERROR_SUCCESS != result) 
	{
		hr = HRESULT_FROM_WIN32(result);
		XMSITRACEH(hInstall, L"RegQueryInfoKey(Wow6432Node...RunOnceEntries) failed, hr=0x%x\n", hr);
		return hr;
	}

	// returned length does not include the terminating null character
	++valueNameAllocLength; 

	DWORD valueNameLength = valueNameAllocLength;
	DWORD roentryValueDataBytes = roentryValueDataAllocBytes;

	LPWSTR valueName = (LPWSTR) calloc(valueNameAllocLength, sizeof(WCHAR));

	if (NULL == valueName)
	{
		hr = E_OUTOFMEMORY;
		XMSITRACEH(hInstall, L"EOUTOFMEMORY %d bytes, hr=0x%x\n", valueNameAllocLength * sizeof(WCHAR), hr);
		RegCloseKey(entries32KeyHandle);
		return hr;
	}

	DWORD roentryValueType;
	LPWSTR roentryValueData = (LPWSTR) calloc(roentryValueDataAllocBytes, sizeof(BYTE));

	if (NULL == valueName)
	{
		hr = E_OUTOFMEMORY;
		XMSITRACEH(hInstall, L"EOUTOFMEMORY %d bytes, hr=0x%x\n", roentryValueDataAllocBytes * sizeof(BYTE), hr);
		free(valueName);
		RegCloseKey(entries32KeyHandle);
		return hr;
	}

	DWORD runonceValueDataBytes = 0;
	DWORD runonceValueType = REG_NONE;
	LPWSTR runonceValueData = NULL;

	for (DWORD index = 0; ; ++index)
	{
		result = RegEnumValueW(
			entries32KeyHandle, index, 
			valueName, &valueNameLength, 
			NULL, &roentryValueType, 
			(LPBYTE)roentryValueData, &roentryValueDataBytes);

		if (ERROR_NO_MORE_ITEMS == result) 
		{
			break;
		}
		if (ERROR_SUCCESS != result) continue;
		if (REG_SZ != roentryValueType) continue;

		XMSITRACE(L"RunOnceEntries: %ls=%ls\n", valueName, roentryValueData);

		if (NULL != StrStrI(roentryValueData, productCode))
		{
			XMSITRACEH(hInstall, L"Patching RunOnceEntries: %ls=%ls\n", valueName, roentryValueData);
			
			result = RegOpenKeyExW(
				HKEY_LOCAL_MACHINE,
				L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
				0,
				KEY_ALL_ACCESS,
				&runonce32KeyHandle);

			if (ERROR_SUCCESS != result)
			{
				hr = HRESULT_FROM_WIN32(result);
				XMSITRACEH(hInstall, L"RegOpenKeyExW(Wow6432Node...RunOnce) failed, hr=0x%x\n", hr);
				free(roentryValueData);
				free(valueName);
				RegCloseKey(entries32KeyHandle);
				return hr;
			}

			runonceValueDataBytes = 0;

			result = RegQueryValueExW(
				runonce32KeyHandle, 
				valueName, NULL, &runonceValueType, 
				NULL, &runonceValueDataBytes);
			
			if (ERROR_SUCCESS != result)
			{
				hr = HRESULT_FROM_WIN32(result);
				XMSITRACEH(hInstall, L"RegQueryValueExW(Wow6432Node...RunOnce) failed, hr=0x%x\n", hr);
				RegCloseKey(runonce32KeyHandle);
				free(roentryValueData);
				free(valueName);
				RegCloseKey(entries32KeyHandle);
				return hr;
			}

			runonceValueData = (LPWSTR) calloc(runonceValueDataBytes, sizeof(BYTE));

			if (NULL == runonceValueData)
			{
				hr = E_OUTOFMEMORY;
				XMSITRACEH(hInstall, L"EOUTOFMEMORY %d bytes, hr=0x%x\n", runonceValueDataBytes * sizeof(BYTE), hr);
				RegCloseKey(runonce32KeyHandle);
				free(roentryValueData);
				free(valueName);
				RegCloseKey(entries32KeyHandle);
				return hr;
			}

			result = RegQueryValueExW(
				runonce32KeyHandle, 
				valueName, NULL, &runonceValueType, 
				(LPBYTE) runonceValueData, &runonceValueDataBytes);

			if (ERROR_SUCCESS != result)
			{
				hr = HRESULT_FROM_WIN32(result);
				XMSITRACEH(hInstall, L"RegQueryValueExW(Wow6432Node...RunOnce) failed, hr=0x%x\n", hr);
				free(runonceValueData);
				RegCloseKey(runonce32KeyHandle);
				free(roentryValueData);
				free(valueName);
				RegCloseKey(entries32KeyHandle);
				return hr;
			}

			break;
		}
	}

	//
	// Now let's create a key
	//
	hr = S_OK;
	if (runonceValueData)
	{
		HKEY runonce64KeyHandle;

		result = RegCreateKeyExW(
			HKEY_LOCAL_MACHINE, 
			L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
			NULL,
			NULL,
			REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS,
			NULL,
			&runonce64KeyHandle,
			NULL);

		if (ERROR_SUCCESS != result)
		{
			hr = HRESULT_FROM_WIN32(result);
			XMSITRACEH(hInstall, L"RegCreateKeyExW(RunOnce) failed, hr=0x%x\n", hr);
		}
		else
		{
			result = RegSetValueExW(
				runonce64KeyHandle,
				valueName,
				0,
				runonceValueType,
				(LPBYTE) runonceValueData,
				runonceValueDataBytes);

			if (ERROR_SUCCESS != result)
			{
				hr = HRESULT_FROM_WIN32(result);
				XMSITRACEH(hInstall, L"RegSetValueExW(RunOnce) failed, hr=0x%x\n", hr);				
			}
			else
			{
				HKEY entries64KeyHandle;

				result = RegCreateKeyExW(
					HKEY_LOCAL_MACHINE, 
					L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\RunOnceEntries",
					NULL,
					NULL,
					REG_OPTION_NON_VOLATILE,
					KEY_ALL_ACCESS,
					NULL,
					&entries64KeyHandle,
					NULL);

				if (ERROR_SUCCESS != result)
				{
					hr = HRESULT_FROM_WIN32(result);
					XMSITRACEH(hInstall, L"RegCreateKeyExW(RunOnceEntries) failed, hr=0x%x\n", hr);				
				}
				else
				{
					result = RegSetValueExW(
						entries64KeyHandle,
						valueName,
						0,
						roentryValueType,
						(LPBYTE) roentryValueData,
						roentryValueDataBytes);

					if (ERROR_SUCCESS != result)
					{
						hr = HRESULT_FROM_WIN32(result);
						XMSITRACEH(hInstall, L"RegSetValueExW(RunOnceEntries) failed, hr=0x%x\n", hr);				
					}

					_ASSERTE(INVALID_HANDLE_VALUE != runonce32KeyHandle);
					_ASSERTE(INVALID_HANDLE_VALUE != entries32KeyHandle);

					RegDeleteValueW(runonce32KeyHandle, valueName);
					RegDeleteValueW(entries32KeyHandle, valueName);

					RegCloseKey(entries64KeyHandle);
				}
			}

			RegCloseKey(runonce64KeyHandle);
		}

		_ASSERTE(INVALID_HANDLE_VALUE != runonce32KeyHandle);
		RegCloseKey(runonce32KeyHandle);
	}

	_ASSERTE(INVALID_HANDLE_VALUE != entries32KeyHandle);
	RegCloseKey(entries32KeyHandle);

	free(runonceValueData); // free does nothing when p is null
	free(roentryValueData);
	free(valueName);

	return hr;
}

#endif
