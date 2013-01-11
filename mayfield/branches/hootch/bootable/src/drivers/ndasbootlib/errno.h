#ifndef __ERRNO_H
#define __ERRNO_H

//#include <asm/errno.h>
#include "asm_errno.h"

/* Should never be seen by user programs */
#define ERESTARTSYS	512
#define ERESTARTNOINTR	513
#define ERESTARTNOHAND	514	/* restart if no handler.. */
#define ENOIOCTLCMD	515	/* No ioctl command */

#endif
