/*
	ASCII85 Encoding

	Copyright 2006 Chesong Lee <patria@enterprisent.com>

	http://www.chesong.com/sw/ascii85

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
*/
/*
 * References
 *
 * - 3.3.2 ASCII85Decode Filter, PDF Reference, version 1.6
 *   ( http://www.adobe.com )
 *
 * - ASCII85 Encoding, Binary Encoding, 
 *   ( http://www.morello.co.uk/binaryencoding.pdf )
 */
#include "ascii85.h"

#define DECODE_START ((size_t)-9)
#define DECODE_BOD ((size_t)-8)
#define DECODE_EOD ((size_t)-7)
#define DECODE_END ((size_t)-6)

static const int pow85[] = { 85*85*85*85, 85*85*85, 85*85, 85, 1};

static int is_ascii85char(const char c)
{
	return (c >= '!' /* 33 */ && c <= 'u' /* 117 */);
}

static int is_whitespace(const char c)
{
	return (0 /* NUL */ == c ||
			9 /* HT */ == c || 
			10 /* LF */ == c || 
			12 /* FF */ == c ||
			13 /* CR */ == c || 
			32 /* SP */ == c);
}

size_t decode85(
	const char* p,
	size_t len, 
	decode85_callback_t out, 
	void* context)
{
	unsigned char buf[4];
	size_t i, c;
	unsigned int x;

	c = 0;
	x = 0;

	/* skip preceding whitespace */
	for (i = 0; i < len && is_whitespace(p[i]); ++i) ;

	/* skip BOD marker (optional) */
	if ('<' == p[i])
	{
		for (++i; i < len; ++i)
		{
			if (!is_whitespace(p[i]))
			{
				if ('~' == p[i]) 
				{
					/* BOD marker skip '<~' */
					++i;
					break;
				}
				else
				{
					/* not a BOD marker */
					x += ('<' - '!') * pow85[c++];
					break;
				}
			}
		}
	}

	for (; i < len; ++i)
	{
		/* whitespace */
		if (is_whitespace(p[i])) continue;
		/* z -> !!!!! -> 0 zeros, not in the middle of a group */
		else if ('z' == p[i] && c == 0)
		{
			buf[0] = buf[1] = buf[2] = buf[3] = 0;
			(*out)(buf, 4, context);
		}
		/* legitimate characters */
		else if (is_ascii85char(p[i]))
		{
			x += (p[i] - '!') * pow85[c++];
			if (c == 5)
			{
				buf[0] = (unsigned char)((x & 0xFF000000) >> 24);
				buf[1] = (unsigned char)((x & 0x00FF0000) >> 16);
				buf[2] = (unsigned char)((x & 0x0000FF00) >> 8);
				buf[3] = (unsigned char)((x & 0x000000FF));
				(*out)(buf, 4, context);
				c = 0;
				x = 0;
			}
		}
		/* EOD marker */
		/* BUG: unlike decode85_p, decode85 does not accept 
		   while spaces between markers */
		else if (p[i] == '~' && i + 1 < len && p[i+1] == '>')
		{
			if (c > 0)
			{
				--c;
				x += pow85[c];
				buf[0] = (unsigned char)(0xFF & ((x & 0xFF000000) >> 24));
				buf[1] = (unsigned char)(0xFF & ((x & 0x00FF0000) >> 16));
				buf[2] = (unsigned char)(0xFF & ((x & 0x0000FF00) >> 8));
				buf[3] = (unsigned char)(0xFF & ((x & 0x000000FF)));
				(*out)(buf, c, context);
			}
			return 0;
		}
		/* invalid character */
		else
		{
			return i + 1;
		}
	}
	/* no EOD marker */
	return i + 1;
}

