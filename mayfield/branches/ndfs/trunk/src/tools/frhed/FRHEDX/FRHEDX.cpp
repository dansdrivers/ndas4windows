#include "precomp.h"
#include "FRHEDX.h"
#include "std_defs.h"
#include "des.h"

typedef void (*LPFN_set_key)(const u4byte in_key[], const u4byte key_len);
typedef void (*LPFN_encrypt)(const u4byte in_blk[4], u4byte out_blk[]);
typedef void (*LPFN_decrypt)(const u4byte in_blk[4], u4byte out_blk[4]);

typedef struct
{
    DWORD dwKeySizeInBits;
    LPFN_set_key set_key;
    LPFN_encrypt encrypt;
    LPFN_decrypt decrypt;
} AESBASED_CRYPTENGINE;

void AESBasedCryptEngine( AESBASED_CRYPTENGINE* q, s_MEMORY_ENCODING* p )
{
    BYTE bKey[16];
    ZeroMemory(bKey,sizeof(bKey));

    ULONG ulIndex = 0;
    for( LPCSTR pwd = p->lpszArguments; pwd && *pwd; pwd++ )
        bKey[ulIndex++ % 16] ^= *pwd;
                   
    // make memory size fit 64-bit block size
    DWORD dwMaxIndex = p->dwSize / 16, dwIndex;

    // create key
    q->set_key((u4byte*)bKey,q->dwKeySizeInBits);

    LPBYTE lpbMemory = p->lpbMemory;

    if( p->bEncode )
    {
        for( dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
        {
            q->encrypt((u4byte*)lpbMemory,(u4byte*)lpbMemory);
            lpbMemory += 16;
        }
    }
    else
    {
        for( dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
        {
            q->decrypt((u4byte*)lpbMemory,(u4byte*)lpbMemory);
            lpbMemory += 16;
        }
    }
} // Process()

#define DECLARE_AESBASED_CRYPTENGINE(name,bits) \
AESBASED_CRYPTENGINE ac##name##bits =           \
{   bits,                                       \
    (LPFN_set_key)name##_set_key,               \
    (LPFN_encrypt)name##_encrypt,               \
    (LPFN_decrypt)name##_decrypt };             \
                                                \
void WINAPI name##bits( s_MEMORY_ENCODING* p )  \
{                                               \
    AESBasedCryptEngine(&ac##name##bits,p);     \
}


DECLARE_AESBASED_CRYPTENGINE(rijndael,128)
DECLARE_AESBASED_CRYPTENGINE(rijndael,192)
DECLARE_AESBASED_CRYPTENGINE(rijndael,256)

DECLARE_AESBASED_CRYPTENGINE(serpent,128)
DECLARE_AESBASED_CRYPTENGINE(serpent,192)
DECLARE_AESBASED_CRYPTENGINE(serpent,256)

DECLARE_AESBASED_CRYPTENGINE(twofish,128)
DECLARE_AESBASED_CRYPTENGINE(twofish,192)
DECLARE_AESBASED_CRYPTENGINE(twofish,256)

void WINAPI PlainDes( s_MEMORY_ENCODING* p )
{ 
    BYTE bKey[8];
    ZeroMemory(bKey,sizeof(bKey));

    ULONG ulIndex = 0;
    for( LPCSTR pwd = p->lpszArguments; pwd && *pwd; pwd++ )
        bKey[ulIndex++ % 8] ^= *pwd;

    DWORD dwMaxIndex = p->dwSize / 8;

    DES_CODEBLOCK key;
    for( DWORD dwIndex = 0; dwIndex < 8; dwIndex++ )
        key.Byte[dwIndex] = bKey[dwIndex];

    DES_KEY_SCHEDULE ks;
    des_key_sched(&key,&ks);

    int des_mode = p->bEncode ? DESMODE_ENCRYPT : DESMODE_DECRYPT;
    
    LPBYTE lpbMemory = p->lpbMemory;
    for( dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
    {
        LPDES_CODEBLOCK block = (LPDES_CODEBLOCK) lpbMemory;
        des_ecb_encrypt(block,&ks,des_mode);
        lpbMemory += 8;
    }
}

void WINAPI TripleDes( s_MEMORY_ENCODING* p )
{
    BYTE bKey[24];
    ZeroMemory(bKey,sizeof(bKey));

    ULONG ulIndex = 0;
    for( LPCSTR pwd = p->lpszArguments; pwd && *pwd; pwd++ )
        bKey[ulIndex++ % 24] ^= *pwd;

    DWORD dwMaxIndex = p->dwSize / 8;

    DES_CODEBLOCK key1, key2, key3;
    DES_KEY_SCHEDULE ks1, ks2, ks3;
    DWORD dwIndex;

    #define PREP_DES_BLOCK(KEY_NAME,KS_NAME)            \
        for( dwIndex = 0; dwIndex < 8; dwIndex++ )      \
            KEY_NAME.Byte[dwIndex] = *(lpKey++);        \
        des_key_sched(&KEY_NAME,&KS_NAME);

    LPBYTE lpKey = bKey;
    PREP_DES_BLOCK(key1,ks1)
    PREP_DES_BLOCK(key2,ks2)
    PREP_DES_BLOCK(key3,ks3)

    int des_mode = p->bEncode ? DESMODE_ENCRYPT : DESMODE_DECRYPT;
    
    LPBYTE lpbMemory = p->lpbMemory;
    for( dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
    {
        LPDES_CODEBLOCK block = (LPDES_CODEBLOCK) lpbMemory;
        triple_des_ecb_encrypt(block,&ks1,&ks2,&ks3,des_mode);
        lpbMemory += 8;
    }
}

void Blowfish_encryptBlock(ULONG& xl, ULONG& xr);
void Blowfish_decryptBlock(ULONG& xl, ULONG& xr);
void Blowfish_init(const char* key, int keylen);

void WINAPI Blowfish( s_MEMORY_ENCODING* p )
{
    BYTE bKey[56];
    ZeroMemory(bKey,sizeof(bKey));

    ULONG ulIndex = 0;
    for( LPCSTR pwd = p->lpszArguments; pwd && *pwd; pwd++ )
        bKey[ulIndex++ % 56] ^= *pwd;

    Blowfish_init((LPCSTR)bKey,(int) 56);

    DWORD dwMaxIndex = p->dwSize / 8;
    LPBYTE lpbMemory = p->lpbMemory;

    if( p->bEncode )
    {
        for( DWORD dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
        {
            LPDWORD p = (LPDWORD) lpbMemory;
            Blowfish_encryptBlock(p[0],p[1]);
            lpbMemory += 8;
        }
    }
    else
    {
        for( DWORD dwIndex = 0; dwIndex < dwMaxIndex; dwIndex++ )
        {
            LPDWORD p = (LPDWORD) lpbMemory;
            Blowfish_decryptBlock(p[0],p[1]);
            lpbMemory += 8;
        }
    }
} // Process()


MEMORY_CODING_DESCRIPTION MemoryCodings[] =
{
    { "DES", PlainDes },
    { "Triple-DES", TripleDes },
    { "Blowfish", Blowfish },
    { "256-Bit SERPENT", serpent256 },
    { "256-Bit RIJNDAEL", rijndael256 },
    { "256-Bit TWOFISH", twofish256 },
    { "192-Bit SERPENT", serpent192 },
    { "192-Bit RIJNDAEL",rijndael192 },
    { "192-Bit TWOFISH", twofish192 },
    { "128-Bit SERPENT", serpent128 },
    { "128-Bit RIJNDAEL", rijndael128 },
    { "128-Bit TWOFISH", twofish128 },

    { 0, 0 }
};

EXTERN_C LPMEMORY_CODING_DESCRIPTION WINAPI GetMemoryCodings()
{
    return MemoryCodings;
}
