#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Pack the generated CJK glyph table into the blob the data flash holds (CONFIG_CJK_SPI).

core/ui/cjk_font_data.c carries the glyph bitmaps as a C array when CONFIG_CJK_SPI is off. With it on, the radio reads
the same bytes out of the SPI NOR at CJK_SPI_ADDR (core/ui/cjk_font.h) instead, and this writes them as the blob it
expects: an 8 byte header (magic 'CJKS', glyph count, two reserved bytes) and then CJK_FONT_BYTES a glyph, in the code
point table's order, so entry i of the table and glyph i of the blob are the same character.

    tools/cjkfont_spi.py --c core/ui/cjk_font_data.c --out core/ui/cjk_font_spi.bin

The flasher writes the blob at CJK_SPI_ADDR; without it every Chinese character draws as a box.
"""
import argparse
import re


MAGIC = b"CJKS"
GLYPH_BYTES = 24				# 12x12 one-bit, two bytes a row (core/ui/cjk_font.h)


def read_c(path):
    with open(path, encoding="utf-8") as f:
        src = f.read()
    at = src.index("cjk_font_bits[] = {")
    end = src.index("};", at)
    body = re.sub(r"/\*.*?\*/", " ", src[src.index("{", at) + 1:end], flags=re.S)
    toks = [t.strip(",") for t in body.split()]
    glyphs = bytes(int(t, 16) for t in toks if t.startswith("0x"))
    count = int(src[src.index("cjk_font_count = ") + len("cjk_font_count = "):].split(";")[0].strip())
    if len(glyphs) != count * GLYPH_BYTES:
        raise SystemExit(f"{path}: {len(glyphs)} bytes for {count} glyphs, expected {count * GLYPH_BYTES}")
    return count, glyphs


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--c", default="core/ui/cjk_font_data.c")
    ap.add_argument("--out", default="core/ui/cjk_font_spi.bin")
    a = ap.parse_args()
    count, glyphs = read_c(a.c)
    blob = MAGIC + count.to_bytes(2, "little") + (0).to_bytes(2, "little") + glyphs
    with open(a.out, "wb") as f:
        f.write(blob)
    print(f"{a.out}: {len(blob)} bytes, {count} glyphs of {GLYPH_BYTES}")


if __name__ == "__main__":
    main()
