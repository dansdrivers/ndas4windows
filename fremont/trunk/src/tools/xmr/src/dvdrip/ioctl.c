//#include <windows.h>

#include <stdio.h>

#include <string.h>                                    /* memcpy(), memset() */
#include <sys/types.h>

#include <windows.h>
#include <winioctl.h>

#include "common.h"
#include "ioctl.h"

/*****************************************************************************
 * Local prototypes, win32 (aspi) specific
 *****************************************************************************/
static void WinInitSPTD ( SCSI_PASS_THROUGH_DIRECT *, int );
static void WinInitSSC  ( struct SRB_ExecSCSICmd *, int );
static int  WinSendSSC  ( int, struct SRB_ExecSCSICmd * );

/*****************************************************************************
 * ioctl_ReadCopyright: check whether the disc is encrypted or not
 *****************************************************************************/
int ioctl_ReadCopyright( int i_fd, int i_layer, int *pi_copyright )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        INIT_SPTD( GPCMD_READ_DVD_STRUCTURE, 8 );

        /*  When using IOCTL_DVD_READ_STRUCTURE and
            DVD_COPYRIGHT_DESCRIPTOR, CopyrightProtectionType
            seems to be always 6 ???
            To work around this MS bug we try to send a raw scsi command
            instead (if we've got enough privileges to do so). */

        sptd.Cdb[ 6 ] = i_layer;
        sptd.Cdb[ 7 ] = DVD_STRUCT_COPYRIGHT;

        i_ret = SEND_SPTD( i_fd, &sptd, &tmp );

        if( i_ret == 0 )
        {
            *pi_copyright = p_buffer[ 4 ];
        }
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, 8 );

        ssc.CDBByte[ 6 ] = i_layer;
        ssc.CDBByte[ 7 ] = DVD_STRUCT_COPYRIGHT;

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_copyright = p_buffer[ 4 ];
    }
	return i_ret;
}

/*****************************************************************************
 * ioctl_ReadDiscKey: get the disc key
 *****************************************************************************/
int ioctl_ReadDiscKey( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_DISK_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_DISK_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdDiskKey;
        key->KeyFlags   = 0;

 
        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;


        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_key, key->KeyData, DVD_DISCKEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_READ_DVD_STRUCTURE, DVD_DISCKEY_SIZE + 4 );

        ssc.CDBByte[ 7 ]  = DVD_STRUCT_DISCKEY;
        ssc.CDBByte[ 10 ] = *pi_agid << 6;

        i_ret = WinSendSSC( i_fd, &ssc );

        if( i_ret < 0 )
        {
			fprintf(stderr," tt IOCTL_DVD_READ_KEY i_ret fail\n");
            return i_ret;
        }

        memcpy( p_key, p_buffer + 4, DVD_DISCKEY_SIZE );
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReadTitleKey: get the title key
 *****************************************************************************/
int ioctl_ReadTitleKey( int i_fd, int *pi_agid, int i_pos, uint8_t *p_key )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_TITLE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_TITLE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdTitleKey;
        key->KeyFlags   = 0;
        key->Parameters.TitleOffset.QuadPart = (LONGLONG) i_pos *
                                                   2048 /*DVDCSS_BLOCK_SIZE*/;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        memcpy( p_key, key->KeyData, DVD_KEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 12 );

        ssc.CDBByte[ 2 ] = ( i_pos >> 24 ) & 0xff;
        ssc.CDBByte[ 3 ] = ( i_pos >> 16 ) & 0xff;
        ssc.CDBByte[ 4 ] = ( i_pos >>  8 ) & 0xff;
        ssc.CDBByte[ 5 ] = ( i_pos       ) & 0xff;
        ssc.CDBByte[ 10 ] = DVD_REPORT_TITLE_KEY | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_key, p_buffer + 5, DVD_KEY_SIZE );
    }
    return i_ret;
}


/*****************************************************************************
 * ioctl_ReportAgid: get AGID from the drive
 *****************************************************************************/
int ioctl_ReportAgid( int i_fd, int *pi_agid )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        ULONG id;
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_START_SESSION,
                        &tmp, 4, &id, sizeof( id ), &tmp, NULL ) ? 0 : -1;

        *pi_agid = id;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_agid = p_buffer[ 7 ] >> 6;
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportChallenge: get challenge from the drive
 *****************************************************************************/
int ioctl_ReportChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        memcpy( p_challenge, key->KeyData, DVD_CHALLENGE_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_CHALLENGE | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_challenge, p_buffer + 4, DVD_CHALLENGE_SIZE );
    }
    return i_ret;
}


