#ifndef __NDAS_FAT_H__
#define __NDAS_FAT_H__


#define NDASFAT_BUG						0
#define NDASFAT_INSUFFICIENT_RESOURCES	0
#define NDASFAT_UNEXPECTED				0
#define NDFAT_LPX_BUG					0
#define NDFAT_REQUIRED					0
#define NDFAT_IRP_NOT_IMPLEMENTED		0
#define NDFAT_IRP_NOT_IMPLEMENTED		0
#define NDFAT_IRP_NOT_IMPLEMENTED		0


#define NDASFAT_TIME_OUT						(360000*HZ)
#define NDASFAT_SECONDARY_THREAD_FLAG_TIME_OUT	(3*HZ)
#define NDASFAT_TRY_CLOSE_DURATION				NDASFAT_SECONDARY_THREAD_FLAG_TIME_OUT
#define NDFAT_VOLDO_THREAD_FLAG_TIME_OUT		(3*HZ)
#define NDFAT_TRY_FLUSH_DURATION				NDFAT_VOLDO_THREAD_FLAG_TIME_OUT
#define NDFAT_TRY_PURGE_DURATION				(30*HZ)
#define NDASFAT_QUERY_REMOVE_TIME_OUT			(HZ * 120) 


#define THREAD_WAIT_OBJECTS 3           // Builtin usable wait blocks from <ntddk.h>
#define ADD_ALIGN8(Length) ((Length+7) >> 3 << 3)

#define OPERATION_NAME_BUFFER_SIZE 80

#define IS_SECONDARY_FILEOBJECT( FileObject ) (																\
																											\
	FileObject																							&&	\
	FileObject->FsContext																				&&	\
	FileObject->FsContext2																				&&	\
	(NodeType(FileObject->FsContext) == NDFAT_NTC_VCB || NodeType(FileObject->FsContext) == FAT_NTC_DCB ||	\
	 NodeType(FileObject->FsContext) == FAT_NTC_FCB)													&&	\
	FlagOn(((PFCB)(FileObject->FsContext))->NdasFatFlags, ND_FAT_FCB_FLAG_SECONDARY)						\
)


#define SECONDARY_MESSAGE_TAG		'tmsF'
#define NDASFAT_ALLOC_TAG				'tafF'
#define PRIMARY_SESSION_BUFFERE_TAG	'tbsF'
#define PRIMARY_SESSION_MESSAGE_TAG 'mspF'
#define OPEN_FILE_TAG				'tofF'
#define FILE_NAME_TAG				'tnfF'


#define	MEMORY_CHECK_SIZE		0
#define MAX_RECONNECTION_TRY	60


struct Create {

	PIO_SECURITY_CONTEXT		SecurityContext;
    ULONG						Options;
    USHORT POINTER_ALIGNMENT	FileAttributes;
    USHORT						ShareAccess;
    ULONG  POINTER_ALIGNMENT	EaLength;
};


struct DeviceIoControl {

	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT IoControlCode;
    PVOID					Type3InputBuffer;
};


struct FileSystemControl {

	ULONG					OutputBufferLength;
    ULONG POINTER_ALIGNMENT InputBufferLength;
    ULONG POINTER_ALIGNMENT FsControlCode;
    PVOID					Type3InputBuffer;
};

#ifndef NTDDI_VERSION

#if WINVER >= 0x0502

struct QueryDirectory {

	ULONG					Length;
    PUNICODE_STRING			FileName;
	FILE_INFORMATION_CLASS	FileInformationClass;
	ULONG POINTER_ALIGNMENT FileIndex;
};

#else

struct QueryDirectory {

	ULONG					Length;
    PSTRING					FileName;
	FILE_INFORMATION_CLASS	FileInformationClass;
	ULONG POINTER_ALIGNMENT FileIndex;
};

#endif

#else

struct QueryDirectory {

	ULONG					Length;
    PUNICODE_STRING			FileName;
	FILE_INFORMATION_CLASS	FileInformationClass;
	ULONG POINTER_ALIGNMENT FileIndex;
};

#endif

struct QueryFile {

	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
};

struct SetFile {

	ULONG										Length;
    FILE_INFORMATION_CLASS POINTER_ALIGNMENT	FileInformationClass;
    PFILE_OBJECT								FileObject;
    
	union {

		struct {

			BOOLEAN ReplaceIfExists;
            BOOLEAN AdvanceOnly;
		};
        ULONG  ClusterCount;
        HANDLE DeleteHandle;
	};
};


struct QueryVolume {

	ULONG									Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT	FsInformationClass;
};

        //
        // System service parameters for:  NtSetVolumeInformationFile
        //

