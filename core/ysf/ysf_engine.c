// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional voice and data on a channel (ysf_engine.h).
#include "ysf_engine.h"

#include <string.h>

#include "air/air_syms.h"
#include "ysf_voice.h"

static bool half_rate(uint8_t dt)
{
	return dt == YSF_DT_VD_MODE1 || dt == YSF_DT_VD_MODE2;
}

const air_mode_t *ysf_engine_mode(bool narrow)
{
	return narrow ? &ysf_air_mode_narrow : &ysf_air_mode;
}

uint32_t ysf_engine_fit(const hal_air_t *air, bool narrow)
{
	return air_session_fit(air, ysf_engine_mode(narrow));
}

// ── Receive (provider's task) ──
static void view_begin(ysf_engine_t *e) { e->seq++; }
static void view_end(ysf_engine_t *e) { e->seq++; }

// The DG-ID squelch, first cut (docs/spec/ysf.md): every call, or only the
// ones to our code, to none, or to Break Out. Confirm against a real Yaesu
// radio: whether a station that sets a DG-ID hears a transmission with
// SC 0, and whether Break Out really opens every squelch.
static bool wanted(const ysf_engine_t *e, const ysf_fich *f)
{
	if (!e->cfg.rx_dgid)
		return true;
	return f->sc == e->cfg.rx_dgid || f->sc == 0 || f->sc == 127;
}

// A callsign from the air: 10 characters, space-padded. n bytes are taken
// (DN carries only the first 5 characters of Rem1..Rem4); trailing spaces
// are trimmed, the result NUL-terminated. All spaces becomes empty.
static void copy_call(char dst[YSF_CALLSIGN_LEN + 1], const uint8_t *data, unsigned n)
{
	unsigned len = n;

	while (len && data[len - 1] == ' ')
		len--;
	memcpy(dst, data, len);
	dst[len] = '\0';
}

static void merge_dch(ysf_engine_t *e, const ysf_rx_frame_t *f)
{
	ysf_call_t *c = &e->call;

	if (!f->dch_len)
		return;

	if (f->fich.fi == YSF_FI_HEADER || f->fich.fi == YSF_FI_TERMINATOR) {
		copy_call(c->dest, f->dch, 10);
		copy_call(c->src, f->dch + 10, 10);
		copy_call(c->down, f->dch2, 10);
		copy_call(c->up, f->dch2 + 10, 10);
		return;
	}
	switch (f->fich.dt) {
	case YSF_DT_VD_MODE1:
		if (f->fich.fn == 0) {
			copy_call(c->dest, f->dch, 10);
			copy_call(c->src, f->dch + 10, 10);
		} else if (f->fich.fn == 1) {
			copy_call(c->down, f->dch, 10);
			copy_call(c->up, f->dch + 10, 10);
		} else if (f->fich.fn == 2) {
			copy_call(c->rem1, f->dch, 10);
			copy_call(c->rem2, f->dch + 10, 10);
		}
		break;
	case YSF_DT_VD_MODE2:
		if (f->fich.fn == 0)
			copy_call(c->dest, f->dch, 10);
		else if (f->fich.fn == 1)
			copy_call(c->src, f->dch, 10);
		else if (f->fich.fn == 2)
			copy_call(c->down, f->dch, 10);
		else if (f->fich.fn == 3)
			copy_call(c->up, f->dch, 10);
		else if (f->fich.fn == 4) {
			copy_call(c->rem1, f->dch, 5);
			copy_call(c->rem2, f->dch + 5, 5);
		} else if (f->fich.fn == 5) {
			copy_call(c->rem3, f->dch, 5);
			copy_call(c->rem4, f->dch + 5, 5);
		}
		break;
	case YSF_DT_DATA_FR:
		if (f->fich.fn == 0) {
			copy_call(c->dest, f->dch, 10);
			copy_call(c->src, f->dch + 10, 10);
			copy_call(c->down, f->dch2, 10);
			copy_call(c->up, f->dch2 + 10, 10);
		} else if (f->fich.fn == 1) {
			copy_call(c->rem1, f->dch, 10);
			copy_call(c->rem2, f->dch + 10, 10);
		}
		break;
	case YSF_DT_VOICE_FR:
		if (ysf_vw_subheader(&f->fich)) {
			copy_call(c->rem1, f->dch, 10);
			copy_call(c->rem2, f->dch + 10, 10);
		}
		break;
	default:
		break;
	}
}

