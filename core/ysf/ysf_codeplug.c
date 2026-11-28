// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional channels in the codeplug (ysf_codeplug.h, docs/spec/ysf-codeplug.md)
#include <string.h>
#include "config.h"
#include "ysf/ysf_codeplug.h"
#include "codeplug/codeplug_tag.h"
#include "codeplug/codeplug_records.h"

#if CONFIG_YSF

void ysf_callsign_pack(const char *s, uint8_t out[YSF_CALLSIGN_LEN])
{
	bool ended = s == NULL;

	// A NUL ends the call sign: the rest are spaces (never read past it)
	for (unsigned i = 0; i < YSF_CALLSIGN_LEN; i++) {
		char c = ' ';
		if (!ended) {
			if (s[i] == '\0')
				ended = true;
			else
				c = s[i];
		}
		out[i] = (uint8_t)(c >= 0x20 && c <= 0x7E ? c : ' ');
	}
}

void ysf_callsign_unpack(const uint8_t b[YSF_CALLSIGN_LEN], char out[YSF_CALLSIGN_LEN + 1])
{
	unsigned n = 0;

	for (unsigned i = 0; i < YSF_CALLSIGN_LEN; i++) {
		char c = (char)b[i];
		out[n++] = (c >= 0x20 && c <= 0x7E && c != '\0') ? c : ' ';
	}
	while (n > 0 && out[n - 1] == ' ')
		n--;
	out[n] = '\0';
}

void ysf_conv_decode(const uint8_t *b, int len, ysf_conv_t *out)
{
	uint8_t f = len >= 1 ? b[0] : 0u;
	uint8_t rx = len >= 2 ? b[1] : 0u;
	uint8_t tx = len >= 3 ? b[2] : 0u;

	out->dt = (uint8_t)(f & YSF_CP_DT_MASK);
	out->narrow = (f & YSF_CP_NARROW) != 0;
	if (rx > YSF_CP_DGID_MAX)
		rx = 0;			// any DG-ID
	out->rx_dgid = rx;
	// A TX DG-ID field: its value when set, else the RX DG-ID
	if ((tx & YSF_CP_TX_DGID_SET) != 0)
		out->tx_dgid = (uint8_t)(tx & YSF_CP_DGID_MAX);
	else
		out->tx_dgid = rx;
	if (len > 3) {
		// A short destination reads with its missing characters as spaces
		uint8_t dest[YSF_CALLSIGN_LEN];
		int n = len - 3 < (int)YSF_CALLSIGN_LEN ? len - 3 : (int)YSF_CALLSIGN_LEN;

		memset(dest, ' ', sizeof dest);
		memcpy(dest, &b[3], (size_t)n);
		ysf_callsign_unpack(dest, out->dest);
		if (out->dest[0] == '\0')
			strcpy(out->dest, YSF_CP_DEST_ALL);
	} else {
		strcpy(out->dest, YSF_CP_DEST_ALL);
	}
}

void ysf_conv_encode(const ysf_conv_t *c, uint8_t out[YSF_CP_CONV_LEN])
{
	uint8_t rx = c->rx_dgid <= YSF_CP_DGID_MAX ? c->rx_dgid : 0u;

	out[0] = (uint8_t)((c->dt & YSF_CP_DT_MASK) | (c->narrow ? YSF_CP_NARROW : 0u));
	out[1] = rx;
	out[2] = c->tx_dgid == rx ? 0u : (uint8_t)(YSF_CP_TX_DGID_SET | (c->tx_dgid & YSF_CP_DGID_MAX));
	if (c->dest[0] == '\0' || strcmp(c->dest, YSF_CP_DEST_ALL) == 0)
		ysf_callsign_pack(YSF_CP_DEST_ALL, &out[3]);
	else
		ysf_callsign_pack(c->dest, &out[3]);
}

void ysf_identity_decode(const uint8_t *b, int len, ysf_identity_t *out)
{
	out->src[0] = out->down[0] = out->up[0] = '\0';
	if (len >= (int)YSF_CALLSIGN_LEN)
		ysf_callsign_unpack(&b[0], out->src);
	if (len >= (int)(2u * YSF_CALLSIGN_LEN))
		ysf_callsign_unpack(&b[YSF_CALLSIGN_LEN], out->down);
	if (len >= (int)(3u * YSF_CALLSIGN_LEN))
		ysf_callsign_unpack(&b[2u * YSF_CALLSIGN_LEN], out->up);
}

bool ysf_conv_load(uint16_t number, const channel_t *ch, ysf_conv_cfg_t *out)
{
	uint8_t b[YSF_CP_CONV_LEN + YSF_CP_IDENTITY_LEN + CODEPLUG_TAG_HEAD_MAX];
	const int cap = (int)(sizeof b - CODEPLUG_TAG_HEAD_MAX);
	int len, idlen;

	if (ch->kind != CODEPLUG_KIND_YSF)
		return false;
	len = codeplug_tag_params(number, ch, b, (uint16_t)cap);
	if (len < 0)
		return false;
	ysf_conv_decode(b, len < cap ? len : cap, &out->conv);
	idlen = codeplug_mode_settings(CODEPLUG_KIND_YSF, b, YSF_CP_IDENTITY_LEN);
	ysf_identity_decode(b, idlen < 0 ? 0 : idlen, &out->id);
	return true;
}

bool ysf_conv_save(uint16_t number, const channel_t *ch, const ysf_conv_t *conv)
{
	channel_t c = *ch;
	uint8_t blk[YSF_CP_CONV_LEN];
	// A channel of the kind already keeps the system it names
	uint8_t system = ch->kind == CODEPLUG_KIND_YSF ? ch->system : CODEPLUG_SYSTEM_NONE;
	codeplug_tag_t t = { .kind = CODEPLUG_KIND_YSF, .system = system, .flags = ch->rx_only ? 0u : CODEPLUG_TAG_TX };

	// The safe stand-in: DMR, receive only (the tag carries transmit allowed). Under a tag that says the base record
	// is a channel of its own, codeplug_set_channel keeps that channel's mode instead
	c.kind = CODEPLUG_KIND_YSF;
	c.system = system;
	c.mode = RADIO_MODE_DMR;
	c.rx_only = true;
	ysf_conv_encode(conv, blk);
	// A tag of the kind that followed the record keeps its fallback, its other flags and what it stores after this
	// block (an editor's or a later firmware's); else a new one
	return codeplug_set_channel(number, &c) && (codeplug_tag_rewrite(number, c.name, &t, blk, sizeof blk) ||
						     codeplug_tag_write(number, c.name, &t, blk, sizeof blk));
}

#endif	// CONFIG_YSF
