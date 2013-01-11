#include "ndasport.h"
#include "trace.h"
#include "ndasdlu.h"
#include "ndasdluioctl.h"
#include "constants.h"
#include "utils.h"

#include <initguid.h>
#include "ndasdluguid.h"

#include "ndasscsiioctl.h"
#include "lsutils.h"
#include "lslur.h"

#ifdef RUN_WPP
#include "ndasdlu.tmh"
#endif

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "DLU"

#ifndef countof
#define countof(a) (sizeof(a)/sizeof(a[0]))
#endif

#define NDASDLU_PNP_TAG 'PliF'


#ifdef NdasPortTrace
#undef NdasPortTrace
#endif

#define NdasPortTrace _NdasDluTrace

void _NdasDluTrace(ULONG Flags, ULONG Level, ...)
{
	UNREFERENCED_PARAMETER(Flags);
	UNREFERENCED_PARAMETER(Level);
}

//
// Internal routines
//
static
NTSTATUS
NdasDluInitializeInquiryData(
	__in PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension);

static
VOID
NdasDluInitializeStorageAdapterDescriptor(
	__in PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	__in ULONG	MaximumTransferLength);

static
VOID
NdasDluLurnCallback(
	IN PLURELATION	Lur,
	IN PLURN_EVENT	LurnEvent
);

static
BOOLEAN
NdasDluResetBus(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogiocalUnitExtension,
	IN ULONG	PathId
	);


static
NTSTATUS
NdasDluSendCcbToAllLURs(
		PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		UINT32						CcbOpCode,
		CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
		PVOID						CompletionContext

	);

static
NTSTATUS
NdasDluSendStopCcbToAllLURsSync(
		LONG			LURCount,
		PLURELATION		*LURs
	);

static
NTSTATUS
NdasDluFreeAllLURs(
		LONG			LURCount,
		PLURELATION		*LURs
	);

//
// Inline routines
//


//
// Get DLU extension from the NDASPORT logical unit extension
//

FORCEINLINE
PNDAS_LOGICALUNIT_EXTENSION
NdasDluGetLogicalUnitExtension(
	PNDAS_DLU_EXTENSION DluExtension)
{
	return (PNDAS_LOGICALUNIT_EXTENSION) DluExtension;
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NdasDluInitializeLogicalUnit)
#pragma alloc_text(PAGE, NdasDluInitializeInquiryData)
#endif // ALLOC_PRAGMA

//
// Interface to NDASPORT
//

NDAS_LOGICALUNIT_INTERFACE NdasDluInterface = {
	sizeof(NDAS_LOGICALUNIT_INTERFACE),
	NDAS_LOGICALUNIT_INTERFACE_VERSION,
	NdasDluInitializeLogicalUnit,
	NdasDluCleanupLogicalUnit,
	NdasDluLogicalUnitControl,
	NdasDluBuildIo,
	NdasDluStartIo,
	NdasDluQueryPnpID,
	NdasDluQueryPnpDeviceText,
	NdasDluQueryPnpDeviceCapabilities,
	NdasDluQueryStorageDeviceProperty,
	NULL,
	NdasDluQueryStorageAdapterProperty
};


NTSTATUS
NdasDluGetNdasLogicalUnitInterface(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__inout PNDAS_LOGICALUNIT_INTERFACE LogicalUnitInterface,
	__out PULONG LogicalUnitExtensionSize,
	__out PULONG SrbExtensionSize)
{
	BOOLEAN equal;

	if (NdasExternalType != LogicalUnitDescriptor->Type)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	equal = RtlEqualMemory(
		&LogicalUnitDescriptor->ExternalTypeGuid,
		&NDASPORT_NDAS_DLU_TYPE_GUID,
		sizeof(GUID));

	if (!equal)
	{
		return STATUS_INVALID_PARAMETER_1;
	}

	if (sizeof(NDAS_LOGICALUNIT_INTERFACE) != LogicalUnitInterface->Size ||
		NDAS_LOGICALUNIT_INTERFACE_VERSION != LogicalUnitInterface->Version)
	{
		return STATUS_INVALID_PARAMETER_2;
	}

	RtlCopyMemory(
		LogicalUnitInterface,
		&NdasDluInterface,
		sizeof(NDAS_LOGICALUNIT_INTERFACE));

	*LogicalUnitExtensionSize = sizeof(NDAS_DLU_EXTENSION);

#if __NDASDLU_USE_POOL_FOR_CCBALLOCATION__
	*SrbExtensionSize = 0;
#else
	*SrbExtensionSize = sizeof(CCB);
#endif

	return STATUS_SUCCESS;
}

NTSTATUS
NdasDluInitializeLogicalUnit(
	__in PNDAS_LOGICALUNIT_DESCRIPTOR LogicalUnitDescriptor,
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	NTSTATUS status;
	PNDAS_DLU_EXTENSION ndasDluExtension;
	PNDAS_DLU_DESCRIPTOR ndasDluDescriptor;
	BOOLEAN w2kReadOnlyPatch;
	PDEVICE_OBJECT	phyDevObj = NdasPortExGetWdmDeviceObject(LogicalUnitExtension);
	ULONG	maximumTransferLength;

	PAGED_CODE();

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	if (sizeof(NDAS_LOGICALUNIT_DESCRIPTOR) != LogicalUnitDescriptor->Version)
	{
		NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_FATAL,
			"LogicalUnitDescriptor version is invalid. Version=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Version,
			sizeof(NDAS_LOGICALUNIT_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}
	if (LogicalUnitDescriptor->Size < FIELD_OFFSET(NDAS_DLU_DESCRIPTOR, LurDesc))
	{
		NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_FATAL,
			"NdasDluDescriptor Size is invalid. Size=%d, Expected=%d\n", 
			LogicalUnitDescriptor->Size,
			sizeof(NDAS_DLU_DESCRIPTOR));

		return STATUS_INVALID_PARAMETER;
	}

	NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_FATAL,
		"Initializing NdasDlu Logical Unit\n");

	ndasDluDescriptor = (PNDAS_DLU_DESCRIPTOR) LogicalUnitDescriptor;
	if(ndasDluDescriptor->Header.Size <= FIELD_OFFSET(NDAS_DLU_DESCRIPTOR, LurDesc)) {
		return STATUS_INVALID_PARAMETER;

	}
	if(ndasDluDescriptor->LurDesc.Length >
		ndasDluDescriptor->Header.Size - FIELD_OFFSET(NDAS_DLU_DESCRIPTOR, LurDesc)) {
			return STATUS_INVALID_PARAMETER;
	}

	ndasDluExtension->LogicalUnitAddress = LogicalUnitDescriptor->Address.Address;
	ndasDluExtension->MaximumRequests = DLU_MAXIMUM_REQUESTS;


	//
	// Initialize the DLU spin lock
	//

	KeInitializeSpinLock(&ndasDluExtension->DluSpinLock);

	//
	// Set the enabled time to support LFSFilter
	//

	KeQuerySystemTime(&ndasDluExtension->EnabledTime);

	//
	// LU name
	//

	ndasDluExtension->LuNameLength = ndasDluDescriptor->LurDesc.LurNameLength;
	if(ndasDluDescriptor->LurDesc.LurNameLength) {
		status = RtlStringCchCopyW(
			ndasDluExtension->LuName,
			MAX_LENGTH_LUR_NAME,
			ndasDluDescriptor->LurDesc.LurName);
		if(!NT_SUCCESS(status)) {
			ASSERT(FALSE);
			return status;
		}
	}


	//
	//	If the OS is Windows 2K, request read-only patch.
	//

	if(ndasDluDescriptor->LurDesc.DeviceMode == DEVMODE_SHARED_READONLY &&
		_NdasDluGlobals.MajorVersion == NT_MAJOR_VERSION &&
		_NdasDluGlobals.MinorVersion == W2K_MINOR_VERSION) {

		w2kReadOnlyPatch = TRUE;
	} else {
		w2kReadOnlyPatch = FALSE;
	}

	//
	// Create the LU relation
	//

	status = LurCreate(
				&ndasDluDescriptor->LurDesc,
				FALSE,						// Do not enforce Secondary mode.
				w2kReadOnlyPatch,
				phyDevObj,
				LogicalUnitExtension,
				NdasDluLurnCallback,
				&ndasDluExtension->LUR );

	if(!NT_SUCCESS(status)) {
		NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_ERROR,
			"failed to create an LUR at LogicalUnitAddress=%08X. STATUS=%08lx\n",
			ndasDluExtension->LogicalUnitAddress, status);
		return status;
	}

	status = NdasDluInitializeInquiryData(LogicalUnitExtension);
	if(!NT_SUCCESS(status)) {
		ASSERT(FALSE);
		NdasDluSendStopCcbToAllLURsSync(1, &ndasDluExtension->LUR);
		NdasDluFreeAllLURs(1, &ndasDluExtension->LUR);
		return status;
	}

	//
	// Initialize adapter descriptor that will override the NDASPORT's default.
	//

	if(ndasDluDescriptor->LurDesc.MaxOsRequestLength) {
		maximumTransferLength = ndasDluDescriptor->LurDesc.MaxOsRequestLength;
		NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_ERROR,
			"User defined maximum transfer length = %d\n",
			ndasDluDescriptor->LurDesc.MaxOsRequestLength);
	} else {
		maximumTransferLength = NDASDLU_DEFAULT_MAXIMUM_TRANSFER_LENGTH;
		//
		// HACK: LURN does not support flow control for non-direct-access devices.
		//
		if(ndasDluExtension->InquiryData.DeviceType != DIRECT_ACCESS_DEVICE) {
			maximumTransferLength = 32768;
		}
	}
	NdasDluInitializeStorageAdapterDescriptor(LogicalUnitExtension, maximumTransferLength);

	//
	// To support paging file in the device, you have to turn off DO_POWER_PAGABLE flags.
	// But, the device will go to power saving mode after the network layers.
	// It makes serious obstacles for the device depending on network connections
	// to enter power saving mode.
	// The device could fail to perform system IOs when entering power saving mode.
	// If the flag is on, Windows give the device notification earlier than non-pageable devices.
	//

	phyDevObj->Flags |= DO_POWER_PAGABLE;

	//
	// Increase TDI client device count.
	//

	LsuIncrementTdiClientDevice();

	//
	// Set the internal status to running status
	//

	DLUINTERNAL_SETSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_RUNNING);

	NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_INFORMATION,
		"NdasDlu created successfully at LogicalUnitAddress=%08X.\n",
		ndasDluExtension->LogicalUnitAddress);

	return STATUS_SUCCESS;

}



