/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPXTYPES_
#define _LPXTYPES_



//
// in lpxdrvr.c
//

extern UNICODE_STRING LpxRegistryPath;

//
// We need the driver object to create device context structures.
//

extern PDRIVER_OBJECT LpxDriverObject;

//
// This is a list of all the device contexts that LPX owns,
// used while unloading.
//

extern LIST_ENTRY LpxDeviceList;

//
// And a lock that protects the global list of LPX devices
//
extern FAST_MUTEX LpxDevicesLock;

#define INITIALIZE_DEVICES_LIST_LOCK()                                  \
    ExInitializeFastMutex(&LpxDevicesLock)

#define ACQUIRE_DEVICES_LIST_LOCK()                                     \
    ACQUIRE_FAST_MUTEX_UNSAFE(&LpxDevicesLock)

#define RELEASE_DEVICES_LIST_LOCK()                                     \
    RELEASE_FAST_MUTEX_UNSAFE(&LpxDevicesLock)

//
// A handle to be used in all provider notifications to TDI layer
// 
extern HANDLE LpxProviderHandle;

//
// Global Configuration block for the driver ( no lock required )
// 
extern PCONFIG_DATA   LpxConfig;

//
// This defines the TP_SEND_IRP_PARAMETERS, which is masked onto the
// Parameters section of a send IRP's stack location.
//

typedef union _LPX_TDI_REQUEST_KERNEL_SEND_DUMMY {
	TDI_REQUEST_KERNEL_SEND SendReqeust;
	TDI_REQUEST_KERNEL_SENDDG SendDgramReqeust;
} LPX_TDI_REQUEST_KERNEL_SEND_DUMMY, *PLPX_TDI_REQUEST_KERNEL_SEND_DUMMY;

typedef struct _TP_SEND_IRP_PARAMETERS {
    LPX_TDI_REQUEST_KERNEL_SEND_DUMMY Request_dummy; // this member should not touched by lpx driver.
    LONG ReferenceCount;
    PVOID Irp;
} TP_SEND_IRP_PARAMETERS, *PTP_SEND_IRP_PARAMETERS;

