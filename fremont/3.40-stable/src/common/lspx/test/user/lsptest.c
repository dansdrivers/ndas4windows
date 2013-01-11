#include <stdio.h>
#include <stdlib.h>
#include <lspx/lsp.h>
#include <lspx/lsp_util.h>
#include "lsptest.h"

#if defined(USE_CURSES)
#if defined(_MSC_VER)
#include <conio.h>
#else
#include <curses.h>
#endif /* _MSC_VER */
#endif /* USE_CURSES */

#if !defined(_MSC_VER)
#define _getch getch
#define _strtoui64 strtoull
#endif /* _MSC_VER */

int batch_mode = 0;

/*
const struct {
	const char* cmd;
	int (*proc)(lsptest_context_t*);
} cmdefs[] = {
	// "read # #", lsptest_read,
	"login", lspsh_login,
	"logout", lspsh_logout,
	"handshake", lspsh_ata_handshake,
	"read", lspsh_ata_read,
	"write", lspsh_ata_write,
	"verify", lspsh_ata_verify,
	"identify", lspsh_ata_identify,
	"identify-packet-device", lspsh_ata_identify_packet_device,
	"check-power-mode", lspsh_ata_check_power_mode,
	"idle", lspsh_ata_idle,
	"idle-immediate", lspsh_ata_idle_immediate,
	"standby", lspsh_ata_standby,
	"standby-immediate", lspsh_ata_standby_immediate,
	"read-native-max-address", lsptest_read_native_max_address,
	"read-native-max-address-ext", lsptest_read_native_max_address_ext,
};
*/

int lsptest_parse_bytes(char* arg, unsigned char* buf, size_t buflen);

