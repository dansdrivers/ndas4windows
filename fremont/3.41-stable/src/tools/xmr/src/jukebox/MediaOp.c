#include <stdio.h>
#include <stdlib.h>


#include "../inc/MediaDisk.h"
#include "../inc/diskmgr.h"
#include "MetaOp.h"
#include "MediaInfo.h"
#include "MediaOp.h"



int debuglevelMediaOp = 1;

#define DEBUGLOG( _l_, _x_ )			\
		do{								\
			if(_l_ < debuglevelMediaOp)	\
				printf _x_;				\
		}	while(0)					\



/********************************************************************

	Media log manipulation function

*********************************************************************/
int getHashVal(int hostid)
{
	return hostid % HOST_HASH_VALUE;
}

int getLogIndex(PON_DISK_LOG logdata,int hostid)
{
	int i = 0;
	int j = 0;
	PDISC_HOST_STATUS pStatus;
	pStatus = logdata->Host;
	i = getHashVal(hostid);

	do{
		if(pStatus[i].IsSet == 1)
		{
			if(pStatus[i].hostid == (unsigned int)hostid) return i;		
		}
		
		i = (i + 1) % HOST_HASH_VALUE;
		j++;
	}while(j < HOST_HASH_VALUE);
	return -1;
}

int getTotalRefCount(PON_DISK_LOG logdata)
{
	int i = 0;
	int refcount = 0;
	PDISC_HOST_STATUS pStatus;
	pStatus = logdata->Host;
	for(i = 0; i< HOST_HASH_VALUE; i++)
	{
		if(pStatus[i].IsSet == 1)
		{
			if(pStatus[i].refcount == 1)	refcount++;
		}
	}		
	return refcount;

} 

int getLogRefCount(PON_DISK_LOG logdata, int index)
{
	return (int)logdata->Host[index].refcount;
}


void setLogRefCount(PON_DISK_LOG logdata, int index, unsigned int value)
{
	logdata->Host[index].refcount = value;
	return;
}


int setLogIndex(PON_DISK_LOG logdata, int hostid)
{
	int i; 
	i = getLogIndex(logdata, hostid);
	if(i != -1){ 
		return i;
	} else {
		int j, k;
		PDISC_HOST_STATUS pStatus;

		pStatus = logdata->Host;
		j = getHashVal(hostid);
		k = 0;

		do {
			if(pStatus[j].IsSet == 0) 
			{
				pStatus[j].IsSet = 1;
				pStatus[j].hostid = hostid;
				return j;
			}
			j = (j + 1) % HOST_HASH_VALUE;
			k++;
		} while(k < HOST_HASH_VALUE);

		return -1;
	}	
}



void dumpHostRef(PON_DISK_LOG logdata)
{
	int i = 0; 
	PDISC_HOST_STATUS pStatus;

	pStatus = logdata->Host;
	for(i = 0; i< HOST_HASH_VALUE; i++)
	{
		if(pStatus[i].IsSet == 1)
		{
			DEBUGLOG(2,("Index[%d]: hostid(%d),refcount(%d)\n",i,pStatus[i].hostid, pStatus[i].refcount));
		}
	}
}


void dumpDiscHistory(PON_DISK_LOG logdata)
{
	int i = 0;
	int j = 0;
	PDISC_HISTORY pHistory;
	
	pHistory = logdata->History;
	i = (logdata->latest_disc_history + 1)% MAX_DISC_LOG_HISTORY;

	do{
		if(pHistory[i].action != 0)
		{
			switch(pHistory[i].action)
			{
			case LOG_ACT_WRITE_META_ALLOC_S:
				DEBUGLOG(2,("LOG_ACT_WRITE_META_ALLOC_S\n"));
			break;
			case LOG_ACT_WRITE_META_ALLOC_E:
				DEBUGLOG(2,("LOG_ACT_WRITE_META_ALLOC_E\n"));
			break;
			case LOG_ACT_WRITE_META_END_S:
				DEBUGLOG(2,("LOG_ACT_WRITE_META_END_S\n"));
			break;
			case LOG_ACT_WRITE_META_END_E:
				DEBUGLOG(2,("LOG_ACT_WRITE_META_END_E\n"));
			break;
			case LOG_ACT_DELET_META_S:
				DEBUGLOG(2,("LOG_ACT_DELET_META_S\n"));
			break;
			case LOG_ACT_DELET_META_E:
				DEBUGLOG(2,("LOG_ACT_DELET_META_S\n"));
			break;
			case LOG_ACT_RECOVERY_S:
				DEBUGLOG(2,("LOG_ACT_RECOVERY_S\n"));
			break;
			case LOG_ACT_RECOVERY_E:
				DEBUGLOG(2,("LOG_ACT_RECOVERY_E\n"));
			break;
			default:
				DEBUGLOG(2,("UNKNOWN LOG\n"));
			break;
			}			
		}
		i = (i+ 1) % MAX_DISC_LOG_HISTORY;
		j++;
	}while(j < MAX_DISC_LOG_HISTORY);
}

void dumpDisKHistory(PON_DISK_LOG_HEADER loghead)
{
	int i = 0;
	int j = 0;
	PDISK_HISTORY pHistory;
	
	pHistory = loghead->history;
	i = (loghead->latest_log_history + 1) % MAX_LOG_HISTORY;
	do{
		switch(pHistory[i].actionstatus)
		{

		case ACTION_DEFAULT:
			DEBUGLOG(2,("ACTION_DEFAULT\n"));
		break;
		case ACTION_WS1_DISKFREE_DISCINFO_START:
			DEBUGLOG(2,("ACTION_WS1_DISKFREE_DISCINFO_START\n"));
		break;
		case ACTION_WS1_DISKFREE_DISCINFO_END:
			DEBUGLOG(2,("ACTION_WS1_DISKFREE_DISCINFO_END\n"));
		break;
		case ACTION_WS2_DISKDIRTY_START :
			DEBUGLOG(2,("ACTION_WS2_DISKDIRTY_START\n"));
		break;
		case ACTION_DS1_DISCENTRY_START:
			DEBUGLOG(2,("ACTION_DS1_DISCENTRY_START\n"));
		break;
		case ACTION_DS2_REAL_ERASE_START:
			DEBUGLOG(2,("ACTION_DS2_REAL_ERASE_START\n"));
		break;
		case ACTION_UP_DISK_META:
			DEBUGLOG(2,("ACTION_UP_DISK_META\n"));
		break;
		case ACTION_UP_RECOVERY:
			DEBUGLOG(2,("ACTION_UP_RECOVERY\n"));
		break;
		default:
			DEBUGLOG(2,("UNKNOWN LOG\n"));
		break;
		}		
		i = (i + 1) % MAX_LOG_HISTORY;
		j++;
	}while(j < MAX_LOG_HISTORY);
}


int _GetDiscRefCount(struct nd_diskmgr_struct * diskmgr, unsigned int selectedDisc)
{

	unsigned char *logbuf = NULL;
	PON_DISK_LOG 			hlogdata;			
	int	result = -1;				
		
	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf){

		printf("_GetDiscRefCount can't alloc logbuf\n");
		return -1;
	}

	memset(logbuf, 0, 1024);
	
	if(! GetDiskLogData(diskmgr, logbuf, selectedDisc,DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		
		printf("_GetDiscRefCount GetDiskLogData\n");
		result = -1;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG)logbuf ;
	result = hlogdata->refcount;

error_out:
	free(logbuf);
	return result;	
}



