#ifndef LFSTEST_H
#define LFSTEST_H

//
//	version information
//
#define LFST_VERSION_MAJ	0
#define LFST_VERSION_MIN	4

//
//	excutable name
//
#define LFST_EXEC_NAME		TEXT("lfstest")

//
//	maximum argument number
//
#define MAX_ARGS			5
#define MAX_TOKENS			64

//
//	function return type
//
typedef int	LFSTRET ;
#define LFST_FAIL			(-1)
#define LFST_SUCC			0

//
//	test category code
//
#define LFST_NONE	0

typedef enum _LFSTestCategory {
	LFST_ALL = 1,
	LFST_FILESYSTEM,
	LFST_DISK,
	LFST_VOLUME,
	LFST_DIRECTORY,
	LFST_FILE,
	LFST_BACKUP,
	LFST_SHARING,
	LFST_PERFORMANCE
} LFSTestCategoty, *PLFSTestCategoty ;

typedef enum _LFSTestSubCategory {
	FUNCTION,
	CONTROL
} LFSTestSubCategoty, *PLFSTestSubCategoty ;

typedef enum _LFSFunction {
	// FILE_MANAGEMENT
	FM_CREATE,
	FM_CLOSE,
	FM_READ,
	FM_WRITE,
	FM_COPY,
	FM_FIND,
	FM_GETINFO,
	FM_GETATTR,
	FM_SETATTR,
	FM_COMPRESS,
	FM_MOVE,
	FM_SEARCHPATH,
	FM_GETSEC,
	FM_SETSEC,
	// FILE_IO
	FI_CANCEL,
	FI_COMPLETION,
	FI_FLUSH,
	FI_LOCK,
	FI_UNLOCK,
	FI_FILEPOINTER,
	//FILE_MAPPING
	FP_EXEC,
	FP_CREATEMAPPING,
	FP_FLUSHVIEW,
	FP_MAPVIEW,
	FP_UNMAPVIEW,
	FP_OPENNAMEDMAPPING,
	// FILE_ENCRYPT
	FE_ADDUSERS,
	FE_REMOVEUSERS,
	FE_ENCRYPT,
	FE_DECRYPT,
	FE_DUP_ENCINFO,
	FE_GETINFO,
	FE_SETKEY,
	// FILE_LZ
	FL_OPEN,
	FL_COPY,
	FL_READ,
	FL_SEEK,
	// FILE_CTL
	FL_FINDBYSID,
	FL_GETNTFSREC,
	FL_GETCOMP,
	FL_SETCOMP,
	FL_GETRANGES,
	FL_SETSPARSE,
	FL_SETZERO,

	// PERF
	PF_SEQ_READ,
	PF_SEQ_WRITE,
	PF_RND_READ,
	PF_RND_WRITE,
	PF_OPENCLOSE,
	PF_CREATEDELETE

} LFSTestFunction, *PLFSTestFunction ;

typedef	enum _LFSTOptionEnum {
	LFST_BASEDIR = 1,
	LFST_VERBOSE,
	LFST_EXITONERR,
} LFSTOptionEnum ;


typedef	struct _LFSTCodeString {

	ULONG	Code ;
	LPTSTR	String ;

}	LFSTCodeString, *PLFSTCodeString ;

typedef struct _LFSTCommand {

	ULONG	Category ;
	ULONG	Subcategory ;
	ULONG	Function ;
	ULONG	Flags ;
	LPTSTR	BaseDir ;
	TCHAR	BaseDirBuffer[MAX_PATH] ;
	TCHAR	FullBaseDirBuffer[MAX_PATH] ;
	ULONG	DosDriveNo ;
	ULONG	WinVerMajor ;
	ULONG	WinVerMinor ;
	ULONG	WinBuild ;
	union {
		ULONG	Dword ;
		LPTSTR	String ;
		PCHAR	Data ;
	} ex ;

} LFSTCommand, *PLFSTCommand ;


#define LFST_FLAGON(FLAG, ORDER) (((FLAG) & (1 << (ORDER - 1))) != 0)
#define LFST_SETFLAG(FLAG, ORDER) ((FLAG) |= 1 << (ORDER - 1))

//
//	external functions
//
#define PrintCategoryTitle(TITLE)		_tprintf(TEXT("> ") TEXT(TITLE) TEXT("\n"))
#define PrintSubcategoryTitle(TITLE)	_tprintf(TEXT("  * ") TEXT(TITLE) TEXT("\n"))
#define PrintFunctionTitle(TITLE)		_tprintf(TEXT("    - ") TEXT(TITLE) TEXT("\n"))	
#define PrintPassFail(PASS, TESTMENT, STEPNO)														\
					if(PASS) {																		\
						_tprintf(TEXT("       PASS: ") TEXT(TESTMENT) TEXT(" #%d. ErrCode:%d\n"), (STEPNO), GetLastError()) ;	\
					} else {																		\
						_tprintf(TEXT("       FAIL: ") TEXT(TESTMENT) TEXT(" #%d. ErrCode:%d\n"), (STEPNO), GetLastError()) ;	\
					}
#define PrintFunctionOutput(MESSAGE)	_tprintf(TEXT("        ") TEXT(MESSAGE))
#define ReturnIfExitFlagOn(PASSFAIL, FLAGS)										\
				if(!(PASSFAIL) && LFST_FLAGON(command->Flags, LFST_EXITONERR))	\
						return LFST_FAIL ;										\

#define IsWin2KorLater(MAJ, MIN) ((MAJ) == 5 && (MIN) >= 0)
#define IsWinXPorLater(MAJ, MIN) ((MAJ) == 5 && (MIN) >= 1)

LFSTRET
LfstFile(
		PLFSTCommand	command
	) ;

LFSTRET
LfstDirectory(
		PLFSTCommand	command
	) ;

LFSTRET
LfstPerformance(
		PLFSTCommand	command
	) ;

#endif