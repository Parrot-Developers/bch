/*
 * BCH library tests
 *
 * Error correction verification tool, with 3 modes:
 * - full: test all possible error vectors
 * - rand: test random vectors for a given number of iterations
 * - burst: test all contiguous error bursts vectors
 *
 * Usage:
 * ./tu_correct full tmax [m]
 * OR
 * ./tu_correct rand tmax [m] [niter]
 * OR
 * ./tu_correct burst tmax [m]
 *
 * Error correction is tested from t=2 up to t=tmax.
 * If no 'm' value provided, all m values in range [7;15] are tested.
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
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>

struct gf_poly;

#define RAND_ITER 1000000

#define MAX_TESTS 15000000000ull
//#define DEBUG 1

#if defined(DEBUG)

#define dbg(_fmt, args...) fprintf(stderr, _fmt, ##args)
#define gf_poly_str(_poly) gf_poly_dump(_poly, alloca(((_poly)->deg+1)*12))
char * gf_poly_dump(const struct gf_poly *f, char *buf);
#endif

#include "../../lib/bch.c"

static inline unsigned int rev8(unsigned int x)
{
	return (x & ~7)|(7-(x & 7));
}

char * gf_poly_dump(const struct gf_poly *f, char *buf)
{
	int i, pos = 0;

	pos += sprintf(&buf[pos], "((%d)", f->deg);
	for (i = f->deg; i >= 0; i--) {
		pos += sprintf(&buf[pos], " %x", f->c[i]);
	}
	pos += sprintf(&buf[pos], ")");
	return buf;
}

static inline void update_pct(uint64_t tot)
{
	int pct;
	static int opct = 0;
	static uint64_t iter = 0;
	static uint64_t total = 1;

	if (tot) {
		iter = 0;
		total = tot;
		opct = 0;
		fprintf(stderr, "00%%");
	}
	else {
		iter++;
		pct = iter*100ull/total;

		if (pct != opct) {
			fprintf(stderr, "\b\b\b%02d%%", pct);
			opct = pct;
		}
	}
}

static void encode(struct bch_control *bch, uint8_t *data, int len,
		   uint8_t *ecc)
{
	memset(ecc, 0, bch->ecc_bytes);
	encode_bch(bch, data, len, ecc);

#if defined(DEBUG)
	{
		unsigned int i;
		for (i = 0; i < len+bch->ecc_bytes; i++) {
			dbg("%02x", data[i]);
		}
		dbg("\n");
	}
#endif
}

/*
 * multiply two polynomials in GF(2^m)[X]
 */
static void gf_poly_mul(struct bch_control *bch, const struct gf_poly *a,
                       const struct gf_poly *b, struct gf_poly *res)
{
       unsigned int i, j;

       memset(res, 0, GF_POLY_SZ(a->deg + b->deg));

       /* not very efficient, but used only during generator poly computation*/
       for (i = 0; i <= a->deg; i++) {
               for (j = 0; j <= b->deg; j++) {
                       res->c[i+j] ^= gf_mul(bch, a->c[i], b->c[j]);
               }
       }
       res->deg = a->deg + b->deg;
}

void compute_elp(struct bch_control *bch, int len, const unsigned int *vec,
		 int vecsize)
{
	int i, bit, nbits;
	struct gf_poly *m = alloca(GF_POLY_SZ(1));
	struct gf_poly *p = alloca(GF_POLY_SZ(vecsize));
	struct gf_poly *q = alloca(GF_POLY_SZ(vecsize));

	nbits = 8*len+bch->ecc_bits;
	p->deg = 0;
	p->c[0] = 1;

	for (i = 0; i < vecsize; i++) {
		m->deg = 1;
		bit = rev8(vec[i]);
		bit = nbits-1-bit;
		m->c[0] = gf_inv(bch, a_pow(bch, bit));
		m->c[1] = 1;
		dbg("(X+%x) = (X+a^-%d); %d = nbits-1-%d = nbits-1-rev8(%d)\n",
		    m->c[0], bit, bit, nbits-1-bit, rev8(nbits-1-bit));
		gf_poly_mul(bch, m, p, q);
		gf_poly_copy(p, q);
	}
	for (i = p->deg; i >= 0; i--) {
		p->c[i] = gf_div(bch, p->c[i], p->c[0]);
	}
	dbg("nbits=%d, elp=%s\n", nbits, gf_poly_str(p));
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
		dbg("data[%d] ^= %02x (%d)\n", bit/8, 1 << (bit & 7), bit);
	}
}

