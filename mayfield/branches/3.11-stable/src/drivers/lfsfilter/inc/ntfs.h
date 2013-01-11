zeof( SECURITY_DESCRIPTOR_HEADER ))

#define SetSecurityDescriptorLength(HEADER,LENGTH)  \
    ((HEADER)->Length = (LENGTH) + sizeof( SECURITY_DESCRIPTOR_HEADER ))

//
//  Define standard values for well-known security IDs
//

#define SECURITY_ID_INVALID              (0x00000000)
#define SECURITY_ID_FIRST                (0x00000100)


//
//  Volume Name attribute.  This attribute is just a normal
//  attribute stream containing the unicode characters that make up
//  the volume label.  It is an attribute of the Mft File.
//


//
//  Volume Information attribute.  This attribute is only intended
//  to be used on the Volume DASD file.
//

typedef struct _VOLUME_INFORMATION {

    LONGLONG Reserved;

    //
    //  Major and minor version number of NTFS on this volume,
    //  starting with 1.0.  The major and minor version numbers are
    //  set from the major and minor version of the Format and NTFS
    //  implementation for which they are initialized.  The policy
    //  for incementing major and minor versions will always be
    //  decided on a case by case basis, however, the following two
    //  paragraphs attempt to suggest an approximate strategy.
    //
    //  The major version number is incremented if/when a volume
    //  format change is made which requires major structure changes
    //  (hopefully never?).  If an implementation of NTFS sees a
    //  volume with a higher major version number, it should refuse
    //  to mount the volume.  If a newer implementation of NTFS sees
    //  an older major version number, it knows the volume cannot be
    //  accessed without performing a one-time conversion.
    //
    //  The minor version number is incremented if/when minor
    //  enhancements are made to a major version, which potentially
    //  support enhanced functionality through additional file or
    //  attribute record fields, or new system-defined files or
    //  attributes.  If an older implementation of NTFS sees a newer
    //  minor version number on a volume, it may issue some kind of
    //  warning, but it will proceed to access the volume - with
    //  presumably some degradation in functionality compared to the
    //  version of NTFS which initialized the volume.  If a newer
    //  implementation of NTFS sees a volume with an older minor
    //  version number, it may issue a warning and proceed.  In this
    //  case, it may choose to increment the minor version number on
    //  the volume and begin full or incremental upgrade of the
    //  volume on an as-needed basis.  It may also leave the minor
    //  version number unchanged, until some sort of explicit
    //  directive from the user specifies that the minor version
    //  should be updated.
    //

    UCHAR MajorVersion;                                             //  offset = 0x008

    UCHAR MinorVersion;                                             //  offset = 0x009

    //
    //  VOLUME_xxx flags.
    //

    USHORT VolumeFlags;                                             //  offset = 0x00A

    //
    //  The following fields will only exist on version 4 and greater
    //

    UCHAR LastMountedMajorVersion;                                  //  offset = 0x00C
    UCHAR LastMountedMinorVersion;                                  //  offset = 0x00D

    USHORT Reserved2;                                               //  offset = 0x00E

    USN LowestOpenUsn;                                              //  offset = 0x010

} VOLUME_INFORMATION;                                               //  sizeof = 0xC or 0x18
typedef VOLUME_INFORMATION *PVOLUME_INFORMATION;




//
//  Volume is Dirty
//

#define VOLUME_DIRTY                     (0x0001)
#define VOLUME_RESIZE_LOG_FILE           (0x0002)
#define VOLUME_UPGRADE_ON_MOUNT          (0x0004)
#define VOLUME_MOUNTED_ON_40             (0x0008)
#define VOLUME_DELETE_USN_UNDERWAY       (0x0010)
#define VOLUME_REPAIR_OBJECT_ID          (0x0020)

#define VOLUME_CHKDSK_RAN_ONCE           (0x4000)   // this bit is used by autochk/chkdsk only
#define VOLUME_MODIFIED_BY_CHKDSK        (0x8000)


//
//  Common Index Header for Index Root and Index Allocation Buffers.
//  This structure is used to locate the Index Entries and describe
//  the free space in either of the two structures above.
//

typedef struct _INDEX_HEADER {

    //
    //  Offset from the start of this structure to the first Index
    //  Entry.
    //

    ULONG FirstIndexEntry;                                          //  offset = 0x000

    //
    //  Offset from the start of the first index entry to the first
    //  (quad-word aligned) free byte.
    //

    ULONG FirstFreeByte;                                            //  offset = 0x004

    //
    //  Total number of bytes available, from the start of the first
    //  index entry.  In the Index Root, this number must always be
    //  equal to FirstFreeByte, as the total attribute record will
    //  be grown and shrunk as required.
    //

    ULONG BytesAvailable;                                           //  offset = 0x008

    //
    //  INDEX_xxx flags.
    //

    UCHAR Flags;                                                    //  offset = 0x00C

    //
    //  Reserved to round up to quad word boundary.
    //

    UCHAR Reserved[3];                                              //  offset = 0x00D

} INDEX_HEADER;                                                     //  sizeof = 0x010
typedef INDEX_HEADER *PINDEX_HEADER;

//
//  INDEX_xxx flags
//

//
//  This Index or Index Allocation buffer is an intermediate node,
//  as opposed to a leaf in the Btree.  All Index Entries will have
//  a block down pointer.
//

#define INDEX_NODE                       (0x01)

//
//  Index Root attribute.  The index attribute consists of an index
//  header record followed by one or more index entries.
//

typedef struct _INDEX_ROOT {

    //
    //  Attribute Type Code of the attribute being indexed.
    //

    ATTRIBUTE_TYPE_CODE IndexedAttributeType;                       //  offset = 0x000

    //
    //  Collation rule for this index.
    //

    COLLATION_RULE CollationRule;                                   //  offset = 0x004

    //
    //  Size of Index Allocation Buffer in bytes.
    //

    ULONG BytesPerIndexBuffer;                                      //  offset = 0x008

    //
    //  Size of Index Allocation Buffers in units of blocks.
    //  Blocks will be clusters when index buffer is equal or
    //  larger than clusters and log blocks for large
    //  cluster systems.
    //

    UCHAR BlocksPerIndexBuffer;                                     //  offset = 0x00C

    //
    //  Reserved to round to quad word boundary.
    //

    UCHAR Reserved[3];                                              //  offset = 0x00D

    //
    //  Index Header to describe the Index Entries which follow
    //

    INDEX_HEADER IndexHeader;                                       //  offset = 0x010

} INDEX_ROOT;                                                       //  sizeof = 0x020
typedef INDEX_ROOT *PINDEX_ROOT;

//
//  Index Allocation record is used for non-root clusters of the
//  b-tree.  Each non root cluster is contained in the data part of
//  the index allocation attribute.  Each cluster starts with an
//  index allocation list header and is followed by one or more
//  index entries.
//

typedef struct _INDEX_ALLOCATION_BUFFER {

    //
    //  Multi-Sector Header as defined by the Cache Manager.  This
    //  structure will always contain the signature "INDX" and a
    //  description of the location and size of the Update Sequence
    //  Array.
    //

    MULTI_SECTOR_HEADER MultiSectorHeader;                          //  offset = 0x000

    //
    //  Log File Sequence Number of last logged update to this Index
    //  Allocation Buffer.
    //

    LSN Lsn;                                                        //  offset = 0x008

    //
    //  We store the index block of this Index Allocation buffer for
    //  convenience and possible consistency checking.
    //

    VCN ThisBlock;                                                  //  offset = 0x010

    //
    //  Index Header to describe the Index Entries which follow
    //

    INDEX_HEADER IndexHeader;                                       //  offset = 0x018

    //
    //  Update Sequence Array to protect multi-sector transfers of
    //  the Index Allocation Buffer.
    //

    UPDATE_SEQUENCE_ARRAY UpdateSequenceArray;                      //  offset = 0x028

} INDEX_ALLOCATION_BUFFER;
typedef INDEX_ALLOCATION_BUFFER *PINDEX_ALLOCATION_BUFFER;

//
//  Default size of index buffer and index blocks.
//

#define DEFAULT_INDEX_BLOCK_SIZE        (0x200)
#define DEFAULT_INDEX_BLOCK_BYTE_SHIFT  (9)

//
//  Index Entry.  This structure is common to both the resident
//  index list attribute and the Index Allocation records
//

typedef struct _INDEX_ENTRY {

    //
    //  Define a union to distinguish directory indices from view indices
    //

    union {

        //
        //  Reference to file containing the attribute with this
        //  attribute value.
        //

        FILE_REFERENCE FileReference;                               //  offset = 0x000

        //
        //  For views, describe the Data Offset and Length in bytes
        //

        struct {

            USHORT DataOffset;                                      //  offset = 0x000
            USHORT DataLength;                                      //  offset = 0x001
            ULONG ReservedForZero;                                  //  offset = 0x002
        };
    };

    //
    //  Length of this index entry, in bytes.
    //

    USHORT Length;                                                  //  offset = 0x008

    //
    //  Length of attribute value, in bytes.  The attribute value
    //  immediately follows this record.
    //

    USHORT AttributeLength;                                         //  offset = 0x00A

    //
    //  INDEX_ENTRY_xxx Flags.
    //

    USHORT Flags;                                                   //  offset = 0x00C

    //
    //  Reserved to round to quad-word boundary.
    //

    USHORT Reserved;                                                //  offset = 0x00E

    //
    //  If this Index Entry is an intermediate node in the tree, as
    //  determined by the INDEX_xxx flags, then a VCN  is stored at
    //  the end of this entry at Length - sizeof(VCN).
    //

} INDEX_ENTRY;                                                      //  sizeof = 0x010
typedef INDEX_ENTRY *PINDEX_ENTRY;

//
//  INDEX_ENTRY_xxx flags
//

//
//  This entry is currently in the intermediate node form, i.e., it
//  has a Vcn at the end.
//

#define INDEX_ENTRY_NODE                 (0x0001)

//
//  This entry is the special END record for the Index or Index
//  Allocation buffer.
//

#define INDEX_ENTRY_END                  (0x0002)

//
//  This flag is *not* part of the on-disk structure.  It is defined
//  and reserved here for the convenience of the implementation to
//  help avoid allocating buffers from the pool and copying.
//

#define INDEX_ENTRY_POINTER_FORM         (0x8000)

#define NtfsIndexEntryBlock(IE) (                                       \
    *(PLONGLONG)((PCHAR)(IE) + (ULONG)(IE)->Length - sizeof(LONGLONG))  \
    )

#define NtfsSetIndexEntryBlock(IE,IB) {                                         \
    *(PLONGLONG)((PCHAR)(IE) + (ULONG)(IE)->Length - sizeof(LONGLONG)) = (IB);  \
    }

#define NtfsFirstIndexEntry(IH) (                       \
    (PINDEX_ENTRY)((PCHAR)(IH) + (IH)->FirstIndexEntry) \
    )

#define NtfsNextIndexEntry(IE) (                        \
    (PINDEX_ENTRY)((PCHAR)(IE) + (ULONG)(IE)->Length)   \
    )

#define NtfsCheckIndexBound(IE, IH) {                                                               \
    if (((PCHAR)(IE) < (PCHAR)(IH)) ||                                                              \
        (((PCHAR)(IE) + sizeof( INDEX_ENTRY )) > ((PCHAR)Add2Ptr((IH), (IH)->BytesAvailable)))) {   \
        NtfsRaiseStatus(IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NULL );                        \
    }                                                                                               \
}