/*****************************************************************************
 * ioctl_ReportASF: get ASF from the drive
 *****************************************************************************/
int ioctl_ReportASF( int i_fd, int *pi_remove_me, int *pi_asf )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_ASF_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_ASF_LENGTH;
        key->KeyType    = DvdAsf;
        key->KeyFlags   = 0;

        ((PDVD_ASF)key->KeyData)->SuccessFlag = *pi_asf;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        *pi_asf = ((PDVD_ASF)key->KeyData)->SuccessFlag;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_ASF;

        i_ret = WinSendSSC( i_fd, &ssc );

        *pi_asf = p_buffer[ 7 ] & 1;
    }

    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportKey1: get the first key from the drive
 *****************************************************************************/
int ioctl_ReportKey1( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey1;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        memcpy( p_key, key->KeyData, DVD_KEY_SIZE );
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_KEY1 | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );

        memcpy( p_key, p_buffer + 4, DVD_KEY_SIZE );
    }
    return i_ret;
}


/*****************************************************************************
 * ioctl_InvalidateAgid: invalidate the current AGID
 *****************************************************************************/
int ioctl_InvalidateAgid( int i_fd, int *pi_agid )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_END_SESSION,
                    pi_agid, sizeof( *pi_agid ), NULL, 0, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
#if defined( __MINGW32__ )
        INIT_SSC( GPCMD_REPORT_KEY, 0 );
#else
        INIT_SSC( GPCMD_REPORT_KEY, 1 );

        ssc.SRB_BufLen    = 0;
        ssc.CDBByte[ 8 ]  = 0;
        ssc.CDBByte[ 9 ]  = 0;
#endif

        ssc.CDBByte[ 10 ] = DVD_INVALIDATE_AGID | (*pi_agid << 6);

        i_ret = WinSendSSC( i_fd, &ssc );
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendChallenge: send challenge to the drive
 *****************************************************************************/
int ioctl_SendChallenge( int i_fd, int *pi_agid, uint8_t *p_challenge )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_CHALLENGE_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_CHALLENGE_KEY_LENGTH;
		
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdChallengeKey;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_challenge, DVD_CHALLENGE_SIZE );

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key,
                 key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 16 );

        ssc.CDBByte[ 10 ] = DVD_SEND_CHALLENGE | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xe;
        memcpy( p_buffer + 4, p_challenge, DVD_CHALLENGE_SIZE );

        i_ret = WinSendSSC( i_fd, &ssc );
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendKey2: send the second key to the drive
 *****************************************************************************/
int ioctl_SendKey2( int i_fd, int *pi_agid, uint8_t *p_key )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_BUS_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_BUS_KEY_LENGTH;
        key->SessionId  = *pi_agid;
        key->KeyType    = DvdBusKey2;
        key->KeyFlags   = 0;

        memcpy( key->KeyData, p_key, DVD_KEY_SIZE );

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_SEND_KEY, key,
                 key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 12 );

        ssc.CDBByte[ 10 ] = DVD_SEND_KEY2 | (*pi_agid << 6);

        p_buffer[ 1 ] = 0xa;
        memcpy( p_buffer + 4, p_key, DVD_KEY_SIZE );

        i_ret = WinSendSSC( i_fd, &ssc );
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_ReportRPC: get RPC status for the drive
 *****************************************************************************/
int ioctl_ReportRPC( int i_fd, int *p_type, int *p_mask, int *p_scheme )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        DWORD tmp;
        uint8_t buffer[DVD_RPC_KEY_LENGTH];
        PDVD_COPY_PROTECT_KEY key = (PDVD_COPY_PROTECT_KEY) &buffer;

        memset( &buffer, 0, sizeof( buffer ) );

        key->KeyLength  = DVD_RPC_KEY_LENGTH;
        key->KeyType    = DvdGetRpcKey;
        key->KeyFlags   = 0;

        i_ret = DeviceIoControl( (HANDLE) i_fd, IOCTL_DVD_READ_KEY, key,
                key->KeyLength, key, key->KeyLength, &tmp, NULL ) ? 0 : -1;

        if( i_ret < 0 )
        {
            return i_ret;
        }

        *p_type = ((PDVD_RPC_KEY)key->KeyData)->TypeCode;
        *p_mask = ((PDVD_RPC_KEY)key->KeyData)->RegionMask;
        *p_scheme = ((PDVD_RPC_KEY)key->KeyData)->RpcScheme;
    }
    else
    {
        INIT_SSC( GPCMD_REPORT_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_REPORT_RPC;

        i_ret = WinSendSSC( i_fd, &ssc );

        *p_type = p_buffer[ 4 ] >> 6;
        *p_mask = p_buffer[ 5 ];
        *p_scheme = p_buffer[ 6 ];
    }
    return i_ret;
}