VOID
NdasDluCleanupLogicalUnit(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension)
{
	PNDAS_DLU_EXTENSION ndasDluExtension;
	NTSTATUS			status;

	PAGED_CODE();

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	ASSERT(NULL != ndasDluExtension);

	ASSERT(NULL == ndasDluExtension->CurrentSrb);

	ASSERT(NULL != ndasDluExtension->LUR);

	ASSERT(0 == ndasDluExtension->RequestExecuting);

	//
	// Decrease TDI client device count.
	//

	LsuDecrementTdiClientDevice();

	if(ndasDluExtension->LUR) {
		//
		//	CCB_OPCODE_STOP must succeed.
		//

		status = NdasDluSendStopCcbToAllLURsSync(1, &ndasDluExtension->LUR);
		ASSERT(NT_SUCCESS(status));
		NdasDluFreeAllLURs(1, &ndasDluExtension->LUR);
	}
	else {
		KDPrint(1,("======> no LUR\n"));
		return;
	}

	KDPrint(1,("Cleanup done.\n"));
}


static
NTSTATUS
NdasDluInitializeInquiryData(
	__in PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension)
{
	PCCB		ccb;
	NTSTATUS	ntStatus;
	PNDAS_DLU_EXTENSION ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);
	PLURELATION	LUR = ndasDluExtension->LUR;
	PCDB cdb;
	KEVENT		compEvent;
	

	KDPrint(1,("entered.\n"));

	//
	// Set up CCB to send SCSI Inquiry command.
	//

	ntStatus = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode				= CCB_OPCODE_EXECUTE;
	ccb->LurId[0]					= LUR->LurId[0]; // PathId
	ccb->LurId[1]					= LUR->LurId[1]; // TargetId
	ccb->LurId[2]					= LUR->LurId[2]; // LUN
	ccb->LurId[3]					= LUR->LurId[3]; // PortNumber
	ccb->HwDeviceExtension			= LogicalUnitExtension;
	LsCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	// Set completion event.
	//

	KeInitializeEvent(&compEvent, NotificationEvent, FALSE);
	ccb->CompletionEvent			= &compEvent;

	//
	// Set up CDB
	//

	ccb->CdbLength = 6;
	cdb = (PCDB)ccb->Cdb;

	// Set CDB operation code.
	cdb->CDB6INQUIRY.OperationCode = SCSIOP_INQUIRY;
	// Set CDB LUN.
	cdb->CDB6INQUIRY.LogicalUnitNumber = 0;
	cdb->CDB6INQUIRY.Reserved1 = 0;
	// Set allocation length to inquiry data buffer size.
	cdb->CDB6INQUIRY.AllocationLength = INQUIRYDATABUFFERSIZE;

	//
	// Zero reserve field and
	// Set EVPD Page Code to zero.
	// Set Control field to zero.
	// (See SCSI-II Specification.)
	//

	cdb->CDB6INQUIRY.PageCode = 0;
	cdb->CDB6INQUIRY.IReserved = 0;
	cdb->CDB6INQUIRY.Control = 0;

	//
	// Set up Inquiry buffer
	//

	ccb->DataBuffer = (PVOID)&ndasDluExtension->InquiryData;
	ccb->DataBufferLength = INQUIRYDATABUFFERSIZE;

	//
	//	Send a CCB to the root of LURelation.
	//
	ntStatus = LurRequest(
		LUR,
		ccb
		);
	if(!NT_SUCCESS(ntStatus)) {
		LsCcbPostCompleteCcb(ccb);
		return ntStatus;
	}
	if(ntStatus == STATUS_PENDING) {
		KeWaitForSingleObject(&compEvent, Executive, KernelMode, FALSE, NULL);
	}
	if(ccb->CcbStatus != CCB_STATUS_SUCCESS) {
		LsCcbFree(ccb);
		return STATUS_UNSUCCESSFUL;
	}
#if 0
	//
	// Inquiry with VPD option.
	//

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode				= CCB_OPCODE_EXECUTE;
	ccb->LurId[0]					= LUR->LurId[0]; // PathId
	ccb->LurId[1]					= LUR->LurId[1]; // TargetId
	ccb->LurId[2]					= LUR->LurId[2]; // LUN
	ccb->LurId[3]					= LUR->LurId[3]; // PortNumber
	ccb->HwDeviceExtension			= LogicalUnitExtension;
	LsCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	// Set completion event.
	//

	KeInitializeEvent(&compEvent, NotificationEvent, FALSE);
	ccb->CompletionEvent			= &compEvent;

	//
	// Set up CDB
	//

	ccb->CdbLength = 6;
	cdb = (PCDB)ccb->Cdb;

	// Set CDB operation code.
	cdb->CDB6INQUIRY3.OperationCode = SCSIOP_INQUIRY;
	// Set CDB LUN.
	cdb->CDB6INQUIRY3.EnableVitalProductData = 1;
	cdb->CDB6INQUIRY3.Reserved1 = 0;
	// Set allocation length to inquiry data buffer size.
	cdb->CDB6INQUIRY3.AllocationLength = VPD_MAX_BUFFER_SIZE;

	//
	// Zero reserve field and
	// Set EVPD Page Code to zero.
	// Set Control field to zero.
	// (See SCSI-II Specification.)
	//

	cdb->CDB6INQUIRY3.PageCode = VPD_SUPPORTED_PAGES;
	cdb->CDB6INQUIRY3.Control = 0;

	//
	// Set up Inquiry buffer
	//

	ccb->DataBuffer = (PVOID)ndasDluExtension->InquiryDataVpd;
	ccb->DataBufferLength = VPD_MAX_BUFFER_SIZE;

	//
	//	Send a CCB to the root of LURelation.
	//
	ntStatus = LurRequest(
		LUR,
		ccb
		);
	if(!NT_SUCCESS(ntStatus)) {
		LsCcbPostCompleteCcb(ccb);
		return ntStatus;
	}
	if(ntStatus == STATUS_PENDING) {
		KeWaitForSingleObject(&compEvent, Executive, KernelMode, FALSE, NULL);
	}
	if(ccb->CcbStatus != CCB_STATUS_SUCCESS) {
		ndasDluExtension->ValidInquiryDataVpd = FALSE;
		KDPrint(1, ("Inquiry with VPD failed.\n"));
	} else {
		ndasDluExtension->ValidInquiryDataVpd = TRUE;
	}
#endif
	//
	// CcbCompletion API will not free the CCB because we didn't
	// set CCB_FLAG_ALLOCATED.
	//
	LsCcbFree(ccb);

	return STATUS_SUCCESS;
}

