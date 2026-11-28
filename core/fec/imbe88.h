// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
#ifndef FEC_IMBE88_H
#define FEC_IMBE88_H
#include <stdint.h>

#define FEC_IMBE88_BYTES 11
// Eight systematic Golay/Hamming words, including whitening, before the mode's air interleaver.
// Decode returns the total corrections in bits 0–3, C0 corrections in 4–5, and C4 correction in bit 6.
void fec_imbe88_encode(const uint8_t imbe[FEC_IMBE88_BYTES], uint32_t words[8]);
uint8_t fec_imbe88_decode(const uint32_t words[8], uint8_t imbe[FEC_IMBE88_BYTES]);
#endif
