// ndasmgmt.h
#pragma once
#include "ndascls.h"
#include <ndas/ndasuser.h>
#include "resource.h"

#define NDASMGMT_INST_UID _T("{1A7FAF5F-4D93-42d3-88AB-35590810D61F}")

#define INTERAPPMSG_POPUP	0xFF00
#define INTERAPPMSG_WELCOME	0xFF01
#define INTERAPPMSG_EXIT	0xFF02
#define INTERAPPMSG_REGDEV	0xFF03

namespace ndasmgmt
{
	extern LANGID CurrentUILangID;

	const LPCTSTR NDASMGMT_ATOM_HOTKEY =  _T("ndasmgmt_hotkey_6DD79D98-9144-4780-AFDC-ABE48E9A57E1");
	const LPCTSTR NDASMGMT_ATOM_DDE_APP = _T("ndasmgmt");
	const LPCTSTR NDASMGMT_ATOM_DDE_DEFAULT_TOPIC = _T("System");

}
