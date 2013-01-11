#include <stdio.h>
#include <windows.h>
#include <wincrypt.h>
#include <crtdbg.h>

#pragma comment(lib, "crypt32.lib")

#define MY_ENCODING_TYPE  (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)

void MyHandleError(char *s);

void main()
{

	// Encrypt data from DATA_BLOB DataIn to DATA_BLOB DataOut.
	// Then decrypt to DATA_BLOB DataVerify.

	//--------------------------------------------------------------------
	// Declare and initialize variables.

	DATA_BLOB Entropy;
	BYTE *pbEntropy = (BYTE*)L"0097357949";
	DWORD cbEntropy = (wcslen((WCHAR*)pbEntropy) + 1 )* sizeof(WCHAR);
	Entropy.pbData = pbEntropy;
	Entropy.cbData = cbEntropy;

	Entropy.pbData = NULL;
	Entropy.cbData = 0;

	DATA_BLOB DataIn;
	DATA_BLOB DataOut;
	DATA_BLOB DataVerify;
	BYTE *pbDataInput =(BYTE *)L"000bd00086eb";
	DWORD cbDataInput = (wcslen((WCHAR*)pbDataInput) + 1) * sizeof(WCHAR);
	DataIn.pbData = pbDataInput;    
	DataIn.cbData = cbDataInput;
	CRYPTPROTECT_PROMPTSTRUCT PromptStruct;
	LPWSTR pDescrOut = NULL;

	//--------------------------------------------------------------------
	//  Begin processing.

	printf("The data to be encrypted is: %ws\n",pbDataInput);


	//--------------------------------------------------------------------
	//  Begin protect phase.

	WCHAR szDescr[] = L"";
	if(CryptProtectData(
		&DataIn,
		szDescr, // A description string. 
		&Entropy,                               // Optional entropy not used.
		NULL,                               // Reserved.
		NULL,                      // Pass a PromptStruct.
		NULL, // CRYPTPROTECT_LOCAL_MACHINE, // | CRYPTPROTECT_UI_FORBIDDEN,
		&DataOut))
	{
		printf("The encryption phase worked. \n");
	}
	else
	{
		MyHandleError("Encryption error!");
	}

	DataOut.cbData;
	DataOut.pbData;

	for (DWORD i = 0; i < DataOut.cbData; ++i) {
		register WCHAR wc[3];

		register WCHAR* pwc;
		pwc = &wc[0];
		register BYTE b;

		b = DataOut.pbData[i] >> 4;
		{
			if (b >= 0x0 && b <= 0x9) {
				*pwc = WCHAR(CHAR(b + '0'));
			} else if (b >= 0xA && b <= 0xF) {
				*pwc = WCHAR(CHAR(b  - 0xA + 'A'));
			}
		}

		pwc = &wc[1];
		b = DataOut.pbData[i] & 0xF;
		{
			if (b >= 0x0 && b <= 0x9) {
				*pwc = WCHAR(CHAR(b + '0'));
			} else if (b >= 0xA && b <= 0xF) {
				*pwc = WCHAR(CHAR(b  - 0xA + 'A'));
			}
		}
		wc[2] = L'\0';

		printf("%ws%c", wc, (i > 0 && ((i + 1)% 20 == 0)) ? '\n' : ' ');
	}

	printf("\n");

//	DataOut.pbData[0] = 0;
	//-----------------------------------------------------------------
	//   Begin unprotect phase.

	if (CryptUnprotectData(
		&DataOut,
		NULL,
		NULL,                 // Optional entropy
		NULL,                 // Reserved
		NULL,        // Optional PromptStruct
		0,
		&DataVerify))
	{
		printf("The decrypted data is: %ws\n", DataVerify.pbData);
		printf("The description of the data was: %S\n",pDescrOut);
	}
	else
	{
		MyHandleError("Decryption error!");
	}
	//-------------------------------------------------------------------
	// At this point, memcmp could be used to compare DataIn.pbData and 
	// DataVerify.pbDate for equality. If the two functions worked
	// correctly, the two byte strings are identical. 

	//-------------------------------------------------------------------
	//  Clean up.

	LocalFree(pDescrOut);
	LocalFree(DataOut.pbData);
	LocalFree(DataVerify.pbData);
} // End of main

//--------------------------------------------------------------------
//  This example uses the function MyHandleError, a simple error
//  handling function, to print an error message to the standard error 
//  (stderr) file and exit the program. 
//  For most applications, replace this function with one 
//  that does more extensive error reporting.

void MyHandleError(char *s)
{
	fprintf(stderr,"An error occurred in running the program. \n");
	fprintf(stderr,"%s\n",s);
	fprintf(stderr, "Error number %d (0x%08X).\n", GetLastError(), GetLastError());
	fprintf(stderr, "Program terminating. \n");
	exit(1);
} // End of MyHandleError