void encode85(
	const unsigned char* p, 
	size_t len, 
	encode85_callback_t out, 
	void* context)
{
	char buf[5];
	size_t i, fglen;
	unsigned int x;

	/* BOD marker */
	buf[0] = '<';
	buf[1] = '~';
	(*out)(buf, 2, context);

	/* other than the final partial group */
	for (i = 0; i + 4 <= len; i += 4) 
	{
		x = (p[i] << 24) | (p[i+1] << 16) | (p[i+2] << 8) | p[i+3];
		if (0 == x) 
		{
			buf[0] = 'z';
			(*out)(buf, 1, context);
		}
		else
		{
			buf[0] = (char)((x / pow85[0]) % 85 + '!');
			buf[1] = (char)((x / pow85[1]) % 85 + '!');
			buf[2] = (char)((x / pow85[2]) % 85 + '!');
			buf[3] = (char)((x / pow85[3]) % 85 + '!');
			buf[4] = (char)((x / pow85[4]) % 85 + '!');
			(*out)(buf, 5, context);
		}
	}
	/* final partial group */
	fglen = len - i;
	if (fglen > 0)
	{
		x = 0;
		if (i < len) x |= p[i++] << 24;
		if (i < len) x |= p[i++] << 16;
		if (i < len) x |= p[i++] << 8;
		if (i < len) x |= p[i++];
		buf[0] = (char)((x / pow85[0]) % 85 + '!');
		buf[1] = (char)((x / pow85[1]) % 85 + '!');
		buf[2] = (char)((x / pow85[2]) % 85 + '!');
		buf[3] = (char)((x / pow85[3]) % 85 + '!');
		buf[4] = (char)((x / pow85[4]) % 85 + '!');
		(*out)(buf, fglen + 1, context);
	}

	/* EOD marker */
	buf[0] = '~';
	buf[1] = '>';
	(*out)(buf, 2, context);
}

static void fill_obuf(encode85_context_t* ec, char c)
{
	ec->obuf[ec->obufp++] = c;
	if (36 == ec->obufp)
	{
		(*ec->out)(ec->obuf, 36, ec->context);
		ec->obufp = 0;
	}
}

static void flush_obuf(encode85_context_t* ec)
{
	if (ec->obufp > 0)
	{
		(*ec->out)(ec->obuf, ec->obufp, ec->context);
		ec->obufp = 0;
	}
}

void encode85_start(
	encode85_context_t* ec, 
	encode85_callback_t out, 
	void* context)
{
	ec->obufp = ec->ibufp = 0;
	ec->out = out;
	ec->context = context;
	/* BOD marker */
	fill_obuf(ec, '<');
	fill_obuf(ec, '~');
}

void encode85_p(
	encode85_context_t* ec, 
	const unsigned char* p, 
	size_t len)
{
	size_t i;
	unsigned int x;
	/* fill out ibuf */
	const unsigned char* ep;
	i = 0;
	for (ep = p + len; p < ep; ++p, ++i)
	{
		ec->ibuf[ec->ibufp++] = *p;
		if (4 == ec->ibufp)
		{
			x = (ec->ibuf[0] << 24) | 
				(ec->ibuf[1] << 16) |
				(ec->ibuf[2] << 8) |
				(ec->ibuf[3]);
			if (0 == x)
			{
				fill_obuf(ec, 'z');
			}
			else
			{
				fill_obuf(ec, (char)(x / pow85[0] % 85 + '!'));
				fill_obuf(ec, (char)(x / pow85[1] % 85 + '!'));
				fill_obuf(ec, (char)(x / pow85[2] % 85 + '!'));
				fill_obuf(ec, (char)(x / pow85[3] % 85 + '!'));
				fill_obuf(ec, (char)(x / pow85[4] % 85 + '!'));
			}
			ec->ibufp = 0;
		}
	}
}

void encode85_end(
	encode85_context_t* ec)
{
	/* final partial group */
	size_t i;
	unsigned int x;
	x = 0;
	for (i = 0; i < ec->ibufp; ++i)
	{
		x |= ec->ibuf[i] << (24 - 8 * i);
	}
	for (i = 0; i < ec->ibufp; ++i)
	{
		fill_obuf(ec, (char)(x / pow85[i] % 85 + '!'));
	}

	/* EOD marker */
	fill_obuf(ec, '~');
	fill_obuf(ec, '>');
	flush_obuf(ec);
}

