#include <ntddk.h>
#include "LSKLib.h"
#include "basetsdex.h"
#include "KDebug.h"
#include "cipher.h"
#include "rijndael-api-fst.h"
#include "hash.h"

#ifdef __MODULE__
#undef __MODULE__
#endif // __MODULE__
#define __MODULE__ "NdasCipher"

//////////////////////////////////////////////////////////////////////////
//
//	Cipher instance management.
//


//
//
//
NTSTATUS
CreateCipher(
	 PNCIPHER_INSTANCE		*Cipher,
	 BYTE					CipherType,
	 BYTE					Mode,
	 int					ExtraKeyLength,
	 PBYTE					ExtraKey
){
	int					instLength;
	PNCIPHER_INSTANCE	tmpCipher;
	int					ret;

	if(!Cipher) {
		KDPrintM(DBG_OTHER_ERROR, ("Cipher parameter is NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	switch(CipherType)
	{
	case NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH	cipherHash;
	
		if(!ExtraKey) {
			KDPrintM(DBG_OTHER_ERROR, ("Hash key is NULL.\n"));
			return STATUS_INVALID_PARAMETER;
		}
		if(ExtraKeyLength != HASH_KEY_LENGTH) {
			KDPrintM(DBG_OTHER_ERROR, ("Hash key length invalid. ExtraKeyLength=%d\n", ExtraKeyLength));
			return STATUS_INVALID_PARAMETER;
		}

		instLength = sizeof(NCIPHER_INSTANCE) - sizeof(BYTE) + sizeof(CIPHER_HASH);
		tmpCipher = (PNCIPHER_INSTANCE)ExAllocatePoolWithTag(NonPagedPool, instLength, NCIPHER_POOLTAG_CIPHERINSTANCE);
		if(!tmpCipher) {
			KDPrintM(DBG_OTHER_ERROR, ("Could not allocate Cipher instance.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlZeroMemory(tmpCipher, instLength);
		tmpCipher->CipherType = NDAS_CIPHER_SIMPLE;
		tmpCipher->InstanceSpecificLength = sizeof(CIPHER_HASH);

		//
		//	Set Hash key.
		//
		cipherHash = (PCIPHER_HASH)tmpCipher->InstanceSpecific;
		RtlCopyMemory(cipherHash->HashKey, ExtraKey, HASH_KEY_LENGTH);

		*Cipher = tmpCipher;
	}
	break;
	case NDAS_CIPHER_AES:
		instLength = sizeof(NCIPHER_INSTANCE) - sizeof(BYTE) + sizeof(cipherInstance);
		tmpCipher = (PNCIPHER_INSTANCE)ExAllocatePoolWithTag(NonPagedPool, instLength, NCIPHER_POOLTAG_CIPHERINSTANCE);
		if(!tmpCipher) {
			KDPrintM(DBG_OTHER_ERROR, ("Could not allocate Cipher instance.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlZeroMemory(tmpCipher, instLength);
		tmpCipher->CipherType = NDAS_CIPHER_AES;
		tmpCipher->InstanceSpecificLength = sizeof(cipherInstance);

		ret = cipherInit((cipherInstance *)tmpCipher->InstanceSpecific, Mode, NULL);
		if(ret != TRUE) {
			ExFreePool(tmpCipher);
			KDPrintM(DBG_OTHER_ERROR, ("Could not initialize the Cipher. ERRORCODE:%d\n", ret));
			return STATUS_UNSUCCESSFUL;
		}

		*Cipher = tmpCipher;
	break;
	default:
		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
CloseCipher(
	 PNCIPHER_INSTANCE	Cipher
){
	if(!Cipher) {
		KDPrintM(DBG_OTHER_ERROR, ("Cipher parameter is NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	ExFreePoolWithTag(Cipher, NCIPHER_POOLTAG_CIPHERINSTANCE);

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	Key instance management.
//

//
//
//
NTSTATUS
CreateCipherKey(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY *		Key,
	int					KeyBinaryLength,
	PBYTE				KeyBinary
){
	int					keyLength;
	PNCIPHER_KEY		tmpKey;
	int					ret;

	if(KeyBinaryLength > NCIPHER_MAX_KEYLENGTH) {
		KDPrintM(DBG_OTHER_ERROR, ("Key length too large. KeyBinaryLen=%d\n", KeyBinaryLength));
		return STATUS_INVALID_PARAMETER;
	}

	switch(Cipher->CipherType)
	{
	case NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH		hash;
		PCIPHER_HASH_KEY	hashKey;

		if(KeyBinaryLength != sizeof(UINT32)) {
			KDPrintM(DBG_OTHER_ERROR, ("Key length invalid. KeyBinaryLen=%d\n", KeyBinaryLength));
			return STATUS_INVALID_PARAMETER;
		}

		keyLength = sizeof(NCIPHER_KEY) - sizeof(BYTE) + sizeof(CIPHER_HASH_KEY);
		tmpKey = (PNCIPHER_KEY)ExAllocatePoolWithTag(NonPagedPool, keyLength, NCIPHER_POOLTAG_CIPHERKEY);
		if(!tmpKey) {
			KDPrintM(DBG_OTHER_ERROR, ("Could not allocate Cipher key.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlZeroMemory(tmpKey, keyLength);
		tmpKey->CipherType = NDAS_CIPHER_SIMPLE;
		tmpKey->CipherSpecificKeyLength = sizeof(CIPHER_HASH_KEY);
		tmpKey->KeyBinaryLength = KeyBinaryLength * 8;
		RtlCopyMemory(tmpKey->KeyBinary, KeyBinary, KeyBinaryLength);

		//
		//	Calculate intermediate values.
		//
		hashKey = (PCIPHER_HASH_KEY)tmpKey->CipherSpecificKey;
		hash = (PCIPHER_HASH)Cipher->InstanceSpecific;

		hashKey->CntEcr_IR[0] = hash->HashKey[1] ^ hash->HashKey[7] ^ KeyBinary[3];
		hashKey->CntEcr_IR[1] = hash->HashKey[0] ^ hash->HashKey[3] ^ KeyBinary[0];
		hashKey->CntEcr_IR[2] = hash->HashKey[2] ^ hash->HashKey[6] ^ KeyBinary[2];
		hashKey->CntEcr_IR[3] = hash->HashKey[4] ^ hash->HashKey[5] ^ KeyBinary[1];

		hashKey->CntDcr_IR[0] = ~(hash->HashKey[0] ^ hash->HashKey[3] ^ KeyBinary[0]);
		hashKey->CntDcr_IR[1] = hash->HashKey[2] ^ hash->HashKey[6] ^ KeyBinary[2];
		hashKey->CntDcr_IR[2] = hash->HashKey[4] ^ hash->HashKey[5] ^ ~(KeyBinary[1]);
		hashKey->CntDcr_IR[3] = hash->HashKey[1] ^ hash->HashKey[7] ^ KeyBinary[3];

		*Key = tmpKey;
	}
	break;
	case NDAS_CIPHER_AES:
		keyLength = sizeof(NCIPHER_KEY) - sizeof(BYTE) + sizeof(keyInstance);
		tmpKey = (PNCIPHER_KEY)ExAllocatePoolWithTag(NonPagedPool, keyLength, NCIPHER_POOLTAG_CIPHERKEY);
		if(!tmpKey) {
			KDPrintM(DBG_OTHER_ERROR, ("Could not allocate Cipher key.\n"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		RtlZeroMemory(tmpKey, keyLength);
		tmpKey->CipherType = NDAS_CIPHER_AES;
		tmpKey->CipherSpecificKeyLength = sizeof(keyInstance);
		tmpKey->KeyBinaryLength = KeyBinaryLength * 8;
		RtlCopyMemory(tmpKey->KeyBinary, KeyBinary, KeyBinaryLength);

		KDPrintM(DBG_OTHER_INFO, ("the Cipher Key. Length:%d\n", tmpKey->KeyBinaryLength));
		ret = makeKeyEncDec((keyInstance *)tmpKey->CipherSpecificKey, tmpKey->KeyBinaryLength, KeyBinary);
		if(ret != TRUE) {
			ExFreePool(tmpKey);
			KDPrintM(DBG_OTHER_ERROR, ("Could not initialize the Cipher Key.\n"));
			return STATUS_UNSUCCESSFUL;
		}

		*Key = tmpKey;
	break;
	default:
		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
CloseCipherKey(
	PNCIPHER_KEY key
){
	if(!key) {
		KDPrintM(DBG_OTHER_ERROR, ("Cipher parameter is NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	ExFreePoolWithTag(key, NCIPHER_POOLTAG_CIPHERKEY);

	return STATUS_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////
//
//	Encrypt/decrypt
//

NTSTATUS
EncryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	LONG				BufferLength,
	PBYTE				InBuffer,
	PBYTE				OutBuffer
){
	int ret;

	if(!InBuffer || !OutBuffer) {
		KDPrintM(DBG_OTHER_ERROR, ("Buffer parameter is NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	switch(Cipher->CipherType) {
	case	NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH_KEY	key = (PCIPHER_HASH_KEY)Key->CipherSpecificKey;


		if(InBuffer != OutBuffer) {
			Encrypt32SPAndCopy(
					OutBuffer,
					InBuffer,
					BufferLength,
					key->CntEcr_IR
				);
		} else {
			Encrypt32SP(
					InBuffer,
					BufferLength,
					key->CntEcr_IR
				);
		}
		break;
	}
	case	NDAS_CIPHER_AES: {
		keyInstance *aesKey = (keyInstance *)Key->CipherSpecificKey;

		aesKey->direction = DIR_ENCRYPT;
		ret = blockEncrypt(
						(cipherInstance *)Cipher->InstanceSpecific,
						aesKey,
						InBuffer,				// Input buffer
						BufferLength<<3,		// bits
						OutBuffer				// output buffer
					);
		if(ret < 0) {
			KDPrintM(DBG_OTHER_ERROR, ("blockEncrypt() failed. Ret=%d.\n", ret));
			return STATUS_UNSUCCESSFUL;
		}
		break;
	}
	default:
		return STATUS_INVALID_PARAMETER;
	}
	return STATUS_SUCCESS;
}

NTSTATUS
EncryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	PBYTE				InBuffer,
	int					InputOctets,
	PBYTE				OutBuffer
){

	UNREFERENCED_PARAMETER(Cipher);
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(InBuffer);
	UNREFERENCED_PARAMETER(InputOctets);
	UNREFERENCED_PARAMETER(OutBuffer);

	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
DecryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	int					BufferLength,
	PBYTE				InBuffer,
	PBYTE				OutBuffer
){
	int ret;

	if(!InBuffer || !OutBuffer) {
		KDPrintM(DBG_OTHER_ERROR, ("Buffer parameter is NULL!\n"));
		return STATUS_INVALID_PARAMETER;
	}

	switch(Cipher->CipherType) {
	case	NDAS_CIPHER_SIMPLE: {
		PCIPHER_HASH_KEY	key = (PCIPHER_HASH_KEY)Key->CipherSpecificKey;


		if(InBuffer != OutBuffer) {
			KDPrintM(DBG_OTHER_ERROR, ("Does not support encryption-copy!\n"));
			return STATUS_NOT_IMPLEMENTED;
		} else {
			Decrypt32SP(
					InBuffer,
					BufferLength,
					key->CntDcr_IR
				);
		}
		break;
	}
	case	NDAS_CIPHER_AES: {
		keyInstance *aesKey = (keyInstance *)Key->CipherSpecificKey;

		aesKey->direction = DIR_DECRYPT;
		ret = blockDecrypt(
						(cipherInstance *)Cipher->InstanceSpecific,
						aesKey,
						InBuffer,				// Input buffer
						BufferLength<<3,		// bits
						OutBuffer				// output buffer
					);
		if(ret < 0) {
			KDPrintM(DBG_OTHER_ERROR, ("blockDecrypt() failed. Ret=%d.\n", ret));
			return STATUS_UNSUCCESSFUL;
		}
		break;
	}
	default:
		return STATUS_INVALID_PARAMETER;
	}
	return STATUS_SUCCESS;
}

NTSTATUS
DecryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	PBYTE				InBuffer,
	int					InputOctets,
	PBYTE				OutBuffer
){

	UNREFERENCED_PARAMETER(Cipher);
	UNREFERENCED_PARAMETER(Key);
	UNREFERENCED_PARAMETER(InBuffer);
	UNREFERENCED_PARAMETER(InputOctets);
	UNREFERENCED_PARAMETER(OutBuffer);

	return STATUS_NOT_IMPLEMENTED;
}