#define IRP_SEND_REFCOUNT(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_SEND_IRP(_IrpSp) \
    (((PTP_SEND_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Irp)

#define IRP_SEND_CONNECTION(_IrpSp) \
    ((PTP_CONNECTION)((_IrpSp)->FileObject->FsContext))

#define IRP_DEVICE_CONTEXT(_IrpSp) \
    ((PDEVICE_CONTEXT)((_IrpSp)->DeviceObject))

//
// This defines the TP_RECEIVE_IRP_PARAMETERS, which is masked onto the
// Parameters section of a receive IRP's stack location.
//

typedef struct _TP_RECEIVE_IRP_PARAMETERS {
    TDI_REQUEST_KERNEL_RECEIVE Request;
    LONG ReferenceCount;
    PIRP Irp;
} TP_RECEIVE_IRP_PARAMETERS, *PTP_RECEIVE_IRP_PARAMETERS;

#define IRP_RECEIVE_REFCOUNT(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->ReferenceCount)

#define IRP_RECEIVE_IRP(_IrpSp) \
    (((PTP_RECEIVE_IRP_PARAMETERS)&(_IrpSp)->Parameters)->Irp)

#define IRP_RECEIVE_CONNECTION(_IrpSp) \
    ((PTP_CONNECTION)((_IrpSp)->FileObject->FsContext))



//
// This structure defines a TP_CONNECTION, or active transport connection,
// maintained on a transport address.
//

#if DBG
#define CREF_SPECIAL_CREATION 0
#define CREF_SPECIAL_TEMP 1
#define CREF_SEND_IRP 2
#define CREF_BY_ID 3
#define CREF_REQUEST 4
#define CREF_TEMP 5
#define CREF_PROCESS_DATA 6
#define CREF_WORKDPC            7
#define CREF_TIMERDPC           8

#define NUMBER_OF_CREFS 9
#endif

//
// This structure holds our "complex send pointer" indicating
// where we are in the packetization of a send.
//

typedef struct _TP_CONNECTION {

	SERVICE_POINT 	ServicePoint; // for lpx.

#if DBG
	LONG RefTypes[NUMBER_OF_CREFS];
#endif

	CSHORT Type;
	USHORT Size;

#if 0 // No connection pool
	LIST_ENTRY LinkList;                // used for link thread or for free
                                        // resource list
#endif
	KSPIN_LOCK TpConnectionSpinLock;	// spinlock for connection protection.

	LONG ReferenceCount;				// number of references to this object.
	LONG SpecialRefCount;				// controls freeing of connection.

	//
	// The following lists are used to associate this connection with a
	// particular address.
	//

     LIST_ENTRY AddressFileList;		// list for connections bound to a
										// given address reference

    //
    // The following field points to the TP_LINK object that describes the
    // (active) data link connection for this transport connection.  To be
    // valid, this field is non-NULL.
    //

    struct _TP_ADDRESS_FILE *AddressFile;   // pointer to owning Address.
    struct _CONTROL_CONTEXT *Provider;       // device context to which we are attached.
    PKSPIN_LOCK ProviderInterlock;          // &Provider->Interlock
    PFILE_OBJECT FileObject;                // easy backlink to file object.(But not used)

    //
    // The following field contains the actual ID we expose to the TDI client
    // to represent this connection.  A unique one is created from the address.
    //

    //
    // The following field is specified by the user at connection open time.
    // It is the context that the user associates with the connection so that
    // indications to and from the client can be associated with a particular
    // connection.
    //

    CONNECTION_CONTEXT Context;         // client-specified value.

    //
    // If the connection is being disconnected as a result of
    // a TdiDisconnect call (RemoteDisconnect is FALSE) then this
    // will hold the IRP passed to TdiDisconnect. It is needed
    // when the TdiDisconnect request is completed.
    //

    PIRP DisconnectIrp;

    //
    // If the connection is being closed, this will hold
    // the IRP passed to TdiCloseConnection. It is needed
    // when the request is completed.
    //

    PIRP CloseIrp;

    //
    // The following fields are used for connection housekeeping.
    //

    ULONG Flags2;                       // attributes guarded by TpConnectionSpinLock
 
    LPX_ADDRESS CalledAddress;			// TdiConnect request's T.A.
} TP_CONNECTION, *PTP_CONNECTION;

#define CONNECTION_FLAGS2_STOPPING      0x00000001 // connection is running down.
#define CONNECTION_FLAGS2_CLOSING       0x00000010 // connection is closing
#define CONNECTION_FLAGS2_ASSOCIATED    0x00000020 // associated with address
#define CONNECTION_FLAGS2_DISASSOCIATED 0x00000200 // associate CRef has been removed


//
// This structure is pointed to by the FsContext field in the FILE_OBJECT
// for this Address.  This structure is the base for all activities on
// the open file object within the transport provider.  All active connections
// on the address point to this structure, although no queues exist here to do
// work from. This structure also maintains a reference to a TP_ADDRESS
// structure, which describes the address that it is bound to. Thus, a
// connection will point to this structure, which describes the address the
// connection was associated with. When the address file closes, all connections
// opened on this address file get closed, too. Note that this may leave an
// address hanging around, with other references.
//

typedef struct _TP_ADDRESS_FILE {

    CSHORT Type;
    CSHORT Size;

    LIST_ENTRY Linkage;                 // next address file on this address.
                                        // also used for linkage in the
                                        // look-aside list

    LONG ReferenceCount;                // number of references to this object.

    //
    // This structure is edited after taking the Address spinlock for the
    // owning address. This ensures that the address and this structure
    // will never get out of synchronization with each other.
    //

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per connection open on this address.  This list of connections
    // is used to help the cleanup process if a process closes an address
    // before disassociating all connections on it. By design, connections
    // will stay around until they are explicitly
    // closed; we use this database to ensure that we clean up properly.
    //

    LIST_ENTRY ConnectionDatabase;      // list of defined transport connections.

    //
    // the current state of the address file structure; this is either open or
    // closing
    //

    UCHAR State;

    //
    // The following fields are kept for housekeeping purposes.
    //

    PIRP Irp;                           // the irp used for open or close
    struct _TP_ADDRESS *Address;        // address to which we are bound.
    PFILE_OBJECT FileObject;            // easy backlink to file object.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following queue is used to queue receive datagram requests
    // on this address file. Send datagram requests are queued on the
    // address itself. These queues are managed by the EXECUTIVE interlocked
    // list management routines. The actual objects which get queued to this
    // structure are request control blocks (RCBs).
    //

    LIST_ENTRY ReceiveDatagramQueue;    // FIFO of outstanding TdiReceiveDatagrams.

    //
    // This holds the Irp used to close this address file,
    // for pended completion.
    //

    PIRP CloseIrp;

    //
    // handler for kernel event actions. First we have a set of booleans that
    // indicate whether or not this address has an event handler of the given
    // type registered.
    //

    BOOLEAN RegisteredConnectionHandler; // connection handler is not imple?
    BOOLEAN RegisteredDisconnectHandler;
    BOOLEAN RegisteredReceiveHandler;
    BOOLEAN RegisteredReceiveDatagramHandler;
    BOOLEAN RegisteredExpeditedDataHandler;
    BOOLEAN RegisteredErrorHandler;

    //
    // This function pointer points to a connection indication handler for this
    // Address. Any time a connect request is received on the address, this
    // routine is invoked.
    //
    //

    PTDI_IND_CONNECT ConnectionHandler;
    PVOID ConnectionHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_DISCONNECT
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_DISCONNECT DisconnectHandler;
    PVOID DisconnectHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE
    // event handler for connections on this address.  If the NULL handler
    // is specified in a TdiSetEventHandler, then this points to an internal
    // routine which does not accept the incoming data.
    //

    PTDI_IND_RECEIVE ReceiveHandler;
    PVOID ReceiveHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_RECEIVE_DATAGRAM
    // event handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which does
    // not accept the incoming data.
    //

    PTDI_IND_RECEIVE_DATAGRAM ReceiveDatagramHandler;
    PVOID ReceiveDatagramHandlerContext;

    //
    // An expedited data handler. This handler is used if expedited data is
    // expected; it never is in LPX, thus this handler should always point to
    // the default handler.
    //

    PTDI_IND_RECEIVE_EXPEDITED ExpeditedDataHandler;
    PVOID ExpeditedDataHandlerContext;

    //
    // The following function pointer always points to a TDI_IND_ERROR
    // handler for the address.  If the NULL handler is specified in a
    // TdiSetEventHandler, this this points to an internal routine which
    // simply returns successfully.
    //

    PTDI_IND_ERROR ErrorHandler;
    PVOID ErrorHandlerContext;


} TP_ADDRESS_FILE, *PTP_ADDRESS_FILE;

#define ADDRESSFILE_STATE_OPENING   0x00    // not yet open for business
#define ADDRESSFILE_STATE_OPEN      0x01    // open for business
#define ADDRESSFILE_STATE_CLOSING   0x02    // closing


//
// This structure defines a TP_ADDRESS, or active transport address,
// maintained by the transport provider.  It contains all the visible
// components of the address (such as the TSAP and network name components),
// and it also contains other maintenance parts, such as a reference count,
// ACL, and so on. All outstanding connection-oriented and connectionless
// data transfer requests are queued here.
//

#if DBG
#define AREF_TEMP_CREATE        0
#define AREF_OPEN               1
#define AREF_VERIFY            2
#define AREF_LOOKUP             3
#define AREF_CONNECTION         4
#define AREF_REQUEST            5

#define NUMBER_OF_AREFS        6
#endif

typedef struct _TP_ADDRESS {

#if DBG
    ULONG RefTypes[NUMBER_OF_AREFS];
#endif

    USHORT Size;
    CSHORT Type;

    LIST_ENTRY Linkage;                 // next address/this device object.
    LONG ReferenceCount;                // number of references to this object.

    //
    // The following spin lock is acquired to edit this TP_ADDRESS structure
    // or to scan down or edit the list of address files.
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this structure.

    //
    // The following fields comprise the actual address itself.
    //

    PLPX_ADDRESS NetworkName;    // this address

    //
    // The following fields are used to maintain state about this address.
    //

    ULONG Flags;                        // attributes of the address.
    struct _DEVICE_CONTEXT *Provider;   // device context to which we are attached.

    //
    // The following field points to a list of TP_CONNECTION structures,
    // one per active, connecting, or disconnecting connections on this
    // address.  By definition, if a connection is on this list, then
    // it is visible to the client in terms of receiving events and being
    // able to post requests by naming the ConnectionId.  If the connection
    // is not on this list, then it is not valid, and it is guaranteed that
    // no indications to the client will be made with reference to it, and
    // no requests specifying its ConnectionId will be accepted by the transport.
    //

    LIST_ENTRY AddressFileDatabase; // list of defined address file objects

    //
    // This structure is used for checking share access.
    //

    SHARE_ACCESS ShareAccess;

    //
    // Used for delaying LpxDestroyAddress to a thread so
    // we can access the security descriptor.
    //

    PIO_WORKITEM  DestroyAddressQueueItem;


    //
    // This structure is used to hold ACLs on the address.

    PSECURITY_DESCRIPTOR SecurityDescriptor;

    LIST_ENTRY				ConnectionServicePointList;	 // added for lpx

} TP_ADDRESS, *PTP_ADDRESS;

#define ADDRESS_FLAGS_STOPPING          0x00000040 // TpStopAddress is in progress.


//
// This structure defines the DEVICE_OBJECT and its extension allocated at
// the time the transport provider creates its device object.
//

#if DBG
#define DCREF_CREATION    0
#define DCREF_ADDRESS     1
#define DCREF_CONNECTION  2
#define DCREF_TEMP_USE    3

#define NUMBER_OF_DCREFS 4
#endif


typedef struct _DEVICE_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
#if 1 /* Added for lpx */
	USHORT				LastPortNum;

	//
	// Packet descriptor pool handle
	//

	NDIS_HANDLE         LpxPacketPool;

	//
	// Packet buffer descriptor pool handle
	//

	NDIS_HANDLE			LpxBufferPool;

	// Received packet.
	KSPIN_LOCK			PacketInProgressQSpinLock;
	LIST_ENTRY			PacketInProgressList;

	BOOL				bDeviceInit;
#endif

#if DBG
    ULONG RefTypes[NUMBER_OF_DCREFS];
#endif

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    LIST_ENTRY DeviceListLinkage;                   // links them on LpxDeviceList
                                        
    LONG ReferenceCount;                // activity count/this provider.
    LONG CreateRefRemoved;              // has unload or unbind been called ?


    //
    // Following are protected by Global Device Context SpinLock
    //

    KSPIN_LOCK SpinLock;                // lock to manipulate this object.
                                        //  (used in KeAcquireSpinLock calls)

    //
    // the device context state, among open, closing
    //

    UCHAR State;

    //
    // Used when processing a STATUS_CLOSING indication.
    //

    PIO_WORKITEM StatusClosingQueueItem;
    
    //
    // The following queue holds free TP_ADDRESS objects available for allocation.
    //

    LIST_ENTRY AddressPool;

    //
    // These counters keep track of resources uses by TP_ADDRESS objects.
    //

    ULONG AddressAllocated;
    ULONG AddressInitAllocated;
    ULONG AddressMaxAllocated;
    ULONG AddressInUse;
    ULONG AddressMaxInUse;
    ULONG AddressExhausted;
    ULONG AddressTotal;


    //
    // The following queue holds free TP_ADDRESS_FILE objects available for allocation.
    //

    LIST_ENTRY AddressFilePool;

    //
    // These counters keep track of resources uses by TP_ADDRESS_FILE objects.
    //

    ULONG AddressFileAllocated;
    ULONG AddressFileInitAllocated;
    ULONG AddressFileMaxAllocated;
    ULONG AddressFileInUse;
    ULONG AddressFileMaxInUse;
    ULONG AddressFileTotal;

    //
    // The following field is a head of a list of TP_ADDRESS objects that
    // are defined for this transport provider.  To edit the list, you must
    // hold the spinlock of the device context object.
    //

    LIST_ENTRY AddressDatabase;        // list of defined transport addresses.
 
    //
    // following is used to keep adapter information.
    //

    NDIS_HANDLE NdisBindingHandle;

    ULONG MaxConnections;
    ULONG MaxAddressFiles;
    ULONG MaxAddresses;
    PWCHAR DeviceName;
    ULONG DeviceNameLength;

    //
    // This is the Mac type we must build the packet header for and know the
    // offsets for.
    //

    ULONG MaxReceivePacketSize;         // does not include the MAC header
    ULONG MaxSendPacketSize;            // includes the MAC header

    ULONG MaxUserData;
    //
    // some MAC addresses we use in the transport
    //

    HARDWARE_ADDRESS LocalAddress;      // our local hardware address.

    HANDLE TdiDeviceHandle;
    HANDLE ReservedAddressHandle;

    //
    // These are used while initializing the MAC driver.
    //

    KEVENT NdisRequestEvent;            // used for pended requests.
    NDIS_STATUS NdisRequestStatus;      // records request status.

    //
    // This information is used to keep track of the speed of
    // the underlying medium.
    //

    ULONG MediumSpeed;                    // in units of 100 bytes/sec

	//
	//	General Mac options supplied by underlying NIC drivers
	//

	ULONG	MacOptions;

	//
    // Counters for most of the statistics that LPX maintains;
    // some of these are kept elsewhere. Including the structure
    // itself wastes a little space but ensures that the alignment
    // inside the structure is correct.
    //

    TDI_PROVIDER_STATISTICS Statistics;

    //
    // This resource guards access to the ShareAccess
    // and SecurityDescriptor fields in addresses.
    //

    ERESOURCE AddressResource;

    //
    // This is to hold the underlying PDO of the device so
    // that we can answer DEVICE_RELATION IRPs from above
    //

    PVOID PnPContext;

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;



typedef struct _CONTROL_CONTEXT {

    DEVICE_OBJECT DeviceObject;         // the I/O system's device object.
    BOOL				bDeviceInit;

#if DBG
    ULONG RefTypes[NUMBER_OF_DCREFS];
#endif

    CSHORT Type;                          // type of this structure
    USHORT Size;                          // size of this structure

    KSPIN_LOCK Interlock;               // GLOBAL spinlock for reference count.
                                        //  (used in ExInterlockedXxx calls)
                                        
    LONG ReferenceCount;                // activity count/this provider.
    LONG CreateRefRemoved;              // has unload or unbind been called ?

    //
    // Following are protected by Global Device Co ∞32 àˇˇ       — &} æ® # +@   @÷ö Ö¬         # ,@   z÷ö ó  Ω } # -@       œ  )} } # .@    ≠÷ö ˙  *} zx # /@       Ñ
  +} x # 0@        &  ,} ä] # 1@       &      x] # 2@    Â÷ö ó  .} '} # 3@   È÷ö f%  /} á| # 4@   ˆ÷ö u1  0}  { # 5@       òF 1} Ô{ # 6@    ˙÷ö   2} } # 7@       _   3}  } # 8@        z  Ω ‰\ # 9@    2◊ö ó  5} -} # :@       ä¬ 6}     # ;@    \◊ö ã¬ "Ω     # <@   ó◊ö ó  8} 4} # =@   õ◊ö ò  9} N| # >@   °◊ö ¸  :} É| # ?@       AŒ  ;} êd # @@    ™◊ö ¨
  <} [{ # A@   ∞◊ö BŒ  =} Õ[ # B@   ∑◊ö „  >} é| # C@   ø◊ö £Ø  ?} ßO # D@       å¬ @}     # E@    ﬁ◊ö DŒ  A} .‘ # F@   ‰◊ö ú;  B} ◊c # G@   Í◊ö uX C} @| # H@   Ì◊ö   D} D| # I@       h!  E} ©O # J@        KŒ  F} ◊{ # K@    ˆ◊ö Á  G} ë| # L@   ¸◊ö ¿  H} ≥{ # M@       MŒ  I} bÇ # N@        NŒ  J} J‘ # O@        ‡3     ]Ç # P@    ÿö ó  L} 7} # Q@   ÿö 	  M} } # R@       
  N} } # S@        ]  O} Ÿo # T@       9  =Ω Ty # U@        9  Q} O} # V@        oŒ  R} o # W@        pŒ  BΩ  o # X@    Äÿö ó  T} K} # Y@       FŒ  U} Æ’ # Z@       GŒ  GΩ Ø’ # [@    ∞ÿö ó  W} S} # \@       æ  X} T| # ]@        h;  Y} Œ% # ^@        á
  Z} Ñ{ # _@        2  NΩ i] # `@    Áÿö ó  \} V} # a@       ê¬ ]}     # b@        É  SΩ ˘| # c@        ≤
  _} —{ # d@       ∏  `} ßD # e@        ;c     ëˇ # f@        ¸1  b} ,{ # g@        §;  c} ≠{ # h@        “‹  d} ŒY # i@        ÷‹      { # j@        ≤
  f} ^} # k@        î¬ g}     # l@        çØ  h} )\ # m@        ï¬         # n@        ≤
  j} e} !# o@        &  k} ‰p !# p@        ñb l} µ/ !# q@       »‹      æq !# r@        ô¬ n}     "# s@        ö¬ o}     "# t@        õ¬ p}     "# u@        ú¬ q}     "# v@        ù¬ r}     "# w@        û¬ s}     "# x@        hI t} Ÿ™ "# y@        ü¬ u}     "# z@        †¬         "# {@        ≤
  w} i} ## |@        ÃØ x} é] ## }@        m     è] ## ~@        «  z} «{ $# @        M   {} òX $# Ä@        £¬         $# Å@        Ö   }} Rr %# Ç@        ¶¬ ~