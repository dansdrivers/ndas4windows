#ifndef _NDAS_FLOW_CONTROL_H_
#define _NDAS_FLOW_CONTROL_H_


#define NDAS_FC_LOG_OF_FRACTION_RATE	(6)
#define NDAS_FC_FRACTION_RATE			(1 << NDAS_FC_LOG_OF_FRACTION_RATE)

#define NDAS_FC_LOG_OF_IO_UNIT			(12)
#define NDAS_FC_IO_UNIT					(1 << NDAS_FC_LOG_OF_IO_UNIT)

#define NDAS_FC_MAX_IO_SIZE				(64 << 10)

#define NDAS_FC_DEFAULT_TROUGHPUT		(32LL << 20 >> 3) /* 32 Mbps */

#define NDAS_FC_IO_TERM					(32)

C_ASSERT( (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT) % 8 == 0 );

typedef struct _NDAS_FC_STATISTICS {

	ULONG		TotalIoCount;

	ULONGLONG	IoSize[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONGLONG	IoTime[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONG		HitCount[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];

	ULONGLONG	TotalIoSize;
	ULONGLONG	TotalIoTime;

	ULONGLONG	TermIoSize[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONGLONG	TermIoTime[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONG		TermHitCount[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];

	ULONGLONG	TotalTermIoSize;
	ULONGLONG	TotalTermIoTime;

	ULONG		MinimumIdx;

	UINT32		PreviousIoSize;
	ULONG		PreviousIdx;
	
} NDAS_FC_STATISTICS, *PNDAS_FC_STATISTICS;


NTSTATUS
NdasFcInitialize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	);

UINT32
NdasFcChooseTransferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				IoSize
	);

VOID
NdasFcUpdateTrasnferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				UnitIoSize,
	IN UINT32				TotalIoSize,
	IN LARGE_INTEGER		StartTime,
	IN LARGE_INTEGER		EndTime
	);

// bytes per sec

LONGLONG
NdasFcTransferRate (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	);

#endif
