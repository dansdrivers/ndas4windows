/******************************************************************************
*
*       FileMon - File System Monitor for Windows NT/9x
*		
*		Copyright (c) 1996 Mark Russinovich and Bryce Cogswell
*
*		See readme.txt for terms and conditions.
*
*    	PROGRAM: Instdrv.c
*
*    	PURPOSE: Loads and unloads the Filemon device driver. This code
*		is taken from the instdrv example in the NT DDK.
*
******************************************************************************/
#include "NDSetup.h"
#include "ROIoctlcmd.h"
#include "NDFilter.h"

/****************************************************************************
*
*    FUNCTION: InstallDriver( IN SC_HANDLE, IN LPCTSTR, IN LPCTSTR)
*
*    PURPOSE: Creates a driver service.
*
****************************************************************************/
BOOL InstallDriver( IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName, IN LPCTSTR ServiceExe )
{
    SC_HANDLE  schService;

    //
    // NOTE: This creates an entry for a standalone driver. If this
    //       is modified for use with a driver that requires a Tag,
    //       Group, and/or Dependencies, it may be necessary to
    //       query the registry for existing driver information
    //       (in order to determine a unique Tag, etc.).
    //

    schService = CreateService( SchSCManager,          // SCManager database
                                DriverName,           // name of service
                                DriverName,           // name to display
                                SERVICE_ALL_ACCESS,    // desired access
                                SERVICE_KERNEL_DRIVER, // service type  
								SERVICE_AUTO_START,    // start type   SERVICE_DEMAND_START, SERVICE_BOOT_START,
                                SERVICE_ERROR_NORMAL,  // error control type
                                ServiceExe,            // service's binary
                                NULL,                  // no load ordering group
                                NULL,                  // no tag identifier
                                NULL,                  // no dependencies
                                NULL,                  // LocalSystem account
                                NULL                   // no password
                                );
    if ( schService == NULL )
        return FALSE;

	CloseServiceHandle( schService );

    return TRUE;
}



/****************************************************************************
*
*    FUNCTION: InstallDriver( IN SC_HANDLE, IN LPCTSTR, IN LPCTSTR)
*
*    PURPOSE: Creates a driver service.
*
****************************************************************************/
BOOL InstallDriverEx(
		IN SC_HANDLE SchSCManager,
		IN LPCTSTR	DriverName,
		IN LPCTSTR	DisplayName,
		IN LPCTSTR	ServiceExe,
		IN DWORD	StartType,
		IN DWORD	ErrorControl,
		IN LPCTSTR	LoadOrderGroup,
		IN LPCTSTR	Dependencies
	)
{
    SC_HANDLE  schService;

    //
    // NOTE: This creates an entry for a standalone driver. If this
    //       is modified for use with a driver that requires a Tag,
    //       Group, and/or Dependencies, it may be necessary to
    //       query the registry for existing driver information
    //       (in order to determine a unique Tag, etc.).
    //

    schService = CreateService( SchSCManager,          // SCManager database
                                DriverName,           // name of service
                                DisplayName,           // name to display
                                SERVICE_ALL_ACCESS,    // desired access
                                SERVICE_KERNEL_DRIVER, // service type  
								StartType,				// start type   SERVICE_DEMAND_START, SERVICE_BOOT_START,
                                ErrorControl,			// error control type
                                ServiceExe,            // service's binary
                                LoadOrderGroup,        // no load ordering group
                                NULL,                  // no tag identifier
                                NULL,                  // no dependencies
                                NULL,                  // LocalSystem account
                                NULL                   // no password
                                );
    if ( schService == NULL )
        return FALSE;

	CloseServiceHandle( schService );

    return TRUE;
}

/****************************************************************************
*
*    FUNCTION: StartDriver( IN SC_HANDLE, IN LPCTSTR)
*
*    PURPOSE: Starts the driver service.
*
****************************************************************************/
BOOL StartDriver( IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName )
{
    SC_HANDLE  schService;
    BOOL       ret;

    schService = OpenService( SchSCManager,
                              DriverName,
                              SERVICE_ALL_ACCESS
                              );
    if ( schService == NULL )
        return FALSE;

    ret = StartService( schService, 0, NULL )
       || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING 
	   || GetLastError() == ERROR_SERVICE_DISABLED;

    CloseServiceHandle( schService );

    return ret;
}

