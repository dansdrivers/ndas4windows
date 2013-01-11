/*++ Copyright (C) XIMETA, Inc. All rights reserved. --*/

#if ! (defined(lint) || defined(RC_INVOKED))
#if ( _MSC_VER >= 800 && !defined(_M_I86)) || defined(_PUSHPOP_SUPPORTED)
#pragma warning(disable:4103)
#if !(defined( MIDL_PASS )) || defined( __midl )
#pragma pack(pop)
#else
#pragma pack()
#endif
#elif defined(__GNUC__) 
/* old GNUC(<3.x) does not support #pragma pop/pack */
#else
#pragma pack()
#endif
#endif // ! (defined(lint) || defined(RC_INVOKED))
