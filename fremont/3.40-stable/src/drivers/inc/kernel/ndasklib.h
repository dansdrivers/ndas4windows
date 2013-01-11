#ifndef __NDAS_KLIB_H__
#define __NDAS_KLIB_H__

#include <ndis.h>
#include <tdikrnl.h>

#if DBG

typedef enum _NDASSCSI_DBG_FLAGS {

	NDASSCSI_DBG_MINIPORT_NOISE		= 0x00000001,
	NDASSCSI_DBG_MINIPORT_TRACE		= 0x00000002,
	NDASSCSI_DBG_MINIPORT_INFO		= 0x00000004,
	NDASSCSI_DBG_MINIPORT_ERROR		= 0x00000008,

    NDASSCSI_DBG_LUR_NOISE		    = 0x00000010,
    NDASSCSI_DBG_LUR_TRACE          = 0x00000020,
    NDASSCSI_DBG_LUR_INFO	        = 0x00000040,
    NDASSCSI_DBG_LUR_ERROR			= 0x00000080,

	NDASSCSI_DBG_LURN_NDASR_NOISE	= 0x00000100,
	NDASSCSI_DBG_LURN_NDASR_TRACE	= 0x00000200,
	NDASSCSI_DBG_LURN_NDASR_INFO	= 0x00000400,
	NDASSCSI_DBG_LURN_NDASR_ERROR	= 0x00000800,

	NDASSCSI_DBG_LURN_IDE_NOISE		= 0x00001000,
	NDASSCSI_DBG_LURN_IDE_TRACE		= 0x00002000,
	NDASSCSI_DBG_LURN_IDE_INFO		= 0x00004000,
	NDASSCSI_DBG_LURN_IDE_ERROR		= 0x00008000,

	NDASSCSI_DBG_CCB_NOISE			= 0x00010000,
	NDASSCSI_DBG_CCB_TRACE			= 0x00020000,
	NDASSCSI_DBG_CCB_INFO			= 0x00040000,
	NDASSCSI_DBG_CCB_ERROR			= 0x00080000,

	NDASSCSI_DBG_TRANSPORT_NOISE	= 0x00100000,
	NDASSCSI_DBG_TRANSPORT_TRACE	= 0x00200000,
	NDASSCSI_DBG_TRANSPORT_INFO		= 0x00400000,
	NDASSCSI_DBG_TRANSPORT_ERROR	= 0x00800000,

	NDASSCSI_DBG_ALL_NOISE		    = 0x10000000,
	NDASSCSI_DBG_ALL_TRACE			= 0x20000000,
	NDASSCSI_DBG_ALL_INFO			= 0x40000000,
	NDASSCSI_DBG_ALL_ERROR			= 0x80000000,

} NDASSCSI_DBG_FLAGS;


extern NDASSCSI_DBG_FLAGS NdasScsiDebugLevel;

#ifndef FlagOn
#define FlagOn(F,SF) ( \
    (((F) & (SF)))     \
)
#endif

#define DebugTrace( _dbgLevel, _string )							\
	(FlagOn(NdasScsiDebugLevel, (_dbgLevel)) ?						\
		(DbgPrint ("[%s:%d] ", __FUNCTION__, __LINE__), DbgPrint _string) : ((void)0))

#else

#define DebugTrace( _dbgLevel, _string )

#endif


#define LSPROTO_PASSWORD_LENGTH		sizeof(UINT64)
#define LSPROTO_USERID_LENGTH		sizeof(UINT32)
#define CONVERT_TO_ROUSERID(USERID)	((USERID) & 0xffff)

//
//	Structure types
//
#define LSSTRUC_TYPE_CCB					0x0001
#define LSSTRUC_TYPE_LUR					0x0002
#define LSSTRUC_TYPE_LURN					0x0003
#define LSSTRUC_TYPE_LURN_INTERFACE			0x0004
#define LSSTRUC_TYPE_PROTOCOL				0x0005
#define LSSTRUC_TYPE_PROTOCOL_INTERFACE		0x0006
#define LSSTRUC_TYPE_TRANSPORT				0x0007

#include "ndasccb.h"

#include "ndasutils.h"
#include "ndaslur.h"

