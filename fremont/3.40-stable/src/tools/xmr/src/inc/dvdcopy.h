#ifndef __DVDCOPY_DVD_COPY_H__
#define __DVDCOPY_DVD_COPY_H__

#ifdef __cplusplus
extern "C" {
#endif


int GetDVDInfo( char * Sdevfile);
int GetDVDSize( char * Sdevfile, int * size);
int SetDVDCopytoMedia( char * Sdevfile, int FD);
int FileDVDCopytoMedia( char * Sdevfile, char * Directory);
int GetDVDTitleName(char * Sdevfile, char * buf); // buf must be larget than 33 --> 50

#ifdef __cplusplus
}
#endif
#endif //#ifndef __DVDCOPY_DVD_COPY_H__