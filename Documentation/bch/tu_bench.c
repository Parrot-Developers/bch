/*
 * BCH library tests
 *
 * Benchmarking and verification on random error vectors.
 *
 * Usage: ./tu_bench <m> <t> <sec>
 *
 * <m>: value for parameter m
 * <t>: value for parameter t
 * <sec>: target duration of a single (m,t) run
 *
 * Copyright (C) 2011 Parrot S.A.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <arpa/inet.h>

#include "linux/bch.h"

#define MIN_ITER_US    10000
#define MAX_LOOP_MS    10000

static struct timespec ts1;
static struct timespec ts2;

static inline void start_measure(void)
{
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts1);
}

static inline double stop_measure(void)
{
	double d;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
	d = (ts2.tv_sec-ts1.tv_sec)*1000000.0+(ts2.tv_nsec-ts1.tv_nsec)/1000.0;
	return d;
}

static inline unsigned int rev8(unsigned int x)
{
	return (x & ~7)|(7-(x & 7));
}

static void generate_random_vector(struct bch_control *bch, int len,
				   unsigned int *vec, int vecsize)
{
	int i, j, ok, nbits;
	nbits = 8*len+bch->ecc_bits;

	for (i = 0; i < vecsize; i++) {
		do {
			ok = 1;
			vec[i] = lrand48() % nbits;
			/* make sure we stay in linear interval */
			vec[i] = rev8(vec[i]);
			for (j = 0; j < i; j++) {
				if (vec[j] == vec[i]) {
					ok = 0;
					break;
				}
			}
		} while (!ok);
	}
}

static void corrupt_data(uint8_t *data, const unsigned int *vec, int vecsize)
{
	int i, bit;

	for (i = 0; i < vecsize; i++) {
		bit = vec[i];
		data[bit/8] ^= 1 << (bit & 7);
	}
}

static void compare_vectors(const unsigned int *vec, int vecsize,
			    const unsigned int *errloc, int errsize)
{
	int i, j, ok;

	assert(vecsize == errsize);

	for (i = 0; i < vecsize; i++) {
		ok = 0;
		for (j = 0; j < vecsize; j++) {
			if (errloc[j] == vec[i]) {
				ok = 1;
				break;
			}
		}
		if (!ok) {
			fprintf(stderr, "vec={");
			for (j = 0; j < vecsize; j++) {
				fprintf(stderr, "%d,", vec[j]);
			}
			fprintf(stderr, "}\nerr={");
			for (j = 0; j < vecsize; j++) {
				fprintf(stderr, "%d,", errloc[j]);
			}
			fprintf(stderr, "}\n");
		}
		assert(ok);
	}
}

static double check_vector(struct bch_control *bch, uint8_t *data, int len,
			   const unsigned int *vec, int vecsize,
			   int encode_once, int niter)
{
	int i, nerrors = 0;
	unsigned int errloc[bch->t];
	uint8_t sum, ecc[bch->ecc_bytes];
	uint8_t *read_ecc = (data+len);
	double d = 0.0;

	corrupt_data(data, vec, vecsize);

	if (encode_once) {
		memset(ecc, 0, bch->ecc_bytes);
		encode_bch(bch, data, len, ecc);
		for (i = 0, sum = 0; i < (int)bch->ecc_bytes; i++) {
			ecc[i] ^= read_ecc[i];
			sum |= ecc[i];
		}

		if (sum) {
			start_measure();
			for (i = 0; i < niter; i++) {
				nerrors = decode_bch(bch, NULL, len, NULL,
						     ecc, NULL, errloc);
			}
			d = stop_measure();
		}
	}
	else {
		start_measure();
		for (i = 0; i < niter; i++) {
			nerrors = decode_bch(bch, data, len, read_ecc, NULL,
					     NULL, errloc);
		}
		d = stop_measure();
	}

	corrupt_data(data, vec, vecsize);
	compare_vectors(vec, vecsize, errloc, nerrors);
	return d;
}

