#ifndef _NDASCOMM_TYPE_H_
#define _NDASCOMM_TYPE_H_

#pragma once
#include <windows.h>
#include <basetsd.h>

/* 8 byte alignment (in case of no serialization) */
#include <pshpack8.h>

#ifdef STRICT
typedef struct _NDASCOMM_HANDLE *HNDAS;
#else
typedef PVOID HNDAS;
#endif

/* constants */

enum {
	NDASCOMM_SEND_TIMEOUT_DEFAULT = 5000,
	NDASCOMM_RECEIVE_TIMEOUT_DEFAULT = 20000,
};

//
// obsolete macros
// USE NDASCOMM_CIT_XXXX instead
//
// #define NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX			0 /* 6 bytes */
// #define NDASCOMM_CONNECTION_INFO_TYPE_ADDR_IP			3 /* 4 bytes, not supported yet */
// #define NDASCOMM_CONNECTION_INFO_TYPE_ID_DEVICE			4 /* 6 bytes */
// #define NDASCOMM_CONNECTION_INFO_TYPE_DEVICE_ID			4 /* 6 bytes */
// #define NDASCOMM_CONNECTION_INFO_TYPE_ID_A				2 /* (20 + 5) * sizeof(CHAR) */
// #define NDASCOMM_CONNECTION_INFO_TYPE_ID_W				1 /* (20 + 5) * sizeof(WCHAR) */
//#ifdef UNICODE
//#define NDASCOMM_CONNECTION_INFO_TYPE_STRING_ID NDASCOMM_CONNECTION_INFO_TYPE_ID_W
//#else
//#define NDASCOMM_CONNECTION_INFO_TYPE_STRING_ID NDASCOMM_CONNECTION_INFO_TYPE_ID_A
//#endif

typedef enum _NDASCOMM_CONNECTION_INFO_TYPE {
	NDASCOMM_CIT_UNSPECIFIED = 0x00,
	NDASCOMM_CIT_NDAS_IDW    = 0x01,
	NDASCOMM_CIT_NDAS_IDA    = 0x02,
	NDASCOMM_CIT_DEVICE_ID   = 0x04,
	/* reserved for future use */
	NDASCOMM_CIT_SA_LPX      = 0x10,
	NDASCOMM_CIT_SA_IN       = 0x11
} NDASCOMM_CONNECTION_INFO_TYPE, *PNDASCOMM_CONNECTION_INFO_TYPE;

#ifdef UNICODE
#define NDASCOMM_CIT_NDAS_ID NDASCOMM_CIT_NDAS_IDW
#else
#define NDASCOMM_CIT_NDAS_ID NDASCOMM_CIT_NDAS_IDA
#endif

typedef enum _NDASCOMM_LOGIN_TYPE {
	NDASCOMM_LOGIN_TYPE_NORMAL     = 0x00,
	NDASCOMM_LOGIN_TYPE_DISCOVER   = 0xFF
} NDASCOMM_LOGIN_TYPE, *PNDASCOMM_LOGIN_TYPE;

typedef enum _NDASCOMM_TRANSPORT_TYPE {
	NDASCOMM_TRANSPORT_UNSPECIFIED = 0x00,
	NDASCOMM_TRANSPORT_LPX         = 0x01,
	NDASCOMM_TRANSPORT_IP          = 0x02 /* Not available */
} NDASCOMM_TRANSPORT_TYPE, *PNDASCOMM_TRANSPORT_TYPE;

typedef enum _NDASCOMM_CONNECTION_FLAG {
	NDASCOMM_CNF_ENABLE_SHARED_WRITE             = 0x00000001,
	NDASCOMM_CNF_ENABLE_DEFAULT_PRIVILEGED_MODE  = 0x00000002,
	NDASCOMM_CNF_ENFORCE_LOCKED_WRITE            = 0x00000010,
	NDASCOMM_CNF_ENFORCE_LOCKED_READ             = 0x00000020,
	NDASCOMM_CNF_ENFORCE_LOCKED_OTHER            = 0x00000040,
	NDASCOMM_CNF_ENFORCE_LOCKED_ALL = (
		NDASCOMM_CNF_ENFORCE_LOCKED_WRITE | 
		NDASCOMM_CNF_ENFORCE_LOCKED_READ | 
		NDASCOMM_CNF_ENFORCE_LOCKED_OTHER),
	NDASCOMM_CNF_DISABLE_LOCK_CLEANUP_ON_CONNECT = 0x00000080,
} NDASCOMM_CONNECTION_FLAG, *PNDASCOMM_CONNECTION_FLAG;

