#ifndef _JB_M_MEDIA_OP_H_
#define _JB_M_MEDIA_OP_H_

#ifdef  __cplusplus
extern "C"
{
#endif 

int getHashVal(int hostid);

int getLogIndex(PON_DISK_LOG logdata,int hostid);

int getLogRefCount(PON_DISK_LOG logdata, int index);

void setLogRefCount(PON_DISK_LOG logdata, int index, unsigned int value);

int setLogIndex(PON_DISK_LOG logdata, int hostid);

void dumpHostRef(PON_DISK_LOG logdata);

void dumpDiscHistory(PON_DISK_LOG logdata);

void dumpDisKHistory(PON_DISK_LOG_HEADER loghead);

int _GetDiscRefCount(struct nd_diskmgr_struct * diskmgr, unsigned int selectedDisc);

// _AddRefCount and _RemoveRefCount must be done after Error checking routine
//	_CheckCurrentDisk
//	_CheckCurrentDisc
//	_DoDelayedOperation

int _AddRefCount(
		struct nd_diskmgr_struct * diskmgr,
		unsigned int selectedDisc,
		unsigned int hostid);

int _RemoveRefCount(
		struct nd_diskmgr_struct * diskmgr,
		unsigned int selectedDisc,
		unsigned int hostid);

int _WriteAndLogStart(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc, 
		unsigned int sector_count,
		unsigned int encrypt);

int _WriteAndLogEnd(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc);

int _DeleteAndLog(
		struct nd_diskmgr_struct * diskmgr, 
		PON_DISK_META	hdisk,
		PON_DISC_ENTRY 	entry,
		unsigned int hostid,	
		unsigned int currentDisc);

int _CheckUpdateDisk(struct nd_diskmgr_struct	*diskmgr,  unsigned int update, unsigned int hostid);

int _CheckUpdateSelectedDisk(
	struct nd_diskmgr_struct	*diskmgr,  
	unsigned int update, 
	unsigned int hostid, 
	unsigned int selected_disc);

/*
must call GetDiskMeta   GetDiscMeta before call this function 
*/
int _InvalidateDisc(
	struct nd_diskmgr_struct	*diskmgr,  
	unsigned char * buf,
	unsigned char * buf2, 
	unsigned int hostid);

int _CheckWriteFail(
	struct nd_diskmgr_struct * diskmgr, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	);

int _CheckDeleteFail(
	struct nd_diskmgr_struct * diskmgr, 
	PON_DISK_META		hdisk,
	PON_DISC_ENTRY		entry,
	unsigned int		selectedDisk,
	unsigned int 		hostid
	);

int _CheckHostOwner(
	struct nd_diskmgr_struct * diskmgr, 
	unsigned int		selectedDisc,
	unsigned int 		hostid
	);



int _CheckValidateCount(
		struct nd_diskmgr_struct * diskmgr, 
		unsigned int selected_disc
		);
#ifdef  __cplusplus
}
#endif 
#endif //_JB_M_MEDIA_OP_H_
