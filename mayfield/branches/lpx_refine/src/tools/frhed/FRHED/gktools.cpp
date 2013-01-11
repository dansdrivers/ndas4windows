#include "precomp.h"
#include <imagehlp.h>
#include <ctype.h>
#include "gktools.h"
#include "resource.h"
#include "simparr.h"
#include "Physicaldrive.h"
#include "version.h"

//extern IPhysicalDrive* Drive;
extern IPhysicalDrive* Drive;
extern IPhysicalDrive* PhysicalDrive;
extern IPhysicalDrive* NDASDrive;
extern INT64 CurrentSectorNumber;
NDASCOMM_CONNECTION_INFO NdasConnectionInfo;
PartitionInfo* SelectedPartitionInfo = NULL;
static PList PartitionInfoList;

typedef BOOL (__stdcall* LPFNUnMapAndLoad)( PLOADED_IMAGE LoadedImage );
typedef PVOID (__stdcall* LPFNImageRvaToVa)(
	IN PIMAGE_NT_HEADERS NtHeaders,
	IN PVOID Base,
	IN ULONG Rva,
	IN OUT PIMAGE_SECTION_HEADER *LastRvaSection
);

typedef BOOL (__stdcall* LPFNMapAndLoad)(
	PSTR ImageName,
	PSTR DllPath,
	PLOADED_IMAGE LoadedImage,
	BOOL DotDll,
	BOOL ReadOnly
);

LPFNMapAndLoad fMapAndLoad = 0;
LPFNImageRvaToVa fImageRvaToVa = 0;
LPFNUnMapAndLoad fUnMapAndLoad = 0;

BOOL CanUseImagehelpDll()
{
	if( fUnMapAndLoad && fMapAndLoad && fImageRvaToVa )
		return TRUE;

	HMODULE hModule = LoadLibrary( "IMAGEHLP.DLL" );
	if( hModule )
	{
		fMapAndLoad = (LPFNMapAndLoad) GetProcAddress( hModule, "MapAndLoad" );
		fImageRvaToVa = (LPFNImageRvaToVa) GetProcAddress( hModule, "ImageRvaToVa" );
		fUnMapAndLoad = (LPFNUnMapAndLoad) GetProcAddress( hModule, "UnMapAndLoad" );

		return fUnMapAndLoad && fMapAndLoad && fImageRvaToVa;
	}
	return FALSE;
}

#define IRTV(x) fImageRvaToVa( li.FileHeader, li.MappedAddress, (DWORD)x, 0 )

