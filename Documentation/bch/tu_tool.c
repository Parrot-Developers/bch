/*
 * BCH library tests
 *
 * Swiss-army knife tool for testing the BCH library.
 * This tool performs encoding/decoding tests on buffers filled with 0xff bytes.
 *
 * Usage: ./tu_tool [OPTIONS]
 *   Available options:
 *   -b <niter>  Benchmark mode: run operation <niter> times
 *   -c <nbits>  Corrupt nbits in data, default=0
 *   -d          Decode data (default is encode)
 *   -g <poly>   Generator polynomial (default is use built-in)
 *   -h          Display this help
 *   -l <size>   Set data size in bytes, default=2^(m-4)
 *   -m <order>  Set Galois field order, default=13
 *   -p b1,b2,.. Corrupt comma-separated list of bits in data
 *   -r <seed>   Corrupt bits with randomized parameters
 *   -s          Encode only once and use cached result
 *   -t <bits>   Set error correction capability, default=4
 *   -v          Verbose mode
 *
 * Copyright (C) 2010 Parrot S.A.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

//#define DEBUG 1

#if defined(DEBUG)
struct gf_poly;
#define dbg(_fmt, args...) fprintf(stderr, _fmt, ##args)
#define gf_poly_str(_poly) gf_poly_dump(_poly, alloca(((_poly)->deg+1)*12))
static char * gf_poly_dump(const struct gf_poly *f, char *buf);
#endif

#include "../../lib/bch.c"

#if defined(DEBUG)
static char * gf_poly_dump(const struct gf_poly *f, char *buf)
{
	int i, pos = 0;
	pos += sprintf(&buf[pos], "((%d)", f->deg);
	for (i = f->deg; i >= 0; i--) {
		pos += sprintf(&buf[pos], " %x", f->c[i]);
	}
	pos += sprintf(&buf[pos], ")");
	return buf;
}
#endif

#define UNLIKELY(_expr) __builtin_expect((_expr), 0)

#define MAX_ERRORS 2048

static int opt_verbose = 0;

static int rev8(int bit)
{
	return (bit & ~7)|(7-(bit & 7));
}

static void generate_error_vector(int len, int *bit, int size,
				  unsigned int seed)
{
	int i, done = 0;
	int compar(const void *a, const void *b) {return *(int*)a - *(int*)b;}

	/* corrupt data */
	srand48(seed);

	while (!done) {
		for (i = 0; i < size; i++) {
			bit[i] = lrand48() % len;
		}
		qsort(bit, size, sizeof(int), compar);
		done = 1;
		for (i = 0; i < size-1; i++) {
			if (bit[i] == bit[i+1]) {
				if ((i > 0) && (bit[i-1]+1 < bit[i])) {
					bit[i]--;
					continue;
				}
				if ((i+1 < size-1) && (bit[i+1]+1 < bit[i+2])) {
					bit[i+1]++;
					continue;
				}
				done = 0;
				break;
			}
		}
	}
	for (i = 0; i < size; i++) {
		bit[i] = rev8(bit[i]);
	}
}

static void dump_errors(int nerrors, unsigned int *errloc)
{
	int i, bit, byte;
	int compar(const void *a, const void *b) {return *(int*)a - *(int*)b;}

	/* sort error locations */
	qsort(errloc, nerrors, sizeof(int), compar);

	for (i = 0; i < nerrors; i++) {
		bit = errloc[i];
		byte = bit/8;
		bit = bit & 7;
		fprintf(stderr, "error in bit %d = data[%d].%d\n",
			byte*8+bit, byte, bit);
	}
}

static void verify_errors(int *bitflip, int ncorrupt, unsigned int *errloc,
			  int nerrors)
{
	int i, bit;
	int compar(const void *a, const void *b) {return *(int*)a - *(int*)b;}

	/* sort error locations */
	qsort(bitflip, ncorrupt, sizeof(int), compar);
	qsort(errloc, nerrors, sizeof(int), compar);

	/* verify error locations */
	for (i = 0; i < ncorrupt; i++) {
		bit = (i < nerrors)? (int)errloc[i] : -1;
		if (bitflip[i] != bit) {
			fprintf(stderr,"mismatch: fixed bit %d instead of %d\n",
				bit, bitflip[i]);
		}
	}
}

