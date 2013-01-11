/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#ifndef _LPXPROCS_
#define _LPXPROCS_

//
// MACROS.
//
//
// Debugging aids
//

#if DBG
#define IF_LPXDBG(flags) \
	if (LpxDebug & (flags))
#else
#define IF_LPXDBG(flags) \
	if (0)
#endif

#if DBG
#define PANIC(Msg) \
	DbgPrint ((Msg))
#else
#define PANIC(Msg)
#endif


//
// These are define to allow DbgPrints that disappear when
// DBG is 0.
//

#if DBG
#define LpxPrint0(fmt) DbgPrint(fmt)
#define LpxPrint1(fmt,v0) DbgPrint(fmt,v0)
#define LpxPrint2(fmt,v0,v1) DbgPrint(fmt,v0,v1)
#define LpxPrint3(fmt,v0,v1,v2) DbgPrint(fmt,v0,v1,v2)
#define LpxPrint4(fmt,v0,v1,v2,v3) DbgPrint(fmt,v0,v1,v2,v3)
#define LpxPrint5(fmt,v0,v1,v2,v3,v4) DbgPrint(fmt,v0,v1,v2,v3,v4)
#define LpxPrint6(fmt,v0,v1,v2,v3,v4,v5) DbgPrint(fmt,v0,v1,v2,v3,v4,v5)
#define LpxPrint7(fmt,v0,v1,v2,v3,v4,v5,v6) DbgPrint(fmt,v0,v1,v2,v3,v4,v5,v6)
#else
#define LpxPrint0(fmt)
#define LpxPrint1(fmt,v0)
#define LpxPrint2(fmt,v0,v1)
#define LpxPrint3(fmt,v0,v1,v2)
#define LpxPrint4(fmt,v0,v1,v2,v3)
#define LpxPrint5(fmt,v0,v1,v2,v3,v4)
#define LpxPrint6(fmt,v0,v1,v2,v3,v4,v5)
#endif

//
// The REFCOUNTS message take up a lot of room, so make
// removing them easy.
//

#if 1
#define IF_REFDBG IF_LPXDBG (LPX_DEBUG_REFCOUNTS)
#else
#define IF_REFDBG if (0)
#endif

