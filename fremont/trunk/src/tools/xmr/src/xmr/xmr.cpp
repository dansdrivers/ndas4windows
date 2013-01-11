// xmr.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "../inc/LanScsiOp.h"
#include "../inc/dvdcopy.h"
#include "../inc/JBOp.h"


#define HASH_KEY_USER           0x1F4A50731530EABB
#define HASH_KEY_SUPER1         0x0F0E0D0304050607
#define HASH_KEY_SUPER          HASH_KEY_USER

#define HASH_KEY_USER_SAMPLE	0x0001020304050607
#define HASH_KEY_SUPER_SAMPLE	HASH_KEY_USER_SAMPLE

#define Release	"1.0"
#define Version "jukebox 1.0"
#define E_USAGE		4
#define DEVSIZE		30
#define ADDSIZE		30
#define DISKSIZE		30
#define FILESIZE		215
#define FILEPATH		512
#define TITLE_NAME	128
#define	LPX_PORT_NUMBER	10000

static unsigned long ulSlotNo = 0;
static int opt_v;

static struct nd_diskmgr_struct * SelDisk;

static void CleanUpWSAsock()
{
	int err;
	err = WSACleanup();
	if(err != 0) {
		printf("main: WSACleanup Fail");
		return;
	}
}

static char * safe_strncpy(char *dst, const char * src, size_t size)
{
	errno_t err = 0;
	dst[size-1] = '\0';
	err = strncpy_s(dst, size, src, size-1);
	if(err != 0) {
		return NULL;
	}
	return dst;
}

int sscanf_lpx_addr(char *host, PLPX_ADDRESS slpx)
{
        unsigned __int16 hi = 0;
        unsigned __int32 lo = 0;
        unsigned int c;
                                                                                                                        
        for (;;) {
                if (isxdigit(c = *host++)) {
                        hi = (hi << 4) | ((lo >> 28) & 0x0F);
                        c -= '0';
                        if(c > 9)
                                c = (c + '0' - 'A' + 10) & 0x0F;
                        lo = (lo << 4) | c;
                } else if(c == ':')
                        continue;
                else
                        break;
        }
                                                                                                                        
        slpx->Node[0] = hi >> 8;
        slpx->Node[1] = (UCHAR)hi;
        slpx->Node[2] = lo >> 24;
        slpx->Node[3] = lo >> 16;
        slpx->Node[4] = lo >> 8;
        slpx->Node[5] = lo;
                                                                                                                        
        return 0;
}

static void version(void)
{
    fprintf(stderr, "%s\n%s\n", Release, Version);
    return ;
}

static void usage(void)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "jukeboxtest disk  MAC_ADDR \n");
	fprintf(stderr, "jukeboxtest disc  MAC_ADDR disc_index \n");
	fprintf(stderr, "jukeboxtest burnS  MAC_ADDR sel_disc sector_size\n");
	fprintf(stderr, "jukeboxtest burnE  MAC_ADDR sel_disc \n");
	fprintf(stderr, "jukeboxtest format MAC_ADDR \n");	
	fprintf(stderr, "jukeboxtest del MAC_ADDR  sel_disc\n");
	fprintf(stderr, "jukeboxtest Read MAC_ADDR sel_disc sector\n");
	fprintf(stderr, "jukeboxtest Rcopy MAC_ADDR DVD_SOURCE_FILE disc\n");
	fprintf(stderr, "jukeboxtest ValDisc MAC_ADDR DVD_SOURCE_FILE disc_num\n");
}


