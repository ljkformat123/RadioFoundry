# -*- coding: utf-8 -*-
"""从手台 SPI 读 16x16 菜单字库，确认 息(页0/CA) 与 休(页1/8D) 是否真是这两个字"""
import gzip, time, serial

BASE = 0xB2002
CELL = 0x20
PAGE = 0x2000

def load_uni():
    try:
        t = gzip.open("unifont.hex", "rt", encoding="utf-8").read()
    except Exception:
        t = open("unifont.hex", encoding="utf-8").read()
    m = {}
    for l in t.splitlines():
        if ":" in l:
            a, b = l.split(":", 1)
            m[int(a, 16)] = b.strip()
    return m

UM = load_uni()

def g16(ch):
    b = UM.get(ord(ch))
    if b is None:
        return [[0] * 16 for _ in range(16)]
    b = b.ljust(64, "0")
    return [[(int(b[r * 4:r * 4 + 4], 16) >> (15 - x)) & 1 for x in range(16)]
            for r in range(16)]

def pack(rows, h, w):
    out = bytearray()
    for x in range(w):
        for band in range(2):
            v = 0
            for i in range(8):
                y = band * 8 + i
                if rows[y][x]:
                    v |= 1 << i
            out.append(v)
    return bytes(out)

s = serial.Serial("COM3", 38400, timeout=2)
time.sleep(0.4)

def rd(a):
    a = a & ~0x7F
    for _ in range(6):
        s.reset_input_buffer()
        s.write(bytes([0xAA, 0x30]) + a.to_bytes(4, "little") + b"\x55")
        time.sleep(0.2)
        r = s.read(200)
        if len(r) >= 135 and (sum(r[6:134]) + sum(a.to_bytes(4, "little"))) & 0xFF == r[134]:
            return bytes(r[6:134])
        time.sleep(0.2)
    return None

def get(addr):
    blk = rd(addr)
    if blk is None:
        return None
    off = addr - (addr & ~0x7F)
    return blk[off:off + 32]

def art(b):
    rows = []
    for y in range(16):
        line = ""
        for x in range(16):
            band = y // 8
            bit = y % 8
            v = b[x * 2 + band]
            line += "#" if (v >> bit) & 1 else "."
        rows.append(line)
    return "\n".join(rows)

tests = [("息", 0, 0xCA), ("休", 1, 0x8D)]
for ch, page, code in tests:
    for label, addr in (("码原样", BASE + page * PAGE + code * CELL),
                        ("码-0x80", BASE + page * PAGE + (code - 0x80) * CELL)):
        b = get(addr)
        exp = pack(g16(ch), 16, 16)
        print("\n=== %s  @0x%05X (%s) ===" % (ch, addr, label))
        if b is None:
            print("  读失败")
            continue
        print("  实读:", b.hex(" "))
        print("  字模:", exp.hex(" "))
        print("  一致:", "是" if b == exp else "否")
        if b != exp:
            print(art(b))

# 顺带看页0 码 CA 与页1 码 8D 是不是同一个字
s.close()
