#include <stdio.h>
#include <stdlib.h>
#include "../inc/BinaryParameters.h"
#include "../inc/hash.h"
#include "../inc/LanScsiOp.h"

#include "../inc/diskmgr.h"
#include "cipher.h"
#include "lockOp.h"
#include "../inc/MediaDisk.h"
#include "MediaInfo.h"
#include "MediaOp.h"
#include "MetaOp.h"
#include "DIB.h"
#include "../inc/JBOp.h"
#include "scrc32.h"

int debuglevelJbOp = 1;

#define DEBUGJBOP( _l_, _x_ )			\
		do{								\
			if(_l_ < debuglevelJbOp)	\
				printf _x_;				\
		}	while(0)					\

static int Iocount = 0;
/***************************************************

  Global Data

*****************************************************/
struct nd_diskmgr_struct * DISK = NULL;

BOOL IsJukeBox(struct nd_diskmgr_struct *diskmgr, unsigned _int64 sector_count);
int MakeDiscView(struct nd_diskmgr_struct *diskmgr);
int closeJB(struct nd_diskmgr_struct * diskmgr);
int  Disk_init(struct nd_diskmgr_struct * diskmgr, 
				PLANSCSI_ADD_TARGET_DATA add_adapter_data);
int MakeKeybuff(struct nd_diskmgr_struct * diskmgr);
int MakeKey(struct nd_diskmgr_struct *diskmgr, unsigned int disc, struct media_key * MKey);
///////////////////////////////////////////////////////////
//
//
//	virtual jukebox file interface
//
//
///////////////////////////////////////////////////////////

int JKBox_open(int disc, int rw)
{
	struct media_disc_info * Mdiscinfo = NULL;
	if(DISK == NULL) return -1;
	if(DISK->isInitialized == FALSE) return -1;
	if(DISK->isJukeBox == FALSE) return -1;
	if(DISK->flags & DMGR_FLAG_NEED_FORMAT) return -1;
	if(DISK->OpenFileFd != JUKEMAGIC ) return -1;
	

	Mdiscinfo = (struct media_disc_info *)DISK->mediainfo;
	if(disc >= Mdiscinfo->count)	return -1;
	
	if(!(rw & O_MASK))	return -1;

	if(rw == O_READ)
	{
		if(!(Mdiscinfo->discinfo[disc].valid & DISC_LIST_ASSIGNED))
		{
			return -1;
		}

		if(! (Mdiscinfo->discinfo[disc].valid & (DISC_STATUS_VALID | DISC_STATUS_VALID_END) ) )
		{
			return -1;
		}
	}
	else if(rw == O_WRITE)
	{
		if(!(Mdiscinfo->discinfo[disc].valid & DISC_LIST_ASSIGNED))
		{
			return -1;
		}
		
		if(!(Mdiscinfo->discinfo[disc].valid & DISC_STATUS_WRITING))
		{
			return -1;
		}
	}else {
		return -1;
	}

	if(Mdiscinfo->discinfo[disc].encrypt == 1)
	{
		MakeKey(DISK, disc, DISK->DiscKey);
	}

	if(SetDiscAddr(DISK,DISK->DiscAddr,disc) < 0)
	{
		return -1;
	}

	DISK->OpenFileFd = (JUKEMAGIC + disc);
	DISK->CurrentFilePointer = 0;
	return DISK->OpenFileFd;
}

int JKBox_close(int fd)
{
	if(fd != (int)DISK->OpenFileFd) return -1;
	DISK->OpenFileFd = JUKEMAGIC;
	DISK->CurrentFilePointer = 0;
	return 0;
}

int JKBox_lseek(int fd, unsigned int sector_count, int pose)
{
	if(fd != (int)DISK->OpenFileFd) return -1;
	if(!(pose & SEEK_MASK))	return -1;

	if(pose == SEEK_B)
	{
		DISK->CurrentFilePointer = sector_count;
	}
	else if( pose == SEEK_E )
	{
		// not supported 
		return DISK->CurrentFilePointer;
	}
	else if( pose == SEEK_C )
	{
		DISK->CurrentFilePointer += sector_count;
		if(DISK->CurrentFilePointer < 0){
			DISK->CurrentFilePointer = 0;
		}
		return DISK->CurrentFilePointer;
	}
	else {
		return -1;
	}
	return -1;
}

int JKBox_read(int fd, char * buff, int sector_count)
{
	int disc;
	char * pbuff = buff;
	int	len ,size, iReceived, s_sector, received_len;
	if(fd != (int)DISK->OpenFileFd) return -1;
	if(fd < JUKEMAGIC) return -1;
	
	disc = fd - JUKEMAGIC;
	len = size = sector_count * 4 ;// 512 based size
	 s_sector = DISK->CurrentFilePointer * 4 ;//512 based start sector
	received_len = 0;
	iReceived = 0;


	if( (s_sector + size -1 ) >(int)  DISK->mediainfo->discinfo[disc].sector_count)
	{
		printf("sector size is too big\n");
		return -1;
	}

	

	while(iReceived < size)
	{
		received_len = JukeBoxOp(DISK,disc,WIN_READ,s_sector, len, (unsigned char *)pbuff);
		if(received_len == 0) return -1;

		iReceived += received_len;
		s_sector += received_len;
		len -= received_len,
		pbuff += (received_len*SECTOR_SIZE);
	}
	DISK->CurrentFilePointer += sector_count; 

	Iocount ++;
//	if(!(Iocount % 200)) {
//		printf("call sleep\n");
//		Sleep(100);
//	}

	printf("Read current (%d) total(%d)\r", (s_sector + size -1), DISK->mediainfo->discinfo[disc].sector_count);
	return sector_count;
}

int JKBox_write(int fd, char *buff, int sector_count)
{
	int disc;
	char * pbuff = buff;
	int	len ,size, iReceived, s_sector, received_len;
	if(fd != (int)DISK->OpenFileFd) {
		printf("OpenFileFD%d is not FD %d\n",DISK->OpenFileFd, fd);
		return -1;
	}
	if(fd < JUKEMAGIC) {
		printf("fd is small than JUKEMAGIC %d\n",fd);
		return -1;
	}
	disc = fd - JUKEMAGIC;
	len = size = sector_count * 4 ;// 512 based size
	s_sector = DISK->CurrentFilePointer * 4; //512 based start sector
	received_len = 0;
	iReceived = 0;


	if( (s_sector + size -1 ) > (int) DISK->mediainfo->discinfo[disc].sector_count)
	{
		printf("sector size is too big\n");
		return -1;
	}


	while(iReceived < size)
	{
		received_len = JukeBoxOp(DISK,disc,WIN_WRITE,s_sector, len, (unsigned char *)pbuff);
		if(received_len == 0) return -1;

		iReceived += received_len;
		s_sector += received_len;
		len -= received_len,
		pbuff += (received_len*SECTOR_SIZE);
	}

	DISK->CurrentFilePointer += sector_count;
	
	Iocount ++;
//	if(!(Iocount % 200)) {
//		printf("call sleep\n");
//		Sleep(100);
//	}

	printf("WRITING current (%d) total(%d)\r", (s_sector + size -1), DISK->mediainfo->discinfo[disc].sector_count);
	return sector_count;
}

