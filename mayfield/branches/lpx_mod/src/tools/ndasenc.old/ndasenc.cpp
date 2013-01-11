#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "ndastype.h"
#include "scrc32.h"
#include "ndascomm_api.h"
#include "socketlpx.h"
#include "ndasdib.h"
#include "md5.h"

void ShowErrorMessage(BOOL bExit = FALSE)
{
	DWORD dwError = ::GetLastError();

	HMODULE hModule = ::LoadLibraryEx(
		_T("ndasmsg.dll"),
		NULL,
		LOAD_LIBRARY_AS_DATAFILE);

	LPTSTR lpszErrorMessage = NULL;

	if (dwError & APPLICATION_ERROR_MASK) {
		if (NULL != hModule) {

			INT iChars = ::FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE,
				hModule,
				dwError,
				0,
				(LPTSTR) &lpszErrorMessage,
				0,
				NULL);
		}
	}
	else
	{
		INT iChars = ::FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwError,
			0,
			(LPTSTR) &lpszErrorMessage,
			0,
			NULL);
	}

	if (NULL != lpszErrorMessage) {
		_tprintf(_T("Error : %d(%08x) : %s\n"), dwError, dwError, lpszErrorMessage);
		::LocalFree(lpszErrorMessage);
	}
	else
	{
		_tprintf(_T("Unknown error : %d(%08x)\n"), dwError, dwError);
	}

	if(bExit)
	{
		::SetLastError(dwError);
		exit(dwError);
	}
}

void Header()
{
  printf(
    "NDAS Device Encrypt Key Generator Version 0.1\n"
    "Copyright (C) 2003-2004 XIMETA, Inc.\n\n");
}

void Usage(int err)
{
	printf(
	  "usage: ndasenc COMMAND -p PASS_PHRASE -m METHOD -l LENGTH -i ID\n"
	  "%s"
		"\n"
		"COMMAND : Only set command available\n"
		"PASS_PHRASE : Pass phrase, 4~8 chars of any characters\n"
		"METHOD : Encrypt method. 'simple' or 'aes'\n"
		"LENGTH : Encrypt key length.\n"
		"\tThe SIMPLE method supports 32 bits encrypt key.\n"
		"\tThe AES method supports 128, 192 and 256 bits encrypt key\n"
		"ID : 25 characters of the NDAS Device to write encrypt key on\n"
		"\n"
		"ex) ndasenc set -p mypass -t simple -l 32 -i 01234ABCDE56789FGHIJ13579\n",
		(0 == err) ? "ERROR : Wrong command" :
		(1 == err) ? "ERROR : Wrong pass pharse" :
		(2 == err) ? "ERROR : Wrong encrypt method" :
		(3 == err) ? "ERROR : Wrong encrypt key length" :
		(4 == err) ? "ERROR : Wrong NDAS Device ID" :
		(5 == err) ? "ERROR : Not enough parameters" :
		(6 == err) ? "ERROR : Encrypt key length does not match to encryption type" : ""
		);
	exit(err);
}

typedef enum _NDAS_ENC_CMD {
  NDAS_ENC_CMD_SET = 1,
} NDAS_ENC_CMD;

