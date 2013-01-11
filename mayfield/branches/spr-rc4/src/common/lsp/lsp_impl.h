#ifndef _LSP_IMPL_H_
#define _LSP_IMPL_H_

#include <lsp_type.h>

#define HTONS(Data) ( (((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))
#define NTOHS(Data) (short)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))

#define HTONL(Data) \
	( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) <<  8) | \
	(((Data)&0x00FF0000) >>  8) | (((Data)&0xFF000000) >> 24) )

#define NTOHL(Data) \
	( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) | \
	(((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define HTONLL(Data) \
	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) | \
	(((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) <<  8) | \
	(((Data)&0x000000FF00000000) >>  8) | (((Data)&0x0000FF0000000000) >> 24) | \
	(((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56) )

#define NTOHLL(Data) \
	( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) | \
	(((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) <<  8) | \
	(((Data)&0x000000FF00000000) >>  8) | (((Data)&0x0000FF0000000000) >> 24) | \
	(((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56) )

#ifdef __CONVERT_ORDER_WITH_MACRO__

// caution : if you use these macros, you must check variable type yourself
// and you must take care of using them. (ex : lsp_htons(++lVar);)

#define lsp_htons(x) HTONS(x)
#define lsp_ntohs(x) NTOHS(x)
#define lsp_htonl(x) HTONL(x)
#define lsp_ntohl(x) NTOHL(x)
#define lsp_htonll(x) HTONLL(x)
#define lsp_ntohll(x) NTOHLL(x)

#else

__inline lsp_uint16 lsp_htons(lsp_uint16 x)
{
	return HTONS(x);
}

__inline lsp_uint16 lsp_ntohs(lsp_uint16 x)
{
	return NTOHS(x);
}

__inline lsp_uint32 lsp_htonl(lsp_uint32 x)
{
	return HTONL(x);
}

__inline lsp_uint32 lsp_ntohl(lsp_uint32 x)
{
	return NTOHL(x);
}

__inline lsp_uint64 lsp_htonll(lsp_uint64 x)
{
	return HTONLL(x);
}

__inline lsp_uint64 lsp_ntohll(lsp_uint64 x)
{
	return NTOHLL(x);
}

__inline void lsp_ntohx(lsp_uint8 *dst, lsp_uint8 *src, lsp_uint8 length)
{
	lsp_uint8 i;
	for(i = 0; i < length; i++)
		dst[i] = src[length - i - 1];
}

__inline void lsp_htonx(lsp_uint8 *dst, lsp_uint8 *src, lsp_uint8 length)
{
	lsp_uint8 i;
	for(i = 0; i < length; i++)
		dst[i] = src[length - i - 1];
}
#endif



#define lsp_memset(dst,val,count) memset(dst,val,count)
#define lsp_memcpy(dst,src,count) memcpy(dst,src,count)

#endif /* _LSP_IMPL_H_ */
