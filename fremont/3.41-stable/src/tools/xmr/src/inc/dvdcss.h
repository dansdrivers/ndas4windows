#ifndef __DVDCSS_DVD_CSS_H__
#define __DVDCSS_DVD_CSS_H__

#ifdef __cplusplus
extern "C" {
#endif

/** Library instance handle, to be used for each library call. */
typedef struct dvdcss_s* dvdcss_t;


/** The block size of a DVD. */
#define DVDCSS_BLOCK_SIZE      2048

/** The default flag to be used by \e libdvdcss functions. */
#define DVDCSS_NOFLAGS         0

/** Flag to ask dvdcss_read() to decrypt the data it reads. */
#define DVDCSS_READ_DECRYPT    (1 << 0)

/** Flag to tell dvdcss_seek() it is seeking in MPEG data. */
#define DVDCSS_SEEK_MPEG       (1 << 0)

/** Flag to ask dvdcss_seek() to check the current title key. */
#define DVDCSS_SEEK_KEY        (1 << 1)

/*
 * Exported prototypes.
 */
extern dvdcss_t dvdcss_open  ( const char *psz_target );
extern int      dvdcss_close ( dvdcss_t );
extern int      dvdcss_seek  ( dvdcss_t,
                               int i_blocks,
                               int i_flags );
extern int      dvdcss_read  ( dvdcss_t,
                               void *p_buffer,
                               int i_blocks,
                               int i_flags );
extern int      dvdcss_readv ( dvdcss_t,
                               void *p_iovec,
                               int i_blocks,
                               int i_flags );
extern char *   dvdcss_error ( dvdcss_t );

extern int dvdcss_title ( dvdcss_t dvdcss, int i_block );

#define Ddvdcss_title(a,b) dvdcss_seek(a,b,DVDCSS_SEEK_KEY)


#ifdef __cplusplus
}
#endif

#endif//#ifndef __DVDCSS_DVD_CSS_H__