/* Device Port should be converted to network endian */
#define	NDAS_DEVICE_DEFAULT_PORT 10000

typedef struct _NDAS_IDA {
	CHAR Id[20 + 1];
	CHAR Reserved[3];
	CHAR Key[5 + 1];
	CHAR Reserved2[2];
} NDAS_IDA, *PNDAS_IDA;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(32 == sizeof(NDAS_IDA));
/*DOM-IGNORE-END*/

typedef struct _NDAS_IDW {
	WCHAR Id[20 + 1];
	WCHAR Reserved[3];
	WCHAR Key[5 + 1];
	WCHAR align2[2];
} NDAS_IDW, *PNDAS_IDW;

/*DOM-IGNORE-BEGIN*/
C_ASSERT(64 == sizeof(NDAS_IDW));
/*DOM-IGNORE-END*/

#ifdef UNICODE
#define NDAS_ID NDAS_IDW
#else
#define NDAS_ID NDAS_IDA
#endif

/* <TITLE NDASCOMM_CONNECTION_INFO>
   
   Summary
   Specifies the attributes of connection information. This
   structure is used to create connection to a NDAS Device.
   
   Description
   address_type member can have one of the following values.
   <TABLE>
   Value                                     Meaning
   ----------------------------------------  --------------------------------------
   NDASCOMM_CIT_DEVICE_ID                    Use DeviceId field.
   NDASCOMM_CIT_NDAS_ID                      Use NdasId field. 
   NDASCOMM_CIT_SA_LPX                       Use SaLpx field.
                                              (Reserved for the future use)
   NDASCOMM_CIT_SA_IN                        Use SaIn field.
                                              (Reserved for the future use)
   </TABLE>
   
   
   login_type member can have one of the following values.
   <TABLE>
   Value                          Meaning
   -----------------------------  ---------------------------------------------
   NDASCOMM_LOGIN_TYPE_NORMAL     Connects to the NDAS Device to operate IDE
                                   Commands. Use this value only.
   NDASCOMM_LOGIN_TYPE_DISCOVER   Internal use only. There is no exported API
                                   function that supports discover login type.
   </TABLE>
   
   
   protocol member can have one of the following values.
   <TABLE>
   Value                    Meaning
   -----------------------  ----------------------------
   NDASCOMM_TRANSPORT_LPX   Use LPX transport protocol.
   NDASCOMM_TRANSPORT_IP    Not supported.
   </TABLE>
   See Also
   NdasCommConnect, NdasCommGetUnitDeviceStat                                       */
