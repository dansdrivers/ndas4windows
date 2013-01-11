#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mbr.h"
#include "lsp_host.h"

#if defined(__BIG_ENDIAN__)

#define htoles(x) (lsp_byteswap_ushort(x))
#define htolel(x) (lsp_byteswap_ulong(x))
#define htolell(x) (lsp_byteswap_uint64(x))
#define letohs(x) (lsp_byteswap_ushort(x))
#define letohl(x) (lsp_byteswap_ulong(x))
#define letohll(x) (lsp_byteswap_uint64(x))

#else

#define htoles(x) (x)
#define htolel(x) (x)
#define htolell(x) (x)
#define letohs(x) (x)
#define letohl(x) (x)
#define letohll(x) (x)

#endif

#if defined(_MSC_VER)
#define dbg_printf __noop
#else
void dbg_printf(char* format, ...) {}
#endif

static const uint8_t mbr_signature[2] = {0x55, 0xAA};

static uint16_t cs_enc(uint16_t cyl, uint16_t sec)
{
	if (sec >= 0x40 || cyl >= 0x400)
	{
		return 0xFFFF;
	}
	assert(sec < 0x40);
	assert(cyl < 0x400);
	return ((cyl & 0xFF) << 8) | (cyl & 0xFF00) >> 2 | (sec & 0x3F);
}

static void cs_dec(uint16_t cs, uint16_t* cyl, uint16_t* sec)
{
	*cyl = ((cs & 0xFF00) >> 8) | ((cs & 0xC0) << 16);
	*sec = cs & 0x3F;
}

enum { 

	LBA_SECTORS_PER_TRACK = 63,

	LBA_HEADS_PER_CYLINDER_16 = 16,
	LBA_HEADS_PER_CYLINDER_32 = 32,
	LBA_HEADS_PER_CYLINDER_64 = 64,
	LBA_HEADS_PER_CYLINDER_255 = 255,

	LBA_HEADS_PER_CYLINDER = LBA_HEADS_PER_CYLINDER_255,
 };

typedef struct _mbr_chs_t {
	uint16_t c;
	uint16_t h;
	uint16_t s;
} mbr_chs_t;

/*
 * CHS    1024 cyls *  16 heads * 63 sectors * 512 bytes = 528 MB (INT 13h)
 * L-CHS  1024 cyls * 256 heads * 63 sectors * 512 bytes = 8 GB (INT 13h AH=0xH)
 * P-CHS 65535 cyls *  16 heads * 63 sectors * 512 bytes = 136 GB
 *
 * LBA 0 == CHS (0,0,1)
 */

static void lba_to_chs(uint32_t lba, mbr_chs_t* chs)
{
#if 0
	/* http://www.osdever.net/tutorials/chs_lba.php?the_id=87 */
	chs->s = (lba % LBA_SECTORS_PER_TRACK) + 1;
	chs->c = (lba / LBA_SECTORS_PER_TRACK) / LBA_HEADS_PER_CYLINDER;
	chs->h = (lba / LBA_SECTORS_PER_TRACK) % LBA_HEADS_PER_CYLINDER;
#else
	/* http://www.ata-atapi.com/hiwtab.htm */
	uint16_t tmp;
	chs->c = lba / (LBA_HEADS_PER_CYLINDER * LBA_SECTORS_PER_TRACK);
	if (chs->c > 0x3FFF)
	{
		chs->c = 0x3FFF;
		chs->h = 0xFE;
		chs->s = 0x3F;
		return;
	}
	tmp = lba % (LBA_HEADS_PER_CYLINDER * LBA_SECTORS_PER_TRACK);
	chs->h = tmp / LBA_SECTORS_PER_TRACK;
	chs->s = tmp % LBA_SECTORS_PER_TRACK + 1;
#endif
}

uint32_t chs_to_lba(mbr_chs_t chs)
{
	/* http://www.ata-atapi.com/hiwtab.htm */
	return (chs.c * LBA_HEADS_PER_CYLINDER + chs.h) * 
		LBA_SECTORS_PER_TRACK + chs.s - 1;
}