static PCHAR
CcbOperationCodeString (
	UINT32	OperationCode
	)
{
	switch (OperationCode) {

	case CCB_OPCODE_RESETBUS:

		return "CCB_OPCODE_RESETBUS";

	case CCB_OPCODE_ABORT_COMMAND:

		return "CCB_OPCODE_ABORT_COMMAND";

	case CCB_OPCODE_EXECUTE:

		return "CCB_OPCODE_EXECUTE";

	case CCB_OPCODE_SHUTDOWN:

		return "CCB_OPCODE_SHUTDOWN";

	case CCB_OPCODE_FLUSH:

		return "CCB_OPCODE_FLUSH";

	case CCB_OPCODE_STOP:

		return "CCB_OPCODE_STOP";

	case CCB_OPCODE_RESTART:

		return "CCB_OPCODE_RESTART";

	case CCB_OPCODE_QUERY:

		return "CCB_OPCODE_QUERY";

	case CCB_OPCODE_UPDATE:

		return "CCB_OPCODE_UPDATE";

	case CCB_OPCODE_DVD_STATUS:

		return "CCB_OPCODE_DVD_STATUS";

	case CCB_OPCODE_SETBUSY:

		return "CCB_OPCODE_SETBUSY";

	case CCB_OPCODE_SMART:

		return "CCB_OPCODE_SMART";

	case CCB_OPCODE_DEVLOCK:

		return "CCB_OPCODE_DEVLOCK";
		
	default:

		return "** Unknown Operation code **";
	}
}

