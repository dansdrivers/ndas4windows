#ifndef __DVDREAD_NAV_TYPES_H__
#define __DVDREAD_NAV_TYPES_H__

#include "ifo_types.h" // only dvd_time_t, vm_cmd_t and user_ops_t





/* The length including the substream id byte. */
#define PCI_BYTES 0x3d4
#define DSI_BYTES 0x3fa

#define PS2_PCI_SUBSTREAM_ID 0x00
#define PS2_DSI_SUBSTREAM_ID 0x01

/* Remove this */
#define DSI_START_BYTE 1031



#include <pshpack1.h>



/**
 * PCI General Information 
 */
typedef struct {
  uint32_t nv_pck_lbn;
  uint16_t vobu_cat;
  uint16_t zero1;
  user_ops_t vobu_uop_ctl;
  uint32_t vobu_s_ptm;
  uint32_t vobu_e_ptm;
  uint32_t vobu_se_e_ptm;
  dvd_time_t e_eltm;
  char vobu_isrc[32];
}  pci_gi_t;

/**
 * Non Seamless Angle Information
 */
typedef struct {
  uint32_t nsml_agl_dsta[9]; 
}  nsml_agli_t;

/** 
 * Highlight General Information 
 */
typedef struct {
  uint16_t hli_ss; ///< only low 2 bits
  uint32_t hli_s_ptm;
  uint32_t hli_e_ptm;
  uint32_t btn_se_e_ptm;
#ifdef WORDS_BIGENDIAN
  unsigned short zero1 : 2;
  unsigned short btngr_ns : 2;
  unsigned short zero2 : 1;
  unsigned short btngr1_dsp_ty : 3;
  unsigned short zero3 : 1;
  unsigned short btngr2_dsp_ty : 3;
  unsigned short zero4 : 1;
  unsigned short btngr3_dsp_ty : 3;
#else
  unsigned short btngr1_dsp_ty : 3;
  unsigned short zero2 : 1;
  unsigned short btngr_ns : 2;
  unsigned short zero1 : 2;
  unsigned short btngr3_dsp_ty : 3;
  unsigned short zero4 : 1;
  unsigned short btngr2_dsp_ty : 3;
  unsigned short zero3 : 1;
#endif
  uint8_t btn_ofn;
  uint8_t btn_ns;     ///< only low 6 bits
  uint8_t nsl_btn_ns; ///< only low 6 bits
  uint8_t zero5;
  uint8_t fosl_btnn;  ///< only low 6 bits
  uint8_t foac_btnn;  ///< only low 6 bits
}  hl_gi_t;


/** 
 * Button Color Information Table 
 */
typedef struct {
  uint32_t btn_coli[3][2];
}  btn_colit_t;

/** 
 * Button Information
 */
typedef struct {
#ifdef WORDS_BIGENDIAN
  __int64 btn_coln         : 2;
  __int64 x_start          : 10;
  __int64 zero1            : 2;
  __int64 x_end            : 10;
  __int64 auto_action_mode : 2;
  __int64 y_start          : 10;
  __int64 zero2            : 2;
  __int64 y_end            : 10;

  __int64 zero3            : 2;
  __int64 up               : 6;
  __int64 zero4            : 2;
  __int64 down             : 6;
  unsigned short zero5            : 2;
  unsigned short left             : 6;
  unsigned short zero6            : 2;
  unsigned short right            : 6;
#else
  __int64 x_end            : 10;
  __int64 zero1            : 2;
  __int64 x_start          : 10;
  __int64 btn_coln         : 2;
  __int64 y_end            : 10;
  __int64 zero2            : 2;
  __int64 y_start          : 10;
  __int64 auto_action_mode : 2;

  __int64 up               : 6;
  __int64 zero3            : 2;
  __int64 down             : 6;
  __int64 zero4            : 2;
  unsigned short left             : 6;
  unsigned short zero5            : 2;
  unsigned short right            : 6;
  unsigned short zero6            : 2;
#endif
  vm_cmd_t cmd;
}  btni_t;

/**
 * Highlight Information 
 */
typedef struct {
  hl_gi_t     hl_gi;
  btn_colit_t btn_colit;
  btni_t      btnit[36];
}  hli_t;

/**
 * PCI packet
 */
typedef struct {
  pci_gi_t    pci_gi;
  nsml_agli_t nsml_agli;
  hli_t       hli;
  uint8_t     zero1[189];
}  pci_t;




/**
 * DSI General Information 
 */
typedef struct {
  uint32_t nv_pck_scr;
  uint32_t nv_pck_lbn;
  uint32_t vobu_ea;
  uint32_t vobu_1stref_ea;
  uint32_t vobu_2ndref_ea;
  uint32_t vobu_3rdref_ea;
  uint16_t vobu_vob_idn;
  uint8_t  zero1;
  uint8_t  vobu_c_idn;
  dvd_time_t c_eltm;
}  dsi_gi_t;

/**
 * Seamless Playback Information
 */
typedef struct {
  uint16_t category; ///< category of seamless VOBU
  uint32_t ilvu_ea;  ///< end address of interleaved Unit (sectors)
  uint32_t ilvu_sa;  ///< start address of next interleaved unit (sectors)
  uint16_t size;     ///< size of next interleaved unit (sectors)
  uint32_t vob_v_s_s_ptm; ///< video start ptm in vob
  uint32_t vob_v_e_e_ptm; ///< video end ptm in vob
  struct {
    uint32_t stp_ptm1;
    uint32_t stp_ptm2;
    uint32_t gap_len1;
    uint32_t gap_len2;      
  } vob_a[8];
}  sml_pbi_t;

/**
 * Seamless Angle Infromation for one angle
 */
typedef struct {
    uint32_t address; ///< Sector offset to next ILVU, high bit is before/after
    uint16_t size;    ///< Byte size of the ILVU poited to by address.
}  sml_agl_data_t;

/**
 * Seamless Angle Infromation
 */
typedef struct {
  sml_agl_data_t data[9];
}  sml_agli_t;

/**
 * VOBU Search Information 
 */
typedef struct {
  uint32_t next_video; ///< Next vobu that contains video
  uint32_t fwda[19];   ///< Forwards, time
  uint32_t next_vobu;
  uint32_t prev_vobu;
  uint32_t bwda[19];   ///< Backwards, time
  uint32_t prev_video;
}  vobu_sri_t;

#define SRI_END_OF_CELL 0x3fffffff

/**
 * Synchronous Information
 */ 
typedef struct {
  uint16_t a_synca[8];   ///< Sector offset to first audio packet for this VOBU
  uint32_t sp_synca[32]; ///< Sector offset to first subpicture packet
}  synci_t;

/**
 * DSI packet
 */
typedef struct {
  dsi_gi_t   dsi_gi;
  sml_pbi_t  sml_pbi;
  sml_agli_t sml_agli;
  vobu_sri_t vobu_sri;
  synci_t    synci;
  uint8_t    zero1[471];
}  dsi_t;



#include <poppack.h>


#endif // __DVDREAD_NAV_TYPES_H__
