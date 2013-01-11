//#include "..\Inc\LanScsi.h"
//#include "..\Inc\BinaryParameters.h"
//#include "..\Inc\Hash.h"
//#include "..\Inc\hdreg.h"
//#include "..\Inc\socketlpx.h"

#define	NR_MAX_TARGET			2
#define	MAX_DATA_BUFFER_SIZE	64 * 1024
#define BLOCK_SIZE				512
#define	MAX_TRANSFER_SIZE		(64 * 1024)
#define MAX_TRANSFER_BLOCKS		 MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC			(LONGLONG)(1000)
#define TIME_OUT	(SEC * 30)					// 5 min.

typedef	struct _TARGET_DATA {
	BOOL	bPresent;
	_int8	NRRWHost;
	_int8	NRROHost;
	_int64	TargetData;
	
	// IDE Info.
	BOOL			bLBA;
	BOOL			bLBA48;
	unsigned _int64	SectorCount;
} TARGET_DATA, *PTARGET_DATA;

// Global Variable.
extern _int32			HPID;
extern _int16			RPID;
extern _int32			iTag;
extern int				NRTarget;
extern unsigned		CHAP_C;
extern unsigned		requestBlocks;

extern TARGET_DATA		PerTarget[NR_MAX_TARGET];

extern unsigned _int16	HeaderEncryptAlgo;
extern unsigned _int16	DataEncryptAlgo;
extern int				iSessionPhase;
extern unsigned _int64	iPassword;

int 
GetInterfaceList(
				 LPSOCKET_ADDRESS_LIST	socketAddressList,
				 DWORD					socketAddressListLength
				 );
BOOL
MakeConnection(
			   IN	PLPX_ADDRESS		pAddress,
			   OUT	SOCKET				*pSocketData
			   );
