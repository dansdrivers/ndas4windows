#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <crtdbg.h>
#include "ndasportctl.h"
#include <xtl/xtlautores.h>

#include <initguid.h>
#include <ndas/filediskguid.h>

#ifdef WPP_TRACING
#define WPP_CONTROL_GUIDS \
	WPP_DEFINE_CONTROL_GUID(NdasPortCmdTraceGuid,(14D0A80C,FE91,4182,AD80,C34DF0812572), \
	WPP_DEFINE_BIT(Error)      \
	WPP_DEFINE_BIT(Unusual)    \
	WPP_DEFINE_BIT(Noise)      \
	)
#include "ndasportcmd.tmh"
#endif

class ErrorHolder
{
	const DWORD code;
	LPSTR description;
public:
	explicit ErrorHolder(DWORD errorCode = ::GetLastError()) throw() : 
		code(errorCode),
		description(NULL)
	{
	}
	~ErrorHolder() throw()
	{
		if (description) LocalFree(description);
		::SetLastError(code);
	}
	operator DWORD () const throw()
	{
		return code;
	}
	DWORD GetCode() const throw()
	{
		return code;
	}
	LPCSTR GetDescriptionA()
	{
		if (NULL != description)
		{
			return description;
		}
		LANGID langid = LANGIDFROMLCID(GetThreadLocale());

		LPSTR buffer = NULL;
		DWORD charCount = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL,
			code,
			langid,
			(LPSTR)&buffer,
			0,
			NULL);
		UNREFERENCED_PARAMETER(charCount);
		description = buffer;
		return buffer;
	}
private:
	ErrorHolder(const ErrorHolder& );
	operator = (ErrorHolder&);
};

void usage()
{
	_tprintf(_T("usage: ndasportctl <command> <parameters>\n")
		_T(" -p <address> 00:00:00:00:00:00[:unit-number] <ro|rw> [noreserve]\n")
		_T(" -p <address> filedisk <filepath> <size> <kb|mb|gb|tb>\n")
		_T(" -p <address> ramdisk <size> <kb|mb|gb|tb>\n")
		_T(" -p <address> ndasdlu [disk | aggr | stripe | odd | mo] <ro|rw> [ending address] [number of member disks] ")
		_T("00:00:00:00:00:00[:unit-number] [hw ver] [hw rev] ...\n")
		_T(" -e <address>\n")
		_T(" -u <address>\n"));
}

LPTSTR
GetErrorDescription(DWORD ErrorCode = ::GetLastError())
{
	DWORD charCount;
	LPTSTR buffer = NULL;

	charCount = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		ErrorCode,
		0,
		(LPTSTR)&buffer,
		0,
		NULL);

	return buffer;
}

FORCEINLINE
BOOL
CharToHex(TCHAR HexChar, PUCHAR HexValue)
{
	if (HexChar >= _T('0') && HexChar <= _T('9'))
	{
		*HexValue = static_cast<UCHAR>(HexChar - _T('0') + 0x00);
		return TRUE;
	}
	else if (HexChar >= _T('A') && HexChar <= _T('F'))
	{
		*HexValue = static_cast<UCHAR>(HexChar - _T('A') + 0x0A);
		return TRUE;
	}
	else if (HexChar >= _T('a') && HexChar <= _T('f'))
	{
		*HexValue = static_cast<UCHAR>(HexChar - _T('a') + 0x0A);
		return TRUE;
	}
	return FALSE;
}

FORCEINLINE
BOOL
StringToHex(LPCTSTR HexString, PUCHAR HexValue)
{
	UCHAR lower, upper;
	if (!CharToHex(HexString[0], &upper)) return FALSE;
	if (!CharToHex(HexString[1], &lower)) return FALSE;
	*HexValue = static_cast<UCHAR>((upper << 4) | lower);
	return TRUE;
}