/****************************************************************************
*
*    FUNCTION: StartDriver(IN LPCTSTR)
*
*    PURPOSE: Starts the driver service.
*
****************************************************************************/
BOOL __stdcall StartDriver(IN LPCTSTR DriverName )
{
	SC_HANDLE schSCManager ;
    SC_HANDLE  schService;
    BOOL       ret;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if ( schSCManager == NULL )
        return FALSE;

    schService = OpenService( schSCManager,
                              DriverName,
                              SERVICE_ALL_ACCESS
                              );
    if ( schService == NULL )
        return FALSE;

    ret = StartService( schService, 0, NULL )
       || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING 
	   || GetLastError() == ERROR_SERVICE_DISABLED;

    CloseServiceHandle( schService );
 	CloseServiceHandle( schSCManager );

    return ret;
}


/****************************************************************************
*
*    FUNCTION: OpenDevice( IN LPCTSTR, HANDLE *)
*
*    PURPOSE: Opens the device and returns a handle if desired.
*
****************************************************************************/
BOOL OpenDevice( IN LPCTSTR DriverName, HANDLE * lphDevice )
{
    TCHAR    completeDeviceName[64];
    HANDLE   hDevice;

    //
    // Create a \\.\XXX device name that CreateFile can use
    //
    // NOTE: We're making an assumption here that the driver
    //       has created a symbolic link using it's own name
    //       (i.e. if the driver has the name "XXX" we assume
    //       that it used IoCreateSymbolicLink to create a
    //       symbolic link "\DosDevices\XXX". Usually, there
    //       is this understanding between related apps/drivers.
    //
    //       An application might also peruse the DEVICEMAP
    //       section of the registry, or use the QueryDosDevice
    //       API to enumerate the existing symbolic links in the
    //       system.
    //

	if( (GetVersion() & 0xFF) >= 5 ) {

		//
		// We reference the global name so that the application can
		// be executed in Terminal Services sessions on Win2K
		//
		StringCchPrintf(completeDeviceName, sizeof(completeDeviceName),
			TEXT("\\\\.\\Global\\%s"), DriverName);

	} else {

		StringCchPrintf(completeDeviceName, sizeof(completeDeviceName),
			TEXT("\\\\.\\%s"), DriverName );

	}
    hDevice = CreateFile( completeDeviceName,
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL
                          );
    if ( hDevice == ((HANDLE)-1) )
        return FALSE;

	// If user wants handle, give it to them.  Otherwise, just close it.
	if ( lphDevice )
		*lphDevice = hDevice;
	else
	    CloseHandle( hDevice );

    return TRUE;
}



/****************************************************************************
*
*    FUNCTION: StopDriver( IN SC_HANDLE, IN LPCTSTR)
*
*    PURPOSE: Has the configuration manager stop the driver (unload it)
*
****************************************************************************/
BOOL StopDriver( IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName )
{
    SC_HANDLE       schService;
    BOOL            ret;
    SERVICE_STATUS  serviceStatus;

    schService = OpenService( SchSCManager, DriverName, SERVICE_ALL_ACCESS );
    if ( schService == NULL )
        return FALSE;

    ret = ControlService( schService, SERVICE_CONTROL_STOP, &serviceStatus );

    CloseServiceHandle( schService );

    return ret;
}


/****************************************************************************
*
*    FUNCTION: StopDriver(IN LPCTSTR)
*
*    PURPOSE: Has the configuration manager stop the driver (unload it)
*
****************************************************************************/
BOOL __stdcall StopDriver(IN LPCTSTR DriverName )
{
	SC_HANDLE		schSCManager ;
    SC_HANDLE       schService;
    BOOL            ret;
    SERVICE_STATUS  serviceStatus;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    if (schSCManager == NULL)
        return FALSE;

    schService = OpenService( schSCManager, DriverName, SERVICE_ALL_ACCESS );
    if (schService == NULL)
        return FALSE;

    ret = ControlService( schService, SERVICE_CONTROL_STOP, &serviceStatus );

    CloseServiceHandle( schService );
    CloseServiceHandle( schSCManager );

    return ret;
}