int __cdecl main(int argc, char *argv[])
{
	BOOL bResults;
	HNDAS hNdas;
	int i, j;
	int x;
	BOOL bCommandOk, bPassPhraseOk, bTypeOk, bLengthOk, bIDOk;
	NDAS_ENC_CMD cmd;
	CHAR PassPhrase[100];

	CONTENT_ENCRYPT ce;
	ZeroMemory(&ce, sizeof(ce));

	CHAR buffer[100];

	Header();

	bResults = NdasCommInitialize();

	if(!bResults)
	{
		ShowErrorMessage(TRUE);
	}

	NDAS_CONNECTION_INFO ci;

	bCommandOk = FALSE;
	bPassPhraseOk = FALSE;
	bTypeOk = FALSE;
	bLengthOk = FALSE;
	bIDOk = FALSE;

	for(i = 1; i < argc; i++)
	{
	  // parse command
	  if(1 == i)
	  {
  	  if(!strcmp(argv[i], "set"))
  	  {
  	    bCommandOk  = TRUE;
  	    cmd = NDAS_ENC_CMD_SET;
  	    continue;
  	  }
  	  Usage(0);
  	}

    // parse parameters
		if(!strcmp(argv[i], "-i") && ++i < argc) // NDAS Device ID
		{
			ZeroMemory(&ci, sizeof(ci));
			ci.UnitNo = 0;
			ci.protocol = IPPROTO_LPXTCP;
			ci.bWriteAccess = TRUE;
			ci.type = NDAS_CONNECTION_INFO_TYPE_IDA;
			if(::IsBadReadPtr(argv[i], 25))
				Usage(4);
			strncpy(ci.szDeviceStringId, argv[i], 25);

			bIDOk = TRUE;
		}
		else if(!strcmp(argv[i], "-p") && ++i < argc) // Pass phrase
		{
		  ZeroMemory(PassPhrase, sizeof(PassPhrase));
		  strcpy(PassPhrase, argv[i]);
		  if(strlen(PassPhrase) < 4 || strlen(PassPhrase) > 8)
		  {
		    Usage(1);
		  }
		  bPassPhraseOk = TRUE;
		}
		else if(!strcmp(argv[i], "-m") && ++i < argc) // encrypt type
		{
		  if(!strcmp(argv[i], "simple"))
		  {
		    ce.Method  = NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE;
		    bTypeOk = TRUE;
		  }
		  else if(!strcmp(argv[i], "aes"))
		  {
		    ce.Method  = NDAS_CONTENT_ENCRYPT_METHOD_AES;
		    bTypeOk = TRUE;
		  }
		  else
		  {
  		  Usage(2);
		  }
		}
		else if(!strcmp(argv[i], "-l") && ++i < argc) // encrypt key length
		{
    	int iEncryptKeyLength;
		  if(1 != sscanf(argv[i], "%d", &iEncryptKeyLength))
		    Usage(3);
		  ce.KeyLength = (unsigned _int16)iEncryptKeyLength;
		  bLengthOk = TRUE; // key length will be checked later
		}
	}

  if(!bCommandOk) Usage(0);
  if(!bPassPhraseOk) Usage(1);
  if(!bTypeOk) Usage(2);
  if(!bLengthOk) Usage(3);
  if(!bIDOk) Usage(4);

  switch(ce.Method)
  {
    case NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE:
      if(32 != ce.KeyLength)
        Usage(6);
      break;
    case NDAS_CONTENT_ENCRYPT_METHOD_AES:
      if(
        128 != ce.KeyLength &&
        192 != ce.KeyLength &&
        256 != ce.KeyLength)
        Usage(6);
      break;
    default:
      Usage(2);
  }

  // encrypt key
  md5_context ctx;
  for(i = 0; i < 4 /* NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH / 16 */; i++)
  {
    md5_starts(&ctx);
    for(j = 0; j < i +1; j++)
    {
      md5_update(&ctx, (uint8 *)PassPhrase, strlen(PassPhrase));
    }
    md5_finish(&ctx, (unsigned char*)&ce.Key[i * 16]);
  }

  // clear remain bits
  for(i = ce.KeyLength / 8; i < sizeof(ce.Key); i++)
  {
	  ce.Key[i] = 0;
  }

  // create crc32
  ce.CRC32 = crc32_calc((PBYTE)&ce, sizeof(ce.bytes_508));

#ifdef _DEBUG
  // show encrypted key
  for(i = 0; i < 4; i++)
  {
	  for(j = 0; j < 16; j++)
		  printf("%02x ", ce.Key[j + i * 16]);
	  printf("\n");
  }
#endif

	HNDAS hNDAS;
	printf("Connecting to the NDAS Device\n");
	hNDAS = NdasCommConnect(&ci);
	if(NULL == hNDAS)
	{
		printf("Error : Failed to connect to the NDAS Device\n");
		ShowErrorMessage(TRUE);
	}

	printf("Writing encrypt key to the NDAS Device\n");
	bResults = NdasCommBlockDeviceWriteSafeBuffer(hNDAS, NDAS_BLOCK_LOCATION_ENCRYPT, 1, (PCHAR)&ce);
	if(NULL == bResults)
	{
		printf("Error : Failed to write key to NDAS Device\n");
		ShowErrorMessage(TRUE);
	}

	printf("Disconnecting from the NDAS Device\n");
	bResults = NdasCommDisconnect(hNDAS);
	if(NULL == bResults)
	{
		printf("Error : Failed to close connection from the NDAS Device\n");
		ShowErrorMessage(TRUE);
	}

	printf("Encryption key is written to the NDAS Device successfully\n");
	return 0;
}
