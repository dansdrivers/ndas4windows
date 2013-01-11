/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2008, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 12/11/2008
 -------------------------------------------------------------------------
*/

#pragma once

#include "stdafx.h"

#include <pshpack1.h>

// This structure is LSB (Little Endian)
// If EEPROM reset is asserted, 
// 0x10~0xaf(MaxConnectionTimeout~UserPasswords) is set to default value.

typedef struct _PROM_DATA {

	UINT8	EthAddr[6]; 
	UINT8	Signature[2];
	UINT8	Reserved1[8];
	UINT16	MaxConnectionTimeout; 		// not valid in emu. depends on LPX
	UINT16	HeartBeatTimeout; 
	UINT32	MaxRetransmissionTimeout; 	// not valid in emu. depends on LPX 
	
	union {
	
		UINT8	Options;
	
		struct {

			UINT8 DataEncryption:1;  // Bit 0?
			UINT8 HeaderEncryption:1; 
			UINT8 DataCrc:1;
			UINT8 HeaderCrc:1;
			UINT8 JumboFrame:1;
			UINT8 NoHeartFrame:1;
			UINT8 ReservedOption:2;
		};
	};

	UINT8	DeadLockTimeout;
	UINT16	SataTimeout;
	UINT32	StandbyTimeout;
	UINT8	WatchdogTimeout; 
	UINT8   IdentifyTimeout;
	UINT8  	HighSpeedUsbDataTimeout;
	UINT8	FullSpeedUsbTxDataTimeout;
	UINT16	HighSpeedUsbRxAckTimeout;
	UINT16	FullSpeedUSBRxAckTimeout;
	UINT8	UserPermissions[8]; // 8 user. one byte for each
	UINT8	UserPasswords[8][16]; // 8 user. 128 bit for each.
	UINT8	Reserved2[336];
	UINT8	DeviceDescriptorHS[18];
	UINT8	DeviceQualifierHS[10];
	UINT8   ConfigurationDescriptorHS[9];
	UINT8	Interface[9];
	UINT8	BulkInEndpoint[7];
	UINT8	BulkOutEndpoint[7];
	UINT8	OtherSpeedConfigurationDescriptor[36];
	UINT8	DeviceDescriptorFS[18];
	UINT8	DeviceQualifierFS[10];
	UINT8 	ConfigurationDescriptorFS[9];

	UINT8	Reserved3[1024-645];

	UINT8	UserArea[1024];

} PROM_DATA, *PPROM_DATA; // Protected PROM area

#include <poppack.h>