/****************************************************************************
*
*    FUNCTION: RemoveDriver( IN SC_HANDLE, IN LPCTSTR)
*
*    PURPOSE: Deletes the driver service.
*
****************************************************************************/
BOOL RemoveDriver( IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName )
{
    SC_HANDLE  schService;
    BOOL       ret;

    schService = OpenService( SchSCManager,
                              DriverName,
                              SERVICE_ALL_ACCESS
                              );

    if ( schService == NULL )
        return FALSE;

    ret = DeleteService( schService );

    CloseServiceHandle( schService );

    return ret;
}

/****************************************************************************
*
*    FUNCTION: DriverExist( IN SC_HANDLE, IN LPCTSTR)
*
*    PURPOSE: Deletes the driver service.
*
****************************************************************************/
BOOL DriverExist( IN SC_HANDLE SchSCManager, IN LPCTSTR DriverName )
{
    SC_HANDLE  schService;

    schService = OpenService( SchSCManager,
                              DriverName,
                              SERVICE_ALL_ACCESS
                              );

    if ( schService == NULL )
        return FALSE;

    CloseServiceHandle( schService );

    return TRUE ;
}

/****************************************************************************
*
*    FUNCTION: UnloadDeviceDriver( const TCHAR *)
*
*    PURPOSE: Stops the driver and has the configuration manager unload it.
*
****************************************************************************/
BOOL UnloadDeviceDriver( const TCHAR * Name )
{
	SC_HANDLE	schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager != NULL)
	{
		StopDriver(schSCManager, Name);
		RemoveDriver(schSCManager, Name);
	 	CloseServiceHandle(schSCManager);
	}
	else
	{
		LogPrintfErr(TEXT("Cannot open Service Control Manager"));
	}

	return TRUE;
}



/****************************************************************************
*
*    FUNCTION: LoadDeviceDriver( const TCHAR, const TCHAR, HANDLE *)
*
*    PURPOSE: Registers a driver with the system configuration manager 
*	 and then loads it.
*
****************************************************************************/
BOOL LoadDeviceDriver( const TCHAR * Name, const TCHAR * Path, 
					  HANDLE * lphDevice, PDWORD Error )
{
	SC_HANDLE	schSCManager;
	BOOL		okay;


	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	// Remove previous instance
	RemoveDriver( schSCManager, Name );

	// Ignore success of installation: it may already be installed.
	if(!DriverExist(schSCManager, Name)) {
		InstallDriver( schSCManager, Name, Path );
	}

	// Ignore success of start: it may already be started.
	StartDriver( schSCManager, Name );

	// Do make sure we can open it.
	okay = OpenDevice( Name, lphDevice );
	*Error = GetLastError();
 	CloseServiceHandle( schSCManager );

	return okay;
}


