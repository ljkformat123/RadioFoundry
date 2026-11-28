// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional on a channel, the radio core's side (radio_ysf.h)
#include "config.h"
#if CONFIG_YSF	// the whole file
#include <stddef.h>
#include <string.h>
#include "board.h"
#include "radio/radio_ysf.h"
#include "radio/radio_bands.h"
#include "ysf/ysf_engine.h"
#include "ysf/ysf_codeplug.h"
#include "codeplug/codeplug_modes.h"
#include "dmr/ambe/ambeVoice.h"
#include "dmr/call/dmr_call.h"
#if CONFIG_P25 && CONFIG_IMBE
#include "p25/p25Voice.h"
#endif
#if CONFIG_P25 && !defined(HAL_VOCODER_LEND_BYTES)
#include "radio/radio_p25.h"
#endif

#if CONFIG_P25 && !defined(HAL_VOCODER_LEND_BYTES)
// P25's memory is idle while YSF runs (one air mode at a time): no copy of our own
#define YSF_MEM_SHARED	1
#else
#define YSF_MEM_SHARED	0
static ysf_engine_t own_mem;
#endif

static const radio_ysf_settings_t defaults = {
	.dt = YSF_DT_VD_MODE1, .narrow = false, .rx_dgid = 0, .tx_dgid = 0,
	.dest = YSF_CP_DEST_ALL, .src = "BG5FLU", .down = "", .up = "",
};
static radio_ysf_settings_t set;
static ysf_engine_t *eng;		// while running
static radio_ysf_stats_t stats;
static uint32_t rx_calls_base, tx_calls_base, filler_base, rejects_base, frames_base, voice_base;
static air_session_stats_t air_base;

void radio_ysf_init(void)
{
	set = defaults;
	eng = NULL;
}

void radio_ysf_settings(radio_ysf_settings_t *out)
{
	*out = set;
}

void radio_ysf_set_settings(const radio_ysf_settings_t *s)
{
	set = *s;
	if (set.dt > YSF_DT_VOICE_FR)
		set.dt = YSF_DT_VD_MODE1;
	if (set.rx_dgid > RADIO_YSF_DGID_MAX)
		set.rx_dgid = 0;
	if (set.tx_dgid > RADIO_YSF_DGID_MAX)
		set.tx_dgid = 0;
	if (set.dest[0] == '\0')
		strcpy(set.dest, YSF_CP_DEST_ALL);
}

uint32_t radio_ysf_fit(bool narrow)
{
	return board.air != NULL ? ysf_engine_fit(board.air, narrow) : 0;
}

bool radio_ysf_fits(void)
{
	return ((radio_ysf_fit(false) | radio_ysf_fit(true)) & (AIR_FIT_RX | AIR_FIT_TX)) != 0;
}

bool radio_ysf_take(channel_t *ch)
{
	if (ch->kind != CODEPLUG_KIND_YSF || !radio_ysf_fits() || radio_listen_band(ch->rx_hz))
		return false;
	ch->mode = RADIO_MODE_YSF;
	ch->bw = RADIO_BW_12K5;
	return true;
}

bool radio_ysf_receives(const channel_t *ch, uint16_t number)
{
	uint32_t wide = radio_ysf_fit(false) & AIR_FIT_RX, narrow = radio_ysf_fit(true) & AIR_FIT_RX;
	ysf_conv_cfg_t cc;

	if (wide == narrow)
		return wide != 0;	// the deviation doesn't matter: no channel record to read
	return (number != 0 && ysf_conv_load(number, ch, &cc) ? cc.conv.narrow : set.narrow) ? narrow != 0 : wide != 0;
}

// The engine's configuration for channel ch at number (0: the VFO): the one place YSF parameters come from. A live
// stored YSF channel's tag and record give them (docs/spec/ysf-codeplug.md 2), else the settings
static bool monitor;		// digital monitor: radio_ysf_monitor

void radio_ysf_monitor(bool on)
{
	monitor = on;
}