typedef struct _NDASCOMM_CONNECTION_INFO
{
	/* [in] Size of this data structure, in bytes. 
	   Set this member to sizeof(NDASCOMM_CONNECTION_INFO) 
	   before calling the function. */
	DWORD Size;
	/* [in] Value specifying which login type will be used to make
	   connection to a NDAS Device. See description for available
	   values. */
	DWORD LoginType;
	/* [in] Value specifying which unit of the NDAS Device will be
	   used. Currently there is only 1 unit attached to a NDAS
	   Device. Set this value to 0. */
	DWORD UnitNo;
	/* [in] If true, connects to the NDAS Device with read-write
	   mode. Otherwise, connects to the NDAS Device with read only
	   mode. */
	BOOL WriteAccess;
	/* [in] Optional Flags for the connection.
	   NDASCOMM_CNF_USE_SHARED_WRITE
       NDASCOMM_CNF_ENFORCE_LOCKED_WRITE
	   NDASCOMM_CNF_ENFORCE_LOCKED_READ
	   NDASCOMM_CNF_ENFORCE_LOCKED_OTHER
	   NDASCOMM_CNF_DONT_LOCK_CLEANUP_ON_CONNECT

	   If NDASCOMM_CNF_USE_SHARED_WRITE is specified,
	   Key field of the NDAS_ID should contain a valid write key,
	   even if WriteAccess is zero.
	   */
	DWORD Flags;
	/* [in] If this value is not null, NdasCommConnect function will
	   login as supervisor. You must set ui64OEMCode also if you
	   don't use default password.                                 */	   
	NDAS_OEM_CODE PrivilegedOEMCode;
	/* [in] Value to verify host as valid user. This value is used
	   as password. If UI64Value is 0 or Bytes fields are all filled with zero, 
	   API function tries with default code for normal user. To login as super user, you
	   must specify this value. NdasCommVendorCommand needs PrivilegedOEMCode
	   user privilege for some commands.                           */
	NDAS_OEM_CODE OEMCode;
	/* [in] Value specifying which protocol will be used to make a
	   connection to the NDAS Device. See description for available
	   values.                                                      */
	DWORD Protocol;
	/* [in] Value specifying which address type will be used to make
	connection to a NDAS Device. See description for available
	values. */
	DWORD AddressType;
	union	{
		/* Used when AddressType is NDASCOMM_CIT_DEVICE_ID */
		NDAS_DEVICE_ID DeviceId;
		/* Used when AddressType is NDASCOMM_CIT_NDAS_ID */
		NDAS_ID        NdasId;
		/* Used when AddressType is NDASCOMM_CIT_NDAS_IDA */
		NDAS_IDA       NdasIdA;
		/* Used when AddressType is NDASCOMM_CIT_NDAS_IDW */
		NDAS_IDW       NdasIdW;
#ifdef _SOCKET_LPX_H_
		/* Used when AddressType is NDASCOMM_CIT_SA_LPX */
		SOCKADDR_LPX   SaLpx;
#endif
#ifdef _WINSOCK2API_
		/* Used when AddressType is NDASCOMM_CIT_SA_IN */
		SOCKADDR_IN    SaIn;
#endif
	} Address;
	/* Initial timeout value for send. 
	   If zero, the default value is used.*/
	DWORD SendTimeout;
	/* Initial timeout value for receive. 
	   If zero, the default value is used. */
	DWORD ReceiveTimeout;
	/* Socket Address List to specify the binding address list,
	   in the order where higher priority addresses come first.
	   If zero (NULL), all available local addresses are used
	   in the order of the appearance in the list 
	   retrieved from the operating system. */
	LPVOID BindingSocketAddressList;
} NDASCOMM_CONNECTION_INFO, *PNDASCOMM_CONNECTION_INFO;

///* <TITLE NDASCOMM_UNIT_DEVICE_INFO>
//   
//   Summary
//   Receives the static attributes of unit device attached to the
//   NDAS Device. All attributes are obtained by WIN_IDENTIFY or
//   WIN_PIDENTIFY IDE command. Using NdasCommIdeCommand, same
//   information can be obtained also.
//   
//   Description
//   MediaType member can have one of the following values.
//   
//   
//   <TABLE>
//   Value                                             Meaning
//   ------------------------------------------------  ------------------------
//   NDAS_UNITDEVICE_MEDIA_TYPE_UNKNOWN_DEVICE         Unknown(not supported)
//   NDAS_UNITDEVICE_MEDIA_TYPE_BLOCK_DEVICE           Non-packet mass-storage
//                                                      device (HDD)
//   NDAS_UNITDEVICE_MEDIA_TYPE_COMPACT_BLOCK_DEVICE   Non-packet compact
//                                                      storage device (Flash
//                                                      card)
//   NDAS_UNITDEVICE_MEDIA_TYPE_CDROM_DEVICE           device (CD/DVD)
//    CD-ROM                                            
//   NDAS_UNITDEVICE_MEDIA_TYPE_OPMEM_DEVICE           Optical memory device
//                                                      (MO)
//   </TABLE>
//   See Also
//   NdasCommGetUnitDeviceInfo, NdasCommIdeCommand                              */

