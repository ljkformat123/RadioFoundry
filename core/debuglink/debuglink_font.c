// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// DL_CMD_FONT (docs/spec/debuglink.md 3.31): the CJK glyph blob in the data flash, written over the link
// (CONFIG_CJK_SPI, core/ui/cjk_font.h). The image is flashed without the glyphs, so the host writes the blob once after
// flashing: the sectors are erased, then the bytes go in 48 at a request, then the same command reads them back so the
// host can compare. Nothing here touches another area of the flash: the blob has its own 28 sectors at CJK_SPI_ADDR.
#include <string.h>
#include "config.h"
#include "debuglink/debuglink.h"
#include "debuglink/debuglink_proto.h"

#if CONFIG_CJK_SPI
#include "board.h"
#include "ui/cjk_font.h"

#define FONT_SECTOR	4096u	// the chip's erase unit (docs/spec/hardware-dm1701.md 13.1)
#define FONT_WRITE_MAX	44u	// bytes a WRITE carries: 5 + 44 inside DL_REQ_MAX (64)

static uint32_t rd16(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t rd32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

// The blob: its header, then one glyph a table entry (cjkfont_spi.py)
static uint32_t blob_bytes(void)
{
	extern const uint16_t cjk_font_count;
	return CJK_SPI_HDR + (uint32_t)cjk_font_count * CJK_FONT_BYTES;
}

// 0: not stored (or another build's), 1: the blob this image reads
static uint8_t blob_state(void)
{
	extern const uint16_t cjk_font_count;
	uint8_t h[CJK_SPI_HDR];
	if (!board.flash->read(CJK_SPI_ADDR, h, sizeof h)) {
		return 0;
	}
	uint32_t magic = (uint32_t)h[0] | (uint32_t)h[1] << 8 | (uint32_t)h[2] << 16 | (uint32_t)h[3] << 24;
	if (magic != CJK_SPI_MAGIC || h[4] != (uint8_t)cjk_font_count || h[5] != (uint8_t)(cjk_font_count >> 8)) {
		return 0;
	}
	return 1;
}

size_t debuglink_cmd_font(const uint8_t *req, uint16_t len, uint8_t *out, size_t cap)
{
	if (cap < 12u || len < 1u) {
		out[0] = DL_ST_BAD_ARG;
		return 1;
	}
	if (board.flash == NULL || board.flash->read == NULL || board.flash->write == NULL || board.flash->erase == NULL ||
	    board.flash->size < CJK_SPI_ADDR + blob_bytes()) {
		out[0] = DL_ST_UNAVAILABLE;	// no data flash, or one too small for the blob
		return 1;
	}
	uint32_t size = blob_bytes();
	uint32_t sectors = (size + FONT_SECTOR - 1u) / FONT_SECTOR;
	out[0] = DL_ST_OK;
	switch (req[0]) {
	case DL_FONT_INFO:		// -> present u8, address u32, bytes u32, sectors u8
		out[1] = blob_state();
		put32(out + 2, CJK_SPI_ADDR);
		put32(out + 6, size);
		out[10] = (uint8_t)sectors;
		return 11;
	case DL_FONT_ERASE:		// sector u8: one erase, up to 400 ms on the chip (a maintenance op)
		if (len < 2u || req[1] >= sectors) {
			out[0] = DL_ST_BAD_ARG;
			return 1;
		}
		if (!board.flash->erase(CJK_SPI_ADDR + (uint32_t)req[1] * FONT_SECTOR)) {
			out[0] = DL_ST_UNAVAILABLE;	// the chip refused: erase size mismatch, or no chip
		}
		return 1;
	case DL_FONT_WRITE: {		// offset u32, bytes: program the erased sector
		if (len < 5u || (uint32_t)(len - 5u) > FONT_WRITE_MAX) {
			out[0] = DL_ST_BAD_ARG;
			return 1;
		}
		uint32_t off = rd32(req + 1);
		uint16_t n = (uint16_t)(len - 5u);
		if (off + n > size) {
			out[0] = DL_ST_BAD_ARG;
			return 1;
		}
		if (!board.flash->write(CJK_SPI_ADDR + off, req + 5, n)) {
			out[0] = DL_ST_UNAVAILABLE;
		}
		return 1;
	}
	case DL_FONT_READ: {		// offset u32, length u16 -> the bytes (the host compares them)
		if (len < 7u) {
			out[0] = DL_ST_BAD_ARG;
			return 1;
		}
		uint32_t off = rd32(req + 1), n = (uint32_t)rd16(req + 5);
		if (off + n > size) {
			out[0] = DL_ST_BAD_ARG;
			return 1;
		}
		if (n > cap - 1u) {
			n = cap - 1u;
		}
		if (n == 0u || !board.flash->read(CJK_SPI_ADDR + off, out + 1, (uint32_t)n)) {
			out[0] = n == 0u ? DL_ST_BAD_ARG : DL_ST_UNAVAILABLE;
			return 1;
		}
		return (size_t)(1u + n);
	}
	default:
		out[0] = DL_ST_BAD_ARG;
		return 1;
	}
}
#else
size_t debuglink_cmd_font(const uint8_t *req, uint16_t len, uint8_t *out, size_t cap)
{
	(void)req;
	(void)len;
	(void)cap;
	out[0] = DL_ST_UNAVAILABLE;	// the glyphs are in the image (CONFIG_CJK_SPI off)
	return 1;
}
#endif