//
//  MFT Bitmap attribute
//
//  The MFT Bitmap is simply a normal attribute stream in which
//  there is one bit to represent the allocation state of each File
//  Record Segment in the MFT.  Bit clear means free, and bit set
//  means allocated.
//
//  Whenever the MFT Data attribute is extended, the MFT Bitmap
//  attribute must also be extended.  If the bitmap is still in a
//  file record segment for the MFT, then it must be extended and
//  the new bits cleared.  When the MFT Bitmap is in the Nonresident
//  form, then the allocation should always be sufficient to store
//  enough bits to describe the MFT, however ValidDataLength insures
//  that newly allocated space to the MFT Bitmap has an initial
//  value of all 0's.  This means that if the MFT Bitmap is extended,
//  the newly represented file record segments are automatically in
//  the free state.
//
//  No structure definition is required; the positional offset of
//  the file record segment is exactly equal to the bit offset of
//  its corresponding bit in the Bitmap.
//


//
//  USN Journal Instance
//
//  The following describe the current instance of the Usn journal.
//

typedef struct _USN_JOURNAL_INSTANCE {

#ifdef __cplusplus
    CREATE_USN_JOURNAL_DATA JournalData;
#else   // __cplusplus
    CREATE_USN_JOURNAL_DATA;
#endif  // __cplusplus

    ULONGLONG JournalId;
    USN LowestValidUsn;

} USN_JOURNAL_INSTANCE, *PUSN_JOURNAL_INSTANCE;

//
//  Reparse point index keys.
//
//  The index with all the reparse points that exist in a volume at a
//  given time contains entries with keys of the form
//                        <reparse tag, file record id>.
//  The data part of these records is empty.
//

typedef struct _REPARSE_INDEX_KEY {

    //
    //  The tag of the reparse point.
    //

    ULONG FileReparseTag;

    //
    //  The file record Id where the reparse point is set.
    //

    LARGE_INTEGER FileId;

} REPARSE_INDEX_KEY, *PREPARSE_INDEX_KEY;



//
//  Ea Information attribute
//
//  This attribute is only present if the file/directory also has an
//  EA attribute.  It is used to store common EA query information.
//

typedef struct _EA_INFORMATION {

    //
    //  The size of buffer needed to pack these Ea's
    //

    USHORT PackedEaSize;                                            //  offset = 0x000

    //
    //  This is the count of Ea's with their NEED_EA
    //  bit set.
    //

    USHORT NeedEaCount;                                             //  offset = 0x002

    //
    //  The size of the buffer needed to return all Ea's
    //  in their unpacked form.
    //

    ULONG UnpackedEaSize;                                           //  offset = 0x004

}  EA_INFORMATION;                                                  //  sizeof = 0x008
typedef EA_INFORMATION *PEA_INFORMATION;


//
//  Define the struture of the quota data in the quota index.  The key for
//  the quota index is the 32 bit owner id.
//

typedef struct _QUOTA_USER_DATA {
    ULONG QuotaVersion;
    ULONG QuotaFlags;
    ULONGLONG QuotaUsed;
    ULONGLONG QuotaChangeTime;
    ULONGLONG QuotaThreshold;
    ULONGLONG QuotaLimit;
    ULONGLONG QuotaExceededTime;
    SID QuotaSid;
} QUOTA_USER_DATA, *PQUOTA_USER_DATA;

//
//  Define the size of the quota user data structure without the quota SID.
//

#define SIZEOF_QUOTA_USER_DATA FIELD_OFFSET(QUOTA_USER_DATA, QuotaSid)

//
//  Define the current version of the quote user data.
//

#define QUOTA_USER_VERSION 2

//
//  Define the quota flags.
//

#define QUOTA_FLAG_DEFAULT_LIMITS           (0x00000001)
#define QUOTA_FLAG_LIMIT_REACHED            (0x00000002)
#define QUOTA_FLAG_ID_DELETED               (0x00000004)
#define QUOTA_FLAG_USER_MASK                (0x00000007)

//
//  The following flags are only stored in the quota defaults index entry.
//

#define QUOTA_FLAG_TRACKING_ENABLED         (0x00000010)
#define QUOTA_FLAG_ENFORCEMENT_ENABLED      (0x00000020)
#define QUOTA_FLAG_TRACKING_REQUESTED       (0x00000040)
#define QUOTA_FLAG_LOG_THRESHOLD            (0x00000080)
#define QUOTA_FLAG_LOG_LIMIT                (0x00000100)
#define QUOTA_FLAG_OUT_OF_DATE              (0x00000200)
#define QUOTA_FLAG_CORRUPT                  (0x00000400)
#define QUOTA_FLAG_PENDING_DELETES          (0x00000800)

//
//  Define special quota owner ids.
//

#define QUOTA_INVALID_ID        0x00000000
#define QUOTA_DEFAULTS_ID       0x00000001
#define QUOTA_FISRT_USER_ID     0x00000100


//
//  Attribute Definition Table
//
//  The following struct defines the columns of this table.
//  Initially they will be stored as simple records, and ordered by
//  Attribute Type Code.
//

typedef struct _ATTRIBUTE_DEFINITION_COLUMNS {

    //
    //  Unicode attribute name.
    //

    WCHAR AttributeName[64];                                        //  offset = 0x000

    //
    //  Attribute Type Code.
    //

    ATTRIBUTE_TYPE_CODE AttributeTypeCode;                          //  offset = 0x080

    //
    //  Default Display Rule for this attribute
    //

    DISPLAY_RULE DisplayRule;                                       //  offset = 0x084

    //
    //  Default Collation rule
    //

    COLLATION_RULE CollationRule;                                   //  offset = 0x088

    //
    //  ATTRIBUTE_DEF_xxx flags
    //

    ULONG Flags;                                                    //  offset = 0x08C

    //
    //  Minimum Length for attribute, if present.
    //

    LONGLONG MinimumLength;                                         //  offset = 0x090

    //
    //  Maximum Length for attribute.
    //

    LONGLONG MaximumLength;                                         //  offset = 0x098

} ATTRIBUTE_DEFINITION_COLUMNS;                                     //  sizeof = 0x0A0
typedef ATTRIBUTE_DEFINITION_COLUMNS *PATTRIBUTE_DEFINITION_COLUMNS;

//
//  ATTRIBUTE_DEF_xxx flags
//

//
//  This flag is set if the attribute may be indexed.
//

#define ATTRIBUTE_DEF_INDEXABLE          (0x00000002)

//
//  This flag is set if the attribute may occur more than once, such
//  as is allowed for the File Name attribute.
//

#define ATTRIBUTE_DEF_DUPLICATES_ALLOWED (0x00000004)

//
//  This flag is set if the value of the attribute may not be
//  entirely null, i.e., all binary 0's.
//

#define ATTRIBUTE_DEF_MAY_NOT_BE_NULL    (0x00000008)

//
//  This attribute must be indexed, and no two attributes may exist
//  with the same value in the same file record segment.
//

#define ATTRIBUTE_DEF_MUST_BE_INDEXED    (0x00000010)

//
//  This attribute must be named, and no two attributes may exist
//  with the same name in the same file record segment.
//

#define ATTRIBUTE_DEF_MUST_BE_NAMED      (0x00000020)

//
//  This attribute must be in the Resident Form.
//

#define ATTRIBUTE_DEF_MUST_BE_RESIDENT   (0x00000040)

//
//  Modifications to this attribute should be logged even if the
//  attribute is nonresident.
//

#define ATTRIBUTE_DEF_LOG_NONRESIDENT    (0X00000080)



//
//  MACROS
//
//  Define some macros that are helpful for manipulating NTFS on
//  disk structures.
//

//
//  The following macro returns the first attribute record in a file
//  record segment.
//
//      PATTRIBUTE_RECORD_HEADER
//      NtfsFirstAttribute (
//          IN PFILE_RECORD_SEGMENT_HEADER FileRecord
//          );
//
//  The following macro takes a pointer to an attribute record (or
//  attribute list entry) and returns a pointer to the next
//  attribute record (or attribute list entry) in the list
//
//      PVOID
//      NtfsGetNextRecord (
//          IN PATTRIB_RECORD or PATTRIB_LIST_ENTRY Struct
//          );
//
//
//  The following macro takes as input a attribute record or
//  attribute list entry and initializes a string variable to the
//  name found in the record or entry.  The memory used for the
//  string buffer is the memory found in the attribute.
//
//      VOID
//      NtfsInitializeStringFromAttribute (
//          IN OUT PUNICODE_STRING Name,
//          IN PATTRIBUTE_RECORD_HEADER Attribute
//          );
//
//      VOID
//      NtfsInitializeStringFromEntry (
//          IN OUT PUNICODE_STRING Name,
//          IN PATTRIBUTE_LIST_ENTRY Entry
//          );
//
//
//  The following two macros assume resident form and should only be
//  used when that state is known.  They return a pointer to the
//  value a resident attribute or a pointer to the byte one beyond
//  the value.
//
//      PVOID
//      NtfsGetValue (
//          IN PATTRIBUTE_RECORD_HEADER Attribute
//          );
//
//      PVOID
//      NtfsGetBeyondValue (
//          IN PATTRIBUTE_RECORD_HEADER Attribute
//          );
//
//  The following two macros return a boolean value indicating if
//  the input attribute record is of the specified type code, or the
//  indicated value.  The equivalent routine to comparing attribute
//  names cannot be defined as a macro and is declared in AttrSup.c
//
//      BOOLEAN
//      NtfsEqualAttributeTypeCode (
//          IN PATTRIBUTE_RECORD_HEADER Attribute,
//          IN ATTRIBUTE_TYPE_CODE Code
//          );
//
//      BOOLEAN
//      NtfsEqualAttributeValue (
//          IN PATTRIBUTE_RECORD_HEADER Attribute,
//          IN PVOID Value,
//          IN ULONG Length
//          );
//

#define NtfsFirstAttribute(FRS) (                                          \
    (PATTRIBUTE_RECORD_HEADER)((PCHAR)(FRS) + (FRS)->FirstAttributeOffset) \
)

#define NtfsGetNextRecord(STRUCT) (                    \
    (PVOID)((PUCHAR)(STRUCT) + (STRUCT)->RecordLength) \
)

#define NtfsCheckRecordBound(PTR, SPTR, SIZ) {                                          \
    if (((PCHAR)(PTR) < (PCHAR)(SPTR)) || ((PCHAR)(PTR) >= ((PCHAR)(SPTR) + (SIZ)))) {  \
        NtfsRaiseStatus(IrpContext, STATUS_FILE_CORRUPT_ERROR, NULL, NULL );            \
    }                                                                                   \
}

#define NtfsInitializeStringFromAttribute(NAME,ATTRIBUTE) {                \
    (NAME)->Length = (USHORT)(ATTRIBUTE)->NameLength << 1;                 \
    (NAME)->MaximumLength = (NAME)->Length;                                \
    (NAME)->Buffer = (PWSTR)Add2Ptr((ATTRIBUTE), (ATTRIBUTE)->NameOffset); \
}

#define NtfsInitializeStringFromEntry(NAME,ENTRY) {                        \
    (NAME)->Length = (USHORT)(ENTRY)->AttributeNameLength << 1;            \
    (NAME)->MaximumLength = (NAME)->Length;                                \
    (NAME)->Buffer = (PWSTR)((ENTRY) + 1);                                 \
}

#define NtfsGetValue(ATTRIBUTE) (                                \
    Add2Ptr((ATTRIBUTE), (ATTRIBUTE)->Form.Resident.ValueOffset) \
)

#define NtfsGetBeyondValue(ATTRIBUTE) (                                      \
    Add2Ptr(NtfsGetValue(ATTRIBUTE), (ATTRIBUTE)->Form.Resident.ValueLength) \
)

#define NtfsEqualAttributeTypeCode(A,C) ( \
    (C) == (A)->TypeCode                  \
)

