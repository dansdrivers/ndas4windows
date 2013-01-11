#ifndef __JB_M_METAOP_H__
#define __JB_M_METAOP_H__
/////////////////////////////////////////////
//
//		MetaOp.h
//		
//		Acess MediaJukebox Raw Operation
//
//
/////////////////////////////////////////////
#ifdef  __cplusplus
extern "C"
{
#endif 

int GetDiskMeta(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int GetDiskMetaMirror(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int GetDiskLogHead(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int GetDiskLogData(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int index, int count);

int SetDiskMeta(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int SetDiskMetaMirror(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int SetDiskLogHead(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int count);

int SetDiskLogData(struct nd_diskmgr_struct * diskmgr, unsigned char * buf, int index, int count);

int GetDiscMeta(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count);

int GetDiscMetaMirror(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count);

int SetDiscMeta(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count);

int SetDiscMetaMirror(struct nd_diskmgr_struct * diskmgr, PDISC_LIST disc_list, unsigned char * buf, int count);

int RawDiskOp( 
	struct nd_diskmgr_struct * diskmgr, 
	unsigned _int8 Command, 
	unsigned _int64 start_sector, 
	int count,  
	unsigned _int8 feature, 
	unsigned char * buff);

int JukeBoxOp(
			struct nd_diskmgr_struct * diskmgr,
			int		disc,
			unsigned _int8 Command, 
			unsigned int s_sector, //512 base
			int s_count,   //512 base
			unsigned char * buff
				);

#ifdef  __cplusplus
}
#endif 
#endif//#ifndef __JB_M_METAOP_H__