BOOL WINAPI GetDllExportNames( LPCSTR pszFilename, ULONG* lpulOffset, ULONG* lpulSize )
{
	if( !CanUseImagehelpDll() )
		return FALSE;

	LOADED_IMAGE li;
	if( !fMapAndLoad( (LPSTR) pszFilename, NULL, &li, TRUE, TRUE ) )
		return FALSE;

	PIMAGE_EXPORT_DIRECTORY pExpDir = (PIMAGE_EXPORT_DIRECTORY)(li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if( !pExpDir )
		return FALSE;

	pExpDir = (PIMAGE_EXPORT_DIRECTORY)IRTV(pExpDir);
	if( !pExpDir->NumberOfNames )
		return FALSE;

	PDWORD* pExpNames = (LPDWORD*) pExpDir->AddressOfNames;
	pExpNames = (LPDWORD*)IRTV(pExpNames);
	ULONG ulStart = (ULONG) IRTV(*pExpNames);
	*lpulOffset = ulStart - (ULONG) li.MappedAddress;
	pExpNames += pExpDir->NumberOfNames-1;
	ULONG ulStop = (ULONG) IRTV(*pExpNames);
	*lpulSize = ulStop - ulStart + strlen((LPCSTR)ulStop);	// hihi

	fUnMapAndLoad( &li );
	return TRUE;
}

// structures are undocumented
typedef struct
{
	// Addr +0 = start of import declaration
	// Addr +1,+2 are -1 always
	// Addr +3 = virtual name-of-dll
	// Addr +4 = ???
	ULONG Addr[5];
} IMPS0;

BOOL WINAPI GetDllImportNames( LPCSTR pszFilename, ULONG* lpulOffset, ULONG* lpulSize )
{
	if( !CanUseImagehelpDll() )
		return FALSE;

	LOADED_IMAGE li;
	if( !fMapAndLoad( (LPSTR) pszFilename, NULL, &li, TRUE, TRUE ) )
		return FALSE;

	PVOID pExpDir = (LPVOID)(li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	if( !pExpDir )
		return FALSE;

	IMPS0* p = (IMPS0*)IRTV(pExpDir);
	pExpDir = (PVOID)IRTV(p->Addr[0]);
	pExpDir = (PVOID)IRTV(*(ULONG*)pExpDir);
	*lpulOffset = (ULONG)pExpDir - (ULONG) li.MappedAddress;

	LPBYTE lpbEnd = (LPBYTE) pExpDir;
	while(!( !lpbEnd[0] && !lpbEnd[1] && !lpbEnd[2] && !lpbEnd[3] ))
	{
		lpbEnd++;
	}
	*lpulSize = (ULONG)lpbEnd - (ULONG) pExpDir;

	fUnMapAndLoad( &li );
	return TRUE;
}

void WINAPI XorEncoder( MEMORY_CODING* p )
{
	LPBYTE q = p->lpbMemory;
	LPBYTE qMax = q+p->dwSize;
	while(q<qMax)
		*(q++)^=-1;
}

void WINAPI Rot13Encoder( LPMEMORY_CODING p )
{
	LPBYTE q = p->lpbMemory;
	LPBYTE qMax = q+p->dwSize;
	while(q<qMax)
		*(q++)=isalpha(*q)?(BYTE)(tolower(*q)<'n'?*q+13:*q-13):*q;
}

MEMORY_CODING_DESCRIPTION BuiltinEncoders[] =
{
	{ "ROT-13", Rot13Encoder },
	{ "XOR -1", XorEncoder },
	{ 0, 0 }
};

typedef struct
{
	HMODULE hLibrary;
	LPFNGetMemoryCodings Callback;
} ENCODE_DLL;

#define MAX_ENCODE_DLL 32

static ENCODE_DLL EncodeDlls[MAX_ENCODE_DLL];

void AddEncoders(HWND hListbox,LPMEMORY_CODING_DESCRIPTION lpEncoders)
{
	for( ULONG ulIndex = 0; lpEncoders[ulIndex].lpszDescription; ulIndex++ )
	{
		SendMessage(hListbox,LB_SETITEMDATA,
			SendMessage(hListbox,LB_ADDSTRING,0,(LPARAM)lpEncoders[ulIndex].lpszDescription),
			(LPARAM)lpEncoders[ulIndex].fpEncodeFunc);
	}
}

static LPMEMORY_CODING theCoding = 0;

static BOOL CALLBACK EncodeDecodeDialogProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			HWND hListbox = GetDlgItem(hDlg,IDC_LIST1);
			AddEncoders(hListbox,BuiltinEncoders);
			for( ULONG ulIndex = 0; EncodeDlls[ulIndex].Callback; ulIndex++ )
				AddEncoders(hListbox,EncodeDlls[ulIndex].Callback());

			SendMessage(hListbox,LB_SETCURSEL,0,0);
			CheckDlgButton(hDlg,IDC_RADIO1,BST_CHECKED);
		}
		return FALSE;

	case WM_COMMAND:
		if( HIWORD(wParam) == BN_CLICKED )
		{
			if( LOWORD (wParam) == IDOK )
			{
				static CHAR szBuffer[1024];
				GetDlgItemText(hDlg,IDC_EDIT1,szBuffer,sizeof(szBuffer));
				theCoding->bEncode = (IsDlgButtonChecked( hDlg, IDC_RADIO1 ) == BST_CHECKED);
				theCoding->lpszArguments = szBuffer;
				HWND hListbox = GetDlgItem(hDlg,IDC_LIST1);
				int nCurSel = SendMessage(hListbox,LB_GETCURSEL,0,0);
				if( nCurSel>=0)
				{
					theCoding->fpEncodeFunc = (LPFNEncodeMemoryFunction) SendMessage(hListbox,LB_GETITEMDATA,nCurSel,0);
					EndDialog( hDlg, IDOK);
					return TRUE;
				}
			}
			else if( LOWORD (wParam) == IDCANCEL )
			{
				EndDialog(hDlg, IDCANCEL );
				return FALSE;
			}
		}
		break;
	}
	return FALSE;
}

