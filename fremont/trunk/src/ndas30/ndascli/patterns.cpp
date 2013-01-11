#include "stdafx.h"


#define NUM_PATTERNS RAND_MAX // 0x7fff 32767

static int _inline PatternGen(int PatternType, int Offset)
{
	BYTE byte0 = (BYTE)(Offset & 0x0ff);
	return ((PatternType<<17) + PatternType) ^ Offset ^ ((byte0 <<9) + (byte0 <<18) + (byte0 <<27));
#if 0
	int Byte4 = Offset & 0x0ff;
	int LowWord = Offset & 0x0ffff;
	int HighWord = (Offset >>16) & 0x0ffff;

	switch(PatternType){
	case 0:		return Offset; 
	case 1:		return Offset/2;
	case 2:		return Offset/4;
	case 3:		return Offset ^ 0x0f0f0f0f;
	case 4:		return Offset ^ 0xaaaaaaaa;
	case 5:		return (LowWord <<16) + LowWord;
	case 6:		return Offset*Offset;
	case 7:		return Offset |= 0xaaaaaaaa;
	case 8:		return Offset &= 0xaaaaaaaa;
	case 9:		return Offset &= 0x55555555;
	case 10:	return Offset |= 0x77777777;
	case 11:	return Offset |= 0x55555555;
	case 12:	return (Byte4) * 0x01010101;
	case 13:	return (Byte4^0x0ff) * 0x01010101;
	case 14:	return (Byte4^0x0aa) * 0x01010101;
	case 15:	return (Byte4^0x055) * 0x01010101;
	case 16:	return (Byte4<<(Byte4%2)) * 0x01010101;
	case 17:	return (Byte4<<(Byte4%3)) * 0x01010101;
	case 18:	return (Byte4<<(Byte4%4)) * 0x01010101;
	case 19:	return (Byte4>>(Byte4%2)) * 0x01010101;
	case 20:	return (Byte4>>(Byte4%3)) * 0x01010101;
	case 21:	return (Byte4>>(Byte4%4)) * 0x01010101;
	case 22:	return LowWord * 0x303;
	case 23:	return LowWord * 0x5005;
	case 24:	return LowWord * 0x70007;
	case 25:	return LowWord * 0x11111;
	case 26:	return ((Byte4) * 0x01010101) ^ 0x1234568;
	case 27:	return ((Byte4) * 0x01010201) ^ 0x9abcdef0;
	case 28:	return ((Byte4) * 0x01030101) ^ 0x192a3b4c;
	case 29:	return ((Byte4) * 0x04010101) ^ 0x12345668;
	case 30:	return ((Byte4) * 0x04010101) ^ 0x12345668;
	case 31:	return ((Byte4) * 0x01030201) ^ 0x12345668;
	case 32:	return ((Byte4) * 0x01030201) ^ 0x12345668;
	case 33:	return ((Byte4) * 0x04010101) ^ 0x12345668;
	case 34:	return ((Byte4) * 0x01010101) ^ 0x87654321;
	default:
		printf("Invalid pattern number\n");
		return 0;
	}
#endif
}

int GetNumberOfPattern(void)
{
	return NUM_PATTERNS;
}
#if 0 // This version is too slow..
#define NUM_PATTERNS RAND_MAX


UINT64 _inline longhash1(UINT64 key)
{
  key += ~(key << 32);
  key ^= _rotr64(key,22);
  key += ~(key << 13);
  key ^= _rotr64(key,8);
  key += (key << 3);
  key ^= _rotr64(key,15);
  key += ~(key << 27);
  key ^= _rotr64(key,31);
  return key;
}

int PatternGen(int PatternType, int Offset)
{
	UINT64 key = ((UINT64)PatternType<<32) + Offset;
	int i = sizeof(long);
	key = longhash1(key);
	return (int)(key & 0x0ffffffff);
}

int GetNumberOfPattern(void)
{
	return RAND_MAX;
}
#endif
BOOL CheckPattern(int PatternNum, int PatternOffset, PUCHAR Buf, int BufLen)
{
	int i;
	UINT32* Pattern = (UINT32*)Buf;
	int pattern = PatternNum%NUM_PATTERNS;
	int mismatch_count = 0;
	if (BufLen%4 !=0 || PatternOffset%4 !=0) {
		printf("Invalid buffer size\n");
		return FALSE;
	}

	for(i=0;i<BufLen/4;i++) {
		if (Pattern[i] != PatternGen(pattern, PatternOffset/4+i)) {
			if (mismatch_count ==0) {
				printf("\nPattern mismatch:\n");
				printf("[Offset  ] Expected -> Actual\n");
			}
			printf("[%08x] %08x -> %08x\n", PatternOffset + i*4, 
				htonl(PatternGen(pattern, PatternOffset/4+i)), 
				htonl(Pattern[i])
			);
			if (i*4 == 0x5cc) {
				printf("Check whether LPX MTU is not multiple of 16. Use up-to-date LPX driver\n");
			}
			mismatch_count++;
			if (mismatch_count>64) {
				printf("Too much mistch. Stopped comparing\n");
				return FALSE;
			}
		}
	}
	if (mismatch_count)
		return FALSE;
	return TRUE;
}

void FillPattern(int PatternNum, int PatternOffset, PUCHAR Buf, int BufLen)
{
	UINT32* Pattern = (UINT32*)Buf;
	int pattern = PatternNum%NUM_PATTERNS;
	int i;
	if (BufLen%4 !=0 || PatternOffset%4 !=0) {
		printf("Invalid buffer size\n");
		return;
	}
	for(i=0;i<BufLen/4;i++) 
		Pattern[i] = PatternGen(pattern, PatternOffset/4+i);
}

