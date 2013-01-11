/*++

Copyright (C)2002-2005 XIMETA, Inc.
All rights reserved.

--*/

#include "ndasemupriv.h"
#include "winioctl.h"

BOOL
GetCurrentVolumeSparseFlag(
	VOID
){
	BOOL	bret;
    DWORD filesystemFlags;

    bret = GetVolumeInformation(
					NULL,
					NULL,
					0,
					NULL,
					NULL,
					&filesystemFlags,
					NULL,
					0);

    if (bret == TRUE && 
        (filesystemFlags & FILE_SUPPORTS_SPARSE_FILES) != 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

BOOL
GetFileSparseFlag(
	IN PTCHAR	FilePathName
){
	DWORD	fileAttr;
	fileAttr = GetFileAttributes(FilePathName);
	if(fileAttr != INVALID_FILE_ATTRIBUTES &&
		(fileAttr&FILE_ATTRIBUTE_SPARSE_FILE) != 0) {

		return TRUE;
	} else {
		return FALSE;
	}
}

BOOL
SetSparseFile(
	IN HANDLE	FileHandle
){
    BOOL bret;
    DWORD bytesReturned;

    bret = DeviceIoControl(
			FileHandle,
			FSCTL_SET_SPARSE,
			NULL,
			0,
			NULL,
			0,
			&bytesReturned,
			(LPOVERLAPPED)NULL);
    if (!bret) {
		fprintf(stderr, "Can't set sparse file.\n");
    }

	return bret;
}