struct SetVolume {

	ULONG Length;
	FS_INFORMATION_CLASS POINTER_ALIGNMENT FsInformationClass;
};

struct Read {

	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct Write {

	ULONG					Length;
	ULONG POINTER_ALIGNMENT Key;
	LARGE_INTEGER			ByteOffset;
};


struct LockControl {

	PLARGE_INTEGER			Length;
    ULONG POINTER_ALIGNMENT Key;
    LARGE_INTEGER			ByteOffset;
};


struct QuerySecurity {

	SECURITY_INFORMATION SecurityInformation;
	ULONG POINTER_ALIGNMENT Length;
} ;


struct SetSecurity {

	SECURITY_INFORMATION SecurityInformation;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
} ;


struct QueryEa {
	
	ULONG Length;
	PVOID EaList;
	ULONG EaListLength;
	ULONG POINTER_ALIGNMENT EaIndex;
};

struct SetEa {
	ULONG Length;
};


struct QueryQuota {
	
	ULONG Length;
	PSID StartSid;
	PFILE_GET_QUOTA_INFORMATION SidList;
	ULONG SidListLength;
};


struct SetQuota {
	
	ULONG Length;
};


extern ULONG gOsMajorVersion;
extern ULONG gOsMinorVersion;


#define IS_WINDOWSXP_OR_LATER() \
    (((gOsMajorVersion == 5) && (gOsMinorVersion >= 1)) || \
     (gOsMajorVersion > 5))

#define IS_WINDOWS2K() \
    ((gOsMajorVersion == 5) && (gOsMinorVersion == 0))

#define IS_WINDOWSXP() \
    ((gOsMajorVersion == 5) && (gOsMinorVersion == 1))

#define IS_WINDOWSNET() \
    ((gOsMajorVersion == 5) && (gOsMinorVersion == 2))

#define IS_WINDOWSVISTA() \
	(gOsMajorVersion == 6)

#define IS_WINDOWSVISTA_OR_LATER() \
	(gOsMajorVersion >= 6)

#endif

typedef
NTSTATUS 
(*NDFAT_SET_FILTER_TOKEN) (
	IN PACCESS_TOKEN  ExistingToken,
	IN ULONG  Flags,
	IN PTOKEN_GROUPS  SidsToDisable  OPTIONAL,
	IN PTOKEN_PRIVILEGES  PrivilegesToDelete  OPTIONAL,
	IN PTOKEN_GROUPS  RestrictedSids  OPTIONAL,
	OUT PACCESS_TOKEN  *NewToken 
	);

extern NDFAT_SET_FILTER_TOKEN NdasFatSeFilterToken;

typedef
BOOLEAN 
(*NDFAT_KE_ARE_APCS_DISABLED) ( 
	VOID 
	);

extern NDFAT_KE_ARE_APCS_DISABLED NdasFatKeAreApcsDisabled;
	
typedef
BOOLEAN
(*NDFAT_KE_ARE_ALL_APCS_DISABLED) (
    VOID
    );

extern NDFAT_KE_ARE_ALL_APCS_DISABLED NdasFatKeAreAllApcsDisabled;

typedef
VOID 
(*NDFAT_CC_MDL_WRITE_ABORT) ( 
	IN PFILE_OBJECT FileObject, 
	IN PMDL MdlChain 
	);

extern NDFAT_CC_MDL_WRITE_ABORT NdasFatCcMdlWriteAbort;

typedef
NTSTATUS
(*NDFAT_FSRTL_REGISTER_FILESYSTEM_FILTER_CALLBACKS) (
    IN struct _DRIVER_OBJECT *FilterDriverObject,
    IN PFS_FILTER_CALLBACKS Callbacks
    );

extern NDFAT_FSRTL_REGISTER_FILESYSTEM_FILTER_CALLBACKS NdasFatFsRtlRegisterFileSystemFilterCallbacks;

typedef
BOOLEAN
(*NDFAT_FSRTL_ARE_VOLUME_START_APPLICATIONS_COMPLETE) (
    VOID
    );

extern NDFAT_FSRTL_ARE_VOLUME_START_APPLICATIONS_COMPLETE NdasFatFsRtlAreVolumeStartupApplicationsComplete;

typedef
ULONG
(*NDFAT_MM_DOES_FILE_HAVE_USER_WRITABLE_REFERENCES) (
    __in PSECTION_OBJECT_POINTERS SectionPointer
    );

extern NDFAT_MM_DOES_FILE_HAVE_USER_WRITABLE_REFERENCES NdasFatMmDoesFileHaveUserWritableReferences;
