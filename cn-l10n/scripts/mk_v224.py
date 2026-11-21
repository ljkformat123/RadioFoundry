# -*- coding: utf-8 -*-
"""mk_v224.py —— 短波三行改成统一干净排版（基于 v223）
0x2820C SW  12 - 30  -> SW 12-30
0x2822E SW   6 - 12  -> SW 6-12
0x28249 SW 1.7 -  6  -> SW 1.7-6
"""
import hashlib, os
FW = os.path.join("刷机工具", "固件")
src = os.path.join(FW, "nicFW_C2_v223.bin")
dst = os.path.join(FW, "nicFW_C2_v224.bin")
data = bytearray(open(src, "rb").read())

P = {0x2820C: b"SW 12-30", 0x2822E: b"SW 6-12", 0x28249: b"SW 1.7-6"}
CAP = 12
for off, new in P.items():
    assert len(new) <= CAP
    print("0x%05X 旧: %r  新: %r" % (off, bytes(data[off:off + CAP]).decode("latin1"), new.decode()))
    data[off:off + CAP] = new + b"\x20" * (CAP - len(new))

open(dst, "wb").write(data)
print("输出:", dst, len(data), "B")
print("md5:", hashlib.md5(data).hexdigest().upper())