BOOL WINAPI GetMemoryCoding( HINSTANCE hInstance, HWND hParent, LPMEMORY_CODING p, LPCSTR lpszDlls )
{
	theCoding = p;

	static BOOL bDllsLoaded = FALSE;
	if( !bDllsLoaded )
	{
		SimpleString buffer((LPSTR)lpszDlls);
		LPCSTR lpszToken = strtok(buffer,";");
		ULONG ulIndex = 0;
		while( lpszToken )
		{
			if( ulIndex == MAX_ENCODE_DLL )
				break;

			EncodeDlls[ulIndex].hLibrary = LoadLibrary(lpszToken);
			if( EncodeDlls[ulIndex].hLibrary )
			{
				EncodeDlls[ulIndex].Callback = (LPFNGetMemoryCodings) GetProcAddress(EncodeDlls[ulIndex].hLibrary,"GetMemoryCodings");
				if( EncodeDlls[ulIndex].Callback )
				{
					ulIndex++;
				}
				else EncodeDlls[ulIndex].hLibrary = 0;
			}

			lpszToken = strtok(0,";");
		}
	}
	return DialogBox( hInstance, MAKEINTRESOURCE(IDD_ENCODE_DECODE_DIALOG), hParent, (DLGPROC) EncodeDecodeDialogProc ) == IDOK;
}


static BOOL CALLBACK OpenDriveDialogProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			HWND hListbox = GetDlgItem(hDlg,IDC_LIST1);

			SelectedPartitionInfo = NULL;
			if( PartitionInfoList.IsEmpty() )
			{
				PhysicalDrive->GetPartitionInfo(&PartitionInfoList);
			}
			ENUMERATE(&PartitionInfoList, PartitionInfo, pi)
			{
				int iIndex = SendMessage(hListbox,LB_ADDSTRING,0,(LPARAM) (LPCSTR) pi->GetNameAsString());
				SendMessage(hListbox,LB_SETITEMDATA,iIndex,(LPARAM)pi);
			}
			SendMessage(hListbox,LB_SETCURSEL,0,0);
		}
		return FALSE;

	case WM_COMMAND:
		if( HIWORD(wParam) == BN_CLICKED )
		{
			if( LOWORD (wParam) == IDOK )
			{
				HWND hListbox = GetDlgItem(hDlg,IDC_LIST1);
				int nCurSel = SendMessage(hListbox,LB_GETCURSEL,0,0);
				if( nCurSel>=0)
				{
					SelectedPartitionInfo = (PartitionInfo*)SendMessage(hListbox,LB_GETITEMDATA,nCurSel,0);
					EndDialog( hDlg, IDOK );
				}
			}
			else if( LOWORD (wParam) == IDCANCEL )
			{
				EndDialog( hDlg, IDCANCEL );
				return FALSE;
			}
		}
		break;
	}
	return FALSE;
}


BOOL WINAPI GetDriveNameDialog( HINSTANCE hInstance, HWND hParent )
{
	return ( DialogBox( hInstance, MAKEINTRESOURCE(IDD_OPEN_DRIVE_DIALOG), hParent, (DLGPROC) OpenDriveDialogProc ) == IDOK );
}

#define REGISTRY_NDAS_PATH "Software\\frhed\\v"CURRENT_VERSION"." SUB_RELEASE_NO "\\NDAS"
static BOOL AddNDASIDComboBox(HWND hComboID, PCHAR szValueName)
{
	if(!szValueName)
		return FALSE;

	BOOL bReturn = FALSE;
	HKEY hk;
	if(
		ERROR_SUCCESS == RegOpenKeyEx(
								HKEY_CURRENT_USER,
								REGISTRY_NDAS_PATH,
								0,
								KEY_ALL_ACCESS,
								&hk)
		)
	{
		if(ERROR_SUCCESS == RegSetValueEx(hk, szValueName, 0, REG_SZ, (const unsigned char *)szValueName, strlen(szValueName) +1))
			bReturn = TRUE;
		
		RegCloseKey(hk);
	}

	return bReturn;
}

static BOOL DeleteNDASIDComboBox(HWND hComboID, PCHAR szValueName)
{
	if(!szValueName)
		return FALSE;

	BOOL bReturn = FALSE;
	HKEY hk;
	if(
		ERROR_SUCCESS == RegOpenKeyEx(
								HKEY_CURRENT_USER,
								REGISTRY_NDAS_PATH,
								0,
								KEY_ALL_ACCESS,
								&hk)
		)
	{
		if(ERROR_SUCCESS == RegDeleteValue(hk, szValueName))
			bReturn = TRUE;
		
		RegCloseKey(hk);
	}

	return bReturn;
}

