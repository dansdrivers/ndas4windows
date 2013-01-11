#include <pch.cxx>

#define _UXIXFS_MEMBER_
#include "message.hxx"
#include "rtmsg.h"
#include "wstring.hxx"


DEFINE_CONSTRUCTOR( XIXFS_VOL, VOL_LIODPDRV );

XIXFS_VOL::~XIXFS_VOL(
    )
{
    Destroy();
}


VOID
XIXFS_VOL::Construct ()
{
    // unreferenced parameters
    (void)(this);
}

VOID
XIXFS_VOL::Destroy()
{
}



FORMAT_ERROR_CODE
XIXFS_VOL::Initialize(
		IN      PCWSTRING   NtDriveName,
		IN OUT  PMESSAGE    Message
		)
{
    MESSAGE             msg;
    FORMAT_ERROR_CODE   errcode;


	Destroy();
	
	DebugPrint( "XIXFS_VOL::Initialize 1!");

	errcode = VOL_LIODPDRV::Initialize(NtDriveName, &_xixfs_sa, Message, TRUE, FALSE);

	DebugPrint( "XIXFS_VOL::Initialize 2!");
	if(errcode != NoError)	{
		Destroy();
		return errcode;
	}

	DebugPrint( "XIXFS_VOL::Initialize 3!");
	if(!Message) {
		Message = &msg;
	}


	if(! _xixfs_sa.Initialize(this, Message))	{
		Destroy();
		Message->Set(MSG_CHK_NO_MEMORY);
		Message->Display("");
        return GeneralError;
	}
	DebugPrint( "XIXFS_VOL::Initialize 4!");
	
	return NoError;
}








PVOL_LIODPDRV
XIXFS_VOL::QueryDupVolume(
    IN      PCWSTRING   NtDriveName,
    IN OUT  PMESSAGE    Message,
    IN      BOOLEAN     ExclusiveWrite,
    IN      BOOLEAN     FormatMedia,
    IN      MEDIA_TYPE  MediaType
    ) CONST
{

	PXIXFS_VOL	vol;

	(void)(this);

	if(	!(vol = NEW XIXFS_VOL)	) {
		Message ? Message->DisplayMsg(MSG_FMT_NO_MEMORY) : 1;
		return NULL;
	}

	if(	!vol->Initialize(NtDriveName, Message))	{
		DELETE(vol);
		return NULL;
	}


	return vol;
}