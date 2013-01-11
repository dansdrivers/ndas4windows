/******************************************************************************
 *
 * Name:	sktypes.h
 * Project:	GEnesis, PCI Gigabit Ethernet Adapter
 * Version:	$Revision: 1.2.2.1 $
 * Date:	$Date: 2005/04/11 09:00:53 $
 * Purpose:	Define data types for Linux
 *
 ******************************************************************************/

/******************************************************************************
 *
 *	(C)Copyright 1998-2002 SysKonnect GmbH.
 *	(C)Copyright 2002-2005 Marvell.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/
 
#ifndef __INC_SKTYPES_H
#define __INC_SKTYPES_H

/* defines *******************************************************************/

#define u64		ULONGLONG
#define s64		LONGLONG
#define u32		ULONG
#define s32		LONG
#define u16		USHORT
#define s16		SHORT
#define u8		UCHAR
#define s8		CHAR

#define SK_I8    s8    /* 8 bits (1 byte) signed       */
#define SK_U8    u8    /* 8 bits (1 byte) unsigned     */
#define SK_I16  s16    /* 16 bits (2 bytes) signed     */
#define SK_U16  u16    /* 16 bits (2 bytes) unsigned   */
#define SK_I32  s32    /* 32 bits (4 bytes) signed     */
#define SK_U32  u32    /* 32 bits (4 bytes) unsigned   */
#define SK_I64  s64    /* 64 bits (8 bytes) signed     */
#define SK_U64  u64    /* 64 bits (8 bytes) unsigned   */

#define SK_UPTR	ULONG  /* casting pointer <-> integral */

#define SK_BOOL   SK_U8
#define SK_FALSE  0
#define SK_TRUE   (!SK_FALSE)

#define caddr_t		PUCHAR
#define __init
#define __initdata
#define __devinit
#define __devinitdata

#define num_online_cpus() 1

#endif	/* __INC_SKTYPES_H */

/*******************************************************************************
 *
 * End of file
 *
 ******************************************************************************/
