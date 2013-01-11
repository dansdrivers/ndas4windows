#ifndef _BASE_TYPE_STANDARD_DEF_EX_H
#define	_BASE_TYPE_STANDARD_DEF_EX_H


//////////////////////////////////////////////////////////////////////////
//
//	Basic types
//
#if defined(_X86_) || defined(_AMD64_) || defined(_IA64_)
typedef short int			INT16, *PINT16;
typedef unsigned short int	UINT16, *PUINT16;
typedef unsigned char		BYTE, *PBYTE;
#else
#error "INT16 and UINT16 is only defined for X86"
#endif



#endif