/* <TITLE NDASCOMM_IDE_REGISTER>
   
   Summary
   Specifies and receives IDE command register information to
   the unit device attached the NDAS Device. Fill registers
   according to command member. reg, device and command members
   are bi-directional. So, those will be filled by return
   register informations.
   
   Description
   <B>[Members]
   
   use_dma</B>
   
   [in] 1 : DMA, 0 : PIO
   
   <B>use_48</B>
   
   [in] 1 : use 48 bit address feature, 0 : does not use. Setting LBA 48 mode
   is required for some IDE commands.
   
   <B>device</B>
   
   [in, out] Device register
   
   <B>command</B>
   
   [in, out] Command(in), Status(out) register
   
   <B>command.command</B>
   
   [in] Command register
   
   <B>command.status</B>
   
   [out] Status register
   
   <B>reg</B>
   
   [in, out] The reg member is union of 5 types of structure.
   Select structure by purpose.
   
   <B>reg.basic, reg.named</B>
   
   [in, out] Used when LBA 48 mode is not set.
   
   <B>reg.basic_48, reg.named_48</B>
   
   [in, out] Used when LBA 48 mode is set. The NDAS Device
   writes prev member first, cur member next to the unit device.
   
   <B>reg.ret.err.err_op</B>
   
   [out] Valid if command.status.err is set.
   
   
   
   See Also
   NdasCommIdeCommand                                            */
typedef struct _NDASCOMM_IDE_REGISTER {

#if 0
	UINT8 use_dma;
	UINT8 use_48;
#else
	UINT8 reserved1;
	UINT8 reserved2;
#endif

	union {
		UINT8 device;
		struct {
			UINT8 lba_head_nr : 4; /* reserved(used as 4 MSBs) */
			UINT8 dev         : 1; /* set with target id */
			UINT8 obs1        : 1; /* obsolete */
			UINT8 lba         : 1; /* 1 : use lba */
			UINT8 obs2        : 1; /* obsolete */
		};
	} device;

	union {
		UINT8  command;
		struct {
			UINT8 err         : 1; /* ERR (only valid  bit) */
			UINT8 obs1        : 1; /* obsolete */
			UINT8 obs2        : 1; /* obsolete */
			UINT8 drq         : 1; /* DRQ */
			UINT8 obs         : 1; /* obsolete */
			UINT8 df          : 1; /* DF */
			UINT8 drdy        : 1; /* DRDY */
			UINT8 bsy         : 1; /* BSY */
		} status;
	} command;

	union {
		struct {
			UINT8 reg[5];
		} basic;

		struct {
			UINT8  features;
			UINT8  sector_count;
			UINT8  lba_low;
			UINT8  lba_mid;
			UINT8  lba_high;
		} named;

		struct {
			UINT8 reg_prev[5];
			UINT8 reg_cur[5];
		} basic_48;

		struct {
			struct {
				UINT8  features;
				UINT8  sector_count;
				UINT8  lba_low;
				UINT8  lba_mid;
				UINT8  lba_high;
			} prev;

			struct {
				UINT8  features;
				UINT8  sector_count;
				UINT8  lba_low;
				UINT8  lba_mid;
				UINT8  lba_high;
			} cur;
		} named_48;

		struct {
			UINT8 obs1[5];
			union{
				UINT8  err_na;
				struct {
					UINT8  obs    : 1;
					UINT8  nm     : 1;
					UINT8  abrt   : 1;
					UINT8  mcr    : 1;
					UINT8  idnf   : 1;
					UINT8  mc     : 1;
					UINT8  wp     : 1;
					UINT8  icrc   : 1;
				} err_op;
			} err;
			UINT8 obs2[4];
		} ret;

	} reg;

} NDASCOMM_IDE_REGISTER, *PNDASCOMM_IDE_REGISTER;

