//	huffman.c
//	huffman encoding/decoding for use in hexenworld networking

#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "compiler.h"

#if defined(DEBUG_BUILD) && !defined(_WIN32)
#define OutputDebugString(X) fprintf(stderr,"%s",(X))
#endif


extern void Sys_Error (const char *error, ...) _FUNC_PRINTF(1);

//
// huffman types and vars
//

typedef struct huffnode_s
{
	struct huffnode_s *zero;
	struct huffnode_s *one;
	unsigned char	val;
	float		freq;
} huffnode_t;

typedef struct
{
	unsigned int	bits;
	int		len;
} hufftab_t;

static huffnode_t *HuffTree = 0;
static hufftab_t HuffLookup[256];

static float HuffFreq[256] =
{
#	include "hufffreq.h"
};


//=============================================================================

//
// huffman debugging
//
#ifdef DEBUG_BUILD
int HuffIn = 0;
int HuffOut= 0;
static int freqs[256];

void ZeroFreq (void)
{
	memset(freqs, 0, 256*sizeof(int));
}

void CalcFreq (unsigned char *packet, int packetlen)
{
	int		ix;

	for (ix = 0; ix < packetlen; ix++)
	{
		freqs[packet[ix]]++;
	}
}

void PrintFreqs (void)
{
	int		ix;
	float	total = 0;
	char	string[100];

	for (ix = 0; ix < 256; ix++)
	{
		total += freqs[ix];
	}

	if (total > .01)
	{
		for (ix = 0; ix < 256; ix++)
		{
			sprintf(string, "\t%.8f,\n", ((float)freqs[ix])/total);
			OutputDebugString(string);
		}
	}

	ZeroFreq();
}
#endif	// DEBUG_BUILD


//=============================================================================

//
// huffman functions
//

static void FindTab (huffnode_t *tmp, int len, unsigned int bits)
{
	if (!tmp)
		Sys_Error("no huff node");

	if (tmp->zero)
	{
		if (!tmp->one)
			Sys_Error("no one in node");
		if (len >= 32)
			Sys_Error("compression screwd");
		FindTab (tmp->zero, len+1, bits<<1);
		FindTab (tmp->one, len+1, (bits<<1)|1);
		return;
	}

	HuffLookup[tmp->val].len = len;
	HuffLookup[tmp->val].bits = bits;
	return;
}

static unsigned char Masks[8] =
{
	0x1,
	0x2,
	0x4,
	0x8,
	0x10,
	0x20,
	0x40,
	0x80
};

static void PutBit (unsigned char *buf, int pos, int bit)
{
	if (bit)
		buf[pos/8] |= Masks[pos%8];
	else
		buf[pos/8] &=~Masks[pos%8];
}

static int GetBit (unsigned char *buf, int pos)
{
	if (buf[pos/8] & Masks[pos%8])
		return 1;
	else
		return 0;
}

static void BuildTree (float *freq)
{
	float	min1, min2;
	int	i, j, minat1, minat2;
	huffnode_t	*work[256];
	huffnode_t	*tmp;

	for (i = 0; i < 256; i++)
	{
		work[i] = (huffnode_t *) malloc(sizeof(huffnode_t));
		work[i]->val = (unsigned char)i;
		work[i]->freq = freq[i];
		work[i]->zero = 0;
		work[i]->one = 0;
		HuffLookup[i].len = 0;
	}

	for (i = 0; i < 255; i++)
	{
		minat1 = -1;
		minat2 = -1;
		min1 = 1E30;
		min2 = 1E30;

		for (j = 0; j < 256; j++)
		{
			if (!work[j])
				continue;
			if (work[j]->freq < min1)
			{
				minat2 = minat1;
				min2 = min1;
				minat1 = j;
				min1 = work[j]->freq;
			}
			else if (work[j]->freq < min2)
			{
				minat2 = j;
				min2 = work[j]->freq;
			}
		}
		if (minat1 < 0)
			Sys_Error("minatl: %d", minat1);
		if (minat2 < 0)
			Sys_Error("minat2: %d", minat2);
		tmp = (huffnode_t *) malloc(sizeof(huffnode_t));
		tmp->zero = work[minat2];
		tmp->one = work[minat1];
		tmp->freq = work[minat2]->freq + work[minat1]->freq;
		tmp->val = 0xff;
		work[minat1] = tmp;
		work[minat2] = 0;
	}

	HuffTree = tmp;
	FindTab (HuffTree, 0, 0);

#if DEBUG_BUILD
	for (i = 0; i < 256; i++)
	{
		if (!HuffLookup[i].len && HuffLookup[i].len <= 32)
		{
		//	Con_Printf("%d %d %2X\n", HuffLookup[i].len, HuffLookup[i].bits, i);
			Sys_Error("bad frequency table");
		}
	}
#endif
}

void HuffDecode (unsigned char *in, unsigned char *out, int inlen, int *outlen, const int maxlen)
{
	int	bits,tbits;
	huffnode_t	*tmp;

	if (*in == 0xff)
	{
		memcpy (out, in+1, inlen-1);
		*outlen = inlen-1;
		return;
	}

	tbits = (inlen-1)*8 - *in;
	bits = 0;
	*outlen = 0;

	while (bits < tbits)
	{
		tmp = HuffTree;
		do
		{
			if ( GetBit(in+1, bits) )
				tmp = tmp->one;
			else
				tmp = tmp->zero;
			bits++;
		} while (tmp->zero);

		if ( ++(*outlen) > maxlen )
			return;
		*out++ = tmp->val;
	}
}

void HuffEncode (unsigned char *in, unsigned char *out, int inlen, int *outlen)
{
	int	i, j, bitat;
	unsigned int	t;
#ifdef DEBUG_BUILD
	unsigned char	*buf;
	int	tlen;
#endif	// DEBUG_BUILD

	bitat = 0;

	for (i = 0; i < inlen; i++)
	{
		t = HuffLookup[in[i]].bits;
		for (j = 0; j < HuffLookup[in[i]].len; j++)
		{
			PutBit (out+1, bitat + HuffLookup[in[i]].len-j-1, t&1);
			t >>= 1;
		}
		bitat += HuffLookup[in[i]].len;
	}

	*outlen = 1 + (bitat + 7)/8;
	*out = 8 * ((*outlen)-1) - bitat;

	if (*outlen >= inlen+1)
	{
		*out = 0xff;
		memcpy (out+1, in, inlen);
		*outlen = inlen+1;
	}

#ifdef DEBUG_BUILD
	HuffIn += inlen;
	HuffOut += *outlen;

	buf = malloc(inlen);
	HuffDecode (out, buf, *outlen, &tlen, inlen);
	if (tlen != inlen)
		Sys_Error("bogus compression");
	for (i = 0; i < inlen; i++)
	{
		if (in[i] != buf[i])
			Sys_Error("bogus compression");
	}
	free (buf);
#endif	// DEBUG_BUILD
}

void HuffInit (void)
{
	BuildTree(HuffFreq);
}