NTSTATUS
NdasDluLogicalUnitControl(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in NDAS_LOGICALUNIT_CONTROL_TYPE ControlType,
	__in PVOID Parameters)
{
	UNREFERENCED_PARAMETER(LogicalUnitExtension);
	UNREFERENCED_PARAMETER(ControlType);
	UNREFERENCED_PARAMETER(Parameters);

	return STATUS_SUCCESS;
}

//
// PNP ID Routines
//

#define NDASDLU_HARDWARE_ID_COUNT		3
#define NDASDLU_COMPATIABLE_ID_COUNT	1

NTSTATUS
NdasDluQueryPnpID(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in BUS_QUERY_ID_TYPE QueryType,
	__in ULONG Index,
	__inout PUNICODE_STRING UnicodeStringId)
{
	static CONST WCHAR* HARDWARE_IDS[NDASDLU_HARDWARE_ID_COUNT] = { 
		NDASPORT_ENUMERATOR_GUID_PREFIX L"NdasDlu",
		NDASPORT_ENUMERATOR_NAMED_PREFIX L"NdasDlu", };
	static CONST WCHAR* COMPATIBLE_IDS[NDASDLU_COMPATIABLE_ID_COUNT] = {NULL,};
	WCHAR instanceId[20];
	WCHAR* instanceIdList[] = { instanceId };

	NTSTATUS status;
	PNDAS_DLU_EXTENSION ndasDluExtension;

	CONST WCHAR** idList;
	ULONG idListCount;
	CONST NDASPORT_SCSI_DEVICE_TYPE *scsiDeviceType;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	// Build hardware IDs, compatible IDs
	scsiDeviceType = NdasPortExGetScsiDeviceTypeInfo(ndasDluExtension->InquiryData.DeviceType);
	if(scsiDeviceType == NULL) {
		return STATUS_UNSUCCESSFUL;
	}
	HARDWARE_IDS[NDASDLU_HARDWARE_ID_COUNT-1] = scsiDeviceType->GenericTypeString;
	COMPATIBLE_IDS[0] = scsiDeviceType->GenericTypeString;

	switch (QueryType)
	{
	case BusQueryDeviceID:
		idList = HARDWARE_IDS;
		idListCount = countof(HARDWARE_IDS);
		break;
	case BusQueryHardwareIDs:
		idList = HARDWARE_IDS;
		idListCount = countof(HARDWARE_IDS);
		break;
	case BusQueryCompatibleIDs:
		idList = COMPATIBLE_IDS;
		idListCount = countof(COMPATIBLE_IDS);
		break;
	case BusQueryInstanceID:
		idList = (const WCHAR**) instanceIdList;
		idListCount = 1;
		status = RtlStringCchPrintfW(
			instanceId, 
			countof(instanceId),
			L"%08X", 
			RtlUlongByteSwap(ndasDluExtension->LogicalUnitAddress));
		ASSERT(NT_SUCCESS(status));
		break;
	case 4 /*BusQueryDeviceSerialNumber*/:
	default:
		return STATUS_NOT_SUPPORTED;
	}

	if (Index >= idListCount)
	{
		return STATUS_NO_MORE_ENTRIES;
	}

	status = RtlUnicodeStringCopyString(
		UnicodeStringId,
		idList[Index]);

	return status;
}

static
PWCHAR
NdasDluGetDeviceTextDescription(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension
){
	PNDAS_DLU_EXTENSION ndasDluExtension;
	UCHAR ansiBuffer[256];
	ANSI_STRING ansiText;
	UNICODE_STRING unicodeText;
	NTSTATUS status;
	CONST NDASPORT_SCSI_DEVICE_TYPE *scsiDeviceType;
	PUCHAR c;
	LONG i;
	PWCHAR	outputText;

	PAGED_CODE();

	RtlInitUnicodeString(&unicodeText, NULL);
	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	if(ndasDluExtension->LuNameLength == 0) {
		scsiDeviceType = 
			NdasPortExGetScsiDeviceTypeInfo(ndasDluExtension->InquiryData.DeviceType);


		RtlZeroMemory(ansiBuffer, sizeof(ansiBuffer));

		//
		// Build "VendorID ProductID" ANSI string.
		//

		// Initialize the ANSI temporary buffer with Vendor ID
		RtlCopyMemory(ansiBuffer, 
			ndasDluExtension->InquiryData.VendorId,
			sizeof(ndasDluExtension->InquiryData.VendorId));
		c = ansiBuffer;

		// Remove the trailers
		for(i = sizeof(ndasDluExtension->InquiryData.VendorId); i >= 0; i--) {
			if((c[i] != '\0') &&
				(c[i] != ' ')) {
					break;
			}
			c[i] = '\0';
		}

		// Add a blank.
		c = &(c[i + 1]);
		*c = ' ';
		c++;

		// Add a product ID
		RtlCopyMemory(c,
			ndasDluExtension->InquiryData.ProductId,
			sizeof(ndasDluExtension->InquiryData.ProductId));

		// Remove the trailers
		for(i = sizeof(ndasDluExtension->InquiryData.ProductId); i >= 0; i--) {
			if((c[i] != '\0') &&
				(c[i] != ' ')) {
					break;
			}
			c[i] = '\0';
		}
		c[i+1] = '\0';

		//
		// Translate ANSI text to UNICODE text
		//

		RtlInitAnsiString(&ansiText, ansiBuffer);
		status = RtlAnsiStringToUnicodeString(&unicodeText,
			&ansiText,
			TRUE);
		if(!NT_SUCCESS(status)) {
			return NULL;
		}

		//
		// Add a device type string.
		//
		outputText = (PWCHAR)ExAllocatePoolWithTag(PagedPool, 256 * sizeof(WCHAR), NDAS_DLU_PTAG_DEVTEXT);

		status = RtlStringCchPrintfW(
						outputText,
						256,
						L"%s %s Device",
						unicodeText.Buffer, scsiDeviceType->DeviceTypeString);
		if(!NT_SUCCESS(status)) {
			ExFreePool(outputText);
			outputText = NULL;
		}

		//
		// Free "VendorID ProductID" text allocated by RtlAnsiStringToUnicodeString().
		//
		ExFreePool(unicodeText.Buffer);
	} else {
		outputText = (PWCHAR)ExAllocatePoolWithTag(
			PagedPool,
			MAX_LENGTH_LUR_NAME * sizeof(WCHAR),
			NDAS_DLU_PTAG_DEVTEXT);
		status = RtlStringCchCopyW(
			outputText,
			MAX_LENGTH_LUR_NAME,
			ndasDluExtension->LuName);
		if(!NT_SUCCESS(status)) {
			ExFreePool(outputText);
			outputText = NULL;
		}
	}

	return outputText;
}

NTSTATUS
NdasDluQueryPnpDeviceText(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in DEVICE_TEXT_TYPE DeviceTextType,
	__in LCID Locale,
	__in PWCHAR* DeviceText)
{
	NTSTATUS status;
	PNDAS_DLU_EXTENSION ndasDluExtension;

	UNREFERENCED_PARAMETER(Locale);
	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	switch (DeviceTextType)
	{
	case DeviceTextDescription:
		
		*DeviceText = NdasDluGetDeviceTextDescription(LogicalUnitExtension);
		if (NULL == *DeviceText)
		{
			status = STATUS_INSUFFICIENT_RESOURCES;
		} else {
			status = STATUS_SUCCESS; 
		}

		ASSERT(NT_SUCCESS(status));
		return status;
	case DeviceTextLocationInformation:
		{
			*DeviceText = ExAllocatePoolWithTag(PagedPool, 256 * sizeof(WCHAR), NDAS_DLU_PTAG_DEVTEXT);
			if(*DeviceText == NULL) {
				return STATUS_INSUFFICIENT_RESOURCES;
			}
			status = RtlStringCchPrintfW(
				*DeviceText, 256,
				L"Bus Number %d, Target ID %d, LUN %d",
				ndasDluExtension->PathId,
				ndasDluExtension->TargetId,
				ndasDluExtension->Lun);

			return status;
		}
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}
}

NTSTATUS
NdasDluQueryPnpDeviceCapabilities(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__inout PDEVICE_CAPABILITIES Capabilities)
{
	PNDAS_DLU_EXTENSION ndasDluExtension;
	DEVICE_CAPABILITIES deviceCaps;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	RtlZeroMemory(&deviceCaps, sizeof(DEVICE_CAPABILITIES));
	deviceCaps.Version = 1;
	deviceCaps.Size = sizeof(DEVICE_CAPABILITIES);
	deviceCaps.Removable = TRUE;
	deviceCaps.EjectSupported = TRUE;
	deviceCaps.SurpriseRemovalOK = FALSE;
	deviceCaps.Address = ndasDluExtension->LogicalUnitAddress;
	deviceCaps.UINumber = 0xFFFFFFFF;

	if (Capabilities->Version != 1)
	{
		return STATUS_NOT_SUPPORTED;
	}
	if (Capabilities->Size < sizeof(DEVICE_CAPABILITIES))
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(
		Capabilities,
		&deviceCaps,
		min(sizeof(DEVICE_CAPABILITIES), Capabilities->Size));

	return STATUS_SUCCESS;
}

