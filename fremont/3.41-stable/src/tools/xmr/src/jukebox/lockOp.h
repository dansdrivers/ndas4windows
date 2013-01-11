#ifndef _JB_M_NDAS_LOCK_H_
#define _JB_M_NDAS_LOCK_H_
#define DISK_LOCK	0
#ifdef  __cplusplus
extern "C"
{
#endif 
int GetLock( struct nd_diskmgr_struct * diskmgr, int lock);
int ReleaseLock( struct nd_diskmgr_struct * diskmgr, int lock);


#ifdef  __cplusplus
}
#endif 
#endif //_JB_M_NDAS_LOCK_H_