/****************************************************

	Juke box manager operation 

*****************************************************/
struct nd_diskmgr_struct * InitializeJB( PLANSCSI_ADD_TARGET_DATA add_adapter_data )
{

//	char buff[512];
//	struct media_key * MKey;
	
	DISK = (struct nd_diskmgr_struct *)malloc(sizeof(struct nd_diskmgr_struct));
	
	if(DISK == NULL){
		printf("Fail Alloc DISK \n");
	}

	if(Disk_init(DISK, add_adapter_data) < 0)
	{
		printf("Fail initialization \n");
		closeJB(DISK);
		return NULL;
	}

	
	if(MakeKeybuff(DISK) <0 )
	{
		printf("Fail alloc Keybuffer\n");
		closeJB(DISK);
		return NULL;
	}

/*
	//	test encryption / decryption

		MKey = (struct media_key *)DISK->MetaKey;
		memset(buff,0,512);
		sprintf(buff,"ILGU LOVE ILGU LOVE ILGU LOVE!!!!! ILGU LOVE ILGU LOVE\n");
		printf("TEST STR:%s\n",buff);
		EncryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			512,
			(unsigned char*) buff, 
			(unsigned char*) buff
			);

		
		DecryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			512,
			(unsigned char*) buff,
			(unsigned char*) buff
		);
		printf("RESULT STR:%s\n",buff);
	//
*/
	if(DISK->isJukeBox == TRUE)
	{
		 CheckFormat(DISK);
		 if(DISK->flags & DMGR_FLAG_FORMATED)
		 {
			MakeDiscView(DISK);
		 }
	}
	
	DISK->OpenFileFd = JUKEMAGIC;

	return DISK;
}


int closeJB(struct nd_diskmgr_struct * diskmgr)
{

	if(diskmgr->MetaKey) {
		free(diskmgr->MetaKey);
		diskmgr->MetaKey = NULL;
	}

	if(diskmgr->DiscKey) {
		free(diskmgr->DiscKey);
		diskmgr->DiscKey = NULL;
	}

	if(diskmgr->DiscAddr){
		free(diskmgr->DiscAddr);
		diskmgr->DiscAddr= NULL;
	}

	if(diskmgr->encryptInfo){
		free(diskmgr->encryptInfo);
		diskmgr->encryptInfo= NULL;
	}

	if(diskmgr->mediainfo){
		free(diskmgr->mediainfo);
		diskmgr->mediainfo= NULL;
	}

	if(diskmgr->Trnasferbuff){
		free(diskmgr->Trnasferbuff);
		diskmgr->Trnasferbuff= NULL;
	}

	Logout(&diskmgr->Path);
	closesocket(diskmgr->Path.connsock);
	free(diskmgr);
	return 0;
}