static PCHAR
CdbOperationString (
	UCHAR	Code
	)
{
	switch (Code) {

	case SCSIOP_READ_CAPACITY:
		return "READ_CAPACITY";
	
	case SCSIOP_READ_CAPACITY16:
		return "READ_CAPACITY16";
	
	case SCSIOP_READ:
		return "READ";
	
	case SCSIOP_READ16:
		return "READ16";
	
	case SCSIOP_INQUIRY:
		return "INQUIRY";
	
	case SCSIOP_RELEASE_UNIT:
		return "RELEASE_UNIT";
	
	case SCSIOP_RELEASE_UNIT10:
		return "RELEASE_UNIT10";
	
	case SCSIOP_TEST_UNIT_READY:
		return "TEST_UNIT_READY";
	
	case SCSIOP_MODE_SENSE:
		return "MODE_SENSE";
	
	case SCSIOP_WRITE:
		return "WRITE";
	
	case SCSIOP_WRITE16:
		return "WRITE16";
	
	case SCSIOP_VERIFY:
		return "VERIFY";
	
	case SCSIOP_VERIFY16:
		return "VERIFY16";
	
	case SCSIOP_MEDIUM_REMOVAL:
		return "MEDIUM_REMOVAL";
	
	case SCSIOP_REZERO_UNIT:
		return "REZERO_UNIT/REWIND";
	
	case SCSIOP_REQUEST_BLOCK_ADDR:
		return "REQUEST_BLOCK_ADDR";
	
	case SCSIOP_REQUEST_SENSE:
		return "REQUEST_SENSE";
	
	case SCSIOP_FORMAT_UNIT:
		return "FORMAT_UNIT";
	
	case SCSIOP_READ_BLOCK_LIMITS:
		return "READ_BLOCK_LIMITS";
	
	case SCSIOP_REASSIGN_BLOCKS:
		return "REASSIGN_BLOCKS/INIT_ELEMENT_STATUS";
	
	case SCSIOP_READ6:
		return "READ6/RECEIVE";
	
	case SCSIOP_WRITE6:
		return "WRITE6/PRINT/SEND";
	
	case SCSIOP_SEEK6:
		return "SEEK6/TRACK_SELECT/SLEW_PRINT";
	
	case SCSIOP_SEEK_BLOCK:
		return "SEEK_BLOCK";
	
	case SCSIOP_PARTITION:
		return "PARTITION";
	
	case SCSIOP_READ_REVERSE:
        return "READ_REVERSE";
	
	case SCSIOP_WRITE_FILEMARKS:
		return "WRITE_FILEMARKS/FLUSH_BUFFER";
	
	case SCSIOP_SPACE:
		return "SPACE";
	
	case SCSIOP_VERIFY6:
		return "VERIFY6";
	
	case SCSIOP_RECOVER_BUF_DATA:
		return "RECOVER_BUF_DATA";
	
	case SCSIOP_MODE_SELECT:
		return "MODE_SELECT";
	
	case SCSIOP_RESERVE_UNIT:
        return "RESERVE_UNIT";
	
	case SCSIOP_RESERVE_UNIT10:
		return "RESERVE_UNIT10";
	
	case SCSIOP_COPY:
		return "COPY";
	
	case SCSIOP_ERASE:
		return "ERASE";
	
	case SCSIOP_START_STOP_UNIT:
		return "START_STOP_UNIT/STOP_PRINT/LOAD_UNLOAD";
	
	case SCSIOP_RECEIVE_DIAGNOSTIC:
		return "RECEIVE_DIAGNOSTIC";
	
	case SCSIOP_SEND_DIAGNOSTIC:
		return "SEND_DIAGNOSTIC";
	
	case SCSIOP_READ_FORMATTED_CAPACITY:
		return "READ_FORMATTED_CAPACITY";
	
	case SCSIOP_SEEK:
		return "SEEK10/LOCATE/POSITION_TO_ELEMENT";
	
	case SCSIOP_WRITE_VERIFY:    
		return "WRITE_VERIFY";

	case SCSIOP_SEARCH_DATA_HIGH:
		return "SEARCH_DATA_HIGH";
	
	case SCSIOP_SEARCH_DATA_EQUAL:
		return "SEARCH_DATA_EQUAL";
	
	case SCSIOP_SEARCH_DATA_LOW:
		return "SEARCH_DATA_LOW";
	
	case SCSIOP_SET_LIMITS:
		return "SET_LIMITS";
	
	case SCSIOP_READ_POSITION:
		return "READ_POSITION";
	
	case SCSIOP_SYNCHRONIZE_CACHE:
		return "SYNCHRONIZE_CACHE";
	
	case SCSIOP_SYNCHRONIZE_CACHE16:
		return "SYNCHRONIZE_CACHE16";
	
	case SCSIOP_COMPARE:
		return "COMPARE";
	
	case SCSIOP_COPY_COMPARE:
        return "COPY_COMPARE";
	
	case SCSIOP_WRITE_DATA_BUFF:
		return "WRITE_DATA_BUFF";
	
	case SCSIOP_READ_DATA_BUFF:
		return "READ_DATA_BUFF";
	
	case SCSIOP_CHANGE_DEFINITION:
		return "CHANGE_DEFINITION";
	
	case SCSIOP_READ_SUB_CHANNEL:
		return "READ_SUB_CHANNEL";
	
	case SCSIOP_READ_TOC:
		return "READ_TOC";
	
	case SCSIOP_READ_HEADER:
		return "READ_HEADER";
	
	case SCSIOP_PLAY_AUDIO:
		return "PLAY_AUDIO";
	
	case SCSIOP_PLAY_AUDIO_MSF:
		return "PLAY_AUDIO_MSF";
	
	case SCSIOP_PLAY_TRACK_INDEX:
		return "PLAY_TRACK_INDEX";
	
	case SCSIOP_PLAY_TRACK_RELATIVE:
		return "PLAY_TRACK_RELATIVE";
	
	case SCSIOP_GET_EVENT_STATUS:
		return "GET_EVENT_STATUS";
	
	case SCSIOP_PAUSE_RESUME:
        return "PAUSE_RESUME";
	
	case SCSIOP_LOG_SELECT:
		return "LOG_SELECT";
	
	case SCSIOP_LOG_SENSE:
		return "LOG_SENSE";
	
	case SCSIOP_STOP_PLAY_SCAN:
		return "STOP_PLAY_SCAN";
	
	case SCSIOP_READ_DISK_INFORMATION :
		return "READ_DISK_INFORMATION";
	
	case SCSIOP_READ_TRACK_INFORMATION:
		return "READ_TRACK_INFORMATION";
	
	case SCSIOP_MODE_SELECT10:
		return "MODE_SELECT10";
	
	case SCSIOP_MODE_SENSE10:
        return "MODE_SENSE10";
	
	case SCSIOP_REPORT_LUNS:
		return "REPORT_LUNS";
	
	case SCSIOP_SEND_KEY:
		return "SEND_KEY";
	
	case SCSIOP_REPORT_KEY:
		return "REPORT_KEY";
	
	case SCSIOP_MOVE_MEDIUM:
		return "MOVE_MEDIUM";
	
	case SCSIOP_LOAD_UNLOAD_SLOT:
		return "LOAD_UNLOAD_SLOT or SCSIOP_EXCHANGE_MEDIUM";
	
	case SCSIOP_SET_READ_AHEAD:
		return "SET_READ_AHEAD";
	
	case SCSIOP_READ_DVD_STRUCTURE:
		return "READ_DVD_STRUCTURE";
	
	case SCSIOP_REQUEST_VOL_ELEMENT:
		return "REQUEST_VOL_ELEMENT";
	
	case SCSIOP_SEND_VOLUME_TAG:
		return "SEND_VOLUME_TAG";
	
	case SCSIOP_READ_ELEMENT_STATUS:
		return "READ_ELEMENT_STATUS";
	
	case SCSIOP_READ_CD_MSF         :
		return "READ_CD_MSF";
	
	case SCSIOP_SCAN_CD:
		return "SCAN_CD";
	
	case SCSIOP_PLAY_CD:
		return "PLAY_CD";
	
	case SCSIOP_MECHANISM_STATUS:
		return "MECHANISM_STATUS";
	
	case SCSIOP_READ_CD:
		return "READ_CD";
	
	case SCSIOP_INIT_ELEMENT_RANGE:
		return "INIT_ELEMENT_RANGE";
	
	case SCSIOP_SET_CD_SPEED:
		return "SET_CD_SPEED";
	
	case SCSIOP_READ_BUFFER_CAPACITY:
		return "READ_BUFFER_CAPACITY";
	
	case SCSIOP_GET_CONFIGURATION:
		return "GET_CONFIGURATION";
	
	case SCSIOP_BLANK:
		return "BLANK";
	
	case SCSIOP_GET_PERFORMANCE:
		return "GET_PERFORMANCE";
	
	case SCSIOP_CLOSE_TRACK_SESSION:
		return "CLOSE_TRACK_SESSION";
	
	case SCSIOP_RESERVE_TRACK_RZONE:
		return "RESERVE_TRACK";
	
	case SCSIOP_WRITE12:
		return "WRITE12";
	
	case SCSIOP_READ12:
		return "READ12";
	
	case SCSIOP_SEND_OPC_INFORMATION:
		return "SEND_OPC_INFORMATION";
	
	case SCSIOP_SEND_CUE_SHEET:
		return "SEND_QUE_SHEET";
	
	//	Scsi commands undefined in Windows.
	
	case 0x3E:
		return "READ_LONG";
	
	case 0xD8:
	case 0xF0:
	case 0xF1:
		return "Vendor-specific";

	default:
		return "** Unknown Operation code **";
	}
}

