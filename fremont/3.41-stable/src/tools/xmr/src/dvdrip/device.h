#ifndef __DVDCSS_DEVICE_H__
#define __DVDCSS_DEVICE_H__
#include <io.h>
struct iovec
{
    void *iov_base;     /* Pointer to data. */
    size_t iov_len;     /* Length of data.  */
};
/*****************************************************************************
 * Device reading prototypes
 *****************************************************************************/
int _dvdcss_use_ioctls ( dvdcss_t );
int _dvdcss_open       ( dvdcss_t );
int _dvdcss_close      ( dvdcss_t );

/*****************************************************************************
 * Device reading prototypes, raw-device specific
 *****************************************************************************/    
#endif//#ifndef __DVDCSS_DEVICE_H__