int do_nd_up(char *nd_addr)
{
	LANSCSI_ADD_TARGET_DATA	Target;
	int			err = 0;
	unsigned long		SlotNumber;
	LPX_ADDRESS		Address;
	
	SlotNumber = ulSlotNo++;	
	
	sscanf_lpx_addr(nd_addr, &Address);
	Address.Port = LPX_PORT_NUMBER;

	memset(&Target,0,sizeof(LANSCSI_ADD_TARGET_DATA));

	Target.ulSize = sizeof(LANSCSI_ADD_TARGET_DATA);
	Target.ulSlotNo = SlotNumber;
	Target.ucTargetType = 0;
	Target.ulTargetBlocks = 0;
	
	Target.DesiredAccess = GENERIC_WRITE;
	
	Target.ucNumberOfUnitDiskList = 1;

	memcpy(&Target.UnitDiskList[0].Address.Node, &Address.Node, 6);
	
	fprintf(stderr,"TARGET ADDRESS %02x:%02x:%02x:%02x:%02x:%02x\n", 
			Target.UnitDiskList[0].Address.Node[0],
			Target.UnitDiskList[0].Address.Node[1],
			Target.UnitDiskList[0].Address.Node[2],
			Target.UnitDiskList[0].Address.Node[3],
			Target.UnitDiskList[0].Address.Node[4],
			Target.UnitDiskList[0].Address.Node[5]);

	Target.UnitDiskList[0].Address.Port = htons(LPX_PORT_NUMBER);

	fprintf(stderr, "TARGET ENABLE for READ_WRITE\n");
	Target.UnitDiskList[0].iUserID = FIRST_TARGET_RW_USER;
	
	
	Target.UnitDiskList[0].iPassword = HASH_KEY_USER;
	Target.UnitDiskList[0].ucUnitNumber = 0;
	Target.UnitDiskList[0].ulUnitBlocks = 0;
	Target.UnitDiskList[0].ucHWType = 0;
	Target.UnitDiskList[0].ucHWVersion = 0;

	SelDisk = InitializeJB( &Target );
	
	return err;
}


int nd_down()
{
	CleanUpWSAsock();
	return closeJB(SelDisk);
}


int do_nd_get_disk(char *nd_addr)
{
	int i = 0;
	struct media_disc_info * Mdiscinfo = NULL;
	int err = 0;

	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}
	
	printf("SelDisk INFORMATION\n");
	Mdiscinfo = (struct media_disc_info *)SelDisk->mediainfo;
	for(i = 0; i < Mdiscinfo->count ; i++)
	{
		printf("INDEX : %d\n", i);
		printf("STATSU :\n");
		if(Mdiscinfo->discinfo[i].valid & DISC_LIST_ASSIGNED)
			printf("\t DISC_LIST_ASSIGNED\n");
		else
			printf("\t DISC_LIST_EMPTY\n");

		if(Mdiscinfo->discinfo[i].valid & DISC_LIST_ASSIGNED)
		{
			if(Mdiscinfo->discinfo[i].valid &(DISC_STATUS_VALID | DISC_STATUS_VALID_END))
				printf("\t DISC_STATUS_VALID\n");

			if(Mdiscinfo->discinfo[i].valid & DISC_STATUS_WRITING)
				printf("\t DISC_STATUS_WRITING\n");

			if(Mdiscinfo->discinfo[i].valid & DISC_STATUS_INVALID)
				printf("\t DISC_STATUS_INVALID\n");
		}

		if(Mdiscinfo->discinfo[i].encrypt == 1){
			printf("ENCRYPTED : TRUE\n");
		}else{
			printf("ENCRYPTED : FALSE\n");
		}
			
	}
error_out:	
	nd_down();
	return err;

}

int nd_get_disk(int argc, char * argv[])
{
	char			nd_addr[ADDSIZE];
	char			**spp;
	
	if(argc < 1) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	
	return do_nd_get_disk(nd_addr);
}


int do_nd_get_disc_data(char *nd_addr, int disc_index, char * pdata )
{
	PGET_DISC_INFO		pdisc_info;
	PU_DISC_DATA		pdisc_data;
	char 			*buf = pdata;
	
	int			err = 0;

	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}

	pdisc_info = (PGET_DISC_INFO) buf;
	pdisc_info->selected_disk = 0;
	pdisc_info->selected_disc = disc_index;
	pdisc_info->OpType = COM_GET_DISC_DATA;
	pdisc_info->size = sizeof(GET_DISC_INFO);
	
	if( !GetCurrentDiscInfo( SelDisk, disc_index,COM_GET_DISC_DATA, (unsigned char *)buf))
	{
		printf("Fail GetDiscInfo\n");
		err = -1;
		goto error_out;
	}

	if(pdisc_info->result == 0)
	{
		printf("FAIL GET DISC INFO : TRY AGAIN!!\n");
		err = -1;
		goto error_out;
	}

	printf("DISC INFO : disk(%d) disc index(%d)\n", 0, disc_index);
	if(pdisc_info->status == DISC_STATUS_INVALID)
	{
		printf("\t DISC IS IN INVALID STATUS\n");
		goto error_out;
	}

	if(pdisc_info->status == DISC_STATUS_ERASING)
	{
		printf("\t DISC IS IN WAITING ERASING\n");
		goto error_out;
	}

	if(pdisc_info->status == DISC_STATUS_WRITING)
	{
		printf("\t DISC IS IN WRITING STATUS\n");
		goto error_out;
	}


	printf("\t DISC IS IN VALID STATUS\n");
	


	pdisc_data = (PU_DISC_DATA) &(pdisc_info->data[0]);

	printf("\t TITLE %s\n", pdisc_data->title_name);
	printf("\t TITLE INFO %s\n", pdisc_data->title_info);
	printf("\t ADDITIONAL INFO %s\n", pdisc_data->additional_infomation);

	