BOOL
ParseNdasDeviceId(
	__in LPCTSTR Argument, 
	__out PNDAS_DEVICE_IDENTIFIER NdasDeviceIdentifier)
{
	LPCTSTR p = Argument;

	for (DWORD i = 0; i < 5; ++i)
	{
		if (NULL == *p || NULL == *(p+1)) return FALSE;
		BOOL success = StringToHex(p, &NdasDeviceIdentifier->Identifier[i]);
		if (!success) return FALSE;
		p += 2;
		if (_T(':') == *p || _T('-') == *p) ++p;
	}

	if (NULL == *p || NULL == *(p+1)) return FALSE;
	BOOL success = StringToHex(p, &NdasDeviceIdentifier->Identifier[5]);
	if (!success) return FALSE;
	p += 2;

	if (_T(':') == *p || _T('-') == *p)
	{
		++p;
		success = CharToHex(*p, &NdasDeviceIdentifier->UnitNumber);
		if (!success)
		{
			return FALSE;
		}
		++p;
	}
	else
	{
		NdasDeviceIdentifier->UnitNumber = 0;
	}

	if (NULL != *p)
	{
		// redundant characters
		return FALSE;
	}

	return TRUE;
}

static const struct {
	LPCTSTR TypeStringList; /* doubly null terminated string */
	LURN_TYPE LurnType;
} LurnTypeStrings[] = {
	_T("disk\0"), LURN_IDE_DISK,
	_T("odd\0cdrom\0"), LURN_IDE_ODD,
	_T("mo\0"), LURN_IDE_MO,
	_T("aggr\0aggregation\0"), LURN_AGGREGATION, 
	_T("stripe\0raid0"), LURN_RAID0,
	_T("mirror\0raid1\0"), LURN_RAID1R,
	_T("raid5\0"), LURN_RAID5,
};

static
LURN_TYPE
ConvertStringToType(LPCTSTR TypeString)
{
	for (int i = 0; i < RTL_NUMBER_OF(LurnTypeStrings); ++i)
	{
		for (LPCTSTR p = LurnTypeStrings[i].TypeStringList; *p; p += lstrlen(p) + 1)
		{
			if (0 == lstrcmpi(p, TypeString))
			{
				return LurnTypeStrings[i].LurnType;
			}
		}
	}

	return LURN_NULL;
}

static
BOOL
GetDeviceIdentifierAndVersion(
	__in int			argc, 
	__in TCHAR**		argv,
	__out PNDAS_DEVICE_IDENTIFIER NdasDeviceIndentify,
	__out int *		HardwareVersion,
	__out int *		HardwareRevision

){
	if(argc < 3) {
		_tprintf(_T("Error: NDAS Device Identifier is invalid.\n\n"));
		return FALSE;
	}

	// LU Device ID
	BOOL success = ParseNdasDeviceId(argv[0], NdasDeviceIndentify);

	if (!success)
	{
		_tprintf(_T("Error: NDAS Device Identifier is invalid.\n\n"));
		usage();
		return FALSE;
	}

	// Hardware version and revision

	if( StrToIntEx(argv[1], STIF_DEFAULT, HardwareVersion) == FALSE) {
		_tprintf(_T("Error: Hardware version is invalid or not specified.\n\n"));
		usage();
		return FALSE;
	}

	if( StrToIntEx(argv[2], STIF_DEFAULT, HardwareRevision) == FALSE) {
		_tprintf(_T("Error: Hardware revision is invalid or not specified.\n\n"));
		usage();
		return FALSE;
	}
#ifdef _DEBUG
	_tprintf(_T("%02X-%02X-%02X-%02X-%02X-%02X %d HwVer %d HwRev %d\n"),
		NdasDeviceIndentify->Identifier[0],
		NdasDeviceIndentify->Identifier[1],
		NdasDeviceIndentify->Identifier[2],
		NdasDeviceIndentify->Identifier[3],
		NdasDeviceIndentify->Identifier[4],
		NdasDeviceIndentify->Identifier[5],
		NdasDeviceIndentify->UnitNumber,
		*HardwareVersion,
		*HardwareRevision
		);
#endif
	return TRUE;
}

