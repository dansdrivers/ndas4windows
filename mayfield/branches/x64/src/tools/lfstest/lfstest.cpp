// lfstest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lfstest.h"

LFSTCodeString	Category[] = {
	{ LFST_ALL,			TEXT("all") },
	{ LFST_FILESYSTEM,	TEXT("fs") },
	{ LFST_DISK,		TEXT("disk") },
	{ LFST_VOLUME,		TEXT("vol") },
	{ LFST_DIRECTORY,	TEXT("dir") },
	{ LFST_FILE,		TEXT("file") },
	{ LFST_BACKUP,		TEXT("bup") },
	{ LFST_SHARING,		TEXT("sh") },
	{ LFST_PERFORMANCE,	TEXT("pf") },
	{ LFST_NONE,		TEXT("") }
} ;

LFSTCodeString		Options[] = {
	{ LFST_BASEDIR, TEXT("b")},
	{ LFST_VERBOSE, TEXT("v")},
	{ LFST_EXITONERR, TEXT("x")},
	{ LFST_NONE, TEXT("")}
} ;

LFSTRET
TranslateStringToCode(
		LFSTCodeString	CodeString[],
		LPTSTR		keyword,
		PLFSTCodeString	*code
	) {
	ULONG	idx_arg ;
	LFSTRET		ret ;

	ret = LFST_FAIL ;
	for(idx_arg = 0 ; ; idx_arg ++) {
		if(CodeString[idx_arg].Code == LFST_NONE) {
			fprintf(stderr, "ERROR: Invaild keyword specified.\n") ;
			break ;
		}

		if(_tcscmp(keyword, CodeString[idx_arg].String) == 0) {
			*code = CodeString + idx_arg ;
			ret = LFST_SUCC ;
			break ;
		}
	}

	return ret ;
}


#include <windows.h>
#include <stdio.h>

#define BUFSIZE 80

LFSTRET
GetOSVersionInfo(
		PLFSTCommand	command
	)
{
   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;

   // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
   // If that fails, try using the OSVERSIONINFO structure.

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if( !(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi)) )
   {
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (! GetVersionEx ( (OSVERSIONINFO *) &osvi) ) 
         return LFST_FAIL;
   }

   command->WinVerMajor = osvi.dwMajorVersion ;
   command->WinVerMinor = osvi.dwMinorVersion ;
   command->WinBuild = osvi.dwBuildNumber ;

   switch (osvi.dwPlatformId)
   {
      // Test for the Windows NT product family.
      case VER_PLATFORM_WIN32_NT:

         // Test for the specific product family.
         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
            printf ("Microsoft Windows Server 2003 family, ");

         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
            printf ("Microsoft Windows XP ");

         if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
            printf ("Microsoft Windows 2000 ");

         if ( osvi.dwMajorVersion <= 4 )
            printf("Microsoft Windows NT ");

         // Test for specific product on Windows NT 4.0 SP6 and later.
         if( bOsVersionInfoEx )
         {
            // Test for the workstation type.
            if ( osvi.wProductType == VER_NT_WORKSTATION )
            {
               if( osvi.dwMajorVersion == 4 )
                  printf ( "Workstation 4.0 " );
               else if( osvi.wSuiteMask & VER_SUITE_PERSONAL )
                  printf ( "Home Edition " );
               else
                  printf ( "Professional " );
            }
            
            // Test for the server type.
            else if ( osvi.wProductType == VER_NT_SERVER )
            {
               if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
               {
                  if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                     printf ( "Datacenter Edition " );
                  else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                     printf ( "Enterprise Edition " );
                  else if ( osvi.wSuiteMask == VER_SUITE_BLADE )
                     printf ( "Web Edition " );
                  else
                     printf ( "Standard Edition " );
               }

               else if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
               {
                  if( osvi.wSuiteMask & VER_SUITE_DATACENTER )
                     printf ( "Datacenter Server " );
                  else if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                     printf ( "Advanced Server " );
                  else
                     printf ( "Server " );
               }

               else  // Windows NT 4.0 
               {
                  if( osvi.wSuiteMask & VER_SUITE_ENTERPRISE )
                     printf ("Server 4.0, Enterprise Edition " );
                  else
                     printf ( "Server 4.0 " );
               }
            }
         }
         else  // Test for specific product on Windows NT 4.0 SP5 and earlier
         {
            HKEY hKey;
            TCHAR szProductType[BUFSIZE];
            DWORD dwBufLen=BUFSIZE;
            LONG lRet;

            lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
               TEXT("SYSTEM\\CurrentControlSet\\Control\\ProductOptions"),
               0, KEY_QUERY_VALUE, &hKey );
            if( lRet != ERROR_SUCCESS )
               return FALSE;

            lRet = RegQueryValueEx( hKey, TEXT("ProductType"), NULL, NULL,
               (LPBYTE) szProductType, &dwBufLen);
            if( (lRet != ERROR_SUCCESS) || (dwBufLen > BUFSIZE) )
               return FALSE;

            RegCloseKey( hKey );

            if ( lstrcmpi( TEXT("WINNT"), szProductType) == 0 )
               printf( "Workstation " );
            if ( lstrcmpi( TEXT("LANMANNT"), szProductType) == 0 )
               printf( "Server " );
            if ( lstrcmpi( TEXT("SERVERNT"), szProductType) == 0 )
               printf( "Advanced Server " );

            printf( "%d.%d ", osvi.dwMajorVersion, osvi.dwMinorVersion );
         }

      // Display service pack (if any) and build number.

         if( osvi.dwMajorVersion == 4 && 
             lstrcmpi( osvi.szCSDVersion, TEXT("Service Pack 6") ) == 0 )
         {
            HKEY hKey;
            LONG lRet;

            // Test for SP6 versus SP6a.
            lRet = RegOpenKeyEx( HKEY_LOCAL_MACHINE,
               TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Hotfix\\Q246009"),
               0, KEY_QUERY_VALUE, &hKey );
            if( lRet == ERROR_SUCCESS )
               printf( "Service Pack 6a (Build %d)\n", osvi.dwBuildNumber & 0xFFFF );         
            else // Windows NT 4.0 prior to SP6a
            {
               printf( "%s (Build %d)\n",
                  osvi.szCSDVersion,
                  osvi.dwBuildNumber & 0xFFFF);
            }

            RegCloseKey( hKey );
         }
         else // Windows NT 3.51 and earlier or Windows 2000 and later
         {
            printf( "%s (Build %d)\n",
               osvi.szCSDVersion,
               osvi.dwBuildNumber & 0xFFFF);
         }


         break;

      // Test for the Windows 95 product family.
      case VER_PLATFORM_WIN32_WINDOWS:

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
         {
             printf ("Microsoft Windows 95 ");
             if ( osvi.szCSDVersion[1] == 'C' || osvi.szCSDVersion[1] == 'B' )
                printf("OSR2 " );
         } 

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
         {
             printf ("Microsoft Windows 98 ");
             if ( osvi.szCSDVersion[1] == 'A' )
                printf("SE " );
         } 

         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
         {
             printf ("Microsoft Windows Millennium Edition\n");
         } 
         break;

      case VER_PLATFORM_WIN32s:

         printf ("Microsoft Win32s\n");
         break;
   }

   return LFST_SUCC; 
}