/*****************************************************************************
 * ioctl_SendRPC: set RPC status for the drive
 *****************************************************************************/
int ioctl_SendRPC( int i_fd, int i_pdrc )
{
    int i_ret;
    if( WIN2K ) /* NT/2k/XP */
    {
        INIT_SPTD( GPCMD_SEND_KEY, 8 );

        sptd.Cdb[ 10 ] = DVD_SEND_RPC;

        p_buffer[ 1 ] = 6;
        p_buffer[ 4 ] = i_pdrc;

        i_ret = SEND_SPTD( i_fd, &sptd, &tmp );
    }
    else
    {
        INIT_SSC( GPCMD_SEND_KEY, 8 );

        ssc.CDBByte[ 10 ] = DVD_SEND_RPC;

        p_buffer[ 1 ] = 6;
        p_buffer[ 4 ] = i_pdrc;

        i_ret = WinSendSSC( i_fd, &ssc );
    }
    return i_ret;
}


/* Local prototypes */
/*****************************************************************************
 * WinInitSPTD: initialize a sptd structure
 *****************************************************************************
 * This function initializes a SCSI pass through command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void WinInitSPTD( SCSI_PASS_THROUGH_DIRECT *p_sptd, int i_type )
{
    memset( p_sptd->DataBuffer, 0, p_sptd->DataTransferLength );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_sptd->DataIn = SCSI_IOCTL_DATA_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_sptd->DataIn = SCSI_IOCTL_DATA_IN;
            break;
    }

    p_sptd->Cdb[ 0 ] = i_type;
    p_sptd->Cdb[ 8 ] = (uint8_t)(p_sptd->DataTransferLength >> 8) & 0xff;
    p_sptd->Cdb[ 9 ] = (uint8_t) p_sptd->DataTransferLength       & 0xff;
    p_sptd->CdbLength = 12;

    p_sptd->TimeOutValue = 2;
}

/*****************************************************************************
 * WinInitSSC: initialize a ssc structure for the win32 aspi layer
 *****************************************************************************
 * This function initializes a ssc raw device command structure for future
 * use, either a read command or a write command.
 *****************************************************************************/
static void WinInitSSC( struct SRB_ExecSCSICmd *p_ssc, int i_type )
{
    memset( p_ssc->SRB_BufPointer, 0, p_ssc->SRB_BufLen );

    switch( i_type )
    {
        case GPCMD_SEND_KEY:
            p_ssc->SRB_Flags = SRB_DIR_OUT;
            break;

        case GPCMD_READ_DVD_STRUCTURE:
        case GPCMD_REPORT_KEY:
            p_ssc->SRB_Flags = SRB_DIR_IN;
            break;
    }

    p_ssc->SRB_Cmd      = SC_EXEC_SCSI_CMD;
    p_ssc->SRB_Flags    |= SRB_EVENT_NOTIFY;

    p_ssc->CDBByte[ 0 ] = i_type;

    p_ssc->CDBByte[ 8 ] = (uint8_t)(p_ssc->SRB_BufLen >> 8) & 0xff;
    p_ssc->CDBByte[ 9 ] = (uint8_t) p_ssc->SRB_BufLen       & 0xff;
    p_ssc->SRB_CDBLen   = 12;

    p_ssc->SRB_SenseLen = SENSE_LEN;
}

/*****************************************************************************
 * WinSendSSC: send a ssc structure to the aspi layer
 *****************************************************************************/
static int WinSendSSC( int i_fd, struct SRB_ExecSCSICmd *p_ssc )
{
    HANDLE hEvent = NULL;
    struct w32_aspidev *fd = (struct w32_aspidev *) i_fd;

    hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    if( hEvent == NULL )
    {
        return -1;
    }

    p_ssc->SRB_PostProc  = (unsigned long *)hEvent;
    p_ssc->SRB_HaId      = LOBYTE( fd->i_sid );
    p_ssc->SRB_Target    = HIBYTE( fd->i_sid );

    ResetEvent( hEvent );

    if( fd->lpSendCommand( (void*) p_ssc ) == SS_PENDING )
        WaitForSingleObject( hEvent, INFINITE );

    CloseHandle( hEvent );

    return p_ssc->SRB_Status == SS_COMP ? 0 : -1;
}