#ifndef PRODVER_H
#define PRODVER_H

#define PV_VER_MAJOR 3
#define PV_VER_MINOR 10
#define PV_VER_MINOR_DEC	8
#define PV_VER_BUILD 1210
#define PV_VER_PRIVATE 0

#define PV_PRODUCTVER 3,10,1210,0

//#if     (PV_VER_BUILD < 10)
//#define PV_BPAD "000"
//#elif   (PV_VER_BUILD < 100)
//#define PV_BPAD "00"
//#elif   (PV_VER_BUILD < 1000)
//#define PV_BPAD "0"
//#else
#define PV_BPAD
//#endif

#if PV_VER_PRIVATE > 0
#define PV_VER_STR2(w,x,y,z)    #w "." #x "." PV_BPAD #y "." #z
#else
#define PV_VER_STR2(w,x,y,z)    #w "." #x "." PV_BPAD #y
#endif
#define PV_VER_STR1(w,x,y,z)    PV_VER_STR2(w, x, y, z)
#define PV_PRODUCTVERSION       PV_VER_STR1(PV_VER_MAJOR, PV_VER_MINOR, PV_VER_BUILD, PV_VER_PRIVATE)    
#define PV_PRODUCTNAME 	        "NDAS Software\0"
#define PV_COMPANYNAME 		"XIMETA, Inc.\0"
#define PV_LEGALCOPYRIGHT	"Copyright \251 2003-2004 XIMETA, Inc.\0"
#define PV_LEGALTRADEMARKS	\
    "NDAS\256 (Network Direct Attached Storage) is "	\
	"a registered trademark of XIMETA, Inc. " \
	"in the United States and/or other countries.\0"
#define PV_SPECIALBUILD 	"\0"
#define PV_PRIVATEBUILD     "\0"

// #define OEM_BUILD
// #define OEM_MORITANI

#ifdef OEM_BULID

#endif // OEM_BUILD

#ifdef OEM_MORITANI

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"Eoseed\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"Eoseed\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"EOS\0"

#endif // OEM_MORITANI

#ifdef OEM_GENNETWORKS

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"GenDisk\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"GenDisk\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"Gennetworks, Inc.\0"
#undef  PV_LEGALCOPYRIGHT
#define PV_LEGALCOPYRIGHT	"Gennetworks and NDAS created by XIMETA\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OGN\0"

#endif // OEM_GENNETWORKS

#ifdef OEM_IOMEGA

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"Network Hard Drive\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"Network Hard Drive\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"Iomega Corporation\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	"Copyright (C) 2003 Iomega Corporation\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OIM\0"

#endif

#ifdef OEM_LOGITEC

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"LHD-LU2\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"LHD-LU2\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"Logitec Corporation\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	"Copyright (C) 2003 Logitec Corporation\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OLT\0"

#endif

#ifdef OEM_RUTTER

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"NetDisk\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"NetDisk\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"Rutter technologies, Inc.\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	"Copyright (C) 2003 Rutter technologies, Inc.\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"ORT\0"

#define	OEM_START_MACADDR0	0x00
#define	OEM_START_MACADDR1	0x0b
#define	OEM_START_MACADDR2	0xd0
#define	OEM_START_MACADDR3	0x20
#define	OEM_START_MACADDR4	0x00
#define	OEM_START_MACADDR5	0x00

#define	OEM_END_MACADDR0	0x00
#define	OEM_END_MACADDR1	0x0b
#define	OEM_END_MACADDR2	0xd0
#define	OEM_END_MACADDR3	0x23
#define	OEM_END_MACADDR4	0xff
#define	OEM_END_MACADDR5	0xff

#endif

#ifdef OEM_NDAS

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"NDAS\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"NDAS\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"XIMETA, Inc.\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	"Copyright (C) 2003-2004 XIMETA, Inc.\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OND\0"

#endif

#ifdef OEM_VERNCO

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"NDAS\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"NDAS\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"NDAS\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	""
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OVC\0"

#endif

#ifdef OEM_TULSIENT

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"NDAS\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"NDAS\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"Tulsient\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	""
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OTS\0"

#endif

#ifdef OEM_IODATA

#undef  PV_PRODUCTNAME
#define PV_PRODUCTNAME		"HDH-UL\0"
#undef  PV_LEGALTRADEMARKS
#define PV_LEGALTRADEMARKS	"HDH-UL\0"
#undef  PV_COMPANYNAME
#define PV_COMPANYNAME		"I-O DATA DEVICE, INC.\0"
//#undef  PV_LEGALCOPYRIGHT
//#define PV_LEGALCOPYRIGHT	"Copyright (C) 2003-2004 XIMETA, Inc.\0"
#undef  PV_PRIVATEBUILD
#define PV_PRIVATEBUILD		"OIO\0"

#endif

#endif
