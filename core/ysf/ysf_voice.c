// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF voice: half-rate AMBE+2 between a decoded YSF frame and the 9-byte
// DMR-format AMBE frame the vocoder path speaks. See ysf_voice.h for what
// is verified and what is not.
#include "ysf_voice.h"

#include <string.h>

#include "../dmr/ambe/ambeFec.h"

// Bit helpers, MSB first (the same order as ysf_frame.c).
static int get_bit(const uint8_t *p, unsigned i) { return (p[i >> 3] >> (7 - (i & 7))) & 1; }
static void put_bit(uint8_t *p, unsigned i, int b)
{
	if (b) p[i >> 3] |= (uint8_t)(0x80u >> (i & 7));
	else p[i >> 3] &= (uint8_t)~(0x80u >> (i & 7));
}

unsigned ysf_voice_len(uint8_t dt)
{
	switch (dt) {
	case YSF_DT_VD_MODE1: return 9;	// VCH
	case YSF_DT_VD_MODE2: return 13;	// VCH 9 + VeCH 4
	case YSF_DT_VOICE_FR: return 18;	// VCH
	default: return 0;			// Data FR: no voice blocks
	}
}

unsigned ysf_voice_get(const uint8_t frame[YSF_FRAME_BYTES], uint8_t dt, unsigned block, uint8_t out[18])
{
	unsigned len = ysf_voice_len(dt);

	if (block >= YSF_BLOCKS || !len)
		return 0;
	// Payload layout per 20 ms block (18 bytes): V/D-1 [DCH1 9][VCH 9],
	// DN [DCH2 5][VCH 9][VeCH 4], VW [VCH 18].
	const uint8_t *payload = frame + YSF_SYNC_BYTES + YSF_FICH_BYTES + 18 * block;
	switch (dt) {
	case YSF_DT_VD_MODE1: memcpy(out, payload + 9, 9); break;
	case YSF_DT_VD_MODE2: memcpy(out, payload + 5, 13); break;
	default: memcpy(out, payload, 18); break;
	}
	return len;
}

// DN voice interleave, 26x4 over the 104 bits: TABLE[i] = 4*(i mod 26) + (i div 26)
// (MMDVM AMBEFEC::regenerateYSFDN; Yaesu Fig. 4-32).
static unsigned dn_table(unsigned i) { return 4 * (i % 26) + (i / 26); }

void ysf_voice_dn_to_frame(const uint8_t field[13], uint8_t frame9[9], unsigned *errors)
{
	uint8_t vch[13];
	uint8_t white[20];
	uint8_t params[AMBE_PARAM_BITS];
	unsigned errs = 0;

	// De-interleave: vch[i] = field[TABLE[i]].
	memset(vch, 0, sizeof vch);
	for (unsigned i = 0; i < 104; i++)
		put_bit(vch, i, get_bit(field, dn_table(i)));
	// De-whiten with the first 13 bytes of the PN(9,5) sequence.
	ysf_whitening_generate(white);
	for (unsigned i = 0; i < 13; i++)
		vch[i] ^= white[i];
	// The first 27 parameters are tripled: majority vote. A non-unanimous
	// triplet counts as one error (MMDVM counts it the same way).
	for (unsigned k = 0; k < 27; k++) {
		unsigned ones = (unsigned)get_bit(vch, 3 * k) + (unsigned)get_bit(vch, 3 * k + 1) +
				(unsigned)get_bit(vch, 3 * k + 2);

		params[k] = ones >= 2 ? 1 : 0;
		if (ones != 0 && ones != 3)
			errs++;
	}
	// The last 22 parameters are direct: bits 81..102.
	for (unsigned k = 27; k < 49; k++)
		params[k] = (uint8_t)get_bit(vch, k + 54);
	// Bit 103 is padding: ignored.
	if (errors)
		*errors = errs;
	ambeFecEncode(params, frame9);
}

void ysf_voice_dn_from_frame(const uint8_t frame9[9], uint8_t field[13])
{
	uint8_t params[AMBE_PARAM_BITS];
	uint8_t vch[13];
	uint8_t white[20];

	ambeFecDecode(frame9, params, NULL);
	memset(vch, 0, sizeof vch);
	for (unsigned k = 0; k < 27; k++) {
		put_bit(vch, 3 * k, params[k]);
		put_bit(vch, 3 * k + 1, params[k]);
		put_bit(vch, 3 * k + 2, params[k]);
	}
	for (unsigned k = 27; k < 49; k++)
		put_bit(vch, k + 54, params[k]);
	// Bit 103 stays 0 (padding).
	ysf_whitening_generate(white);
	for (unsigned i = 0; i < 13; i++)
		vch[i] ^= white[i];
	// Interleave: field[TABLE[i]] = vch[i].
	memset(field, 0, 13);
	for (unsigned i = 0; i < 104; i++)
		put_bit(field, dn_table(i), get_bit(vch, i));
}


// VW air bit for each consecutive bit in the eight IMBE codewords (MSB first).
// Same permutation as MMDVMHost AMBEFEC::regenerateIMBE and DSD-FME fr_interleave.
static const uint8_t vw_interleave[144] = {
	  0,   7,  12,  19,  24,  31,  36,  43,  48,  55,  60,  67,
	 72,  79,  84,  91,  96, 103, 108, 115, 120, 127, 132, 139,
	  1,   6,  13,  18,  25,  30,  37,  42,  49,  54,  61,  66,
	 73,  78,  85,  90,  97, 102, 109, 114, 121, 126, 133, 138,
	  2,   9,  14,  21,  26,  33,  38,  45,  50,  57,  62,  69,
	 74,  81,  86,  93,  98, 105, 110, 117, 122, 129, 134, 141,
	  3,   8,  15,  20,  27,  32,  39,  44,  51,  56,  63,  68,
	 75,  80,  87,  92,  99, 104, 111, 116, 123, 128, 135, 140,
	  4,  11,  16,  23,  28,  35,  40,  47,  52,  59,  64,  71,
	 76,  83,  88,  95, 100, 107, 112, 119, 124, 131, 136, 143,
	  5,  10,  17,  22,  29,  34,  41,  46,  53,  58,  65,  70,
	 77,  82,  89,  94, 101, 106, 113, 118, 125, 130, 137, 142,
};
static const uint8_t vw_word_bits[8] = { 23, 23, 23, 23, 15, 15, 15, 7 };

uint8_t ysf_voice_vw_decode(const uint8_t field[18], uint8_t imbe[FEC_IMBE88_BYTES])
{
	uint32_t words[8] = {0};
	unsigned k = 0;

	for (unsigned i = 0; i < 8; i++)
		for (unsigned b = 0; b < vw_word_bits[i]; b++)
			words[i] = (words[i] << 1) | (uint32_t)get_bit(field, vw_interleave[k++]);
	return fec_imbe88_decode(words, imbe);
}

void ysf_voice_vw_encode(const uint8_t imbe[FEC_IMBE88_BYTES], uint8_t field[18])
{
	uint32_t words[8];
	unsigned k = 0;

	fec_imbe88_encode(imbe, words);
	memset(field, 0, 18);
	for (unsigned i = 0; i < 8; i++)
		for (unsigned b = vw_word_bits[i]; b > 0; b--)
			put_bit(field, vw_interleave[k++], (words[i] >> (b - 1)) & 1u);
}
