# -*- utf-8 -*-
"""mk_v222_swband.py —— v221 基础上，把“波段定义”那组 SW 字符串回退英文
原因：调谐器页顶部那行用 8x16(f=1) 小字画同一段波段名，多字节汉字编码在 f=1 路径下会被
     重映射成 4 个乱码字节（抓到 ?c进?1.7-6），所以小字那一路必须保持 ASCII。
范围：0x28316–0x2833A（原厂 SW 12.0-30.0 / SW 6.0-12.0 / SW 1.7-6.0 三条定义串）
列表那组 12 格显示串（0x2820C 一带）保持汉字不动。
"""
import hashlib, os

SQ = r'C:\Users\Administrator\Doubao\chats\2026-11-13\new-chat\uv98fw'
SRC = os.path.join(SQ, '刷机工具', '固件', 'nicFW_C2_v221.bin')
STOCK = os.path.join(SQ, '刷机工具', '固件', 'nicfw880_v5.18.07.bin')
OUT = os.path.join(SQ, '刷机工具', '固件', 'nicFW_C2_v222.bin')

LO, HI = 0x28316, 0x2833A

a = open(STOCK, 'rb').read()
b = bytearray(open(SRC, 'rb').read())

print('回退前 v221 :', bytes(b[LO:HI]))
print('回退成原厂 :', a[LO:HI])
n = 0
for i in range(LO, HI):
    if b[i] != a[i]:
        b[i] = a[i]
        n += 1

open(OUT, 'wb').write(bytes(b))
print('回退后      :', bytes(b[LO:HI]))
print('v222 = %s' % OUT)
print('  大小 %d B，md5 %s（改动 %d 字节）' % (len(b), hashlib.md5(bytes(b)).hexdigest().upper(), n))