int  Disk_init(struct nd_diskmgr_struct * diskmgr, 
		PLANSCSI_ADD_TARGET_DATA add_adapter_data)
{
	REMOTE_TARGET           remote;
	int 			ndStatus;
	struct 		media_disc_info * Mdiscinfo;
	PDISC_LIST	pdisclist;
	int index = 0;
	int i;

	DEBUGJBOP(2,("enter diskmgr_init---\n"));
	memset(diskmgr, 0, sizeof(struct nd_diskmgr_struct));

	diskmgr->isInitialized = FALSE;
	diskmgr->isJukeBox = FALSE;

	diskmgr->MetaKey = NULL;
	diskmgr->DiscKey = NULL;
	diskmgr->DiscAddr = NULL;
	diskmgr->encryptInfo = NULL;
	diskmgr->mediainfo = NULL;
	diskmgr->Trnasferbuff = NULL;

	diskmgr->MetaKey = (struct media_key *)malloc(sizeof(struct media_key));
	if(!diskmgr->MetaKey) {
		printf("Fail Allocation diskmgr->MetaKey\n");
		return -1;
	}
	memset(diskmgr->MetaKey, 0, sizeof(struct media_key));

	diskmgr->DiscKey = (struct media_key *)malloc(sizeof(struct media_key));
	if(!diskmgr->DiscKey) {
		printf("Fail Allocation diskmgr->DiscKey\n");
		return -1;
	}
	memset(diskmgr->DiscKey, 0, sizeof(struct media_key));

	diskmgr->DiscAddr = (struct media_addrmap *) malloc(sizeof(struct media_addrmap));
	if(!diskmgr->DiscAddr) {
		printf("Fail Allocation diskmgr->DiscAddr\n");
		return -1;
	}
	memset(diskmgr->DiscAddr, 0, sizeof(struct media_addrmap));

	diskmgr->mediainfo = (struct media_disc_info *)malloc(sizeof(struct media_disc_info));
	if(!diskmgr->mediainfo) {
		printf("Fail Allocation diskmgr->mediainfo\n");
		return -1;
	}
	memset(diskmgr->mediainfo, 0, sizeof(struct media_disc_info));

	diskmgr->encryptInfo = (struct media_encryptID *)malloc(sizeof(struct media_encryptID));
	if(!diskmgr->encryptInfo) {
		printf("Fail Allocation diskmgr->encryptInfo\n");
		return -1;
	}
	memset(diskmgr->encryptInfo, 0, sizeof(struct media_encryptID));


	diskmgr->Trnasferbuff = (unsigned char*)malloc(MAX_REQ_SIZE); 
	if(!diskmgr->Trnasferbuff) {
		printf("Fail Allocation diskmgr->Trnasferbuff\n");
		return -1;
	}

	memcpy( &remote.LpxAddress,
		&add_adapter_data->UnitDiskList[index].Address,
		sizeof(LPX_ADDRESS)
		);

	remote.ucUnitNumber = add_adapter_data->UnitDiskList[index].ucUnitNumber;
	remote.ulUnitBlocks = add_adapter_data->UnitDiskList[index].ulUnitBlocks;
	remote.DesiredAccess = add_adapter_data->DesiredAccess;
	remote.iUserID = add_adapter_data->UnitDiskList[index].iUserID;
	remote.iPassword = add_adapter_data->UnitDiskList[index].iPassword;	
	//
	// to support version 1.1 2.0 20040401
	//

	memcpy(&diskmgr->remote,
		&remote, 
		sizeof(REMOTE_TARGET)
		);
	
	//	Initialize Path
	memset((char *)&diskmgr->Path, 0, sizeof(LANSCSI_PATH));
	diskmgr->Path.iPassword = remote.iPassword;
	diskmgr->Path.iUserID = remote.iUserID;

	//
	// to support version 1.1 2.0 20040401
	//

	// end of supporting version
	DEBUGJBOP(2,("passwd = %016llx\n", (*(unsigned __int64 *)(&diskmgr->Path.iPassword))));

		
	if(!MakeConnection(&remote.LpxAddress,&diskmgr->Path)) {
		printf("can't connect\n");
		return -1;
	}

	if((ndStatus = Login(&diskmgr->Path,LOGIN_TYPE_NORMAL)) < 0) {
		printf("First Login fail this may be version mismatch Retry!\n");  
	

		if((ndStatus = Login(&diskmgr->Path,LOGIN_TYPE_NORMAL)) < 0) {
				printf("can't login\n");
				closesocket(diskmgr->Path.connsock);
				return -1;
		}
	}
		
	if((ndStatus = GetDiskInfo(&diskmgr->Path, remote.ucUnitNumber)) <0 ) {
		printf("can't get disk info\n");
		Logout(&diskmgr->Path);
		closesocket(diskmgr->Path.connsock);
		return -1;
	}

	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	memset(Mdiscinfo, 0, sizeof(struct media_disc_info));
	Mdiscinfo->count = -1;
	for(i =0; i <MAX_DISC_LIST; i++)
	{
		pdisclist = &(Mdiscinfo->discinfo[i]);
		pdisclist->minor = -1;			
	}

	diskmgr->remote.ucHWType = diskmgr->Path.HWType;
	diskmgr->remote.ucHWVersion = diskmgr->Path.HWVersion;

	diskmgr->isJukeBox= IsJukeBox(diskmgr, diskmgr->Path.PerTarget[0].SectorCount);


	//
	//	alloc variable for type DMGR_TYPE_MEDIA
	//

	DEBUGJBOP(2,("SectorCount %d\n", (int)(diskmgr->Path.PerTarget[0].SectorCount)));
	diskmgr->nr_sectors =(int)( diskmgr->Path.PerTarget[0].SectorCount - XIMETA_MEDIA_INFO_SECTOR_COUNT );
	DEBUGJBOP(2,("SectorCount is set : diskmgr->nr_sectors (%ld)\n", diskmgr->nr_sectors));
	if(diskmgr->heads == 0) diskmgr->heads = (SECTOR_SIZE);
	if(diskmgr->sectors == 0) diskmgr->sectors = SECTOR_SIZE;
	diskmgr->cyls = diskmgr->nr_sectors / (diskmgr->heads*diskmgr->sectors);	

	DEBUGJBOP(2,("nr_sectors(%ld), cyls(%ld), heads(%ld), sectors(%ld)\n", 
		diskmgr->nr_sectors,
		diskmgr->cyls,
		diskmgr->heads,
		diskmgr->sectors)); 

	// assign memory for driver_state information

	DEBUGJBOP(2,("DISK %d FLAGS %08x\n", index, diskmgr->flags));
	DEBUGJBOP(2,("---end diskmgr_init\n"));
	diskmgr->isInitialized = TRUE;
	return ndStatus;
}



int MakeKeybuff(struct nd_diskmgr_struct * diskmgr)
{
	unsigned _int64 Password = METAKEY;
	unsigned char Key[32];
	struct media_key * MKey;
	memset(Key,0,32);
	memcpy(Key,METASTRING,16);

	if(diskmgr->isInitialized == FALSE) return -1;
//	if(diskmgr->isJukeBox == FALSE) return -1;



	MKey = (struct media_key *)diskmgr->MetaKey;

	memset(MKey, 0, sizeof(struct media_key));
	MKey->disk = 0;
	if(!CreateCipher(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,	
			NDAS_CIPHER_AES,
			NCIPHER_MODE_ECB,
			HASH_KEY_LENGTH,
			(unsigned char *)&Password,
			0,
			0)
	)
	{
		printf( "can't set Keyinstance  disk->MetaKey\n");
		return -1;
	}

	if(!CreateCipherKey(
			(PNCIPHER_KEY)MKey->cipherKey,
			(PNCIPHER_INSTANCE)MKey->cipherInstance,	
			NDAS_CIPHER_AES_LENGTH,
			Key, 
			0,
			0)
	)
 	{
		printf( "can't set Key disk->MetaKey\n");
		return -1;
	}
	
	
	return 0;
		
}



int MakeKey(struct nd_diskmgr_struct *diskmgr, unsigned int disc, struct media_key * MKey)
{
	struct 		media_disc_info * Mdiscinfo;
	PDISC_LIST		disclist = NULL;
	unsigned char * buf = NULL;


		
	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;
	disclist = &Mdiscinfo->discinfo[disc];


	printf("enter Makekey---\n");
	if(disclist->valid & DISC_STATUS_INVALID)
	{
		printf("MakeKey invalid status\n");
		return 0;
	}
		


	if(disclist->encrypt == 1) {
		unsigned char * pKey;
		struct	media_encryptID * Mencrypt = NULL;
		EHINT	* Ehint = NULL;
//		unsigned char * buf;
//		int i;

		Mencrypt = (struct media_encryptID *)diskmgr->encryptInfo;
		Ehint = &Mencrypt->hint[disc];		
		pKey = (unsigned char *)Ehint;
/*
		buf = pKey;
		for(i = 0; i< 2; i++)
		{
			printf("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
		}
*/

		memset(MKey,0,sizeof(struct media_key));
		MKey->disk = 0;
		MKey->disc = disc;	
/*
		buf = (unsigned char *)&diskmgr->Path.iPassword;
		printf("iPassword 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
			buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
*/
		if(!CreateCipher(
				(PNCIPHER_INSTANCE)MKey->cipherInstance,	
				NDAS_CIPHER_AES,
				NCIPHER_MODE_ECB,
				HASH_KEY_LENGTH,
				(unsigned char *)&diskmgr->Path.iPassword,
				MKey->disk,
				MKey->disc)
		)
		{
			printf("can't set Keyinstance  Media Key\n");
			return 0;
			
		}

		if(!CreateCipherKey(
				(PNCIPHER_KEY)MKey->cipherKey,
				(PNCIPHER_INSTANCE)MKey->cipherInstance,	
				NDAS_CIPHER_AES_LENGTH,
				pKey, 
				MKey->disk,
				MKey->disc)
		)
 		{
			printf("can't set Key  Media Key\n");
			return 0;
		}

	}

	printf("---end Makekey\n");
	return 1;
}


