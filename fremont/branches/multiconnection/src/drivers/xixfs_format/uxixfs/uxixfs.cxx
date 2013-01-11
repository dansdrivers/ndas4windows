#include <pch.cxx>

#define _UXIXFS_MEMBER_
#include "ulib.hxx"
#include "uxixfs.hxx"

extern "C" {
//    #include <patchbc.h>
    #include "rtmsg.h"
}

/*
#ifdef _AUTOCHECK_

BOOLEAN
SimpleFetchMessageTextInOemCharSet(
    IN  ULONG  MessageId,
    OUT CHAR  *Text,
    IN  ULONG  BufferLen
    );

#endif
*/
//      Local prototypes

STATIC
BOOLEAN
DefineClassDescriptors(
        );

STATIC
BOOLEAN
UndefineClassDescriptors(
        );



extern "C"
UXIXFS_EXPORT
BOOLEAN
InitializeUxixfs (
    PVOID DllHandle,
    ULONG Reason,
    PCONTEXT Context
        );

UXIXFS_EXPORT
BOOLEAN
InitializeUxixfs (
    PVOID DllHandle,
    ULONG Reason,
    PCONTEXT Context
        )
/*++

Routine Description:

        Initialize Ufat by constructing and initializing all
        global objects. These include:

                - all CLASS_DESCRIPTORs (class_cd)

Arguments:

        None.

Return Value:

        BOOLEAN - Returns TRUE if all global objects were succesfully constructed
                and initialized.

--*/

{
    UNREFERENCED_PARAMETER( DllHandle );
    UNREFERENCED_PARAMETER( Context );

#if defined(FE_SB) && defined(_X86_)
    if (Reason == DLL_PROCESS_ATTACH) {
        //
        // Initialize Machine Id
        //
        InitializeMachineData();
    }
#endif

#if _AUTOCHECK_

    UNREFERENCED_PARAMETER( Reason );

    if (!DefineClassDescriptors()) {
        UndefineClassDescriptors();
        DebugAbort( "Uxixfs initialization failed!!!\n" );
        return( FALSE );
    }

#if defined(TRACE_UFAT_MEM_LEAK)
    DebugPrint("UXIXFS.DLL got attached.\n");
#endif

#else // _AUTOCHECK_ and _SETUP_LOADER_ not defined

    STATIC ULONG    count = 0;

    switch (Reason) {
        case DLL_PROCESS_ATTACH:
            //
            // Get translated boot messages into FAT boot code.
            //
            

            // Success, FALL THROUGH to thread attach case

        case DLL_THREAD_ATTACH:

            if (count > 0) {
                ++count;
#if defined(TRACE_UFAT_MEM_LEAK)
                DebugPrintTrace(("UXIXFS.DLL got attached %d times.\n", count));
#endif
                return TRUE;
            }

            if (!DefineClassDescriptors()) {
                UndefineClassDescriptors();
                DebugAbort( "UXIXFS initialization failed!!!\n" );
                return( FALSE );
            }

#if defined(TRACE_UFAT_MEM_LEAK)
            DebugPrint("UXIXFS.DLL got attached.\n");
#endif

            count++;
            break;

        case DLL_PROCESS_DETACH:
        case DLL_THREAD_DETACH:

            if (count > 1) {
                --count;
#if defined(TRACE_UFAT_MEM_LEAK)
                DebugPrintTrace(("UXIXFS.DLL got detached.  %d time(s) left.\n", count));
#endif
                return TRUE;
            }
            if (count == 1) {

#if defined(TRACE_UFAT_MEM_LEAK)
                DebugPrint("UXIXFS.DLL got detached.\n");
#endif

                UndefineClassDescriptors();
                count--;
            } else {
#if defined(TRACE_UFAT_MEM_LEAK)
                DebugPrint("UXIXFS.DLL detached more than attached\n");
#endif
            }
            break;
    }
#endif _AUTOCHECK_ || _SETUP_LOADER_

    return TRUE;
}



DECLARE_CLASS(	XIXFS		);
DECLARE_CLASS(	XIXFS_VOL	);
DECLARE_CLASS(	XIXFS_SA	);

STATIC
BOOLEAN
DefineClassDescriptors(
        )
{

	if(	DEFINE_CLASS_DESCRIPTOR(	XIXFS					) &&
		DEFINE_CLASS_DESCRIPTOR(	XIXFS_VOL				) &&
		DEFINE_CLASS_DESCRIPTOR(	XIXFS_SA				)
		)
	{
		DebugPrint( "initialize class descriptors!");
		return TRUE;
	}	else {
		DebugPrint( "could not initialize class descriptors!");
		return FALSE;	
	}


}

STATIC
BOOLEAN
UndefineClassDescriptors(
        )
{
	UNDEFINE_CLASS_DESCRIPTOR(	XIXFS		);
	UNDEFINE_CLASS_DESCRIPTOR(	XIXFS_VOL	);
	UNDEFINE_CLASS_DESCRIPTOR(	XIXFS_SA	);
    return TRUE;
}