#define NtfsEqualAttributeValue(A,V,L) (     \
    NtfsIsAttributeResident(A) &&            \
    (A)->Form.Resident.ValueLength == (L) && \
    RtlEqualMemory(NtfsGetValue(A),(V),(L))  \
)

#pragma pack()

#endif //  _NTFS_


                                                                                                                                                                                                                                                                                                                                                                                         
    //  Record Segment.
    //

    LSN Lsn;                                                        //  offset = 0x008

    //
    //  Sequence Number.  This is incremented each time that a File
    //  Record segment is freed, and 0 is not used.  The
    //  SequenceNumber field of a File Reference must match the
    //  contents of this field, or else the File Reference is
    //  incorrect (presumably stale).
    //

    USHORT SequenceNumber;                                          //  offset = 0x010

    //
    //  This is the count of the number of references which exist
    //  for this segment, from an INDEX_xxx attribute.  In File
    //  Records Segments other than the Base File Record Segment,
    //  this field is 0.
    //

    USHORT ReferenceCount;                                          //  offset = 0x012

    //
    //  Offset to the first Attribute record in bytes.
    //

    USHORT FirstAttributeOffset;                                    //  offset = 0x014

    //
    //  FILE_xxx flags.
    //

    USHORT Flags;                                                   //  offset = 0x016

    //
    //  First free byte available for attribute storage, from start
    //  of this header.  This value should always be aligned to a
    //  quad-word boundary, since attributes are quad-word aligned.
    //

    ULONG FirstFreeByte;                                            //  offset = x0018

    //
    //  Total bytes available in this file record segment, from the
    //  start of this header.  This is essentially the file record
    //  segment size.
    //

    ULONG BytesAvailable;                                           //  offset = 0x01C

    //
    //  This is a File Reference to the Base file record segment for
    //  this file.  If this is the Base, then the value of this
    //  field is all 0's.
    //

    FILE_REFERENCE BaseFileRecordSegment;                           //  offset = 0x020

    //
    //  This is the attribute instance number to be used when
    //  creating an attribute.  It is zeroed when the base file
    //  record is created, and captured for each new attribute as it
    //  is created and incremented afterwards for the next
    //  attribute.  Instance numbering must also occur for the
    //  initial attributes.  Zero is a valid attribute instance
    //  number, and typically used for standard information.
    //

    USHORT NextAttributeInstance;                                   //  offset = 0x028

    //
    //  Current FRS record - this is here for recovery alone and added in 5.1
    //  Note: this is not aligned
    // 

    USHORT SegmentNumberHighPart;                                  //  offset = 0x02A
    ULONG SegmentNumberLowPart;                                    //  offset = 0x02C

    //
    //  Update Sequence Array to protect multi-sector transfers of
    //  the File Record Segment.  Accesses to already initialized
    //  File Record Segments should go through the offset above, for
    //  upwards compatibility.
    //

    UPDATE_SEQUENCE_ARRAY UpdateArrayForCreateOnly;                 //  offset = 0x030

} FILE_RECORD_SEGMENT_HEADER;
typedef FILE_RECORD_SEGMENT_HEADER *PFILE_RECORD_SEGMENT_HEADER;


//
//  earlier version of FRS from 5.0
//  

typedef struct _FILE_RECORD_SEGMENT_HEADER_V0 {

    //
    //  Multi-Sector Header as defined by the Cache Manager.  This
    //  structure will always contain the signature "FILE" and a
    //  description of the location and size of the Update Sequence
    //  Array.
    //

    MULTI_SECTOR_HEADER MultiSectorHeader;                          //  offset = 0x000

    //
    //  Log File Sequence Number of last logged update to this File
    //  Record Segment.
    //

    LSN Lsn;                                                        //  offset = 0x008

    //
    //  Sequence Number.  This is incremented each time that a File
    //  Record segment is freed, and 0 is not used.  The
    //  SequenceNumber field of a File Reference must match the
    //  contents of this field, or else the File Reference is
    //  incorrect (presumably stale).
    //

    USHORT SequenceNumber;                                          //  offset = 0x010

    //
    //  This is the count of the number of references which exist
    //  for this segment, from an INDEX_xxx attribute.  In File
    //  Records Segments other than the Base File Record Segment,
    //  this field is 0.
    //

    USHORT ReferenceCount;                                          //  offset = 0x012

    //
    //  Offset to the first Attribute record in bytes.
    //

    USHORT FirstAttributeOffset;                                    //  offset = 0x014

    //
    //  FILE_xxx flags.
    //

    USHORT Flags;                                                   //  offset = 0x016

    //
    //  First free byte available for attribute storage, from start
    //  of this header.  This value should always be aligned to a
    //  quad-word boundary, since attributes are quad-word aligned.
    //

    ULONG FirstFreeByte;                                            //  offset = x0018

    //
    //  Total bytes available in this file record segment, from the
    //  start of this header.  This is essentially the file record
    //  segment size.
    //

    ULONG BytesAvailable;                                           //  offset = 0x01C

    //
    //  This is a File Reference to the Base file record segment for
    //  this file.  If this is the Base, then the value of this
    //  field is all 0's.
    //

    FILE_REFERENCE BaseFileRecordSegment;                           //  offset = 0x020

    //
    //  This is the attribute instance number to be used when
    //  creating an attribute.  It is zeroed when the base file
    //  record is created, and captured for each new attribute as it
    //  is created and incremented afterwards for the next
    //  attribute.  Instance numbering must also occur for the
    //  initial attributes.  Zero is a valid attribute instance
    //  number, and typically used for standard information.
    //

    USHORT NextAttributeInstance;                                   //  offset = 0x028

    //
    //  Update Sequence Array to protect multi-sector transfers of
    //  the File Record Segment.  Accesses to already initialized
    //  File Record Segments should go through the offset above, for
    //  upwards compatibility.
    //

    UPDATE_SEQUENCE_ARRAY UpdateArrayForCreateOnly;                 //  offset = 0x02A

} FILE_RECORD_SEGMENT_HEADER_V0;

//
//  FILE_xxx flags.
//

#define FILE_RECORD_SEGMENT_IN_USE       (0x0001)
#define FILE_FILE_NAME_INDEX_PRESENT     (0x0002)
#define FILE_SYSTEM_FILE                 (0x0004)
#define FILE_VIEW_INDEX_PRESENT          (0x0008)

//
//  Define a macro to determine the maximum space available for a
//  single attribute.  For example, this is required when a
//  nonresident attribute has to split into multiple file records -
//  we need to know how much we can squeeze into a single file
//  record.  If this macro has any inaccurracy, it must be in the
//  direction of returning a slightly smaller number than actually
//  required.
//
//      ULONG
//      NtfsMaximumAttributeSize (
//          IN ULONG FileRecordSegmentSize
//          );
//

#define NtfsMaximumAttributeSize(FRSS) (                                               \
    (FRSS) - QuadAlign(sizeof(FILE_RECORD_SEGMENT_HEADER)) -                           \
    QuadAlign((((FRSS) / SEQUENCE_NUMBER_STRIDE) * sizeof(UPDATE_SEQUENCE_NUMBER))) -  \
    QuadAlign(sizeof(ATTRIBUTE_TYPE_CODE))                                             \
)


//
//  Attribute Record.  Logically an attribute has a type, an
//  optional name, and a value, however the storage details make it
//  a little more complicated.  For starters, an attribute's value
//  may either be resident in the file record segment itself, on
//  nonresident in a separate data stream.  If it is nonresident, it
//  may actually exist multiple times in multiple file record
//  segments to describe different ranges of VCNs.
//
//  Attribute Records are always aligned on a quad word (64-bit)
//  boundary.
//