// _AddRefCount and _RemoveRefCount must be done after Error checking routine
//	_CheckCurrentDisk
//	_CheckCurrentDisc
//	_DoDelayedOperation

int _AddRefCount(
		struct nd_diskmgr_struct * diskmgr,
		unsigned int selectedDisc,
		unsigned int hostid)
{
	unsigned char *buf2 = NULL;
	unsigned char *logbuf = NULL;
	PON_DISC_ENTRY	 		hentry;
	DISC_LIST			disclist;
	PON_DISK_LOG 			hlogdata;								
	unsigned int		start_sector_disc = 0;
	int			result = -1;		
	
	buf2 = (unsigned char *)malloc(4096);
	if(!buf2){
		printf("_AddRefCount can't alloc !4096\n");
		return 0;
	}	
	
	memset(buf2, 0, 4096);

	logbuf = (unsigned char *)(1024);
	if(!logbuf){
		printf("_AddRefCount can't alloc !1024\n");
		result = 0;
		goto error_out;
		
	}	
		
	memset(logbuf, 0, 1024);

	
	start_sector_disc = MEDIA_DISC_INFO_START_ADDR_SECTOR + selectedDisc * MEDIA_DISC_INFO_SIZE_SECTOR;
	disclist.pt_loc = start_sector_disc;

	if(! GetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("_AddRefCount GetDiscMeta\n");
		result= 0;
		goto error_out;
	}

		
	hentry = (PON_DISC_ENTRY) buf2;

	if((hentry->status == DISC_STATUS_VALID) || (hentry->status == DISC_STATUS_VALID_END))
	{
		int hostindex = 0;
		int refcount = 0;
		if(!GetDiskLogData(diskmgr, logbuf, selectedDisc,DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
		
			printf("_AddRefCount GetDiskLogData\n");
			result = 0;
			goto error_out;
		}

		hlogdata = (PON_DISK_LOG)logbuf ;

		hostindex= setLogIndex(hlogdata, hostid);

		if(hostindex == -1)
		{
			printf("_AddRefCount Error can't get hostindex\n");
			result = 0;
			goto error_out;

		}else{
			refcount = getLogRefCount(hlogdata, hostindex);	
			if(refcount != 0) {
				printf("_AddRefCount Error host index is alread set to 1\n");
			}
			setLogRefCount(hlogdata, hostindex,1);
			hlogdata->refcount = getTotalRefCount(hlogdata);
			hlogdata->validcount--;
		}

		if(! SetDiskLogData(diskmgr, logbuf, selectedDisc,DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
		
			printf("_AddRefCount error GetDiskLogData\n");
			result = 0; 
			goto error_out;
		}
	}else{
		DEBUGLOG(2,("Is not VALID STATUS\n"));
		result = 1;
		goto error_out;
	}
	
	result = 1;
error_out:
	if(logbuf)
		free(logbuf);
	if(buf2)
		free(buf2);
	return result;
}


int _RemoveRefCount(
		struct nd_diskmgr_struct * diskmgr,
		unsigned int selectedDisc,
		unsigned int hostid)
{
	unsigned char *buf2 = NULL;
	unsigned char *logbuf = NULL;

	PON_DISC_ENTRY	 		hentry;
	DISC_LIST			disclist;
	PON_DISK_LOG 			hlogdata;								
	unsigned int		start_sector_disc = 0;
	int			result = -1;		
	
	buf2 = (unsigned char *)malloc(4096);
	if(!buf2){
		printf("_RemoveRefCount can't alloc !4096\n");
		return 0;
	}	
	
	memset(buf2, 0, 4096);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf){
		printf("_RemoveRefCount can't alloc !1024\n");
		result = 0;
		goto error_out;
		
	}	
		
	memset(logbuf,0,1024);
	
	start_sector_disc = MEDIA_DISC_INFO_START_ADDR_SECTOR + selectedDisc * MEDIA_DISC_INFO_SIZE_SECTOR;
	disclist.pt_loc = start_sector_disc;

	if(! GetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("_RemoveRefCount GetDiscMeta\n");
		result = 0;
		goto error_out;
	}

		
	hentry = (PON_DISC_ENTRY) buf2;

	if((hentry->status == DISC_STATUS_VALID) || (hentry->status == DISC_STATUS_VALID_END))
	{
		int hostindex = 0;
		int refcount = 0;
		if(! GetDiskLogData(diskmgr, logbuf, selectedDisc,DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
		
			printf("_RemoveRefCount GetDiskLogData\n");
			result = 0;
			goto error_out;
		}

		hlogdata = (PON_DISK_LOG)logbuf ;

		hostindex= setLogIndex(hlogdata, hostid);

		if(hostindex == -1)
		{
			printf("_RemoveRefCount Error can't get hostindex\n");
			result =0;
			goto error_out;

		}else{
			refcount = getLogRefCount(hlogdata, hostindex);	
			if(refcount != 1) {
				printf("_RemoveRefCount Error host index is alread set to 1\n");
			}
			setLogRefCount(hlogdata, hostindex,0);
			if(hlogdata->refcount == 0) {
				printf("_RemoveRefCount Error host logdata->refcount is alread set to 0\n");
			}
			hlogdata->refcount = getTotalRefCount(hlogdata);
		}

		if(! SetDiskLogData(diskmgr, logbuf, selectedDisc,DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
		
			printf("_RemoveRefCount error  GetDiskLogData\n");
			result = 0;
			goto error_out;
		}
	}else{
		DEBUGLOG(2,("Is not VALID STATUS\n"));
		result = 1;
		goto error_out;
	}
	
	result = 1;
error_out:
	if(logbuf)
		free(logbuf);
	if(buf2)
		free(buf2);
	return result;

}		


int _WriteAndLogStart(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc, 
		unsigned int sector_count,
		unsigned int encrypt)
{
	unsigned char		*logheadbuf = NULL;
	unsigned char		*logbuf = NULL;
	PON_DISK_LOG_HEADER	hloghead;
	PON_DISK_LOG		hlogdata;
	unsigned int		logindex;
	PBITEMAP			freebitmap;
	PBITEMAP			discInfobitmap;

	ULARGE_INTEGER		time_result;
	DWORD				sec;
	DWORD				milisecond;

	unsigned int cluster_count  = 0;
	unsigned int free_cluster_count = 0;
	int 	result = 0;
	PDISC_LIST	disc_list;
	struct media_disc_info * Mdiscinfo = NULL;
	
	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	disc_list = Mdiscinfo->discinfo;


	logheadbuf = (unsigned char *)malloc(1024);
	if(!logheadbuf)
	{
		printf("WriteAndLogStart can't alloc logheadbuf\n");
		return 0;
	}
	memset(logheadbuf, 0, 1024);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf)
	{
		printf("WriteAndLogStart can't alloc logbuf\n");
		result = 0;
		goto error_out;
	}

	memset(logbuf, 0, 1024);

	// get loghead
	if(!GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		printf("WriteAndLogStart Error 1\n");
		result= 0;
		goto error_out;
	}
		
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;


	// get logdata
	logindex = currentDisc;

	if(! GetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 2\n");
		result = 0;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG) logbuf  ;


	// calcurate and assign clusters
	cluster_count = (unsigned int)(( sector_count  + MEDIA_CLUSTER_SIZE_SECTOR_COUNT -1) /MEDIA_CLUSTER_SIZE_SECTOR_COUNT);
	if(cluster_count > DEFAULT_DISC_CLUSTER_COUNT)
	{
		printf(" File size is too big \n");
		printf("sizeof sector count %d\n", sector_count);
		result = 0;
		goto error_out;	
	}

	freebitmap = hdisk->FreeCluster_bitmap;
	free_cluster_count  = findFreeMapCount( hdisk->nr_DiscCluster, freebitmap); 
	DEBUGLOG(2,("Write START free_cluster_count (%d)\n", free_cluster_count));
	if(free_cluster_count < cluster_count)
	{
		printf("Write START disk error : cluster size: (%d) alloc size(%d) too big\n", cluster_count, free_cluster_count);
		result = 0;
		goto error_out;	
	}


	// set disc meta information
	//	change function for windows
	GetSystemTimeAsFileTime((PFILETIME)&time_result);
	sec = (DWORD)(time_result.QuadPart / 10000000);
	milisecond = (DWORD)((time_result.QuadPart - (sec*10000000)) / 10);

	entry->MAGIC_NUMBER = MEDIA_DISK_MAGIC;
	entry->nr_DiscCluster = cluster_count;
	entry->nr_DiscSector = sector_count;
	entry->status = DISC_STATUS_WRITING;
	entry->time = ( ( (unsigned _int64)(sec) << 32 ) | (unsigned _int64)(milisecond) );
	if(encrypt == 1)entry->encrypt = 1;
	else entry->encrypt = 0;
	DEBUGLOG(2,("BurnS Set time (%d)  BurnS Current Time (%ld)\n", (unsigned int)(entry->time >> 32), sec));
	

	
	// set disk_information
		// set cluster bitmap information
	setDiscAddr_diskFreeMap(hdisk->nr_DiscCluster, cluster_count, freebitmap, entry);
		// set dic bitmap information
	discInfobitmap = hdisk->DiscInfo_bitmap;
	setDiscInfoMap(entry->index, discInfobitmap);
		// update cluster free information
	hdisk->nr_AvailableCluster = free_cluster_count - cluster_count;




	// set loghead to current logdata
	hloghead->latest_index = logindex;
	hloghead->latest_log_action = hlogdata->action;


	// write log
		// logdata
		// step 1 : set allocation relate information
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action |= ACTION_WS1_DISKFREE_DISCINFO_START;		
	setDiscAddrtoLogData(entry, hlogdata);
		// step 2 : set log specific information
	hlogdata->hostId = hostid;
	hlogdata->time = ( ( (unsigned _int64)(sec) << 32 ) | (unsigned _int64)(milisecond) );
	hlogdata->valid = DISK_LOG_INVALID;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_WRITE_META_ALLOC_S;
				
	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 3\n");
		result= 0;
		goto error_out;			
	}
	
	printf("hlogdata->nr_DiscCluster %d\n", hlogdata->nr_DiscCluster);

	// logheader
	hloghead->latest_log_action = (hlogdata->action | ACTION_UP_DISK_META);
	hloghead->latest_age = hloghead->latest_age + 1;

	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 4\n");
		result = 0;
		goto error_out;						
	}

	// write disk meta
		// disc meta
		// Step 3:
	entry->age = hlogdata->age + 1;

	if( ! SetDiscMeta(diskmgr, &disc_list[currentDisc], (unsigned char *)entry, DISC_META_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 5\n");
		result = 0;
		goto error_out;
	}
		// disk meta
		// Step 4:
	hdisk->age = hloghead->latest_age + 1;

	if( ! SetDiskMeta( diskmgr, (unsigned char *)hdisk, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 6\n");
		result = 0;
		goto error_out;
	}

			

	// write log 
		// log header
		// Step 5
	hloghead->latest_log_action &= ~(ACTION_WS1_DISKFREE_DISCINFO_START & ACTION_UP_DISK_META);
	hloghead->latest_log_action = ACTION_WS1_DISKFREE_DISCINFO_END;
	hloghead->latest_age = hloghead->latest_age + 1;
	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_WRITE_SECTOR_COUNT ) )
	{
		printf("WriteAndLogStart Error 7\n");
		result = 0;
		goto error_out;						
	}

		
		// log data
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action  &= ~ACTION_WS1_DISKFREE_DISCINFO_START;	
	hlogdata->action = ACTION_WS1_DISKFREE_DISCINFO_END;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->valid = DISK_LOG_VALID;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_WRITE_META_ALLOC_E;
	
	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogStart Error 8\n");
		result = 0;
		goto error_out;			
	}

	printf("hlogdata->nr_DiscCluster %d\n", hlogdata->nr_DiscCluster);
	result = 1;
