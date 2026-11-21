# -*- coding: utf-8 -*-
"""mk_v225.py —— 4732 菜单“开关”类统一写成“开/关”（基于 v224）
0x2879B 频谱开关 -> 频谱开/关
"""
import hashlib, os
FW = os.path.join("刷机工具", "固件")
src = os.path.join(FW, "nicFW_C2_v224.bin")
dst = os.path.join(FW, "nicFW_C2_v225.bin")
data = bytearray(open(src, "rb").read())

off = 0x2879B
old = b"\x03\xF2\xCB\x01\xC4\xAD"
new = b"\x03\xF2\xCB\x01\xC4\x2F\xAD"
CAP = 12
assert bytes(data[off:off + len(old)]) == old, bytes(data[off:off + 12])
assert len(new) <= CAP
data[off:off + CAP] = new + b"\x20" * (CAP - len(new))
print("0x2879B 频谱开关 -> 频谱开/关  %d 字节" % len(new))

open(dst, "wb").write(data)
print("输出:", dst, len(data), "B")
print("md5:", hashlib.md5(data).hexdigest().upper())