static int bitflip[MAX_ERRORS];

static void corrupt_data(int *bitflip, uint8_t *data, int ncorrupt)
{
	int i;
	for (i = 0; i < ncorrupt; i++) {
		data[bitflip[i]/8] ^= 1 << (bitflip[i] & 7);
	}
}

static void generic_decode(struct bch_control *bch, uint8_t *data, int len,
                           unsigned int *errloc, int ncorrupt, int cache_encode)
{
	int i, nerrors, nbits;
	static uint8_t *ecc = NULL;
	static int iteration = 0;

	cache_encode = cache_encode && !ncorrupt;
	nbits = len*8+bch->ecc_bits;
	iteration++;

	if (cache_encode && !ecc) {
		ecc = calloc(1, bch->ecc_bytes);
		assert(ecc);
		encode_bch(bch, data, len, ecc);
		for (i = 0; i < (int)bch->ecc_bytes; i++) {
			ecc[i] ^= data[len+i];
		}
	}

	if (UNLIKELY(ncorrupt)) {
		generate_error_vector(nbits, bitflip, ncorrupt,
				      ncorrupt+data[0]+iteration);
		corrupt_data(bitflip, data, ncorrupt);
	}

	nerrors = decode_bch(bch, data, len,
			     cache_encode? NULL : data+len,
			     cache_encode? ecc : NULL,
			     NULL, errloc);

	if (UNLIKELY(ncorrupt)) {
		/* uncorrupt data */
		corrupt_data(bitflip, data, ncorrupt);
	}

	if (UNLIKELY(nerrors < 0)) {
		fprintf(stderr, "BCH decoding failed !\n");
		return;
	}

	if (UNLIKELY(ncorrupt) && (ncorrupt <= (int)bch->t) &&
	    (nerrors != ncorrupt)) {
		fprintf(stderr,	"BCH decoding failed: %d errors, expected %d\n",
			nerrors, ncorrupt);
		fprintf(stderr, "corrupt=");
		for (i = 0; i < ncorrupt; i++) {
			fprintf(stderr, "%d,", bitflip[i]);
		}
		fprintf(stderr, "\n");
		fprintf(stderr, "errloc=");
		for (i = 0; i < nerrors; i++) {
			fprintf(stderr, "%d,", errloc[i]);
		}
		fprintf(stderr, "\n");
		return;
	}

	if (UNLIKELY(opt_verbose)) {
		dump_errors(nerrors, errloc);
	}

	if (UNLIKELY(ncorrupt)) {
		verify_errors(bitflip, ncorrupt, errloc, nerrors);
	}
}

