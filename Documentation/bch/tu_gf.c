/*
 * BCH library tests
 *
 * Galois field operations unitary tests: gf_mul, gf_div, gf_sqr, gf_inv.
 *
 * Usage: ./tu_gf [m]
 *
 * If m is not specified, test all m values in range [5;15].
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

/* Galois field basic operations unitary testing */

#include "../../lib/bch.c"

static unsigned int primpoly = 0;
static int m;

static unsigned int multiply_ak(unsigned int x, int k)
{
	int i;
	for (i = 0; i < k; i++) {
		x <<= 1;
		if (x & (1 << m)) {
			x ^= primpoly;
		}
	}
	return x;
}

static unsigned int multiply(unsigned int x, unsigned int y)
{
	int i;
	unsigned int r = 0;

	for (i = 0; i < m; i++) {
		if (x & (1 << i)) {
			r ^= multiply_ak(y, i);
		}
	}
	return r;
}

static unsigned int inverse(unsigned int x)
{
	unsigned int y;

	assert(x);

	/* brute force */
	for (y = 1; y < (1u << m); y++) {
		if (multiply(x, y) == 1) {
			return y;
		}
	}
	assert(0);
	return 0;
}

static void bch_test_gf_ops(struct bch_control *bch)
{
	unsigned int x, y, r1, r2, x1 = 0;

	fprintf(stderr, "m=%d: checking Galois field mul,div,sqr,inv\n",bch->m);

	for (x = 0; x <= bch->n; x++) {
		r1 = multiply(x, x);
		r2 = gf_sqr(bch, x);
		assert(r1 == r2);

		if (x) {
			x1 = inverse(x);
			r2 = gf_inv(bch, x);
			assert(x1 == r2);
		}

		for (y = 0; y <= bch->n; y++) {
			r1 = multiply(x, y);
			r2 = gf_mul(bch, x, y);
			assert(r1 == r2);
			if (x) {
				r1 = multiply(y, x1);
				r2 = gf_div(bch, y, x);
				assert(r1 == r2);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	/* default primitive polynomials */
	const int min_m = 5;
	const int max_m = 15;
	static const unsigned int prim_poly_tab[] = {
		0x25, 0x43, 0x83, 0x11d, 0x211, 0x409, 0x805, 0x1053, 0x201b,
		0x402b, 0x8003,
	};
	struct bch_control *bch;
	int m1 = 5, m2 = 15;

	if (argc == 2) {
		m1 = m2 = atoi(argv[1]);
	}
	for (m = m1; m <= m2; m++) {
		bch = init_bch(m, 4, 0);
		assert(bch);
		assert(m >= min_m);
		assert(m <= max_m);
		primpoly = prim_poly_tab[m-min_m];
		bch_test_gf_ops(bch);
		free_bch(bch);
	}

	return 0;
}
