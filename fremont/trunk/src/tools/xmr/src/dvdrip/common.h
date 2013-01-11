#ifndef __DVDCSS_COMMON_H__
#define __DVDCSS_COMMON_H__

typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef signed int			ssize_t;
typedef unsigned short		uint16_t;
typedef __int64				uint64_t;
typedef __int64				int64_t;

#define PATH_MAX 256


/* several type definitions */
#   if defined( __MINGW32__ )
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           define off_t long long
#       endif
#   endif

#   if defined( _MSC_VER )
#       if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#           define _OFF_T_DEFINED
#       else
#           define off_t __int64
#       endif
#       define stat _stati64
#   endif

#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif


#endif //#ifndef __DVDCSS_COMMON_H__