// One DCH data unit per frame, indexed as the TX data callback counts them
// (ysf_tx.h): V/D-1 FN3..7 -> 0..4, DN FN6/7 -> 0..1, Data FR FN1 DCH-2 -> 0,
// FN2..7 DCH-1/DCH-2 -> 2*FN-3 / 2*FN-2.
static void data_units(ysf_engine_t *e, const ysf_rx_frame_t *f)
{
	if (!f->dch_len || !e->hooks.rx_data || f->fich.fi != YSF_FI_COMMUNICATION)
		return;
	switch (f->fich.dt) {
	case YSF_DT_VD_MODE1:
		if (f->fich.fn >= 3)
			e->hooks.rx_data(e->ctx, f->fich.fn - 3, f->dch, 20);
		break;
	case YSF_DT_VD_MODE2:
		if (f->fich.fn >= 6)
			e->hooks.rx_data(e->ctx, f->fich.fn - 6, f->dch, 10);
		break;
	case YSF_DT_DATA_FR:
		if (f->fich.fn == 1)
			e->hooks.rx_data(e->ctx, 0, f->dch2, 20);
		else if (f->fich.fn >= 2) {
			e->hooks.rx_data(e->ctx, 2 * f->fich.fn - 3, f->dch, 20);
			e->hooks.rx_data(e->ctx, 2 * f->fich.fn - 2, f->dch2, 20);
		}
		break;
	default:
		break;
	}
}

static void voice_blocks(ysf_engine_t *e, const ysf_rx_frame_t *f)
{
	unsigned played = 0;

	if (!e->played || f->fich.fi != YSF_FI_COMMUNICATION)
		return;
	switch (f->fich.dt) {
	case YSF_DT_VD_MODE1:
		if (!e->hooks.rx_voice)
			break;
		for (unsigned b = 0; b < YSF_BLOCKS; b++) {
			e->hooks.rx_voice(f->voice[b], e->ctx);
			played++;
		}
		break;
	case YSF_DT_VD_MODE2: {
		uint8_t frame9[9];

		if (!e->hooks.rx_voice)
			break;
		for (unsigned b = 0; b < YSF_BLOCKS; b++) {
			ysf_voice_dn_to_frame(f->voice[b], frame9, NULL);
			e->hooks.rx_voice(frame9, e->ctx);
			played++;
		}
		break;
	}
	case YSF_DT_VOICE_FR: {
		uint8_t imbe[FEC_IMBE88_BYTES];

		if (!e->hooks.rx_vw)
			break;
		for (unsigned b = f->voice_first; b < YSF_BLOCKS; b++) {
			uint8_t errs = ysf_voice_vw_decode(f->voice[b], imbe);

			e->hooks.rx_vw(imbe, errs, e->ctx);
			played++;
		}
		break;
	}
	default:
		break;	// Data FR has no voice
	}
	if (played) {
		e->call.voice_frames += played;
		view_begin(e);
		e->view.rx_voice_frames += played;
		view_end(e);
	}
}

static void rx_fields(ysf_engine_t *e, bool start)
{
	bool play = wanted(e, &e->call.fich);

	view_begin(e);
	e->view.rx = e->call;
	e->view.rx_active = true;
	e->view.rx_played = play;
	if (start)
		e->view.rx_calls++;
	view_end(e);
	if (play && !e->played) {
		e->played = true;
		if (e->hooks.rx_start)
			e->hooks.rx_start(&e->call, e->ctx);
	} else if (play) {
		if (e->hooks.rx_update)
			e->hooks.rx_update(&e->call, e->ctx);
	} else if (e->played) {
		// A field that came in late says the call is not for us after all
		e->played = false;
		if (e->hooks.rx_end)
			e->hooks.rx_end(e->ctx);
	}
}

static void start_call(ysf_engine_t *e, const ysf_rx_frame_t *f, bool late)
{
	memset(&e->call, 0, sizeof e->call);
	e->call.fich = f->fich;
	e->call.late = late;
	e->call.frames = 1;
	e->played = false;
	merge_dch(e, f);
}

static void end_call(ysf_engine_t *e)
{
	view_begin(e);
	e->view.rx_active = false;
	e->view.rx_played = false;
	view_end(e);
	if (e->played && e->hooks.rx_end)
		e->hooks.rx_end(e->ctx);
	e->played = false;
}

