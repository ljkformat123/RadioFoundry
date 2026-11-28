// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF on the air interface (core/air): the air_mode_t profile and the
// framer hooks, over the soft modem (RX) and the HR_C7000 native 4FSK slot
// engine (TX). The receive chain is the 8 kHz FM copy -> air_softmodem ->
// air_framer -> this layer -> the engine's call tracking (ysf_engine.c).
//
// Profile (Yaesu Rev. 1.02): 4800 symbols/s, 4 levels, RRC 0.2, Wide
// +-2700/+-900 Hz (dev_outer_hz 2700) or Narrow +-1350/+-450 Hz (1350), a
// 20-symbol frame sync D471C9634D at the start of every 480-symbol frame,
// no head hook (fixed-length frames), lost after 4 missed syncs (N2).
// The sync tolerance is 2 symbols (up to 4 bit errors); the frame is really
// validated by the FICH CRC. The DCH CRC controls whether its data is used.
#ifndef YSF_RX_H
#define YSF_RX_H
#include <stdbool.h>
#include <stdint.h>

#include "air/air_framer.h"
#include "ysf_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

// One decoded frame, handed to the sink during the framer's rx_frame
// callback. The voice blocks are copied out of the frame here, so nothing
// in this struct points at the framer's buffer: the sink may keep it.
typedef struct {
	ysf_fich fich;
	uint8_t dch[20];	// first DCH's data (CSD1 / DN unit / DT unit)
	uint8_t dch2[20];	// second DCH (CSD2 / Data FR DCH-2), zeros when none
	unsigned dch_len;	// first DCH length in bytes: 20, 10 or 0 (absent or failed CRC)
	uint8_t voice[YSF_BLOCKS][18];	// voice bytes per 20 ms block (communication frames only;
					// header/terminator frames carry DCHs there instead, voice_len 0)
	uint8_t voice_first; // first voice block (three slots hold the VW sub-header and reserved data)
	uint8_t voice_len;	// bytes per voice block: 0, 9, 13 or 18
	uint32_t t_us;		// framer time of the frame's sync
	int16_t rssi_dbm;
} ysf_rx_frame_t;

typedef struct {
	void (*frame)(const ysf_rx_frame_t *f, void *ctx);
	void (*lost)(void *ctx);
} ysf_rx_sink_t;

typedef struct {
	ysf_rx_sink_t sink;
	void *ctx;
	uint32_t frames;	// frames delivered to the engine
	uint32_t fich_fail;	// frames dropped: sync or FICH bad
	uint32_t dch_fail;	// frames with valid FICH and failed DCH CRC
} ysf_rx_t;

void ysf_rx_init(ysf_rx_t *r, const ysf_rx_sink_t *sink, void *ctx);

// The air modes, Wide (2700 Hz) and Narrow (1350 Hz). The engine picks one.
extern const air_mode_t ysf_air_mode;
extern const air_mode_t ysf_air_mode_narrow;

#ifdef __cplusplus
}
#endif
#endif