static void params_channel(const channel_t *ch, uint16_t number, ysf_engine_cfg_t *c);

static void params(const channel_t *ch, uint16_t number, ysf_engine_cfg_t *c)
{
	params_channel(ch, number, c);
	if (monitor)
		c->rx_dgid = 0;		// every DG-ID; what is sent stays
}

static void callsigns_to_cfg(const radio_ysf_settings_t *s, const ysf_identity_t *id, ysf_engine_cfg_t *c)
{
	// A stored channel's call signs come from the radio-wide record; the VFO's from the settings
	if (id != NULL) {
		ysf_callsign_pack(id->src, (uint8_t *)c->src);
		ysf_callsign_pack(id->down, (uint8_t *)c->down);
		ysf_callsign_pack(id->up, (uint8_t *)c->up);
	} else {
		ysf_callsign_pack(s->src, (uint8_t *)c->src);
		ysf_callsign_pack(s->down, (uint8_t *)c->down);
		ysf_callsign_pack(s->up, (uint8_t *)c->up);
	}
	memset(c->rem1, ' ', YSF_CALLSIGN_LEN);
	memset(c->rem2, ' ', YSF_CALLSIGN_LEN);
	memset(c->rem3, ' ', YSF_CALLSIGN_LEN);
	memset(c->rem4, ' ', YSF_CALLSIGN_LEN);
}

static void params_channel(const channel_t *ch, uint16_t number, ysf_engine_cfg_t *c)
{
	ysf_conv_cfg_t cc;

	memset(c, 0, sizeof(*c));
	c->silence = dmr_ambe_silence;
	c->cm = YSF_CM_GROUP;
	c->mr = YSF_MR_DIRECT;
	if (number != 0 && ysf_conv_load(number, ch, &cc)) {
		c->dt = cc.conv.dt;
		c->narrow = cc.conv.narrow;
		c->rx_dgid = cc.conv.rx_dgid;
		c->tx_dgid = cc.conv.tx_dgid;
		ysf_callsign_pack(cc.conv.dest, (uint8_t *)c->dest);
		callsigns_to_cfg(NULL, &cc.id, c);
		return;
	}
	c->dt = set.dt;
	c->narrow = set.narrow;
	c->rx_dgid = set.rx_dgid;
	c->tx_dgid = set.tx_dgid;
	ysf_callsign_pack(set.dest, (uint8_t *)c->dest);
	callsigns_to_cfg(&set, NULL, c);
}

// ── Voice: the DMR side's voice path (ambeVoice.h), its sink on receive and its source on transmit. The hooks run in
// the provider's task, which is the only one feeding that path while YSF runs (the DMR stack is stopped); the
// audio stream is started and stopped by the radio task (begin, end). Data units have no sink yet: they are dropped
// on receive and sent zeroed ──
static void v_start(const ysf_call_t *call, void *ctx)
{
	const dmr_voice_sink_t *s = ambeVoiceSink();

	(void)call;
	(void)ctx;
	s->begin(NULL, s->ctx);
}

static void v_voice(const uint8_t vch[9], void *ctx)
{
	const dmr_voice_sink_t *s = ambeVoiceSink();

	(void)ctx;
	s->frame(vch, s->ctx);
}

static void v_end(void *ctx)
{
	const dmr_voice_sink_t *s = ambeVoiceSink();

	(void)ctx;
	s->end(s->ctx);
}

static bool v_tx(uint8_t vch[9], void *ctx)
{
	const dmr_voice_source_t *s = ambeVoiceSource();

	(void)ctx;
	return s->next(vch, s->ctx);
}

static const ysf_engine_hooks_t voice_hooks = { .rx_start = v_start, .rx_voice = v_voice, .rx_end = v_end,
						 .tx_voice = v_tx };

