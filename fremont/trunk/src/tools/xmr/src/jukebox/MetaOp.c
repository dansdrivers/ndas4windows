/////////////////////////////////////////////
//
//		MetaOp.cpp
//		
//		Acess MediaJukebox Raw Operation
//
//
/////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include "../inc/LanScsiOp.h"
#include "cipher.h"
#include "../inc/MediaDisk.h"
#include "../inc/diskmgr.h"

//#define DDBG	

int debuglevelMetaOp = 1;

#define DEBUGMETACALL( _l_, _x_ )		\
		do{								\
			if(_l_ < debuglevelMetaOp)	\
				printf _x_;				\
		}	while(0)					\




static int RawDiskSecureRWOp(
	struct nd_diskmgr_struct * diskmgr,
	unsigned _int8 Command,
	unsigned _int64 start_sector,
	int	count,
	unsigned _int8 feature,
	unsigned char * buff)
{
	int result;
	unsigned _int8 response;


	struct media_key * MKey;
	MKey = (struct media_key *)diskmgr->MetaKey;

	DEBUGMETACALL(2, ("POSE in RawDiskSecureRWOp diskmgr->MetaKey(%p)\n", MKey)); 
	
	
	memset(diskmgr->Trnasferbuff,0,MAX_REQ_SIZE);

	if(Command == WIN_WRITE)
	{
		DEBUGMETACALL(2,("Call EncryptBlock buff(%p)\n", buff)); 
		EncryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			count*SECTOR_SIZE,
			(unsigned char*) buff, 
			(unsigned char*) diskmgr->Trnasferbuff
			);

	}
 

	DEBUGMETACALL(2, ("StarSec(%d), Count(%d)\n", (int)start_sector, count));
	
	result = IdeCommand(&diskmgr->Path, diskmgr->remote.ucUnitNumber,
						0,Command,start_sector,(__int16)count,0,(char *)diskmgr->Trnasferbuff,&response);

	if((result < 0) || response != LANSCSI_RESPONSE_SUCCESS)
	{
		printf("Error Writing MetaOp DISC\n");
		result= -1;
	}

/*
	{
		int i;
		unsigned char *buf = buff;
		for(i =0; i< 32; i++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
	}
*/
	if(Command == WIN_READ)
	{
		DEBUGMETACALL(2,("Call DecryptBlock buff(%p)\n", buff)); 
		DecryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			count*SECTOR_SIZE,
			(unsigned char*) diskmgr->Trnasferbuff,
			(unsigned char*) buff
			);
	}
/*
	{
		int i;
		unsigned char *buf = buff;
		for(i =0; i< 32; i++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
	}
*/
	if(result < 0) return 0;
	return 1;

}

int GetDiskMeta(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DDBG
	PON_DISK_META 	phead;
	phead = (PON_DISK_META)buf;
#endif //DDBG	
	if(count < 3) return 0;
DEBUGMETACALL(2,("_GetDiskMeta Enter %p\n", diskmgr));
	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,DISK_META_START,count,0,buf);
	if(ndStatus == 0) return 0;

#ifdef DDBG
DEBUGMETACALL(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DDBG

DEBUGMETACALL(2,("---> _GetDiskMeta Leave %p\n", diskmgr));
	return 1;	
}



int GetDiskMetaMirror(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int start_sector;
#ifdef DDBG
	PON_DISK_META 	phead;
	phead = (PON_DISK_META)buf;
#endif //DDBG	
	if(count < DISK_META_READ_SECTOR_COUNT) return 0;

DEBUGMETACALL(2,("_GetDiskMetaMirror Enter %p\n", diskmgr));
	start_sector = DISK_META_MIRROR_START;	

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,start_sector, count, 0, buf);
	if(ndStatus == 0) return 0;
#ifdef DDBG
DEBUGMETACALL(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DDBG

DEBUGMETACALL(2,("---> _GetDiskMetaMirror Leave %p\n", diskmgr));
	return 1;	
}



