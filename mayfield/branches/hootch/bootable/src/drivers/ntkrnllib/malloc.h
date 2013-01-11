#ifndef __MALLOC_H
#define __MALLOC_H

#define MAX_MALLOC_SIZE (4 * 1024 * 1024)  // 2 MB
//#define MAX_MALLOC_SIZE (128 * 1024)  // 4 MB
unsigned long malloc_init(unsigned long size); 

void *(Malloc)(unsigned long size);
void (Free)(void *ptr);

extern unsigned char *malloc_buffer;

#endif __MALLOC_H
