#include "ndasscsiproc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "ndasflowcontrol"


ULONG 
Log2 (
	ULONGLONG	Seed
	) 
{
	ULONG	Log2 = 0;

	while (Seed >>= 1) {

		Log2 ++;
	}
	
	return Log2;
}

NTSTATUS
NdasFcInitialize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	)
{
	LONG	idx;

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, ("Called\n") );

	RtlZeroMemory( NdasFcStatistics, sizeof(NDAS_FC_STATISTICS) );

	for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

		NdasFcStatistics->IoSize[idx] =	NDAS_FC_DEFAULT_TROUGHPUT;
		NdasFcStatistics->IoTime[idx] = NANO100_PER_SEC;

		NdasFcStatistics->TotalIoSize += NdasFcStatistics->IoSize[idx];
		NdasFcStatistics->TotalIoTime += NdasFcStatistics->IoTime[idx];
	} 

	DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
				("NdasFcStatistics->TotalIoSize >> (Log2(NdasFcStatistics->TotalIoTime >> Log2(NANO100_PER_SEC))) = %I64d "
				 "NDAS_FC_DEFAULT_TROUGHPUT = %I64d\n",
				 NdasFcStatistics->TotalIoSize >> (Log2(NdasFcStatistics->TotalIoTime >> Log2(NANO100_PER_SEC))),
				 NDAS_FC_DEFAULT_TROUGHPUT) );

	return STATUS_SUCCESS;
}