error_out:
	nd_down();
	return err;

}

int nd_get_disc(int argc, char * argv[])
{
	char			nd_addr[ADDSIZE];
	char			buff[2048];
	char			**spp;
	int				disc;

	if(argc < 2) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	disc = atoi(*spp++);

	return do_nd_get_disc_data(nd_addr, disc, buff );
}


int do_nd_format_disk( char *nd_addr)
{
	int			err = 0;

	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		//err = -1;
		//goto error_out;
	}	

	if(DISK_FORMAT(SelDisk) < 0)
	{
		printf("Fail Disk Format\n");
		err = -1;
		goto error_out;
	}
	
error_out:
	nd_down();
	return err;

}

int nd_format_disk(int argc, char **argv)
{
	char			nd_addr[ADDSIZE];
	char			**spp;
	
	if(argc < 1) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	return do_nd_format_disk(nd_addr);
}


int do_nd_del_disc(char * nd_addr, int selected_disc)
{
	int		err = 0;
	
	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}	
	
	if(DeleteCurrentDisc( SelDisk, selected_disc, 0) < 0)
	{
		printf("Fail DelectCurrentDisk\n");
		err = -1;
		goto error_out;
	}
	
error_out:
	nd_down();
	return err;

}

int nd_del_disc(int argc, char **argv)
{
	char			nd_addr[ADDSIZE];
	char			**spp;
	int				disc;

	if(argc < 2) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	disc = atoi(*spp++);

	return do_nd_del_disc(nd_addr, disc);
}


int do_nd_burn_disc_start(struct nd_diskmgr_struct * diskmgr, int selected_disc, char * Title, int sector_count)
{

	char					hint[32];
//	WRITE_DISC					write_lock;
	int			err = 0;
	
	memset(hint,0,32);
	memcpy(hint, Title, 16);

//	if(BurnStartCurrentDisc ( diskmgr,sector_count, selected_disc, 1, (unsigned char *)hint, 0 ) < 0)
	if(BurnStartCurrentDisc ( diskmgr,sector_count, selected_disc, 0, (unsigned char *)hint, 0 ) < 0)
	{	
		printf("Fail BurnStartCurrentDisc\n");
		err = -1;
		goto error_out;
	}
	
error_out:
	return err;
											
}




int do_nd_burn_disc_end(struct nd_diskmgr_struct * diskmgr,int selected_disc, char * title_name, char * additionalInfo)
{
	PEND_BURN_INFO			Info;
	char 				*Infobuf;
	char				buf[128];
	int			err = 0;
	time_t		t;
	
	Infobuf = (char *)malloc(1024);
	if(!Infobuf)
	{
		fprintf(stderr,"Can't alloc Infobuf \n");
		return -1;
	}

	memset(Infobuf,0,1024);
	Info = (PEND_BURN_INFO)Infobuf;
	memcpy(Info->title_name, title_name, 128);	
	fprintf(stderr,"title %s\n", Info->title_name);
	if(time(&t))
	{
		memset(buf, 0 , 128);
		sprintf_s(buf, 128, "TIME %d\n", t);
		memcpy(Info->title_info, buf, 128);
		fprintf(stderr,"title info %s\n", Info->title_info);
	}
	memset(buf, 0 , 128);
	sprintf_s(buf,128, "%s",additionalInfo);
	memcpy(Info->additional_infomation, buf, 128);
	fprintf(stderr,"additional info %s\n",Info->additional_infomation);
	
	
	if(BurnEndCurrentDisc( diskmgr ,selected_disc, Info, 0) < 0)
	{
		printf("Fail burnEndCurrentDisc\n");
		err = -1;
		goto error_out;
	}


error_out:
	free(Infobuf);
	return err;
											
}

