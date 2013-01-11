#if !defined ( _UXIXFS_INCLUDED_ )

#define _UXIXFS_INCLUDED_


// Set up the UFAT_EXPORT macro for exporting from UFAT (if the
// source file is a member of UFAT) or importing from UFAT (if
// the source file is a client of UFAT).
//
#if defined ( _AUTOCHECK_ )
#define UXIXFS_EXPORT
#elif defined ( _UXIXFS_MEMBER_ )
#define UXIXFS_EXPORT    __declspec(dllexport)
#else
#define UXIXFS_EXPORT    __declspec(dllimport)
#endif


#endif // _UXIXFS_INCLUDED_

