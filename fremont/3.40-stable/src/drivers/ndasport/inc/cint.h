#pragma once

#if _WIN32_WINNT <= 0x0501
//
// Supplementary Interlocked Routines
//

#if defined(_M_IX86) && !defined(RC_INVOKED) && !defined(MIDL_PASS)

#ifdef __cplusplus
extern "C" {
#endif

#if (_MSC_FULL_VER >= 14000101)

//
// Define bit test intrinsics.
//

#define BitTest _bittest
#define BitTestAndComplement _bittestandcomplement
#define BitTestAndSet _bittestandset
#define BitTestAndReset _bittestandreset
#define InterlockedBitTestAndSet _interlockedbittestandset
#define InterlockedBitTestAndReset _interlockedbittestandreset

BOOLEAN
_bittest (
    IN LONG const *Base,
    IN LONG Offset
    );

BOOLEAN
_bittestandcomplement (
    IN LONG *Base,
    IN LONG Offset
    );

BOOLEAN
_bittestandset (
    IN LONG *Base,
    IN LONG Offset
    );

BOOLEAN
_bittestandreset (
    IN LONG *Base,
    IN LONG Offset
    );

BOOLEAN
_interlockedbittestandset (
    IN LONG *Base,
    IN LONG Offset
    );

BOOLEAN
_interlockedbittestandreset (
    IN LONG *Base,
    IN LONG Offset
    );

#pragma intrinsic(_bittest)
#pragma intrinsic(_bittestandcomplement)
#pragma intrinsic(_bittestandset)
#pragma intrinsic(_bittestandreset)
#pragma intrinsic(_interlockedbittestandset)
#pragma intrinsic(_interlockedbittestandreset)

//
// Define bit scan intrinsics.
//

#define BitScanForward _BitScanForward
#define BitScanReverse _BitScanReverse

BOOLEAN
_BitScanForward (
    OUT ULONG *Index,
    IN ULONG Mask
    );

BOOLEAN
_BitScanReverse (
    OUT ULONG *Index,
    IN ULONG Mask
    );

#pragma intrinsic(_BitScanForward)
#pragma intrinsic(_BitScanReverse)

#else

#pragma warning(push)
#pragma warning(disable:4035 4793)

FORCEINLINE
BOOLEAN
InterlockedBitTestAndSet (
    IN LONG *Base,
    IN LONG Bit
    )
{
    __asm {
           mov eax, Bit
           mov ecx, Base
           lock bts [ecx], eax
           setc al
    };
}

FORCEINLINE
BOOLEAN
InterlockedBitTestAndReset (
    IN LONG *Base,
    IN LONG Bit
    )
{
    __asm {
           mov eax, Bit
           mov ecx, Base
           lock btr [ecx], eax
           setc al
    };
}
#pragma warning(pop)

#endif	/* _MSC_FULL_VER >= 14000101 */

#if !defined(_M_CEE_PURE)
#pragma warning(push)
#pragma warning(disable:4035 4793)

//FORCEINLINE
//BOOLEAN
//InterlockedBitTestAndComplement (
//    IN LONG *Base,
//    IN LONG Bit
//    )
//{
//    __asm {
//           mov eax, Bit
//           mov ecx, Base
//           lock btc [ecx], eax
//           setc al
//    };
//}
#pragma warning(pop)
#endif	/* _M_CEE_PURE */

//
// [pfx_parse]
// guard against __readfsbyte parsing error
//
#if (_MSC_FULL_VER >= 13012035) || defined(_PREFIX_) || defined(_PREFAST_)

//
// Define FS referencing intrinsics
//

UCHAR
__readfsbyte (
    IN ULONG Offset
    );
 
USHORT
__readfsword (
    IN ULONG Offset
    );
 
ULONG
__readfsdword (
    IN ULONG Offset
    );
 
VOID
__writefsbyte (
    IN ULONG Offset,
    IN UCHAR Data
    );
 
VOID
__writefsword (
    IN ULONG Offset,
    IN USHORT Data
    );
 
VOID
__writefsdword (
    IN ULONG Offset,
    IN ULONG Data
    );

#pragma intrinsic(__readfsbyte)
#pragma intrinsic(__readfsword)
#pragma intrinsic(__readfsdword)
#pragma intrinsic(__writefsbyte)
#pragma intrinsic(__writefsword)
#pragma intrinsic(__writefsdword)

#endif	/* _MSC_FULL_VER >= 13012035 */

#ifdef __cplusplus
}
#endif

#endif  /* !defined(MIDL_PASS) || defined(_M_IX86) */

#endif /* TARGETING_Win2K */

#ifndef BitScanForward
#define BitScanForward _BitScanForward

BOOLEAN
_BitScanForward (
	OUT ULONG *Index,
	IN ULONG Mask
	);

#pragma intrinsic(_BitScanForward)
#endif

#ifndef BitScanReverse
#define BitScanReverse _BitScanReverse
BOOLEAN
_BitScanReverse (
	 OUT ULONG *Index,
	 IN ULONG Mask
	 );

#pragma intrinsic(_BitScanReverse)
#endif



#ifndef InterlockedOr
#define InterlockedOr _InterlockedOr
#define InterlockedOrAffinity InterlockedOr

LONG
_InterlockedOr (
    IN OUT LONG volatile *Target,
    IN LONG Set
    );

#pragma intrinsic(_InterlockedOr)
#endif