int do_nd_burnS_test(int argc, char * argv[])
{
	char			nd_addr[ADDSIZE];
	char			Title[512];
	char			**spp;
	int				disc;
	int				sector_size;
	int				err = 0;
	if(argc < 3) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	disc = atoi(*spp++);
	sector_size = atoi(*spp++);
	sprintf_s(Title, 512, "ILGU LOVE");

	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}	

	do_nd_burn_disc_start(SelDisk, disc, Title, sector_size);
error_out:
	CleanUpWSAsock();
	return err;
}

int do_nd_burnE_test(int argc, char * argv[])
{
	char			nd_addr[ADDSIZE];
	char			Title[512];
	char			AddInfo[512];
	char			**spp;
	int				disc;
	int				err = 0;

	if(argc < 2) {
		usage();
		CleanUpWSAsock();
		return -1;
	}
	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	disc = atoi(*spp++);
	sprintf_s(Title,512, "ILGU LOVE");
	sprintf_s(AddInfo,512, "ADD iiiiifon");

	do_nd_up(nd_addr);

	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}	

	do_nd_burn_disc_end(SelDisk, disc, Title, AddInfo);
error_out:
	CleanUpWSAsock();
	return err;
}


int do_nd_real_copy(char * nd_addr, char * dvd_dev_name, int disc)
{
	unsigned int	TotalSectorCount;
	char		Title_name[128];
	char		Title_info[128];
//	unsigned char*		buf;
//	int i,j;
//	int max_loop = 0;

	int			err;
	int			FD;
	do_nd_up(nd_addr);


	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}	

	//get DVD info
	if(GetDVDTitleName(dvd_dev_name, Title_name) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error GetDvdTitleName %s\n", dvd_dev_name);
		err = -1;
		goto error_out;
	}
	
	fprintf(stderr,"TITLE NAME of %s : %s\n",dvd_dev_name,Title_name);
	
	if( GetDVDSize(dvd_dev_name, (int *)&TotalSectorCount) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error GetDvdSize %s\n", dvd_dev_name);
		err = -1;
		goto error_out;
	}
	
	fprintf(stderr,"SIZE of DVD TITLE : %s (%d)\n", Title_name, TotalSectorCount);

	//start Burn
	if(do_nd_burn_disc_start(SelDisk, disc, Title_name, TotalSectorCount) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error : do_nd_burn_disc_start\n");
		err = -1;
		goto error_out;
	}

	FD = JKBox_open(disc, O_WRITE);
	if(FD < 0){
		printf("Fail Open Disc\n");
		err = -1;
		goto error_out;
	}

/*
	buf = (unsigned char *)malloc((1024*64));
	if(buf == NULL){
		printf("fail alloc buf\n");
		err = -1;
		goto error_out;
	}

	max_loop = (int)(TotalSectorCount/128);
	for(i = 0; i<max_loop; i++)
	{
		memset(buf,(i%10),(1024*64));

		for(j = 0; j<32; j++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}

		JKBox_write(FD,(char *)buf,32);
	}

	JKBox_lseek(FD,0,SEEK_B);

	for(i =0; i< 5; i++)
	{
		memset(buf,0,2048);
		JKBox_read(FD,(char *)buf,1);	
		for(j = 0; j<32; j++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
	}
*/

	// Writing Data to Disc
	if(SetDVDCopytoMedia(dvd_dev_name, FD) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error : GetDvdCopytoMedia \n");
		return -1;
	}	

	
	if(JKBox_close(FD) < 0)
	{
		printf("Fail Close Disc\n");
		err = -1;
		goto error_out;
	}


	
	//end Burn
	sprintf_s(Title_info,128, "%d\n", TotalSectorCount);	
	if(do_nd_burn_disc_end(SelDisk, disc, Title_name,Title_info  ) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error : do_nd_burn_disc_end\n");
		err = -1;
		goto error_out;
	}
