#include "prodver.h"

#define FV_PRODUCTVER       PV_PRODUCTVER
#define FV_PRODUCTVERSION   PV_PRODUCTVERSION
#define FV_PRODUCTNAME      PV_PRODUCTNAME
#define FV_COMPANYNAME      PV_COMPANYNAME
#define FV_LEGALCOPYRIGHT   PV_LEGALCOPYRIGHT
#define FV_LEGALTRADEMARKS  PV_LEGALTRADEMARKS
#define FV_SPECIALBUILD     PV_SPECIALBUILD
#define FV_PRIVATEBUILD     PV_PRIVATEBUILD

//#define FV_VER_MAJOR	2
//#define FV_VER_MINOR	1
//#define FV_VER_BUILD	5
//#define FV_VER_PRIVATE	0

//#define FV_FILEVER		2,1,5,0

#ifndef FV_FILEVER
#define FV_FILEVER          PV_PRODUCTVER
#endif  // FV_FILEVER

#ifndef FV_VER_MAJOR
#define FV_FILEVERSION      PV_PRODUCTVERSION
#else   // FV_VER_MAJOR

//#if     (FV_VER_BUILD < 10)
//#define FV_BPAD "000"
//#elif   (FV_VER_BUILD < 100)
//#define FV_BPAD "00"
//#elif   (FV_VER_BUILD < 1000)
//#define FV_BPAD "0"
//#else
#define FV_BPAD
//#endif


#if FV_VER_PRIVATE > 0
#define FV_VER_STR2(w,x,y,z)    #w "." #x "." FV_BPAD #y "." #z
#else
#define FV_VER_STR2(w,x,y,z)    #w "." #x "." FV_BPAD #y
#endif

#define FV_VER_STR1(w,x,y,z)    FV_VER_STR2(w, x, y, z)
#define FV_FILEVERSION          FV_VER_STR1(FV_VER_MAJOR, FV_VER_MINOR, FV_VER_BUILD, FV_VER_PRIVATE)    

#endif // FV_VER_MAJOR

#define FV_COMMENTS		"\0"
#define FV_FILEDESCRIPTION	"Lean Packet Exchange Protocol\0"
#define FV_INTERNALNAME		"lpx\0"
#define FV_ORIGINALFILENAME	"lpx.sys\0"