/* <TITLE NDASCOMM_BIN_PARAM_TARGET_LIST_ELEMENT>
   
   Binary type parameter for text request command. Not used. Use
   NdasCommGetUnitDeviceStat function to retrieve the
   information instead.
   
   
   
   See Also
   NdasCommGetUnitDeviceStat                                     */

typedef	struct _NDASCOMM_BIN_PARAM_TARGET_LIST_ELEMENT {
	UINT32	TargetID;
	UINT8	NRRWHost;
	UINT8	NRROHost;
	UINT16	Reserved1;
	UINT32	TargetData0;
	UINT32	TargetData1;
} NDASCOMM_BIN_PARAM_TARGET_LIST_ELEMENT, *PNDASCOMM_BIN_PARAM_TARGET_LIST_ELEMENT;

/* <TITLE NDASCOMM_BIN_PARAM_TARGET_LIST>
   
   Binary type parameter for text request command. Not used. Use
   NdasCommGetUnitDeviceStat function to retrieve the
   information instead.
   
   
   
   See Also
   NdasCommGetUnitDeviceStat                                     */
typedef struct _NDASCOMM_BIN_PARAM_TARGET_LIST {
	UINT8	ParamType;
	UINT8	NRTarget;
	UINT16	Reserved1;
	NDASCOMM_BIN_PARAM_TARGET_LIST_ELEMENT	PerTarget[2];
} NDASCOMM_BIN_PARAM_TARGET_LIST, *PNDASCOMM_BIN_PARAM_TARGET_LIST;

/* <TITLE NDASCOMM_BIN_PARAM_TARGET_DATA>
   
   Binary type parameter for text request command. Not used. Use
   NdasCommGetUnitDeviceStat function to retrieve the
   information instead.
   
   
   
   See Also
   NdasCommGetUnitDeviceStat                                     */
typedef struct _NDASCOMM_BIN_PARAM_TARGET_DATA {
	UINT8	ParamType;
	UINT8	GetOrSet;
	UINT16	Reserved1;
	UINT32	TargetID;
	UINT64	TargetData;
} NDASCOMM_BIN_PARAM_TARGET_DATA, *PNDASCOMM_BIN_PARAM_TARGET_DATA;