/****************************************************************************
*
*    FUNCTION: LoadDeviceDriver( const TCHAR, const TCHAR, HANDLE *)
*
*    PURPOSE: Registers a driver with the system configuration manager 
*	 and then loads it.
*
****************************************************************************/
int LoadAndStartROFilter(TCHAR *DrvFilePath)
{
	static TCHAR	driverPath[ MAX_PATH + 1];
	TCHAR			systemRoot[ MAX_PATH + 1];
	DWORD			error ;
	DWORD			nb, versionNumber;
	HANDLE			hROFilter = INVALID_HANDLE_VALUE ;
	HANDLE			findHandle;
	WIN32_FIND_DATA findData;
	TCHAR			*File;

	LogPrintf(TEXT("Entering LoadAndStartROFilter(%s)..."), DrvFilePath);

	//
	// copy the driver to <winnt>\system32\drivers so that we
	// can run off of a CD or network drive
	//

	GetWindowsDirectory(systemRoot, MAX_PATH);

	StringCchPrintf(driverPath, sizeof(driverPath),
		TEXT("%s\\system32\\drivers\\%s"), systemRoot, ROFILT_SYS_FILE);

	if( !CopyFile( DrvFilePath, driverPath, FALSE )) {
		LogPrintf(
			TEXT("Unable to copy %s to %s.\n")
			TEXT("Make sure that %s is in the current directory. ErrCode:%d"), 
			ROFILT_SYS_NAME, driverPath, DrvFilePath, GetLastError());
		LogPrintf(TEXT("Leaving LoadAndStartROFilter()... %d"), NDS_FAIL);
		return NDS_FAIL;
	}
	
	SetFileAttributes( driverPath, FILE_ATTRIBUTE_NORMAL );

	//
	// load device driver
	//

	if( !LoadDeviceDriver(ROFILT_SYS_NAME, driverPath, &hROFilter, &error) )  {
		UnloadDeviceDriver(ROFILT_SYS_NAME);
		if( !LoadDeviceDriver(ROFILT_SYS_NAME, driverPath, &hROFilter, &error) )  {
			LogPrintf(
				TEXT("Error loading %s (%s), Error %d"), 
				ROFILT_SYS_NAME, DrvFilePath, error);
			LogPrintf(TEXT("Leaving LoadAndStartROFilter()... %d"), NDS_FAIL);
			return NDS_FAIL;
		}
	}

	LogPrintf(TEXT("%s loaded successfully\n"), ROFILT_SYS_NAME);

	//
	// correct driver version?
	//
	
	if( !DeviceIoControl(hROFilter, IOCTL_ROFILT_VERSION,
						NULL, 0, &versionNumber, sizeof(DWORD), &nb, NULL ) ||
		versionNumber != ROFILTVERSION ) {

		LogPrintf(TEXT("LDServ located a driver with the wrong version."));
		LogPrintf(TEXT("If you just installed a new version you must reboot before you are")
			TEXT("able to use it."));
		LogPrintf(TEXT("Leaving LoadAndStartROFilter()... %d"), NDS_REBOOT_REQUIRED);
		return NDS_REBOOT_REQUIRED ;
	}

	//
	// tells the driver to start filtering
	//

	if ( ! DeviceIoControl(	hROFilter, IOCTL_FILEMON_STARTFILTER,
							NULL, 0, NULL, 0, &nb, NULL ) )	{
		LogPrintf(
			TEXT("Couldn't access device driver, Error %d"),
			GetLastError()) ;
		LogPrintf(TEXT("Leaving LoadAndStartROFilter()... %d"), NDS_FAIL);
		return NDS_FAIL;
	}	

	CloseHandle( hROFilter );

	LogPrintf(TEXT("Leaving LoadAndStartROFilter()... %d"), NDS_SUCCESS);
	return NDS_SUCCESS ;
}


/****************************************************************************
*
*    FUNCTION: LoadDeviceDriver( const TCHAR, const TCHAR, HANDLE *)
*
*    PURPOSE: Registers a driver with the system configuration manager 
*	 and then loads it.
*
****************************************************************************/
int UnloadROFilter()
{
//	ULONG		irpcount;
	ULONG	nb;
	BOOL	bRet;
	HANDLE	hROFilter = INVALID_HANDLE_VALUE ;

	LogPrintf(TEXT("Entering UnloadROFilter()..."));

	//
	// Open Device, if failed just continue to delete service
	//
	if (!OpenDevice( ROFILT_SYS_NAME, &hROFilter))
	{
		// safely ignore error
		LogPrintf(TEXT("Cannot open device"));
	}
	else
	{
		//
		// Tell driver to stop filtering
		//
		if (!DeviceIoControl(hROFilter, IOCTL_ROFILT_STOPFILTER,
							NULL, 0, NULL, 0, &nb, NULL ) )
		{
			LogPrintfErr(TEXT("Cannot access device driver."));
			LogPrintf(TEXT("Leaving UnloadROFilter()... %d"), NDS_FAIL);
		}
	}

	if ( !UnloadDeviceDriver(ROFILT_SYS_NAME) )
	{
		// safely ignore error
		LogPrintf(TEXT("Error unloading \"%s\""));
	}

	if (hROFilter != INVALID_HANDLE_VALUE)
		CloseHandle( hROFilter );
	
	LogPrintf(TEXT("ROFilter stopped successfully\n"));

	LogPrintf(TEXT("Leaving UnloadROFilter()... %d"), NDS_REBOOT_REQUIRED);
	return NDS_REBOOT_REQUIRED;
}





