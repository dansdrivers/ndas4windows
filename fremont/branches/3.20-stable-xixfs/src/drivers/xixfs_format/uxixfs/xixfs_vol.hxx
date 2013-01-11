#ifndef XIXFSVOL
#define XIXFSVOL

#include "volume.hxx"
#include "drive.hxx"
#include "xixfs_sa.hxx"

//
//      Forward references
//

DECLARE_CLASS(	DP_DRIVE	);
DECLARE_CLASS(	XIXFS_VOL	);
DECLARE_CLASS(	MESSAGE		);





class XIXFS_VOL: public VOL_LIODPDRV {

        public:

        DECLARE_CONSTRUCTOR( XIXFS_VOL );

        VIRTUAL
        ~XIXFS_VOL(
            );

        NONVIRTUAL
        FORMAT_ERROR_CODE
        Initialize(
            IN      PCWSTRING   NtDriveName,
            IN OUT  PMESSAGE    Message         DEFAULT NULL
            );


        NONVIRTUAL
        PVOL_LIODPDRV
        QueryDupVolume(
            IN      PCWSTRING   NtDriveName,
            IN OUT  PMESSAGE    Message         DEFAULT NULL,
            IN      BOOLEAN     ExclusiveWrite  DEFAULT FALSE,
            IN      BOOLEAN     FormatMedia     DEFAULT FALSE,
            IN      MEDIA_TYPE  MediaType       DEFAULT Unknown
            ) CONST;

 

    private:

        NONVIRTUAL
        VOID
        Construct (
                );

        NONVIRTUAL
        VOID
        Destroy(
            );

        XIXFS_SA  _xixfs_sa;

};


#endif //#ifndef XIXFSVOL