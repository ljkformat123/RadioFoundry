// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Lookup in the generated CJK face: a sorted array of the code points it has, and a glyph per entry, so a character
// costs a binary search and no RAM. cjk_font_data.c is built by tools/cjkfont.py from the catalogs themselves; this
// file is the search, and stays small on purpose.
//
// With CONFIG_CJK_SPI the glyphs are not in the image: they sit in the data flash (CJK_SPI_ADDR, cjk_font.h) as the
// blob tools/cjkfont_spi.py writes, and the search finds the entry then reads the one glyph out of it. The code point
// table stays in program flash, so the search itself costs no bus transfer.
#include <stddef.h>
#include "cjk_font.h"
#if CONFIG_CJK_SPI
#include "board.h"
#endif

#if CONFIG_CJK_SPI
// 0: not checked yet, 1: the blob is there and matches this build, 2: it is not (draw boxes, do not retry every glyph)
static uint8_t blob_state;

static bool blob_ok(void)
{
	extern const uint16_t cjk_font_count;
	if (blob_state == 0) {
		uint8_t h[CJK_SPI_HDR];
		blob_state = 2;
		if (board.flash != NULL && board.flash->read != NULL && board.flash->size >= CJK_SPI_ADDR + CJK_SPI_HDR &&
		    board.flash->read(CJK_SPI_ADDR, h, CJK_SPI_HDR) &&
		    (uint32_t)h[0] + ((uint32_t)h[1] << 8) + ((uint32_t)h[2] << 16) + ((uint32_t)h[3] << 24) == CJK_SPI_MAGIC &&
		    h[4] == (uint8_t)cjk_font_count && h[5] == (uint8_t)(cjk_font_count >> 8)) {
			blob_state = 1;
		}
	}
	return blob_state == 1;
}

bool cjk_font_load(uint32_t cp, uint8_t out[CJK_FONT_BYTES])
{
	extern const uint16_t cjk_font_cp[];
	extern const uint16_t cjk_font_count;
	if (cp > 0xFFFF || !blob_ok())
		return false;			// the table holds the BMP only: every CJK block is below it
	uint16_t lo = 0, hi = cjk_font_count;
	while (lo < hi) {
		uint16_t mid = (uint16_t)((lo + hi) / 2u);
		uint16_t at = cjk_font_cp[mid];
		if (at == (uint16_t)cp)
			return board.flash->read(CJK_SPI_ADDR + CJK_SPI_HDR + (uint32_t)mid * CJK_FONT_BYTES,
						 out, CJK_FONT_BYTES);
		if (at < (uint16_t)cp)
			lo = (uint16_t)(mid + 1u);
		else
			hi = mid;
	}
	return false;				// no such glyph: the text layer draws a box
}
#endif

const uint8_t *cjk_font_glyph(uint32_t cp)
{
#if CONFIG_CJK_GLYPHS && !CONFIG_CJK_SPI
	// core/ui/cjk_font_data.c, built by tools/cjkfont.py
	extern const uint16_t cjk_font_cp[];
	extern const uint8_t cjk_font_bits[];
	extern const uint16_t cjk_font_count;
	if (cp > 0xFFFF)
		return NULL;			// the table holds the BMP only: every CJK block is below it
	uint16_t lo = 0, hi = cjk_font_count;
	while (lo < hi) {
		uint16_t mid = (uint16_t)((lo + hi) / 2u);
		uint16_t at = cjk_font_cp[mid];
		if (at == (uint16_t)cp)
			return cjk_font_bits + (size_t)mid * CJK_FONT_BYTES;
		if (at < (uint16_t)cp)
			lo = (uint16_t)(mid + 1u);
		else
			hi = mid;
	}
#else
	(void)cp;
#endif
	return NULL;			// no table built (CONFIG_CJK_GLYPHS off), or the glyphs are in the data flash
}
