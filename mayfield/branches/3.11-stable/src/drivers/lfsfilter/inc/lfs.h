ple file record segments
    //  to describe all of its runs, and this is a reference to a
    //  segment other than the first one.  The field says what the
    //  lowest Vcn is that is described by the referenced segment.
    //

    VCN LowestVcn;                                                  //  offset = 0x008

    //
    //  Reference to the MFT segment in which the attribute resides.
    //

    MFT_SEGMENT_REFERENCE SegmentReference;                         //  offset = 0x010

    //
    //  The file-record-unique attribute instance number for this
    //  attribute.
    //

    USHORT Instance;                                                //  offset = 0x018

    //
    //  When creating an attribute list entry, start the name here.
    //  (When reading one, use the AttributeNameOffset field.)
    //

    WCHAR AttributeName[1];                                         //  offset = 0x01A

} ATTRIBUTE_LIST_ENTRY;
typedef ATTRIBUTE_LIST_ENTRY *PATTRIBUTE_LIST_ENTRY;


typedef struct _DUPLICATED_INFORMATION {

    //
    //  File creation time.
    //

    LONGLONG CreationTime;                                          //  offset = 0x000

    //
    //  Last time the DATA attribute was modified.
    //

    LONGLONG LastModificationTime;                                  //  offset = 0x008

    //
    //  Last time any attribute was modified.
    //

    LONGLONG LastChangeTime;                                        //  offset = 0x010

    //
    //  Last time the file was accessed.  This field may not always
    //  be updated (write-protected media), and even when it is
    //  updated, it may only be updated if the time would change by
    //  a certain delta.  It is meant to tell someone approximately
    //  when the file was last accessed, for purposes of possible
    //  file migration.
    //

    LONGLONG LastAccessTime;                                        //  offset = 0x018

    //
    //  Allocated Length of the file in bytes.  This is obviously
    //  an even multiple of the cluster size.  (Not present if
    //  LowestVcn != 0.)
    //

    LONGLONG AllocatedLength;                                       //  offset = 0x020

    //
    //  File Size in bytes (highest byte which may be read + 1).
    //  (Not present if LowestVcn != 0.)
    //

    LONGLONG FileSize;                                              //  offset = 0x028

    //
    //  File attributes.  The first byte is the standard "Fat"
    //  flags for this file.
    //

    ULONG FileAttributes;                                           //  offset = 0x030

    //
    //  This union enables the retrieval of the tag in reparse
    //  points when there are no Ea's.
    //

    union {

        struct {

            //
            //  The size of buffer needed to pack these Ea's
            //

            USHORT  PackedEaSize;                                   //  offset = 0x034

            //
            //  Reserved for quad word alignment
            //

            USHORT  Reserved;                                       //  offset = 0x036
        };

        //
        //  The tag of the data in a reparse point. It represents
        //  the type of a reparse point. It enables different layered
        //  filters to operate on their own reparse points.
        //

        ULONG  ReparsePointTag;                                     //  offset = 0x034
    };

} DUPLICATED_INFORMATION;                                           //  sizeof = 0x038
typedef DUPLICATED_INFORMATION *PDUPLICATED_INFORMATION;

//
//  This bit is duplicated from the file record, to indicate that
//  this file has a file name index present (is a "directory").
//

#define DUP_FILE_NAME_INDEX_PRESENT      (0x10000000)

//
//  This bit is duplicated from the file record, to indicate that
//  this file has a view index present, such as the quota or
//  object id index.
//

#define DUP_VIEW_INDEX_PRESENT        (0x20000000)

//
//  The following macros examine fields of the duplicated structure.
//

#define IsDirectory( DUPLICATE )                                        \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             DUP_FILE_NAME_INDEX_PRESENT ))

#define IsViewIndex( DUPLICATE )                                        \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             DUP_VIEW_INDEX_PRESENT ))

#define IsReadOnly( DUPLICATE )                                         \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_READONLY ))

#define IsHidden( DUPLICATE )                                           \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_HIDDEN ))

#define IsSystem( DUPLICATE )                                           \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_SYSTEM ))

#define IsEncrypted( DUPLICATE )                                        \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_ENCRYPTED ))

#define IsCompressed( DUPLICATE )                                       \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_COMPRESSED ))

#define BooleanIsDirectory( DUPLICATE )                                        \
    (BooleanFlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
                    DUP_FILE_NAME_INDEX_PRESENT ))

#define BooleanIsReadOnly( DUPLICATE )                                         \
    (BooleanFlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
                    FILE_ATTRIBUTE_READONLY ))

#define BooleanIsHidden( DUPLICATE )                                           \
    (BooleanFlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
                    FILE_ATTRIBUTE_HIDDEN ))

#define BooleanIsSystem( DUPLICATE )                                           \
    (BooleanFlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
                    FILE_ATTRIBUTE_SYSTEM ))

