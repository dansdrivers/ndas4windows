#ifndef __DVDREAD_DVD_UDF_H__
#define __DVDREAD_DVD_UDF_H__


#include "../inc/dvd_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Looks for a file on the UDF disc/imagefile and returns the block number
 * where it begins, or 0 if it is not found.  The filename should be an
 * absolute pathname on the UDF filesystem, starting with '/'.  For example,
 * '/VIDEO_TS/VTS_01_1.IFO'.  On success, filesize will be set to the size of
 * the file in bytes.
 */
uint32_t UDFFindFile( dvd_reader_t *device, char *filename, uint32_t *size );
int UDFFindPartition( dvd_reader_t *device, int partnum, struct Partition *part ) ;

#ifdef __cplusplus
};
#endif
#endif // __DVDREAD_DVD_UDF_H__
