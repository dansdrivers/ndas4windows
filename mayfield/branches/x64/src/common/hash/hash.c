#include <windows.h>
#include "hash.h"

//
// pResult is Big Endian...
// pSource and pKey are Little Endian.
//
void
__stdcall
Hash32To128(
			unsigned char	*pSource,
			unsigned char	*pResult,
			unsigned char	*pKey
			)
{
	unsigned		uiTemp;
	unsigned char	ucKey_con0[4];
	unsigned char	ucKey_con1[4];
	
	uiTemp = KEY_CON0;
	memcpy(ucKey_con0, &uiTemp, 4);

	uiTemp = KEY_CON1;
	memcpy(ucKey_con1, &uiTemp, 4);

	pResult[ 0] = pKey[2] ^ pSource[1];
	pResult[ 1] = ucKey_con0[3] | pSource[2];
	pResult[ 2] = ucKey_con0[2] & pSource[1];
	pResult[ 3] = ~(pKey[6] ^ pSource[2]);
	pResult[ 4] = ucKey_con1[2] ^ pSource[3];
	pResult[ 5] = pKey[0] & pSource[0];
	pResult[ 6] = ~(ucKey_con1[0] ^ pSource[1]);
	pResult[ 7] = pKey[5] | pSource[3];
	pResult[ 8] = pKey[7] & pSource[2];
	pResult[ 9] = ucKey_con0[0] ^ pSource[1];
	pResult[10] = pKey[4] ^ pSource[3];
	pResult[11] = ~(ucKey_con1[1] ^ pSource[0]);
	pResult[12] = pKey[3] ^ pSource[2];
	pResult[13] = ucKey_con0[1] & pSource[3];
	pResult[14] = pKey[1] ^ pSource[3];
	pResult[15] = ucKey_con1[3] | pSource[0];

	return;
}

//////////////////////////////////////////////////////////////////////////
//
//	Encryption
//
void
__stdcall
Encrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  )
{
	unsigned int	i, j;
	char			temp[4];
	unsigned char	*pDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		temp[0] = pPassword[1] ^ pPassword[7] ^ pDataPtr[3] ^ pKey[3];
		temp[1] = ~(pPassword[0] ^ pPassword[3] ^ pKey[0] ^ pDataPtr[0]);
		temp[2] = pPassword[2] ^ pPassword[6] ^ pDataPtr[1] ^ pKey[2];
		temp[3] = ~(pPassword[4] ^ pPassword[5] ^ pDataPtr[2] ^ pKey[1]);

		for(j = 0; j < 4; j++)
			pDataPtr[j] = temp[j];
	}
}


void
__stdcall
Encrypt32SP(
			unsigned char		*pData,
			unsigned _int32		uiDataLength,
			unsigned _int8		*pEncryptIR
			)
{
	unsigned int	i;
	register char	temp0;
	register char	temp1;
	register char	temp2;
	register char	temp3;
	unsigned char	*pDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		temp0 = pEncryptIR[0] ^ pDataPtr[3];
		temp1 = ~(pEncryptIR[1] ^ pDataPtr[0]);
		temp2 = pEncryptIR[2] ^ pDataPtr[1];
		temp3 = ~(pEncryptIR[3] ^ pDataPtr[2]);
		
		pDataPtr[0] = temp0;
		pDataPtr[1] = temp1;
		pDataPtr[2] = temp2;
		pDataPtr[3] = temp3;
	}
}

void
__stdcall
Encrypt32SPAndCopy(
				   unsigned char		*pDestinationData,
				   unsigned char		*pSourceData,
				   unsigned _int32		uiDataLength,
				   unsigned _int8		*pEncryptIR
				   )
{
	unsigned int	i;
	//register char	temp0;
	//register char	temp1;
	//register char	temp2;
	//register char	temp3;
	unsigned char	*pSrcDataPtr;
	unsigned char	*pDestDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pSrcDataPtr = &pSourceData[i * 4];
		pDestDataPtr = &pDestinationData[i * 4];

		pDestDataPtr[0] = pEncryptIR[0] ^ pSrcDataPtr[3];
		pDestDataPtr[1] = ~(pEncryptIR[1] ^ pSrcDataPtr[0]);
		pDestDataPtr[2] = pEncryptIR[2] ^ pSrcDataPtr[1];
		pDestDataPtr[3] = ~(pEncryptIR[3] ^ pSrcDataPtr[2]);
		
		//pDataPtr[0] = temp0;
		//pDataPtr[1] = temp1;
		//pDataPtr[2] = temp2;
		//pDataPtr[3] = temp3;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//	Decryption
//
void
__stdcall
Decrypt32(
		  unsigned char		*pData,
		  unsigned	_int32	uiDataLength,
		  unsigned char		*pKey,
		  unsigned char		*pPassword
		  )
{
	unsigned int	i, j;
	char			temp[4];
	unsigned char	*pDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		temp[3] = pPassword[1] ^ pPassword[7] ^ pDataPtr[0] ^ pKey[3];
		temp[0] = pPassword[0] ^ pPassword[3] ^ pKey[0] ^ ~(pDataPtr[1]);
		temp[1] = pPassword[2] ^ pPassword[6] ^ pDataPtr[2] ^ pKey[2];
		temp[2] = pPassword[4] ^ pPassword[5] ^ pDataPtr[3] ^ ~(pKey[1]);

		for(j = 0; j < 4; j++)
			pDataPtr[j] = temp[j];
	}
}

void
__stdcall
Decrypt32SP(
		  unsigned char		*pData,
		  unsigned _int32	uiDataLength,
		  unsigned _int8	*pDecryptIR
		  )
{
	unsigned int	i;
	register char	temp0;
	register char	temp1;
	register char	temp2;
	register char	temp3;
	unsigned char	*pDataPtr;

	for(i = 0; i < uiDataLength / 4; i++) {
		pDataPtr = &pData[i * 4];

		temp3 = pDecryptIR[3] ^ pDataPtr[0];
		temp0 = pDecryptIR[0] ^ pDataPtr[1];
		temp1 = pDecryptIR[1] ^ pDataPtr[2];
		temp2 = pDecryptIR[2] ^ pDataPtr[3];

		pDataPtr[0] = temp0;
		pDataPtr[1] = temp1;
		pDataPtr[2] = temp2;
		pDataPtr[3] = temp3;
	}
}
