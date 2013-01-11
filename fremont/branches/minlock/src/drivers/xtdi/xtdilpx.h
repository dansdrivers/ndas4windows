#ifndef _XTDILPX_H_
#define _XTDILPX_H_
#include <tdikrnl.h>
#include <socketlpx.h>

#define	LPX_ADDRESS_EA_BUFFER_LENGTH \
	(								\
		FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName)  \
		+ TDI_TRANSPORT_ADDRESS_LENGTH + 1				\
		+ FIELD_OFFSET(TRANSPORT_ADDRESS, Address)		\
		+ FIELD_OFFSET(TA_ADDRESS, Address)				\
		+ TDI_ADDRESS_LENGTH_LPX						\
	)

VOID
xLpxTdiFillAddressEaInformation(
	__in PTDI_ADDRESS_LPX Address,
	__inout_bcount(LPX_ADDRESS_EA_BUFFER_LENGTH)
		PFILE_FULL_EA_INFORMATION EaInformation);

NTSTATUS
xLpxTdiCreateAddressObject(
	__in PTDI_ADDRESS_LPX Address,
	__out HANDLE *AddressHandle,
	__out PFILE_OBJECT *AddressFileObject,
	__out PDEVICE_OBJECT *AddressDeviceObject);

NTSTATUS
xLpxTdiCreateConnectionObject(
	__in CONNECTION_CONTEXT ConnectionContext,
	__out HANDLE *ConnectionHandle,
	__out PFILE_OBJECT *ConnectionFileObject,
	__out PDEVICE_OBJECT *ConnectionDeviceObject);

NTSTATUS
xLpxTdiConnect(
	__in PDEVICE_OBJECT TdiConnDeviceObject,
	__in PFILE_OBJECT TdiConnFileObject,
	__in PTDI_ADDRESS_LPX Address,
	__in PLARGE_INTEGER ConnectionTimeout);

NTSTATUS
xLpxTdiConnectEx(
	__in PIRP Irp,
	__in PDEVICE_OBJECT ConnectionDeviceObject,
	__in PFILE_OBJECT ConnectionFileObject,
	__in PTDI_ADDRESS_LPX Address,
	__in_opt PLARGE_INTEGER ConnectionTimeout,
	__in PXTDI_OVERLAPPED Overlapped);

#endif /* _XTDILPX_H_ */