static void compare_vectors(const unsigned int *vec, int vecsize,
			    const unsigned int *errloc, int errsize)
{
	int i, j, ok;

#ifdef DEBUG
	fprintf(stderr, "vec={");
	for (i = 0; i < vecsize; i++) {
		fprintf(stderr, "%u,", vec[i]);
	}
	fprintf(stderr, "}\n");
	fprintf(stderr, "err={");
	for (i = 0; i < errsize; i++) {
		fprintf(stderr, "%u,", errloc[i]);
	}
	fprintf(stderr, "}\n");
#endif

	assert(vecsize == errsize);

	for (i = 0; i < vecsize; i++) {
		ok = 0;
		for (j = 0; j < vecsize; j++) {
			if (errloc[j] == vec[i]) {
				ok = 1;
				break;
			}
		}
		assert(ok);
	}
}

static void check_vector(struct bch_control *bch, uint8_t *data, int len,
			 const unsigned int *vec, int vecsize)
{
	int nerrors;
	unsigned int errloc[bch->t];
	uint8_t *read_ecc = (data+len);

	compute_elp(bch, len, vec, vecsize);
	if (!vecsize) {
		return;
	}
	corrupt_data(data, vec, vecsize);
	nerrors = decode_bch(bch, data, len, read_ecc, NULL, NULL, errloc);
	corrupt_data(data, vec, vecsize);
	assert(nerrors >= 0);

#if defined(DEBUG)
	{
		unsigned int i;
		dbg("ecc=");
		for (i = 0; i < BCH_ECC_WORDS(bch); i++) {
			dbg("%08x", bch->ecc_buf[i]);
		}
		dbg("\n");
	}
#endif
	compare_vectors(vec, vecsize, errloc, nerrors);
	update_pct(0);
}

static void bch_test_errors_random(int m, int t, int iter)
{
	int i, len, vecsize;
	struct bch_control *bch;
	unsigned int vec[t];
	uint8_t *data;

	fprintf(stderr,"m=%d: checking %d random %d error vectors: ",m, iter,t);
	update_pct(iter);

	bch = init_bch(m, t, 0);
	assert(bch);

	dbg("ecc_bits=%d ecc_bytes=%d\n", bch->ecc_bits, bch->ecc_bytes);

	srand48(m);
	len = (1 << (m-1))/8;

	data = malloc(len+bch->ecc_bytes);
	assert(data);

	/* prepare data buffer */
	for (i = 0; i < len; i++) {
		data[i] = lrand48() & 0xff;
	}
	encode(bch, data, len, data+len);

	while (iter-- > 0) {
		vecsize = (lrand48() % t)+1;
		generate_random_vector(bch, len, vec, vecsize);
		check_vector(bch, data, len, vec, vecsize);
	}
	fprintf(stderr,"\n");
	free(data);
	free_bch(bch);
}

static void bch_test_errors_full_k(struct bch_control *bch, uint8_t *data,
				   int len, int k, unsigned int *vec,
				   int nerrors)
{
	unsigned int i, j, nbits = 8*len+bch->ecc_bits;

	if (k == nerrors) {
		/* make sure we stay in linear interval */
		for (i = 0; i < (unsigned int)nerrors; i++) {
			vec[i] = rev8(vec[i]);
		}
		check_vector(bch, data, len, vec, nerrors);
		/* make sure we stay in linear interval */
		for (i = 0; i < (unsigned int)nerrors; i++) {
			vec[i] = rev8(vec[i]);
		}
	}
	else {
		j = (k > 0)? vec[k-1]+1 : 0;
		for (i = j; i < nbits-(nerrors-k-1); i++) {
			vec[k] = i;
			bch_test_errors_full_k(bch, data, len,k+1,vec, nerrors);
		}
	}
}

