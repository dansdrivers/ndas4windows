#include <ntddk.h>
#include <scsi.h>
#include "winscsiext.h"
#include <stdio.h>
#include <stdarg.h>
#include "KDebug.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "KDebug"

#define DEBUG_BUFFER_LENGTH 256

ULONG	KDebugLevel = 1;
ULONG	KDebugMask = DBG_DEFAULT_MASK;
//ULONG	KDebugMask = 0x88888888; // show error only
UCHAR	MiniBuffer[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	MiniBufferSentinal = NULL;
UCHAR	MiniBufferWithLocation[DEBUG_BUFFER_LENGTH + 1];
PUCHAR	MiniBufferWithLocationSentinal = NULL;

VOID
_LSKDebugPrint(
   IN PCCHAR	DebugMessage,
   ...
){
    va_list ap;

    va_start(ap, DebugMessage);

	_vsnprintf(MiniBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	ASSERTMSG("_KDebugPrint overwrote sentinal byte",
		((MiniBufferSentinal == NULL) || (*MiniBufferSentinal == 0xff)));

	if(MiniBufferSentinal) {
		*MiniBufferSentinal = 0xff;
	}

	DbgPrint(MiniBuffer);

    va_end(ap);
}

PCHAR
_LSKDebugPrintVa(
		IN PCCHAR	DebugMessage,
		...
){
	va_list ap;

	va_start(ap, DebugMessage);

	_vsnprintf(MiniBuffer, DEBUG_BUFFER_LENGTH, DebugMessage, ap);
	ASSERTMSG("_KDebugPrint overwrote sentinal byte",
		((MiniBufferSentinal == NULL) || (*MiniBufferSentinal == 0xff)));

	if(MiniBufferSentinal) {
		*MiniBufferSentinal = 0xff;
	}

	va_end(ap);

	return (PCHAR)MiniBuffer;
}

VOID
_LSKDebugPrintWithLocation(
   IN PCCHAR	DebugMessage,
   PCCHAR		ModuleName,
   UINT32		LineNumber,
   PCCHAR		FunctionName
   )
{
	_snprintf(MiniBufferWithLocation, DEBUG_BUFFER_LENGTH, 
		"[%s:%04d] %s : %s", ModuleName, LineNumber, FunctionName, DebugMessage);

	ASSERTMSG("_KDebugPrintWithLocation overwrote sentinal byte",
		((MiniBufferWithLocationSentinal == NULL) || (*MiniBufferWithLocationSentinal == 0xff)));

	if(MiniBufferWithLocationSentinal) {
		*MiniBufferWithLocationSentinal = 0xff;
	}

	DbgPrint(MiniBufferWithLocation);
}

PCHAR
CdbOperationString(
	UCHAR	Code
	)
{
	switch(Code) {
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
	case 0x57:
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
	case 0x56:
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
	//
	//	Scsi commands undefined in Windows.
	//
	case 0xBB:
		return "REDUNDANCY_GROUP (OUT)";
	case 0x3E:
		return "READ_LONG";
	case 0x5C:
		return "READ_BUFFER_CAPACITY";
	case 0xD8:
	case 0xF0:
	case 0xF1:
		return "Vendor-specific";
	case 0x46:
		return "GET_CONFIGURATION";
	case 0xA1:
		return "BLANK";
	case 0xAC:
		return "GET_PERFORMANCE";
	case 0x5B:
		return "CLOSE_TRACK_SESSION";
	case 0x53:
		return "RESERVE_TRACK";
	case 0xAa:
		return "WRITE12";
	case 0xA8:
		return "READ12";

	default:
		return "** Unknown Operation code **";
	}
}
PCHAR
SrbFunctionCodeString(
	ULONG	Code
	)
{
	switch(Code) {
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