/****************************************************************************
*
*    FUNCTION: InstallNonPnPDriver( const TCHAR, const TCHAR, HANDLE *)
*
*    PURPOSE: Registers a driver with the system configuration manager 
*	 and then loads it.
*
****************************************************************************/
int __stdcall InstallNonPnPDriver(
		IN LPCTSTR	DrvFilePath,
		IN LPCTSTR	SysName,
		IN LPCTSTR	FileName,
		IN LPCTSTR	DisplayName,
		IN DWORD	StartType,
		IN DWORD	ErrorControl,
		IN LPCTSTR	LoadOrderGroup,
		IN LPCTSTR	Dependencies
	)
{
	static TCHAR	driverPath[ MAX_PATH + 1];
	TCHAR			systemRoot[ MAX_PATH + 1];
	SC_HANDLE	schSCManager;

	LogPrintf(TEXT("Entering InstallNonPnPDriver(%s)..."), DrvFilePath);

	//
	// copy the driver to <winnt>\system32\drivers so that we
	// can run off of a CD or network drive
	//

	GetWindowsDirectory(systemRoot, MAX_PATH);

	StringCchPrintf(driverPath, sizeof(driverPath),
		TEXT("%s\\system32\\drivers\\%s"), systemRoot, FileName);

	if( !CopyFile( DrvFilePath, driverPath, FALSE )) {
		LogPrintf(
			TEXT("Unable to copy %s to %s.\n")
			TEXT("Make sure that %s is in the current directory. ErrCode:%d"), 
					SysName, DrvFilePath, driverPath, GetLastError());
		LogPrintf(TEXT("Leaving InstallNonPnPDriver()... %d"), NDS_FAIL);
		return NDS_FAIL;
	}
	
	SetFileAttributes( driverPath, FILE_ATTRIBUTE_NORMAL );

	//
	// register as a service.
	// Remove previous instance
	// Ignore success of installation: it may already be installed.
	//
	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	if(schSCManager) {
		RemoveDriver( schSCManager, SysName );
		InstallDriverEx(
				schSCManager,
				SysName,
				DisplayName,
				driverPath,
				StartType,
				ErrorControl,
				LoadOrderGroup,
				Dependencies
				);

	 	CloseServiceHandle( schSCManager );
	}

	LogPrintf(TEXT("%s installed successfully\n"), SysName);
	LogPrintf(TEXT("Leaving InstallNonPnPDriver()... %d"), NDS_SUCCESS);
	return NDS_SUCCESS ;
}


int __stdcall StopNonPnPDriver(
		IN LPCTSTR	SysName
	) {
	SC_HANDLE		schSCManager;

	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	if( !schSCManager || StopDriver( schSCManager, SysName ) == FALSE)
		return NDS_FAIL ;

	CloseServiceHandle(schSCManager) ;

	return NDS_SUCCESS ;
}


int __stdcall UninstallNonPnPDriver(
		IN LPCTSTR	SysName,
		IN LPCTSTR	FileName
	)
{
	static TCHAR	driverPath[ MAX_PATH + 1];
	TCHAR			systemRoot[ MAX_PATH + 1];
	SC_HANDLE		schSCManager;


	LogPrintf(TEXT("Entering UninstallNonPnPDriver(%s)..."), SysName);

	//
	// Remove previous instance.
	// Ignore success of uninstallation
	//
	schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );

	if(schSCManager) {
		RemoveDriver( schSCManager, SysName );
		CloseServiceHandle(schSCManager) ;
	} else {
		LogPrintf(
			TEXT("Unable to unregister %s. ErrorCode:%d\n"),
			SysName, GetLastError());
	}

	//
	// delete the driver file in <winnt>\system32\drivers.
	//
	GetWindowsDirectory(systemRoot, MAX_PATH);

	StringCchPrintf(driverPath, sizeof(driverPath),
		TEXT("%s\\system32\\drivers\\%s"), systemRoot, FileName);


	if( !DeleteFile(driverPath) ) {
		LogPrintf(
			TEXT("Unable to delete %s. ErrorCode:%d\n"),
			driverPath, GetLastError());
		LogPrintf(TEXT("Leaving UninstallNonPnPDriver()... %d"), NDS_FAIL);
		return NDS_FAIL;
	}

	
	LogPrintf(TEXT("%s uninstalled successfully\n"), SysName);
	LogPrintf(TEXT("Leaving UninstallNonPnPDriver()... %d"), NDS_SUCCESS);
	return NDS_SUCCESS ;
}