int GetDiskLogHead(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DDBG
	PON_DISK_LOG_HEADER phead;
	phead = (PON_DISK_LOG_HEADER) buf;
#endif //DDBG	
	if(count < 1) return 0;
DEBUGMETACALL(2,("_GetDiskLogHead Enter %p\n", diskmgr));


	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,DISK_LOG_START,count,0,buf);
	if(ndStatus == 0) return 0;
#ifdef DDBG
DEBUGMETACALL(2,("L-AGE(%d) L-INDEX(%d) L-ACTION(%d) L-HISTORY(%d)\n", phead->latest_age, phead->latest_index, phead->latest_log_action, phead->latest_log_history));
#endif //DDBG

DEBUGMETACALL(2,("---> _GetDiskLogHead Leave %p\n", diskmgr));
	return 1;	
}





int GetDiskLogData( struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int index, int count)
{
	unsigned int temp ;
	int ndStatus;
#ifdef DDBG
	PON_DISK_LOG phead;
#endif //DDBG
	if(count < 2) return 0;
	
DEBUGMETACALL(2,("_GetDiskLogData Enter %p : index %d\n", diskmgr, index));
	temp = index*DISK_LOG_DATA_SECTOR_COUNT;
	temp = temp + DISK_LOG_DATA_START;
	

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,temp,count,0,buf);
	if(ndStatus == 0) return 0;

#ifdef DDBG
	phead =(PON_DISK_LOG)(buf);
DEBUGMETACALL(2,("AGE(%d) INDEX(%d) ACTION(%d) VALID(%d)\n", phead->age, index, phead->action, phead->valid));
#endif //DDBG
DEBUGMETACALL(2,("---> _GetDiskLogData Leave %p : index %d\n", diskmgr, index));
	return 1;	
	
}


int SetDiskMeta(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
	//unsigned char	*pfree;
#ifdef DDBG
	PON_DISK_META phead;
	phead = (PON_DISK_META)buf;
#endif // DDBG	
	if(count < 1) return 0;

DEBUGMETACALL(2,("_SetDiskMeta Enter %p\n", diskmgr));
#ifdef DDBG
DEBUGMETACALL(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DDBG

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_WRITE,DISK_META_START,count,0,buf);
	if(ndStatus == 0) return 0;

DEBUGMETACALL(2,("---> _SetDiskMeta Leave %p\n", diskmgr));
	return 1;	
}




int SetDiskMetaMirror(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int	start_sector;
#ifdef DDBG
	PON_DISK_META phead;
	phead = (PON_DISK_META)buf;
#endif //DDBG	
	if(count < 1) return 0;

DEBUGMETACALL(2,("_SetDiskMetaMirror Enter %p\n", diskmgr));
#ifdef DDBG
DEBUGMETACALL(2,("AGE(%d) E-DISC(%d) NR-CLUTER(%d) NR-Avail-CLUTER(%d)\n",phead->age, phead->nr_Enable_disc, phead->nr_DiscCluster, phead->nr_AvailableCluster));
#endif //DDBG

	start_sector = DISK_META_MIRROR_START;

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_WRITE,start_sector,count,0,buf);
	if(ndStatus == 0) return 0;
DEBUGMETACALL(2,("---> _SetDiskMetaMirror Leave %p\n", diskmgr));
	return 1;	
}



int SetDiskLogHead(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DDBG
	PON_DISK_LOG_HEADER phead;
	phead = (PON_DISK_LOG_HEADER) buf;
#endif //DDBG	
	if(count < 1) return 0;

DEBUGMETACALL(2,("_SetDiskLogHead Enter %p\n", diskmgr));
#ifdef DDBG
DEBUGMETACALL(2,("L-AGE(%d) L-INDEX(%d) L-ACTION(%d) L-HISTORY(%d)\n", phead->latest_age, phead->latest_index, phead->latest_log_action, phead->latest_log_history));
#endif //DDBG

	ndStatus = RawDiskSecureRWOp(diskmgr,WIN_WRITE,DISK_LOG_START,count,0,buf);

	if(ndStatus == 0) return 0;
DEBUGMETACALL(2,("---> _SetDiskLogHead Leave %p\n", diskmgr));
	return 1;	
}





