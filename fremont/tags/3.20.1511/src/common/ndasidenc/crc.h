/*
 *	Copyright (C) 2001-2004 XIMETA, Inc. All rights reserved.
 */

#pragma once
#ifndef _CRC_
#define _CRC_

#ifdef __cplusplus
extern "C" {
#endif

void			CRC_init_table(unsigned long *table);
unsigned long	CRC_reflect(unsigned long ref, char ch);
unsigned long	CRC_calculate(unsigned char *buffer, unsigned int size);

#ifdef __cplusplus
}
#endif

#endif /* _CRC_ */