#ifndef __DVDCSS_LIB_DVD_CSS_H__
#define __DVDCSS_LIB_DVD_CSS_H__

struct iovec;

/*****************************************************************************
 * The libdvdcss structure
 *****************************************************************************/
struct dvdcss_s
{
    /* File descriptor */
    char * psz_device;
    int    i_fd;
    int    i_read_fd;
    int    i_pos;

    /* File handling */
    int ( * pf_seek )  ( dvdcss_t, int );
    int ( * pf_read )  ( dvdcss_t, void *, int );
    int ( * pf_readv ) ( dvdcss_t, struct iovec *, int );

    /* Decryption stuff */
    int          i_method;
    css_t        css;
    int          b_ioctls;
    int          b_scrambled;
    dvd_title_t *p_titles;

    /* Key cache directory and pointer to the filename */
    char   psz_cachefile[PATH_MAX];
    char * psz_block;

    /* Error management */
    char * psz_error;
    int    b_errors;
    int    b_debug;


    int    b_file;
    char * p_readv_buffer;
    int    i_readv_buf_size;

};

/*****************************************************************************
 * libdvdcss method: used like init flags
 *****************************************************************************/
#define DVDCSS_METHOD_KEY        0
#define DVDCSS_METHOD_DISC       1
#define DVDCSS_METHOD_TITLE      2

/*****************************************************************************
 * Functions used across the library
 *****************************************************************************/
void _dvdcss_error ( dvdcss_t, char * );
void _dvdcss_debug ( dvdcss_t, char * );

#endif//#ifndef __DVDCSS_LIB_DVD_CSS_H__