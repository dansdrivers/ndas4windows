#ifndef _LSP_SPEC_STRING_H_
#define _LSP_SPEC_STRING_H_

/* simple spec strings */

#if defined(_MSC_VER)
#include <specstrings.h>
#else

#ifndef __in
#define __in
#endif

#ifndef __out
#define __out
#endif

#ifndef __inout
#define __inout
#endif

#ifndef __in_opt
#define __in_opt
#endif

#ifndef __out_opt
#define __out_opt
#endif

#ifndef __out_ecount
#define __out_ecount(x)
#endif

#ifndef __out_bcount
#define __out_bcount(x)
#endif

#ifndef __in_ecount
#define __in_ecount(x)
#endif

#ifndef __in_bcount
#define __in_bcount(x)
#endif

#ifndef __inout_ecount
#define __inout_ecount(x)
#endif

#ifndef __inout_bcount
#define __inout_bcount(x)
#endif

#ifndef __reserved
#define __reserved
#endif

#endif

#endif /* _LSP_SPEC_STRING_H_ */
