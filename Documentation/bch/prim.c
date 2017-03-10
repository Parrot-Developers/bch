/*
 * BCH library tests
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
#include <string.h>
#include <stdlib.h>

#define M 13
#define N ((1U << M)-1)

static int is_primitive(unsigned int poly)
{
	unsigned int i, x = 1;
	const unsigned int k = 1 << M;

	for (i = 0; i < N; i++) {
		if (i && (x == 1))
			return 0;
		x <<= 1;
		if (x & k)
			x ^= poly;
	}
	return (x == 1);
}

int main(int argc, char *argv[])
{
	unsigned int i, count = 0;

	for (i = 0; i < (1 << M); i++) {
		/* skip polynomials divisible by X or X+1 */
		if ((__builtin_popcount(i) & 1) || !(i & 1)) {
			continue;
		}
		if (is_primitive(i|(1 << M))) {
			printf("%d ", i|(1 << M));
			count++;
		}
	}
	fprintf(stderr, "\nfound %d polynomials\n", count);
	return 0;
}