#if !(DBG==0)
#define LpxReferenceConnection(Reason, Connection, Type)\
	 IF_REFDBG { \
		DbgPrint ("RefC %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
	} \
	(VOID)InterlockedIncrement( \
		&(Connection)->RefTypes[Type]); \
	LpxRefConnection (Connection)

#define LpxDereferenceConnection(Reason, Connection, Type)\
	 IF_REFDBG { \
		DbgPrint ("DeRefC %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
	} \
	(VOID)InterlockedDecrement( \
		&((Connection)->RefTypes[Type])); \
	ASSERT((Connection)->RefTypes[Type]>=0); \
	LpxDerefConnection (Connection)

#define LpxDereferenceConnectionMacro(Reason, Connection, Type)\
	LpxDereferenceConnection(Reason, Connection, Type)

#define LpxDereferenceConnectionSpecial(Reason, Connection, Type)\
	IF_REFDBG { \
		DbgPrint ("DeRefCL %p: %s %s, %ld : %ld\n", Connection, Reason, __FILE__, __LINE__, (Connection)->ReferenceCount);\
	} \
	(VOID)InterlockedDecrement( \
		&((Connection)->RefTypes[Type])); \
	ASSERT((Connection)->RefTypes[Type]>=0); \
	LpxDerefConnectionSpecial (Connection)
  
#define LpxReferenceSendIrp( Reason, IrpSp, Type)\
	IF_REFDBG {   \
		DbgPrint ("RefSI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_SEND_REFCOUNT(IrpSp));}\
	LpxRefSendIrp (IrpSp)

#define LpxDereferenceSendIrp(Reason, IrpSp, Type)\
	IF_REFDBG { \
		DbgPrint ("DeRefSI %p: %s %s, %ld : %ld\n", IrpSp, Reason, __FILE__, __LINE__, IRP_SEND_REFCOUNT(IrpSp));\
	} \
	LpxDerefSendIrp (IrpSp)
 
#define LpxReferenceAddress( Reason, Address, Type)\
	IF_REFDBG {   \
		DbgPrint ("RefA %p: %s %s, %ld : %ld\n", Address, Reason, __FILE__, __LINE__, (Address)->ReferenceCount);}\
	(VOID)InterlockedIncrement(\
		(PULONG)(&(Address)->RefTypes[Type])); \
	LpxRefAddress (Address)

#define LpxDereferenceAddress(Reason, Address, Type)\
	IF_REFDBG { \
		DbgPrint ("DeRefA %p: %s %s, %ld : %ld\n", Address, Reason, __FILE__, __LINE__, (Address)->ReferenceCount);\
	} \
	(VOID)InterlockedDecrement( \
		(PULONG)(&(Address)->RefTypes[Type])); \
	LpxDerefAddress (Address)

#define LpxReferenceDeviceContext( Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	 \
		DbgBreakPoint();                          \
	IF_REFDBG {   \
		DbgPrint ("RefDC %p: %s %s, %ld : %ld\n", DeviceContext, Reason, __FILE__, __LINE__, (DeviceContext)->ReferenceCount);}\
	(VOID)InterlockedIncrement( \
		(PULONG)(&(DeviceContext)->RefTypes[Type])); \
	LpxRefDeviceContext (DeviceContext)

#define LpxDereferenceDeviceContext(Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	 \
		DbgBreakPoint();                          \
	IF_REFDBG { \
		DbgPrint ("DeRefDC %p: %s %s, %ld : %ld\n", DeviceContext, Reason, __FILE__, __LINE__, (DeviceContext)->ReferenceCount);\
	} \
	(VOID)InterlockedDecrement ( \
		(PULONG)(&(DeviceContext)->RefTypes[Type])); \
	LpxDerefDeviceContext (DeviceContext)

#else

#define LpxReferenceConnection(Reason, Connection, Type)\
	if (((Connection)->ReferenceCount == -1) &&   \
		((Connection)->SpecialRefCount == 0))     \
		ASSERT(TRUE);                          \
		                                          \
	if (InterlockedIncrement( \
		    &(Connection)->ReferenceCount) == 0) { \
		InterlockedIncrement( \
		    (PULONG)(&(Connection)->SpecialRefCount)); \
	}

#define LpxDereferenceConnection(Reason, Connection, Type)\
	if (((Connection)->ReferenceCount == -1) &&   \
		((Connection)->SpecialRefCount == 0))     \
		ASSERT(TRUE);                          \
		                                          \
	LpxDerefConnection (Connection)

#define LpxDereferenceConnectionMacro(Reason, Connection, Type){ \
	if (((Connection)->ReferenceCount == -1) &&   \
		((Connection)->SpecialRefCount == 0))     \
		DbgBreakPoint();                          \
		                                          \
		                                          \
	if (InterlockedDecrement( \
		    &(Connection)->ReferenceCount) < 0) { \
		LpxDerefConnectionSpecial (Connection); \
	} \
}

#define LpxDereferenceConnectionSpecial(Reason, Connection, Type)\
	LpxDerefConnectionSpecial (Connection)
 
#define LpxReferenceSendIrp(Reason, IrpSp, Type)\
	(VOID)InterlockedIncrement( \
		&IRP_SEND_REFCOUNT(IrpSp))


#define LpxDereferenceSendIrp(Reason, IrpSp, Type) {\
	PIO_STACK_LOCATION _IrpSp = (IrpSp); \
	if (InterlockedDecrement( \
		    &IRP_SEND_REFCOUNT(_IrpSp)) == 0) { \
		PIRP _Irp = IRP_SEND_IRP(_IrpSp); \
		IRP_SEND_REFCOUNT(_IrpSp) = 0; \
		IRP_SEND_IRP (_IrpSp) = NULL; \
		{ \
			KIRQL	ilgu_cancelIrql; \
			IoAcquireCancelSpinLock(&ilgu_cancelIrql);	\
			IoSetCancelRoutine(_Irp, NULL);	\
			IoReleaseCancelSpinLock(ilgu_cancelIrql);	\
			IoCompleteRequest (_Irp, IO_NETWORK_INCREMENT); \
		} \
	} \
}

#define LpxReferenceAddress(Reason, Address, Type)\
	if ((Address)->ReferenceCount <= 0){ ASSERT(TRUE); }\
	(VOID)InterlockedIncrement(&(Address)->ReferenceCount)

#define LpxDereferenceAddress(Reason, Address, Type)\
	if ((Address)->ReferenceCount <= 0){ ASSERT(TRUE); }\
	LpxDerefAddress (Address)

#define LpxReferenceDeviceContext(Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	             \
		ASSERT(TRUE);                                      \
	LpxRefDeviceContext (DeviceContext)

#define LpxDereferenceDeviceContext(Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	               \
		ASSERT(TRUE);                                        \
	LpxDerefDeviceContext (DeviceContext)
  
#endif

#define LpxReferenceControlContext(Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	             \
		ASSERT(TRUE);                                      \
	LpxRefControlContext (DeviceContext)

#define LpxDereferenceControlContext(Reason, DeviceContext, Type)\
	if ((DeviceContext)->ReferenceCount == 0)	               \
		ASSERT(TRUE);                                        \
	LpxDerefControlContext (DeviceContext)

 
//
// Routines in SEND.C (Receive engine).
//

NTSTATUS
LpxTdiSend(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiSendDatagram(
	IN PIRP Irp
	);

VOID
LpxSendComplete(
	IN NDIS_HANDLE ProtocolBindingContext,
	IN PNDIS_PACKET NdisPacket,
	IN NDIS_STATUS NdisStatus
	);

//
// Routines in DEVCTX.C (TP_DEVCTX object manager).
//

VOID
LpxRefDeviceContext(
	IN PDEVICE_CONTEXT DeviceContext
	);

VOID
LpxDerefDeviceContext(
	IN PDEVICE_CONTEXT DeviceContext
	);



VOID
LpxRefControlContext(
	IN PCONTROL_CONTEXT DeviceContext
	);

VOID
LpxDerefControlContext(
	IN PCONTROL_CONTEXT DeviceContext
	);

NTSTATUS
LpxCreateDeviceContext(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING DeviceName,
	IN OUT PDEVICE_CONTEXT *DeviceContext
	);

VOID
LpxDestroyDeviceContext(
	IN PDEVICE_CONTEXT DeviceContext
	);

VOID
LpxDestroyControlContext(
	IN PCONTROL_CONTEXT DeviceContext
	);
	
VOID
LpxCreateControlDevice(
	PDRIVER_OBJECT DriverObject
	); 

VOID
LpxDestroyControlDevice(
	PCONTROL_CONTEXT DeviceContext
	);

PDEVICE_CONTEXT
LpxFindDeviceContext(
	PLPX_ADDRESS networkName
	);


//
// Routines in ADDRESS.C (TP_ADDRESS object manager).
//

#if DBG
VOID
LpxRefAddress(
	IN PTP_ADDRESS Address
	);
#endif

VOID
LpxDerefAddress(
	IN PTP_ADDRESS Address
	);

VOID
LpxAllocateAddressFile(
	IN PDEVICE_CONTEXT DeviceContext,
	OUT PTP_ADDRESS_FILE *TransportAddressFile
	);

VOID
LpxDeallocateAddressFile(
	IN PDEVICE_CONTEXT DeviceContext,
	IN PTP_ADDRESS_FILE TransportAddressFile
	);

NTSTATUS
LpxCreateAddressFile(
	IN PDEVICE_CONTEXT DeviceContext,
	OUT PTP_ADDRESS_FILE * AddressFile
	);

VOID
LpxReferenceAddressFile(
	IN PTP_ADDRESS_FILE AddressFile
	);

VOID
LpxDereferenceAddressFile(
	IN PTP_ADDRESS_FILE AddressFile
	);

VOID
LpxDestroyAddress(
	IN PDEVICE_OBJECT DeviceObject,
	IN PVOID Parameter
	);

NTSTATUS
LpxOpenAddress(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PIO_STACK_LOCATION IrpSp
	);

NTSTATUS
LpxCloseAddress(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PIO_STACK_LOCATION IrpSp
	);


VOID
LpxAllocateAddress(
	IN PDEVICE_CONTEXT DeviceContext,
	OUT PTP_ADDRESS *TransportAddress
	);

VOID
LpxDeallocateAddress(
	IN PDEVICE_CONTEXT DeviceContext,
	IN PTP_ADDRESS TransportAddress
	);

NTSTATUS
LpxCreateAddress(
	IN PDEVICE_CONTEXT DeviceContext,
	IN PLPX_ADDRESS NetworkName,
	OUT PTP_ADDRESS *Address
	);

NTSTATUS
LpxStopAddressFile(
	IN PTP_ADDRESS_FILE AddressFile,
	IN PTP_ADDRESS Address
	);


TDI_ADDRESS_LPX *
LpxParseTdiAddress(
	IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
	IN BOOLEAN BroadcastAddressOk
);

BOOLEAN
LpxValidateTdiAddress(
	IN TRANSPORT_ADDRESS UNALIGNED * TransportAddress,
	IN ULONG TransportAddressLength
);

NTSTATUS
LpxVerifyAddressObject (
	IN PTP_ADDRESS_FILE AddressFile
	);

//
// Routines in CONNECT.C.
//

NTSTATUS
LpxTdiAccept(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiConnect(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiDisconnect(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiDisassociateAddress (
	IN PIRP Irp
	);

NTSTATUS
LpxTdiAssociateAddress(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiListen(
	IN PIRP Irp
	);

NTSTATUS
LpxOpenConnection(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PIO_STACK_LOCATION IrpSp
	);

NTSTATUS
LpxCloseConnection(
	IN PDEVICE_OBJECT DeviceObject,
	IN PIRP Irp,
	IN PIO_STACK_LOCATION IrpSp
	);

#if DBG
VOID
LpxRefConnection(
	IN PTP_CONNECTION TransportConnection
	);
#endif

VOID
LpxDerefConnection(
	IN PTP_CONNECTION TransportConnection
	);

VOID
LpxDerefConnectionSpecial(
	IN PTP_CONNECTION TransportConnection
	);

VOID
LpxStopConnection(
	IN PTP_CONNECTION TransportConnection
	 );

VOID
LpxAllocateConnection(
	IN PCONTROL_CONTEXT DeviceContext,
	OUT PTP_CONNECTION *TransportConnection
	);

VOID
LpxDeallocateConnection(
	IN PCONTROL_CONTEXT DeviceContext,
	IN PTP_CONNECTION TransportConnection
	);

NTSTATUS
LpxCreateConnection(
	IN PCONTROL_CONTEXT DeviceContext,
	OUT PTP_CONNECTION *TransportConnection
	);


NTSTATUS
LpxVerifyConnectionObject (
	IN PTP_CONNECTION Connection
	);

//
// Routines in INFO.C (QUERY_INFO manager).
//

NTSTATUS
LpxTdiQueryInformation(
	IN PCONTROL_CONTEXT DeviceContext,
	IN PIRP Irp
	);

NTSTATUS
LpxTdiSetInformation(
	IN PIRP Irp
	);

//
// Routines in EVENT.C.
//

NTSTATUS
LpxTdiSetEventHandler(
	IN PIRP Irp
	);


#if DBG
VOID
LpxRefSendIrp(
	IN PIO_STACK_LOCATION IrpSp
	);

VOID
LpxDerefSendIrp(
	IN PIO_STACK_LOCATION IrpSp
	);
#endif

  
NDIS_STATUS
LpxReceiveIndicate(
	IN NDIS_HANDLE BindingContext,
	IN NDIS_HANDLE ReceiveContext,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookaheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	);

VOID
LpxTransferDataComplete(
	IN NDIS_HANDLE BindingContext,
	IN PNDIS_PACKET NdisPacket,
	IN NDIS_STATUS Status,
	IN UINT BytesTransferred
	);

//
// Routines in RCV.C (data copying routines for receives).
//

NTSTATUS
LpxTdiReceive(
	IN PIRP Irp
	);

NTSTATUS
LpxTdiReceiveDatagram(
	IN PIRP Irp
	);


//
// Routines in nbfndis.c.
//

#if DBG
PUCHAR
LpxGetNdisStatus (
	IN NDIS_STATUS NdisStatus
	);
#endif

//
// Routines in nbfdrvr.c
//

VOID
LpxFreeResources(
	IN PDEVICE_CONTEXT DeviceContext
	);


extern
ULONG
LpxInitializeOneDeviceContext(
	OUT PNDIS_STATUS NdisStatus,
	IN PDRIVER_OBJECT DriverObject,
	IN PCONFIG_DATA LpxConfig,
	IN PUNICODE_STRING BindName,
	IN PUNICODE_STRING ExportName,
	IN PVOID SystemSpecific1,
	IN PVOID SystemSpecific2
	);


extern
VOID
LpxReInitializeDeviceContext(
	OUT PNDIS_STATUS NdisStatus,
	IN PDRIVER_OBJECT DriverObject,
	IN PCONFIG_DATA LpxConfig,
	IN PUNICODE_STRING BindName,
	IN PUNICODE_STRING ExportName,
	IN PVOID SystemSpecific1,
	IN PVOID SystemSpecific2
	);

//
// routines in nbfcnfg.c
//

NTSTATUS
LpxConfigureTransport (
	IN PUNICODE_STRING RegistryPath,
	IN PCONFIG_DATA * ConfigData
	);

NTSTATUS
LpxGetExportNameFromRegistry(
	IN  PUNICODE_STRING RegistryPath,
	IN  PUNICODE_STRING BindName,
	OUT PUNICODE_STRING ExportName
	);

//
// Routines in nbfndis.c
//

NTSTATUS
LpxRegisterProtocol (
	IN PUNICODE_STRING NameString
	);

VOID
LpxDeregisterProtocol (
	VOID
	);


NTSTATUS
LpxInitializeNdis (
	IN PDEVICE_CONTEXT DeviceContext,
	IN PCONFIG_DATA ConfigInfo,
	IN PUNICODE_STRING AdapterString
	);

VOID
LpxCloseNdis (
	IN PDEVICE_CONTEXT DeviceContext
	);


//
// Routines in lpx.c
//

VOID
LpxInitServicePoint(
	IN OUT	PTP_CONNECTION    Connection
	);

BOOLEAN
LpxCloseServicePoint(
	IN OUT	PSERVICE_POINT Connection
	);

NTSTATUS
LpxAssignPort(
	IN PDEVICE_CONTEXT	AddressDeviceContext,
	IN PLPX_ADDRESS	    SourceAddress
	);

PTP_ADDRESS
LpxLookupAddress(
	IN PDEVICE_CONTEXT	DeviceContext,
	IN PLPX_ADDRESS	    SourceAddress
	);


VOID
LpxAssociateAddress(
	IN OUT	PTP_CONNECTION    Connection
	);

NTSTATUS
LpxConnect(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
   );

NTSTATUS
LpxListen(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
   );

NDIS_STATUS
LpxAccept(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
   );

NDIS_STATUS
LpxDisconnect(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
	);

NTSTATUS
LpxSend(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
   );

VOID
LpxSendComplete(
	IN NDIS_HANDLE   ProtocolBindingContext,
	IN PNDIS_PACKET  pPacket,
	IN NDIS_STATUS   Status
	);

NTSTATUS
LpxRecv(
	IN	     PTP_CONNECTION    Connection,
	 IN OUT	PIRP            Irp
   );


NDIS_STATUS
LpxSendDatagram(
	IN	     PTP_ADDRESS    Address,
	 IN OUT	PIRP        Irp
   );

NDIS_STATUS
LpxReceiveIndicate(
	IN NDIS_HANDLE ProtocolBindingContext,
	IN NDIS_HANDLE MacReceiveContext,
	IN PVOID HeaderBuffer,
	IN UINT HeaderBufferSize,
	IN PVOID LookAheadBuffer,
	IN UINT LookaheadBufferSize,
	IN UINT PacketSize
	);

VOID
LpxTransferDataComplete (
	IN NDIS_HANDLE   ProtocolBindingContext,
	IN PNDIS_PACKET  Packet,
	IN NDIS_STATUS   Status,
	IN UINT	      BytesTransfered
	);

VOID
LpxReceiveComplete (
		            IN NDIS_HANDLE BindingContext
		            );

//
// routines in lpxpacket.c
//
#if DBG
extern LONG		NumberOfPackets;
extern ULONG	NumberOfSentComplete;

extern ULONG	NumberOfSent;
extern ULONG	NumberOfSentComplete;
#endif

PNDIS_PACKET
PacketDequeue(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

VOID
PacketFree(
	IN PNDIS_PACKET	Packet
	);

NTSTATUS
PacketAllocate(
	IN	PSERVICE_POINT        ServicePoint,
	IN	ULONG                PacketLength,
	IN	PDEVICE_CONTEXT        DeviceContext,
	IN	BOOLEAN                Send,
	IN	PUCHAR                CopyData,
	IN	ULONG                CopyDataLength,
	IN	PIO_STACK_LOCATION    IrpSp,
	OUT	PNDIS_PACKET        *Packet
	);

PNDIS_PACKET
PacketCopy(
	IN	PNDIS_PACKET Packet,
	OUT	PLONG    Cloned
	) ;

PNDIS_PACKET
PacketClone(
	IN	PNDIS_PACKET Packet
	);

BOOLEAN
PacketQueueEmpty(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

PNDIS_PACKET
PacketPeek(
	PLIST_ENTRY	PacketQueue,
	PKSPIN_LOCK	QSpinLock
	);

//
// Pool tag for LPX's memory allocations
//
NDIS_STATUS
LpxAllocatePacketMemory(
	IN PDEVICE_CONTEXT DeviceContext,
    OUT PVOID  *VirtualAddress,
    IN UINT  Length
	);

VOID
LpxFreePacketMemory(
	IN PDEVICE_CONTEXT DeviceContext,
	IN UINT	  Length,
    IN PVOID  VirtualAddress
	);

void
CallUserDisconnectHandler(
	IN	PSERVICE_POINT    pServicePoint,
	IN	ULONG            DisconnectFlags
	);

VOID LpxChangeState(
	IN PTP_CONNECTION Connection,
	IN SMP_STATE NewState,
	IN BOOLEAN Locked	
);


#endif // def _LPXPROCS_
