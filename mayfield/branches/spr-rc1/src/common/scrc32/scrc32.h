/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#ifndef _SCRC32_H_
#define _SCRC32_H_
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*++
    
CRC32 with Polynomial 0x04C11DB7, which is used in 
PKZIP, AUTODIN II, Ethernet, FDDI.

Implementation is based on the static table lookup scheme.

Note: 

crc32.cpp included in 'ndserial' project has the value of 
XorOut as 00000000.

--*/

unsigned int 
__stdcall
crc32_calc(const unsigned char* p, unsigned int len);

#ifdef __cplusplus
}
#endif

#endif /* _SCRC32_H_ */