typedef struct _ATTRIBUTE_RECORD_HEADER {

    //
    //  Attribute Type Code.
    //

    ATTRIBUTE_TYPE_CODE TypeCode;                                   //  offset = 0x000

    //
    //  Length of this Attribute Record in bytes.  The length is
    //  always rounded to a quad word boundary, if necessary.  Also
    //  the length only reflects the size necessary to store the
    //  given record variant.
    //

    ULONG RecordLength;                                             //  offset = 0x004

    //
    //  Attribute Form Code (see below)
    //

    UCHAR FormCode;                                                 //  offset = 0x008

    //
    //  Length of the optional attribute name in characters, or 0 if
    //  there is none.
    //

    UCHAR NameLength;                                               //  offset = 0x009

    //
    //  Offset to the attribute name from start of attribute record,
    //  in bytes, if it exists.  This field is undefined if
    //  NameLength is 0.
    //

    USHORT NameOffset;                                              //  offset = 0x00A

    //
    //  ATTRIBUTE_xxx flags.
    //

    USHORT Flags;                                                   //  offset = 0x00C

    //
    //  The file-record-unique attribute instance number for this
    //  attribute.
    //

    USHORT Instance;                                                //  offset = 0x00E

    //
    //  The following union handles the cases distinguished by the
    //  Form Code.
    //

    union {

        //
        //  Resident Form.  Attribute resides in file record segment.
        //

        struct {

            //
            //  Length of attribute value in bytes.
            //

            ULONG ValueLength;                                      //  offset = 0x010

            //
            //  Offset to value from start of attribute record, in
            //  bytes.
            //

            USHORT ValueOffset;                                     //  offset = 0x014

            //
            //  RESIDENT_FORM_xxx Flags.
            //

            UCHAR ResidentFlags;                                    //  offset = 0x016

            //
            //  Reserved.
            //

            UCHAR Reserved;                                         //  offset = 0x017

        } Resident;

        //
        //  Nonresident Form.  Attribute resides in separate stream.
        //

        struct {

            //
            //  Lowest VCN covered by this attribute record.
            //

            VCN LowestVcn;                                          //  offset = 0x010

            //
            //  Highest VCN covered by this attribute record.
            //

            VCN HighestVcn;                                         //  offset = 0x018

            //
            //  Offset to the Mapping Pairs Array  (defined below),
            //  in bytes, from the start of the attribute record.
            //

            USHORT MappingPairsOffset;                              //  offset = 0x020

            //
            //  Unit of Compression size for this stream, expressed
            //  as a log of the cluster size.
            //
            //      0 means file is not compressed
            //      1, 2, 3, and 4 are potentially legal values if the
            //          stream is compressed, however the implementation
            //          may only choose to use 4, or possibly 3.  Note
            //          that 4 means cluster size time 16.  If convenient
            //          the implementation may wish to accept a
            //          reasonable range of legal values here (1-5?),
            //          even if the implementation only generates
            //          a smaller set of values itself.
            //

            UCHAR CompressionUnit;                                  //  offset = 0x022

            //
            //  Reserved to get to quad word boundary.
            //

            UCHAR Reserved[5];                                      //  offset = 0x023

            //
            //  Allocated Length of the file in bytes.  This is
            //  obviously an even multiple of the cluster size.
            //  (Not present if LowestVcn != 0.)
            //

            LONGLONG AllocatedLength;                               //  offset = 0x028

            //
            //  File Size in bytes (highest byte which may be read +
            //  1).  (Not present if LowestVcn != 0.)
            //

            LONGLONG FileSize;                                      //  offset = 0x030

            //
            //  Valid Data Length (highest initialized byte + 1).
            //  This field must also be rounded to a cluster
            //  boundary, and the data must always be initialized to
            //  a cluster boundary. (Not present if LowestVcn != 0.)
            //

            LONGLONG ValidDataLength;                               //  offset = 0x038

            //
            //  Totally allocated.  This field is only present for the first
            //  file record of a compressed stream.  It represents the sum of
            //  the allocated clusters for a file.
            //

            LONGLONG TotalAllocated;                                //  offset = 0x040

            //
            //
            //  Mapping Pairs Array, starting at the offset stored
            //  above.
            //
            //  The Mapping Pairs Array is stored in a compressed
            //  form, and assumes that this information is
            //  decompressed and cached by the system.  The reason
            //  for compressing this information is clear, it is
            //  done in the hopes that all of the retrieval
            //  information always fits in a single file record
            //  segment.
            //
            //  Logically, the MappingPairs Array stores a series of
            //  NextVcn/CurrentLcn pairs.  So, for example, given
            //  that we know the first Vcn (from LowestVcn above),
            //  the first Mapping Pair tells us what the next Vcn is
            //  (for the next Mapping Pair), and what Lcn the
            //  current Vcn is mapped to, or 0 if the Current Vcn is
            //  not allocated.  (This is exactly the FsRtl MCB
            //  structure).
            //
            //  For example, if a file has a single run of 8
            //  clusters, starting at Lcn 128, and the file starts
            //  at LowestVcn=0, then the Mapping Pairs array has
            //  just one entry, which is:
            //
            //    NextVcn = 8
            //    CurrentLcn = 128
            //
            //  The compression is implemented with the following
            //  algorithm.  Assume that you initialize two "working"
            //  variables as follows:
            //
            //    NextVcn = LowestVcn (from above)
            //    CurrentLcn = 0
            //
            //  The MappingPairs array is byte stream, which simply
            //  store the changes to the working variables above,
            //  when processed sequentially.  The byte stream is to
            //  be interpreted as a zero-terminated stream of
            //  triples, as follows:
            //
            //    count byte = v + (l * 16)
            //
            //      where v = number of changed low-order Vcn bytes
            //            l = number of changed low-order Lcn bytes
            //
            //    v Vcn change bytes
            //    l Lcn change bytes
            //
            //  The byte stream terminates when a count byte of 0 is †Q$ àˇˇ       @ÍÇ  Íˇˇ       Ø; L  _        9ö  ïÏ î L  `        kñ  ñÏ èÏ L  a    cJã  óÏ êÏ L  b       8ö      ìÏ L  c        ì  ôÏ íÏ M  d       ;ö  öÏ œ; M  e        <ö  õÏ î M  f        kñ  úÏ ïÏ M  g    ~Jã  ùÏ ñÏ M  h       ;ö      ôÏ M  i        ì  üÏ òÏ N  j       >ö  †Ï „; N  k        ?ö  °Ï 	î N  l        kñ  ¢Ï õÏ N  m    ôJã  £Ï úÏ N  n       >ö      üÏ N  o        ì  •Ï ûÏ O  p       Aö  ¶Ï < O  q        Bö  ßÏ î O  r        kñ  ®Ï °Ï O  s    ¥Jã  ©Ï ¢Ï O  t       Aö      •Ï O  u        ì  ´Ï §Ï P  v       Dö  ¨Ï < P  w        Eö  ≠Ï î P  x        kñ  ÆÏ ßÏ P  y    œJã  ØÏ ®Ï P  z       Dö      ´Ï P  {        ì  ±Ï ™Ï Q  |       Gö  ≤Ï *< Q  }        Hö  ≥Ï î Q  ~        kñ  ¥Ï ≠Ï Q      ÍJã  µÏ ÆÏ Q  Ä       Gö      ±Ï Q  Å        ì  ∑Ï ∞Ï R  Ç       Jö  ∏Ï 5< R  É        Kö  πÏ î R  Ñ        kñ  ∫Ï ≥Ï R  Ö    Jã  ªÏ ¥Ï R  Ü       Jö      ∑Ï R  á        ì  ΩÏ ∂Ï S  à       Mö  æÏ @ S  â        Nö  øÏ î S  ä        kñ  ¿Ï πÏ S  ã     Jã  ¡Ï ∫Ï S  å       Mö      ΩÏ S  ç        ì  √Ï ºÏ T  é       Pö  ƒÏ ?@ T  è        Qö  ≈Ï î T  ê        kñ  ∆Ï øÏ T  ë    ;Jã  «Ï ¿Ï T  í       Pö      √Ï T  ì        ì  …Ï ¬Ï U  î       Sö   Ï ^@ U  ï        Tö  ÀÏ !î U  ñ        kñ  ÃÏ ≈Ï U  ó    VJã  ÕÏ ∆Ï U  ò       Sö      …Ï U  ô        ì  œÏ »Ï V  ö       Vö  –Ï r@ V  õ        Wö  —Ï "î V  ú        kñ  “Ï ÀÏ V  ù    qJã  ”Ï ÃÏ V  û       Vö      œÏ V  ü        ì  ’Ï ŒÏ W  †       Vö  ÷Ï ”Ï W  °        Yö  ◊Ï #î W  ¢        kñ  ÿÏ —Ï W  £    åJã  ŸÏ “Ï W  §       Vö      ’Ï W  •        ì  €Ï ‘Ï X  ¶       [ö  ‹Ï “ﬂ X  ß        \ö  ›Ï %î X  ®        kñ  ﬁÏ ◊Ï X  ©    ßJã  ﬂÏ ÿÏ X  ™       [ö      €Ï X  ´        ì  ·Ï ⁄Ï Y  ¨       ^ö  ‚Ï §@ Y  ≠        _ö  „Ï (î Y  Æ        [ó  ‰Ï πì Y  Ø        kñ  ÂÏ ›Ï Y  ∞    ¬Jã  ÊÏ ﬁÏ Y  ±       ^ö      ·Ï Y  ≤        ì  ËÏ ‡Ï Z  ≥       aö  ÈÏ Æ@ Z  ¥        bö  ÍÏ )î Z  µ        kñ  ÎÏ ‰Ï Z  ∂    ›Jã  ÏÏ ÂÏ Z  ∑       aö      ËÏ Z  ∏        ì  ÓÏ ÁÏ [  π       dö  ÔÏ ‘ﬂ [  ∫        eö  Ï -î [  ª        kñ  ÒÏ ÍÏ [  º    ¯Jã  ÚÏ ÎÏ [  Ω       dö      ÓÏ [  æ        ì  ÙÏ ÌÏ \  ø       gö  ıÏ {A \  ¿        hö  ˆÏ GÛ \  ¡        kñ  ˜Ï Ï \  ¬    Jã  ¯Ï ÒÏ \  √       gö      ÙÏ \  ƒ        ì  ˙Ï ÛÏ ]  ≈       gö  ˚Ï ¯Ï ]  ∆        jö  ¸Ï =î ]  «        kñ  ˝Ï ˆÏ ]  »    .Jã  ˛Ï ˜Ï ]  …       gö      ˙Ï ]       IJã   Ì ˝Ï ^  À   \Jå  Ì ØÎ ^  Ã   oJó  Ì 4Ï ^  Õ   sJ„  Ì 5Ï ^  Œ   {JÁ  Ì :Ï ^  œ   Jï  Ì <Ï ^  –       ñ  Ì =Ï ^  —        ©ñ      Í ^  “        ä  Ì rÏ _  ”        ˆ%      ¶  _  ‘    ´Jã  
Ì ˇÏ `  ’       èF Ì yﬂ `  ÷    æJó  Ì Ì `  ◊   ¬J„  Ì Ì `  ÿ    Jˇ  Ì ’‚ `  Ÿ   “J   Ì ÷‚ `  ⁄   ⁄J  Ì {‚ `  €   ‚J  Ì |‚ `  ‹   ÍJ  Ì ËË `  ›   ¯J  Ì ÈË `  ﬁ    JÁ  Ì Ì `  ﬂ       æ  Ì ;Ï `  ‡    Jï  Ì Ì `  ·       ñ  Ì Ì `  ‚        86  Ì Î∂ `  „        B  Ì !Õ `  ‰        lö  Ì ]Í `  Â        )B  Ì +Í `  Ê    Jã  Ì 	Ì `  Á   J≤°  Ì §Î `  Ë   ,Jmö  Ì µ `  È   5Jnö  Ì V} `  Í       Uó   Ì AÏ `  Î 	       ™ñ       Í `  Ï          "Ì çπ a  Ì       Õ #Ì ;A
 a  Ó       Œ $Ì Q
 a  Ô        rö  %Ì °:  ∞Q$ àˇˇ       sö  &Ì ¢: a  Ò        tö  'Ì ÿì a  Ú        uö  (Ì §: a  Û        vö  )Ì •: a  Ù    QJã  *Ì Ì a  ı       pö      ß: a  ˆ          ,Ì !Ì b  ˜       Õ -Ì "Ì b  ¯       Œ .Ì #Ì b  ˘        zö  /Ì ´: b  ˙       {ö  0Ì ¨: b  ˚        |ö  1Ì ≠: b  ¸        }ö  2Ì Æ: b  ˝        ~ö  3Ì Ø: b  ˛    lJã  4Ì )Ì b  ˇ       yö  5Ì ±: b           ö      $u b      áJ  7Ì +Ì c     ìJŒ 8Ì -Ì c         Çö  9Ì µ: c         Éö  :Ì ∂: c          Ñö  ;Ì ∑: c          Öö  <Ì ∏: c          Üö  =Ì π: c      üJã  >Ì 3Ì c  	       Åö      ª: c  
          @Ì 6Ì d         Õ AÌ ,Ì d         Œ BÌ 7Ì d          âö  CÌ ø: d         äö  DÌ ¿: d          ãö  EÌ ¡: d          åö  FÌ ¬: d          çö  GÌ √: d      ∫Jã  HÌ =Ì d         àö      ≈: d            JÌ ?Ì e         Õ KÌ @Ì e         Œ LÌ AÌ e          – MÌ …: e         — NÌ  : e          “ OÌ À: e          ” PÌ Ã: e          ‘ QÌ Õ: e      ’Jã  RÌ GÌ e         èö      œ: e            TÌ IÌ f         Õ UÌ JÌ f          Œ VÌ KÌ f  !        óö  WÌ ”: f  "       òö  XÌ ‘: f  #        ôö  YÌ ’: f  $        öö  ZÌ ÷: f  %        õö  [Ì ◊: f  &    Jã  \Ì QÌ f  '       ñö      Ÿ: f  (          ^Ì SÌ g  )       Õ _Ì TÌ g  *       Œ `Ì UÌ g  +        ûö  aÌ ›: g  ,       üö  bÌ ﬁ: g  -        †ö  cÌ ﬂ: g  .        °ö  dÌ ‡: g  /        ¢ö  eÌ ·: g  0    Jã  fÌ [Ì g  1       ùö  gÌ „: g  2        £ö      Ou g  3          iÌ ]Ì h  4       Õ jÌ ^Ì h  5       Œ kÌ _Ì h  6        ¶ö  lÌ Ë: h  7       ßö  mÌ È: h  8        ®ö  nÌ Í: h  9        ©ö  oÌ Î: h  :        ™ö  pÌ Ï: h  ;    &Jã  qÌ eÌ h  <       •ö      Ó: h  =          sÌ hÌ i  >       Õ tÌ iÌ i  ?       Œ uÌ jÌ i  @        ≠ö  vÌ Ú: i  A       Æö  wÌ Û: i  B        Øö  xÌ Ù: i  C        ∞ö  yÌ ı: i  D        ±ö  zÌ ˆ: i  E    AJã  {Ì pÌ i  F       ¨ö      ¯: i  G          }Ì rÌ j  H       Õ ~Ì sÌ j  I       Œ Ì tÌ j  J        ≤ö  ÄÌ ¸: j  K       ≥ö  ÅÌ ˝: j  L        ¥ö  ÇÌ Ÿì j  M        µö  ÉÌ Cî j  N        Wó  ÑÌ ∏ì j  O    \Jã  ÖÌ zÌ j  P       Xó      HÏ j  Q          áÌ |Ì k  R       Õ àÌ }Ì k  S       Œ âÌ ~Ì k  T        ∂ö  äÌ ; k  U       ∑ö  ãÌ ; k  V        ∏ö  åÌ ; k  W        πö  çÌ Dî k  X        [ó  éÌ „Ï k  Y    wJã  èÌ ÑÌ k  Z       \ó      NÏ k  [          ëÌ ÜÌ l  \       Õ íÌ áÌ l  ]       Œ ìÌ àÌ l  ^        ºö  îÌ ; l  _       Ωö  ïÌ ; l  `        æö  ñÌ ; l  a        øö  óÌ ; l  b        ¿ö  òÌ ; l  c    íJã  ôÌ éÌ l  d       ªö      ; l  e          õÌ êÌ m  f       Õ úÌ ëÌ m  g       Œ ùÌ íÌ m  h        ¡ö  ûÌ ; m  i       ¬ö  üÌ ; m  j        √ö  †Ì ; m  k        ƒö  °Ì ; m  l        ^ó  ¢Ì ∫ì m  m    ≠Jã  £Ì òÌ m  n       ö      TÏ m  o          •Ì öÌ n  p       Õ ¶Ì õÌ n  q       Œ ßÌ úÌ n  r        ≈ö  ®Ì $; n  s       ∆ö  ©Ì %; n  t        «ö  ™Ì ⁄ì n  u        »ö  ´Ì Eî n  v        ö  ¨Ì ªì n  w    »Jã  ≠Ì ¢Ì n  x       ö      ZÏ n  y          ØÌ §Ì o  z       Õ ∞Ì •Ì o  {       Œ ±Ì ¶Ì o  |        …ö  ≤Ì .; o  }        ö  ≥Ì /; o  ~        Àö  ¥Ì 0; o          Ãö  µÌ 1; o  Ä        ö  ∂Ì ºì o  Å    „Jã  ∑Ì ¨Ì o  Ç  ¿Q$ àˇˇ       @î√  Íˇˇ       @“ã  Íˇˇ       @ƒ#  Íˇˇ       @&g  Íˇˇ       @G  Íˇˇ       @-∑  Íˇˇ         0d2536b877e96ad9c44daf97e5f1bdf2 1:2.10.3-0ubuntu1  0bb5fd55724d9d7cf0369b3935f345b1 1:2.10.3-0ubuntu1 1:2.10.3 1:2.10.3+1~ 1:2.10.3-0ubuntu1 0.6.16 0.6.16 0.6.16 2.15 1.0.2 0.78 0.1.1 2.30.0 0.10.0 0.10.20 1.13 1.0.2 1.8.0.10 3.12.0~1.9b1 5.14.2 2.7.4 5.14.2-6ubuntu2 8.5.0 8.5.0 0.9.0 0.9.0  88d4769bcea5af6ec25fbcf73513ead2 libpwl-dev 0.11.2-6ubuntu3 0.11.2-6ubuntu3 libpwl3 libpwl4 libppl-pwl-dev  482566ce24b1e703479e7191461cde0d 0.11.2-6ubuntu3 2.2.5 1:4.1.1 4.1.1 libppl-pwl  3b61b92ccc9ee800ad95146dcaf4f2fb 2.7.3-0ubuntu3 2.7.3-0ubuntu3 2.15 1:4.1.1 1.0.0 1:1.2.0 2.6 2.6 3.2.3-0ubuntu1 3.2.3-0ubuntu1 2.15 1.95.8 3.0.4 1:4.1.1 1.0.0 1:1.2.0 3.0~rc1 3.0~rc1 libqalculate-dev 0.9.7-6ubuntu2 0.9.7-6ubuntu2 1.2  f539c1ac1488eeb06b0232761399ea2e libqalculate-doc 0.9.7-6ubuntu2  02fc32ed83533c6b69ef67af7a10ef00 0.9.7-6ubuntu2 2.14 1:4.1.1 2.12.0 4.6 2.7.4 libqalculate4 qalc 0.9.7-2 0.9.7-2 0.9.7-2 0.9.7-2  d259e364d39e86d2ec2ec0bbf8522e97 libqapt-dev 1.3.1-0ubuntu2 1.3.1-0ubuntu2  ebfb8a382214854d21b85301af1fea89 1.3.1-0ubuntu2 0.8.16~exp12ubuntu7 2.14 1:4.1.1 0.99.0 1.1.65 4:4.5.3 4:4.8.0 4.1.1  1383b3859e9d954f26494c46e0594b54 1.3.1-0ubuntu2 0.8.0 0.8.16~exp12ubuntu7 2.14 1:4.1.1 4:4.6.1 4:4.7.0~beta1 4.1.1  038c717459bf7e7e73e27ab32c90ffc8 2.0.3-2 2.4 1:4.1.1 4:4.7.0~beta1 4.1.1 libqca2-plugin-cyrus-sasl libqca2-plugin-gnupg  e6821571ac9f35265d4086f952b5ec4a libqca2-dbg 2.0.3-2 2.0.3-2  06022ce95ceac07fa4ef9298fd8640de 2.0.3-2 2.0.3-2 4.4.0~ libqca2-doc 2.0.3-2 qca-dev  e5f738a0e88b215b00c26ce7899aa25a 2.0.3-2 2.0.3-2  dd72ebca568996d714100bc0c6c8978a 2.0.0~beta3-1 2.4 1:4.1.1 2.0.2 4:4.7.0~beta1 1.0.0 4.4.0  18c68833f28b9d7bcccbad9a4477d5e9 libqdox-java 1.12-1 libqdox-java-doc  e922f227dda69707530f2d1100182c21 1.12-1  e8e95b9c3669693544d9ffda441e77b8 4:4.8.2-0ubuntu2 2.14 4:4.8.2-0ubuntu2 4:4.5.3 4.1.1 4:4.4.85 4:4.4.85 4:4.4.85 4:4.4.85 libqgpsmm-dev 3.4-2 libqgpsmm20 3.4 3.4+1~ 3.4 3.4+1~  58dc95e31af1e8f60364cd6d73c81b45 3.4-2 2.4 1.0.2 1:4.1.1 4:4.5.3 4:4.7.0~beta1 4.1.1  c10285f0d14c1d7f263002160056c2c5 libqimageblitz-dbg 1:0.0.6-4 1:0.0.6-4  5b48a652765b3ad3da3a55f2cfaa901b libqimageblitz-dev 1:0.0.6-4 2.2.5 1:0.0.6-4 4:4.7.0~beta1 4:4.5.3 4.1.1  f673e737d1e06571d143a1e9cad6d83d libqimageblitz-perl 4:4.8.2-0ubuntu2 4:4.8.2-0ubuntu2 5.14.2-6ubuntu2 2.14 1:4.1.1 5.14.2 4:4.7.0~beta1 4:4.8.2 4.1.1  57481bb825c282c20b855faa63e82b32 1:0.0.6-4 2.4 4:4.6.1 4:4.5.3 4.1.1  ad225a1dd490b89caffce639650e1c32 libqjson-dbg 0.7.1-6 0.7.1-6 libqjson0-dbg 0.7.1-1 0.7.1-1 0.7.1-1 0.7.1-1  9c9877993a4be9a23828d7e2d13a6619 0.7.1-6 0.7.1-6  0269cb6bdbafd90c897ef35ae88db023 0.7.1-6 2.2.5 1:4.1.1 4:4.7.0~beta1 4.4.0  dc695edb04d44eb425bf12b42096e29d 0.7.1-6  bbc706c035a4064d07e5e1cebd4c1f3d libqoauth-dev 1.0.1-1ubuntu1 1.0.1-1ubuntu1  72eed5d7237fbb79c3f0dd070f99d04f 1.0.1-1ubuntu1 2.2.5 1:4.1.1 4:4.5.3 4:4.7.0~beta1 4.1.1  5646f72d9d22acf579c6414bdb113eb1 libqrencode-dev 3.1.1-1ubuntu1 3.1.1-1ubuntu1  937628d3fc3fc49f40ec5c11eccb2f6c 3.1.1-1ubuntu1 2.2.5  bb4f6fa67db97fff114149c7ae73927d libqscintilla-perl 4:4.8.2-0ubuntu2 4:4.8.2-0ubuntu2 5.14.2-6ubuntu2 2.14 1:4.1.1 5.14.2 4:4.7.0~beta1 libsmokeqsci3 4:4.8.2 4.1.1  a0a36f3f5798edf0407e47bf5fcb9cf4 libqscintilla2-8 2.6.1-4 2.14 1:4.1.1 4:4.8.0~ 4:4.8.0~ 4.1.1 libqscintilla2-6  00d5deee4952fcddcb5e856a0f16f69a 2.6.1-4 2.2.5 1:4.1.1 4:4.8.0~ 4:4.8.0~ 4.1.1  fc7121805924d55a1d7e2195d99804cf libqscintilla2-dev 2.6.1-4 2.6.1 2.6.1+1~  b00baa53e3c0197937730759bc6c4e7a libqscintilla2-doc 2.6.1-4  01d737e0b909fd4291b9d702632ec2ed libqt3-compat-headers 3:3.3.8-b-8ubuntu3 libqt3-headers 3:3.3.8-b-8ubuntu3 libqt3-mt-dev 3:3.1.1-2 3:3.1.1-2 3:3.1.1-2 3:3.1.1-2 3:3.1.1-2 3:3.1.1-2  c93320e924d131f08f10078bf670da1d 3:3.3.8-b-8ubuntu3 libqt3-plugins-headers  0cbc8e4920cd40fcad7dfcf777e9647f 3:3.3.8-b-8ubuntu3 2.11 2.8.0 2.2.1 1:1.0.0 8c 1.0.10 1.2.13-4 4.1.1 1.1.2 2.1.1 1:1.1.4 libqt3-mt-psql libqt3-mt-mysql libqt3-mt-odbc 4.3.0.dfsg.1-4 libqt3c-mt libqt3c102-mt libqui1-emb libqt3 libqt3-hUñ                   .       Iñ     @             ..      Vñ     h       	      prop-base       Wñ     à             props   Xñ     ∞       	      text-base       ÿ             xcrypt_hash.h.svn-base  Rñ                 xmuisel.h.svn-base      Sñ     8            xregcfg.h.svn-base      Tñ     h            xstrhelp.h.svn-base                            `tV' àˇˇ                       ‡qV' àˇˇ                        rV' àˇˇ                        rV' àˇˇ                       @rV' àˇˇ                       `rV' àˇˇ                       ¿qV' àˇˇ                       ÄrV' àˇˇ                       †rV' àˇˇ                       ¿rV' àˇˇ                       ‡rV' àˇˇ                        sV' àˇˇ                        sV' àˇˇ                       †pV' àˇˇ                       ÄsV' àˇˇ                        tV' àˇˇ                       †sV' àˇˇ                       ¿sV' àˇˇ                       ‡sV' àˇˇ                       `sV' àˇˇ                        tV' àˇˇ                       @tV' àˇˇ                       ÄqV' àˇˇ                       ÄtV' àˇˇ                       ¿tV' àˇˇ                       `zV' àˇˇ                        uV' àˇˇ                       @uV' àˇˇ                       ‡tV' àˇˇ                       †wV' àˇˇ                       `uV' àˇˇ                       ÄuV' àˇˇ                       †uV' àˇˇ                        vV' àˇˇ                       `V' àˇˇ                       ÄpV' àˇˇ                        vV' àˇˇ                       @vV' àˇˇ                       `vV' àˇˇ                       ÄvV' àˇˇ                       †vV' àˇˇ                       ¿vV' àˇˇ                       ‡vV' àˇˇ                        wV' àˇˇ                       ÄwV' àˇˇ                       ‡uV' àˇˇ                       ¿uV' àˇˇ                        pV' àˇˇ                        uV' àˇˇ                       ‡wV' àˇˇ                       ¿V' àˇˇ                       @xV' àˇˇ                       ¿wV' àˇˇ                       †tV' àˇˇ                       `xV' àˇˇ                       ÄxV' àˇˇ                       †xV' àˇˇ                       ¿pV' àˇˇ                       ‡xV' àˇˇ                        yV' àˇˇ                       @yV' àˇˇ                       ÄzV' àˇˇ                       `yV' àˇˇ                       ÄyV' àˇˇ                        xV' àˇˇ                       †|V' àˇˇ                        }V' àˇˇ                        xV' àˇˇ                       @V' àˇˇ                       ÄV' àˇˇ                       ¿yV' àˇˇ                        yV' àˇˇ                       ¿zV' àˇˇ                       Ä|V' àˇˇ                       ‡zV' àˇˇ                        {V' àˇˇ                        {V' àˇˇ                       `{V' àˇˇ                       @|V' àˇˇ                       Ä{V' àˇˇ                       †{V' àˇˇ                       ‡{V' àˇˇ                       †}V' àˇˇ                        |V' àˇˇ                       ‡|V' àˇˇ                       @{V' àˇˇ                       †zV' àˇˇ                       †~V' àˇˇ                       †yV' àˇˇ                       ¿|V' àˇˇ                        |V' àˇˇ                        }V' àˇˇ                        ~V' àˇˇ                       @}V' àˇˇ                       `}V' àˇˇ                       ¿{V' àˇˇ                       ‡}V' àˇˇ                       ¿}V' àˇˇ                       Ä}V' àˇˇ                        ~V' àˇˇ                       @zV' àˇˇ                       @~V' àˇˇ                       `~V' àˇˇ                       `|V' àˇˇ                        V' àˇˇ                       ¿~V' àˇˇ                       ‡~V' àˇˇ                        V' àˇˇ                       Ä~V' àˇˇ                        zV' àˇˇ                        zV' àˇˇ                       @pV' àˇˇ                       †V' àˇˇ                       ‡V' àˇˇ                       `wV' àˇˇ                       ‡yV' àˇˇ                        ÄQ$ àˇˇ          Íˇˇ       Snapshot->LowestModifiedVcn) {

                    ((PSCB) (Mcb->FcbHeader))->ScbSnapshot->LowestModifiedVcn = Mcb->NtfsMcbArray[RangeIndex].StartingVcn;
                }

                if (Mcb->NtfsMcbArray[RangeIndex].EndingVcn > ((PSCB) (Mcb->FcbHeader))->ScbSnapshot->HighestModifiedVcn) {

                    ((PSCB) (Mcb->FcbHeader))->ScbSnapshot->HighestModifiedVcn = Mcb->NtfsMcbArray[RangeIndex].EndingVcn;
                }

                //
                //  If the count in the this Mcb is non-zero then we must be growing the
                //  range.  We can simply split at the previoius end of the Mcb.  It must
                //  be legal.
                //

                if (FsRtlNumberOfRunsInLargeMcb( &Entry->LargeMcb ) != 0) {

                    ASSERT( PrevEndingVcn < EntryEndingVcn );

                    NtfsInsertNewRange( Mcb, PrevEndingVcn + 1, RangeIndex, FALSE );

                //
                //  There are no runs currently in this range.  If we are at the
                //  start of the range then split at our maximum range value.
                //  Otherwise split at the Vcn being inserted.  We don't need
                //  to be too smart here.  The mapping pair package will decide where
                //  the final range values are.
                //

                } else if (LocalVcn == EntryStartingVcn) {

                    NtfsInsertNewRange( Mcb,
                                        EntryStartingVcn + MAX_CLUSTERS_PER_RANGE,
                                        RangeIndex,
                                        FALSE );

                //
                //  Go ahead and split at the CurrentVcn.  On our next pass we will
                //  trim the length of this new range if necessary.
                //

                } else {

                    NtfsInsertNewRange( Mcb,
                                        LocalVcn,
                                        RangeIndex,
                                        FALSE );
                }

                //
                //  Set the run length to 0 and go back to the start of the loop.
                //  We will encounter the inserted range on the next pass.
                //

                RunLength = 0;
                continue;
            }

            //
            //  Now add the mapping from the large mcb, bias the vcn
            //  by the start of the range
            //

            ASSERT( (LocalVcn - EntryStartingVcn) >= 0 );

            if (!FsRtlAddLargeMcbEntry( &Entry->LargeMcb,
                                        LocalVcn - EntryStartingVcn,
                                        LocalLcn,
                                        RunLength )) {

                try_return( Result = FALSE );
            }
        }

        Result = TRUE;

    try_exit: NOTHING;

    } finally {

        NtfsVerifyNtfsMcb(Mcb);

        if (!AlreadySynchronized) { NtfsReleaseNtfsMcbMutex( Mcb ); }

        if (NewEntry != NULL) { NtfsFreePool( NewEntry ); }
    }

    return Result;
}


