#ifndef _JB_M_NDAS_METAINFO_H_
#define _JB_M_NDAS_METAINFO_H_

#ifdef  __cplusplus
extern "C"
{
#endif 
int findFreeMapCount(unsigned int bitmap_count, PBITEMAP bitmap);
int findDirtyMapCount(unsigned int bitmap_count, PBITEMAP bitmap);
void setDiscAddr_diskFreeMap(unsigned int bitmap_count, unsigned int request_alloc, PBITEMAP diskfreemap, PON_DISC_ENTRY entry);
void setDiscAddrtoLogData(PON_DISC_ENTRY entry, PON_DISK_LOG logdata);
void freeClusterMap (unsigned int index , PBITEMAP DiscMap);
void setClusterMap (unsigned int index, PBITEMAP DiscMap);
void setClusterMapFromEntry(PBITEMAP clustermap, PON_DISC_ENTRY entry);
void freeClusterMapFromEntry(PBITEMAP clustermap, PON_DISC_ENTRY entry);
void setAllClusterMapFromEntry(PBITEMAP freeclustermap, PBITEMAP dirtyclustermap, PON_DISC_ENTRY entry);
void freeAllClusterMapFromEntry(PBITEMAP freeclustermap, PBITEMAP dirtyclustermap, PON_DISC_ENTRY entry);
void freeDiscMapbyLogData(PBITEMAP freeMap, PBITEMAP dirtyMap, PON_DISK_LOG logdata);
void freeDiscInfoMap (unsigned int index , PBITEMAP DiscMap);
void setDiscInfoMap (unsigned int index, PBITEMAP DiscMap);

void SetDiskMetaInformation(struct nd_diskmgr_struct * diskmgr, PON_DISK_META buf, unsigned int update);
void AllocAddrEntities(struct media_addrmap * amap, PON_DISC_ENTRY entry);
void InitDiscEntry(
	struct nd_diskmgr_struct * diskmgr, 
	unsigned int cluster_size, 
	unsigned int index, 
	unsigned int loc, 
	PON_DISC_ENTRY buf);
void SetDiscEntryInformation (struct nd_diskmgr_struct * diskmgr, PON_DISC_ENTRY buf, unsigned int update);
int SetDiscAddr(struct nd_diskmgr_struct *diskmgr, struct media_addrmap * amap, int disc);
#ifdef  __cplusplus
extern "C"
}
#endif
 
#endif //_JB_M_NDAS_METAINFO_H_