static BOOL RefreshNDASIDComboBox(HWND hComboID)
{
	BOOL bReturn = FALSE;
	HKEY hk;

	if(
		ERROR_SUCCESS == RegOpenKeyEx(
								HKEY_CURRENT_USER,
								REGISTRY_NDAS_PATH,
								0,
								KEY_ALL_ACCESS,
								&hk)
		)
	{
		int i = 0;
		CHAR szValueName[100], Data[100];
		DWORD cValueName, cData;
		DWORD type;
		LONG lResults;

		SendMessage(hComboID, CB_RESETCONTENT, NULL, NULL);

		while(1)
		{
			cValueName = sizeof(szValueName);
			cData = sizeof(Data);
			lResults = RegEnumValue(
							hk,
							i,
							szValueName,
							&cValueName,
							NULL,
							&type,
							(unsigned char *)Data,
							&cData);

			if(ERROR_SUCCESS == lResults)
			{
				SendMessage(hComboID, CB_ADDSTRING, NULL, (LPARAM)szValueName);
			}
			else
			{
				break;
			}
			
			i++;
		}
		
		RegCloseKey(hk);
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

#define HEX_INT(CHA) ((CHA >= '0' && CHA <= '9') ? CHA - '0' : tolower(CHA) - 'a' + 10 )
#define STR_TO_MAC(ENUM, NODE, STR, STEP) \
	do{ \
		for((ENUM) = 0; (ENUM) < 6; (ENUM)++) \
			(NODE)[(ENUM)] = (HEX_INT((STR)[(ENUM) * (STEP)]) * 16 + HEX_INT((STR)[(ENUM) * (STEP) +1]));\
	} while(0);


static BOOL CALLBACK OpenAddressDialogProc (HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );
	HWND hEditID = GetDlgItem(hDlg,IDC_EDIT_ID);
	HWND hComboID = GetDlgItem(hDlg,IDC_COMBO_ID);
	HWND hEditUnit = GetDlgItem(hDlg, IDC_EDIT_UNIT);
	HWND hCheckReadOnly = GetDlgItem(hDlg, IDC_CHECK_READONLY);

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			HKEY hk;
			LONG res;
			
			res = RegCreateKeyEx(
				HKEY_CURRENT_USER, 
				REGISTRY_NDAS_PATH, 
				0, 
				NULL, 
				REG_OPTION_NON_VOLATILE, 
				KEY_ALL_ACCESS, 
				NULL, 
				&hk,
				NULL);
			if(ERROR_SUCCESS == res)
				RegCloseKey(hk);

			SendMessage(hEditUnit, WM_SETTEXT, NULL, (LPARAM)"0");
			RefreshNDASIDComboBox(hComboID);
		}
		return FALSE;

	case WM_COMMAND:
		switch(HIWORD(wParam))
		{
		case BN_CLICKED:
			{
				if( LOWORD (wParam) == IDOK )
				{
					ZeroMemory(&NdasConnectionInfo, sizeof(NdasConnectionInfo));
					NdasConnectionInfo.Size = sizeof(NDASCOMM_CONNECTION_INFO);
					NdasConnectionInfo.WriteAccess = !SendMessage(hCheckReadOnly, BM_GETCHECK, 0, 0) == BST_CHECKED;

					int i;
					CHAR buf[255];
					ZeroMemory(buf, sizeof(buf));
//					GetWindowText(hEditID, buf, sizeof(buf));
					GetWindowText(hComboID, buf, sizeof(buf));

//					switch(GetWindowTextLength(hEditID))
					switch(GetWindowTextLength(hComboID))
					{
					case 12:
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_DEVICE_ID;
						STR_TO_MAC(i, NdasConnectionInfo.DeviceId.Node, buf, 2);
						break;
					case 17:
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_DEVICE_ID;
						STR_TO_MAC(i, NdasConnectionInfo.DeviceId.Node, buf, 3);
						break;
					case 20:
						if(NdasConnectionInfo.WriteAccess)
						{
							MessageBox(hDlg, "Missing write key", "Wrong format", MB_ICONERROR);
							return FALSE;
						}
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_IDA;
						CopyMemory(NdasConnectionInfo.NdasIdA.Id, buf, 20);
						break;
					case 23:
						if(NdasConnectionInfo.WriteAccess)
						{
							MessageBox(hDlg, "Missing write key", "Wrong format", MB_ICONERROR);
							return FALSE;
						}
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_IDA;
						CopyMemory(NdasConnectionInfo.NdasIdA.Id, buf, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +5, buf +6, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +10, buf +12, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +15, buf +18, 5);
						break;
					case 25:
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_IDA;
						CopyMemory(NdasConnectionInfo.NdasIdA.Id, buf, 20);
						CopyMemory(NdasConnectionInfo.NdasIdA.Key, buf +20, 5);
						break;
					case 29:
						NdasConnectionInfo.AddressType = NDASCOMM_CIT_NDAS_IDA;
						CopyMemory(NdasConnectionInfo.NdasIdA.Id, buf, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +5, buf +6, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +10, buf +12, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Id +15, buf +18, 5);
						CopyMemory(NdasConnectionInfo.NdasIdA.Key, buf +24, 5);
						break;
					default:
						MessageBox(hDlg, "Form : 00:0B:D0:00:01:02 or\n01234ABCDE567890FGHIJ or\n01234ABCDE567890FGHIJ01234 or\n01234-ABCDE5-67890-FGHIJ or\n01234-ABCDE5-67890-FGHIJ-01234", "Wrong format", MB_ICONERROR);
						return FALSE;							
					}
					NdasConnectionInfo.PrivilegedOEMCode.UI64Value = 0;
					NdasConnectionInfo.OEMCode.UI64Value = 0;
					NdasConnectionInfo.LoginType = NDASCOMM_LOGIN_TYPE_NORMAL;
					NdasConnectionInfo.Protocol = NDASCOMM_TRANSPORT_LPX;

					char szUnitNumber[10];					
					GetWindowText(hEditUnit, szUnitNumber, 10);
					sscanf(szUnitNumber, "%d", &NdasConnectionInfo.UnitNo);
					
					AddNDASIDComboBox(hComboID, buf);
					EndDialog( hDlg, IDOK );
				}
				else if( LOWORD (wParam) == IDCANCEL )
				{
					EndDialog( hDlg, IDCANCEL );
					return FALSE;
				}
				else if( LOWORD (wParam) == ID_BTN_DELETE_NDAS_ADDRESS )
				{
					CHAR buf[255];
					ZeroMemory(buf, sizeof(buf));
					GetWindowText(hComboID, buf, sizeof(buf));

					if(DeleteNDASIDComboBox(hComboID, buf))
					{
						SetWindowText(hComboID, "");
						RefreshNDASIDComboBox(hComboID);
					}
				}
			}
			break;
		case LBN_SETFOCUS:
//		case EN_SETFOCUS:
//			SendMessage(hRadioMAC, BM_SETCHECK, (IDC_EDIT_MAC == LOWORD(wParam)) ? BST_CHECKED : BST_UNCHECKED, 0);
//			SendMessage(hRadioID, BM_SETCHECK, (IDC_EDIT_ID == LOWORD(wParam)) ? BST_CHECKED : BST_UNCHECKED, 0);
			break;
		}
		break;
		
	case WM_DESTROY:
		{
		}
		break;
	}
	return FALSE;
}

