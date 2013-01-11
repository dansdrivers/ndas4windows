/*++

Copyright (C) 2002-2004 XIMETA, Inc.
All rights reserved.

--*/
#ifndef _SCRC32_H_
#define _SCRC32_H_


/*++
    
CRC32 with Polynomial 0x04C11DB7, which is used in 
PKZIP, AUTODIN II, Ethernet, FDDI.

Implementation is based on the static table lookup scheme.

Note: 

crc32.cpp included in 'ndserial' project has the value of 
XorOut as 00000000.

--*/

unsigned int 
crc32_calc(const unsigned char* p, int len);

#endif /* _SCRC32_H_ */