static
BOOL
InitializeLurNodes(
	__in int argc, 
	__in TCHAR** argv,
	__in __out PNDAS_DLU_DESCRIPTOR	NdasDluDesc,
	__in BOOL					RootNodeExists
){
	BOOL bret;
	NDASPORTCTL_NODE_INITDATA nodeInit;
	INT	hardwareVersion, hardwareRevision;
	ULONG	memberCnt;
	PLURELATION_NODE_DESC	curNode;
	UINT64					eachNodeEndAddress;
	PLURELATION_DESC	lurDesc;
	ULONG				nodeCountToBeInit;
	ULONG				lurNodeBaseIndex;
	PLURELATION_NODE_DESC	rootNodeDesc;

	lurDesc = &NdasDluDesc->LurDesc;
	rootNodeDesc = lurDesc->LurnDesc;
	if(RootNodeExists) {
		eachNodeEndAddress = (rootNodeDesc->EndBlockAddr + 1) / rootNodeDesc->LurnChildrenCnt - 1;
		nodeCountToBeInit = rootNodeDesc->LurnChildrenCnt;
		lurNodeBaseIndex = 1;
	} else {
		eachNodeEndAddress = lurDesc->EndingBlockAddr;
		nodeCountToBeInit = 1;
		lurNodeBaseIndex = 0;
	}
	_tprintf(_T("Info: Each member disk length is %I64d blocks.\n"), eachNodeEndAddress);
	memberCnt = 0;
	for(;;) {
		if(argc < 3) {
			_tprintf(_T("Error: Member disk information is invalid.\n\n"));
			return FALSE;
		}

		//
		// Check the number of members so far.
		//

		if(memberCnt >= nodeCountToBeInit) {
			_tprintf(_T("Error: Members are too many.\n\n"));
			return FALSE;
		}

		//
		// Set up node init data.
		//
		nodeInit.NodeSpecificData.Ata.ValidFieldMask = 0;
		// TODO: Support more device types.
		nodeInit.NodeType = LURN_IDE_DISK;
		nodeInit.StartLogicalBlockAddress.QuadPart = 0;
		nodeInit.EndLogicalBlockAddress.QuadPart = eachNodeEndAddress;

		bret = GetDeviceIdentifierAndVersion(
			argc,
			argv,
			&nodeInit.NodeSpecificData.Ata.DeviceIdentifier,
			&hardwareVersion,
			&hardwareRevision);
		if(bret == FALSE)
			return FALSE;
		nodeInit.NodeSpecificData.Ata.HardwareVersion = (UCHAR)hardwareVersion;
		nodeInit.NodeSpecificData.Ata.HardwareRevision = (UCHAR)hardwareRevision;

		//
		// Find the current child node.
		//

		curNode = NdasPortCtlFindNodeDesc(
					(PNDAS_LOGICALUNIT_DESCRIPTOR)NdasDluDesc,
					lurNodeBaseIndex + memberCnt);
		if(curNode == NULL) {
			_tprintf(_T("Internal error: could not find a node.\n\n"));
			return FALSE;
		}

		//
		// Initialize the current child node.
		//

		bret = NdasPortCtlSetupLurNode(curNode, lurDesc->DeviceMode, &nodeInit);
		if(bret == FALSE) {
			_tprintf(_T("Error: could not initialize a node.\n\n"));
			return FALSE;
		}

		memberCnt++;

		//
		// Move to the next arguments
		//
		argc -= 3;
		argv += 3;
		if(argc == 0)
			break;
	}

	if(memberCnt < nodeCountToBeInit) {
		_tprintf(_T("Error: Members are too few.\n\n"));
		return FALSE;
	}

	return TRUE;
}