//
// Storage Device Property
//

NTSTATUS
NdasDluQueryStorageDeviceProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_DEVICE_DESCRIPTOR DeviceDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PNDAS_DLU_EXTENSION ndasDluExtension;
	STORAGE_DEVICE_DESCRIPTOR tmp;
	ULONG offset;
	ULONG realLength;
	PINQUIRYDATA inquiryData;
	ULONG inquiryDataLength;

	//
	// Fill the storage device descriptor as much as possible
	//

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	inquiryData = &ndasDluExtension->InquiryData;
	inquiryDataLength = sizeof(INQUIRYDATA);

	//
	// Zero out the provided buffer
	//
	RtlZeroMemory(DeviceDescriptor, BufferLength);

	realLength = sizeof(STORAGE_DEVICE_DESCRIPTOR) - 1 +
		inquiryDataLength +
		sizeof(inquiryData->VendorId) +
		sizeof(inquiryData->ProductId) +
		sizeof(inquiryData->ProductRevisionLevel);

	RtlZeroMemory(&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR));
	tmp.Version = sizeof(STORAGE_DEVICE_DESCRIPTOR);
	tmp.Size = realLength;
	tmp.DeviceType = inquiryData->DeviceType;
	tmp.DeviceTypeModifier = inquiryData->DeviceTypeModifier;
	tmp.RemovableMedia = inquiryData->RemovableMedia;
	tmp.CommandQueueing = inquiryData->CommandQueue;
	tmp.VendorIdOffset;
	tmp.ProductIdOffset;
	tmp.ProductRevisionOffset;
	tmp.SerialNumberOffset;
	tmp.BusType = BusTypeScsi;
	// INQUIRYDATA or ATAIDENTIFYDATA is stored in RawDeviceProperties
	tmp.RawPropertiesLength;
	tmp.RawDeviceProperties[0];

	offset = 0;

	//
	// Copies up to sizeof(STORAGE_DEVICE_DESCRIPTOR),
	// excluding RawDeviceProperties[0]
	//

	NdasPortSetStoragePropertyData(
		&tmp, sizeof(STORAGE_DEVICE_DESCRIPTOR) - 1,
		DeviceDescriptor, BufferLength,
		&offset, NULL);

	//
	// Set Raw Device Properties
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyData(
			inquiryData, inquiryDataLength,
			DeviceDescriptor, BufferLength,
			&offset, 
			NULL);
		tmp.RawPropertiesLength  = inquiryDataLength;
	}

	//
	// Set Vendor Id
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->VendorId, sizeof(inquiryData->VendorId), 
			DeviceDescriptor, BufferLength, 
			&offset,
			&DeviceDescriptor->VendorIdOffset);
	}

	//
	// Set Product Id
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->ProductId, sizeof(inquiryData->ProductId), 
			DeviceDescriptor, BufferLength,
			&offset, 
			&DeviceDescriptor->ProductIdOffset);
	}

	//
	// Set Product Revision
	//
	if (offset < BufferLength)
	{
		NdasPortSetStoragePropertyString(
			inquiryData->ProductRevisionLevel, sizeof(inquiryData->ProductRevisionLevel), 
			DeviceDescriptor, BufferLength,
			&offset, 
			&DeviceDescriptor->ProductRevisionOffset);
	}

	*ResultLength = offset;

	return STATUS_SUCCESS;
}

static
VOID
NdasDluInitializeStorageAdapterDescriptor(
	__in PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	__in ULONG	MaximumTransferLength)
{
	PNDAS_DLU_EXTENSION ndasDluExtension;
	PSTORAGE_ADAPTER_DESCRIPTOR tmp;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);
	if(ndasDluExtension == NULL)
		return;

	tmp = &ndasDluExtension->StorageAdapterDescriptor;

	RtlZeroMemory(tmp, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	tmp->Version = sizeof(STORAGE_ADAPTER_DESCRIPTOR);
	tmp->Size = sizeof(STORAGE_ADAPTER_DESCRIPTOR);

	tmp->MaximumTransferLength = MaximumTransferLength;
	tmp->MaximumPhysicalPages = 0xFFFFFFFF;

	tmp->AlignmentMask = FILE_BYTE_ALIGNMENT;

	tmp->AdapterUsesPio = FALSE;
	tmp->AdapterScansDown = FALSE;
	tmp->CommandQueueing =TRUE ;
	tmp->AcceleratedTransfer = FALSE;

	tmp->BusType = BusTypeScsi;
	tmp->BusMajorVersion = 0x04;
	tmp->BusMinorVersion = 0x00;
}


NTSTATUS
NdasDluQueryStorageAdapterProperty(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out_bcount(BufferLength) PSTORAGE_ADAPTER_DESCRIPTOR AdapterDescriptor,
	__in ULONG BufferLength,
	__out PULONG ResultLength)
{
	PNDAS_DLU_EXTENSION ndasDluExtension;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	*ResultLength = min(BufferLength, sizeof(STORAGE_ADAPTER_DESCRIPTOR));

	RtlCopyMemory(
		AdapterDescriptor,
		&ndasDluExtension->StorageAdapterDescriptor,
		*ResultLength);

	return STATUS_SUCCESS;
}

#define NDASDLU_VALUEKEY_NAME_USER_REMOVALPOLICY	L"UserRemovalPolicy"

NTSTATUS
NdasDluSetRemovalPolicy(
	__in PDEVICE_OBJECT			PhysicalDeviceObject,
	__in DEVICE_REMOVAL_POLICY 	RemovalPolicy
){
	NTSTATUS status;
	HANDLE rootKeyHandle, targetKeyHandle;
	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING unicodeKeyName;
	UINT32	keyValue;
	ULONG  disposition;

	status = IoOpenDeviceRegistryKey(
		PhysicalDeviceObject,
		PLUGPLAY_REGKEY_DEVICE, 
		KEY_READ | KEY_WRITE,
		&rootKeyHandle);

	if (!NT_SUCCESS(status))
	{
		KDPrint(1,
			("IoOpenDeviceRegistryKey failed, status=%X, pdo=%p\n", 
			status, PhysicalDeviceObject));
		return status;
	}

	//
	// Open 'Classpnp' key.
	//
	RtlInitUnicodeString(&unicodeKeyName, L"Classpnp");
	InitializeObjectAttributes(
		&objectAttributes,
		&unicodeKeyName,
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
		rootKeyHandle,
		NULL);
	status = ZwCreateKey(&targetKeyHandle,
		KEY_READ | KEY_WRITE,
		&objectAttributes,
		0, NULL, 0,
		&disposition
		);
	if (!NT_SUCCESS(status))
	{
		KDPrint(1,
			("IoOpenDeviceRegistryKey failed, status=%X, pdo=%p\n", 
			status, PhysicalDeviceObject));
		ZwClose(rootKeyHandle);
		return status;
	}
	//
	// If Classpnp registry key exists, do not touch anything.
	// Set removal policy only before Classpnp creates it. 
	//
	if(disposition == REG_OPENED_EXISTING_KEY) {
		ZwClose(targetKeyHandle);
		ZwClose(rootKeyHandle);
		return status;
	}

	keyValue = RemovalPolicy;
	status = RtlWriteRegistryValue(
		RTL_REGISTRY_HANDLE,
		(PWSTR)targetKeyHandle,
		NDASDLU_VALUEKEY_NAME_USER_REMOVALPOLICY,
		REG_DWORD,
		&keyValue,
		sizeof(UINT32));

	if (!NT_SUCCESS(status))
	{
		KDPrint(1,
			("RtlWriteRegistryValue failed, status=%X, pdo=%p\n", 
			status, PhysicalDeviceObject));
	}

	ZwClose(targetKeyHandle);
	ZwClose(rootKeyHandle);

	return status;
}

//
// Scsi IO Routines
//

BOOLEAN
NdasDluBuildIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	UNREFERENCED_PARAMETER(LogicalUnitExtension);
	UNREFERENCED_PARAMETER(Srb);

	return TRUE;
}


//
//
//

