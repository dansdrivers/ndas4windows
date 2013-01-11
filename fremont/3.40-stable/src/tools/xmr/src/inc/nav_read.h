#ifndef __DVDREAD_NAV_READ_H__
#define __DVDREAD_NAV_READ_H__

#include "nav_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Reads the PCI packet data pointed to into pci struct.
 */
void navRead_PCI(pci_t *, unsigned char *);

/**
 * Reads the DSI packet data pointed to into dsi struct.
 */
void navRead_DSI(dsi_t *, unsigned char *);

#ifdef __cplusplus
};
#endif
#endif // __DVDREAD_NAV_READ_H__
