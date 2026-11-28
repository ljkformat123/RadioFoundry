// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// YSF conventional channels in the codeplug (docs/spec/ysf-codeplug.md): the codec from the stored bytes to plain
// structs the radio core sets the YSF engine up from. Bytes in, values out: no engine here. Built with CONFIG_YSF
#ifndef YSF_CODEPLUG_H
#define YSF_CODEPLUG_H

#include <stdint.h>
#include <stdbool.h>
#include "codeplug/codeplug.h"
#include "ysf/ysf_frame.h"

#define YSF_CP_DGID_MAX		127u
#define YSF_CP_TX_DGID_SET	0x80u	// a stored TX DG-ID holds a value (clear: the RX DG-ID)
#define YSF_CP_DEST_ALL		"ALL"	// the destination an all-spaces field reads as, and is written as

// A conventional channel's parameter block (2)
#define YSF_CP_CONV_LEN		13u
#define YSF_CP_DT_MASK		0x03u	// byte 0 bits 0-1: the data type (YSF_DT_*)
#define YSF_CP_NARROW		0x04u	// byte 0 bit 2: Narrow deviation, else Wide
// The radio-wide identity record (1): three call signs of 10 ASCII characters each
#define YSF_CP_IDENTITY_LEN	30u

typedef struct {
	uint8_t dt;		// YSF_DT_VD_MODE1, YSF_DT_DATA_FR, YSF_DT_VD_MODE2, YSF_DT_VOICE_FR
	bool narrow;		// Narrow deviation (1350 Hz) instead of Wide (2700 Hz)
	uint8_t rx_dgid;	// 0: any DG-ID
	uint8_t tx_dgid;	// the DG-ID sent (FICH SC)
	char dest[YSF_CALLSIGN_LEN + 1];	// the destination call sign, NUL-terminated ("ALL" when unset)
} ysf_conv_t;

// The radio-wide call signs: this radio's identity, NUL-terminated, empty when unset
typedef struct {
	char src[YSF_CALLSIGN_LEN + 1];
	char down[YSF_CALLSIGN_LEN + 1];
	char up[YSF_CALLSIGN_LEN + 1];
} ysf_identity_t;

// A call sign to its 10 stored bytes (space padded; an empty one is all spaces). A byte outside printable ASCII
// reads as a space, and trailing spaces are trimmed on the way back; an all-spaces field reads as empty
void ysf_callsign_pack(const char *s, uint8_t out[YSF_CALLSIGN_LEN]);
void ysf_callsign_unpack(const uint8_t b[YSF_CALLSIGN_LEN], char out[YSF_CALLSIGN_LEN + 1]);
// Decode len bytes of a block (a short one, or none, takes the defaults); never fails: a value out of range is read as
// its default
void ysf_conv_decode(const uint8_t *b, int len, ysf_conv_t *out);
// The block for c (YSF_CP_CONV_LEN bytes): a TX DG-ID equal to the RX DG-ID is stored as "follows", a blank
// destination as "ALL"
void ysf_conv_encode(const ysf_conv_t *c, uint8_t out[YSF_CP_CONV_LEN]);
// Decode len bytes of the radio-wide identity record (a short one, or none, leaves the rest empty); never fails
void ysf_identity_decode(const uint8_t *b, int len, ysf_identity_t *out);

// What a conventional call is set up from: the channel's block and the radio-wide call signs
typedef struct {
	ysf_conv_t conv;
	ysf_identity_t id;
} ysf_conv_cfg_t;
// Channel number's set-up from its tag and the radio-wide record (read by codeplug_channel); false when it isn't a
// live YSF channel
bool ysf_conv_load(uint16_t number, const channel_t *ch, ysf_conv_cfg_t *out);
// Write ch at number as a YSF channel (channel-modes.md 2.4): the base record in its safe stand-in shape,
// then the tag with conv's block, transmit allowed unless ch->rx_only. A channel that is YSF already keeps its tag's
// system, fallback, other flags and what its block holds after conv's (codeplug_tag_rewrite), and under a base-own
// tag its record's own mode; else the tag is new.
// Where a YSF channel is made from the radio's own settings (the VFO, a mode changed for now) or its block edited
bool ysf_conv_save(uint16_t number, const channel_t *ch, const ysf_conv_t *conv);

#endif
