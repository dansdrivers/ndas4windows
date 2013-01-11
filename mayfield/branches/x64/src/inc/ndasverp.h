/****************************************************************************
 *                                                                          *
 *      ntverp.H        -- Version information for internal builds          *
 *                                                                          *
 *      This file is only modified by the official builder to update the    *
 *      VERSION, VER_PRODUCTVERSION, VER_PRODUCTVERSION_STR and             *
 *      VER_PRODUCTBETA_STR values.                                         *
 *                                                                          *
 ****************************************************************************
/*--------------------------------------------------------------*/
/* the following values should be modified by the official      */
/* builder for each build                                       */
/*                                                              */
/* the VER_PRODUCTBUILD lines must contain the product          */
/* comments and end with the build#<CR><LF>                     */
/*                                                              */
/* the VER_PRODUCTBETA_STR lines must  contain the product      */
/* comments and end with "some string"<CR><LF>                  */
/*--------------------------------------------------------------*/

#if _MSC_VER > 1000
#pragma once
#endif

#include "ndas.ver"
//#define VER_PRODUCTBUILD            /* NT */   3790
//
//#define VER_PRODUCTBUILD_QFE        1218
//
//#define VER_PRODUCTMAJORVERSION     5
//#define VER_PRODUCTMINORVERSION     2
//
//#define VER_PRODUCTBETA_STR         /* NT */     ""

#define VER_PRODUCTVERSION_MAJORMINOR2(x,y) #x "." #y
#define VER_PRODUCTVERSION_MAJORMINOR1(x,y) VER_PRODUCTVERSION_MAJORMINOR2(x, y)
#define VER_PRODUCTVERSION_STRING   VER_PRODUCTVERSION_MAJORMINOR1(VER_PRODUCTMAJORVERSION, VER_PRODUCTMINORVERSION)

#define LVER_PRODUCTVERSION_MAJORMINOR2(x,y) L#x L"." L#y
#define LVER_PRODUCTVERSION_MAJORMINOR1(x,y) LVER_PRODUCTVERSION_MAJORMINOR2(x, y)
#define LVER_PRODUCTVERSION_STRING  LVER_PRODUCTVERSION_MAJORMINOR1(VER_PRODUCTMAJORVERSION, VER_PRODUCTMINORVERSION)

#define VER_PRODUCTVERSION          VER_PRODUCTMAJORVERSION,VER_PRODUCTMINORVERSION,VER_PRODUCTBUILD,VER_PRODUCTBUILD_QFE
// #define VER_PRODUCTVERSION_W        (0x0502)
// #define VER_PRODUCTVERSION_DW       (0x05020000 | VER_PRODUCTBUILD)

#if     (VER_PRODUCTBUILD < 10)
#define VER_BPAD "000"
#elif   (VER_PRODUCTBUILD < 100)
#define VER_BPAD "00"
#elif   (VER_PRODUCTBUILD < 1000)
#define VER_BPAD "0"
#else
#define VER_BPAD
#endif

#if     (VER_PRODUCTBUILD < 10)
#define LVER_BPAD L"000"
#elif   (VER_PRODUCTBUILD < 100)
#define LVER_BPAD L"00"
#elif   (VER_PRODUCTBUILD < 1000)
#define LVER_BPAD L"0"
#else
#define LVER_BPAD
#endif

#define VER_PRODUCTVERSION_STR2(x,y) VER_PRODUCTVERSION_STRING "." VER_BPAD #x "." #y
#define VER_PRODUCTVERSION_STR1(x,y) VER_PRODUCTVERSION_STR2(x, y)
#define VER_PRODUCTVERSION_STR       VER_PRODUCTVERSION_STR1(VER_PRODUCTBUILD, VER_PRODUCTBUILD_QFE)

#define LVER_PRODUCTVERSION_STR2(x,y) LVER_PRODUCTVERSION_STRING L"." LVER_BPAD L#x L"." L#y
#define LVER_PRODUCTVERSION_STR1(x,y) LVER_PRODUCTVERSION_STR2(x, y)
#define LVER_PRODUCTVERSION_STR       LVER_PRODUCTVERSION_STR1(VER_PRODUCTBUILD, VER_PRODUCTBUILD_QFE)