// Command:
// [disk | aggr | stripe | mirror | odd | mo] <ro|rw> [ending address] [number of member disks]
static
PNDAS_LOGICALUNIT_DESCRIPTOR
ProcessPlugIn_Dlu(
	NDAS_LOGICALUNIT_ADDRESS Address,
	int argc, 
	TCHAR** argv
){
	PNDAS_LOGICALUNIT_DESCRIPTOR logicalUnitDescriptor = NULL;
	INT	memberCnt;
	LARGE_INTEGER	endingAddress;
	NDASPORTCTL_NODE_INITDATA nodeInit;
	PNDASPORTCTL_NODE_INITDATA rootNodeInit = NULL;
	LARGE_INTEGER logicalBlockAddress;
	LURN_TYPE		lurnType;
	BOOL			bret;

	//
	// Reserve 2MB from the end
	//
	logicalBlockAddress.QuadPart = -2 * 1024 * 1024 / 512;

	if(argc < 1) {
		usage();
		return NULL;
	}

	// Device type
	lurnType = ConvertStringToType(argv[0]);
	if(lurnType == LURN_NULL) {
		_tprintf(_T("Error: NDAS Device type is invalid.\n\n"));
		usage();
		return NULL;
	}

	// Access mode
	NDAS_DEV_ACCESSMODE ndasDevAccessMode;
	if (0 == lstrcmpi(_T("ro"), argv[1]))
	{
		ndasDevAccessMode = DEVMODE_SHARED_READONLY;
	}
	else if (0 == lstrcmpi(_T("rw"), argv[1]))
	{
		ndasDevAccessMode = DEVMODE_SHARED_READWRITE;
	}
	else
	{
		_tprintf(_T("Error: Access mode is invalid or not specified.\n\n"));
		return NULL;
	}

	// Disk ending address
	endingAddress.QuadPart = 0;
	if( StrToIntEx(argv[2], STIF_DEFAULT, (INT *)&endingAddress.LowPart) == FALSE) {
		_tprintf(_T("Error: Ending address is invalid or not specified.\n\n"));
		usage();
		return NULL;
	}

	// Member count
	memberCnt = 0;
	if( StrToIntEx(argv[3], STIF_DEFAULT, &memberCnt) == FALSE) {
		_tprintf(_T("Error: Member count is invalid or not specified.\n\n"));
		usage();
		return NULL;
	}
	if(memberCnt == 0) {
		_tprintf(_T("Error: Member count is zero.\n\n"));
		return NULL;
	}

#ifdef _DEBUG
	_tprintf(_T("(%d,%d,%d) %d Access %x Ending addr %I64u Member count %d\n"),
		Address.PathId,
		Address.TargetId,
		Address.Lun,
		lurnType,
		ndasDevAccessMode,
		endingAddress.QuadPart,
		memberCnt
		);
#endif
	//
	// Create the descriptor including a root node.
	//
	switch(lurnType) {
		case LURN_IDE_DISK:
			// We will initialize this root node later. Set NULL.
			rootNodeInit = NULL;
			memberCnt = 0;
			break;
		case LURN_IDE_ODD:
			// We will initialize this root node later. Set NULL.
			rootNodeInit = NULL;
			memberCnt = 0;
			break;
		case LURN_IDE_MO:
			// We will initialize this root node later. Set NULL.
			rootNodeInit = NULL;
			memberCnt = 0;
			break;
		case LURN_AGGREGATION:
		case LURN_RAID0:

			bret = NdasPortCtlGetRaidEndAddress(
				 lurnType, endingAddress.QuadPart, memberCnt, &nodeInit.EndLogicalBlockAddress.QuadPart);
			 if(bret == FALSE) {
				 _tprintf(_T("Could not retreive RAID's end address.\n\n"));
				 return NULL;
			 }

			 nodeInit.NodeType = lurnType;
			 nodeInit.StartLogicalBlockAddress.QuadPart = 0;
			// RAID information is not needed.
			nodeInit.NodeSpecificData.Raid.BlocksPerBit = 0;
			nodeInit.NodeSpecificData.Raid.SpareDiskCount = 0;
			ZeroMemory(&nodeInit.NodeSpecificData.Raid.NdasRaidId, sizeof(GUID));
			ZeroMemory(&nodeInit.NodeSpecificData.Raid.ConfigSetId, sizeof(GUID));

			rootNodeInit = &nodeInit;
			break;
		case LURN_RAID1R:
		case LURN_RAID4R:
			_tprintf(_T("Error: RAID1/4 not supported by this command line tool.\n"));
			return NULL;
		default:
			_tprintf(_T("Error: Invalid type!\n"));
			return NULL;
	}
	logicalUnitDescriptor = NdasPortCtlBuildNdasDluDeviceDescriptor(
		Address,
		0,
		ndasDevAccessMode,
		0,
		&endingAddress,
		memberCnt, // Member count does not include the root node.
		0,
		0,
		rootNodeInit);

	if(NULL == logicalUnitDescriptor){
		_tprintf(_T("Error: Out of memory!\n"));
		return NULL;
	}

	//
	// Initialize child nodes for RAID
	//

	PNDAS_DLU_DESCRIPTOR	dluDesc = (PNDAS_DLU_DESCRIPTOR)logicalUnitDescriptor;

	bret = InitializeLurNodes(
		argc - 4,
		argv + 4,
		dluDesc,
		rootNodeInit?TRUE:FALSE // Root node
		);
	if(bret == FALSE) {
		HeapFree(GetProcessHeap(), 0, logicalUnitDescriptor);
		return NULL;
	}

	return logicalUnitDescriptor;
}