LFSTRET
SetDefaultValues(
		PLFSTCommand	command
	) {

	if(command->Category == 0) {
		command->Category = LFST_ALL ;
	}

	return LFST_SUCC ;
}
	

LFSTRET
AnalyzeBaseDir(
		PLFSTCommand	command
	) {
	LPSECURITY_ATTRIBUTES	securityAttr = NULL ;
	BOOL					ret ;
	DWORD					lastError ;
	DWORD					dwret ;
	LPTSTR					FileName ;

	//
	//	get the full path name of the base directory.
	//
	dwret = GetFullPathName(
			command->BaseDir,
			MAX_PATH,
			command->FullBaseDirBuffer,
			&FileName
		) ;
	if(dwret == 0) {
		_tprintf(TEXT("ERROR: Invalid the base directory path.\n")) ;
		return LFST_FAIL ;
	}

	//
	//	set new buffer to the base directory
	//
	command->BaseDir = command->FullBaseDirBuffer ;
	if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
		_tprintf(TEXT("Base directory name:%s\n"), FileName) ;
		_tprintf(TEXT("Base directory full path:%s\n"), command->FullBaseDirBuffer) ;
	}

	//
	//	create new one
	//
	ret = CreateDirectory(command->BaseDir, securityAttr) ;
	if(ret == FALSE) {
		lastError = GetLastError() ;

		if(lastError == ERROR_ALREADY_EXISTS) {
			if(LFST_FLAGON(command->Flags, LFST_VERBOSE)) {
				_tprintf(TEXT("The base directory already exists.\n")) ;
			}
		} else {
			printf("ERROR: could not create the base directory. errcode:%d\n", lastError) ;
			return LFST_FAIL ;
		}
	} else {
		_tprintf(TEXT("Created the base directory \"%s\"\n"), command->BaseDir) ;
	}

	//
	//	get the drive letter
	//

#ifdef _UNICODE
	//
	//	convert the first letter to the upper case.
	//
	if(command->BaseDir[0] >= L'a')
		command->BaseDir[0] -= L'a' - L'A' ;
#else
	if(command->BaseDir[0] >= 'a')
		command->BaseDir[0] -= 'a' - 'A' ;
#endif
	command->DosDriveNo = command->BaseDir[0] - TEXT('A') ;

	return LFST_SUCC ;
}


