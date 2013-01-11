#include "lsp_type.h"
#ifdef LSPIMP_DEBUG_UMODE
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include "lsp_debug.h"

void
lsp_call
lsp_debug(const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}

void
lsp_call
lsp_debug_payload(const char* header, const void* buf, size_t len)
{
	static const size_t col = 16;
	static const size_t delimit = 512;
	size_t i, j;

	printf("=====================================================\n");
	if (header)
	{
		printf(" %s (%d,0x%02X bytes)\n", header, len, len);
		printf("-----------------------------------------------------\n");
	}

	if (len == 0)
	{
		return;
	}

	printf("      ");
	for (i = 0; i < col; ++i)
	{
		printf("%02X ", i);
	}
	printf("\n");
	for (i = 0; i < len; i += col)
	{
		if ((i%delimit) == 0)
		{
			printf("-----------------------------------------------------\n");
		}
		printf("%04X: ", i);
		for (j = 0; j < col && (i + j < len); ++j)
		{
			printf("%02X ", ((unsigned char*)buf)[i+j]);
		}
		for (; j < col; ++j)
		{
			printf("   ");
		}
		for (j = 0; j < col && (i + j < len); ++j)
		{
			unsigned char c = ((unsigned char*)buf)[i+j];
			if (!isprint(c)) c = '.';
			printf("%c", c);
		}
		printf("\n");
	}
	printf("-----------------------------------------------------\n");
}

#endif