#define VER_FILEVERSION_MAJORMINOR2(x,y) #x "." #y
#define VER_FILEVERSION_MAJORMINOR1(x,y) VER_FILEVERSION_MAJORMINOR2(x, y)
#define VER_FILEVERSION_STRING   VER_FILEVERSION_MAJORMINOR1(VER_FILEMAJORVERSION, VER_FILEMINORVERSION)

#define LVER_FILEVERSION_MAJORMINOR2(x,y) L#x L"." L#y
#define LVER_FILEVERSION_MAJORMINOR1(x,y) LVER_FILEVERSION_MAJORMINOR2(x, y)
#define LVER_FILEVERSION_STRING  LVER_FILEVERSION_MAJORMINOR1(VER_FILEMAJORVERSION, VER_FILEMINORVERSION)

#define VER_FILEVERSION          VER_FILEMAJORVERSION,VER_FILEMINORVERSION,VER_FILEBUILD,VER_FILEBUILD_QFE
// #define VER_FILEVERSION_W        (0x0502)
// #define VER_FILEVERSION_DW       (0x05020000 | VER_FILEBUILD)

#if     (VER_FILEBUILD < 10)
#define VER_BPAD "000"
#elif   (VER_FILEBUILD < 100)
#define VER_BPAD "00"
#elif   (VER_FILEBUILD < 1000)
#define VER_BPAD "0"
#else
#define VER_BPAD
#endif

#if     (VER_FILEBUILD < 10)
#define LVER_BPAD L"000"
#elif   (VER_FILEBUILD < 100)
#define LVER_BPAD L"00"
#elif   (VER_FILEBUILD < 1000)
#define LVER_BPAD L"0"
#else
#define LVER_BPAD
#endif

#define VER_FILEVERSION_STR2(x,y) VER_FILEVERSION_STRING "." VER_BPAD #x "." #y
#define VER_FILEVERSION_STR1(x,y) VER_FILEVERSION_STR2(x, y)
#define VER_FILEVERSION_STR       VER_FILEVERSION_STR1(VER_FILEBUILD, VER_FILEBUILD_QFE)

#define LVER_FILEVERSION_STR2(x,y) LVER_FILEVERSION_STRING L"." LVER_BPAD L#x L"." L#y
#define LVER_FILEVERSION_STR1(x,y) LVER_FILEVERSION_STR2(x, y)
#define LVER_FILEVERSION_STR       LVER_FILEVERSION_STR1(VER_FILEBUILD, VER_FILEBUILD_QFE)


/*--------------------------------------------------------------*/
/* the following section defines values used in the version     */
/* data structure for all files, and which do not change.       */
/*--------------------------------------------------------------*/

/* default is nodebug */
#if DBG
#define VER_DEBUG                   VS_FF_DEBUG
#else
#define VER_DEBUG                   0
#endif /* DBG */

/* default is prerelease */
#if BETA
#define VER_PRERELEASE              VS_FF_PRERELEASE
#else
#define VER_PRERELEASE              0
#endif /* BETA */

#if OFFICIAL_BUILD
#define VER_PRIVATE                 0
#else
#define VER_PRIVATE                 VS_FF_PRIVATEBUILD
#endif /* OFFICIAL_BUILD */

#define VER_FILEFLAGSMASK           VS_FFI_FILEFLAGSMASK
#define VER_FILEOS                  VOS_NT_WINDOWS32
#define VER_FILEFLAGS               (VER_PRERELEASE|VER_DEBUG|VER_PRIVATE)

#define VER_COMPANYNAME_STR         "XIMETA, Inc."
#define VER_PRODUCTNAME_STR         "NDAS\256 Software"
#define VER_LEGALTRADEMARKS_STR     "NDAS\256 (Network Direct Attached Storage) is a registered trademark of XIMETA, Inc. in the United States and/or other countries."
