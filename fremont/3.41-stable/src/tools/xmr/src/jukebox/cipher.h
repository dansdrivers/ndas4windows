#ifndef _JB_M_NDAS_CIPHER_LIBRARY_
#define _JB_M_NDAS_CIPHER_LIBRARY_

//////////////////////////////////////////////////////////////////////////
//
//	Cipher method specification.
//
//						Simple hash		AES(rijndael)
//
//	Key length (bits)	32				128, 192, 256
//	Block unit (bits)	32				128
//
//

#define NCIPHER_POOLTAG_CIPHERINSTANCE	'iCNL'
#define NCIPHER_POOLTAG_CIPHERKEY		'kCNL'

#define	NDAS_CIPHER_NONE	0
#define	NDAS_CIPHER_SIMPLE	1
#define	NDAS_CIPHER_AES		2

#define NDAS_CIPHER_AES_LENGTH		16
#define NCIPHER_MAX_KEYLENGTH		64	// 512 bits.
#define NCIPHER_MAX_KEYLENGTH_BIT	512

//
//	keep the same value as in individual ciphers.
//
#define	NCIPHER_MODE_ECB			1 //  Are we ciphering in ECB mode?
#define	NCIPHER_MODE_CBC			2 //  Are we ciphering in CBC mode?
#define	NCIPHER_MODE_CFB1			3 //  Are we ciphering in 1-bit CFB mode?

#define HASH_KEY_LENGTH				8

#include <pshpack1.h>
typedef struct _CIPHER_HASH {
	unsigned char HashKey[HASH_KEY_LENGTH];
} CIPHER_HASH, *PCIPHER_HASH;


typedef struct _CIPHER_HASH_KEY {
	unsigned char CntEcr_IR[HASH_KEY_LENGTH];
	unsigned char CntDcr_IR[HASH_KEY_LENGTH];
} CIPHER_HASH_KEY, *PCIPHER_HASH_KEY;




typedef	struct _NCIPHER_INSTANCE {
	int 	currentNd;
	int	currentDisc;
	unsigned short	CipherType;
	unsigned char	Mode;
	void *		CipherInterface;
	unsigned int	InstanceSpecificLength;
	unsigned int	InstanceSpecific[0];
}NCIPHER_INSTANCE, *PNCIPHER_INSTANCE;

typedef	struct _NCIPHER_KEY {
	int	currentNd;
	int 	currentDisc;
	unsigned short	CipherType;
	unsigned short	KeyBinaryLength;			// bits
	unsigned char	KeyBinary[NCIPHER_MAX_KEYLENGTH];
	unsigned int	CipherSpecificKeyLength;
	unsigned int	CipherSpecificKey[0];
}NCIPHER_KEY, *PNCIPHER_KEY;

#include <poppack.h>
#ifdef  __cplusplus
extern "C"
{
#endif 
//////////////////////////////////////////////////////////////////////////
//
//	exported functions.
//
PNCIPHER_INSTANCE
CreateCipher(
	 PNCIPHER_INSTANCE		pCipherInstance,	
	 unsigned char			CipherType,
	 unsigned char			Mode,
	 int				ExtraKeyLength,
	 unsigned char *		ExtraKey,
	 int			currentNd,
	 int			currentDisc );

int
CloseCipher(
	 PNCIPHER_INSTANCE	Cipher
);


PNCIPHER_KEY
CreateCipherKey(
	PNCIPHER_KEY		pCipherKey,
	PNCIPHER_INSTANCE	Cipher,
	int			KeyBinaryLength,
	unsigned char *		KeyBinary,
	int 			currentNd,
	int			currentDisc);	


int
CloseCipherKey(
	PNCIPHER_KEY key
);

int
EncryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned int		BufferLength,
	unsigned char *		InBuffer,
	unsigned char *		OutBuffer
);

int
EncryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned char *		InBuffer,
	int			InputOctets,
	unsigned char *		OutBuffer
);

int
DecryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	int			BufferLength,
	unsigned char *		InBuffer,
	unsigned char *		OutBuffer
);

int
DecryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	unsigned char *		InBuffer,
	int			InputOctets,
	unsigned char * 	OutBuffer
);

#ifdef  __cplusplus
}
#endif 
#endif