BOOL 
ProcessPlugIn(
	NDAS_LOGICALUNIT_ADDRESS Address,
	int argc, 
	TCHAR** argv)
{
	HRESULT hr;
	
	if (argc < 1)
	{
		usage();
		return FALSE;
	}

	PNDAS_LOGICALUNIT_DESCRIPTOR logicalUnitDescriptor = NULL;

	if (0 == lstrcmpi(_T("filedisk"), argv[0]))
	{
		if (argc != 4 )
		{
			usage();
			return FALSE;
		}

		LPCTSTR filePath = argv[1];
		// const WCHAR FileNamePrefix[] = L"\\\\?\\";
		const WCHAR FileNamePrefix[] = L"\\??\\";

#ifdef UNICODE

		DWORD fileFullPathLength = GetFullPathName(filePath, 0, NULL, NULL);
		if (0 == fileFullPathLength)
		{
			_tprintf(_T("Error: GetFullPathName failed!\n"));
			return FALSE;
		}

		fileFullPathLength += RTL_NUMBER_OF(FileNamePrefix);

		LPWSTR fileFullPathBuffer = static_cast<LPWSTR>(HeapAlloc(
			GetProcessHeap(),
			HEAP_ZERO_MEMORY,
			fileFullPathLength * sizeof(WCHAR)));

		if (!fileFullPathBuffer)
		{
			_tprintf(_T("Error: Out of memory!\n"));
			SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}

		LPWSTR fileFullPath = NULL;

		size_t remainingLength = 0;

		StringCchCopyExW(
			fileFullPathBuffer,
			fileFullPathLength,
			FileNamePrefix,
			&fileFullPath,
			&remainingLength,
			STRSAFE_IGNORE_NULLS);

		GetFullPathName(
			filePath, 
			static_cast<DWORD>(remainingLength), 
			fileFullPath,
			NULL);

#else
		C_ASSERT(FALSE && "not implemented");
#endif
		_tprintf(_T("File Name=%s\n"), fileFullPathBuffer);
		_tprintf(_T("Length=%d bytes\n"), fileFullPathLength * sizeof(WCHAR));

		DWORD descriptorLength = 
			FIELD_OFFSET(FILEDISK_DESCRIPTOR, FilePath) +
			fileFullPathLength * sizeof(WCHAR);

		PFILEDISK_DESCRIPTOR descriptor = 
			static_cast<PFILEDISK_DESCRIPTOR>(
				HeapAlloc(
					GetProcessHeap(),
					HEAP_ZERO_MEMORY,
					descriptorLength));

		if (NULL == descriptor)
		{
			_tprintf(_T("Error: Out of memory!\n"));
			HeapFree(GetProcessHeap(), 0, fileFullPathBuffer);
			SetLastError(ERROR_OUTOFMEMORY);
			return FALSE;
		}

		descriptor->Header.Version = sizeof(NDAS_LOGICALUNIT_DESCRIPTOR);
		descriptor->Header.Size = descriptorLength;
		descriptor->Header.Address = Address;
		descriptor->Header.Type = NdasExternalType;
		descriptor->Header.ExternalTypeGuid = NDASPORT_FILEDISK_TYPE_GUID;

		descriptor->LogicalBlockAddress.QuadPart = 750LL * 1024LL * 1024LL * 1024LL / 512LL;
		descriptor->BytesPerBlock = 512;
		descriptor->FileDiskFlags = FILEDISK_FLAG_USE_SPARSE_FILE;

		CopyMemory(
			descriptor->FilePath,
			fileFullPathBuffer,
			fileFullPathLength * sizeof(WCHAR));

		HeapFree(GetProcessHeap(), 0, fileFullPathBuffer);

		logicalUnitDescriptor = 
			reinterpret_cast<PNDAS_LOGICALUNIT_DESCRIPTOR>(
				static_cast<PFILEDISK_DESCRIPTOR>(descriptor));
	}
	else if (0 == lstrcmpi(_T("ramdisk"), argv[0]))
	{
		return TRUE;
	}
	else if (0 == lstrcmpi(_T("ndasdlu"), argv[0]))
	{
		logicalUnitDescriptor = ProcessPlugIn_Dlu(Address, argc - 1, argv + 1);

		if (NULL == logicalUnitDescriptor)
		{
			return FALSE;
		}
	}
	else
	{
		//
		// Reserve 2MB from the end
		//
		LARGE_INTEGER logicalBlockAddress;
		logicalBlockAddress.QuadPart = -2 * 1024 * 1024 / 512;

		NDAS_DEVICE_IDENTIFIER ndasDeviceIdentifier;
		BOOL success = ParseNdasDeviceId(argv[0], &ndasDeviceIdentifier);

		if (!success)
		{
			_tprintf(_T("Error: NDAS Device Identifier is invalid.\n\n"));
			usage();
			return FALSE;
		}

		ACCESS_MASK accessMode;
		if (0 == lstrcmpi(_T("ro"), argv[1]))
		{
			accessMode = GENERIC_READ;
		}
		else if (0 == lstrcmpi(_T("rw"), argv[1]))
		{
			accessMode = GENERIC_READ | GENERIC_WRITE;
		}
		else
		{
			_tprintf(_T("Error: Access mode is invalid or not specified.\n\n"));
			usage();
			return FALSE;
		}

		if (argc > 2 && 0 == lstrcmpi(_T("noreserve"), argv[2]))
		{
			logicalBlockAddress.QuadPart = 0;
			_tprintf(_T("No reserve mode\n"));
		}

#ifdef _DEBUG
		_tprintf(_T("(%d,%d,%d) %02X-%02X-%02X-%02X-%02X-%02X %d\n"),
				 Address.PathId,
				 Address.TargetId,
				 Address.Lun,			
				 ndasDeviceIdentifier.Identifier[0],
				 ndasDeviceIdentifier.Identifier[1],
				 ndasDeviceIdentifier.Identifier[2],
				 ndasDeviceIdentifier.Identifier[3],
				 ndasDeviceIdentifier.Identifier[4],
				 ndasDeviceIdentifier.Identifier[5],
				 ndasDeviceIdentifier.UnitNumber);
#endif

		hr = NdasPortCtlBuildNdasAtaDeviceDescriptor(
			Address,
			0,
			&ndasDeviceIdentifier,
			0, 
			0,
			accessMode,
			0,
			&logicalBlockAddress,
			&logicalUnitDescriptor);

		if (FAILED(hr))
		{
			_tprintf(_T("Error: Descriptor build failed, hr=0x%X\n"), hr);
			return FALSE;
		}
	}
	
	_ASSERT(NULL != logicalUnitDescriptor);

	XTL::AutoProcessHeap logicalUnitDescriptorPtr = logicalUnitDescriptor;

	XTL::AutoFileHandle handle;

	hr = NdasPortCtlCreateControlDevice(GENERIC_READ | GENERIC_WRITE, &handle);

	if (FAILED(hr))
	{
		ErrorHolder lastError(hr);
		
		_tprintf(_T("Opening NDAS Port device file failed.\n"));
		_tprintf(_T("Error %u (0x%X): %hs\n"), 
			lastError.GetCode(), 
			lastError.GetCode(), 
			lastError.GetDescriptionA());

		return FALSE;
	}

	hr = NdasPortCtlGetPortNumber(
		handle, 
		&logicalUnitDescriptor->Address.PortNumber);

	if (FAILED(hr))
	{
		ErrorHolder lastError(hr);

		_tprintf(_T("Getting the port number failed.\n"));
		_tprintf(_T("Error %u (0x%X): %hs\n"), 
			lastError.GetCode(), 
			lastError.GetCode(), 
			lastError.GetDescriptionA());
		
		return FALSE;
	}

	hr = NdasPortCtlPlugInLogicalUnit(
		handle,
		logicalUnitDescriptor);

	if (FAILED(hr))
	{
		ErrorHolder lastError(hr);

		_tprintf(_T("PlugIn failed.\n"));
		_tprintf(_T("Error %u (0x%X): %hs\n"), 
			lastError.GetCode(), 
			lastError.GetCode(), 
			lastError.GetDescriptionA());
		return FALSE;
	}

	return TRUE;
}