static void calibrate(struct bch_control *bch, uint8_t *data, unsigned int len,
		      int ms, int *niter, int *nsamples)
{
	double d;
	unsigned int i, nbits, vec[bch->t];

	*niter = 0;
	nbits = 8*len+bch->ecc_bits-1;

	/* sample test case */
	for (i = 0; i < bch->t; i++) {
		vec[i] = rev8(i*nbits/((bch->t >= 2)? (bch->t-1) : 2));
	}
	/* compute number of iterations of a single case */
	d = check_vector(bch, data, len, vec, bch->t, 0, 100);
	if (d) {
		*niter = (int)floor(MIN_ITER_US*100.0/d);
	}
	if (!*niter || (*niter > 100000)) {
		*niter = 1;
	}

	/* compute number of samples */
	*nsamples = d? (int)floor(ms*100000.0/(d*(*niter))) : 10000;

	fprintf(stderr, "calibration: iter=%gµs niter=%d nsamples=%d\n",
		d/100.0, *niter, *nsamples);
}

static void bch_test_bench(int m, int t, int ms)
{
	int i, len, vecsize, cache, niter, nsamples;
	struct bch_control *bch;
	unsigned int vec[t];
	uint8_t *data;
	double d, dsum, dmax, avg;
	FILE *fp;
#if defined(CONFIG_BCH_CONST_PARAMS)
	static const int cst = 1;
#else
	static const int cst = 0;
#endif

	fp = fopen("/proc/cpuinfo", "r");
	if (fp) {
		char buf[128];
		const char *const name1 = "model name\t: ";
		const char *const name2 = "Processor\t: ";
		while (fgets(buf, sizeof(buf), fp)) {
			if (!strncmp(name1, buf, strlen(name1))) {
				fprintf(stderr, "cpu: %s", buf+strlen(name1));
				break;
			}
			if (!strncmp(name2, buf, strlen(name2))) {
				fprintf(stderr, "cpu: %s", buf+strlen(name2));
				break;
			}
		}
		fclose(fp);
	}

	bch = init_bch(m, t, 0);
	assert(bch);

	srand48(m);
	len = (1 << (m-1))/8;

	data = malloc(len+bch->ecc_bytes);
	assert(data);

	/* prepare data buffer */
	for (i = 0; i < len; i++) {
		data[i] = lrand48() & 0xff;
	}
	memset(data+len, 0, bch->ecc_bytes);
	encode_bch(bch, data, len, data+len);

	/* calibrate loops */
	calibrate(bch, data, len, ms, &niter, &nsamples);

	for (cache = 1; cache >= 0; cache--) {
		for (vecsize = 0; vecsize <= t; vecsize++) {
			dmax = 0.0;
			dsum = 0.0;
			for (i = 0; i < nsamples; i++) {
				if (vecsize) {
					generate_random_vector(bch, len, vec,
							       vecsize);
				}
				d = check_vector(bch, data, len, vec, vecsize,
						 cache, niter);
				if (d > dmax) {
					dmax = d;
				}
				dsum += d;
			}
			avg = dsum/(1.0*nsamples*niter);
			fprintf(stderr,
				"decode:const=%d:m=%d:t=%d:e=%d:enc=%d:"
				"avg=%g:worst=%g:avg_thr=%d\n", cst, m, t,
				vecsize, !cache, avg, dmax/(1.0*niter),
				avg? (int)floor(len*8.0/avg) : (int)0);
		}
	}

	free(data);
	free_bch(bch);
}

int main(int argc, char *argv[])
{
	int m, t, nbits, ms;

	fprintf(stderr, "%s: bch decoder benchmark\n", argv[0]);
	fprintf(stderr, "%s-endian, type sizes: int=%d long=%d longlong=%d\n",
		(htonl(0x01020304) == 0x01020304)? "big" : "little",
		(int)sizeof(int), (int)sizeof(long), (int)sizeof(long long));

	if (argc != 4) {
		fprintf(stderr, "Usage: %s m t <sec>\n", argv[0]);
		exit(1);
	}
	m = atoi(argv[1]);
	t = atoi(argv[2]);
	ms = atoi(argv[3])*1000;

	nbits = (1 << (m-1))+m*t;
	assert(nbits < (1 << m));

	bch_test_bench(m, t, ms);

	return 0;
}