BOOL IsJukeBox(struct nd_diskmgr_struct *diskmgr, unsigned _int64 sector_count)
{
	unsigned char *		buf = NULL;
	PNDAS_DIB_V2	DIBV2 = NULL;
	BOOL				bResult = FALSE;

	DEBUGJBOP(2,("enter diskmgr_setDiskType---\n"));
	buf =(unsigned char*)malloc(1024);
	if(!buf){
		printf("data buffer not allocated\n");
		return FALSE;
	}
	memset(buf,0,1024);

	DEBUGJBOP(2,("setDiskType SecCount %lld\n", sector_count));


	if(!RawDiskOp(diskmgr, WIN_READ, sector_count -2 ,1 ,0 ,buf))
	{
		printf("fail RawDiskOp Read DIB\n");
		bResult = FALSE;
		goto error_out;
	}		
		
	DIBV2 = (PNDAS_DIB_V2)buf;
	if((DIBV2->Signature != NDAS_DIB_V2_SIGNATURE)	
		|| (DIBV2->MajorVersion != NDAS_DIB_VERSION_MAJOR_V2)
		|| (DIBV2->MinorVersion != NDAS_DIB_VERSION_MINOR_V2)
		)
	{

		bResult = FALSE;
		goto error_out;
	}

	if(DIBV2->iMediaType == NMT_MEDIA_DISK){
		DEBUGJBOP(2,("---set type MMGR_TYPE_DISK\n"));
		bResult = FALSE;
		goto error_out;
	} else if (DIBV2->iMediaType == NMT_MEDIAJUKE){
		DEBUGJBOP(2,("---set type MMGR_TYPE_MEDIA\n"));
		bResult = TRUE;
	} else {
		printf("fail unsupported DIB type\n");
		bResult = FALSE;
		goto error_out;
	}

	DEBUGJBOP(2,("---end diskmgr_setDiskType\n"));
error_out:
	free(buf);
	return bResult;
}



int MakeDiscView(struct nd_diskmgr_struct *diskmgr)
{
	unsigned char * buf;
	struct media_disc_info * Mdiscinfo = NULL;
	int result = 0;
	int i = 0;
	
	DEBUGJBOP(2,("enter diskmgr_MakeDiscView---\n"));
	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT)
	{
		printf("DMGR_FLAG_NEED_FORMAT\n");
		DEBUGJBOP(2,("POSE diskmgr->mediainfo->data %p", diskmgr->mediainfo));
		Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
		memset(Mdiscinfo, 0, sizeof(struct media_disc_info));
		Mdiscinfo->count = 0;
		return 0;
	}
	
	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("fail get ubuf\n");
		return 0;
	}
	memset(buf, 0, 4096);

	// get Disk Meta
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT))
	{
		printf("fail GetDiskMeta\n");
		result = 0;
		goto error_out;
	}	
	SetDiskMetaInformation(diskmgr, (PON_DISK_META)buf, 0);

	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT))
	{
		printf("fail GetDiskMeta\n");
		result = 0;
		goto error_out;
	}	

	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	
	for(i = 0; i< Mdiscinfo->count; i++)
	{
		memset(buf, 0, 4096);
		if(Mdiscinfo->discinfo[i].valid & DISC_LIST_ASSIGNED)
		{
			if(!GetDiscMeta(diskmgr, 
					&Mdiscinfo->discinfo[i], 
					buf, 
					DISC_META_SECTOR_COUNT))
			{
				printf("fail GetDiscMeta\n");
				result = 0;
				goto error_out;
			}
			SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf, 0);
		}
	}		
	DEBUGJBOP(2,("---end diskmgr_MakeDiscView\n"));
	result = 1;

error_out:
	free(buf);
	return  result;
}





