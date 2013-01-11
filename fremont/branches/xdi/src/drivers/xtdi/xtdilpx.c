#include <ntddk.h>
#include <socketlpx.h>
#include "xtdi.h"
#include "xtdilpx.h"

VOID
TdiBuildLpxAddress(
	__in PTDI_ADDRESS_LPX TdiAddressLpx,
	__inout_bcount(sizeof(TA_LPX_ADDRESS)) PTA_LPX_ADDRESS TaLpxAddress)
{

}

/*++

Parameters:

	Buffer - Pointer to a caller-supplied buffer, which must be at least
			?? bytes in length;
--*/

NTSTATUS
TdiBuildLpxAddressEa(
	__in PUCHAR Buffer,
	__in PTDI_ADDRESS_LPX TdiAddressLpx)
{
	return STATUS_NOT_IMPLEMENTED;
}

VOID
xLpxTdiFillAddressEaInformation(
	__in PTDI_ADDRESS_LPX Address,
	__inout_bcount(LPX_ADDRESS_EA_BUFFER_LENGTH)
		PFILE_FULL_EA_INFORMATION EaInformation)
{
	PTRANSPORT_ADDRESS transportAddress;
	PTA_ADDRESS taAddress;
	PTDI_ADDRESS_LPX lpxAddress;

	RtlZeroMemory(EaInformation, LPX_ADDRESS_EA_BUFFER_LENGTH);

	EaInformation->NextEntryOffset = 0;
	EaInformation->Flags = 0;
	EaInformation->EaNameLength = TDI_TRANSPORT_ADDRESS_LENGTH;
	EaInformation->EaValueLength =
		FIELD_OFFSET(TRANSPORT_ADDRESS, Address)
		+ FIELD_OFFSET(TA_ADDRESS, Address)
		+ TDI_ADDRESS_LENGTH_LPX;

	//
	// Set EaName
	//
	RtlMoveMemory(
		&(EaInformation->EaName[0]),
		TdiTransportAddress,
		TDI_TRANSPORT_ADDRESS_LENGTH + 1);

	//
	// Set EaValue
	//
	transportAddress = (PTRANSPORT_ADDRESS)
		&EaInformation->EaName[TDI_TRANSPORT_ADDRESS_LENGTH+1];
	transportAddress->TAAddressCount = 1;

	taAddress = (PTA_ADDRESS) transportAddress->Address;
	taAddress->AddressType   = TDI_ADDRESS_TYPE_LPX;
	taAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;

	lpxAddress = (PTDI_ADDRESS_LPX)taAddress->Address;

	RtlCopyMemory(
		lpxAddress,
		Address,
		sizeof(TDI_ADDRESS_LPX));
}

NTSTATUS
xLpxTdiCreateAddressObject(
	__in PTDI_ADDRESS_LPX Address,
	__out HANDLE *AddressHandle,
	__out PFILE_OBJECT *AddressFileObject,
	__out PDEVICE_OBJECT *AddressDeviceObject)
{
	UCHAR eaFullBuffer[LPX_ADDRESS_EA_BUFFER_LENGTH];
	PFILE_FULL_EA_INFORMATION eaBuffer = (PFILE_FULL_EA_INFORMATION) eaFullBuffer;

	xLpxTdiFillAddressEaInformation(Address, eaBuffer);

	return xTdiCreateAddressObject(
		SOCKETLPX_DEVICE_NAME,
		eaBuffer,
		LPX_ADDRESS_EA_BUFFER_LENGTH,
		AddressHandle,
		AddressFileObject,
		AddressDeviceObject);
}

NTSTATUS
xLpxTdiCreateConnectionObject(
	__in CONNECTION_CONTEXT ConnectionContext,
	__out HANDLE *ConnectionHandle,
	__out PFILE_OBJECT *ConnectionFileObject,
	__out PDEVICE_OBJECT *ConnectionDeviceObject)
{
	return xTdiCreateConnectionObject(
		SOCKETLPX_DEVICE_NAME,
		ConnectionContext,
		ConnectionHandle,
		ConnectionFileObject,
		ConnectionDeviceObject);
}

#define LPX_TRANSPORT_ADDRESS_LENGTH \
	FIELD_OFFSET(TRANSPORT_ADDRESS, Address) + \
	FIELD_OFFSET(TA_LPX_ADDRESS, Address) + \
	TDI_ADDRESS_LENGTH_LPX

NTSTATUS
xLpxTdiConnect(
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PTDI_ADDRESS_LPX Address,
	__in PLARGE_INTEGER   ConnectionTimeout)
{
	NTSTATUS status;
	UCHAR addressBuffer[LPX_TRANSPORT_ADDRESS_LENGTH];
	PTRANSPORT_ADDRESS transportAddress;
	PTA_ADDRESS taAddress;

	RtlZeroMemory(addressBuffer, sizeof(addressBuffer));
	transportAddress = (PTRANSPORT_ADDRESS) addressBuffer;
	transportAddress->TAAddressCount = 1;

	taAddress = (PTA_ADDRESS) transportAddress->Address;
	taAddress->AddressType = TDI_ADDRESS_TYPE_LPX;
	taAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;

	RtlCopyMemory(
		taAddress->Address,
		Address,
		TDI_ADDRESS_LENGTH_LPX);

	status = xTdiConnect(
		ConnectionDeviceObject,
		ConnectionFileObject,
		transportAddress,
		LPX_TRANSPORT_ADDRESS_LENGTH,
		ConnectionTimeout);

	return status;
}

NTSTATUS
xLpxTdiConnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PTDI_ADDRESS_LPX Address,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PXTDI_OVERLAPPED Overlapped)
{
	NTSTATUS status;
	UCHAR addressBuffer[LPX_TRANSPORT_ADDRESS_LENGTH];
	PTRANSPORT_ADDRESS transportAddress;
	PTA_ADDRESS taAddress;

	RtlZeroMemory(addressBuffer, sizeof(addressBuffer));
	transportAddress = (PTRANSPORT_ADDRESS) addressBuffer;
	transportAddress->TAAddressCount = 1;

	taAddress = (PTA_ADDRESS) transportAddress->Address;
	taAddress->AddressType = TDI_ADDRESS_TYPE_LPX;
	taAddress->AddressLength = TDI_ADDRESS_LENGTH_LPX;

	RtlCopyMemory(
		taAddress->Address,
		Address,
		TDI_ADDRESS_LENGTH_LPX);

	status = xTdiConnectEx(
		Irp,
		ConnectionDeviceObject,
		ConnectionFileObject,
		transportAddress,
		LPX_TRANSPORT_ADDRESS_LENGTH,
		ConnectionTimeout,
		Overlapped);

	return status;
}