UINT32
NdasFcChooseTransferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				IoSize
	)
{
	ULONG	idx;
	ULONG	maxIdx;


	if (IoSize < ((NdasFcStatistics->MinimumIdx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

		return IoSize;
	}

	maxIdx = NdasFcStatistics->MinimumIdx;
	
	if (NdasFcStatistics->PreviousIoSize) {

		if (NdasFcStatistics->PreviousIoSize >= IoSize) {

			return (NdasFcStatistics->PreviousIdx+1) << NDAS_FC_LOG_OF_IO_UNIT;
		}

		maxIdx = NdasFcStatistics->PreviousIdx;
	}

	for (idx = maxIdx+1; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

		if (IoSize < ((idx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

			break;
		}

		if ((NdasFcStatistics->IoSize[idx] >> (Log2(NdasFcStatistics->IoTime[idx] >> Log2(NANO100_PER_MSEC)))) >= 
			(NdasFcStatistics->IoSize[maxIdx] >> (Log2(NdasFcStatistics->IoTime[maxIdx] >> Log2(NANO100_PER_MSEC))))) {

			maxIdx = idx;
		}
	}

	NdasFcStatistics->PreviousIoSize = IoSize;
	NdasFcStatistics->PreviousIdx	 = maxIdx;

	return (maxIdx+1) << NDAS_FC_LOG_OF_IO_UNIT;
}

VOID
NdasFcUpdateTrasnferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				UnitIoSize,
	IN UINT32				TotalIoSize,
	IN LARGE_INTEGER		StartTime,
	IN LARGE_INTEGER		EndTime
	)
{
	ULONG			unitIoIdx;
	ULONG			idx;


	if (UnitIoSize < ((NdasFcStatistics->MinimumIdx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

		return;
	}

	NDAS_ASSERT( EndTime.QuadPart >= StartTime.QuadPart );

	unitIoIdx = (UnitIoSize >> NDAS_FC_LOG_OF_IO_UNIT) - 1;

	NdasFcStatistics->TotalTermIoSize += TotalIoSize;
	NdasFcStatistics->TotalTermIoTime += EndTime.QuadPart - StartTime.QuadPart;

	NdasFcStatistics->TotalIoCount ++;

	NdasFcStatistics->TermIoSize[unitIoIdx] += TotalIoSize;
	NdasFcStatistics->TermIoTime[unitIoIdx] += EndTime.QuadPart - StartTime.QuadPart;

	NdasFcStatistics->HitCount[unitIoIdx] ++;

	if (NdasFcStatistics->HitCount[unitIoIdx] % NDAS_FC_IO_TERM == 0) {

		NdasFcStatistics->PreviousIoSize = 0;

		NdasFcStatistics->IoSize[unitIoIdx] += NdasFcStatistics->TermIoSize[unitIoIdx];
		NdasFcStatistics->IoTime[unitIoIdx] += NdasFcStatistics->TermIoTime[unitIoIdx];

		NdasFcStatistics->TermIoSize[unitIoIdx] = 0;
		NdasFcStatistics->TermIoTime[unitIoIdx] = 0;

	} else if (NdasFcStatistics->TermIoTime[unitIoIdx] >= 100 * NANO100_PER_MSEC) {	// for low speed

		if (((NdasFcStatistics->PreviousIdx+1) << NDAS_FC_LOG_OF_IO_UNIT) >= (24 << 10)) {	// up to 24 KByte

			NdasFcStatistics->PreviousIdx -= 2;
		
		} else {

			NdasFcStatistics->PreviousIoSize = 0;
		}

		NdasFcStatistics->IoSize[unitIoIdx] += NdasFcStatistics->TermIoSize[unitIoIdx];
		NdasFcStatistics->IoTime[unitIoIdx] += NdasFcStatistics->TermIoTime[unitIoIdx];

		NdasFcStatistics->TermIoSize[unitIoIdx] = 0;
		NdasFcStatistics->TermIoTime[unitIoIdx] = 0;
	}

	if (NdasFcStatistics->TotalIoCount % NDAS_FC_FRACTION_RATE == 0) {

		for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

			if ((NdasFcStatistics->IoSize[idx] >> (Log2(NdasFcStatistics->IoTime[idx] >> Log2(NANO100_PER_MSEC)))) >= 
				(NdasFcStatistics->TotalTermIoSize >> (Log2(NdasFcStatistics->TotalTermIoTime >> Log2(NANO100_PER_MSEC))))) {

				NdasFcStatistics->IoSize[idx] -= NdasFcStatistics->TotalTermIoSize >> NDAS_FC_LOG_OF_FRACTION_RATE;
				NdasFcStatistics->IoTime[idx] -= NdasFcStatistics->TotalTermIoTime >> NDAS_FC_LOG_OF_FRACTION_RATE;
			
			} else {

				NdasFcStatistics->IoSize[idx] += NdasFcStatistics->TotalTermIoSize >> NDAS_FC_LOG_OF_FRACTION_RATE;
				NdasFcStatistics->IoTime[idx] += NdasFcStatistics->TotalTermIoTime >> NDAS_FC_LOG_OF_FRACTION_RATE;
			}
		}

		NdasFcStatistics->TotalIoSize += NdasFcStatistics->TotalTermIoSize;
		NdasFcStatistics->TotalIoTime += NdasFcStatistics->TotalTermIoTime;

		NdasFcStatistics->TotalTermIoSize = 0;
		NdasFcStatistics->TotalTermIoTime = 0;

		NdasFcStatistics->PreviousIoSize = 0;

		if (NdasFcTransferRate(NdasFcStatistics) <= NDAS_FC_DEFAULT_TROUGHPUT) {

			NdasFcStatistics->MinimumIdx = 0;
		
		} else {

			NdasFcStatistics->MinimumIdx = (8<<10) >> NDAS_FC_LOG_OF_IO_UNIT;
		}
	}

	if (NdasFcStatistics->TotalIoCount % (NDAS_FC_FRACTION_RATE << 3) == 0 || 
		NdasFcStatistics->TotalIoCount % NDAS_FC_FRACTION_RATE == 0 && 
		NdasFcStatistics->TermIoTime[unitIoIdx] >= 100 * NANO100_PER_MSEC) {

		DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
					("TotalIoSize = %I64d, TotalIoTime = %I64d, Throughput = %I64d, NDAS_FC_DEFAULT_TROUGHPUT=%I64d\n",
					 NdasFcStatistics->TotalIoSize, NdasFcStatistics->TotalIoTime,
					 NdasFcStatistics->TotalIoSize >> (Log2(NdasFcStatistics->TotalIoTime >> Log2(NANO100_PER_MSEC))),
					 NDAS_FC_DEFAULT_TROUGHPUT) );

		for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx+=4) {

			DebugTrace( NDASSCSI_DBG_LURN_NDASR_INFO, 
						("%3i, %6i, %7I64d, %6u " 
						 "%3i, %6i, %7I64d, %6u " 
						 "%3i, %6i, %7I64d, %6u " 
						 "%3i, %6i, %7I64d, %6u\n", 
						idx,   (idx+1)<<NDAS_FC_LOG_OF_IO_UNIT, NdasFcStatistics->IoSize[idx] >> (Log2(NdasFcStatistics->IoTime[idx] >> Log2(NANO100_PER_MSEC))), NdasFcStatistics->HitCount[idx],
						idx+1, (idx+2)<<NDAS_FC_LOG_OF_IO_UNIT, NdasFcStatistics->IoSize[idx+1] >> (Log2(NdasFcStatistics->IoTime[idx+1] >> Log2(NANO100_PER_MSEC))), NdasFcStatistics->HitCount[idx+1],
						idx+2, (idx+3)<<NDAS_FC_LOG_OF_IO_UNIT, NdasFcStatistics->IoSize[idx+2] >> (Log2(NdasFcStatistics->IoTime[idx+2] >> Log2(NANO100_PER_MSEC))), NdasFcStatistics->HitCount[idx+2],
						idx+3, (idx+4)<<NDAS_FC_LOG_OF_IO_UNIT, NdasFcStatistics->IoSize[idx+3] >> (Log2(NdasFcStatistics->IoTime[idx+3] >> Log2(NANO100_PER_MSEC))), NdasFcStatistics->HitCount[idx+3]) );
		}
	}
}

// bytes per sec

LONGLONG
NdasFcTransferRate (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	)
{
	return NdasFcStatistics->TotalIoSize >> (Log2(NdasFcStatistics->TotalIoTime >> Log2(NANO100_PER_SEC)));
}
