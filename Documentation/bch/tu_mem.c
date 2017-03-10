/*
 * BCH library tests
 *
 * Memory leak and fault injection test.
 *
 * Usage: ./tu_mem
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
#include <assert.h>

/* basic memory allocation check */

#include <stdlib.h>

static int count = 0;
static int allocated = 0;
static int fault = 0;

static void *my_malloc(size_t size)
{
	count++;
	if (fault && ((count % fault) == (fault-1))) {
		fprintf(stderr, "injecting fault size=%d\n", (int)size);
		return NULL;
	}
	allocated++;
	return malloc(size);
}
static void *my_calloc(size_t nmemb, size_t size)
{
	count++;
	if (fault && ((count % fault) == (fault-1))) {
		fprintf(stderr, "injecting fault size=%d\n", (int)(nmemb*size));
		return NULL;
	}
	allocated++;
	return calloc(nmemb, size);
}
static void my_free(void *ptr)
{
	if (ptr) {
		allocated--;
	}
	free(ptr);
}

#define malloc my_malloc
#define calloc my_calloc
#define free   my_free

#include "../../lib/bch.c"

int main(void)
{
	struct bch_control *bch;
	int m, t, m1 = 5, m2 = 15;

	for (m = m1; m <= m2; m++) {
		for (t = 4; t <= 16; t++) {
			fprintf(stderr, "m=%d:t=%d: basic memory check\n", m,
				t);
			bch = init_bch(m, 4, 0);
			assert(bch);
			free_bch(bch);
			assert(allocated == 0);
		}
	}
	/* inject fault */
	fault = 2;
	bch = init_bch(13, 4, 0);
	assert(bch == NULL);
	assert(allocated == 0);

	return 0;
}
