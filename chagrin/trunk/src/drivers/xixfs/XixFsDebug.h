#ifndef __XIXFS_DEBUG_H__
#define __XIXFS_DEBUG_H__

#include "XixFsType.h"
#include "XixFsDrv.h"

#define XIXFS_DEBUG

#ifdef XIXFS_DEBUG
extern LONG XifsdDebugLevel;
extern LONG XifsdDebugTarget;
#endif

extern BOOLEAN XifsdTestRaisedStatus;

/*
 *	define debug level
 */


#define DEBUG_LEVEL_ERROR				(0x00000008)
#define DEBUG_LEVEL_CRITICAL			(0x00000004)
#define DEBUG_LEVEL_INFO				(0x00000002)
#define DEBUG_LEVEL_TRACE				(0x00000001)

#define DEBUG_LEVEL_UNWIND				(0x00000010)
#define DEBUG_LEVEL_EXCEPTIONS			(0x00000020)
	
#define DEBUG_LEVEL_ALL					(0xffffffff)
#define DEBUG_LEVEL_TEST				(DEBUG_LEVEL_ERROR|DEBUG_LEVEL_INFO|DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_UNWIND|DEBUG_LEVEL_EXCEPTIONS)
//#define DEBUG_LEVEL_TEST				(DEBUG_LEVEL_ERROR|DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_UNWIND|DEBUG_LEVEL_EXCEPTIONS)
/*
 *	define debug target
 */
#define DEBUG_TARGET_INIT				(0x00000001)
#define DEBUG_TARGET_DISPATCH			(0x00000002)
#define DEBUG_TARGET_FSCTL				(0x00000004)
#define DEBUG_TARGET_DEVCTL				(0x00000008)

#define DEBUG_TARGET_CREATE				(0x00000010)
#define DEBUG_TARGET_CLEANUP			(0x00000020)
#define DEBUG_TARGET_CLOSE				(0x00000040)

#define DEBUG_TARGET_FASTIO				(0x00000100)
#define DEBUG_TARGET_READ				(0x00000200)
#define DEBUG_TARGET_WRITE				(0x00000400)
#define DEBUG_TARGET_ADDRTRANS			(0x00000800)

#define DEBUG_TARGET_FILEINFO			(0x00001000)
#define DEBUG_TARGET_DIRINFO			(0x00002000)
#define DEBUG_TARGET_VOLINFO			(0x00004000)

#define DEBUG_TARGET_HOSTCOM			(0x00010000)
#define DEBUG_TARGET_PNP				(0x00020000)
#define DEBUG_TARGET_FLUSH				(0x00040000)

#define DEBUG_TARGET_IRPCONTEXT			(0x00100000)
#define DEBUG_TARGET_FCB				(0x00200000)
#define DEBUG_TARGET_CCB				(0x00400000)
#define DEBUG_TARGET_LCB				(0x00800000)
#define DEBUG_TARGET_VCB				(0x01000000)
#define DEBUG_TARGET_FILEOBJECT			(0x02000000)
#define DEBUG_TARGET_RESOURCE			(0x04000000)
#define DEBUG_TARGET_REFCOUNT			(0x08000000)
#define DEBUG_TARGET_GDATA				(0x10000000)
#define DEBUG_TARGET_CRITICAL			(0x20000000)
#define DEBUG_TARGET_LOCK				(0x40000000)

#define DEBUG_TARGET_TEST				(DEBUG_TARGET_PNP| DEBUG_TARGET_IRPCONTEXT)
#define DEBUG_TARGET_ALL				(0xffffffff)

#ifdef XIXFS_DEBUG

__inline BOOLEAN
XifsdDebugTracePre(LONG Level, LONG Target)
{
    if ((Target & XifsdDebugTarget) != 0 && (XifsdDebugLevel & Level) != 0) {
        DbgPrint( "PS[%08lx]:", PsGetCurrentThread( ));
        if (Level > 0) { 
			DbgPrint( "%*s", Level, "" );
        }
        return TRUE;
    } else {
        return FALSE;
    }
}




#define DebugTrace(LEVEL,TARGET,M) {					\
    if (XifsdDebugTracePre( (LEVEL), (TARGET))) {		\
		if((LEVEL) == DEBUG_LEVEL_ERROR){				\
				DbgPrint("<%s:%d>:", __FILE__,__LINE__);\
		}												\
        DbgPrint M;										\
    }													\
}

#else
#define DebugTrace(LEVEL,TARGET,M)		{NOTHING;}
#endif



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
    IN PXIFS_IRPCONTEXT IrpContext,
    IN NTSTATUS Status
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
    IN PXIFS_IRPCONTEXT IrpContext,
    IN NTSTATUS Status
    )
{
	if(IrpContext){
		IrpContext->SavedExceptionCode = STATUS_SUCCESS;
	}
}


__inline
VOID
XifsdNormalizeAndRaiseStatus (
    IN PXIFS_IRPCONTEXT IrpContext,
    IN NTSTATUS Status
    )
{
    IrpContext->SavedExceptionCode = FsRtlNormalizeNtstatus( Status, STATUS_UNEXPECTED_IO_ERROR );
    ExRaiseStatus( IrpContext->SavedExceptionCode );
}



#endif //	#ifndef __XIXFS_DEBUG_H__