VOID
NtfsUnloadNtfsMcbRange (
    IN PNTFS_MCB Mcb,
    IN LONGLONG StartingVcn,
    IN LONGLONG EndingVcn,
    IN BOOLEAN TruncateOnly,
    IN BOOLEAN AlreadySynchronized
    )

/*++

Routine Description:

    This routine unloads the mapping stored in the Mcb.  After
    the call everything from startingVcn and endingvcn is now unmapped and unknown.

Arguments:

    Mcb - Supplies the Mcb being manipulated

    StartingVcn - Supplies the first Vcn which is no longer being mapped

    EndingVcn - Supplies the last vcn to be unloaded

    TruncateOnly - Supplies TRUE if last affected range should only be
                   truncated, or FALSE if it should be unloaded (as during
                   error recovery)

    AlreadySynchronized - Supplies TRUE if our caller already owns the Mcb mutex.

Return Value:

    None.

--*/

{
    ULONG Starti –Q$ àˇˇ          ULONG EndingRangeIndex;

    ULONG i;

    if (!AlreadySynchronized) { NtfsAcquireNtfsMcbMutex( Mcb ); }

    //
    //  Verify that we've been called to unload a valid range.  If we haven't,
    //  then there's nothing we can unload, so we just return here.  Still,
    //  we'll assert so we can see why we were called with an invalid range.
    //

    if ((StartingVcn < 0) || (EndingVcn < StartingVcn)) {

        //
        //  The only legal case is if the range is empty.
        //

        ASSERT( StartingVcn == EndingVcn + 1 );
        if (!AlreadySynchronized) { NtfsReleaseNtfsMcbMutex( Mcb ); }
        return;
    }

    NtfsVerifyNtfsMcb(Mcb);
    NtfsVerifyUncompressedNtfsMcb(Mcb,StartingVcn,EndingVcn);

    //
    //  Get the starting and ending range indices for this call
    //

    StartingRangeIndex = NtfsMcbLookupArrayIndex( Mcb, StartingVcn );
    EndingRangeIndex = NtfsMcbLookupArrayIndex( Mcb, EndingVcn );

    //
    //  Use try finally to enforce common termination processing.
    //

    try {

        //
        //  For all paged Mcbs, just unload all ranges touched by the
        //  unload range, and collapse with any unloaded neighbors.
        //

        if (Mcb->PoolType == PagedPool) {

            //
            //  Handle truncate case.  The first test insures that we only truncate
            //  the Mcb were were initialized with (we cannot deallocate it).
            //
            //  Also only truncate if ending is MAXLONGLONG and we are not eliminating
            //  the entire range, because that is the common truncate case, and we
            //  do not want to unload the last range every time we truncate on close.
            //

            if (((StartingRangeIndex == 0) && (Mcb->NtfsMcbArraySizeInUse == 1))

                ||

                (TruncateOnly && (StartingVcn != Mcb->NtfsMcbArray[StartingRangeIndex].StartingVcn))) {

                //
                //  If this is not a truncate call, make sure to eliminate the
                //  entire range.
                //

                if (!TruncateOnly) {
                    StartingVcn = 0;
                }

                if (Mcb->NtfsMcbArray[StartingRangeIndex].NtfsMcbEntry != NULL) {

                    FsRtlTruncateLargeMcb( &Mcb->NtfsMcbArray[StartingRangeIndex].NtfsMcbEntry->LargeMcb,
                                           StartingVcn - Mcb->NtfsMcbArray[StartingRangeIndex].StartingVcn );
                }

                Mcb->NtfsMcbArray[StartingRangeIndex].EndingVcn = StartingVcn - 1;

                StartingRangeIndex += 1;
            }

            //
            //  Unload entries that are beyond the starting range index
            //

            for (i = StartingRangeIndex; i <= EndingRangeIndex; i += 1) {

                UnloadEntry( Mcb, i );
            }

            //
            //  If there is a preceding unloaded range, we must collapse him too.
            //

            if ((StartingRangeIndex != 0) &&
                (Mcb->NtfsMcbArray[StartingRangeIndex - 1].NtfsMcbEntry == NULL)) {

                StartingRangeIndex -= 1;
            }

            //
            //  If there is a subsequent unloaded range, we must collapse him too.
            //

            if ((EndingRangeIndex != (Mcb->NtfsMcbArraySizeInUse - 1)) &&
                (Mcb->NtfsMcbArray[EndingRangeIndex + 1].NtfsMcbEntry == NULL)) {

                EndingRangeIndex += 1;
            }

            //
            //  Now collapse empty ranges.
            //

            if (StartingRangeIndex < EndingRangeIndex) {
                NtfsCollapseRanges( Mcb, StartingRangeIndex, EndingRangeIndex );
            }

            try_return(NOTHING);
        }

        //
        //  For nonpaged Mcbs, there is only one range and we truncate it.
        //

        ASSERT((StartingRangeIndex | EndingRangeIndex) == 0);

        if (Mcb->NtfsMcbArray[0].NtfsMcbEntry != NU `V$ àˇˇ              ±! °,     rO     sO Í1 ,ó          `ÿ`      ‡ú    5£  8	         ï±! Ñõ     uO     vO Û1 tñ           34      –4    è  9	         L≤! Ñ     xO     yO ˇ1 é  +y     ÿÕ            ?«  :	         ˇ≤! Ñ     {O     |O 2 è          ¯è      
     B¡  ;	         ø≥! X˝    ~O     O 2 Wó  0y     ¸˘i      ∞a    ∑  <	         \¥! X˝    ÅO     ÇO $2 [ó  5y     ∂^j      ∞R    t)  =	         ‚¥! X˝    ÑO     ÖO *2 ^ó  9y     ‡h      T    `ô  >	         í" X˝    áO     àO 02 ö  =y     t‘l      `g    Mk  ?	         " X˝    äO     ãO 62 ö  Ay     @§â             @	         û" X˝    çO     éO <2  ö  Ey     îy      ∞√    L˙  A	         '" X˝    êO     ëO B2 #ö  Iy     íq]      8    †ﬂ  B	         ú" X˝    ìO     îO H2 á  Jy      ∑[      Ä9    iÔ  C	         (" X˝    ñO     óO O2 (ö  Oy     ±j      c    —‚  D	         Æ" X˝    ôO     öO U2 +ö  Sy     </h       P    û_  E	         4	" X˝    úO     ùO [2 .ö  Wy     Ïgi      ÄV    Éê  F	         ∫	" X˝    üO     †O a2 1ö  [y     ú|m      †y    (  G	         @
" X˝    ¢O     £O g2 4ö  _y     >nl      Äk    Ô  H	         ∆
" X˝    •O     ¶O m2 7ö  cy     Qh      @S    q‹  I	         L" X˝    ®O     ©O s2 :ö  gy     x¯g      0W    Û)  J	         “" X˝    ´O     ¨O y2 =ö  ky     (’m      h    Íí  K	         X" X˝    ÆO     ØO 2 @ö  oy     $«j      –b    Ú>  L	         Ù\" X˝    5k     6k Ö2 Cö  sy     ∂ìò      Ë    ‚n  M	         z]" X˝    8k     9k ã2 Fö  wy     –Hy      ‡
    ¥ã  N	          ^" X˝    ;k     <k ë2 Iö  {y     @Ll      ê`    Ì´  O	         Ü^" X˝    >k     ?k ó2 Lö  y     ⁄Jh       ]    ‰Z  P	         _" X˝    Ak     Bk ù2 Oö  Éy     ∂m      †^    ª.  Q	         í_" X˝    Dk     Ek £2 Rö  áy     ‹Ñm      @_     c  R	         `" X˝    Gk     Hk ©2 Uö  ãy     bj      ¿b    ˘  S	         °`" X˝    Jk     Kk Ø2 Xö  éy     ûYi       ]    †  T	         	a" X˝    Mk     Nk µ2 Zö  íy     0u      `¨    	¨  U	         èa" X˝    Pk     Qk ª2 ]ö  ñy     U      p     å  V	         b" X˝    Sk     Tk ¬2 `ö  öy     ¸Ñi      T    uÂ  W	         õb" X˝    Vk     Wk »2 cö  ûy     ÿ‡g      T    ÙÈ  X	         $c" X˝    Yk     Zk Œ2 fö  ¢y     ‡p      \    éY  Y	         ∞c" X˝    \k     ]k ‘2 iö  •y     ìq      ∞a    ƒ  Z	         d" Ñõ     _k     `k ⁄2 ê          ‰è      P'     ≈ô  [	         wd" Ñõ    bk     ck ‡2 kñ          4dA      `Ü     ·  \	         ∂d" a;     ek     fk ‚2 (B          ﬁ      0     Z6  ]	         ûe" ±e"    hk     ik ¯2 oö  ±y     –ﬁ      ¿L     ª‡  ^	         ∞f" ±e"    kk     lk 3 xö  ªy     ¯      `Q     vÍ  _	         ìg" ±e"    nk     ok 3 Äö  √y     ¸0      W     Ø{  `	         p∏" ±e"    qk     rk ô: áö  Ày     ÿ      ¿M     ∞í  a	         Oπ" ±e"    tk     uk •: éö  ”y     h      ¿R     [“  b	         4∫" ±e"    wk     xk ±: ïö  €y     †       pV     ü^  c	         
ª" ±e"    zk     {k Ω: úö  ‰y     ∆I       Z     n`  d	         Úª" ±e"    }k     ~k  : §ö  Ïy     ∏˛      ‡M     D  e	         »º" ±e"    Äk     Åk ÷: ´ö  Ùy     ˙      M     ˝  f	         äΩ" ±e"    Ék     Ñk ‚: Yó  ˘y     Â      ‡M     hÁ  g	         æ" Ñõ    Ük     ák     qö          ^h      `     ƒC  h	         Læ" ±e"    âk     äk Ó: ]ó  ˛y     ""       O     e[  i	         Ìæ" ±e"    åk     çk ˙: ∫ö  z     Äœ      ¿K     RC  j	         Øø" ±e"    èk     êk ; ö  z     “      êL     h£  k	         <¿" ±e"    ík     ìk ; ö  z     ∞~      pN     R  l	         …¿" ±e"    ïk     ñk ; ö  z     ˆÜ      P^     ¨   m	         c¡" ±e"    òk     ôk +; "ö  z     ¿X      ∞V     €$  n	         ¡" ±e"    õk     úk 7; %ö   z     pU      –N     Å◊  o	         †¬" ±e"    ûk     ük C; ÷ö  'z                    usiveLite(((PFCB)(FCB))->PagingIoResource, TRUE);  \
    (IC)->CleanupStructure = (FCB);                                     \
}

#define NtfsReleasePagingIo(IC,FCB) {                                   \
    ASSERT((IC)->CleanupStructure == (FCB));                            \
    ExReleaseResourceLite(((PFCB)(FCB))->PagingIoResource);                 \
    (IC)->CleanupStructure = NULL;                                      \
}

BOOLEAN
NtfsAcquireFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG AcquireFlags
    );

