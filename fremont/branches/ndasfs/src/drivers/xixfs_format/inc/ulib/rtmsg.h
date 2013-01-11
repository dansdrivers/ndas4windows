/*++ BUILD Version: 0001    // Increment this if a change has global effects

Copyright (c) 1990-2001 Microsoft Corporation

Module Name:

    rtmsg.h

Abstract:

    This file contains the message definitions for the Win32 utilities
    library.

Author:

    Norbert P. Kusters (norbertk) 2-Apr-1991

Revision History:

--*/
//----------------------
//
// DOS 5 chkdsk message.
//
//----------------------
//
//  Values are 32 bit values layed out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: MSG_CONVERT_LOST_CHAINS
//
// MessageText:
//
//  Convert lost chains to files (Y/N)? %0
//
#define MSG_CONVERT_LOST_CHAINS          0x000003E8L

//
// MessageId: MSG_CHK_ERROR_IN_DIR
//
// MessageText:
//
//  Unrecoverable error in folder %1.
//
#define MSG_CHK_ERROR_IN_DIR             0x000003E9L

//
// MessageId: MSG_CHK_CONVERT_DIR_TO_FILE
//
// MessageText:
//
//  Convert folder to file (Y/N)? %0
//
#define MSG_CHK_CONVERT_DIR_TO_FILE      0x000003EAL

//
// MessageId: MSG_TOTAL_DISK_SPACE
//
// MessageText:
//
//  
//  %1 bytes total disk space.
//
#define MSG_TOTAL_DISK_SPACE             0x000003EBL

//
// MessageId: MSG_BAD_SECTORS
//
// MessageText:
//
//  %1 bytes in bad sectors.
//
#define MSG_BAD_SECTORS                  0x000003ECL

//
// MessageId: MSG_HIDDEN_FILES
//
// MessageText:
//
//  %1 bytes in %2 hidden files.
//
#define MSG_HIDDEN_FILES                 0x000003EDL

//
// MessageId: MSG_DIRECTORIES
//
// MessageText:
//
//  %1 bytes in %2 folders.
//
#define MSG_DIRECTORIES                  0x000003EEL

//
// MessageId: MSG_USER_FILES
//
// MessageText:
//
//  %1 bytes in %2 files.
//
#define MSG_USER_FILES                   0x000003EFL

//
// MessageId: MSG_RECOVERED_FILES
//
// MessageText:
//
//  %1 bytes in %2 recovered files.
//
#define MSG_RECOVERED_FILES              0x000003F0L

//
// MessageId: MSG_WOULD_BE_RECOVERED_FILES
//
// MessageText:
//
//  %1 bytes in %2 recoverable files.
//
#define MSG_WOULD_BE_RECOVERED_FILES     0x000003F1L

//
// MessageId: MSG_AVAILABLE_DISK_SPACE
//
// MessageText:
//
//  %1 bytes available on disk.
//
#define MSG_AVAILABLE_DISK_SPACE         0x000003F2L

//
// MessageId: MSG_TOTAL_MEMORY
//
// MessageText:
//
//  %1 total bytes memory.
//
#define MSG_TOTAL_MEMORY                 0x000003F3L

//
// MessageId: MSG_AVAILABLE_MEMORY
//
// MessageText:
//
//  %1 bytes free.
//
#define MSG_AVAILABLE_MEMORY             0x000003F4L

//
// MessageId: MSG_CHK_CANT_NETWORK
//
// MessageText:
//
//  Windows cannot check a disk attached through a network.
//
#define MSG_CHK_CANT_NETWORK             0x000003F5L

//
// MessageId: MSG_1014
//
// MessageText:
//
//  Windows cannot check a disk that is substituted or
//  assigned using the SUBST or ASSIGN command.
//
#define MSG_1014                         0x000003F6L

//
// MessageId: MSG_PROBABLE_NON_DOS_DISK
//
// MessageText:
//
//  The specified disk appears to be a non-Windows XP disk.
//  Do you want to continue? (Y/N) %0
//
#define MSG_PROBABLE_NON_DOS_DISK        0x000003F7L

//
// MessageId: MSG_DISK_ERROR_READING_FAT
//
// MessageText:
//
//  An error occurred while reading the file allocation table (FAT %1).
//
#define MSG_DISK_ERROR_READING_FAT       0x000003F8L

//
// MessageId: MSG_DIRECTORY
//
// MessageText:
//
//  Folder %1.
//
#define MSG_DIRECTORY                    0x000003F9L

//
// MessageId: MSG_CONTIGUITY_REPORT
//
// MessageText:
//
//  %1 contains %2 non-contiguous blocks.
//
#define MSG_CONTIGUITY_REPORT            0x000003FAL

//
// MessageId: MSG_ALL_FILES_CONTIGUOUS
//
// MessageText:
//
//  All specified files are contiguous.
//
#define MSG_ALL_FILES_CONTIGUOUS         0x000003FBL

//
// MessageId: MSG_CORRECTIONS_WILL_NOT_BE_WRITTEN
//
// MessageText:
//
//  Windows found errors on the disk, but will not fix them
//  because disk checking was run without the /F (fix) parameter.
//
#define MSG_CORRECTIONS_WILL_NOT_BE_WRITTEN 0x000003FCL

//
// MessageId: MSG_BAD_FAT_DRIVE
//
// MessageText:
//
//     The file allocation table (FAT) on disk %1 is corrupted.
//
#define MSG_BAD_FAT_DRIVE                0x000003FDL

//
// MessageId: MSG_BAD_FIRST_UNIT
//
// MessageText:
//
//  %1  first allocation unit is not valid. The entry will be truncated.
//
#define MSG_BAD_FIRST_UNIT               0x000003FEL

//
// MessageId: MSG_CHK_DONE_CHECKING
//
// MessageText:
//
//  File and folder verification is complete.
//
#define MSG_CHK_DONE_CHECKING            0x000003FFL

//
// MessageId: MSG_DISK_TOO_LARGE_TO_CONVERT
//
// MessageText:
//
//  The volume is too large to convert.
//
#define MSG_DISK_TOO_LARGE_TO_CONVERT    0x00000400L

//
// MessageId: MSG_CONV_NTFS_CHKDSK
//
// MessageText:
//
//  The volume may have inconsistencies. Run Chkdsk, the disk checking utility.
//
#define MSG_CONV_NTFS_CHKDSK             0x00000401L

//
// MessageId: MSG_1028
//
// MessageText:
//
//     An allocation error occurred. The file size will be adjusted.
//
#define MSG_1028                         0x00000404L

//
// MessageId: MSG_1029
//
// MessageText:
//
//     Cannot recover .. entry, processing continued.
//
#define MSG_1029                         0x00000405L

//
// MessageId: MSG_1030
//
// MessageText:
//
//     Folder is totally empty, no . or ..
//
#define MSG_1030                         0x00000406L

//
// MessageId: MSG_1031
//
// MessageText:
//
//     Folder is joined.
//
#define MSG_1031                         0x00000407L

//
// MessageId: MSG_1032
//
// MessageText:
//
//     Cannot recover .. entry.
//
#define MSG_1032                         0x00000408L

//
// MessageId: MSG_BAD_LINK
//
// MessageText:
//
//  The %1 entry contains a nonvalid link.
//
#define MSG_BAD_LINK                     0x00000409L

//
// MessageId: MSG_BAD_ATTRIBUTE
//
// MessageText:
//
//     Windows has found an entry that contains a nonvalid attribute.
//
#define MSG_BAD_ATTRIBUTE                0x0000040AL

//
// MessageId: MSG_BAD_FILE_SIZE
//
// MessageText:
//
//  The size of the %1 entry is not valid.
//
#define MSG_BAD_FILE_SIZE                0x0000040BL

//
// MessageId: MSG_CROSS_LINK
//
// MessageText:
//
//  %1 is cross-linked on allocation unit %2.
//
#define MSG_CROSS_LINK                   0x0000040CL

//
// MessageId: MSG_1037
//
// MessageText:
//
//     Windows cannot find the %1 folder.
//     Disk check cannot continue past this point in the folder structure.
//
#define MSG_1037                         0x0000040DL

//
// MessageId: MSG_1038
//
// MessageText:
//
//     The folder structure past this point cannot be processed.
//
#define MSG_1038                         0x0000040EL

//
// MessageId: MSG_BYTES_FREED
//
// MessageText:
//
//  %1 bytes of free disk space added.
//
#define MSG_BYTES_FREED                  0x0000040FL

//
// MessageId: MSG_BYTES_WOULD_BE_FREED
//
// MessageText:
//
//  %1 bytes of free disk space would be added.
//
#define MSG_BYTES_WOULD_BE_FREED         0x00000410L

//
// MessageId: MSG_VOLUME_LABEL_AND_DATE
//
// MessageText:
//
//  Volume %1 created %2 %3
//
#define MSG_VOLUME_LABEL_AND_DATE        0x00000411L

//
// MessageId: MSG_TOTAL_ALLOCATION_UNITS
//
// MessageText:
//
//  %1 total allocation units on disk.
//
#define MSG_TOTAL_ALLOCATION_UNITS       0x00000412L

//
// MessageId: MSG_BYTES_PER_ALLOCATION_UNIT
//
// MessageText:
//
//  %1 bytes in each allocation unit.
//
#define MSG_BYTES_PER_ALLOCATION_UNIT    0x00000413L

//
// MessageId: MSG_1044
//
// MessageText:
//
//  Disk checking is not available on disk %1.
//
#define MSG_1044                         0x00000414L

//
// MessageId: MSG_1045
//
// MessageText:
//
//  A nonvalid parameter was specified.
//
#define MSG_1045                         0x00000415L

//
// MessageId: MSG_PATH_NOT_FOUND
//
// MessageText:
//
//  The specified path was not found.
//
#define MSG_PATH_NOT_FOUND               0x00000416L

//
// MessageId: MSG_FILE_NOT_FOUND
//
// MessageText:
//
//  The %1 file was not found.
//
#define MSG_FILE_NOT_FOUND               0x00000417L

//
// MessageId: MSG_LOST_CHAINS
//
// MessageText:
//
//     %1 lost allocation units were found in %2 chains.
//
#define MSG_LOST_CHAINS                  0x00000418L

//
// MessageId: MSG_BLANK_LINE
//
// MessageText:
//
//  
//
#define MSG_BLANK_LINE                   0x00000419L

//
// MessageId: MSG_1050
//
// MessageText:
//
//     The CHDIR command cannot switch to the root folder.
//
#define MSG_1050                         0x0000041AL

//
// MessageId: MSG_BAD_FAT_WRITE
//
// MessageText:
//
//     A disk error occurred during writing of the file allocation table.
//
#define MSG_BAD_FAT_WRITE                0x0000041BL

//
// MessageId: MSG_ONE_STRING
//
// MessageText:
//
//  %1.
//
#define MSG_ONE_STRING                   0x0000041CL

//
// MessageId: MSG_ONE_STRING_NEWLINE
//
// MessageText:
//
//  %1
//
#define MSG_ONE_STRING_NEWLINE           0x0000041EL

//
// MessageId: MSG_NO_ROOM_IN_ROOT
//
// MessageText:
//
//     The root folder on this volume is full. To perform a disk check,
//     Windows requires space in the root folder. Remove some files
//     from this folder, then run disk checking again.
//
#define MSG_NO_ROOM_IN_ROOT              0x0000041FL

//
// MessageId: MSG_1056
//
// MessageText:
//
//  %1 %2 %3.
//
#define MSG_1056                         0x00000420L

//
// MessageId: MSG_1057
//
// MessageText:
//
//  %1 %2, %3.
//
#define MSG_1057                         0x00000421L

//
// MessageId: MSG_1058
//
// MessageText:
//
//  %1%2%3%4%5.
//
#define MSG_1058                         0x00000422L

//
// MessageId: MSG_1059
//
// MessageText:
//
//  %1%2%3%4.
//
#define MSG_1059                         0x00000423L

//
// MessageId: MSG_UNITS_ON_DISK
//
// MessageText:
//
//  %1 available allocation units on disk.
//
#define MSG_UNITS_ON_DISK                0x00000424L

//
// MessageId: MSG_1061
//
// MessageText:
//
//  Windows disk checking cannot fix errors (/F) when run from an
//  MS-DOS window. Try again from the Windows XP shell or command prompt.
//
#define MSG_1061                         0x00000425L

//
// MessageId: MSG_CHK_NO_MEMORY
//
// MessageText:
//
//  An unspecified error occurred.
//
#define MSG_CHK_NO_MEMORY                0x00000426L

//
// MessageId: MSG_HIDDEN_STATUS
//
// MessageText:
//
//  This never gets printed.
//
#define MSG_HIDDEN_STATUS                0x00000427L

//
// MessageId: MSG_CHK_USAGE_HEADER
//
// MessageText:
//
//  Checks a disk and displays a status report.
//  
//
#define MSG_CHK_USAGE_HEADER             0x00000428L

//
// MessageId: MSG_CHK_COMMAND_LINE
//
// MessageText:
//
//  CHKDSK [volume[[path]filename]]] [/F] [/V] [/R] [/X] [/I] [/C] [/L[:size]]
//  
//
#define MSG_CHK_COMMAND_LINE             0x00000429L

//
// MessageId: MSG_CHK_DRIVE
//
// MessageText:
//
//    volume          Specifies the drive letter (followed by a colon),
//                    mount point, or volume name.
//
#define MSG_CHK_DRIVE                    0x0000042AL

//
// MessageId: MSG_CHK_USG_FILENAME
//
// MessageText:
//
//    filename        FAT/FAT32 only: Specifies the files to check for fragmentation.
//
#define MSG_CHK_USG_FILENAME             0x0000042BL

//
// MessageId: MSG_CHK_F_SWITCH
//
// MessageText:
//
//    /F              Fixes errors on the disk.
//
#define MSG_CHK_F_SWITCH                 0x0000042CL

//
// MessageId: MSG_CHK_V_SWITCH
//
// MessageText:
//
//    /V              On FAT/FAT32: Displays the full path and name of every file
//                    on the disk.
//                    On NTFS: Displays cleanup messages if any.
//    /R              Locates bad sectors and recovers readable information
//                    (implies /F).
//    /L:size         NTFS only:  Changes the log file size to the specified number
//                    of kilobytes.  If size is not specified, displays current
//                    size.
//    /X              Forces the volume to dismount first if necessary.
//                    All opened handles to the volume would then be invalid
//                    (implies /F).
//    /I              NTFS only: Performs a less vigorous check of index entries.
//    /C              NTFS only: Skips checking of cycles within the folder
//                    structure.
//  
//  The /I or /C switch reduces the amount of time required to run Chkdsk by
//  skipping certain checks of the volume.
//
#define MSG_CHK_V_SWITCH                 0x0000042DL

//
// MessageId: MSG_WITHOUT_PARAMETERS
//
// MessageText:
//
//  To check the current disk, type CHKDSK with no parameters.
//
#define MSG_WITHOUT_PARAMETERS           0x0000042EL

//
// MessageId: MSG_CHK_CANT_CDROM
//
// MessageText:
//
//  Windows cannot run disk checking on CD-ROM and DVD-ROM drives.
//
#define MSG_CHK_CANT_CDROM               0x0000042FL

//
// MessageId: MSG_CHK_RUNNING
//
// MessageText:
//
//  Checking file system on %1
//
#define MSG_CHK_RUNNING                  0x00000430L

//
// MessageId: MSG_CHK_VOLUME_CLEAN
//
// MessageText:
//
//  The volume is clean.
//
#define MSG_CHK_VOLUME_CLEAN             0x00000431L

//
// MessageId: MSG_CHK_TRAILING_DIRENTS
//
// MessageText:
//
//  Removing trailing folder entries from %1
//
#define MSG_CHK_TRAILING_DIRENTS         0x00000432L

//
// MessageId: MSG_CHK_BAD_CLUSTERS_IN_FILE_SUCCESS
//
// MessageText:
//
//  Windows replaced bad clusters in file %1
//  of name %2.
//
#define MSG_CHK_BAD_CLUSTERS_IN_FILE_SUCCESS 0x00000433L

//
// MessageId: MSG_CHK_BAD_CLUSTERS_IN_FILE_FAILURE
//
// MessageText:
//
//  The disk does not have enough space to replace bad clusters
//  detected in file %1 of name %2.
//  
//
#define MSG_CHK_BAD_CLUSTERS_IN_FILE_FAILURE 0x00000434L

//
// MessageId: MSG_CHK_RECOVERING_FREE_SPACE
//
// MessageText:
//
//  Windows is verifying free space...
//
#define MSG_CHK_RECOVERING_FREE_SPACE    0x00000435L

//
// MessageId: MSG_CHK_DONE_RECOVERING_FREE_SPACE
//
// MessageText:
//
//  Free space verification is complete.
//
#define MSG_CHK_DONE_RECOVERING_FREE_SPACE 0x00000436L

//
// MessageId: MSG_CHK_CHECKING_FILES
//
// MessageText:
//
//  Windows is verifying files and folders...
//
#define MSG_CHK_CHECKING_FILES           0x00000437L

//
// MessageId: MSG_CHK_CANNOT_UPGRADE_DOWNGRADE_FAT
//
// MessageText:
//
//  Windows cannot upgrade this FAT volume.
//
#define MSG_CHK_CANNOT_UPGRADE_DOWNGRADE_FAT 0x00000438L

//
// MessageId: MSG_CHK_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH
//
// MessageText:
//
//  The specified volume name does not have a mount point or drive letter.
//
#define MSG_CHK_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH 0x00000439L

//
// MessageId: MSG_CHK_VOLUME_IS_DIRTY
//
// MessageText:
//
//  The volume is dirty.
//
#define MSG_CHK_VOLUME_IS_DIRTY          0x0000043AL

//-----------------------
//
// Windows XP Chkdsk messages.
//
//-----------------------
//
// MessageId: MSG_CHK_ON_REBOOT
//
// MessageText:
//
//  Do you want to schedule Windows to check your disk the next time
//  you start your computer? (Y/N) %0
//
#define MSG_CHK_ON_REBOOT                0x0000044CL

//
// MessageId: MSG_CHK_VOLUME_SET_DIRTY
//
// MessageText:
//
//  Windows will check your disk the next time you start
//  your computer.
//
#define MSG_CHK_VOLUME_SET_DIRTY         0x0000044DL

//
// MessageId: MSG_CHK_BOOT_PARTITION_REBOOT
//
// MessageText:
//
//  
//  Windows has finished checking your disk.
//  Please wait while your computer restarts.
//
#define MSG_CHK_BOOT_PARTITION_REBOOT    0x0000044EL

//
// MessageId: MSG_CHK_BAD_LONG_NAME
//
// MessageText:
//
//  Removing nonvalid long folder entry from %1...
//
#define MSG_CHK_BAD_LONG_NAME            0x0000044FL

//
// MessageId: MSG_CHK_CHECKING_VOLUME
//
// MessageText:
//
//  Now checking %1...
//
#define MSG_CHK_CHECKING_VOLUME          0x00000450L

//
// MessageId: MSG_CHK_BAD_LONG_NAME_IS
//
// MessageText:
//
//  Removing orphaned long folder entry %1...
//
#define MSG_CHK_BAD_LONG_NAME_IS         0x00000451L

//
// MessageId: MSG_CHK_WONT_ZERO_LOGFILE
//
// MessageText:
//
//  The log file size must be greater than 0.
//
#define MSG_CHK_WONT_ZERO_LOGFILE        0x00000452L

//
// MessageId: MSG_CHK_LOGFILE_NOT_NTFS
//
// MessageText:
//
//  Windows can set log file size on NTFS volumes only.
//
#define MSG_CHK_LOGFILE_NOT_NTFS         0x00000453L

//
// MessageId: MSG_CHK_BAD_DRIVE_PATH_FILENAME
//
// MessageText:
//
//  The drive, the path, or the file name is not valid.
//
#define MSG_CHK_BAD_DRIVE_PATH_FILENAME  0x00000454L

//
// MessageId: MSG_KILOBYTES_IN_USER_FILES
//
// MessageText:
//
//  %1 KB in %2 files.
//
#define MSG_KILOBYTES_IN_USER_FILES      0x00000455L

//
// MessageId: MSG_KILOBYTES_IN_DIRECTORIES
//
// MessageText:
//
//  %1 KB in %2 folders.
//
#define MSG_KILOBYTES_IN_DIRECTORIES     0x00000456L

//
// MessageId: MSG_KILOBYTES_IN_HIDDEN_FILES
//
// MessageText:
//
//  %1 KB in %2 hidden files.
//
#define MSG_KILOBYTES_IN_HIDDEN_FILES    0x00000457L

//
// MessageId: MSG_KILOBYTES_IN_WOULD_BE_RECOVERED_FILES
//
// MessageText:
//
//  %1 KB in %2 recoverable files.
//
#define MSG_KILOBYTES_IN_WOULD_BE_RECOVERED_FILES 0x00000458L

//
// MessageId: MSG_KILOBYTES_IN_RECOVERED_FILES
//
// MessageText:
//
//  %1 KB in %2 recovered files.
//
#define MSG_KILOBYTES_IN_RECOVERED_FILES 0x00000459L

//
// MessageId: MSG_CHK_ABORT_AUTOCHK
//
// MessageText:
//
//  To skip disk checking, press any key within %1 second(s). %r%0
//
#define MSG_CHK_ABORT_AUTOCHK            0x0000045AL

//
// MessageId: MSG_CHK_AUTOCHK_ABORTED
//
// MessageText:
//
//  Disk checking has been cancelled.                       %b
//
#define MSG_CHK_AUTOCHK_ABORTED          0x0000045BL

//
// MessageId: MSG_CHK_AUTOCHK_RESUMED
//
// MessageText:
//
//  Windows will now check the disk.                        %b
//
#define MSG_CHK_AUTOCHK_RESUMED          0x0000045CL

//
// MessageId: MSG_KILOBYTES_FREED
//
// MessageText:
//
//  %1 KB of free disk space added.
//
#define MSG_KILOBYTES_FREED              0x0000045DL

//
// MessageId: MSG_KILOBYTES_WOULD_BE_FREED
//
// MessageText:
//
//  %1 KB of free disk space would be added.
//
#define MSG_KILOBYTES_WOULD_BE_FREED     0x0000045EL

//
// MessageId: MSG_CHK_SKIP_INDEX_NOT_NTFS
//
// MessageText:
//
//  The /I option functions only on NTFS volumes.
//
#define MSG_CHK_SKIP_INDEX_NOT_NTFS      0x0000045FL

//
// MessageId: MSG_CHK_SKIP_CYCLE_NOT_NTFS
//
// MessageText:
//
//  The /C option functions only on NTFS volumes.
//
#define MSG_CHK_SKIP_CYCLE_NOT_NTFS      0x00000460L

//
// MessageId: MSG_CHK_AUTOCHK_COMPLETE
//
// MessageText:
//
//  Windows has finished checking the disk.
//
#define MSG_CHK_AUTOCHK_COMPLETE         0x00000461L

//
// MessageId: MSG_CHK_AUTOCHK_SKIP_WARNING
//
// MessageText:
//
//  
//  One of your disks needs to be checked for consistency. You
//  may cancel the disk check, but it is strongly recommended
//  that you continue.
//
#define MSG_CHK_AUTOCHK_SKIP_WARNING     0x00000462L

//
// MessageId: MSG_CHK_USER_AUTOCHK_SKIP_WARNING
//
// MessageText:
//
//  
//  A disk check has been scheduled.
//
#define MSG_CHK_USER_AUTOCHK_SKIP_WARNING 0x00000463L

//
// MessageId: MSG_CHK_UNABLE_TO_TELL_IF_SYSTEM_DRIVE
//
// MessageText:
//
//  Windows was unable to determine if the specified volume is a system volume.
//
#define MSG_CHK_UNABLE_TO_TELL_IF_SYSTEM_DRIVE 0x00000464L

//
// MessageId: MSG_CHK_NO_PROBLEM_FOUND
//
// MessageText:
//
//  Windows has checked the file system and found no problems.
//
#define MSG_CHK_NO_PROBLEM_FOUND         0x00000465L

//
// MessageId: MSG_CHK_ERRORS_FIXED
//
// MessageText:
//
//  Windows has made corrections to the file system.
//
#define MSG_CHK_ERRORS_FIXED             0x00000466L

//
// MessageId: MSG_CHK_NEED_F_PARAMETER
//
// MessageText:
//
//  Windows found problems with the file system.
//  Run CHKDSK with the /F (fix) option to correct these.
//
#define MSG_CHK_NEED_F_PARAMETER         0x00000467L

//
// MessageId: MSG_CHK_ERRORS_NOT_FIXED
//
// MessageText:
//
//  Windows found problems with the file system that could not be corrected.
//
#define MSG_CHK_ERRORS_NOT_FIXED         0x00000468L

//
// MessageId: MSG_CHK_PRE_RELEASE_NOTICE
//
// MessageText:
//
//  THIS IS AN EXPERIMENTAL VERSION OF CHKDSK.
//  PLEASE USE IT ACCORDING TO THE GIVEN INSTRUCTIONS.
//
#define MSG_CHK_PRE_RELEASE_NOTICE       0x00000469L

//
// MessageId: MSG_CHK_ALGORITHM_AND_SKIP_INDEX_SPECIFIED
//
// MessageText:
//
//  The /I and /I:passes cannot be specified at the same time.
//
#define MSG_CHK_ALGORITHM_AND_SKIP_INDEX_SPECIFIED 0x0000046AL

//
// MessageId: MSG_CHK_INCORRECT_ALGORITHM_VALUE
//
// MessageText:
//
//  The number of passes specified through /I is invalid.
//
#define MSG_CHK_INCORRECT_ALGORITHM_VALUE 0x0000046BL

//
// MessageId: MSG_CHK_WRITE_PROTECTED
//
// MessageText:
//
//  Windows cannot run disk checking on this volume because it is write protected.
//
#define MSG_CHK_WRITE_PROTECTED          0x0000046CL

//
// MessageId: MSG_CHK_NO_MULTI_THREAD
//
// MessageText:
//
//  Windows cannot run disk checking on more than one volume of the same file system.
//  To do so, please run CHKDSK from the command line.
//
#define MSG_CHK_NO_MULTI_THREAD          0x0000046DL

//
// MessageId: MSG_CHK_OUTPUT_LOG_ERROR
//
// MessageText:
//
//  Error in writing the output log.
//
#define MSG_CHK_OUTPUT_LOG_ERROR         0x0000046EL

//-----------------------
//
// DOS 5 Format messages.
//
//-----------------------
//
// MessageId: MSG_PERCENT_COMPLETE
//
// MessageText:
//
//  %1 percent completed.               %r%0
//
#define MSG_PERCENT_COMPLETE             0x000007D0L

//
// MessageId: MSG_PERCENT_COMPLETE2
//
// MessageText:
//
//  %1 percent completed.%2             %r%0
//
#define MSG_PERCENT_COMPLETE2            0x000007D4L

//
// MessageId: MSG_FORMAT_COMPLETE
//
// MessageText:
//
//  Format complete.                        %b
//
#define MSG_FORMAT_COMPLETE              0x000007D1L

//
// MessageId: MSG_INSERT_DISK
//
// MessageText:
//
//  Insert new disk for drive %1
//
#define MSG_INSERT_DISK                  0x000007D2L

//
// MessageId: MSG_REINSERT_DISKETTE
//
// MessageText:
//
//  Reinsert disk for drive %1:
//
#define MSG_REINSERT_DISKETTE            0x000007D3L

//
// MessageId: MSG_BAD_IOCTL
//
// MessageText:
//
//  Error in IOCTL call.
//
#define MSG_BAD_IOCTL                    0x000007D6L

//
// MessageId: MSG_CANT_DASD
//
// MessageText:
//
//  Cannot open volume for direct access.
//
#define MSG_CANT_DASD                    0x000007D7L

//
// MessageId: MSG_CANT_WRITE_FAT
//
// MessageText:
//
//  Error writing File Allocation Table (FAT).
//
#define MSG_CANT_WRITE_FAT               0x000007D8L

//
// MessageId: MSG_CANT_WRITE_ROOT_DIR
//
// MessageText:
//
//  Error writing folder.
//
#define MSG_CANT_WRITE_ROOT_DIR          0x000007D9L

//
// MessageId: MSG_FORMAT_NO_NETWORK
//
// MessageText:
//
//  Cannot format a network drive.
//
#define MSG_FORMAT_NO_NETWORK            0x000007DCL

//
// MessageId: MSG_UNSUPPORTED_PARAMETER
//
// MessageText:
//
//  Parameters not supported.
//
#define MSG_UNSUPPORTED_PARAMETER        0x000007DDL

//
// MessageId: MSG_UNUSABLE_DISK
//
// MessageText:
//
//  Invalid media or Track 0 bad - disk unusable.
//
#define MSG_UNUSABLE_DISK                0x000007E0L

//
// MessageId: MSG_BAD_DIR_READ
//
// MessageText:
//
//  Error reading folder %1.
//
#define MSG_BAD_DIR_READ                 0x000007E2L

//
// MessageId: MSG_PRESS_ENTER_WHEN_READY
//
// MessageText:
//
//  and press ENTER when ready... %0
//
#define MSG_PRESS_ENTER_WHEN_READY       0x000007E3L

//
// MessageId: MSG_ENTER_CURRENT_LABEL
//
// MessageText:
//
//  Enter current volume label for drive %1 %0
//
#define MSG_ENTER_CURRENT_LABEL          0x000007E5L

//
// MessageId: MSG_INCOMPATIBLE_PARAMETERS_FOR_FIXED
//
// MessageText:
//
//  Parameters incompatible with fixed disk.
//
#define MSG_INCOMPATIBLE_PARAMETERS_FOR_FIXED 0x000007E6L

//
// MessageId: MSG_READ_PARTITION_TABLE
//
// MessageText:
//
//  Error reading partition table.
//
#define MSG_READ_PARTITION_TABLE         0x000007E7L

//
// MessageId: MSG_NOT_SUPPORTED_BY_DRIVE
//
// MessageText:
//
//  Parameters not supported by drive.
//
#define MSG_NOT_SUPPORTED_BY_DRIVE       0x000007ECL

//
// MessageId: MSG_2029
//
// MessageText:
//
//  
//
#define MSG_2029                         0x000007EDL

//
// MessageId: MSG_2030
//
// MessageText:
//
//  
//  
//
#define MSG_2030                         0x000007EEL

//
// MessageId: MSG_INSERT_DOS_DISK
//
// MessageText:
//
//  Insert Windows XP disk in drive %1:
//
#define MSG_INSERT_DOS_DISK              0x000007EFL

//
// MessageId: MSG_WARNING_FORMAT
//
// MessageText:
//
//  
//  WARNING, ALL DATA ON NON-REMOVABLE DISK
//  DRIVE %1 WILL BE LOST!
//  Proceed with Format (Y/N)? %0
//
#define MSG_WARNING_FORMAT               0x000007F0L

//
// MessageId: MSG_FORMAT_ANOTHER
//
// MessageText:
//
//  
//  Format another (Y/N)? %0
//
#define MSG_FORMAT_ANOTHER               0x000007F1L

//
// MessageId: MSG_WRITE_PARTITION_TABLE
//
// MessageText:
//
//  Error writing partition table.
//
#define MSG_WRITE_PARTITION_TABLE        0x000007F3L

//
// MessageId: MSG_INCOMPATIBLE_PARAMETERS
//
// MessageText:
//
//  Parameters not compatible.
//
#define MSG_INCOMPATIBLE_PARAMETERS      0x000007F4L

//
// MessageId: MSG_AVAILABLE_ALLOCATION_UNITS
//
// MessageText:
//
//  %1 allocation units available on disk.
//
#define MSG_AVAILABLE_ALLOCATION_UNITS   0x000007F5L

//
// MessageId: MSG_ALLOCATION_UNIT_SIZE
//
// MessageText:
//
//  
//  %1 bytes in each allocation unit.
//
#define MSG_ALLOCATION_UNIT_SIZE         0x000007F6L

//
// MessageId: MSG_PARAMETER_TWICE
//
// MessageText:
//
//  Same parameter entered twice.
//
#define MSG_PARAMETER_TWICE              0x000007F8L

//
// MessageId: MSG_NEED_BOTH_T_AND_N
//
// MessageText:
//
//  Must enter both /t and /n parameters.
//
#define MSG_NEED_BOTH_T_AND_N            0x000007F9L

//
// MessageId: MSG_2042
//
// MessageText:
//
//  Trying to recover allocation unit %1.                          %0
//
#define MSG_2042                         0x000007FAL

//
// MessageId: MSG_NO_LABEL_WITH_8
//
// MessageText:
//
//  Volume label is not supported with /8 parameter.
//
#define MSG_NO_LABEL_WITH_8              0x000007FFL

//
// MessageId: MSG_FMT_NO_MEMORY
//
// MessageText:
//
//  Insufficient memory.
//
#define MSG_FMT_NO_MEMORY                0x00000801L

//
// MessageId: MSG_QUICKFMT_ANOTHER
//
// MessageText:
//
//  
//  QuickFormat another (Y/N)? %0
//
#define MSG_QUICKFMT_ANOTHER             0x00000802L

//
// MessageId: MSG_CANT_QUICKFMT
//
// MessageText:
//
//  Invalid existing format.
//  This disk cannot be QuickFormatted.
//  Proceed with unconditional format (Y/N)? %0
//
#define MSG_CANT_QUICKFMT                0x00000804L

//
// MessageId: MSG_FORMATTING_KB
//
// MessageText:
//
//  Formatting %1K
//
#define MSG_FORMATTING_KB                0x00000805L

//
// MessageId: MSG_FORMATTING_MB
//
// MessageText:
//
//  Formatting %1M
//
#define MSG_FORMATTING_MB                0x00000806L

//
// MessageId: MSG_FORMATTING_DOT_MB
//
// MessageText:
//
//  Formatting %1.%2M
//
#define MSG_FORMATTING_DOT_MB            0x00000807L

//
// MessageId: MSG_VERIFYING_KB
//
// MessageText:
//
//  Verifying %1K
//
#define MSG_VERIFYING_KB                 0x00000809L

//
// MessageId: MSG_VERIFYING_MB
//
// MessageText:
//
//  Verifying %1M
//
#define MSG_VERIFYING_MB                 0x0000080AL

//
// MessageId: MSG_VERIFYING_DOT_MB
//
// MessageText:
//
//  Verifying %1.%2M
//
#define MSG_VERIFYING_DOT_MB             0x0000080BL

//
// MessageId: MSG_2060
//
// MessageText:
//
//  Saving UNFORMAT information.
//
#define MSG_2060                         0x0000080CL

//
// MessageId: MSG_2061
//
// MessageText:
//
//  Checking existing disk format.
//
#define MSG_2061                         0x0000080DL

//
// MessageId: MSG_QUICKFORMATTING_KB
//
// MessageText:
//
//  QuickFormatting %1K
//
#define MSG_QUICKFORMATTING_KB           0x0000080EL

//
// MessageId: MSG_QUICKFORMATTING_MB
//
// MessageText:
//
//  QuickFormatting %1M
//
#define MSG_QUICKFORMATTING_MB           0x0000080FL

//
// MessageId: MSG_QUICKFORMATTING_DOT_MB
//
// MessageText:
//
//  QuickFormatting %1.%2M
//
#define MSG_QUICKFORMATTING_DOT_MB       0x00000810L

//
// MessageId: MSG_FORMAT_INFO
//
// MessageText:
//
//  Formats a disk for use with Windows XP.
//  
//
#define MSG_FORMAT_INFO                  0x00000811L

//
// MessageId: MSG_FORMAT_COMMAND_LINE_1
//
// MessageText:
//
//  FORMAT volume [/FS:file-system] [/V:label] [/Q] [/A:size] [/C] [/X]
//  FORMAT volume [/V:label] [/Q] [/F:size]
//
#define MSG_FORMAT_COMMAND_LINE_1        0x00000812L

//
// MessageId: MSG_FORMAT_COMMAND_LINE_2
//
// MessageText:
//
//  FORMAT volume [/V:label] [/Q] [/T:tracks /N:sectors]
//
#define MSG_FORMAT_COMMAND_LINE_2        0x00000813L

//
// MessageId: MSG_FORMAT_COMMAND_LINE_3
//
// MessageText:
//
//  FORMAT volume [/V:label] [/Q]
//
#define MSG_FORMAT_COMMAND_LINE_3        0x00000814L

//
// MessageId: MSG_FORMAT_COMMAND_LINE_4
//
// MessageText:
//
//  FORMAT volume [/Q]
//  
//    volume          Specifies the drive letter (followed by a colon),
//                    mount point, or volume name.
//    /FS:filesystem  Specifies the type of the file system (FAT, FAT32, or NTFS).
//
#define MSG_FORMAT_COMMAND_LINE_4        0x00000815L

//
// MessageId: MSG_FORMAT_SLASH_V
//
// MessageText:
//
//    /V:label        Specifies the volume label.
//
#define MSG_FORMAT_SLASH_V               0x00000816L

//
// MessageId: MSG_FORMAT_SLASH_Q
//
// MessageText:
//
//    /Q              Performs a quick format.
//
#define MSG_FORMAT_SLASH_Q               0x00000817L

//
// MessageId: MSG_FORMAT_SLASH_C
//
// MessageText:
//
//    /C              NTFS only: Files created on the new volume will be compressed
//                    by default.
//
#define MSG_FORMAT_SLASH_C               0x00000818L

//
// MessageId: MSG_FORMAT_SLASH_F
//
// MessageText:
//
//    /A:size         Overrides the default allocation unit size. Default settings
//                    are strongly recommended for general use.
//                    NTFS supports 512, 1024, 2048, 4096, 8192, 16K, 32K, 64K.
//                    FAT supports 512, 1024, 2048, 4096, 8192, 16K, 32K, 64K,
//                    (128K, 256K for sector size > 512 bytes).
//                    FAT32 supports 512, 1024, 2048, 4096, 8192, 16K, 32K, 64K,
//                    (128K, 256K for sector size > 512 bytes).
//  
//                    Note that the FAT and FAT32 files systems impose the
//                    following restrictions on the number of clusters on a volume:
//  
//                    FAT: Number of clusters <= 65526
//                    FAT32: 65526 < Number of clusters < 4177918
//  
//                    Format will immediately stop processing if it decides that
//                    the above requirements cannot be met using the specified
//                    cluster size.
//  
//                    NTFS compression is not supported for allocation unit sizes
//                    above 4096.
//  
//    /F:size         Specifies the size of the floppy disk to format (1.44)
//
#define MSG_FORMAT_SLASH_F               0x00000819L

//
// MessageId: MSG_FORMAT_SUPPORTED_SIZES
//
// MessageText:
//
//                    180, 320, 360, 640, 720, 1.2, 1.23, 1.44, 2.88, or 20.8).
//
#define MSG_FORMAT_SUPPORTED_SIZES       0x0000081AL

//
// MessageId: MSG_WRONG_CURRENT_LABEL
//
// MessageText:
//
//  An incorrect volume label was entered for this drive.
//
#define MSG_WRONG_CURRENT_LABEL          0x0000081BL

//
// MessageId: MSG_FORMAT_SLASH_T
//
// MessageText:
//
//    /T:tracks       Specifies the number of tracks per disk side.
//
#define MSG_FORMAT_SLASH_T               0x0000081DL

//
// MessageId: MSG_FORMAT_SLASH_N
//
// MessageText:
//
//    /N:sectors      Specifies the number of sectors per track.
//
#define MSG_FORMAT_SLASH_N               0x0000081EL

//
// MessageId: MSG_FORMAT_SLASH_1
//
// MessageText:
//
//    /1              Formats a single side of a floppy disk.
//
#define MSG_FORMAT_SLASH_1               0x0000081FL

//
// MessageId: MSG_FORMAT_SLASH_4
//
// MessageText:
//
//    /4              Formats a 5.25-inch 360K floppy disk in a
//                    high-density drive.
//
#define MSG_FORMAT_SLASH_4               0x00000820L

//
// MessageId: MSG_FORMAT_SLASH_8
//
// MessageText:
//
//    /8              Formats eight sectors per track.
//
#define MSG_FORMAT_SLASH_8               0x00000821L

//
// MessageId: MSG_FORMAT_SLASH_X
//
// MessageText:
//
//    /X              Forces the volume to dismount first if necessary.  All opened
//                    handles to the volume would no longer be valid.
//
#define MSG_FORMAT_SLASH_X               0x00000822L

//
// MessageId: MSG_FORMAT_NO_CDROM
//
// MessageText:
//
//  Cannot format a CD-ROM drive.
//
#define MSG_FORMAT_NO_CDROM              0x00000823L

//
// MessageId: MSG_FORMAT_NO_RAMDISK
//
// MessageText:
//
//  Cannot format a RAM DISK drive.
//
#define MSG_FORMAT_NO_RAMDISK            0x00000824L

//
// MessageId: MSG_FORMAT_PLEASE_USE_FS_SWITCH
//
// MessageText:
//
//  Please use the /FS switch to specify the file system
//  you wish to use on this volume.
//
#define MSG_FORMAT_PLEASE_USE_FS_SWITCH  0x00000826L

//
// MessageId: MSG_FORMAT_FAILED
//
// MessageText:
//
//  Format failed.
//
#define MSG_FORMAT_FAILED                0x00000827L

//
// MessageId: MSG_FMT_WRITE_PROTECTED_MEDIA
//
// MessageText:
//
//  Cannot format.  This volume is write protected.
//
#define MSG_FMT_WRITE_PROTECTED_MEDIA    0x00000828L

//
// MessageId: MSG_FMT_INSTALL_FILE_SYSTEM
//
// MessageText:
//
//  
//  WARNING!  The %1 file system is not enabled.
//  Would you like to enable it (Y/N)? %0
//
#define MSG_FMT_INSTALL_FILE_SYSTEM      0x00000829L

//
// MessageId: MSG_FMT_FILE_SYSTEM_INSTALLED
//
// MessageText:
//
//  
//  The file system will be enabled when you restart the system.
//
#define MSG_FMT_FILE_SYSTEM_INSTALLED    0x0000082AL

//
// MessageId: MSG_FMT_CANT_INSTALL_FILE_SYSTEM
//
// MessageText:
//
//  
//  FORMAT cannot enable the file system.
//
#define MSG_FMT_CANT_INSTALL_FILE_SYSTEM 0x0000082BL

//
// MessageId: MSG_FMT_VOLUME_TOO_SMALL
//
// MessageText:
//
//  The volume is too small for the specified file system.
//
#define MSG_FMT_VOLUME_TOO_SMALL         0x0000082CL

//
// MessageId: MSG_FMT_CREATING_FILE_SYSTEM
//
// MessageText:
//
//  Creating file system structures.
//
#define MSG_FMT_CREATING_FILE_SYSTEM     0x0000082DL

//
// MessageId: MSG_FMT_VARIABLE_CLUSTERS_NOT_SUPPORTED
//
// MessageText:
//
//  %1 FORMAT does not support user selected allocation unit sizes.
//
#define MSG_FMT_VARIABLE_CLUSTERS_NOT_SUPPORTED 0x0000082EL

//
// MessageId: MSG_DEVICE_BUSY
//
// MessageText:
//
//  The device is busy.
//
#define MSG_DEVICE_BUSY                  0x00000830L

//
// MessageId: MSG_FMT_DMF_NOT_SUPPORTED_ON_288_DRIVES
//
// MessageText:
//
//  The specified format cannot be mastered on 2.88MB drives.
//
#define MSG_FMT_DMF_NOT_SUPPORTED_ON_288_DRIVES 0x00000831L

//
// MessageId: MSG_HPFS_NO_FORMAT
//
// MessageText:
//
//  FORMAT does not support the HPFS file system type.
//
#define MSG_HPFS_NO_FORMAT               0x00000832L

//
// MessageId: MSG_FMT_ALLOCATION_SIZE_CHANGED
//
// MessageText:
//
//  Allocation unit size changed to %1 bytes.
//
#define MSG_FMT_ALLOCATION_SIZE_CHANGED  0x00000833L

//
// MessageId: MSG_FMT_ALLOCATION_SIZE_EXCEEDED
//
// MessageText:
//
//  Allocation unit size must be less than or equal to 64K.
//
#define MSG_FMT_ALLOCATION_SIZE_EXCEEDED 0x00000834L

//
// MessageId: MSG_FMT_TOO_MANY_CLUSTERS
//
// MessageText:
//
//  Number of clusters exceeds 32 bits.
//
#define MSG_FMT_TOO_MANY_CLUSTERS        0x00000835L

//
// MessageId: MSG_FMT_INVALID_SECTOR_COUNT
//
// MessageText:
//
//  Cannot determine the number of sectors on this volume.
//
#define MSG_FMT_INVALID_SECTOR_COUNT     0x00000836L

//
// MessageId: MSG_CONV_PAUSE_BEFORE_REBOOT
//
// MessageText:
//
//  
//  Preinstallation completed successfully.  Press any key to
//  shut down/reboot.
//
#define MSG_CONV_PAUSE_BEFORE_REBOOT     0x0000089BL

//
// MessageId: MSG_CONV_WILL_REBOOT
//
// MessageText:
//
//  
//  Convert will take some time to process the files on the volume.
//  When this phase of conversion is complete, the computer will restart.
//
#define MSG_CONV_WILL_REBOOT             0x0000089CL

//
// MessageId: MSG_FMT_FAT_ENTRY_SIZE
//
// MessageText:
//
//  %1 bits in each FAT entry.
//
#define MSG_FMT_FAT_ENTRY_SIZE           0x0000089DL

//
// MessageId: MSG_FMT_CLUSTER_SIZE_MISMATCH
//
// MessageText:
//
//  WARNING!  The cluster size chosen by the system is %1 bytes which
//  differs from the specified cluster size.
//  Proceed with Format using the cluster size chosen by the
//  system (Y/N)? %0
//
#define MSG_FMT_CLUSTER_SIZE_MISMATCH    0x0000089EL

//
// MessageId: MSG_FMT_CLUSTER_SIZE_MISMATCH_WARNING
//
// MessageText:
//
//  WARNING!  The cluster size chosen by the system is %1 bytes which
//  differs from the specified cluster size.
//
#define MSG_FMT_CLUSTER_SIZE_MISMATCH_WARNING 0x0000089FL

//
// MessageId: MSG_FMT_CLUSTER_SIZE_TOO_BIG
//
// MessageText:
//
//  The specified cluster size is too big for %1.
//
#define MSG_FMT_CLUSTER_SIZE_TOO_BIG     0x000008A0L

//
// MessageId: MSG_FMT_VOL_TOO_BIG
//
// MessageText:
//
//  The volume is too big for %1.
//
#define MSG_FMT_VOL_TOO_BIG              0x000008A1L

//
// MessageId: MSG_FMT_VOL_TOO_SMALL
//
// MessageText:
//
//  The volume is too small for %1.
//
#define MSG_FMT_VOL_TOO_SMALL            0x000008A2L

//
// MessageId: MSG_FMT_ROOTDIR_WRITE_FAILED
//
// MessageText:
//
//  Failed to write to the root folder.
//
#define MSG_FMT_ROOTDIR_WRITE_FAILED     0x000008A3L

//
// MessageId: MSG_FMT_INIT_LABEL_FAILED
//
// MessageText:
//
//  Failed to initialize the volume label.
//
#define MSG_FMT_INIT_LABEL_FAILED        0x000008A4L

//
// MessageId: MSG_FMT_INITIALIZING_FATS
//
// MessageText:
//
//  Initializing the File Allocation Table (FAT)...
//
#define MSG_FMT_INITIALIZING_FATS        0x000008A5L

//
// MessageId: MSG_FMT_CLUSTER_SIZE_64K
//
// MessageText:
//
//  WARNING!  The cluster size for this volume, 64K bytes, may cause
//  application compatibility problems, particularly with setup applications.
//  The volume must be less than 2048 MB in size to change this if the
//  default cluster size is being used.
//  Proceed with Format using a 64K cluster (Y/N)? %0
//
#define MSG_FMT_CLUSTER_SIZE_64K         0x000008A6L

//
// MessageId: MSG_FMT_CLUSTER_SIZE_64K_WARNING
//
// MessageText:
//
//  WARNING!  The cluster size for this volume, 64K bytes, may cause
//  application compatibility problems, particularly with setup applications.
//  The volume must be less than 2048 MB in size to change this if the
//  default cluster size is being used.
//
#define MSG_FMT_CLUSTER_SIZE_64K_WARNING 0x000008A7L

//
// MessageId: MSG_FMT_BAD_SECTORS
//
// MessageText:
//
//  Environmental variable FORMAT_SECTORS error.
//
#define MSG_FMT_BAD_SECTORS              0x000008A8L

//
// MessageId: MSG_FMT_FORCE_DISMOUNT_PROMPT
//
// MessageText:
//
//  
//  Format cannot run because the volume is in use by another
//  process.  Format may run if this volume is dismounted first.
//  ALL OPENED HANDLES TO THIS VOLUME WOULD THEN BE INVALID.
//  Would you like to force a dismount on this volume? (Y/N) %0
//
#define MSG_FMT_FORCE_DISMOUNT_PROMPT    0x000008A9L

//
// MessageId: MSG_FORMAT_NO_MEDIA_IN_DRIVE
//
// MessageText:
//
//  There is no media in the drive.
//
#define MSG_FORMAT_NO_MEDIA_IN_DRIVE     0x000008AAL

//
// MessageId: MSG_FMT_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH
//
// MessageText:
//
//  The given volume name does not have a mount point or drive letter.
//
#define MSG_FMT_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH 0x000008ABL

//
// MessageId: MSG_FMT_INVALID_DRIVE_SPEC
//
// MessageText:
//
//  Invalid drive specification.
//
#define MSG_FMT_INVALID_DRIVE_SPEC       0x000008ACL

//
// MessageId: MSG_CONV_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH
//
// MessageText:
//
//  The given volume name does not have a mount point or drive letter.
//
#define MSG_CONV_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH 0x000008ADL

//
// MessageId: MSG_FMT_CLUSTER_SIZE_TOO_SMALL_MIN
//
// MessageText:
//
//  The specified cluster size is too small. The minimum valid
//  cluster size value for this drive is %1.
//
#define MSG_FMT_CLUSTER_SIZE_TOO_SMALL_MIN 0x000008AEL

//
// MessageId: MSG_FMT_FAT32_NO_FLOPPIES
//
// MessageText:
//
//  Floppy disk is too small to hold the FAT32 file system.
//
#define MSG_FMT_FAT32_NO_FLOPPIES        0x000008AFL

//
// MessageId: MSG_FMT_NO_NTFS_ALLOWED
//
// MessageText:
//
//  NTFS file system is not supported on this device.
//
#define MSG_FMT_NO_NTFS_ALLOWED          0x000008B0L

//
// MessageId: MSG_FMT_CLUSTER_SIZE_TOO_SMALL
//
// MessageText:
//
//  The specified cluster size is too small for %1.
//
#define MSG_FMT_CLUSTER_SIZE_TOO_SMALL   0x000008B1L

//
// MessageId: MSG_FMT_SECTORS
//
// MessageText:
//
//  Set number of sectors on drive to %1.
//
#define MSG_FMT_SECTORS                  0x000008B2L

//
// MessageId: MSG_FORMAT_SLASH_R
//
// MessageText:
//
//    /R:revision     UDF only: Forces the format to a specific UDF version
//                    (2.00, 2.01).  The default revision is 2.01.
//
#define MSG_FORMAT_SLASH_R               0x000008B3L

//
// MessageId: MSG_FMT_QUICK_FORMAT_NOT_AVAILABLE
//
// MessageText:
//
//  This device cannot be formatted in quick mode.
//  Proceed with unconditional format (Y/N)? %0
//
#define MSG_FMT_QUICK_FORMAT_NOT_AVAILABLE 0x000008B4L

//
// MessageId: MSG_FMT_CANNOT_TALK_TO_DEVICE
//
// MessageText:
//
//  Communication failure with device.  Format failed.
//
#define MSG_FMT_CANNOT_TALK_TO_DEVICE    0x000008B5L

//
// MessageId: MSG_FMT_TIMEOUT
//
// MessageText:
//
//  Format failed due to timeout.
//
#define MSG_FMT_TIMEOUT                  0x000008B6L

//
// MessageId: MSG_FMT_QUICK_FORMAT_NOT_AVAILABLE_WARNING
//
// MessageText:
//
//  This device cannot be formatted in quick mode.
//  An unconditional format will be performed.
//
#define MSG_FMT_QUICK_FORMAT_NOT_AVAILABLE_WARNING 0x000008B7L

//
// MessageId: MSG_FMT_SONY_MEM_STICK_ON_FLOPPY_NOT_ALLOWED
//
// MessageText:
//
//  This device requires proprietary format utility from manufacturer.
//
#define MSG_FMT_SONY_MEM_STICK_ON_FLOPPY_NOT_ALLOWED 0x000008B8L

//
// MessageId: MSG_FMT_CLUS_SIZE_NOT_ALLOWED
//
// MessageText:
//
//  WARNING!  This device may not work properly if /A option is specified.
//  Proceed (Y/N)? %0
//
#define MSG_FMT_CLUS_SIZE_NOT_ALLOWED    0x000008B9L

//
// MessageId: MSG_FMT_CLUS_SIZE_NOT_ALLOWED_WARNING
//
// MessageText:
//
//  WARNING!  This device may not work properly since /A option is specified.
//
#define MSG_FMT_CLUS_SIZE_NOT_ALLOWED_WARNING 0x000008BAL

//
// MessageId: MSG_FMT_UNABLE_TO_CREATE_MEMSTICK_FILE
//
// MessageText:
//
//  Unable to create the MEMSTICK.IND file.
//
#define MSG_FMT_UNABLE_TO_CREATE_MEMSTICK_FILE 0x000008BBL

//
// MessageId: MSG_FMT_NO_FAT32_ALLOWED
//
// MessageText:
//
//  FAT32 file system is not supported on this device.
//
#define MSG_FMT_NO_FAT32_ALLOWED         0x000008BCL

//
// MessageId: MSG_FMT_INVALID_SLASH_F_OPTION
//
// MessageText:
//
//  This device does not support the /F option.
//
#define MSG_FMT_INVALID_SLASH_F_OPTION   0x000008BDL

//
// MessageId: MSG_FMT_SYSTEM_PARTITION_NOT_ALLOWED
//
// MessageText:
//
//  System Partition is not allowed to be formatted.
//
#define MSG_FMT_SYSTEM_PARTITION_NOT_ALLOWED 0x000008BEL

//
// MessageId: MSG_FMT_NTFS_NOT_SUPPORTED
//
// MessageText:
//
//  NTFS file system is not supported on this device optimized for removal.
//  To change the way this device is optimized, select the Policies tab in
//  the device's property sheet.
//
#define MSG_FMT_NTFS_NOT_SUPPORTED       0x000008BFL

//----------------------
//
// Common ulib messages.
//
//----------------------
//
// MessageId: MSG_CANT_LOCK_THE_DRIVE
//
// MessageText:
//
//  Cannot lock the drive.  The volume is still in use.
//
#define MSG_CANT_LOCK_THE_DRIVE          0x00000BB8L

//
// MessageId: MSG_CANT_READ_BOOT_SECTOR
//
// MessageText:
//
//  Cannot read boot sector.
//
#define MSG_CANT_READ_BOOT_SECTOR        0x00000BBAL

//
// MessageId: MSG_VOLUME_SERIAL_NUMBER
//
// MessageText:
//
//  Volume Serial Number is %1-%2
//
#define MSG_VOLUME_SERIAL_NUMBER         0x00000BBBL

//
// MessageId: MSG_VOLUME_LABEL_PROMPT
//
// MessageText:
//
//  Volume label (11 characters, ENTER for none)? %0
//
#define MSG_VOLUME_LABEL_PROMPT          0x00000BBCL

//
// MessageId: MSG_INVALID_LABEL_CHARACTERS
//
// MessageText:
//
//  Invalid characters in volume label
//
#define MSG_INVALID_LABEL_CHARACTERS     0x00000BBDL

//
// MessageId: MSG_CANT_READ_ANY_FAT
//
// MessageText:
//
//  There are no readable file allocation tables (FAT).
//
#define MSG_CANT_READ_ANY_FAT            0x00000BBEL

//
// MessageId: MSG_SOME_FATS_UNREADABLE
//
// MessageText:
//
//  Some file allocation tables (FAT) are unreadable.
//
#define MSG_SOME_FATS_UNREADABLE         0x00000BBFL

//
// MessageId: MSG_CANT_WRITE_BOOT_SECTOR
//
// MessageText:
//
//  Cannot write boot sector.
//
#define MSG_CANT_WRITE_BOOT_SECTOR       0x00000BC0L

//
// MessageId: MSG_SOME_FATS_UNWRITABLE
//
// MessageText:
//
//  Some file allocation tables (FAT) are unwriteable.
//
#define MSG_SOME_FATS_UNWRITABLE         0x00000BC1L

//
// MessageId: MSG_INSUFFICIENT_DISK_SPACE
//
// MessageText:
//
//  Insufficient disk space.
//
#define MSG_INSUFFICIENT_DISK_SPACE      0x00000BC2L

//
// MessageId: MSG_TOTAL_KILOBYTES
//
// MessageText:
//
//  %1 KB total disk space.
//
#define MSG_TOTAL_KILOBYTES              0x00000BC3L

//
// MessageId: MSG_AVAILABLE_KILOBYTES
//
// MessageText:
//
//  %1 KB are available.
//
#define MSG_AVAILABLE_KILOBYTES          0x00000BC4L

//
// MessageId: MSG_NOT_FAT
//
// MessageText:
//
//  Disk not formatted or not FAT.
//
#define MSG_NOT_FAT                      0x00000BC5L

//
// MessageId: MSG_REQUIRED_PARAMETER
//
// MessageText:
//
//  Required parameter missing -
//
#define MSG_REQUIRED_PARAMETER           0x00000BC6L

//
// MessageId: MSG_FILE_SYSTEM_TYPE
//
// MessageText:
//
//  The type of the file system is %1.
//
#define MSG_FILE_SYSTEM_TYPE             0x00000BC7L

//
// MessageId: MSG_NEW_FILE_SYSTEM_TYPE
//
// MessageText:
//
//  The new file system is %1.
//
#define MSG_NEW_FILE_SYSTEM_TYPE         0x00000BC8L

//
// MessageId: MSG_FMT_AN_ERROR_OCCURRED
//
// MessageText:
//
//  An error occurred while running Format.
//
#define MSG_FMT_AN_ERROR_OCCURRED        0x00000BC9L

//
// MessageId: MSG_FS_NOT_SUPPORTED
//
// MessageText:
//
//  %1 is not available for %2 drives.
//
#define MSG_FS_NOT_SUPPORTED             0x00000BCAL

//
// MessageId: MSG_FS_NOT_DETERMINED
//
// MessageText:
//
//  Cannot determine file system of drive %1.
//
#define MSG_FS_NOT_DETERMINED            0x00000BCBL

//
// MessageId: MSG_CANT_DISMOUNT
//
// MessageText:
//
//  Cannot dismount the drive.
//
#define MSG_CANT_DISMOUNT                0x00000BCCL

//
// MessageId: MSG_NOT_FULL_PATH_NAME
//
// MessageText:
//
//  %1 is not a complete name.
//
#define MSG_NOT_FULL_PATH_NAME           0x00000BCDL

//
// MessageId: MSG_YES
//
// MessageText:
//
//  Yes
//
#define MSG_YES                          0x00000BCEL

//
// MessageId: MSG_NO
//
// MessageText:
//
//  No
//
#define MSG_NO                           0x00000BCFL

//
// MessageId: MSG_DISK_NOT_FORMATTED
//
// MessageText:
//
//  Disk is not formatted.
//
#define MSG_DISK_NOT_FORMATTED           0x00000BD0L

//
// MessageId: MSG_NONEXISTENT_DRIVE
//
// MessageText:
//
//  Specified drive does not exist.
//
#define MSG_NONEXISTENT_DRIVE            0x00000BD1L

//
// MessageId: MSG_INVALID_PARAMETER
//
// MessageText:
//
//  Invalid parameter - %1
//
#define MSG_INVALID_PARAMETER            0x00000BD2L

//
// MessageId: MSG_INSUFFICIENT_MEMORY
//
// MessageText:
//
//  Out of memory.
//
#define MSG_INSUFFICIENT_MEMORY          0x00000BD3L

//
// MessageId: MSG_ACCESS_DENIED
//
// MessageText:
//
//  Access denied - %1
//
#define MSG_ACCESS_DENIED                0x00000BD4L

//
// MessageId: MSG_DASD_ACCESS_DENIED
//
// MessageText:
//
//  Access denied.
//
#define MSG_DASD_ACCESS_DENIED           0x00000BD5L

//
// MessageId: MSG_CANT_LOCK_CURRENT_DRIVE
//
// MessageText:
//
//  Cannot lock current drive.
//
#define MSG_CANT_LOCK_CURRENT_DRIVE      0x00000BD6L

//
// MessageId: MSG_INVALID_LABEL
//
// MessageText:
//
//  Invalid volume label
//
#define MSG_INVALID_LABEL                0x00000BD7L

//
// MessageId: MSG_DISK_TOO_LARGE_TO_FORMAT
//
// MessageText:
//
//  The disk is too large to format for the specified file system.
//
#define MSG_DISK_TOO_LARGE_TO_FORMAT     0x00000BD8L

//
// MessageId: MSG_VOLUME_LABEL_NO_MAX
//
// MessageText:
//
//  Volume label (ENTER for none)? %0
//
#define MSG_VOLUME_LABEL_NO_MAX          0x00000BD9L

//
// MessageId: MSG_CHKDSK_ON_REBOOT_PROMPT
//
// MessageText:
//
//  
//  Chkdsk cannot run because the volume is in use by another
//  process.  Would you like to schedule this volume to be
//  checked the next time the system restarts? (Y/N) %0
//
#define MSG_CHKDSK_ON_REBOOT_PROMPT      0x00000BDAL

//
// MessageId: MSG_CHKDSK_CANNOT_SCHEDULE
//
// MessageText:
//
//  
//  Chkdsk could not schedule this volume to be checked
//  the next time the system restarts.
//
#define MSG_CHKDSK_CANNOT_SCHEDULE       0x00000BDBL

//
// MessageId: MSG_CHKDSK_SCHEDULED
//
// MessageText:
//
//  
//  This volume will be checked the next time the system restarts.
//
#define MSG_CHKDSK_SCHEDULED             0x00000BDCL

//
// MessageId: MSG_COMPRESSION_NOT_AVAILABLE
//
// MessageText:
//
//  Compression is not available for %1.
//
#define MSG_COMPRESSION_NOT_AVAILABLE    0x00000BDDL

//
// MessageId: MSG_CANNOT_ENABLE_COMPRESSION
//
// MessageText:
//
//  Cannot enable compression for the volume.
//
#define MSG_CANNOT_ENABLE_COMPRESSION    0x00000BDEL

//
// MessageId: MSG_CANNOT_COMPRESS_HUGE_CLUSTERS
//
// MessageText:
//
//  Compression is not supported on volumes with clusters larger than
//  4096 bytes.
//
#define MSG_CANNOT_COMPRESS_HUGE_CLUSTERS 0x00000BDFL

//
// MessageId: MSG_CANT_UNLOCK_THE_DRIVE
//
// MessageText:
//
//  Cannot unlock the drive.
//
#define MSG_CANT_UNLOCK_THE_DRIVE        0x00000BE0L

//
// MessageId: MSG_CHKDSK_FORCE_DISMOUNT_PROMPT
//
// MessageText:
//
//  
//  Chkdsk cannot run because the volume is in use by another
//  process.  Chkdsk may run if this volume is dismounted first.
//  ALL OPENED HANDLES TO THIS VOLUME WOULD THEN BE INVALID.
//  Would you like to force a dismount on this volume? (Y/N) %0
//
#define MSG_CHKDSK_FORCE_DISMOUNT_PROMPT 0x00000BE1L

//
// MessageId: MSG_VOLUME_DISMOUNTED
//
// MessageText:
//
//  Volume dismounted.  All opened handles to this volume are now invalid.
//
#define MSG_VOLUME_DISMOUNTED            0x00000BE2L

//
// MessageId: MSG_CHKDSK_DISMOUNT_ON_REBOOT_PROMPT
//
// MessageText:
//
//  
//  Chkdsk cannot dismount the volume because it is a system drive or
//  there is an active paging file on it.  Would you like to schedule
//  this volume to be checked the next time the system restarts? (Y/N) %0
//
#define MSG_CHKDSK_DISMOUNT_ON_REBOOT_PROMPT 0x00000BE3L

//
// MessageId: MSG_TOTAL_MEGABYTES
//
// MessageText:
//
//  %1 MB total disk space.
//
#define MSG_TOTAL_MEGABYTES              0x00000BE4L

//
// MessageId: MSG_AVAILABLE_MEGABYTES
//
// MessageText:
//
//  %1 MB are available.
//
#define MSG_AVAILABLE_MEGABYTES          0x00000BE5L

//---------------------
//
// FAT ChkDsk Messages.
//
//---------------------
//
// MessageId: MSG_CHK_ERRORS_IN_FAT
//
// MessageText:
//
//  Errors in file allocation table (FAT) corrected.
//
#define MSG_CHK_ERRORS_IN_FAT            0x00001388L

//
// MessageId: MSG_CHK_EAFILE_HAS_HANDLE
//
// MessageText:
//
//  Extended attribute file has handle.  Handle removed.
//
#define MSG_CHK_EAFILE_HAS_HANDLE        0x00001389L

//
// MessageId: MSG_CHK_EMPTY_EA_FILE
//
// MessageText:
//
//  Extended attribute file contains no extended attributes.  File deleted.
//
#define MSG_CHK_EMPTY_EA_FILE            0x0000138AL

//
// MessageId: MSG_CHK_ERASING_INVALID_LABEL
//
// MessageText:
//
//  Erasing invalid label.
//
#define MSG_CHK_ERASING_INVALID_LABEL    0x0000138BL

//
// MessageId: MSG_CHK_EA_SIZE
//
// MessageText:
//
//  %1 bytes in extended attributes.
//
#define MSG_CHK_EA_SIZE                  0x0000138CL

//
// MessageId: MSG_CHK_CANT_CHECK_EA_LOG
//
// MessageText:
//
//  Unreadable extended attribute header.
//  Cannot check extended attribute log.
//
#define MSG_CHK_CANT_CHECK_EA_LOG        0x0000138DL

//
// MessageId: MSG_CHK_BAD_LOG
//
// MessageText:
//
//  Extended attribute log is unintelligible.
//  Ignore log and continue? (Y/N) %0
//
#define MSG_CHK_BAD_LOG                  0x0000138EL

//
// MessageId: MSG_CHK_UNUSED_EA_PORTION
//
// MessageText:
//
//  Unused, unreadable, or unwriteable portion of extended attribute file removed.
//
#define MSG_CHK_UNUSED_EA_PORTION        0x0000138FL

//
// MessageId: MSG_CHK_EASET_SIZE
//
// MessageText:
//
//  Total size entry for extended attribute set at cluster %1 corrected.
//
#define MSG_CHK_EASET_SIZE               0x00001390L

//
// MessageId: MSG_CHK_EASET_NEED_COUNT
//
// MessageText:
//
//  Need count entry for extended attribute set at cluster %1 corrected.
//
#define MSG_CHK_EASET_NEED_COUNT         0x00001391L

//
// MessageId: MSG_CHK_UNORDERED_EA_SETS
//
// MessageText:
//
//  Extended attribute file is unsorted.
//  Sorting extended attribute file.
//
#define MSG_CHK_UNORDERED_EA_SETS        0x00001392L

//
// MessageId: MSG_CHK_NEED_MORE_HEADER_SPACE
//
// MessageText:
//
//  Insufficient space in extended attribute file for its header.
//  Attempting to allocate more disk space.
//
#define MSG_CHK_NEED_MORE_HEADER_SPACE   0x00001393L

//
// MessageId: MSG_CHK_INSUFFICIENT_DISK_SPACE
//
// MessageText:
//
//  Insufficient disk space to correct disk error.
//  Please free some disk space and run CHKDSK again.
//
#define MSG_CHK_INSUFFICIENT_DISK_SPACE  0x00001394L

//
// MessageId: MSG_CHK_RELOCATED_EA_HEADER
//
// MessageText:
//
//  Bad clusters in extended attribute file header relocated.
//
#define MSG_CHK_RELOCATED_EA_HEADER      0x00001395L

//
// MessageId: MSG_CHK_ERROR_IN_EA_HEADER
//
// MessageText:
//
//  Errors in extended attribute file header corrected.
//
#define MSG_CHK_ERROR_IN_EA_HEADER       0x00001396L

//
// MessageId: MSG_CHK_MORE_THAN_ONE_DOT
//
// MessageText:
//
//  More than one dot entry in folder %1.  Entry removed.
//
#define MSG_CHK_MORE_THAN_ONE_DOT        0x00001397L

//
// MessageId: MSG_CHK_DOT_IN_ROOT
//
// MessageText:
//
//  Dot entry found in root folder.  Entry removed.
//
#define MSG_CHK_DOT_IN_ROOT              0x00001398L

//
// MessageId: MSG_CHK_DOTDOT_IN_ROOT
//
// MessageText:
//
//  Dot-dot entry found in root folder.  Entry removed.
//
#define MSG_CHK_DOTDOT_IN_ROOT           0x00001399L

//
// MessageId: MSG_CHK_ERR_IN_DOT
//
// MessageText:
//
//  Dot entry in folder %1 has incorrect link.  Link corrected.
//
#define MSG_CHK_ERR_IN_DOT               0x0000139AL

//
// MessageId: MSG_CHK_ERR_IN_DOTDOT
//
// MessageText:
//
//  Dot-dot entry in folder %1 has incorrect link.  Link corrected.
//
#define MSG_CHK_ERR_IN_DOTDOT            0x0000139BL

//
// MessageId: MSG_CHK_DELETE_REPEATED_ENTRY
//
// MessageText:
//
//  More than one %1 entry in folder %2.  Entry removed.
//
#define MSG_CHK_DELETE_REPEATED_ENTRY    0x0000139CL

//
// MessageId: MSG_CHK_CYCLE_IN_TREE
//
// MessageText:
//
//  Folder %1 causes cycle in folder structure.
//  Folder entry removed.
//
#define MSG_CHK_CYCLE_IN_TREE            0x0000139DL

//
// MessageId: MSG_CHK_BAD_CLUSTERS_IN_DIR
//
// MessageText:
//
//  Folder %1 has bad clusters.
//  Bad clusters removed from folder.
//
#define MSG_CHK_BAD_CLUSTERS_IN_DIR      0x0000139EL

//
// MessageId: MSG_CHK_BAD_DIR
//
// MessageText:
//
//  Folder %1 is entirely unreadable.
//  Folder entry removed.
//
#define MSG_CHK_BAD_DIR                  0x0000139FL

//
// MessageId: MSG_CHK_FILENAME
//
// MessageText:
//
//  %1
//
#define MSG_CHK_FILENAME                 0x000013A0L

//
// MessageId: MSG_CHK_DIR_TRUNC
//
// MessageText:
//
//  Folder truncated.
//
#define MSG_CHK_DIR_TRUNC                0x000013A1L

//
// MessageId: MSG_CHK_CROSS_LINK_COPY
//
// MessageText:
//
//  Cross link resolved by copying.
//
#define MSG_CHK_CROSS_LINK_COPY          0x000013A2L

//
// MessageId: MSG_CHK_CROSS_LINK_TRUNC
//
// MessageText:
//
//  Insufficient disk space to copy cross-linked portion.
//  File being truncated.
//
#define MSG_CHK_CROSS_LINK_TRUNC         0x000013A3L

//
// MessageId: MSG_CHK_INVALID_NAME
//
// MessageText:
//
//  %1  Invalid name.  Folder entry removed.
//
#define MSG_CHK_INVALID_NAME             0x000013A4L

//
// MessageId: MSG_CHK_INVALID_TIME_STAMP
//
// MessageText:
//
//  %1  Invalid time stamp.
//
#define MSG_CHK_INVALID_TIME_STAMP       0x000013A5L

//
// MessageId: MSG_CHK_DIR_HAS_FILESIZE
//
// MessageText:
//
//  %1  Folder has non-zero file size.
//
#define MSG_CHK_DIR_HAS_FILESIZE         0x000013A6L

//
// MessageId: MSG_CHK_UNRECOG_EA_HANDLE
//
// MessageText:
//
//  %1  Unrecognized extended attribute handle.
//
#define MSG_CHK_UNRECOG_EA_HANDLE        0x000013A7L

//
// MessageId: MSG_CHK_SHARED_EA
//
// MessageText:
//
//  %1  Has handle extended attribute set belonging to another file.
//      Handle removed.
//
#define MSG_CHK_SHARED_EA                0x000013A8L

//
// MessageId: MSG_CHK_UNUSED_EA_SET
//
// MessageText:
//
//  Unused extended attribute set with handle %1 deleted from
//  extended attribute file.
//
#define MSG_CHK_UNUSED_EA_SET            0x000013A9L

//
// MessageId: MSG_CHK_NEW_OWNER_NAME
//
// MessageText:
//
//  Extended attribute set with handle %1 owner changed
//  from %2 to %3.
//
#define MSG_CHK_NEW_OWNER_NAME           0x000013AAL

//
// MessageId: MSG_CHK_BAD_LINKS_IN_ORPHANS
//
// MessageText:
//
//  Bad links in lost chain at cluster %1 corrected.
//
#define MSG_CHK_BAD_LINKS_IN_ORPHANS     0x000013ABL

//
// MessageId: MSG_CHK_CROSS_LINKED_ORPHAN
//
// MessageText:
//
//  Lost chain cross-linked at cluster %1.  Orphan truncated.
//
#define MSG_CHK_CROSS_LINKED_ORPHAN      0x000013ACL

//
// MessageId: MSG_ORPHAN_DISK_SPACE
//
// MessageText:
//
//  Insufficient disk space to recover lost data.
//
#define MSG_ORPHAN_DISK_SPACE            0x000013ADL

//
// MessageId: MSG_TOO_MANY_ORPHANS
//
// MessageText:
//
//  Insufficient disk space to recover lost data.
//
#define MSG_TOO_MANY_ORPHANS             0x000013AEL

//
// MessageId: MSG_CHK_ERROR_IN_LOG
//
// MessageText:
//
//  Error in extended attribute log.
//
#define MSG_CHK_ERROR_IN_LOG             0x000013AFL

//
// MessageId: MSG_CHK_ERRORS_IN_DIR_CORR
//
// MessageText:
//
//  %1 Errors in . and/or .. corrected.
//
#define MSG_CHK_ERRORS_IN_DIR_CORR       0x000013B0L

//
// MessageId: MSG_CHK_RENAMING_FAILURE
//
// MessageText:
//
//  More than one %1 entry in folder %2.
//  Renamed to %3 but still could not resolve the name conflict.
//
#define MSG_CHK_RENAMING_FAILURE         0x000013B1L

//
// MessageId: MSG_CHK_RENAMED_REPEATED_ENTRY
//
// MessageText:
//
//  More than one %1 entry in folder %2.
//  Renamed to %3.
//
#define MSG_CHK_RENAMED_REPEATED_ENTRY   0x000013B2L

//
// MessageId: MSG_CHK_UNHANDLED_INVALID_NAME
//
// MessageText:
//
//  %1 may be an invalid name in folder %2.
//
#define MSG_CHK_UNHANDLED_INVALID_NAME   0x000013B3L

//
// MessageId: MSG_CHK_INVALID_NAME_CORRECTED
//
// MessageText:
//
//  Corrected name %1 in folder %2.
//
#define MSG_CHK_INVALID_NAME_CORRECTED   0x000013B4L

//
// MessageId: MSG_CHK_INVALID_MEDIA_BYTE
//
// MessageText:
//
//  Invalid media byte.
//
#define MSG_CHK_INVALID_MEDIA_BYTE       0x000013B5L

//
// MessageId: MSG_CHK_REPAIRED_EA
//
// MessageText:
//
//  Repaired extended attributes.
//
#define MSG_CHK_REPAIRED_EA              0x000013B6L

//
// MessageId: MSG_CHK_BAD_SECTORS_FOUND
//
// MessageText:
//
//  Bad sectors found.
//
#define MSG_CHK_BAD_SECTORS_FOUND        0x000013B7L

//
// MessageId: MSG_CHK_MINOR_ERRORS_DETECTED
//
// MessageText:
//
//  Detected minor inconsistencies on the drive.  This is not a corruption.
//
#define MSG_CHK_MINOR_ERRORS_DETECTED    0x000013B8L

//
// MessageId: MSG_CHK_MINOR_ERRORS_FIXED
//
// MessageText:
//
//  Cleaning up minor inconsistencies on the drive.
//
#define MSG_CHK_MINOR_ERRORS_FIXED       0x000013B9L

//--------------------
//
// Messages for label.
//
//--------------------
//
// MessageId: MSG_LBL_INFO
//
// MessageText:
//
//  Creates, changes, or deletes the volume label of a disk.
//  
//
#define MSG_LBL_INFO                     0x00001770L

//
// MessageId: MSG_LBL_USAGE
//
// MessageText:
//
//  LABEL [drive:][label]
//  LABEL [/MP] [volume] [label]
//  
//    drive:          Specifies the drive letter of a drive.
//    label           Specifies the label of the volume.
//    /MP             Specifies that the volume should be treated as a
//                    mount point or volume name.
//    volume          Specifies the drive letter (followed by a colon),
//                    mount point, or volume name.  If volume name is specified,
//                    the /MP flag is unnecessary.
//
#define MSG_LBL_USAGE                    0x00001771L

//
// MessageId: MSG_LBL_NO_LABEL
//
// MessageText:
//
//  Volume in drive %1 has no label
//
#define MSG_LBL_NO_LABEL                 0x00001772L

//
// MessageId: MSG_LBL_THE_LABEL
//
// MessageText:
//
//  Volume in drive %1 is %2
//
#define MSG_LBL_THE_LABEL                0x00001773L

//
// MessageId: MSG_LBL_DELETE_LABEL
//
// MessageText:
//
//  
//  Delete current volume label (Y/N)? %0
//
#define MSG_LBL_DELETE_LABEL             0x00001775L

//
// MessageId: MSG_LBL_NOT_SUPPORTED
//
// MessageText:
//
//  Cannot change label on this volume.  The request is not supported.
//
#define MSG_LBL_NOT_SUPPORTED            0x00001776L

//
// MessageId: MSG_LBL_INVALID_DRIVE_SPEC
//
// MessageText:
//
//  Invalid drive specification.
//
#define MSG_LBL_INVALID_DRIVE_SPEC       0x00001777L

//
// MessageId: MSG_LBL_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH
//
// MessageText:
//
//  The given volume name does not have a mount point or drive letter.
//
#define MSG_LBL_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH 0x00001778L

//
// MessageId: MSG_LBL_WRITE_PROTECTED_MEDIA
//
// MessageText:
//
//  Cannot change label.  This volume is write protected.
//
#define MSG_LBL_WRITE_PROTECTED_MEDIA    0x00001779L

//
// MessageId: MSG_LBL_ROOT_DIRECTORY_FULL
//
// MessageText:
//
//  Volume label cannot be added because the root directory on this volume is full.
//
#define MSG_LBL_ROOT_DIRECTORY_FULL      0x0000177AL

//
// MessageId: MSG_LBL_CHANGE_CANCEL
//
// MessageText:
//
//  Volume label change cancelled.
//
#define MSG_LBL_CHANGE_CANCEL            0x0000177BL

//---------------------
//
// Messages for attrib.
//
//---------------------
//
// MessageId: MSG_ATTRIB_ARCHIVE
//
// MessageText:
//
//  A
//
#define MSG_ATTRIB_ARCHIVE               0x00001B58L

//
// MessageId: MSG_ATTRIB_HIDDEN
//
// MessageText:
//
//  H
//
#define MSG_ATTRIB_HIDDEN                0x00001B59L

//
// MessageId: MSG_ATTRIB_READ_ONLY
//
// MessageText:
//
//  R
//
#define MSG_ATTRIB_READ_ONLY             0x00001B5AL

//
// MessageId: MSG_ATTRIB_SYSTEM
//
// MessageText:
//
//  R
//
#define MSG_ATTRIB_SYSTEM                0x00001B5BL

//
// MessageId: MSG_ATTRIB_FILE_NOT_FOUND
//
// MessageText:
//
//  File not found - %1
//
#define MSG_ATTRIB_FILE_NOT_FOUND        0x00001B5CL

//
// MessageId: MSG_ATTRIB_PATH_NOT_FOUND
//
// MessageText:
//
//  Path not found - %1
//
#define MSG_ATTRIB_PATH_NOT_FOUND        0x00001B5DL

//
// MessageId: MSG_ATTRIB_PARAMETER_NOT_CORRECT
//
// MessageText:
//
//  Parameter format not correct -
//
#define MSG_ATTRIB_PARAMETER_NOT_CORRECT 0x00001B5EL

//
// MessageId: MSG_ATTRIB_NOT_RESETTING_SYS_FILE
//
// MessageText:
//
//  Not resetting system file - %1
//
#define MSG_ATTRIB_NOT_RESETTING_SYS_FILE 0x00001B5FL

//
// MessageId: MSG_ATTRIB_NOT_RESETTING_HIDDEN_FILE
//
// MessageText:
//
//  Not resetting hidden file - %1
//
#define MSG_ATTRIB_NOT_RESETTING_HIDDEN_FILE 0x00001B60L

//
// MessageId: MSG_ATTRIB_DISPLAY_ATTRIBUTE
//
// MessageText:
//
//  %1  %2%3%4     %5
//
#define MSG_ATTRIB_DISPLAY_ATTRIBUTE     0x00001B61L

//
// MessageId: MSG_ATTRIB_HELP_MESSAGE
//
// MessageText:
//
//  Displays or changes file attributes.
//  
//  ATTRIB [+R | -R] [+A | -A ] [+S | -S] [+H | -H] [drive:][path][filename]
//         [/S [/D]]
//  
//    +   Sets an attribute.
//    -   Clears an attribute.
//    R   Read-only file attribute.
//    A   Archive file attribute.
//    S   System file attribute.
//    H   Hidden file attribute.
//    [drive:][path][filename]
//        Specifies a file or files for attrib to process.
//    /S  Processes matching files in the current folder
//        and all subfolders.
//    /D  Processes folders as well.
//  
//
#define MSG_ATTRIB_HELP_MESSAGE          0x00001B62L

//
// MessageId: MSG_ATTRIB_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch - %1
//
#define MSG_ATTRIB_INVALID_SWITCH        0x00001B64L

//
// MessageId: MSG_ATTRIB_ACCESS_DENIED
//
// MessageText:
//
//  Access denied - %1
//
#define MSG_ATTRIB_ACCESS_DENIED         0x00001B65L

//
// MessageId: MSG_ATTRIB_UNABLE_TO_CHANGE_ATTRIBUTE
//
// MessageText:
//
//  Unable to change attribute - %1
//
#define MSG_ATTRIB_UNABLE_TO_CHANGE_ATTRIBUTE 0x00001B66L

//
// MessageId: MSG_ATTRIB_INVALID_COMBINATION
//
// MessageText:
//
//  The /D switch is only valid with the /S switch.
//
#define MSG_ATTRIB_INVALID_COMBINATION   0x00001B67L

//-------------------
//
// Diskcopy messages.
//
//-------------------
//
// MessageId: MSG_9000
//
// MessageText:
//
//  
//
#define MSG_9000                         0x00002328L

//
// MessageId: MSG_9001
//
// MessageText:
//
//  Do not specify filename(s)
//  Command Format: DISKCOPY [drive1: [drive2:]] [/1] [/V]
//
#define MSG_9001                         0x00002329L

//
// MessageId: MSG_DCOPY_INVALID_DRIVE
//
// MessageText:
//
//  
//  Invalid drive specification.
//  The specified drive does not exist
//  or is non-removable.
//
#define MSG_DCOPY_INVALID_DRIVE          0x0000232AL

//
// MessageId: MSG_9003
//
// MessageText:
//
//  
//  Cannot DISKCOPY to or from
//  a network drive
//
#define MSG_9003                         0x0000232BL

//
// MessageId: MSG_DCOPY_FORMATTING_WHILE_COPYING
//
// MessageText:
//
//  
//  Formatting while copying
//
#define MSG_DCOPY_FORMATTING_WHILE_COPYING 0x0000232CL

//
// MessageId: MSG_DCOPY_INSERT_SOURCE
//
// MessageText:
//
//  
//  Insert SOURCE disk in drive %1
//
#define MSG_DCOPY_INSERT_SOURCE          0x0000232DL

//
// MessageId: MSG_DCOPY_INSERT_TARGET
//
// MessageText:
//
//  
//  Insert TARGET disk in drive %1
//
#define MSG_DCOPY_INSERT_TARGET          0x0000232EL

//
// MessageId: MSG_9007
//
// MessageText:
//
//  Make sure a disk is inserted into
//  the drive and the drive door is closed
//
#define MSG_9007                         0x0000232FL

//
// MessageId: MSG_9008
//
// MessageText:
//
//  
//  The target disk may be unusable.
//
#define MSG_9008                         0x00002330L

//
// MessageId: MSG_DCOPY_BAD_TARGET
//
// MessageText:
//
//  
//  The target disk is unusable.
//
#define MSG_DCOPY_BAD_TARGET             0x00002331L

//
// MessageId: MSG_DCOPY_ANOTHER
//
// MessageText:
//
//  
//  Copy another disk (Y/N)?  %0
//
#define MSG_DCOPY_ANOTHER                0x00002332L

//
// MessageId: MSG_DCOPY_COPYING
//
// MessageText:
//
//  
//  Copying %1 tracks
//  %2 sectors per track, %3 side(s)
//
#define MSG_DCOPY_COPYING                0x00002333L

//
// MessageId: MSG_DCOPY_NON_COMPAT_DISKS
//
// MessageText:
//
//  
//  The drive types or disk types
//  are not compatible.
//
#define MSG_DCOPY_NON_COMPAT_DISKS       0x00002334L

//
// MessageId: MSG_DCOPY_READ_ERROR
//
// MessageText:
//
//  
//  Unrecoverable read error on drive %1
//  side %2, track %3
//
#define MSG_DCOPY_READ_ERROR             0x00002335L

//
// MessageId: MSG_DCOPY_WRITE_ERROR
//
// MessageText:
//
//  
//  Unrecoverable write error on drive %1
//  side %2, track %3
//
#define MSG_DCOPY_WRITE_ERROR            0x00002336L

//
// MessageId: MSG_DCOPY_ENDED
//
// MessageText:
//
//  
//  Copy process ended
//
#define MSG_DCOPY_ENDED                  0x00002337L

//
// MessageId: MSG_DCOPY_BAD_SOURCE
//
// MessageText:
//
//  
//  SOURCE disk bad or incompatible.
//
#define MSG_DCOPY_BAD_SOURCE             0x00002338L

//
// MessageId: MSG_DCOPY_BAD_DEST
//
// MessageText:
//
//  
//  TARGET disk bad or incompatible.
//
#define MSG_DCOPY_BAD_DEST               0x00002339L

//
// MessageId: MSG_DCOPY_INFO
//
// MessageText:
//
//  Copies the contents of one floppy disk to another.
//  
//
#define MSG_DCOPY_INFO                   0x0000233CL

//
// MessageId: MSG_DCOPY_USAGE
//
// MessageText:
//
//  DISKCOPY [drive1: [drive2:]] [/V]
//  
//
#define MSG_DCOPY_USAGE                  0x0000233DL

//
// MessageId: MSG_DCOPY_SLASH_V
//
// MessageText:
//
//    /V   Verifies that the information is copied correctly.
//  
//
#define MSG_DCOPY_SLASH_V                0x0000233FL

//
// MessageId: MSG_DCOPY_INFO_2
//
// MessageText:
//
//  The two floppy disks must be the same type.
//  You may specify the same drive for drive1 and drive2.
//
#define MSG_DCOPY_INFO_2                 0x00002340L

//
// MessageId: MSG_DCOPY_INSERT_SOURCE_AND_TARGET
//
// MessageText:
//
//  
//  Insert SOURCE disk in drive %1
//  
//  Insert TARGET disk in drive %2
//
#define MSG_DCOPY_INSERT_SOURCE_AND_TARGET 0x00002341L

//
// MessageId: MSG_DCOPY_UNRECOGNIZED_FORMAT
//
// MessageText:
//
//  Unrecognized format.
//
#define MSG_DCOPY_UNRECOGNIZED_FORMAT    0x00002342L

//
// MessageId: MSG_DCOPY_NOT_ADMINISTRATOR
//
// MessageText:
//
//  Only an administrator can copy this disk.
//
#define MSG_DCOPY_NOT_ADMINISTRATOR      0x00002343L

//
// MessageId: MSG_DCOPY_DISK_TOO_LARGE
//
// MessageText:
//
//  Cannot copy disk larger than %1 megabytes.
//
#define MSG_DCOPY_DISK_TOO_LARGE         0x00002344L

// this message will never appear as text message.
// this is a placeholder for the GUI version of the message.
//
// MessageId: MSG_DCOPY_UNRECOGNIZED_MEDIA
//
// MessageText:
//
//  Unrecognized media.  Please insert the correct media into drive %1.
//
#define MSG_DCOPY_UNRECOGNIZED_MEDIA     0x00002345L

// this message will never appear as text message.
// this is a placeholder for the GUI version of the message.
//
// MessageId: MSG_DCOPY_NO_MEDIA_IN_DEVICE
//
// MessageText:
//
//  There is no disk in the drive.  Please insert a disk into drive %1.
//
#define MSG_DCOPY_NO_MEDIA_IN_DEVICE     0x00002346L

// this message will never appear as text message.
// this is a placeholder for the GUI version of the message.
//
// MessageId: MSG_DCOPY_MEDIA_WRITE_PROTECTED
//
// MessageText:
//
//  The disk in drive %1 is write-protected.  Please use a writeable disk.
//
#define MSG_DCOPY_MEDIA_WRITE_PROTECTED  0x00002347L

//-------------------
//
// Diskcomp messages.
//
//-------------------
//
// MessageId: MSG_10000
//
// MessageText:
//
//  Do not specify filename(s)
//  Command format: DISKCOMP [drive1: [drive2:]] [/1] [/8]
//
#define MSG_10000                        0x00002710L

//
// MessageId: MSG_10001
//
// MessageText:
//
//  
//  Invalid drive specification.
//  The specified drive does not exist
//  or is non-removable.
//
#define MSG_10001                        0x00002711L

//
// MessageId: MSG_DCOMP_INSERT_FIRST
//
// MessageText:
//
//  
//  Insert FIRST disk in drive %1
//
#define MSG_DCOMP_INSERT_FIRST           0x00002713L

//
// MessageId: MSG_DCOMP_INSERT_SECOND
//
// MessageText:
//
//  
//  Insert SECOND disk in drive %1
//
#define MSG_DCOMP_INSERT_SECOND          0x00002714L

//
// MessageId: MSG_DCOMP_FIRST_DISK_BAD
//
// MessageText:
//
//  
//  FIRST disk is bad or incompatible
//
#define MSG_DCOMP_FIRST_DISK_BAD         0x00002715L

//
// MessageId: MSG_DCOMP_SECOND_DISK_BAD
//
// MessageText:
//
//  
//  SECOND disk is bad or incompatible
//
#define MSG_DCOMP_SECOND_DISK_BAD        0x00002716L

//
// MessageId: MSG_DCOMP_ANOTHER
//
// MessageText:
//
//  
//  Compare another disk (Y/N) ? %0
//
#define MSG_DCOMP_ANOTHER                0x00002717L

//
// MessageId: MSG_DCOMP_COMPARING
//
// MessageText:
//
//  
//  Comparing %1 tracks
//  %2 sectors per track, %3 side(s)
//
#define MSG_DCOMP_COMPARING              0x00002718L

//
// MessageId: MSG_DCOMP_NOT_COMPATIBLE
//
// MessageText:
//
//  
//  The drive types or disk types are not compatible.
//
#define MSG_DCOMP_NOT_COMPATIBLE         0x00002719L

//
// MessageId: MSG_10010
//
// MessageText:
//
//  
//  Unrecoverable read error on drive %1
//  side %2, track %3
//
#define MSG_10010                        0x0000271AL

//
// MessageId: MSG_DCOMP_COMPARE_ERROR
//
// MessageText:
//
//  
//  Compare error on
//  side %1, track %2
//
#define MSG_DCOMP_COMPARE_ERROR          0x0000271BL

//
// MessageId: MSG_10012
//
// MessageText:
//
//  Make sure a disk is inserted into
//  the drive and the drive door is closed.
//
#define MSG_10012                        0x0000271CL

//
// MessageId: MSG_DCOMP_ENDED
//
// MessageText:
//
//  
//  Compare process ended.
//
#define MSG_DCOMP_ENDED                  0x0000271DL

//
// MessageId: MSG_DCOMP_OK
//
// MessageText:
//
//  
//  Compare OK
//
#define MSG_DCOMP_OK                     0x0000271EL

//
// MessageId: MSG_10015
//
// MessageText:
//
//  
//
#define MSG_10015                        0x0000271FL

//
// MessageId: MSG_DCOMP_INFO
//
// MessageText:
//
//  Compares the contents of two floppy disks.
//  
//
#define MSG_DCOMP_INFO                   0x00002720L

//
// MessageId: MSG_DCOMP_USAGE
//
// MessageText:
//
//  DISKCOMP [drive1: [drive2:]]
//  
//
#define MSG_DCOMP_USAGE                  0x00002721L

//--------------------
//
// Messages for tree
//
//--------------------
//
// MessageId: MSG_TREE_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch - /%1
//
#define MSG_TREE_INVALID_SWITCH          0x00002AF8L

//
// MessageId: MSG_TREE_INVALID_PATH
//
// MessageText:
//
//  Invalid path - %1
//
#define MSG_TREE_INVALID_PATH            0x00002AF9L

//
// MessageId: MSG_TREE_NO_SUBDIRECTORIES
//
// MessageText:
//
//  No subfolders exist %1
//
#define MSG_TREE_NO_SUBDIRECTORIES       0x00002AFAL

//
// MessageId: MSG_TREE_DIR_LISTING_NO_VOLUME_NAME
//
// MessageText:
//
//  Folder PATH listing
//
#define MSG_TREE_DIR_LISTING_NO_VOLUME_NAME 0x00002AFBL

//
// MessageId: MSG_TREE_DIR_LISTING_WITH_VOLUME_NAME
//
// MessageText:
//
//  Folder PATH listing for volume %1
//
#define MSG_TREE_DIR_LISTING_WITH_VOLUME_NAME 0x00002AFCL

//
// MessageId: MSG_TREE_32_BIT_SERIAL_NUMBER
//
// MessageText:
//
//  Volume serial number is %1-%2
//
#define MSG_TREE_32_BIT_SERIAL_NUMBER    0x00002AFDL

//
// MessageId: MSG_TREE_64_BIT_SERIAL_NUMBER
//
// MessageText:
//
//  Volume serial number is %1 %2:%3
//
#define MSG_TREE_64_BIT_SERIAL_NUMBER    0x00002AFEL

//
// MessageId: MSG_TREE_HELP_MESSAGE
//
// MessageText:
//
//  Graphically displays the folder structure of a drive or path.
//  
//  TREE [drive:][path] [/F] [/A]
//  
//     /F   Display the names of the files in each folder.
//     /A   Use ASCII instead of extended characters.
//  
//
#define MSG_TREE_HELP_MESSAGE            0x00002AFFL

//
// MessageId: MSG_TREE_SINGLE_BOTTOM_LEFT_CORNER
//
// MessageText:
//
//  €
//
#define MSG_TREE_SINGLE_BOTTOM_LEFT_CORNER 0x00002B00L

//
// MessageId: MSG_TREE_SINGLE_BOTTOM_HORIZONTAL
//
// MessageText:
//
//  ?
//
#define MSG_TREE_SINGLE_BOTTOM_HORIZONTAL 0x00002B01L

//
// MessageId: MSG_TREE_SINGLE_LEFT_T
//
// MessageText:
//
//  ?
//
#define MSG_TREE_SINGLE_LEFT_T           0x00002B02L

//
// MessageId: MSG_TREE_PARAMETER_NOT_CORRECT
//
// MessageText:
//
//  Parameter format not correct - %1
//
#define MSG_TREE_PARAMETER_NOT_CORRECT   0x00002B03L

//
// MessageId: MSG_TREE_TOO_MANY_PARAMETERS
//
// MessageText:
//
//  Too many parameters - %1
//
#define MSG_TREE_TOO_MANY_PARAMETERS     0x00002B04L

//
// MessageId: MSG_TREE_INVALID_DRIVE
//
// MessageText:
//
//  Invalid drive specification
//
#define MSG_TREE_INVALID_DRIVE           0x00002B05L

//-------------------
//
// Find messages.
//
//-------------------
//
// MessageId: MSG_FIND
//
// MessageText:
//
//  FIND:  %0
//
#define MSG_FIND                         0x00002EE0L

//
// MessageId: MSG_FIND_INCORRECT_VERSION
//
// MessageText:
//
//  FIND: Incorrect Windows XP version
//
#define MSG_FIND_INCORRECT_VERSION       0x00002EE1L

//
// MessageId: MSG_FIND_INVALID_SWITCH
//
// MessageText:
//
//  FIND: Invalid switch
//
#define MSG_FIND_INVALID_SWITCH          0x00002EE2L

//
// MessageId: MSG_FIND_INVALID_FORMAT
//
// MessageText:
//
//  FIND: Parameter format not correct
//
#define MSG_FIND_INVALID_FORMAT          0x00002EE3L

//
// MessageId: MSG_FIND_USAGE
//
// MessageText:
//
//  Searches for a text string in a file or files.
//  
//  FIND [/V] [/C] [/N] [/I] [/OFF[LINE]] "string" [[drive:][path]filename[ ...]]
//  
//    /V         Displays all lines NOT containing the specified string.
//    /C         Displays only the count of lines containing the string.
//    /N         Displays line numbers with the displayed lines.
//    /I         Ignores the case of characters when searching for the string.
//    /OFF[LINE] Do not skip files with offline attribute set.
//    "string"   Specifies the text string to find.
//    [drive:][path]filename
//               Specifies a file or files to search.
//  
//  If a path is not specified, FIND searches the text typed at the prompt
//  or piped from another command.
//
#define MSG_FIND_USAGE                   0x00002EE4L

//
// MessageId: MSG_FIND_MISSING_PARM
//
// MessageText:
//
//  FIND: Required parameter missing
//
#define MSG_FIND_MISSING_PARM            0x00002EE5L

//
// MessageId: MSG_FIND_FILE_NOT_FOUND
//
// MessageText:
//
//  File not found - %1
//
#define MSG_FIND_FILE_NOT_FOUND          0x00002EE6L

//
// MessageId: MSG_FIND_COUNT
//
// MessageText:
//
//  %1
//
#define MSG_FIND_COUNT                   0x00002EE7L

//
// MessageId: MSG_FIND_COUNT_BANNER
//
// MessageText:
//
//  
//  ---------- %1: %2
//
#define MSG_FIND_COUNT_BANNER            0x00002EE8L

//
// MessageId: MSG_FIND_BANNER
//
// MessageText:
//
//  
//  ---------- %1
//
#define MSG_FIND_BANNER                  0x00002EE9L

//
// MessageId: MSG_FIND_LINEONLY
//
// MessageText:
//
//  %1
//
#define MSG_FIND_LINEONLY                0x00002EEAL

//
// MessageId: MSG_FIND_LINE_AND_NUMBER
//
// MessageText:
//
//  [%1]%2
//
#define MSG_FIND_LINE_AND_NUMBER         0x00002EEBL

//
// MessageId: MSG_FIND_OUT_OF_MEMORY
//
// MessageText:
//
//  Insufficient memory
//
#define MSG_FIND_OUT_OF_MEMORY           0x00002EECL

//
// MessageId: MSG_FIND_UNABLE_TO_READ_FILE
//
// MessageText:
//
//  Unable to read file
//
#define MSG_FIND_UNABLE_TO_READ_FILE     0x00002EEDL

//
// MessageId: MSG_FIND_OFFLINE_FILES_SKIPPED
//
// MessageText:
//
//  Files with offline attribute were skipped.
//  Use /OFFLINE for not skipping such files.
//
#define MSG_FIND_OFFLINE_FILES_SKIPPED   0x00002EEEL

//-----------------
//
// FC Messages
//
//-----------------
//
// MessageId: MSG_FC_HELP_MESSAGE
//
// MessageText:
//
//  Compares two files or sets of files and displays the differences between
//  them
//  
//  
//  FC [/A] [/C] [/L] [/LBn] [/N] [/OFF[LINE]] [/T] [/U] [/W] [/nnnn]
//     [drive1:][path1]filename1 [drive2:][path2]filename2
//  FC /B [drive1:][path1]filename1 [drive2:][path2]filename2
//  
//    /A         Displays only first and last lines for each set of differences.
//    /B         Performs a binary comparison.
//    /C         Disregards the case of letters.
//    /L         Compares files as ASCII text.
//    /LBn       Sets the maximum consecutive mismatches to the specified
//               number of lines.
//    /N         Displays the line numbers on an ASCII comparison.
//    /OFF[LINE] Do not skip files with offline attribute set.
//    /T         Does not expand tabs to spaces.
//    /U         Compare files as UNICODE text files.
//    /W         Compresses white space (tabs and spaces) for comparison.
//    /nnnn      Specifies the number of consecutive lines that must match
//               after a mismatch.
//    [drive1:][path1]filename1
//               Specifies the first file or set of files to compare.
//    [drive2:][path2]filename2
//               Specifies the second file or set of files to compare.
//  
//
#define MSG_FC_HELP_MESSAGE              0x000032C8L

//
// MessageId: MSG_FC_INCOMPATIBLE_SWITCHES
//
// MessageText:
//
//  FC: Incompatible Switches
//  
//
#define MSG_FC_INCOMPATIBLE_SWITCHES     0x000032C9L

//
// MessageId: MSG_FC_INVALID_SWITCH
//
// MessageText:
//
//  FC: Invalid Switch
//  
//
#define MSG_FC_INVALID_SWITCH            0x000032CAL

//
// MessageId: MSG_FC_INSUFFICIENT_FILES
//
// MessageText:
//
//  FC: Insufficient number of file specifications
//  
//
#define MSG_FC_INSUFFICIENT_FILES        0x000032CBL

//
// MessageId: MSG_13004
//
// MessageText:
//
//  Comparing files %1 and %2
//
#define MSG_13004                        0x000032CCL

//
// MessageId: MSG_FC_UNABLE_TO_OPEN
//
// MessageText:
//
//  FC: cannot open %1 - No such file or folder
//  
//
#define MSG_FC_UNABLE_TO_OPEN            0x000032CDL

//
// MessageId: MSG_FC_CANT_EXPAND_TO_MATCH
//
// MessageText:
//
//  %1      %2
//  Could not expand second file name so as to match first
//  
//
#define MSG_FC_CANT_EXPAND_TO_MATCH      0x000032CEL

//
// MessageId: MSG_FC_NO_DIFFERENCES
//
// MessageText:
//
//  FC: no differences encountered
//  
//
#define MSG_FC_NO_DIFFERENCES            0x000032CFL

//
// MessageId: MSG_FC_COMPARING_FILES
//
// MessageText:
//
//  Comparing files %1 and %2
//
#define MSG_FC_COMPARING_FILES           0x000032D0L

//
// MessageId: MSG_FC_FILES_NOT_FOUND
//
// MessageText:
//
//  File(s) not found : %1
//  
//
#define MSG_FC_FILES_NOT_FOUND           0x000032D1L

//
// MessageId: MSG_FC_DATA
//
// MessageText:
//
//  %1
//
#define MSG_FC_DATA                      0x000032D2L

//
// MessageId: MSG_FC_NUMBERED_DATA
//
// MessageText:
//
//  %1:  %2
//
#define MSG_FC_NUMBERED_DATA             0x000032D3L

//
// MessageId: MSG_FC_OUTPUT_FILENAME
//
// MessageText:
//
//  ***** %1
//
#define MSG_FC_OUTPUT_FILENAME           0x000032D4L

//
// MessageId: MSG_FC_DUMP_END
//
// MessageText:
//
//  *****
//  
//
#define MSG_FC_DUMP_END                  0x000032D5L

//
// MessageId: MSG_FC_FILES_DIFFERENT_LENGTH
//
// MessageText:
//
//  FC: %1 longer than %2
//  
//  
//
#define MSG_FC_FILES_DIFFERENT_LENGTH    0x000032D6L

//
// MessageId: MSG_FC_RESYNC_FAILED
//
// MessageText:
//
//  Resync Failed.  Files are too different.
//
#define MSG_FC_RESYNC_FAILED             0x000032D7L

//
// MessageId: MSG_FC_CANT_CREATE_STREAM
//
// MessageText:
//
//  FC: Unable to open %1.  File unavailable for read access.
//  
//
#define MSG_FC_CANT_CREATE_STREAM        0x000032D8L

//
// MessageId: MSG_FC_INCORRECT_VERSION
//
// MessageText:
//
//  FC: Incorrect Windows XP Version
//  
//
#define MSG_FC_INCORRECT_VERSION         0x000032D9L

//
// MessageId: MSG_FC_ABBREVIATE_SYMBOL
//
// MessageText:
//
//  ...
//
#define MSG_FC_ABBREVIATE_SYMBOL         0x000032DAL

//
// MessageId: MSG_FC_ABBREVIATE_SYMBOL_SHIFTED
//
// MessageText:
//
//    ...
//
#define MSG_FC_ABBREVIATE_SYMBOL_SHIFTED 0x000032DBL

//
// MessageId: MSG_FC_HEX_OUT
//
// MessageText:
//
//  %1: %2 %3
//
#define MSG_FC_HEX_OUT                   0x000032DCL

//
// MessageId: MSG_FC_OUT_OF_MEMORY
//
// MessageText:
//
//  FC: Out of memory
//
#define MSG_FC_OUT_OF_MEMORY             0x000032DDL

//
// MessageId: MSG_FC_OFFLINE_FILES_SKIPPED
//
// MessageText:
//
//  Files with offline attribute were skipped.
//  Use /OFFLINE for not skipping such files.
//
#define MSG_FC_OFFLINE_FILES_SKIPPED     0x000032DEL

//-----------------
//
// Comp Messages
//
//-----------------
//
// MessageId: MSG_COMP_HELP_MESSAGE
//
// MessageText:
//
//  Compares the contents of two files or sets of files.
//  
//  COMP [data1] [data2] [/D] [/A] [/L] [/N=number] [/C] [/OFF[LINE]]
//  
//    data1      Specifies location and name(s) of first file(s) to compare.
//    data2      Specifies location and name(s) of second files to compare.
//    /D         Displays differences in decimal format.
//    /A         Displays differences in ASCII characters.
//    /L         Displays line numbers for differences.
//    /N=number  Compares only the first specified number of lines in each file.
//    /C         Disregards case of ASCII letters when comparing files.
//    /OFF[LINE] Do not skip files with offline attribute set.
//  
//  To compare sets of files, use wildcards in data1 and data2 parameters.
//
#define MSG_COMP_HELP_MESSAGE            0x000036B0L

//
// MessageId: MSG_COMP_FILES_OK
//
// MessageText:
//
//  Files compare OK
//  
//
#define MSG_COMP_FILES_OK                0x000036B1L

//
// MessageId: MSG_COMP_NO_MEMORY
//
// MessageText:
//
//  No memory available.
//  
//
#define MSG_COMP_NO_MEMORY               0x000036B2L

//
// MessageId: MSG_COMP_UNABLE_TO_OPEN
//
// MessageText:
//
//  Can't find/open file: %1
//  
//
#define MSG_COMP_UNABLE_TO_OPEN          0x000036B3L

//
// MessageId: MSG_COMP_UNABLE_TO_READ
//
// MessageText:
//
//  Can't read file: %1
//  
//
#define MSG_COMP_UNABLE_TO_READ          0x000036B4L

//
// MessageId: MSG_COMP_BAD_COMMAND_LINE
//
// MessageText:
//
//  Bad command line syntax
//  
//
#define MSG_COMP_BAD_COMMAND_LINE        0x000036B5L

//
// MessageId: MSG_COMP_BAD_NUMERIC_ARG
//
// MessageText:
//
//  Bad numeric argument:
//  %1
//  
//
#define MSG_COMP_BAD_NUMERIC_ARG         0x000036B6L

//
// MessageId: MSG_COMP_COMPARE_ERROR
//
// MessageText:
//
//  Compare error at %1 %2
//  file1 = %3
//  file2 = %4
//
#define MSG_COMP_COMPARE_ERROR           0x000036B7L

//
// MessageId: MSG_COMP_QUERY_FILE1
//
// MessageText:
//
//  Name of first file to compare: %0
//
#define MSG_COMP_QUERY_FILE1             0x000036B8L

//
// MessageId: MSG_COMP_QUERY_FILE2
//
// MessageText:
//
//  Name of second file to compare: %0
//
#define MSG_COMP_QUERY_FILE2             0x000036B9L

//
// MessageId: MSG_COMP_OPTION
//
// MessageText:
//
//  Option: %0
//
#define MSG_COMP_OPTION                  0x000036BAL

//
// MessageId: MSG_COMP_COMPARE_FILES
//
// MessageText:
//
//  Comparing %1 and %2...
//
#define MSG_COMP_COMPARE_FILES           0x000036BBL

//
// MessageId: MSG_COMP_DIFFERENT_SIZES
//
// MessageText:
//
//  Files are different sizes.
//  
//
#define MSG_COMP_DIFFERENT_SIZES         0x000036BCL

//
// MessageId: MSG_COMP_NUMERIC_FORMAT
//
// MessageText:
//
//  Format for /n switch is /n=XXXX
//
#define MSG_COMP_NUMERIC_FORMAT          0x000036BDL

//
// MessageId: MSG_COMP_MORE
//
// MessageText:
//
//  Compare more files (Y/N) ? %0
//
#define MSG_COMP_MORE                    0x000036BEL

//
// MessageId: MSG_COMP_UNABLE_TO_EXPAND
//
// MessageText:
//
//  %1      %2
//  Could not expand second file name so as to match first
//  
//
#define MSG_COMP_UNABLE_TO_EXPAND        0x000036BFL

//
// MessageId: MSG_COMP_TOO_MANY_ERRORS
//
// MessageText:
//
//  10 mismatches - ending compare
//  
//
#define MSG_COMP_TOO_MANY_ERRORS         0x000036C0L

//
// MessageId: MSG_COMP_INCORRECT_VERSION
//
// MessageText:
//
//  Incorrect Windows XP version
//  
//
#define MSG_COMP_INCORRECT_VERSION       0x000036C1L

//
// MessageId: MSG_COMP_UNEXPECTED_END
//
// MessageText:
//
//  Unexpected end of file
//  
//
#define MSG_COMP_UNEXPECTED_END          0x000036C2L

//
// MessageId: MSG_COMP_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch - %1
//  
//
#define MSG_COMP_INVALID_SWITCH          0x000036C3L

//
// MessageId: MSG_COMP_FILE1_TOO_SHORT
//
// MessageText:
//
//  
//  File1 only has %1 lines
//  
//
#define MSG_COMP_FILE1_TOO_SHORT         0x000036C4L

//
// MessageId: MSG_COMP_FILE2_TOO_SHORT
//
// MessageText:
//
//  
//  File2 only has %1 lines
//  
//
#define MSG_COMP_FILE2_TOO_SHORT         0x000036C5L

//
// MessageId: MSG_COMP_WILDCARD_STRING
//
// MessageText:
//
//  *.*%0
//
#define MSG_COMP_WILDCARD_STRING         0x000036C6L

//
// MessageId: MSG_COMP_OFFSET_STRING
//
// MessageText:
//
//  OFFSET%0
//
#define MSG_COMP_OFFSET_STRING           0x000036C7L

//
// MessageId: MSG_COMP_LINE_STRING
//
// MessageText:
//
//  LINE%0
//
#define MSG_COMP_LINE_STRING             0x000036C8L

//
// MessageId: MSG_COMP_OFFLINE_FILES_SKIPPED
//
// MessageText:
//
//  Files with offline attribute were skipped.
//  Use /OFFLINE for not skipping such files.
//
#define MSG_COMP_OFFLINE_FILES_SKIPPED   0x000036C9L

//---------------------------
//
// FAT/HPFS Recover messages.
//
//---------------------------
//
// MessageId: MSG_RECOV_FILE_NOT_FOUND
//
// MessageText:
//
//  
//  File not found
//
#define MSG_RECOV_FILE_NOT_FOUND         0x00003A98L

//
// MessageId: MSG_15001
//
// MessageText:
//
//  
//  Cannot RECOVER an ASSIGNed or SUBSTed drive
//
#define MSG_15001                        0x00003A99L

//
// MessageId: MSG_INVALID_DRIVE
//
// MessageText:
//
//  
//  Invalid drive or file name
//
#define MSG_INVALID_DRIVE                0x00003A9AL

//
// MessageId: MSG_RECOV_CANT_NETWORK
//
// MessageText:
//
//  
//  Cannot RECOVER a network drive
//
#define MSG_RECOV_CANT_NETWORK           0x00003A9CL

//
// MessageId: MSG_15005
//
// MessageText:
//
//  
//  %1 file(s) recovered.
//
#define MSG_15005                        0x00003A9DL

//
// MessageId: MSG_RECOV_BYTES_RECOVERED
//
// MessageText:
//
//  
//  %1 of %2 bytes recovered.
//
#define MSG_RECOV_BYTES_RECOVERED        0x00003A9EL

//
// MessageId: MSG_RECOV_BEGIN
//
// MessageText:
//
//  
//  Press ENTER to begin recovery of the file on drive %1
//  
//
#define MSG_RECOV_BEGIN                  0x00003A9FL

//
// MessageId: MSG_RECOV_CANT_READ_FAT
//
// MessageText:
//
//  
//  Cannot read the file allocation table (FAT).
//
#define MSG_RECOV_CANT_READ_FAT          0x00003AA0L

//
// MessageId: MSG_RECOV_CANT_WRITE_FAT
//
// MessageText:
//
//  
//  Cannot write the file allocation table (FAT).
//
#define MSG_RECOV_CANT_WRITE_FAT         0x00003AA1L

//
// MessageId: MSG_15010
//
// MessageText:
//
//  
//
#define MSG_15010                        0x00003AA2L

//
// MessageId: MSG_RECOV_INFO
//
// MessageText:
//
//  Recovers readable information from a bad or defective disk.
//  
//
#define MSG_RECOV_INFO                   0x00003AA3L

//
// MessageId: MSG_RECOV_USAGE
//
// MessageText:
//
//  RECOVER [drive:][path]filename
//
#define MSG_RECOV_USAGE                  0x00003AA4L

//
// MessageId: MSG_15013
//
// MessageText:
//
//  RECOVER drive:
//  
//
#define MSG_15013                        0x00003AA5L

//
// MessageId: MSG_RECOV_INFO2
//
// MessageText:
//
//  Consult the online Command Reference in Windows XP Help
//  before using the RECOVER command.
//
#define MSG_RECOV_INFO2                  0x00003AA6L

//
// MessageId: MSG_RECOV_WRITE_ERROR
//
// MessageText:
//
//  Write error.
//
#define MSG_RECOV_WRITE_ERROR            0x00003AA9L

//
// MessageId: MSG_RECOV_INTERNAL_ERROR
//
// MessageText:
//
//  Internal consistency error.
//
#define MSG_RECOV_INTERNAL_ERROR         0x00003AAAL

//
// MessageId: MSG_RECOV_READ_ERROR
//
// MessageText:
//
//  Read error.
//
#define MSG_RECOV_READ_ERROR             0x00003AABL

//
// MessageId: MSG_RECOV_NOT_SUPPORTED
//
// MessageText:
//
//  RECOVER on an entire volume is no longer supported.
//  To get equivalent functionality use CHKDSK.
//
#define MSG_RECOV_NOT_SUPPORTED          0x00003AACL

//----------------------------------
//
//  NTFS-specific recover messages
//
//----------------------------------
//
// MessageId: MSG_NTFS_RECOV_SYSTEM_FILE
//
// MessageText:
//
//  NTFS RECOVER cannot be used to recover system files. Use CHKDSK instead.
//
#define MSG_NTFS_RECOV_SYSTEM_FILE       0x00003C29L

//
// MessageId: MSG_NTFS_RECOV_FAILED
//
// MessageText:
//
//  NTFS RECOVER failed.
//
#define MSG_NTFS_RECOV_FAILED            0x00003C2AL

//
// MessageId: MSG_NTFS_RECOV_CORRUPT_VOLUME
//
// MessageText:
//
//  NTFS RECOVER has detected that the volume is corrupt.  Run CHKDSK /f
//  to fix it.
//
#define MSG_NTFS_RECOV_CORRUPT_VOLUME    0x00003AC3L

//
// MessageId: MSG_NTFS_RECOV_CANT_WRITE_ELEMENTARY
//
// MessageText:
//
//  NTFS Recover could not write elementary disk structures.  The volume
//  may be corrupt; run CHKDSK /f to fix it.
//
#define MSG_NTFS_RECOV_CANT_WRITE_ELEMENTARY 0x00003AC4L

//
// MessageId: MSG_NTFS_RECOV_WRONG_VERSION
//
// MessageText:
//
//  Files on this volume cannot be recovered with this version of UNTFS.DLL.
//
#define MSG_NTFS_RECOV_WRONG_VERSION     0x00003AC5L

//--------------------
//
// Messages for Print
//
//--------------------
//
// MessageId: MSG_PRINT_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch - %1
//
#define MSG_PRINT_INVALID_SWITCH         0x00003E80L

//
// MessageId: MSG_PRINT_NOT_IMPLEMENTED
//
// MessageText:
//
//  Switch %1 is not implemented
//
#define MSG_PRINT_NOT_IMPLEMENTED        0x00003E81L

//
// MessageId: MSG_PRINT_NO_FILE
//
// MessageText:
//
//  No file to print
//
#define MSG_PRINT_NO_FILE                0x00003E82L

//
// MessageId: MSG_PRINT_UNABLE_INIT_DEVICE
//
// MessageText:
//
//  Unable to initialize device %1
//
#define MSG_PRINT_UNABLE_INIT_DEVICE     0x00003E83L

//
// MessageId: MSG_PRINT_FILE_NOT_FOUND
//
// MessageText:
//
//  Can't find file %1
//
#define MSG_PRINT_FILE_NOT_FOUND         0x00003E84L

//
// MessageId: MSG_PRINT_PRINTING
//
// MessageText:
//
//  %1 is currently being printed
//
#define MSG_PRINT_PRINTING               0x00003E85L

//
// MessageId: MSG_PRINT_HELP_MESSAGE
//
// MessageText:
//
//  Prints a text file.
//  
//  PRINT [/D:device] [[drive:][path]filename[...]]
//  
//     /D:device   Specifies a print device.
//  
//
#define MSG_PRINT_HELP_MESSAGE           0x00003E86L

//---------------
//
// Help Messages
//
//---------------
//
// MessageId: MSG_HELP_HELP_MESSAGE
//
// MessageText:
//
//  Provides help information for Windows XP commands.
//  
//  HELP [command]
//  
//      command - displays help information on that command.
//  
//
#define MSG_HELP_HELP_MESSAGE            0x00004268L

//
// MessageId: MSG_HELP_HELP_FILE_NOT_FOUND
//
// MessageText:
//
//  Help file could not be found.
//  
//
#define MSG_HELP_HELP_FILE_NOT_FOUND     0x00004269L

//
// MessageId: MSG_HELP_HELP_FILE_ERROR
//
// MessageText:
//
//  Error reading help file.
//  
//
#define MSG_HELP_HELP_FILE_ERROR         0x0000426AL

//
// MessageId: MSG_HELP_GENERAL_HELP
//
// MessageText:
//
//  
//  
//  For more information on a specific command, type HELP command-name.
//
#define MSG_HELP_GENERAL_HELP            0x0000426BL

//
// MessageId: MSG_HELP_HELP_UNAVAILABLE
//
// MessageText:
//
//  This command is not supported by the help utility.  Try "%1 /?".
//
#define MSG_HELP_HELP_UNAVAILABLE        0x0000426CL

//
// MessageId: MSG_HELP_HELP_COMMENT
//
// MessageText:
//
//  @ %0
//
#define MSG_HELP_HELP_COMMENT            0x0000426DL

//
// MessageId: MSG_HELP_EXECUTE_WITH_CMD
//
// MessageText:
//
//  cmd /c %1 /? %0
//
#define MSG_HELP_EXECUTE_WITH_CMD        0x0000426EL

//
// MessageId: MSG_HELP_EXECUTE_WITHOUT_CMD
//
// MessageText:
//
//  %1 /? %0
//
#define MSG_HELP_EXECUTE_WITHOUT_CMD     0x0000426FL

//
// MessageId: MSG_HELP_HELP_FILE_DATA
//
// MessageText:
//
//  %1
//
#define MSG_HELP_HELP_FILE_DATA          0x00004271L

//
// MessageId: MSG_HELP_INCORRECT_VERSION
//
// MessageText:
//
//  Incorrect Windows XP version
//  
//
#define MSG_HELP_INCORRECT_VERSION       0x00004272L

//
// MessageId: MSG_HELP_MORE
//
// MessageText:
//
//  --- MORE ---%0
//
#define MSG_HELP_MORE                    0x00004273L

//
// MessageId: MSG_HELP_NO_MEMORY
//
// MessageText:
//
//  Insufficient memory.
//
#define MSG_HELP_NO_MEMORY               0x00004274L

//
// MessageId: MSG_HELP_HELP_FILE_DIRECTORY
//
// MessageText:
//
//  \help%0
//
#define MSG_HELP_HELP_FILE_DIRECTORY     0x00004275L

//---------------
//
// MORE messages.
//
//---------------
//
// MessageId: MORE_ENVIRONMENT_VARIABLE_NAME
//
// MessageText:
//
//  MORE%0
//
#define MORE_ENVIRONMENT_VARIABLE_NAME   0x00004E21L

//
// MessageId: MORE_PATTERN_SWITCH_EXTENDED
//
// MessageText:
//
//  /E%0
//
#define MORE_PATTERN_SWITCH_EXTENDED     0x00004E22L

//
// MessageId: MORE_PATTERN_SWITCH_CLEARSCREEN
//
// MessageText:
//
//  /C%0
//
#define MORE_PATTERN_SWITCH_CLEARSCREEN  0x00004E23L

//
// MessageId: MORE_PATTERN_SWITCH_EXPANDFORMFEED
//
// MessageText:
//
//  /P%0
//
#define MORE_PATTERN_SWITCH_EXPANDFORMFEED 0x00004E24L

//
// MessageId: MORE_PATTERN_SWITCH_SQUEEZEBLANKS
//
// MessageText:
//
//  /S%0
//
#define MORE_PATTERN_SWITCH_SQUEEZEBLANKS 0x00004E25L

//
// MessageId: MORE_PATTERN_SWITCH_HELP1
//
// MessageText:
//
//  /?%0
//
#define MORE_PATTERN_SWITCH_HELP1        0x00004E26L

//
// MessageId: MORE_PATTERN_SWITCH_HELP2
//
// MessageText:
//
//  /H%0
//
#define MORE_PATTERN_SWITCH_HELP2        0x00004E27L

//
// MessageId: MORE_PATTENR_ARG_STARTATLINE
//
// MessageText:
//
//  +*%0
//
#define MORE_PATTENR_ARG_STARTATLINE     0x00004E28L

//
// MessageId: MORE_LEXEMIZER_MULTIPLESWITCH
//
// MessageText:
//
//  /ECPSH?%0
//
#define MORE_LEXEMIZER_MULTIPLESWITCH    0x00004E2AL

//
// MessageId: MORE_LEXEMIZER_SWITCHES
//
// MessageText:
//
//  /-%0
//
#define MORE_LEXEMIZER_SWITCHES          0x00004E2BL

//
// MessageId: MORE_PROMPT
//
// MessageText:
//
//  -- More %1%2%3 -- %4%0
//
#define MORE_PROMPT                      0x00004E34L

//
// MessageId: MORE_PERCENT
//
// MessageText:
//
//  (%1%%)%0
//
#define MORE_PERCENT                     0x00004E35L

//
// MessageId: MORE_LINE
//
// MessageText:
//
//  [Line: %1]%0
//
#define MORE_LINE                        0x00004E36L

//
// MessageId: MORE_HELP
//
// MessageText:
//
//  [Options: psfq=<space><ret>]%0
//
#define MORE_HELP                        0x00004E37L

//
// MessageId: MORE_LINEPROMPT
//
// MessageText:
//
//  Lines: %0
//
#define MORE_LINEPROMPT                  0x00004E38L

//
// MessageId: MORE_OPTION_DISPLAYLINES
//
// MessageText:
//
//  P%0
//
#define MORE_OPTION_DISPLAYLINES         0x00004E3EL

//
// MessageId: MORE_OPTION_SKIPLINES
//
// MessageText:
//
//  S%0
//
#define MORE_OPTION_SKIPLINES            0x00004E3FL

//
// MessageId: MORE_OPTION_SHOWLINENUMBER
//
// MessageText:
//
//  =%0
//
#define MORE_OPTION_SHOWLINENUMBER       0x00004E40L

//
// MessageId: MORE_OPTION_QUIT
//
// MessageText:
//
//  Q%0
//
#define MORE_OPTION_QUIT                 0x00004E41L

//
// MessageId: MORE_OPTION_HELP1
//
// MessageText:
//
//  ?%0
//
#define MORE_OPTION_HELP1                0x00004E42L

//
// MessageId: MORE_OPTION_HELP2
//
// MessageText:
//
//  H%0
//
#define MORE_OPTION_HELP2                0x00004E43L

//
// MessageId: MORE_OPTION_NEXTFILE
//
// MessageText:
//
//  F%0
//
#define MORE_OPTION_NEXTFILE             0x00004E44L

//
// MessageId: MORE_MESSAGE_USAGE
//
// MessageText:
//
//  Displays output one screen at a time.
//  
//  MORE [/E [/C] [/P] [/S] [/Tn] [+n]] < [drive:][path]filename
//  command-name | MORE [/E [/C] [/P] [/S] [/Tn] [+n]]
//  MORE /E [/C] [/P] [/S] [/Tn] [+n] [files]
//  
//      [drive:][path]filename  Specifies a file to display one
//                              screen at a time.
//  
//      command-name            Specifies a command whose output
//                              will be displayed.
//  
//      /E      Enable extended features
//      /C      Clear screen before displaying page
//      /P      Expand FormFeed characters
//      /S      Squeeze multiple blank lines into a single line
//      /Tn     Expand tabs to n spaces (default 8)
//  
//              Switches can be present in the MORE environment
//              variable.
//  
//      +n      Start displaying the first file at line n
//  
//      files   List of files to be displayed. Files in the list
//              are separated by blanks.
//  
//      If extended features are enabled, the following commands
//      are accepted at the -- More -- prompt:
//  
//      P n     Display next n lines
//      S n     Skip next n lines
//      F       Display next file
//      Q       Quit
//      =       Show line number
//      ?       Show help line
//      <space> Display next page
//      <ret>   Display next line
//
#define MORE_MESSAGE_USAGE               0x00004E48L

//
// MessageId: MORE_ERROR_GENERAL
//
// MessageText:
//
//  Internal error.
//
#define MORE_ERROR_GENERAL               0x00004E52L

//
// MessageId: MORE_ERROR_TOO_MANY_ARGUMENTS
//
// MessageText:
//
//  Too many arguments in command line.
//
#define MORE_ERROR_TOO_MANY_ARGUMENTS    0x00004E53L

//
// MessageId: MORE_ERROR_NO_MEMORY
//
// MessageText:
//
//  Not enough memory.
//
#define MORE_ERROR_NO_MEMORY             0x00004E54L

//
// MessageId: MORE_ERROR_CANNOT_ACCESS
//
// MessageText:
//
//  Cannot access file %1
//
#define MORE_ERROR_CANNOT_ACCESS         0x00004E55L

//
// MessageId: MORE_ERROR_INVALID_REGISTRY_ENTRY
//
// MessageText:
//
//  Invalid registry entry.
//
#define MORE_ERROR_INVALID_REGISTRY_ENTRY 0x00004E56L

//------------------
//
// REPLACE messages.
//
//------------------
//
// MessageId: REPLACE_PATTERN_SWITCH_ADD
//
// MessageText:
//
//  /A%0
//
#define REPLACE_PATTERN_SWITCH_ADD       0x00005209L

//
// MessageId: REPLACE_PATTERN_SWITCH_PROMPT
//
// MessageText:
//
//  /P%0
//
#define REPLACE_PATTERN_SWITCH_PROMPT    0x0000520AL

//
// MessageId: REPLACE_PATTERN_SWITCH_READONLY
//
// MessageText:
//
//  /R%0
//
#define REPLACE_PATTERN_SWITCH_READONLY  0x0000520BL

//
// MessageId: REPLACE_PATTERN_SWITCH_SUBDIR
//
// MessageText:
//
//  /S%0
//
#define REPLACE_PATTERN_SWITCH_SUBDIR    0x0000520CL

//
// MessageId: REPLACE_PATTERN_SWITCH_COMPARETIME
//
// MessageText:
//
//  /U%0
//
#define REPLACE_PATTERN_SWITCH_COMPARETIME 0x0000520DL

//
// MessageId: REPLACE_PATTERN_SWITCH_WAIT
//
// MessageText:
//
//  /W%0
//
#define REPLACE_PATTERN_SWITCH_WAIT      0x0000520EL

//
// MessageId: REPLACE_PATTERN_SWITCH_HELP
//
// MessageText:
//
//  /?%0
//
#define REPLACE_PATTERN_SWITCH_HELP      0x0000520FL

//
// MessageId: REPLACE_LEXEMIZER_SWITCHES
//
// MessageText:
//
//  /-%0
//
#define REPLACE_LEXEMIZER_SWITCHES       0x00005212L

//
// MessageId: REPLACE_LEXEMIZER_MULTIPLESWITCH
//
// MessageText:
//
//  /APRSUW?%0
//
#define REPLACE_LEXEMIZER_MULTIPLESWITCH 0x00005213L

//
// MessageId: REPLACE_MESSAGE_REPLACING
//
// MessageText:
//
//  Replacing %1
//
#define REPLACE_MESSAGE_REPLACING        0x0000521CL

//
// MessageId: REPLACE_MESSAGE_ADDING
//
// MessageText:
//
//  Adding %1
//
#define REPLACE_MESSAGE_ADDING           0x0000521DL

//
// MessageId: REPLACE_MESSAGE_FILES_REPLACED
//
// MessageText:
//
//  %1 file(s) replaced
//
#define REPLACE_MESSAGE_FILES_REPLACED   0x0000521EL

//
// MessageId: REPLACE_MESSAGE_FILES_ADDED
//
// MessageText:
//
//  %1 file(s) added
//
#define REPLACE_MESSAGE_FILES_ADDED      0x0000521FL

//
// MessageId: REPLACE_MESSAGE_NO_FILES_REPLACED
//
// MessageText:
//
//  No files replaced
//
#define REPLACE_MESSAGE_NO_FILES_REPLACED 0x00005220L

//
// MessageId: REPLACE_MESSAGE_NO_FILES_ADDED
//
// MessageText:
//
//  No files added
//
#define REPLACE_MESSAGE_NO_FILES_ADDED   0x00005221L

//
// MessageId: REPLACE_MESSAGE_PRESS_ANY_KEY
//
// MessageText:
//
//  Press any key to continue . . .
//
#define REPLACE_MESSAGE_PRESS_ANY_KEY    0x00005222L

//
// MessageId: REPLACE_MESSAGE_REPLACE_YES_NO
//
// MessageText:
//
//  Replace %1? (Y/N) %0
//
#define REPLACE_MESSAGE_REPLACE_YES_NO   0x00005223L

//
// MessageId: REPLACE_MESSAGE_ADD_YES_NO
//
// MessageText:
//
//  Add %1? (Y/N) %0
//
#define REPLACE_MESSAGE_ADD_YES_NO       0x00005224L

//
// MessageId: REPLACE_MESSAGE_USAGE
//
// MessageText:
//
//  Replaces files.
//  
//  REPLACE [drive1:][path1]filename [drive2:][path2] [/A] [/P] [/R] [/W]
//  REPLACE [drive1:][path1]filename [drive2:][path2] [/P] [/R] [/S] [/W] [/U]
//  
//    [drive1:][path1]filename Specifies the source file or files.
//    [drive2:][path2]         Specifies the directory where files are to be
//                             replaced.
//    /A                       Adds new files to destination directory. Cannot
//                             use with /S or /U switches.
//    /P                       Prompts for confirmation before replacing a file or
//                             adding a source file.
//    /R                       Replaces read-only files as well as unprotected
//                             files.
//    /S                       Replaces files in all subdirectories of the
//                             destination directory. Cannot use with the /A
//                             switch.
//    /W                       Waits for you to insert a disk before beginning.
//    /U                       Replaces (updates) only files that are older than
//                             source files. Cannot use with the /A switch.
//
#define REPLACE_MESSAGE_USAGE            0x00005225L

//
// MessageId: REPLACE_ERROR_INCORRECT_OS_VERSION
//
// MessageText:
//
//  Incorrect Windows version
//
#define REPLACE_ERROR_INCORRECT_OS_VERSION 0x0000523AL

//
// MessageId: REPLACE_ERROR_SOURCE_PATH_REQUIRED
//
// MessageText:
//
//  Source path required
//
#define REPLACE_ERROR_SOURCE_PATH_REQUIRED 0x0000523BL

//
// MessageId: REPLACE_ERROR_SELF_REPLACE
//
// MessageText:
//
//  File cannot be copied onto itself
//
#define REPLACE_ERROR_SELF_REPLACE       0x0000523CL

//
// MessageId: REPLACE_ERROR_NO_DISK_SPACE
//
// MessageText:
//
//  Insufficient disk space
//
#define REPLACE_ERROR_NO_DISK_SPACE      0x0000523DL

//
// MessageId: REPLACE_ERROR_NO_FILES_FOUND
//
// MessageText:
//
//  No files found - %1
//
#define REPLACE_ERROR_NO_FILES_FOUND     0x0000523EL

//
// MessageId: REPLACE_ERROR_EXTENDED
//
// MessageText:
//
//  Extended Error %1
//
#define REPLACE_ERROR_EXTENDED           0x0000523FL

//
// MessageId: REPLACE_ERROR_PARSE
//
// MessageText:
//
//  Parse Error %1
//
#define REPLACE_ERROR_PARSE              0x00005240L

//
// MessageId: REPLACE_ERROR_NO_MEMORY
//
// MessageText:
//
//  Out of memory
//
#define REPLACE_ERROR_NO_MEMORY          0x00005241L

//
// MessageId: REPLACE_ERROR_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch - %1
//
#define REPLACE_ERROR_INVALID_SWITCH     0x00005242L

//
// MessageId: REPLACE_ERROR_INVALID_PARAMETER_COMBINATION
//
// MessageText:
//
//  Invalid parameter combination
//
#define REPLACE_ERROR_INVALID_PARAMETER_COMBINATION 0x00005243L

//
// MessageId: REPLACE_ERROR_PATH_NOT_FOUND
//
// MessageText:
//
//  Path not found - %1
//
#define REPLACE_ERROR_PATH_NOT_FOUND     0x00005244L

//
// MessageId: REPLACE_ERROR_ACCESS_DENIED
//
// MessageText:
//
//  Access denied - %1
//
#define REPLACE_ERROR_ACCESS_DENIED      0x00005245L

//----------------
//
// XCOPY messages.
//
//----------------
//
// MessageId: XCOPY_ERROR_Z_X_CONFLICT
//
// MessageText:
//
//  The /Z and /O (or /X) options conflict: cannot copy security
//  in restartable mode.
//
#define XCOPY_ERROR_Z_X_CONFLICT         0x000055F0L

//
// MessageId: XCOPY_PATTERN_SWITCH_ARCHIVE
//
// MessageText:
//
//  /A%0
//
#define XCOPY_PATTERN_SWITCH_ARCHIVE     0x000055F1L

//
// MessageId: XCOPY_PATTERN_SWITCH_DATE
//
// MessageText:
//
//  /D:*%0
//
#define XCOPY_PATTERN_SWITCH_DATE        0x000055F2L

//
// MessageId: XCOPY_PATTERN_SWITCH_EMPTY
//
// MessageText:
//
//  /E%0
//
#define XCOPY_PATTERN_SWITCH_EMPTY       0x000055F3L

//
// MessageId: XCOPY_PATTERN_SWITCH_MODIFY
//
// MessageText:
//
//  /M%0
//
#define XCOPY_PATTERN_SWITCH_MODIFY      0x000055F4L

//
// MessageId: XCOPY_PATTERN_SWITCH_PROMPT
//
// MessageText:
//
//  /P%0
//
#define XCOPY_PATTERN_SWITCH_PROMPT      0x000055F5L

//
// MessageId: XCOPY_PATTERN_SWITCH_SUBDIR
//
// MessageText:
//
//  /S%0
//
#define XCOPY_PATTERN_SWITCH_SUBDIR      0x000055F6L

//
// MessageId: XCOPY_PATTERN_SWITCH_VERIFY
//
// MessageText:
//
//  /V%0
//
#define XCOPY_PATTERN_SWITCH_VERIFY      0x000055F7L

//
// MessageId: XCOPY_PATTERN_SWITCH_WAIT
//
// MessageText:
//
//  /W%0
//
#define XCOPY_PATTERN_SWITCH_WAIT        0x000055F8L

//
// MessageId: XCOPY_PATTERN_SWITCH_HELP
//
// MessageText:
//
//  /?%0
//
#define XCOPY_PATTERN_SWITCH_HELP        0x000055F9L

//
// MessageId: XCOPY_LEXEMIZER_SWITCHES
//
// MessageText:
//
//  /-%0
//
#define XCOPY_LEXEMIZER_SWITCHES         0x00005604L

//
// MessageId: XCOPY_LEXEMIZER_MULTIPLESWITCH
//
// MessageText:
//
//  /AEMPSVW?%0
//
#define XCOPY_LEXEMIZER_MULTIPLESWITCH   0x00005605L

//
// MessageId: XCOPY_ERROR_NO_MEMORY
//
// MessageText:
//
//  Insufficient memory
//
#define XCOPY_ERROR_NO_MEMORY            0x0000560FL

//
// MessageId: XCOPY_ERROR_INVALID_PARAMETER
//
// MessageText:
//
//  Invalid parameter - %1
//
#define XCOPY_ERROR_INVALID_PARAMETER    0x00005610L

//
// MessageId: XCOPY_ERROR_INVALID_PATH
//
// MessageText:
//
//  Invalid path
//
#define XCOPY_ERROR_INVALID_PATH         0x00005612L

//
// MessageId: XCOPY_ERROR_CYCLE
//
// MessageText:
//
//  Cannot perform a cyclic copy
//
#define XCOPY_ERROR_CYCLE                0x00005613L

//
// MessageId: XCOPY_ERROR_INVALID_DATE
//
// MessageText:
//
//  Invalid date
//
#define XCOPY_ERROR_INVALID_DATE         0x00005614L

//
// MessageId: XCOPY_ERROR_CREATE_DIRECTORY
//
// MessageText:
//
//  Unable to create directory
//
#define XCOPY_ERROR_CREATE_DIRECTORY     0x00005615L

//
// MessageId: XCOPY_ERROR_INVALID_DRIVE
//
// MessageText:
//
//  Invalid drive specification
//
#define XCOPY_ERROR_INVALID_DRIVE        0x00005616L

//
// MessageId: XCOPY_ERROR_RESERVED_DEVICE
//
// MessageText:
//
//  Cannot XCOPY from a reserved device
//
#define XCOPY_ERROR_RESERVED_DEVICE      0x00005617L

//
// MessageId: XCOPY_ERROR_ACCESS_DENIED
//
// MessageText:
//
//  Access denied
//
#define XCOPY_ERROR_ACCESS_DENIED        0x00005618L

//
// MessageId: XCOPY_ERROR_TOO_MANY_OPEN_FILES
//
// MessageText:
//
//  Too many open files
//
#define XCOPY_ERROR_TOO_MANY_OPEN_FILES  0x00005619L

//
// MessageId: XCOPY_ERROR_GENERAL
//
// MessageText:
//
//  General failure
//
#define XCOPY_ERROR_GENERAL              0x0000561AL

//
// MessageId: XCOPY_ERROR_SHARING_VIOLATION
//
// MessageText:
//
//  Sharing violation
//
#define XCOPY_ERROR_SHARING_VIOLATION    0x0000561BL

//
// MessageId: XCOPY_ERROR_LOCK_VIOLATION
//
// MessageText:
//
//  Lock violation
//
#define XCOPY_ERROR_LOCK_VIOLATION       0x0000561CL

//
// MessageId: XCOPY_ERROR_PATH_NOT_FOUND
//
// MessageText:
//
//  Path not found
//
#define XCOPY_ERROR_PATH_NOT_FOUND       0x0000561DL

//
// MessageId: XCOPY_ERROR_DISK_FULL
//
// MessageText:
//
//  Insufficient disk space
//
#define XCOPY_ERROR_DISK_FULL            0x0000561EL

//
// MessageId: XCOPY_ERROR_SELF_COPY
//
// MessageText:
//
//  File cannot be copied onto itself
//
#define XCOPY_ERROR_SELF_COPY            0x0000561FL

//
// MessageId: XCOPY_ERROR_INVALID_NUMBER_PARAMETERS
//
// MessageText:
//
//  Invalid number of parameters
//
#define XCOPY_ERROR_INVALID_NUMBER_PARAMETERS 0x00005620L

//
// MessageId: XCOPY_ERROR_CREATE_DIRECTORY1
//
// MessageText:
//
//  Unable to create directory - %1
//
#define XCOPY_ERROR_CREATE_DIRECTORY1    0x00005621L

//
// MessageId: XCOPY_ERROR_FILE_NOT_FOUND
//
// MessageText:
//
//  File not found - %1
//
#define XCOPY_ERROR_FILE_NOT_FOUND       0x00005622L

//
// MessageId: XCOPY_ERROR_CANNOT_MAKE
//
// MessageText:
//
//  File creation error - %1
//
#define XCOPY_ERROR_CANNOT_MAKE          0x00005623L

//
// MessageId: XCOPY_ERROR_INVALID_SWITCH
//
// MessageText:
//
//  Invalid switch
//
#define XCOPY_ERROR_INVALID_SWITCH       0x00005624L

//
// MessageId: XCOPY_ERROR_INVALID_PATH_PARTIAL_COPY
//
// MessageText:
//
//  Invalid Path, not all directories/files copied
//
#define XCOPY_ERROR_INVALID_PATH_PARTIAL_COPY 0x00005625L

//
// MessageId: XCOPY_ERROR_EXTENDED
//
// MessageText:
//
//  Extended Error %1
//
#define XCOPY_ERROR_EXTENDED             0x00005626L

//
// MessageId: XCOPY_ERROR_PARSE
//
// MessageText:
//
//  Parse Error
//
#define XCOPY_ERROR_PARSE                0x00005627L

//
// MessageId: XCOPY_ERROR_WRITE_PROTECT
//
// MessageText:
//
//  Write protect error accessing drive.
//
#define XCOPY_ERROR_WRITE_PROTECT        0x00005628L

//
// MessageId: XCOPY_ERROR_INVALID_SWITCH_SWITCH
//
// MessageText:
//
//  Invalid switch - %1
//
#define XCOPY_ERROR_INVALID_SWITCH_SWITCH 0x00005629L

//
// MessageId: XCOPY_MESSAGE_USAGE
//
// MessageText:
//
//  Copies files and directory trees.
//  
//  XCOPY source [destination] [/A | /M] [/D[:date]] [/P] [/S [/E]] [/V] [/W]
//                             [/C] [/I] [/Q] [/F] [/L] [/G] [/H] [/R] [/T] [/U]
//                             [/K] [/N] [/O] [/X] [/Y] [/-Y] [/Z]
//                             [/EXCLUDE:file1[+file2][+file3]...]
//  
//    source       Specifies the file(s) to copy.
//    destination  Specifies the location and/or name of new files.
//    /A           Copies only files with the archive attribute set,
//                 doesn't change the attribute.
//    /M           Copies only files with the archive attribute set,
//                 turns off the archive attribute.
//    /D:m-d-y     Copies files changed on or after the specified date.
//                 If no date is given, copies only those files whose
//                 source time is newer than the destination time.
//    /EXCLUDE:file1[+file2][+file3]...
//                 Specifies a list of files containing strings.  Each string
//                 should be in a separate line in the files.  When any of the
//                 strings match any part of the absolute path of the file to be
//                 copied, that file will be excluded from being copied.  For
//                 example, specifying a string like \obj\ or .obj will exclude
//                 all files underneath the directory obj or all files with the
//                 .obj extension respectively.
//    /P           Prompts you before creating each destination file.
//    /S           Copies directories and subdirectories except empty ones.
//    /E           Copies directories and subdirectories, including empty ones.
//                 Same as /S /E. May be used to modify /T.
//    /V           Verifies each new file.
//    /W           Prompts you to press a key before copying.
//    /C           Continues copying even if errors occur.
//    /I           If destination does not exist and copying more than one file,
//                 assumes that destination must be a directory.
//    /Q           Does not display file names while copying.
//    /F           Displays full source and destination file names while copying.
//    /L           Displays files that would be copied.
//    /G           Allows the copying of encrypted files to destination that does
//                 not support encryption.
//    /H           Copies hidden and system files also.
//    /R           Overwrites read-only files.
//    /T           Creates directory structure, but does not copy files. Does not
//                 include empty directories or subdirectories. /T /E includes
//                 empty directories and subdirectories.
//    /U           Copies only files that already exist in destination.
//    /K           Copies attributes. Normal Xcopy will reset read-only attributes.
//    /N           Copies using the generated short names.
//    /O           Copies file ownership and ACL information.
//    /X           Copies file audit settings (implies /O).
//    /Y           Suppresses prompting to confirm you want to overwrite an
//                 existing destination file.
//    /-Y          Causes prompting to confirm you want to overwrite an
//                 existing destination file.
//    /Z           Copies networked files in restartable mode.
//  
//  The switch /Y may be preset in the COPYCMD environment variable.
//  This may be overridden with /-Y on the command line.
//
#define XCOPY_MESSAGE_USAGE              0x0000562CL

//
// MessageId: XCOPY_MESSAGE_WAIT
//
// MessageText:
//
//  Press any key when ready to begin copying file(s)%0
//
#define XCOPY_MESSAGE_WAIT               0x0000562DL

//
// MessageId: XCOPY_MESSAGE_CONFIRM
//
// MessageText:
//
//  %1 (Y/N)? %0
//
#define XCOPY_MESSAGE_CONFIRM            0x0000562EL

//
// MessageId: XCOPY_MESSAGE_FILE_OR_DIRECTORY
//
// MessageText:
//
//  Does %1 specify a file name
//  or directory name on the target
//  (F = file, D = directory)? %0
//
#define XCOPY_MESSAGE_FILE_OR_DIRECTORY  0x0000562FL

//
// MessageId: XCOPY_MESSAGE_FILES_COPIED
//
// MessageText:
//
//  %1 File(s) copied
//
#define XCOPY_MESSAGE_FILES_COPIED       0x00005630L

//
// MessageId: XCOPY_MESSAGE_FILENAME
//
// MessageText:
//
//  %1
//
#define XCOPY_MESSAGE_FILENAME           0x00005631L

//
// MessageId: XCOPY_MESSAGE_VERBOSE_COPY
//
// MessageText:
//
//  %1 -> %2
//
#define XCOPY_MESSAGE_VERBOSE_COPY       0x00005632L

//
// MessageId: XCOPY_MESSAGE_CHANGE_DISK
//
// MessageText:
//
//  
//  Insufficient disk space on current disk.
//  Insert another disk and type <Return> to continue... %0
//
#define XCOPY_MESSAGE_CHANGE_DISK        0x00005633L

//
// MessageId: XCOPY_RESPONSE_FILE
//
// MessageText:
//
//  F%0
//
#define XCOPY_RESPONSE_FILE              0x00005636L

//
// MessageId: XCOPY_RESPONSE_DIRECTORY
//
// MessageText:
//
//  D%0
//
#define XCOPY_RESPONSE_DIRECTORY         0x00005637L

//
// MessageId: XCOPY_RESPONSE_YES
//
// MessageText:
//
//  Y%0
//
#define XCOPY_RESPONSE_YES               0x00005638L

//
// MessageId: XCOPY_RESPONSE_NO
//
// MessageText:
//
//  N%0
//
#define XCOPY_RESPONSE_NO                0x00005639L

//
// MessageId: XCOPY_MESSAGE_FILES
//
// MessageText:
//
//  %1 File(s)
//
#define XCOPY_MESSAGE_FILES              0x0000563AL

//
// MessageId: XCOPY_ERROR_VERIFY_FAILED
//
// MessageText:
//
//  File verification failed.
//
#define XCOPY_ERROR_VERIFY_FAILED        0x0000563BL

//
// MessageId: XCOPY_RESPONSE_ALL
//
// MessageText:
//
//  A%0
//
#define XCOPY_RESPONSE_ALL               0x0000563CL

//
// MessageId: XCOPY_MESSAGE_CONFIRM2
//
// MessageText:
//
//  Overwrite %1 (Yes/No/All)? %0
//
#define XCOPY_MESSAGE_CONFIRM2           0x0000563DL

//
// MessageId: XCOPY_ERROR_PATH_TOO_LONG
//
// MessageText:
//
//  File path is too long to be displayed.
//
#define XCOPY_ERROR_PATH_TOO_LONG        0x0000563EL

//
// MessageId: XCOPY_ERROR_INSUFFICIENT_PRIVILEGE
//
// MessageText:
//
//  Insufficient privilege to perform the operation or access denied.
//
#define XCOPY_ERROR_INSUFFICIENT_PRIVILEGE 0x0000563FL

//
// MessageId: XCOPY_ERROR_ENCRYPTION_FAILED
//
// MessageText:
//
//  Encryption failed.
//
#define XCOPY_ERROR_ENCRYPTION_FAILED    0x00005640L

//
// MessageId: XCOPY_ERROR_SECURITY_INFO_NOT_SUPPORTED
//
// MessageText:
//
//  Security Information not supported by destination file system.
//
#define XCOPY_ERROR_SECURITY_INFO_NOT_SUPPORTED 0x00005641L

//
// MessageId: XCOPY_ERROR_UNKNOWN
//
// MessageText:
//
//  Unknown error.
//
#define XCOPY_ERROR_UNKNOWN              0x00005642L

//
// MessageId: XCOPY_ERROR_INCOMPLETE_COPY
//
// MessageText:
//
//  Source directory may be changing.
//  XCOPY may not be able to copy all files or directories within the directory.
//
#define XCOPY_ERROR_INCOMPLETE_COPY      0x00005643L

//
// MessageId: XCOPY_ERROR_STACK_SPACE
//
// MessageText:
//
//  Insufficient stack space.
//
#define XCOPY_ERROR_STACK_SPACE          0x00005644L

//
// MessageId: XCOPY_MESSAGE_CONFIRM3
//
// MessageText:
//
//  %1 -> %2 (Y/N)? %0
//
#define XCOPY_MESSAGE_CONFIRM3           0x00005645L

//---------------
//
// MODE messages.
//
//---------------
//
// MessageId: MODE_MESSAGE_REROUTED
//
// MessageText:
//
//  LPT%1: rerouted to COM%2:
//
#define MODE_MESSAGE_REROUTED            0x00005A0AL

//
// MessageId: MODE_MESSAGE_ACTIVE_CODEPAGE
//
// MessageText:
//
//  Active code page for device %1 is %2
//
#define MODE_MESSAGE_ACTIVE_CODEPAGE     0x00005A0BL

//
// MessageId: MODE_MESSAGE_HELP
//
// MessageText:
//
//  Configures system devices.
//  
//  Serial port:       MODE COMm[:] [BAUD=b] [PARITY=p] [DATA=d] [STOP=s]
//                                  [to=on|off] [xon=on|off] [odsr=on|off]
//                                  [octs=on|off] [dtr=on|off|hs]
//                                  [rts=on|off|hs|tg] [idsr=on|off]
//  
//  Device Status:     MODE [device] [/STATUS]
//  
//  Redirect printing: MODE LPTn[:]=COMm[:]
//  
//  Select code page:  MODE CON[:] CP SELECT=yyy
//  
//  Code page status:  MODE CON[:] CP [/STATUS]
//  
//  Display mode:      MODE CON[:] [COLS=c] [LINES=n]
//  
//  Typematic rate:    MODE CON[:] [RATE=r DELAY=d]
//
#define MODE_MESSAGE_HELP                0x00005A0CL

//
// MessageId: MODE_MESSAGE_STATUS
//
// MessageText:
//
//  Status for device *:%0
//
#define MODE_MESSAGE_STATUS              0x00005A0DL

//
// MessageId: MODE_MESSAGE_STATUS_BAUD
//
// MessageText:
//
//      Baud:            %1
//
#define MODE_MESSAGE_STATUS_BAUD         0x00005A0FL

//
// MessageId: MODE_MESSAGE_STATUS_PARITY
//
// MessageText:
//
//      Parity:          %1
//
#define MODE_MESSAGE_STATUS_PARITY       0x00005A10L

//
// MessageId: MODE_MESSAGE_STATUS_DATA
//
// MessageText:
//
//      Data Bits:       %1
//
#define MODE_MESSAGE_STATUS_DATA         0x00005A11L

//
// MessageId: MODE_MESSAGE_STATUS_STOP
//
// MessageText:
//
//      Stop Bits:       %1
//
#define MODE_MESSAGE_STATUS_STOP         0x00005A12L

//
// MessageId: MODE_MESSAGE_STATUS_TIMEOUT
//
// MessageText:
//
//      Timeout:         %1
//
#define MODE_MESSAGE_STATUS_TIMEOUT      0x00005A13L

//
// MessageId: MODE_MESSAGE_STATUS_XON
//
// MessageText:
//
//      XON/XOFF:        %1
//
#define MODE_MESSAGE_STATUS_XON          0x00005A14L

//
// MessageId: MODE_MESSAGE_STATUS_OCTS
//
// MessageText:
//
//      CTS handshaking: %1
//
#define MODE_MESSAGE_STATUS_OCTS         0x00005A15L

//
// MessageId: MODE_MESSAGE_STATUS_ODSR
//
// MessageText:
//
//      DSR handshaking: %1
//
#define MODE_MESSAGE_STATUS_ODSR         0x00005A16L

//
// MessageId: MODE_MESSAGE_STATUS_IDSR
//
// MessageText:
//
//      DSR sensitivity: %1
//
#define MODE_MESSAGE_STATUS_IDSR         0x00005A17L

//
// MessageId: MODE_MESSAGE_STATUS_DTR
//
// MessageText:
//
//      DTR circuit:     %1
//
#define MODE_MESSAGE_STATUS_DTR          0x00005A18L

//
// MessageId: MODE_MESSAGE_STATUS_RTS
//
// MessageText:
//
//      RTS circuit:     %1
//
#define MODE_MESSAGE_STATUS_RTS          0x00005A19L

//
// MessageId: MODE_MESSAGE_STATUS_LINES
//
// MessageText:
//
//      Lines:          %1
//
#define MODE_MESSAGE_STATUS_LINES        0x00005A1EL

//
// MessageId: MODE_MESSAGE_STATUS_COLS
//
// MessageText:
//
//      Columns:        %1
//
#define MODE_MESSAGE_STATUS_COLS         0x00005A1FL

//
// MessageId: MODE_MESSAGE_STATUS_CODEPAGE
//
// MessageText:
//
//      Code page:      %1
//
#define MODE_MESSAGE_STATUS_CODEPAGE     0x00005A20L

//
// MessageId: MODE_MESSAGE_STATUS_REROUTED
//
// MessageText:
//
//      Printer output is being rerouted to serial port %1
//
#define MODE_MESSAGE_STATUS_REROUTED     0x00005A21L

//
// MessageId: MODE_MESSAGE_STATUS_NOT_REROUTED
//
// MessageText:
//
//      Printer output is not being rerouted.
//
#define MODE_MESSAGE_STATUS_NOT_REROUTED 0x00005A22L

//
// MessageId: MODE_MESSAGE_STATUS_RATE
//
// MessageText:
//
//      Keyboard rate:  %1
//
#define MODE_MESSAGE_STATUS_RATE         0x00005A23L

//
// MessageId: MODE_MESSAGE_STATUS_DELAY
//
// MessageText:
//
//      Keyboard delay: %1
//
#define MODE_MESSAGE_STATUS_DELAY        0x00005A24L

//
// MessageId: MODE_MESSAGE_LPT_USE_CONTROL_PANEL
//
// MessageText:
//
//  To change printer settings use the Printers option in Control Panel
//
#define MODE_MESSAGE_LPT_USE_CONTROL_PANEL 0x00005A27L

//
// MessageId: MODE_ERROR_INCORRECT_OS_VERSION
//
// MessageText:
//
//  Incorrect operating system version
//
#define MODE_ERROR_INCORRECT_OS_VERSION  0x00005A28L

//
// MessageId: MODE_ERROR_INVALID_DEVICE_NAME
//
// MessageText:
//
//  Illegal device name - %1
//
#define MODE_ERROR_INVALID_DEVICE_NAME   0x00005A29L

//
// MessageId: MODE_ERROR_INVALID_BAUD_RATE
//
// MessageText:
//
//  Invalid baud rate specified
//
#define MODE_ERROR_INVALID_BAUD_RATE     0x00005A2AL

//
// MessageId: MODE_ERROR_NOT_REROUTED
//
// MessageText:
//
//  %1: not rerouted
//
#define MODE_ERROR_NOT_REROUTED          0x00005A2BL

//
// MessageId: MODE_ERROR_INVALID_PARAMETER
//
// MessageText:
//
//  Invalid parameter - %1
//
#define MODE_ERROR_INVALID_PARAMETER     0x00005A2CL

//
// MessageId: MODE_ERROR_INVALID_NUMBER_OF_PARAMETERS
//
// MessageText:
//
//  Invalid number of parameters
//
#define MODE_ERROR_INVALID_NUMBER_OF_PARAMETERS 0x00005A2DL

//
// MessageId: MODE_ERROR_CANNOT_ACCESS_DEVICE
//
// MessageText:
//
//  Failure to access device: %1
//
#define MODE_ERROR_CANNOT_ACCESS_DEVICE  0x00005A2EL

//
// MessageId: MODE_ERROR_CODEPAGE_OPERATION_NOT_SUPPORTED
//
// MessageText:
//
//  Code page operation not supported on this device
//
#define MODE_ERROR_CODEPAGE_OPERATION_NOT_SUPPORTED 0x00005A2FL

//
// MessageId: MODE_ERROR_CODEPAGE_NOT_SUPPORTED
//
// MessageText:
//
//  Current keyboard does not support this code page
//
#define MODE_ERROR_CODEPAGE_NOT_SUPPORTED 0x00005A30L

//
// MessageId: MODE_ERROR_NO_MEMORY
//
// MessageText:
//
//  Out of memory
//
#define MODE_ERROR_NO_MEMORY             0x00005A31L

//
// MessageId: MODE_ERROR_PARSE
//
// MessageText:
//
//  Parse Error
//
#define MODE_ERROR_PARSE                 0x00005A32L

//
// MessageId: MODE_ERROR_EXTENDED
//
// MessageText:
//
//  Extended error %1
//
#define MODE_ERROR_EXTENDED              0x00005A33L

//
// MessageId: MODE_ERROR_SERIAL_OPTIONS_NOT_SUPPORTED
//
// MessageText:
//
//  The specified options are not supported by this serial device
//
#define MODE_ERROR_SERIAL_OPTIONS_NOT_SUPPORTED 0x00005A34L

//
// MessageId: MODE_ERROR_INVALID_SCREEN_SIZE
//
// MessageText:
//
//  The screen cannot be set to the number of lines and columns specified.
//
#define MODE_ERROR_INVALID_SCREEN_SIZE   0x00005A35L

//
// MessageId: MODE_ERROR_LPT_CANNOT_SET
//
// MessageText:
//
//  The device cannot be set to the specified number of lines and/or columns.
//
#define MODE_ERROR_LPT_CANNOT_SET        0x00005A36L

//
// MessageId: MODE_ERROR_LPT_CANNOT_ENDREROUTE
//
// MessageText:
//
//  Cannot stop printer rerouting at this time.
//
#define MODE_ERROR_LPT_CANNOT_ENDREROUTE 0x00005A37L

//
// MessageId: MODE_ERROR_LPT_CANNOT_REROUTE
//
// MessageText:
//
//  Cannot reroute printer output to serial device %1.
//
#define MODE_ERROR_LPT_CANNOT_REROUTE    0x00005A38L

//
// MessageId: MODE_ERROR_INVALID_RATE
//
// MessageText:
//
//  Invalid keyboard rate
//
#define MODE_ERROR_INVALID_RATE          0x00005A39L

//
// MessageId: MODE_ERROR_INVALID_DELAY
//
// MessageText:
//
//  Invalid keyboard delay
//
#define MODE_ERROR_INVALID_DELAY         0x00005A3AL

//
// MessageId: MODE_ERROR_FULL_SCREEN
//
// MessageText:
//
//  The number of lines and columns cannot be changed in a full screen.
//
#define MODE_ERROR_FULL_SCREEN           0x00005A3BL

//
// MessageId: MODE_ERROR_INVALID_CODEPAGE
//
// MessageText:
//
//  The code page specified is not valid.
//
#define MODE_ERROR_INVALID_CODEPAGE      0x00005A3CL

//
// MessageId: MODE_ERROR_NOT_SUPPORTED
//
// MessageText:
//
//  The specified option is not supported.
//
#define MODE_ERROR_NOT_SUPPORTED         0x00005A3DL

//
// MessageId: MODE_MESSAGE_USED_DEFAULT_PARITY
//
// MessageText:
//
//  Default to even parity.
//
#define MODE_MESSAGE_USED_DEFAULT_PARITY 0x00005A46L

//
// MessageId: MODE_MESSAGE_USED_DEFAULT_DATA
//
// MessageText:
//
//  Default to %1 data bits.
//
#define MODE_MESSAGE_USED_DEFAULT_DATA   0x00005A47L

//
// MessageId: MODE_MESSAGE_USED_DEFAULT_STOP
//
// MessageText:
//
//  Default to %1 stop bits.
//
#define MODE_MESSAGE_USED_DEFAULT_STOP   0x00005A48L

//
// MessageId: MODE_MESSAGE_COM_NO_CHANGE
//
// MessageText:
//
//  No serial port setting changed.
//
#define MODE_MESSAGE_COM_NO_CHANGE       0x00005A49L

//
// MessageId: MODE_MESSAGE_NOT_NEEDED
//
// MessageText:
//
//  This operation is not necessary under Windows XP.
//
#define MODE_MESSAGE_NOT_NEEDED          0x00005A4AL

//
// MessageId: MODE_ERROR_DEVICE_UNAVAILABLE
//
// MessageText:
//
//  Device %1 is not currently available.
//
#define MODE_ERROR_DEVICE_UNAVAILABLE    0x00005A4BL

//---------------
//
// NTFS messages.
//
//---------------
//
// MessageId: MSG_NTFS_UNREADABLE_BOOT_SECTOR
//
// MessageText:
//
//  The first NTFS boot sector is unreadable.
//  Reading second NTFS boot sector instead.
//
#define MSG_NTFS_UNREADABLE_BOOT_SECTOR  0x00005DC0L

//
// MessageId: MSG_NTFS_ALL_BOOT_SECTORS_UNREADABLE
//
// MessageText:
//
//  All NTFS boot sectors are unreadable.  Cannot continue.
//
#define MSG_NTFS_ALL_BOOT_SECTORS_UNREADABLE 0x00005DC1L

//
// MessageId: MSG_NTFS_SECOND_BOOT_SECTOR_UNWRITEABLE
//
// MessageText:
//
//  The second NTFS boot sector is unwriteable.
//
#define MSG_NTFS_SECOND_BOOT_SECTOR_UNWRITEABLE 0x00005DC2L

//
// MessageId: MSG_NTFS_FIRST_BOOT_SECTOR_UNWRITEABLE
//
// MessageText:
//
//  The first NTFS boot sector is unwriteable.
//
#define MSG_NTFS_FIRST_BOOT_SECTOR_UNWRITEABLE 0x00005DC3L

//
// MessageId: MSG_NTFS_ALL_BOOT_SECTORS_UNWRITEABLE
//
// MessageText:
//
//  All NTFS boot sectors are unwriteable.  Cannot continue.
//
#define MSG_NTFS_ALL_BOOT_SECTORS_UNWRITEABLE 0x00005DC4L

//
// MessageId: MSG_NTFS_FORMAT_NO_FLOPPIES
//
// MessageText:
//
//  The NTFS file system does not function on floppy disks.
//
#define MSG_NTFS_FORMAT_NO_FLOPPIES      0x00005DC5L

//----------------------
//
// NTFS CHKDSK messages.
//
//----------------------
//
// MessageId: MSG_CHK_NTFS_BAD_FRS
//
// MessageText:
//
//  Deleting corrupt file record segment %1.
//
#define MSG_CHK_NTFS_BAD_FRS             0x00006590L

//
// MessageId: MSG_CHK_NTFS_BAD_ATTR
//
// MessageText:
//
//  Deleting corrupt attribute record (%1, %2)
//  from file record segment %3.
//
#define MSG_CHK_NTFS_BAD_ATTR            0x00006591L

//
// MessageId: MSG_CHK_NTFS_FRS_TRUNC_RECORDS
//
// MessageText:
//
//  Truncating badly linked attribute records
//  from file record segment %1.
//
#define MSG_CHK_NTFS_FRS_TRUNC_RECORDS   0x00006592L

//
// MessageId: MSG_CHK_NTFS_UNSORTED_FRS
//
// MessageText:
//
//  Sorting attribute records for file record segment %1.
//
#define MSG_CHK_NTFS_UNSORTED_FRS        0x00006593L

//
// MessageId: MSG_CHK_NTFS_DUPLICATE_ATTRIBUTES
//
// MessageText:
//
//  Deleting duplicate attribute records (%1, %2)
//  from file record segment %3.
//
#define MSG_CHK_NTFS_DUPLICATE_ATTRIBUTES 0x00006594L

//
// MessageId: MSG_CHK_NTFS_BAD_ATTR_LIST
//
// MessageText:
//
//  Deleted corrupt attribute list for file %1.
//
#define MSG_CHK_NTFS_BAD_ATTR_LIST       0x00006595L

//
// MessageId: MSG_CHK_NTFS_CANT_READ_ATTR_LIST
//
// MessageText:
//
//  Deleted unreadable attribute list for file %1.
//
#define MSG_CHK_NTFS_CANT_READ_ATTR_LIST 0x00006596L

//
// MessageId: MSG_CHK_NTFS_BAD_ATTR_LIST_ENTRY
//
// MessageText:
//
//  Deleted corrupt attribute list entry
//  with type code %1 in file %2.
//
#define MSG_CHK_NTFS_BAD_ATTR_LIST_ENTRY 0x00006597L

//
// MessageId: MSG_CHK_NTFS_ATTR_LIST_TRUNC
//
// MessageText:
//
//  Truncating corrupt attribute list for file %1.
//
#define MSG_CHK_NTFS_ATTR_LIST_TRUNC     0x00006598L

//
// MessageId: MSG_CHK_NTFS_UNSORTED_ATTR_LIST
//
// MessageText:
//
//  Sorting attribute list for file %1.
//
#define MSG_CHK_NTFS_UNSORTED_ATTR_LIST  0x00006599L

//
// MessageId: MSG_CHK_NTFS_UNREADABLE_MFT
//
// MessageText:
//
//  Unreadable master file table.  CHKDSK aborted.
//
#define MSG_CHK_NTFS_UNREADABLE_MFT      0x0000659AL

//
// MessageId: MSG_CHK_NTFS_BAD_MFT
//
// MessageText:
//
//  Corrupt master file table.  CHKDSK aborted.
//
#define MSG_CHK_NTFS_BAD_MFT             0x0000659BL

//
// MessageId: MSG_CHK_NTFS_BAD_ATTR_DEF_TABLE
//
// MessageText:
//
//  Corrupt Attribute Definition Table.
//  CHKDSK is assuming the default.
//
#define MSG_CHK_NTFS_BAD_ATTR_DEF_TABLE  0x0000659CL

//
// MessageId: MSG_NTFS_CHK_NOT_NTFS
//
// MessageText:
//
//  Unable to determine volume version and state.  CHKDSK aborted.
//
#define MSG_NTFS_CHK_NOT_NTFS            0x0000659DL

//
// MessageId: MSG_CHK_NTFS_UNREADABLE_FRS
//
// MessageText:
//
//  File record segment %1 is unreadable.
//
#define MSG_CHK_NTFS_UNREADABLE_FRS      0x0000659EL

//
// MessageId: MSG_CHK_NTFS_ORPHAN_FRS
//
// MessageText:
//
//  Deleting orphan file record segment %1.
//
#define MSG_CHK_NTFS_ORPHAN_FRS          0x0000659FL

//
// MessageId: MSG_CHK_NTFS_CANT_HOTFIX_SYSTEM_FILES
//
// MessageText:
//
//  Insufficient disk space to hotfix unreadable system file %1.
//  CHKDSK Aborted.
//
#define MSG_CHK_NTFS_CANT_HOTFIX_SYSTEM_FILES 0x000065A0L

//
// MessageId: MSG_CHK_NTFS_CANT_HOTFIX
//
// MessageText:
//
//  Insufficient disk space to hotfix unreadable file %1.
//
#define MSG_CHK_NTFS_CANT_HOTFIX         0x000065A1L

//
// MessageId: MSG_CHK_NTFS_BAD_FIRST_FREE
//
// MessageText:
//
//  First free byte offset corrected in file record segment %1.
//
#define MSG_CHK_NTFS_BAD_FIRST_FREE      0x000065A2L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_MFT_MIRROR
//
// MessageText:
//
//  Correcting errors in the Master File Table (MFT) mirror.
//
#define MSG_CHK_NTFS_CORRECTING_MFT_MIRROR 0x000065A3L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_MFT_MIRROR
//
// MessageText:
//
//  Insufficient disk space to repair master file table (MFT) mirror.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_MFT_MIRROR 0x000065A4L

//
// MessageId: MSG_CHK_NTFS_CANT_ADD_BAD_CLUSTERS
//
// MessageText:
//
//  Insufficient disk space to record bad clusters.
//
#define MSG_CHK_NTFS_CANT_ADD_BAD_CLUSTERS 0x000065A5L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_MFT_DATA
//
// MessageText:
//
//  Correcting errors in the master file table's (MFT) DATA attribute.
//
#define MSG_CHK_NTFS_CORRECTING_MFT_DATA 0x000065A6L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_MFT
//
// MessageText:
//
//  Insufficient disk space to fix master file table (MFT).  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_MFT        0x000065A7L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_MFT_BITMAP
//
// MessageText:
//
//  Correcting errors in the master file table's (MFT) BITMAP attribute.
//
#define MSG_CHK_NTFS_CORRECTING_MFT_BITMAP 0x000065A8L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_VOLUME_BITMAP
//
// MessageText:
//
//  Insufficient disk space to fix volume bitmap.  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_VOLUME_BITMAP 0x000065A9L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_VOLUME_BITMAP
//
// MessageText:
//
//  Correcting errors in the Volume Bitmap.
//
#define MSG_CHK_NTFS_CORRECTING_VOLUME_BITMAP 0x000065AAL

//
// MessageId: MSG_CHK_NTFS_CORRECTING_ATTR_DEF
//
// MessageText:
//
//  Correcting errors in the Attribute Definition Table.
//
#define MSG_CHK_NTFS_CORRECTING_ATTR_DEF 0x000065ABL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_ATTR_DEF
//
// MessageText:
//
//  Insufficient disk space to fix the attribute definition table.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_ATTR_DEF   0x000065ACL

//
// MessageId: MSG_CHK_NTFS_CORRECTING_BAD_FILE
//
// MessageText:
//
//  Correcting errors in the Bad Clusters File.
//
#define MSG_CHK_NTFS_CORRECTING_BAD_FILE 0x000065ADL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_BAD_FILE
//
// MessageText:
//
//  Insufficient disk space to fix the bad clusters file.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_BAD_FILE   0x000065AEL

//
// MessageId: MSG_CHK_NTFS_CORRECTING_BOOT_FILE
//
// MessageText:
//
//  Correcting errors in the Boot File.
//
#define MSG_CHK_NTFS_CORRECTING_BOOT_FILE 0x000065AFL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_BOOT_FILE
//
// MessageText:
//
//  Insufficient disk space to fix the boot file.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_BOOT_FILE  0x000065B0L

//
// MessageId: MSG_CHK_NTFS_ADDING_BAD_CLUSTERS
//
// MessageText:
//
//  Adding %1 bad clusters to the Bad Clusters File.
//
#define MSG_CHK_NTFS_ADDING_BAD_CLUSTERS 0x000065B1L

//
// MessageId: MSG_CHK_NTFS_TOTAL_DISK_SPACE_IN_KB
//
// MessageText:
//
//  
//  %1 KB total disk space.
//
#define MSG_CHK_NTFS_TOTAL_DISK_SPACE_IN_KB 0x000065B2L

//
// MessageId: MSG_CHK_NTFS_USER_FILES_IN_KB
//
// MessageText:
//
//  %1 KB in %2 files.
//
#define MSG_CHK_NTFS_USER_FILES_IN_KB    0x000065B3L

//
// MessageId: MSG_CHK_NTFS_INDICES_REPORT_IN_KB
//
// MessageText:
//
//  %1 KB in %2 indexes.
//
#define MSG_CHK_NTFS_INDICES_REPORT_IN_KB 0x000065B4L

//
// MessageId: MSG_CHK_NTFS_BAD_SECTORS_REPORT_IN_KB
//
// MessageText:
//
//  %1 KB in bad sectors.
//
#define MSG_CHK_NTFS_BAD_SECTORS_REPORT_IN_KB 0x000065B5L

//
// MessageId: MSG_CHK_NTFS_SYSTEM_SPACE_IN_KB
//
// MessageText:
//
//  %1 KB in use by the system.
//
#define MSG_CHK_NTFS_SYSTEM_SPACE_IN_KB  0x000065B6L

//
// MessageId: MSG_CHK_NTFS_AVAILABLE_SPACE_IN_KB
//
// MessageText:
//
//  %1 KB available on disk.
//  
//
#define MSG_CHK_NTFS_AVAILABLE_SPACE_IN_KB 0x000065B7L

//
// MessageId: MSG_CHK_NTFS_ERROR_IN_INDEX
//
// MessageText:
//
//  Correcting error in index %2 for file %1.
//
#define MSG_CHK_NTFS_ERROR_IN_INDEX      0x000065B8L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_INDEX
//
// MessageText:
//
//  Insufficient disk space to correct errors
//  in index %2 of file %1.
//
#define MSG_CHK_NTFS_CANT_FIX_INDEX      0x000065B9L

//
// MessageId: MSG_CHK_NTFS_BAD_INDEX
//
// MessageText:
//
//  Removing corrupt index %2 in file %1.
//
#define MSG_CHK_NTFS_BAD_INDEX           0x000065BAL

//
// MessageId: MSG_CHK_NTFS_DELETING_DIRECTORY_ENTRIES
//
// MessageText:
//
//  Deleting directory entries in %1
//
#define MSG_CHK_NTFS_DELETING_DIRECTORY_ENTRIES 0x000065BBL

//
// MessageId: MSG_CHK_NTFS_CANT_DELETE_ALL_DIRECTORY_ENTRIES
//
// MessageText:
//
//  CHKDSK cannot delete all corrupt directory entries.
//
#define MSG_CHK_NTFS_CANT_DELETE_ALL_DIRECTORY_ENTRIES 0x000065BCL

//
// MessageId: MSG_CHK_NTFS_RECOVERING_ORPHANS
//
// MessageText:
//
//  CHKDSK is recovering lost files.
//
#define MSG_CHK_NTFS_RECOVERING_ORPHANS  0x000065BDL

//
// MessageId: MSG_CHK_NTFS_CANT_CREATE_ORPHANS
//
// MessageText:
//
//  Insufficient disk space for CHKDSK to recover lost files.
//
#define MSG_CHK_NTFS_CANT_CREATE_ORPHANS 0x000065BEL

//
// MessageId: MSG_CHK_NTFS_CORRECTING_ERROR_IN_DIRECTORY
//
// MessageText:
//
//  Correcting error in directory %1
//
#define MSG_CHK_NTFS_CORRECTING_ERROR_IN_DIRECTORY 0x000065BFL

//
// MessageId: MSG_CHK_NTFS_BADLY_ORDERED_INDEX
//
// MessageText:
//
//  Sorting index %2 in file %1.
//
#define MSG_CHK_NTFS_BADLY_ORDERED_INDEX 0x000065C0L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_EA
//
// MessageText:
//
//  Correcting extended attribute information in file %1.
//
#define MSG_CHK_NTFS_CORRECTING_EA       0x000065C1L

//
// MessageId: MSG_CHK_NTFS_DELETING_CORRUPT_EA_SET
//
// MessageText:
//
//  Deleting corrupt extended attribute set in file %1.
//
#define MSG_CHK_NTFS_DELETING_CORRUPT_EA_SET 0x000065C2L

//
// MessageId: MSG_CHK_NTFS_INACCURATE_DUPLICATED_INFORMATION
//
// MessageText:
//
//  Incorrect duplicate information in file %1.
//
#define MSG_CHK_NTFS_INACCURATE_DUPLICATED_INFORMATION 0x000065C3L

//
// MessageId: MSG_CHK_NTFS_CREATING_ROOT_DIRECTORY
//
// MessageText:
//
//  CHKDSK is creating new root directory.
//
#define MSG_CHK_NTFS_CREATING_ROOT_DIRECTORY 0x000065C4L

//
// MessageId: MSG_CHK_NTFS_CANT_CREATE_ROOT_DIRECTORY
//
// MessageText:
//
//  Insufficient disk space to create new root directory.
//
#define MSG_CHK_NTFS_CANT_CREATE_ROOT_DIRECTORY 0x000065C5L

//
// MessageId: MSG_CHK_NTFS_RECOVERING_ORPHAN
//
// MessageText:
//
//  Recovering orphaned file %1 (%2) into directory file %3.
//
#define MSG_CHK_NTFS_RECOVERING_ORPHAN   0x000065C6L

//
// MessageId: MSG_CHK_NTFS_CANT_RECOVER_ORPHAN
//
// MessageText:
//
//  Insufficient disk space to recover lost data.
//
#define MSG_CHK_NTFS_CANT_RECOVER_ORPHAN 0x000065C7L

//
// MessageId: MSG_CHK_NTFS_TOO_MANY_ORPHANS
//
// MessageText:
//
//  Too much lost data to recover it all.
//
#define MSG_CHK_NTFS_TOO_MANY_ORPHANS    0x000065C8L

//
// MessageId: MSG_CHK_NTFS_USING_MFT_MIRROR
//
// MessageText:
//
//  Fixing critical master file table (MFT) files with MFT mirror.
//
#define MSG_CHK_NTFS_USING_MFT_MIRROR    0x000065C9L

//
// MessageId: MSG_CHK_NTFS_MINOR_CHANGES_TO_FRS
//
// MessageText:
//
//  Correcting a minor error in file %1.
//
#define MSG_CHK_NTFS_MINOR_CHANGES_TO_FRS 0x000065CAL

//
// MessageId: MSG_CHK_NTFS_BAD_UPCASE_TABLE
//
// MessageText:
//
//  Corrupt uppercase Table.
//  Using current system uppercase Table.
//
#define MSG_CHK_NTFS_BAD_UPCASE_TABLE    0x000065CBL

//
// MessageId: MSG_CHK_NTFS_CANT_GET_UPCASE_TABLE
//
// MessageText:
//
//  Cannot retrieve current system uppercase table.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_GET_UPCASE_TABLE 0x000065CCL

//
// MessageId: MSG_CHK_NTFS_MINOR_MFT_BITMAP_ERROR
//
// MessageText:
//
//  CHKDSK discovered free space marked as allocated in the
//  master file table (MFT) bitmap.
//
#define MSG_CHK_NTFS_MINOR_MFT_BITMAP_ERROR 0x000065CDL

//
// MessageId: MSG_CHK_NTFS_MINOR_VOLUME_BITMAP_ERROR
//
// MessageText:
//
//  CHKDSK discovered free space marked as allocated in the volume bitmap.
//
#define MSG_CHK_NTFS_MINOR_VOLUME_BITMAP_ERROR 0x000065CEL

//
// MessageId: MSG_CHK_NTFS_CORRECTING_UPCASE_FILE
//
// MessageText:
//
//  Correcting errors in the uppercase file.
//
#define MSG_CHK_NTFS_CORRECTING_UPCASE_FILE 0x000065CFL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_UPCASE_FILE
//
// MessageText:
//
//  Insufficient disk space to fix the uppercase file.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_UPCASE_FILE 0x000065D0L

//
// MessageId: MSG_CHK_NTFS_DELETING_INDEX_ENTRY
//
// MessageText:
//
//  Deleting index entry %3 in index %2 of file %1.
//
#define MSG_CHK_NTFS_DELETING_INDEX_ENTRY 0x000065D1L

//
// MessageId: MSG_CHK_NTFS_SLASH_V_NOT_SUPPORTED
//
// MessageText:
//
//  Verbose output not supported by NTFS CHKDSK.
//
#define MSG_CHK_NTFS_SLASH_V_NOT_SUPPORTED 0x000065D2L

//
// MessageId: MSG_CHK_NTFS_READ_ONLY_MODE
//
// MessageText:
//
//  
//  WARNING!  F parameter not specified.
//  Running CHKDSK in read-only mode.
//
#define MSG_CHK_NTFS_READ_ONLY_MODE      0x000065D3L

//
// MessageId: MSG_CHK_NTFS_ERRORS_FOUND
//
// MessageText:
//
//  
//  Errors found.  CHKDSK cannot continue in read-only mode.
//
#define MSG_CHK_NTFS_ERRORS_FOUND        0x000065D4L

//
// MessageId: MSG_CHK_NTFS_CYCLES_IN_DIR_TREE
//
// MessageText:
//
//  Correcting cycles in directory tree.
//  Breaking links between parent file %1 and child file %2.
//
#define MSG_CHK_NTFS_CYCLES_IN_DIR_TREE  0x000065D5L

//
// MessageId: MSG_CHK_NTFS_MINOR_FILE_NAME_ERRORS
//
// MessageText:
//
//  Correcting minor file name errors in file %1.
//
#define MSG_CHK_NTFS_MINOR_FILE_NAME_ERRORS 0x000065D6L

//
// MessageId: MSG_CHK_NTFS_MISSING_DATA_ATTRIBUTE
//
// MessageText:
//
//  Inserting data attribute into file %1.
//
#define MSG_CHK_NTFS_MISSING_DATA_ATTRIBUTE 0x000065D7L

//
// MessageId: MSG_CHK_NTFS_CANT_PUT_DATA_ATTRIBUTE
//
// MessageText:
//
//  Insufficient disk space to insert missing data attribute.
//
#define MSG_CHK_NTFS_CANT_PUT_DATA_ATTRIBUTE 0x000065D8L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_LOG_FILE
//
// MessageText:
//
//  Correcting errors in the Log File.
//
#define MSG_CHK_NTFS_CORRECTING_LOG_FILE 0x000065D9L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_LOG_FILE
//
// MessageText:
//
//  Insufficient disk space to fix the log file.
//  CHKDSK aborted.
//
#define MSG_CHK_NTFS_CANT_FIX_LOG_FILE   0x000065DAL

//
// MessageId: MSG_CHK_NTFS_CHECKING_FILES
//
// MessageText:
//
//  
//  CHKDSK is verifying files (stage %1 of %2)...
//
#define MSG_CHK_NTFS_CHECKING_FILES      0x000065DBL

//
// MessageId: MSG_CHK_NTFS_CHECKING_INDICES
//
// MessageText:
//
//  CHKDSK is verifying indexes (stage %1 of %2)...
//
#define MSG_CHK_NTFS_CHECKING_INDICES    0x000065DCL

//
// MessageId: MSG_CHK_NTFS_INDEX_VERIFICATION_COMPLETED
//
// MessageText:
//
//  Index verification completed.
//
#define MSG_CHK_NTFS_INDEX_VERIFICATION_COMPLETED 0x000065DDL

//
// MessageId: MSG_CHK_NTFS_FILE_VERIFICATION_COMPLETED
//
// MessageText:
//
//  File verification completed.
//
#define MSG_CHK_NTFS_FILE_VERIFICATION_COMPLETED 0x000065DEL

//
// MessageId: MSG_CHK_NTFS_CHECKING_SECURITY
//
// MessageText:
//
//  CHKDSK is verifying security descriptors (stage %1 of %2)...
//
#define MSG_CHK_NTFS_CHECKING_SECURITY   0x000065DFL

//
// MessageId: MSG_CHK_NTFS_SECURITY_VERIFICATION_COMPLETED
//
// MessageText:
//
//  Security descriptor verification completed.
//
#define MSG_CHK_NTFS_SECURITY_VERIFICATION_COMPLETED 0x000065E0L

//
// MessageId: MSG_CHK_NTFS_INVALID_SECURITY_DESCRIPTOR
//
// MessageText:
//
//  Replacing missing or invalid security descriptor for file %1.
//
#define MSG_CHK_NTFS_INVALID_SECURITY_DESCRIPTOR 0x000065E1L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_SECURITY
//
// MessageText:
//
//  Insufficient disk space for security descriptor for file %1.
//
#define MSG_CHK_NTFS_CANT_FIX_SECURITY   0x000065E2L

//
// MessageId: MSG_CHK_NTFS_WRONG_VERSION
//
// MessageText:
//
//  This volume cannot be checked with this version of UNTFS.DLL.
//
#define MSG_CHK_NTFS_WRONG_VERSION       0x000065E3L

//
// MessageId: MSG_CHK_NTFS_DELETING_GENERIC_INDEX_ENTRY
//
// MessageText:
//
//  Deleting an index entry from index %2 of file %1.
//
#define MSG_CHK_NTFS_DELETING_GENERIC_INDEX_ENTRY 0x000065E4L

//
// MessageId: MSG_CHK_NTFS_CORRECTING_CROSS_LINK
//
// MessageText:
//
//  Correcting cross-link for file %1.
//
#define MSG_CHK_NTFS_CORRECTING_CROSS_LINK 0x000065E5L

//
// MessageId: MSG_CHK_NTFS_VERIFYING_FILE_DATA
//
// MessageText:
//
//  CHKDSK is verifying file data (stage %1 of %2)...
//
#define MSG_CHK_NTFS_VERIFYING_FILE_DATA 0x000065E6L

//
// MessageId: MSG_CHK_NTFS_VERIFYING_FILE_DATA_COMPLETED
//
// MessageText:
//
//  File data verification completed.
//
#define MSG_CHK_NTFS_VERIFYING_FILE_DATA_COMPLETED 0x000065E7L

//
// MessageId: MSG_CHK_NTFS_TOO_MANY_FILE_NAMES
//
// MessageText:
//
//  Index entries referencing file %1 will not be validated
//  because this file contains too many file names.
//
#define MSG_CHK_NTFS_TOO_MANY_FILE_NAMES 0x000065E8L

//
// MessageId: MSG_CHK_NTFS_RESETTING_LSNS
//
// MessageText:
//
//  CHKDSK is resetting recovery information...
//
#define MSG_CHK_NTFS_RESETTING_LSNS      0x000065E9L

//
// MessageId: MSG_CHK_NTFS_RESETTING_LOG_FILE
//
// MessageText:
//
//  CHKDSK is resetting the log file.
//
#define MSG_CHK_NTFS_RESETTING_LOG_FILE  0x000065EAL

//
// MessageId: MSG_CHK_NTFS_RESIZING_LOG_FILE
//
// MessageText:
//
//  CHKDSK is adjusting the size of the log file.
//
#define MSG_CHK_NTFS_RESIZING_LOG_FILE   0x000065EBL

//
// MessageId: MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED
//
// MessageText:
//
//  CHKDSK was unable to adjust the size of the log file.
//
#define MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED 0x000065ECL

//
// MessageId: MSG_CHK_NTFS_ADJUSTING_INSTANCE_TAGS
//
// MessageText:
//
//  Cleaning up instance tags for file %1.
//
#define MSG_CHK_NTFS_ADJUSTING_INSTANCE_TAGS 0x000065EDL

//
// MessageId: MSG_CHK_NTFS_FIX_ATTR
//
// MessageText:
//
//  Fixing corrupt attribute record (%1, %2)
//  in file record segment %3.
//
#define MSG_CHK_NTFS_FIX_ATTR            0x000065EEL

//
// MessageId: MSG_CHK_NTFS_LOGFILE_SPACE
//
// MessageText:
//
//  %1 KB occupied by the log file.
//
#define MSG_CHK_NTFS_LOGFILE_SPACE       0x000065EFL

//
// MessageId: MSG_CHK_READABLE_FRS_UNWRITEABLE
//
// MessageText:
//
//  Readable file record segment %1 is not writeable.
//
#define MSG_CHK_READABLE_FRS_UNWRITEABLE 0x000065F0L

//
// MessageId: MSG_CHK_NTFS_DEFAULT_QUOTA_ENTRY_MISSING
//
// MessageText:
//
//  Inserting default quota record into index %2 in file %1.
//
#define MSG_CHK_NTFS_DEFAULT_QUOTA_ENTRY_MISSING 0x000065F1L

//
// MessageId: MSG_CHK_NTFS_CREATING_DEFAULT_SECURITY_DESCRIPTOR
//
// MessageText:
//
//  Creating a default security descriptor.
//
#define MSG_CHK_NTFS_CREATING_DEFAULT_SECURITY_DESCRIPTOR 0x000065F2L

//
// MessageId: MSG_CHK_NTFS_CANNOT_SET_QUOTA_FLAG_OUT_OF_DATE
//
// MessageText:
//
//  Unable to set the quota out of date flag.
//
#define MSG_CHK_NTFS_CANNOT_SET_QUOTA_FLAG_OUT_OF_DATE 0x000065F3L

//
// MessageId: MSG_CHK_NTFS_REPAIRING_INDEX_ENTRY
//
// MessageText:
//
//  Repairing an index entry in index %2 of file %1.
//
#define MSG_CHK_NTFS_REPAIRING_INDEX_ENTRY 0x000065F4L

//
// MessageId: MSG_CHK_NTFS_INSERTING_INDEX_ENTRY
//
// MessageText:
//
//  Inserting an index entry into index %2 of file %1.
//
#define MSG_CHK_NTFS_INSERTING_INDEX_ENTRY 0x000065F5L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_SECURITY_DATA_STREAM
//
// MessageText:
//
//  Insufficient disk space to fix the security descriptors data stream.
//
#define MSG_CHK_NTFS_CANT_FIX_SECURITY_DATA_STREAM 0x000065F6L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_ATTRIBUTE
//
// MessageText:
//
//  Unable to write to attribute %1 of file %2.
//
#define MSG_CHK_NTFS_CANT_FIX_ATTRIBUTE  0x000065F7L

//
// MessageId: MSG_CHK_NTFS_CANT_READ_SECURITY_DATA_STREAM
//
// MessageText:
//
//  Unable to read the security descriptors data stream.
//
#define MSG_CHK_NTFS_CANT_READ_SECURITY_DATA_STREAM 0x000065F8L

//
// MessageId: MSG_CHK_NTFS_FIXING_SECURITY_DATA_STREAM_MIRROR
//
// MessageText:
//
//  Fixing mirror copy of the security descriptors data stream.
//
#define MSG_CHK_NTFS_FIXING_SECURITY_DATA_STREAM_MIRROR 0x000065F9L

//
// MessageId: MSG_CHK_NTFS_FIXING_COLLATION_RULE
//
// MessageText:
//
//  Fixing collation rule value for index %1 of file %2.
//
#define MSG_CHK_NTFS_FIXING_COLLATION_RULE 0x000065FAL

//
// MessageId: MSG_CHK_NTFS_CREATE_INDEX
//
// MessageText:
//
//  Creating index %1 for file %2.
//
#define MSG_CHK_NTFS_CREATE_INDEX        0x000065FBL

//
// MessageId: MSG_CHK_NTFS_REPAIRING_SECURITY_FRS
//
// MessageText:
//
//  Repairing the security file record segment.
//
#define MSG_CHK_NTFS_REPAIRING_SECURITY_FRS 0x000065FCL

//
// MessageId: MSG_CHK_NTFS_REPAIRING_UNREADABLE_SECURITY_DATA_STREAM
//
// MessageText:
//
//  Repairing the unreadable security descriptors data stream.
//
#define MSG_CHK_NTFS_REPAIRING_UNREADABLE_SECURITY_DATA_STREAM 0x000065FDL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_OBJID
//
// MessageText:
//
//  Insufficient disk space to fix the object id file.
//
#define MSG_CHK_NTFS_CANT_FIX_OBJID      0x000065FEL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_QUOTA
//
// MessageText:
//
//  Insufficient disk space to fix the quota file.
//
#define MSG_CHK_NTFS_CANT_FIX_QUOTA      0x000065FFL

//
// MessageId: MSG_CHK_NTFS_CREATE_OBJID
//
// MessageText:
//
//  Creating object id file.
//
#define MSG_CHK_NTFS_CREATE_OBJID        0x00006600L

//
// MessageId: MSG_CHK_NTFS_CREATE_QUOTA
//
// MessageText:
//
//  Creating quota file.
//
#define MSG_CHK_NTFS_CREATE_QUOTA        0x00006601L

//
// MessageId: MSG_CHK_NTFS_FIX_FLAGS
//
// MessageText:
//
//  Fixing flags for file record segment %1.
//
#define MSG_CHK_NTFS_FIX_FLAGS           0x00006602L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_SYSTEM_FILE
//
// MessageText:
//
//  Unable to correct an error in system file %1.
//
#define MSG_CHK_NTFS_CANT_FIX_SYSTEM_FILE 0x00006603L

//
// MessageId: MSG_CHK_NTFS_CANT_CREATE_INDEX
//
// MessageText:
//
//  Unable to create index %1 for file %2.
//
#define MSG_CHK_NTFS_CANT_CREATE_INDEX   0x00006604L

//
// MessageId: MSG_CHK_NTFS_INVALID_SECURITY_ID
//
// MessageText:
//
//  Replacing invalid security id with default security id for file %1.
//
#define MSG_CHK_NTFS_INVALID_SECURITY_ID 0x00006605L

//
// MessageId: MSG_CHK_NTFS_MULTIPLE_QUOTA_FILE
//
// MessageText:
//
//  Multiple quota file found.  Ignoring extra quota files.
//
#define MSG_CHK_NTFS_MULTIPLE_QUOTA_FILE 0x00006606L

//
// MessageId: MSG_CHK_NTFS_MULTIPLE_OBJECTID_FILE
//
// MessageText:
//
//  Multiple object id file found.  Ignoring extra object id files.
//
#define MSG_CHK_NTFS_MULTIPLE_OBJECTID_FILE 0x00006607L

//
// MessageId: MSG_CHK_NTFS_SPECIFIED_LOGFILE_SIZE_TOO_BIG
//
// MessageText:
//
//  The size specified for the log file is too big.
//
#define MSG_CHK_NTFS_SPECIFIED_LOGFILE_SIZE_TOO_BIG 0x00006608L

//
// MessageId: MSG_CHK_NTFS_LOGFILE_SIZE
//
// MessageText:
//
//  The current log file size is %1 KB.
//  The default log file size for this volume is %2 KB.
//
#define MSG_CHK_NTFS_LOGFILE_SIZE        0x00006609L

//
// MessageId: MSG_CHK_NTFS_SPECIFIED_LOGFILE_SIZE_TOO_SMALL
//
// MessageText:
//
//  The size specified for the log file is too small.
//
#define MSG_CHK_NTFS_SPECIFIED_LOGFILE_SIZE_TOO_SMALL 0x0000660CL

//
// Dummy message for debug purpose
//
//
// MessageId: MSG_CHK_NTFS_MESSAGE
//
// MessageText:
//
//  %1%2.
//
#define MSG_CHK_NTFS_MESSAGE             0x0000660DL

//
// MessageId: MSG_CHK_NTFS_FIX_SYSTEM_FILE_NAME
//
// MessageText:
//
//  Correcting file name errors in system file record segment %1.
//
#define MSG_CHK_NTFS_FIX_SYSTEM_FILE_NAME 0x0000660EL

//
// MessageId: MSG_CHK_NTFS_INSERTING_INDEX_ENTRY_WITH_ID
//
// MessageText:
//
//  Inserting an index entry with Id %3 into index %2 of file %1.
//
#define MSG_CHK_NTFS_INSERTING_INDEX_ENTRY_WITH_ID 0x0000660FL

//
// MessageId: MSG_CHK_NTFS_DELETING_INDEX_ENTRY_WITH_ID
//
// MessageText:
//
//  Deleting an index entry with Id %3 from index %2 of file %1.
//
#define MSG_CHK_NTFS_DELETING_INDEX_ENTRY_WITH_ID 0x00006610L

//
// MessageId: MSG_CHK_NTFS_DELETING_UNUSED_INDEX_ENTRY
//
// MessageText:
//
//  Cleaning up %3 unused index entries from index %2 of file %1.
//
#define MSG_CHK_NTFS_DELETING_UNUSED_INDEX_ENTRY 0x00006611L

//
// MessageId: MSG_CHK_NTFS_REPAIRING_INDEX_ENTRY_WITH_ID
//
// MessageText:
//
//  Repairing an index entry with id %3 in index %2 of file %1.
//
#define MSG_CHK_NTFS_REPAIRING_INDEX_ENTRY_WITH_ID 0x00006612L

//
// MessageId: MSG_CHK_NTFS_SPARSE_FILE
//
// MessageText:
//
//  Correcting sparse file record segment %1.
//
#define MSG_CHK_NTFS_SPARSE_FILE         0x00006613L

//
// MessageId: MSG_CHK_NTFS_DELETING_EA_SET
//
// MessageText:
//
//  Deleting extended attribute set
//  due to the presence of reparse point in file %1.
//
#define MSG_CHK_NTFS_DELETING_EA_SET     0x00006614L

//
// MessageId: MSG_CHK_NTFS_REPARSE_POINT
//
// MessageText:
//
//  Correcting reparse point file record segment %1.
//
#define MSG_CHK_NTFS_REPARSE_POINT       0x00006615L

//
// MessageId: MSG_CHK_NTFS_ENCRYPTED_FILE
//
// MessageText:
//
//  Correcting encrypted file record segment %1.
//
#define MSG_CHK_NTFS_ENCRYPTED_FILE      0x00006616L

//
// MessageId: MSG_CHK_NTFS_MULTIPLE_USNJRNL_FILE
//
// MessageText:
//
//  Multiple Usn Journal file found.  Ignoring extra Usn Journal files.
//
#define MSG_CHK_NTFS_MULTIPLE_USNJRNL_FILE 0x00006617L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_USNJRNL
//
// MessageText:
//
//  Insufficient disk space to fix the Usn Journal file.
//
#define MSG_CHK_NTFS_CANT_FIX_USNJRNL    0x00006618L

//
// MessageId: MSG_CHK_NTFS_CREATE_USNJRNL
//
// MessageText:
//
//  Creating Usn Journal file.
//
#define MSG_CHK_NTFS_CREATE_USNJRNL      0x00006619L

//
// MessageId: MSG_CHK_NTFS_CHECKING_USNJRNL
//
// MessageText:
//
//  CHKDSK is verifying Usn Journal...
//
#define MSG_CHK_NTFS_CHECKING_USNJRNL    0x0000661AL

//
// MessageId: MSG_CHK_NTFS_CREATE_USNJRNL_DATA
//
// MessageText:
//
//  Creating Usn Journal %1 data stream
//
#define MSG_CHK_NTFS_CREATE_USNJRNL_DATA 0x0000661BL

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_USN_DATA_STREAM
//
// MessageText:
//
//  Insufficient disk space to fix the Usn Journal %1 data stream.
//
#define MSG_CHK_NTFS_CANT_FIX_USN_DATA_STREAM 0x0000661CL

//
// MessageId: MSG_CHK_NTFS_REPAIR_USN_DATA_STREAM
//
// MessageText:
//
//  Repairing Usn Journal %1 data stream.
//
#define MSG_CHK_NTFS_REPAIR_USN_DATA_STREAM 0x0000661DL

//
// MessageId: MSG_CHK_NTFS_CANT_READ_USN_DATA_STREAM
//
// MessageText:
//
//  Unable to read the Usn Journal %1 data stream.
//
#define MSG_CHK_NTFS_CANT_READ_USN_DATA_STREAM 0x0000661EL

//
// MessageId: MSG_CHK_NTFS_REPAIRING_USN_FRS
//
// MessageText:
//
//  Repairing Usn Journal file record segment.
//
#define MSG_CHK_NTFS_REPAIRING_USN_FRS   0x0000661FL

//
// MessageId: MSG_CHK_NTFS_USNJRNL_VERIFICATION_COMPLETED
//
// MessageText:
//
//  Usn Journal verification completed.
//
#define MSG_CHK_NTFS_USNJRNL_VERIFICATION_COMPLETED 0x00006620L

//
// MessageId: MSG_CHK_NTFS_RESETTING_USNS
//
// MessageText:
//
//  CHKDSK is resetting Usn information...
//
#define MSG_CHK_NTFS_RESETTING_USNS      0x00006621L

//
// MessageId: MSG_CHK_NTFS_UNABLE_TO_DOWNGRADE
//
// MessageText:
//
//  This version of NTFS volume cannot be downgraded.
//
#define MSG_CHK_NTFS_UNABLE_TO_DOWNGRADE 0x00006622L

//
// MessageId: MSG_CHK_NTFS_DOWNGRADE_SCANNING
//
// MessageText:
//
//  CHKDSK is determining if the volume is downgradeable...
//
#define MSG_CHK_NTFS_DOWNGRADE_SCANNING  0x00006623L

//
// MessageId: MSG_CHK_NTFS_DOWNGRADE_SCANNING_COMPLETED
//
// MessageText:
//
//  The volume is downgradeable.
//
#define MSG_CHK_NTFS_DOWNGRADE_SCANNING_COMPLETED 0x00006624L

//
// MessageId: MSG_CHK_NTFS_DOWNGRADE
//
// MessageText:
//
//  CHKDSK is downgrading the volume...
//
#define MSG_CHK_NTFS_DOWNGRADE           0x00006625L

//
// MessageId: MSG_CHK_NTFS_DOWNGRADE_COMPLETED
//
// MessageText:
//
//  Volume downgraded.                              %b
//
#define MSG_CHK_NTFS_DOWNGRADE_COMPLETED 0x00006626L

//
// MessageId: MSG_CHK_NTFS_REPARSE_POINT_FOUND
//
// MessageText:
//
//  Reparse point found in file record segment %1.
//
#define MSG_CHK_NTFS_REPARSE_POINT_FOUND 0x00006627L

//
// MessageId: MSG_CHK_NTFS_ENCRYPTED_FILE_FOUND
//
// MessageText:
//
//  Encrypted data stream found in file record segment %1.
//
#define MSG_CHK_NTFS_ENCRYPTED_FILE_FOUND 0x00006628L

//
// MessageId: MSG_CHK_NTFS_SPARSE_FILE_FOUND
//
// MessageText:
//
//  Sparse data stream found in file record segment %1.
//
#define MSG_CHK_NTFS_SPARSE_FILE_FOUND   0x00006629L

//
// MessageId: MSG_CHK_NTFS_BAD_VOLUME_VERSION
//
// MessageText:
//
//  Unable to change the volume version.
//
#define MSG_CHK_NTFS_BAD_VOLUME_VERSION  0x0000662AL

//
// MessageId: MSG_CHK_NTFS_UNABLE_DOWNGRADE
//
// MessageText:
//
//  Unable to downgrade the volume.
//
#define MSG_CHK_NTFS_UNABLE_DOWNGRADE    0x0000662BL

//
// MessageId: MSG_CHK_NTFS_UPDATING_MFT_DATA
//
// MessageText:
//
//  Updating the master file table's (MFT) DATA attribute.
//
#define MSG_CHK_NTFS_UPDATING_MFT_DATA   0x0000662CL

//
// MessageId: MSG_CHK_NTFS_UPDATING_MFT_BITMAP
//
// MessageText:
//
//  Updating the master file table's (MFT) BITMAP attribute.
//
#define MSG_CHK_NTFS_UPDATING_MFT_BITMAP 0x0000662DL

//
// MessageId: MSG_CHK_NTFS_UPDATING_VOLUME_BITMAP
//
// MessageText:
//
//  Updating the Volume Bitmap.
//
#define MSG_CHK_NTFS_UPDATING_VOLUME_BITMAP 0x0000662EL

//
// MessageId: MSG_CHK_NTFS_UPGRADE_DOWNGRADE
//
// MessageText:
//
//  Upgrading and downgrading the volume at the same time will have no effect.
//
#define MSG_CHK_NTFS_UPGRADE_DOWNGRADE   0x0000662FL

//
// MessageId: MSG_CHK_NTFS_PROPERTY_SET_FOUND
//
// MessageText:
//
//  Property set found in file record segment %1.
//
#define MSG_CHK_NTFS_PROPERTY_SET_FOUND  0x00006630L

//
// MessageId: MSG_CHK_NTFS_MULTIPLE_REPARSE_FILE
//
// MessageText:
//
//  Multiple reparse file found.  Ignoring extra reparse files.
//
#define MSG_CHK_NTFS_MULTIPLE_REPARSE_FILE 0x00006631L

//
// MessageId: MSG_CHK_NTFS_CANT_FIX_REPARSE
//
// MessageText:
//
//  Insufficient disk space to fix the reparse point file.
//
#define MSG_CHK_NTFS_CANT_FIX_REPARSE    0x00006632L

//
// MessageId: MSG_CHK_NTFS_CREATE_REPARSE
//
// MessageText:
//
//  Creating reparse point file.
//
#define MSG_CHK_NTFS_CREATE_REPARSE      0x00006633L

//
// MessageId: MSG_CHK_NTFS_MISSING_DUPLICATE_OBJID
//
// MessageText:
//
//  Missing object id index entry or duplicate object id detected
//  for file record segment %1.
//
#define MSG_CHK_NTFS_MISSING_DUPLICATE_OBJID 0x00006634L

//
// MessageId: MSG_CHK_NTFS_CANT_INSERT_INDEX_ENTRY
//
// MessageText:
//
//  Insufficient disk space to insert the index entry.
//
#define MSG_CHK_NTFS_CANT_INSERT_INDEX_ENTRY 0x00006635L

//
// MessageId: MSG_CHK_NTFS_SKIP_INDEX_SCAN
//
// MessageText:
//
//  WARNING!  I parameter specified.
//
#define MSG_CHK_NTFS_SKIP_INDEX_SCAN     0x00006636L

//
// MessageId: MSG_CHK_NTFS_SKIP_CYCLE_SCAN
//
// MessageText:
//
//  WARNING!  C parameter specified.
//
#define MSG_CHK_NTFS_SKIP_CYCLE_SCAN     0x00006637L

//
// MessageId: MSG_CHK_NTFS_SKIP_SCAN_WARNING
//
// MessageText:
//
//  Your drive may still be corrupt even after running CHKDSK.
//
#define MSG_CHK_NTFS_SKIP_SCAN_WARNING   0x00006638L

//
// MessageId: MSG_CHK_NTFS_TOTAL_DISK_SPACE_IN_MB
//
// MessageText:
//
//  
//  %1 MB total disk space.
//
#define MSG_CHK_NTFS_TOTAL_DISK_SPACE_IN_MB 0x00006639L

//
// MessageId: MSG_CHK_NTFS_USER_FILES_IN_MB
//
// MessageText:
//
//  %1 MB in %2 files.
//
#define MSG_CHK_NTFS_USER_FILES_IN_MB    0x0000663AL

//
// MessageId: MSG_CHK_NTFS_INDICES_REPORT_IN_MB
//
// MessageText:
//
//  %1 MB in %2 indexes.
//
#define MSG_CHK_NTFS_INDICES_REPORT_IN_MB 0x0000663BL

//
// MessageId: MSG_CHK_NTFS_BAD_SECTORS_REPORT_IN_MB
//
// MessageText:
//
//  %1 MB in bad sectors.
//
#define MSG_CHK_NTFS_BAD_SECTORS_REPORT_IN_MB 0x0000663CL

//
// MessageId: MSG_CHK_NTFS_SYSTEM_SPACE_IN_MB
//
// MessageText:
//
//  %1 MB in use by the system.
//
#define MSG_CHK_NTFS_SYSTEM_SPACE_IN_MB  0x0000663DL

//
// MessageId: MSG_CHK_NTFS_AVAILABLE_SPACE_IN_MB
//
// MessageText:
//
//  %1 MB available on disk.
//  
//
#define MSG_CHK_NTFS_AVAILABLE_SPACE_IN_MB 0x0000663EL

//
// MessageId: MSG_CHK_NTFS_CANNOT_SET_VOLUME_CHKDSK_RAN_FLAG
//
// MessageText:
//
//  Unable to set chkdsk ran flag.
//
#define MSG_CHK_NTFS_CANNOT_SET_VOLUME_CHKDSK_RAN_FLAG 0x0000663FL

//
// MessageId: MSG_CHK_NTFS_DELETING_USNJRNL
//
// MessageText:
//
//  Deleting USN Journal file.
//
#define MSG_CHK_NTFS_DELETING_USNJRNL    0x00006640L

//
// MessageId: MSG_CHK_NTFS_RECOVERING_FREE_SPACE
//
// MessageText:
//
//  CHKDSK is verifying free space (stage %1 of %2)...
//
#define MSG_CHK_NTFS_RECOVERING_FREE_SPACE 0x00006641L

//
// MessageId: MSG_CHK_NTFS_DELETING_DUPLICATE_OBJID
//
// MessageText:
//
//  Deleting duplicate object id from file record segment %1.
//
#define MSG_CHK_NTFS_DELETING_DUPLICATE_OBJID 0x00006642L

//
// MessageId: MSG_CHK_UNABLE_TO_CREATE_THREAD
//
// MessageText:
//
//  Unable to create a thread with error code %1.
//
#define MSG_CHK_UNABLE_TO_CREATE_THREAD  0x00006643L

//
// Do not change this Message Id 26180 as test script depends on it
//
//
// MessageId: MSG_CHK_NTFS_EVENTLOG
//
// MessageText:
//
//  %1
//
#define MSG_CHK_NTFS_EVENTLOG            0x00006644L

//
// MessageId: MSG_CHK_NTFS_UNABLE_TO_COLLECT_LOGGED_MESSAGES
//
// MessageText:
//
//  Unable to collect logged messages.
//
#define MSG_CHK_NTFS_UNABLE_TO_COLLECT_LOGGED_MESSAGES 0x00006645L

//
// MessageId: MSG_CHK_NTFS_UNABLE_TO_OBTAIN_EVENTLOG_HANDLE
//
// MessageText:
//
//  Unable to obtain a handle to the event log.
//
#define MSG_CHK_NTFS_UNABLE_TO_OBTAIN_EVENTLOG_HANDLE 0x00006646L

//
// MessageId: MSG_CHK_NTFS_FAILED_TO_CREATE_EVENTLOG
//
// MessageText:
//
//  Failed to transfer logged messages to the event log with status %1.
//
#define MSG_CHK_NTFS_FAILED_TO_CREATE_EVENTLOG 0x00006647L

//
// MessageId: MSG_CHK_NTFS_VOLUME_LABEL
//
// MessageText:
//
//  Volume label is %1.
//
#define MSG_CHK_NTFS_VOLUME_LABEL        0x00006648L

//
// MessageId: MSG_CHK_NTFS_CANNOT_SET_VOLUME_CHKDSK_RAN_ONCE_FLAG
//
// MessageText:
//
//  Unable to set chkdsk ran once flag.
//
#define MSG_CHK_NTFS_CANNOT_SET_VOLUME_CHKDSK_RAN_ONCE_FLAG 0x00006649L

//
// MessageId: MSG_CHK_NTFS_CANNOT_CLEAR_VOLUME_CHKDSK_RAN_ONCE_FLAG
//
// MessageText:
//
//  Unable to clear chkdsk ran once flag.
//
#define MSG_CHK_NTFS_CANNOT_CLEAR_VOLUME_CHKDSK_RAN_ONCE_FLAG 0x0000664AL

//
// MessageId: MSG_CHK_NTFS_CLEANUP_UNUSED_SECURITY_DESCRIPTORS
//
// MessageText:
//
//  Cleaning up %1 unused security descriptors.
//
#define MSG_CHK_NTFS_CLEANUP_UNUSED_SECURITY_DESCRIPTORS 0x0000664BL

//
// MessageId: MSG_NTFS_CHKDSK_ERRORS_DETECTED
//
// MessageText:
//
//  Detected minor inconsistencies on the drive.  This is not a corruption.
//
#define MSG_NTFS_CHKDSK_ERRORS_DETECTED  0x0000664CL

//
// MessageId: MSG_NTFS_CHKDSK_ERRORS_FIXED
//
// MessageText:
//
//  Cleaning up minor inconsistencies on the drive.
//
#define MSG_NTFS_CHKDSK_ERRORS_FIXED     0x0000664DL

//
// MessageId: MSG_CHK_NTFS_BAD_CLUSTERS_IN_LOG_FILE
//
// MessageText:
//
//  Replacing bad clusters in logfile.
//
#define MSG_CHK_NTFS_BAD_CLUSTERS_IN_LOG_FILE 0x0000664EL

//
// MessageId: MSG_CHK_NTFS_DELETING_USNJRNL_UNDERWAY
//
// MessageText:
//
//  The deleting of the USN Journal is in progress.
//  Your volume cannot be checked at this time.
//  Please wait a few minutes and try again.
//
#define MSG_CHK_NTFS_DELETING_USNJRNL_UNDERWAY 0x0000664FL

//
// MessageId: MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED_DUE_TO_ATTR_LIST
//
// MessageText:
//
//  CHKDSK was unable to adjust the size of the log file
//  due to too much fragmentation within the volume.
//
#define MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED_DUE_TO_ATTR_LIST 0x00006650L

//
// MessageId: MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED_DUE_TO_ATTR_LIST_PRESENT
//
// MessageText:
//
//  CHKDSK was unable to adjust the size of the log file.
//  Please use the /f parameter as well.
//
#define MSG_CHK_NTFS_RESIZING_LOG_FILE_FAILED_DUE_TO_ATTR_LIST_PRESENT 0x00006651L

//
// MessageId: MSG_CHK_NTFS_ATTR_LIST_IN_LOG_FILE
//
// MessageText:
//
//  Attribute list found in the log file.
//
#define MSG_CHK_NTFS_ATTR_LIST_IN_LOG_FILE 0x00006652L

//
// MessageId: MSG_CHK_NTFS_OUT_OF_SPACE_FOR_SPECIFIED_LOGFILE_SIZE
//
// MessageText:
//
//  There is not enough disk space to enlarge the logfile to the specified size.
//
#define MSG_CHK_NTFS_OUT_OF_SPACE_FOR_SPECIFIED_LOGFILE_SIZE 0x00006653L

//
// MessageId: MSG_CHK_NTFS_OUT_OF_SPACE_TO_ENLARGE_LOGFILE_TO_DEFAULT_SIZE
//
// MessageText:
//
//  There is not enough disk space to enlarge the logfile to the default size.
//
#define MSG_CHK_NTFS_OUT_OF_SPACE_TO_ENLARGE_LOGFILE_TO_DEFAULT_SIZE 0x00006654L

//
// MessageId: MSG_CHK_NTFS_LOGFILE_SIZE_TOO_BIG
//
// MessageText:
//
//  The current logfile size is too big.
//
#define MSG_CHK_NTFS_LOGFILE_SIZE_TOO_BIG 0x00006655L

//
// MessageId: MSG_CHK_NTFS_MORE_MEMORY_IS_NEEDED_TO_RUN_AT_FULL_SPEED
//
// MessageText:
//
//  %1 MB of additional physical memory is needed
//  to enable windows to check your disk at full speed.
//
#define MSG_CHK_NTFS_MORE_MEMORY_IS_NEEDED_TO_RUN_AT_FULL_SPEED 0x00006656L

//
// MessageId: MSG_CHK_NTFS_PASSES_NEEDED
//
// MessageText:
//
//  Index verification stage will be splitted into %1 passes.
//
#define MSG_CHK_NTFS_PASSES_NEEDED       0x00006657L

//
// MessageId: MSG_CHK_NTFS_SLOWER_ALGORITHM
//
// MessageText:
//
//  Windows is checking your disk with a slower index verification algorithm
//  as commanded.
//
#define MSG_CHK_NTFS_SLOWER_ALGORITHM    0x00006658L

//
// MessageId: MSG_CHK_NTFS_ADJUST_INDEX_PASSES
//
// MessageText:
//
//  Index verification stage cannot be splitted into %1 passes.
//  It will be reduced.
//
#define MSG_CHK_NTFS_ADJUST_INDEX_PASSES 0x00006659L

//
// MessageId: MSG_CHK_NTFS_TOO_MANY_FILES_TO_RUN_AT_FULL_SPEED
//
// MessageText:
//
//  There are too many files and directories on this volume for windows to
//  check your disk at full speed.  Make sure the allowed user address space
//  is set to a maximum.
//
#define MSG_CHK_NTFS_TOO_MANY_FILES_TO_RUN_AT_FULL_SPEED 0x0000665AL

//
// MessageId: MSG_CHK_NTFS_TOO_MANY_PASSES
//
// MessageText:
//
//  An attempt to split the index verification stage into %1 or less passes
//  has failed.  Windows will not be able to check your disk at full speed.
//
#define MSG_CHK_NTFS_TOO_MANY_PASSES     0x0000665BL

//
// MessageId: MSG_CHK_NTFS_RESIZING_LOGFILE_BUT_DIRTY
//
// MessageText:
//
//  Windows cannot adjust the size of the log file due to possible
//  corruption.  Please run CHKDSK with the /F (fix) option to correct these.
//
#define MSG_CHK_NTFS_RESIZING_LOGFILE_BUT_DIRTY 0x0000665CL

//---------------
//
// NTFS Chkdsk logging messages.
//
//---------------
//
// MessageId: MSG_CHKLOG_NTFS_TOO_MANY_FILES
//
// MessageText:
//
//  Volume has 0x%1 file record segments which is more than 32 bits.
//
#define MSG_CHKLOG_NTFS_TOO_MANY_FILES   0x00006978L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_FILE_NAME_INDEX_PRESENT_BIT
//
// MessageText:
//
//  The file name index present bit is not set for file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_FILE_NAME_INDEX_PRESENT_BIT 0x00006979L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_VIEW_INDEX_PRESENT_BIT
//
// MessageText:
//
//  The view index present bit is not set for file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_VIEW_INDEX_PRESENT_BIT 0x0000697AL

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_SYSTEM_FILE_BIT
//
// MessageText:
//
//  The system file bit is not set for file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_SYSTEM_FILE_BIT 0x0000697BL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_IS_MISSING
//
// MessageText:
//
//  The %2 index is missing from file 0x%1.
//
#define MSG_CHKLOG_NTFS_INDEX_IS_MISSING 0x0000697CL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_EA_INFO
//
// MessageText:
//
//  EA Information is incorrect.
//                   Actual          On Disk
//  PackedEaSize      0x%1            0x%4
//  NeedEaCount       0x%2            0x%5
//  UnpackedEaSize    0x%3            0x%6
//
#define MSG_CHKLOG_NTFS_INCORRECT_EA_INFO 0x0000697DL

//
// MessageId: MSG_CHKLOG_NTFS_EA_INFO_XOR_EA_DATA
//
// MessageText:
//
//  The EA INFORMATION attribute is not consistency with the EA DATA attribute
//  for file 0x%1.  EA INFORMATION equals 0x%2 while EA DATA equals 0x%3.
//
#define MSG_CHKLOG_NTFS_EA_INFO_XOR_EA_DATA 0x0000697EL

//
// MessageId: MSG_CHKLOG_NTFS_UNREADABLE_EA_INFO
//
// MessageText:
//
//  The EA INFORMATION is not readable for file 0x%1.
//
#define MSG_CHKLOG_NTFS_UNREADABLE_EA_INFO 0x0000697FL

//
// MessageId: MSG_CHKLOG_NTFS_EA_INFO_INCORRECT_SIZE
//
// MessageText:
//
//  The EA INFORMATION size, 0x%2, in file 0x%1 is incorrect.
//  The expected size is 0x%3.
//
#define MSG_CHKLOG_NTFS_EA_INFO_INCORRECT_SIZE 0x00006980L

//
// MessageId: MSG_CHKLOG_NTFS_UNREADABLE_EA_DATA
//
// MessageText:
//
//  The EA DATA is not readable for file 0x%1.
//
#define MSG_CHKLOG_NTFS_UNREADABLE_EA_DATA 0x00006981L

//
// MessageId: MSG_CHKLOG_NTFS_EA_DATA_INCORRECT_SIZE
//
// MessageText:
//
//  The EA DATA size, 0x%2, in file 0x%1 is incorrect.
//  The expected size is 0x%3.
//
#define MSG_CHKLOG_NTFS_EA_DATA_INCORRECT_SIZE 0x00006982L

//
// MessageId: MSG_CHKLOG_DUMP_DATA
//
// MessageText:
//
//  %1%2
//
#define MSG_CHKLOG_DUMP_DATA             0x00006983L

//
// MessageId: MSG_CHKLOG_NTFS_CORRUPT_EA_SET
//
// MessageText:
//
//  Corrupt EA set for file 0x%1.  The remaining length, 0x%2,
//  is too small.
//
#define MSG_CHKLOG_NTFS_CORRUPT_EA_SET   0x00006984L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_TOTAL_EA_SIZE
//
// MessageText:
//
//  Corrupt EA set for file 0x%1.  The unpacked total length, 0x%2,
//  is larger than the total data length, 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_TOTAL_EA_SIZE 0x00006985L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_EA_NAME
//
// MessageText:
//
//  Corrupt EA set for file 0x%1.  The EA name is of length 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_EA_NAME 0x00006986L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_EA_SIZE
//
// MessageText:
//
//  Corrupt EA set for file 0x%1.  The unpacked length, 0x%2,
//  is not the same as the record length, 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_EA_SIZE 0x00006987L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_EA_INFO_LENGTH
//
// MessageText:
//
//  The EA Information value length, 0x%1, in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_EA_INFO_LENGTH 0x00006988L

//
// MessageId: MSG_CHKLOG_NTFS_TOTAL_PACKED_TOO_LARGE
//
// MessageText:
//
//  The EA total packed length, 0x%2, is too large in file 0x%1.
//
#define MSG_CHKLOG_NTFS_TOTAL_PACKED_TOO_LARGE 0x00006989L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_STARTING_LCN
//
// MessageText:
//
//  The second MFT starting LCN in the boot sector is incorrect.
//  The actual value is 0x%2 while the expected value is 0x%1.
//
#define MSG_CHKLOG_NTFS_INCORRECT_STARTING_LCN 0x0000698AL

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_POINT_LENGTH_TOO_LARGE
//
// MessageText:
//
//  The reparse point length, 0x%1, has exceeded a maximum of 0x%2.
//
#define MSG_CHKLOG_NTFS_REPARSE_POINT_LENGTH_TOO_LARGE 0x0000698BL

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_POINT_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The reparse point length, 0x%1, is less than a minimum of 0x%2.
//
#define MSG_CHKLOG_NTFS_REPARSE_POINT_LENGTH_TOO_SMALL 0x0000698CL

//
// MessageId: MSG_CHKLOG_NTFS_UNREADABLE_REPARSE_POINT
//
// MessageText:
//
//  Unable to read reparse point data buffer.
//
#define MSG_CHKLOG_NTFS_UNREADABLE_REPARSE_POINT 0x0000698DL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_REPARSE_POINT_SIZE
//
// MessageText:
//
//  Only 0x%1 bytes returned from a read of 0x%d bytes
//  of the reparse data buffer.
//
#define MSG_CHKLOG_NTFS_INCORRECT_REPARSE_POINT_SIZE 0x0000698EL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_REPARSE_DATA_LENGTH
//
// MessageText:
//
//  ReparseDataLength, 0x%1, inconsistence with the attribute length 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_REPARSE_DATA_LENGTH 0x0000698FL

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_TAG_IS_RESERVED
//
// MessageText:
//
//  Reparse Tag, 0x%1, is a reserved tag.
//
#define MSG_CHKLOG_NTFS_REPARSE_TAG_IS_RESERVED 0x00006990L

//
// MessageId: MSG_CHKLOG_NTFS_BAD_REPARSE_POINT
//
// MessageText:
//
//  File 0x%1 has bad reparse point attribute.
//
#define MSG_CHKLOG_NTFS_BAD_REPARSE_POINT 0x00006992L

//
// MessageId: MSG_CHKLOG_NTFS_EA_INFORMATION_WITH_REPARSE_POINT
//
// MessageText:
//
//  Both reparse point and EA INFORMATION attributes exist in file 0x%1.
//
#define MSG_CHKLOG_NTFS_EA_INFORMATION_WITH_REPARSE_POINT 0x00006993L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_DEF_TABLE_LENGTH_NOT_IN_MULTIPLES_OF_ATTR_DEF_COLUMNS
//
// MessageText:
//
//  The attribute definition table length, 0x%1, is not divisible by 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_DEF_TABLE_LENGTH_NOT_IN_MULTIPLES_OF_ATTR_DEF_COLUMNS 0x00006994L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_LOCATE_CHILD_FRS
//
// MessageText:
//
//  Unable to find child frs 0x%1 with sequence number 0x%2.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_LOCATE_CHILD_FRS 0x00006995L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_LOCATE_ATTR_IN_ATTR_LIST
//
// MessageText:
//
//  Unable to locate attribute of type 0x%1, lowest vcn 0x%2,
//  instance tag 0x%3 in file 0x%4.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_LOCATE_ATTR_IN_ATTR_LIST 0x00006996L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LIST_WITHIN_ATTR_LIST
//
// MessageText:
//
//  The is an attribute list attribute within the attribute list in file 0x%1.
//
#define MSG_CHKLOG_NTFS_ATTR_LIST_WITHIN_ATTR_LIST 0x00006997L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LOWEST_VCN_IS_NOT_ZERO
//
// MessageText:
//
//  The lowest vcn, 0x%2, is not zero for attribute of type 0x%1
//  and instance tag 0x%3 in file 0x%4.
//
#define MSG_CHKLOG_NTFS_ATTR_LOWEST_VCN_IS_NOT_ZERO 0x00006998L

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_LOWEST_VCN_IS_NOT_ZERO
//
// MessageText:
//
//  The lowest vcn, 0x%2, is not zero for attribute of type 0x%1
//  in file 0x%3.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_LOWEST_VCN_IS_NOT_ZERO 0x00006999L

//
// MessageId: MSG_CHKLOG_NTFS_FIRST_ATTR_RECORD_CANNOT_BE_RESIDENT
//
// MessageText:
//
//  The first attribute of type 0x%1 and instance tag 0x%2
//  in file 0x%3 should not be resident.
//
#define MSG_CHKLOG_NTFS_FIRST_ATTR_RECORD_CANNOT_BE_RESIDENT 0x0000699AL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_CANNOT_BE_RESIDENT
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2
//  in file 0x%3 should not be resident.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_CANNOT_BE_RESIDENT 0x0000699BL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_TYPE_CODE_MISMATCH
//
// MessageText:
//
//  The attributes with instance tags 0x%2 and 0x%4 have different
//  type codes 0x%1 and 0x%3 respectively in file 0x%5.
//
#define MSG_CHKLOG_NTFS_ATTR_TYPE_CODE_MISMATCH 0x0000699CL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_VCN_NOT_CONTIGUOUS
//
// MessageText:
//
//  The attributes with same type code 0x%1 but different instance tags
//  0x%2 and 0x%4 have non-contiguous VCN numbers 0x%3 and 0x%5
//  respectively in file 0x%6.
//
#define MSG_CHKLOG_NTFS_ATTR_VCN_NOT_CONTIGUOUS 0x0000699DL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_NAME_MISMATCH
//
// MessageText:
//
//  The attributes with same type code 0x%1 but different instance tags
//  0x%2 and 0x%4 have different names %3 and %5
//  respectively in file 0x%6.
//
#define MSG_CHKLOG_NTFS_ATTR_NAME_MISMATCH 0x0000699EL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_INCORRECT_ALLOCATE_LENGTH
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 in file 0x%5
//  has allocated length of 0x%3 instead of 0x%4.
//
#define MSG_CHKLOG_NTFS_ATTR_INCORRECT_ALLOCATE_LENGTH 0x0000699FL

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_INCORRECT_ALLOCATE_LENGTH
//
// MessageText:
//
//  The attribute of type 0x%1 in file 0x%4 has allocated length
//  of 0x%2 instead of 0x%3.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_INCORRECT_ALLOCATE_LENGTH 0x000069A0L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_FILE_ATTR
//
// MessageText:
//
//  The file attributes flag 0x%1 in file 0x%3 is incorrect.
//  The expected value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INVALID_FILE_ATTR 0x000069A1L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_SEQUENCE_NUMBER
//
// MessageText:
//
//  The sequence number 0x%1 in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_SEQUENCE_NUMBER 0x000069A2L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_TOTAL_ALLOCATION
//
// MessageText:
//
//  The total allocated size 0x%3 of attribute of type 0x%1 and instance
//  tag 0x%2 in file 0x%5 is incorrect.  The expected value is %4.
//
#define MSG_CHKLOG_NTFS_INCORRECT_TOTAL_ALLOCATION 0x000069A3L

//
// MessageId: MSG_CHKLOG_READ_FAILURE
//
// MessageText:
//
//  Read failure with status 0x%1 at offset 0x%2 for 0x%3 bytes.
//
#define MSG_CHKLOG_READ_FAILURE          0x000069A4L

//
// MessageId: MSG_CHKLOG_READ_INCORRECT
//
// MessageText:
//
//  Incorrect read at offset 0x%1 for 0x%3 bytes but got 0x%2 bytes.
//
#define MSG_CHKLOG_READ_INCORRECT        0x000069A5L

//
// MessageId: MSG_CHKLOG_WRITE_FAILURE
//
// MessageText:
//
//  Write failure with status 0x%1 at offset 0x%2 for 0x%3 bytes.
//
#define MSG_CHKLOG_WRITE_FAILURE         0x000069A6L

//
// MessageId: MSG_CHKLOG_WRITE_INCORRECT
//
// MessageText:
//
//  Incorrect write at offset 0x%1 for 0x%3 bytes but wrote 0x%2 bytes.
//
#define MSG_CHKLOG_WRITE_INCORRECT       0x000069A7L

//
// MessageId: MSG_CHKLOG_READ_BACK_FAILURE
//
// MessageText:
//
//  The data written out is different from what is being read back
//  at offset 0x%1 for 0x%2 bytes.
//
#define MSG_CHKLOG_READ_BACK_FAILURE     0x000069A8L

//
// MessageId: MSG_CHKLOG_NTFS_FILENAME_HAS_INCORRECT_PARENT
//
// MessageText:
//
//  The file 0x%1 belongs to parent 0x%3 but got 0x%2 as parent.
//
#define MSG_CHKLOG_NTFS_FILENAME_HAS_INCORRECT_PARENT 0x000069A9L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FILENAME
//
// MessageText:
//
//  The file 0x%1 has file name %2 when it should be %3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FILENAME 0x000069AAL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_MULTI_SECTOR_HEADER
//
// MessageText:
//
//  The multi-sector header with total size 0x%1, USA offset 0x%2,
//  and USA count 0x%3 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_MULTI_SECTOR_HEADER 0x000069ABL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_USA
//
// MessageText:
//
//  The USA check value, 0x%2, at block 0x%1 is incorrect.
//  The expected value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_USA    0x000069ACL

//
// MessageId: MSG_CHKLOG_NTFS_QUERY_LCN_FROM_VCN_FAILED
//
// MessageText:
//
//  Unable to query LCN from VCN 0x%2 for attribute of type 0x%1.
//
#define MSG_CHKLOG_NTFS_QUERY_LCN_FROM_VCN_FAILED 0x000069ADL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_REC_CROSS_LINKED
//
// MessageText:
//
//  Attribute record of type 0x%1 and instance tag 0x%2 is cross linked
//  starting at 0x%3 for possibly 0x%4 clusters.
//
#define MSG_CHKLOG_NTFS_ATTR_REC_CROSS_LINKED 0x000069AEL

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_REC_CROSS_LINKED
//
// MessageText:
//
//  Attribute record of type 0x%1 is cross linked starting at
//  cluster 0x%2 for possibly 0x%3 clusters.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_TAG_ATTR_REC_CROSS_LINKED 0x000069AFL

//
// MessageId: MSG_CHKLOG_NTFS_STANDARD_INFORMATION_MISSING_FROM_ATTR_LIST
//
// MessageText:
//
//  The attribute list in file 0x%1 does not contain
//  standard information attribute.
//
#define MSG_CHKLOG_NTFS_STANDARD_INFORMATION_MISSING_FROM_ATTR_LIST 0x000069B0L

//
// MessageId: MSG_CHKLOG_NTFS_STANDARD_INFORMATION_OUTSIDE_BASE_FRS
//
// MessageText:
//
//  The attribute list in file 0x%1 indicates the standard information
//  attribute is outside the base file record segment.
//
#define MSG_CHKLOG_NTFS_STANDARD_INFORMATION_OUTSIDE_BASE_FRS 0x000069B1L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_INDEX_ROOT
//
// MessageText:
//
//  The index root %2 is missing in file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_INDEX_ROOT 0x000069B2L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_BITMAP_MISSING
//
// MessageText:
//
//  The index bitmap %2 is missing in file 0x%1.
//
#define MSG_CHKLOG_NTFS_INDEX_BITMAP_MISSING 0x000069B3L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_BITMAP
//
// MessageText:
//
//  The index bitmap %2 in file 0x%1 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_BITMAP 0x000069B4L

//
// MessageId: MSG_CHKLOG_NTFS_EXTRA_INDEX_BITMAP
//
// MessageText:
//
//  The index bitmap %2 is present but there is no corresponding
//  index allocation attribute in file 0x%1.
//
#define MSG_CHKLOG_NTFS_EXTRA_INDEX_BITMAP 0x000069B5L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The length, 0x%2, of the root index %1 in file 0x%4
//  is too small.  The minimum length is 0x%3.
//
#define MSG_CHKLOG_NTFS_INDEX_LENGTH_TOO_SMALL 0x000069B6L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_NAME
//
// MessageText:
//
//  The root index %1 in file 0x%3 is incorrect.
//  The expected name is %2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_NAME 0x000069B7L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_COLLATION_RULE
//
// MessageText:
//
//  The collation rule 0x%3 for index root %1 in file 0x%2
//  is incorrect.  The expected value is 0x%4.
//
#define MSG_CHKLOG_NTFS_INCORRECT_COLLATION_RULE 0x000069B8L

//
// MessageId: MSG_CHKLOG_NTFS_ORPHAN_CREATED_ON_BREAKING_CYCLE
//
// MessageText:
//
//  Breaking the parent 0x%1 and child 0x%2
//  file relationship.  This also makes the child an orphan.
//
#define MSG_CHKLOG_NTFS_ORPHAN_CREATED_ON_BREAKING_CYCLE 0x000069B9L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ATTR_TYPE
//
// MessageText:
//
//  The index attribute of type 0x%2 for index root %1
//  in file 0x%4 is incorrect.  The expected value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ATTR_TYPE 0x000069BAL

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_INDEX_NAME_FOR_QUOTA_FILE
//
// MessageText:
//
//  The index %1 is not a known quota index in file 0x%2.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_INDEX_NAME_FOR_QUOTA_FILE 0x000069BBL

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_INDEX_NAME_FOR_SECURITY_FILE
//
// MessageText:
//
//  The index %1 is not a known security index in file 0x%2.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_INDEX_NAME_FOR_SECURITY_FILE 0x000069BCL

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_INDEX_ATTR_TYPE
//
// MessageText:
//
//  The index attribute of type 0x%2 for index root %1
//  in file 0x%3 is not recognized.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_INDEX_ATTR_TYPE 0x000069BDL

//
// MessageId: MSG_CHKLOG_NTFS_NON_INDEXABLE_INDEX_ATTR_TYPE
//
// MessageText:
//
//  The index attribute of type 0x%2 for index root %1
//  in file 0x%3 is not indexable.
//
#define MSG_CHKLOG_NTFS_NON_INDEXABLE_INDEX_ATTR_TYPE 0x000069BEL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_BYTES_PER_INDEX_BUFFER
//
// MessageText:
//
//  The bytes per index buffer, 0x%2, for index root %1 in file
//  0x%4 is incorrect.  The expected value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_BYTES_PER_INDEX_BUFFER 0x000069BFL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_CLUSTERS_PER_INDEX_BUFFER
//
// MessageText:
//
//  The clusters per index buffer, 0x%2, for index root %1 in file
//  0x%4 is incorrect.  The expected value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_CLUSTERS_PER_INDEX_BUFFER 0x000069C0L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_VALUE_LENGTH
//
// MessageText:
//
//  The index allocation value length, 0x%2, for index %1 in file
//  0x%4 is not in multiple of 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_VALUE_LENGTH 0x000069C1L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_ALLOC_LENGTH
//
// MessageText:
//
//  The index allocation allocated length, 0x%2, for index %1 in file
//  0x%4 is not in multiple of 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_ALLOC_LENGTH 0x000069C2L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ROOT_INDEX_HEADER
//
// MessageText:
//
//  The first free byte, 0x%2, and bytes available, 0x%3, for
//  root index %1 in file 0x%4 are not equal.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ROOT_INDEX_HEADER 0x000069C3L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_OFFSET
//
// MessageText:
//
//  The index entry offset, 0x%3, of index %1 and VCN 0x%2
//  in file 0x%4 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_OFFSET 0x000069C4L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_UNKNOWN_VCN_INDEX_ENTRY_OFFSET
//
// MessageText:
//
//  The index entry offset, 0x%2, of index %1
//  in file 0x%3 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_UNKNOWN_VCN_INDEX_ENTRY_OFFSET 0x000069C5L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_HEADER_BYTES_AVAILABLE
//
// MessageText:
//
//  The bytes available, 0x%2, in index header for index %1 in file
//  0x%4 is not equal to 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_HEADER_BYTES_AVAILABLE 0x000069C6L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_HEADER_NOT_MARKED_AS_INDEX_NODE
//
// MessageText:
//
//  The index header for index %1 and VCN 0x%2 in file 0x%3
//  is not marked as index node.
//
#define MSG_CHKLOG_NTFS_INDEX_HEADER_NOT_MARKED_AS_INDEX_NODE 0x000069C7L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_DOWN_POINTER
//
// MessageText:
//
//  The VCN 0x%2 of index %1 in file 0x%3 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_DOWN_POINTER 0x000069C8L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_ALLOC_BITMAP
//
// MessageText:
//
//  The index bitmap for index %1 in file 0x%2 is invalid or missing.
//
#define MSG_CHKLOG_NTFS_INVALID_ALLOC_BITMAP 0x000069C9L

//
// MessageId: MSG_CHKLOG_NTFS_DOWN_POINTER_ALREADY_IN_USE
//
// MessageText:
//
//  The VCN 0x%2 of index %1 in file 0x%3 is already in use.
//
#define MSG_CHKLOG_NTFS_DOWN_POINTER_ALREADY_IN_USE 0x000069CAL

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_INDEX_ALLOC
//
// MessageText:
//
//  The index allocation for index %1 in file 0x%2 is invalid or missing.
//
#define MSG_CHKLOG_NTFS_INVALID_INDEX_ALLOC 0x000069CBL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_MULTI_SECTOR_HEADER_SIGNATURE
//
// MessageText:
//
//  The multi-sector header signature for VCN 0x%2 of index %1
//  in file 0x%3 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_MULTI_SECTOR_HEADER_SIGNATURE 0x000069CCL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_BUFFER_USA_OFFSET_BELOW_MINIMUM
//
// MessageText:
//
//  The USA offset 0x%3 of VCN 0x%2 of index %1
//  in file 0x%5 is below 0x%4.
//
#define MSG_CHKLOG_NTFS_INDEX_BUFFER_USA_OFFSET_BELOW_MINIMUM 0x000069CDL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_DOWN_BLOCK
//
// MessageText:
//
//  The VCN 0x%2 of index %1 in file 0x%4 is inconsistence with
//  the VCN 0x%3 stored inside the index buffer.
//
#define MSG_CHKLOG_NTFS_INCORRECT_DOWN_BLOCK 0x000069CEL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_SIZE
//
// MessageText:
//
//  The bytes per block, 0x%3, read from index buffer of VCN 0x%2
//  of index %1 in file 0x%4 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ALLOC_SIZE 0x000069CFL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_USA_OFFSET
//
// MessageText:
//
//  The USA offset 0x%3 of VCN 0x%2 of index %1
//  in file 0x%4 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_USA_OFFSET 0x000069D0L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_USA_SIZE
//
// MessageText:
//
//  The USA size 0x%3 of VCN 0x%2 of index %1 in file
//  0x%5 is incorrect.  The expected value is 0x%4.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_BUFFER_USA_SIZE 0x000069D1L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_HEADER_MARKED_AS_INDEX_NODE
//
// MessageText:
//
//  The index header of index %1 in file 0x%2
//  is marked as index node when it should not.
//
#define MSG_CHKLOG_NTFS_INDEX_HEADER_MARKED_AS_INDEX_NODE 0x000069D2L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_HEADER_FIRST_FREE_BYTE
//
// MessageText:
//
//  The first free byte, 0x%2, in index header of index %1
//  in file 0x%4 is not equal to 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_HEADER_FIRST_FREE_BYTE 0x000069D3L

//
// MessageId: MSG_CHKLOG_NTFS_BAD_FILE_NAME_IN_INDEX_ENTRY_VALUE
//
// MessageText:
//
//  Unable to query the name of a file name index entry of length 0x%3
//  of index %2 in file 0x%4 with parent 0x%1.
//
#define MSG_CHKLOG_NTFS_BAD_FILE_NAME_IN_INDEX_ENTRY_VALUE 0x000069D4L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_FREE_FRS
//
// MessageText:
//
//  Index entry %3 of index %2 in file 0x%1 points to unused file 0x%4.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_FREE_FRS 0x000069D5L

//
// MessageId: MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_FREE_FRS
//
// MessageText:
//
//  An index entry of index %2 in file 0x%1 points to unused file 0x%3.
//
#define MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_FREE_FRS 0x000069D6L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_NON_BASE_FRS
//
// MessageText:
//
//  The file 0x%4 pointed to by index entry %3 of index %2 with
//  parent file 0x%1 is not a base file record segment.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_NON_BASE_FRS 0x000069D7L

//
// MessageId: MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_NON_BASE_FRS
//
// MessageText:
//
//  The file 0x%3 pointed to by an index entry of index %2 with
//  parent file 0x%1 is not a base file record segment.
//
#define MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_NON_BASE_FRS 0x000069D8L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_FIND_INDEX_ENTRY_VALUE_FILE_NAME
//
// MessageText:
//
//  Unable to locate the file name attribute of index entry %3
//  of index %2 with parent 0x%1 in file 0x%4.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_FIND_INDEX_ENTRY_VALUE_FILE_NAME 0x000069D9L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_FIND_UNNAMED_INDEX_ENTRY_VALUE_FILE_NAME
//
// MessageText:
//
//  Unable to locate the file name attribute of an index entry
//  of index %2 with parent 0x%1 in file 0x%3.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_FIND_UNNAMED_INDEX_ENTRY_VALUE_FILE_NAME 0x000069DAL

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NO_OBJID_FRS
//
// MessageText:
//
//  The object id index entry in file 0x%1 points to file 0x%2
//  but the file has no object id in it.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NO_OBJID_FRS 0x000069DBL

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NON_BASE_FRS
//
// MessageText:
//
//  The object id index entry in file 0x%1 points to file 0x%2
//  which is not a base file record segment.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NON_BASE_FRS 0x000069DCL

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_HAS_INCORRECT_OBJID
//
// MessageText:
//
//  The object id in index entry in file 0x%1 is incorrect.
//  The entry points to file 0x%2.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_HAS_INCORRECT_OBJID 0x000069DDL

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_HAS_INCORRECT_PARENT
//
// MessageText:
//
//  The parent 0x%2 in an object id index entry in file 0x%1
//  is incorrect.  The correct value is 0x%3.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_HAS_INCORRECT_PARENT 0x000069DEL

//
// MessageId: MSG_CHKLOG_NTFS_DUPLICATE_OBJID
//
// MessageText:
//
//  The object id in file 0x%2 already existed in the object
//  id index in file 0x%1.
//
#define MSG_CHKLOG_NTFS_DUPLICATE_OBJID  0x000069DFL

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_OBJID_INDEX_ENTRY
//
// MessageText:
//
//  The object id in file 0x%2 does not appear in the object
//  id index in file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_OBJID_INDEX_ENTRY 0x000069E0L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NON_BASE_FRS
//
// MessageText:
//
//  The reparse point index entry in file 0x%1 points to file 0x%2
//  which is not a base file record segment.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NON_BASE_FRS 0x000069E1L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_HAS_INCORRECT_REPARSE_TAG
//
// MessageText:
//
//  The reparse tag 0x%2 of reparse point index entry in file 0x%1
//  is incorrect.  The correct reparse tag in file 0x%4 is 0x%3.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_HAS_INCORRECT_REPARSE_TAG 0x000069E2L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_HAS_INCORRECT_PARENT
//
// MessageText:
//
//  The parent 0x%2 in the reparse point index entry with tag 0x%4
//  in file 0x%1 is incorrect.  The correct value is 0x%3.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_HAS_INCORRECT_PARENT 0x000069E3L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NO_REPARSE_FRS
//
// MessageText:
//
//  The reparse point index entry in file 0x%1 points to file 0x%2
//  but the file has no reparse point in it.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NO_REPARSE_FRS 0x000069E4L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_REPARSE_INDEX_ENTRY
//
// MessageText:
//
//  The reparse point in file 0x%2 does not appear in
//  the reparse point index in file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_REPARSE_INDEX_ENTRY 0x000069E5L

//
// MessageId: MSG_CHKLOG_NTFS_FILE_NAME_INDEX_PRESENT_BIT_SET
//
// MessageText:
//
//  The file name index present bit is set in file 0x%1
//  but there is no file name index.
//
#define MSG_CHKLOG_NTFS_FILE_NAME_INDEX_PRESENT_BIT_SET 0x000069E6L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_INVALID_ROOT_INDEX
//
// MessageText:
//
//  The root index %2 in file 0x%1 is missing or invalid.
//
#define MSG_CHKLOG_NTFS_MISSING_INVALID_ROOT_INDEX 0x000069E7L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_LENGTH
//
// MessageText:
//
//  The index entry length 0x%1 is incorrect.  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_LENGTH 0x000069E8L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_ATTR_LENGTH
//
// MessageText:
//
//  The index entry attribute length 0x%2 of index entry type 0x%1
//  is incorrect.  The correct length is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_ATTR_LENGTH 0x000069E9L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_DATA_LENGTH
//
// MessageText:
//
//  The index entry data offset 0x%1 and length 0x%2 is
//  inconsistence with the index entry length 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_DATA_LENGTH 0x000069EAL

//
// MessageId: MSG_CHKLOG_NTFS_MISALIGNED_INDEX_ENTRY_LENGTH
//
// MessageText:
//
//  The index entry length is incorrect for index entry type 0x%1.
//
#define MSG_CHKLOG_NTFS_MISALIGNED_INDEX_ENTRY_LENGTH 0x000069EBL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The index entry length is too small for index entry type 0x%1.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_TOO_SMALL 0x000069ECL

//
// MessageId: MSG_CHKLOG_NTFS_VOLUME_INFORMATION_MISSING
//
// MessageText:
//
//  The volume information attribute is missing from file 0x%1.
//
#define MSG_CHKLOG_NTFS_VOLUME_INFORMATION_MISSING 0x000069EDL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The attribute record length 0x%1 is too small for attribute of
//  type 0x%3 and instance tag 0x%4.  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_TOO_SMALL 0x000069EEL

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_ATTR_FORM_CODE
//
// MessageText:
//
//  The attribute form code 0x%1 is invalid for attribute of type 0x%2
//  and instance tag 0x%3.
//
#define MSG_CHKLOG_NTFS_INVALID_ATTR_FORM_CODE 0x000069EFL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_RESIDENT
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should be resident.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_RESIDENT 0x000069F0L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_STD_INFO_ATTR_SIZE
//
// MessageText:
//
//  The standard information attribute length 0x%1 is incorrect.
//  The expected value is 0x%2 or 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_STD_INFO_ATTR_SIZE 0x000069F1L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_HAVE_NAME
//
// MessageText:
//
//  Attribute name is not allowed for attribute of type 0x%1
//  and instance tag 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_HAVE_NAME 0x000069F2L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_BE_RESIDENT
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should not be resident.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_BE_RESIDENT 0x000069F3L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ATTR_NAME_OFFSET
//
// MessageText:
//
//  The attribute name offset for attribute of type 0x%1
//  and instance tag 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ATTR_NAME_OFFSET 0x000069F4L

//
// MessageId: MSG_CHKLOG_NTFS_NULL_FOUND_IN_ATTR_NAME
//
// MessageText:
//
//  The attribute name for attribute of type 0x%1 and instance tag 0x%2
//  contains unicode NULL.
//
#define MSG_CHKLOG_NTFS_NULL_FOUND_IN_ATTR_NAME 0x000069F5L

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_ATTR_TO_ATTR_DEF_TABLE
//
// MessageText:
//
//  Unknown attribute of type 0x%1 and instance tag 0x%2.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_ATTR_TO_ATTR_DEF_TABLE 0x000069F6L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_BE_INDEXED
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should not be indexed.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_NOT_BE_INDEXED 0x000069F7L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_INDEXED
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should be indexed.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_INDEXED 0x000069F8L

//
// MessageId: MSG_CHKLOG_NTFS_INDEXABLE_ATTR_SHOULD_NOT_HAVE_NAME
//
// MessageText:
//
//  The indexable attribute of type 0x%1 and instance tag 0x%2
//  should not have name.
//
#define MSG_CHKLOG_NTFS_INDEXABLE_ATTR_SHOULD_NOT_HAVE_NAME 0x000069F9L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_NAMED
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should have a name.
//
#define MSG_CHKLOG_NTFS_ATTR_SHOULD_BE_NAMED 0x000069FAL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The attribute length 0x%1 for attribute of type 0x%3 and
//  instance tag 0x%4 is too small.  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_SMALL 0x000069FBL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_BIG
//
// MessageText:
//
//  The attribute length 0x%1 for attribute of type 0x%3 and
//  instance tag 0x%4 is too big.  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_BIG 0x000069FCL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_RESIDENT_ATTR
//
// MessageText:
//
//  The resident attribute for attribute of type 0x%1 and instance
//  tag 0x%2 is incorrect.  The attribute has value of length 0x%1,
//  and offset 0x%2.  The attribute length is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_RESIDENT_ATTR 0x000069FDL

//
// MessageId: MSG_CHKLOG_NTFS_RESIDENT_ATTR_COLLISION
//
// MessageText:
//
//  The resident attribute name is colliding with the resident value for attribute
//  of type 0x%4 and instance tag 0x%5.  The attribute name offset is
//  0x%2, length 0x%1, and the attribute value offset is 0x%3.
//
#define MSG_CHKLOG_NTFS_RESIDENT_ATTR_COLLISION 0x000069FEL

//
// MessageId: MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_HAS_BAD_MAPPING_PAIRS_OFFSET
//
// MessageText:
//
//  The mapping pairs offset 0x%1 for attribute of type 0x%3 and instance
//  tag 0x%4 exceeded the attribute length 0x%2.
//
#define MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_HAS_BAD_MAPPING_PAIRS_OFFSET 0x000069FFL

//
// MessageId: MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_MAPPING_PAIRS_OFFSET_TOO_SMALL
//
// MessageText:
//
//  The mapping pairs offset 0x%1 for attribute of type 0x%3 and instance
//  tag 0x%4 is too small.  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_MAPPING_PAIRS_OFFSET_TOO_SMALL 0x00006A00L

//
// MessageId: MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_COLLISION
//
// MessageText:
//
//  The attribute name is colliding with the mapping pairs for attribute
//  of type %4 and instance tag 0x%5.  The attribute name offset is
//  0x%2, length 0x%1, and the mapping pairs offset is 0x%3.
//
#define MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_COLLISION 0x00006A01L

//
// MessageId: MSG_CHKLOG_NTFS_BAD_MAPPING_PAIRS
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 has bad mapping pairs.
//
#define MSG_CHKLOG_NTFS_BAD_MAPPING_PAIRS 0x00006A02L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_INITIALIZE_EXTENT_LIST
//
// MessageText:
//
//  Unable to initialize an extent list for attribute type 0x%1 with
//  instance tag 0x%2.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_INITIALIZE_EXTENT_LIST 0x00006A03L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_HAS_INVALID_HIGHEST_VCN
//
// MessageText:
//
//  The highest VCN 0x%1 of attribute of type 0x%3 and instance
//  tag 0x%4 is incorrect.  The expected value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_HAS_INVALID_HIGHEST_VCN 0x00006A04L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_SIZES
//
// MessageText:
//
//  The non resident attribute of type 0x%4 and instance tag 0x%5 is
//  inconsistent.  The valid data length is 0x%1, file size 0x%2, and
//  allocated length 0x%3.
//
#define MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_SIZES 0x00006A05L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_UNKNOWN_TAG_ATTR_SIZES
//
// MessageText:
//
//  The non resident attribute of type 0x%4 is inconsistent.  The valid data
//  length is 0x%1, file size 0x%2, and allocated length 0x%3.
//
#define MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_UNKNOWN_TAG_ATTR_SIZES 0x00006A06L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_TOTAL_ALLOC_BLOCK
//
// MessageText:
//
//  The allocated length 0x%1 is not in multiple of 0x%2 for attribute
//  of type 0x%3 and instance tag 0x%4.
//
#define MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_TOTAL_ALLOC_BLOCK 0x00006A07L

//
// MessageId: MSG_CHKLOG_NTFS_FILE_NAME_VALUE_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The file name value length 0x%1 for attribute of type 0x%3 with
//  instance tag 0x%4 is too small.  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_FILE_NAME_VALUE_LENGTH_TOO_SMALL 0x00006A08L

//
// MessageId: MSG_CHKLOG_NTFS_INCONSISTENCE_FILE_NAME_VALUE
//
// MessageText:
//
//  The attribute of type 0x%2 and instance tag 0x%3 is inconsistence.
//  The attribute value length is 0x%1.
//
#define MSG_CHKLOG_NTFS_INCONSISTENCE_FILE_NAME_VALUE 0x00006A09L

//
// MessageId: MSG_CHKLOG_NTFS_BAD_FILE_NAME_LENGTH_IN_FILE_NAME_VALUE
//
// MessageText:
//
//  The file name length is zero for attribute of type 0x%1
//  and instance tag 0x%2.
//
#define MSG_CHKLOG_NTFS_BAD_FILE_NAME_LENGTH_IN_FILE_NAME_VALUE 0x00006A0AL

//
// MessageId: MSG_CHKLOG_NTFS_NULL_FOUND_IN_FILE_NAME_OF_FILE_NAME_VALUE
//
// MessageText:
//
//  The file name in file name value in attribute of type 0x%1 and instance
//  tag %2 contains unicode NULL.
//
#define MSG_CHKLOG_NTFS_NULL_FOUND_IN_FILE_NAME_OF_FILE_NAME_VALUE 0x00006A0BL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FRS_MULTI_SECTOR_HEADER_SIGNATURE
//
// MessageText:
//
//  The multi-sector header signature in file 0x%1 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FRS_MULTI_SECTOR_HEADER_SIGNATURE 0x00006A0CL

//
// MessageId: MSG_CHKLOG_NTFS_FRS_USA_OFFSET_BELOW_MINIMUM
//
// MessageText:
//
//  The USA offset 0x%1 in file 0x%3 is too small.
//  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_FRS_USA_OFFSET_BELOW_MINIMUM 0x00006A0DL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FRS_SIZE
//
// MessageText:
//
//  The file record segment size 0x%1 is invalid in file 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FRS_SIZE 0x00006A0EL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FRS_USA_OFFSET
//
// MessageText:
//
//  The USA offset 0x%1 in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FRS_USA_OFFSET 0x00006A0FL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FRS_USA_SIZE
//
// MessageText:
//
//  The USA size 0x%1 in file 0x%3 is incorrect.
//  The expected value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FRS_USA_SIZE 0x00006A10L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FIRST_ATTR_OFFSET
//
// MessageText:
//
//  The first attribute offset 0x%1 in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FIRST_ATTR_OFFSET 0x00006A11L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FRS_HEADER
//
// MessageText:
//
//  The bytes available, 0x%1, in the file record segment header for
//  file 0x%3 is incorrect.  The expected value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FRS_HEADER 0x00006A12L

//
// MessageId: MSG_CHKLOG_NTFS_INSTANCE_NUMBER_COLLISION
//
// MessageText:
//
//  The instance tag 0x%2 of attribute of type 0x%1 in file 0x%3
//  is already in use.
//
#define MSG_CHKLOG_NTFS_INSTANCE_NUMBER_COLLISION 0x00006A13L

//
// MessageId: MSG_CHKLOG_NTFS_INSTANCE_NUMBER_TOO_LARGE
//
// MessageText:
//
//  The instance tag 0x%2 of attribute of type 0x%1 in file 0x%4
//  is too large.  The instance tag should be less than 0x%3.
//
#define MSG_CHKLOG_NTFS_INSTANCE_NUMBER_TOO_LARGE 0x00006A14L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_STANDARD_INFO
//
// MessageText:
//
//  The standard information attribute in file 0x%1 is missing.
//
#define MSG_CHKLOG_NTFS_MISSING_STANDARD_INFO 0x00006A15L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_OFFSET_TOO_LARGE
//
// MessageText:
//
//  The attribute record offset 0x%1 is too large for attribute of type 0x%3
//  and instance tag 0x%4 in file 0x%5.  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_OFFSET_TOO_LARGE 0x00006A16L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_CANNOT_BE_ZERO
//
// MessageText:
//
//  The record length of attribute of type 0x%1 and instance tag 0x%2
//  in file 0x%3 should not be zero.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_CANNOT_BE_ZERO 0x00006A17L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_MISALIGNED
//
// MessageText:
//
//  The record length 0x%1 of attribute of type 0x%2 and
//  instance tag 0x%3 in file 0x%4 is not aligned.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_LENGTH_MISALIGNED 0x00006A18L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_RECORD_TOO_LARGE
//
// MessageText:
//
//  The record length 0x%1 is too large for attribute of type 0x%3
//  and instance tag 0x%4 in file 0x%5.  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_ATTR_RECORD_TOO_LARGE 0x00006A19L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_FIRST_FREE_BYTE
//
// MessageText:
//
//  The first free byte, 0x%1, in file 0x%4 is incorrect.  The number of
//  bytes free in the file record segment is 0x%2 and the total length
//  is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_FIRST_FREE_BYTE 0x00006A1AL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_OUT_OF_ORDER
//
// MessageText:
//
//  The attribute of type 0x%1 and instance tag 0x%2 should be after
//  attribute of type 0x%3 and instance tag 0x%4 in file 0x%5.
//
#define MSG_CHKLOG_NTFS_ATTR_OUT_OF_ORDER 0x00006A1BL

//
// MessageId: MSG_CHKLOG_NTFS_IDENTICAL_ATTR_RECORD_WITHOUT_INDEX
//
// MessageText:
//
//  All attribute of type 0x%1 and instance tag 0x%2 should be indexed
//  in file 0x%5.
//
#define MSG_CHKLOG_NTFS_IDENTICAL_ATTR_RECORD_WITHOUT_INDEX 0x00006A1CL

//
// MessageId: MSG_CHKLOG_NTFS_IDENTICAL_ATTR_VALUE
//
// MessageText:
//
//  Two identical attributes of type 0x%1 and instance tag 0x%2 are
//  in file 0x%3.
//
#define MSG_CHKLOG_NTFS_IDENTICAL_ATTR_VALUE 0x00006A1DL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_PRESENT_BIT
//
// MessageText:
//
//  The file name index present bit in file 0x%1 should not be set.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_PRESENT_BIT 0x00006A1EL

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_SPARSE_FLAG_IN_STD_INFO
//
// MessageText:
//
//  The sparse flag in the standard information attribute in file 0x%1
//  is not set.
//
#define MSG_CHKLOG_NTFS_MISSING_SPARSE_FLAG_IN_STD_INFO 0x00006A20L

//
// MessageId: MSG_CHKLOG_NTFS_REPLACING_OLD_ENCRYPTED_FLAG_WITH_NEW_ONE_IN_STD_INFO
//
// MessageText:
//
//  The old encrypted flag is being replaced by the new encrypted flag
//  in file 0x%1.
//
#define MSG_CHKLOG_NTFS_REPLACING_OLD_ENCRYPTED_FLAG_WITH_NEW_ONE_IN_STD_INFO 0x00006A22L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_ENCRYPTED_FLAG_IN_STD_INFO
//
// MessageText:
//
//  The encrypted flag in standard information attribute in file 0x%1
//  is not set.
//
#define MSG_CHKLOG_NTFS_MISSING_ENCRYPTED_FLAG_IN_STD_INFO 0x00006A23L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_REPARSE_POINT_FLAG_IN_STD_INFO
//
// MessageText:
//
//  The reparse flag in standard information attribute in file 0x%1
//  is not set.
//
#define MSG_CHKLOG_NTFS_MISSING_REPARSE_POINT_FLAG_IN_STD_INFO 0x00006A24L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_POINT_FLAG_SET_IN_STD_INFO
//
// MessageText:
//
//  The reparse flag in standard information attribute in file 0x%1
//  should not be set.
//
#define MSG_CHKLOG_NTFS_REPARSE_POINT_FLAG_SET_IN_STD_INFO 0x00006A25L

//
// MessageId: MSG_CHKLOG_NTFS_NTFS_FILE_NAME_ALREADY_ENCOUNTERED
//
// MessageText:
//
//  There are more than one NTFS file name attribute in file 0x%1.
//
#define MSG_CHKLOG_NTFS_NTFS_FILE_NAME_ALREADY_ENCOUNTERED 0x00006A26L

//
// MessageId: MSG_CHKLOG_NTFS_DOS_NTFS_NAMES_HAVE_DIFFERENT_PARENTS
//
// MessageText:
//
//  The file name attributes in file 0x%3 has different parents.
//  The DOS name has 0x%1 as parent.  The NTFS name has 0x%2 as parent.
//
#define MSG_CHKLOG_NTFS_DOS_NTFS_NAMES_HAVE_DIFFERENT_PARENTS 0x00006A27L

//
// MessageId: MSG_CHKLOG_NTFS_DOS_FILE_NAME_ALREADY_ENCOUNTERED
//
// MessageText:
//
//  There are more than one DOS file name attribute in file 0x%1.
//
#define MSG_CHKLOG_NTFS_DOS_FILE_NAME_ALREADY_ENCOUNTERED 0x00006A28L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_DOS_NAME
//
// MessageText:
//
//  The DOS file name attribute in file 0x%1 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_DOS_NAME 0x00006A29L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_NTFS_NAME
//
// MessageText:
//
//  There is no NTFS file name attribute in file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_NTFS_NAME 0x00006A2AL

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_DOS_NAME
//
// MessageText:
//
//  There is no DOS file name attribute in file 0x%1.
//
#define MSG_CHKLOG_NTFS_MISSING_DOS_NAME 0x00006A2BL

//
// MessageId: MSG_CHKLOG_NTFS_DOS_NTFS_NAMES_ARE_IDENTICAL
//
// MessageText:
//
//  The DOS and NTFS file name attributes in file 0x%1 are identical.
//
#define MSG_CHKLOG_NTFS_DOS_NTFS_NAMES_ARE_IDENTICAL 0x00006A2CL

//
// MessageId: MSG_CHKLOG_NTFS_CANNOT_SETUP_ATTR_LIST
//
// MessageText:
//
//  Unable to setup the attribute list in file 0x%1.
//
#define MSG_CHKLOG_NTFS_CANNOT_SETUP_ATTR_LIST 0x00006A2DL

//
// MessageId: MSG_CHKLOG_NTFS_CANNOT_FIND_ATTR
//
// MessageText:
//
//  The attribute type 0x%1 with name %2 in file 0x%3 is missing.
//
#define MSG_CHKLOG_NTFS_CANNOT_FIND_ATTR 0x00006A2EL

//
// MessageId: MSG_CHKLOG_NTFS_CANNOT_FIND_UNNAMED_ATTR
//
// MessageText:
//
//  The attribute of type 0x%1 in file 0x%2 is missing.
//
#define MSG_CHKLOG_NTFS_CANNOT_FIND_UNNAMED_ATTR 0x00006A2FL

//
// MessageId: MSG_CHKLOG_NTFS_CANNOT_FIND_UNNAMED_DATA_ATTR
//
// MessageText:
//
//  The unnamed data attribute in file 0x%1 is missing.
//
#define MSG_CHKLOG_NTFS_CANNOT_FIND_UNNAMED_DATA_ATTR 0x00006A30L

//
// MessageId: MSG_CHKLOG_NTFS_CANNOT_FIND_ATTR_LIST_ATTR
//
// MessageText:
//
//  The attribute list in file 0x%1 is missing.
//
#define MSG_CHKLOG_NTFS_CANNOT_FIND_ATTR_LIST_ATTR 0x00006A31L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LIST_ENTRY_LENGTH_MISALIGNED
//
// MessageText:
//
//  The length, 0x%3, of the attribute list entry with attribute of type
//  0x%1 and instance tag 0x%2 in file 0x%4 is not aligned.
//
#define MSG_CHKLOG_NTFS_ATTR_LIST_ENTRY_LENGTH_MISALIGNED 0x00006A32L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ATTR_NAME_IN_ATTR_LIST_ENTRY
//
// MessageText:
//
//  The attribute list entry with attribute of type 0x%1 and instance tag 0x%2
//  in file 0x%6 is incorrect.  The attribute list entry name length is 0x%3,
//  and offset 0x%4.  The attribute list length is 0x%5.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ATTR_NAME_IN_ATTR_LIST_ENTRY 0x00006A33L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_NAME_OFFSET_IN_ATTR_LIST_ENTRY_TOO_SMALL
//
// MessageText:
//
//  The attribute name offset 0x%3 of attribute list entry with attribute of
//  type 0x%1 and instance tag 0x%2 in file 0x%5 is too small.
//  The minimum value is 0x%4.
//
#define MSG_CHKLOG_NTFS_ATTR_NAME_OFFSET_IN_ATTR_LIST_ENTRY_TOO_SMALL 0x00006A34L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ATTR_LIST_LENGTH
//
// MessageText:
//
//  The attribute list length 0x%2 in file 0x%3 is incorrect.
//  The expected value is 0x%1.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ATTR_LIST_LENGTH 0x00006A35L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LIST_CROSS_LINKED
//
// MessageText:
//
//  The extent list of the attribute list is crossed linked at 0x%1
//  for possibly 0x%2 clusters in file 0x%3.
//
#define MSG_CHKLOG_NTFS_ATTR_LIST_CROSS_LINKED 0x00006A36L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LIST_ENTRIES_OUT_OF_ORDER
//
// MessageText:
//
//  The attribute list entry with attribute of type 0x%1 and instance tag
//  0x%2 should be after attribute of type 0x%3 and instance tag 0x%4.
//
#define MSG_CHKLOG_NTFS_ATTR_LIST_ENTRIES_OUT_OF_ORDER 0x00006A37L

//
// MessageId: MSG_CHKLOG_NTFS_IDENTICAL_ATTR_LIST_ENTRIES
//
// MessageText:
//
//  Two identical attribute list entries of type 0x%1 and instance tag 0x%2
//  are found.
//
#define MSG_CHKLOG_NTFS_IDENTICAL_ATTR_LIST_ENTRIES 0x00006A38L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_SMALL_FOR_FILE
//
// MessageText:
//
//  The attribute length 0x%3 of attribute of type 0x%1 and name %2 in
//  file 0x%5 is too small.  The minimum value is 0x%4.
//
#define MSG_CHKLOG_NTFS_ATTR_LENGTH_TOO_SMALL_FOR_FILE 0x00006A39L

//
// MessageId: MSG_CHKLOG_NTFS_SPARSE_FLAG_NOT_SET_FOR_ATTR
//
// MessageText:
//
//  The sparse flag of attribute of type 0x%1 and name %2 in file
//  0x%3 is not set.
//
#define MSG_CHKLOG_NTFS_SPARSE_FLAG_NOT_SET_FOR_ATTR 0x00006A3AL

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_OFFSET_NOT_AT_PAGE_BOUNDARY
//
// MessageText:
//
//  The USN Journal offset 0x%1 in file 0x%2 is not at a page boundary.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_OFFSET_NOT_AT_PAGE_BOUNDARY 0x00006A3BL

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_TOO_LARGE
//
// MessageText:
//
//  The USN Journal length 0x%1 in file 0x%3 is too large.
//  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_TOO_LARGE 0x00006A3CL

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_LESS_THAN_OFFSET
//
// MessageText:
//
//  The USN Journal length 0x%1 in file 0x%3 is less than
//  its offset 0x%2.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_LESS_THAN_OFFSET 0x00006A3DL

//
// MessageId: MSG_CHKLOG_NTFS_INCOMPLETE_LAST_USN_JRNL_PAGE
//
// MessageText:
//
//  The remaining USN block at offset 0x%1 in file 0x%2 is
//  less than a page.
//
#define MSG_CHKLOG_NTFS_INCOMPLETE_LAST_USN_JRNL_PAGE 0x00006A3EL

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_REMAINING_OF_A_PAGE_CONTAINS_NON_ZERO
//
// MessageText:
//
//  The remaining of an USN page at offset 0x%1 in file 0x%2
//  should be filled with zeros.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_REMAINING_OF_A_PAGE_CONTAINS_NON_ZERO 0x00006A3FL

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_CROSSES_PAGE_BOUNDARY
//
// MessageText:
//
//  The USN Journal entry at offset 0x%1 and length 0x%2 crosses
//  the page boundary.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_CROSSES_PAGE_BOUNDARY 0x00006A40L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_EXCEEDS_REMAINING_PAGE_LENGTH
//
// MessageText:
//
//  The USN Journal entry length 0x%2 at offset 0x%1, in file
//  0x%4 is larger than the remaining length 0x%3 of a page.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_EXCEEDS_REMAINING_PAGE_LENGTH 0x00006A41L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_EXCEEDS_PAGE_BOUNDARY
//
// MessageText:
//
//  The USN Journal entry length 0x%2 at offset 0x%1 in file
//  0x%4 exceeded the page size 0x%3.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_EXCEEDS_PAGE_BOUNDARY 0x00006A42L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_MISALIGNED
//
// MessageText:
//
//  The USN Journal entry length 0x%2 at offset 0x%1 in file
//  0x%3 is not aligned.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_MISALIGNED 0x00006A43L

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_USN_JRNL_ENTRY_VERSION
//
// MessageText:
//
//  The USN Journal entry version %2.%3 at offset 0x%1
//  in file 0x%4 is not recognized.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_USN_JRNL_ENTRY_VERSION 0x00006A44L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The USN Journal entry length 0x%2 at offset 0x%1 in file
//  0x%4 is too small.  The minimum value is 0x%3.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_ENTRY_LENGTH_TOO_SMALL 0x00006A45L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_REMAINING_PAGE_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The remaining USN page length 0x%2 is too small to fit another
//  USN Journal entry at offset 0x%1 in file 0x%4.
//  It needs at least 0x%3 bytes.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_REMAINING_PAGE_LENGTH_TOO_SMALL 0x00006A46L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_USN_JRNL_ENTRY_OFFSET
//
// MessageText:
//
//  The USN value 0x%1 of USN Journal entry at offset 0x%2
//  in file 0x%3 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_USN_JRNL_ENTRY_OFFSET 0x00006A47L

//
// MessageId: MSG_CHKLOG_NTFS_INCONSISTENCE_USN_JRNL_ENTRY
//
// MessageText:
//
//  The USN Journal entry at offset 0x%1 in file 0x%4 is not
//  consistence.  The entry has length of 0x%3 and a file name length of 0x%2.
//
#define MSG_CHKLOG_NTFS_INCONSISTENCE_USN_JRNL_ENTRY 0x00006A48L

//
// MessageId: MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_LESS_THAN_LARGEST_USN_ENCOUNTERED
//
// MessageText:
//
//  The USN Journal length 0x%1 in file 0x%4 is less the
//  largest USN encountered, 0x%2, plus eight in file 0x%3.
//
#define MSG_CHKLOG_NTFS_USN_JRNL_LENGTH_LESS_THAN_LARGEST_USN_ENCOUNTERED 0x00006A49L

//
// MessageId: MSG_CHKLOG_NTFS_SECURITY_DATA_STREAM_MISSING
//
// MessageText:
//
//  The security data stream is missing from file 0x%1.
//
#define MSG_CHKLOG_NTFS_SECURITY_DATA_STREAM_MISSING 0x00006A4AL

//
// MessageId: MSG_CHKLOG_NTFS_SECURITY_DATA_STREAM_SIZE_TOO_SMALL
//
// MessageText:
//
//  The security data stream size 0x%1 should not be less than 0x%2.
//
#define MSG_CHKLOG_NTFS_SECURITY_DATA_STREAM_SIZE_TOO_SMALL 0x00006A4BL

//
// MessageId: MSG_CHKLOG_NTFS_REMAINING_SECURITY_DATA_BLOCK_CONTAINS_NON_ZERO
//
// MessageText:
//
//  The remaining of a security data stream page starting at offset 0x%1
//  for 0x%2 bytes contains non-zero.
//
#define MSG_CHKLOG_NTFS_REMAINING_SECURITY_DATA_BLOCK_CONTAINS_NON_ZERO 0x00006A4CL

//
// MessageId: MSG_CHKLOG_NTFS_SDS_ENTRY_CROSSES_PAGE_BOUNDARY
//
// MessageText:
//
//  The security data stream entry at offset 0x%1 with length 0x%2
//  crosses the page boundary.
//
#define MSG_CHKLOG_NTFS_SDS_ENTRY_CROSSES_PAGE_BOUNDARY 0x00006A4DL

//
// MessageId: MSG_CHKLOG_NTFS_SDS_ENTRY_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The length, 0x%2, of the security data stream entry at offset
//  0x%1 is too small.  The minimum value is 0x%3.
//
#define MSG_CHKLOG_NTFS_SDS_ENTRY_LENGTH_TOO_SMALL 0x00006A4EL

//
// MessageId: MSG_CHKLOG_NTFS_SDS_ENTRY_LENGTH_EXCEEDS_PAGE_BOUNDARY
//
// MessageText:
//
//  The length, 0x%2, of the security data stream entry at offset
//  0x%1 exceeded the page size 0x%3.
//
#define MSG_CHKLOG_NTFS_SDS_ENTRY_LENGTH_EXCEEDS_PAGE_BOUNDARY 0x00006A4FL

//
// MessageId: MSG_CHKLOG_NTFS_SDS_REMAINING_PAGE_LENGTH_TOO_SMALL
//
// MessageText:
//
//  The security data stream entry at offset 0x%1 does not fit into the
//  remaining length, 0x%2, of a page.  The minimum value is 0x%3.
//  
//
#define MSG_CHKLOG_NTFS_SDS_REMAINING_PAGE_LENGTH_TOO_SMALL 0x00006A50L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_SECURITY_DESCRIPTOR_IN_SDS_ENTRY
//
// MessageText:
//
//  The security descriptor entry with Id 0x%2 at offset 0x%1 is invalid.
//
#define MSG_CHKLOG_NTFS_INVALID_SECURITY_DESCRIPTOR_IN_SDS_ENTRY 0x00006A51L

//
// MessageId: MSG_CHKLOG_NTFS_DUPLICATE_SID_IN_SDS_ENTRY
//
// MessageText:
//
//  The security Id 0x%2 of security descriptor entry at offset 0x%1
//  is a duplicate.
//
#define MSG_CHKLOG_NTFS_DUPLICATE_SID_IN_SDS_ENTRY 0x00006A52L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_HASH_IN_SDS_ENTRY
//
// MessageText:
//
//  The hash value 0x%2 of the security descriptor entry with Id
//  0x%4 at offset 0x%1 is invalid.  The correct value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_HASH_IN_SDS_ENTRY 0x00006A53L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_OFFSET_IN_SDS_ENTRY
//
// MessageText:
//
//  The offset 0x%2 stored in the security descriptor entry with Id
//  0x%4 at offset 0x%1 is invalid.  The correct value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INCORRECT_OFFSET_IN_SDS_ENTRY 0x00006A54L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_SECURITY_DESCRIPTOR_IN_FILE
//
// MessageText:
//
//  The security descriptor in file 0x%1 is invalid.
//
#define MSG_CHKLOG_NTFS_INVALID_SECURITY_DESCRIPTOR_IN_FILE 0x00006A55L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_SECURITY_ID_IN_FILE
//
// MessageText:
//
//  The security Id 0x%1 in file 0x%2 is invalid.
//
#define MSG_CHKLOG_NTFS_INVALID_SECURITY_ID_IN_FILE 0x00006A56L

//
// MessageId: MSG_CHKLOG_NTFS_UNKNOWN_SECURITY_DATA_STREAM
//
// MessageText:
//
//  The data stream with name %1 in file 0x%2 is not recognized.
//
#define MSG_CHKLOG_NTFS_UNKNOWN_SECURITY_DATA_STREAM 0x00006A57L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ATTR_DEF_TABLE_LENGTH
//
// MessageText:
//
//  The attribute definition table length 0x%1 is incorrect.
//  The correct value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ATTR_DEF_TABLE_LENGTH 0x00006A58L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_ATTR_DEF_TABLE
//
// MessageText:
//
//  The attribute defintion table content is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_ATTR_DEF_TABLE 0x00006A59L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_SECTOR_ZERO_IN_BOOT_FILE
//
// MessageText:
//
//  Cluster zero is missing from the data attribute in boot file.
//
#define MSG_CHKLOG_NTFS_MISSING_SECTOR_ZERO_IN_BOOT_FILE 0x00006A5AL

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_LIST_IN_LOG_FILE
//
// MessageText:
//
//  Attribute list found in the log file.
//
#define MSG_CHKLOG_NTFS_ATTR_LIST_IN_LOG_FILE 0x00006A5BL

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_OR_RESIDENT_DATA_ATTR_IN_LOG_FILE
//
// MessageText:
//
//  The data attribute is either resident or missing in the log file.
//
#define MSG_CHKLOG_NTFS_MISSING_OR_RESIDENT_DATA_ATTR_IN_LOG_FILE 0x00006A5CL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_QUERY_LCN_FROM_VCN_FOR_MFT_MIRROR
//
// MessageText:
//
//  Unable to obtain the LCN in data attribute of the MFT mirror.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_QUERY_LCN_FROM_VCN_FOR_MFT_MIRROR 0x00006A5DL

//
// MessageId: MSG_CHKLOG_NTFS_LCN_NOT_PRESENT_FOR_VCN_ZERO_OF_MFT_MIRROR
//
// MessageText:
//
//  There is no physical LCN for VCN 0 in data attribute of the MFT mirror.
//
#define MSG_CHKLOG_NTFS_LCN_NOT_PRESENT_FOR_VCN_ZERO_OF_MFT_MIRROR 0x00006A5EL

//
// MessageId: MSG_CHKLOG_NTFS_DISCONTIGUOUS_MFT_MIRROR
//
// MessageText:
//
//  The data attribute of the MFT mirror is not contiguous for 0x%1 sectors.
//
#define MSG_CHKLOG_NTFS_DISCONTIGUOUS_MFT_MIRROR 0x00006A5FL

//
// MessageId: MSG_CHKLOG_NTFS_MFT_MIRROR_DIFFERENT_FROM_MFT
//
// MessageText:
//
//  The MFT mirror is different from the MFT.
//
#define MSG_CHKLOG_NTFS_MFT_MIRROR_DIFFERENT_FROM_MFT 0x00006A60L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_DATA_ATTR_IN_UPCASE_FILE
//
// MessageText:
//
//  The data attribute is missing from the upcase file.
//
#define MSG_CHKLOG_NTFS_MISSING_DATA_ATTR_IN_UPCASE_FILE 0x00006A61L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_UPCASE_FILE_LENGTH
//
// MessageText:
//
//  The upcase file length 0x%1 is incorrect.  The expected value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INCORRECT_UPCASE_FILE_LENGTH 0x00006A62L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_UPCASE_FILE
//
// MessageText:
//
//  The upcase file content is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_UPCASE_FILE 0x00006A63L

//
// MessageId: MSG_CHKLOG_NTFS_MISSING_OR_RESIDENT_DATA_ATTR_IN_MFT_MIRROR
//
// MessageText:
//
//  The data attribute is either resident or missing in the MFT mirror.
//
#define MSG_CHKLOG_NTFS_MISSING_OR_RESIDENT_DATA_ATTR_IN_MFT_MIRROR 0x00006A64L

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_ORDER
//
// MessageText:
//
//  The two index entries of length 0x%1 and 0x%2 are either identical
//  or appear in the wrong order.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_ORDER 0x00006A65L

//
// MessageId: MSG_CHKLOG_NTFS_FIRST_INDEX_ENTRY_IS_LEAF_BUT_NOT_AT_ROOT
//
// MessageText:
//
//  The first index entry of length 0x%1 is a leaf but it is not in the root.
//
#define MSG_CHKLOG_NTFS_FIRST_INDEX_ENTRY_IS_LEAF_BUT_NOT_AT_ROOT 0x00006A66L

//
// MessageId: MSG_CHKLOG_NTFS_EMPTY_INDEX_BUFFER
//
// MessageText:
//
//  The first index entry of length 0x%1 is a leaf but the previous entry is not.
//
#define MSG_CHKLOG_NTFS_EMPTY_INDEX_BUFFER 0x00006A67L

//
// MessageId: MSG_CHKLOG_NTFS_LEAF_DEPTH_NOT_THE_SAME
//
// MessageText:
//
//  Current leaf index entry of length 0x%3 is at depth 0x%2 which
//  is different from other leaf index entry which has a depth of 0x%1.
//
#define MSG_CHKLOG_NTFS_LEAF_DEPTH_NOT_THE_SAME 0x00006A68L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_DOWN_POINTER
//
// MessageText:
//
//  The down pointer of current index entry with length 0x%1 is invalid.
//
#define MSG_CHKLOG_NTFS_INVALID_DOWN_POINTER 0x00006A69L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_TOO_LARGE
//
// MessageText:
//
//  The index entry length 0x%1 is too large.  The maximum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_TOO_LARGE 0x00006A6AL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ALLOC_DOES_NOT_EXIST
//
// MessageText:
//
//  The allocation attribute does not exist.
//
#define MSG_CHKLOG_NTFS_INDEX_ALLOC_DOES_NOT_EXIST 0x00006A6BL

//
// MessageId: MSG_CHKLOG_NTFS_CLEARED_UNUSED_SECURITY_DESCRIPTORS
//
// MessageText:
//
//  Clearing unused security descriptor stream entries.
//
#define MSG_CHKLOG_NTFS_CLEARED_UNUSED_SECURITY_DESCRIPTORS 0x00006A6CL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_SDS_MIRROR
//
// MessageText:
//
//  Mirror security descriptor block different from that of
//  master security descriptor at offset 0x%1.
//
#define MSG_CHKLOG_NTFS_INCORRECT_SDS_MIRROR 0x00006A6DL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_ATTR_DEF_TABLE
//
// MessageText:
//
//  The attribute defintion table cannot be read.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_ATTR_DEF_TABLE 0x00006A6EL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_INDEX_BUFFER
//
// MessageText:
//
//  The index buffer at VCN 0x%2 of index %1 in file 0x%3
//  cannot be read.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_INDEX_BUFFER 0x00006A6FL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_MFT_MIRROR
//
// MessageText:
//
//  The MFT mirror starting at cluster 0x%1 for 0x%2 sectors
//  cannot be read.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_MFT_MIRROR 0x00006A70L

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_SECURITY_DESCRIPTOR
//
// MessageText:
//
//  The security descriptor in file 0x%1 cannot be read.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_SECURITY_DESCRIPTOR 0x00006A71L    

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_UPCASE_TABLE
//
// MessageText:
//
//  The upcase table cannot be read.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_UPCASE_TABLE 0x00006A72L    

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_USN_JRNL_ATTR
//
// MessageText:
//
//  The USN attrib of type code 0x%1 and name %2 cannot be
//  read in file 0x%3.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_USN_JRNL_ATTR 0x00006A73L    

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_EA_DATA_LENGTH
//
// MessageText:
//
//  The EA Data value length, 0x%1, in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_EA_DATA_LENGTH 0x00006A74L

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_BEYOND_INDEX_LENGTH
//
// MessageText:
//
//  The index entry length 0x%2 for index %1 in file 0x%4
//  is too large.  The maximum value is 0x%3.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_LENGTH_BEYOND_INDEX_LENGTH 0x00006A75L

//
// MessageId: MSG_CHKLOG_NTFS_QUERY_EXTENT_FAILED
//
// MessageText:
//
//  Unable to query extent list entry 0x%3 from attribute
//  of type 0x%1 and instance tag 0x%2.
//
#define MSG_CHKLOG_NTFS_QUERY_EXTENT_FAILED 0x00006A76L

//
// MessageId: MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_TOTAL_ALLOC
//
// MessageText:
//
//  The total allocated size 0x%1 for attribute of type 0x%3 and
//  instance tag 0x%4 is larger than the allocated length 0x%2.
//
#define MSG_CHKLOG_NTFS_INVALID_NON_RESIDENT_ATTR_TOTAL_ALLOC 0x00006A77L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_NOT_IN_ATTR_LIST
//
// MessageText:
//
//  Unable to locate attribute with instance tag 0x%2 and segment
//  reference 0x%3.  The expected attribute type is 0x%1.
//
#define MSG_CHKLOG_NTFS_ATTR_NOT_IN_ATTR_LIST 0x00006A78L

//
// MessageId: MSG_CHKLOG_NTFS_FIRST_INDEX_ENTRY_OFFSET_BEYOND_INDEX_LENGTH
//
// MessageText:
//
//  The first index entry offset, 0x%2, for index %1 in file 0x%4
//  points beyond the length, 0x%3, of the index.  VCN is unknown.
//
#define MSG_CHKLOG_NTFS_FIRST_INDEX_ENTRY_OFFSET_BEYOND_INDEX_LENGTH 0x00006A79L

//
// MessageId: MSG_CHKLOG_NTFS_ATTR_CLUSTERS_IN_USE
//
// MessageText:
//
//  Some clusters occupied by attribute of type 0x%1 and instance tag 0x%2
//  in file 0x%3 is already in use.
//
#define MSG_CHKLOG_NTFS_ATTR_CLUSTERS_IN_USE 0x00006A7AL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_READ_CHILD_FRS
//
// MessageText:
//
//  Unable to setup the child file record segment 0x%2 in file 0x%1.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_READ_CHILD_FRS 0x00006A7BL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_FILE_NAME_HAS_INCORRECT_PARENT
//
// MessageText:
//
//  The parent 0x%3 of index entry %4 of index %2
//  in file 0x%5 is incorrect.  The expected parent is 0x%1.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_FILE_NAME_HAS_INCORRECT_PARENT 0x00006A7CL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_FILE_REF
//
// MessageText:
//
//  The file reference 0x%4 of index entry %3 of index %2
//  with parent 0x%1 is not the same as 0x%5.
//
#define MSG_CHKLOG_NTFS_INCORRECT_INDEX_ENTRY_FILE_REF 0x00006A7DL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_UNNAMED_INDEX_ENTRY_FILE_REF
//
// MessageText:
//
//  The file reference 0x%3 of an index entry of index %2
//  with parent 0x%1 is not the same as 0x%4.
//
#define MSG_CHKLOG_NTFS_INCORRECT_UNNAMED_INDEX_ENTRY_FILE_REF 0x00006A7EL

//
// MessageId: MSG_CHKLOG_NTFS_MULTIPLE_OBJID_INDEX_ENTRIES_WITH_SAME_FILE_NUMBER
//
// MessageText:
//
//  Multiple object id index entries in file 0x%1
//  point to the same file 0x%2.
//
#define MSG_CHKLOG_NTFS_MULTIPLE_OBJID_INDEX_ENTRIES_WITH_SAME_FILE_NUMBER 0x00006A7FL

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_UNREADABLE_FRS
//
// MessageText:
//
//  The object id index entry in file 0x%1 points to file 0x%2
//  which is unreadable.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_UNREADABLE_FRS 0x00006A80L

//
// MessageId: MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NOT_INUSE_FRS
//
// MessageText:
//
//  The object id index entry in file 0x%1 points to file 0x%2
//  which is not in use.
//
#define MSG_CHKLOG_NTFS_OBJID_INDEX_ENTRY_WITH_NOT_INUSE_FRS 0x00006A81L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NOT_INUSE_FRS
//
// MessageText:
//
//  The reparse point index entry in file 0x%1 points to file 0x%2
//  which is not in use.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_NOT_INUSE_FRS 0x00006A82L

//
// MessageId: MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_UNREADABLE_FRS
//
// MessageText:
//
//  The reparse point index entry in file 0x%1 points to file 0x%2
//  which is unreadable.
//
#define MSG_CHKLOG_NTFS_REPARSE_INDEX_ENTRY_WITH_UNREADABLE_FRS 0x00006A83L

//
// MessageId: MSG_CHKLOG_NTFS_DIVIDER
//
// MessageText:
//
//  ----------------------------------------------------------------------
//
#define MSG_CHKLOG_NTFS_DIVIDER          0x00006A84L

//
// MessageId: MSG_CHKLOG_NTFS_CLEANUP_INSTANCE_TAG
//
// MessageText:
//
//  Cleaning up instance tags for file 0x%1.
//
#define MSG_CHKLOG_NTFS_CLEANUP_INSTANCE_TAG 0x00006A85L

//
// MessageId: MSG_CHKLOG_NTFS_CLEANUP_ENCRYPTED_FLAG
//
// MessageText:
//
//  Cleaning up encrypted flag for file 0x%1.
//
#define MSG_CHKLOG_NTFS_CLEANUP_ENCRYPTED_FLAG 0x00006A86L

//
// MessageId: MSG_CHKLOG_NTFS_CLEANUP_SPARSE_FLAG
//
// MessageText:
//
//  Cleaning up sparse flag for file 0x%1.
//
#define MSG_CHKLOG_NTFS_CLEANUP_SPARSE_FLAG 0x00006A87L

//
// MessageId: MSG_CHKLOG_NTFS_CLEANUP_INDEX_ENTRIES
//
// MessageText:
//
//  Cleaning up %3 unused index entries from index %2 of file 0x%1.
//
#define MSG_CHKLOG_NTFS_CLEANUP_INDEX_ENTRIES 0x00006A88L

//
// MessageId: MSG_CHKLOG_NTFS_CLEANUP_UNUSED_SECURITY_DESCRIPTORS
//
// MessageText:
//
//  Cleaning up %1 unused security descriptors.
//
#define MSG_CHKLOG_NTFS_CLEANUP_UNUSED_SECURITY_DESCRIPTORS 0x00006A89L

//
// MessageId: MSG_CHKLOG_NTFS_MFT_MIRROR_HAS_INVALID_VALUE_LENGTH
//
// MessageText:
//
//  The value length, 0x%1, of the MFT mirror is too small.
//  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_MFT_MIRROR_HAS_INVALID_VALUE_LENGTH 0x00006A8AL

//
// MessageId: MSG_CHKLOG_NTFS_MFT_MIRROR_HAS_INVALID_DATA_LENGTH
//
// MessageText:
//
//  The valid data length, 0x%1, of the MFT mirror is too small.
//  The minimum value is 0x%2.
//
#define MSG_CHKLOG_NTFS_MFT_MIRROR_HAS_INVALID_DATA_LENGTH 0x00006A8BL

//
// MessageId: MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_FREE_OR_NON_BASE_FRS
//
// MessageText:
//
//  Index entry %3 of index %2 in file 0x%1 points to unused file 0x%4.
//
#define MSG_CHKLOG_NTFS_INDEX_ENTRY_POINTS_TO_FREE_OR_NON_BASE_FRS 0x00006A8CL

//
// MessageId: MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_FREE_OR_NON_BASE_FRS
//
// MessageText:
//
//  An index entry of index %2 in file 0x%1 points to unused file 0x%3.
//
#define MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_TO_FREE_OR_NON_BASE_FRS 0x00006A8DL

//
// MessageId: MSG_CHKLOG_NTFS_UNABLE_TO_QUERY_LCN_FROM_VCN_FOR_MFT
//
// MessageText:
//
//  Unable to obtain the LCN in data attribute for VCN 0x%1 of the MFT.
//
#define MSG_CHKLOG_NTFS_UNABLE_TO_QUERY_LCN_FROM_VCN_FOR_MFT 0x00006A8EL

//
// MessageId: MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_BEYOND_MFT
//
// MessageText:
//
//  An index entry of index %2 in file 0x%1 points to file 0x%3
//  which is beyond the MFT.
//
#define MSG_CHKLOG_NTFS_UNNAMED_INDEX_ENTRY_POINTS_BEYOND_MFT 0x00006A8FL

//
// MessageId: MSG_CHKLOG_NTFS_INCORRECT_SEGMENT_NUMBER
//
// MessageText:
//
//  The segment number 0x%1 in file 0x%2 is incorrect.
//
#define MSG_CHKLOG_NTFS_INCORRECT_SEGMENT_NUMBER 0x00006A90L

//
// MessageId: MSG_CHKLOG_NTFS_INTERNAL_INFO
//
// MessageText:
//
//  
//  Internal Info:
//
#define MSG_CHKLOG_NTFS_INTERNAL_INFO    0x00006A91L

//
// MessageId: MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_HAS_UNALIGNED_MAPPING_PAIRS_OFFSET
//
// MessageText:
//
//  The mapping pairs offset 0x%1 for attribute of type 0x%2 and instance
//  tag 0x%3 is not quad aligned.
//
#define MSG_CHKLOG_NTFS_NON_RESIDENT_ATTR_HAS_UNALIGNED_MAPPING_PAIRS_OFFSET 0x00006A92L

//---------------
//
// Common messages.
//
//---------------
//
// MessageId: MSG_UTILS_HELP
//
// MessageText:
//
//  There is no help for this utility.
//
#define MSG_UTILS_HELP                   0x00007530L

//
// MessageId: MSG_UTILS_ERROR_FATAL
//
// MessageText:
//
//  Critical error encountered.
//
#define MSG_UTILS_ERROR_FATAL            0x00007531L

//
// MessageId: MSG_UTILS_ERROR_INVALID_VERSION
//
// MessageText:
//
//  Incorrect Windows XP version
//
#define MSG_UTILS_ERROR_INVALID_VERSION  0x00007532L

//----------------------
//
// Convert messages.
//
//----------------------
//
// MessageId: MSG_CONV_USAGE
//
// MessageText:
//
//  Converts FAT volumes to NTFS.
//  
//  CONVERT volume /FS:NTFS [/V] [/CvtArea:filename] [/NoSecurity] [/X]
//  
//    volume      Specifies the drive letter (followed by a colon),
//                mount point, or volume name.
//    /FS:NTFS    Specifies that the volume is to be converted to NTFS.
//    /V          Specifies that Convert should be run in verbose mode.
//    /CvtArea:filename
//                Specifies a contiguous file in the root directory to be
//                the place holder for NTFS system files.
//    /NoSecurity Specifies the converted files and directories security
//                settings to be accessible by everyone.
//    /X          Forces the volume to dismount first if necessary.
//                All opened handles to the volume would then be invalid.
//
#define MSG_CONV_USAGE                   0x00007594L

//
// MessageId: MSG_CONV_INVALID_PARAMETER
//
// MessageText:
//
//  Invalid Parameter - %1
//
#define MSG_CONV_INVALID_PARAMETER       0x00007595L

//
// MessageId: MSG_CONV_NO_FILESYSTEM_SPECIFIED
//
// MessageText:
//
//  Must specify a file system
//
#define MSG_CONV_NO_FILESYSTEM_SPECIFIED 0x00007596L

//
// MessageId: MSG_CONV_INVALID_DRIVE_SPEC
//
// MessageText:
//
//  Invalid drive specification.
//
#define MSG_CONV_INVALID_DRIVE_SPEC      0x00007597L

//
// MessageId: MSG_CONV_CANT_NETWORK
//
// MessageText:
//
//  Cannot CONVERT a network drive.
//
#define MSG_CONV_CANT_NETWORK            0x00007598L

//
// MessageId: MSG_CONV_INVALID_FILESYSTEM
//
// MessageText:
//
//  %1 is not a valid file system
//
#define MSG_CONV_INVALID_FILESYSTEM      0x00007599L

//
// MessageId: MSG_CONV_CONVERSION_NOT_AVAILABLE
//
// MessageText:
//
//  Conversion from %1 volume to %2 volume is not available.
//
#define MSG_CONV_CONVERSION_NOT_AVAILABLE 0x0000759AL

//
// MessageId: MSG_CONV_WILL_CONVERT_ON_REBOOT
//
// MessageText:
//
//  The conversion will take place automatically the next time the
//  system restarts.
//
#define MSG_CONV_WILL_CONVERT_ON_REBOOT  0x0000759BL

//
// MessageId: MSG_CONV_CANNOT_FIND_SYSTEM_DIR
//
// MessageText:
//
//  Cannot determine location of system directory.
//
#define MSG_CONV_CANNOT_FIND_SYSTEM_DIR  0x0000759CL

//
// MessageId: MSG_CONV_CANNOT_FIND_FILE
//
// MessageText:
//
//  Could not find file %1
//  Make sure that the required file exists and try again.
//
#define MSG_CONV_CANNOT_FIND_FILE        0x0000759DL

//
// MessageId: MSG_CONV_CANNOT_SCHEDULE
//
// MessageText:
//
//  Could not schedule an automatic conversion of the drive.
//
#define MSG_CONV_CANNOT_SCHEDULE         0x0000759EL

//
// MessageId: MSG_CONV_ALREADY_SCHEDULED
//
// MessageText:
//
//  The %1 drive is already scheduled for an automatic
//  conversion.
//
#define MSG_CONV_ALREADY_SCHEDULED       0x0000759FL

//
// MessageId: MSG_CONV_CONVERTING
//
// MessageText:
//
//  Converting drive %1 to %2...
//
#define MSG_CONV_CONVERTING              0x000075A0L

//
// MessageId: MSG_CONV_ALREADY_CONVERTED
//
// MessageText:
//
//  Drive %1 is already %2.
//
#define MSG_CONV_ALREADY_CONVERTED       0x000075A1L

//
// MessageId: MSG_CONV_CANNOT_AUTOCHK
//
// MessageText:
//
//  Could not check volume %1 for errors.
//  The conversion to %2 did not take place.
//
#define MSG_CONV_CANNOT_AUTOCHK          0x000075A2L

//
// MessageId: MSG_CONV_SLASH_C_INVALID
//
// MessageText:
//
//  The /C option is only valid with the /UNCOMPRESS option.
//
#define MSG_CONV_SLASH_C_INVALID         0x000075A3L

//
// MessageId: MSG_CONV_CHECKING_SPACE
//
// MessageText:
//
//  Determining disk space required for file system conversion...
//
#define MSG_CONV_CHECKING_SPACE          0x000075A8L

//
// MessageId: MSG_CONV_KBYTES_TOTAL
//
// MessageText:
//
//  Total disk space:              %1 KB
//
#define MSG_CONV_KBYTES_TOTAL            0x000075A9L

//
// MessageId: MSG_CONV_KBYTES_FREE
//
// MessageText:
//
//  Free space on volume:          %1 KB
//
#define MSG_CONV_KBYTES_FREE             0x000075AAL

//
// MessageId: MSG_CONV_KBYTES_NEEDED
//
// MessageText:
//
//  Space required for conversion: %1 KB
//
#define MSG_CONV_KBYTES_NEEDED           0x000075ABL

//
// MessageId: MSG_CONV_CONVERTING_FS
//
// MessageText:
//
//  Converting file system
//
#define MSG_CONV_CONVERTING_FS           0x000075ACL

//
// MessageId: MSG_CONV_PERCENT_COMPLETE
//
// MessageText:
//
//  %1 percent completed.                  %r%0
//
#define MSG_CONV_PERCENT_COMPLETE        0x000075ADL

//
// MessageId: MSG_CONV_CONVERSION_COMPLETE
//
// MessageText:
//
//  Conversion complete
//
#define MSG_CONV_CONVERSION_COMPLETE     0x000075AEL

//
// MessageId: MSG_CONV_CONVERSION_FAILED
//
// MessageText:
//
//  The conversion failed.
//  %1 was not converted to %2
//
#define MSG_CONV_CONVERSION_FAILED       0x000075AFL

//
// MessageId: MSG_CONV_CONVERSION_MAYHAVE_FAILED
//
// MessageText:
//
//  The conversion has probably failed.
//  %1 may not be converted to %2
//
#define MSG_CONV_CONVERSION_MAYHAVE_FAILED 0x000075B0L

//
// MessageId: MSG_CONV_CANNOT_READ
//
// MessageText:
//
//  Error during disk read
//
#define MSG_CONV_CANNOT_READ             0x000075C6L

//
// MessageId: MSG_CONV_CANNOT_WRITE
//
// MessageText:
//
//  Error during disk write
//
#define MSG_CONV_CANNOT_WRITE            0x000075C7L

//
// MessageId: MSG_CONV_NO_MEMORY
//
// MessageText:
//
//  Insufficient Memory
//
#define MSG_CONV_NO_MEMORY               0x000075C8L

//
// MessageId: MSG_CONV_NO_DISK_SPACE
//
// MessageText:
//
//  Insufficient disk space for conversion
//
#define MSG_CONV_NO_DISK_SPACE           0x000075C9L

//
// MessageId: MSG_CONV_CANNOT_RELOCATE
//
// MessageText:
//
//  Cannot relocate existing file system structures
//
#define MSG_CONV_CANNOT_RELOCATE         0x000075CAL

//
// MessageId: MSG_CONV_CANNOT_CREATE_ELEMENTARY
//
// MessageText:
//
//  Cannot create the elementary file system structures.
//
#define MSG_CONV_CANNOT_CREATE_ELEMENTARY 0x000075CBL

//
// MessageId: MSG_CONV_ERROR_READING_DIRECTORY
//
// MessageText:
//
//  Error reading directory %1
//
#define MSG_CONV_ERROR_READING_DIRECTORY 0x000075CCL

//
// MessageId: MSG_CONV_CANNOT_CONVERT_DIRECTORY
//
// MessageText:
//
//  Error converting directory %1.
//  The directory may be damaged or there may be insufficient disk space.
//
#define MSG_CONV_CANNOT_CONVERT_DIRECTORY 0x000075CDL

//
// MessageId: MSG_CONV_CANNOT_CONVERT_FILE
//
// MessageText:
//
//  Error converting file %1.
//  The file may be damaged or there may be insufficient disk space.
//
#define MSG_CONV_CANNOT_CONVERT_FILE     0x000075CEL

//
// MessageId: MSG_CONV_CANNOT_CONVERT_DATA
//
// MessageText:
//
//  Error converting file data.
//
#define MSG_CONV_CANNOT_CONVERT_DATA     0x000075CFL

//
// MessageId: MSG_CONV_CANNOT_CONVERT_EA
//
// MessageText:
//
//  Cannot convert an extended attribute.
//
#define MSG_CONV_CANNOT_CONVERT_EA       0x000075D0L

//
// MessageId: MSG_CONV_NO_EA_FILE
//
// MessageText:
//
//  A file contains extended attributes,
//  but the extended attribute file was not found.
//
#define MSG_CONV_NO_EA_FILE              0x000075D1L

//
// MessageId: MSG_CONV_CANNOT_MAKE_INDEX
//
// MessageText:
//
//  Cannot locate or create an NTFS index.
//
#define MSG_CONV_CANNOT_MAKE_INDEX       0x000075D2L

//
// MessageId: MSG_CONV_CANNOT_CONVERT_VOLUME
//
// MessageText:
//
//  This volume cannot be converted to %1.
//  Possible causes are:
//      1.- Bad sectors in required areas of the volume.
//      2.- %2 structures in areas required by %1.
//
#define MSG_CONV_CANNOT_CONVERT_VOLUME   0x000075D3L

//
// MessageId: MSG_CONVERT_ON_REBOOT_PROMPT
//
// MessageText:
//
//  Convert cannot gain exclusive access to the %1 drive,
//  so it cannot convert it now.  Would you like to
//  schedule it to be converted the next time the
//  system restarts (Y/N)? %0
//
#define MSG_CONVERT_ON_REBOOT_PROMPT     0x000075D4L

//
// MessageId: MSG_CONVERT_FILE_SYSTEM_NOT_ENABLED
//
// MessageText:
//
//  The %1 file system is not enabled.  The volume
//  will not be converted.
//
#define MSG_CONVERT_FILE_SYSTEM_NOT_ENABLED 0x000075D5L

//
// MessageId: MSG_CONVERT_UNSUPPORTED_SECTOR_SIZE
//
// MessageText:
//
//  Unsupported sector size.  Cannot convert volume to %1.
//
#define MSG_CONVERT_UNSUPPORTED_SECTOR_SIZE 0x000075D6L

//
// MessageId: MSG_CONVERT_REBOOT
//
// MessageText:
//
//  
//  The file system has been converted.
//  Please wait while the system restarts.
//
#define MSG_CONVERT_REBOOT               0x000075D7L

//
// MessageId: MSG_CONV_ARC_SYSTEM_PARTITION
//
// MessageText:
//
//  The specified drive is the system partition on an ARC-compliant
//  system; its file system cannot be converted
//
#define MSG_CONV_ARC_SYSTEM_PARTITION    0x000075D8L

//
// MessageId: MSG_CONV_GEOMETRY_MISMATCH
//
// MessageText:
//
//  The disk geometry recorded in the volume's Bios Parameter
//  Block differs from the geometry reported by the driver.
//  This volume cannot be converted to %1.
//
#define MSG_CONV_GEOMETRY_MISMATCH       0x000075D9L

//
// MessageId: MSG_CONV_NAME_TABLE_NOT_SUPPORTED
//
// MessageText:
//
//  Name table translation is not available for conversion to %1.
//
#define MSG_CONV_NAME_TABLE_NOT_SUPPORTED 0x000075DAL

//
// MessageId: MSG_CONV_VOLUME_TOO_FRAGMENTED
//
// MessageText:
//
//  The volume is too fragmented to be converted to NTFS.
//
#define MSG_CONV_VOLUME_TOO_FRAGMENTED   0x000075E9L

//
// MessageId: MSG_CONV_NO_FILE_SYSTEM
//
// MessageText:
//
//  Cannot find the utility library which contains CHKDSK for the
//  %1 file system. This volume cannot be converted to %2.
//
#define MSG_CONV_NO_FILE_SYSTEM          0x000075EAL

//
// MessageId: MSG_CONV_NTFS_RESERVED_NAMES
//
// MessageText:
//
//  %1 cannot be converted because it contains files or directories
//  with reserved NTFS names in the root directory. There can be no
//  files or directories named $Mft, $MftMirr, $LogFile, $Volume,
//  $AttrDef, $BitMap, $Boot, $BadClus, $Secure, $UpCase, $Extend
//  or $Quota in the root directory.
//
#define MSG_CONV_NTFS_RESERVED_NAMES     0x000075EBL

//
// MessageId: MSG_CONV_DISK_IS_DIRTY
//
// MessageText:
//
//  This drive is dirty and cannot be converted. You will need to
//  clear the dirty bit on this drive by running CHKDSK /F or allowing
//  AUTOCHK to run on it the next time you reboot.
//
#define MSG_CONV_DISK_IS_DIRTY           0x000075ECL

//
// MessageId: MSG_CONV_CVTAREA_FILE_MISSING
//
// MessageText:
//
//  The file %1 specified to the /CvtArea option cannot be found at the root.
//
#define MSG_CONV_CVTAREA_FILE_MISSING    0x000075EDL

//
// MessageId: MSG_CONV_CVTAREA_FILE_NOT_CONTIGUOUS
//
// MessageText:
//
//  The file %1 specified to the /CvtArea option must be in one contiguous block.
//
#define MSG_CONV_CVTAREA_FILE_NOT_CONTIGUOUS 0x000075EEL

//
// MessageId: MSG_CONV_CVTAREA_MUST_BE_FILE
//
// MessageText:
//
//  The name %1 specified to the /CvtArea option must be a file name.
//
#define MSG_CONV_CVTAREA_MUST_BE_FILE    0x000075EFL

//
// MessageId: MSG_CONV_FORCE_DISMOUNT_PROMPT
//
// MessageText:
//
//  Convert cannot run because the volume is in use by another
//  process.  Convert may run if this volume is dismounted first.
//  ALL OPENED HANDLES TO THIS VOLUME WOULD THEN BE INVALID.
//  Would you like to force a dismount on this volume? (Y/N) %0
//
#define MSG_CONV_FORCE_DISMOUNT_PROMPT   0x000075F0L

//
// MessageId: MSG_CONV_UNABLE_TO_DISMOUNT
//
// MessageText:
//
//  Convert failed to dismount the volume.
//
#define MSG_CONV_UNABLE_TO_DISMOUNT      0x000075F1L

//
// MessageId: MSG_CONV_SCE_FAILURE_WITH_MESSAGE
//
// MessageText:
//
//  %1
//
#define MSG_CONV_SCE_FAILURE_WITH_MESSAGE 0x000075F2L

//
// MessageId: MSG_CONV_SCE_SET_FAILURE
//
// MessageText:
//
//  Unable to set security attributes (%1).
//
#define MSG_CONV_SCE_SET_FAILURE         0x000075F3L

//
// MessageId: MSG_CONV_SCE_SCHEDULE_FAILURE
//
// MessageText:
//
//  Unable to schedule the setting of security attributes (%1).
//
#define MSG_CONV_SCE_SCHEDULE_FAILURE    0x000075F4L

//
// MessageId: MSG_CONV_WRITE_PROTECTED
//
// MessageText:
//
//  Cannot convert %1.  The volume is write protected.
//
#define MSG_CONV_WRITE_PROTECTED         0x000075F5L

//
// MessageId: MSG_CONV_CANT_CDROM
//
// MessageText:
//
//  Cannot CONVERT volume on this device.
//
#define MSG_CONV_CANT_CDROM              0x000075F6L

//
// MessageId: MSG_CONV_CVTAREA_TOO_SMALL
//
// MessageText:
//
//  WARNING!  The file specified to /CvtArea is too small and its
//  space will not be used.  A file of at least %1 MB is needed.
//
#define MSG_CONV_CVTAREA_TOO_SMALL       0x000075F7L

//
// MessageId: MSG_CONV_DELETE_UNINSTALL_BACKUP
//
// MessageText:
//
//  This conversion will also remove your previous operating system
//  backup.  Do you want to continue? (Y/N) %0
//
#define MSG_CONV_DELETE_UNINSTALL_BACKUP 0x000075F8L

//
// MessageId: MSG_CONV_DELETE_UNINSTALL_BACKUP_ERROR
//
// MessageText:
//
//  Unable to delete uninstall backup image - %1
//
#define MSG_CONV_DELETE_UNINSTALL_BACKUP_ERROR 0x000075F9L

//
// MessageId: MSG_CONV_UNABLE_TO_NOTIFY
//
// MessageText:
//
//  Unable to notify other components that this volume has changed.
//
#define MSG_CONV_UNABLE_TO_NOTIFY        0x000075FAL

//
// MessageId: MSG_CONV_REBOOT_AFTER_RELOCATION
//
// MessageText:
//
//  Convert has relocated existing file system structures.
//  A restart of your computer is necessary in order for convert to continue.
//
#define MSG_CONV_REBOOT_AFTER_RELOCATION 0x000075FBL

//----------------------
//
// CHCP messages.
//
//----------------------
//
// MessageId: MSG_CHCP_INVALID_PARAMETER
//
// MessageText:
//
//  Parameter format not correct - %1
//
#define MSG_CHCP_INVALID_PARAMETER       0x0000768EL

//
// MessageId: MSG_CHCP_ACTIVE_CODEPAGE
//
// MessageText:
//
//  Active code page: %1
//
#define MSG_CHCP_ACTIVE_CODEPAGE         0x00007692L

//
// MessageId: MSG_CHCP_INVALID_CODEPAGE
//
// MessageText:
//
//  Invalid code page
//
#define MSG_CHCP_INVALID_CODEPAGE        0x00007693L

//
// MessageId: MSG_CHCP_USAGE
//
// MessageText:
//
//  Displays or sets the active code page number.
//  
//  CHCP [nnn]
//  
//    nnn   Specifies a code page number.
//  
//  Type CHCP without a parameter to display the active code page number.
//
#define MSG_CHCP_USAGE                   0x00007694L

//
// MessageId: MSG_CHCP_INTERNAL_ERROR
//
// MessageText:
//
//  Internal error.
//
#define MSG_CHCP_INTERNAL_ERROR          0x00007695L

//----------------
//
// DOSKEY messages
//
//----------------
//
// MessageId: MSG_DOSKEY_INVALID_MACRO_DEFINITION
//
// MessageText:
//
//  Invalid macro definition.
//
#define MSG_DOSKEY_INVALID_MACRO_DEFINITION 0x00007727L

//
// MessageId: MSG_DOSKEY_HELP
//
// MessageText:
//
//  Edits command lines, recalls Windows XP commands, and creates macros.
//  
//  DOSKEY [/REINSTALL] [/LISTSIZE=size] [/MACROS[:ALL | :exename]]
//    [/HISTORY] [/INSERT | /OVERSTRIKE] [/EXENAME=exename] [/MACROFILE=filename]
//    [macroname=[text]]
//  
//    /REINSTALL          Installs a new copy of Doskey.
//    /LISTSIZE=size      Sets size of command history buffer.
//    /MACROS             Displays all Doskey macros.
//    /MACROS:ALL         Displays all Doskey macros for all executables which have
//                        Doskey macros.
//    /MACROS:exename     Displays all Doskey macros for the given executable.
//    /HISTORY            Displays all commands stored in memory.
//    /INSERT             Specifies that new text you type is inserted in old text.
//    /OVERSTRIKE         Specifies that new text overwrites old text.
//    /EXENAME=exename    Specifies the executable.
//    /MACROFILE=filename Specifies a file of macros to install.
//    macroname           Specifies a name for a macro you create.
//    text                Specifies commands you want to record.
//  
//  UP and DOWN ARROWS recall commands; ESC clears command line; F7 displays
//  command history; ALT+F7 clears command history; F8 searches command
//  history; F9 selects a command by number; ALT+F10 clears macro definitions.
//  
//  The following are some special codes in Doskey macro definitions:
//  $T     Command separator.  Allows multiple commands in a macro.
//  $1-$9  Batch parameters.  Equivalent to %%1-%%9 in batch programs.
//  $*     Symbol replaced by everything following macro name on command line.
//
#define MSG_DOSKEY_HELP                  0x00007728L

//
// MessageId: MSG_DOSKEY_CANT_DO_BUFSIZE
//
// MessageText:
//
//  To specify the size of the command history buffer under Window NT,
//  use the /listsize switch which sets the number of commands to remember.
//
#define MSG_DOSKEY_CANT_DO_BUFSIZE       0x00007729L

//
// MessageId: MSG_DOSKEY_CANT_SIZE_LIST
//
// MessageText:
//
//  Insufficient memory to grow DOSKEY list.
//
#define MSG_DOSKEY_CANT_SIZE_LIST        0x0000772AL

//----------------
//
// SUBST messages
//
//----------------
//
// MessageId: MSG_SUBST_INFO
//
// MessageText:
//
//  Associates a path with a drive letter.
//  
//
#define MSG_SUBST_INFO                   0x0000772BL

//
// MessageId: MSG_SUBST_ALREADY_SUBSTED
//
// MessageText:
//
//  Drive already SUBSTed
//
#define MSG_SUBST_ALREADY_SUBSTED        0x0000772CL

//
// MessageId: MSG_SUBST_USAGE
//
// MessageText:
//
//  SUBST [drive1: [drive2:]path]
//  SUBST drive1: /D
//  
//    drive1:        Specifies a virtual drive to which you want to assign a path.
//    [drive2:]path  Specifies a physical drive and path you want to assign to
//                   a virtual drive.
//    /D             Deletes a substituted (virtual) drive.
//  
//  Type SUBST with no parameters to display a list of current virtual drives.
//
#define MSG_SUBST_USAGE                  0x0000772DL

//
// MessageId: MSG_SUBST_SUBSTED_DRIVE
//
// MessageText:
//
//  %1: => %2
//
#define MSG_SUBST_SUBSTED_DRIVE          0x0000772EL

//
// MessageId: MSG_SUBST_INVALID_PARAMETER
//
// MessageText:
//
//  Invalid parameter - %1
//
#define MSG_SUBST_INVALID_PARAMETER      0x0000772FL

//
// MessageId: MSG_SUBST_TOO_MANY_PARAMETERS
//
// MessageText:
//
//  Incorrect number of parameters - %1
//
#define MSG_SUBST_TOO_MANY_PARAMETERS    0x00007730L

//
// MessageId: MSG_SUBST_PATH_NOT_FOUND
//
// MessageText:
//
//  Path not found - %1
//
#define MSG_SUBST_PATH_NOT_FOUND         0x00007731L

//
// MessageId: MSG_SUBST_ACCESS_DENIED
//
// MessageText:
//
//  Access denied - %1
//
#define MSG_SUBST_ACCESS_DENIED          0x00007732L

//----------------
//
// CHKNTFS messages
//
//----------------
//
// MessageId: MSG_CHKNTFS_INVALID_FORMAT
//
// MessageText:
//
//  CHKNTFS: Incorrect command-line format.
//
#define MSG_CHKNTFS_INVALID_FORMAT       0x00007738L

//
// MessageId: MSG_CHKNTFS_INVALID_SWITCH
//
// MessageText:
//
//  Invalid parameter - %1
//
#define MSG_CHKNTFS_INVALID_SWITCH       0x00007739L

//
// MessageId: MSG_CHKNTFS_NO_WILDCARDS
//
// MessageText:
//
//  CHKNTFS: drive specifiers may not contain wildcards.
//
#define MSG_CHKNTFS_NO_WILDCARDS         0x0000773AL

//
// MessageId: MSG_CHKNTFS_USAGE
//
// MessageText:
//
//  Displays or modifies the checking of disk at boot time.
//  
//  CHKNTFS volume [...]
//  CHKNTFS /D
//  CHKNTFS /T[:time]
//  CHKNTFS /X volume [...]
//  CHKNTFS /C volume [...]
//  
//    volume         Specifies the drive letter (followed by a colon),
//                   mount point, or volume name.
//    /D             Restores the machine to the default behavior; all drives are
//                   checked at boot time and chkdsk is run on those that are
//                   dirty.
//    /T:time        Changes the AUTOCHK initiation countdown time to the
//                   specified amount of time in seconds.  If time is not
//                   specified, displays the current setting.
//    /X             Excludes a drive from the default boot-time check.  Excluded
//                   drives are not accumulated between command invocations.
//    /C             Schedules a drive to be checked at boot time; chkdsk will run
//                   if the drive is dirty.
//  
//  If no switches are specified, CHKNTFS will display if the specified drive is
//  dirty or scheduled to be checked on next reboot.
//
#define MSG_CHKNTFS_USAGE                0x0000773BL

//
// MessageId: MSG_CHKNTFS_ARGS_CONFLICT
//
// MessageText:
//
//  Specify only one of /D, /X, /C, and /E.
//
#define MSG_CHKNTFS_ARGS_CONFLICT        0x0000773CL

//
// MessageId: MSG_CHKNTFS_REQUIRES_DRIVE
//
// MessageText:
//
//  You must specify at least one drive name.
//
#define MSG_CHKNTFS_REQUIRES_DRIVE       0x0000773DL

//
// MessageId: MSG_CHKNTFS_BAD_ARG
//
// MessageText:
//
//  %1 is not a valid drive specification.
//
#define MSG_CHKNTFS_BAD_ARG              0x0000773EL

//
// MessageId: MSG_CHKNTFS_CANNOT_CHECK
//
// MessageText:
//
//  Cannot query state of drive %1
//
#define MSG_CHKNTFS_CANNOT_CHECK         0x0000773FL

//
// MessageId: MSG_CHKNTFS_DIRTY
//
// MessageText:
//
//  %1 is dirty.  You may use the /C option to schedule chkdsk for
//      this drive.
//
#define MSG_CHKNTFS_DIRTY                0x00007740L

//
// MessageId: MSG_CHKNTFS_CLEAN
//
// MessageText:
//
//  %1 is not dirty.
//
#define MSG_CHKNTFS_CLEAN                0x00007741L

//
// MessageId: MSG_CHKNTFS_NONEXISTENT_DRIVE
//
// MessageText:
//
//  Drive %1 does not exist.
//
#define MSG_CHKNTFS_NONEXISTENT_DRIVE    0x00007742L

//
// MessageId: MSG_CHKNTFS_NO_NETWORK
//
// MessageText:
//
//  CHKNTFS cannot be used for the network drive %1.
//
#define MSG_CHKNTFS_NO_NETWORK           0x00007743L

//
// MessageId: MSG_CHKNTFS_NO_CDROM
//
// MessageText:
//
//  CHKNTFS cannot be used for the cdrom drive %1.
//
#define MSG_CHKNTFS_NO_CDROM             0x00007744L

//
// MessageId: MSG_CHKNTFS_NO_RAMDISK
//
// MessageText:
//
//  CHKNTFS cannot be used for the ram disk %1.
//
#define MSG_CHKNTFS_NO_RAMDISK           0x00007745L

//
// MessageId: MSG_CHKNTFS_NOT_ENABLE_UPGRADE
//
// MessageText:
//
//  Unable to enable automatic volume upgrade on drive %1.
//
#define MSG_CHKNTFS_NOT_ENABLE_UPGRADE   0x00007746L

//
// MessageId: MSG_CHKNTFS_SKIP_DRIVE
//
// MessageText:
//
//  Skipping drive %1 because it is not an NTFS volume.
//
#define MSG_CHKNTFS_SKIP_DRIVE           0x00007747L

//
// MessageId: MSG_CHKNTFS_FLOPPY_DRIVE
//
// MessageText:
//
//  CHKNTFS does not work on floppy drive.
//
#define MSG_CHKNTFS_FLOPPY_DRIVE         0x00007748L

//
// MessageId: MSG_CHKNTFS_SKIP_DRIVE_RAW
//
// MessageText:
//
//  Skipping drive %1 because it is not an NTFS, FAT, or FAT32 volume.
//
#define MSG_CHKNTFS_SKIP_DRIVE_RAW       0x00007749L

//
// MessageId: MSG_CHKNTFS_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH
//
// MessageText:
//
//  The volume %1
//  does not have a mount point or drive letter.
//
#define MSG_CHKNTFS_NO_MOUNT_POINT_FOR_GUID_VOLNAME_PATH 0x0000774AL

//
// MessageId: MSG_CHKNTFS_CHKDSK_WILL_RUN
//
// MessageText:
//
//  Chkdsk has been scheduled manually to run on next reboot
//  on volume %1.
//
#define MSG_CHKNTFS_CHKDSK_WILL_RUN      0x0000774BL

//
// MessageId: MSG_CHKNTFS_INVALID_AUTOCHK_COUNT_DOWN_TIME
//
// MessageText:
//
//  The specified AUTOCHK initiation countdown time cannot be less than zero or
//  larger than %1 seconds.  The default is %2 seconds.
//
#define MSG_CHKNTFS_INVALID_AUTOCHK_COUNT_DOWN_TIME 0x0000774CL

//
// MessageId: MSG_CHKNTFS_AUTOCHK_SET_COUNT_DOWN_TIME_FAILED
//
// MessageText:
//
//  Unable to set the AUTOCHK initiation countdown time.
//
#define MSG_CHKNTFS_AUTOCHK_SET_COUNT_DOWN_TIME_FAILED 0x0000774DL

//
// MessageId: MSG_CHKNTFS_AUTOCHK_COUNT_DOWN_TIME
//
// MessageText:
//
//  The AUTOCHK initiation countdown time is set to %1 second(s).
//
#define MSG_CHKNTFS_AUTOCHK_COUNT_DOWN_TIME 0x0000774EL

//-------------------------------------
//
// Messages for FAT and NTFS boot code
//
//-------------------------------------
//
// MessageId: MSG_BOOT_FAT_NTLDR_MISSING
//
// MessageText:
//
//  Remove disks or other media.%0
//
#define MSG_BOOT_FAT_NTLDR_MISSING       0x00007756L

//
// MessageId: MSG_BOOT_FAT_IO_ERROR
//
// MessageText:
//
//  Disk error%0
//
#define MSG_BOOT_FAT_IO_ERROR            0x00007757L

//
// MessageId: MSG_BOOT_FAT_PRESS_KEY
//
// MessageText:
//
//  Press any key to restart%0
//
#define MSG_BOOT_FAT_PRESS_KEY           0x00007758L

//
// MessageId: MSG_BOOT_NTFS_NTLDR_MISSING
//
// MessageText:
//
//  NTLDR is missing%0
//
#define MSG_BOOT_NTFS_NTLDR_MISSING      0x00007759L

//
// MessageId: MSG_BOOT_NTFS_NTLDR_COMPRESSED
//
// MessageText:
//
//  NTLDR is compressed%0
//
#define MSG_BOOT_NTFS_NTLDR_COMPRESSED   0x0000775AL

//
// MessageId: MSG_BOOT_NTFS_IO_ERROR
//
// MessageText:
//
//  A disk read error occurred%0
//
#define MSG_BOOT_NTFS_IO_ERROR           0x0000775BL

//
// MessageId: MSG_BOOT_NTFS_PRESS_KEY
//
// MessageText:
//
//  Press Ctrl+Alt+Del to restart%0
//
#define MSG_BOOT_NTFS_PRESS_KEY          0x0000775CL

//-------------------------------------
//
// Messages for UUDF
//
//-------------------------------------
//
// MessageId: MSG_UDF_VOLUME_INFO
//
// MessageText:
//
//  Volume %1 is UDF version %2.
//
#define MSG_UDF_VOLUME_INFO              0x00007760L

//
// MessageId: MSG_UDF_VERSION_UNSUPPORTED
//
// MessageText:
//
//  The volume in %1 contains an unsupported UDF version.
//
#define MSG_UDF_VERSION_UNSUPPORTED      0x00007761L

//
// MessageId: MSG_UDF_FILE_SYSTEM_INFO
//
// MessageText:
//
//  Checking File System %1.
//
#define MSG_UDF_FILE_SYSTEM_INFO         0x00007762L

//
// MessageId: MSG_UDF_INVALID_SYSTEM_STREAM
//
// MessageText:
//
//  Invalid System Stream detected.  System stream information is lost.
//
#define MSG_UDF_INVALID_SYSTEM_STREAM    0x00007763L

//
// MessageId: MSG_UDF_VOLUME_NOT_CLOSED
//
// MessageText:
//
//  The volume was not previously closed properly and may contain errors.
//
#define MSG_UDF_VOLUME_NOT_CLOSED        0x00007764L