/* <TITLE NDASCOMM_VCMD_COMMAND>
   
   Specifies the vendor command type parameter for
   NdasCommVendorCommand
   
   
   <TABLE>
   Value                                     Meaning                                      Permission   Min Ver
   ----------------------------------------  -------------------------------------------  -----------  --------
   ndascomm_vcmd_set_ret_time                Sets retransmission time delay when the      supervisor   1.0
                                              transmission from the NDAS Device to the                  
                                              host has failed.                                          
   ndascomm_vcmd_set_max_conn_time           Sets maximum connection time. If no packet   supervisor   1.0
                                              was sent to the NDAS Device for the time,                 
                                              connection would be closed.                               
   ndascomm_vcmd_set_supervisor_pw           Sets 8 bytes of supervisor password of the   supervisor   1.0
                                              NDAS Device.                                              
   ndascomm_vcmd_set_user_pw                 Sets 8 bytes of normal user password of      supervisor   1.0
                                              the NDAS Device.                                          
   ndascomm_vcmd_set_enc_opt                 Sets header and data encrypt option for      supervisor   1.1
                                              the NDAS Device.                                          
   ndascomm_vcmd_set_standby_timer           Enables/disables standby timer and set       supervisor   1.1
                                              timer value for the NDAS Device. the NDAS                 
                                              Device sends WIN_STANDBY1 ATA command to                  
                                              the attached unit device(s).                              
   ndascomm_vcmd_reset                       Resets the NDAS Device. The NDAS Device      supervisor   1.0
                                              will reboot as soon as the command is                     
                                              accepted.                                                 
   ndascomm_vcmd_set_lpx_address             not implemented                                           1.1
   ndascomm_vcmd_set_sema                    Sets a selected semaphore of the NDAS        user         1.1
                                              Device and retrieves auto-increasing                      
                                              semaphore counter.                                        
   ndascomm_vcmd_free_sema                   Frees selected semaphore of the NDAS         user         1.1
                                              Device.                                                   
   ndascomm_vcmd_get_sema                    Retrieves a selected semaphore counter of    user         1.1
                                              the NDAS Device.                                          
   ndascomm_vcmd_get_owner_sema              Retrieves LPX address of the host which      user         1.1
                                              locked selected semaphore of the NDAS                     
                                              Device.                                                   
   ndascomm_vcmd_set_delay                   Sets the maximum delay time between each     user         2.0
                                              packet from the NDAS Device to host.                      
   ndascomm_vcmd_get_delay                   Retrieves the maximum delay time between     user         2.0
                                              each packet from the NDAS Device to host.                 
   ndascomm_vcmd_set_dynamic_max_conn_time   \obsolete                                                 
   ndascomm_vcmd_set_dynamic_ret_time        Sets retransmission time delay when the      user         1.1
                                              transmission from the NDAS Device to the                  
                                              host has failed. This value is not saved                  
                                              to the NDAS Device and applied to the                     
                                              connection only.                                          
   ndascomm_vcmd_get_dynamic_ret_time        Retrieves retransmission time delay when     user         1.1
                                              the transmission from the NDAS Device to                  
                                              the host has failed. This value is not                    
                                              saved to the NDAS Device and applied to                   
                                              the connection only.                                      
   ndascomm_vcmd_set_d_enc_opt               not implemented                                           
   ndascomm_vcmd_get_d_enc_opt               not implemented                                           
   ndascomm_vcmd_get_ret_time                Retrieves retransmission time delay when     both         1.1
                                              the transmission from the NDAS Device to                  
                                              the host has failed.                                      
   ndascomm_vcmd_get_max_conn_time           Retrieves maximum connection time. If no     both         1.1
                                              packet was sent to the NDAS Device for the                
                                              time, connection would be closed.                         
   ndascomm_vcmd_get_standby_timer           Retrieves standby timer setting for the      both         1.1
                                              NDAS Device. the NDAS Device sends                        
                                              WIN_STANDBY1 ATA command to the attached                  
                                              unit device(s).                                           
   </TABLE>                                                                                                     */

typedef enum _NDASCOMM_VCMD_COMMAND
{
	ndascomm_vcmd_set_ret_time = 0x01,
	ndascomm_vcmd_set_max_conn_time = 0x02,
	ndascomm_vcmd_set_supervisor_pw = 0x11,
	ndascomm_vcmd_set_user_pw = 0x12,
	ndascomm_vcmd_set_enc_opt = 0x13,
	ndascomm_vcmd_set_standby_timer = 0x14,
	ndascomm_vcmd_reset = 0xff,
	ndascomm_vcmd_set_lpx_address = 0xfe,
	ndascomm_vcmd_set_sema = 0x05,
	ndascomm_vcmd_free_sema = 0x06,
	ndascomm_vcmd_get_sema = 0x07,
	ndascomm_vcmd_get_owner_sema = 0x08,
	ndascomm_vcmd_set_delay = 0x16,
	ndascomm_vcmd_get_delay = 0x17,
	ndascomm_vcmd_set_dynamic_max_conn_time = 0x0a,
	ndascomm_vcmd_set_dynamic_ret_time = 0x18,
	ndascomm_vcmd_get_dynamic_ret_time = 0x19,
	ndascomm_vcmd_set_d_enc_opt = 0x1a,
	ndascomm_vcmd_get_d_enc_opt = 0x1b,
	ndascomm_vcmd_get_ret_time = 0x03,
	ndascomm_vcmd_get_max_conn_time = 0x04,
	ndascomm_vcmd_get_standby_timer = 0x15,
} NDASCOMM_VCMD_COMMAND, *PNDASCOMM_VCMD_COMMAND;


