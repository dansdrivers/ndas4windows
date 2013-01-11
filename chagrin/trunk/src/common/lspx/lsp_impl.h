#ifndef _LSP_IMPL_H_
#define _LSP_IMPL_H_

#include <lsp_type.h>

#if DBG

void
__stdcall
RtlAssert(
    void* FailedAssertion,
    void* FileName,
    unsigned long LineNumber,
    char* Message);

#define ASSERT( exp ) \
    if (!(exp)) \
        RtlAssert( #exp, __FILE__, __LINE__, NULL )

#define ASSERTMSG( msg, exp ) \
    if (!(exp)) \
        RtlAssert( #exp, __FILE__, __LINE__, msg )

#else

#define ASSERT( exp )
#define ASSERTMSG( msg, exp )

#endif /* DBG */

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

__forceinline unsigned long lsp_byteswap_ulong(unsigned long i)
{
#if defined(_X86_) && !defined(_NO_INLINE_ASM_)
	__asm 
	{
		mov eax, i
		bswap eax
	}
#else
	unsigned int j;
	j =  (i << 24);
	j += (i <<  8) & 0x00FF0000;
	j += (i >>  8) & 0x0000FF00;
	j += (i >> 24);
	return j;
#endif
}

#define lsp_memset(dst,val,count) memset(dst,val,count)
#define lsp_memcpy(dst,src,count) memcpy(dst,src,count)

__forceinline unsigned short lsp_byteswap_ushort(unsigned short i)
{
#if defined(_X86_) && !defined(LSP_NO_INLINE_ASM)
	__asm 
	{
		mov ax, i
		xchg al, ah
	}
#else
	unsigned short j;
	j =  (i << 8) ;
	j += (i >> 8) ;
	return j;
#endif
}

__forceinline unsigned __int64 lsp_byteswap_uint64(unsigned __int64 i)
{
	unsigned __int64 j;
	j =  (i << 56);
	j += (i << 40)&0x00FF000000000000;
	j += (i << 24)&0x0000FF0000000000;
	j += (i <<  8)&0x000000FF00000000;
	j += (i >>  8)&0x00000000FF000000;
	j += (i >> 24)&0x0000000000FF0000;
	j += (i >> 40)&0x000000000000FF00;
	j += (i >> 56);
	return j;

}

#ifdef LSP_BIG_ENDIAN

#define lsp_htons(x)
#define lsp_ntohs(x)
#define lsp_htonl(x)
#define lsp_ntohl(x)
#define lsp_htonll(x)
#define lsp_ntohll(x)
#define lsp_ntohx(dst, src, length)
#define lsp_htonx(dst, src, length)

#else

__forceinline lsp_uint16 lsp_htons(lsp_uint16 x)
{
	return lsp_byteswap_ushort(x);
}

__forceinline lsp_uint16 lsp_ntohs(lsp_uint16 x)
{
	return lsp_byteswap_ushort(x);
}

__forceinline lsp_uint32 lsp_htonl(lsp_uint32 x)
{
	return lsp_byteswap_ulong(x);
}

__forceinline lsp_uint32 lsp_ntohl(lsp_uint32 x)
{
	return lsp_byteswap_ulong(x);
}

__forceinline lsp_uint64 lsp_htonll(lsp_uint64 x)
{
	return lsp_byteswap_uint64(x);
}

__forceinline lsp_uint64 lsp_ntohll(lsp_uint64 x)
{
	return lsp_byteswap_uint64(x);
}

__forceinline void lsp_ntohx(lsp_uint8 *dst, const lsp_uint8 *src, lsp_uint8 length)
{
	lsp_uint8 i;
	for(i = 0; i < length; i++)
	{
		dst[i] = src[length - i - 1];
	}
}

__forceinline void lsp_htonx(lsp_uint8 *dst, const lsp_uint8 *src, lsp_uint8 length)
{
	lsp_uint8 i;
	for(i = 0; i < length; i++)
	{
		dst[i] = src[length - i - 1];
	}
}

#endif

#endif /* _LSP_IMPL_H_ */