int SetDiskLogData( struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int index, int count)
{
	unsigned int temp ;
	int ndStatus;
#ifdef DDBG
	PON_DISK_LOG phead;
#endif //DDBG
	if(count < 2) return 0;
	
DEBUGMETACALL(2,("_SetDiskLogData Enter %p : index %d\n", diskmgr, index));
#ifdef DDBG
	phead = (PON_DISK_LOG)(buf);
DEBUGMETACALL(2,("AGE(%d) INDEX(%d) ACTION(%d) VALID(%d)\n", phead->age, index, phead->action, phead->valid));
#endif
	temp = index * DISK_LOG_DATA_SECTOR_COUNT;
	temp = temp + DISK_LOG_DATA_START;
	


	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_WRITE,temp,count,0,buf);
	if(ndStatus == 0) return 0;

DEBUGMETACALL(2,("---> _SetDiskLogData Leave %p : index %d\n", diskmgr, index));
	return 1;	
	
}

/*
	Read / Write Disc information
	buf must be larger than (1<<(DISK_META_SECTOR_COUNT + SECTOR_SIZE_BIT ))
*/
int GetDiscMeta(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DDBG
	PON_DISC_ENTRY phead;
#endif //DDBG	
	if(count < 1) return 0;

DEBUGMETACALL(2,("_GetDiscMeta Enter %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,disc_list->pt_loc,count,0,buf);
	if(ndStatus == 0) return 0;
#ifdef DDBG
	phead = (PON_DISC_ENTRY)buf;
DEBUGMETACALL(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DDBG
DEBUGMETACALL(2,("---> _GetDiscMeta Leave %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	return 1;	
}

int GetDiscMetaMirror(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int startsector = disc_list->pt_loc + DISC_META_MIRROR_START;
#ifdef DDBG
	PON_DISC_ENTRY phead;
#endif //DDBG
	if(count < 1) return 0;

DEBUGMETACALL(2,("_GetDiscMetaMirror Enter %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_READ,startsector,count,0,buf);
	if(ndStatus == 0) return 0;

#ifdef DDBG
	phead = (PON_DISC_ENTRY)buf;
DEBUGMETACALL(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DDBG

DEBUGMETACALL(2,("---> _GetDiscMetaMirror Leave %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	return 1;	
}

int SetDiscMeta(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
#ifdef DDBG
	PON_DISC_ENTRY phead;
#endif //DDBG	
	if(count < 1) return 0;

DEBUGMETACALL(2,("_SetDiscMeta Enter %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
#ifdef DDBG
	phead = (PON_DISC_ENTRY)buf;
DEBUGMETACALL(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DDBG


	DEBUGMETACALL(2,("_SetDiscMeta %p Loc %d\n",diskmgr, disc_list->pt_loc));

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_WRITE,disc_list->pt_loc,count,0,buf);
	if(ndStatus == 0) return 0;

DEBUGMETACALL(2,("---> _SetDiscMeta Leave %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	return 1;	
}

int SetDiscMetaMirror(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count)
{
	int ndStatus;
	unsigned int startsector = disc_list->pt_loc + DISC_META_MIRROR_START;
#ifdef DDBG
	PON_DISC_ENTRY phead;
#endif //DDBG
DEBUGMETACALL(2,("_SetDiscMetaMirror Enter %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
#ifdef DDBG
	phead = (PON_DISC_ENTRY)buf;
DEBUGMETACALL(2,("INDEX(%d) LOC(%d) STATUS(%d)  ACT(%d) NR_SEC(%d) NR_CT(%d)\n", 
		phead->index, phead->loc, phead->status,  phead->action, phead->nr_DiscSector, phead->nr_DiscCluster));
#endif //DDBG

	if(count < 1) return 0;
	DEBUGMETACALL(2,("_SetDiskMetaMirror %p Loc %d\n", diskmgr, disc_list->pt_loc));

	ndStatus = RawDiskSecureRWOp(diskmgr, WIN_WRITE,startsector,count,0,buf);

	if(ndStatus == 0) return 0;
DEBUGMETACALL(2,("---> _SetDiscMetaMirror Leave %p : pt_loc %d\n", diskmgr, disc_list->pt_loc));
	return 1;	
}

int
RawDiskOp( 
	struct nd_diskmgr_struct * diskmgr, 
	unsigned _int8 Command, 
	unsigned _int64 start_sector, 
	int count,  
	unsigned _int8 feature, 
	unsigned char * buff)
{
	unsigned _int8 response;
	int ndStatus;


	ndStatus = IdeCommand(&diskmgr->Path, diskmgr->remote.ucUnitNumber,
						0,Command,start_sector,(__int16)count,0,(char *)buff,&response);

	if((ndStatus < 0) || response != LANSCSI_RESPONSE_SUCCESS)
	{
		printf("Error Reading header to DISC\n");
		return 0;
	}
	return 1;
}

/*
int traslate(
				struct media_addrmap *map,
				unsigned int req_startsec, 
				unsigned int req_sectors, 
				unsigned int *start_sec, 
				unsigned int *sectors
			)
{
	PDISC_MAP_LIST	addr = NULL;
	unsigned int	end_sector =  0;	
	unsigned int 	map_count = 0, i= 0;
	

	end_sector = req_startsec + req_sectors -1 ;
	map_count = map->count;
	for(i = 0; i < map_count; i++)
	{
		addr = &(map->addrMap[i]);
		
		if( (addr->Lg_StartSector  + addr->nr_SecCount -1) > end_sector )
		{
			if(addr->Lg_StartSector <= req_startsec  )
			{
				*start_sec = addr->StartSector + ( req_startsec - addr->Lg_StartSector) ;
				*sectors = req_sectors;
			}else{

				if(i == 0)
				{
					return 0;
				}

				addr = &(map->addrMap[i-1]);
				*start_sec = addr->StartSector + ( req_startsec - addr->Lg_StartSector);
				*sectors = addr->nr_SecCount - (req_startsec - addr->Lg_StartSector);
			}	
			break;
		}
	}

	if(i >= map_count) return 0;
	return 1;	
}

int JukeBoxOp(
			struct nd_diskmgr_struct * diskmgr,
			int		disc,
			unsigned _int8 Command, 
			unsigned int s_sector, //512 base
			int s_count,   //512 base
			unsigned char * buff
				)
{
	struct media_disc_info * Mdiscinfo = NULL;
	struct media_addrmap * map = diskmgr->DiscAddr;
	struct media_key * MKey;

	unsigned int	req_start_sector = s_sector; //  512
	unsigned int	req_sectors = s_count; // 512
	unsigned int	calc_s_sec =0;
	unsigned int	calc_sectors= 0;
	unsigned char * pbuff;
	unsigned _int8 response;
	int	ndStatus;

	if(map->disc != disc) {
		printf("DiscAddr disc %d is not same %d\n",map->disc, disc);
		return 0;
	}

	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	
	if(disc >= Mdiscinfo->count){
		printf("disc %d is too big\n", disc);
		return 0;
	}

	if(Mdiscinfo->discinfo[disc].encrypt == 1)
	{

		if(diskmgr->DiscKey->disc != disc) {
			printf("Disc %d key is not set\n",disc); 
			return 0;
		}

	}

	if(Command == WIN_WRITE)
	{
		if(Mdiscinfo->discinfo[disc].encrypt == 1){
			printf("Fail EncryptBlock \n");
			if(EncryptBlock(
				(PNCIPHER_INSTANCE)MKey->cipherInstance,
				(PNCIPHER_KEY)MKey->cipherKey,
				(s_count *SECTOR_SIZE),
				(unsigned char*) buff, 
				(unsigned char*) diskmgr->Trnasferbuff
				) < 0){
				
					printf("Fail EncryptBlock \n");
					return 0;
				}
		}else{
			memcpy(diskmgr->Trnasferbuff, buff, req_sectors*SECTOR_SIZE);
		}
	}	

	pbuff = diskmgr->Trnasferbuff;

	while(req_sectors > 0)
	{
			if(!translate(map, req_start_sector, req_sectors, &calc_s_sec, &calc_sectors) )
			{
				printf("fail get address\n");
				return 0;
			}

			if((calc_s_sec + calc_sectors -1) > diskmgr->Path.PerTarget[0].SectorCount)
			{
				printf("Request sector_size is too big\n");
				return 0;
			}

			if((calc_sectors * SECTOR_SIZE) >  MAX_REQ_SIZE)
			{
				printf("calc_sector_size %d too big\n", calc_sectors);
				return 0;
			}	
			
			ndStatus = IdeCommand(&diskmgr->Path, diskmgr->remote.ucUnitNumber,
								0,Command,calc_s_sec,(__int16)calc_sectors,0,(char *)pbuff,&response);

			if((ndStatus < 0) || response != LANSCSI_RESPONSE_SUCCESS)
			{
				printf("Error Reading header to DISC\n");
				return 0;
			}
			
			req_sectors -= calc_sectors;
			req_start_sector += calc_sectors;
			pbuff += calc_sectors;

	}


	if(Command == WIN_READ){
		if(Mdiscinfo->discinfo[disc].encrypt == 1){
		DEBUGMETACALL(2,("Call DecryptBlock buff(%p)\n", buff)); 
		DecryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			s_count*SECTOR_SIZE,
			(unsigned char*) diskmgr->Trnasferbuff,
			(unsigned char*) buff
			);
		}else{
			memcpy(buff,diskmgr->Trnasferbuff, s_count*SECTOR_SIZE);
		}
	}

	return s_count;
}
*/



int JukeBoxOp(
			struct nd_diskmgr_struct * diskmgr,
			int		disc,
			unsigned _int8 Command, 
			unsigned int s_sector, //512 base
			int s_count,   //512 base
			unsigned char * buff
				)
{
	
	struct media_disc_info * Mdiscinfo = NULL;
	struct media_addrmap * map = diskmgr->DiscAddr;
	struct media_key * MKey;

	unsigned int	req_start_sector = s_sector; //  512
	unsigned int	i = 0;
	unsigned int	AddrMap_count = 0;
	PDISC_MAP_LIST	addr = NULL;
	unsigned int	req_sectors = s_count; // 512
	unsigned int	end_sector =0;
	unsigned int	start_sector = 0;
	unsigned int	bSet = 0;	

	unsigned __int64	calc_start_sector = 0;
	int		calc_sector_size = 0;

	unsigned _int8 response;
	int	ndStatus;

	end_sector = req_start_sector + req_sectors -1;
	DEBUGMETACALL(2,("req_start_sector(%d)  req_sectors(%d) end_sector(%d)\n",
		req_start_sector, req_sectors, end_sector));
	////////////////////////////////////////////////////////////////
	//
	//		check Parameter
	//
	////////////////////////////////////////////////////////////////

	if(map->disc != disc) {
		printf("DiscAddr disc %d is not same %d\n",map->disc, disc);
		return 0;
	}

	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	
	if(disc >= Mdiscinfo->count){
		printf("disc %d is too big\n", disc);
		return 0;
	}

	if(Mdiscinfo->discinfo[disc].encrypt == 1)
	{

		if(diskmgr->DiscKey->disc != disc) {
			printf("Disc %d key is not set\n",disc); 
			return 0;
		}

	}

	/////////////////////////////////////////////////////
	//
	//		Get Address
	//
	/////////////////////////////////////////////////////
	

	
	AddrMap_count = map->count;
	DEBUGMETACALL(2,("Find Addr Map count %d  disc %d\n",map->count, map->disc));	
	
	for(i = 0; i< AddrMap_count; i++)
	{
		addr = &(map->addrMap[i]);
		DEBUGMETACALL(2,("LG_S_Sec(%d), NR_SEC(%d) S_Sec(%d) EndSector(%d)\n", 
				addr->Lg_StartSector, addr->nr_SecCount, addr->StartSector, end_sector));
		if( (addr->Lg_StartSector  + addr->nr_SecCount -1) > end_sector )
		{
			DEBUGMETACALL(2,("Find Addr Map\n"));
			DEBUGMETACALL(2,("LG_S_Sec(%d), NR_SEC(%d) S_Sec(%d)\n", 
				addr->Lg_StartSector, addr->nr_SecCount, addr->StartSector));
			if(addr->Lg_StartSector <= req_start_sector )
			{
				start_sector = addr->StartSector + ( req_start_sector - addr->Lg_StartSector) ;
				req_sectors = req_sectors;
				bSet =1;
			}else{

				if(i == 0)
				{
					printf("BUG BUG!!! Can't Find Address Map\n");
					goto error_out;						
				}

				addr = &(map->addrMap[i-1]);
				start_sector = addr->StartSector + ( req_start_sector - addr->Lg_StartSector);
				req_sectors = addr->nr_SecCount - (req_start_sector - addr->Lg_StartSector);
				bSet =1;
			}	
			break;
		}
	}

	if(bSet != 1)
	{
		printf("Can't Find Address Map\n");
		return 0;					
	}

	calc_start_sector = start_sector;
	calc_sector_size = req_sectors;

	if((calc_start_sector + calc_sector_size -1) > diskmgr->Path.PerTarget[0].SectorCount)
	{
		printf("Request sector_size is too big\n");
		return 0;
	}

	if((calc_sector_size * SECTOR_SIZE) >  MAX_REQ_SIZE)
	{
		printf("calc_sector_size %d too big\n", calc_sector_size);
		return 0;
	}


	///////////////////////////////////////////////////////
	//
	//		RawDiscIo
	//
	////////////////////////////////////////////////////////
	MKey = diskmgr->DiscKey;

	DEBUGMETACALL(2, ("POSE in JukeBoxOp diskmgr->DiscKey(%p)\n", MKey)); 	
	memset(diskmgr->Trnasferbuff,0,MAX_REQ_SIZE);

	
	if(calc_sector_size == 0) return  0;


	if(Command == WIN_WRITE)
	{
		if(Mdiscinfo->discinfo[disc].encrypt == 1){
			printf("Fail EncryptBlock \n");
			if(EncryptBlock(
				(PNCIPHER_INSTANCE)MKey->cipherInstance,
				(PNCIPHER_KEY)MKey->cipherKey,
				(calc_sector_size *SECTOR_SIZE),
				(unsigned char*) buff, 
				(unsigned char*) diskmgr->Trnasferbuff
				) < 0){
				
				printf("Fail EncryptBlock \n");
				return 0;
			}
		}else{
			memcpy(diskmgr->Trnasferbuff, buff, calc_sector_size*SECTOR_SIZE);
		}
	}	





	ndStatus = IdeCommand(&diskmgr->Path, diskmgr->remote.ucUnitNumber,
						0,Command,calc_start_sector,(__int16)calc_sector_size,0,(char *)diskmgr->Trnasferbuff,&response);

	if((ndStatus < 0) || response != LANSCSI_RESPONSE_SUCCESS)
	{
		printf("Error Reading header to DISC\n");
		return 0;
	}


//	{
//		int i;
//		unsigned char *buf = buff;
//		for(i =0; i< 32; i++)
//      {
//			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
//				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
//				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
//		}
//	}

	
//	printf("Call DecryptBlock buff(%p)\n", buff); 

	if(Command == WIN_READ){
		if(Mdiscinfo->discinfo[disc].encrypt == 1){
		DEBUGMETACALL(2,("Call DecryptBlock buff(%p)\n", buff)); 
		DecryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			calc_sector_size*SECTOR_SIZE,
			(unsigned char*) diskmgr->Trnasferbuff,
			(unsigned char*) buff
			);
		}else{
			memcpy(buff,diskmgr->Trnasferbuff, calc_sector_size*SECTOR_SIZE);
		}
	}

//	{
//		int i;
//		unsigned char *buf = buff;
//		for(i =0; i< 32; i++)
//		{
//			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
//				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
//				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
//		}
//	}
	
	return calc_sector_size;

error_out:
	return 0;
}