BOOL WINAPI GetAddressDialog( HINSTANCE hInstance, HWND hParent )
{
	return ( DialogBox( hInstance, MAKEINTRESOURCE(IDD_OPEN_ADDRESS_DIALOG), hParent, (DLGPROC) OpenAddressDialogProc ) == IDOK );
}

static BOOL CALLBACK GotoTrackDialogProc(HWND hDlg, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER( lParam );

	HWND hEditTrack = GetDlgItem(hDlg, IDC_EDIT1);
	HWND hEditBitmapTrack = GetDlgItem(hDlg, IDC_EDIT_NDAS_BITMAP);

	switch (iMsg)
	{
	case WM_INITDIALOG:
		{
			CHAR szTempBuffer[10240];

			sprintf( szTempBuffer, "%I64d", CurrentSectorNumber );
			SetDlgItemText( hDlg, IDC_EDIT1, szTempBuffer );

			DISK_GEOMETRY dg;
			Drive->GetDriveGeometry(&dg);

			INT64 TotalSizeInBytes = dg.SectorsPerTrack;
			TotalSizeInBytes *= dg.BytesPerSector;
			TotalSizeInBytes *= dg.TracksPerCylinder;
			TotalSizeInBytes *= dg.Cylinders.QuadPart;

			sprintf( szTempBuffer,
				"Cylinders = %I64d\r\n"
				"Sectors = %I64d\r\n"
				"TracksPerCylinder = %ld\r\n"
				"SectorsPerTrack = %ld\r\n"
				"BytesPerSector = %ld\r\n"
				"TotalSizeInBytes = %I64d\r\n",
				dg.Cylinders.QuadPart,
				SelectedPartitionInfo->m_NumberOfSectors,
				dg.TracksPerCylinder,
				dg.SectorsPerTrack,
				dg.BytesPerSector,
				TotalSizeInBytes );

			SetDlgItemText(hDlg,IDC_EDIT3,szTempBuffer);
			SetDlgItemText(hDlg,IDC_EDIT_NDAS_BITMAP, "0");

			if(!NDASDrive || !NDASDrive->IsOpen())
			{
				RECT rtDlg;
				GetWindowRect(hDlg, &rtDlg);
				MoveWindow(hDlg, 0, 0, rtDlg.right - rtDlg.left, 250, TRUE);
			}
			else
			{
			}
		}
		return FALSE;

	case WM_COMMAND:
		if( HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD (wParam))
			{
			case IDOK:
				{
					CHAR szBuffer[256];
					GetDlgItemText(hDlg,IDC_EDIT1, szBuffer, sizeof(szBuffer) );

					INT64 TempCurrentSectorNumber = 0;
					if(0 == strncmp(szBuffer, "0x", 2) ||
						'-' == szBuffer[0] && 0 == strncmp(szBuffer +1, "0x", 2))
						sscanf( szBuffer, "%I64x", &TempCurrentSectorNumber );
					else
						sscanf( szBuffer, "%I64d", &TempCurrentSectorNumber );

					if(TempCurrentSectorNumber < 0)
					{
						TempCurrentSectorNumber = SelectedPartitionInfo->m_NumberOfSectors + TempCurrentSectorNumber;
					}

					if( (TempCurrentSectorNumber < SelectedPartitionInfo->m_NumberOfSectors) && (TempCurrentSectorNumber >= 0) )
					{
						CurrentSectorNumber = TempCurrentSectorNumber;
						EndDialog( hDlg, IDOK );
					}
				}
				return TRUE;

			case IDCANCEL:
				EndDialog( hDlg, IDCANCEL );
				return TRUE;
			case IDC_BTN_NDAS_MBR:
				SetWindowText(hEditTrack, "0");
				return TRUE;
			case IDC_BTN_NDAS_PARTITION_1:
				SetWindowText(hEditTrack, "0x80");
				return TRUE;
			case IDC_BTN_NDAS_PARTITION_2:
			case IDC_BTN_NDAS_PARTITION_3:
			case IDC_BTN_NDAS_PARTITION_4:
			case IDC_BTN_NDAS_LDM:
//				SetWindowText(hEditTrack, "0x80");
				return TRUE;
			case IDC_BTN_NDAS_LAST_WRITTEN_SECTOR:
				SetWindowText(hEditTrack, "-0x1000");
				return TRUE;
			case IDC_BTN_NDAS_BITMAP:
				{
					CHAR szBuffer[256];
					GetDlgItemText(hDlg, IDC_EDIT_NDAS_BITMAP, szBuffer, sizeof(szBuffer));

					INT64 TempCurrentSectorNumber = 0;
					if(0 == strncmp(szBuffer, "0x", 2) ||
						'-' == szBuffer[0] && 0 == strncmp(szBuffer +1, "0x", 2))
						sscanf( szBuffer, "%I64x", &TempCurrentSectorNumber );
					else
						sscanf( szBuffer, "%I64d", &TempCurrentSectorNumber );

					TempCurrentSectorNumber /= (512 * 8 * 128);

					if(TempCurrentSectorNumber < 0 || TempCurrentSectorNumber >= 0x800)
					{
						return TRUE;
					}

					TempCurrentSectorNumber -= 0x0f00;
					TempCurrentSectorNumber *= -1;

					sprintf(szBuffer, "-0x%I64x", TempCurrentSectorNumber);

					SetWindowText(hEditTrack, szBuffer);
				}
				return TRUE;
			case IDC_BTN_NDAS_DIB_V2:
				SetWindowText(hEditTrack, "-0x0002");
				return TRUE;
			case IDC_BTN_NDAS_DIB_V1:
				SetWindowText(hEditTrack, "-0x0001");
			case IDC_BTN_NDAS_RMD:
				SetWindowText(hEditTrack, "-0x0004");
				return TRUE;
				
			}
		}
		break;
	}
	return FALSE;
}

BOOL WINAPI GotoTrackDialog( HINSTANCE hInstance, HWND hParent )
{
	return DialogBox( hInstance, MAKEINTRESOURCE(IDD_GOTO_TRACK_DIALOG), hParent, (DLGPROC) GotoTrackDialogProc ) == IDOK;
}