void set_partition_lba(
	mbr_partition_entry_t* part,
	uint32_t begin_lba, 
	uint32_t lba_capacity)
{
	mbr_chs_t begin_chs;
	mbr_chs_t end_chs;
	uint32_t end_lba;

	lba_to_chs(begin_lba, &begin_chs);
	if (begin_chs.c < 1024)
	{
		part->begin_cyl_sec = htoles(cs_enc(begin_chs.c, begin_chs.s));
		if (part->end_cyl_sec == 0xFFFF) end_chs.h = 0xFF;
		part->begin_head = (uint8_t) begin_chs.h;
	}
	else
	{
		part->begin_head = 0xFF;
		part->begin_cyl_sec = htoles(0xFFFF);
	}

	dbg_printf("part.begin(%02X, %04X)\n", 
		part->begin_head, part->begin_cyl_sec);

	end_lba = begin_lba + lba_capacity - 1;
	lba_to_chs(end_lba, &end_chs);

	dbg_printf("part (%d,%d,%d) %u-%u\n",
		end_chs.c, end_chs.h, end_chs.s, begin_lba, lba_capacity);

	if (end_chs.c < 1024)
	{
		part->end_cyl_sec = htoles(cs_enc(end_chs.c, end_chs.s));
		if (part->end_cyl_sec == 0xFFFF) end_chs.h = 0xFF;
		part->end_head = (uint8_t) end_chs.h;
	}
	else
	{
		part->end_head = 0xFF;
		part->end_cyl_sec = htoles(0xFFFF);
	}

	dbg_printf("part.end(%02X, %04X)\n", 
		part->end_head, part->end_cyl_sec);

	part->lba = htolel(begin_lba);
	part->sector_count = htolel(lba_capacity);

}

int ndavs_disp_mbr(void* buf)
{
	int i;
	master_boot_record_t* mbr;

	mbr = (master_boot_record_t*) buf;
	if (mbr_signature[0] != mbr->signature[0] ||
		mbr_signature[1] != mbr->signature[1])
	{
		fprintf(stderr, "MBR signature is invalid.\n");
		return 1;
	}

	fprintf(stdout, "p a type chs_begin        chs_end          lba_begin  sectors\n");
	fprintf(stdout, "----------------------------------------------------------------\n");

	for (i = 0; i < 4; ++i)
	{
		mbr_partition_entry_t* part = &mbr->partition[i];
		mbr_chs_t begin_chs, end_chs;

		cs_dec(letohs(part->begin_cyl_sec), &begin_chs.c, &begin_chs.s);
		begin_chs.h = part->begin_head; 

		cs_dec(letohs(part->end_cyl_sec), &end_chs.c, &end_chs.s);
		end_chs.h = part->end_head; 

		fprintf(stdout, "%d %d %02X (%5d, %3d, %2d) (%5d, %3d, %2d) %10d %10d \n",
			i, part->state, part->type,
			begin_chs.c, begin_chs.h, begin_chs.s,
			end_chs.c, end_chs.h, end_chs.s,
			letohl(part->lba),
			letohl(part->sector_count));
		fprintf(stdout, "       (%5X, %3X, %2X) (%5X, %3X, %2X)   %8X   %8X \n",
			begin_chs.c, begin_chs.h, begin_chs.s,
			end_chs.c, end_chs.h, end_chs.s,
			letohl(part->lba),
			letohl(part->sector_count));
	}

	return 0;
}

