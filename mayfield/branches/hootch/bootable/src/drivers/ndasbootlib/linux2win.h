#ifndef __LINUX2WIN_H__
#define __LINUX2WIN_H__

#define kmalloc(size, flags) (VOID *) ExAllocatePoolWithTag(NonPagedPool, size, LSMP_PTAG_NDASBOOT)
#define kfree(p) ExFreePool(p)

#define KERN_DEBUG "NDBOOT_DBG:"
#define KERN_INFO  "NDBOOT_INFO:"
#define KERN_ERR   "NDBOOT_ERR:"

#endif __LINUX2WIN_H__