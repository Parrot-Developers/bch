/*
 * BCH library tests
 *
 * Test BCH library encoding on unaligned data buffers.
 *
 * Usage: ./tu_unaligned tmax
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
#include <string.h>
#include <assert.h>

#include "../../lib/bch.c"

static void bch_test_unaligned(int m, int t)
{
	int i, j, len, err;
	struct bch_control *bch;
	uint8_t *data, *udata[3];

	fprintf(stderr, "m=%d:t=%d: checking encoding on unaligned buffers\n",
		m, t);

	bch = init_bch(m, t, 0);
	assert(bch);

	srand48(m);
	len = (1 << (m-1))/8;

	/* prepare data buffer */
	data = malloc(len+bch->ecc_bytes);
	assert(data);
	for (i = 0; i < len; i++) {
		data[i] = lrand48() & 0xff;
	}

	/* prepare identical but unaligned buffers */
	for (i = 0; i < 3; i++) {
		udata[i] = malloc(len+bch->ecc_bytes+4);
		assert(udata[i]);
		memcpy(udata[i]+i, data, len);
	}

	for (j = 0; j < 3; j++) {
		memset(data+len, 0, bch->ecc_bytes);
		encode_bch(bch, data, len, data+len);
		for (i = 0; i < 3; i++) {
			memset(udata[i]+len+i, 0, bch->ecc_bytes);
			encode_bch(bch, udata[i]+i, len, udata[i]+i+len);
			err = memcmp(data+len, udata[i]+i+len, bch->ecc_bytes);
			assert(err == 0);
		}
		len--;
	}

	for (i = 0; i < 3; i++) {
		free(udata[i]);
	}
	free(data);
	free_bch(bch);
}

int main(int argc, char *argv[])
{
	int m, t, tmax;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s tmax\n", argv[0]);
		exit(1);
	}
	tmax = atoi(argv[1]);
	for (m = 7; m <= 15; m++) {
		for (t = 1; t <= tmax; t++) {
			bch_test_unaligned(m, t);
		}
	}
	return 0;
}