static void on_frame(const ysf_rx_frame_t *f, void *ctx)
{
	ysf_engine_t *e = ctx;
	bool start;

	view_begin(e);
	e->view.rx_frames++;
	view_end(e);
	// A DG-ID the squelch does not take ends the call in hand: it is a
	// different transmission, not a continuation of this one.
	if (!wanted(e, &f->fich)) {
		view_begin(e);
		e->view.rx_rejects++;
		view_end(e);
		if (e->view.rx_active)
			end_call(e);
		return;
	}
	if (f->fich.fi == YSF_FI_TEST)
		return;
	if (e->recheck) {
		e->recheck = false;
		if (e->view.rx_active && !wanted(e, &e->call.fich))
			end_call(e);
	}
	// A different DG-ID or voice format starts a new call.
	if (e->view.rx_active && (f->fich.sc != e->call.fich.sc ||
				 f->fich.dt != e->call.fich.dt))
		end_call(e);
	switch (f->fich.fi) {
	case YSF_FI_HEADER:
		if (e->view.rx_active)
			end_call(e);
		start_call(e, f, false);
		rx_fields(e, true);
		break;
	case YSF_FI_COMMUNICATION:
		start = !e->view.rx_active;
		if (start) {
			start_call(e, f, true);
			rx_fields(e, true);
		} else {
			e->call.fich = f->fich;
			e->call.frames++;
			merge_dch(e, f);
		}
		data_units(e, f);
		voice_blocks(e, f);
		if (!start)
			rx_fields(e, false);
		break;
	case YSF_FI_TERMINATOR:
		if (e->view.rx_active) {
			e->call.fich = f->fich;
			e->call.frames++;
			merge_dch(e, f);
			rx_fields(e, false);
			end_call(e);
		}
		break;
	default:
		break;
	}
}

static void on_lost(void *ctx)
{
	ysf_engine_t *e = ctx;

	if (e->view.rx_active)
		end_call(e);
}

static const ysf_rx_sink_t sink = { on_frame, on_lost };

// ── Transmit ──
// The sequence builder's voice callback: the staged 9-byte AMBE frames
// become the frame's voice blocks. V/D-1 takes them as they are, DN converts
// them, and VW protects and interleaves full-rate IMBE.
static void tx_voice_cb(void *ctx, unsigned frame_no, unsigned block, uint8_t *vch, unsigned vch_len)
{
	ysf_engine_t *e = ctx;

	(void)frame_no;
	switch (e->cfg.dt) {
	case YSF_DT_VD_MODE1:
		if (vch_len == 9)
			memcpy(vch, e->buf.tx.stage[block], 9);
		break;
	case YSF_DT_VD_MODE2:
		if (vch_len == 13)
			ysf_voice_dn_from_frame(e->buf.tx.stage[block], vch);
		break;
	case YSF_DT_VOICE_FR:
		ysf_voice_vw_encode(e->buf.tx.stage[e->txcfg.vw_subheader ? block - YSF_VW_SUBHEADER_FIRST_VOICE : block], vch);
		break;
	default:
		if (vch_len)
			memset(vch, 0, vch_len);
		break;
	}
}

static void tx_data_wrap(void *ctx, unsigned index, uint8_t out[20])
{
	ysf_engine_t *e = ctx;

	if (e->hooks.tx_data)
		e->hooks.tx_data(e->ctx, index, out);
	else
		memset(out, 0, 20);
}

// Pull every voice block the encoder has made into the staging ring (two
// frames). The encoder makes blocks at real-time rate, a frame needs five
// at once, and the ring is what lets the two rates meet: tx_fill tops it up
// on every call, so a frame is built from blocks collected over time, not
// from whatever the encoder happened to have made at that instant.
static void top_up(ysf_engine_t *e)
{
	bool vw = e->cfg.dt == YSF_DT_VOICE_FR;
	bool (*next)(uint8_t *, void *) = vw ? e->hooks.tx_vw : half_rate(e->cfg.dt) ? e->hooks.tx_voice : NULL;

	if (!next)
		return;
	while (e->sq_n < 10) {
		uint8_t *dst = e->buf.tx.sq[(e->sq_head + e->sq_n) % 10];

		if (!next(dst, e->ctx))
			return;
		e->sq_n++;
	}
}

