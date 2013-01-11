#ifndef __DVDREAD_NAV_PRINT_H__
#define __DVDREAD_NAV_PRINT_H__

#include "nav_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prints information contained in the PCI to stdout.
 */
void navPrint_PCI(pci_t *);
  
/**
 * Prints information contained in the DSI to stdout.
 */
void navPrint_DSI(dsi_t *);

#ifdef __cplusplus
};
#endif
#endif // __DVDREAD_NAV_PRINT_H__ 
