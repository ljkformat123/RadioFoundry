// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional on a channel, the radio core's side (docs/spec/ysf.md 8): the engine (core/ysf/ysf_engine.h)
// on the memory it runs in, its voice frames to and from the vocoder the board hosts for DMR (core/dmr/ambe), the
// channel's YSF parameters, and a compact view for the radio view. radio_sm.c decides when YSF runs and keys the
// transmitter (radio_air.h); nothing here keys it.
//
// The parameters of a stored YSF channel are its own (docs/spec/ysf-codeplug.md); the VFO and a channel whose mode
// was changed to YSF for now take the RAM settings the debug link changes (radio_req_ysf_settings).
#ifndef RADIO_YSF_H
#define RADIO_YSF_H

#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "codeplug/codeplug.h"
#include "ysf/ysf_frame.h"

// The YSF settings (RAM only, reset at start-up; ysf.md 8.2): the VFO's, and the shape of what is in force
typedef struct {
	uint8_t dt;		// YSF_DT_* (0 V/D mode 1, 1 Data FR, 2 V/D mode 2 (DN), 3 Voice FR (VW))
	bool narrow;		// Narrow deviation (1350 Hz) instead of Wide (2700 Hz)
	uint8_t rx_dgid;	// 0 (any DG-ID) to 127
	uint8_t tx_dgid;	// 0 (none) to 127
	char dest[YSF_CALLSIGN_LEN + 1];	// the destination call sign ("ALL" when unset)
	char src[YSF_CALLSIGN_LEN + 1];	// this radio's call sign, empty when unset
	char down[YSF_CALLSIGN_LEN + 1];	// the downlink call sign, empty when unset
	char up[YSF_CALLSIGN_LEN + 1];	// the uplink call sign, empty when unset
} radio_ysf_settings_t;

#define RADIO_YSF_DGID_MAX	127

// view flags
#define RADIO_YSF_ON		0x01u	// the engine runs on this channel
#define RADIO_YSF_RX		0x02u	// a call is being received
#define RADIO_YSF_PLAYED	0x04u	// ... and passes the squelch (its voice plays)
#define RADIO_YSF_LATE		0x08u	// ... was entered without its header
#define RADIO_YSF_NARROW	0x10u	// the deviation in force is Narrow, else Wide

// The radio view's part (radio_view_t.ysf): the call heard now, or the last one
typedef struct {
	uint8_t flags;		// RADIO_YSF_*
	uint8_t tx;		// air_tx_state_t of the engine
	uint8_t dgid;		// the DG-ID of the call heard (FICH SC)
	uint8_t dt;		// its data type (YSF_DT_*)
	uint8_t rx_dgid;	// the settings' (0: any)
	uint8_t tx_dgid;
	uint16_t fit;		// what the board's provider gives this deviation (AIR_FIT_*), 0: nothing
	char dest[YSF_CALLSIGN_LEN + 1];	// the call heard's destination call sign
	char src[YSF_CALLSIGN_LEN + 1];	// its source call sign
	char talk[YSF_CALLSIGN_LEN + 1];	// what PTT calls (the settings' destination)
} radio_ysf_view_t;

// Counters for the debug link (any task reads them; the radio task writes them while YSF runs)
typedef struct {
	uint32_t rx_calls, tx_calls, tx_filler, rx_rejects;	// the engine's
	uint32_t frames, voice;		// the frames decoded and the voice blocks played
	uint32_t rx_chunks, tx_underruns, tx_syms;	// the session's
	uint32_t begins, begin_fails;	// engine starts, and the ones the provider refused (no provider, no fit, no memory)
} radio_ysf_stats_t;

#if CONFIG_YSF
#include "radio/radio_air.h"

extern const radio_air_ops_t radio_ysf_air;

void radio_ysf_init(void);
// The board's provider gives YSF something (receive or transmit) at either deviation (any task): what the UI and the
// mode request offer YSF on
bool radio_ysf_fits(void);
// What the provider gives the deviation (AIR_FIT_*; any task)
uint32_t radio_ysf_fit(bool narrow);
// A channel read from the codeplug, as the radio runs it: a live YSF channel (kind 6) becomes RADIO_MODE_YSF where
// the board carries YSF and its frequency allows a digital mode. False: left as read (parked)
bool radio_ysf_take(channel_t *ch);
// The board's provider receives channel ch at number (0: the VFO) at its deviation (a stored channel's own, else the
// settings'): what scan and dual watch visit
bool radio_ysf_receives(const channel_t *ch, uint16_t number);
void radio_ysf_settings(radio_ysf_settings_t *out);		// any task
void radio_ysf_set_settings(const radio_ysf_settings_t *s);	// radio task (RREQ_YSF_SET)

// The parameters in force, as settings: false when the engine isn't running. Any task: a read that crosses a change
// of channel may mix the two (diagnostics only)
bool radio_ysf_in_force(radio_ysf_settings_t *out);

// Radio task. begin: the board tuned in its digital mode, its DMR stack stopped; the audio stream starts. number: the
// stored channel ch is (0: the VFO, or none): a live YSF channel's tag gives the parameters, else the settings. False:
// no memory, no provider or no fit at the deviation (YSF then receives nothing and refuses PTT)
bool radio_ysf_begin(const channel_t *ch, uint16_t number);
void radio_ysf_end(void);
bool radio_ysf_running(void);
void radio_ysf_retuned(void);
// The settings changed: the engine takes them (not while transmitting). A new data type or deviation starts the
// engine over
void radio_ysf_apply(const channel_t *ch, uint16_t number);
// Digital monitor (ysf.md 8.5): while on, the squelch hears every DG-ID, whatever the channel or the settings say.
// Taken by the next begin or apply
void radio_ysf_monitor(bool on);
// Every tick: the view and the counters. True when the view changed
bool radio_ysf_poll(radio_ysf_view_t *v);

void radio_ysf_stats(radio_ysf_stats_t *out);
void radio_ysf_reset_stats(void);
#endif

#endif
