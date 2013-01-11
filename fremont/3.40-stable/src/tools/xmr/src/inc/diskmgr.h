#ifndef __JB_M_DISKMGR_H__
#define __JB_M_DISKMGR_H__

#include "../inc/LanScsiOp.h"
#include "../inc/MediaDisk.h"
#include <pshpack1.h>

#define MAX_REQ_SIZE (1024*64)

struct media_addrmap{	
	int disc;
	int count;
	int ref;
	DISC_MAP_LIST	addrMap[MAX_ADDR_ALLOC_COUNT];
};

struct media_disc_info{
	int count;
	DISC_LIST	discinfo[MAX_DISC_LIST];
};

struct media_key{
	int disk;
	int disc;
	char cipherInstance[2040];
	char cipherKey[2048];
};

#define MAX_HINT	128
typedef struct {
	char _data[32];
}EHINT;

struct media_encryptID{
	EHINT	hint[MAX_HINT];
};

typedef struct remote_target {
	LPX_ADDRESS	LpxAddress;
	
	ULONG		ulUnitBlocks;
	unsigned _int32	iUserID;
	unsigned _int64	iPassword;

	UCHAR		ucUnitNumber;
	UCHAR		ucAccess;
	ACCESS_MASK	DesiredAccess;
	//
	// to support version 1.1 2.0 20040401
	//
	UCHAR	ucHWType;
	UCHAR	ucHWVersion;
	//
	// end of supporting version
	//
} REMOTE_TARGET, *PREMOTE_TARGET;


struct nd_diskmgr_struct {
	// Disk Information block

#define DMGR_FLAG_NOT_20		0x00000010	//	not 20	
#define DMGR_FLAG_FORMATED		0x00000100	//	formated for media juke
#define DMGR_FLAG_NEED_FORMAT	0x00000200	//	need format
	unsigned int 	flags;
	BOOL			isJukeBox;
	unsigned long	nr_sectors;
	unsigned long	cyls;
	unsigned long	heads;
	unsigned long	sectors;

	struct media_key		*MetaKey;
	struct media_key		*DiscKey;
	struct media_disc_info	*mediainfo;
	struct media_encryptID	*encryptInfo;
	struct media_addrmap	*DiscAddr;
	REMOTE_TARGET			remote;
	LANSCSI_PATH			Path;

#define JUKEMAGIC			1919
	unsigned int			OpenFileFd;
	unsigned int		CurrentFilePointer;
	BOOL					isInitialized;
	unsigned char			*Trnasferbuff;
};
#include <poppack.h>
#endif//#ifndef __JB_M_DISKMGR_H__