void ndavs_build_mbr(
	master_boot_record_t* mbr,
	uint32_t disk_lba_cap,
	uint32_t rmpart_lba_cap,
	uint8_t rmpart_type)
{
	uint32_t begin_lba, end_lba, lba_capacity;
	mbr_partition_entry_t* part;
	mbr_chs_t begin_chs, end_chs, cap_chs;

	memcpy(mbr->signature, mbr_signature, 2);

	/* the first sector of a partition will be aligned such that it is
	at head 0, sector 1 of a cylinder */

	/* most new versions of FDISK start the first partition (primary
	or extended) at cylinder 0, head 1, sector 1 */

	/* readme partition type */

	part = &mbr->partition[0];
	begin_chs.c = 0; begin_chs.h = 1; begin_chs.s = 1;
	begin_lba = chs_to_lba(begin_chs);
	dbg_printf(" begin_lba=%u\n", begin_lba);

	/* partition end sector is aligned to the cylinder size (8MB) */
	dbg_printf(" required_lba_cap=%u\n", rmpart_lba_cap);

	end_lba = begin_lba + rmpart_lba_cap - 1;
	dbg_printf(" end_lba=%u\n", end_lba);

	lba_to_chs(end_lba, &end_chs);
	dbg_printf(" end_chs=%d,%d,%d\n", end_chs.c, end_chs.h, end_chs.s);

	end_chs.h = 0xFE;
	end_chs.s = 0x3F;
	dbg_printf(" adjusted end_chs=%d,%d,%d\n", end_chs.c, end_chs.h, end_chs.s);
	end_lba = chs_to_lba(end_chs);
	dbg_printf(" adjusted end_lba=%u\n", end_lba);

	lba_capacity = end_lba - begin_lba + 1;
	dbg_printf(" adjusted lba_cap=%u\n", lba_capacity);

	set_partition_lba(part, begin_lba, lba_capacity);
	part->state = PARTITION_STATE_NONE;
	part->type = rmpart_type;

	/* ndas virtual storage area partition */

	/* for this area, we only applies begin_lba to the alignment convention.
	 * end_head/lba_capacity fully extents to the lba_capacity of the disk
	 * to protect other soft partition application to invade (e.g. dynamic disk)
	 */

	part = &mbr->partition[1];
	begin_lba = begin_lba + lba_capacity;
	lba_to_chs(begin_lba, &begin_chs);

	/* alignment to head 0, sector 1 */
	if (begin_chs.h != 0 || begin_chs.s != 1)
	{
		begin_chs.c += 1;
		begin_chs.h = 0;
		begin_chs.s = 1;
	}
	begin_lba = chs_to_lba(begin_chs);
	lba_capacity = disk_lba_cap - begin_lba;
	set_partition_lba(part, begin_lba, lba_capacity);
	part->type = PARTITION_ID_NDAS_VS;
	part->state = PARTITION_STATE_NONE;

}

#if defined(MBR_CMD)
int 
#if defined(_MSC_VER)
__cdecl
#endif
main(int argc, char** argv)
{
	char* bc_fname, *mbr_fname;
	FILE* bc_file, *mbr_file;
	size_t read;
	master_boot_record_t mbr;
	uint32_t disk_lba_cap;
	uint32_t readme_lba_cap;
	uint8_t readme_ptype;

	static const char usage[] = 
		"usage: ndavsmbr" 
		" <output-file> <disk-lba-capacity> "
		" [readme-part-lba-capacity] [readme-part-type] [boot-code-file]";

	if (argc != 3 && argc != 4 && argc != 5 && argc != 6)
	{
		fprintf(stderr, "%s\n", usage);
		return 1;
	}

	memset(&mbr, 0, sizeof(mbr));

	/* default value */
	readme_lba_cap = 16002;
	readme_ptype = PARTITION_ID_FAT12;

	mbr_fname = argv[1];
	disk_lba_cap = strtol(argv[2], NULL, 0);
	if (argc > 3) 
	{
		readme_lba_cap = strtol(argv[3], NULL, 0);
	}
	if (argc > 4)
	{
		readme_ptype = (uint8_t) strtol(argv[4], NULL, 0);
	}
	if (argc > 5) 
	{
		bc_fname = argv[5];
		bc_file = fopen(bc_fname, "rb");
		if (NULL == bc_file)
		{
			fprintf(stderr, "error: opening %s failed, %s\n", 
					bc_fname, strerror(errno));
			return 1;
		}

		read = fread(mbr.bootcode, 446, 1, bc_file);
		if (read != 1)
		{
			fprintf(stderr, "error: reading boot code from %s failed, %s\n",
					bc_fname, strerror(errno));
			fclose(bc_file);
			return 1;
		}
		fclose(bc_file);
	}

	mbr_file = fopen(mbr_fname, "wb");
	if (NULL == mbr_file)
	{
		fprintf(stderr, "error: creating %s failed, %s\n", 
				mbr_fname, strerror(errno));
		return 1;
	}

	ndavs_build_mbr(
		&mbr, disk_lba_cap, readme_lba_cap, readme_ptype);

	ndavs_disp_mbr(&mbr);

	if (1 != fwrite(&mbr, 512, 1, mbr_file))
	{
		fprintf(stderr, "error: writing to %s failed, %s\n", 
				mbr_fname, strerror(errno));
		fclose(mbr_file);
		return 1;
	}

	fclose(mbr_file);

	return 0;
}

#endif
