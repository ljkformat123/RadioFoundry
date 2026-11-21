# -*- coding: utf-8 -*-
"""mk_v223.py —— 4732 菜单 6 处措辞修正（基于 v222）
1) 0x2817D  PLF On/Off      -> PLF开/关
2) 0x287A8  Meter On/Off    -> S表开/关
3) 0x281B1  Exit Tuner      -> 退出4732      （退出调谐器 15 格超限，改短写）
4) 0x27E9F  FM              -> 调频
5) 0x27EAC  LW              -> 长波
6) 0x27EB9  MW              -> 中波
"""
import hashlib, shutil, os
FW = os.path.join("刷机工具", "固件")
src = os.path.join(FW, "nicFW_C2_v222.bin")
dst = os.path.join(FW, "nicFW_C2_v223.bin")
data = bytearray(open(src, "rb").read())

P = {
    0x2817D: ("\x01\xD3\x03\x87\x01\xC4\xAD", b"PLF\x01\xC4\x2F\xAD", 12),
    0x287A8: ("\x05\xCA\xCB\xCC\xCD\xCE", b"S\x01\x82\xC4\x2F\xAD", 12),
    0x281B1: ("\x03\xDB\x02\x9A\x03\xC8\xC9", b"\x03\xDB\x02\x9A4732", 12),
}

for off, (old, new, cap) in P.items():
    assert data[off:off + len(old)] == old.encode("latin1"), (hex(off), data[off:off+12])
    assert len(new) <= cap, (hex(off), len(new), cap)
    body = new + b"\x20" * (cap - len(new))
    data[off:off + cap] = body
    cells = 0
    page = 0
    for x in new:
        if 1 <= x <= 0x1F and x not in (0x0A, 0x0D):
            cells += 1; page = x - 1
        elif 0x20 <= x <= 0x7F:
            cells += 1
        else:
            cells += 2
    print("%-14s -> %-22s  %d 字节, %d 格" % (old, new.decode("utf-8", "replace"), len(new), cells))

# 波段三行还原成原厂 SW 写法（统一成英文）
FROM_OFFICIAL = [0x2820C, 0x2822E, 0x28249]
official = open(os.path.join(FW, "nicfw880_v5.18.07.bin"), "rb").read()
for off in FROM_OFFICIAL:
    data[off:off + 12] = official[off:off + 12]
    print("还原 0x%05X -> %r" % (off, official[off:off + 12].decode("latin1")))

open(dst, "wb").write(data)
print("\n输出:", dst, len(data), "B")
print("md5:", hashlib.md5(data).hexdigest().upper())