VOID
NtfsReleaseFcbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

VOID
NtfsReleaseScbWithPaging (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    );

BOOLEAN
NtfsAcquireExclusiveFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN ULONG AcquireFlags
    );

VOID
NtfsAcquireSharedFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSCB Scb OPTIONAL,
    IN ULONG AcquireFlags
    );

BOOLEAN
NtfsAcquireSharedFcbCheckWait (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN ULONG AcquireFlags
    );

VOID
NtfsReleaseFcb (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

VOID
NtfsAcquireExclusiveScb (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    );

#ifdef NTFSDBG

BOOLEAN
NtfsAcquireResourceExclusive (
    IN PIRP_CONTEXT IrpContext OPTIONAL,
    IN PVOID FcbOrScb,
    IN BOOLEAN Wait
    );

#else

INLINE
BOOLEAN
NtfsAcquireResourceExclusive (
    IN PIRP_CONTEXT IrpContext OPTIONAL,
    IN PVOID FcbOrScb,
    IN BOOLEAN Wait
    )
{
    UNREFERENCED_PARAMETER( IrpContext );

    if (NTFS_NTC_FCB == ((PFCB)FcbOrScb)->NodeTypeCode) {
        return ExAcquireResourceExclusiveLite( ((PFCB)FcbOrScb)->Resource, Wait );
    } else {
        return ExAcquireResourceExclusiveLite( ((PSCB)(FcbOrScb))->Header.Resource, Wait );
    }
}

#endif

#ifdef NTFSDBG
BOOLEAN
NtfsAcquireResourceShared (
   IN PIRP_CONTEXT IrpContext OPTIONAL,
   IN PVOID FcbOrScb,
   IN BOOLEAN Wait
   );
#else

INLINE
BOOLEAN
NtfsAcquireResourceShared (
   IN PIRP_CONTEXT IrpContext OPTIONAL,
   IN PVOID FcbOrScb,
   IN BOOLEAN Wait
   )
{
    BOOLEAN Result;

    UNREFERENCED_PARAMETER( IrpContext );

    if (NTFS_NTC_FCB == ((PFCB)FcbOrScb)->NodeTypeCode) {
        Result =  ExAcquireResourceSharedLite( ((PFCB)FcbOrScb)->Resource, Wait );
    } else {

        ASSERT_SCB( FcbOrScb );

        Result = ExAcquireResourceSharedLite( ((PSCB)(FcbOrScb))->Header.Resource, Wait );
    }
    return Result;
}

#endif

//
//  VOID
//  NtfsReleaseResource(
//      IN PIRP_CONTEXT IrpContext OPTIONAL,
//      IN PVOID FcbOrScb
//      };
//

#ifdef NTFSDBG

VOID
NtfsReleaseResource(
    IN PIRP_CONTEXT IrpContext OPTIONAL,
    IN PVOID FcbOrScb
    );

#else
#define NtfsReleaseResource( IC, F ) {                                        \
        if (NTFS_NTC_FCB == ((PFCB)(F))->NodeTypeCode) {                      \
            ExReleaseResourceLite( ((PFCB)(F))->Resource );                       \
        } else {                                                              \
            ExReleaseResourceLite( ((PSCB)(F))->Header.Resource );                \
        }                                                                     \
    }

