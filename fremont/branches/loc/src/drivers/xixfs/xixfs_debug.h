#ifndef __XIXFS_DEBUG_H__
#define __XIXFS_DEBUG_H__

#include "xixfs_types.h"
#include "xixfs_drv.h"
#include "xcsystem/system.h"
#include "xcsystem/debug.h"

extern BOOLEAN XifsdTestRaisedStatus;


#define XifsdBugCheck(A,B,C) { KeBugCheckEx(RESOURCE_NOT_OWNED,  __LINE__, A, B, C ); }



#define DebugUnwind(X) {																			\
    if (AbnormalTermination()) {																	\
        DebugTrace( DEBUG_LEVEL_UNWIND, DEBUG_TARGET_CRITICAL, (#X ", Abnormal termination.\n") );  \
    }																								\
}



#define DebugBreakOnStatus(S) {                                                       \
    if (XifsdTestRaisedStatus) {                                                        \
        if ((S) == STATUS_DISK_CORRUPT_ERROR ||                                       \
            (S) == STATUS_FILE_CORRUPT_ERROR ||                                       \
            (S) == STATUS_CRC_ERROR) {                                                \
            DbgPrint( "XixFs: Breaking on interesting raised status (%08x)\n", (S) );  \
            DbgBreakPoint();                                                          \
        }                                                                             \
    }                                                                                 \
}    


__inline
DECLSPEC_NORETURN
VOID
XifsdRaiseStatus (
	PXIXFS_IRPCONTEXT IrpContext,
    NTSTATUS Status
    )
{
	if(IrpContext){
		IrpContext->SavedExceptionCode = Status;
	}
    DebugBreakOnStatus( Status );
    ExRaiseStatus( Status );
}


__inline
DECLSPEC_NORETURN
VOID
XifsdResetExceptionStatus (
	PXIXFS_IRPCONTEXT IrpContext,
    NTSTATUS Status
    )
{
	if(IrpContext){
		IrpContext->SavedExceptionCode = STATUS_SUCCESS;
	}
}


__inline
DECLSPEC_NORETURN
VOID
XifsdNormalizeAndRaiseStatus (
	PXIXFS_IRPCONTEXT IrpContext,
	NTSTATUS Status
    )
{
    IrpContext->SavedExceptionCode = FsRtlNormalizeNtstatus( Status, STATUS_UNEXPECTED_IO_ERROR );
    ExRaiseStatus( IrpContext->SavedExceptionCode );
}



#endif //	#ifndef __XIXFS_DEBUG_H__