LFSTRET
TranslateArgvToCommand(
		LONG			argc,
		LPTSTR			argv[],
		PLFSTCommand	command
	) {
	LONG			idx_word ;
	int				ret ;
	PLFSTCodeString	code ;

	for(idx_word = 0 ; idx_word < argc ; idx_word ++) {
		if(argv[idx_word][0] == TEXT('/')) {
			//
			// options
			//
			ret = TranslateStringToCode(
						Options,
						argv[idx_word] + 1,
						&code
					) ;
			if(ret != LFST_SUCC) {
				return ret ;
			}

			LFST_SETFLAG(command->Flags, code->Code ) ;

			//
			//	option's extra parameters.
			//
			if(code->Code == LFST_BASEDIR) {
				if(idx_word + 1 >= argc) {
					printf("ERROR: not enough option /%s \n", code->String ) ;
					return LFST_FAIL ;
				}

				command->BaseDir = argv[idx_word+1] ;
				idx_word++ ;
			}
		} else {
			//
			//	category
			//
			ret = TranslateStringToCode(
					Category,
					argv[idx_word],
					&code
				) ;
			if(ret != LFST_SUCC) {
				return ret ;
			}

			command->Category = code->Code ;
		}
	}

	return LFST_SUCC ;
}


void PrintHelp(LPTSTR	ExecName) {
	LONG	idx ;

	_tprintf(TEXT("\n")) ;
	_tprintf(TEXT("%s [options] [basedir] [category] [subcategory] [function]\n"), ExecName) ;
	_tprintf(TEXT("[category] = ")) ;
	for(idx = 0 ; ; idx ++ ) {
		_tprintf(TEXT("%s"), Category[idx].String ) ;

		if(Category[idx + 1].Code == LFST_NONE) {
			break ;
		}
		_tprintf(TEXT(" | ")) ;
	}

	_tprintf(TEXT("\n")) ;
	_tprintf(TEXT("[options] = ")) ;
	for(idx = 0 ; ; idx ++ ) {
		_tprintf(TEXT("%s"), Options[idx].String ) ;

		if(Options[idx + 1].Code == LFST_NONE) {
			break ;
		}
		_tprintf(TEXT(" | ")) ;
	}

	_tprintf(TEXT("\n")) ;
	_tprintf(TEXT("\n")) ;
}


int __cdecl _tmain(int argc, _TCHAR **argv, _TCHAR **envp)
{
	LFSTCommand	command ;
	LFSTRET		ret ;
	
	_tprintf(TEXT("LFS tester version %d.%d    copyright(C) 2003 XiMeta, Inc.\n"), LFST_VERSION_MAJ, LFST_VERSION_MIN) ;
	if(argc < 3) {
		_ftprintf(stderr, TEXT("ERROR: Too few arguments.\n")) ;

		PrintHelp(LFST_EXEC_NAME) ;

		return 1 ;
	}

	ZeroMemory(&command, sizeof(LFSTCommand)) ;
	command.BaseDir = command.BaseDirBuffer ;

	//
	//	parse arguments
	//
	ret = TranslateArgvToCommand(argc - 1, argv + 1, &command) ;
	if(LFST_SUCC != ret) {
		return 1 ;
	}

	if(!LFST_FLAGON(command.Flags,LFST_BASEDIR)) {
		_ftprintf(stderr, TEXT("ERROR: No base directory specified.\n")) ;

		PrintHelp(LFST_EXEC_NAME) ;

		return 1 ;
	}

	ret = SetDefaultValues(&command) ;
	if(LFST_SUCC != ret) {
		return 1 ;
	}
	//
	//	detect OS versions.
	//
	_tprintf(TEXT("Compiled on Win%04x. Targeted Win%04x. Running on "), WINVER, _WIN32_WINNT) ;
	ret = GetOSVersionInfo(
				&command
		) ;
	if(LFST_SUCC != ret) {
		return 1 ;
	}
	_tprintf(TEXT("\n\n")) ;

	//
	//	verify the base directory
	//
	if(LFST_FLAGON(command.Flags, LFST_VERBOSE)) {
		_tprintf(TEXT("Base directory: %s\n"), command.BaseDir) ;
	}

	ret = AnalyzeBaseDir(&command) ;
	if(LFST_SUCC != ret) {
		return 1 ;
	}

	if(LFST_FLAGON(command.Flags, LFST_VERBOSE)) {
		_tprintf(TEXT("Normalized Base directory: %s\n"), command.BaseDir) ;
	}

	//
	//	jump into sub-routines.
	//
	switch(command.Category) {
	case LFST_ALL:	// go throuth all test.
		ret = LfstDirectory(&command) ;
		if(LFST_FLAGON(command.Flags, LFST_EXITONERR) && ret != LFST_SUCC)
			break ;
		ret = LfstFile(&command) ;
		if(LFST_FLAGON(command.Flags, LFST_EXITONERR) && ret != LFST_SUCC)
			break ;
		ret = LfstPerformance(&command) ;
		if(LFST_FLAGON(command.Flags, LFST_EXITONERR) && ret != LFST_SUCC)
			break ;
		break ;
	case LFST_FILESYSTEM:
		break ;
	case LFST_DISK:
		break ;
	case LFST_VOLUME:
		break ;
	case LFST_DIRECTORY:
		break ;
	case LFST_FILE:
		LfstFile(&command) ;
		break ;
	case LFST_BACKUP:
		break ;
	case LFST_SHARING:
		break ;
	case LFST_PERFORMANCE:
		break ;
	default:
		;
	}

	if(LFST_FLAGON(command.Flags, LFST_VERBOSE)) {
		_tprintf(TEXT("\nprogrammed by hootch 12262003\n")) ;
	}

	return 0;
}

