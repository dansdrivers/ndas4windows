#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lspx/lsp_hash.h>

#if defined(_MSC_VER)
#include <windows.h>
#endif /* _MSC_VER */

unsigned int checksum(
	lsp_uint8_t* buf,
	lsp_uint32_t bufsize)
{
	unsigned int sum = 0;
	unsigned int i;

	for (i = 0; i < bufsize; ++i)
	{
		sum += buf[i];
	}
	return sum;
}

int test(
	const lsp_uint8_t* refbuf, 
	lsp_uint32_t bufsize,
	lsp_uint32_t key,
	lsp_uint8_t* pwd)
{
	const lsp_uint8_t* src;
	lsp_uint8_t* dst, *bufe, *bufd;
	lsp_uint32_t ckey;
	int i;

	bufe = malloc(bufsize);
	bufd = malloc(bufsize);
	if (0 == bufe || 0 == bufd) 
	{
		free(bufd);
		free(bufe);
		fprintf(stderr, "error: out of memory\n");
		return -1;
	}

	lsp_encrypt32_build_combined_key(&ckey, key, pwd);

	dst = bufe;
	src = refbuf;

	/* lsp_encrypt32 */
	memcpy(dst, src, bufsize);

	lsp_encrypt32(dst, bufsize, key, pwd);

	fprintf(stdout, "%-20s: %08X\n", 
		"lsp_encrypt32", checksum(dst, bufsize));

	/* lsp_encrypt32ex */
	memcpy(dst, src, bufsize);

	lsp_encrypt32ex(dst, bufsize, ckey);

	fprintf(stdout, "%-20s: %08X\n", 
		"lsp_encrypt32ex", checksum(dst, bufsize));

	/* lsp_encrypt32exx */
	memcpy(dst, src, bufsize);

	lsp_encrypt32exx(dst, bufsize, ckey);

	fprintf(stdout, "%-20s: %08X\n", 
		"lsp_encrypt32exx", checksum(dst, bufsize));

	/* decryption test */

	dst = bufd;
	src = bufe;

	lsp_decrypt32_build_combined_key(&ckey, key, pwd);

	/* lsp_decrypt32 */
	memcpy(dst, src, bufsize);

	lsp_decrypt32(dst, bufsize, key, pwd);

	fprintf(stdout, "%-20s: %08X - should_be_zero=%d\n", 
		"lsp_decrypt32", checksum(dst, bufsize), memcmp(dst, refbuf, bufsize));

	/* lsp_decrypt32ex */
	memcpy(dst, src, bufsize);

	lsp_decrypt32ex(dst, bufsize, ckey);

	fprintf(stdout, "%-20s: %08X - should_be_zero=%d\n", 
		"lsp_decrypt32ex", checksum(dst, bufsize), memcmp(dst, refbuf, bufsize));

	/* lsp_decrypt32exx */
	memcpy(dst, src, bufsize);

	lsp_decrypt32exx(dst, bufsize, ckey);

	fprintf(stdout, "%-20s: %08X - should_be_zero=%d\n", 
		"lsp_decrypt32exx", checksum(dst, bufsize), memcmp(dst, refbuf, bufsize));

	free(bufd);
	free(bufe);

	return 0;
}

int
#ifdef _MSC_VER
__cdecl
#endif
main()
{
	lsp_uint32_t i;
	lsp_uint32_t bufsize;
	lsp_uint8_t* buf;
	lsp_uint32_t key;
	lsp_uint8_t pwd[8] = {0,1,2,3,4,5,6,7};

	key = 0x1A2B3C4D;
	bufsize = 64 * 1024 * 1024; /* 64 KB */
	buf = malloc(bufsize);
	if (0 == buf)
	{
		fprintf(stderr, "error: out of memory\n");
		return -1;
	}
	/* fill the reference buffer */
	for (i = 0; i < bufsize; i += 4)
	{
		*(lsp_uint32_t*)&buf[i] = rand();
	}
	test(buf, bufsize, key, pwd);
	free(buf);
	return 0;
}
