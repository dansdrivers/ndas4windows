#ifndef __IOV_H
#define __IOV_H

/* A word of warning: Our uio structure will clash with the C library one (which is now obsolete). Remove the C
   library one from sys/uio.h if you have a very old library set */

struct iovec
{
        void *iov_base;         /* BSD uses caddr_t (1003.1g requires void *) */
        ULONG iov_len;			/* Must be size_t (1003.1g) */
};


int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len);
int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len);

#endif
