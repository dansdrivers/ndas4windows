#include "StdAfx.h"
#include "lfstest.h"



LFSTRET
LfstFileManagement(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;
	BOOL	bret ;

	PrintSubcategoryTitle("File Management") ;

	//
	//	CheckNameLegalDOS8Dot3
	//
	if(IsWinXPorLater(command->WinVerMajor,command->WinVerMinor))
	{
		LPTSTR	DosFileName = TEXT("DOSFILE.DOS") ;
		LPTSTR	WrongDosFileName = TEXT("\n") ;
		BOOL	Spaces, Legal ;
		CHAR	OemName[64] ;

		PrintFunctionTitle("CheckNameLegalDOS8Dot3()") ;

		bret = CheckNameLegalDOS8Dot3(
					DosFileName,
					OemName,
					64,
					&Spaces,
					&Legal
				);
		PrintPassFail(bret,"CheckNameLegalDOS8Dot3()", 1) ;
		if(!bret) {
			if(LFST_FLAGON(command->Flags, LFST_EXITONERR))
				return LFST_FAIL ;
		}
		bret = CheckNameLegalDOS8Dot3(
				DosFileName,
				OemName,
				64,
				&Spaces,
				&Legal
		);
		PrintPassFail(bret,"CheckNameLegalDOS8Dot3()", 2) ;
		if(!bret) {
			if(LFST_FLAGON(command->Flags, LFST_EXITONERR))
				return LFST_FAIL ;
		}

	}

	//
	//	CopyFile
	//
	{
		LPTSTR	FileName1 = TEXT("File1.dat") ;
		LPTSTR	FileName2 = TEXT("\\File2.dat") ;	// do not remove back-slash
		LPTSTR	FileName3 = TEXT("File3.dat") ;
		TCHAR	DestFileFullPath[MAX_PATH] ;

		PrintFunctionTitle("CopyFile() into the test device.") ;

		//
		//	make up DestFile full path.
		//
		_tcscpy(DestFileFullPath, command->BaseDir) ;
		_tcscat(DestFileFullPath, FileName2) ;
		if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
			_tprintf(TEXT("        Target full path:%s\n"), DestFileFullPath) ;
		}

		bret = CopyFile(FileName1, DestFileFullPath, FALSE) ;

		PrintPassFail(bret,"CopyFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		PrintFunctionTitle("CopyFile() from the test device.") ;

		bret = CopyFile(DestFileFullPath, FileName3, FALSE) ;
		PrintPassFail(bret,"CopyFile()", 2) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;
	}

	//
	//	MoveFile()
	//
	{
		LPTSTR	FileName3 = TEXT("File3.dat") ;
		LPTSTR	FileName4 = TEXT("\\File4.dat") ;	// do not remove back-slash
		TCHAR	DestFileFullPath[MAX_PATH] ;

		PrintFunctionTitle("MoveFile() into the test device.") ;

		//
		//	make up DestFile full path.
		//
		_tcscpy(DestFileFullPath, command->BaseDir) ;
		_tcscat(DestFileFullPath, FileName4) ;
		if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
			_tprintf(TEXT("        Target full path:%s\n"), DestFileFullPath) ;
		}

		bret = MoveFile(FileName3, DestFileFullPath) ;

		PrintPassFail(bret,"MoveFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		PrintFunctionTitle("MoveFile() from the test device.") ;

		bret = MoveFile(DestFileFullPath, FileName3) ;
		PrintPassFail(bret,"MoveFile()", 2) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;
	}

	//
	//	DeleteFile
	//
	{
		LPTSTR	FileName2 = TEXT("\\File2.dat") ;	// do not remove back-slash
		TCHAR	DestFileFullPath[MAX_PATH] ;

		PrintFunctionTitle("DeleteFile()") ;

		//
		//	make up DestFile full path.
		//
		_tcscpy(DestFileFullPath, command->BaseDir) ;
		_tcscat(DestFileFullPath, FileName2) ;
		if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
			_tprintf(TEXT("       Full file path to be deleted:%s\n"), DestFileFullPath) ;
		}

		bret = DeleteFile(DestFileFullPath) ;
		PrintPassFail(bret,"DeleteFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;
	}


	return ret ;
}


LFSTRET
LfstFileIo(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;


	return ret ;
}


