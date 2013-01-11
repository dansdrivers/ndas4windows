#include <windows.h>
#include <tchar.h>
#include <crtdbg.h>

#include "xcrypt_encrypt.h"
#include "xcrypt_hash.h"

// {AA5DCB92-D599-4b4c-A7AD-4DA2882D61FF}
static const GUID TEST_KEY = 
{ 0xaa5dcb92, 0xd599, 0x4b4c, { 0xa7, 0xad, 0x4d, 0xa2, 0x88, 0x2d, 0x61, 0xff } };

// {4EB25CD1-A47C-468d-BA62-1315A236ED86}
static const GUID  TEST_DATA = 
{ 0x4eb25cd1, 0xa47c, 0x468d, { 0xba, 0x62, 0x13, 0x15, 0xa2, 0x36, 0xed, 0x86 } };

int __cdecl _tmain(int argc, TCHAR** argv)
{
	xcrypt::IHashing* pHashing = xcrypt::CreateCryptAPIMD5Hashing();
	BOOL fSuccess = pHashing->Initialize();
	_ASSERTE(fSuccess);

	pHashing->HashData((CONST BYTE*)&TEST_DATA, sizeof(TEST_DATA));
	DWORD cbHash; // 16
	LPBYTE lpHash = pHashing->GetHashValue(&cbHash);

	_tprintf(_T("Hash: %d bytes\n"), cbHash);

	delete pHashing;

	xcrypt::IEncryption* pEncryption = xcrypt::CreateCryptAPIDESEncryption();
	fSuccess = pEncryption->Initialize();
	_ASSERTE(fSuccess);

	BYTE buffer[256] = {0};
	DWORD cbData = sizeof(TEST_DATA) - 1;
	::CopyMemory(buffer, &TEST_DATA, sizeof(TEST_DATA));

	_tprintf(_T("Plain: %d bytes\n"), cbData);

	pEncryption->SetKey((CONST BYTE*)&TEST_KEY, sizeof(TEST_KEY));
	pEncryption->Encrypt(buffer, &cbData, sizeof(buffer));

	_tprintf(_T("Encrypted: %d bytes\n"), cbData);

	delete pEncryption;

	return 0;
}