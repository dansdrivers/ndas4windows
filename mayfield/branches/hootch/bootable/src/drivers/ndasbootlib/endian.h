#ifndef __NDASBOOT_ENDIAN_H__
#define __NDASBOOT_ENDIAN_H__

//
//	Endian detection
//

#if defined(_M_AMD64) || defined(_M_IA64) || defined(_M_IX86)
#define _NDASBOOT_LITENDIAN_CPU_
#elif defined(_M_ALPHA) || defined(_M_MPPC)
#define _NDASBOOT_BIGENDIAN_CPU_
#else
#error "Not supported machine platform."
#endif

#ifdef _NDASBOOT_LITENDIAN_CPU_

#define le32_to_cpu(val) ((int)(val))
#define le16_to_cpu(val) ((unsigned short)(val))
#define cpu_to_le32(val) ((int)(val))
#define cpu_to_le16(val) ((unsigned short)(val))

#else

#error "Not implemented."

#endif

#endif