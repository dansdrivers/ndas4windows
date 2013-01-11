#ifndef __DVDREAD_DVD_INPUT_H__
#define __DVDREAD_DVD_INPUT_H__

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Defines and flags.  Make sure they fit the libdvdcss API!
 */
#define DVDINPUT_NOFLAGS         0

#define DVDINPUT_READ_DECRYPT    (1 << 0)

#define DVDINPUT_SEEK_MPEG       (1 << 0)
#define DVDINPUT_SEEK_KEY        (1 << 1)


typedef struct dvd_input_s *dvd_input_t;

/**
 * Pointers which will be filled either the input meathods functions.
 */
dvd_input_t (*DVDinput_open)  (const char *);
int         (*DVDinput_close) (dvd_input_t);
int         (*DVDinput_seek)  (dvd_input_t, int, int);
int         (*DVDinput_title) (dvd_input_t, int); 
int         (*DVDinput_read)  (dvd_input_t, void *, int, int);
char *      (*DVDinput_error) (dvd_input_t);

/**
 * Setup function accessed by dvd_reader.c.  Returns 1 if there is CSS support.
 */
int DVDInputSetup(void);

#ifdef __cplusplus
}
#endif

#endif // __DVDREAD_DVD_INPUT_H__ 
