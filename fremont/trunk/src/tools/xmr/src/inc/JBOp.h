#ifndef _JB_M_JBOP_H_
#define _JB_M_JBOP_H_

#include "../inc/SocketLpx.h"
#include "../inc/diskmgr.h"
#include "../inc/JukeFileApi.h"

#include <pshpack1.h>


typedef DWORD	ACCESS_MASK;

typedef struct _UNITDISK {
	LPX_ADDRESS	Address;
	unsigned _int32	iUserID;
	unsigned _int64	iPassword;
	UCHAR		ucUnitNumber;
	ULONG		ulUnitBlocks;

	//
	// to support version 1.1 2.0 20040401
	//

	UCHAR		ucHWType;
	UCHAR		ucHWVersion;

	// end of supporting version
} UNITDISK, *PUNITDISK;

typedef struct _LANSCSI_ADD_TARGET_DATA
{
	// Size of Parameter.
    ULONG		ulSize;                          

	// LanscsiBus Slot Number.
	ULONG		ulSlotNo;
	
	// Disk Info.
	UCHAR		ucTargetType;
	ULONG		ulTargetBlocks;
	ACCESS_MASK	DesiredAccess;

	// hostid
	ULONG		hostid;

	UCHAR		ucNumberOfUnitDiskList;
	UNITDISK	UnitDiskList[1];

} LANSCSI_ADD_TARGET_DATA, *PLANSCSI_ADD_TARGET_DATA;


typedef struct _U_DISC_DATA
{
	unsigned char		title_name[128];
	unsigned char		additional_infomation[128];
	unsigned char		key[128];
	unsigned char		title_info[128];	
}U_DISC_DATA, *PU_DISC_DATA;

#define	COM_GET_DISC_HEADER		0x00000001
#define COM_GET_DISC_DATA		0x00000002

typedef struct _GET_DISC_INFO
{
	IN ULONG	OpType;	
	IN unsigned int	size	;
	IN unsigned int	selected_disk;
	IN unsigned int	selected_disc;
	OUT unsigned int 	result;
	unsigned int		loc;
	unsigned int		status;
	unsigned int		refcount;
	unsigned int		nr_DiscSector;
	unsigned int		nr_DiscCluster;
	unsigned char 	data[1];
}GET_DISC_INFO, *PGET_DISC_INFO;


typedef struct 	_END_BURN_INFO
{
	unsigned char		title_name[128];
	unsigned char		additional_infomation[128];
	unsigned char		key[128];
	unsigned char		title_info[128];
}END_BURN_INFO, *PEND_BURN_INFO;

#include <poppack.h>

//////////////////////////////////////////////////////////
//
//		Function description
//
//////////////////////////////////////////////////////////
#define O_READ				0x1
#define	O_WRITE				0x2
#define O_MASK				0x3

#define SEEK_B				0x1
#define SEEK_E				0x2
#define SEEK_C				0x4
#define SEEK_MASK			0x7

#ifdef  __cplusplus
extern "C"
{
#endif 
//////////////////////////////////////////////////////////////
//
//	return value < 0 error
//
//
///////////////////////////////////////////////////////////////
/*
int JKBox_open(int disc, int rw);
int JKBox_close(int fd);
int JKBox_lseek(int fd, unsigned int sector_offset, int pose);
int JKBox_read(int fd, char * buff, int sector_count);
int JKBox_write(int fd, char *buff, int sector_count);
*/
struct nd_diskmgr_struct * InitializeJB( PLANSCSI_ADD_TARGET_DATA add_adapter_data );
int closeJB(struct nd_diskmgr_struct * diskmgr);


///////////////////////////////////////////////////////////////
//
//
//	return value == 0 error
//
////////////////////////////////////////////////////////////////
int BurnStartCurrentDisc ( 
						  struct nd_diskmgr_struct * diskmgr,
						  unsigned int	Size_sector,
						  unsigned int	selected_disc,
						  unsigned int	encrypted,
						  unsigned char * HINT,
						  unsigned int hostid
						  );

int BurnEndCurrentDisc( 
					   struct nd_diskmgr_struct * diskmgr ,
					   unsigned int  selected_disc, 
					   PEND_BURN_INFO	info,
					   unsigned int hostid
					   );

int DeleteCurrentDisc( 
					struct nd_diskmgr_struct * diskmgr, 
					unsigned int selected_disc, 
					unsigned int hostid
					);

int CheckDiscValidity( struct nd_diskmgr_struct * diskmgr, 
			unsigned int select_disc, 
			unsigned int hostid);

int GetCurrentDiscInfo( 
			struct nd_diskmgr_struct * diskmgr, 
			unsigned int selected_disc,
			unsigned int command, 
			unsigned char *buf
			);

int DISK_FORMAT( struct nd_diskmgr_struct * diskmgr);

int ValidateDisc(
				 struct nd_diskmgr_struct * diskmgr, 
				 unsigned int selected_disc, 
				 unsigned int hostid
				 );



void CheckFormat(struct nd_diskmgr_struct * diskmgr);

#ifdef  __cplusplus
}
#endif 

#endif//#ifndef _JB_M_JBOP_H_
