// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The CJK face (CONFIG_LANG_ZH): a 12x12 one-bit glyph for every character core/ui/lang/zh.po uses, which
// tools/cjkfont.py rasterizes from an outline font into cjk_font_data.c. The Latin fonts have no glyph for Chinese, so
// text is UTF-8 and the text layer (gfx.c) looks each character up here. The cell is 12 pixels square, the height of
// the 6x12 font, so a line with Chinese keeps that height and a 160x120 panel keeps its rows (local change: the
// upstream cell is 16, the height of 8x16).
#ifndef UI_CJK_FONT_H
#define UI_CJK_FONT_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define CJK_FONT_W		12
#define CJK_FONT_H		12
#define CJK_FONT_ROW_BYTES	2
#define CJK_FONT_BYTES		(CJK_FONT_H * CJK_FONT_ROW_BYTES)

#if CONFIG_CJK_SPI
// The glyph bitmaps kept in the data flash instead of the program flash (CONFIG_CJK_SPI): the blob
// tools/cjkfont_spi.py builds from the same table sits at this address, an 8 byte header then CJK_FONT_BYTES a glyph
// in the code point table's order. The code point table stays in program flash, so the search is the one below
#define CJK_SPI_ADDR	0x120000u		// the first free area of the 16 MB SPI NOR (docs/spec/hardware-dm1701.md 13.2)
#define CJK_SPI_MAGIC	0x534B4A43u		// 'CJKS' little-endian: magic, count (u16), reserved (u16), then the glyphs
#define CJK_SPI_HDR	8u

// Copy the glyph of a code point out of the blob into out (CJK_FONT_BYTES). False when the blob is missing or holds
// no such glyph: the text layer then draws a box, the same as for a character the table has none for
bool cjk_font_load(uint32_t cp, uint8_t out[CJK_FONT_BYTES]);
#endif

// The glyph of a code point: CJK_FONT_H rows of CJK_FONT_ROW_BYTES bytes, MSB the left of the 16, or NULL when the
// font has none (the text layer draws a box). Built only with CONFIG_CJK_GLYPHS; without it the call returns NULL,
// and with CONFIG_CJK_SPI it always does (the glyphs are in the data flash: cjk_font_load)
const uint8_t *cjk_font_glyph(uint32_t cp);

#endif