error_out:
	if(logheadbuf) free(logheadbuf);
	if(logbuf) free(logbuf);
	return result;
}



int _WriteAndLogEnd(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc)


{
	unsigned char			*logheadbuf = NULL;
	unsigned char			*logbuf = NULL;
	PON_DISK_LOG_HEADER 		hloghead;
	PON_DISK_LOG			hlogdata;
	PBITEMAP				dirtybitmap;
	unsigned int			logindex;

	ULARGE_INTEGER		time_result;
	DWORD				sec;
	DWORD				milisecond;
	int hostindex = 0;
	int result = -1;

	PDISC_LIST	disc_list;
	struct media_disc_info * Mdiscinfo = NULL;
	
	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	disc_list = Mdiscinfo->discinfo;


	logheadbuf = (unsigned char *)malloc(1024);
	if(!logheadbuf)
	{
		printf("WriteAndLogEnd can't alloc logheadbuf\n");
		return 0;
	}

	
	memset(logheadbuf,0,1024);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf)
	{
		printf("WriteAndLogEnd can't alloc logbuf\n");
		result = 0;
		goto error_out;
	}
	
	memset(logbuf, 0, 1024);


	// get loghead
	if(!GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		printf("WriteAndLogEnd Error 1\n");
		result = 0;
		goto error_out;
	}
		
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;

	// get logdata
	logindex = currentDisc;

	if(! GetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 2\n");
		result = 0;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG)logbuf;
		
	// assign cluster and upstae to disk meta.
	dirtybitmap = hdisk->DirtyCluster_bitmap;
	setClusterMapFromEntry(dirtybitmap, entry);
	hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
	hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);
		
	//	change function for windows
	GetSystemTimeAsFileTime((PFILETIME)&time_result);
	sec = (DWORD)(time_result.QuadPart / 10000000);
	milisecond = (DWORD)((time_result.QuadPart - (sec*10000000)) / 10);
                // disc meta information
                        // disc meta
    entry->status = DISC_STATUS_VALID;
	entry->time = ( ( (unsigned _int64)(sec) << 32 ) | (unsigned _int64)(milisecond) );




	// set loghead to current logdata
	hloghead->latest_index = logindex;
	hloghead->latest_log_action = hlogdata->action;


	// write log
		// logdata	
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action |= ACTION_WS2_DISKDIRTY_START;
	hlogdata->hostId = hostid;
	hlogdata->time = ( ( (unsigned _int64)(sec) << 32 ) | (unsigned _int64)(milisecond) );
	hlogdata->valid = DISK_LOG_INVALID;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_WRITE_META_END_S;
		
	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 3\n");
		result = 0;
		goto error_out;			
	}

		// logheader
	hloghead->latest_log_action =  (hlogdata->action | ACTION_UP_DISK_META);
	hloghead->latest_age = hloghead->latest_age + 1;

	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 4\n");
		result = 0;
		goto error_out;						
	}

	// write disk meta
		// disc meta
	entry->age = hlogdata->age + 1;

	if( ! SetDiscMeta(diskmgr, &disc_list[currentDisc], (unsigned char *)entry, DISC_META_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 5\n");
		result = 0;
		goto error_out;
	}

	if( ! SetDiscMetaMirror(diskmgr, &disc_list[currentDisc], (unsigned char *)entry, DISC_META_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 6\n");
		result = 0;
		goto error_out;
	}

		// disk meta
	hdisk->age = hloghead->latest_age + 1;

	if( ! SetDiskMeta( diskmgr, (unsigned char *)hdisk, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 7\n");
		result = 0;
		goto error_out;
	}

	if( ! SetDiskMetaMirror( diskmgr, (unsigned char *)hdisk, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 8\n");
		result = 0;
		goto error_out;
	}			

	// write log 
		// log header
	hloghead->latest_log_action &= ~(ACTION_WS2_DISKDIRTY_START| ACTION_UP_DISK_META);
	hloghead->latest_log_action = ACTION_WS1_DISKFREE_DISCINFO_END;
	hloghead->latest_age = hloghead->latest_age + 1;
	hloghead->latest_log_history = (hloghead->latest_log_history + 1) % MAX_LOG_HISTORY;
	hloghead->history[hloghead->latest_log_history].diskId = logindex;
	hloghead->history[hloghead->latest_log_history].actionstatus =ACTION_STATUS_WRITE;
	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 9\n");
		result = 0;
		goto error_out;						
	}

		
	// log data
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action &= ~ACTION_WS2_DISKDIRTY_START;
	hlogdata->action = ACTION_WS1_DISKFREE_DISCINFO_END;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->valid = DISK_LOG_VALID;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_WRITE_META_END_E;
	hlogdata->refcount = 0;
	hlogdata->validcount = 20;
	memset(hlogdata->Host,0,SECTOR_SIZE);

	hostindex= setLogIndex(hlogdata, hostid);
	if(hostindex == -1)
	{
		printf("WriteAndLogEnd Error can't get hostindex\n");

	}else{
		
		setLogRefCount(hlogdata, hostindex,1);
		hlogdata->refcount = getTotalRefCount(hlogdata);
	}
	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("WriteAndLogEnd Error 10\n");
		result = 0;
		goto error_out;			
	}

	result = 1;
error_out:
	if(logheadbuf) free(logheadbuf);
	if(logbuf) free(logbuf);
	return result;
}




int _DeleteAndLog(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,
		unsigned int currentDisc)
{
	unsigned char	  *logheadbuf = NULL;
	unsigned char	  *logbuf = NULL;
	PON_DISK_LOG_HEADER 	hloghead;
	PON_DISK_LOG		hlogdata;
	PBITEMAP			dirtybitmap;
	PBITEMAP			freebitmap;
	unsigned int		logindex;	
	ULARGE_INTEGER		time_result;
	DWORD				sec;
	DWORD				milisecond;
	int 	result = 0;
	PDISC_LIST	disc_list;
	struct media_disc_info * Mdiscinfo = NULL;
	
	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;
	disc_list = Mdiscinfo->discinfo;


	logheadbuf = (unsigned char *)malloc(1024);
	if(!logheadbuf)
	{
		printf("WriteAndLogEnd can't alloc logheadbuf\n");
		return 0;
	}

	memset(logheadbuf, 0, 1024);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf)
	{
		printf("WriteAndLogEnd can't alloc logbuf\n");
		result = 0;
		goto error_out;
	}

	memset(logbuf, 0, 1024);

	DEBUGLOG(2,("enter DeleteAndLog\n"));
	// get loghead
	if(!GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		printf("DeleteAndLog Error 1\n");
		result = 0;
		goto error_out;
	}
	
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;


	// get logdata
	logindex = currentDisc;

	if(! GetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 2\n");
		result = 0;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG)logbuf;


	// set disk meta information	
	freebitmap = hdisk->FreeCluster_bitmap;
	dirtybitmap = hdisk->DirtyCluster_bitmap;
	freeAllClusterMapFromEntry( freebitmap, dirtybitmap, entry);
	freeDiscInfoMap(entry->index, hdisk->DiscInfo_bitmap);
	hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
	hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);

	// set loghead to current logdata
	hloghead->latest_index = logindex;
	hloghead->latest_log_action = hlogdata->action;


	// write log
		// logdata
	//	change function for windows
	GetSystemTimeAsFileTime((PFILETIME)&time_result);
	sec = (DWORD)(time_result.QuadPart / 10000000);
	milisecond = (DWORD)((time_result.QuadPart - (sec*10000000)) / 10);

	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action |= ACTION_DS2_REAL_ERASE_START	;
	setDiscAddrtoLogData(entry, hlogdata);
	hlogdata->hostId = hostid;
	hlogdata->time = (((unsigned _int64)(sec) << 32) | (unsigned _int64)(milisecond));
	hlogdata->valid = DISK_LOG_INVALID;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_DELET_META_S;

	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 3\n");		
		result = 0;
		goto error_out;			
	}

		// logheader
	hloghead->latest_log_action = (hlogdata->action | ACTION_UP_DISK_META);
	hloghead->latest_age = hloghead->latest_age + 1;

	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 4\n");	
		result = 0;
		goto error_out;						
	}

	
	// write disk meta
		// disc meta
	InitDiscEntry(diskmgr, hdisk->nr_DiscCluster, logindex, entry->loc, entry);
	entry->age = hlogdata->age + 1;

	if( ! SetDiscMeta(diskmgr, &disc_list[currentDisc], (unsigned char *)entry, DISC_META_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 5\n");		
		result = 0;
		goto error_out;
	}

	if( ! SetDiscMetaMirror(diskmgr, &disc_list[currentDisc], (unsigned char *)entry, DISC_META_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 6\n");			
		result = 0;
		goto error_out;
	}
		// disk meta
	hdisk->age = hloghead->latest_age + 1;
	if( ! SetDiskMeta( diskmgr, (unsigned char *)hdisk, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 7\n");		
		result = 0;
		goto error_out;
	}

	if( ! SetDiskMetaMirror( diskmgr, (unsigned char *)hdisk, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 8\n");		
		result = 0;
		goto error_out;
	}		

	// write log 
		// log header
	hloghead->latest_log_action &= ~(ACTION_DS2_REAL_ERASE_START & ACTION_UP_DISK_META);
	hloghead->latest_log_action = ACTION_DS2_REAL_ERASE_END;
	hloghead->latest_age = hloghead->latest_age + 1;
	hloghead->latest_log_history = (hloghead->latest_log_history + 1) % MAX_LOG_HISTORY;
	hloghead->history[hloghead->latest_log_history].diskId = logindex;
	hloghead->history[hloghead->latest_log_history].actionstatus = ACTION_STATUS_DELETE;
	
	if(! SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 9\n");			
		result = 0;
		goto error_out;						
	}

	
		// log data
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action  &= ~ACTION_DS2_REAL_ERASE_START;	
	hlogdata->action = ACTION_DS2_REAL_ERASE_END;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->valid = DISK_LOG_VALID;
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_WRITE_META_END_E;
	hlogdata->refcount = 0;
	memset(hlogdata->Host,0,SECTOR_SIZE);
	if(! SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("DeleteAndLog Error 10\n");		
		result = 0;
		goto error_out;			
	}

	result = 1;
error_out:
	if(logheadbuf) free(logheadbuf);
	if(logbuf) free(logbuf);
	return result;
}


int _CheckUpdateDisk(struct nd_diskmgr_struct	*diskmgr,  unsigned int update, unsigned int hostid)
{
	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	unsigned char *logheadbuf = NULL;
	unsigned char *logbuf = NULL;
	int 	result = 0;

	PBITEMAP DiscBitmap;
	PBITEMAP	FreeCluster;
	PBITEMAP	DirtyCluster;

	PON_DISK_META	 		hdisk;
	PON_DISC_ENTRY	 		hentry;
	DISC_LIST				disclist;
	PON_DISK_LOG_HEADER	 hloghead;
	PON_DISK_LOG 			 hlogdata;								
	unsigned int		logindex = 0;
	unsigned int		start_sector_disc = 0;
	struct media_disc_info * Mdiscinfo = NULL;	
	Mdiscinfo = (struct media_disc_info *)diskmgr->mediainfo;


	buf = (unsigned char *)malloc(4096);
	if(!buf){
		printf("_CheckUpdateDisk error :  get buf\n");
		return 0;
	}	
		
	memset(buf, 0, 4096);

	buf2 = (unsigned char *)malloc(4096);
	if(!buf2){
		printf("_CheckUpdateDisk error :  get buf2\n");
		result = 0;
		goto error_out;
	}

	memset(buf2, 0, 4096);

	logheadbuf = (unsigned char *)malloc(1024);
	if(!logheadbuf){
		printf("_CheckUpdateDisk error :  get logheadbuf\n");
		result = 0;
		goto error_out;
	}
	
	memset(logheadbuf, 0, 1024);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf){
		printf("_CheckUpdateDisk error :  get logbuf\n");
		result = 0;
		goto error_out;
	}

	memset(logbuf, 0, 1024);

		
	DEBUGLOG(2,("_CheckUpdateDisk ENTER  driver %p update %d hostid %d\n", diskmgr, update, hostid)); 
		
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("_CheckUpdateDisk error 1  GetDiskMeta\n");
		result = 0;
		goto error_out;
	}

		
	hdisk = (PON_DISK_META) buf;
	
	if(! GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		
		printf("_CheckUpdateDisk error 2 GetDiskLogHead\n");
		result = 0;
		goto error_out;
	}
	
		
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;
	logindex = hloghead->latest_index;
	
	if(logindex >= (unsigned int)Mdiscinfo->count) logindex = 0;	
//	DEBUGLOG("hloghead->latest_index(%d) hloghead->latest_age(%d)\n", 
//		hloghead->latest_index, hloghead->latest_age);
	
	start_sector_disc = MEDIA_DISC_INFO_START_ADDR_SECTOR + logindex * MEDIA_DISC_INFO_SIZE_SECTOR;
	disclist.pt_loc = start_sector_disc;

	
	if(! GetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		
		printf("_CheckUpdateDisk error 2-1 GetDiskLogData\n");
		result = 0;
		goto error_out;
	}

		
	
	hlogdata = (PON_DISK_LOG)logbuf ;

	printf("value of hlogdata->nr_DiscCluster %d\n",hlogdata->nr_DiscCluster);
	if(! GetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("_CheckUpdate error 3 GetDiscMeta\n");
		result = 0;
		goto error_out;
	}

		
	hentry = (PON_DISC_ENTRY) buf2;

	if( (hdisk->age != hloghead->latest_age)
		|| (hentry->age != hlogdata->age)
		||  (hlogdata->action & ACTION_ERROR_MASK) )
	{

		if(update ==0 ) { 
			result = 2;
			goto error_out;
		}

		if(hlogdata->action & ACTION_ERROR_MASK)
		{
			printf("_CheckUpDate Fine Error of disk %p Pysical Disc %d\n invalid Log",diskmgr,  logindex);
			if((hlogdata->action & ACTION_WRITE_STEP_MASK)
				|| ( hlogdata->action & ACTION_DELETE_MASK))
			{
RECHECK:			
							
				DEBUGLOG(2,("hentry->age %d\n",hentry->age));
				DEBUGLOG(2,("hlogdata->age %d\n",hlogdata->age));
				DEBUGLOG(2,("hlogdata->action 0x%x\n",hlogdata->action));
			
				hlogdata->action |= ACTION_UP_RECOVERY;
				hlogdata->valid = DISK_LOG_INVALID;
				hlogdata->hostId = hostid;
				hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
				hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
				hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_S;


				hloghead->latest_log_action |= ACTION_UP_RECOVERY;

				printf("value of hlogdata->nr_DiscCluster %d\n",hlogdata->nr_DiscCluster);
				
				// set log
					// logdata
				if(!SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_WRITE_SECTOR_COUNT))
				{
					
					printf("_CheckUpdate error 4 SetDiskLogData\n");
					result = 0;
					goto error_out;
				}
					// logheader
				if(!SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_WRITE_SECTOR_COUNT))
				{
					
					printf("_CheckUpdate error 5 SetDiskLogHead\n");
					result = 0;
					goto error_out;
				}

				// undo writing
					// disk meta
				FreeCluster = hdisk->FreeCluster_bitmap;
				DirtyCluster = hdisk->DirtyCluster_bitmap;
				printf("value of hlogdata->nr_DiscCluster %d\n",hlogdata->nr_DiscCluster);
				freeDiscMapbyLogData( FreeCluster, DirtyCluster, hlogdata);

				DiscBitmap = hdisk->DiscInfo_bitmap;
				freeDiscInfoMap( logindex, DiscBitmap);
				hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
				hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);
				hdisk->age = hloghead->latest_age + 1;
				
					
				if( ! SetDiskMeta(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
				{	
					printf("_CheckUpdate error 6 SetDiskMeta\n");
					result = 0;
					goto error_out;
				}

				if( ! SetDiskMetaMirror(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
				{	
					printf("_CheckUpdate error 7 SetDiskMetaMirror\n");
					result = 0;
					goto error_out;
				}

				// disc meta
				InitDiscEntry(diskmgr, hdisk->nr_DiscCluster, logindex, start_sector_disc, (PON_DISC_ENTRY)buf2);
				hentry->age = hlogdata->age + 1;
			
				if(! SetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					
					printf("_CheckUpdate error 8 SetDiscMeta\n");
					result = 0;
					goto error_out;
				}
				
				if(! SetDiscMetaMirror(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					
					printf("_CheckUpdate error 8 SetDiscMetaMirror\n");
					result = 0;
					goto error_out;
				}
	
				// set log
					// logheader
				hloghead->latest_log_action = ACTION_DEFAULT;
				hloghead->latest_age = hloghead->latest_age + 1;
				hloghead->latest_index = hloghead->latest_index;
				hloghead->latest_log_history = ((hloghead->latest_log_history + 1) % MAX_LOG_HISTORY);
				hloghead->history[hloghead->latest_log_history].diskId = logindex;
				hloghead->history[hloghead->latest_log_history].actionstatus = ACTION_STATUS_RECOVERY;

				if (! SetDiskLogHead(diskmgr, logbuf,DISK_LOG_HEAD_READ_SECTOR_COUNT) )
				{
					
					printf("_CheckUpdate error 8 SetDiskLogHead\n");
					result = 0;
					goto error_out;
				}
					// logdata
				memset((char *)hlogdata, 0, sizeof(ON_DISK_LOG ));
				hlogdata->prevActionStatus = hlogdata->action;
				hlogdata->action = ACTION_DEFAULT;
				hlogdata->age = hlogdata->age + 1;
				hlogdata->valid = DISK_LOG_VALID;
				hlogdata->hostId = hostid;
				hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
				hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
				hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_E;
				hlogdata->refcount = 0;
				memset(hlogdata->Host,0,SECTOR_SIZE);
				if(! SetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_READ_SECTOR_COUNT))
				{

					printf("_CheckUpdate error 9 SetDiskLogData\n");
					result = 0;
					goto error_out;
				}

				result = 2;
				goto error_out;
			}else{
				// unreachable code

				printf("_CheckUpDate Fine Error of disk %p Pysical Disc %d\n invald log action",diskmgr,  logindex);
				// what can i do for this one --> disable....TT!
				goto RECHECK;
			}
			
		}else{
			// unreachable code
			// what can i do for this one --> disable....TT!

			DEBUGLOG(2,("_CheckUpDate Fine Error of disk %p Pysical Disc %d\n in-consistance age",diskmgr,  logindex));
			if(hdisk->age != hloghead->latest_age)
			{
				printf("hdisk age %d  : log head age %d\n", hdisk->age, hloghead->latest_age);
				hdisk->age = hloghead->latest_age;
				if( ! SetDiskMeta(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
				{
					
					printf("_CheckUpdate error 10 SetDiskMeta\n");
					result = 0;
					goto error_out;
				}


			}

			if(hentry->age != hlogdata->age)
			{
				hentry->age = hlogdata->age;
				
				DEBUGLOG(2,("2hentry->age %d\n",hentry->age));
				DEBUGLOG(2,("2hlogdata->age %d\n",hlogdata->age));
				if(! SetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					printf("disc entry age %d : log data age %d\n", hentry->age, hlogdata->age);
					printf("_CheckUpdate error 11 SetDiscMeta\n");
					result = 0;
					goto error_out;
				}
		
			}
		}
		
	}

	result = 1;
error_out:
	if(buf) free(buf);
	if(buf2) free(buf2);
	if(logheadbuf) free(logheadbuf);
	if(logbuf) free(logbuf);
	return result;
}



int _CheckUpdateSelectedDisk(
	struct nd_diskmgr_struct	*diskmgr,  
	unsigned int update, 
	unsigned int hostid, 
	unsigned int selected_disc)
{

	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	unsigned char *logheadbuf = NULL;
	unsigned char *logbuf = NULL;
	int result = 0;	
	PBITEMAP DiscBitmap;
	PBITEMAP	FreeCluster;
	PBITEMAP	DirtyCluster;

	PON_DISK_META	 		hdisk;
	PON_DISC_ENTRY	 		hentry;
	DISC_LIST				disclist;
	PON_DISK_LOG_HEADER	 hloghead;
	PON_DISK_LOG 			 hlogdata;								
	unsigned int		logindex = 0;
	unsigned int		start_sector_disc = 0;



	buf = (unsigned char *)malloc(4096);
	if(!buf){
		printf("_CheckUpdateSelectedDisk error :  get buf\n");
		return 0;
	}	
		
	memset(buf, 0, 4096);

	buf2 = (unsigned char *)malloc(4096);
	if(!buf2){
		printf("_CheckUpdateSelectedDisk error :  get buf2\n");
		result = 0;
		goto error_out;
	}

	memset(buf2, 0, 4096);

	logheadbuf = (unsigned char *)malloc(1024);
	if(!logheadbuf){
		printf("_CheckUpdateDisk error :  get logheadbuf\n");
		result = 0;
		goto error_out;
	}
	
	memset(logheadbuf, 0, 1024);

	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf){
		printf("_CheckUpdateDisk error :  get logbuf\n");
		result = 0;
		goto error_out;
	}

	memset(logbuf, 0, 1024);

	
	DEBUGLOG(2,("enter _CheckUpdateSeletctedDisk\n"));
		
	if(!GetDiskMeta(diskmgr, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		printf("CheckUpdataSelectDisk Error 1\n");
		result = 0;
		goto error_out;
	}

	hdisk = (PON_DISK_META) buf;
	
	if(!GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		printf("CheckUpdataSelectDisk Error 2\n");
		result = 0;
		goto error_out;
	}
	
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;

	logindex = selected_disc;
	
	start_sector_disc = MEDIA_DISC_INFO_START_ADDR_SECTOR + logindex * MEDIA_DISC_INFO_SIZE_SECTOR;
	disclist.pt_loc = start_sector_disc;

	
	if(! GetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("CheckUpdataSelectDisk Error 3\n");
		result = 0;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG)logbuf ;

#ifdef LOGDBG
	// debug infor 
	dumpHostRef(hlogdata);
	dumpDiscHistory(hlogdata);
	dumpDisKHistory(hloghead);
#endif

	DEBUGLOG(2,("enter _CheckUpdateSeletctedDisk 3\n"));
	if(! GetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("CheckUpdataSelectDisk Error 4\n");
		result = 0;
		goto error_out;
	}

	hentry = (PON_DISC_ENTRY) buf2;


	if(  (hentry->age != hlogdata->age)
		||  (hlogdata->action & ACTION_ERROR_MASK) )
	{

		if(update ==0 ) {
			result = 2;
			goto error_out;
		}
		

		if(hlogdata->action & ACTION_ERROR_MASK)
		{
			if((hlogdata->action & ACTION_WRITE_STEP_MASK)
				|| ( hlogdata->action & ACTION_DELETE_MASK))
			{
RECHECK:			
	
				DEBUGLOG(2,("hentry->age %d\n",hentry->age));
				DEBUGLOG(2,("hlogdata->age %d\n",hlogdata->age));
				DEBUGLOG(2,("hlogdata->action 0x%x\n",hlogdata->action));
					
				hlogdata->action |= ACTION_UP_RECOVERY;
				hlogdata->valid = DISK_LOG_INVALID;
				hlogdata->hostId = hostid;		
				hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
				hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
				hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_S;
			
				hloghead->latest_index = logindex;
				hloghead->latest_log_action = hlogdata->action;
				hloghead->latest_log_action |= ACTION_UP_RECOVERY;
				// set log
					// logdata
				if(!SetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_READ_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 5\n");
					result = 0;
					goto error_out;
				}
					// logheader
				if(!SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 6\n");
					result = 0;
					goto error_out;
				}

				// undo writing
					// disk meta
				FreeCluster = hdisk->FreeCluster_bitmap;
				DirtyCluster = hdisk->DirtyCluster_bitmap;
				freeDiscMapbyLogData( FreeCluster, DirtyCluster, hlogdata);

				DiscBitmap = hdisk->DiscInfo_bitmap;
				freeDiscInfoMap( logindex, DiscBitmap);
				hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
				hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);
				hdisk->age = hloghead->latest_age + 1;
					
				if( ! SetDiskMeta(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 7\n");
					result = 0;
					goto error_out;
				}


				if( ! SetDiskMetaMirror(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 8\n");
					result = 0;
					goto error_out;
				}
					// disc meta
				InitDiscEntry(diskmgr, hdisk->nr_DiscCluster, logindex, start_sector_disc, (PON_DISC_ENTRY)buf2);
				hentry->age = hlogdata->age + 1;
			
				if(! SetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 9\n");
					result = 0;
					goto error_out;
				}

				if(! SetDiscMetaMirror(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 10\n");
					result = 0;
					goto error_out;
				}
				// set log
					// logheader
				hloghead->latest_log_action = ACTION_DEFAULT;
				hloghead->latest_age = hloghead->latest_age + 1;
				hloghead->latest_index = hloghead->latest_index;
				hloghead->latest_log_history = ((hloghead->latest_log_history + 1) % MAX_LOG_HISTORY);
				hloghead->history[hloghead->latest_log_history].diskId = logindex;
				hloghead->history[hloghead->latest_log_history].actionstatus = ACTION_STATUS_RECOVERY;

				if (! SetDiskLogHead(diskmgr, logbuf,DISK_LOG_HEAD_WRITE_SECTOR_COUNT) )
				{
					printf("CheckUpdataSelectDisk Error 11\n");
					result = 0;
					goto error_out;
				}
					// logdata
				memset((char *)hlogdata, 0, sizeof(ON_DISK_LOG ));
				hlogdata->prevActionStatus = hlogdata->action;
				hlogdata->action = ACTION_DEFAULT;
				hlogdata->age = hlogdata->age + 1;
				hlogdata->valid = DISK_LOG_VALID;
				hlogdata->hostId = hostid;
				hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
				hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
				hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_E;
				hlogdata->refcount = 0;
				memset(hlogdata->Host,0,SECTOR_SIZE);	
				
				if(! SetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_WRITE_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 12\n");
					result = 0;
					goto error_out;
				}

				result = 2;
				goto error_out;
			}else{
				// unreachable code

				// what can i do for this one --> disable....TT!
				goto RECHECK;
			}
			
		}else{
		
			// unreachable code
			// what can i do for this one --> disable....TT!
			if(hentry->age != hlogdata->age)
			{
				hentry->age = hlogdata->age;

				DEBUGLOG(2,("2hentry->age %d\n",hentry->age));
				DEBUGLOG(2,("2hlogdata->age %d\n",hlogdata->age));
				if(! SetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
				{
					printf("CheckUpdataSelectDisk Error 13\n");
					result = 0;
					goto error_out;
				}
		
			}

		}
		
	}
	result = 1;	
error_out:
	if(buf) free(buf);
	if(buf2) free(buf2);
	if(logheadbuf) free(logheadbuf);
	if(logbuf) free(logbuf);
	return result;

}

/*
must call GetDiskMeta   GetDiscMeta before call this function 
*/
int _InvalidateDisc(
	struct nd_diskmgr_struct	*diskmgr,  
	unsigned char * buf,
	unsigned char * buf2, 
	unsigned int hostid)
{
	unsigned char *logheadbuf = NULL;
	unsigned char *logbuf = NULL;
	int result = 0;


	PON_DISK_META	 		hdisk;
	PON_DISC_ENTRY	 		hentry;
	DISC_LIST			disclist;
	PON_DISK_LOG_HEADER	 	hloghead;
	PON_DISK_LOG 			 hlogdata;								
	unsigned int		logindex = 0;
	unsigned int		start_sector_disc = 0;

	PBITEMAP DiscBitmap;
	PBITEMAP	FreeCluster;
	PBITEMAP	DirtyCluster;	

	hdisk = (PON_DISK_META) buf;
	hentry = (PON_DISC_ENTRY) buf2;


	logheadbuf  = (unsigned char *)malloc(1024);
	if(!logheadbuf ){
		printf("_InvalidateDisc Error : can't alloc logheadbuf \n");
		return 0;
	}
	memset(logheadbuf, 0, 1024);
	
	logbuf = (unsigned char *)malloc(1024);
	if(!logbuf){
		printf("_InvalidateDisc Error : can't alloc logbuf\n");
		result = 0;
		goto error_out;
	}
	memset(logbuf, 0, 1024);
	
	DEBUGLOG(2,("enter _InvalidateDisc\n"));
	if(!GetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_READ_SECTOR_COUNT)) 
	{	
		printf("_InvalidateDisc Error 1\n");
		result = 0;
		goto error_out;
	}
	
	hloghead = (PON_DISK_LOG_HEADER)logheadbuf;

	logindex = hentry->index;
	
	start_sector_disc = MEDIA_DISC_INFO_START_ADDR_SECTOR + logindex * MEDIA_DISC_INFO_SIZE_SECTOR;
	disclist.pt_loc = start_sector_disc;

	
	if(! GetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_READ_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 2\n");
		result = 0;
		goto error_out;
	}

	hlogdata = (PON_DISK_LOG)logbuf;


	hloghead->latest_index = logindex;
	hloghead->latest_log_action = hlogdata->action;
	hloghead->latest_log_action |= ACTION_UP_RECOVERY;

				
	hlogdata->action |= ACTION_UP_RECOVERY;
	hlogdata->valid = DISK_LOG_INVALID;
	hlogdata->hostId = hostid;
	
	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_S;
				
	// set log
		// logdata
	if(!SetDiskLogData(diskmgr, logbuf, logindex, DISK_LOG_DATA_WRITE_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 3\n");
		result = 0;
		goto error_out;
	}
		// logheader
	if(!SetDiskLogHead(diskmgr, logheadbuf, DISK_LOG_HEAD_WRITE_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 4\n");
		result = 0;
		goto error_out;
	}

	// undo writing
		// disk meta
	FreeCluster = hdisk->FreeCluster_bitmap;
	DirtyCluster = hdisk->DirtyCluster_bitmap;
	freeDiscMapbyLogData( FreeCluster, DirtyCluster, hlogdata);

	DiscBitmap = hdisk->DiscInfo_bitmap;
	freeDiscInfoMap( logindex, DiscBitmap);
	hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
	hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);
	hdisk->age = hloghead->latest_age + 1;
		
	if( ! SetDiskMeta(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 5\n");
		result = 0;
		goto error_out;
	}

	if( ! SetDiskMetaMirror(diskmgr, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 6\n");
		result = 0;
		goto error_out;
	}
		// disc meta
	InitDiscEntry(diskmgr, hdisk->nr_DiscCluster, logindex, start_sector_disc, (PON_DISC_ENTRY)buf2);
	hentry->age = hlogdata->age + 1;

	if(! SetDiscMeta(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 7\n");
		result = 0;
		goto error_out;
	}

	if(! SetDiscMetaMirror(diskmgr, &disclist, buf2, DISC_META_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 8\n");
		result = 0;
		goto error_out;
	}
	
	// set log
		// logheader
	hloghead->latest_log_action = ACTION_DEFAULT;
	hloghead->latest_age = hloghead->latest_age + 1;
	hloghead->latest_index = hloghead->latest_index;
	hloghead->latest_log_history = ((hloghead->latest_log_history + 1) % MAX_LOG_HISTORY);
	hloghead->history[hloghead->latest_log_history].diskId = logindex;
	hloghead->history[hloghead->latest_log_history].actionstatus = ACTION_STATUS_RECOVERY;

	if (! SetDiskLogHead(diskmgr, logbuf,DISK_LOG_HEAD_WRITE_SECTOR_COUNT) )
	{
		printf("_InvalidateDisc Error 9\n");
		result = 0;
		goto error_out;
	}
		// logdata
	memset((char *)hlogdata, 0, sizeof(ON_DISK_LOG ));
	hlogdata->prevActionStatus = hlogdata->action;
	hlogdata->action = ACTION_DEFAULT;
	hlogdata->age = hlogdata->age + 1;
	hlogdata->valid = DISK_LOG_VALID;
	hlogdata->hostId = hostid;

	hlogdata->latest_disc_history = ((hlogdata->latest_disc_history + 1) % MAX_DISC_LOG_HISTORY);
	hlogdata->History[hlogdata->latest_disc_history].hostid = hostid;
	hlogdata->History[hlogdata->latest_disc_history].action = LOG_ACT_RECOVERY_E;
	hlogdata->refcount = 0;
	memset(hlogdata->Host, 0, SECTOR_SIZE);

	if(! SetDiskLogData(diskmgr, logbuf, logindex,DISK_LOG_DATA_WRITE_SECTOR_COUNT))
	{
		printf("_InvalidateDisc Error 10\n");
		result = 0;
		goto error_out;
	}

	result = 1;
error_out:
	if(logbuf) free(logbuf);
	if(logheadbuf) free(logheadbuf);
	return result;
}


int _CheckHostOwner(
	struct nd_diskmgr_struct * diskmgr, 
	unsigned int		selectedDisc,
	unsigned int 		hostid
	)
{
	PON_DISK_LOG	hlogdata;
	unsigned char *logbuf = NULL;
	int result = 0;

	logbuf = (unsigned char *)malloc(1024);

	if(!logbuf){
		printf("CheckHostOwner Error: can't alloc logbuf\n");
		return 0;
	}

	memset(logbuf, 0, 1024);

    if( !GetDiskLogData(diskmgr, logbuf, selectedDisc, DISK_LOG_DATA_READ_SECTOR_COUNT) )
    {
            printf("CheckHostOwner Error: _GetDiskLogData\n");
            result= 0;
			goto error_out;
    }

	hlogdata = (PON_DISK_LOG)logbuf;
	if(hlogdata->hostId == hostid) result = 1;
	else result = 0;

error_out:
	if(logbuf) free(logbuf);
	return result;
}


int _CheckWriteFail(
	struct nd_diskmgr_struct * diskmgr, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	)
{
	PON_DISK_LOG	hlogdata;
	unsigned char *logbuf = NULL;
	int result = 0;

	logbuf = (unsigned char *)malloc(1024);

	if(!logbuf){
		printf("CheckWriteFail Error: can't alloc logbuf\n");
		return 0;
	}
	memset(logbuf, 0, 1024);

	if(entry->status != DISC_STATUS_WRITING) return 0;

        if( !GetDiskLogData(diskmgr, logbuf, entry->index, DISK_LOG_DATA_READ_SECTOR_COUNT) )
        {
                printf("CheckWriteFail Error: _GetDiskLogData\n");
                return 0;
        }

	hlogdata = (PON_DISK_LOG)logbuf;

	// check starting time :
		
	if(hlogdata->action & ACTION_WRITE_STEP1_MASK)
	{
#ifdef	GTIME
		struct timeval tv;
		unsigned int sec;
		sec = (unsigned int)(logdata->time >> 32);
		do_gettimeofday(&tv);
		DEBUGLOG(2,("Writing Set time (%d) : Current time (%ld)\n", sec, tv.tv_sec));
		if( (tv.tv_sec - sec) > 10800 )  // 3 hous
		{	

			if(!_InvalidateDisc (diskmgr, (char *)hdisk, (char *)entry, hostid))
			{
				printf("SetCurrentDisc Error 20: _InvalidateDisc\n");
				result = 0;
				goto error_out;
			}
			result = 2;
			goto error_out;
				
		}
#endif
		if(hlogdata->hostId == hostid)
		{
			DEBUGLOG(2,("This DISC is in Writing fail status\n"));	
			if(!_InvalidateDisc (diskmgr, (unsigned char *)hdisk, (unsigned char *)entry, hostid))
			{
				printf("SetCurrentDisc Error 20: _InvalidateDisc\n");
				result = 0;
				goto error_out;
			}
			result = 2;
			goto error_out;

		}
	}

	result = 1;
error_out:
	if(logbuf) free(logbuf);
	return result;		
}


int _CheckDeleteFail(
	struct nd_diskmgr_struct * diskmgr, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	)
{
	PON_DISK_LOG	hlogdata;
	struct uhead * ubuf = NULL;
	unsigned char *logbuf = NULL;
	int result = 0;

	logbuf = (unsigned char *)malloc(1024);

	if(!logbuf){
		printf("CheckDeleteFail Error: can't alloc logbuf\n");
		return 0;
	}
	memset(logbuf,0,1024);

	if(entry->status != DISC_STATUS_ERASING) return 0;

    if( !GetDiskLogData(diskmgr, logbuf, entry->index, DISK_LOG_DATA_READ_SECTOR_COUNT) )
    {
            printf("CheckDeleteFail Error: _GetDiskLogData\n");
            result = 0;
			goto error_out;
    }
	hlogdata = (PON_DISK_LOG)logbuf;


	// check starting time :
		
	if(hlogdata->action & ACTION_DS1_DISCENTRY_END)
	{
		int hostindex = 0;
		int refcount = 0;
	
	
#ifdef	GTIME
		struct timeval tv;
		unsigned int sec;
		sec = (unsigned int)(logdata->time >> 32);
		do_gettimeofday(&tv);
		DEBUGLOG(2,("Writing Set time (%d) : Current time (%ld)\n", sec, tv.tv_sec));
		if( (tv.tv_sec - sec) > 10800 )  // 3 hous
		{	

			if(!_InvalidateDisc (diskmgr, (char *)hdisk, (char *)entry, hostid))
			{
				printf("_CheckDeleteFail Error: _InvalidateDisc\n");
				result = 0;
				goto error_out;
			}
			result = 2;
			goto error_out;
				
		}
#endif
		if(hlogdata->refcount == 1)
		{

			hostindex= getLogIndex(hlogdata, hostid);

			if(hostindex == -1)
			{
				printf("_CheckDeleteFail Error can't get hostindex\n");
				result = 1;
				goto error_out;

			}else{
				refcount = getLogRefCount(hlogdata, hostindex);	
				if(refcount == 1) {
					if(!_InvalidateDisc (diskmgr, (unsigned char *)hdisk, (unsigned char *)entry, hostid))
					{
						printf("_CheckDeleteFail Error can't InvalidateDisc\n");
						result = 0;
						goto error_out;
					}
					result = 2;
					goto error_out;
				}
			}
		}
	}
	result = 1;
error_out:
	if(logbuf) free(logbuf);
	return result;		
}




int _CheckValidateCount(
		struct nd_diskmgr_struct * diskmgr, 
		unsigned int selected_disc
		)
{
	
	PON_DISK_LOG	hlogdata;
	unsigned char *logbuf = NULL;
	int result = 0;

	logbuf = (unsigned char *)malloc(1024);

	if(!logbuf){
		printf("CheckValidateCount Error: can't alloc logbuf\n");
		return 0;
	}

	memset(logbuf,0,1024);

    if( !GetDiskLogData(diskmgr, logbuf, selected_disc, DISK_LOG_DATA_READ_SECTOR_COUNT) )
    {
            printf("CheckWriteFail Error: _GetDiskLogData\n");
            result = 0;
			goto error_out;
    }
	hlogdata = (PON_DISK_LOG)logbuf;
	if(hlogdata->validcount == 0) result = -1;
	else result =  1;

error_out:
	if(logbuf) free(logbuf);
	return result;
}	
