// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional voice and data on a channel (docs/spec/ysf.md): the
// frame layer (ysf_rx.h) on an air interface session (core/air/air_session.h).
// The radio core begins it on a YSF channel and ends it on leaving; in
// between it receives calls, hands their voice frames to a sink, and
// transmits when the radio core says so. No RTOS call: the radio core's task
// and the provider's task use it as air_session.h describes, voice frames
// cross to and from the audio side through the caller's hooks.
//
// Voice is half-rate AMBE+2, one 9-byte DMR-format frame per 20 ms block,
// both ways (ysf_voice.c): V/D-1 passes the VCH bytes through, DN converts
// them to and from its tripled/whitened/interleaved field. VW uses the
// separate full-rate hooks with 11-byte IMBE speech parameters.
#ifndef YSF_ENGINE_H
#define YSF_ENGINE_H
#include <stdbool.h>
#include <stdint.h>

#include "air/air_session.h"
#include "ysf_frame.h"
#include "ysf_rx.h"
#include "ysf_tx.h"
#include "fec/imbe88.h"

#ifdef __cplusplus
extern "C" {
#endif

// A call, as the radio glue sees it: the FICH fields and the callsigns the
// DCHs carried (10 characters each on the air, NUL-terminated here, trailing
// spaces trimmed; empty when not heard yet). DN carries only the first 5
// characters of Rem1..Rem4.
typedef struct {
	ysf_fich fich;
	char dest[YSF_CALLSIGN_LEN + 1];
	char src[YSF_CALLSIGN_LEN + 1];
	char down[YSF_CALLSIGN_LEN + 1];
	char up[YSF_CALLSIGN_LEN + 1];
	char rem1[YSF_CALLSIGN_LEN + 1];
	char rem2[YSF_CALLSIGN_LEN + 1];
	char rem3[YSF_CALLSIGN_LEN + 1];
	char rem4[YSF_CALLSIGN_LEN + 1];
	bool late;		// started mid-call (no header heard)
	uint32_t frames;	// frames of this call
	uint32_t voice_frames;	// voice blocks played
} ysf_call_t;

typedef struct {
	uint8_t dt;		// DT mode to transmit (and to expect voice in)
	bool narrow;		// Narrow deviation (1350 Hz) instead of Wide (2700 Hz)
	uint8_t rx_dgid;	// DG-ID to listen for: 0 = all
	uint8_t tx_dgid;	// DG-ID to send (FICH SC)
	uint8_t cm;		// YSF_CM_GROUP / RADIO_ID / INDIVIDUAL
	uint8_t mr;		// YSF_MR_DIRECT / DOWNLINK_FREE / DOWNLINK_BUSY
	char dest[YSF_CALLSIGN_LEN];
	char src[YSF_CALLSIGN_LEN];
	char down[YSF_CALLSIGN_LEN];
	char up[YSF_CALLSIGN_LEN];
	char rem1[YSF_CALLSIGN_LEN];
	char rem2[YSF_CALLSIGN_LEN];
	char rem3[YSF_CALLSIGN_LEN];
	char rem4[YSF_CALLSIGN_LEN];
	const uint8_t *silence;	// a 9-byte AMBE silence frame (the DMR one)
	const uint8_t *silence_vw; // 11-byte packed IMBE silence for VW
} ysf_engine_cfg_t;

typedef struct {
	// Receive, from the provider's task. start: a call we take began (its
	// fields may still fill in: update). voice: a 9-byte DMR-format AMBE
	// frame, one per 20 ms block. data: one DCH data unit (20 bytes, DN
	// 10), indexed as the TX data callback counts them (ysf_tx.h).
	void (*rx_start)(const ysf_call_t *c, void *ctx);
	void (*rx_update)(const ysf_call_t *c, void *ctx);
	void (*rx_voice)(const uint8_t vch[9], void *ctx);
	void (*rx_end)(void *ctx);
	void (*rx_data)(void *ctx, unsigned index, const uint8_t *data, unsigned len);
	// Transmit, from the provider's task: the next 9-byte AMBE frame, false
	// when none is ready. NULL: every block is `silence`
	bool (*tx_voice)(uint8_t vch[9], void *ctx);
	// Transmit: one data unit (20 bytes, DN uses the first 10), indexed as
	// ysf_tx.h counts them. NULL: data units are zero-filled
	void (*tx_data)(void *ctx, unsigned index, uint8_t out[20]);
	void (*rx_vw)(const uint8_t imbe[FEC_IMBE88_BYTES], uint8_t errs, void *ctx);
	bool (*tx_vw)(uint8_t imbe[FEC_IMBE88_BYTES], void *ctx);
} ysf_engine_hooks_t;

typedef struct {
	bool rx_active;	// a call is being received (whether or not it is played)
	bool rx_played;
	ysf_call_t rx;		// its fields; kept after it ends (the last call heard)
	air_tx_state_t tx;
	uint32_t rx_calls;
	uint32_t rx_rejects;	// frames dropped by the DG-ID squelch
	uint32_t tx_calls;
	uint32_t tx_filler;	// silence blocks sent because the encoder had none ready
	uint32_t rx_frames;	// frames accepted after sync and FICH validation
	uint32_t rx_voice_frames;	// voice blocks played
} ysf_engine_view_t;

typedef struct {
	air_session_t session;
	const air_mode_t *mode;
	ysf_rx_t rx;
	ysf_engine_cfg_t cfg;
	ysf_engine_hooks_t hooks;
	void *ctx;
	ysf_tx_config txcfg;	// the sequence builder's config (refreshed on prepare)
	// The frame buffers, receive and transmit, share storage: the radio is half-duplex, so a transmission
	// discards any partial received frame (unreceivable while transmitting), and reception never runs while
	// a frame is being built. A frame assembled from the shared bytes still has to pass sync and FICH
	// (ysf_rx.c), so a clobbered partial frame is rejected as noise, not played. The two sides apart cost
	// 975 bytes of RAM; shared they cost 600.
	union {
		struct {	// receive: the framer's frame and its soft values
			uint8_t frame[YSF_FRAME_BYTES];
			int8_t soft[YSF_FRAME_DIBITS];
		} rx;
		struct {	// transmit: the built frame, its dibits, the staged voice blocks
			uint8_t txframe[YSF_FRAME_BYTES];
			uint8_t dibits[YSF_FRAME_DIBITS / 4];
			uint8_t stage[YSF_BLOCKS][FEC_IMBE88_BYTES];
			uint8_t sq[10][FEC_IMBE88_BYTES];
		} tx;
	} buf;
	uint16_t dib_pos;	// next dibit to queue from dibits[]
	unsigned frame_no;	// next sequence frame to build (0 header, 1..8 CC, 9 terminator)
	bool tx_started;	// the header is queued
	volatile bool tx_stop;	// the radio core let go: end at the next frame boundary
	bool tx_ended;		// ... and the terminator is queued
	ysf_call_t call;	// the call being tracked
	bool played;		// the call in hand passes the squelch
	volatile bool recheck;	// ysf_engine_set changed what passes it
	volatile uint8_t seq;	// odd while the provider's task writes view
	uint8_t stage_n;	// blocks already staged for the frame in hand
	uint8_t sq_head;	// next block to take from sq
	uint8_t sq_n;		// blocks held in sq
	uint8_t stop_frames;	// communication frames built since tx_stop: the terminator is forced past the bound
	ysf_engine_view_t view;
} ysf_engine_t;

// The mode for a deviation
const air_mode_t *ysf_engine_mode(bool narrow);
// What the board's provider gives YSF: AIR_FIT_xxx, 0 = this board can't
uint32_t ysf_engine_fit(const hal_air_t *air, bool narrow);
// Start on the tuned channel (the board in its digital mode, its DMR stack stopped). False: no provider, no fit
bool ysf_engine_begin(ysf_engine_t *e, const hal_air_t *air, const ysf_engine_cfg_t *cfg,
		      const ysf_engine_hooks_t *hooks, void *ctx);
void ysf_engine_end(ysf_engine_t *e);
// The receiver is on a new frequency: what it held is dropped
void ysf_engine_retuned(ysf_engine_t *e);
// Change what is received and sent while running (a DG-ID or callsign change that needs no retune; not the mode
// or the deviation). Not while transmitting
void ysf_engine_set(ysf_engine_t *e, const ysf_engine_cfg_t *cfg);

// Transmit (the radio core's task). prepare: the header is queued; then hal_radio_t.tx(), then go. stop: the
// operator let go: the frame on air is finished and a terminator sent. state: AIR_TX_DONE = switch the
// transmitter off (hal_radio_t.tx_off), then clear
bool ysf_engine_tx_prepare(ysf_engine_t *e);
bool ysf_engine_tx_go(ysf_engine_t *e);
void ysf_engine_tx_stop(ysf_engine_t *e);
void ysf_engine_tx_abort(ysf_engine_t *e);
air_tx_state_t ysf_engine_tx_state(const ysf_engine_t *e);
void ysf_engine_tx_clear(ysf_engine_t *e);

// A copy of the state for the radio core's task
void ysf_engine_view(const ysf_engine_t *e, ysf_engine_view_t *out);

#ifdef __cplusplus
}
#endif
#endif
