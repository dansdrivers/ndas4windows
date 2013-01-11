#ifndef __DVDREAD_IFO_PRINT_H__
#define __DVDREAD_IFO_PRINT_H__



#include "ifo_types.h"
#include "dvd_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This  provides example functions for printing information about the IFO
 * file to stdout.
 */

/**
 * Print the complete parsing information for the given file.
 */

/* ifoPrint(dvd, title); */
void ifoPrint(dvd_reader_t *, int);

void ifoPrint_VMGI_MAT(vmgi_mat_t *);
void ifoPrint_VTSI_MAT(vtsi_mat_t *);

void ifoPrint_PTL_MAIT(ptl_mait_t *);
void ifoPrint_VTS_ATRT(vts_atrt_t *);
void ifoPrint_TT_SRPT(tt_srpt_t *);
void ifoPrint_VTS_PTT_SRPT(vts_ptt_srpt_t *);
void ifoPrint_PGC(pgc_t *);
void ifoPrint_PGCIT(pgcit_t *);
void ifoPrint_PGCI_UT(pgci_ut_t *);
void ifoPrint_C_ADT(c_adt_t *);
void ifoPrint_VOBU_ADMAP(vobu_admap_t *);

#ifdef __cplusplus
};
#endif
#endif // __DVDREAD_IFO_PRINT_H__