// general interface for wirting start
int BurnStartCurrentDisc ( 
						  struct nd_diskmgr_struct * diskmgr,
						  unsigned int	Size_sector,
						  unsigned int	selected_disc,
						  unsigned int	encrypted,
						  unsigned char * HINT,
						  unsigned int hostid
						  )
{	
	unsigned int currentDisc;
	unsigned int sector_count = Size_sector;		
	PON_DISK_META	hdisk;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list;
	int		result;
	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	struct media_disc_info * Mdiscinfo;
	
	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;
	currentDisc = selected_disc;

	DEBUGJBOP(2,("enter BurnStartCurrentDisc\n"));

	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		printf("BurnStartCurrentDisc : invalid burn->disc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		printf("BurnStartCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		printf("BurnStartCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}

	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("BurnStartCurrentDisc can't alloc ubuf1!\n");
		return 0;
	}
	memset(buf, 0, 4096);

	buf2 = (unsigned char *)malloc(4096);
	if(!buf2)
	{
		printf("BurnStartCurrentDisc can't alloc ubuf2!\n");
		result = 0;
		goto error_out;
	}
	memset(buf2, 0, 4096);

	disc_list = Mdiscinfo->discinfo;

	// update Disk meta
	if((result = _CheckUpdateDisk(diskmgr, 1, hostid ))== 0 )
	{
		printf("BurnStartCurrentDisc Error _CheckUpdateDisk \n");
		result= 0;
		goto error_out;		
	}
	
RECHECK:
	if(( result = _CheckUpdateSelectedDisk( diskmgr, 1, hostid,currentDisc)) == 0)
	{
		printf("BurnStartCurrentDisc Error _CheckUpdateSelectedDisk 3\n");
		result = 0;
		goto error_out;
	}

	
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("BurnStartCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}
	
	SetDiskMetaInformation(diskmgr, (PON_DISK_META)buf, 1);

	hdisk = (PON_DISK_META)(buf);
	
	// check current dics status
	if(disc_list[currentDisc].valid & DISC_LIST_ASSIGNED)
	{
		printf("BurnStartCurrentDisc Error already alloc\n");
		result = 0;
		goto error_out;
	}

	if(!GetDiscMeta(diskmgr, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		printf("BurnStartCurrentDisc Error GetDiskMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf2;

	if(entry->status != DISC_STATUS_INVALID) 
	{
			// un reachable code			
		if(!_InvalidateDisc (diskmgr, buf, buf2, hostid))
		{
			printf("BurnStartCurrentDisc Error _InvalidateDisc\n");
			result = 0;
			goto error_out;
		}
		goto RECHECK;			
	}

	if(encrypted == 1)
	{
		memset(entry->HINT,0,32);
		memcpy(entry->HINT,HINT,32);		
	}


	// update disk and disc information
	if(!_WriteAndLogStart(diskmgr, hdisk,entry, hostid,currentDisc,sector_count, encrypted))
	{	
		printf("BurnStartCurrentDisc Error: WriteAdnLogStart\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 1);
	SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf2, 1);
	
	MakeKey(diskmgr,currentDisc, diskmgr->DiscKey);

	result = 1;

error_out:
	if(buf) free(buf);			
	if(buf2) free(buf2);
	return result;			
	
}





int BurnEndCurrentDisc( 
					   struct nd_diskmgr_struct * diskmgr ,
					   unsigned int  selected_disc, 
					   PEND_BURN_INFO	info,
					   unsigned int hostid
					   )
{
	unsigned int currentDisc;
	int result;
	
	PON_DISK_META	hdisk;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list = NULL;
	
	unsigned char *	p;	

	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	struct media_disc_info * Mdiscinfo;

	DEBUGJBOP(2,("enter BurnEndCurrentDisc\n"));
	
	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;

	currentDisc = selected_disc;
	disc_list = Mdiscinfo->discinfo;
		
	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		printf("BurnEndCurrentDisc : invalid burn->disc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		printf("BurnEndCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		printf("BurnEndCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}
	
	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("BurnEndCurrentDisc can't alloc ubuf1!\n");
		return 0;
	}
	memset(buf, 0, 4096);


	buf2 = (unsigned char *)malloc(4096);
	if(!buf2)
	{
		printf("BurnEndCurrentDisc can't alloc ubuf2!\n");
		result = 0;
		goto error_out;
	}
	memset(buf2, 0, 4096);

	if((result = _CheckUpdateDisk(diskmgr, 1, hostid )) == 0 )
	{
		printf("BurnEndCurrentDisc Error _CheckUpdateDisk\n");
		result = 0;
		goto error_out;		
	}
	
	if(( result = _CheckUpdateSelectedDisk( diskmgr, 1, hostid,currentDisc)) == 0)
	{
		printf("BurnEndCurrentDisc Error _CheckUpdateSelectedDisk\n");
		result = 0;
		goto error_out;
	}
DEBUGJBOP(2,("enter BurnEndCurrentDisc 6\n"));
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("BurnEndCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 1);		

	hdisk = (PON_DISK_META)(buf);


	if(!GetDiscMeta(diskmgr, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		printf("BurnEndCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}


	
	entry = (PON_DISC_ENTRY) buf2;

	if(entry->status != DISC_STATUS_WRITING) 
	{
		if(!_InvalidateDisc (diskmgr, buf, buf2, hostid))
		{
			printf("BurnEndCurrentDisc Error InvalidateDisc\n");
			result = 0;
			goto error_out;
		}
		
		result = 0;
		goto error_out;	
	}


	
	// update disk and disc information
	DEBUGJBOP(2,("enter BurnEndCurrentDisc 9\n"));	
	// disc title
	p =(unsigned char *) ( buf2 + 	DISC_TITLE_START * SECTOR_SIZE) ;
	memset(p, 0, 512);
	memcpy(p, info->title_name, 128);
	p[127] = '\0';
	p += 128;
	memcpy(p, info->title_info, 128);
	p[127] = '\0';		
		// disc additional info
	p =(unsigned char *) ( buf2 + 	DISC_ADD_INFO_START * SECTOR_SIZE) ;
	memset(p, 0, 512);
	memcpy(p, info->additional_infomation, 128);
	p[127] = '\0';
		// disc key info
	p =(unsigned char *) ( buf2 + 	DISC_KEY_START* SECTOR_SIZE) ;
	memset(p,0,512);
	memcpy(p, info->key, 128);		
	p[127] = '\0';
	
	if(!_WriteAndLogEnd( diskmgr, hdisk,entry, hostid, currentDisc))
	{
		result = 0;
		goto error_out;	
	}
	
	DEBUGJBOP(2,("enter BurnEndCurrentDisc 5\n"));
	SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 1);
	SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf2, 1);

	result =  1;
error_out:
	if(buf) free(buf);		
	if(buf2) free(buf2);
	return result;		
}


int CheckDiscValidity( struct nd_diskmgr_struct * diskmgr, 
			unsigned int select_disc, 
			unsigned int hostid)
{
	unsigned int currentDisc;
	PON_DISC_ENTRY  entry;
	struct media_disc_info * Mdiscinfo = NULL;	
	unsigned char *buf = NULL;
	int result = 0;

	PDISC_LIST	disc_list = NULL;
	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;
	disc_list = Mdiscinfo->discinfo;
	
	currentDisc = select_disc;	

	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("CheckDiscValidity can't alloc ubuf!\n");
		result = 0;
		goto error_out;
	}
	memset(buf, 0, 4096);

	if(!GetDiscMeta(diskmgr, &disc_list[currentDisc], buf, DISC_META_SECTOR_COUNT))
	{
		printf("DeleteCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf;

	if((entry->status != DISC_STATUS_VALID) && (entry->status != DISC_STATUS_VALID_END))
	{
		result = 0;
		goto error_out;
	}
	
	if(entry->status == DISC_STATUS_VALID)
	{
		int ret = 0;
#ifdef GTIME
		ULARGE_INTEGER		time_result;
		DWORD				sec;
		DWORD				milisecond;
		DWORD				savedsec;
		//	change function for windows
		GetSystemTimeAsFileTime((PFILETIME)&time_result);
		sec = (DWORD)(time_result.QuadPart / 10000000);
		milisecond = (DWORD)((time_result.QuadPart - (sec*10000000)) / 10);	

		unsigned int savdedsec;

		
		savdedsec = (unsigned int)(entry->time >> 32);
		DEBUGJBOP(2,("VALIDITY CHECK Set time (%d) : Current time (%ld)\n", savdedsec, sec));
		if( (sec - savdedsec) > 604800 )  // 7 days
		{
			printf("NEED VALIDATION PROCESS (%d)!!!!\n",entry->status);
			result =  -1;
			goto error_out;
		}
#else
			
		ret = _CheckValidateCount(diskmgr, currentDisc);
		if(ret == 0){
			result = 0;
			goto error_out;
		}else if(ret == -1){
			
			printf("NEED VALIDATION PROCESS (%d)!!!!\n",entry->status);
			result =  -1;
			goto error_out;
		}
#endif	
	}
			
	result= 1;
error_out:
	if(buf) free(buf);
	return result;
}


int DeleteCurrentDisc( 
					struct nd_diskmgr_struct * diskmgr, 
					unsigned int selected_disc, 
					unsigned int hostid
					)
{
	unsigned int currentDisc;
	int result;
	
	PON_DISK_META	hdisk;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list = NULL;
	
	struct uhead	*ubuf1 = NULL;
	struct uhead 	*ubuf2 = NULL;	
	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	struct media_disc_info * Mdiscinfo;

	
	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;
	currentDisc = selected_disc;
	disc_list = Mdiscinfo->discinfo;

	DEBUGJBOP(2,("enter DeleteCurrentDisc\n"));
	


	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		printf("DeleteCurrentDisc : invalid currentDisc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		printf("DeleteCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		printf("DeleteCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}

	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("DeleteCurrentDisc can't alloc buf!\n");
		return 0;
	}
	memset(buf,0, 4096);


	buf2 = (unsigned char *)malloc(4096);
	if(!buf2)
	{
		printf("DeleteCurrentDisc can't alloc buf2!\n");
		result = 0;
		goto error_out;
	}
	memset(buf2, 0, 4096);
	
	// update Disk meta
	if((result = _CheckUpdateDisk(diskmgr, 1, hostid ))== 0 )
	{
		printf("DeleteCurrentDisc Error _CheckUpdateDisk\n");
		result = 0;
		goto error_out;		
	}
	
	if(( result = _CheckUpdateSelectedDisk( diskmgr, 1, hostid,currentDisc)) == 0)
	{
		printf("DeleteCurrentDisc Error _CheckUpdateSelectedDisk\n");
		result = 0;
		goto error_out;
	}

	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("DeleteCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}
		


	hdisk = (PON_DISK_META)(buf);
	

	if(!GetDiscMeta(diskmgr, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		printf("DeleteCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf2;

	

	if(entry->status ==  DISC_STATUS_WRITING)
	{
		int ret = 0;
		ret = _CheckWriteFail(diskmgr, hdisk, entry, currentDisc, hostid);
		if(( ret == 0) || (ret == 1)){
			printf("DeleteCurrentDisc Error _CheckWriteFail\n");
			result = 0;
			goto error_out;
		}
		
		if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
		{
			printf("DeleteCurrentDisc Error GetDiskMeta\n");
			result = 0;
			goto error_out ;
		}
	
		hdisk = (PON_DISK_META)(buf);
	

		if(!GetDiscMeta(diskmgr, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
		{
			printf("DeleteCurrentDisc Error _GetDiscMeta\n");
			result = 0;
			goto error_out;		
		}
	
		entry = (PON_DISC_ENTRY) buf2;

		SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 1);
		SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf2, 1);

		result = 1;
		goto error_out;
	}


	if((entry->status == DISC_STATUS_VALID)
		|| (entry->status == DISC_STATUS_VALID_END))
	{
		int refcount = 0;
		refcount =  _GetDiscRefCount(diskmgr, currentDisc);
		if(refcount == -1){
			
			printf("DeleteCurrentDisc Error _GetDiscRefCount\n");
			result = 0;
			goto error_out;				
		}		
		if(refcount  <= 1){

			if(!_CheckHostOwner( diskmgr, currentDisc, hostid))
			{
				printf("DeleteCurrentDisc Is not ownde\n");
				result = 0;
				goto error_out;			
			}

			if( !_InvalidateDisc(diskmgr, buf, buf2, hostid) )
			{
				printf("DeleteCurrentDisc Error InvalidateDisc\n");
				result = 0;
				goto error_out;				
			}
			
		}else {
	
			if(!_DeleteAndLog( diskmgr, hdisk, entry, hostid, currentDisc))
			{
				 
				printf("DeleteCurrentDisc Error DeleteAndLog\n");
				result = 0;
				goto error_out;				
			}
		}
		SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 1);
		SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf2, 1);
	}

	result= 1;
error_out:
	if(buf) free(buf);
	if(buf2) free(buf2);
	return result;

}



int GetCurrentDiscInfo( 
			struct nd_diskmgr_struct * diskmgr, 
			unsigned int selected_disc,
			unsigned int command, 
			unsigned char *buf
			)
{
	unsigned char		*disk_buf = NULL;
	unsigned char 		*disc_buf = NULL;
	PON_DISC_ENTRY		DiscEntry;
	PGET_DISC_INFO		DiscInfo;
	PDISC_LIST		disclist;
	PU_DISC_DATA			data;
	unsigned char 			* source;
	struct media_disc_info * Mdiscinfo;
	int result = 0;

	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;
	disclist = &Mdiscinfo->discinfo[selected_disc];
	
	DEBUGJBOP(2,("enter GetCurrentDiscInfo\n"));


	if(selected_disc >= (unsigned int)Mdiscinfo->count)
	{
		printf("GetCurrentDiscInfo : invalid selected_disc(%d) Mdiscinfo->count(%d)!!\n",
				selected_disc, Mdiscinfo->count);
		return 0;
	}
	
	if(selected_disc < 0){
		
		printf("GetCurrentDiscInfo : invalid disc (%d)\n", selected_disc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		printf("GetCurrentDiscInfo : Error NEED FORMAT!!\n");
		return 0;
	}


	disk_buf = (unsigned char *)malloc(4096);
	if(!disk_buf)
	{
		printf("GetCurrentDiscInfo : can't alloc disk_buf!\n");
		return 0;
	}
	memset(disk_buf, 0, 4096);

	disc_buf = (unsigned char *)malloc(4096);
	if(!disc_buf)
	{
		printf("GetCurrentCurrentDisc : can't alloc disc_buf!\n");
		result = 0;
		goto error_out;
	}
	memset(disc_buf,0, 4096);


	DEBUGJBOP(2,("GetCurrentDiscInfo %p selected_disc %d\n", diskmgr, selected_disc));
	


	if(!GetDiskMeta(diskmgr, disk_buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("GetCurrentDiscInfo  Error can't read DISK META!!\n");
		result= 0;
		goto error_out;
	}

	SetDiskMetaInformation( diskmgr, (PON_DISK_META)disk_buf, 1);


	if(!GetDiscMeta(diskmgr, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		printf("GetCurrentDiscInfo  Error can't read DISC META!!\n");
		result= 0;
		goto error_out;
	}
	

		

	DiscEntry = (PON_DISC_ENTRY) disc_buf;
	DiscInfo = (PGET_DISC_INFO) buf;

 

	DiscInfo->selected_disk = 0;
	DiscInfo->selected_disc = DiscEntry->index;
	DiscInfo->loc = DiscEntry->loc;
	DEBUGJBOP(2,("Disc Info loc %d\n", DiscEntry->loc));
	DiscInfo->nr_DiscCluster = DiscEntry->nr_DiscCluster;
	DiscInfo->nr_DiscSector = DiscEntry->nr_DiscSector;
	DiscInfo->status = DiscEntry->status;
	DiscInfo->result = 1;

	if(command == COM_GET_DISC_HEADER) {
		result = 1;
		goto error_out;
	}

	if(!(DiscEntry->status & (DISC_STATUS_VALID | DISC_STATUS_VALID_END)))
	{
		printf("GetCurrentDiscInfo Error : Invalid status (entry->status (%d)\n", DiscEntry->status);
		result = 1;
		goto error_out;
	}
/*
	unsigned char		title_name[128];
	unsigned char		additional_infomation[128];
	unsigned char		key[128];
	unsigned char		title_info[128];

	#define DISC_TITLE_START			3
	#define DISC_ADD_INFO_START			4
	#define DISC_KEY_START				5	
*/
	source = disc_buf + DISC_TITLE_START * SECTOR_SIZE;
	data =(PU_DISC_DATA) &(DiscInfo->data[0]);
	memcpy(data->title_name, source, 128);
	memcpy(data->title_info, source + 128, 128);
	source = disc_buf + DISC_ADD_INFO_START * SECTOR_SIZE;
	memcpy(data->additional_infomation, source,128);

	source = disc_buf + DISC_KEY_START* SECTOR_SIZE;
	memcpy(data->key, source, 128);

	result= 1;

error_out:
	if(disk_buf) free(disk_buf);	
	if(disc_buf) free(disc_buf);
	return result;	
		
}




/****************************************************************
*
*		disk structure
*		# Cluster size Parameter
*		| 8M (G meta) | 16k (Media meta)| ... | 16k| Media cluseter 
*					.... Media cluster | Ximeta Information block (2k)|
*
*		G meta : 		0				??
*		Media meta : 	8M 				(8M )		[4M(disk Info)][4M(disc info mirror)]
*		Data		    :	16M		 		(total - 18M)   
*		XimetaInfo   :  total - 2M		2M
****************************************************************/


int DISK_FORMAT( struct nd_diskmgr_struct * diskmgr)
{
	unsigned char 		*buf;
	PON_DISK_META		hdisk;
	unsigned _int64		total_available_sector;
	unsigned int		total_available_cluster;
	unsigned int		i;
	unsigned int		max_discs;
	unsigned _int64		sector_count;
	PNDAS_DIB_V2		DIBV2;	
	int result = 0;

	DEBUGJBOP(2,("enter DISK_FORMAT\n"));

	sector_count =  diskmgr->Path.PerTarget[0].SectorCount;


	if( sector_count  < (unsigned _int64)( XIMETA_MEDIA_INFO_SECTOR_COUNT + MEDIA_META_INFO_SECTOR_COUNT ) )
	{
		printf("DISK FORMAT Error Sector size is too small \n");
		return 0;
	}


	buf = (unsigned char *)malloc(4096);
	if(!buf){
		printf("DISK FORMAT Error can't alloc buf\n");
		return 0;
	}	
	memset(buf,0, 4096);


	
	max_discs = MEDIA_MAX_DISC_COUNT;
	total_available_sector = (sector_count  - XIMETA_MEDIA_INFO_SECTOR_COUNT - MEDIA_META_INFO_SECTOR_COUNT) ;
	total_available_cluster = (unsigned int)((total_available_sector ) /MEDIA_CLUSTER_SIZE_SECTOR_COUNT);

	for(i = 0; i< max_discs; i++)
	{
		DISC_LIST list; 
		memset(buf,0,DISC_META_SIZE);
		list.pt_loc = i*MEDIA_DISC_INFO_SIZE_SECTOR  + MEDIA_DISC_INFO_START_ADDR_SECTOR;
		InitDiscEntry(diskmgr,total_available_cluster, i, list.pt_loc, (PON_DISC_ENTRY)buf);
		if(!SetDiscMeta(diskmgr, &list, buf, DISC_META_SECTOR_COUNT))
		{
			printf("DISK FORMAT Error SetDiscMeta\n");
			result = 0;
			goto error_out;		
		}

		if(!SetDiscMetaMirror(diskmgr, &list, buf, DISC_META_SECTOR_COUNT))
		{
			printf("DISK FORMAT Error _SetDiscMetaMirror\n");
			result = 0;
			goto error_out;				
		}

		SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)buf, 0);
		
	}


	// logdata setting

	for(i = 0; i < max_discs; i++)
	{
		unsigned int index = i;
		memset(buf, 0, SECTOR_SIZE*DISK_LOG_DATA_SECTOR_COUNT);
		if(!SetDiskLogData(diskmgr, buf, index, DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
			printf("DISK FORMAT Error !_SetDisckLogData\n");
			result = 0;
			goto error_out;						
		}
	}

	// loghead
	memset(buf,0, SECTOR_SIZE);
	if(!SetDiskLogHead(diskmgr, buf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		printf("DISK FORMAT Error SetDiscLogHead\n");
		result = 0;
		goto error_out;					
	}
	
	memset(buf,0,SECTOR_SIZE*DISK_META_READ_SECTOR_COUNT);
	hdisk = (PON_DISK_META)buf;
	hdisk->MAGIC_NUMBER = MEDIA_DISK_MAGIC;
	hdisk->VERSION = MEDIA_DISK_VERSION;
	hdisk->age = 0;
	hdisk->nr_Enable_disc =0 ;
	hdisk->nr_DiscInfo = max_discs;
	hdisk->nr_DiscInfo_byte = (unsigned int)((hdisk->nr_DiscInfo + 7) / 8);
	hdisk->nr_DiscCluster = total_available_cluster ;
	hdisk->nr_DiscCluster_byte =(unsigned int)( (hdisk->nr_DiscCluster + 7)/ 8) ;
	hdisk->nr_AvailableCluster = hdisk->nr_DiscCluster;


	if(!SetDiskMeta(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("DISK FORMAT Error SetDiskMeta\n");
		result = 0;
		goto error_out;		
	}

	if(!SetDiskMetaMirror(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("DISK FORMAT Error 8\n");
		goto error_out;		
	}

	SetDiskMetaInformation( diskmgr, (PON_DISK_META)buf, 0);


	//add disk signal
	memset(buf,0, SECTOR_SIZE);
	DIBV2 = (PNDAS_DIB_V2)(buf);
    DIBV2->Signature = NDAS_DIB_V2_SIGNATURE;
    DIBV2->MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
    DIBV2->MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
    DIBV2->iMediaType = NMT_MEDIAJUKE;
	DIBV2->nDiskCount = 1;
	DIBV2->crc32 = crc32_calc((unsigned char *)DIBV2, sizeof(DIBV2->byte_248));
	DIBV2->crc32_unitdisks = crc32_calc((unsigned char *)DIBV2->data, sizeof(DIBV2->data));
	if(!RawDiskOp(diskmgr,WIN_WRITE,sector_count -2,1,0,buf))
	{
		printf("Error Writing signature \n");
		result = 0;
		goto error_out;		
	}

	// update vmap table information
	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT)
	{
		diskmgr->flags &= ~DMGR_FLAG_NEED_FORMAT;
		diskmgr->flags |= DMGR_FLAG_FORMATED;															
	}
	
	if(diskmgr->isJukeBox == FALSE)
	{
		diskmgr->isJukeBox = TRUE;
	}


	result = 1;
error_out:
	if(buf) free(buf);
	return result;
}




int ValidateDisc(
				 struct nd_diskmgr_struct * diskmgr, 
				 unsigned int selected_disc, 
				 unsigned int hostid
				 )
{
	PON_DISC_ENTRY		entry;
	unsigned char *		disk_buf = NULL;
	unsigned char *		disc_buf = NULL;
	PDISC_LIST		disclist = NULL;
	struct media_disc_info * Mdiscinfo;
	int result = 0;

#ifdef GTIME
	ULARGE_INTEGER		time_result;
	DWORD				sec;
	DWORD				milisecond;
	DWORD				savedsec;
		//	change function for windows
	GetSystemTimeAsFileTime((PFILETIME)&time_result);
	sec = (DWORD)(time_result.QuadPart / 10000000);
	milisecond = (DWORD)((time_result.QuadPart - (sec*10000000)) / 10);	
#endif

	

	Mdiscinfo = (struct media_disc_info *) diskmgr->mediainfo;

	if(selected_disc > (unsigned int)Mdiscinfo->count) 
	{
		printf("ValidateDisc Error : selected_disc (%d) is too big\n", selected_disc);
		return 0;
	}
	
	disk_buf = (unsigned char *)malloc(4096);
	if(!disk_buf){
		printf("ValidateDisc  Error can't alloc disk_buf!!\n");
		return 0;
	}
	memset(disk_buf, 0, 4096);


	disc_buf = (unsigned char *)malloc(4096);
	if(!disc_buf){
		printf("ValidateDisc  Error can't alloc disc_buf!!\n");
		result = 0;
		goto error_out;
	}
	memset(disc_buf, 0, 4096);

	if(!GetDiskMeta(diskmgr, disk_buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("ValidateDisc  Error can't read DISK META!!\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation( diskmgr, (PON_DISK_META)disk_buf, 1);

	disclist = &Mdiscinfo->discinfo[selected_disc];	
	
	if(!GetDiscMeta(diskmgr, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		printf("ValidateDisc  Error can't read DISC META!!\n");
		result = 0;
		goto error_out;
	}

	entry = (PON_DISC_ENTRY) disc_buf;

	if(entry->status != DISC_STATUS_VALID)
	{
		printf("ValidateDisc Error Not VALID DISC (%d)\n",entry->status);
		result = 0;
		goto error_out;
	}

	if(!_CheckHostOwner( diskmgr, selected_disc, hostid))
	{
		printf("ValidateDisc Is not owned 8\n");
		result = 0;
		goto error_out;			
	}


#ifdef GTIME
	savedsec = (unsigned int)(entry->time >> 32);
	DEBUGJBOP(2,("VALIDITY CHECK Set time (%d) : Current time (%ld)\n", savedsec, sec));
	if( (sec - savedsec) < 604800 )  // 7 days
	{
		printf("TOO EARLY TRY VALIDATION  PROCESS !!!!\n");
		result = 0;
		goto error_out;
	}	
#else
	if(_CheckValidateCount(diskmgr, selected_disc) != -1)
	{
		printf("TOO EARLY TRY VALIDATION PROCESS !!!!\n");
		result = 0;
		goto error_out;
	}	
#endif	
			
	
	entry->status = DISC_STATUS_VALID_END;
	
	if( ! SetDiscMeta(diskmgr, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		printf("DeleteCurrentDisc Error 11\n");
		result = 0;
		goto error_out;
	}

	SetDiscEntryInformation(diskmgr, (PON_DISC_ENTRY)disc_buf, 1);
	result = 1;
error_out:
	if(disk_buf) free(disk_buf);
	if(disc_buf) free(disc_buf);
	return result;
}




void CheckFormat(struct nd_diskmgr_struct * diskmgr)
{
	PON_DISK_META hdisk = NULL;
	unsigned char *	buf = NULL;
	
	buf = (unsigned char *)malloc(4096);
	if(!buf)
	{
		printf("CheckFormat Error : can't alloc buf\n");
		return;	
	}

	memset(buf,0,4096);

	DEBUGJBOP(2,("enter CheckFormat---\n"));


		
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("CheckFormat  Error can't read DISK META!!\n");
		goto error_out;
	}

	hdisk = (PON_DISK_META)buf;
	if((hdisk->MAGIC_NUMBER != MEDIA_DISK_MAGIC)
			|| (hdisk->VERSION != MEDIA_DISK_VERSION))
	{
		DEBUGJBOP(2,("DMGR_FLAG_NEED_FORMAT\n"));
		diskmgr->flags &= ~DMGR_FLAG_FORMATED;
		diskmgr->flags |= DMGR_FLAG_NEED_FORMAT;
	}else{
		DEBUGJBOP(2,("DMGR_FLAG_FORMATED\n"));
		diskmgr->flags &= ~DMGR_FLAG_NEED_FORMAT;
		diskmgr->flags |= DMGR_FLAG_FORMATED;
	}
		
error_out:
	free(buf);
	DEBUGJBOP(2,("---end CheckFormat\n"));
	return;
}