/* <TITLE NDASCOMM_VCMD_PARAM>
   
   Summary
   Specifies the attributes of connection information. This
   structure is used to create connection to a NDAS Device.
   
   Description
   address_type member can have one of the following values.
   <TABLE>
   Value                                     Meaning
   ----------------------------------------  --------------------------------------
   NDASCOMM_CONNECTION_INFO_TYPE_ADDR_LPX    Use LpxAddress member to address a
                                              NDAS Device.
   NDASCOMM_CONNECTION_INFO_TYPE_ADDR_IP     Use IP member to address a NDAS
                                              Device.
   NDASCOMM_CONNECTION_INFO_TYPE_ID_W        Use wszDeviceStringId and
                                              wszDeviceStringKey member to address
                                              a NDAS Device.
   NDASCOMM_CONNECTION_INFO_TYPE_ID_A        Use szDeviceStringId and
                                              szDeviceStringKey member to address a
                                              NDAS Device.
   NDASCOMM_CONNECTION_INFO_TYPE_ID_DEVICE   Use Device ID to address a NDAS
                                              Device
   </TABLE>
   
   
   login_type member can have one of the following values.
   <TABLE>
   Value                          Meaning
   -----------------------------  ---------------------------------------------
   NDASCOMM_LOGIN_TYPE_NORMAL     Connects to the NDAS Device to operate IDE
                                   Commands. Use this value only.
   NDASCOMM_LOGIN_TYPE_DISCOVER   \Internal use only. There is no exported API
                                   function that supports discover login type.
   </TABLE>
   
   
   transport member can have one of the following values.
   <TABLE>
   Value                    Meaning
   -----------------------  ----------------------------
   NDASCOMM_TRANSPORT_LPX   Use LPX transport protocol.
   NDASCOMM_TRANSPORT_IP    Not supported.
   </TABLE>
   See Also
   NdasCommVendorCommand                                                            */
