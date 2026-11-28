// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// The ysf section: YSF conventional on the channel (docs/spec/ysf.md 8, core/radio/radio_ysf.h): the call heard,
// the settings, what the board's provider gives the deviation, the engine's counters. The voice path's are the DMR
// side's (dmr_stats, ambe.*). "ysf.built=0" without CONFIG_YSF
#include "config.h"
#include "debuglink/debuglink.h"
#if CONFIG_YSF
#include "radio/radio_ysf.h"
#endif

void debuglink_view_ysf(const radio_view_t *v, dl_text_t *t)
{
#if CONFIG_YSF
	const radio_ysf_view_t *n = &v->ysf;
	radio_ysf_settings_t s;
	radio_ysf_stats_t c;

	// The call heard now, or the last one (on=0: the engine is not running on this channel)
	dl_text(t, "ysf.built=1\nysf.on=%u\nysf.rx=%u\nysf.played=%u\nysf.late=%u\nysf.narrow=%u\n",
		(n->flags & RADIO_YSF_ON) != 0, (n->flags & RADIO_YSF_RX) != 0, (n->flags & RADIO_YSF_PLAYED) != 0,
		(n->flags & RADIO_YSF_LATE) != 0, (n->flags & RADIO_YSF_NARROW) != 0);
	dl_text(t, "ysf.dgid=%u\nysf.dt=%u\nysf.dest=%s\nysf.src=%s\nysf.tx=%u\nysf.deny=%u\n", n->dgid, n->dt, n->dest,
		n->src, n->tx, v->tx_deny);
	// What the board's provider gives the deviation in force (AIR_FIT_*: 1 receive, 2 transmit, 4 soft values,
	// 32 rate doubled, 32768 untested), each deviation's too
	dl_text(t, "ysf.fit=%u\nysf.fit_wide=%u\nysf.fit_narrow=%u\n", n->fit, (unsigned)radio_ysf_fit(false),
		(unsigned)radio_ysf_fit(true));
	radio_ysf_settings(&s);
	dl_text(t, "ysf.dt_set=%u\nysf.rx_dgid=%u\nysf.tx_dgid=%u\nysf.dest_set=%s\nysf.src_set=%s\n",
		s.dt, s.rx_dgid, s.tx_dgid, s.dest, s.src);
	dl_text(t, "ysf.down_set=%s\nysf.up_set=%s\n", s.down, s.up);
	if (radio_ysf_in_force(&s)) {
		// What the engine runs on: a stored YSF channel's own, else the settings above
		dl_text(t, "ysf.run.dt=%u\nysf.run.rx_dgid=%u\nysf.run.tx_dgid=%u\nysf.run.dest=%s\n",
			s.dt, s.rx_dgid, s.tx_dgid, s.dest);
		dl_text(t, "ysf.run.src=%s\nysf.run.down=%s\nysf.run.up=%s\n", s.src, s.down, s.up);
	}
	radio_ysf_stats(&c);
	dl_text(t, "ysf.begins=%u\nysf.begin_fails=%u\nysf.rx_calls=%u\nysf.tx_calls=%u\nysf.tx_filler=%u\n",
		(unsigned)c.begins, (unsigned)c.begin_fails, (unsigned)c.rx_calls, (unsigned)c.tx_calls,
		(unsigned)c.tx_filler);
	dl_text(t, "ysf.rx_rejects=%u\nysf.frames=%u\nysf.voice=%u\nysf.rx_chunks=%u\n",
		(unsigned)c.rx_rejects, (unsigned)c.frames, (unsigned)c.voice, (unsigned)c.rx_chunks);
	dl_text(t, "ysf.tx_underruns=%u\nysf.tx_syms=%u\n", (unsigned)c.tx_underruns, (unsigned)c.tx_syms);
#else
	(void)v;
	dl_text(t, "ysf.built=0\n");
#endif
}

void debuglink_ysf_reset(void)
{
#if CONFIG_YSF
	radio_ysf_reset_stats();
#endif
}