#endif

VOID
NtfsAcquireSharedScbForTransaction (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    );

VOID
NtfsReleaseSharedResources (
    IN PIRP_CONTEXT IrpContext
    );

VOID
NtfsReleaseAllResources (
    IN PIRP_CONTEXT IrpContext
    );

VOID
NtfsAcquireIndexCcb (
    IN PSCB Scb,
    IN PCCB Ccb,
    IN PEOF_WAIT_BLOCK EofWaitBlock
    );

VOID
NtfsReleaseIndexCcb (
    IN PSCB Scb,
    IN PCCB Ccb
    );

//
//  VOID
//  NtfsAcquireSharedScb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PSCB Scb
//      );
//
//  VOID
//  NtfsReleaseScb (
//      IN PIRP_CONTEXT IrpContext,
//   pV$ àˇˇ        π  Íˇˇ       @π  Íˇˇ       Ä   Íˇˇ       ¿   Íˇˇ       ¿n  Íˇˇ       Ä¢>  Íˇˇ       ¿¢>  Íˇˇ        y4  Íˇˇ       @y4  Íˇˇ        ∂4  Íˇˇ       @∂4  Íˇˇ       Ä¸   Íˇˇ       ¿¸   Íˇˇ        }ç  Íˇˇ       @}ç  Íˇˇ       ÄM  Íˇˇ       ¿M  Íˇˇ       TEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsLockVcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsUnlockVcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsLockFcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB Fcb
//      );
//
//  VOID
//  NtfsUnlockFcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB Fcb
//      );
//
//  VOID
//  NtfsAcquireFcbSecurity (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      );
//
//  VOID
//  NtfsReleaseFcbSecurity (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsAcquireHashTable (
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsReleaseHashTable (
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsAcquireCheckpoint (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb,
//      );
//
//  VOID
//  NtfsReleaseCheckpoint (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsWaitOnCheckpointNotify (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsSetCheckpointNotify (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsResetCheckpointNotify (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsAcquireReservedClusters (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsReleaseReservedClusters (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsAcquireUsnNotify (
//      IN PVCB Vcb
//      );
//
//  VOID
//  NtfsDeleteUsnNotify (
//      IN PVCB Vcb
//      );
//
//  VOID NtfsAcquireFsrtlHeader (
//      IN PSCB Scb
//      );
//
//  VOID NtfsReleaseFsrtlHeader (
//      IN PSCB Scb
//      );
//
//  VOID
//  NtfsReleaseVcb (
//      IN PIRP_CONTEXT IrpContext,
//      IN PVCB Vcb
//      );
//

VOID
NtfsReleaseVcbCheckDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN UCHAR MajorCode,
    IN PFILE_OBJECT FileObject OPTIONAL
    );

#define NtfsAcquireSharedScb(IC,S) {                \
    NtfsAcquireSharedFcb((IC),(S)->Fcb, S, 0);      \
}

#define NtfsReleaseScb(IC,S) {     \
    NtfsReleaseFcb((IC),(S)->Fcb); \
}

#define NtfsReleaseGlobal(IC) {              \
    ExReleaseResourceLite( &NtfsData.Resource ); \
}

#define NtfsAcquireFcbTable(IC,V) {                         \
    ExAcquireFastMutexUnsafe( &(V)->FcbTableMutex );        \
}

#define NtfsReleaseFcbTable(IC,V) {                         \
    ExReleaseFastMutexUnsafe( &(V)->FcbTableMutex );        \
}

#define NtfsLockVcb(IC,V) {                                 \
    ExAcquireFastMutexUnsafe( &(V)->FcbSecurityMutex );     \
}

#define NtfsUnlockVcb(IC,V) {                               \
    ExReleaseFastMutexUnsafe( &(V)->FcbSecurityMutex );     \
}

#define NtfsLockFcb(IC,F) {                                 \
    ExAcquireFastMutexUnsafe( (F)->FcbMutex );              \
}

#define NtfsUnlockFcb(IC,F) {                               \
    ExReleaseFastMutexUnsafe( (F)->FcbMutex );              \
}

#define NtfsAcquireFcbSecurity(V) {                         \
    ExAcquireFastMutexUnsafe( &(V)->FcbSecurityMutex );     \
}

#define NtfsReleaseFcbSecurity(V) {                         \
    ExReleaseFastMutexUnsafe( &(V)->FcbSecurityMutex );     \
}

#define NtfsAcquireHashTable(V) {                           \
    ExAcquireFastMutexUnsafe( &(V)->HashTableMutex );       \
}

