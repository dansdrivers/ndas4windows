/  It is maintained in the running system.
//

#pragma pack(4)

typedef struct _OPEN_ATTRIBUTE_ENTRY_V0 {

    //
    //  Entry is allocated if this field contains RESTART_ENTRY_ALLOCATED.
    //  Otherwise, it is a free link.
    //

    ULONG AllocatedOrNextFree;

    //
    //  Placeholder for Scb in V0.  We use it to point to the index
    //  in the in-memory structure.
    //

    ULONG OatIndex;

    //
    //  File Reference of file containing attribute.
    //

    FILE_REFERENCE FileReference;

    //
    //  Lsn of OpenNonresidentAttribute log record, to distinguish reuses
    //  of this open file record.  Log records referring to this Open
    //  Attribute Entry Index, but with Lsns  older than this field, can
    //  only occur when the attribute was subsequently deleted - these
    //  log records can be ignored.
    //

    LSN LsnOfOpenRecord;

    //
    //  Flag to say if dirty pages seen for this attribute during dirty
    //  page scan.
    //

    BOOLEAN DirtyPagesSeen;

    //
    //  Flag to indicate if the pointer in Overlay above is to an Scb or
    //  attribute name.  It is only used during restart when cleaning up
    //  the open attribute table.
    //

    BOOLEAN AttributeNamePresent;

    //
    //  Reserved for alignment
    //

    UCHAR Reserved[2];

    //
    //  The following two fields identify the actual attribute
    //  with respect to its file.   We identify the attribute by
    //  its type code and name.  When the Restart Area is written,
    //  all of the names for all of the open attributes are temporarily
    //  copied to the end of the Restart Area.
    //  The name is not used on disk but must be a 64-bit value.
    //

    ATTRIBUTE_TYPE_CODE AttributeTypeCode;
    LONGLONG AttributeName;

    //
    //  This field is only relevant to indices, i.e., if AttributeTypeCode
    //  above is $INDEX_ALLOCATION.
    //

    ULONG BytesPerIndexBuffer;

} OPEN_ATTRIBUTE_ENTRY_V0, *POPEN_ATTRIBUTE_ENTRY_V0;

#pragma pack()

#define SIZEOF_OPEN_ATTRIBUTE_ENTRY_V0 (                                \
    FIELD_OFFSET( OPEN_ATTRIBUTE_ENTRY_V0, BytesPerIndexBuffer ) + 4    \
)

//
//  Auxiliary OpenAttribute data.  This is the data that doesn't need to be
//  logged.
//

typedef struct OPEN_ATTRIBUTE_DATA {

    //
    //  Queue of these structures attached to the Vcb.
    //  NOTE - This must be the first entry in this structure.
    //

    LIST_ENTRY Links;

    //
    //  Index for this entry in the On-disk open attribute table.
    //

    ULONG OnDiskAttributeIndex;

    BOOLEAN AttributeNamePresent;

    //
    //  The following overlay either contains an optional pointer to an
    //  Attribute Name Entry from the Analysis Phase of Restart, or a
    //  pointer to an Scb once attributes have been open and in the normal
    //  running system.
    //
    //  Specifically, after the Analysis Phase of Restart:
    //
    //      AttributeName == NULL if there is no attribute name, or the
    //                       attribute name was captured in the Attribute
    //                       Names Dump in the last successful checkpoint.
    //      AttributeName != NULL if an OpenNonresidentAttribute log record
    //                       was encountered, and an Attribute Name Entry
    //                       was allocated at that time (and must be
    //                       deallocated when no longer needed).
    //
    //  Once the Nonresident Attributes have been opened during Restart,
    //  and in the running system, this is an Scb pointer.
    //

    union {
        PWSTR AttributeName;
        PSCB Scb;
    } Overlay;

    //
    //  Store the unicode string for the attribute name.
    //

    UNICODE_STRING AttributeName;

} OPEN_ATTRIBUTE_DATA, *POPEN_ATTRIBUTE_DATA;

//
//  Open Attribute Table.  This is the on-disk structure for version 1 and
//  it is the version we always use in-memory.
//
//  One entry exists in the Open Attribute Table for each nonresident
//  attribute of each file that is open with modify access.
//
//  This table is initialized at Restart to the maximum of
//  DEFAULT_ATTRIBUTE_TABLE_SIZE or the size of the table in the log file.
//  It is maintained in the running system.
//

typedef struct _OPEN_ATTRIBUTE_ENTRY {

    //
    //  Entry is allocated if this field contains RESTART_ENTRY_ALLOCATED.
    //  Otherwise, it is a free link.
    //

    ULONG AllocatedOrNextFree;

    //
    //  This field is only relevant to indices, i.e., if AttributeTypeCode
    //  above is $INDEX_ALLOCATION.
    //

    ULONG BytesPerIndexBuffer;

    //
    //  The following two fields identify the actual attribute
    //  with respect to its file.   We identify the attribute by
    //  its t