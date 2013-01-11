//#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include <limits.h>

#include "../inc/dvdcss.h"
#include "common.h"
#include "css.h"
#include "libdvdcss.h"

/*****************************************************************************
 * Error messages
 *****************************************************************************/
void _dvdcss_error( dvdcss_t dvdcss, char *psz_string )
{
    if( dvdcss->b_errors )
    {
        fprintf( stderr, "libdvdcss error: %s\n", psz_string );
    }

    dvdcss->psz_error = psz_string;
}

/*****************************************************************************
 * Debug messages
 *****************************************************************************/
void _dvdcss_debug( dvdcss_t dvdcss, char *psz_string )
{
    if( dvdcss->b_debug )
    {
        fprintf( stderr, "libdvdcss debug: %s\n", psz_string );
    }
}
