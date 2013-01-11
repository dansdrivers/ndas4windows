#include "LfsProc.h"


#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "ndasflowcontrol"


NTSTATUS
NdasFcInitialize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	)
{
	LONG	idx;

	SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, ("Called\n") );

	RtlZeroMemory( NdasFcStatistics, sizeof(NDAS_FC_STATISTICS) );

	for (idx = 0; idx < NDAS_FC_MAX_SEND_SIZE/NDAS_FC_SEND_BLOCK_SIZE; idx++) {

		NdasFcStatistics->Throughput[idx].QuadPart = (NDAS_FC_DEFAULT_TROUGHPUT << NDAS_FC_LOG_OF_AMPLIFIER);
	} 

	NdasFcStatistics->TotalThroughput.QuadPart = NDAS_FC_DEFAULT_TROUGHPUT << NDAS_FC_LOG_OF_AMPLIFIER;
	NdasFcStatistics->PreviousTotalThroughput.QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;
	NdasFcStatistics->MaxTotalThroughput.QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;
	NdasFcStatistics->MinimumRoundTripTime.QuadPart = 
		(NANO100_PER_SEC << NDAS_FC_LOG_OF_AMPLIFIER) / (NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER);

	NDAS_ASSERT( NdasFcStatistics->TotalThroughput.QuadPart > 0 );

	return STATUS_SUCCESS;
}

UINT32
NdasFcChooseSendSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				MaxSize
	)
{
	ULONG	idx;
	UINT32	tempSize;
	ULONG	maxIdx;

	if (MaxSize <= NDAS_FC_SEND_BLOCK_SIZE) {

		return MaxSize;
	}

	if (NdasFcStatistics->TotalThroughput.QuadPart > (NDAS_FC_DEFAULT_TROUGHPUT << NDAS_FC_LOG_OF_AMPLIFIER)) {
		
		return (MaxSize < NDAS_FC_MAX_SEND_SIZE)?MaxSize:NDAS_FC_MAX_SEND_SIZE;
	}

	tempSize = MaxSize + NDAS_FC_SEND_BLOCK_SIZE - 1;
	maxIdx = 0;

	idx = 0;

	while (idx < ((tempSize < NDAS_FC_MAX_SEND_SIZE) ? tempSize : NDAS_FC_MAX_SEND_SIZE)/NDAS_FC_SEND_BLOCK_SIZE) {
			
		if (NdasFcStatistics->Throughput[maxIdx].QuadPart <= NdasFcStatistics->Throughput[idx].QuadPart) {

			maxIdx = idx;
		}

		if (NdasFcStatistics->TotalThroughput.QuadPart >= (NDAS_FC_DEFAULT_TROUGHPUT << NDAS_FC_LOG_OF_AMPLIFIER)) {

			idx++;
				
		} else {
					
			idx = ((idx+1) << 1) - 1;
		}
	}

	if ((NdasFcStatistics->SentCount) % (NDAS_FC_FRACTION_RATE << 2) == 0) {

		SPY_LOG_PRINT( LFS_DEBUG_LFS_INFO, 
					("MaxSize = %d, maxIdx = %i, size = %d, statistics = %I64d Bytes/Sec TotalThroughput = %I64d, "
					  "roundTripTime = %I64d\n", 
					 MaxSize, maxIdx, (maxIdx+1)*NDAS_FC_SEND_BLOCK_SIZE, 
					 NdasFcStatistics->Throughput[maxIdx].QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER,
					 NdasFcStatistics->TotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER,
					 NdasFcStatistics->MinimumRoundTripTime.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER) );

		for (idx = 0; idx < NDAS_FC_MAX_SEND_SIZE/NDAS_FC_SEND_BLOCK_SIZE; idx+=4) {

			SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
						("%3i, %6i, %8I64d, %8u " 
						 "%3i, %6i, %8I64d, %8u " 
						 "%3i, %6i, %8I64d, %8u " 
						 "%3i, %6i, %8I64d, %8u\n", 
						idx, (idx+1)*NDAS_FC_SEND_BLOCK_SIZE, NdasFcStatistics->Throughput[idx].QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER, NdasFcStatistics->HitCount[idx],
						idx+1, (idx+2)*NDAS_FC_SEND_BLOCK_SIZE, NdasFcStatistics->Throughput[idx+1].QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER, NdasFcStatistics->HitCount[idx+1],
						idx+2, (idx+3)*NDAS_FC_SEND_BLOCK_SIZE, NdasFcStatistics->Throughput[idx+2].QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER, NdasFcStatistics->HitCount[idx+2],
						idx+3, (idx+4)*NDAS_FC_SEND_BLOCK_SIZE, NdasFcStatistics->Throughput[idx+3].QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER, NdasFcStatistics->HitCount[idx+3]) );
		}
	}

	NdasFcStatistics->HitCount[maxIdx]++;

	if ((maxIdx+1) * NDAS_FC_SEND_BLOCK_SIZE < MaxSize) {

		return (maxIdx+1) * NDAS_FC_SEND_BLOCK_SIZE;
	
	} else {

		return MaxSize;
	}
}

