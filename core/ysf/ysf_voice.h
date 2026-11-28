// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF voice: half-rate AMBE+2 (49 parameter bits) between a decoded YSF
// frame and the 9-byte DMR-format AMBE frame the vocoder path speaks.
//
// V/D-1 (DT 0): the 72-bit VCH is exactly the DMR AMBE+2 3600x2450 frame
// (same code words, same interleaving; core/dmr/ambe/ambeFec.c), so its
// bytes pass through untouched. DN (DT 2): the 104-bit voice field (VCH 72
// + VeCH 32, block bits 40..143) carries the 49 parameters raw: the first
// 27 parameters tripled, the last 22 direct, one zero pad bit, whitened
// with the first 13 bytes of the PN(9,5) sequence and interleaved 26x4
// (MMDVM AMBEFEC::regenerateYSFDN / AMBEFEC.cpp; Yaesu Fig. 4-32).
//
// What is NOT verified here: that the 49 YSF parameters are semantically
// the same AMBE+2 parameters the DVSI blob expects. The channel mapping is
// proven against MMDVMHost (see the test), the parameter semantics are not:
// no public YSF voice vector or air capture exists to prove them against.
//
// VW (DT 3) uses the IMBE-compatible 88-bit speech data, with its own permutation
// of the Golay/Hamming-protected bits.
#ifndef YSF_VOICE_H
#define YSF_VOICE_H
#include <stdbool.h>
#include <stdint.h>

#include "ysf_frame.h"
#include "fec/imbe88.h"

#ifdef __cplusplus
extern "C" {
#endif

// Voice bytes of one 20 ms block in this mode: V/D-1 9, DN 13, VW 18,
// Data FR 0.
unsigned ysf_voice_len(uint8_t dt);

// Voice bytes of block `block` (0..4) of a decoded frame, into out (18
// bytes). Returns the length (0, 9, 13 or 18); 0 when the mode has no voice
// blocks or the block is out of range.
unsigned ysf_voice_get(const uint8_t frame[YSF_FRAME_BYTES], uint8_t dt, unsigned block, uint8_t out[18]);

// DN voice field (13 bytes: VCH 9 + VeCH 4) -> 9-byte DMR-format AMBE frame:
// de-interleave, de-whiten, majority vote per triplet, ambeFecEncode.
// errors counts the non-unanimous triplets (NULL to ignore); the pad bit
// (bit 103) is ignored -- MMDVM notes it is apparently not always 0 on air.
void ysf_voice_dn_to_frame(const uint8_t field[13], uint8_t frame9[9], unsigned *errors);

// 9-byte DMR-format AMBE frame -> DN voice field: ambeFecDecode, triplicate
// the first 27 parameters, whiten, interleave. The pad bit is sent as 0.
void ysf_voice_dn_from_frame(const uint8_t frame9[9], uint8_t field[13]);

// VW: 18 bytes on air, 11 packed vocoder bytes. Decode returns the IMBE FEC error byte.
uint8_t ysf_voice_vw_decode(const uint8_t field[18], uint8_t imbe[FEC_IMBE88_BYTES]);
void ysf_voice_vw_encode(const uint8_t imbe[FEC_IMBE88_BYTES], uint8_t field[18]);

#ifdef __cplusplus
}
#endif
#endif