// The encoder runs from the header on, so the first voice frames are ready when the engine asks
static void voice_tx(bool on)
{
#if CONFIG_P25 && CONFIG_IMBE
	if (eng->cfg.dt == YSF_DT_VOICE_FR) {
		p25VoiceTx(on);
		return;
	}
#endif
	const dmr_voice_source_t *s = ambeVoiceSource();

	if (on)
		s->begin(s->ctx);
	else
		s->end(s->ctx);
}

#if CONFIG_P25 && CONFIG_IMBE
// VW and AMBE+2 are exclusive on the DM-32UV: the IMBE voice path borrows the AMBE vocoder's state memory.
static p25_voice_mem_t *vw_mem_get(void)
{
	size_t len = 0;
#ifdef HAL_VOCODER_LEND_BYTES
	_Static_assert(sizeof(p25_voice_mem_t) <= HAL_VOCODER_LEND_BYTES, "VW voice exceeds vocoder memory");
	void *m = board.vocoder != NULL && board.vocoder->lend != NULL ? board.vocoder->lend(&len) : NULL;
#else
	size_t offset = (sizeof(ysf_engine_t) + 7u) & ~(size_t)7u;
	void *m = radio_p25_idle_mem(&len);

	if (m == NULL || len < offset)
		return NULL;
	m = (uint8_t *)m + offset;
	len -= offset;
#endif
	return m != NULL && len >= sizeof(p25_voice_mem_t) && ((uintptr_t)m & 7u) == 0 ? m : NULL;
}

static bool vw_rx_open;	// provider task, while it owns the YSF session

static void vw_start(const ysf_call_t *call, void *ctx)
{
	vw_rx_open = call->fich.dt == YSF_DT_VOICE_FR;
	if (vw_rx_open)
		p25VoiceHooks()->rx_start(NULL, ctx);
}

static void vw_end(void *ctx)
{
	if (vw_rx_open)
		p25VoiceHooks()->rx_end(ctx);
	vw_rx_open = false;
}

static void vw_voice(const uint8_t imbe[FEC_IMBE88_BYTES], uint8_t errs, void *ctx)
{
	p25VoiceHooks()->rx_voice(imbe, errs, ctx);
}

static bool vw_next(uint8_t imbe[FEC_IMBE88_BYTES], void *ctx)
{
	return p25VoiceHooks()->tx_voice(imbe, ctx);
}

static const ysf_engine_hooks_t vw_hooks = { .rx_start = vw_start, .rx_end = vw_end,
					      .rx_vw = vw_voice, .tx_vw = vw_next };
#endif

static void voice_end(bool vw)
{
#if CONFIG_P25 && CONFIG_IMBE
	if (vw) {
		p25VoiceEnd();
		return;
	}
#else
	(void)vw;
#endif
	ambeVoiceStop();
}

static ysf_engine_t *mem_get(void)
{
#if YSF_MEM_SHARED
	size_t len = 0;
	void *m = radio_p25_idle_mem(&len);

	if (m == NULL || len < sizeof(ysf_engine_t) || ((uintptr_t)m & 7u) != 0)
		return NULL;
	return m;
#else
	return &own_mem;
#endif
}

bool radio_ysf_begin(const channel_t *ch, uint16_t number)
{
	ysf_engine_cfg_t cfg;

	radio_ysf_end();
	stats.begins++;
	params(ch, number, &cfg);
	eng = mem_get();
	if (eng == NULL || board.air == NULL || ysf_engine_fit(board.air, cfg.narrow) == 0) {
		eng = NULL;
		stats.begin_fails++;
		return false;
	}
	const ysf_engine_hooks_t *hooks = &voice_hooks;
	bool vw = cfg.dt == YSF_DT_VOICE_FR;

	if (vw) {
#if CONFIG_P25 && CONFIG_IMBE
		p25_voice_mem_t *mem = vw_mem_get();

		if (mem != NULL) {
			vw_rx_open = false;
			p25VoiceBegin(mem, YSF_BLOCKS);
			cfg.silence_vw = imbe_silence;
			hooks = &vw_hooks;
		}
#endif
		if (!cfg.silence_vw) {
			eng = NULL;
			stats.begin_fails++;
			return false;
		}
	} else {
		ambeVoiceStart();
	}
	if (!ysf_engine_begin(eng, board.air, &cfg, hooks, NULL)) {
		voice_end(vw);
		eng = NULL;
		stats.begin_fails++;
		return false;
	}
	rx_calls_base = tx_calls_base = filler_base = rejects_base = frames_base = voice_base = 0;
	memset(&air_base, 0, sizeof(air_base));
	return true;
}

