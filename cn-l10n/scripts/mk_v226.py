# -*- coding: utf-8 -*-
"""mk_v226.py —— 调谐器页顶部波段全称还原英文（基于 v225）
0x2877C Long Wave    (现 长波 03 E7 87 + 6 空格)
0x28786 Medium Wave  (现 中波 FE 03 87 + 8 空格)
两条串所在槽长度分别为 9 / 11 字节，原厂英文正好同长。
"""
import hashlib, os
FW = os.path.join("刷机工具", "固件")
src = os.path.join(FW, "nicFW_C2_v225.bin")
dst = os.path.join(FW, "nicFW_C2_v226.bin")
data = bytearray(open(src, "rb").read())

P = [
    (0x2877C, b"\x03\xE7\x87\x20\x20\x20\x20\x20\x20", b"Long Wave"),
    (0x28786, b"\xFE\x03\x87\x20\x20\x20\x20\x20\x20\x20\x20", b"Medium Wave"),
]
for off, old, new in P:
    assert bytes(data[off:off + len(old)]) == old, (hex(off), bytes(data[off:off + len(old)]))
    assert len(new) == len(old)
    data[off:off + len(new)] = new
    print("0x%05X %-10s -> %s" % (off, old.decode("latin1").strip(), new.decode()))

open(dst, "wb").write(data)
print("输出:", dst, len(data), "B")
print("md5:", hashlib.md5(data).hexdigest().upper())