#define NtfsReleaseHashTable(V) {                           \
    ExReleaseFastMutexUnsafe( &(V)->H PV$ àˇˇ        OE  Íˇˇ       define NtfsAcquireCheckpoint(IC,V) {                       \
    ExAcquireFastMutexUnsafe( &(V)->CheckpointMutex );      \
}

#define NtfsReleaseCheckpoint(IC,V) {                       \
    ExReleaseFastMutexUnsafe( &(V)->CheckpointMutex );      \
}

#define NtfsWaitOnCheckpointNotify(IC,V) {                          \
    NTSTATUS _Status;                                               \
    _Status = KeWaitForSingleObject( &(V)->CheckpointNotifyEvent,   \
                                     Executive,                     \
                                     KernelMode,                    \
                                     FALSE,                         \
                                     NULL );                        \
    if (!NT_SUCCESS( _Status )) {                                   \
        NtfsRaiseStatus( IrpContext, _Status, NULL, NULL );         \
    }                                                               \
}

#define NtfsSetCheckpointNotify(IC,V) {                             \
    (V)->CheckpointOwnerThread = NULL;                              \
    KeSetEvent( &(V)->CheckpointNotifyEvent, 0, FALSE );            \
}

#define NtfsResetCheckpointNotify(IC,V) {                           \
    (V)->CheckpointOwnerThread = (PVOID) PsGetCurrentThread();      \
    KeClearEvent( &(V)->CheckpointNotifyEvent );                    \
}

#define NtfsAcquireUsnNotify(V) {                           \
    ExAcquireFastMutex( &(V)->CheckpointMutex );            \
}

#define NtfsReleaseUsnNotify(V) {                           \
    ExReleaseFastMutex( &(V)->CheckpointMutex );            \
}

#define NtfsAcquireReservedClusters(V) {                    \
    ExAcquireFastMutexUnsafe( &(V)->ReservedClustersMutex );\
}

#define NtfsReleaseReservedClusters(V) {                    \
    ExReleaseFastMutexUnsafe( &(V)->ReservedClustersMutex );\
}

#define NtfsAcquireFsrtlHeader(S) {                         \
    ExAcquireFastMutex((S)->Header.FastMutex);              \
}

#define NtfsReleaseFsrtlHeader(S) {                         \
    ExReleaseFastMutex((S)->Header.FastMutex);              \
}

#ifdef NTFSDBG

VOID NtfsReleaseVcb(
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );

#else

#define NtfsReleaseVcb(IC,V) {                              \
    ExReleaseResourceLite( &(V)->Resource );                    \
}

#endif

//
//  Macros to test resources for exclusivity.
//

#define NtfsIsExclusiveResource(R) (                            \
    ExIsResourceAcquiredExclusiveLite(R)                        \
)

#define NtfsIsExclusiveFcb(F) (                                 \
    (NtfsIsExclusiveResource((F)->Resource))                    \
)

#define NtfsIsExclusiveFcbPagingIo(F) (                         \
    (NtfsIsExclusiveResource((F)->PagingIoResource))            \
)

#define NtfsIsExclusiveScbPagingIo(S) (                         \
    (NtfsIsExclusiveFcbPagingIo((S)->Fcb))                      \
)

#define NtfsIsExclusiveScb(S) (                                 \
    (NtfsIsExclusiveFcb((S)->Fcb))                              \
)

#define NtfsIsExclusiveVcb(V) (                                 \
    (NtfsIsExclusiveResource(&(V)->Resource))                   \
)

//
//  Macros to test resources for shared acquire
//

#define NtfsIsSharedResource(R) (                               \
    ExIsResourceAcquiredSharedLite(R)                           \
)

#define NtfsIsSharedFcb(F) (                                    \
    (NtfsIsSharedResource((F)->Resource))                       \
)

#define NtfsIsSharedFcbPagingIo(F) (                            \
    (NtfsIsSharedResource((F)->PagingIoResource))               \
)

#define NtfsIsSharedScbPagingIo(S) (                            \
    (NtfsIsSharedFcbPagingIo((S)->Fcb))                         \
)

#define NtfsIsSharedScb(S) (                                    \
    (NtfsIsSharedFcb((S)->Fcb))                                 \
)

#define NtfsIsSharedVcb(V) (                                    \
    (NtfsIsSharedResource(&(V)->Resource))                      \
)

__inline
VOID
NtfsReleaseExclusiveScbIfOwned(
    IN PIRP_CONTEXT IrpContext,
    IN PSCB Scb
    )
/*++

Routine Description:

    This routine is called release an Scb that may or may not be currently
    owned exclusive.

Arguments:

    IrpContext - Context of call

    Scb - Scb to be released

Return Value:

    None.

--*/
{
    if (Scb->Fcb->ExclusiveFcbLinks.Flink != NULL &&
        NtfsIsExclusiveScb( Scb )) {

        NtfsReleaseScb( IrpContext, Scb );
    }
}

//
//  The following are cache manager call backs.  They return FALSE
//  if the resource cannot be acquired with waiting and wait is false.
//

BOOLEAN
NtfsAcquireScbForLazyWrite (
    IN PVOID Null,
    IN BOOLEAN Wait
    );

VOID
NtfsReleaseScbFromLazyWrite (
    IN PVOID Null
    );

NTSTATUS
NtfsAcquireFileForModWrite (
    IN PFILE_OBJECT FileObject,
    IN PLARGE_INTEGER EndingOffset,
    OUT PERESOURCE *ResourceToRelease,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
NtfsAcquireFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
NtfsReleaseFileForCcFlush (
    IN PFILE_OBJECT FileObject,
    IN PDEVICE_OBJECT DeviceObject
    );

VOID
NtfsAcquireForCreateSection (
    IN PFILE_OBJECT FileObject
    );

VOID
NtfsReleaseForCreateSection (
    IN PFILE_OBJECT FileObject
    );


BOOLEAN
NtfsAcquireScbForReadAhead (
    IN PVOID Null,
    IN BOOLEAN Wait
    );

VOID
NtfsReleaseScbFromReadAhead (
    IN PVOID Null
    );

BOOLEAN
NtfsAcquireVolumeFileForLazyWrite (
    IN PVOID Vcb,
    IN BOOLEAN Wait
    );

VOID
NtfsReleaseVolumeFileFromLazyWrite (
    IN PVOID Vcb
    );


//
//  Ntfs Logging Routine interfaces in RestrSup.c
//

BOOLEAN
NtfsRestartVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    OUT PBOOLEAN UnrecognizedRestart
    );

VOID
NtfsAbortTransaction (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb,
    IN PTRANSACTION_ENTRY Transaction OPTIONAL
    );

NTSTATUS
NtfsCloseAttributesFromRestart (
    IN PIRP_CONTEXT IrpContext,
    IN PVCB Vcb
    );


//
//  Security support routines, implemented in SecurSup.c
//

//
//  VOID
//  NtfsTraverseCheck (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB ParentFcb,
//      IN PIRP Irp
//      );
//
//  VOID
//  NtfsOpenCheck (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB Fcb,
//      IN PFCB ParentFcb OPTIONAL,
//      IN PIRP Irp
//      );
//
//  VOID
//  NtfsCreateCheck (
//      IN PIRP_CONTEXT IrpContext,
//      IN PFCB ParentFcb,
//      IN PIRP Irp
//      );
//

#define NtfsTraverseCheck(IC,F,IR) { \
    NtfsAccessCheck( IC,             \
                     F,              \
                     NULL,           \
                     IR,             \
                     FILE_TRAVERSE,  \
                     TRUE );         \
}

#define NtfsOpenCheck(IC,F,PF,IR) {                                                                      \
    NtfsAccessCheck( IC,                                                                                 \
                     F,                                                                                  \
                     PF,                                                                                 \
                     IR,                                                                                 \
                     IoGetCurrentIrpStackLocation(IR)->Parameters.Create.SecurityContext->DesiredAccess, \
                     FALSE );                                                                            \
}

#define NtfsCreateCheck(IC,PF,IR) {                                                                              \
    NtfsAccessCheck( IC,                                                                                         \
                     PF,                                                                                         \
                     NULL,                                                                                       \
                     IR,                                                                                         \
                     (FlagOn(IoGetCurrentIrpStackLocation(IR)->Parameters.Create.Options, FILE_DIRECTORY_FILE) ? \
                        FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE),                                                  \
                     TRUE );                                                                                     \
}

VOID
NtfsAssignSecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ParentFcb,
    IN PIRP Irp,
    IN PFCB NewFcb,
    IN PFILE_RECORD_SEGMENT_HEADER FileRecord,
    IN PBCB FileRecordBcb,
    IN LONGLONG FileOffset,
    IN OUT PBOOLEAN LogIt
    );

NTSTATUS
NtfsModifySecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor
    );

NTSTATUS
NtfsQuerySecurity (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PSECURITY_INFORMATION SecurityInformation,
    OUT PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN OUT PULONG SecurityDescriptorLength
    );

VOID
NtfsAccessCheck (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb,
    IN PFCB ParentFcb OPTIONAL,
    IN PIRP Irp,
    IN ACCESS_MASK DesiredAccess,
    IN BOOLEAN CheckOnly
    );

BOOLEAN
NtfsCanAdministerVolume (
    IN PIRP_CONTEXT IrpContext,
    IN PIRP Irp,
    IN PFCB Fcb,
    IN PSECURITY_DESCRIPTOR TestSecurityDescriptor OPTIONAL,
    IN PULONG TestDesiredAccess OPTIONAL
    );

NTSTATUS
NtfsCheckFileForDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PSCB ParentScb,
    IN PFCB ThisFcb,
    IN BOOLEAN FcbExisted,
    IN PINDEX_ENTRY IndexEntry
    );

VOID
NtfsCheckIndexForAddOrDelete (
    IN PIRP_CONTEXT IrpContext,
    IN PFCB ParentFcb,
    IN ACCESS_MASK DesiredAccess,
    IN ULONG CreatePrivileges
    );

VOID
NtfsSetFcbSecurityFromDescriptor (
    IN PIRP_CONTEXT IrpContext,
    IN OUT PFCB Fcb,
    IN PSECURITY_DESCRIPTOR SecurityDescriptor,
    IN ULONG SecurityDescriptorLength,
    IN BOOLEAN RaiseIfInvalid
    );

INLINE
VOID
RemoveReferenceSharedSecurityUnsafe (
    IN OUT PSHARED_SECURITY *SharedSecurity
    )
/*++

Routine Description:

    This routine is called to manage the reference count on a shared security
    descriptor.  If the reference count goes to zero, the shared security is
    freed.

Arguments:

    SharedSecurity - security that is being dereferenced.

Return Value:

    None.

--*/
{
    DebugTrace( 0, (DEBUG_TRACE_SECURSUP | DEBUG_TRACE_ACLINDEX),
                ( "RemoveReferenceSharedSecurityUnsafe( %08x )\n", *SharedSecurity ));
    //
    //  Note that there will be one less reference shortly
    //

    ASSERT( (*SharedSecurity)->ReferenceCount != 0 );

    (*SharedSecurity)->ReferenceCount--;

    if ((*SharedSecurity)->ReferenceCount == 0) {
        DebugTrace( 0, (DEBUG_TRACE_SECURSUP | DEBUG_TRACE_ACLINDEX),
                    ( "RemoveReferenceSharedSecurityUnsafe freeing\n" ));
        NtfsFreePool( *SharedSecurity );
    }
    *SharedSecurity = NULL;
}

BOOLEAN
NtfsNotifyTraverseCheck (
    IN PCCB Ccb,
    IN PFCB Fcb,
    IN PSECURITY_SUBJECT_CONTEXT SubjectContext
    );

VOID
NtfsLoadSecurityDescriptor (
    PIRP_CONTEXT IrpContext,
    IN PFCB Fcb
    );

VOID
NtfsStoreSecurityDescript