static void bch_test_errors_full(int m, int t, int nerrors)
{
	int i, len;
	uint64_t iter, den;
	struct bch_control *bch;
	unsigned int vec[t];
	uint8_t *data;

	assert(nerrors <= t);

	bch = init_bch(m, t, 0);
	assert(bch);

	srand48(m);
	len = (1 << (m-1))/8;

	/* nb of cases = n!/p!(n-p)! with p=nerrors, n=8*len+bch->ecc_bits */
	iter = 1;
	den = 1;
	for (i = 0; i < nerrors; i++) {
		iter *= (8*len+bch->ecc_bits-i);
		den *= (i+1ull);
	}
	iter /= den;

	fprintf(stderr,"m=%d:t=%d:checking all %d error vectors (%" PRIu64
		" cases): ", m, t, nerrors, iter);

#ifdef MAX_TESTS
	if (((m-1)*nerrors >= 64) || (iter > (uint64_t)MAX_TESTS)) {
		fprintf(stderr,"skipping...\n");
		return;
	}
#endif
	update_pct(iter);

	data = malloc(len+bch->ecc_bytes);
	assert(data);

	/* prepare data buffer */
	for (i = 0; i < len; i++) {
		data[i] = lrand48() & 0xff;
	}
	encode(bch, data, len, data+len);

	bch_test_errors_full_k(bch, data, len, 0, vec, nerrors);

	fprintf(stderr,"\n");
	free(data);
	free_bch(bch);
}

static void bch_test_errors_bursts(int m, int t, int nerrors)
{
	int i, j, len;
	uint64_t iter;
	struct bch_control *bch;
	unsigned int vec[t];
	uint8_t *data;

	assert(nerrors <= t);

	bch = init_bch(m, t, 0);
	assert(bch);

	srand48(m);
	len = (1 << (m-1))/8;

	iter = 8*len+bch->ecc_bits-nerrors+1;

	fprintf(stderr,"m=%d:t=%d: checking all %d error bursts (%" PRIu64
		" cases): ", m, t, nerrors, iter);

	update_pct(iter);

	data = malloc(len+bch->ecc_bytes);
	assert(data);

	/* prepare data buffer */
	for (i = 0; i < len; i++) {
		data[i] = lrand48() & 0xff;
	}
	encode(bch, data, len, data+len);

	for (i = 0; i < (int)iter; i++) {
		for (j = 0; j < nerrors; j++) {
			/* make sure we stay in linear interval */
			vec[j] = rev8(i+j);
		}
		check_vector(bch, data, len, vec, nerrors);
	}

	fprintf(stderr,"\n");
	free(data);
	free_bch(bch);
}

int main(int argc, char *argv[])
{
	int m, t, k, nbits, tmax, m1 = 7, m2 = 15, niter = RAND_ITER;

	if (argc < 3) {
		fprintf(stderr, "Usage: %s "
			"[full tmax [m]]|"
			"[rand tmax [m] [niter]]\n"
			"[burst tmax [m]]\n",
			argv[0]);
		exit(1);
	}
	tmax = atoi(argv[2]);
	if (argc == 4) {
		m1 = m2 = atoi(argv[3]);
	}

	if (strcmp(argv[1], "full") == 0) {
		for (m = m1; m <= m2; m++) {
			for (t = 2; t <= tmax; t++) {
				nbits = (1 << (m-1))+m*t;
				if (nbits < (1 << m)) {
					for (k = 2; k <= t; k++) {
						bch_test_errors_full(m, t, k);
					}
				}
			}
		}
	}
	else if (strcmp(argv[1], "rand") == 0) {
		if (argc == 5) {
			niter = atoi(argv[4]);
		}
		for (m = m1; m <= m2; m++) {
			nbits = (1 << (m-1))+m*tmax;
			if (nbits < (1 << m)) {
				bch_test_errors_random(m, tmax,  niter);
			}
		}
	}
	else if (strcmp(argv[1], "burst") == 0) {
		for (m = m1; m <= m2; m++) {
			for (t = 2; t <= tmax; t++) {
				nbits = (1 << (m-1))+m*t;
				if (nbits < (1 << m)) {
					for (k = 2; k <= t; k++) {
						bch_test_errors_bursts(m, t, k);
					}
				}
			}
		}
	}

	return 0;
}
