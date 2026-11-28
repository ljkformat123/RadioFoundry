// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// IMBE 7200x4400 voice codewords, used by P25 and YSF VW with different interleavers.
#include "imbe88.h"

#include <string.h>
#include "../air/air_syms.h"
#include "golay.h"
#include "hamming.h"

// The PN masks m_1..m_6 from u_0 (5.3): 114 values of p = 173 p + 13849 mod 65536 from p0 = 16 u_0, bit 15 of each
static void voice_masks(uint16_t u0, uint32_t m[8])
{
	static const uint8_t LEN[8] = { 0, 23, 23, 23, 15, 15, 15, 0 };
	uint16_t p = (uint16_t)(16 * u0);

	for (uint8_t c = 0; c < 8; c++) {
		m[c] = 0;
		for (uint8_t i = 0; i < LEN[c]; i++) {
			p = (uint16_t)(173u * p + 13849u);
			m[c] = (m[c] << 1) | (p >> 15);
		}
	}
}

uint8_t fec_imbe88_decode(const uint32_t c[8], uint8_t imbe[FEC_IMBE88_BYTES])
{
	uint32_t m[8];
	uint16_t u[8], o = 0;
	int errs, e0, e4 = 0;

	// u_0 first: the masks of the other words come from it
	errs = e0 = fec_golay23_decode(c[0], &u[0]);
	voice_masks(u[0], m);
	for (uint8_t k = 1; k < 4; k++)
		errs += fec_golay23_decode(c[k] ^ m[k], &u[k]);
	for (uint8_t k = 4; k < 7; k++) {
		uint16_t cw = (uint16_t)(c[k] ^ m[k]), was = cw;

		fec_hamming15_correct(&cw);
		errs += cw != was;
		if (k == 4)
			e4 = cw != was;
		u[k] = cw >> 4;
	}
	u[7] = (uint16_t)c[7];
	memset(imbe, 0, FEC_IMBE88_BYTES);
	for (uint8_t k = 0; k < 8; k++) {
		uint8_t len = k < 4 ? 12 : k < 7 ? 11 : 7;

		for (uint8_t b = 0; b < len; b++)
			air_bit_put(imbe, o++, (uint8_t)((u[k] >> (len - 1 - b)) & 1));
	}
	return (uint8_t)(errs | (e0 << 4) | (e4 << 6));
}

void fec_imbe88_encode(const uint8_t imbe[FEC_IMBE88_BYTES], uint32_t c[8])
{
	uint32_t m[8];
	uint16_t u[8], o = 0;

	for (uint8_t k = 0; k < 8; k++) {
		uint8_t len = k < 4 ? 12 : k < 7 ? 11 : 7;

		u[k] = 0;
		for (uint8_t b = 0; b < len; b++)
			u[k] = (uint16_t)((u[k] << 1) | air_bit_get(imbe, o++));
	}
	voice_masks(u[0], m);
	c[0] = fec_golay23_encode(u[0]);
	for (uint8_t k = 1; k < 4; k++)
		c[k] = fec_golay23_encode(u[k]) ^ m[k];
	for (uint8_t k = 4; k < 7; k++)
		c[k] = fec_hamming15_encode(u[k]) ^ m[k];
	c[7] = u[7];
}
