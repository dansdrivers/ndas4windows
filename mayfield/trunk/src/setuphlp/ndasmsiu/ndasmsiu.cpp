#include "precomp.hpp"
#include <instmsg.hpp>

const LPCTSTR NDASMGMT_INST_UID =  _T("{1A7FAF5F-4D93-42d3-88AB-35590810D61F}");
const WPARAM NDASMGMT_INST_WPARAM_EXIT = 0xFF02;

using namespace local;

BOOL
PostToStopNdasmgmt()
{
    UINT uMsgId = GetInstanceMessageId(NDASMGMT_INST_UID);
    if (0 == uMsgId)
    {
        return FALSE;
    }
    return PostInstanceMessage(uMsgId, NDASMGMT_INST_WPARAM_EXIT, 0);
}

extern "C"
UINT
__stdcall
NdasMsiStopNdasmgmt(MSIHANDLE hInstall)
{
    UNREFERENCED_PARAMETER(hInstall);
    (void) PostToStopNdasmgmt();
    return ERROR_SUCCESS;
}