typedef union _NDASCOMM_VCMD_PARAM
{
	/* Reserved, do not use
	                        */
	struct {
		BYTE Data[12];
	} COMMON;

	/* ndascomm_vcmd_set_ret_time
	   
	   RetTime : [in] Retransmission time to set, in microsecond. Should
	   be greater than 0.                                            */
	struct {
		UINT32 RetTime;
	} SET_RET_TIME;

	/* ndascomm_vcmd_set_max_conn_time
	   
	   MaxConnTime : [in] Maximum connection time to set, in second.
	   Should be greater than 0.                                     */
	struct {
		UINT32 MaxConnTime;
	} SET_MAX_CONN_TIME;

	/* ndascomm_vcmd_set_supervisor_pw
	   
	   SupervisorPassword : [in] Supervisor password. 8 bytes */
	struct {
		UCHAR SupervisorPassword[8];
	} SET_SUPERVISOR_PW;

	/* ndascomm_vcmd_set_user_pw
	   
	   UserPassword : [in] Normal user password. 8 bytes */
	struct {
		UCHAR UserPassword[8];
	} SET_USER_PW; /* IN */

	/* ndascomm_vcmd_set_enc_opt
	   
	   EncryptHeader : [in] TRUE if encrypt header, FALSE otherwise.
	   
	   EncryptData : [in] TRUE if encrypt data, FALSE otherwise.     */
	struct {
		BOOL EncryptHeader;
		BOOL EncryptData;
	} SET_ENC_OPT;

	/* ndascomm_vcmd_set_standby_timer
	   
	   EnableTimer : [in] TRUE if enable standby timer, FALSE
	   \otherwise.
	   
	   TimeValue : [in] Standby timer value, in minutes. */
	struct {
		BOOL EnableTimer;
		UINT32 TimeValue; /* minutes, < 2^31 */
	} SET_STANDBY_TIMER; /* IN */

	/* ndascomm_vcmd_reset
	   
	   Do not set any value. */
	struct {
		UINT32 Obsolete;
	} RESET;

	/* ndascomm_vcmd_set_lpx_address
	   
	   AddressLPX : [in] LPX(Mac) address to set. 6 bytes */
	struct {
		BYTE AddressLPX[6];
	} SET_LPX_ADDRESS; /* IN */

	/* ndascomm_vcmd_set_sema
	   
	   Index : [in] Semaphore index. 0 ~ 3
	   
	   SemaCounter : [out] Current counter value of semaphore */
	struct {
		UINT8 Index; /* IN, 0 ~ 3 */
		UINT32 SemaCounter; /* OUT */
	} SET_SEMA; /* IN, OUT */

	/* ndascomm_vcmd_free_sema
	   
	   Index : [in] Semaphore index. 0 ~ 3
	   
	   SemaCounter : [out] Current counter value of semaphore */
	struct {
		UINT8 Index; /* IN, 0 ~ 3 */
		UINT32 SemaCounter; /* OUT */
	} FREE_SEMA; /* IN, OUT */

	/* ndascomm_vcmd_get_sema
	   
	   Index : [in] Semaphore index. 0 ~ 3
	   
	   SemaCounter : [out] Current counter value of semaphore */
	struct {
		UINT8 Index; /* IN, 0 ~ 3 */
		UINT32 SemaCounter; /* OUT */
	} GET_SEMA; /* IN, OUT */

	/* ndascomm_vcmd_get_owner_sema
	   
	   Index : [in] Semaphore index. 0 ~ 3
	   
	   AddressLPX : [out] LPX Address of the NDAS Device which
	   locked the semaphore at the moment.                     */
	struct {
		UINT8 Index; /* IN, 0 ~ 3 */
		UINT8 AddressLPX[8]; /* OUT */
	} GET_OWNER_SEMA; /* IN, OUT */

	/* ndascomm_vcmd_set_delay
	   
	   Delay : [in] Time delay between packets from the NDAS Device
	   to the host, in nanosecond. Should be multiples of 8. Default
	   is 0                                                          */
	struct {
		UINT32 Delay;
	} SET_DELAY;

	/* ndascomm_vcmd_get_delay
	   
	   TimeValue : [out] Time delay between packets from the NDAS
	   Device to the host, in nanosecond.                         */
	struct {
		UINT32 TimeValue; /* nano seconds, >= 8 */
	} GET_DELAY; /* OUT */

	/* \obsolete */
	struct {
		UINT32 Obsolete;
	} SET_DYNAMIC_MAX_CONN_TIME;

	/* ndascomm_vcmd_set_dynamic_ret_time
	   
	   MaxRetTime : [in] Retransmission time to set, in microsecond.
	   Should be greater than 0.                                 */
	struct {
		UINT32 TimeValue; /* micro second */
	} SET_DYNAMIC_RET_TIME; /* IN */

	/* ndascomm_vcmd_get_dynamic_ret_time
	   
	   RetTime : [out] Retransmission time to set, in microsecond. */
	struct {
		UINT32 RetTime; /* micro second, >= 1 */
	} GET_DYNAMIC_RET_TIME; /* OUT */

	/* not implemented */
	struct {
		BOOL EncryptHeader;
		BOOL EncryptData;
	} SET_D_ENC_OPT; /* IN */

	/* not implemented */
	struct {
		BOOL EncryptHeader;
		BOOL EncryptData;
	} GET_D_ENC_OPT; /* IN, OUT */

	/* ndascomm_vcmd_get_ret_time
	   
	   RetTime : [out] Retransmission time to set, in microsecond. */
	struct {
		UINT32 RetTime;
	} GET_RET_TIME; /* OUT */

	/* ndascomm_vcmd_get_max_conn_time
	   MaxConnTime : [out] Maximum connection time, in second. */
	struct {
		UINT32 MaxConnTime; /* second(*) */
	} GET_MAX_CONN_TIME; /* OUT */

	/* ndascomm_vcmd_get_standby_timer
	   EnableTimer : [out] TRUE if enable standby timer, FALSE
	   \otherwise.
	   
	   TimeValue : [out] Standby timer value, in minutes. */
	struct {
		BOOL EnableTimer;
		UINT32 TimeValue; /* minutes, < 2^31 */
	} GET_STANDBY_TIMER; /* OUT */
} NDASCOMM_VCMD_PARAM, *PNDASCOMM_VCMD_PARAM;

/* End of packing */
#include <poppack.h>

#endif /* _NDASCOMM_TYPE_H_ */
