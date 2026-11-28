// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF on the air interface: the profile and the framer hooks. The frame is
// validated here (sync, FICH, DCH); the call tracking lives in ysf_engine.c.
#include "ysf_rx.h"

#include <string.h>

#include "ysf_voice.h"
#include "../../hal/hal_radio.h"

void ysf_rx_init(ysf_rx_t *r, const ysf_rx_sink_t *sink, void *ctx)
{
	memset(r, 0, sizeof *r);
	r->sink = *sink;
	r->ctx = ctx;
}

static void rx_frame(const air_frame_t *f, void *ctx)
{
	ysf_rx_t *r = ctx;
	const uint8_t *frame = f->hard;	// four dibits per byte, MSB first
	ysf_rx_frame_t out = {0};

	if (f->nsyms != YSF_FRAME_DIBITS || !ysf_frame_decode_fich(frame, &out.fich)) {
		r->fich_fail++;
		return;
	}
	if (!ysf_frame_decode_dch(frame, &out.fich, out.dch, out.dch2, &out.dch_len)) {
		r->dch_fail++;
		out.dch_len = 0;
		// The CRC covers data only. Keep call boundaries and independently coded voice.
		if (out.fich.fi == YSF_FI_COMMUNICATION && out.fich.dt == YSF_DT_DATA_FR)
			return;
	}
	out.t_us = f->t_us;
	out.rssi_dbm = f->rssi_dbm;
	// Only communication frames carry voice blocks: a header or terminator
	// holds two DCH1s where the voice would be.
	if (out.fich.fi == YSF_FI_COMMUNICATION) {
		out.voice_first = ysf_vw_subheader(&out.fich) ? YSF_VW_SUBHEADER_FIRST_VOICE : 0;
		for (unsigned b = out.voice_first; b < YSF_BLOCKS; b++) {
			unsigned len = ysf_voice_get(frame, out.fich.dt, b, out.voice[b]);

			if (b == out.voice_first)
				out.voice_len = (uint8_t)len;
		}
	}
	r->frames++;
	if (r->sink.frame)
		r->sink.frame(&out, r->ctx);
}

static void rx_lost(void *ctx)
{
	ysf_rx_t *r = ctx;

	if (r->sink.lost)
		r->sink.lost(r->ctx);
}

// The profile, shared by the two modes except for the deviation.
#define YSF_PROFILE(dev_hz) \
	.sym_rate = 4800, \
	.levels = 4, \
	.shape = AIR_SHAPE_RRC, \
	.shape_param = 20, \
	.dev_outer_hz = (dev_hz), \
	.timing = AIR_TIMING_CONTINUOUS, \
	.bw = RADIO_BW_12K5, \
	.sync_every_syms = YSF_FRAME_DIBITS, \
	.sync_any_syms = YSF_FRAME_DIBITS, \
	.idle = 0xD4, \
	.nsyncs = 1, \
	.sync = { { .id = 0, .nsyms = 20, .max_errs = 2, .frame_at = 0, \
		    .syms = { 0xD4, 0x71, 0xC9, 0x63, 0x4D } } }

const air_mode_t ysf_air_mode = {
	.name = "YSF",
	.profile = {
		YSF_PROFILE(2700),	// Wide: +-2700/+-900 Hz
	},
	.frame_syms = YSF_FRAME_DIBITS,
	.head_syms = 0,		// no head hook: fixed-length frames
	.lost_frames = 4,		// N2 (Yaesu 4.2.1(6))
	.rx_frame = rx_frame,
	.rx_lost = rx_lost,
};

const air_mode_t ysf_air_mode_narrow = {
	.name = "YSF Narrow",
	.profile = {
		YSF_PROFILE(1350),	// Narrow: +-1350/+-450 Hz
	},
	.frame_syms = YSF_FRAME_DIBITS,
	.head_syms = 0,
	.lost_frames = 4,
	.rx_frame = rx_frame,
	.rx_lost = rx_lost,
};