/*
	FD = JKBox_open(disc, O_READ);
	if(FD < 0){
		printf("Fail Open Disc\n");
		err = -1;
		goto error_out;
	}

	for(i =0; i< 5; i++)
	{
		memset(buf,0,2048);
		JKBox_read(FD,(char *)buf,1);	
		for(j = 0; j<32; j++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
	}
*/

error_out:
	CleanUpWSAsock();
	return err;
}


int nd_real_copy(int argc, char **argv)
{
	char	**spp;
	char	nd_addr[ADDSIZE];
	char	dvd_dev_name[DEVSIZE];
	int		disc;

	
	if(argc < 3)
	{
		usage();
		return -1;
	}

	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	safe_strncpy(dvd_dev_name, *spp++, DEVSIZE);
	disc = atoi(*spp++);
	return do_nd_real_copy(nd_addr, dvd_dev_name,disc);
}


int nd_Read_test(char *nd_addr, int disc, int sector)
{
	int i,j;
	int			err;
	int			FD;
	unsigned char		buf[2048];

	do_nd_up(nd_addr);


	if(SelDisk == NULL) return -1;
	if(SelDisk->isJukeBox == TRUE){
		printf("SelDisk TYPE IS : MEDIA_JUKE_BOX\n");
	}else {
		printf("SelDisk TYPE IS NOT MEDIA_JUKE_BOX\n");
		err = -1;
		goto error_out;
	}	

	FD = JKBox_open(disc, O_READ);
	if(FD < 0){
		printf("Fail Open Disc\n");
		err = -1;
		goto error_out;
	}

	JKBox_lseek(FD,sector,SEEK_B);

	for(j =0; j< 5; j++)
	{
		printf("Sector %d\n",j);
		memset(buf,0,2048);
		JKBox_read(FD,(char *)buf,1);	
		for(i = 0;i<32; i++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
	}

error_out:
	CleanUpWSAsock();
	return err;
}


int do_nd_Read_test(int argc, char ** argv)
{
	char	**spp;
	char	nd_addr[ADDSIZE];
	int		disc;
	int		sector;
	
	if(argc < 3)
	{
		usage();
		return -1;
	}

	spp = argv;
	safe_strncpy(nd_addr, *spp++, ADDSIZE);
	disc = atoi(*spp++);
	sector = atoi(*spp++);
	return nd_Read_test(nd_addr, disc, sector);
}

int main(int argc, char* argv[])
{
	int err;
	WORD				wVersionRequested;
	WSADATA				wsaData;

	if(argc < 2) 
		goto out;

	/* Find any options. */
	argc--;
	argv++;
	while (argc && *argv[0] == '-') {
		if (!strcmp(*argv, "-v"))
			opt_v = 1;

		if (!strcmp(*argv, "-V") || !strcmp(*argv, "-version") ||
		    !strcmp(*argv, "--version"))
			version();

		if (!strcmp(*argv, "-?") || !strcmp(*argv, "-h") ||
		    !strcmp(*argv, "-help") || !strcmp(*argv, "--help")) {
			usage();
			exit(E_USAGE);
		}

		argv++;
		argc--;
	}	

	wVersionRequested = MAKEWORD( 2, 2 );
	err = WSAStartup(wVersionRequested, &wsaData);
	if(err != 0) {
		printf("main: WSAStartup Faile");
		return -1;
	}



	if(!strcmp(argv[0], "disk")){
		return nd_get_disk(--argc, ++argv);
	} else if(!strcmp(argv[0], "disc")){
		return nd_get_disc(--argc, ++argv);
	} else if(!strcmp(argv[0], "format")){
		return nd_format_disk(--argc, ++argv);	
	} else if(!strcmp(argv[0], "del")){
		return nd_del_disc(--argc, ++argv);
	} else if(!strcmp(argv[0], "Rcopy")){
		return nd_real_copy(--argc, ++argv);
	} else if(!strcmp(argv[0], "ValDisc")){
		//return nd_validate_disc(--argc, ++argv);
	} else if(!strcmp(argv[0], "burnS")){
		return do_nd_burnS_test(--argc, ++argv);
	} else if(!strcmp(argv[0], "burnE")){
		return do_nd_burnE_test(--argc, ++argv);
	}else if(!strcmp(argv[0], "Read")){
		return do_nd_Read_test(--argc, ++argv);
	}

out:
	usage();
	exit(1);
}