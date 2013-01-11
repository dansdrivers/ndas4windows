#ifndef __DVDCSS_CSS_H__
#define __DVDCSS_CSS_H__

#define KEY_SIZE 5

typedef uint8_t dvd_key_t[KEY_SIZE];

typedef struct dvd_title_s
{
    int                 i_startlb;
    dvd_key_t           p_key;
    struct dvd_title_s *p_next;
} dvd_title_t;

typedef struct css_s
{
    int             i_agid;      /* Current Authenication Grant ID. */
    dvd_key_t       p_bus_key;   /* Current session key. */
    dvd_key_t       p_disc_key;  /* This DVD disc's key. */
    dvd_key_t       p_title_key; /* Current title key. */
} css_t;

/*****************************************************************************
 * Prototypes in css.c
 *****************************************************************************/
int   _dvdcss_test        ( dvdcss_t );
int   _dvdcss_title       ( dvdcss_t, int );
int   _dvdcss_disckey     ( dvdcss_t );
int   _dvdcss_titlekey    ( dvdcss_t, int , dvd_key_t );
int   _dvdcss_unscramble  ( uint8_t *, uint8_t * );

#endif//#ifndef __DVDCSS_CSS_H__