int lsptest_run_cmds(lsptest_context_t* context)
{
	int ret, ret2, fc;
	lsp_login_info_t lsp_login_info;
	void* session_buffer;

	lsp_large_integer_t location = {0};

	char line[1024];
	const char* seps = " \t\r\n";
	char* s;
	char* token;

	FILE* in;

	in = stdin;

	session_buffer = malloc(LSP_SESSION_BUFFER_SIZE);

	if (NULL == session_buffer)
	{
		printf("error: out of memory\n");
		return 1;
	}

	context->lsp_handle = lsp_initialize_session(
		session_buffer, LSP_SESSION_BUFFER_SIZE);

	for (;;)
	{
		if (!batch_mode) fprintf(stderr, "lsp> ");

#if defined(USE_CURSES)
		s = line;
		*s = (char)_getch();
		while (1)
		{
			if ('\r' == *s || '\n' == *s)
			{
				fprintf(stderr, "%s\n", 'r' == *s ? "<cr>" : "<lf>");
				// putc(*s,stderr);
				*s = 0;
				break;
			}
			else if ('\004' == *s)
			{
				*(++s) = 0;
				break;
			}
			putc(*s,stderr);
			s++;
			*s = (char)_getch();
		}
#else
		s = fgets(line, 1024, in);
#endif

		if (NULL == s)
		{
			fprintf(stdout, "\n");
			break;
		}

		token = strtok(line, seps);

		if (NULL == token)
		{
			/* empty line */
			continue;
		}
		else if (0 == strcmp(token, "//"))
		{
			/* line comment */
			continue;
		}
		else if (token[0] == '/' && token[1] == '/')
		{
			/* line comment */
			continue;
		}
		else if (0 == strcmp(token, "login"))
		{
			/* login [discover|normal] [ro|rw] [unit_no]*/
			memset(&lsp_login_info, 0, sizeof(lsp_login_info_t));

			/* default login values */
			memcpy(lsp_login_info.password, LSP_LOGIN_PASSWORD_ANY, 8);
			lsp_login_info.login_type = LSP_LOGIN_TYPE_NORMAL;
			lsp_login_info.write_access = 0;
			lsp_login_info.unit_no = 0;

			token = strtok(NULL, seps);
			if (NULL != token)
			{
				if (0 == strcmp("discover", token))
				{
					lsp_login_info.login_type = LSP_LOGIN_TYPE_DISCOVER;
					token = strtok(NULL, seps);
				}
				else if (0 == strcmp("normal", token))
				{
					lsp_login_info.login_type = LSP_LOGIN_TYPE_NORMAL;
					token = strtok(NULL, seps);
				}
			}

			if (NULL != token)
			{
				if (0 == strcmp("rw", token))
				{
					lsp_login_info.write_access = 1;
					token = strtok(NULL, seps);
				}
				else if (0 == strcmp("ro", token))
				{
					lsp_login_info.write_access = 0;
					token = strtok(NULL, seps);
				}
			}

			if (NULL != token)
			{
				lsp_login_info.unit_no = (lsp_uint8_t)strtol(token, NULL, 0);
			}

			fprintf(stdout, "login %s %s %d\n",
				lsp_login_info.login_type == LSP_LOGIN_TYPE_NORMAL ? 
				"normal" : "discover",
				lsp_login_info.write_access ?
				"rw" : "ro",
				lsp_login_info.unit_no);

			ret = lsptest_login(context, &lsp_login_info);
			if (0 != ret)
			{
				fprintf(stderr, "error: login failed.\n");

				context->lsp_handle = lsp_initialize_session(
					session_buffer, LSP_SESSION_BUFFER_SIZE);
			}
		}
		else if (0 == strcmp(token, "logout"))
		{
			ret = lsptest_logout(context);
			if (0 != ret)
			{
				fprintf(stderr, "error: logout failed.\n");
			}
		}
		else if (0 == strcmp(token, "identify"))
		{
			ret = lspsh_ata_identify(context);
		}
		else if (0 == strcmp(token, "handshake"))
		{
			ret = lspsh_ata_handshake(context);
		}
		else if (0 == strcmp(token, "check_power_mode"))
		{
			ret = lspsh_ata_check_power_mode(context);
		}
		else if (0 == strcmp(token, "device_reset"))
		{
			ret = lspsh_ata_device_reset(context);
		}
		else if (0 == strcmp(token, "execute_device_diagnostic"))
		{
			ret = lspsh_ata_execute_device_diagnostic(context);
		}
		else if (0 == strcmp(token, "flush_cache"))
		{
			ret = lspsh_ata_flush_cache(context);
		}
		else if (0 == strcmp(token, "flush_cache_ext"))
		{
			ret = lspsh_ata_flush_cache_ext(context);
		}
		else if (0 == strcmp(token, "read_mbr"))
		{
			lsp_int64_t lba = 0;
			const char* usage = "read_mbr [lba:0]";

			token = strtok(NULL, seps);
			if (NULL != token)
			{
				lba = _strtoui64(token, NULL, 0);
			}

			lsptest_read(
				context, 
				(lsp_large_integer_t*) &lba, 
				NULL, 
				1, 
				lspsh_read_mbr);
		}
		else if (0 == strcmp(token, "read"))
		{
			lsp_int64_t lba = 0;
			lsp_int32_t sectors = 1;
			char* fp;
			const char* usage = "read <lba> [sectors:1] [output-file]";
			char* filename = NULL;
			FILE* outfile = stdout;

			token = strtok(NULL, seps);
			if (NULL != token)
			{
				lba = _strtoui64(token, NULL, 0);
			}
			if (NULL == token || (0 == lba && token[0] != '0'))
			{
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			token = strtok(NULL, seps);
			if (NULL != token)
			{
				sectors = strtol(token, NULL, 0);
				if (0 == sectors)
				{
					fprintf(stderr, "error: sector count is not a valid number\n");
					fprintf(stderr, "%s\n", usage);
					continue;
				}
			}

			token = strtok(NULL, seps);
			if (NULL != token)
			{
				filename = token;
				outfile = fopen(filename, "wb");
				if (NULL == outfile)
				{
					fprintf(stderr, "error: fopen failed\n");
					continue;
				}
			}

#if defined(_MSC_VER)
			fprintf(stderr, "reading %d sector(s), LBA=%I64d (%I64Xh) to %s\n", 
				sectors, lba, lba, filename ? filename : "stdout");
#else
			fprintf(stderr, "reading %d sector(s), LBA=%lld (%llXh) to %s\n", 
				sectors, lba, lba, filename ? filename : "stdout");
#endif
			lsptest_read(
				context, 
				(lsp_large_integer_t*) &lba, 
				outfile, 
				sectors, 
				(stdout == outfile) ? lspsh_read_text : lspsh_read_bin);

			if (stdout != outfile)
			{
				fclose(outfile);
			}
		}
		else if (0 == strcmp(token, "write"))
		{
			lsp_int64_t lba = 0;
			lsp_int32_t sectors = 0;
			char* fp;
			const char* usage = "write <lba> <sectors> <input-file>";
			char* filename = NULL;
			FILE* infile = NULL;

			token = strtok(NULL, seps);
			if (NULL == token)
			{
				fprintf(stderr, "%s\n", usage);
				continue;
			}
			lba = _strtoui64(token, NULL, 0);
			if (0 == lba && token[0] != '0')
			{
				fprintf(stderr, "error: lba is not a valid number\n");
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			token = strtok(NULL, seps);
			if (NULL == token)
			{
				fprintf(stderr, "error: sector count is not specified.\n");
				fprintf(stderr, "%s\n", usage);
				continue;
			}
			sectors = strtol(token, NULL, 0);
			if (0 == sectors)
			{
				fprintf(stderr, "error: sector count is not a valid number\n");
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			token = strtok(NULL, seps);
			if (NULL == token)
			{
				fprintf(stderr, "error: filename is not specified.\n");
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			filename = token;
			if (0 == strcmp("0", filename))
			{
				infile = NULL;
			}
			else
			{
				infile = fopen(filename, "rb");
				if (NULL == infile)
				{
					fprintf(stderr, "error: fopen failed\n");
					continue;
				}
			}

			ret = lsptest_write(
				context, 
				(lsp_large_integer_t*)&lba,
				infile,
				sectors);

			if (NULL != infile)
			{
				fclose(infile);
			}
		}
		else if (0 == strcmp(token, "read_native_max_address"))
		{
			ret = lsptest_read_native_max_address(context);
		}
		else if (0 == strcmp(token, "read_native_max_address_ext"))
		{
			ret = lsptest_read_native_max_address_ext(context);
		}
		else if (0 == strcmp(token, "set_feature_disable_automatic_acoustic_management"))
		{
			ret = lsptest_set_feature_disable_automatic_acoustic_management(context);
		}
		else if (0 == strcmp(token, "set_feature_enable_automatic_acoustic_management"))
		{
			ret = lsptest_set_feature_enable_automatic_acoustic_management(context, 0x80);
		}
		else if (0 == strcmp(token, "sleep"))
		{
			ret = lspsh_ata_sleep(context);
		}
		else if (0 == strcmp(token, "smart_return_status"))
		{
			ret = lsptest_smart_return_status(context);
		}
		else if (0 == strcmp(token, "smart_read_data"))
		{
			ret = lsptest_smart_read_data(context);
		}
		else if (0 == strcmp(token, "smart_read_log"))
		{
			ret = lsptest_smart_read_log(context);
		}
		else if (0 == strcmp(token, "standby"))
		{
			ret = lspsh_ata_standby(context);
		}
		else if (0 == strcmp(token, "standby_immediate"))
		{
			ret = lspsh_ata_standby_immediate(context);
		}
		else if (0 == strcmp(token, "identify_packet_device"))
		{
			ret = lspsh_ata_identify_packet_device(context);
		}
		else if (0 == strcmp(token, "idle"))
		{
			ret = lspsh_ata_idle(context);
		}
		else if (0 == strcmp(token, "idle_immediate"))
		{
			ret = lspsh_ata_idle_immediate(context);
		}
		else if (0 == strcmp(token, "text_target_list"))
		{
			ret = lspsh_text_target_list(context);
		}
		else if (0 == strcmp(token, "text_target_data"))
		{
			static const char* usage = 
				"text_target_data read\n"
				"                 write <00:00:00:00:00:00:00:00>";
			int write = 0;
			unsigned char data[8];

			memset(data, 0, sizeof(data));
			token = strtok(NULL, seps);
			if (NULL == token)
			{
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			if (0 == strcmp(token, "read"))
			{
				write = 0;
			}
			else if (0 == strcmp(token, "write"))
			{
				write = 1;
				token = strtok(NULL, seps);
				if (NULL == token)
				{
					fprintf(stderr, "error: write requires 8 byte data\n");
					fprintf(stderr, "%s\n", usage);
					continue;
				}
				else
				{
					if (-1 == lsptest_parse_bytes(token, data, sizeof(data)))
					{
						fprintf(stderr, "error: write requires 8 byte data\n");
						fprintf(stderr, "%s\n", usage);
						continue;
					}
				}
			}
			else
			{
				fprintf(stderr, "%s\n", usage);
				continue;
			}

			lspsh_text_target_data(context, write, data);
		}
		else if (0 == strcmp(token, "\004") || 
			0 == strcmp(token, "q") || 
			0 == strcmp(token, "quit") || 
			0 == strcmp(token, "exit"))
		{
			break;
		}
		else if (0 == strcmp(token, "help"))
		{
			fprintf(stderr, "help...\n");
		}
		else
		{
			fprintf(stderr, "unrecognized command '%s', use 'help.\n", token);
		}
	}

	if (NULL == s)
	{
		if (0 != ferror(in))
		{
			fprintf(stderr, "error: input stream read failed\n");
		}
	}

	free(session_buffer);

	return ret;
}

/*
 * returns the value of the char represented in hexadecimal.
 * this function returns the value between 0 to 15 if c is valid,
 * otherwise returns -1.
 */
int parse_hex_numchar(char c)
{
	if (c >= '0' && c <= '9') return c - '0' + 0x0;
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xa;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xa;
	return -1;
}

/*
 * parses arg of which a form is '00:0b:d0:01:6b:76', where
 * ':' may be substituted with '-' or omitted.
 * addr must be a pointer to a buffer holding at least 6 bytes
 * returns 0 if successful, otherwise returns -1
 */
/*
 * parses arg of which a form is '00:0b:d0:01:6b:76', where
 * ':' may be substituted with '-' or omitted.
 * addr must be a pointer to a buffer holding at least 6 bytes
 * returns 0 if successful, otherwise returns -1
 */
int lsptest_parse_bytes(char* arg, unsigned char* buf, size_t buflen)
{
	size_t i;
	int u, l;
	char* p;
	
	p = arg;
	for (i = 0; i < buflen; ++i)
	{
		u = *p ? parse_hex_numchar(*p++) : -1;
		if (-1 == u) return -1;

		l = *p ? parse_hex_numchar(*p++) : -1;
		if (-1 == l) return -1;

		buf[i] = u * 0x10 + l;

		if (*p == ':' || *p == '-') ++p;
	}
	return 0;
}

int lsptest_run(int argc, char** argv)
{
	static const char* usage = "usage: lsptest [device-address]";
	lsptest_context_t* context;
	int ret;

	lsp_uint8_t ndas_dev_addr[6];

	if (argc < 2)
	{
		fprintf(stderr, "%s\n", usage);
		return 1;
	}

	if (0 != lsptest_parse_bytes(argv[1], ndas_dev_addr, 6))
	{
		fprintf(stderr, "error: target address format is invalid.\n");
		return 1;
	}

	context = lsptest_transport_create();
	if (0 == context)
	{
		fprintf(stderr, "error: transport creation failed.");
		return -1;
	}

	ret = lsptest_transport_connect(context, ndas_dev_addr);
	if (0 != ret)
	{
		fprintf(stderr, "error: connection failed.");
		return -1;
	}

	ret = lsptest_run_cmds(context);

	lsptest_transport_disconnect(context);

	return ret;
}

#if defined(_MSC_VER)
int __cdecl main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
	int ret;

	ret = lsptest_transport_static_initialize();

	if (0 != ret)
	{
		fprintf(stderr, "error: transport initialization failed.\n");
		return ret;
	}

	ret = lsptest_run(argc, argv);

	lsptest_transport_static_cleanup();

	return ret;
}