static PCHAR
SrbFunctionCodeString (
	ULONG	Code
	)
{
	switch (Code) {

	case SRB_FUNCTION_EXECUTE_SCSI:
		return "SRB_FUNCTION_EXECUTE_SCSI";

	case SRB_FUNCTION_CLAIM_DEVICE:
		return "SRB_FUNCTION_CLAIM_DEVICE";
	
	case SRB_FUNCTION_IO_CONTROL:
		return "SRB_FUNCTION_IO_CONTROL";
	
	case SRB_FUNCTION_RECEIVE_EVENT:
		return "SRB_FUNCTION_RECEIVE_EVENT";
	
	case SRB_FUNCTION_RELEASE_QUEUE:
		return "SRB_FUNCTION_RELEASE_QUEUE";
	
	case SRB_FUNCTION_ATTACH_DEVICE:
		return "SRB_FUNCTION_ATTACH_DEVICE";
	
	case SRB_FUNCTION_RELEASE_DEVICE:
		return "SRB_FUNCTION_RELEASE_DEVICE";
	
	case SRB_FUNCTION_SHUTDOWN:
		return "SRB_FUNCTION_SHUTDOWN";
	
	case SRB_FUNCTION_FLUSH:
		return "SRB_FUNCTION_FLUSH";
	
	case SRB_FUNCTION_ABORT_COMMAND:
		return "SRB_FUNCTION_ABORT_COMMAND";
	
	case SRB_FUNCTION_RELEASE_RECOVERY:
		return "SRB_FUNCTION_RELEASE_RECOVERY";
	
	case SRB_FUNCTION_RESET_BUS:
		return "SRB_FUNCTION_RESET_BUS";
	
	case SRB_FUNCTION_RESET_DEVICE:
		return "SRB_FUNCTION_RESET_DEVICE";
	
	case SRB_FUNCTION_TERMINATE_IO:
		return "SRB_FUNCTION_TERMINATE_IO";
	
	case SRB_FUNCTION_FLUSH_QUEUE:
		return "SRB_FUNCTION_FLUSH_QUEUE";
	
	case SRB_FUNCTION_REMOVE_DEVICE:
		return "SRB_FUNCTION_REMOVE_DEVICE";
	
	case SRB_FUNCTION_WMI:
		return "SRB_FUNCTION_WMI";
	
	case SRB_FUNCTION_LOCK_QUEUE:
		return "SRB_FUNCTION_LOCK_QUEUE";
	
	case SRB_FUNCTION_UNLOCK_QUEUE:
		return "SRB_FUNCTION_UNLOCK_QUEUE";
	
	default:
		return "**********Unknown Function**********";
	}
}

#endif
