#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <strsafe.h>
#include "ndasbusctl.h"

#define XDBG_MAIN_MODULE
#include "xdebug.h"

void usage()
{
	_tprintf(TEXT("lsbuscmd [version | mpversion | slotlist | fdoinfo | status | pdoevent | pdofile] slotno\n"));
}

int __cdecl _tmain(int argc, LPTSTR* argv)
{
	BOOL	fSuccess(FALSE);
	ULONG	SlotNo;
	BOOL	bret;

	if(argc < 2) {
		usage();
		return 1;
	}

	if (lstrcmpi(argv[1],TEXT("version")) == 0) {
		WORD VersionMajor;
		WORD VersionMinor;
		WORD VersionBuild;
		WORD VersionPrivate;

		bret = LsBusCtlGetVersion(
					&VersionMajor,
					&VersionMinor,
					&VersionBuild,
					&VersionPrivate
				);
		if(bret == FALSE) {
			_tprintf(TEXT("LanscsiBus control failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("- LanscsiBus version\n"));
			_tprintf(	TEXT("Major   : %u\n")
						TEXT("Minor   : %u\n")
						TEXT("Build   : %u\n")
						TEXT("Private : %u\n"),
						VersionMajor, VersionMinor, VersionBuild, VersionPrivate);
		}

	} else if(lstrcmpi(argv[1],TEXT("mpversion")) == 0) {
		WORD VersionMajor;
		WORD VersionMinor;
		WORD VersionBuild;
		WORD VersionPrivate;

		if(argc < 3) {
			usage();
			return 1;
		}

		SlotNo = _tstoi(argv[2]);
		bret = LsBusCtlGetMiniportVersion(
					SlotNo,
					&VersionMajor,
					&VersionMinor,
					&VersionBuild,
					&VersionPrivate
			);
		if(bret == FALSE) {
			_tprintf(TEXT("NDASSCSI control failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("- NDASSCSI version\n"));
			_tprintf(	TEXT("Major   : %u\n")
				TEXT("Minor   : %u\n")
				TEXT("Build   : %u\n")
				TEXT("Private : %u\n"),
				VersionMajor, VersionMinor, VersionBuild, VersionPrivate);
		}

	} else if(lstrcmpi(argv[1],TEXT("slotlist")) == 0) {
		PBUSENUM_INFORMATION	busInfo;

		bret = LsBusCtlQueryPdoSlotList(&busInfo);
		if(bret == FALSE) {
			_tprintf(TEXT("Querying slot list failed. LastError:%lu\n"), GetLastError());
		} else {
			ULONG idx_slot;
			_tprintf(TEXT("Slot list:"));
			for(idx_slot = 0; idx_slot < busInfo->PdoSlotList.SlotNoCnt; idx_slot++ ) {
				_tprintf(TEXT(" %lu"), busInfo->PdoSlotList.SlotNo[idx_slot]);
			}
			_tprintf(TEXT("\n"));
			HeapFree(GetProcessHeap(), 0, busInfo);
		}

	} else if(lstrcmpi(argv[1],TEXT("pdoevent")) == 0) {
		HANDLE	alarm;
		HANDLE	discon;

		if(argc < 3) {
			usage();
			return 1;
		}

		SlotNo = _tstoi(argv[2]);
		bret = LsBusCtlQueryPdoEvent(SlotNo, &alarm, &discon);
		if(bret == FALSE) {
			_tprintf(TEXT("Querying pdo events failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(	TEXT("Alarm event        : %p\n")
						TEXT("Disconnection event: %p\n"),
						alarm, discon
				);
			bret = CloseHandle(alarm);
			if(bret == FALSE) {
				_tprintf(TEXT("Closing alarm event failed. LastError:%lu\n"), GetLastError());
			}
			bret = CloseHandle(discon);
			if(bret == FALSE) {
				_tprintf(TEXT("Closing disconnection event failed. LastError:%lu\n"), GetLastError());
			}
		}

	} else if(lstrcmpi(argv[1],TEXT("status")) == 0) {
	  ULONG ulStatus;
		if(argc < 3) {
			usage();
			return 1;
		}

		SlotNo = _tstoi(argv[2]);
		bret = LsBusCtlQueryStatus(SlotNo, &ulStatus);

		if(bret == FALSE) {
			_tprintf(TEXT("Querying LUR full information  failed. LastError:%lu\n"), GetLastError());
		} else {
		  _tprintf(TEXT("ADAPTERINFO_STATUS : %s\n"),
  		  ADAPTERINFO_ISSTATUS(ulStatus, NDASSCSI_ADAPTERINFO_STATUS_INIT) ? TEXT("NDASSCSI_ADAPTERINFO_STATUS_INIT") :
  		  ADAPTERINFO_ISSTATUS(ulStatus, NDASSCSI_ADAPTERINFO_STATUS_RUNNING) ? TEXT("NDASSCSI_ADAPTERINFO_STATUS_RUNNING") :
  		  ADAPTERINFO_ISSTATUS(ulStatus, NDASSCSI_ADAPTERINFO_STATUS_STOPPING) ? TEXT("NDASSCSI_ADAPTERINFO_STATUS_STOPPING") :
  		  ADAPTERINFO_ISSTATUS(ulStatus, NDASSCSI_ADAPTERINFO_STATUS_IN_ERROR) ? TEXT("NDASSCSI_ADAPTERINFO_STATUS_IN_ERROR") :
  		  ADAPTERINFO_ISSTATUS(ulStatus, NDASSCSI_ADAPTERINFO_STATUS_STOPPED) ? TEXT("NDASSCSI_ADAPTERINFO_STATUS_STOPPED") :
  		  TEXT("Unknown status"));

#define PRINT_FLAG(STATUS, FLAG) \
  		_tprintf(TEXT("") TEXT(#FLAG) TEXT(": %s\n"), ADAPTERINFO_ISSTATUSFLAG((STATUS), (FLAG)) ? TEXT("ON") : TEXT("off"));
  		PRINT_FLAG(ulStatus, NDASSCSI_ADAPTERINFO_STATUSFLAG_RECONNECT_PENDING);
  		PRINT_FLAG(ulStatus, ADAPTERINFO_STATUSFLAG_POWERSAVING_PENDING);
  		PRINT_FLAG(ulStatus, ADAPTERINFO_STATUSFLAG_BUSRESET_PENDING);
  		PRINT_FLAG(ulStatus, NDASSCSI_ADAPTERINFO_STATUSFLAG_MEMBER_FAULT);
#undef PRINT_FLAG
		}


	} else if(lstrcmpi(argv[1],TEXT("fdoinfo")) == 0) {
		PNDSCIOCTL_ADAPTERLURINFO	lurFullInfo;

		if(argc < 3) {
			usage();
			return 1;
		}

		SlotNo = _tstoi(argv[2]);
		bret = LsBusCtlQueryMiniportFullInformation(SlotNo, &lurFullInfo);
		if(bret == FALSE) {
			_tprintf(TEXT("Querying LUR full information  failed. LastError:%lu\n"), GetLastError());
		} else {
			ULONG				idx_ud;
			PNDSC_LURN_FULL		unitDisk;

			_tprintf(TEXT("Structure length                     :%u\n"), lurFullInfo->Length);
			_tprintf(TEXT("Adapter.SlotNo                       :%u\n"), lurFullInfo->Adapter.SlotNo);
			_tprintf(TEXT("Adapter.Length                       :%u\n"), lurFullInfo->Adapter.Length);
			_tprintf(TEXT("Adapter.InitiatorId                  :%u\n"), (int)lurFullInfo->Adapter.InitiatorId);
			_tprintf(TEXT("Adapter.NumberOfBuses                :%u\n"), (int)lurFullInfo->Adapter.NumberOfBuses);
			_tprintf(TEXT("Adapter.MaximumNumberOfTargets       :%u\n"), (int)lurFullInfo->Adapter.MaximumNumberOfTargets);
			_tprintf(TEXT("Adapter.MaximumNumberOfLogicalUnits  :%u\n"), (int)lurFullInfo->Adapter.MaximumNumberOfLogicalUnits);
			_tprintf(TEXT("Adapter.MaxBlocksPerRequest          :%u\n"), lurFullInfo->Adapter.MaxBlocksPerRequest);
			_tprintf(TEXT("Adapter.Status                       :%08lx\n"), lurFullInfo->Adapter.Status);
			_tprintf(TEXT("Lur.Length                           :%u\n"), lurFullInfo->Lur.Length);
			_tprintf(TEXT("Lur.DevType                          :%08lx\n"), lurFullInfo->Lur.DevType);
			_tprintf(TEXT("Lur.TargetId                         :%u\n"), lurFullInfo->Lur.TargetId);
			_tprintf(TEXT("Lur.Lun                              :%u\n"), lurFullInfo->Lur.Lun);
			_tprintf(TEXT("Lur.DesiredAccess                    :%08lx\n"), lurFullInfo->Lur.DesiredAccess);
			_tprintf(TEXT("Lur.GrantedAccess                    :%08lx\n"), lurFullInfo->Lur.GrantedAccess);
			_tprintf(TEXT("Lur.LurnCnt                          :%u\n"), lurFullInfo->Lur.LurnCnt);
			_tprintf(TEXT("EnableTime                           :%I64u\n"), lurFullInfo->EnabledTime.QuadPart);
			_tprintf(TEXT("UnitDiskCnt                          :%u\n"), lurFullInfo->UnitDiskCnt);
			for(idx_ud = 0; idx_ud < lurFullInfo->UnitDiskCnt; idx_ud++) {
				unitDisk = lurFullInfo->UnitDisks + idx_ud;
				_tprintf(TEXT("- LURN #%u   :\n"), idx_ud);
				_tprintf(TEXT("Length       :%u\n"), unitDisk->Length);
				_tprintf(TEXT("LurnId       :%u\n"), unitDisk->LurnId);
				_tprintf(TEXT("LurnType     :%u\n"), unitDisk->LurnType);

				_tprintf(TEXT("NetDiskAddress.TAAddressCount            :%u\n"), unitDisk->NetDiskAddress.TAAddressCount);
				_tprintf(TEXT("NetDiskAddress.Address[0].AddressType    :%x\n"), unitDisk->NetDiskAddress.Address[0].AddressType);
				_tprintf(TEXT("NetDiskAddress.Address[0].AddressLength  :%x\n"), unitDisk->NetDiskAddress.Address[0].AddressLength);
				_tprintf(TEXT("NetDiskAddress.Address[0].Address        :%02X %02X | %02X %02X %02X %02X %02X %02X\n"),
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[0],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[1],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[2],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[3],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[4],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[5],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[6],
													(int)unitDisk->NetDiskAddress.Address[0].Address.Address[7]
													);

				_tprintf(TEXT("BindingAddress.TAAddressCount            :%u\n"), unitDisk->BindingAddress.TAAddressCount);
				_tprintf(TEXT("BindingAddress.Address[0].AddressType    :%x\n"), unitDisk->BindingAddress.Address[0].AddressType);
				_tprintf(TEXT("BindingAddress.Address[0].AddressLength  :%x\n"), unitDisk->BindingAddress.Address[0].AddressLength);
				_tprintf(TEXT("BindingAddress.Address[0].Address        :%02X %02X | %02X %02X %02X %02X %02X %02X\n"),
													(int)unitDisk->BindingAddress.Address[0].Address.Address[0],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[1],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[2],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[3],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[4],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[5],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[6],
													(int)unitDisk->BindingAddress.Address[0].Address.Address[7]
													);
				_tprintf(TEXT("UnitDiskId   :%u\n"), (int)unitDisk->UnitDiskId);
				_tprintf(TEXT("UserID       :%02x %02x %02x %02x\n"),
															(int)unitDisk->UserID[0],
															(int)unitDisk->UserID[1],
															(int)unitDisk->UserID[2],
															(int)unitDisk->UserID[3]
															);
				_tprintf(TEXT("Password     :%02x %02x %02x %02x %02x %02x\n"),
															(int)unitDisk->Password[0],
															(int)unitDisk->Password[1],
															(int)unitDisk->Password[2],
															(int)unitDisk->Password[3],
															(int)unitDisk->Password[4],
															(int)unitDisk->Password[5]
															);
				_tprintf(TEXT("AccessRight  :%08lx\n"), unitDisk->AccessRight);
				_tprintf(TEXT("UnitBlocks   :%u\n"), unitDisk->UnitBlocks);
				_tprintf(TEXT("StatusFlags  :%u\n"), unitDisk->StatusFlags);
			}
			_tprintf(TEXT("\n"));
			HeapFree(GetProcessHeap(), 0, lurFullInfo);
		}

	}  else if(lstrcmpi(argv[1],TEXT("pdofile")) == 0) {
		HANDLE	pdoFileHandle;

		SlotNo = _tstoi(argv[2]);
		bret = LsBusCtlQueryPdoFileHandle(SlotNo, &pdoFileHandle);
		if(bret == FALSE) {
			_tprintf(TEXT("Querying PDO file handle  failed. LastError:%lu\n"), GetLastError());
		} else {
			_tprintf(TEXT("PDO file handle  :%p\n"), pdoFileHandle);
			ZwClose(pdoFileHandle);
		}
	} else {
		usage();
	}


	return fSuccess ? 0 : 1;
}
