#include "precomp.h"
#include "RAWIO32.h"
#include "stdio.h"

extern "C" 
{
      // prototype for function in .obj file from the thunk script
      BOOL WINAPI thk_ThunkConnect32(LPSTR     lpDll16,
                                     LPSTR     lpDll32,
                                     HINSTANCE hDllInst,
                                     DWORD     dwReason);

      BOOL WINAPI DllMain(HINSTANCE hDLLInst,
                          DWORD     dwReason,
                          LPVOID    lpvReserved)
      {
         if (!thk_ThunkConnect32("RAWIO16.DLL", "RAWIO32.DLL",
                                 hDLLInst, dwReason))
         {
            return FALSE;
         }
         switch (dwReason)
         {
            case DLL_PROCESS_ATTACH:
               break;

            case DLL_PROCESS_DETACH:
               break;

            case DLL_THREAD_ATTACH:
               break;

            case DLL_THREAD_DETACH:
               break;
         }
         return TRUE;
      } 
} // extern "C" 