void radio_ysf_end(void)
{
	if (eng == NULL)
		return;
	ysf_engine_end(eng);	// the provider closed: no hook runs after this
	voice_end(eng->cfg.dt == YSF_DT_VOICE_FR);
	eng = NULL;
}

bool radio_ysf_running(void)
{
	return eng != NULL;
}

void radio_ysf_retuned(void)
{
	if (eng != NULL)
		ysf_engine_retuned(eng);
}

void radio_ysf_apply(const channel_t *ch, uint16_t number)
{
	ysf_engine_cfg_t cfg;

	if (eng == NULL || ysf_engine_tx_state(eng) != AIR_TX_IDLE)
		return;
	params(ch, number, &cfg);
	if (cfg.dt != eng->cfg.dt || cfg.narrow != eng->cfg.narrow) {
		radio_ysf_begin(ch, number);	// the mode is the session's: a new data type or deviation is a new session
		return;
	}
	cfg.silence_vw = eng->cfg.silence_vw;	// the voice path's memory stays lent
	ysf_engine_set(eng, &cfg);
}

// The engine's counters into ours; they restart with each engine, so ours add what moved since the last look
static void count(const ysf_engine_view_t *ev)
{
	const air_session_stats_t *a = &eng->session.stats;

	stats.rx_calls += ev->rx_calls - rx_calls_base;
	stats.tx_calls += ev->tx_calls - tx_calls_base;
	stats.tx_filler += ev->tx_filler - filler_base;
	stats.rx_rejects += ev->rx_rejects - rejects_base;
	stats.frames += ev->rx_frames - frames_base;
	stats.voice += ev->rx_voice_frames - voice_base;
	rx_calls_base = ev->rx_calls;
	tx_calls_base = ev->tx_calls;
	filler_base = ev->tx_filler;
	rejects_base = ev->rx_rejects;
	frames_base = ev->rx_frames;
	voice_base = ev->rx_voice_frames;
	stats.rx_chunks += a->rx_chunks - air_base.rx_chunks;
	stats.tx_underruns += a->tx_underruns - air_base.tx_underruns;
	stats.tx_syms += a->tx_syms - air_base.tx_syms;
	air_base = *a;
}

// The engine's configuration as settings: what the screen, the view and the debug link show
static void cfg_settings(const ysf_engine_cfg_t *c, radio_ysf_settings_t *out)
{
	out->dt = c->dt;
	out->narrow = c->narrow;
	out->rx_dgid = c->rx_dgid;
	out->tx_dgid = c->tx_dgid;
	ysf_callsign_unpack((const uint8_t *)c->dest, out->dest);
	ysf_callsign_unpack((const uint8_t *)c->src, out->src);
	ysf_callsign_unpack((const uint8_t *)c->down, out->down);
	ysf_callsign_unpack((const uint8_t *)c->up, out->up);
	if (out->dest[0] == '\0')
		strcpy(out->dest, YSF_CP_DEST_ALL);
}

bool radio_ysf_in_force(radio_ysf_settings_t *out)
{
	ysf_engine_t *e = eng;

	if (e == NULL)
		return false;
	cfg_settings(&e->cfg, out);
	return true;
}

// What the screen shows of the parameters: the DG-IDs, the data type and the deviation, what PTT calls. Those in
// force while the engine runs (a stored channel's own), else the settings
static void settings_view(radio_ysf_view_t *n)
{
	radio_ysf_settings_t s = set;

	if (eng != NULL)
		cfg_settings(&eng->cfg, &s);
	n->rx_dgid = s.rx_dgid;
	n->tx_dgid = s.tx_dgid;
	n->fit = (uint16_t)radio_ysf_fit(s.narrow);
	strcpy(n->talk, s.dest);
	if (s.narrow)
		n->flags |= RADIO_YSF_NARROW;
}

