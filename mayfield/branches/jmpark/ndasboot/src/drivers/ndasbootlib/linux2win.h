#ifndef __LINUX2WIN_H__
#define __LINUX2WIN_H__

#define kmalloc(size, flags) (VOID *) ExAllocatePoolWithTag(NonPagedPool, size, LSMP_PTAG_NDASBOOT)
#define kfree(p) ExFreePool(p)

#endif __LINUX2WIN_H__