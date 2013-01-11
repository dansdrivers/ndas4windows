#include "stdafx.h"
#include "confirmdlg.h"
#include "appconf.h"

int
pTaskDialogVerify(
	HWND hWndOwner,
	ATL::_U_STRINGorID Title,
	ATL::_U_STRINGorID MainInstruction,
	ATL::_U_STRINGorID Content,
	LPCTSTR DontShowOptionValueName,
	DWORD CommonButtons,
	int DefaultButton,
	int DefaultResponse)
{
	BOOL dontVerify = FALSE;
	BOOL success;

	if (DontShowOptionValueName)
	{
		success = pGetAppConfigValue(DontShowOptionValueName, &dontVerify);

		if (success && dontVerify) 
		{
			return DefaultResponse;
		}
	}

	WTLEX::CTaskDialogEx taskDialog(hWndOwner);
	taskDialog.ModifyFlags(0, TDF_ALLOW_DIALOG_CANCELLATION);
	if (CommonButtons)
	{
		taskDialog.SetCommonButtons(CommonButtons);
	}
	taskDialog.SetWindowTitle(Title.m_lpstr);
	taskDialog.SetMainIcon(IDR_MAINFRAME);
	taskDialog.SetMainInstructionText(MainInstruction.m_lpstr);
	taskDialog.SetContentText(Content.m_lpstr);
	if (DefaultButton)
	{
		taskDialog.SetDefaultButton(DefaultButton);
	}
	taskDialog.SetMainIcon(IDR_MAINFRAME);

	if (DontShowOptionValueName)
	{
		taskDialog.SetVerificationText(IDC_DONT_SHOW_AGAIN);
	}

	int selectedButton;
	HRESULT hr = taskDialog.DoModal(hWndOwner, &selectedButton, NULL, &dontVerify);

	if (FAILED(hr))
	{
		ATLTRACE("xTaskDialogIndirect failed, hr=0x%X\n", hr);
		return DefaultResponse;
	}

	if (DontShowOptionValueName)
	{
		if (dontVerify) 
		{
			success = pSetAppConfigValue(
				DontShowOptionValueName,
				(BOOL)TRUE);
		}
	}

	return selectedButton;
}