// Stage the five voice blocks of the next communication frame. Blocks
// already staged for the frame in hand stay staged (stage_n): a wait is
// resumed, never re-fetched, so the encoder's blocks are taken once, in
// order. A block the encoder has not made yet is waited for as long as the
// air can spare: once less than the provider's lead plus one more chunk is
// queued, silence goes instead. Before the transmitter is on nothing is
// lost by waiting.
static bool stage_voice(ysf_engine_t *e)
{
	const hal_air_t *air = e->session.air;
	uint16_t lead = (uint16_t)(((uint32_t)air->tx_lead_ms * e->mode->profile.sym_rate) / 1000 +
				   air->tx_chunk_syms);
	bool must = air_session_tx_state(&e->session) != AIR_TX_PRIMING &&
		    air_session_tx_queued(&e->session) < lead;

	bool vw = e->cfg.dt == YSF_DT_VOICE_FR;
	unsigned blocks = e->txcfg.vw_subheader ?
		YSF_BLOCKS - YSF_VW_SUBHEADER_FIRST_VOICE : YSF_BLOCKS;
	unsigned size = vw ? FEC_IMBE88_BYTES : 9;
	const uint8_t *silence = vw ? e->cfg.silence_vw : e->cfg.silence;
	bool source = vw ? e->hooks.tx_vw != NULL : e->hooks.tx_voice != NULL;

	for (; e->stage_n < blocks; e->stage_n++) {
		unsigned b = e->stage_n;

		if (e->sq_n) {
			memcpy(e->buf.tx.stage[b], e->buf.tx.sq[e->sq_head], size);
			e->sq_head = (uint8_t)((e->sq_head + 1) % 10);
			e->sq_n--;
			continue;
		}
		if (!source || must) {
			memcpy(e->buf.tx.stage[b], silence, size);
			view_begin(e);
			e->view.tx_filler++;
			view_end(e);
		} else {
			return false;	// wait: the queue is deep enough to coast
		}
	}
	e->stage_n = 0;
	return true;
}

// Pack a built frame's bytes into dibits (4 per byte, MSB first), the form
// the session queues.
static void pack_dibits(const uint8_t *frame, uint8_t *packed)
{
	memset(packed, 0, YSF_FRAME_DIBITS / 4);
	for (unsigned i = 0; i < YSF_FRAME_DIBITS; i++)
		air_sym_put(packed, (uint16_t)i, (uint8_t)((frame[i / 4] >> (6 - 2 * (i % 4))) & 3u));
}

// Build the next sequence frame: 0 header, 1..8 communication (FN cycles
// 0..7, repeated while PTT is held), 9 terminator. False when the frame
// waits for voice. After the key, a communication frame is still built while
// voice the encoder already made waits in the ring: the tail of the over is
// sent, not dropped, and the terminator follows once the ring is empty. The
// tail is at most the ring (10 blocks in sq, 5 staged: 3 frames), so past the
// bound below the voice source is still feeding, and waiting for it would
// hold the transmitter on until the radio is switched off: the terminator is
// forced and the rest of the ring is dropped. A normal over drains in at most
// 3 frames, so the bound never fires there.
#define YSF_TX_STOP_MAX_FRAMES	10u	// communication frames built after the key, at most
static bool build_next(ysf_engine_t *e)
{
	unsigned seq;

	if (e->frame_no == 0)
		seq = 0;	// header
	else if (e->tx_stop && !e->tx_ended &&
		 ((e->sq_n == 0 && e->stage_n == 0) || e->stop_frames >= YSF_TX_STOP_MAX_FRAMES))
		seq = 9;	// terminator: no voice left to drain, or the drain is stuck
	else
		seq = e->frame_no;	// 1..8, communication
	if (seq >= 1 && seq <= 8 &&
	    (half_rate(e->cfg.dt) || e->cfg.dt == YSF_DT_VOICE_FR) && !stage_voice(e))
		return false;
	if (!ysf_tx_frame_build(&e->txcfg, seq, e->buf.tx.txframe))
		return false;	// unknown mode: nothing to send
	pack_dibits(e->buf.tx.txframe, e->buf.tx.dibits);
	e->dib_pos = 0;
	if (seq == 0) {
		e->frame_no = 1;
		e->tx_started = true;
	} else if (seq == 9) {
		e->tx_ended = true;
	} else {
		e->txcfg.vw_subheader = false;
		e->frame_no = e->frame_no >= 8 ? 1 : e->frame_no + 1;
		if (e->tx_stop && e->stop_frames < 255)
			e->stop_frames++;
	}
	return true;
}

// Symbols to the session while its queue has room
static void tx_fill(void *ctx)
{
	ysf_engine_t *e = ctx;

	top_up(e);
	for (;;) {
		uint16_t space, n;

		if (e->tx_ended && e->dib_pos >= YSF_FRAME_DIBITS) {
			air_session_tx_finish(&e->session);
			return;
		}
		if (e->dib_pos >= YSF_FRAME_DIBITS && !build_next(e))
			return;
		space = air_session_tx_space(&e->session);
		if (!space)
			return;
		n = (uint16_t)(YSF_FRAME_DIBITS - e->dib_pos);
		if (n > space)
			n = space;
		if (!air_session_tx(&e->session, e->buf.tx.dibits, e->dib_pos, n))
			return;
		e->dib_pos += n;
	}
}

