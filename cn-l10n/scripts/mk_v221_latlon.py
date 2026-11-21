# -*- utf-8 -*-
"""mk_v221_latlon.py —— v220 基础上，把最下面两栏标签补成“纬度 / 经度”
  0x8A4C  WP Lat: -> 纬度   (原厂槽 7 字节)
  0x8A54  WP Lng: -> 经度   (原厂槽 7 字节)
只改这两个槽，其余不动。
"""
import hashlib, os

SQ = r'C:\Users\Administrator\Doubao\chats\2026-11-13\new-chat\uv98fw'
SRC = os.path.join(SQ, '刷机工具', '固件', 'nicFW_C2_v220.bin')
OUT = os.path.join(SQ, '刷机工具', '固件', 'nicFW_C2_v221.bin')

JOBS = [
    (0x08A4C, 7, bytes([0x03, 0xB5, 0x04, 0x8A]), b'WP Lat:'),  # 纬=页2 0xB5，度=页3 0x8A
    (0x08A54, 7, bytes([0x03, 0xB6, 0x04, 0x8A]), b'WP Lng:'),  # 经=页2 0xB6，度=页3 0x8A
]

b = bytearray(open(SRC, 'rb').read())
for addr, ln, newb, oldname in JOBS:
    b[addr:addr + ln] = newb + b'\x00' * (ln - len(newb))
    print('  0x%05X  原厂 %-8s -> %s' % (addr, oldname.decode(), bytes(b[addr:addr + ln]).hex(' ').upper()))

open(OUT, 'wb').write(bytes(b))
print('v221 = %s' % OUT)
print('  大小 %d B，md5 %s' % (len(b), hashlib.md5(bytes(b)).hexdigest().upper()))