static void fill_obufd(
	decode85_context_t* dc, 
	unsigned char* p, 
	size_t len)
{
	unsigned char* ep = p + len;
	for (; p < ep; ++p)
	{
		dc->obuf[dc->obufp++] = *p;
		if (36 == dc->obufp)
		{
			(*dc->out)(dc->obuf, 36, dc->context);
			dc->obufp = 0;
		}
	}
}

static void flush_obufd(
	decode85_context_t* dc)
{
	if (dc->obufp > 0)
	{
		(*dc->out)(dc->obuf, dc->obufp, dc->context);
		dc->obufp = 0;
	}
}

void decode85_start(
	decode85_context_t* dc, 
	decode85_callback_t out, 
	void* context)
{
	dc->obufp = 0;
	dc->ibufp = DECODE_START;
	dc->out = out;
	dc->context = context;
}

void decode85_end(
	decode85_context_t *dc)
{
	flush_obufd(dc);
}

size_t decode85_p(
	decode85_context_t *dc, 
	const char *p, 
	size_t len)
{
	unsigned char buf[4];
	size_t i;
	const char *ep;

	for (ep = p + len, i = 0; p < ep; ++p, ++i)
	{
		/* skip whitespace */
		if (is_whitespace(*p)) continue;

		switch (dc->ibufp)
		{
		case DECODE_START:
			/* if DECODE_START and BOD marker < is found set
			 * DECODE_BOD. if < is followed by ~ it is BOD marker and
			 * < is ignored, otherwise < is literally decoded */
			if ('<' == *p)
			{
				dc->x = (*p - '!') * pow85[0];
				dc->ibufp = DECODE_BOD;
				continue;
			}
			else
			{
				/* start normal data without BOD */
				dc->ibufp = 0;
				/* continue to the normal decode */
				break;
			}
		case DECODE_BOD:
			/* second character of the BOD marker, if '~' is
			 * encountered other than this position, it would be an
			 * error except for the EOD marker */
			if ('~' == *p)
			{
				dc->ibufp = 0;
				dc->x = 0;
				continue;
			}
			else
			{
				/* there is no BOD marker, use the buffer */
				dc->ibufp = 1;
				/* continue to the normal decode */
				break;
			}
		case DECODE_EOD:
			/* only '>' can be placed here */
			if ('>' == *p)
			{
				dc->ibufp = DECODE_END;
				continue;
			}
			else
			{
				return i + 1;
			}
		case DECODE_END:
			/* redundant string after eod */
			return i + 1;
		}

		/* legitimate characters */
		if (is_ascii85char(*p))
		{
			dc->x += (*p - '!') * pow85[dc->ibufp++];
			if (5 == dc->ibufp)
			{
				buf[0] = (unsigned char)((dc->x & 0xFF000000) >> 24);
				buf[1] = (unsigned char)((dc->x & 0x00FF0000) >> 16);
				buf[2] = (unsigned char)((dc->x & 0x0000FF00) >> 8);
				buf[3] = (unsigned char)((dc->x & 0x000000FF));
				fill_obufd(dc, buf, 4);
				/* reset ibufp and x */
				dc->ibufp = 0;
				dc->x = 0;
			}
		}
		/* z -> !!!!! -> 4 zeros, not in the middle of a group */
		else if ('z' == *p && dc->ibufp == 0)
		{
			buf[0] = buf[1] = buf[2] = buf[3] = 0;
			fill_obufd(dc, buf, 4);
		}
		/* '~' is legitimate only as a EOD marker */
		else if ('~' == *p)
		{
			/* decode the remaining buffer */
			size_t flen = dc->ibufp;
			buf[0] = (unsigned char)(0xFF & ((dc->x & 0xFF000000) >> 24));
			buf[1] = (unsigned char)(0xFF & ((dc->x & 0x00FF0000) >> 16));
			buf[2] = (unsigned char)(0xFF & ((dc->x & 0x0000FF00) >> 8));
			buf[3] = (unsigned char)(0xFF & ((dc->x & 0x000000FF)));
			fill_obufd(dc, buf, flen);
			/* set EOD position */
			dc->ibufp = DECODE_EOD;
		}
		else
		{
			/* not a valid character error */
			return i + 1; /* 1-based index */
		}
	}

	return 0;
}
