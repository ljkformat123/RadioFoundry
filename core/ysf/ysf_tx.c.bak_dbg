// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF transmitter: sequence builder (one frame at a time). See ysf_tx.h for
// the sequence tables (Yaesu Tables 4-3/4-4, MMDVM YSFPayload.cpp). The host
// prototype's streamer (ysf_tx_init / ysf_tx_dibits / ysf_tx_samples, which
// feeds dibits through ysf_mod) is NOT ported: the firmware streams dibits
// through the air session (air_session_tx) to the HR_C7000 native 4FSK slot
// engine, so the engine (ysf_engine.c) calls ysf_tx_frame_build once per
// frame instead.
#include "ysf_tx.h"
#include <string.h>

unsigned ysf_tx_total_frames(const ysf_tx_config *cfg)
{
	return 2u + 8u * (cfg->cycles ? cfg->cycles : 1u);
}

// Voice-block length of one 20 ms block in this mode.
static unsigned voice_len(uint8_t dt)
{
	switch (dt) {
	case YSF_DT_VD_MODE1: return 9;  // VCH
	case YSF_DT_VD_MODE2: return 13;  // VCH 9 + VeCH 4
	case YSF_DT_VOICE_FR: return 18;  // VCH
	default: return 0;  // Data FR: no voice blocks
	}
}

// Where the voice bytes of block b start inside the 90-byte payload.
static unsigned voice_off(uint8_t dt, unsigned b)
{
	switch (dt) {
	case YSF_DT_VD_MODE1: return 18 * b + 9;
	case YSF_DT_VD_MODE2: return 18 * b + 5;
	case YSF_DT_VOICE_FR: return 18 * b;
	default: return 0;
	}
}

static void csd1(const ysf_tx_config *c, uint8_t out[20])
{
	memcpy(out, c->dest, 10);
	memcpy(out + 10, c->src, 10);
}

static void csd2(const ysf_tx_config *c, uint8_t out[20])
{
	memcpy(out, c->down, 10);
	memcpy(out + 10, c->up, 10);
}

static void csd3(const ysf_tx_config *c, uint8_t out[20])
{
	memcpy(out, c->rem1, 10);
	memcpy(out + 10, c->rem2, 10);
}

// One 10-byte DN data unit: Dest / Src / Down / Up / Rem1+2 / Rem3+4.
static void dn_unit(const ysf_tx_config *c, unsigned fn, uint8_t out[10])
{
	switch (fn) {
	case 0: memcpy(out, c->dest, 10); break;
	case 1: memcpy(out, c->src, 10); break;
	case 2: memcpy(out, c->down, 10); break;
	case 3: memcpy(out, c->up, 10); break;
	case 4: memcpy(out, c->rem1, 5); memcpy(out + 5, c->rem2, 5); break;
	default: memcpy(out, c->rem3, 5); memcpy(out + 5, c->rem4, 5); break;
	}
}

static void data_unit(const ysf_tx_config *c, unsigned index, uint8_t out[20])
{
	if (c->data) c->data(c->ctx, index, out);
	else memset(out, 0, 20);
}

bool ysf_tx_frame_build(const ysf_tx_config *cfg, unsigned frame_no, uint8_t frame[YSF_FRAME_BYTES])
{
	unsigned cycles = cfg->cycles ? cfg->cycles : 1u;
	unsigned total = 2u + 8u * cycles;
	if (frame_no >= total) return false;
	switch (cfg->dt) {
	case YSF_DT_VD_MODE1: case YSF_DT_DATA_FR: case YSF_DT_VD_MODE2: case YSF_DT_VOICE_FR:
		break;
	default:
		return false;
	}
	ysf_fich f;
	memset(&f, 0, sizeof f);
	f.cs = cfg->cs;
	f.cm = cfg->cm;
	f.dev = cfg->dev;
	f.mr = cfg->mr;
	f.voip = cfg->voip;
	f.dt = cfg->dt;
	f.sql = cfg->sql;
	f.sc = cfg->dgid;
// Header and terminator: FI 0 / 2, FN 0, CSD1 + CSD2, mode-independent.
	if (frame_no == 0 || frame_no == total - 1) {
		f.fi = frame_no == 0 ? YSF_FI_HEADER : YSF_FI_TERMINATOR;
		f.fn = 0;
		uint8_t d1[20], d2[20];
		csd1(cfg, d1);
		csd2(cfg, d2);
		ysf_frame_build(&f, d1, d2, frame);
		return true;
	}
// Communication frames: FN cycles 0..7, data index counts DT units.
	unsigned fn = (frame_no - 1) % 8;
	f.fi = YSF_FI_COMMUNICATION;
	f.fn = (uint8_t)fn;
	f.ft = 7;	// final index of the eight communication frames in the block
	if (cfg->dt == YSF_DT_VOICE_FR && cfg->vw_subheader && frame_no == 1)
		f.ft = 1;	// identify the CSD3 sub-header
	uint8_t dch[20], dch2[20];
	memset(dch, 0, sizeof dch);
	memset(dch2, 0, sizeof dch2);
	const uint8_t *second = NULL;
	switch (cfg->dt) {
	case YSF_DT_VD_MODE1:
		if (fn == 0) csd1(cfg, dch);
		else if (fn == 1) csd2(cfg, dch);
		else if (fn == 2) csd3(cfg, dch);
		else data_unit(cfg, fn - 3, dch);  // FN3..7 = DT1..DT5
		break;
	case YSF_DT_VD_MODE2:
		if (fn < 6) dn_unit(cfg, fn, dch);
		else data_unit(cfg, fn - 6, dch);  // FN6/7 = DT1/DT2, first 10 bytes
		break;
	case YSF_DT_DATA_FR:
		if (fn == 0) {
			csd1(cfg, dch);
			csd2(cfg, dch2);
			second = dch2;
		} else if (fn == 1) {
			csd3(cfg, dch);
			data_unit(cfg, 0, dch2);  // DT1
			second = dch2;
		} else {
			data_unit(cfg, 2 * fn - 3, dch);  // DT2, DT4, ... DT12
			data_unit(cfg, 2 * fn - 2, dch2);  // DT3, DT5, ... DT13
			second = dch2;
		}
		break;
	default:  // VW has DCH only in the sub-header
		if (ysf_vw_subheader(&f))
			csd3(cfg, dch);
		break;
	}
	ysf_frame_build(&f, dch, second, frame);
// Voice blocks: overwrite the deterministic filler with the caller's
// bytes (or zeros when there is no callback). Header/terminator have no
// voice blocks; Data FR has none either.
	unsigned vlen = voice_len(cfg->dt);
	if (vlen) {
		uint8_t *payload = frame + YSF_SYNC_BYTES + YSF_FICH_BYTES;
		for (unsigned b = ysf_vw_subheader(&f) ? YSF_VW_SUBHEADER_FIRST_VOICE : 0; b < YSF_BLOCKS; b++) {
			uint8_t *vch = payload + voice_off(cfg->dt, b);
			if (cfg->voice) cfg->voice(cfg->ctx, frame_no, b, vch, vlen);
			else memset(vch, 0, vlen);
		}
	}
	return true;
}