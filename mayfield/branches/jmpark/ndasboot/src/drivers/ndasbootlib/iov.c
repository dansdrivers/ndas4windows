#ifdef __ENABLE_LOADER__
#include "ntkrnlapi.h"
#endif

#include <ntddk.h>

#include "iov.h"
#include "errno.h"
 
#define min_t(type, a, b) (((type)(a) < (type)(b)) ? (a) : (b))
#define	copy_from_user memcpy
#define	copy_to_user memcpy

/*
 *      Copy kernel to iovec. Returns -EFAULT on error.
 *
 *      Note: this modifies the original iovec.
 */
 
int memcpy_toiovec(struct iovec *iov, unsigned char *kdata, int len)
{
        int err = -EFAULT;

        while(len>0)
        {
                if(iov->iov_len)
                {
                        int copy = min_t(unsigned int, iov->iov_len, len);
                        if (copy_to_user(iov->iov_base, kdata, copy) == NULL)
                                goto out;
                        kdata+=copy;
                        len-=copy;
                        iov->iov_len-=copy;
                        iov->iov_base=(unsigned char *)iov->iov_base+copy;
                }
                iov++;
        }
        err = 0;
out:
        return err;
}

/*
 *      Copy iovec to kernel. Returns -EFAULT on error.
 *
 *      Note: this modifies the original iovec.
 */
 
int memcpy_fromiovec(unsigned char *kdata, struct iovec *iov, int len)
{       
        int err = -EFAULT;

		while(len>0)
        {       
                if(iov->iov_len)
                {       
                        int copy = min_t(unsigned int, len, iov->iov_len);
                        if (copy_from_user(kdata, iov->iov_base, copy) == NULL) 
                                goto out;
                        len-=copy;
                        kdata+=copy;
                        iov->iov_base=(unsigned char *)iov->iov_base+copy;
                        iov->iov_len-=copy;
                }
                iov++;
        }
        err = 0;
out:    
        return err;
}
