#ifndef LSP_HOST_H_INCLUDED
#define LSP_HOST_H_INCLUDED


#include "lsp_type.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define LSP_INLINE __forceinline
#elif defined(__GNUC__)
#define LSP_INLINE static inline
#else
#define LSP_INLINE static inline __attribute__((always_inline))
#endif

#if defined(_MSC_VER)
#if defined(_DEBUG) || (defined(DBG) && DBG)

__declspec(dllimport)
void
__stdcall
RtlAssert(
    void* FailedAssertion,
    void* FileName,
    unsigned long LineNumber,
    char* Message);

#define LSP_ASSERT( exp ) \
    if (!(exp)) \
        RtlAssert( #exp, __FILE__, __LINE__, NULL )

#define LSP_ASSERTMSG( msg, exp ) \
    if (!(exp)) \
        RtlAssert( #exp, __FILE__, __LINE__, msg )

#endif /* _DEBUG */
#endif /* _MSC_VER */

#ifndef LSP_ASSERT
#define LSP_ASSERT( exp )
#endif

#ifndef LSP_ASSERTMSG
#define LSP_ASSERTMSG( msg, exp )
#endif

#define FALSE   0
#define TRUE    1

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#define NULL64  0
#else
#define NULL    ((void *)0)
#define NULL64  ((void * POINTER_64)0)
#endif
#endif /* NULL */

#define lsp_memset(dst,val,count) memset(dst,val,count)
#define lsp_memcpy(dst,src,count) memcpy(dst,src,count)
#define lsp_memcmp(buf1,buf2,count) memcmp(buf1,buf2,count)

#if defined (_MSC_VER)
#if (defined(_M_IX86) && (_MSC_FULL_VER > 13009037)) || ((defined(_M_AMD64) || defined(_M_IA64)) && (_MSC_FULL_VER > 13009175))
unsigned short __cdecl _byteswap_ushort(unsigned short);
unsigned long  __cdecl _byteswap_ulong (unsigned long);
unsigned __int64 __cdecl _byteswap_uint64(unsigned __int64);
#endif
#pragma intrinsic(_byteswap_ushort)
#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_uint64)
#define lsp_byteswap_ushort(_x) _byteswap_ushort((unsigned short)(_x))
#define lsp_byteswap_ulong(_x) _byteswap_ulong((_x))
#define lsp_byteswap_uint64(_x) _byteswap_uint64((_x))
#endif /* _MSC_VER */

#ifndef lsp_byteswap_ushort
#define lsp_byteswap_ushort lsp_imp_byteswap_ushort
#endif

#ifndef lsp_byteswap_ulong
#define lsp_byteswap_ulong lsp_imp_byteswap_uint32
#endif

#ifndef lsp_byteswap_uint64
#define lsp_byteswap_uint64 lsp_imp_byteswap_uint64
#endif

#ifndef lsp_byteswap_x
#define lsp_byteswap_x lsp_imp_byteswap_x
#endif

#ifdef __BIG_ENDIAN__

#define lsp_htons(x) (x)
#define lsp_ntohs(x) (x)
#define lsp_htonl(x) (x)
#define lsp_ntohl(x) (x)
#define lsp_htonll(x) (x)
#define lsp_ntohll(x) (x)
#define lsp_ntohx(dst, src, length) lsp_memcpy(dst, src, length)
#define lsp_htonx(dst, src, length) lsp_memcpy(dst, src, length)

/* big endian to host */
#define lsp_betohs(x) (x)
#define lsp_betohl(x) (x)
#define lsp_betohll(x) (x)

/* little endian to host */
#define lsp_letohs(x) lsp_byteswap_ushort(x)
#define lsp_letohl(x) lsp_byteswap_ulong(x)
#define lsp_letohll(x) lsp_byteswap_uint64(x)

/* host to big endian */
#define lsp_htobes(x) (x)
#define lsp_htobel(x) (x)
#define lsp_htobell(x) (x)

/* host to little endian */
#define lsp_htoles(x) lsp_byteswap_ushort(x)
#define lsp_htolel(x) lsp_byteswap_ulong(x)
#define lsp_htolell(x) lsp_byteswap_uint64(x)

#else /* __BIG_ENDIAN__ */

#define lsp_htons  lsp_byteswap_ushort
#define lsp_htonl  lsp_byteswap_ulong
#define lsp_htonll lsp_byteswap_uint64
#define lsp_ntohs  lsp_byteswap_ushort
#define lsp_ntohl  lsp_byteswap_ulong
#define lsp_ntohll lsp_byteswap_uint64
#define lsp_ntohx  lsp_byteswap_x
#define lsp_htonx  lsp_byteswap_x

/* big endian to host */
#define lsp_betohs(x) lsp_byteswap_ushort(x)
#define lsp_betohl(x) lsp_byteswap_ulong(x)
#define lsp_betohll(x) lsp_byteswap_uint64(x)

/* little endian to host */
#define lsp_letohs(x) (x)
#define lsp_letohl(x) (x)
#define lsp_letohll(x) (x)

/* host to big endian */
#define lsp_htobes(x) lsp_byteswap_ushort(x)
#define lsp_htobel(x) lsp_byteswap_ulong(x)
#define lsp_htobell(x) lsp_byteswap_uint64(x)

/* host to little endian */
#define lsp_htoles(x) (x)
#define lsp_htolel(x) (x)
#define lsp_htolell(x) (x)

#endif /* __BIG_ENDIAN__ */

LSP_INLINE unsigned short lsp_imp_byteswap_ushort(unsigned short x);
LSP_INLINE lsp_uint32_t lsp_imp_byteswap_uint32(lsp_uint32_t x);
LSP_INLINE lsp_uint64_t lsp_imp_byteswap_uint64(lsp_uint64_t x);
LSP_INLINE void lsp_imp_byteswap_x(lsp_uint8_t *dst, const lsp_uint8_t *src, lsp_uint8_t length);

LSP_INLINE unsigned short lsp_imp_byteswap_ushort(unsigned short x)
{
#if defined(_X86_) && !defined(LSP_NO_INLINE_ASM)
	__asm 
	{
		mov ax, x
		xchg al, ah
	}
#else
	return (x << 8) | (x >> 8);
#endif
}

LSP_INLINE lsp_uint32_t lsp_imp_byteswap_uint32(lsp_uint32_t x)
{
#if defined(_X86_) && !defined(_NO_INLINE_ASM_)
	__asm 
	{
		mov eax, x
		bswap eax
	}
#else
	return ((lsp_uint32_t) (lsp_byteswap_ushort(x & 0xFFFF) << 16)) |
		((lsp_uint32_t) (lsp_byteswap_ushort(x >> 16)));
#endif
}

LSP_INLINE lsp_uint64_t lsp_imp_byteswap_uint64(lsp_uint64_t x)
{
	return (((lsp_uint64_t) lsp_byteswap_ulong((lsp_uint32_t)(x & 0xFFFFFFFFULL))) << 32) |
		((lsp_uint64_t) lsp_byteswap_ulong((lsp_uint32_t)(x >> 32)));
}

LSP_INLINE void lsp_imp_byteswap_x(lsp_uint8_t *dst, const lsp_uint8_t *src, lsp_uint8_t length)
{
	lsp_uint8_t i;
	for (i = 0; i < length; i++)
	{
		dst[i] = src[length - i - 1];
	}
}

#if ! defined(lint)
#define LSP_UNREFERENCED_PARAMETER(P)          do { (P) = (P); } while(0)
#define LSP_DBG_UNREFERENCED_PARAMETER(P)      do { (P) = (P); } while(0)
#define LSP_DBG_UNREFERENCED_LOCAL_VARIABLE(V) do { (V) = (V); } while(0)

#else // lint

// Note: lint -e530 says don't complain about uninitialized variables for
// this varible.  Error 527 has to do with unreachable code.
// -restore restores checking to the -save state

#define LSP_UNREFERENCED_PARAMETER(P)          \
    /*lint -save -e527 -e530 */ \
    { \
        (P) = (P); \
    } \
    /*lint -restore */
#define LSP_DBG_UNREFERENCED_PARAMETER(P)      \
    /*lint -save -e527 -e530 */ \
    { \
        (P) = (P); \
    } \
    /*lint -restore */
#define LSP_DBG_UNREFERENCED_LOCAL_VARIABLE(V) \
    /*lint -save -e527 -e530 */ \
    { \
        (V) = (V); \
    } \
    /*lint -restore */

#endif // lint

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* LSP_HOST_H_INCLUDED */
