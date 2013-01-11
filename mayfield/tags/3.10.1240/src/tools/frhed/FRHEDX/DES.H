#ifndef DES_ENCRYPTION_H
#define DES_ENCRYPTION_H

//typedef BYTE des_cblock[8];

typedef struct
{
    union
    {
        BYTE Byte[8];
        DWORD Long[2];
    };
} DES_CODEBLOCK, *LPDES_CODEBLOCK;

typedef struct 
{
    DES_CODEBLOCK Code[16];
} DES_KEY_SCHEDULE, *LPDES_KEY_SCHEDULE;

//typedef struct des_ks_struct
//{
//	union
//	{
//		des_cblock _;		/* make sure things are correct size on machines with 8 byte longs */
//		DWORD pad[2];
//	} ks;
//#define _	ks._
//} des_key_schedule[16];

#define DESMODE_ENCRYPT (1)
#define DESMODE_DECRYPT (0)

int  des_ecb_encrypt(LPDES_CODEBLOCK input,LPDES_KEY_SCHEDULE ks,int mode);
int  triple_des_ecb_encrypt(LPDES_CODEBLOCK input,LPDES_KEY_SCHEDULE ks1,LPDES_KEY_SCHEDULE ks2,LPDES_KEY_SCHEDULE ks3,int mode);
int  des_key_sched(LPDES_CODEBLOCK key,LPDES_KEY_SCHEDULE schedule);

#endif // DES_ENCRYPTION_H