static const air_session_ops_t ops = { tx_fill, NULL };

// The sequence builder's config follows the engine's: cs/sql/voip are 0
// (RF, DG-ID squelch on), cycles is 1 (the engine repeats the CC block
// itself while PTT is held).
static void refresh_txcfg(ysf_engine_t *e)
{
	e->txcfg.dt = e->cfg.dt;
	e->txcfg.dev = e->cfg.narrow ? 1 : 0;
	e->txcfg.cm = e->cfg.cm;
	e->txcfg.mr = e->cfg.mr;
	e->txcfg.cs = 0;
	e->txcfg.sql = 0;
	e->txcfg.voip = 0;
	e->txcfg.dgid = e->cfg.tx_dgid;
	memcpy(e->txcfg.dest, e->cfg.dest, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.src, e->cfg.src, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.down, e->cfg.down, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.up, e->cfg.up, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.rem1, e->cfg.rem1, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.rem2, e->cfg.rem2, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.rem3, e->cfg.rem3, YSF_CALLSIGN_LEN);
	memcpy(e->txcfg.rem4, e->cfg.rem4, YSF_CALLSIGN_LEN);
}

bool ysf_engine_begin(ysf_engine_t *e, const hal_air_t *air, const ysf_engine_cfg_t *cfg,
		      const ysf_engine_hooks_t *hooks, void *ctx)
{
	memset(e, 0, sizeof *e);
	e->cfg = *cfg;
	e->hooks = *hooks;
	e->ctx = ctx;
	e->mode = ysf_engine_mode(cfg->narrow);
	e->txcfg.data = tx_data_wrap;
	e->txcfg.voice = tx_voice_cb;
	e->txcfg.ctx = e;
	e->txcfg.cycles = 1;
	refresh_txcfg(e);
	ysf_rx_init(&e->rx, &sink, e);
	return air_session_open(&e->session, air, e->mode, &e->rx, e->buf.rx.frame, e->buf.rx.soft, YSF_FRAME_DIBITS,
				&ops, e);
}

void ysf_engine_end(ysf_engine_t *e)
{
	air_session_close(&e->session);
}

void ysf_engine_retuned(ysf_engine_t *e)
{
	air_session_retuned(&e->session);
}

void ysf_engine_set(ysf_engine_t *e, const ysf_engine_cfg_t *cfg)
{
	bool narrow = e->cfg.narrow;
	uint8_t dt = e->cfg.dt;

	e->cfg = *cfg;
	e->cfg.narrow = narrow;	// the mode is the session's: a new deviation is a new session
	e->cfg.dt = dt;		// ... and so is a new mode (the voice path follows it)
	refresh_txcfg(e);
	e->recheck = true;
}

bool ysf_engine_tx_prepare(ysf_engine_t *e)
{
	const uint8_t *silence = e->cfg.dt == YSF_DT_VOICE_FR ? e->cfg.silence_vw : e->cfg.silence;

	if (!air_session_is_open(&e->session) || !(e->session.fit & AIR_FIT_TX) || !silence)
		return false;
	if (air_session_tx_state(&e->session) == AIR_TX_IDLE) {
		e->frame_no = 0;
		e->dib_pos = YSF_FRAME_DIBITS;
		e->tx_started = false;
		e->tx_stop = false;
		e->tx_ended = false;
		e->stop_frames = 0;
		e->stage_n = 0;	// a new over: no staged frame, no queued blocks
		e->sq_head = 0;
		e->sq_n = 0;
		refresh_txcfg(e);
		e->txcfg.vw_subheader = e->cfg.dt == YSF_DT_VOICE_FR;
	}
	return air_session_tx_prime(&e->session);
}

bool ysf_engine_tx_go(ysf_engine_t *e)
{
	if (!air_session_tx_go(&e->session))
		return false;
	e->view.tx_calls++;
	return true;
}

void ysf_engine_tx_stop(ysf_engine_t *e)
{
	e->tx_stop = true;
}

void ysf_engine_tx_abort(ysf_engine_t *e)
{
	air_session_tx_abort(&e->session);
}

air_tx_state_t ysf_engine_tx_state(const ysf_engine_t *e)
{
	return air_session_tx_state(&e->session);
}

void ysf_engine_tx_clear(ysf_engine_t *e)
{
	air_session_tx_clear(&e->session);
}

void ysf_engine_view(const ysf_engine_t *e, ysf_engine_view_t *out)
{
	uint8_t s;

	do {
		s = e->seq;
		*out = e->view;
	} while ((s & 1) || s != e->seq);
	out->tx = air_session_tx_state(&e->session);
}