static void usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n"
		"Available options:\n"
		" -b <niter>  Benchmark mode: run operation <niter> times\n"
		" -c <nbits>  Corrupt nbits in data, default=0\n"
		" -d          Decode data (default is encode)\n"
		" -g <poly>   Generator polynomial (default is use built-in)\n"
		" -h          Display this help\n"
		" -l <size>   Set data size in bytes, default=2^(m-4)\n"
		" -m <order>  Set Galois field order, default=13\n"
		" -p b1,b2,.. Corrupt comma-separated list of bits in data\n"
		" -r <seed>   Corrupt bits with randomized parameters\n"
		" -s          Encode only once and use cached result\n"
		" -t <bits>   Set error correction capability, default=4\n"
		" -v          Verbose mode\n",
		progname);

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int m = 13;
	int t = 4;
	int len = 0;
	int c, i;
	int opt_decode = 0;
	int opt_cache_encode = 0;
	int ncorrupt = 0;
	int niterations = 1;
	uint8_t *data;
	char *progname = basename(argv[0]);
	struct bch_control *bch;
	unsigned int *pattern = NULL;
	unsigned int patsize = 0, tmax;
	unsigned int generator = 0;

	while ((c = getopt(argc, argv, "b:c:dg:hl:m:p:r:st:v")) != -1) {
		switch (c) {

		case 'b':
			niterations = atoi(optarg);
			if (niterations <= 0) {
				usage(progname);
			}
			break;

		case 'v':
			opt_verbose = 1;
			break;

		case 's':
			opt_cache_encode = 1;
			break;

		case 'd':
			opt_decode = 1;
			break;

		case 'c':
			ncorrupt = atoi(optarg);
			if ((ncorrupt < 0) || (ncorrupt > MAX_ERRORS)) {
				usage(progname);
			}
			break;

		case 'g':
			generator = atoi(optarg);
			break;

		case 'l':
			len = atoi(optarg);
			break;

		case 'm':
			m = atoi(optarg);
			break;

		case 'p': {
			const char *s = optarg;
			pattern = malloc(MAX_ERRORS*sizeof(unsigned int));
			assert(pattern);
			while (s && (patsize < MAX_ERRORS)) {
				int bit = atoi(s);
				pattern[patsize++] = (bit & ~7)|(7-(bit & 7));
				s = strchr(s, ',');
				if (s) {
					s++;
				}
			}
		}
			break;

		case 'r':
			opt_decode = 1;
			/* randomize all parameters */
			srand48(atoi(optarg));
			m = 7+(lrand48() % 9);
			tmax = ((1 << (m-1))-1)/m;
			t = 1+(lrand48() % tmax);
			ncorrupt = 1+(lrand48() % t);
			len = ((1 << m)-1)/8 - (m*t+7)/8;
			niterations = 100000;
			fprintf(stderr,
				"random: m=%d t=%d c=%d len=%d iter=%d\n",
				m, t, ncorrupt, len, niterations);
			break;

		case 't':
			t = atoi(optarg);
			break;

		case 'h':
		default:
			usage(progname);
			break;
		}
	}

	assert((m >= 5) && (m <= 15));
	if (len == 0) {
		len = 1 << (m-4);
	}
	assert((t > 0) && (t <= MAX_ERRORS));
	assert((len > 0) && (8*len+m*t <= (1 << m)-1));

	bch = init_bch(m, t, generator);
	if (bch == NULL) {
		fprintf(stderr, "cannot initialize BCH engine\n");
		exit(1);
	}

	data = malloc(len+bch->ecc_bytes);
	assert(data);
	memset(data, 0xff, len);

	if (opt_decode) {
		unsigned int errloc[t];

		memset(data+len, 0, bch->ecc_bytes);
		encode_bch(bch, data, len, data+len);

		/* apply corruption pattern */
		if (patsize) {
			for (i = 0; i < (int)patsize; i++) {
				data[pattern[i]/8] ^= (1 << (pattern[i] & 7));
			}
		}

		for (i = 0; i < niterations; i++) {
			generic_decode(bch, data, len, errloc,
				       ncorrupt, opt_cache_encode);
		}
	}
	else {
		/* apply corruption pattern */
		if (patsize) {
			for (i = 0; i < (int)patsize; i++) {
				data[pattern[i]/8] ^= (1 << (pattern[i] & 7));
			}
		}

		/* encode */
		for (i = 0; i < niterations; i++) {
			memset(data+len, 0, bch->ecc_bytes);
			encode_bch(bch, data, len, data+len);
			if (UNLIKELY(opt_verbose)) {
				fprintf(stderr, "ecc=");
				for (i = 0;
				     i < (int)bch->ecc_bytes; i++) {
					fprintf(stderr, "%02x", data[len+i]);
				}
				fprintf(stderr, "=~");
				for (i = 0;
				     i < (int)bch->ecc_bytes; i++) {
					fprintf(stderr, "%02x",
						(~data[len+i]) & 0xff);
				}
				fprintf(stderr, "\n");
			}
		}
	}

	free(pattern);
	free(data);
	free_bch(bch);

	return 0;
}