static
BOOLEAN
NdasDluResetBus(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogiocalUnitExtension,
	IN ULONG	PathId
	)
{
	NTSTATUS	status;
	KIRQL		oldIrql;
	PNDAS_DLU_EXTENSION	dluExtension = NdasDluGetExtension(LogiocalUnitExtension);

	KDPrint(1, ("PathId = %d KeGetCurrentIrql()= 0x%x\n", 
						PathId, KeGetCurrentIrql()));

	ASSERT(PathId <= 0xFF);
	ASSERT(SCSI_MAXIMUM_TARGETS_PER_BUS <= 0xFF);
	ASSERT(SCSI_MAXIMUM_LOGICAL_UNITS <= 0xFF);

#if !DBG
	UNREFERENCED_PARAMETER(PathId);
#endif

	//
	// Log bus-reset error.
	//

	NdasDluLogError(
		LogiocalUnitExtension,
		NULL,
		0,
		0,
		0,
		NDASSCSI_IO_BUSRESET_OCCUR,
		EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_BUSRESET_OCCUR, 0)
		);

	//
	//	Send a CCB to the root of LURelation.
	//
	status = NdasDluSendCcbToAllLURs(
					LogiocalUnitExtension,
					CCB_OPCODE_RESETBUS,
					NULL,
					NULL
				);
	if(!NT_SUCCESS(status)) {
		KDPrint(1, ("SendCcbToAllLIRs() failed. STATUS=0x%08lx\n", status));
	}

	ACQUIRE_SPIN_LOCK(&dluExtension->DluSpinLock, &oldIrql);

	DLUINTERNAL_SETSTATUSFLAG(dluExtension, DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING);

	RELEASE_SPIN_LOCK(&dluExtension->DluSpinLock, oldIrql);


	//
	// Indicate ready for next request.
	//

	NdasPortNotification(	NextLuRequest,
							LogiocalUnitExtension,
							NULL);


	return TRUE;
}

//
// Process SRBs before sending them to the LUR
// NOTE: This routine executes in the context of ScsiPort synchronized callback.
//

static
NTSTATUS
NdasDluExecuteScsi(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PSCSI_REQUEST_BLOCK			Srb
){
    NTSTATUS	status;
	PLURELATION	LUR;
	KIRQL		oldIrql;
	PNDAS_DLU_EXTENSION			ndasDluExtension;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	ASSERT(NULL != ndasDluExtension);


	status = STATUS_MORE_PROCESSING_REQUIRED;
	LUR = NULL;

	switch(Srb->Cdb[0]) {
	case 0xd8:
//		ASSERT(FALSE);
		break;
	case SCSIOP_INQUIRY: {

		//
		//	Block INQUIRY when NDASSCSI is stopping.
		//
		ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);
		if(DLUINTERNAL_ISSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_STOPPING)) {
			KDPrint(1,("SCSIOP_INQUIRY: DLUINTERNAL_STATUS_STOPPING. returned with error.\n"));

			NdasDluLogError(
				LogicalUnitExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_INQUIRY_WHILE_STOPPING,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_DURING_STOPPING, 0)
				);

			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
		}
		RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);


		//
		//	look up Logical Unit Relations and set it to LuExtension
		//

		LUR = ndasDluExtension->LUR;

		if(	LUR &&
			LUR->LurId[0] == Srb->PathId &&
			LUR->LurId[1] == Srb->TargetId &&
			LUR->LurId[2] == Srb->Lun
			) {
			KDPrint(1,("SCSIOP_INQUIRY: set LUR(%p) to LuExtension(%p)\n", LUR, LogicalUnitExtension));
			break;
		} else {
			KDPrint(1,("LUR == NULL DluExtension(%p) LuExtension(%p)\n",
				ndasDluExtension, LogicalUnitExtension));
#if DBG
			NdasDluLogError(
				LogicalUnitExtension,
				Srb,
				Srb->PathId,
				Srb->TargetId,
				Srb->Lun,
				NDASSCSI_IO_LUR_NOT_FOUND,
				EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_INQUIRY_LUR_NOT_FOUND, 0)
				);
#endif
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
			Srb->ScsiStatus = SCSISTAT_GOOD;
			status = STATUS_SUCCESS;
		}
		break;
	}
	default: {
		break;
		}
	}

	return status;
}


VOID
NdasDluPnpSrbWoker(
    IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
    IN PNDASDLU_WORKITEM_CONTEXT	NdasDluWorkItemContext
){
	PDEVICE_OBJECT	pdo = NdasPortExGetWdmDeviceObject(LogicalUnitExtension);
	PSCSI_PNP_REQUEST_BLOCK pnpSrb = (PSCSI_PNP_REQUEST_BLOCK)NdasDluWorkItemContext->Arg1;

	//
	// Dispatch the PNP SRB.
	//
	switch(pnpSrb->PnPAction) {
		case StorStartDevice:
			NdasDluSetRemovalPolicy(pdo, RemovalPolicyExpectSurpriseRemoval);
			break;
		default: ;
	}

	//
	// Set SRB status
	//

	pnpSrb->SrbStatus = SRB_STATUS_SUCCESS;

	//
	//	Complete the request.
	//
	ASSERT(pnpSrb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
	NdasPortNotification(
		RequestComplete,
		LogicalUnitExtension,
		pnpSrb
		);

	NdasPortNotification(
		NextLuRequest,
		LogicalUnitExtension,
		NULL
		);
}

BOOLEAN
NdasDluPnpSrb(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__out PSCSI_PNP_REQUEST_BLOCK Srb
) {
	NTSTATUS	status;
	NDASDLU_WORKITEM_INIT	ndasDluWorkItemInit;

	ndasDluWorkItemInit.WorkitemRoutine = NdasDluPnpSrbWoker;
	ndasDluWorkItemInit.Ccb = NULL;
	ndasDluWorkItemInit.Arg1 = Srb;
	ndasDluWorkItemInit.Arg2 = NULL;
	ndasDluWorkItemInit.Arg3 = NULL;

	status = NdasDluQueueWorkItem(
		LogicalUnitExtension,
		&ndasDluWorkItemInit
		);
	if(!NT_SUCCESS(status)) {
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		Srb->SrbStatus = SRB_STATUS_ERROR;
		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			Srb
			);
		NdasPortNotification(
			NextLuRequest,
			LogicalUnitExtension,
			NULL
			);
		return FALSE;
	}

	return TRUE;
}

BOOLEAN
NdasDluStartIo(
	__in PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	__in PSCSI_REQUEST_BLOCK Srb)
{
	PNDAS_DLU_EXTENSION			ndasDluExtension;
	PCCB						ccb;
	NTSTATUS					status;
	KIRQL						oldIrql;
	LONG						requestCount;

	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	ASSERT(NULL != ndasDluExtension);
	ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
#if 0
	if(Srb->Function == SRB_FUNCTION_EXECUTE_SCSI &&
		Srb->Cdb[0] == SCSIOP_MODE_SENSE10)  {

		ASSERT(Srb->DataTransferLength != 0);
	}
#endif
#if 1
	if(Srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
		DluPrintSrb("[StartIo]", Srb);
	}
#endif
	//
	//	Clear BusReset flags because a normal request is entered.
	//
	ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);
	if(DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING)) {
		DLUINTERNAL_RESETSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_BUSRESET_PENDING);
		KDPrint(1,("Bus Reset status cleared.\n"));
	}
	RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);


	//
	//	check the adapter status
	//
	ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);
	if(DLUINTERNAL_ISSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_STOPPING) ||
		DLUINTERNAL_ISSTATUSFLAG(ndasDluExtension, DLUINTERNAL_STATUSFLAG_RESTARTING)
		) {
		RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);

#if DBG
		if(DLUINTERNAL_ISSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_STOPPING)) {
			KDPrint(1,("Error! stopping in progress due to internal error.\n"));
		} else {
			KDPrint(1,("Delay the request due to reinit.\n"));
		}