bool radio_ysf_poll(radio_ysf_view_t *v)
{
	ysf_engine_view_t ev;
	radio_ysf_view_t n;

	if (eng == NULL) {
		n = *v;		// the last call heard stays
		n.flags = 0;
		n.tx = AIR_TX_IDLE;
	} else {
#if CONFIG_P25 && CONFIG_IMBE
		if (eng->cfg.dt == YSF_DT_VOICE_FR)
			p25VoicePoll();
#endif
		ysf_engine_view(eng, &ev);
		count(&ev);
		memset(&n, 0, sizeof(n));
		n.flags = RADIO_YSF_ON;
		if (ev.rx_active) {
			n.flags |= RADIO_YSF_RX;
			bool vw = eng->cfg.dt == YSF_DT_VOICE_FR;
			bool voice = vw ? ev.rx.fich.dt == YSF_DT_VOICE_FR :
				ev.rx.fich.dt == YSF_DT_VD_MODE1 || ev.rx.fich.dt == YSF_DT_VD_MODE2;

			if (ev.rx_played && voice)
				n.flags |= RADIO_YSF_PLAYED;
		}
		if (ev.rx.late)
			n.flags |= RADIO_YSF_LATE;
		n.tx = ev.tx;
		n.dgid = ev.rx.fich.sc;
		n.dt = ev.rx.fich.dt;
		strcpy(n.dest, ev.rx.dest);
		strcpy(n.src, ev.rx.src);
	}
	settings_view(&n);
	bool changed = memcmp(&n, v, sizeof(n)) != 0;
	*v = n;
	return changed;
}

static bool ysf_no_tx(void)
{
	return eng != NULL && (eng->session.fit & AIR_FIT_TX) == 0;
}

static bool ysf_tx_prepare(void)
{
	if (eng == NULL || !ysf_engine_tx_prepare(eng))
		return false;
	// DMR restarts the voice stream for every call; YSF runs it since the channel was selected, so a beep or a
	// stream takeover can leave it stale or dead (no callback, the speaker route on, the mic flat) and every over
	// goes out silent. Restart it here, so each over starts with a live callback and a fresh mic route;
	// voice_tx(true) starts it again below, through source_begin's ensure_streaming.
	if (eng->cfg.dt != YSF_DT_VOICE_FR) {
		ambeVoiceStop();
		ambeVoiceStart();
	}
	voice_tx(true);
	return true;
}

static bool ysf_tx_go(void)
{
	return eng != NULL && ysf_engine_tx_go(eng);
}

static void ysf_tx_stop(void)
{
	if (eng != NULL) {
		voice_tx(false);	// the engine fills the frame on air with silence
		ysf_engine_tx_stop(eng);
	}
}

static void ysf_tx_abort(void)
{
	if (eng != NULL) {
		voice_tx(false);
		ysf_engine_tx_abort(eng);
	}
}

static uint8_t ysf_tx_state(void)
{
	return eng != NULL ? (uint8_t)ysf_engine_tx_state(eng) : (uint8_t)AIR_TX_IDLE;
}

static void ysf_tx_clear(void)
{
	if (eng != NULL) {
		voice_tx(false);
		ysf_engine_tx_clear(eng);
	}
}

const radio_air_ops_t radio_ysf_air = {
	radio_ysf_running, ysf_no_tx, ysf_tx_prepare, ysf_tx_go, ysf_tx_stop, ysf_tx_abort, ysf_tx_state,
	ysf_tx_clear,
};

void radio_ysf_stats(radio_ysf_stats_t *out)
{
	*out = stats;
}

void radio_ysf_reset_stats(void)
{
	memset(&stats, 0, sizeof(stats));
}

#endif	// CONFIG_YSF
