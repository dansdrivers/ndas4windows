#pragma once
#include "resource.h"

int
pTaskDialogVerify(
	__in_opt HWND hWndOwner,
	__in_opt ATL::_U_STRINGorID Title,
	__in_opt ATL::_U_STRINGorID MainInstruction,
	__in_opt ATL::_U_STRINGorID Content,
	__in_opt LPCTSTR DontShowOptionValueName /* = NULL */,
	__in DWORD CommonButtons /* = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON */,
	__in int DefaultButton /* = IDYES */,
	__in int DefaultResponse /* = IDYES */);