LFSTRET
LfstFileMapping(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;
	BOOL	bret ;

	PrintSubcategoryTitle("File Mapping") ;

	//
	//	Execute
	//
	{
		LPTSTR	ExecFile = TEXT("Lfstsmpl.exe") ;
		LPTSTR	ExecName = TEXT("\\Lfstsmpl.exe") ;	// Do not remove back-slash
		TCHAR	DestFileFullPath[MAX_PATH] ;
		PROCESS_INFORMATION	ProcessInfo ;
		STARTUPINFO	StartupInfo ;

		PrintFunctionTitle("Executing") ;
	
		//
		//	make up executable full path.
		//
		_tcscpy(DestFileFullPath, command->BaseDir) ;
		_tcscat(DestFileFullPath, ExecName) ;
		if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
			_tprintf(TEXT("       Target executable full path:%s\n"), DestFileFullPath) ;
			PrintFunctionOutput("Copying the executable....\n") ;
		}
		bret = CopyFile(ExecFile, DestFileFullPath, FALSE) ;
		if(!bret) {
			_tprintf(TEXT("ERROR: could not copy the excutable.\n")) ;
		}
		ReturnIfExitFlagOn(bret, command->Flags) ;
	
		ZeroMemory(&StartupInfo, sizeof(STARTUPINFO)) ;
		StartupInfo.cb = sizeof(STARTUPINFO) ;

		bret = CreateProcess(
						NULL,
						DestFileFullPath,
						NULL,
						NULL,
						FALSE,
						0,
						NULL,
						command->BaseDir,
						&StartupInfo,
						&ProcessInfo
					) ;

		PrintPassFail(bret,"CreateProcess()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;


		//
		//	close handles before exit.
		//
		if(bret) {
			CloseHandle(ProcessInfo.hProcess) ;
			CloseHandle(ProcessInfo.hThread) ;
		}
	}

	//
	// Memory mapping
	//
	{
		LPTSTR	FileName1 = TEXT("Filemap.dat") ;
		LPTSTR	FilePath1 = TEXT("\\Filemap.dat") ;	// Do not remove back-slash
		TCHAR	DestFileFullPath[MAX_PATH] ;
		HANDLE	FileHandle ;
		HANDLE	MapHandle ;
		LPVOID	MapAddress ;
		CHAR	WriteData[2] = { 'X', 'X' } ;


		PrintFunctionTitle("CreateFileMapping() with PAGE_READWRITE") ;
		//
		//	make up test data full path.
		//
		_tcscpy(DestFileFullPath, command->BaseDir) ;
		_tcscat(DestFileFullPath, FilePath1) ;
		if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
			_tprintf(TEXT("        Target file full path:%s\n"), DestFileFullPath) ;
			PrintFunctionOutput("Copying the target....\n") ;
		}
		bret = CopyFile(FileName1, DestFileFullPath, FALSE) ;
		if(!bret) {
			_tprintf(TEXT("ERROR: could not copy the excutable.\n")) ;
		}
		ReturnIfExitFlagOn(bret, command->Flags) ;


		FileHandle = CreateFile(
							DestFileFullPath,
							GENERIC_READ|GENERIC_WRITE,
							FILE_SHARE_READ|FILE_SHARE_WRITE,
							NULL,
							OPEN_EXISTING,
							0,
							NULL
						) ;

		if(FileHandle == NULL || FileHandle == INVALID_HANDLE_VALUE) {
			bret = FALSE ;
		}
		PrintPassFail(bret,"CreateFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		MapHandle = CreateFileMapping(
				FileHandle,
				NULL,
				PAGE_READWRITE,
				0,
				0,
				NULL
			) ;
		if(MapHandle == NULL) {
			bret = FALSE ;
		}
		PrintPassFail(bret,"CreateFileMapping()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		MapAddress = MapViewOfFile(
				MapHandle,
				FILE_MAP_ALL_ACCESS,
				0,
				0,
				0
			) ;
		if(MapAddress == NULL) {
			bret = FALSE ;
		}
		PrintPassFail(bret,"MapViewOfFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		//
		// Read!
		//
		printf("        Read from the map: %c%c",
					*((PUCHAR)MapAddress + 0x10), 
					*((PUCHAR)MapAddress + 0x11)
				) ;

		//
		//	Write!
		//
		printf("      Write into the map: %c%c\n", WriteData[0], WriteData[1]) ;
		*((PUCHAR)MapAddress + 0x10) = WriteData[0] ;
		*((PUCHAR)MapAddress + 0x11) = WriteData[1] ;

		//
		//	Flush!
		//
		bret = FlushViewOfFile(MapAddress, 0) ;
		PrintPassFail(bret,"FlushViewOfFile()", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		//
		// Read again!
		//
		printf("        Read again from the map: %c%c\n",
					*((PUCHAR)MapAddress + 0x10), 
					*((PUCHAR)MapAddress + 0x11)
				) ;
		if(*((PUCHAR)MapAddress + 0x10) != WriteData[0] ||
			*((PUCHAR)MapAddress + 0x11) != WriteData[1]
			) {
				bret = FALSE ;
			}

		PrintPassFail(bret,"Direct Operation", 1) ;
		ReturnIfExitFlagOn(bret, command->Flags) ;

		//
		//	clean up
		//
		UnmapViewOfFile(MapAddress) ;
		CloseHandle(MapHandle) ;
		CloseHandle(FileHandle) ;
	}


	return ret ;
}


LFSTRET
LfstFileEncrypt(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;


	return ret ;
}


LFSTRET
LfstFileLZ(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;


	return ret ;
}


LFSTRET
LfstFileControl(
		PLFSTCommand	command
	) {
	LFSTRET	ret = LFST_SUCC ;


	return ret ;
}


LFSTRET
LfstFile(
		PLFSTCommand	command
	) {
	LFSTRET	ret ;

	_tprintf(TEXT("\n"));

	PrintCategoryTitle("File") ;

	ret = LfstFileManagement(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;
	ret = LfstFileIo(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;
	ret = LfstFileMapping(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;
	ret = LfstFileEncrypt(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;
	ret = LfstFileLZ(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;
	ret = LfstFileControl(command) ;
	if(LFST_FLAGON(command->Flags, LFST_EXITONERR) && ret != LFST_SUCC)
		return ret ;

	return LFST_SUCC ;
}