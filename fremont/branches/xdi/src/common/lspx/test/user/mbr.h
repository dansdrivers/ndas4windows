#ifndef _MBR_H_INCLUDED
#define _MBR_H_INCLUDED

#include <stdlib.h>
#include <pshpack1.h>

#ifndef C_ASSERT_TAGGED
#define C_ASSERT_TAGGED(e,tag) \
	typedef char __C_ASSERT_TAGGED__##tag[(e)?1:-1]
#endif

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
#else
#include <stdint.h>
#endif

enum {
	PARTITION_STATE_NONE = 0x00,
	PARTITION_STATE_ACTIVE = 0x80,
};

enum { 
	PARTITION_ID_NONE = 0x00,
	PARTITION_ID_FAT12 = 0x01,
	PARTITION_ID_FAT16 = 0x06,
	PARTITION_ID_IFS = 0x07,
	PARTITION_ID_DELL_OEM = 0xDE,
	PARTITION_ID_IBM_OEM = 0xFE,
	PARTITION_ID_HIDDEN = PARTITION_ID_IBM_OEM,
	PARTITION_ID_NDAS_VS= PARTITION_ID_HIDDEN,
};

typedef struct _partition_entry_t {
	uint8_t  state;
	uint8_t  begin_head;
	uint16_t begin_cyl_sec;
	uint8_t  type;
	uint8_t  end_head;
	uint16_t end_cyl_sec;
	uint32_t lba;
	uint32_t sector_count;
} mbr_partition_entry_t;

C_ASSERT_TAGGED(sizeof(mbr_partition_entry_t) == 16, pe);

typedef struct _master_boot_record_t {
	uint8_t bootcode[446];
	mbr_partition_entry_t partition[4];
	uint8_t signature[2];
} master_boot_record_t;

C_ASSERT_TAGGED(sizeof(master_boot_record_t) == 512, mbr);

#include <poppack.h>

enum {
	KB_IN_SECTORS = 2,
	MB_IN_SECTORS = 1024 * KB_IN_SECTORS,
	GB_IN_SECTORS = 1024 * MB_IN_SECTORS
};

#endif /* _MBR_H_INCLUDED */
