#include <windows.h>
#include <tchar.h>
#include "../xtaskdlg.h"
#include <process.h>

typedef struct _TEST_PARAM {
	BOOL UseImpl;
} TEST_PARAM, *PTEST_PARAM;

DWORD WINAPI ThreadStart(LPVOID Parameter)
{
	PTEST_PARAM testParam = static_cast<PTEST_PARAM>(Parameter);

	int selected;
	HRESULT hr;

	//hr = xTaskDialog(
	//	NULL,
	//	NULL,
	//	L"NDAS Device Management",
	//	L"Please wait while the device is being mounted...",
	//	L"",
	//	TDCBF_CLOSE_BUTTON,
	//	TD_INFORMATION_ICON,
	//	&selected);

	TASKDIALOG_BUTTON buttons[] = {
		IDOK, L"Launch Disk Management\nYou can create and update partitions.\n(Requires Administrative Privileges)"
	};

	TASKDIALOGCONFIG taskConfig = { 0 };

	taskConfig.cbSize = sizeof(TASKDIALOGCONFIG);
	taskConfig.hwndParent = NULL;
	taskConfig.hInstance = NULL;
	taskConfig.dwFlags = 
		TDF_ENABLE_HYPERLINKS |
		TDF_SHOW_MARQUEE_PROGRESS_BAR |
		TDF_EXPANDED_BY_DEFAULT | 
		TDF_USE_COMMAND_LINKS |
		TDF_ALLOW_DIALOG_CANCELLATION;
	taskConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
	taskConfig.cButtons = RTL_NUMBER_OF(buttons);
	taskConfig.pButtons = buttons;
	taskConfig.pszWindowTitle = L"NDAS Device Management";
	taskConfig.pszMainIcon = TD_INFORMATION_ICON;
	taskConfig.pszMainInstruction = L"Please wait while the device is being mounted...";
	taskConfig.pszContent = L"NDAS device is mounted but there are no available partition.";
	//taskConfig.pszExpandedControlText = L"Collapse";
	//taskConfig.pszCollapsedControlText = L"Expand";
	// taskConfig.pszExpandedInformation = L"<A>Click here to Launch Disk Management</A>";
	// taskConfig.pszFooter = L"<A>Launch Disk Management</A>";
	taskConfig.nDefaultButton = IDCLOSE;

	int selectedButton, selectedRadioButton;
	BOOL verificationFlagChecked;

	if (testParam->UseImpl)
	{
		hr = xTaskDialogIndirectImp(
			&taskConfig,
			&selectedButton,
			&selectedRadioButton,
			&verificationFlagChecked);
	}
	else
	{
		hr = xTaskDialogIndirect(
			&taskConfig,
			&selectedButton,
			&selectedRadioButton,
			&verificationFlagChecked);
	}


	return 0;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	HANDLE threadHandles[2];
	TEST_PARAM testParam[2];

	xTaskDialogInitialize();

	testParam[0].UseImpl = TRUE;
	threadHandles[0] = CreateThread(NULL, 0, ThreadStart, &testParam[0], 0, NULL);

	testParam[1].UseImpl = FALSE;
	threadHandles[1] = CreateThread(NULL, 0, ThreadStart, &testParam[1], 0, NULL);

	WaitForMultipleObjects(
		2, threadHandles, TRUE, INFINITE);

	xTaskDialogUninitialize();

	return 0;

}