C_ASSERT( NDAS_FC_MAX_SEND_SIZE/NDAS_FC_SEND_BLOCK_SIZE % 8 == 0 );

VOID
NdasFcUpdateSendSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				ChooseSendSize,
	IN UINT32				TotalSend,
	IN LARGE_INTEGER		StartTime,
	IN LARGE_INTEGER		EndTime
	)
{
	LONG			sendCount;
	LONG			idx;
	LARGE_INTEGER	diff;

	if (EndTime.QuadPart < StartTime.QuadPart) {

		NDAS_ASSERT( FALSE );
	}

	sendCount = (TotalSend + ChooseSendSize - 1) / ChooseSendSize;
	idx = (ChooseSendSize - 1) / NDAS_FC_SEND_BLOCK_SIZE;

	NDAS_ASSERT( NdasFcStatistics->TotalThroughput.QuadPart > 0 );

	if (((EndTime.QuadPart - StartTime.QuadPart) << NDAS_FC_LOG_OF_AMPLIFIER) > (TotalSend * NdasFcStatistics->MinimumRoundTripTime.QuadPart)) {

		diff.QuadPart = EndTime.QuadPart - StartTime.QuadPart;

#if 1
		NdasFcStatistics->MinimumRoundTripTime.QuadPart += NdasFcStatistics->MinimumRoundTripTime.QuadPart >> 10;

		if (NdasFcStatistics->MinimumRoundTripTime.QuadPart > 
			(NANO100_PER_SEC << NDAS_FC_LOG_OF_AMPLIFIER) / (NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER)) {

			if (NdasFcStatistics->SentCount > NDAS_FC_FRACTION_RATE*2) {

				if (NdasFcStatistics->TotalThroughput.QuadPart <= (NDAS_FC_DEFAULT_TROUGHPUT << NDAS_FC_LOG_OF_AMPLIFIER)) {
				
					//NDAS_ASSERT( FALSE );
				}
			}
			
			NdasFcStatistics->MinimumRoundTripTime.QuadPart = 
				(NANO100_PER_SEC << NDAS_FC_LOG_OF_AMPLIFIER) / (NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER);
		}
#endif

	} else {

		diff.QuadPart = ((UINT64)TotalSend * NdasFcStatistics->MinimumRoundTripTime.QuadPart) >> NDAS_FC_LOG_OF_AMPLIFIER;
#if 1
		NdasFcStatistics->MinimumRoundTripTime.QuadPart -= NdasFcStatistics->MinimumRoundTripTime.QuadPart >> 10;

		if (NdasFcStatistics->MinimumRoundTripTime.QuadPart < (NDAS_FC_AMPLIFIER >> 3)) {

			NdasFcStatistics->MinimumRoundTripTime.QuadPart = (NDAS_FC_AMPLIFIER >> 3);
		}
#endif

	}

	NDAS_ASSERT( diff.QuadPart > 0 );

	NdasFcStatistics->Throughput[idx].QuadPart -= NdasFcStatistics->Throughput[idx].QuadPart >> NDAS_FC_LOG_OF_FRACTION;
	NdasFcStatistics->Throughput[idx].QuadPart += 
		((((UINT64)TotalSend * NANO100_PER_SEC) << NDAS_FC_LOG_OF_AMPLIFIER) / diff.QuadPart) >> NDAS_FC_LOG_OF_FRACTION;

	NdasFcStatistics->TotalThroughput.QuadPart -= NdasFcStatistics->TotalThroughput.QuadPart >> NDAS_FC_LOG_OF_FRACTION;
	NdasFcStatistics->TotalThroughput.QuadPart += 
		((((UINT64)TotalSend * NANO100_PER_SEC) << NDAS_FC_LOG_OF_AMPLIFIER) / diff.QuadPart) >> NDAS_FC_LOG_OF_FRACTION;

	NdasFcStatistics->SentCount++;

	if (NdasFcStatistics->SentCount == NDAS_FC_FRACTION_RATE) {

		if (NdasFcStatistics->PreviousTotalThroughput.QuadPart > NdasFcStatistics->TotalThroughput.QuadPart) {

			NdasFcStatistics->TotalThroughput.QuadPart -= NdasFcStatistics->TotalThroughput.QuadPart >> 1;
			NdasFcStatistics->TotalThroughput.QuadPart += NdasFcStatistics->PreviousTotalThroughput.QuadPart >> 1;

			NdasFcStatistics->PreviousTotalThroughput.QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;
			NdasFcStatistics->SentCount = 0;	
		}

		NdasFcStatistics->MaxTotalThroughput.QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;

		NdasFcStatistics->MinimumRoundTripTime.QuadPart = 
			(NANO100_PER_SEC << NDAS_FC_LOG_OF_AMPLIFIER) / (NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER);

		SPY_LOG_PRINT( LFS_DEBUG_LFS_TRACE, 
					("TotalThroughput = %I64d, roundTripTime = %I64d, previousT = %I64d\n", 
						NdasFcStatistics->TotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER,
						NdasFcStatistics->MinimumRoundTripTime.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER,
						NdasFcStatistics->PreviousTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER) );

		for (idx = 0; idx < NDAS_FC_MAX_SEND_SIZE/NDAS_FC_SEND_BLOCK_SIZE; idx++) {

			NdasFcStatistics->Throughput[idx].QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;
		} 

		return;
	}

	if (NdasFcStatistics->SentCount % (NDAS_FC_FRACTION_RATE) == 0) {

		if (NdasFcStatistics->MaxTotalThroughput.QuadPart < NdasFcStatistics->TotalThroughput.QuadPart) {

			NdasFcStatistics->MaxTotalThroughput.QuadPart = NdasFcStatistics->TotalThroughput.QuadPart;

		} else {

			NdasFcStatistics->MaxTotalThroughput.QuadPart -= NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_FRACTION;
			NdasFcStatistics->MaxTotalThroughput.QuadPart += NdasFcStatistics->TotalThroughput.QuadPart >> NDAS_FC_LOG_OF_FRACTION;
		}

		NdasFcStatistics->MinimumRoundTripTime.QuadPart = 
			(NANO100_PER_SEC << NDAS_FC_LOG_OF_AMPLIFIER) / (NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_AMPLIFIER);

		for (idx = 0; idx < NDAS_FC_MAX_SEND_SIZE/NDAS_FC_SEND_BLOCK_SIZE; idx++) {

			NdasFcStatistics->Throughput[idx].QuadPart -= NdasFcStatistics->Throughput[idx].QuadPart >> NDAS_FC_LOG_OF_FRACTION;
			NdasFcStatistics->Throughput[idx].QuadPart += NdasFcStatistics->MaxTotalThroughput.QuadPart >> NDAS_FC_LOG_OF_FRACTION;
		}
	}
}