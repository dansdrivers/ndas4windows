#ifndef _NDAS_CIPHER_LIBRARY_
#define _NDAS_CIPHER_LIBRARY_

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

#define NCIPHER_MAX_KEYLENGTH		64	// 512 bits.
#define NCIPHER_MAX_KEYLENGTH_BIT	512

//
//	keep the same value as in individual ciphers.
//
#define	NCIPHER_MODE_ECB			1 //  Are we ciphering in ECB mode?
#define	NCIPHER_MODE_CBC			2 //  Are we ciphering in CBC mode?
#define	NCIPHER_MODE_CFB1			3 //  Are we ciphering in 1-bit CFB mode?

typedef	struct _NCIPHER_INSTANCE {

	UINT16	CipherType;
	BYTE	Mode;
	PVOID	CipherInterface;
	UINT16	InstanceSpecificLength;
	UCHAR	InstanceSpecific[1];

} NCIPHER_INSTANCE, *PNCIPHER_INSTANCE;

typedef	struct _NCIPHER_KEY {

	UINT16	CipherType;
	UINT16	KeyBinaryLength;			// bits
	BYTE	KeyBinary[NCIPHER_MAX_KEYLENGTH];
	UINT16	CipherSpecificKeyLength;
	UCHAR	CipherSpecificKey[1];

} NCIPHER_KEY, *PNCIPHER_KEY;

//////////////////////////////////////////////////////////////////////////
//
//	exported functions.
//
NTSTATUS
CreateCipher(
	 PNCIPHER_INSTANCE		*Cipher,
	 BYTE					CipherType,
	 BYTE					Mode,
	 int					ExtraKeyLength,
	 PBYTE					ExtraKey
);

NTSTATUS
CloseCipher(
	 PNCIPHER_INSTANCE	Cipher
);

NTSTATUS
CreateCipherKey(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY *		Key,
	int					KeyBinaryLength,	// Bytes
	PBYTE				KeyBinary
);

NTSTATUS
CloseCipherKey(
	PNCIPHER_KEY key
);

NTSTATUS
EncryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	LONG				BufferLength,
	PBYTE				InBuffer,
	PBYTE				OutBuffer
);

NTSTATUS
EncryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	PBYTE				InBuffer,
	int					InputOctets,
	PBYTE				OutBuffer
);

NTSTATUS
DecryptBlock(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	int					BufferLength,
	PBYTE				InBuffer,
	PBYTE				OutBuffer
);

NTSTATUS
DecryptPad(
	PNCIPHER_INSTANCE	Cipher,
	PNCIPHER_KEY		Key,
	PBYTE				InBuffer,
	int					InputOctets,
	PBYTE				OutBuffer
);


#endif
