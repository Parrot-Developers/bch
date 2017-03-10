/*
 * BCH library tests
 *
 * Test error correction of all degree <= 4 polynomials for values m in range
 * [5;7], then 1000000000 random vectors for each value m in range [8;15].
 *
 * Usage: ./tu_poly4
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

#include "../../lib/bch.c"

static unsigned int evaluate_poly(struct bch_control *bch, struct gf_poly *p,
				  unsigned int lr)
{
	unsigned int j, syn = p->c[0];

	for (j = 1; j <= p->deg; j++) {
		if (p->c[j]) {
			syn ^= a_pow(bch, a_log(bch, p->c[j])+j*(bch->n-lr));
		}
	}
	return syn;
}

static void check_polynomial(struct bch_control *bch, struct gf_poly *p)
{
	unsigned int j, nroots, nroots2;
	unsigned int syn, roots2[4];

	nroots2 = find_poly_roots(bch, 1, p, roots2);
	if (nroots2 == p->deg) {
		for (j = 0; j < nroots2; j++) {
			syn = evaluate_poly(bch, p, roots2[j]);
			assert(syn == 0);
		}
	}
	else {
		assert(nroots2 == 0);
		/* count roots */
		nroots = 0;
		for (j = 0; j < bch->n; j++) {
			syn = evaluate_poly(bch, p, j);
			if (!syn) {
				nroots++;
			}
		}
		assert(nroots < p->deg);
	}
}

static void bch_test_deg4_full(int m)
{
	int i, num, den;
	struct bch_control *bch;
	struct gf_poly *p;

	fprintf(stderr,"m=%d: checking all deg <= 4 polynomials: 00%%", m);

	bch = init_bch(m, 4, 0);
	assert(bch);
	num = 100/bch->n;
	num = num? num : 1;
	den = bch->n/100;
	den = den? den : 1;

	p = bch->poly_2t[0];
	/* only generate polynomials without 0 as a root */
	for (p->c[0] = 1; p->c[0] <= bch->n; p->c[0]++) {
		for (p->c[1] = 0; p->c[1] <= bch->n; p->c[1]++) {
			for (p->c[2] = 0; p->c[2] <= bch->n; p->c[2]++) {
				for (p->c[3] = 0; p->c[3] <= bch->n; p->c[3]++) {
					for (p->c[4] = 0; p->c[4] <= bch->n; p->c[4]++) {
						p->deg = 0;
						for (i = 4; i >= 0; i--) {
							if (p->c[i]) {
								p->deg = i;
								break;
							}
						}
						check_polynomial(bch,p);
					}
				}
			}
		}
		if ((p->c[0] % den) == 0) {
			fprintf(stderr, "\b\b\b%02d%%", p->c[0]*num/den);
		}
	}
	fprintf(stderr,"\n");
	free_bch(bch);
}

static void bch_test_deg4_random(int m, int iter)
{
	int i, step = iter/100;
	struct bch_control *bch;
	struct gf_poly *p;

	fprintf(stderr,"m=%d: checking %d random deg <= 4 polynomials: 00%%",
		m, iter);

	bch = init_bch(m, 4, 0);
	assert(bch);

	srand48(m);

	p = bch->poly_2t[0];

	while (iter-- > 0) {
		for (i = 0; i <= 4; i++) {
			p->c[i] = lrand48() & bch->n;
		}
		if (p->c[0]) {
			p->deg = 0;
			for (i = 4; i >= 0; i--) {
				if (p->c[i]) {
					p->deg = i;
					break;
				}
			}
			check_polynomial(bch, p);
		}
		if ((iter % step) == 0) {
			fprintf(stderr, "\b\b\b%02d%%", 100-iter/step);
		}
	}
	fprintf(stderr,"\n");
	free_bch(bch);
}

int main(void)
{
	int m;

	for (m = 5; m <= 7; m++) {
		bch_test_deg4_full(m);
	}
	for (m = 8; m <= 15; m++) {
		bch_test_deg4_random(m, 1000000000);
	}

	return 0;
}