#endif
		KDPrint(1,("Func:%x %s(0x%02x) Srb(%p) CdbLen=%d\n",
			Srb->Function,
			CdbOperationString(Srb->Cdb[0]),
			(int)Srb->Cdb[0],
			Srb,
			(UCHAR)Srb->CdbLength
			));
		KDPrint(1,(" TxLen:%d TO:%d SenseLen:%d SrbFlags:%08lx\n",
			Srb->DataTransferLength,
			Srb->TimeOutValue,
			Srb->SenseInfoBufferLength,
			Srb->SrbFlags));
		ASSERT(!Srb->NextSrb);

		Srb->ScsiStatus = SCSISTAT_GOOD;
		if(DLUINTERNAL_ISSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_STOPPING)) {
			Srb->DataTransferLength = 0;
			Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		} else {
			Srb->SrbStatus = SRB_STATUS_BUSY;
		}
		//
		//	Complete the SRB.
		//
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			Srb
			);

		NdasPortNotification(
			NextLuRequest,
			LogicalUnitExtension,
			NULL
			);

		return TRUE;
	}
	RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);

	if(Srb->Function != SRB_FUNCTION_EXECUTE_SCSI) {
		KDPrint(1,("--->0x%x, Srb->Function = %s, Srb->PathId = %d, Srb->TargetId = %d Srb->Lun = %d\n",
			Srb->Function, SrbFunctionCodeString(Srb->Function), Srb->PathId, Srb->TargetId, Srb->Lun));
	}

	//
	//	SRB dispatcher
	//
	switch(Srb->Function) {

	case SRB_FUNCTION_ABORT_COMMAND:
		KDPrint(1,("ABORT_COMMAND: Srb = %p Srb->NextSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));
		NdasDluLogError(
			LogicalUnitExtension,
			Srb,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun,
			NDASSCSI_IO_ABORT_SRB,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_ABORT_SRB_ENTERED, 0)
			);
		ASSERT(Srb->NextSrb);
		ASSERT(FALSE);
		break;

	case SRB_FUNCTION_RESET_BUS:
		KDPrint(1,("RESET_BUS: Srb = %p\n", Srb));

		NdasDluResetBus(LogicalUnitExtension, Srb->PathId);

		//
		//	Complete the request.
		//
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		NdasPortNotification(
				RequestComplete,
				LogicalUnitExtension,
				Srb
			);

		NdasPortNotification(
				NextLuRequest,
				LogicalUnitExtension,
				NULL
			);

		return TRUE;
	case SRB_FUNCTION_POWER: {
		PSCSI_POWER_REQUEST_BLOCK	srb = (PSCSI_POWER_REQUEST_BLOCK)Srb;
		KDPrint(1,("POWER: Srb=%p DevicePowerState=%d PowerAction=%d\n", srb, srb->DevicePowerState, srb->PowerAction));

		//
		//	Complete the request.
		//
		ASSERT(srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		NdasPortNotification(
			RequestComplete,
			LogicalUnitExtension,
			srb
			);

		NdasPortNotification(
			NextLuRequest,
			LogicalUnitExtension,
			NULL
			);

		return TRUE;
	}

	case SRB_FUNCTION_PNP: {
		PSCSI_PNP_REQUEST_BLOCK srb = (PSCSI_PNP_REQUEST_BLOCK)Srb;
		KDPrint(1,("PNP: Srb=%p PnPAction=%d\n", srb, srb->PnPAction));

		return NdasDluPnpSrb(LogicalUnitExtension, srb);
	}
	case SRB_FUNCTION_SHUTDOWN:
		KDPrint(1,("SHUTDOWN: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));

		break; // Pass to lower LUR
	case SRB_FUNCTION_FLUSH: 
		KDPrint(1,("FLUSH: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));
		break; // Pass to lower LUR
	case SRB_FUNCTION_IO_CONTROL:
		KDPrint(1,("IO_CONTROL: Srb = %p Ccb->AbortSrb = %p, Srb->CdbLength = %d\n", Srb, Srb->NextSrb, (UCHAR)Srb->CdbLength ));

		status = NdasDluSrbControl(LogicalUnitExtension, Srb);
		if(status == STATUS_PENDING) {
			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb pending.\n" ));
			return TRUE;
		} else if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			//
			//	Complete the request.
			//
			ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
			NdasPortNotification(
					RequestComplete,
					LogicalUnitExtension,
					Srb
				);

			NdasPortNotification(
					NextLuRequest,
					LogicalUnitExtension,
					NULL
				);

			KDPrint(1,("SRB_FUNCTION_IO_CONTROL: Srb completed.\n" ));
			return TRUE;
		}
		KDPrint(1,("SRB_FUNCTION_IO_CONTROL: going to LUR\n" ));
		break;

	case SRB_FUNCTION_EXECUTE_SCSI:

#if DBG
		/*if(	(Srb->Cdb[0] != SCSIOP_READ &&
			Srb->Cdb[0] != SCSIOP_READ16 &&
			Srb->Cdb[0] != SCSIOP_WRITE &&
			Srb->Cdb[0] != SCSIOP_WRITE16 &&
			Srb->Cdb[0] != SCSIOP_VERIFY &&
			Srb->Cdb[0] != SCSIOP_VERIFY16 &&
			Srb->Cdb[0] != SCSIOP_READ_CAPACITY &&
			Srb->Cdb[0] != SCSIOP_READ_CAPACITY16) || 
			(Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT))*/
		{

			if(Srb->Cdb[0] == SCSIOP_READ ||
				Srb->Cdb[0] == SCSIOP_WRITE ||
				Srb->Cdb[0] == SCSIOP_VERIFY) {

				UINT32	logicalBlockAddress;
				ULONG	transferBlocks;

				logicalBlockAddress = Cdb10LogicalBlockAddress((PCDB)Srb->Cdb);
				transferBlocks = Cdb10TransferBlocks((PCDB)Srb->Cdb);
				KDPrintCont(1,("[StartIo] %p %s(%X,%d):%u,%u,%u Tx:%d S:%d To:%d F:%x\n",
					Srb,
					CdbOperationString(Srb->Cdb[0]),
					(int)Srb->Cdb[0],
					(UCHAR)Srb->CdbLength,
					((PCDB)Srb->Cdb)->CDB10.ForceUnitAccess,
					logicalBlockAddress,
					transferBlocks,
					Srb->DataTransferLength,
					Srb->SenseInfoBufferLength,
					Srb->TimeOutValue,
					Srb->SrbFlags));

			} else if(Srb->Cdb[0] == SCSIOP_READ16 ||
					Srb->Cdb[0] == SCSIOP_WRITE16 ||
					Srb->Cdb[0] == SCSIOP_VERIFY16)	{

				UINT64	logicalBlockAddress;
				ULONG	transferBlocks;

				logicalBlockAddress = CDB16_LOGICAL_BLOCK_BYTE((PCDB)Srb->Cdb);
				transferBlocks = CDB16_TRANSFER_BLOCKS((PCDB)Srb->Cdb);
				KDPrintCont(1,("[StartIo] %p %s(%X,%d):%u,%u,%u Tx:%d S:%d To:%d F:%x\n",
					Srb,
					CdbOperationString(Srb->Cdb[0]),
					(int)Srb->Cdb[0],
					(UCHAR)Srb->CdbLength,
					((PCDBEXT)Srb->Cdb)->CDB16.ForceUnitAccess,
					logicalBlockAddress,
					transferBlocks,
					Srb->DataTransferLength,
					Srb->SenseInfoBufferLength,
					Srb->TimeOutValue,
					Srb->SrbFlags));
			} else {
	
				KDPrintCont(1,("[StartIo] %p %s(%X,%d) Tx:%d S:%d To:%d F:%x\n",
												Srb,
												CdbOperationString(Srb->Cdb[0]),
												(int)Srb->Cdb[0],
												(UCHAR)Srb->CdbLength,
												Srb->DataTransferLength,
												Srb->SenseInfoBufferLength,
												Srb->TimeOutValue,
												Srb->SrbFlags));
			}
			ASSERT(!Srb->NextSrb);

		}
#endif

		status = NdasDluExecuteScsi(LogicalUnitExtension, Srb);
		if(status != STATUS_MORE_PROCESSING_REQUIRED) {

			KDPrint(1,("SRB_FUNCTION_EXECUTE_SCSI: Srb = %p completed.\n", Srb));
			//
			//	Complete the request.
			//

			ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);

			NdasPortNotification(
					RequestComplete,
					LogicalUnitExtension,
					Srb
				);
			NdasPortNotification(
					NextLuRequest,
					LogicalUnitExtension,
					NULL
				);

			return TRUE;
		}

		break;

	case SRB_FUNCTION_RESET_DEVICE:
			KDPrint(1,("RESET_DEVICE: CurrentSrb is Presented. Srb = %x\n", Srb));

	default:
			KDPrint(1,("Invalid SRB Function:%d Srb = %x\n", 
									Srb->Function,Srb));

		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		//
		//	Complete the request.
		//
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		NdasPortNotification(
				RequestComplete,
				LogicalUnitExtension,
				Srb
			);

		NdasPortNotification(
				NextLuRequest,
				LogicalUnitExtension,
				NULL
			);
		return TRUE;
	}


	//	
	//	initialize Ccb in srb to call LUR dispatchers.
	//
#if __NDASDLU_USE_POOL_FOR_CCBALLOCATION__
	status = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(status)) {
		NdasDluLogError(
			LogicalUnitExtension,
			Srb,
			Srb->PathId,
			Srb->TargetId,
			Srb->Lun,
			NDASSCSI_IO_CCBALLOC_FAIL,
			EVTLOG_UNIQUEID(EVTLOG_MODULE_ADAPTERCONTROL, EVTLOG_CCB_ALLOCATION_FAIL, 0)
			);

		Srb->SrbStatus = SRB_STATUS_ERROR;
		KDPrint(1,("LsCcbAllocate() failed.\n" ));
		return TRUE;
	}
#else
	ccb = (PCCB)Srb->SrbExtension;
#endif
	LsCcbInitialize(
			Srb,
			LogicalUnitExtension,
			ccb
		);
#if  __NDASDLU_USE_POOL_FOR_CCBALLOCATION__
	ccb->Flags |= CCB_FLAG_ALLOCATED;
#endif

	requestCount = InterlockedIncrement(&ndasDluExtension->RequestExecuting);
	LsuIncrementTdiClientInProgress();

	LsCcbSetCompletionRoutine(ccb, NdasDluCcbCompletion, LogicalUnitExtension);

	//
	// Workaround for cache synchronization command failure while entering hibernation mode.
	//
	if(Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE ||
		Srb->Cdb[0] == SCSIOP_SYNCHRONIZE_CACHE16 ||
		Srb->Cdb[0] == SCSIOP_START_STOP_UNIT) {

		//
		// start/stop, and cache synchronization command while entering hibernation.
		//
		if(Srb->SrbFlags & SRB_FLAGS_BYPASS_LOCKED_QUEUE) {
			KDPrint(1, ("SRB %p must succeed.\n", Srb));
			ccb->Flags |= CCB_FLAG_MUST_SUCCEED;
		}
	}

	//
	//	Send a CCB to LURelation.
	//

	status = LurRequest(
					ndasDluExtension->LUR,
					ccb
				);

	//
	//	If failure, SRB was not queued. Complete it with an error.
	//

	if( !NT_SUCCESS(status) ) {

		ASSERT(FALSE);

		//
		//	Free resources for the SRB.
		//

		InterlockedDecrement(&ndasDluExtension->RequestExecuting);
		LsuDecrementTdiClientInProgress();

		LsCcbFree(ccb);

		//
		//	Complete the SRB with an error.
		//

		Srb->SrbStatus = SRB_STATUS_NO_DEVICE;
		Srb->ScsiStatus = SCSISTAT_GOOD;
		ASSERT(Srb->SrbFlags & SRB_FLAGS_IS_ACTIVE);
		NdasPortNotification(
				RequestComplete,
				LogicalUnitExtension,
				Srb
			);
		NdasPortNotification(
				NextLuRequest,
				LogicalUnitExtension,
				NULL
			);
	} else {
		if(requestCount <= ndasDluExtension->MaximumRequests) {
			NdasPortNotification(
				NextLuRequest,
				LogicalUnitExtension,
				NULL
				);
		}

	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// CCB utility
//


NTSTATUS
NdasDluSendCcbToLURSync(
		PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		PLURELATION		LUR,
		UINT32			CcbOpCode
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;

	KDPrint(1,("entered.\n"));

	ntStatus = LsCcbAllocate(&ccb);
	if(!NT_SUCCESS(ntStatus)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	LSCCB_INITIALIZE(ccb);
	ccb->OperationCode				= CcbOpCode;
	ccb->LurId[0]					= LUR->LurId[0];
	ccb->LurId[1]					= LUR->LurId[1];
	ccb->LurId[2]					= LUR->LurId[2];
	ccb->LurId[3]					= LUR->LurId[3];
	ccb->HwDeviceExtension			= LogicalUnitExtension;
	LsCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
	LsCcbSetCompletionRoutine(ccb, NULL, NULL);

	//
	//	Send a CCB to the root of LURelation.
	//
	ntStatus = LurRequest(
			LUR,
			ccb
		);
	if(!NT_SUCCESS(ntStatus)) {
		LsCcbPostCompleteCcb(ccb);
		return ntStatus;
	}

	return STATUS_SUCCESS;
}

static
NTSTATUS
NdasDluSendCcbToAllLURs(
		PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		UINT32						CcbOpCode,
		CCB_COMPLETION_ROUTINE		CcbCompletionRoutine,
		PVOID						CompletionContext

	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	PNDAS_DLU_EXTENSION	ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);

	KDPrint(3,("entered.\n"));


		ntStatus = LsCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("allocation fail.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb);
		ccb->OperationCode				= CcbOpCode;
		ccb->LurId[0]					= ndasDluExtension->LUR->LurId[0];
		ccb->LurId[1]					= ndasDluExtension->LUR->LurId[1];
		ccb->LurId[2]					= ndasDluExtension->LUR->LurId[2];
		ccb->LurId[3]					= ndasDluExtension->LUR->LurId[3];
		ccb->HwDeviceExtension			= LogicalUnitExtension;
		LsCcbSetFlag(ccb, CCB_FLAG_ALLOCATED|CCB_FLAG_DATABUF_ALLOCATED);
		LsCcbSetCompletionRoutine(ccb, CcbCompletionRoutine, CompletionContext);

		//
		//	Send a CCB to the LUR.
		//
		ntStatus = LurRequest(
				ndasDluExtension->LUR,
				ccb
			);
		if(!NT_SUCCESS(ntStatus)) {
			KDPrint(1,("request fail.\n"));
			LsCcbPostCompleteCcb(ccb);
			return ntStatus;
		}

	KDPrint(3,("exit.\n"));

	return STATUS_SUCCESS;
}

static
NTSTATUS
NdasDluFreeAllLURs(
		LONG			LURCount,
		PLURELATION		*LURs
	) {
	LONG		idx_lur;

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		//
		//	call destroy routines for LURNs
		//
		if(LURs[idx_lur])
			LurClose(LURs[idx_lur]);
	}

	return STATUS_SUCCESS;
}

//
//	Send Stop Ccb to all LURs.
//
static
NTSTATUS
NdasDluSendStopCcbToAllLURsSync(
		LONG			LURCount,
		PLURELATION		*LURs
	) {
	PCCB		ccb;
	NTSTATUS	ntStatus;
	LONG		idx_lur;

	KDPrint(1,("entered.\n"));

	for(idx_lur = 0; idx_lur < LURCount; idx_lur ++ ) {

		if(LURs[idx_lur] == NULL) {
			continue;
		}

		ntStatus = LsCcbAllocate(&ccb);
		if(!NT_SUCCESS(ntStatus)) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LSCCB_INITIALIZE(ccb);
		ccb->OperationCode				= CCB_OPCODE_STOP;
		ccb->LurId[0]					= LURs[idx_lur]->LurId[0];
		ccb->LurId[1]					= LURs[idx_lur]->LurId[1];
		ccb->LurId[2]					= LURs[idx_lur]->LurId[2];
		ccb->LurId[3]					= LURs[idx_lur]->LurId[3];
		ccb->HwDeviceExtension			= NULL;
		LsCcbSetFlag(ccb, CCB_FLAG_SYNCHRONOUS|CCB_FLAG_ALLOCATED|CCB_FLAG_RETRY_NOT_ALLOWED);
		LsCcbSetCompletionRoutine(ccb, NULL, NULL);

		//
		//	Send a CCB to the root of LURelation.
		//
		ntStatus = LurRequest(
				LURs[idx_lur],
				ccb
			);
		//
		// Must not return pending for this synchronous request
		//
		ASSERT(ntStatus != STATUS_PENDING);
		if(!NT_SUCCESS(ntStatus)) {
			LsCcbPostCompleteCcb(ccb);
			KDPrint(1,("LurRequest() failed\n"));
			return ntStatus;
		}
	}

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
// Worker routines
//

//
//	Stop worker from MinportControl.
//	NOTE: It must not reference HwDeviceExtenstion while stopping.
//
VOID
NdasDluRetryWorker(
		IN PNDAS_LOGICALUNIT_EXTENSION	NdasLUExtension,
		IN PNDASDLU_WORKITEM_CONTEXT	DluWorkItemCtx
){
	PSCSI_REQUEST_BLOCK	srb = (PSCSI_REQUEST_BLOCK)DluWorkItemCtx->Arg1;
	BOOLEAN		bret;
	KIRQL		oldIrql;
	LARGE_INTEGER	interval;

	//
	// Delay 2 second before retrial.
	//
	interval.QuadPart = - 4 * NANO100_PER_SEC;
	KeDelayExecutionThread(KernelMode, FALSE, &interval);

	KDPrint(1,("Retrying SRB:%p\n", srb));
	//
	// Re-supply the SRB to the StartIo routine.
	//
	oldIrql = KeRaiseIrqlToDpcLevel();
	bret = NdasDluStartIo(NdasLUExtension, srb);
	KeLowerIrql(oldIrql);
	ASSERT(bret == TRUE);
}


//
//	NoOperation worker.
//	Send NoOperation Ioctl to Miniport itself.
//

VOID
NdasDluNoOperationWorker(
		IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
		IN PNDASDLU_WORKITEM_CONTEXT	DluWorkItemCtx
	) {
	NDSCIOCTL_NOOP	NoopData;

	KDPrint(1, ("entered.\n"));
	NoopData.NdasScsiAddress.PathId		= (BYTE)DluWorkItemCtx->Arg1;
	NoopData.NdasScsiAddress.TargetId	= (BYTE)DluWorkItemCtx->Arg2;
	NoopData.NdasScsiAddress.Lun		= (BYTE)DluWorkItemCtx->Arg3;

	if(NdasPortExIsLogicalUnitStarted(LogicalUnitExtension) == FALSE) {
		KDPrint(1,("PDO %p not running.\n",
			NdasPortExGetWdmDeviceObject(LogicalUnitExtension)));

		return;
	}

	NdasDluSendIoctlSrbAsynch(
			NdasPortExGetWdmDeviceObject(LogicalUnitExtension),
			NDASSCSI_IOCTL_NOOP,
			&NoopData,
			sizeof(NDSCIOCTL_NOOP),
			NULL,
			0
		);

}

//////////////////////////////////////////////////////////////////////////
//
//	Device-dedicated work item manipulation
//

static
VOID
NdasDluExecuteWorkItem(
	IN PDEVICE_OBJECT	DeviceObject,
	IN PVOID			Context
) {
	PNDASDLU_WORKITEM_CONTEXT	workItemCtx = (PNDASDLU_WORKITEM_CONTEXT)Context;

	UNREFERENCED_PARAMETER(DeviceObject);

	//
	// Call work item's function
	//

	if(workItemCtx->WorkitemRoutine) {
		workItemCtx->WorkitemRoutine(workItemCtx->TargetNdasLUExtension, workItemCtx);
	}

	//
	// Free a work item and context
	//

	IoFreeWorkItem(workItemCtx->OriginalWorkItem);
	ExFreePoolWithTag(workItemCtx, NDAS_DLU_PTAG_WORKITEM_XTX);


}

NTSTATUS
NdasDluQueueWorkItem(
	IN PNDAS_LOGICALUNIT_EXTENSION	LogicalUnitExtension,
	IN PNDASDLU_WORKITEM_INIT	NdasDluWorkItemInit
) {

	PIO_WORKITEM				workItem;
	PNDASDLU_WORKITEM_CONTEXT	workItemCtx;

	KDPrint(2,("entered.\n"));

	//
	// Create IO manager work item.
	//

	workItem = NdasPortExAllocateWorkItem(LogicalUnitExtension);
	if (NULL == workItem)
	{
		NdasPortTrace(NDASDLU_INIT, TRACE_LEVEL_WARNING,
			"NdasPortExAllocateWorkItem failed with out of resource error.\n");

		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Allocate NDAS DLU context
	//

	workItemCtx = (PNDASDLU_WORKITEM_CONTEXT)ExAllocatePoolWithTag(
						NonPagedPool,
						sizeof(NDASDLU_WORKITEM_CONTEXT),
						NDAS_DLU_PTAG_WORKITEM_XTX);
	if(!workItemCtx) {
		IoFreeWorkItem(workItem);
		ASSERT(FALSE);
		return STATUS_INSUFFICIENT_RESOURCES;
	}


	//
	//	Retrieve work item initializing values
	//

	workItemCtx->TargetNdasLUExtension = LogicalUnitExtension;
	workItemCtx->WorkitemRoutine = NdasDluWorkItemInit->WorkitemRoutine;
	workItemCtx->Ccb = NdasDluWorkItemInit->Ccb;
	workItemCtx->Arg1 = NdasDluWorkItemInit->Arg1;
	workItemCtx->Arg2 = NdasDluWorkItemInit->Arg2;
	workItemCtx->Arg3 = NdasDluWorkItemInit->Arg3;
	workItemCtx->OriginalWorkItem = workItem;


	//
	//	Insert the work item to the queue, and notify to the worker thread.
	//

	IoQueueWorkItem(
		workItem, 
		NdasDluExecuteWorkItem,
		DelayedWorkQueue,
		workItemCtx);

	KDPrint(2,("queued work item!!!!!!\n"));

	return STATUS_SUCCESS;
}



//////////////////////////////////////////////////////////////////////////
//
//	LurnCallback routine.
//

VOID
NdasDluLurnCallback(
	PLURELATION	Lur,
	PLURN_EVENT	LurnEvent
) {
	PNDAS_LOGICALUNIT_EXTENSION	logicalUnitExtension = Lur->AdapterHardwareExtension;
	PNDAS_DLU_EXTENSION			ndasDluExtension = NdasDluGetExtension(logicalUnitExtension);
	KIRQL						oldIrql;

	ASSERT(LurnEvent);
	KDPrint(1,("Lur:%p Event class:%x\n",
									Lur, LurnEvent->LurnEventClass));

	ACQUIRE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, &oldIrql);
	if(!DLUINTERNAL_ISSTATUS(ndasDluExtension, DLUINTERNAL_STATUS_RUNNING)) {
		RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);
		KDPrint(1,("NOOP_EVENT: Adapter %p not running. InternalStatus %x.\n",
			ndasDluExtension, ndasDluExtension->DluInternalStatus));
		return;
	}
	RELEASE_SPIN_LOCK(&ndasDluExtension->DluSpinLock, oldIrql);


	if(NdasPortExIsLogicalUnitStarted(logicalUnitExtension) == FALSE) {
		KDPrint(1,("NOOP_EVENT: PDO %p not running.\n",
			NdasPortExGetWdmDeviceObject(logicalUnitExtension)));
		return;
	}

	switch(LurnEvent->LurnEventClass) {

	case LURN_REQUEST_NOOP_EVENT: {
		NDASDLU_WORKITEM_INIT			WorkitemCtx;

		NDASDLU_INIT_WORKITEM(
					&WorkitemCtx,
					NdasDluNoOperationWorker,
					NULL,
					(PVOID)Lur->LurId[0],
					(PVOID)Lur->LurId[1],
					(PVOID)Lur->LurId[2]
				);
		NdasDluQueueWorkItem(logicalUnitExtension, &WorkitemCtx);

//		KDPrint(1,("LURN_REQUEST_NOOP_EVENT: AdapterStatus %x.\n",
//									AdapterStatus));
	break;
	}

	default:
		KDPrint(1,("Invalid event class:%x\n", LurnEvent->LurnEventClass));
	break;
	}

}


//////////////////////////////////////////////////////////////////////////
//
//	Event log
//

VOID
NdasDluLogError(
	IN PNDAS_LOGICALUNIT_EXTENSION LogicalUnitExtension,
	IN PSCSI_REQUEST_BLOCK Srb OPTIONAL,
	IN UCHAR PathId,
	IN UCHAR TargetId,
	IN UCHAR Lun,
	IN ULONG ErrorCode,
	IN ULONG UniqueId
){
	PDEVICE_OBJECT DeviceObject = NdasPortExGetWdmDeviceObject(LogicalUnitExtension);
	PNDAS_DLU_EXTENSION ndasDluExtension = NdasDluGetExtension(LogicalUnitExtension);
	LSU_ERROR_LOG_ENTRY errorLogEntry;

	UNREFERENCED_PARAMETER(Srb);

	if(DeviceObject == NULL) {
		KDPrint(1, ("HwDeviceExtension NULL!! \n"));
		return;
	}
	if(DeviceObject == NULL) {
		KDPrint(1, ("DeviceObject NULL!! \n"));
		return;
	}

	//
    // Save the error log data in the log entry.
    //

	errorLogEntry.ErrorCode = ErrorCode;
	errorLogEntry.MajorFunctionCode = IRP_MJ_SCSI;
	errorLogEntry.IoctlCode = 0;
    errorLogEntry.UniqueId = UniqueId;
	errorLogEntry.SequenceNumber = 0;
	errorLogEntry.ErrorLogRetryCount = 0;
	errorLogEntry.Parameter2 = ndasDluExtension->LogicalUnitAddress;
	errorLogEntry.DumpDataEntry = 4;
	errorLogEntry.DumpData[0] = TargetId;
	errorLogEntry.DumpData[1] = Lun;
	errorLogEntry.DumpData[2] = PathId;
	errorLogEntry.DumpData[3] = ErrorCode;

	LsuWriteLogErrorEntry(DeviceObject, &errorLogEntry);

	return;
}