typedef enum _SUBCOMMANDS {
	NDASPORTCMD_PLUGIN,
	NDASPORTCMD_EJECT,
	NDASPORTCMD_UNPLUG,
	NDASPORTCMD_ENUM,
	NDASPORTCMD_GET_FULL_INFORMATION
} SUBCOMMANDS;

static const struct {
	LPCTSTR SubCommandNameList;
	SUBCOMMANDS SubCommand;
} SubCommands[] = {
	_T("-p\0plugin\0"), NDASPORTCMD_PLUGIN,
	_T("-e\0eject\0"), NDASPORTCMD_EJECT,
	_T("-u\0unplug\0"), NDASPORTCMD_UNPLUG,
	_T("enum\0"), NDASPORTCMD_ENUM,
	_T("fullinfo\0"), NDASPORTCMD_GET_FULL_INFORMATION,
};

int Run(int argc, TCHAR** argv)
{
	typedef enum _COMMAND_TYPE {
		CmdUnknown,
		CmdPlugIn,
		CmdEject,
		CmdUnplug
	} COMMAND_TYPE;

	if (argc < 3)
	{
		usage();
		return 1;
	}

	COMMAND_TYPE commandType = CmdUnknown;

	if (0 == lstrcmpi(argv[1], _T("-p")))
	{
		commandType = CmdPlugIn;
	}
	else if (0 == lstrcmpi(argv[1], _T("-e")))
	{
		commandType = CmdEject;
	}
	else if (0 == lstrcmpi(argv[1], _T("-u")))
	{
		commandType = CmdUnplug;
	}
	else
	{
		usage();
		return 1;
	}

	NDAS_LOGICALUNIT_ADDRESS address = {0};

	int value;
	BOOL success = StrToIntEx(argv[2], STIF_SUPPORT_HEX, &value);
	if (!success)
	{
		_tprintf(_T("Logical unit address is invalid.\n"));
		return 1;
	}

	//
	// Only TargetId can be used.
	//
	address.TargetId = static_cast<UCHAR>(value);
	address.PathId = 0;
	address.Lun = 0;

	if (CmdPlugIn == commandType)
	{
		success = ProcessPlugIn(address, argc - 3, argv + 3);
		return success ? 0 : 1;
	}

	XTL::AutoFileHandle handle;

	HRESULT hr = NdasPortCtlCreateControlDevice(
		GENERIC_READ | GENERIC_WRITE, &handle);
	
	if (FAILED(hr))
	{
		ErrorHolder lastError(hr);

		_tprintf(_T("Opening NDAS Port device file failed.\n"));
		_tprintf(_T("Error %u (0x%X): %hs\n"), 
			lastError.GetCode(),
			lastError.GetCode(), 
			lastError.GetDescriptionA());

		return 1;
	}

	hr = NdasPortCtlGetPortNumber(handle, &address.PortNumber);
	if (FAILED(hr))
	{
		ErrorHolder lastError(hr);

		_tprintf(_T("Getting the port number failed.\n"));
		_tprintf(_T("Error %u (0x%X): %hs\n"), 
			lastError.GetCode(),
			lastError.GetCode(), 
			lastError.GetDescriptionA());

		return 1;
	}

	success = TRUE;

	switch (commandType)
	{
	case CmdEject:

		hr = NdasPortCtlEjectLogicalUnit(handle, address, 0);
		if (FAILED(hr))
		{
			ErrorHolder lastError(hr);

			_tprintf(_T("Eject failed.\n"));
			_tprintf(_T("Error %u (0x%X): %hs\n"), 
				lastError.GetCode(),
				lastError.GetCode(), 
				lastError.GetDescriptionA());
		}
		break;

	case CmdUnplug:

		hr = NdasPortCtlUnplugLogicalUnit(handle, address, 0);
		if (FAILED(hr))
		{
			ErrorHolder lastError(hr);

			_tprintf(_T("Unplug failed.\n"));
			_tprintf(_T("Error %u (0x%X): %hs\n"), 
				lastError.GetCode(),
				lastError.GetCode(), 
				lastError.GetDescriptionA());
		}
		break;
	}


	return success ? 0 : 1;
}

int __cdecl _tmain(int argc, TCHAR** argv)
{
	WPP_INIT_TRACING(_T("ndasportcmd"));

	int ret = Run(argc, argv);

	WPP_CLEANUP();

	return ret;
}