#define HasReparsePoint( DUPLICATE )                                        \
    (FlagOn( ((PDUPLICATED_INFORMATION) (DUPLICATE))->FileAttributes,   \
             FILE_ATTRIBUTE_REPARSE_POINT ))


//
//  File Name attribute.  A file has one File Name attribute for
//  every directory it is entered into (hard links).
//

typedef struct _FILE_NAME {

    //
    //  This is a File Reference to the directory file which indexes
    //  to this name.
    //

    FILE_REFERENCE ParentDirectory;                                 //  offset = 0x000

    //
    //  Information for faster directory operations.
    //

    DUPLICATED_INFORMATION Info;                                    //  offset = 0x008

    //
    //  Length of the name to follow, in (Unicode) characters.
    //

    UCHAR FileNameLength;                                           //  offset = 0x040

    //
    //  FILE_NAME_xxx flags
    //

    UCHAR Flags;                                                    //  offset = 0x041

    //
    //  First character of Unicode File Name
    //

    WCHAR FileName[1];                                              //  offset = 0x042

} FILE_NAME;
typedef FILE_NAME *PFILE_NAME;

//
//  File Name flags
//

#define FILE_NAME_NTFS                   (0x01)
#define FILE_NAME_DOS                    (0x02)

//
//  The maximum file name length is 255 (in chars)
//

#define NTFS_MAX_FILE_NAME_LENGTH       (255)

//
//  The maximum number of links on a file is 1024
//

#define NTFS_MAX_LINK_COUNT             (1024)

//
//  This flag is not part of the disk structure, but is defined here
//  to explain its use and avoid possible future collisions.  For
//  enumerations of "directories" this bit may be set to convey to
//  the collating routine that it should not match file names that
//  only have the FILE_NAME_DOS bit set.
//

#define FILE_NAME_IGNORE_DOS_ONLY        (0x80)

#define NtfsFileNameSizeFromLength(LEN) (                   \
    (sizeof( FILE_NAME ) + LEN - sizeof( WCHAR ))           \
)

#define NtfsFileNameSize(PFN) (                                             \
    (sizeof( FILE_NAME ) + ((PFN)->FileNameLength - 1) * sizeof( WCHAR ))   \
)


//
//  Object id attribute.
//

//
//  On disk representation of an object id.
//

#define OBJECT_ID_KEY_LENGTH      16
#define OBJECT_ID_EXT_INFO_LENGTH 48

typedef struct _NTFS_OBJECTID_INFORMATION {

    //
    //  Data the filesystem needs to identify the file with this object id.
    //

    FILE_REFERENCE FileSystemReference;

    //
    //  This portion of the object id is not indexed, it's just
    //  some metadata for the user's benefit.
    //

    UCHAR ExtendedInfo[OBJECT_ID_EXT_INFO_LENGTH];

} NTFS_OBJECTID_INFORMATION, *PNTFS_OBJECTID_INFORMATION;

#define OBJECT_ID_FLAG_CORRUPT           (0x00000001)


//
//  Security Descriptor attribute.  This is just a normal attribute
//  stream containing a security descriptor as defined by NT
//  security and is really treated pretty opaque by NTFS.
//

//
//  Security descriptors are stored only once on a volume since there may be
//  many files that share the same descriptor bits.  Typically each principal
//  will create files with a single descriptor.
//
//  The descriptors themselves are stored in a stream, packed on DWORD boundaries.
//  No descriptor will span a 256K cache boundary.  The descriptors are assigned
//  a ULONG Id each time a unique descriptor is stored.  Prefixing each descriptor
//  in the stream is the hash of the descriptor, the assigned security ID, the
//  length, and the offset within the stream to the beginning of the structure.
//
//  For robustness, all security descriptors are written to the stream in two
//  different places, with a fixed offset between them.  The fixed offset is the
//  size of the VACB_MAPPING_GRANULARITY.
//
//  An index is used to map from a security Id to offset within the stream.  This
//  is used to retrieve the security descriptor bits for access validation.  The key
//  format is simply the ULONG security Id.  The data portion of the index record
//  is the header of the security descriptor in the stream (see above paragraph).
//
//  Another index is used to map from a hash to offset within the stream.  To
//  simplify the job of the indexing package, the key used in this index is the
//  hash followed by the assigned Id.  When a security descriptor is stored,
//  a hash is computed and an approximate seek is made on this index.  As entries
//  are enumerated in the index, the descriptor stream is mapped and the security
//  descriptor bits are compared.  The key format is a structure that contains
//  the hash and then the Id.  The collation routine tests the hash before the Id.
//  The data portion of the index record is the header of the security descriptor.
//

//
//  Key structure for Security Hash index
//

typedef struct _SECURITY_HASH_KEY
{
    ULONG   Hash;                           //  Hash value for descriptor
    ULONG   SecurityId;                     //  Security Id (guaranteed unique)
} SECURITY_HASH_KEY, *PSECURITY_HASH_KEY;

//
//  Key structure for Security Id index is simply the SECURITY_ID i