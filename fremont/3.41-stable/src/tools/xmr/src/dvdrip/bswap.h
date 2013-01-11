#ifndef __DVDREAD_BSWAP_H__
#define __DVDREAD_BSWAP_H__

/* This is a slow but portable implementation, it has multiple evaluation 
 * problems so beware.
 * FreeBSD and Solaris don't have <byteswap.h> or any other such 
 * functionality! 
 */



#define B2N_16(x) \
 x = ((((x) & 0xff00) >> 8) | \
      (((x) & 0x00ff) << 8))
#define B2N_32(x) \
 x = ((((x) & 0xff000000) >> 24) | \
      (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | \
      (((x) & 0x000000ff) << 24))
#define B2N_64(x) \
 x = ((((x) & 0xff00000000000000) >> 56) | \
      (((x) & 0x00ff000000000000) >> 40) | \
      (((x) & 0x0000ff0000000000) >> 24) | \
      (((x) & 0x000000ff00000000) >>  8) | \
      (((x) & 0x00000000ff000000) <<  8) | \
      (((x) & 0x0000000000ff0000) << 24) | \
      (((x) & 0x000000000000ff00) << 40) | \
      (((x) & 0x00000000000000ff) << 56))

#endif // __DVDREAD_BSWAP_H__